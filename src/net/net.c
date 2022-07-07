/*
 *	Copied from Linux Monitor (LiMon) - Networking.
 *
 *	Copyright 1994 - 2000 Neil Russell.
 *	(See License)
 *	Copyright 2000 Roland Borde
 *	Copyright 2000 Paolo Scaffardi
 *	Copyright 2000-2002 Wolfgang Denk, wd@denx.de
 */

/*
 * General Desription:
 *
 * The user interface supports commands for BOOTP, RARP, and TFTP.
 * Also, we support ARP internally. Depending on available data,
 * these interact as follows:
 *
 * BOOTP:
 *
 *	Prerequisites:	- own ethernet address
 *	We want:	- own IP address
 *			- TFTP server IP address
 *			- name of bootfile
 *	Next step:	ARP
 *
 * RARP:
 *
 *	Prerequisites:	- own ethernet address
 *	We want:	- own IP address
 *			- TFTP server IP address
 *	Next step:	ARP
 *
 * ARP:
 *
 *	Prerequisites:	- own ethernet address
 *			- own IP address
 *			- TFTP server IP address
 *	We want:	- TFTP server ethernet address
 *	Next step:	TFTP
 *
 * DHCP:
 *
 *     Prerequisites:	- own ethernet address
 *     We want:		- IP, Netmask, ServerIP, Gateway IP
 *			- bootfilename, lease time
 *     Next step:	- TFTP
 *
 * TFTP:
 *
 *	Prerequisites:	- own ethernet address
 *			- own IP address
 *			- TFTP server IP address
 *			- TFTP server ethernet address
 *			- name of bootfile (if unknown, we use a default name
 *			  derived from our own IP address)
 *	We want:	- load the boot file
 *	Next step:	none
 *
 * NFS:
 *
 *	Prerequisites:	- own ethernet address
 *			- own IP address
 *			- name of bootfile (if unknown, we use a default name
 *			  derived from our own IP address)
 *	We want:	- load the boot file
 *	Next step:	none
 *
 * SNTP:
 *
 *	Prerequisites:	- own ethernet address
 *			- own IP address
 *	We want:	- network time
 *	Next step:	none
 */


#include <common.h>
#include <watchdog.h>
#include <command.h>
#include <net.h>
#include "bootp.h"
#include "tftp.h"
#include "rarp.h"
#include "nfs.h"
#include "httpd.h"
#ifdef CONFIG_STATUS_LED
#include <status_led.h>
#include <miiphy.h>
#endif
#if (CONFIG_COMMANDS & CFG_CMD_SNTP)
#include "sntp.h"
#endif

#include "../httpd/uipopt.h"
#include "../httpd/uip.h"
#include "../httpd/uip_arp.h"

#if (CONFIG_COMMANDS & CFG_CMD_NET)

DECLARE_GLOBAL_DATA_PTR;

#define ARP_TIMEOUT		5		/* Seconds before trying ARP again */
#ifndef	CONFIG_NET_RETRY_COUNT
# define ARP_TIMEOUT_COUNT	5		/* # of timeouts before giving up  */
#else
# define ARP_TIMEOUT_COUNT  (CONFIG_NET_RETRY_COUNT)
#endif

extern char NetUipLoop;
extern char dhcpd_end;
void dev_received(volatile uchar * inpkt, int len);

unsigned char *webfailsafe_data_pointer = NULL;
int	webfailsafe_is_running = 0;
int	webfailsafe_ready_for_upgrade = 0;
int	webfailsafe_upgrade_type = WEBFAILSAFE_UPGRADE_TYPE_FIRMWARE;

void NetReceiveHttpd(volatile uchar * inpkt, int len);

extern int do_reset(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);

#if 0
#define ET_DEBUG
#endif

/** BOOTP EXTENTIONS **/

IPaddr_t	NetOurSubnetMask=0;		/* Our subnet mask (0=unknown)	*/
IPaddr_t	NetOurGatewayIP=0;		/* Our gateways IP address	*/
IPaddr_t	NetOurDNSIP=0;			/* Our DNS IP address		*/
#if (CONFIG_BOOTP_MASK & CONFIG_BOOTP_DNS2)
IPaddr_t	NetOurDNS2IP=0;			/* Our 2nd DNS IP address	*/
#endif
char		NetOurNISDomain[32]={0,};	/* Our NIS domain		*/
char		NetOurHostName[32]={0,};	/* Our hostname			*/
char		NetOurRootPath[64]={0,};	/* Our bootpath			*/
ushort		NetBootFileSize=0;		/* Our bootfile size in blocks	*/

/** END OF BOOTP EXTENTIONS **/

ulong		NetBootFileXferSize;	/* The actual transferred size of the bootfile (in bytes) */
uchar		NetOurEther[6];		/* Our ethernet address			*/
uchar		NetServerEther[6] =	/* Boot server enet address		*/
			{ 0, 0, 0, 0, 0, 0 };
IPaddr_t	NetOurIP;		/* Our IP addr (0 = unknown)		*/
IPaddr_t	NetServerIP;		/* Our IP addr (0 = unknown)		*/
volatile uchar *NetRxPkt;		/* Current receive packet		*/
int		NetRxPktLen;		/* Current rx packet length		*/
unsigned	NetIPID;		/* IP packet ID				*/
uchar		NetBcastAddr[6] =	/* Ethernet bcast address		*/
			{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
uchar		NetEtherNullAddr[6] =
			{ 0, 0, 0, 0, 0, 0 };
#if (CONFIG_COMMANDS & CFG_CMD_CDP)
uchar		NetCDPAddr[6] =		/* Ethernet bcast address		*/
			{ 0x01, 0x00, 0x0c, 0xcc, 0xcc, 0xcc };
#endif
int		NetState;		/* Network loop state			*/
#ifdef CONFIG_NET_MULTI
int		NetRestartWrap = 0;	/* Tried all network devices		*/
static int	NetRestarted = 0;	/* Network loop restarted		*/
static int	NetDevExists = 0;	/* At least one device configured	*/
#endif

/* XXX in both little & big endian machines 0xFFFF == ntohs(-1) */
ushort		NetOurVLAN = 0xFFFF;		/* default is without VLAN	*/
ushort		NetOurNativeVLAN = 0xFFFF;	/* ditto			*/

char		BootFile[128];		/* Boot File name			*/

#if (CONFIG_COMMANDS & CFG_CMD_PING)
IPaddr_t	NetPingIP;		/* the ip address to ping 		*/

static void PingStart(void);
#endif
#if defined(CFG_ATHRS26_PHY) && defined(CFG_ATHRHDR_EN)
extern void athr_hdr_func(void);
#endif

#if (CONFIG_COMMANDS & CFG_CMD_CDP)
static void CDPStart(void);
#endif

#if (CONFIG_COMMANDS & CFG_CMD_SNTP)
IPaddr_t	NetNtpServerIP;		/* NTP server IP address		*/
int		NetTimeOffset=0;	/* offset time from UTC			*/
#endif

#ifdef CONFIG_NETCONSOLE
void NcStart(void);
int nc_input_packet(uchar *pkt, unsigned dest, unsigned src, unsigned len);
#endif

volatile uchar	PktBuf[(PKTBUFSRX+1) * PKTSIZE_ALIGN + PKTALIGN];

volatile uchar *NetRxPackets[PKTBUFSRX]; /* Receive packets			*/

static rxhand_f *packetHandler;		/* Current RX packet handler		*/
static thand_f *timeHandler;		/* Current timeout handler		*/
static ulong	timeStart;		/* Time base value			*/
static ulong	timeDelta;		/* Current timeout value		*/
volatile uchar *NetTxPacket = 0;	/* THE transmit packet			*/

static int net_check_prereq (proto_t protocol);

/**********************************************************************/

IPaddr_t	NetArpWaitPacketIP;
IPaddr_t	NetArpWaitReplyIP;
uchar	       *NetArpWaitPacketMAC;	/* MAC address of waiting packet's destination	*/
uchar	       *NetArpWaitTxPacket;	/* THE transmit packet			*/
int		NetArpWaitTxPacketSize;
uchar 		NetArpWaitPacketBuf[PKTSIZE_ALIGN + PKTALIGN];
ulong		NetArpWaitTimerStart;
int		NetArpWaitTry;

void ArpRequest (void)
{
	int i;
	volatile uchar *pkt;
	ARP_t *arp;

#ifdef ET_DEBUG
	printf ("ARP broadcast %d\n", NetArpWaitTry);
#endif
	pkt = NetTxPacket;

	pkt += NetSetEther (pkt, NetBcastAddr, PROT_ARP);

	arp = (ARP_t *) pkt;

	arp->ar_hrd = htons (ARP_ETHER);
	arp->ar_pro = htons (PROT_IP);
	arp->ar_hln = 6;
	arp->ar_pln = 4;
	arp->ar_op = htons (ARPOP_REQUEST);

	memcpy (&arp->ar_data[0], NetOurEther, 6);		/* source ET addr	*/
	NetWriteIP ((uchar *) & arp->ar_data[6], NetOurIP);	/* source IP addr	*/
	for (i = 10; i < 16; ++i) {
		arp->ar_data[i] = 0;				/* dest ET addr = 0     */
	}

	if ((NetArpWaitPacketIP & NetOurSubnetMask) !=
	    (NetOurIP & NetOurSubnetMask)) {
		if (NetOurGatewayIP == 0) {
			puts ("## Warning: gatewayip needed but not set\n");
			NetArpWaitReplyIP = NetArpWaitPacketIP;
		} else {
			NetArpWaitReplyIP = NetOurGatewayIP;
		}
	} else {
		NetArpWaitReplyIP = NetArpWaitPacketIP;
	}

	NetWriteIP ((uchar *) & arp->ar_data[16], NetArpWaitReplyIP);
	(void) eth_send (NetTxPacket, (pkt - NetTxPacket) + ARP_HDR_SIZE);
}

void ArpTimeoutCheck(void)
{
	ulong t;

	if (!NetArpWaitPacketIP)
		return;

	t = get_timer(0);

	/* check for arp timeout */
	if ((t - NetArpWaitTimerStart) > ARP_TIMEOUT * CFG_HZ) {
		NetArpWaitTry++;

		if (NetArpWaitTry >= ARP_TIMEOUT_COUNT) {
			puts ("\nARP Retry count exceeded; starting again\n");
			NetArpWaitTry = 0;
			NetStartAgain();
		} else {
			NetArpWaitTimerStart = t;
			ArpRequest();
		}
	}
}

/**********************************************************************/
/*
 *	Main network processing loop.
 */
char Tftp_stop;
int
NetLoop(proto_t protocol)
{
	
	bd_t *bd = gd->bd;
#if defined(CFG_ATHRS26_PHY) && defined(CFG_ATHRHDR_EN)
	static int AthrHdr_Flag = 0;
#endif

#ifdef CONFIG_NET_MULTI
	NetRestarted = 0;
	NetDevExists = 0;
#endif

	/* XXX problem with bss workaround */
	NetArpWaitPacketMAC = NULL;
	NetArpWaitTxPacket = NULL;
	NetArpWaitPacketIP = 0;
	NetArpWaitReplyIP = 0;
	NetArpWaitTxPacket = NULL;
	NetTxPacket = NULL;
	Tftp_stop = 0;
	if (!NetTxPacket) {
		int	i;
		/*
		 *	Setup packet buffers, aligned correctly.
		 */
		NetTxPacket = &PktBuf[0] + (PKTALIGN - 1);
		NetTxPacket -= (ulong)NetTxPacket % PKTALIGN;
		for (i = 0; i < PKTBUFSRX; i++) {
			NetRxPackets[i] = NetTxPacket + (i+1)*PKTSIZE_ALIGN;
		}
	}

	if (!NetArpWaitTxPacket) {
		NetArpWaitTxPacket = &NetArpWaitPacketBuf[0] + (PKTALIGN - 1);
		NetArpWaitTxPacket -= (ulong)NetArpWaitTxPacket % PKTALIGN;
		NetArpWaitTxPacketSize = 0;
	}
#if defined(CFG_ATHRS26_PHY) && defined(CFG_ATHRHDR_EN)
	if(!AthrHdr_Flag) {
	        eth_halt();
		if (eth_init(bd) < 0) {
            	    eth_halt();
               	 return(-1);
        	}
		AthrHdr_Flag = 1;
	}
#else
	eth_halt();
#ifdef CONFIG_NET_MULTI
#if defined(CFG_VITESSE_73XX_NOPHY) || defined(CFG_REH132)
	/*
	 * There is no PHY in the DNI AP83 board with vitesse switch
	 * VSC7395XYV, so set the eth1 interface to switch ports, so
	 * that u-boot can route all the traffic through the switch
	 * ports.
	 */
	setenv("ethact", "eth1");
#else
        setenv("ethact", "eth0");
#endif
	eth_set_current();
#endif
	if (eth_init(bd) < 0) {
		eth_halt();
		return(-1);
	}
#endif

restart:
#ifdef CONFIG_NET_MULTI
	memcpy (NetOurEther, eth_get_dev()->enetaddr, 6);
#else
	memcpy (NetOurEther, bd->bi_enetaddr, 6);
#endif

	NetState = NETLOOP_CONTINUE;

	/*
	 *	Start the ball rolling with the given start function.  From
	 *	here on, this code is a state machine driven by received
	 *	packets and timer events.
	 */

	switch (protocol) {
#if (CONFIG_COMMANDS & CFG_CMD_NFS)
	case NFS:
#endif
#if (CONFIG_COMMANDS & CFG_CMD_PING)
	case PING:
#endif
#if (CONFIG_COMMANDS & CFG_CMD_SNTP)
	case SNTP:
#endif
	case NETCONS:
	case TFTP:
		NetCopyIP(&NetOurIP, &bd->bi_ip_addr);
		NetOurGatewayIP = getenv_IPaddr ("gatewayip");
		NetOurSubnetMask= getenv_IPaddr ("netmask");
		NetOurVLAN = getenv_VLAN("vlan");
		NetOurNativeVLAN = getenv_VLAN("nvlan");

		switch (protocol) {
#if (CONFIG_COMMANDS & CFG_CMD_NFS)
		case NFS:
#endif
		case NETCONS:
		case TFTP:
			NetServerIP = getenv_IPaddr ("serverip");
			break;
#if (CONFIG_COMMANDS & CFG_CMD_PING)
		case PING:
			/* nothing */
			break;
#endif
#if (CONFIG_COMMANDS & CFG_CMD_SNTP)
		case SNTP:
			/* nothing */
			break;
#endif
		default:
			break;
		}

		break;
	case BOOTP:
	case RARP:
		/*
		 * initialize our IP addr to 0 in order to accept ANY
		 * IP addr assigned to us by the BOOTP / RARP server
		 */
		NetOurIP = 0;
		NetServerIP = getenv_IPaddr ("serverip");
		NetOurVLAN = getenv_VLAN("vlan");	/* VLANs must be read */
		NetOurNativeVLAN = getenv_VLAN("nvlan");
	case CDP:
		NetOurVLAN = getenv_VLAN("vlan");	/* VLANs must be read */
		NetOurNativeVLAN = getenv_VLAN("nvlan");
		break;
#if defined(CFG_ATHRS26_PHY) && defined(CFG_ATHRHDR_EN)
	case ATHRHDR:
		athr_hdr_func();
		break;
#endif
	default:
		break;
	}
#if defined(CFG_ATHRS26_PHY) && defined(CFG_ATHRHDR_EN)
	if(protocol == ATHRHDR)
		goto skip_netloop;
#endif

	switch (net_check_prereq (protocol)) {
	case 1:
		/* network not configured */
		eth_halt();
		return (-1);

#ifdef CONFIG_NET_MULTI
	case 2:
		/* network device not configured */
		break;
#endif /* CONFIG_NET_MULTI */

	case 0:
#ifdef CONFIG_NET_MULTI
		NetDevExists = 1;
#endif
		switch (protocol) {
		case TFTP:
			/* always use ARP to get server ethernet address */
			TftpStart();
			break;

#if (CONFIG_COMMANDS & CFG_CMD_DHCP)
		case DHCP:
			/* Start with a clean slate... */
			BootpTry = 0;
			NetOurIP = 0;
			NetServerIP = getenv_IPaddr ("serverip");
			DhcpRequest();		/* Basically same as BOOTP */
			break;
#endif /* CFG_CMD_DHCP */
#ifndef COMPRESSED_UBOOT
		case BOOTP:
			BootpTry = 0;
			BootpRequest ();
			break;

		case RARP:
			RarpTry = 0;
			RarpRequest ();
			break;
#endif
#if (CONFIG_COMMANDS & CFG_CMD_PING)
		case PING:
			PingStart();
			break;
#endif

#if (CONFIG_COMMANDS & CFG_CMD_NFS)
		case NFS:
			NfsStart();
			break;
#endif
#if (CONFIG_COMMANDS & CFG_CMD_CDP)
		case CDP:
			CDPStart();
			break;
#endif
#ifdef CONFIG_NETCONSOLE
		case NETCONS:
			NcStart();
			break;
#endif
#if (CONFIG_COMMANDS & CFG_CMD_SNTP)
		case SNTP:
			SntpStart();
			break;
#endif
		default:
			break;
		}

		NetBootFileXferSize = 0;
		break;
	}

#if defined(CONFIG_MII) || (CONFIG_COMMANDS & CFG_CMD_MII)
#if defined(CFG_FAULT_ECHO_LINK_DOWN) && defined(CONFIG_STATUS_LED) && defined(STATUS_LED_RED)
	/*
	 * Echo the inverted link state to the fault LED.
	 */
	if(miiphy_link(eth_get_dev()->name, CFG_FAULT_MII_ADDR)) {
		status_led_set (STATUS_LED_RED, STATUS_LED_OFF);
	} else {
		status_led_set (STATUS_LED_RED, STATUS_LED_ON);
	}
#endif /* CFG_FAULT_ECHO_LINK_DOWN, ... */
#endif /* CONFIG_MII, ... */

	/*
	 *	Main packet reception loop.  Loop receiving packets until
	 *	someone sets `NetState' to a state that terminates.
	 */
skip_netloop:
	for (;;) {
		WATCHDOG_RESET();
#ifdef CONFIG_SHOW_ACTIVITY
		{
			extern void show_activity(int arg);
			show_activity(1);
		}
#endif
		/*
		 *	Check the ethernet for a new packet.  The ethernet
		 *	receive routine will process it.
		 */
			eth_rx();

		/*
		 *	Abort if ctrl-c was pressed.
		 */
		if (ctrlc()||Tftp_stop) {
			eth_halt();
			puts ("\nAbort\n");
			Tftp_stop = 0;
			return (-1);
		}
#if defined(CFG_ATHRS26_PHY) && defined(CFG_ATHRHDR_EN)
                if(protocol != ATHRHDR)
			ArpTimeoutCheck();
#else
		ArpTimeoutCheck();
#endif

		/*
		 *	Check for a timeout, and run the timeout handler
		 *	if we have one.
		 */
		if (timeHandler && ((get_timer(0) - timeStart) > timeDelta)) {
			thand_f *x;
#if !defined(CFG_ATHRS26_PHY) && !defined(CFG_ATHRHDR_EN)
#if defined(CONFIG_MII) || (CONFIG_COMMANDS & CFG_CMD_MII)
#  if defined(CFG_FAULT_ECHO_LINK_DOWN) && \
      defined(CONFIG_STATUS_LED) &&	   \
      defined(STATUS_LED_RED)
			/*
			 * Echo the inverted link state to the fault LED.
			 */
			if(miiphy_link(eth_get_dev()->name, CFG_FAULT_MII_ADDR)) {
				status_led_set (STATUS_LED_RED, STATUS_LED_OFF);
			} else {
				status_led_set (STATUS_LED_RED, STATUS_LED_ON);
			}
#  endif /* CFG_FAULT_ECHO_LINK_DOWN, ... */
#endif /* CONFIG_MII, ... */
#endif

			x = timeHandler;
			timeHandler = (thand_f *)0;
			(*x)();
			return -1;
		}
		
		switch (NetState) {

		case NETLOOP_RESTART:
#ifdef CONFIG_NET_MULTI
			NetRestarted = 1;
#endif
			goto restart;

		case NETLOOP_SUCCESS:
#if defined(CFG_ATHRS26_PHY) && defined(CFG_ATHRHDR_EN)
			if(protocol == ATHRHDR)
				return 1;
#endif

			if (NetBootFileXferSize > 0) {
				char buf[10];
				printf("Bytes transferred = %ld (%lx hex)\n",
					NetBootFileXferSize,
					NetBootFileXferSize);

				sprintf(buf, "%lx", NetBootFileXferSize);
				setenv("filesize", buf);
				if(NetBootFileXferSize >= 0x200000){
					sprintf(buf, "%lx", NetBootFileXferSize-0x200000);
					setenv("rootfs_size", buf);
				}
				sprintf(buf, "%lX", (unsigned long)load_addr);
				setenv("fileaddr", buf);
			}
			eth_halt();
			return NetBootFileXferSize;

		case NETLOOP_FAIL:
			return (-1);
		}
	}
}

/**********************************************************************/

static void
startAgainTimeout(void)
{
	NetState = NETLOOP_RESTART;
}

static void
startAgainHandler(uchar * pkt, unsigned dest, unsigned src, unsigned len)
{
	/* Totally ignore the packet */
}

void NetStartAgain (void)
{
	char *nretry;
	int noretry = 0, once = 0;

	if ((nretry = getenv ("netretry")) != NULL) {
		noretry = (strcmp (nretry, "no") == 0);
		once = (strcmp (nretry, "once") == 0);
	}
	if (noretry) {
		eth_halt ();
		NetState = NETLOOP_FAIL;
		return;
	}
#ifndef CONFIG_NET_MULTI
	NetSetTimeout (10 * CFG_HZ, startAgainTimeout);
	NetSetHandler (startAgainHandler);
#else	/* !CONFIG_NET_MULTI*/
	eth_halt ();
	eth_try_another (!NetRestarted);
	eth_init (gd->bd);
	if (NetRestartWrap) {
		NetRestartWrap = 0;
		if (NetDevExists && !once) {
			NetSetTimeout (10 * CFG_HZ, startAgainTimeout);
			NetSetHandler (startAgainHandler);
		} else {
			NetState = NETLOOP_FAIL;
		}
	} else {
		NetState = NETLOOP_RESTART;
	}
#endif	/* CONFIG_NET_MULTI */
}

/**********************************************************************/
/*
 *	Miscelaneous bits.
 */

void
NetSetHandler(rxhand_f * f)
{
	packetHandler = f;
}


void
NetSetTimeout(ulong iv, thand_f * f)
{
	if (iv == 0) {
		timeHandler = (thand_f *)0;
	} else {
		timeHandler = f;
		timeStart = get_timer(0);
		timeDelta = iv;
	}
}


void
NetSendPacket(volatile uchar * pkt, int len)
{
	(void) eth_send(pkt, len);
}

int
NetSendUDPPacket(uchar *ether, IPaddr_t dest, int dport, int sport, int len)
{
	uchar *pkt;

	/* convert to new style broadcast */
	if (dest == 0)
		dest = 0xFFFFFFFF;

	/* if broadcast, make the ether address a broadcast and don't do ARP */
	if (dest == 0xFFFFFFFF)
		ether = NetBcastAddr;

	/* if MAC address was not discovered yet, save the packet and do an ARP request */
	if (memcmp(ether, NetEtherNullAddr, 6) == 0) {

#ifdef ET_DEBUG
		printf("sending ARP for %08lx\n", dest);
#endif
		NetArpWaitPacketIP = dest;
		NetArpWaitPacketMAC = ether;

		pkt = NetArpWaitTxPacket;
		pkt += NetSetEther (pkt, NetArpWaitPacketMAC, PROT_IP);

		NetSetIP (pkt, dest, dport, sport, len);
		memcpy(pkt + IP_HDR_SIZE, (uchar *)NetTxPacket + (pkt - (uchar *)NetArpWaitTxPacket) + IP_HDR_SIZE, len);

		/* size of the waiting packet */
		NetArpWaitTxPacketSize = (pkt - NetArpWaitTxPacket) + IP_HDR_SIZE + len;

		/* and do the ARP request */
		NetArpWaitTry = 1;
		NetArpWaitTimerStart = get_timer(0);
		ArpRequest();
		return 1;	/* waiting */
	}

#ifdef ET_DEBUG
	printf("sending UDP to %08lx/%02x:%02x:%02x:%02x:%02x:%02x\n",
		dest, ether[0], ether[1], ether[2], ether[3], ether[4], ether[5]);
#endif

	pkt = (uchar *)NetTxPacket;
	pkt += NetSetEther (pkt, ether, PROT_IP);
	NetSetIP (pkt, dest, dport, sport, len);
	(void) eth_send(NetTxPacket, (pkt - NetTxPacket) + IP_HDR_SIZE + len);

	return 0;	/* transmitted */
}

#if (CONFIG_COMMANDS & CFG_CMD_PING)
static ushort PingSeqNo;

int PingSend(void)
{
	static uchar mac[6];
	volatile IP_t *ip;
	volatile ushort *s;
	uchar *pkt;

	/* XXX always send arp request */

	memcpy(mac, NetEtherNullAddr, 6);

#ifdef ET_DEBUG
	printf("sending ARP for %08lx\n", NetPingIP);
#endif

	NetArpWaitPacketIP = NetPingIP;
	NetArpWaitPacketMAC = mac;

	pkt = NetArpWaitTxPacket;
	pkt += NetSetEther(pkt, mac, PROT_IP);

	ip = (volatile IP_t *)pkt;

	/*
	 *	Construct an IP and ICMP header.  (need to set no fragment bit - XXX)
	 */
	ip->ip_hl_v  = 0x45;		/* IP_HDR_SIZE / 4 (not including UDP) */
	ip->ip_tos   = 0;
	ip->ip_len   = htons(IP_HDR_SIZE_NO_UDP + 8);
	ip->ip_id    = htons(NetIPID++);
	ip->ip_off   = htons(0x4000);	/* No fragmentation */
	ip->ip_ttl   = 255;
	ip->ip_p     = 0x01;		/* ICMP */
	ip->ip_sum   = 0;
	NetCopyIP((void*)&ip->ip_src, &NetOurIP); /* already in network byte order */
	NetCopyIP((void*)&ip->ip_dst, &NetPingIP);	   /* - "" - */
	ip->ip_sum   = ~NetCksum((uchar *)ip, IP_HDR_SIZE_NO_UDP / 2);

	s = &ip->udp_src;		/* XXX ICMP starts here */
	s[0] = htons(0x0800);		/* echo-request, code */
	s[1] = 0;			/* checksum */
	s[2] = 0; 			/* identifier */
	s[3] = htons(PingSeqNo++);	/* sequence number */
	s[1] = ~NetCksum((uchar *)s, 8/2);

	/* size of the waiting packet */
	NetArpWaitTxPacketSize = (pkt - NetArpWaitTxPacket) + IP_HDR_SIZE_NO_UDP + 8;

	/* and do the ARP request */
	NetArpWaitTry = 1;
	NetArpWaitTimerStart = get_timer(0);
	ArpRequest();
	return 1;	/* waiting */
}

static void
PingTimeout (void)
{
	eth_halt();
	NetState = NETLOOP_FAIL;	/* we did not get the reply */
}

static void
PingHandler (uchar * pkt, unsigned dest, unsigned src, unsigned len)
{
	IPaddr_t tmp;
	volatile IP_t *ip = (volatile IP_t *)pkt;

	tmp = NetReadIP((void *)&ip->ip_src);
	if (tmp != NetPingIP)
		return;

	NetState = NETLOOP_SUCCESS;
}

static void PingStart(void)
{
#if defined(CONFIG_NET_MULTI)
	printf ("Using %s device\n", eth_get_name());
#endif	/* CONFIG_NET_MULTI */
	NetSetTimeout ( 3*CFG_HZ, PingTimeout);
	NetSetHandler (PingHandler);

	PingSend();
}
#endif	/* CFG_CMD_PING */

#if (CONFIG_COMMANDS & CFG_CMD_CDP)

#define CDP_DEVICE_ID_TLV		0x0001
#define CDP_ADDRESS_TLV			0x0002
#define CDP_PORT_ID_TLV			0x0003
#define CDP_CAPABILITIES_TLV		0x0004
#define CDP_VERSION_TLV			0x0005
#define CDP_PLATFORM_TLV		0x0006
#define CDP_NATIVE_VLAN_TLV		0x000a
#define CDP_APPLIANCE_VLAN_TLV		0x000e
#define CDP_TRIGGER_TLV			0x000f
#define CDP_POWER_CONSUMPTION_TLV	0x0010
#define CDP_SYSNAME_TLV			0x0014
#define CDP_SYSOBJECT_TLV		0x0015
#define CDP_MANAGEMENT_ADDRESS_TLV	0x0016

#define CDP_TIMEOUT			(CFG_HZ/4)	/* one packet every 250ms */

static int CDPSeq;
static int CDPOK;

ushort CDPNativeVLAN;
ushort CDPApplianceVLAN;

static const uchar CDP_SNAP_hdr[8] = { 0xAA, 0xAA, 0x03, 0x00, 0x00, 0x0C, 0x20, 0x00 };

static ushort CDP_compute_csum(const uchar *buff, ushort len)
{
	ushort csum;
	int     odd;
	ulong   result = 0;
	ushort  leftover;
	ushort *p;

	if (len > 0) {
		odd = 1 & (ulong)buff;
		if (odd) {
			result = *buff << 8;
			len--;
			buff++;
		}
		while (len > 1) {
			p = (ushort *)buff;
			result += *p++;
			buff = (uchar *)p;
			if (result & 0x80000000)
				result = (result & 0xFFFF) + (result >> 16);
			len -= 2;
		}
		if (len) {
			leftover = (signed short)(*(const signed char *)buff);
			/* CISCO SUCKS big time! (and blows too):
			 * CDP uses the IP checksum algorithm with a twist;
			 * for the last byte it *sign* extends and sums.
			 */
			result = (result & 0xffff0000) | ((result + leftover) & 0x0000ffff);
		}
		while (result >> 16)
			result = (result & 0xFFFF) + (result >> 16);

		if (odd)
			result = ((result >> 8) & 0xff) | ((result & 0xff) << 8);
	}

	/* add up 16-bit and 17-bit words for 17+c bits */
	result = (result & 0xffff) + (result >> 16);
	/* add up 16-bit and 2-bit for 16+c bit */
	result = (result & 0xffff) + (result >> 16);
	/* add up carry.. */
	result = (result & 0xffff) + (result >> 16);

	/* negate */
	csum = ~(ushort)result;

	/* run time endian detection */
	if (csum != htons(csum))	/* little endian */
		csum = htons(csum);

	return csum;
}

int CDPSendTrigger(void)
{
	volatile uchar *pkt;
	volatile ushort *s;
	volatile ushort *cp;
	Ethernet_t *et;
	int len;
	ushort chksum;
#if defined(CONFIG_CDP_DEVICE_ID) || defined(CONFIG_CDP_PORT_ID)   || \
    defined(CONFIG_CDP_VERSION)   || defined(CONFIG_CDP_PLATFORM)
	char buf[32];
#endif

	pkt = NetTxPacket;
	et = (Ethernet_t *)pkt;

	/* NOTE: trigger sent not on any VLAN */

	/* form ethernet header */
	memcpy(et->et_dest, NetCDPAddr, 6);
	memcpy(et->et_src, NetOurEther, 6);

	pkt += ETHER_HDR_SIZE;

	/* SNAP header */
	memcpy((uchar *)pkt, CDP_SNAP_hdr, sizeof(CDP_SNAP_hdr));
	pkt += sizeof(CDP_SNAP_hdr);

	/* CDP header */
	*pkt++ = 0x02;				/* CDP version 2 */
	*pkt++ = 180;				/* TTL */
	s = (volatile ushort *)pkt;
	cp = s;
	*s++ = htons(0);			/* checksum (0 for later calculation) */

	/* CDP fields */
#ifdef CONFIG_CDP_DEVICE_ID
	*s++ = htons(CDP_DEVICE_ID_TLV);
	*s++ = htons(CONFIG_CDP_DEVICE_ID);
	memset(buf, 0, sizeof(buf));
	sprintf(buf, CONFIG_CDP_DEVICE_ID_PREFIX "%02X%02X%02X%02X%02X%02X",
		NetOurEther[0] & 0xff, NetOurEther[1] & 0xff,
		NetOurEther[2] & 0xff, NetOurEther[3] & 0xff,
		NetOurEther[4] & 0xff, NetOurEther[5] & 0xff);
	memcpy((uchar *)s, buf, 16);
	s += 16 / 2;
#endif

#ifdef CONFIG_CDP_PORT_ID
	*s++ = htons(CDP_PORT_ID_TLV);
	memset(buf, 0, sizeof(buf));
	sprintf(buf, CONFIG_CDP_PORT_ID, eth_get_dev_index());
	len = strlen(buf);
	if (len & 1)	/* make it even */
		len++;
	*s++ = htons(len + 4);
	memcpy((uchar *)s, buf, len);
	s += len / 2;
#endif

#ifdef CONFIG_CDP_CAPABILITIES
	*s++ = htons(CDP_CAPABILITIES_TLV);
	*s++ = htons(8);
	*(ulong *)s = htonl(CONFIG_CDP_CAPABILITIES);
	s += 2;
#endif

#ifdef CONFIG_CDP_VERSION
	*s++ = htons(CDP_VERSION_TLV);
	memset(buf, 0, sizeof(buf));
	strcpy(buf, CONFIG_CDP_VERSION);
	len = strlen(buf);
	if (len & 1)	/* make it even */
		len++;
	*s++ = htons(len + 4);
	memcpy((uchar *)s, buf, len);
	s += len / 2;
#endif

#ifdef CONFIG_CDP_PLATFORM
	*s++ = htons(CDP_PLATFORM_TLV);
	memset(buf, 0, sizeof(buf));
	strcpy(buf, CONFIG_CDP_PLATFORM);
	len = strlen(buf);
	if (len & 1)	/* make it even */
		len++;
	*s++ = htons(len + 4);
	memcpy((uchar *)s, buf, len);
	s += len / 2;
#endif

#ifdef CONFIG_CDP_TRIGGER
	*s++ = htons(CDP_TRIGGER_TLV);
	*s++ = htons(8);
	*(ulong *)s = htonl(CONFIG_CDP_TRIGGER);
	s += 2;
#endif

#ifdef CONFIG_CDP_POWER_CONSUMPTION
	*s++ = htons(CDP_POWER_CONSUMPTION_TLV);
	*s++ = htons(6);
	*s++ = htons(CONFIG_CDP_POWER_CONSUMPTION);
#endif

	/* length of ethernet packet */
	len = (uchar *)s - ((uchar *)NetTxPacket + ETHER_HDR_SIZE);
	et->et_protlen = htons(len);

	len = ETHER_HDR_SIZE + sizeof(CDP_SNAP_hdr);
	chksum = CDP_compute_csum((uchar *)NetTxPacket + len, (uchar *)s - (NetTxPacket + len));
	if (chksum == 0)
		chksum = 0xFFFF;
	*cp = htons(chksum);

	(void) eth_send(NetTxPacket, (uchar *)s - NetTxPacket);
	return 0;
}

static void
CDPTimeout (void)
{
	CDPSeq++;

	if (CDPSeq < 3) {
		NetSetTimeout (CDP_TIMEOUT, CDPTimeout);
		CDPSendTrigger();
		return;
	}

	/* if not OK try again */
	if (!CDPOK)
		NetStartAgain();
	else
		NetState = NETLOOP_SUCCESS;
}

static void
CDPDummyHandler (uchar * pkt, unsigned dest, unsigned src, unsigned len)
{
	/* nothing */
}

static void
CDPHandler(const uchar * pkt, unsigned len)
{
	const uchar *t;
	const ushort *ss;
	ushort type, tlen;
	uchar applid;
	ushort vlan, nvlan;

	/* minimum size? */
	if (len < sizeof(CDP_SNAP_hdr) + 4)
		goto pkt_short;

	/* check for valid CDP SNAP header */
	if (memcmp(pkt, CDP_SNAP_hdr, sizeof(CDP_SNAP_hdr)) != 0)
		return;

	pkt += sizeof(CDP_SNAP_hdr);
	len -= sizeof(CDP_SNAP_hdr);

	/* Version of CDP protocol must be >= 2 and TTL != 0 */
	if (pkt[0] < 0x02 || pkt[1] == 0)
		return;

	/* if version is greater than 0x02 maybe we'll have a problem; output a warning */
	if (pkt[0] != 0x02)
		printf("** WARNING: CDP packet received with a protocol version %d > 2\n",
				pkt[0] & 0xff);

	if (CDP_compute_csum(pkt, len) != 0)
		return;

	pkt += 4;
	len -= 4;

	vlan = htons(-1);
	nvlan = htons(-1);
	while (len > 0) {
		if (len < 4)
			goto pkt_short;

		ss = (const ushort *)pkt;
		type = ntohs(ss[0]);
		tlen = ntohs(ss[1]);
		if (tlen > len) {
			goto pkt_short;
		}

		pkt += tlen;
		len -= tlen;

		ss += 2;	/* point ss to the data of the TLV */
		tlen -= 4;

		switch (type) {
			case CDP_DEVICE_ID_TLV:
				break;
			case CDP_ADDRESS_TLV:
				break;
			case CDP_PORT_ID_TLV:
				break;
			case CDP_CAPABILITIES_TLV:
				break;
			case CDP_VERSION_TLV:
				break;
			case CDP_PLATFORM_TLV:
				break;
			case CDP_NATIVE_VLAN_TLV:
				nvlan = *ss;
				break;
			case CDP_APPLIANCE_VLAN_TLV:
				t = (const uchar *)ss;
				while (tlen > 0) {
					if (tlen < 3)
						goto pkt_short;

					applid = t[0];
					ss = (const ushort *)(t + 1);

#ifdef CONFIG_CDP_APPLIANCE_VLAN_TYPE
					if (applid == CONFIG_CDP_APPLIANCE_VLAN_TYPE)
						vlan = *ss;
#else
					vlan = ntohs(*ss);	/* XXX will this work; dunno */
#endif
					t += 3; tlen -= 3;
				}
				break;
			case CDP_TRIGGER_TLV:
				break;
			case CDP_POWER_CONSUMPTION_TLV:
				break;
			case CDP_SYSNAME_TLV:
				break;
			case CDP_SYSOBJECT_TLV:
				break;
			case CDP_MANAGEMENT_ADDRESS_TLV:
				break;
		}
	}

	CDPApplianceVLAN = vlan;
	CDPNativeVLAN = nvlan;

	CDPOK = 1;
	return;

 pkt_short:
	printf("** CDP packet is too short\n");
	return;
}

static void CDPStart(void)
{
#if defined(CONFIG_NET_MULTI)
	printf ("Using %s device\n", eth_get_name());
#endif
	CDPSeq = 0;
	CDPOK = 0;

	CDPNativeVLAN = htons(-1);
	CDPApplianceVLAN = htons(-1);

	NetSetTimeout (CDP_TIMEOUT, CDPTimeout);
	NetSetHandler (CDPDummyHandler);

	CDPSendTrigger();
}
#endif	/* CFG_CMD_CDP */


char gl_probe_upgrade=0;
char upgrade_listen=0;

static char gl_cmd_msg[5][256]={0}; 



void gl_upgrade_send_msg(char *msg)
{
    char i = 0;
	char msg_len=strlen(msg);
    unsigned char rep[2][42]={
		{ 0xff,0xff,0xff,0xff,0xff,0xff,0x14,0x6b,0x9c,0xb7,0x12,0x30,0x08,0x00,0x45,0x00,0x00,0x4d,0x00,0x01,0x00,0x00,0x40,0x01,0xb9,0x06,0xc0,0xa8,0x01,0x01,0xff,0xff,0xff,0xff,0x08,0x00,0xfa,0xb1,0x00,0x01,0x00,0x01},
		{0x00 }
	};
	struct eth_device *eth = eth_get_dev();
	if(eth->state == ETH_STATE_PASSIVE){//命令执行过程中可能会导致网卡关闭，需要重新初始化
			bd_t *bd = gd->bd;
			eth_init(bd);
			udelay(100000);
		}
	memcpy(rep[1],msg,msg_len);
    //dev_init_up();
    for(i=0;i<5;i++){
        NetTxPacket = KSEG1ADDR(NetTxPacket);
        memcpy((void *)NetTxPacket, (unsigned char *)rep, 42 + msg_len);
        eth_send(NetTxPacket, 42 + msg_len);
		udelay (10000);
    }
    //printf("send msg");
}


char get_crc_param(char *buf,char *result,char num)
{
	int i=0;
	int cunt=-1;
	char *start=buf;
	int len=0;
	while((buf[i] != '\0') && (buf[i] != '\r') && (buf[i] != '\n') ){
		if(buf[i] == ','){
			if(++cunt == num){
				len = buf+i-start;
				memcpy(result,start,len);
				result[len]='\0';
				return 0;
			}
			start=buf+i+1;
		}
		i++;
	}
	if((buf[i] == '\0') && (++cunt == num)){//get the end param
		len = buf+i-start;
		memcpy(result,start,len);
		result[len]='\0';
		return 0;
	}
	return -1;
}

int gl_upgrade_cmd_handle(char *cmd)
{
	if(strncmp(cmd,"scan",4)==0){
		if (upgrade_listen == 0){//只有第一次收到scan才回复hello,防止上位机被打断时产生异常
			gl_upgrade_send_msg("glroute:hello");
			upgrade_listen = 1;
			printf("glinet scan\n");
			}
	}
	else if(strncmp(cmd,"cmd-",4)==0){
		int i=0;
		for(i=0;i<5;i++){
			if(strlen(gl_cmd_msg[i])==0){
				strcpy(gl_cmd_msg[i],cmd+4);
				gl_upgrade_send_msg("glroute:ok");
				printf("\nCMD:%s\n",gl_cmd_msg[i]);
				break;
			}
		}
		if( i >= 5 )
			gl_upgrade_send_msg("glroute:err-no_space");
	}
	else if (strncmp(cmd,"do-",3)==0){
		int i=0;
		for(i=0;i<5;i++){
			if(strlen(gl_cmd_msg[i])){
				printf("\nDo cmd\n");
				setenv("gl_do_cmd",gl_cmd_msg[i]);
				run_command("run gl_do_cmd", 0);
            }
			else{
				break;
			}
		}
        memset(gl_cmd_msg,0,sizeof(gl_cmd_msg));
        gl_upgrade_send_msg("glroute:ok");
	}
	else if (strncmp(cmd,"dhcp",4)==0){
		run_command("dhcpd start", 0);
		gl_upgrade_send_msg("glroute:ok");
	}
	else if (strncmp(cmd,"crc-",4)==0){
		char str_value[16]={0};
		ulong addr, length,raw,crc;
		get_crc_param(cmd+4,str_value,0);
		addr  = simple_strtoul(str_value,NULL,16);
		get_crc_param(cmd+4,str_value,1);
		length = simple_strtoul(str_value,NULL,16);
		get_crc_param(cmd+4,str_value,2);
		raw = simple_strtoul(str_value,NULL,16);
		crc = crc32 (0, (const uchar *) addr, length);
		printf("crc:%x,%x,%x,%x\n",addr,length,raw,crc);
		if(crc == raw){
			gl_upgrade_send_msg("glroute:ok");
			gl_probe_upgrade = 0;//升级成功，退出升级模式，防止重复刷机
			upgrade_listen = 0;
		}
		else{
			char err_msg[32]={0};
			sprintf(err_msg,"glroute:err-crc_%x",crc);
			gl_upgrade_send_msg(err_msg);
		}
			
	}

	return 0;
}

void gl_upgrade_hook(volatile uchar * inpkt, int len)
{
	unsigned char pk_buf[1518]={0};
	//int i=0;
	memcpy(pk_buf, inpkt, len);
	/*for(i=0;i<len;i++){
		printf("%d:%02X,",i,pk_buf[i]);
	}*/
	if(strstr(pk_buf+42,"glinet:")){
		//printf("recv gl cmd\n");
		gl_upgrade_cmd_handle(pk_buf+42+7);
	}
}

void gl_upgrade_probe()
{
	eth_rx();
}

void gl_upgrade_listen()
{
	while(upgrade_listen && gl_probe_upgrade)
		eth_rx();
}

void
NetReceive(volatile uchar * inpkt, int len)
{
	Ethernet_t *et;
	IP_t	*ip;
	ARP_t	*arp;
	IPaddr_t tmp;
	int	x;
	uchar *pkt;
#if defined(CFG_ATHRS26_PHY) && defined(CFG_ATHRHDR_EN)
        uint8_t type;
#endif
#if (CONFIG_COMMANDS & CFG_CMD_CDP)
	int iscdp;
#endif
	ushort cti = 0, vlanid = VLAN_NONE, myvlanid, mynvlanid;

#ifdef ET_DEBUG
	printf("packet received\n");
#endif
	if(gl_probe_upgrade){
		gl_probe_upgrade = 0;//执行命令时先停止接受指令，防止命令出错
		gl_upgrade_hook(inpkt, len);
		gl_probe_upgrade = 1;
		return;
	}

    if(NetUipLoop) {
		dev_received(inpkt, len);
		return;
	}

	if(webfailsafe_is_running){
		NetReceiveHttpd(inpkt, len);
		return;
	}

#if defined(CFG_ATHRS26_PHY) && defined(CFG_ATHRHDR_EN)
	type = (inpkt[1] & 0xf);
	/* check for ack */
       if(type == 0x6){
               (*packetHandler)(inpkt,0,0,0);
		return;
	}
	else if (type == 0x0) {
	   inpkt = inpkt + ATHRHDR_LEN;  /* Remove ATHRHDR */
	   len = len - ATHRHDR_LEN;
	}
	else{
		printf("Packet dropped! Type invalid.\n");
		return;
	}
#endif

        NetRxPkt = inpkt;
	NetRxPktLen = len;
        et = (Ethernet_t *)inpkt;

	/* too small packet? */
	if (len < ETHER_HDR_SIZE)
		return;

#if (CONFIG_COMMANDS & CFG_CMD_CDP)
	/* keep track if packet is CDP */
	iscdp = memcmp(et->et_dest, NetCDPAddr, 6) == 0;
#endif

	myvlanid = ntohs(NetOurVLAN);
	if (myvlanid == (ushort)-1)
		myvlanid = VLAN_NONE;
	mynvlanid = ntohs(NetOurNativeVLAN);
	if (mynvlanid == (ushort)-1)
		mynvlanid = VLAN_NONE;

	x = ntohs(et->et_protlen);

#ifdef ET_DEBUG
	printf("packet received2\n");
#endif

	if (x < 1514) {
		/*
		 *	Got a 802 packet.  Check the other protocol field.
		 */
		x = ntohs(et->et_prot);

		ip = (IP_t *)(inpkt + E802_HDR_SIZE);
		len -= E802_HDR_SIZE;

	} else if (x != PROT_VLAN) {	/* normal packet */
		ip = (IP_t *)(inpkt + ETHER_HDR_SIZE);
		len -= ETHER_HDR_SIZE;

	} else {			/* VLAN packet */
		VLAN_Ethernet_t *vet = (VLAN_Ethernet_t *)et;

#ifdef ET_DEBUG
		printf("VLAN packet received\n");
#endif
		/* too small packet? */
		if (len < VLAN_ETHER_HDR_SIZE)
			return;

		/* if no VLAN active */
		if ((ntohs(NetOurVLAN) & VLAN_IDMASK) == VLAN_NONE
#if (CONFIG_COMMANDS & CFG_CMD_CDP)
				&& iscdp == 0
#endif
				)
			return;

		cti = ntohs(vet->vet_tag);
		vlanid = cti & VLAN_IDMASK;
		x = ntohs(vet->vet_type);

		ip = (IP_t *)(inpkt + VLAN_ETHER_HDR_SIZE);
		len -= VLAN_ETHER_HDR_SIZE;
	}

#ifdef ET_DEBUG
	printf("Receive from protocol 0x%x\n", x);
#endif

#if (CONFIG_COMMANDS & CFG_CMD_CDP)
	if (iscdp) {
		CDPHandler((uchar *)ip, len);
		return;
	}
#endif

	if ((myvlanid & VLAN_IDMASK) != VLAN_NONE) {
		if (vlanid == VLAN_NONE)
			vlanid = (mynvlanid & VLAN_IDMASK);
		/* not matched? */
		if (vlanid != (myvlanid & VLAN_IDMASK))
			return;
	}

	switch (x) {

	case PROT_ARP:
		/*
		 * We have to deal with two types of ARP packets:
		 * - REQUEST packets will be answered by sending  our
		 *   IP address - if we know it.
		 * - REPLY packates are expected only after we asked
		 *   for the TFTP server's or the gateway's ethernet
		 *   address; so if we receive such a packet, we set
		 *   the server ethernet address
		 */
#ifdef ET_DEBUG
		puts ("Got ARP\n");
#endif
		arp = (ARP_t *)ip;
		if (len < ARP_HDR_SIZE) {
			printf("bad length %d < %d\n", len, ARP_HDR_SIZE);
			return;
		}
		if (ntohs(arp->ar_hrd) != ARP_ETHER) {
			return;
		}
		if (ntohs(arp->ar_pro) != PROT_IP) {
			return;
		}
		if (arp->ar_hln != 6) {
			return;
		}
		if (arp->ar_pln != 4) {
			return;
		}

		if (NetOurIP == 0) {
			return;
		}

		if (NetReadIP(&arp->ar_data[16]) != NetOurIP) {
			return;
		}

		switch (ntohs(arp->ar_op)) {
		case ARPOP_REQUEST:		/* reply with our IP address	*/
#ifdef ET_DEBUG
			puts ("Got ARP REQUEST, return our IP\n");
#endif
			pkt = (uchar *)et;
			pkt += NetSetEther(pkt, et->et_src, PROT_ARP);
			arp->ar_op = htons(ARPOP_REPLY);
			memcpy   (&arp->ar_data[10], &arp->ar_data[0], 6);
			NetCopyIP(&arp->ar_data[16], &arp->ar_data[6]);
			memcpy   (&arp->ar_data[ 0], NetOurEther, 6);
			NetCopyIP(&arp->ar_data[ 6], &NetOurIP);
			(void) eth_send((uchar *)et, (pkt - (uchar *)et) + ARP_HDR_SIZE);
			return;

		case ARPOP_REPLY:		/* arp reply */
			/* are we waiting for a reply */
			if (!NetArpWaitPacketIP || !NetArpWaitPacketMAC)
				break;
#ifdef ET_DEBUG
			printf("Got ARP REPLY, set server/gtwy eth addr (%02x:%02x:%02x:%02x:%02x:%02x)\n",
				arp->ar_data[0], arp->ar_data[1],
				arp->ar_data[2], arp->ar_data[3],
				arp->ar_data[4], arp->ar_data[5]);
#endif

			tmp = NetReadIP(&arp->ar_data[6]);

			/* matched waiting packet's address */
			if (tmp == NetArpWaitReplyIP) {
#ifdef ET_DEBUG
				puts ("Got it\n");
#endif
				/* save address for later use */
				memcpy(NetArpWaitPacketMAC, &arp->ar_data[0], 6);

#ifdef CONFIG_NETCONSOLE
				(*packetHandler)(0,0,0,0);
#endif
				/* modify header, and transmit it */
                                if (NetArpWaitTxPacketSize > 0) {
                                        memcpy(((Ethernet_t *)NetArpWaitTxPacket)->et_dest, NetArpWaitPacketMAC, 6);
                                        (void) eth_send(NetArpWaitTxPacket, NetArpWaitTxPacketSize);
                                }

				/* no arp request pending now */
				NetArpWaitPacketIP = 0;
				NetArpWaitTxPacketSize = 0;
				NetArpWaitPacketMAC = NULL;

			}
			return;
		default:
#ifdef ET_DEBUG
			printf("Unexpected ARP opcode 0x%x\n", ntohs(arp->ar_op));
#endif
			return;
		}
		break;

	case PROT_RARP:
#ifdef ET_DEBUG
		puts ("Got RARP\n");
#endif
		arp = (ARP_t *)ip;
		if (len < ARP_HDR_SIZE) {
			printf("bad length %d < %d\n", len, ARP_HDR_SIZE);
			return;
		}

		if ((ntohs(arp->ar_op) != RARPOP_REPLY) ||
			(ntohs(arp->ar_hrd) != ARP_ETHER)   ||
			(ntohs(arp->ar_pro) != PROT_IP)     ||
			(arp->ar_hln != 6) || (arp->ar_pln != 4)) {

			puts ("invalid RARP header\n");
		} else {
			NetCopyIP(&NetOurIP,    &arp->ar_data[16]);
			if (NetServerIP == 0)
				NetCopyIP(&NetServerIP, &arp->ar_data[ 6]);
			memcpy (NetServerEther, &arp->ar_data[ 0], 6);

			(*packetHandler)(0,0,0,0);
		}
		break;

	case PROT_IP:
#ifdef ET_DEBUG
		puts ("Got IP\n");
#endif
		if (len < IP_HDR_SIZE) {
			debug ("len bad %d < %d\n", len, IP_HDR_SIZE);
			return;
		}
		if (len < ntohs(ip->ip_len)) {
			printf("len bad %d < %d\n", len, ntohs(ip->ip_len));
			return;
		}
		len = ntohs(ip->ip_len);
#ifdef ET_DEBUG
		printf("len=%d, v=%02x\n", len, ip->ip_hl_v & 0xff);
#endif
		if ((ip->ip_hl_v & 0xf0) != 0x40) {
			return;
		}
		if (ip->ip_off & htons(0x1fff)) { /* Can't deal w/ fragments */
			return;
		}
		if (!NetCksumOk((uchar *)ip, IP_HDR_SIZE_NO_UDP / 2)) {
			puts ("checksum bad\n");
			return;
		}
		tmp = NetReadIP(&ip->ip_dst);
		if (NetOurIP && tmp != NetOurIP && tmp != 0xFFFFFFFF) {
			return;
		}
		/*
		 * watch for ICMP host redirects
		 *
		 * There is no real handler code (yet). We just watch
		 * for ICMP host redirect messages. In case anybody
		 * sees these messages: please contact me
		 * (wd@denx.de), or - even better - send me the
		 * necessary fixes :-)
		 *
		 * Note: in all cases where I have seen this so far
		 * it was a problem with the router configuration,
		 * for instance when a router was configured in the
		 * BOOTP reply, but the TFTP server was on the same
		 * subnet. So this is probably a warning that your
		 * configuration might be wrong. But I'm not really
		 * sure if there aren't any other situations.
		 */
		if (ip->ip_p == IPPROTO_ICMP) {
			ICMP_t *icmph = (ICMP_t *)&(ip->udp_src);

			switch (icmph->type) {
			case ICMP_REDIRECT:
				if (icmph->code != ICMP_REDIR_HOST)
					return;
				puts (" ICMP Host Redirect to ");
				print_IPaddr(icmph->un.gateway);
				putc(' ');
				return;
#if (CONFIG_COMMANDS & CFG_CMD_PING)
			case ICMP_ECHO_REPLY:
				/*
				 *	IP header OK.  Pass the packet to the current handler.
				 */
				/* XXX point to ip packet */
				(*packetHandler)((uchar *)ip, 0, 0, 0);
				return;
#endif
			default:
				return;
			}
		} else if (ip->ip_p != IPPROTO_UDP) {	/* Only UDP packets */
			return;
		}
        

#ifdef CONFIG_UDP_CHECKSUM
		if (ip->udp_xsum != 0) {
			ulong   xsum;
			ushort *sumptr;
			ushort  sumlen;

			xsum  = ip->ip_p;
			xsum += (ntohs(ip->udp_len));
			xsum += (ntohl(ip->ip_src) >> 16) & 0x0000ffff;
			xsum += (ntohl(ip->ip_src) >>  0) & 0x0000ffff;
			xsum += (ntohl(ip->ip_dst) >> 16) & 0x0000ffff;
			xsum += (ntohl(ip->ip_dst) >>  0) & 0x0000ffff;

			sumlen = ntohs(ip->udp_len);
			sumptr = (ushort *) &(ip->udp_src);

			while (sumlen > 1) {
				ushort sumdata;

				sumdata = *sumptr++;
				xsum += ntohs(sumdata);
				sumlen -= 2;
			}
			if (sumlen > 0) {
				ushort sumdata;

				sumdata = *(unsigned char *) sumptr;
				sumdata = (sumdata << 8) & 0xff00;
				xsum += sumdata;
			}
			while ((xsum >> 16) != 0) {
				xsum = (xsum & 0x0000ffff) + ((xsum >> 16) & 0x0000ffff);
			}
			if ((xsum != 0x00000000) && (xsum != 0x0000ffff)) {
				printf(" UDP wrong checksum %08x %08x\n", xsum, ntohs(ip->udp_xsum));
				return;
			}
		}
#endif

#ifdef CONFIG_NETCONSOLE
		nc_input_packet((uchar *)ip +IP_HDR_SIZE,
						ntohs(ip->udp_dst),
						ntohs(ip->udp_src),
						ntohs(ip->udp_len) - 8);
#endif
		/*
		 *	IP header OK.  Pass the packet to the current handler.
		 */
		(*packetHandler)((uchar *)ip +IP_HDR_SIZE,
						ntohs(ip->udp_dst),
						ntohs(ip->udp_src),
						ntohs(ip->udp_len) - 8);
		break;
	}
}


/**********************************************************************/

static int net_check_prereq (proto_t protocol)
{
	switch (protocol) {
		/* Fall through */
#if (CONFIG_COMMANDS & CFG_CMD_PING)
	case PING:
		if (NetPingIP == 0) {
			puts ("*** ERROR: ping address not given\n");
			return (1);
		}
		goto common;
#endif
#if (CONFIG_COMMANDS & CFG_CMD_SNTP)
	case SNTP:
		if (NetNtpServerIP == 0) {
			puts ("*** ERROR: NTP server address not given\n");
			return (1);
		}
		goto common;
#endif
#if (CONFIG_COMMANDS & CFG_CMD_NFS)
	case NFS:
#endif
	case NETCONS:
	case TFTP:
		if (NetServerIP == 0) {
			puts ("*** ERROR: `serverip' not set\n");
			return (1);
		}
#if (CONFIG_COMMANDS & (CFG_CMD_PING | CFG_CMD_SNTP))
    common:
#endif

		if (NetOurIP == 0) {
			puts ("*** ERROR: `ipaddr' not set\n");
			return (1);
		}
		/* Fall through */

	case DHCP:
	case RARP:
	case BOOTP:
	case CDP:
		if (memcmp (NetOurEther, "\0\0\0\0\0\0", 6) == 0) {
#ifdef CONFIG_NET_MULTI
			extern int eth_get_dev_index (void);
			int num = eth_get_dev_index ();

			switch (num) {
			case -1:
				puts ("*** ERROR: No ethernet found.\n");
				return (1);
			case 0:
				puts ("*** ERROR: `ethaddr' not set\n");
				break;
			default:
				printf ("*** ERROR: `eth%daddr' not set\n",
					num);
				break;
			}

			NetStartAgain ();
			return (2);
#else
			puts ("*** ERROR: `ethaddr' not set\n");
			return (1);
#endif
		}
		/* Fall through */
	default:
		return (0);
	}
	return (0);		/* OK */
}
/**********************************************************************/

int
NetCksumOk(uchar * ptr, int len)
{
	return !((NetCksum(ptr, len) + 1) & 0xfffe);
}


unsigned
NetCksum(uchar * ptr, int len)
{
	ulong	xsum;
	ushort *p = (ushort *)ptr;

	xsum = 0;
	while (len-- > 0)
		xsum += *p++;
	xsum = (xsum & 0xffff) + (xsum >> 16);
	xsum = (xsum & 0xffff) + (xsum >> 16);
	return (xsum & 0xffff);
}

int
NetEthHdrSize(void)
{
	ushort myvlanid;

	myvlanid = ntohs(NetOurVLAN);
	if (myvlanid == (ushort)-1)
		myvlanid = VLAN_NONE;

	return ((myvlanid & VLAN_IDMASK) == VLAN_NONE) ? ETHER_HDR_SIZE : VLAN_ETHER_HDR_SIZE;
}

int
NetSetEther(volatile uchar * xet, uchar * addr, uint prot)
{
	Ethernet_t *et = (Ethernet_t *)xet;
	ushort myvlanid;

	myvlanid = ntohs(NetOurVLAN);
	if (myvlanid == (ushort)-1)
		myvlanid = VLAN_NONE;

	memcpy (et->et_dest, addr, 6);
	memcpy (et->et_src, NetOurEther, 6);
	if ((myvlanid & VLAN_IDMASK) == VLAN_NONE) {
	et->et_protlen = htons(prot);
		return ETHER_HDR_SIZE;
	} else {
		VLAN_Ethernet_t *vet = (VLAN_Ethernet_t *)xet;

		vet->vet_vlan_type = htons(PROT_VLAN);
		vet->vet_tag = htons((0 << 5) | (myvlanid & VLAN_IDMASK));
		vet->vet_type = htons(prot);
		return VLAN_ETHER_HDR_SIZE;
	}
}

void
NetSetIP(volatile uchar * xip, IPaddr_t dest, int dport, int sport, int len)
{
	volatile IP_t *ip = (IP_t *)xip;

	/*
	 *	If the data is an odd number of bytes, zero the
	 *	byte after the last byte so that the checksum
	 *	will work.
	 */
	if (len & 1)
		xip[IP_HDR_SIZE + len] = 0;

	/*
	 *	Construct an IP and UDP header.
	 *	(need to set no fragment bit - XXX)
	 */
	ip->ip_hl_v  = 0x45;		/* IP_HDR_SIZE / 4 (not including UDP) */
	ip->ip_tos   = 0;
	ip->ip_len   = htons(IP_HDR_SIZE + len);
	ip->ip_id    = htons(NetIPID++);
	ip->ip_off   = htons(0x4000);	/* No fragmentation */
	ip->ip_ttl   = 255;
	ip->ip_p     = 17;		/* UDP */
	ip->ip_sum   = 0;
	NetCopyIP((void*)&ip->ip_src, &NetOurIP); /* already in network byte order */
	NetCopyIP((void*)&ip->ip_dst, &dest);	   /* - "" - */
	ip->udp_src  = htons(sport);
	ip->udp_dst  = htons(dport);
	ip->udp_len  = htons(8 + len);
	ip->udp_xsum = 0;
	ip->ip_sum   = ~NetCksum((uchar *)ip, IP_HDR_SIZE_NO_UDP / 2);
}

void copy_filename (char *dst, char *src, int size)
{
	if (*src && (*src == '"')) {
		++src;
		--size;
	}

	while ((--size > 0) && *src && (*src != '"')) {
		*dst++ = *src++;
	}
	*dst = '\0';
}

#endif /* CFG_CMD_NET */

void ip_to_string (IPaddr_t x, char *s)
{
	x = ntohl (x);
	sprintf (s, "%d.%d.%d.%d",
		 (int) ((x >> 24) & 0xff),
		 (int) ((x >> 16) & 0xff),
		 (int) ((x >> 8) & 0xff), (int) ((x >> 0) & 0xff)
	);
}

IPaddr_t string_to_ip(char *s)
{
	IPaddr_t addr;
	char *e;
	int i;

	if (s == NULL)
		return(0);

	for (addr=0, i=0; i<4; ++i) {
		ulong val = s ? simple_strtoul(s, &e, 10) : 0;
		addr <<= 8;
		addr |= (val & 0xFF);
		if (s) {
			s = (*e) ? e+1 : e;
		}
	}

	return (htonl(addr));
}

void VLAN_to_string(ushort x, char *s)
{
	x = ntohs(x);

	if (x == (ushort)-1)
		x = VLAN_NONE;

	if (x == VLAN_NONE)
		strcpy(s, "none");
	else
		sprintf(s, "%d", x & VLAN_IDMASK);
}

ushort string_to_VLAN(char *s)
{
	ushort id;

	if (s == NULL)
		return htons(VLAN_NONE);

	if (*s < '0' || *s > '9')
		id = VLAN_NONE;
	else
		id = (ushort)simple_strtoul(s, NULL, 10);

	return htons(id);
}

void print_IPaddr (IPaddr_t x)
{
	char tmp[16];

	ip_to_string (x, tmp);

	puts (tmp);
}

IPaddr_t getenv_IPaddr (char *var)
{
	return (string_to_ip(getenv(var)));
}

ushort getenv_VLAN(char *var)
{
	return (string_to_VLAN(getenv(var)));
}

/**********************************************************************************
 * HTTPD section
 */

#define BUF	((struct uip_eth_hdr *)&uip_buf[0])

void NetSendHttpd(void){
	volatile uchar *tmpbuf = NetTxPacket;
	int i;

	for(i = 0; i < 40 + UIP_LLH_LEN; i++){
		tmpbuf[i] = uip_buf[i];
	}

	for(; i < uip_len; i++){
		tmpbuf[i] = uip_appdata[i - 40 - UIP_LLH_LEN];
	}

	eth_send(NetTxPacket, uip_len);
}

void NetReceiveHttpd(volatile uchar * inpkt, int len){
	memcpy(uip_buf, (const void *)inpkt, len);
	uip_len = len;

	if(BUF->type == htons(UIP_ETHTYPE_IP)){
		uip_arp_ipin();
		uip_input();

		if(uip_len > 0){
			uip_arp_out();
			NetSendHttpd();
		}
	} else if(BUF->type == htons(UIP_ETHTYPE_ARP)){
		uip_arp_arpin();

                NetArpWaitTry = 0;
		if(uip_len > 0){
			NetSendHttpd();
		}
        }
}
/*
void eth0_broadcast_rec_off(void)
{
  ath_reg_wr_nf(0x19000058, ath_reg_rd(0x19000058)|(0x3<<8));
}

void eth1_broadcast_rec_off(void)
{
  ath_reg_wr_nf(0x1A000058, ath_reg_rd(0x19000058)|(0x3<<8));
}
*/
/* *************************************
 *
 * HTTP web server for web failsafe mode
 *
 ***************************************/
extern int clinet_ipaddr;
int NetLoopHttpd(void){
	bd_t *bd = gd->bd;
	unsigned short int ip[2];
	unsigned char ethinit_attempt = 0;
	struct uip_eth_addr eaddr;
    char clinet_ipaddr_str[16]= {0};
    static uchar mac[6];
	char load_count=0;
    dhcpd_end = 0;
    NetUipLoop = 0;
#ifdef CONFIG_NET_MULTI
	NetRestarted = 0;
	NetDevExists = 0;
#endif

	/* XXX problem with bss workaround */
	NetArpWaitPacketMAC	= NULL;
	NetArpWaitTxPacket	= NULL;
	NetArpWaitPacketIP	= 0;
	NetArpWaitReplyIP	= 0;
	NetArpWaitTxPacket	= NULL;
	NetTxPacket			= NULL;

	if(!NetTxPacket){
		int i;
		// Setup packet buffers, aligned correctly.
		NetTxPacket = &PktBuf[0] + (PKTALIGN - 1);
		NetTxPacket -= (ulong)NetTxPacket % PKTALIGN;

		for(i = 0; i < PKTBUFSRX; i++){
			NetRxPackets[i] = NetTxPacket + (i + 1) * PKTSIZE_ALIGN;
		}
	}

	if(!NetArpWaitTxPacket){
		NetArpWaitTxPacket = &NetArpWaitPacketBuf[0] + (PKTALIGN - 1);
		NetArpWaitTxPacket -= (ulong)NetArpWaitTxPacket % PKTALIGN;
		NetArpWaitTxPacketSize = 0;
	}

	// restart label
	restart:

	eth_halt();

#ifdef CONFIG_NET_MULTI
	eth_set_current();
#endif

	while(ethinit_attempt < 10){
		if(eth_init(bd)){
			ethinit_attempt = 0;
			break;
		} else {
			ethinit_attempt++;
			eth_halt();
			udelay(1000000);
		}
	}

	if(ethinit_attempt > 0){
		eth_halt();
		printf("## Error: couldn't initialize eth (cable disconnected?)!\n\n");
		return(-1);
	}



	// get MAC address
#ifdef CONFIG_NET_MULTI
	memcpy(NetOurEther, eth_get_dev()->enetaddr, 6);
#else
	eth_getenv_enetaddr("ethaddr", NetOurEther);
#endif

	eaddr.addr[0] = NetOurEther[0];
	eaddr.addr[1] = NetOurEther[1];
	eaddr.addr[2] = NetOurEther[2];
	eaddr.addr[3] = NetOurEther[3];
	eaddr.addr[4] = NetOurEther[4];
	eaddr.addr[5] = NetOurEther[5];

	// set MAC address
	uip_setethaddr(eaddr);

	// set ip and other addresses
	// TODO: do we need this with uIP stack?
	NetCopyIP(&NetOurIP, &bd->bi_ip_addr);

	NetOurGatewayIP		= getenv_IPaddr("gatewayip");
	NetOurSubnetMask	= getenv_IPaddr("netmask");
	NetOurVLAN			= getenv_VLAN("vlan");
	NetOurNativeVLAN	= getenv_VLAN("nvlan");

	// start server...
	printf("HTTP server is starting at IP: %ld.%ld.%ld.%ld\n", (bd->bi_ip_addr & 0xff000000) >> 24, (bd->bi_ip_addr & 0x00ff0000) >> 16, (bd->bi_ip_addr & 0x0000ff00) >> 8, (bd->bi_ip_addr & 0x000000ff));

	HttpdStart();

	// set local host ip address
	ip[0] = ((bd->bi_ip_addr & 0xFFFF0000) >> 16);
	ip[1] = (bd->bi_ip_addr & 0x0000FFFF);

	uip_sethostaddr(ip);

	// set network mask (255.255.255.0 -> local network)
	ip[0] = ((0xFFFFFF00 & 0xFFFF0000) >> 16);
	ip[1] = (0xFFFFFF00 & 0x0000FFFF);

	uip_setnetmask(ip);
	// should we also set default router ip address?
	//uip_setdraddr();
	// show current progress of the process
	do_http_progress(WEBFAILSAFE_PROGRESS_START);

	webfailsafe_is_running = 1;

#ifdef ET_DEBUG
        printf("sending ARP for %08lx\n", getenv("serverip"));
#endif
        eth0_broadcast_rec_off();
        eth1_broadcast_rec_off();
        memcpy(mac, NetEtherNullAddr, 6);
        NetArpWaitPacketIP = getenv_IPaddr("serverip");
       /*
        NetArpWaitPacketIP = clinet_ipaddr;
        ip_to_string(clinet_ipaddr,clinet_ipaddr_str);
        setenv("serverip",clinet_ipaddr_str);
        saveenv();
        printf("serverip: %s\n",clinet_ipaddr_str);
        */
        NetArpWaitPacketMAC = mac;

        /* size of the waiting packet */
        NetArpWaitTxPacketSize = 0;

        /* and do the ARP request */
        NetArpWaitTry = 1;
        NetArpWaitTimerStart = get_timer(0);
        ArpRequest();

	// infinite loop
	for(;;){
                ArpTimeoutCheck();

		/*
		 *	Check the ethernet for a new packet.
		 *	The ethernet receive routine will process it.
		 */
        if(eth_rx() > 0){
               HttpdHandler();
        }


		// if CTRL+C was pressed -> return!
		if(ctrlc()){
			eth_halt();

			// reset global variables to default state
			webfailsafe_is_running = 0;
			webfailsafe_ready_for_upgrade = 0;
			webfailsafe_upgrade_type = 3;//WEBFAILSAFE_UPGRADE_TYPE_FIRMWARE;

			/* Invalidate the last protocol */
			eth_set_last_protocol(BOOTP);
			
			green_led_off();//GL -- led off
			
			printf("\nWeb failsafe mode aborted!\n\n");
			return(-1);
		}

		// until upload is not completed, get back to the start of the loop
		if(!webfailsafe_ready_for_upgrade){
			load_count++;
			if(load_count == 50){
				green_led_toggle();
#ifdef S200_GPIO
            s200_led_toggle(S200_GPIO_LED_BLUE);
#endif
				load_count=0;
			}
			continue;
		}

#ifdef CONFIG_GL_RSA
		// check rsa verify status, break when the status fail
		if ( rsa_verify_update_is_fail() ) {
			break;
		}
#endif

		printf("\nstop eth interface!!\n");
		// stop eth interface
		eth_halt();

		// show progress
		do_http_progress(WEBFAILSAFE_PROGRESS_UPLOAD_READY);
		green_led_off();
		// try to make upgrade!
		if(do_http_upgrade(NetBootFileXferSize, webfailsafe_upgrade_type) >= 0){	
			udelay(500000);
			do_http_progress(WEBFAILSAFE_PROGRESS_UPGRADE_READY);

			udelay(500000);

			/* reset the board */
			do_reset(NULL, 0, 0, NULL);
		}
		break;
	}

	// reset global variables to default state
	webfailsafe_is_running = 0;
	webfailsafe_ready_for_upgrade = 0;
	webfailsafe_upgrade_type = 3;//WEBFAILSAFE_UPGRADE_TYPE_FIRMWARE;

	NetBootFileXferSize = 0;

	do_http_progress(WEBFAILSAFE_PROGRESS_UPGRADE_FAILED);

	// go to restart
	goto restart;

	return(-1);
}
