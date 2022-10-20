#include "eventing.h"
#include "upnp_common.h"
#include "uuid.h"

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <freertos/semphr.h>

#include <esp_log.h>

#include <esp_http_client.h>

#define SUBSCRIBER_REFRESH_MS 100000

static const char *TAG = "upnp_eventing";

struct subscription {
    TickType_t timeout;
    char* callback;
    uuid_t sid;
    uint32_t seq;
};

#define MAX_SUBSCRIBERS 2
enum subscription_service { AVTransport, ConnectionManager, RenderingControl };
struct {
    struct subscription service[3];
} static subscription_list[MAX_SUBSCRIBERS];
static SemaphoreHandle_t subscription_mutex;

extern char StateChangeEvent_start[] asm("_binary_StateChangeEvent_xml_start");
extern char StateChangeEvent_end[] asm("_binary_StateChangeEvent_xml_end");
static void send_state_change_event(enum subscription_service service_id, const char* message) {
    char service[4];
    switch (service_id) {
        case AVTransport:
            strcpy(service, "AVT");
            break;
        case RenderingControl:
            strcpy(service, "RCS");
            break;
        case ConnectionManager:
        default:
            abort();
    }

    xSemaphoreTake(subscription_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_SUBSCRIBERS && subscription_list[i].service[service_id].timeout != 0; i++) {
        esp_http_client_config_t notify_config = {
                .url = subscription_list[i].service[service_id].callback,
                .method = HTTP_METHOD_NOTIFY,
                .port = SERVER_PORT,
                .keep_alive_enable = false
        };
        esp_http_client_handle_t  notify_request = esp_http_client_init(&notify_config);
        esp_http_client_set_header(notify_request, "Content-Type", "text/xml; charset=\"utf-8\"");

        int buf_len = snprintf(NULL, 0, StateChangeEvent_start, service, message) + 1;
        char* buf = malloc(buf_len);
        buf_len = sprintf(buf, StateChangeEvent_start, service, message);
        esp_http_client_set_post_field(notify_request, buf,buf_len);

        char seq_buf[10];
        sprintf(seq_buf, "%d", subscription_list[i].service[service_id].seq++);
        esp_http_client_set_header(notify_request, "SEQ", seq_buf);
        esp_http_client_set_header(notify_request, "SID",
                                   subscription_list[i].service[service_id].sid.uuid_s);
        esp_http_client_set_header(notify_request, "Server", SERVER_STR);
        esp_http_client_set_header(notify_request, "NTS", "upnp:propchange");
        esp_http_client_set_header(notify_request, "NT", "upnp:event");

        esp_http_client_perform(notify_request);
        free(buf);
        esp_http_client_cleanup(notify_request);
    }
    xSemaphoreGive(subscription_mutex);
}

void event_av_transport(const char* message) {
    send_state_change_event(AVTransport, message);
}

void event_rendering_control(const char* message) {
    send_state_change_event(RenderingControl, message);
}

extern char GetProtocolInfoEvent_start[] asm("_binary_GetProtocolInfoEvent_xml_start");
extern char GetProtocolInfoEvent_end[] asm("_binary_GetProtocolInfoEvent_xml_end");
void send_protocol_info(void) {
    xSemaphoreTake(subscription_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_SUBSCRIBERS && subscription_list[i].service[ConnectionManager].timeout != 0; i++) {
        if (subscription_list[i].service[ConnectionManager].seq == 0) {
            esp_http_client_config_t notify_config = {
                    .url = subscription_list[i].service[ConnectionManager].callback,
                    .method = HTTP_METHOD_NOTIFY,
                    .port = SERVER_PORT,
                    .keep_alive_enable = false,
                    .user_agent = USERAGENT_STR
            };
            esp_http_client_handle_t  notify_request = esp_http_client_init(&notify_config);
            esp_http_client_set_header(notify_request, "Content-Type", "text/xml; charset=\"utf-8\"");
            esp_http_client_set_post_field(notify_request, GetProtocolInfoEvent_start,
                                           (int) (GetProtocolInfoEvent_end - GetProtocolInfoEvent_start-1));

            char seq_buf[10];
            sprintf(seq_buf, "%d", subscription_list[i].service[ConnectionManager].seq++);
            esp_http_client_set_header(notify_request, "SEQ", seq_buf);
            esp_http_client_set_header(notify_request, "SID",
                                       subscription_list[i].service[ConnectionManager].sid.uuid_s);
            esp_http_client_set_header(notify_request, "Connection", "close");
            esp_http_client_set_header(notify_request, "Server", SERVER_STR);
            esp_http_client_set_header(notify_request, "NTS", "upnp:propchange");
            esp_http_client_set_header(notify_request, "NT", "upnp:event");

            esp_http_client_perform(notify_request);
            esp_http_client_cleanup(notify_request);
        }
    }
    xSemaphoreGive(subscription_mutex);
}

static void delete_subscriber(int subscriber_index, int service_id) {
    memset(&subscription_list[subscriber_index].service[service_id], 0, sizeof(struct subscription));
}

void eventing_clean_subscribers(void) {
    xSemaphoreTake(subscription_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        for (int j = 0; j < 3; j++) {
            if (subscription_list[i].service[j].timeout != 0 && subscription_list[i].service[j].timeout < xTaskGetTickCount()) {
                ESP_LOGI(TAG, "Timeout expired for subscriber with SID %s. Removing", subscription_list[i].service[j].sid.uuid_s);
                delete_subscriber(i, j);
            }
        }
    }
    xSemaphoreGive(subscription_mutex);
}

static bool get_header_value(httpd_req_t *req, char* name, char** value) {
    size_t buf_len = httpd_req_get_hdr_value_len(req, name) + 1;

    if (buf_len == 1)
        return false;

    *value = malloc(buf_len);
    httpd_req_get_hdr_value_str(req, name, *value, buf_len);

    return true;
}

static void add_subscriber(httpd_req_t *req, enum subscription_service service_id) {
    char* val_str = NULL;
    int i = 0;
    bool send_notify = false;

    eventing_clean_subscribers();
    xSemaphoreTake(subscription_mutex, portMAX_DELAY);
    // If callback is found, add new subscriber. Else, renew subscription
    if (get_header_value(req, "Callback", &val_str)) {
        // Find an open subscriber slot
        while (i < MAX_SUBSCRIBERS) {
            if (subscription_list[i].service[service_id].timeout == 0)
                break;
            i++;
        }

        if (i == MAX_SUBSCRIBERS) {
            ESP_LOGW(TAG, "No open subscription slot. Declining request");
            httpd_resp_send_500(req);
            goto end_func;
        }

        size_t len = strlen(val_str)-2;
        subscription_list[i].service[service_id].callback = malloc(len+1);
        memcpy(subscription_list[i].service[service_id].callback, val_str+1, len);
        subscription_list[i].service[service_id].callback[len] = '\0';
        generate_uuid(&subscription_list[i].service[service_id].sid);
        send_notify = true;

        ESP_LOGI(TAG, "Adding subscriber with SID %s", subscription_list[i].service[service_id].sid.uuid_s);
    } else if (get_header_value(req, "SID", &val_str)) {
        while (i < MAX_SUBSCRIBERS) {
            if (strcmp(subscription_list[i].service[service_id].sid.uuid_s, val_str) == 0)
                break;
            i++;
        }

        if (i == MAX_SUBSCRIBERS) {
            ESP_LOGI(TAG, "Subscriber SID not in list. Discarding subscription renewal request");
            httpd_resp_send_500(req);
            goto end_func;
        }

        ESP_LOGI(TAG, "Renewing subscriber with SID %s", val_str);
    } else {
        ESP_LOGW(TAG, "No callback or SID value in header. Declining subscription request");
        httpd_resp_send_404(req);
        goto end_func;
    }

    // Retrieve the header values
    char* timeout_str = NULL;
    int timeout = 0;
    if (get_header_value(req, "Timeout", &timeout_str) == false) {
        ESP_LOGW(TAG, "No Timeout value in header. Using default value of 1800");
        timeout = 1800;
    } else {
        char* end;
        timeout = strtol(timeout_str+7, &end, 10);
        if (timeout == 0) {
            ESP_LOGW(TAG, "Timeout formatting error. Using default value of 1800");
            timeout = 1800;
        }
        free(timeout_str);
    }

    subscription_list[i].service[service_id].timeout = xTaskGetTickCount() + (timeout*1000)/portTICK_PERIOD_MS;

    char timeout_resp[20];
    snprintf(timeout_resp, sizeof(timeout_resp), "Second-%d", timeout);
    httpd_resp_set_hdr(req, "Timeout", timeout_resp);
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_set_hdr(req, "Date", get_date());
    httpd_resp_set_hdr(req, "Server", SERVER_STR);
    httpd_resp_set_hdr(req, "SID", subscription_list[i].service[service_id].sid.uuid_s);
    xSemaphoreGive(subscription_mutex);

    httpd_resp_send(req, NULL, 0);
    if (send_notify) {
        switch (service_id) {
            case AVTransport:
                send_event(AV_TRANSPORT_SEND_ALL);
                break;
            case ConnectionManager:
                send_event(SEND_PROTOCOL_INFO);
                break;
            case RenderingControl:
                send_event(RENDERING_CONTROL_SEND_ALL);
                break;
            default:
                abort();
        }
    }

    end_func:
    free(val_str);
}

static void remove_subscriber(httpd_req_t *req, enum subscription_service service_id) {
    char* sid = NULL;
    if (get_header_value(req, "SID", &sid) == false) {
        ESP_LOGI(TAG, "No SID in unsubscribe request. Discarding");
        httpd_resp_send_404(req);
        return;
    }

    int i = 0;
    xSemaphoreTake(subscription_mutex, portMAX_DELAY);
    while (i < MAX_SUBSCRIBERS) {
        if (strcmp(subscription_list[i].service[service_id].sid.uuid_s, sid) == 0)
            break;
        i++;
    }

    if (i == MAX_SUBSCRIBERS) {
        ESP_LOGI(TAG, "Unsubscribe SID %s not in list. Discarding request", sid);
        httpd_resp_send_500(req);
        return;
    }

    ESP_LOGI(TAG, "Unsubscribe request with SID %s. Removing", sid);
    delete_subscriber(i, service_id);
    xSemaphoreGive(subscription_mutex);

    httpd_resp_send(req, NULL, 0);
}

static esp_err_t AVTransport_Subscribe_handler(httpd_req_t *req) {
    add_subscriber(req, AVTransport);

    return ESP_OK;
} static const httpd_uri_t AVTransport_Subscribe = {
        .uri = "/upnp/AVTransport/Event",
        .method = HTTP_SUBSCRIBE,
        .handler = AVTransport_Subscribe_handler
};

static esp_err_t ConnectionManager_Subscribe_handler(httpd_req_t *req) {
    add_subscriber(req, ConnectionManager);
    return ESP_OK;
} static const httpd_uri_t ConnectionManager_Subscribe = {
        .uri = "/upnp/ConnectionManager/Event",
        .method = HTTP_SUBSCRIBE,
        .handler = ConnectionManager_Subscribe_handler
};

static esp_err_t RenderingControl_Subscribe_handler(httpd_req_t *req) {
    add_subscriber(req, RenderingControl);
    return ESP_OK;
} static const httpd_uri_t RenderingControl_Subscribe = {
        .uri = "/upnp/RenderingControl/Event",
        .method = HTTP_SUBSCRIBE,
        .handler = RenderingControl_Subscribe_handler
};

static esp_err_t AVTransport_Unsubscribe_handler(httpd_req_t *req) {
    remove_subscriber(req, AVTransport);
    return ESP_OK;
} static const httpd_uri_t AVTransport_Unsubscribe = {
        .uri = "/upnp/AVTransport/Event",
        .method = HTTP_UNSUBSCRIBE,
        .handler = AVTransport_Unsubscribe_handler
};

static esp_err_t ConnectionManager_Unsubscribe_handler(httpd_req_t *req) {
    remove_subscriber(req, ConnectionManager);
    return ESP_OK;
} static const httpd_uri_t ConnectionManager_Unsubscribe = {
        .uri = "/upnp/ConnectionManager/Event",
        .method = HTTP_UNSUBSCRIBE,
        .handler = ConnectionManager_Unsubscribe_handler
};

static esp_err_t RenderingControl_Unsubscribe_handler(httpd_req_t *req) {
    remove_subscriber(req, RenderingControl);
    return ESP_OK;
} static const httpd_uri_t RenderingControl_Unsubscribe = {
        .uri = "/upnp/RenderingControl/Event",
        .method = HTTP_UNSUBSCRIBE,
        .handler = RenderingControl_Unsubscribe_handler
};

static void eventing_clean_subscribers_cb(TimerHandle_t self) {
    send_event( EVENTING_CLEAN_SUBSCRIBERS);
}

void start_eventing(httpd_handle_t server) {
    ESP_LOGI(TAG, "Starting eventing");
    memset(subscription_list, 0, sizeof(subscription_list));
    subscription_mutex = xSemaphoreCreateMutex();
    TimerHandle_t clean_subscriber_timer = xTimerCreate("Eventing Subscriber Timer", pdMS_TO_TICKS(SUBSCRIBER_REFRESH_MS), pdTRUE, NULL, eventing_clean_subscribers_cb);
    xTimerStart(clean_subscriber_timer, portMAX_DELAY);

    httpd_register_uri_handler(server, &AVTransport_Subscribe);
    httpd_register_uri_handler(server, &ConnectionManager_Subscribe);
    httpd_register_uri_handler(server, &RenderingControl_Subscribe);

    httpd_register_uri_handler(server, &AVTransport_Unsubscribe);
    httpd_register_uri_handler(server, &ConnectionManager_Unsubscribe);
    httpd_register_uri_handler(server, &RenderingControl_Unsubscribe);
}