#include "upnp.h"
#include "upnp_common.h"
#include "uuid.h"
#include "control.h"
#include "eventing.h"
#include "description.h"
#include "discovery.h"
#include "stream.h"

#include "control/av_transport.h"
#include "control/connection_manager.h"
#include "control/rendering_control.h"

#include <audio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_log.h>

#include <esp_netif.h>
#include <esp_http_server.h>
#include <esp_http_client.h>

static const char *TAG = "upnp";

static struct {
    int port;
    char ip_addr[IPADDR_STRLEN_MAX];
    char friendly_name[50];
    uuid_t uuid;
} upnp_info;

static struct {
    char content_type[20];
    int content_length;
    int buff_len;
} content_info;

static void buffer_ready(void) {
    ESP_LOGD(TAG, "Buffer ready!");
    flag_event(BUFFER_READY);
}

static void stream_finished(void) {
    ESP_LOGI(TAG, "Stream finished");
    flag_event(STOP_PLAYBACK);
}

static void stream_failed(void) {
    ESP_LOGW(TAG, "Stream failed");
    flag_event(STOP_PLAYBACK);
}

static void decoder_ready(void) {
    ESP_LOGD(TAG, "Decoder ready!");
    flag_event(DECODER_READY);
}

static void decoder_finished(void) {
    ESP_LOGI(TAG, "Decoder finished");
    flag_event(STOP_PLAYBACK);
}

static void decoder_failed(void) {
    ESP_LOGW(TAG, "Decoder failed!");
    flag_event(STOP_PLAYBACK);
}

static void append_samples(uint32_t samples, uint32_t sample_rate) {
    av_transport_update_counters(samples, sample_rate);
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 5;
    config.max_uri_handlers = CONTROL_URIS + EVENTING_URIS + DESCRIPTION_URIS;
    config.lru_purge_enable = true;
    config.server_port = upnp_info.port;

    // Start the httpd server
    ESP_LOGI(TAG, "Using %d URIs", config.max_uri_handlers);
    ESP_LOGI(TAG, "uPnP server on port %d", config.server_port);
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    return server;
}

static esp_err_t get_content_cb(esp_http_client_event_t *evt) {
    if (evt->event_id == HTTP_EVENT_ON_HEADER && strcmp(evt->header_key, "Content-Type") == 0) {
        strcpy(content_info.content_type, evt->header_value);
    } else if (evt->event_id == HTTP_EVENT_ON_FINISH) {
        content_info.content_length = esp_http_client_get_content_length(evt->client);
    }

    return ESP_OK;
}

static void get_content_info(const char* url) {
    esp_http_client_config_t head_config = {
            .url = url,
            .method = HTTP_METHOD_HEAD,
            .port = upnp_info.port+1,
            .event_handler = get_content_cb,
            .user_agent =useragent_STR
    };
    esp_http_client_handle_t head_request = esp_http_client_init(&head_config);
    esp_http_client_perform(head_request);
    esp_http_client_cleanup(head_request);
}

static void setup_streaming(void) {
    char* url = get_track_url();
    get_content_info(url);

    AudioDecoderConfig_t decoder_config = {
            .content_type = content_info.content_type,
            .file_size = content_info.content_length,
            .decoder_ready_cb = decoder_ready,
            .decoder_finished_cb = decoder_finished,
            .decoder_failed_cb = decoder_failed,
            .wrote_samples_cb = append_samples
    };

    if (audio_init_decoder(&decoder_config) != true) {
        ESP_LOGW(TAG, "File type not supported");
        // Stop AVTransport or something
        return;
    }

    start_stream(url, content_info.content_length);
    free(url);

    const uint8_t* buffer;
    size_t buffer_length;
    stream_take_buffer(&buffer, &buffer_length);

    unflag_event(BUFFER_READY | DECODER_READY);
    audio_decoder_continue(buffer, buffer_length);
    av_transport_stream_ready();
}

static void service_eventing(uint32_t bits) {
    if (bits & AV_TRANSPORT_SEND_ALL) {
        unflag_event(AV_TRANSPORT_SEND_ALL);
        char* message = get_av_transport_all();
        event_av_transport(message);
        free(message);
    } else if (bits & AV_TRANSPORT_CHANGED) {
        unflag_event(AV_TRANSPORT_CHANGED);
        char* message = get_av_transport_changes();
        event_av_transport(message);
        free(message);
    }

    if (bits & RENDERING_CONTROL_SEND_ALL) {
        unflag_event(RENDERING_CONTROL_SEND_ALL);
        char* message = get_rendering_control_all();
        event_rendering_control(message);
        free(message);
    } else if (bits & RENDERING_CONTROL_CHANGED) {
        unflag_event(RENDERING_CONTROL_CHANGED);
        char* message = get_rendering_control_changes();
        event_rendering_control(message);
        free(message);
    }

    if (bits & SEND_PROTOCOL_INFO) {
        unflag_event(SEND_PROTOCOL_INFO);
        send_protocol_info();
    }
}

static void service_av_transport(uint32_t bits) {
    if (bits & STOP_PLAYBACK) {
        unflag_event(STOP_PLAYBACK);
        ESP_LOGI(TAG, "Stopping");

        if (bits & RESET_PLAYBACK) {
            unflag_event(RESET_PLAYBACK);
            av_transport_reset();
        }

        stream_release_buffer();
        audio_reset();
        stop_stream();
        unflag_event(BUFFER_READY | DECODER_READY);
    } else if (bits & PAUSE_PLAYBACK) {
        unflag_event(PAUSE_PLAYBACK);
        audio_pause_playback();
    } else if (bits & RESUME_PLAYBACK) {
        unflag_event(RESUME_PLAYBACK);
        audio_resume_playback();
    } else if ((bits & BUFFER_READY) && (bits & DECODER_READY)) {
        unflag_event(BUFFER_READY | DECODER_READY);
        ESP_LOGD(TAG, "Servicing");
        stream_release_buffer();
        const uint8_t* buffer;
        size_t buffer_length;
        stream_take_buffer(&buffer, &buffer_length);
        audio_decoder_continue(buffer, buffer_length);
    } else if (bits & START_STREAMING) {
        unflag_event(START_STREAMING);

        ESP_LOGI(TAG, "Preparing for playback");
        setup_streaming();
    }
}

static void service_other(uint32_t bits) {
    if (bits & EVENTING_CLEAN_SUBSCRIBERS) {
        unflag_event(EVENTING_CLEAN_SUBSCRIBERS);
        eventing_clean_subscribers();
    }

    if (bits & DISCOVERY_SEND_NOTIFY) {
        unflag_event(DISCOVERY_SEND_NOTIFY);
        discovery_send_notify();
    }
}

_Noreturn void upnp_loop(void* args) {
    while (1) {
        uint32_t bits = get_events();
        service_av_transport(bits);
        service_eventing(bits);
        service_other(bits);

        service_discovery();
    }
}

void upnp_start(size_t stack_size, int priority, int port, const char* ip_addr, const uint8_t* mac_addr, const char* friendly_name) {
    start_events();
    upnp_info.port = port;
    strcpy(upnp_info.ip_addr, ip_addr);
    strcpy(upnp_info.friendly_name, friendly_name);

    uuid_init(mac_addr);
    get_device_uuid(&upnp_info.uuid);
    ESP_LOGI(TAG, "UUID is %s", upnp_info.uuid.uuid_s);

    httpd_handle_t server = start_webserver();
    start_control(server);
    start_eventing(server, port);
    start_description(server, port, upnp_info.friendly_name, upnp_info.uuid.uuid_s, upnp_info.ip_addr);
    start_discovery(upnp_info.ip_addr, upnp_info.uuid.uuid_s);

    StreamConfig_t stream_config = {
            .port = port,
            .user_agent = useragent_STR,
            .buffer_count = 3,
            .buffer_length = 524288,
            .buffer_ready = buffer_ready,
            .stream_finished = stream_finished,
            .stream_failed = stream_failed
    };
    init_stream(stack_size, priority-1, &stream_config);

    xTaskCreate(upnp_loop, "uPnP Loop", stack_size, NULL, priority, NULL);
}