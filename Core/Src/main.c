#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "NUC472_442.h"

#include "lwip/api.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "lwip/dhcp.h"

#include "lwip/apps/lwiperf.h"

#include "ethernetif.h"
#include "app_ethernet.h"

struct netif gnetif;
static lwiperf_report_fn iperf;

void SYS_Init(void)
{
    /* Unlock protected registers */
    SYS_UnlockReg();

    /* Enable External XTAL (4~24 MHz) */
    CLK_EnableXtalRC(CLK_PWRCTL_HXTEN_Msk);

    /* Waiting for 12MHz clock ready */
    CLK_WaitClockReady(CLK_STATUS_HXTSTB_Msk);

    /* Switch HCLK clock source to HXT */
    CLK_SetHCLK(CLK_CLKSEL0_HCLKSEL_HXT, CLK_CLKDIV0_HCLK(1));

    /* Set PLL to power down mode and PLL_STB bit in CLKSTATUS register will be cleared by hardware.*/
    CLK->PLLCTL |= CLK_PLLCTL_PD_Msk;

    /* Set PLL frequency */
    CLK->PLLCTL = CLK_PLLCTL_84MHz_HXT;

    /* Waiting for clock ready */
    CLK_WaitClockReady(CLK_STATUS_PLLSTB_Msk);

    /* Switch HCLK clock source to PLL */
    CLK_SetHCLK(CLK_CLKSEL0_HCLKSEL_PLL, CLK_CLKDIV0_HCLK(1));

    /* Enable IP clock */
    CLK_EnableModuleClock(EMAC_MODULE);

    // Configure MDC clock rate to HCLK / (127 + 1) = 656 kHz if system is running at 84 MHz
    CLK_SetModuleClock(EMAC_MODULE, 0, CLK_CLKDIV3_EMAC(127));

    /* User can use SystemCoreClockUpdate() to calculate SystemCoreClock. */
    SystemCoreClockUpdate();

    /* Set GPG multi-function pins for UART0 RXD and TXD */
    SYS->GPG_MFPL |= SYS_GPG_MFPL_PG1MFP_UART0_RXD | SYS_GPG_MFPL_PG2MFP_UART0_TXD;
    // Configure RMII pins
    SYS->GPC_MFPL |= SYS_GPC_MFPL_PC0MFP_EMAC_REFCLK |
                     SYS_GPC_MFPL_PC1MFP_EMAC_MII_RXERR |
                     SYS_GPC_MFPL_PC2MFP_EMAC_MII_RXDV |
                     SYS_GPC_MFPL_PC3MFP_EMAC_MII_RXD1 |
                     SYS_GPC_MFPL_PC4MFP_EMAC_MII_RXD0 |
                     SYS_GPC_MFPL_PC6MFP_EMAC_MII_TXD0 |
                     SYS_GPC_MFPL_PC7MFP_EMAC_MII_TXD1;

    SYS->GPC_MFPH |= SYS_GPC_MFPH_PC8MFP_EMAC_MII_TXEN;
    // Enable high slew rate on all RMII pins
    PC->SLEWCTL |= 0x1DF;

    // Configure MDC, MDIO at PB14 & PB15
    SYS->GPB_MFPH |= SYS_GPB_MFPH_PB14MFP_EMAC_MII_MDC | SYS_GPB_MFPH_PB15MFP_EMAC_MII_MDIO;

    /* Lock protected registers */
    SYS_LockReg();
}

static void app_main(void *args)
{
    ip4_addr_t ipaddr;
    ip4_addr_t netmask;
    ip4_addr_t gw;

    ip4_addr_set_zero(&ipaddr);
    ip4_addr_set_zero(&netmask);
    ip4_addr_set_zero(&gw);

    tcpip_init(NULL, NULL);

    netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &tcpip_input);

    netif_set_default(&gnetif);

    netif_set_link_callback(&gnetif, ethernet_link_status_updated);
    ethernet_link_status_updated(&gnetif);

    xTaskCreate(ethernet_link_thread, "EthLink", configMINIMAL_STACK_SIZE * 2, &gnetif, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(ethernet_dhcp_thread, "EthDHCP", configMINIMAL_STACK_SIZE * 2, &gnetif, tskIDLE_PRIORITY + 1, NULL);

    lwiperf_start_tcp_server_default(iperf, NULL);

    for (;;)
    {
        vTaskDelay(100);
    }
}

int main(void)
{
    SYS_Init();

    xTaskCreate(app_main, "main", 350, NULL, tskIDLE_PRIORITY, NULL);
    vTaskStartScheduler();

    while (1)
        ;
}

void HardFault_Handler()
{
    while (1)
        ;
}
