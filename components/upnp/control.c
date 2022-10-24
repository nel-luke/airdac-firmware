#include "control.h"
#include "control/av_transport.h"
#include "control/connection_manager.h"
#include "control/rendering_control.h"
#include "control/control_common.h"
#include "upnp_common.h"

#include <sys/param.h>
#include <esp_log.h>

static const char* TAG = "upnp_control";

static void sendSoap(httpd_req_t *req, const char* buf) {
    httpd_resp_set_hdr(req, "EXT", "");
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_set_type(req, "text/xml; charset=\"utf-8\"");
    httpd_resp_set_hdr(req, "Server", server_STR);
    httpd_resp_set_hdr(req, "Date", get_date());
    httpd_resp_set_hdr(req, "User-Agent", useragent_STR);
    httpd_resp_sendstr(req, buf);
}

extern char SoapResponseOk_start[] asm("_binary_SoapResponseOk_xml_start");
extern char SoapResponseOk_end[] asm("_binary_SoapResponseOk_xml_end");
static void sendSoapOk(httpd_req_t *req, const char* service_name, const char* action_name, const char* message) {
    const char* mp = message == NULL ? "" : message;
    size_t buf_len = snprintf(NULL, 0, SoapResponseOk_start, action_name, service_name, mp, action_name);

    char* buf = malloc(buf_len+1);
    assert(buf != NULL);
    sprintf(buf, SoapResponseOk_start, action_name, service_name, mp, action_name);
    sendSoap(req, buf);
    free(buf);
}
extern char SoapResponseErr_start[] asm("_binary_SoapResponseErr_xml_start");
extern char SoapResponseErr_end[] asm("_binary_SoapResponseErr_xml_end");
static void sendSoapError(httpd_req_t *req, action_err_t error) {
    size_t buf_len = snprintf(NULL, 0, SoapResponseErr_start, action_err_d[error].code, action_err_d[error].str);

    char* buf = malloc(buf_len+1);
    sprintf(buf, SoapResponseErr_start, action_err_d[error].code, action_err_d[error].str);
    httpd_resp_set_status(req, "500 Internal Server Error");
    sendSoap(req, buf);
//    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, NULL);
    free(buf);
}

static action_err_t getSoapAction(httpd_req_t *req, const char* service_name, size_t service_name_len,
                          char* action_name, size_t action_name_len, char** arguments) {
    //    SOAPAction: "urn:schemas-upnp-org:service:ConnectionManager:1#GetProtocolInfo"
    size_t buff_len = httpd_req_get_hdr_value_len(req, "SOAPAction");
    if (buff_len == 0) {
        ESP_LOGW(TAG, "Received request for %s with no SOAPAction header. Discarding", service_name);
        return Invalid_Action;
    }

    char* buff = malloc(buff_len+1);
    httpd_req_get_hdr_value_str(req, "SOAPAction", buff, buff_len);

    char* pos = strtok(buff + 29, ":");

    if (strcmp(pos, service_name) != 0) {
        ESP_LOGW(TAG, "SOAP service name mismatch (%s should be %s). Discarding", pos, service_name);
        return Invalid_Action;
    }

    pos = strtok(NULL, "\"");
    memcpy(action_name, pos+2, MIN(action_name_len-1, strlen(pos)));
    action_name[action_name_len-1] = '\0';
    free(buff);

    if (req->content_len < 200)
        goto parse_error;

    buff = malloc(req->content_len+1);
    pos = buff;
    int written = 0;
    do {
retry_write:
        written = httpd_req_recv(req, pos, buff_len);
        pos += written;
    } while (written > 0);

    if (written == HTTPD_SOCK_ERR_TIMEOUT) {
        ESP_LOGW(TAG, "Received timeout (%s | %s). Continuing", action_name, service_name);
        goto retry_write;
    } else if (written == HTTPD_SOCK_ERR_FAIL) {
        ESP_LOGW(TAG, "Socket failure (%s | %s). Closing connection", action_name, service_name);
        free(buff);
        return Socket_Failure;
    }
    buff[req->content_len] = '\0';

    char* arg_start = strstr(buff+100, ":Body>");
    if (arg_start == NULL)
        goto parse_error;

    arg_start = strstr(arg_start+6, ">");
    if (arg_start == NULL) {
parse_error:
        ESP_LOGW(TAG, "SOAPAction parse error (%s | %s). Discarding request", action_name, service_name);
        free(buff);
        return Invalid_Args;
    }

    arg_start++;
    char* arg_end = strstr(arg_start, ":Body>");

    if (arg_end == NULL)
        goto parse_error;

    while (*(--arg_end) != '<')
        ;
    while (*(--arg_end) != '<')
        ;

    if (arg_end <= arg_start)
        goto exit;

    *arg_end = '\0';

    *arguments = malloc(strlen(arg_start)+1);
    strcpy(*arguments, arg_start);
exit:
    free(buff);
    return Action_OK;
}

static esp_err_t AVTransport_Control_handler(httpd_req_t *req) {
    const char service_name[] = "AVTransport";
    char action_name[26] = "";

    char* arguments = NULL;
    action_err_t err = getSoapAction(req, service_name, sizeof(service_name), action_name, sizeof(action_name), &arguments);

    switch (err) {
        case Action_OK:
            break;
        case Socket_Failure:
            return ESP_FAIL;
        default:
            sendSoapError(req, err);
            return ESP_OK;
    }

    char* response = NULL;
    err = av_transport_execute(action_name, arguments, &response);

    switch (err) {
        case Action_OK:
            sendSoapOk(req, service_name, action_name, response);
            free(response);
            break;
        case Invalid_Action:
            ESP_LOGW(TAG, "Action %s of %s not implemented yet", action_name, service_name);
            __attribute__((fallthrough));
        default:
            sendSoapError(req, err);
    }

    free(arguments);
    return ESP_OK;
} static const httpd_uri_t AVTransport_Control = {
        .uri = "/upnp/AVTransport/Control",
        .method = HTTP_POST,
        .handler = AVTransport_Control_handler
};

static esp_err_t ConnectionManager_Control_handler(httpd_req_t *req) {
    char service_name[] = "ConnectionManager";
    char action_name[26] = "";

    char* arguments = NULL;
    action_err_t err = getSoapAction(req, service_name, sizeof(service_name), action_name, sizeof(action_name), &arguments);

    switch (err) {
        case Action_OK:
            break;
        case Socket_Failure:
            return ESP_FAIL;
        default:
            sendSoapError(req, err);
            return ESP_OK;
    }

    char* response = NULL;
    err = connection_manager_execute(action_name, arguments, &response);

    switch (err) {
        case Action_OK:
            sendSoapOk(req, service_name, action_name, response);
//            free(response); // All responses here are static and should NOT be freed!
            break;
        case Invalid_Action:
            ESP_LOGW(TAG, "Action %s of %s not implemented yet", action_name, service_name);
            __attribute__((fallthrough));
        default:
            sendSoapError(req, err);
    }

    free(arguments);
    return ESP_OK;
} static const httpd_uri_t ConnectionManager_Control = {
        .uri = "/upnp/ConnectionManager/Control",
        .method = HTTP_POST,
        .handler = ConnectionManager_Control_handler
};

static esp_err_t RenderingControl_Control_handler(httpd_req_t *req) {
    char service_name[] = "RenderingControl";
    char action_name[26] = "";

    char* arguments = NULL;
    action_err_t err = getSoapAction(req, service_name, sizeof(service_name), action_name, sizeof(action_name), &arguments);

    switch (err) {
        case Action_OK:
            break;
        case Socket_Failure:
            return ESP_FAIL;
        default:
            sendSoapError(req, err);
            return ESP_OK;
    }

    char* response = NULL;
    err = rendering_control_execute(action_name, arguments, &response);

    switch (err) {
        case Action_OK:
            sendSoapOk(req, service_name, action_name, response);
            free(response);
            break;
        case Invalid_Action:
            ESP_LOGW(TAG, "Action %s of %s not implemented yet", action_name, service_name);
            __attribute__((fallthrough));
        default:
            sendSoapError(req, err);
    }

    free(arguments);
    return ESP_OK;
} static const httpd_uri_t RenderingControl_Control = {
        .uri = "/upnp/RenderingControl/Control",
        .method = HTTP_POST,
        .handler = RenderingControl_Control_handler
};

void start_control(httpd_handle_t server) {
    ESP_LOGI(TAG, "Starting control");
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);

    httpd_register_uri_handler(server, &AVTransport_Control);
    httpd_register_uri_handler(server, &ConnectionManager_Control);
    httpd_register_uri_handler(server, &RenderingControl_Control);

    init_av_transport();
    init_connection_manager();
    init_rendering_control();
}