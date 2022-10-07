#include "dlna.h"
#include "ssdp.h"

#include <esp_log.h>
#include <nvs_flash.h>

#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>

#define SSDP_MULTICAST_ADDR_IPV4 "239.255.255.250"
#define SSDP_MULTICAST_PORT 1900



static const char *TAG = "dlna";



static void ssdp_listen_loop(void* sockp) {
    const int sock = (int)sockp;

    struct sockaddr_in groupSock = {
            .sin_family = AF_INET,
            .sin_addr.s_addr = inet_addr(SSDP_MULTICAST_ADDR_IPV4),
            .sin_port = htons(SSDP_MULTICAST_PORT)
    };

    char message[] = "NOTIFY * HTTP/1.1\r\n";
    while (1) {
        ESP_LOGI(TAG, "Sending UDP packet");
        sendto(sock, message, strlen(message), 0, (struct sockaddr*)&groupSock, sizeof(groupSock));
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

void start_dlna(esp_netif_t *netif) {
    // Create socket
    int udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    assert(udpSocket >= 0); // Should never happen

    char enable_loopback = 0;
    setsockopt(udpSocket, IPPROTO_IP, IP_MULTICAST_LOOP, &enable_loopback, sizeof(enable_loopback));

    esp_netif_ip_info_t ip_info = { 0 };
    esp_netif_get_ip_info(netif, &ip_info);

    struct in_addr localInterface = { 0 };
    inet_addr_from_ip4addr(&localInterface, &ip_info.ip);
    setsockopt(udpSocket, IPPROTO_IP, IP_MULTICAST_IF, &localInterface, sizeof(localInterface));

    struct sockaddr_in localSock = {
            .sin_family = AF_INET,
            .sin_addr.s_addr = INADDR_ANY,
            .sin_port = htons(1900)
    };

    bind(udpSocket, (struct sockaddr*)&localSock, sizeof(localSock));

    struct ip_mreq group = {
            .imr_interface.s_addr = INADDR_ANY,
            .imr_multiaddr.s_addr = inet_addr(SSDP_MULTICAST_ADDR_IPV4),
    };
    setsockopt(udpSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &group, sizeof(group));

    xTaskCreate(ssdp_listen_loop, "SSDP Listen Loop", 2048, (void*)udpSocket, 5, NULL);
}