#include "upnp.h"
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
    uuid_t uuid;
    char ip_addr[INET_ADDRSTRLEN];
    int boot_id;
    struct sockaddr_in groupSock;
    TaskHandle_t handle;
} ssdp_listen_vars;

static const char notify_fmt[] =
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age = 1800\r\n"
        "LOCATION: http://%s/upnp/\r\n"
        "NT: %s\r\n"
        "NTS: ssdp:alive\r\n"
        "SERVER: esp-idf/" ESP_VER_STR "UPnP/2.0 AirDAC/1.0\r\n"
        "USN: %s\r\n"
        "BOOTID.UPNP.ORG: %d\r\n"
        "CONFIGID.UPNP.ORG: 1\r\n"
        "\r\n";

static const char root_device_nt1[] = "upnp:rootdevice";
static const char root_device_nt3[] = "urn:schemas-upnp-org:device:MediaRenderer:2";
static const char root_service_nt[] = "urn:schemas-upnp-org:service:MediaRenderer:2";

static const char msearch_resp_fmt[] =
        "HTTP/1.1 200 OK\r\n"
        "CACHE-CONTROL: max-age = 1800\r\n"
        "EST:\r\n"
        "LOCATION: http://%s/upnp\r\n"
        "SERVER: esp-idf/" ESP_VER_STR "UPnP/2.0 AirDAC/1.0\r\n"
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
    snprintf(usn_string, sizeof(usn_string), "%s::%s", ssdp_listen_vars.uuid.uuid_s, root_device_nt1);
    snprintf(send_buf, sizeof(send_buf), fmt, ssdp_listen_vars.ip_addr,
             root_device_nt1, usn_string, ssdp_listen_vars.boot_id);
    sendto(ssdp_listen_vars.sockp, send_buf, strlen(send_buf), 0,
           (struct sockaddr*)&ssdp_listen_vars.groupSock, sizeof(ssdp_listen_vars.groupSock));
}

static inline void send_root_device_2(const char* fmt) {
    snprintf(nt_string, sizeof(nt_string), "uuid:%s", ssdp_listen_vars.uuid.uuid_s);
    snprintf(send_buf, sizeof(send_buf), fmt, ssdp_listen_vars.ip_addr,
             nt_string, nt_string, ssdp_listen_vars.boot_id);
    sendto(ssdp_listen_vars.sockp, send_buf, strlen(send_buf), 0,
           (struct sockaddr*)&ssdp_listen_vars.groupSock, sizeof(ssdp_listen_vars.groupSock));
}

static inline void send_root_device_3(const char* fmt) {
    snprintf(usn_string, sizeof(usn_string), "uuid:%s::%s", ssdp_listen_vars.uuid.uuid_s, root_device_nt3);
    snprintf(send_buf, sizeof(send_buf), fmt, ssdp_listen_vars.ip_addr,
             root_device_nt3, usn_string, ssdp_listen_vars.boot_id);
    sendto(ssdp_listen_vars.sockp, send_buf, strlen(send_buf), 0,
           (struct sockaddr*)&ssdp_listen_vars.groupSock, sizeof(ssdp_listen_vars.groupSock));
}

static inline void send_root_service(const char* fmt) {
    snprintf(usn_string, sizeof(usn_string), "uuid:%s::%s", ssdp_listen_vars.uuid.uuid_s, root_service_nt);
    snprintf(send_buf, sizeof(send_buf), fmt, ssdp_listen_vars.ip_addr,
             root_service_nt, usn_string, ssdp_listen_vars.boot_id);
    sendto(ssdp_listen_vars.sockp, send_buf, strlen(send_buf), 0,
           (struct sockaddr*)&ssdp_listen_vars.groupSock, sizeof(ssdp_listen_vars.groupSock));
}

static void send_all(const char* fmt) {
    send_root_device_1(fmt);
    send_root_device_2(fmt);
    send_root_device_3(fmt);
    send_root_service(fmt);
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
    } else if (strstr(st_start, ssdp_listen_vars.uuid.uuid_s) != NULL) {
        vTaskDelay(random_wait_msec / portTICK_PERIOD_MS);
        send_root_device_2(msearch_resp_fmt);
    } else if (strstr(st_start, root_device_nt3) != NULL) {
        vTaskDelay(random_wait_msec / portTICK_PERIOD_MS);
        send_root_device_3(msearch_resp_fmt);
    } else if (strstr(st_start, root_service_nt) != NULL) {
        vTaskDelay(random_wait_msec / portTICK_PERIOD_MS);
        send_root_service(msearch_resp_fmt);
    } else {
        ESP_LOGV(TAG, "Unkown ST. Discarding");
    }

}

static void remindThreadToSend(TimerHandle_t self) {
    xTaskNotifyGive(ssdp_listen_vars.handle);
}

_Noreturn static void ssdp_listen_loop(void* args) {
    ESP_LOGI(TAG, "SSDP Listen Loop started");
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

        FD_SET(ssdp_listen_vars.sockp, &set);
        select(ssdp_listen_vars.sockp+1, &set, NULL, NULL, &tv);

        if (FD_ISSET(ssdp_listen_vars.sockp, &set)) {
            recvfrom(ssdp_listen_vars.sockp, rec_buf, sizeof(rec_buf), 0, &sender, &sender_length);
            if (strstr(rec_buf, "M-SEARCH * HTTP/1.1") != NULL) {
                ESP_LOGV(TAG, "MSEARCH message received!");
                handle_msearch_message();
            } else {
                ESP_LOGI(TAG, "Message discarded");
            }
            memset(rec_buf, 0, sizeof(rec_buf));
        }
    }
}

void start_discovery(const char* ip_addr, const uuid_t* uuid) {
    // Save IP address string for later use
    strcpy(&ssdp_listen_vars.ip_addr, ip_addr);

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
    ssdp_listen_vars.boot_id = (int)t;

    struct sockaddr_in groupSock = {
            .sin_family = AF_INET,
            .sin_addr.s_addr = inet_addr(SSDP_MULTICAST_ADDR_IPV4),
            .sin_port = htons(SSDP_MULTICAST_PORT)
    };
    memcpy(&ssdp_listen_vars.groupSock, &groupSock, sizeof(groupSock));

    ssdp_listen_vars.sockp = udpSocket;
    memcpy(&ssdp_listen_vars.uuid, uuid, sizeof(uuid_t));
    xTaskCreate(ssdp_listen_loop, "SSDP Listen Loop", 2048, NULL, 5, &ssdp_listen_vars.handle);
}