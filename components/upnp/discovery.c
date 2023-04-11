#include "discovery.h"
#include "upnp_common.h"

#include <string.h>
#include <sys/param.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <esp_log.h>

#include <lwip/sockets.h>

#define SSDP_MULTICAST_ADDR_IPV4 "239.255.255.250"
#define SSDP_MULTICAST_PORT 1900

static const char *TAG = "upnp_discovery";

static struct {
    int sockp;
    const char* uuid;
    const char* ip_addr;
    fd_set set;
    struct sockaddr sender;
    size_t sender_length;
    struct timeval tv;
    struct sockaddr_in groupSock;
} service_discovery_vars;

static const char notify_fmt[] =
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s/upnp/rootDesc.xml\r\n"
        "NT: %s\r\n"
        "NTS: ssdp:alive\r\n"
        "SERVER: "SERVER_STR"\r\n"
        "USN: %s\r\n"
        "\r\n"
        ;

static const char root_device_nt1[] = "upnp:rootdevice";
static const char root_device_nt3[] = "urn:schemas-upnp-org:device:MediaRenderer:1";

enum ServiceType { RenderingControl, ConnectionManager, AVTransport };
static const char service_rendering_control[] = "urn:schemas-upnp-org:service:RenderingControl:1";
static const char service_connection_manager[] = "urn:schemas-upnp-org:service:ConnectionManager:1";
static const char service_av_transport[] = "urn:schemas-upnp-org:service:AVTransport:1";

static const char msearch_resp_fmt[] =
        "HTTP/1.1 200 OK\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "EXT:\r\n"
        "LOCATION: http://%s/upnp/rootDesc.xml\r\n"
        "SERVER: "SERVER_STR"\r\n"
        "ST: %s\r\n"
        "USN: %s\r\n"
        "\r\n"
        ;

//    char (*__kaboom)[sizeof( notify_message )] = 1;
static char send_buf[600];
static char nt_string[60];
static char usn_string[90];

static char rec_buf[500];

static inline void send_root_device_1(const char* fmt, struct sockaddr* send_to, socklen_t len) {
    snprintf(usn_string, sizeof(usn_string), "%s::%s", service_discovery_vars.uuid, root_device_nt1);
    snprintf(send_buf, sizeof(send_buf), fmt, service_discovery_vars.ip_addr,
             root_device_nt1, usn_string);
    sendto(service_discovery_vars.sockp, send_buf, strlen(send_buf), 0,
           send_to, len);
}

static inline void send_root_device_2(const char* fmt, struct sockaddr* send_to, socklen_t len) {
    snprintf(nt_string, sizeof(nt_string), "%s", service_discovery_vars.uuid);
    snprintf(send_buf, sizeof(send_buf), fmt, service_discovery_vars.ip_addr,
             nt_string, nt_string);
    sendto(service_discovery_vars.sockp, send_buf, strlen(send_buf), 0,
           send_to, len);
}

static inline void send_root_device_3(const char* fmt, struct sockaddr* send_to, socklen_t len) {
    snprintf(usn_string, sizeof(usn_string), "%s::%s", service_discovery_vars.uuid, root_device_nt3);
    snprintf(send_buf, sizeof(send_buf), fmt, service_discovery_vars.ip_addr,
             root_device_nt3, usn_string);
    sendto(service_discovery_vars.sockp, send_buf, strlen(send_buf), 0,
           send_to, len);
}

static void send_service(const char* fmt, const enum ServiceType num, struct sockaddr* send_to, socklen_t len) {
    switch (num) {
        case RenderingControl:
            snprintf(usn_string, sizeof(usn_string), "%s::%s", service_discovery_vars.uuid, service_rendering_control);
            snprintf(send_buf, sizeof(send_buf), fmt, service_discovery_vars.ip_addr,
                     service_rendering_control, usn_string);
            break;
        case ConnectionManager:
            snprintf(usn_string, sizeof(usn_string), "%s::%s", service_discovery_vars.uuid, service_connection_manager);
            snprintf(send_buf, sizeof(send_buf), fmt, service_discovery_vars.ip_addr,
                     service_connection_manager, usn_string);
            break;
        case AVTransport:
            snprintf(usn_string, sizeof(usn_string), "%s::%s", service_discovery_vars.uuid, service_av_transport);
            snprintf(send_buf, sizeof(send_buf), fmt, service_discovery_vars.ip_addr,
                     service_av_transport, usn_string);
            break;
        default:
            abort(); // Should never happen
    };
    sendto(service_discovery_vars.sockp, send_buf, strlen(send_buf), 0,
           send_to, len);
}

static void send_all(const char* fmt, struct sockaddr* send_to, socklen_t len) {
    send_root_device_1(fmt, send_to, len);
    send_root_device_2(fmt, send_to, len);
    send_root_device_3(fmt, send_to, len);
    send_service(fmt, RenderingControl, send_to, len);
    send_service(fmt, ConnectionManager, send_to, len);
    send_service(fmt, AVTransport, send_to, len);
}

void discovery_send_notify() {
    // Initial random delay
    int init_delay = esp_random() % 100;
    vTaskDelay(init_delay / portTICK_PERIOD_MS);

    // Send messages three times in a row
    for (int i = 0; i < 3; i++) {
        send_all(notify_fmt, (struct sockaddr*)&service_discovery_vars.groupSock, sizeof(service_discovery_vars.groupSock));
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

static void handle_msearch_message(void) {
    char *mx_start = strstr(rec_buf, "MX: ");

    if (mx_start == NULL)
        return;

    uint8_t mx = MIN((int) (*(mx_start + 4) - '0'), 5);
    uint32_t random_wait_msec = esp_random() % (mx * 1000);
    vTaskDelay(random_wait_msec / portTICK_PERIOD_MS);

    char *st_start = strstr(rec_buf, "ST: ");
    if (strstr(st_start, "ssdp:all") != NULL) {
        send_all(msearch_resp_fmt, &service_discovery_vars.sender, sizeof(service_discovery_vars.sender));
    } else if (strstr(st_start, root_device_nt1) != NULL) {
        send_root_device_1(msearch_resp_fmt, &service_discovery_vars.sender, sizeof(service_discovery_vars.sender));
    } else if (strstr(st_start, service_discovery_vars.uuid) != NULL) {
        send_root_device_2(msearch_resp_fmt, &service_discovery_vars.sender, sizeof(service_discovery_vars.sender));
    } else if (strstr(st_start, root_device_nt3) != NULL) {
        send_root_device_3(msearch_resp_fmt, &service_discovery_vars.sender, sizeof(service_discovery_vars.sender));
    } else if (strstr(st_start, service_av_transport) != NULL) {
        send_service(msearch_resp_fmt, AVTransport, &service_discovery_vars.sender, sizeof(service_discovery_vars.sender));
    } else if (strstr(st_start, service_connection_manager) != NULL) {
        send_service(msearch_resp_fmt, ConnectionManager, &service_discovery_vars.sender, sizeof(service_discovery_vars.sender));
    } else if (strstr(st_start, service_rendering_control) != NULL) {
        send_service(msearch_resp_fmt, RenderingControl, &service_discovery_vars.sender, sizeof(service_discovery_vars.sender));
    } else {
        ESP_LOGV(TAG, "Unknown ST. Discarding");
    }

}

void service_discovery(void) {
    FD_SET(service_discovery_vars.sockp, &service_discovery_vars.set);
    select(service_discovery_vars.sockp + 1, &service_discovery_vars.set, NULL, NULL, &service_discovery_vars.tv);

    if (FD_ISSET(service_discovery_vars.sockp, &service_discovery_vars.set)) {
        recvfrom(service_discovery_vars.sockp, rec_buf, sizeof(rec_buf), 0, &service_discovery_vars.sender, &service_discovery_vars.sender_length);
        if (strstr(rec_buf, "M-SEARCH * HTTP/1.1") != NULL) {
            ESP_LOGV(TAG, "MSEARCH message received!");
            handle_msearch_message();
        } else if (strstr(rec_buf, "MediaServer") != NULL) {
            //send_notify();
        } else {
            ESP_LOGV(TAG, "Message discarded");
        }
        memset(rec_buf, 0, sizeof(rec_buf));
    }
}

static void discovery_send_notify_cb(TimerHandle_t self) {
    flag_event(DISCOVERY_SEND_NOTIFY);
}

void start_discovery(const char* ip_addr, const char* uuid) {
    ESP_LOGI(TAG, "Starting discovery");
    // Save IP address string for later use
    service_discovery_vars.ip_addr = ip_addr;
    service_discovery_vars.uuid = uuid;

    // Create socket
    const int udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    assert(udpSocket >= 0); // Should never happen

    // Disable loopback
    char enable_loopback = 0;
    setsockopt(udpSocket, IPPROTO_IP, IP_MULTICAST_LOOP, &enable_loopback, sizeof(enable_loopback));

    struct sockaddr_in localSock = {
            .sin_family = AF_INET,
            .sin_addr.s_addr = INADDR_ANY,
            .sin_port = htons(SSDP_MULTICAST_PORT)
    };

    // Set Listening socket
    bind(udpSocket, (struct sockaddr*)&localSock, sizeof(localSock));

    struct ip_mreq group = {
            .imr_interface.s_addr = INADDR_ANY,
            .imr_multiaddr.s_addr = inet_addr(SSDP_MULTICAST_ADDR_IPV4),
    };

    // Register to multicast group
    setsockopt(udpSocket, IPPROTO_IP, IP_DROP_MEMBERSHIP, &group, sizeof(group));
    setsockopt(udpSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &group, sizeof(group));

    struct sockaddr_in groupSock = {
            .sin_family = AF_INET,
            .sin_addr.s_addr = inet_addr(SSDP_MULTICAST_ADDR_IPV4),
            .sin_port = htons(SSDP_MULTICAST_PORT)
    };
    memcpy(&service_discovery_vars.groupSock, &groupSock, sizeof(groupSock));
    service_discovery_vars.sockp = udpSocket;

    discovery_send_notify();
    TimerHandle_t notify_timer = xTimerCreate("uPnP Discovery Send Notify", pdMS_TO_TICKS(900000), pdTRUE, NULL, discovery_send_notify_cb);
    xTimerStart(notify_timer, portMAX_DELAY);

    FD_ZERO(&service_discovery_vars.set);

    service_discovery_vars.sender_length = sizeof(service_discovery_vars.sender);
    service_discovery_vars.tv.tv_sec = 0;
    service_discovery_vars.tv.tv_usec = 50000;
}