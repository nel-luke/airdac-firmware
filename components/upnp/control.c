#include "control.h"
#include "upnp_common.h"

#include <sys/param.h>
#include <esp_log.h>

static const char* TAG = "upnp_control";

static const char get_protocol_info_response[] =
        "<Source></Source>\r\n<Sink>"
        "http-get:*:*:*,"
        "http-get:*:audio/mp3:*,"
        "http-get:*:audio/basic:*,"
        "http-get:*:audio/ogg:*,"
        "http-get:*:audio/ac3:*,"
        "http-get:*:audio/aac:*,"
        "http-get:*:audio/vorbis:*,"
        "http-get:*:audio/flac:*,"
        "http-get:*:audio/x-flac:*,"
        "http-get:*:audio/x-wav:*"
        "</Sink>\r\n"
        ;

extern char SoapResponseOk_start[] asm("_binary_SoapResponseOk_xml_start");
extern char SoapResponseOk_end[] asm("_binary_SoapResponseOk_xml_end");
static void sendSoapOk(httpd_req_t *req, const char* service_name, const char* action_name, const char* message) {
    httpd_resp_set_type(req, "text/xml; charset=\"utf-8\"");
    httpd_resp_set_hdr(req, "Server", SERVER_STR);

    size_t buf_len = snprintf(NULL, 0, SoapResponseOk_start, action_name, service_name, message, action_name) + 1;
    char* buf = malloc(buf_len);
    sprintf(buf, SoapResponseOk_start, action_name, service_name, message, action_name);
    httpd_resp_sendstr(req, buf);
    free(buf);
}

static void getSoapAction(httpd_req_t *req, char* service_name, size_t service_name_len, char* action_name, size_t action_name_len) {
    //    SOAPAction: "urn:schemas-upnp-org:service:ConnectionManager:1#GetProtocolInfo"
    size_t buf_len = httpd_req_get_hdr_value_len(req, "SOAPAction")+1;
    char* buf = malloc(buf_len);
    httpd_req_get_hdr_value_str(req, "SOAPAction", buf, buf_len);

    char* pos = strtok(buf+29, ":");
    memcpy(service_name, pos, MIN(service_name_len, buf_len-(size_t)(pos-buf)-1));
    service_name[service_name_len-1] = '\0';

    pos = strtok(NULL, "1#");
    memcpy(action_name, pos, MIN(action_name_len, buf_len-(size_t)(pos-buf)-2));
    action_name[action_name_len-1] = '\0';
    free(buf);
}

static esp_err_t AVTransport_Control_handler(httpd_req_t *req) {
    char service_name[] = "AVTransport";
    char action_name[25];

    getSoapAction(req, service_name, sizeof(service_name), action_name, sizeof(action_name));

    if (strcmp(service_name, "AVTransport") != 0) {
        ESP_LOGI(TAG, "SOAP service name mismatch. Discarding");
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Action %s not implemented yet", action_name);

    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
} static const httpd_uri_t AVTransport_Control = {
        .uri = "/upnp/AVTransport/Control",
        .method = HTTP_POST,
        .handler = AVTransport_Control_handler
};

static esp_err_t ConnectionManager_Control_handler(httpd_req_t *req) {
    char service_name[] = "ConnectionManager";
    char action_name[25];

    getSoapAction(req, service_name, sizeof(service_name), action_name, sizeof(action_name));

    if (strcmp(service_name, "ConnectionManager") != 0) {
        ESP_LOGI(TAG, "SOAP service name mismatch. Discarding");
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    if (strcmp(action_name, "GetProtocolInfo") == 0) {
        ESP_LOGI(TAG, "Sending protocol info!");

        sendSoapOk(req, service_name, action_name, get_protocol_info_response);
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "Action %s not implemented yet", action_name);
    }

    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
} static const httpd_uri_t ConnectionManager_Control = {
        .uri = "/upnp/ConnectionManager/Control",
        .method = HTTP_POST,
        .handler = ConnectionManager_Control_handler
};

static esp_err_t RenderingControl_Control_handler(httpd_req_t *req) {
    char service_name[] = "RenderingControl";
    char action_name[25];

    getSoapAction(req, service_name, sizeof(service_name), action_name, sizeof(action_name));

    if (strcmp(service_name, "RenderingControl") != 0) {
        ESP_LOGI(TAG, "SOAP service name mismatch. Discarding");
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Action %s not implemented yet", action_name);

    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
} static const httpd_uri_t RenderingControl_Control = {
        .uri = "/upnp/RenderingControl/Control",
        .method = HTTP_POST,
        .handler = RenderingControl_Control_handler
};

void start_control(httpd_handle_t server) {
    ESP_LOGI(TAG, "Starting control");

    httpd_register_uri_handler(server, &AVTransport_Control);
    httpd_register_uri_handler(server, &ConnectionManager_Control);
    httpd_register_uri_handler(server, &RenderingControl_Control);
}