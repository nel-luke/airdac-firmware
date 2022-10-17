#include "description.h"
#include "common.h"

#include <esp_log.h>

static const char *TAG = "upnp_description";

extern char rootDesc_start[] asm("_binary_rootDesc_xml_start");
extern char rootDesc_end[] asm("_binary_rootDesc_xml_end");
static char rootDesc_buf[3072];
static ssize_t rootDesc_bytes;

static void send_description(httpd_req_t *req,  char* buf, ssize_t buf_len) {
    httpd_resp_set_type(req, "text/xml; charset=\"utf-8\"");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_set_hdr(req, "Expires", " : 0");
    httpd_resp_send(req, buf, buf_len);
}

static esp_err_t rootDesc_handler(httpd_req_t *req)
{
    send_description(req, rootDesc_buf, rootDesc_bytes);
    return ESP_OK;
} static const httpd_uri_t rootDesc = {
        .uri = "/upnp/rootDesc.xml",
        .method = HTTP_GET,
        .handler = rootDesc_handler
};

extern char logo_start[] asm("_binary_logo_png_start");
extern char logo_end[] asm("_binary_logo_png_end");
static esp_err_t logo_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "image/png");
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=3600, must-revalidate");
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_set_hdr(req, "Expires", " : 0");
    uint32_t logo_len = logo_end - logo_start;
    httpd_resp_send(req, logo_start, logo_len);

    return ESP_OK;
} static const httpd_uri_t logo = {
        .uri = "/upnp/logo.png",
        .method = HTTP_GET,
        .handler = logo_handler
};

extern char AVTransport_start[] asm("_binary_AVTransport_xml_start");
extern char AVTransport_end[] asm("_binary_AVTransport_xml_end");
static esp_err_t AVTransport_handler(httpd_req_t *req)
{
    ssize_t len = AVTransport_end - AVTransport_start - 1;
    send_description(req, AVTransport_start, len);
    return ESP_OK;
} static const httpd_uri_t AVTransport = {
        .uri = "/upnp/AVTransport.xml",
        .method = HTTP_GET,
        .handler = AVTransport_handler
};

extern char ConnectionManager_start[] asm("_binary_ConnectionManager_xml_start");
extern char ConnectionManager_end[] asm("_binary_ConnectionManager_xml_end");
static esp_err_t ConnectionManager_handler(httpd_req_t *req)
{
    ssize_t len = ConnectionManager_end - ConnectionManager_start - 1;
    send_description(req, ConnectionManager_start, len);
    return ESP_OK;
} static const httpd_uri_t ConnectionManager = {
        .uri = "/upnp/ConnectionManager.xml",
        .method = HTTP_GET,
        .handler = ConnectionManager_handler
};

extern char RenderingControl_start[] asm("_binary_RenderingControl_xml_start");
extern char RenderingControl_end[] asm("_binary_RenderingControl_xml_end");
static esp_err_t RenderingControl_handler(httpd_req_t *req)
{
    ssize_t len = RenderingControl_end - RenderingControl_start - 1;
    send_description(req, RenderingControl_start, len);
    return ESP_OK;
} static const httpd_uri_t RenderingControl = {
        .uri = "/upnp/RenderingControl.xml",
        .method = HTTP_GET,
        .handler = RenderingControl_handler
};

void start_description(httpd_handle_t server, const char* friendly_name, const char* uuid, const char* ip_addr) {
    ESP_LOGI(TAG, "Starting description");
    rootDesc_bytes = snprintf(rootDesc_buf, sizeof(rootDesc_buf), rootDesc_start, friendly_name, uuid, ip_addr, SERVER_PORT);

    httpd_register_uri_handler(server, &rootDesc);
    httpd_register_uri_handler(server, &logo);

    httpd_register_uri_handler(server, &AVTransport);
    httpd_register_uri_handler(server, &ConnectionManager);
    httpd_register_uri_handler(server, &RenderingControl);
}