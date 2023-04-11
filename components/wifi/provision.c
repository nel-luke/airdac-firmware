#include "provision.h"
#include "connect.h"
#include "dns.h"

#include <ctype.h>

#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_http_server.h>

#include <freertos/FreeRTOS.h>
#include <freertos/message_buffer.h>

#include <lwip/inet.h>

#define SSID_PRE_LEN 5
#define PASSW_PRE_LEN 6

static const char TAG[] = "wifi_provision";

static MessageBufferHandle_t cred_buf;
static const int cred_buf_len = MAX_SSID_LEN + MAX_PASSPHRASE_LEN + SSID_PRE_LEN + PASSW_PRE_LEN + 1;

extern const char form_start[] asm("_binary_form_html_start");
extern const char form_end[] asm("_binary_form_html_end");

extern const char thank_you_start[] asm("_binary_thank_you_html_start");
extern const char thank_you_end[] asm("_binary_thank_you_html_end");

static void urlDecode(char *dStr) {

//    unsigned char *dStr = malloc(strlen(str) + 1);
    char eStr[] = "00"; /* for a hex code */

//    strcpy(dStr, str);
    unsigned int i; // the counter for the string

//    ESP_LOGI(TAG, "String is %s", dStr);
    for(i=0;i<strlen(dStr);++i) {

        if(dStr[i] == '%') {
            if(dStr[i+1] == 0)
                return;

            if(isxdigit((unsigned char)dStr[i+1]) && isxdigit((unsigned char)dStr[i+2])) {
                // combine the next two numbers into one
                eStr[0] = dStr[i+1];
                eStr[1] = dStr[i+2];

                // convert it to decimal
                unsigned int x = strtol(eStr, NULL, 16);
//                ESP_LOGI(TAG, "x is 0x%x", x);
                if (x == 0x92) x = '\'';

                // remove the hex
                memmove(&dStr[i+1], &dStr[i+3], strlen(&dStr[i+3])+1);
                dStr[i] = (unsigned char)x;
            }
        }
        else if(dStr[i] == '+') { dStr[i] = ' '; }
    }
}


static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

static void init_softap(const char* host_name)
{
    esp_netif_t *netif = esp_netif_create_default_wifi_ap();
    ESP_ERROR_CHECK(esp_netif_set_hostname(netif, host_name));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
            .ap = {
                    .ssid = "",
                    .ssid_len = strlen(host_name),
                    .max_connection = 5,
                    .authmode = WIFI_AUTH_OPEN
            },
    };
    strcpy((char*)wifi_config.ap.ssid, host_name);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);

    char ip_addr[16];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
    ESP_LOGI(TAG, "Set up softAP with IP: %s", ip_addr);
}

// HTTP GET Handler
static esp_err_t root_get_handler(httpd_req_t *req)
{
    const uint32_t form_len = form_end - form_start;

    ESP_LOGI(TAG, "Serve root");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, form_start, form_len);

    return ESP_OK;
}

static const httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler
};

// HTTP Error (404) Handler - Redirects all requests to the root page
static esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    httpd_resp_set_status(req, "302 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/");
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Redirecting to root");
    return ESP_OK;
}

static esp_err_t post_handler(httpd_req_t *req)
{
    char credentials[cred_buf_len];
    int ret, remaining = req->content_len;

    while (remaining > 0) {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, credentials, sizeof(credentials))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            ESP_LOGI(TAG, "Header too long!");
            return ESP_FAIL;
        }

        remaining -= ret;
        urlDecode(credentials);
        ESP_LOGI(TAG, "Sending credentials on buffer");
        xMessageBufferSend(cred_buf, credentials, ret, portMAX_DELAY);
    }

    // End response
    const uint32_t form_len = thank_you_end - thank_you_start;
    httpd_resp_send(req, thank_you_start, form_len);
    return ESP_OK;
}

static const httpd_uri_t send = {
        .uri       = "/",
        .method    = HTTP_POST,
        .handler   = post_handler,
        .user_ctx  = NULL
};

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 5;
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &send);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
    return server;
}

void wifi_get_credentials(bool poll_connected, const char* host_name, char* ssid, char* passphrase) {
    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd_parse", ESP_LOG_ERROR);

    init_softap(host_name);

    cred_buf = xMessageBufferCreate(cred_buf_len);
    assert(cred_buf != NULL);
    start_webserver();
    wifi_start_dns();

    ESP_LOGI(TAG, "Finished. Waiting for credentials.");
    char credentials[cred_buf_len];
    size_t len;
    do {
        len = xMessageBufferReceive(cred_buf, credentials, cred_buf_len, 1000 / portTICK_PERIOD_MS);

        if (poll_connected && wifi_poll_connected()) {
            ESP_LOGI(TAG, "WiFi connected with old credentials. Restarting system.");
            esp_restart();
        }
    } while (len == 0);
    char* passw_index = strstr(credentials, "passw=");
    size_t ssid_len = passw_index - credentials;
    size_t ssid_pre_len = SSID_PRE_LEN;
    size_t passw_pre_len = PASSW_PRE_LEN;

    strncpy(ssid, credentials+ssid_pre_len, ssid_len-SSID_PRE_LEN-1);
    strncpy(passphrase, (char*)(passw_index+passw_pre_len), (size_t)(len-ssid_len-passw_pre_len));

    ESP_LOGI(TAG, "ssid: %s - passw: %s", ssid, passphrase);
}