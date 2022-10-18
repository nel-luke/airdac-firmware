#ifndef AIRDAC_FIRMWARE_UPNP_DISCOVERY_H
#define AIRDAC_FIRMWARE_UPNP_DISCOVERY_H

void start_discovery(const char* ip_addr, const char* uuid);
void discovery_send_notify(void);
void service_discovery(void);

#endif //AIRDAC_FIRMWARE_UPNP_DISCOVERY_H
