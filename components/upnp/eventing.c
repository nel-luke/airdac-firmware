#include "eventing.h"
#include "common.h"
#include "uuid.h"

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

#include <esp_log.h>

#define SUBSCRIBER_REFRESH_MS 900000

static const char *TAG = "upnp_eventing";
static TimerHandle_t clean_subscriber_timer;

struct subscription {
    TickType_t timeout;
    char* callback;
    uuid_t sid;
};

#define MAX_SUBSCRIBERS 2
enum subscription_service { AVTransport, ConnectionManager, RenderingControl };
struct {
    struct subscription service[3];
} subscription_list[MAX_SUBSCRIBERS];

static void delete_subscriber(int subscriber_index, int service_id) {
    memset(&subscription_list[subscriber_index].service[service_id], 0, sizeof(struct subscription));
}

static void clean_subscribers() {
    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        for (int j = 0; j < 3; j++) {
            if (subscription_list[i].service[j].timeout != 0 && subscription_list[i].service[j].timeout < xTaskGetTickCount()) {
                ESP_LOGI(TAG, "Subscriber timeout expired. Removing");
                delete_subscriber(i, j);
            }
        }
    }
}

static bool get_header_value(httpd_req_t *req, char* name, char** value) {
    size_t buf_len = httpd_req_get_hdr_value_len(req, name) + 1;

    if (buf_len == 0)
        return false;

    *value = malloc(buf_len);
    httpd_req_get_hdr_value_str(req, name, *value, buf_len);

    return true;
}

static void add_subscriber(httpd_req_t *req, enum subscription_service service_id) {
    xTimerStop(clean_subscriber_timer, 10);
    // If callback is found, add new subscriber. Else, renew subscription
    char* val_str = NULL;
    int i = 0;
    if (get_header_value(req, "Callback", &val_str)) {
        // Find an open subscriber slot
        clean_subscribers();
        while (i < MAX_SUBSCRIBERS) {
            if (subscription_list[i].service[service_id].timeout == 0)
                break;
            i++;
        }

        if (i > MAX_SUBSCRIBERS) {
            ESP_LOGW(TAG, "No open subscription slot. Declining request");
            httpd_resp_send_500(req);
            goto end_func;
        }

        size_t len = strlen(val_str)-2;
        subscription_list[i].service[service_id].callback = malloc(len+1);
        memcpy(subscription_list[i].service[service_id].callback, val_str+1, len);
        subscription_list[i].service[service_id].callback[len] = '\0';
        generate_uuid(&subscription_list[i].service[service_id].sid);
    } else if (get_header_value(req, "SID", &val_str)) {
        while (i < MAX_SUBSCRIBERS) {
            if (strcmp(subscription_list[i].service[service_id].sid.uuid_s, val_str) == 0)
                break;
            i++;
        }

        if (i > MAX_SUBSCRIBERS) {
            ESP_LOGI(TAG, "Unsubscribe SID not in list. Discarding request");
            httpd_resp_send_500(req);
            goto end_func;
        }
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
    httpd_resp_set_hdr(req, "Server", SERVER_STR);
    httpd_resp_set_hdr(req, "SID", subscription_list[i].service[service_id].sid.uuid_s);

    httpd_resp_send(req, NULL, 0);
    ESP_LOGI(TAG, "Subscriber added!");

    end_func:
    free(val_str);
    xTimerReset(clean_subscriber_timer, 10);
}

static void remove_subscriber(httpd_req_t *req, enum subscription_service service_id) {
    xTimerStop(clean_subscriber_timer, 10);
    char* sid = NULL;
    if (get_header_value(req, "SID", &sid) == false) {
        ESP_LOGI(TAG, "No SID in unsubscribe request. Discarding");
        httpd_resp_send_404(req);
        goto end_func;
    }

    int i = 0;
    while (i < MAX_SUBSCRIBERS) {
        if (strcmp(subscription_list[i].service[service_id].sid.uuid_s, sid) == 0)
            break;
        i++;
    }

    if (i > MAX_SUBSCRIBERS) {
        ESP_LOGI(TAG, "Unsubscribe SID not in list. Discarding request");
        httpd_resp_send_500(req);
        goto end_func;
    }

    delete_subscriber(i, service_id);
    httpd_resp_send(req, NULL, 0);
    ESP_LOGI(TAG, "Removed subscriber");

    end_func:
    xTimerReset(clean_subscriber_timer, 10);
}

static esp_err_t AVTransport_Subscribe_handler(httpd_req_t *req) {
    add_subscriber(req, AVTransport);
    return ESP_OK;
} const httpd_uri_t AVTransport_Subscribe = {
        .uri = "/upnp/AVTransport/Event",
        .method = HTTP_SUBSCRIBE,
        .handler = AVTransport_Subscribe_handler
};

static esp_err_t ConnectionManager_Subscribe_handler(httpd_req_t *req) {
    add_subscriber(req, ConnectionManager);
    return ESP_OK;
} const httpd_uri_t ConnectionManager_Subscribe = {
        .uri = "/upnp/ConnectionManager/Event",
        .method = HTTP_SUBSCRIBE,
        .handler = ConnectionManager_Subscribe_handler
};

static esp_err_t RenderingControl_Subscribe_handler(httpd_req_t *req) {
    add_subscriber(req, RenderingControl);
    return ESP_OK;
} const httpd_uri_t RenderingControl_Subscribe = {
        .uri = "/upnp/RenderingControl/Event",
        .method = HTTP_SUBSCRIBE,
        .handler = RenderingControl_Subscribe_handler
};

static esp_err_t AVTransport_Unsubscribe_handler(httpd_req_t *req) {
    remove_subscriber(req, AVTransport);
    return ESP_OK;
} const httpd_uri_t AVTransport_Unsubscribe = {
        .uri = "/upnp/AVTransport/Event",
        .method = HTTP_UNSUBSCRIBE,
        .handler = AVTransport_Unsubscribe_handler
};

static esp_err_t ConnectionManager_Unsubscribe_handler(httpd_req_t *req) {
    remove_subscriber(req, ConnectionManager);
    return ESP_OK;
} const httpd_uri_t ConnectionManager_Unsubscribe = {
        .uri = "/upnp/ConnectionManager/Event",
        .method = HTTP_UNSUBSCRIBE,
        .handler = ConnectionManager_Unsubscribe_handler
};

static esp_err_t RenderingControl_Unsubscribe_handler(httpd_req_t *req) {
    remove_subscriber(req, RenderingControl);
    return ESP_OK;
} const httpd_uri_t RenderingControl_Unsubscribe = {
        .uri = "/upnp/RenderingControl/Event",
        .method = HTTP_UNSUBSCRIBE,
        .handler = RenderingControl_Unsubscribe_handler
};

void start_eventing(httpd_handle_t server) {
    ESP_LOGI(TAG, "Starting eventing");
    memset(subscription_list, 0, sizeof(subscription_list));
    clean_subscriber_timer = xTimerCreate("Eventing Subscriber Timer", pdMS_TO_TICKS(SUBSCRIBER_REFRESH_MS), pdTRUE, NULL, clean_subscribers);
    xTimerStart(clean_subscriber_timer, 0);

    httpd_register_uri_handler(server, &AVTransport_Subscribe);
    httpd_register_uri_handler(server, &ConnectionManager_Subscribe);
    httpd_register_uri_handler(server, &RenderingControl_Subscribe);

    httpd_register_uri_handler(server, &AVTransport_Unsubscribe);
    httpd_register_uri_handler(server, &ConnectionManager_Unsubscribe);
    httpd_register_uri_handler(server, &RenderingControl_Unsubscribe);
}