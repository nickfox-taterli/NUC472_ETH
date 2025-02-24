#include "NUC472_442.h"

#include "lwip/opt.h"
#include "lwip/dhcp.h"

#include "app_ethernet.h"
#include "ethernetif.h"

#include "FreeRTOS.h"
#include "task.h"

#define MAX_DHCP_TRIES 4

#define DHCP_OFF (uint8_t)0
#define DHCP_START (uint8_t)1
#define DHCP_WAIT_ADDRESS (uint8_t)2
#define DHCP_ADDRESS_ASSIGNED (uint8_t)3
#define DHCP_TIMEOUT (uint8_t)4
#define DHCP_LINK_DOWN (uint8_t)5

volatile uint8_t DHCP_state = DHCP_OFF;

void ethernet_link_status_updated(struct netif *netif)
{
  if (netif_is_up(netif))
    DHCP_state = DHCP_START;
  else
    DHCP_state = DHCP_LINK_DOWN;
}

/**
 * @brief  DHCP Process
 * @param  pvParameters: network interface
 * @retval None
 */
void ethernet_dhcp_thread(void *pvParameters)
{
  struct netif *netif = (struct netif *)pvParameters;
  ip_addr_t ipaddr;
  ip_addr_t netmask;
  ip_addr_t gw;
  struct dhcp *dhcp;

  for (;;)
  {
    switch (DHCP_state)
    {
    case DHCP_START:
    {
      ip4_addr_set_zero(&netif->ip_addr);
      ip4_addr_set_zero(&netif->netmask);
      ip4_addr_set_zero(&netif->gw);

      DHCP_state = DHCP_WAIT_ADDRESS;
      dhcp_start(netif);
    }
    break;

    case DHCP_WAIT_ADDRESS:
    {
      if (dhcp_supplied_address(netif))
      {
        DHCP_state = DHCP_ADDRESS_ASSIGNED;
      }
      else
      {
        dhcp = (struct dhcp *)netif_get_client_data(netif, LWIP_NETIF_CLIENT_DATA_INDEX_DHCP);

        /* DHCP timeout */
        if (dhcp->tries > MAX_DHCP_TRIES)
        {
          DHCP_state = DHCP_START;
        }
      }
    }
    break;
    case DHCP_LINK_DOWN:
    {
      DHCP_state = DHCP_OFF;
    }
    break;
    default:
      break;
    }

    /* wait 500 ms */
    vTaskDelay(500);
  }
}
