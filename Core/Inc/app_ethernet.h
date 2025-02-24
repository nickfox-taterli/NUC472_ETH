//
// Created by TaterLi on 25-2-18.
//

#ifndef APP_ETHERNET_H
#define APP_ETHERNET_H

void ethernet_link_status_updated(struct netif *netif);
void ethernet_dhcp_thread(void *pvParameters);

#endif // APP_ETHERNET_H
