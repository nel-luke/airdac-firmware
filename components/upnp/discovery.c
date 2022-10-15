#include "discovery.h"

#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sockets.h"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define ESP_VER_STR STR(ESP_IDF_VERSION_MAJOR) "." STR(ESP_IDF_VERSION_MINOR) "." STR(ESP_IDF_VERSION_PATCH)

#define SSDP_MULTICAST_ADDR_IPV4 "239.255.255.250"
#define SSDP_MULTICAST_PORT 1900

static const char *TAG = "upnp_discovery";

static struct {
    int sockp;
    const char* uuid;
    const char* ip_addr;
    int boot_id;
    struct sockaddr_in groupSock;
    TaskHandle_t handle;
} discovery_info;

static const char notify_fmt[] =
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age = 1800\r\n"
        "LOCATION: http://%s/upnp/rootDesc.xml\r\n"
        "NT: %s\r\n"
        "NTS: ssdp:alive\r\n"
        "SERVER: esp-idf/" ESP_VER_STR "UPnP/1.0 AirDAC/1.0\r\n"
        "USN: %s\r\n"
        "BOOTID.UPNP.ORG: %d\r\n"
        "CONFIGID.UPNP.ORG: 1\r\n"
        "\r\n";

static const char root_device_nt1[] = "upnp:rootdevice";
static const char root_device_nt3[] = "urn:schemas-upnp-org:device:MediaRenderer:1";

enum ServiceType { RenderingControl, ConnectionManager, AVTransport };
static const char service_rendering_control[] = "urn:schemas-upnp-org:service:RenderingControl:1";
static const char service_connection_manager[] = "urn:schemas-upnp-org:service:ConnectionManager:1";
static const char service_av_transport[] = "urn:schemas-upnp-org:service:AVTransport:1";

static const char msearch_resp_fmt[] =
        "HTTP/1.1 200 OK\r\n"
        "CACHE-CONTROL: max-age = 1800\r\n"
        "EST:\r\n"
        "LOCATION: http://%s/upnp/rootDesc.xml\r\n"
        "SERVER: esp-idf/" ESP_VER_STR "UPnP/1.0 AirDAC/1.0\r\n"
        "ST: %s\r\n"
        "USN: %s\r\n"
        "BOOTID.UPNP.ORG: %d\r\n"
        "CONFIGID.UPNP.ORG: 1\r\n"
        "\r\n";

//    char (*__kaboom)[sizeof( notify_message )] = 1;
static char send_buf[600];
static char nt_string[60];
static char usn_string[90];

static char rec_buf[500];

static inline void send_root_device_1(const char* fmt) {
    snprintf(usn_string, sizeof(usn_string), "%s::%s", discovery_info.uuid, root_device_nt1);
    snprintf(send_buf, sizeof(send_buf), fmt, discovery_info.ip_addr,
             root_device_nt1, usn_string, discovery_info.boot_id);
    sendto(discovery_info.sockp, send_buf, strlen(send_buf), 0,
           (struct sockaddr*)&discovery_info.groupSock, sizeof(discovery_info.groupSock));
}

static inline void send_root_device_2(const char* fmt) {
    snprintf(nt_string, sizeof(nt_string), "uuid:%s", discovery_info.uuid);
    snprintf(send_buf, sizeof(send_buf), fmt, discovery_info.ip_addr,
             nt_string, nt_string, discovery_info.boot_id);
    sendto(discovery_info.sockp, send_buf, strlen(send_buf), 0,
           (struct sockaddr*)&discovery_info.groupSock, sizeof(discovery_info.groupSock));
}

static inline void send_root_device_3(const char* fmt) {
    snprintf(usn_string, sizeof(usn_string), "uuid:%s::%s", discovery_info.uuid, root_device_nt3);
    snprintf(send_buf, sizeof(send_buf), fmt, discovery_info.ip_addr,
             root_device_nt3, usn_string, discovery_info.boot_id);
    sendto(discovery_info.sockp, send_buf, strlen(send_buf), 0,
           (struct sockaddr*)&discovery_info.groupSock, sizeof(discovery_info.groupSock));
}

static inline void send_service(const char* fmt, const enum ServiceType num) {
    switch (num) {
        case RenderingControl:
            snprintf(usn_string, sizeof(usn_string), "uuid:%s::%s", discovery_info.uuid, service_rendering_control);
            snprintf(send_buf, sizeof(send_buf), fmt, discovery_info.ip_addr,
                     service_rendering_control, usn_string, discovery_info.boot_id);
            break;
        case ConnectionManager:
            snprintf(usn_string, sizeof(usn_string), "uuid:%s::%s", discovery_info.uuid, service_connection_manager);
            snprintf(send_buf, sizeof(send_buf), fmt, discovery_info.ip_addr,
                     service_connection_manager, usn_string, discovery_info.boot_id);
            break;
        case AVTransport:
            snprintf(usn_string, sizeof(usn_string), "uuid:%s::%s", discovery_info.uuid, service_av_transport);
            snprintf(send_buf, sizeof(send_buf), fmt, discovery_info.ip_addr,
                     service_av_transport, usn_string, discovery_info.boot_id);
            break;
        default:
            abort(); // Should never happen
    };
    sendto(discovery_info.sockp, send_buf, strlen(send_buf), 0,
           (struct sockaddr*)&discovery_info.groupSock, sizeof(discovery_info.groupSock));
}

static void send_all(const char* fmt) {
    send_root_device_1(fmt);
    send_root_device_2(fmt);
    send_root_device_3(fmt);
    send_service(fmt, RenderingControl);
    send_service(fmt, ConnectionManager);
    send_service(fmt, AVTransport);
}

static void send_notify() {
    ESP_LOGI(TAG, "Sending NOTIFY packets");
    // Initial random delay
    int init_delay = esp_random() % 100;
    vTaskDelay(init_delay / portTICK_PERIOD_MS);

    // Send messages three times in a row
    for (int i = 0; i < 3; i++) {
        send_all(notify_fmt);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

static void handle_msearch_message(void) {
    char* mx_start = strstr(rec_buf, "MX: ");

    if (mx_start == NULL)
        return;

    uint8_t mx = MIN((int)(*(mx_start+4)-'0'), 5);
    uint32_t random_wait_msec = esp_random() % (mx*1000);

    char* st_start = strstr(rec_buf, "ST: ");
    if (strstr(st_start, "ssdp:all") != NULL) {
        vTaskDelay(random_wait_msec / portTICK_PERIOD_MS);
        send_all(msearch_resp_fmt);
    } else if (strstr(st_start, root_device_nt1) != NULL) {
        vTaskDelay(random_wait_msec / portTICK_PERIOD_MS);
        send_root_device_1(msearch_resp_fmt);
    } else if (strstr(st_start, discovery_info.uuid) != NULL) {
        vTaskDelay(random_wait_msec / portTICK_PERIOD_MS);
        send_root_device_2(msearch_resp_fmt);
    } else if (strstr(st_start, root_device_nt3) != NULL) {
        vTaskDelay(random_wait_msec / portTICK_PERIOD_MS);
        send_root_device_3(msearch_resp_fmt);
    } else if (strstr(st_start, service_rendering_control) != NULL) {
        vTaskDelay(random_wait_msec / portTICK_PERIOD_MS);
        send_service(msearch_resp_fmt, RenderingControl);
    } else if (strstr(st_start, service_connection_manager) != NULL) {
        vTaskDelay(random_wait_msec / portTICK_PERIOD_MS);
        send_service(msearch_resp_fmt, ConnectionManager);
    } else if (strstr(st_start, service_connection_manager) != NULL) {
        vTaskDelay(random_wait_msec / portTICK_PERIOD_MS);
        send_service(msearch_resp_fmt, AVTransport);
    } else {
        ESP_LOGV(TAG, "Unkown ST. Discarding");
    }

}

static void remindThreadToSend(TimerHandle_t self) {
    xTaskNotifyGive(discovery_info.handle);
}

_Noreturn static void discovery_listen_loop(void* args) {
    ESP_LOGI(TAG, "Discovery Listen Loop started");
    send_notify();
    TimerHandle_t notify_timer = xTimerCreate("Notify Timer", pdMS_TO_TICKS(90000), pdTRUE, NULL, remindThreadToSend);
    xTimerStart(notify_timer, 0);

    struct timeval tv = {
            .tv_sec = 300,
            .tv_usec = 0
    };

    fd_set set;
    FD_ZERO(&set);

    struct sockaddr sender;
    size_t sender_length = sizeof(sender);
    while (1) {
        if (ulTaskNotifyTake(pdTRUE, 0))
            send_notify();

        FD_SET(discovery_info.sockp, &set);
        select(discovery_info.sockp + 1, &set, NULL, NULL, &tv);

        if (FD_ISSET(discovery_info.sockp, &set)) {
            recvfrom(discovery_info.sockp, rec_buf, sizeof(rec_buf), 0, &sender, &sender_length);
            if (strstr(rec_buf, "M-SEARCH * HTTP/1.1") != NULL) {
                ESP_LOGV(TAG, "MSEARCH message received!");
                handle_msearch_message();
            } else if (strstr(rec_buf, "MediaServer") != NULL) {
                ESP_LOGI(TAG, "Media server found! Sending packets");
                send_notify();
            } else {
                ESP_LOGV(TAG, "Message discarded");
            }
            memset(rec_buf, 0, sizeof(rec_buf));
        }
    }
}

void start_discovery(const char* ip_addr, const char* uuid) {
    // Save IP address string for later use
    discovery_info.ip_addr = ip_addr;
    discovery_info.uuid = uuid;

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
    setsockopt(udpSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &group, sizeof(group));

    time_t t;
    time(&t);
    discovery_info.boot_id = (int)t;

    struct sockaddr_in groupSock = {
            .sin_family = AF_INET,
            .sin_addr.s_addr = inet_addr(SSDP_MULTICAST_ADDR_IPV4),
            .sin_port = htons(SSDP_MULTICAST_PORT)
    };
    memcpy(&discovery_info.groupSock, &groupSock, sizeof(groupSock));

    discovery_info.sockp = udpSocket;
    xTaskCreate(discovery_listen_loop, "Discovery Listen Loop", 2048, NULL, 5, &discovery_info.handle);
}