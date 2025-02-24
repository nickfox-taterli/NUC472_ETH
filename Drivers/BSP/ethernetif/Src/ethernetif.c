#include "NUC472_442.h"

#include "lwip/opt.h"

#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip/opt.h"
#include "lwip/def.h"

#include "netif/etharp.h"
#include "arch/sys_arch.h"
#include "ethernetif.h"

#include <string.h>
#include <stdint.h>

struct ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT] __attribute__((aligned(4)));
struct ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT] __attribute__((aligned(4)));

struct ETH_DMADescTypeDef volatile *DMACurTxDescPtr, *DMACurRxDescPtr, *DMAFinTxDescPtr;

uint32_t DMARxLen = 0;
uint8_t *DMARxBuf = NULL;

uint8_t DMARxBufTab[ETH_RX_DESC_CNT][PACKET_BUFFER_SIZE];

SemaphoreHandle_t RxPktSemaphore = NULL;
SemaphoreHandle_t TxPktSemaphore = NULL;

static void mdio_write(uint8_t addr, uint8_t reg, uint16_t val)
{

    EMAC->MIIMDAT = val;
    EMAC->MIIMCTL = (addr << EMAC_MIIMCTL_PHYADDR_Pos) | reg | EMAC_MIIMCTL_BUSY_Msk | EMAC_MIIMCTL_WRITE_Msk | EMAC_MIIMCTL_MDCON_Msk;

    while (EMAC->MIIMCTL & EMAC_MIIMCTL_BUSY_Msk)
        ;
}
static void ethernetif_input(void *pvParameters);

static uint16_t mdio_read(uint8_t addr, uint8_t reg)
{
    EMAC->MIIMCTL = (addr << EMAC_MIIMCTL_PHYADDR_Pos) | reg | EMAC_MIIMCTL_BUSY_Msk | EMAC_MIIMCTL_MDCON_Msk;
    while (EMAC->MIIMCTL & EMAC_MIIMCTL_BUSY_Msk)
        ;

    return (EMAC->MIIMDAT);
}

static void low_level_init(struct netif *netif)
{
    /* 设置MAC硬件地址长度 */
    netif->hwaddr_len = ETHARP_HWADDR_LEN;

    /* 设置MAC硬件地址 */
    netif->hwaddr[0] = ETH_MAC_ADDR0;
    netif->hwaddr[1] = ETH_MAC_ADDR1;
    netif->hwaddr[2] = ETH_MAC_ADDR2;
    netif->hwaddr[3] = ETH_MAC_ADDR3;
    netif->hwaddr[4] = ETH_MAC_ADDR4;
    netif->hwaddr[5] = ETH_MAC_ADDR5;

    /* 设置最大传输单元（MTU） */
    netif->mtu = 1500;

    RxPktSemaphore = xSemaphoreCreateBinary();
    TxPktSemaphore = xSemaphoreCreateBinary();

    xTaskCreate(ethernetif_input, "EthIf", 350, netif, tskIDLE_PRIORITY + 3, NULL);

    /* 设置设备能力标志 */
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;
#ifdef LWIP_IGMP
    netif->flags |= NETIF_FLAG_IGMP;
#endif

    /* 复位MAC控制器 */
    EMAC->CTL = EMAC_CTL_RST_Msk;

    /* 初始化发送描述符 */
    DMACurTxDescPtr = DMAFinTxDescPtr = &DMATxDscrTab[0];
    for (uint32_t i = 0; i < ETH_TX_DESC_CNT; i++)
    {
        DMATxDscrTab[i].status1 = TXFD_PADEN | TXFD_CRCAPP | TXFD_INTEN;
        DMATxDscrTab[i].buf = NULL;
        DMATxDscrTab[i].status2 = 0;
        DMATxDscrTab[i].next = &DMATxDscrTab[(i + 1) % ETH_TX_DESC_CNT];
    }
    EMAC->TXDSA = (unsigned int)&DMATxDscrTab[0];

    /* 初始化接收描述符 */
    DMACurRxDescPtr = &DMARxDscrTab[0];
    for (uint32_t i = 0; i < ETH_RX_DESC_CNT; i++)
    {
        DMARxDscrTab[i].status1 = OWNERSHIP_EMAC;
        DMARxDscrTab[i].buf = &DMARxBufTab[i][0];
        DMARxDscrTab[i].status2 = 0;
        DMARxDscrTab[i].next = &DMARxDscrTab[(i + 1) % ETH_RX_DESC_CNT];
    }
    EMAC->RXDSA = (unsigned int)&DMARxDscrTab[0];

    /* 配置MAC地址过滤 */
    EMAC->CAM0M = (ETH_MAC_ADDR0 << 24) | (ETH_MAC_ADDR1 << 16) | (ETH_MAC_ADDR2 << 8) | ETH_MAC_ADDR3;
    EMAC->CAM0L = (ETH_MAC_ADDR4 << 24) | (ETH_MAC_ADDR5 << 16);
    EMAC->CAMCTL = EMAC_CAMCTL_CMPEN_Msk | EMAC_CAMCTL_AMP_Msk | EMAC_CAMCTL_ABP_Msk;
    EMAC->CAMEN = 1;

    /* 复位PHY并配置自动协商 */
    mdio_write(CONFIG_PHY_ADDR, MII_BMCR, BMCR_RESET);

    uint32_t delay = 2000;
    while (delay-- > 0)
    {
        if ((mdio_read(CONFIG_PHY_ADDR, MII_BMCR) & BMCR_RESET) == 0)
            break;
    }

    if (delay == 0)
    {
        // PHY复位失败，处理错误
    }

    mdio_write(CONFIG_PHY_ADDR, MII_ADVERTISE, ADVERTISE_CSMA | ADVERTISE_10HALF | ADVERTISE_10FULL | ADVERTISE_100HALF | ADVERTISE_100FULL);

    uint16_t reg = mdio_read(CONFIG_PHY_ADDR, MII_BMCR);
    mdio_write(CONFIG_PHY_ADDR, MII_BMCR, reg | BMCR_ANRESTART);

    delay = 200000;
    while (delay-- > 0)
    {
        if ((mdio_read(CONFIG_PHY_ADDR, MII_BMSR) & (BMSR_ANEGCOMPLETE | BMSR_LSTATUS)) == (BMSR_ANEGCOMPLETE | BMSR_LSTATUS))
            break;
    }

    if (delay == 0)
    {
        // 自动协商失败，设置为100M全双工
        EMAC->CTL |= (EMAC_CTL_OPMODE_Msk | EMAC_CTL_FUDUP_Msk);
    }
    else
    {
        reg = mdio_read(CONFIG_PHY_ADDR, MII_LPA);

        if (reg & ADVERTISE_100FULL)
        {
            EMAC->CTL |= (EMAC_CTL_OPMODE_Msk | EMAC_CTL_FUDUP_Msk);
        }
        else if (reg & ADVERTISE_100HALF)
        {
            EMAC->CTL = (EMAC->CTL & ~EMAC_CTL_FUDUP_Msk) | EMAC_CTL_OPMODE_Msk;
        }
        else if (reg & ADVERTISE_10FULL)
        {
            EMAC->CTL = (EMAC->CTL & ~EMAC_CTL_OPMODE_Msk) | EMAC_CTL_FUDUP_Msk;
        }
        else
        {
            EMAC->CTL &= ~(EMAC_CTL_OPMODE_Msk | EMAC_CTL_FUDUP_Msk);
        }
    }

    /* 启用MAC控制器的接收和发送功能 */
    EMAC->CTL |= EMAC_CTL_STRIPCRC_Msk | EMAC_CTL_RXON_Msk | EMAC_CTL_TXON_Msk | EMAC_CTL_RMIIEN_Msk | EMAC_CTL_RMIIRXCTL_Msk;

    /* 启用MAC中断 */
    EMAC->INTEN |= EMAC_INTEN_RXIEN_Msk | EMAC_INTEN_RXGDIEN_Msk | EMAC_INTEN_RDUIEN_Msk | EMAC_INTEN_RXBEIEN_Msk |
                   EMAC_INTEN_TXIEN_Msk | EMAC_INTEN_TXABTIEN_Msk | EMAC_INTEN_TXCPIEN_Msk | EMAC_INTEN_TXBEIEN_Msk;

    /* 触发接收 */
    EMAC->RXST = 0;

    NVIC_SetPriority(EMAC_TX_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1);
    NVIC_EnableIRQ(EMAC_TX_IRQn);
    NVIC_SetPriority(EMAC_RX_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1);
    NVIC_EnableIRQ(EMAC_RX_IRQn);
}

static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
    struct pbuf *q;
    struct ETH_DMADescTypeDef volatile *currentDesc = DMACurTxDescPtr;
    uint16_t descCount = 0;
    err_t ret = ERR_OK;
#if ETH_PAD_SIZE
    pbuf_header(p, -ETH_PAD_SIZE); // 移除填充字节
#endif

    // 遍历pbuf链表，并映射到发送描述符
    for (q = p; q != NULL; q = q->next)
    {
        // 检查描述符数量是否超出限制
        if (descCount >= ETH_TX_DESC_CNT)
        {
            return ERR_MEM;
        }

        // 检查当前描述符是否被EMAC占用
        // if (currentDesc->status1 & OWNERSHIP_EMAC) {
        //     return ERR_MEM;
        // }

        // 直接引用pbuf的payload，避免内存拷贝
        currentDesc->buf = q->payload;
        currentDesc->status2 = q->len;
        currentDesc->status1 |= OWNERSHIP_EMAC;

        currentDesc = currentDesc->next; // 移动到下一个描述符
        descCount++;
    }

    // 更新当前发送描述符指针
    DMACurTxDescPtr = currentDesc;

    // 触发EMAC发送（只需触发一次）
    EMAC->TXST = 0;

#if ETH_PAD_SIZE
    pbuf_header(p, ETH_PAD_SIZE); // 恢复填充字节
#endif

    xSemaphoreTake(TxPktSemaphore, portMAX_DELAY);
    LINK_STATS_INC(link.xmit);

    return ERR_OK;
}

// 从网络接口接收数据包的低级输入函数
static struct pbuf *low_level_input(struct netif *netif)
{
    struct pbuf *p, *q;

#if ETH_PAD_SIZE
    len += ETH_PAD_SIZE; /* 为以太网填充预留空间 */
#endif

    /* 我们从池中分配一个pbuf链 */
    p = pbuf_alloc(PBUF_RAW, DMARxLen, PBUF_POOL);

    if (p != NULL)
    {

#if ETH_PAD_SIZE
        pbuf_header(p, -ETH_PAD_SIZE); /* 丢弃填充字 */
#endif

        DMARxLen = 0;
        /* 我们遍历pbuf链，直到将整个数据包读入pbuf */
        for (q = p; q != NULL; q = q->next)
        {
            memcpy((uint8_t *)q->payload, (uint8_t *)&DMARxBuf[DMARxLen], q->len);
            DMARxLen = DMARxLen + q->len;
        }

#if ETH_PAD_SIZE
        pbuf_header(p, ETH_PAD_SIZE); /* 重新获取填充字 */
#endif

        LINK_STATS_INC(link.recv); // 增加接收数据包的统计计数
    }
    else
    {
        // 如果分配失败，丢弃数据包
        LINK_STATS_INC(link.memerr); // 增加内存错误的统计计数
        LINK_STATS_INC(link.drop);   // 增加丢弃数据包的统计计数
    }

    return p; // 返回接收到的pbuf链
}

// 以太网接口输入处理函数
static void ethernetif_input(void *pvParameters)
{
    struct pbuf *p = NULL;
    struct netif *netif = (struct netif *)pvParameters;

    for (;;)
    {
        // 等待接收数据包的信号量
        if (xSemaphoreTake(RxPktSemaphore, portMAX_DELAY) == pdTRUE)
        {
            do
            {
                // 从低级输入函数获取数据包
                p = low_level_input(netif);
                if (p != NULL)
                {
                    // 将数据包传递给网络接口的输入函数
                    if (netif->input(p, netif) != ERR_OK)
                    {
                        pbuf_free(p); // 如果输入失败，释放pbuf
                    }
                }
            } while (p != NULL); // 循环处理所有接收到的数据包
        }
    }
}

err_t ethernetif_init(struct netif *netif)
{
#if LWIP_NETIF_HOSTNAME
    netif->hostname = "st"; // 设置网络接口的主机名
#endif                      /* LWIP_NETIF_HOSTNAME */

    NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, 100); // 初始化SNMP

    netif->name[0] = 's'; // 设置网络接口名称
    netif->name[1] = 't';

    netif->output = etharp_output;        // 设置ARP输出函数
    netif->linkoutput = low_level_output; // 设置低级输出函数

    low_level_init(netif); // 初始化硬件

    return ERR_OK; // 返回初始化成功
}

void ethernet_link_thread(void *argument)
{
    uint8_t PHYLinkState = 0;
    struct netif *netif = (struct netif *)argument;

    for (;;)
    {
        PHYLinkState = (mdio_read(CONFIG_PHY_ADDR, MII_BMSR) & 0x04) == 0x04;

        if (netif_is_link_up(netif) && !PHYLinkState)
        {
            netif_set_down(netif);
            netif_set_link_down(netif);
        }
        else if (!netif_is_link_up(netif) && PHYLinkState)
        {
            uint16_t reg = mdio_read(CONFIG_PHY_ADDR, MII_LPA);

            if (reg & ADVERTISE_100FULL)
            {
                EMAC->CTL |= (EMAC_CTL_OPMODE_Msk | EMAC_CTL_FUDUP_Msk);
            }
            else if (reg & ADVERTISE_100HALF)
            {
                EMAC->CTL = (EMAC->CTL & ~EMAC_CTL_FUDUP_Msk) | EMAC_CTL_OPMODE_Msk;
            }
            else if (reg & ADVERTISE_10FULL)
            {
                EMAC->CTL = (EMAC->CTL & ~EMAC_CTL_OPMODE_Msk) | EMAC_CTL_FUDUP_Msk;
            }
            else
            {
                EMAC->CTL &= ~(EMAC_CTL_OPMODE_Msk | EMAC_CTL_FUDUP_Msk);
            }

            netif_set_up(netif);
            netif_set_link_up(netif);
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void EMAC_RX_IRQHandler(void)
{
    unsigned int status;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    status = EMAC->INTSTS & 0xFFFF;
    EMAC->INTSTS = status;
    if (status & EMAC_INTSTS_RXBEIF_Msk)
    {
        // 调试时候也应该关注,不应该执行到这里,可能是坏的数据包.
    }
    do
    {
        status = DMACurRxDescPtr->status1;

        if (status & OWNERSHIP_EMAC)
            break;

        if (status & RXFD_RXGD)
        {
            DMARxLen = DMACurRxDescPtr->status1 & 0xFFFF;
            DMARxBuf = DMACurRxDescPtr->buf;
            xSemaphoreGiveFromISR(RxPktSemaphore, &xHigherPriorityTaskWoken);
        }

        DMACurRxDescPtr->status1 = OWNERSHIP_EMAC;
        DMACurRxDescPtr = DMACurRxDescPtr->next;

    } while (1);

    EMAC->RXST = 0;
    portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
}

void EMAC_TX_IRQHandler(void)
{
    uint32_t CTXDSA, status;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xSemaphoreGiveFromISR(TxPktSemaphore, &xHigherPriorityTaskWoken);

    status = EMAC->INTSTS & 0xFFFF0000;
    EMAC->INTSTS = status;
    if (status & EMAC_INTSTS_TXBEIF_Msk)
    {
        // 调试时候也应该关注,不应该执行到这里,不应继续执行.
        return;
    }

    CTXDSA = EMAC->CTXDSA;

    while (CTXDSA != (uint32_t)DMAFinTxDescPtr)
    {
        DMAFinTxDescPtr = DMAFinTxDescPtr->next;
    }
    portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
}
