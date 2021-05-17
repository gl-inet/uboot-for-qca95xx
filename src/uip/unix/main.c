/*
 * Copyright (c) 2001, Adam Dunkels.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Adam Dunkels.
 * 4. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file is part of the uIP TCP/IP stack.
 *
 * $Id: main.c,v 1.16 2006/06/11 21:55:03 adam Exp $
 *
 */
#include "uip.h"
#include "uip_arp.h"
#include "timer.h"
#include <common.h>
#include <command.h>
#include <net.h>
#include <asm/addrspace.h>
#include <atheros.h>
#define BUF ((struct uip_eth_hdr *)&uip_buf[0])

char  NetUipLoop = 0;
char  dhcpd_end  = 0;
char  update_msg_flag = 0;
#ifndef NULL
#define NULL (void *)0
#endif /* NULL */

extern  char PktBuf[(PKTBUFSRX+1) * PKTSIZE_ALIGN + PKTALIGN];

char
dev_init_up(void);
/*---------------------------------------------------------------------------*/
static int
dev_send(void)
{
    NetTxPacket = KSEG1ADDR(NetTxPacket);
    memcpy((void *)NetTxPacket, uip_buf, uip_len);
    eth_send(NetTxPacket, uip_len);
    return 0;
}

void send_U_G_ok(void)
{
    char i = 0;
    unsigned char file_U_G_ok[]={ 0xff,0xff,0xff,0xff,0xff,0xff,0x14,0x6b,0x9c,0xb7,0x12,0x30,0x08,0x00,0x45,0x00,0x00,0x4d,0x00,0x01,0x00,0x00,0x40,0x01,0xb9,0x06,0xc0,0xa8,0x01,0x01,0xff,0xff,0xff,0xff,0x08,0x00,0xfa,0xb1,0x00,0x01,0x00,0x01,0x67,0x6c,0x2d,0x69,0x6e,0x65,0x74,0x2d,0x75,0x67 };
    dev_init_up();
    for(i=0;i<5;i++){
        NetTxPacket = KSEG1ADDR(NetTxPacket);
        memcpy((void *)NetTxPacket, file_U_G_ok, 52);
        eth_send(NetTxPacket, 52);
    }
    printf("send U_G");
}

char firmware_name[16] = "0";
char file_name_ok_flag = 0;
char s_d_flag = 0;
void dev_received(volatile char * inpkt, int len)
{
    char *p = NULL;
    char i = 0;
    unsigned char dev_name[]={ 0xff,0xff,0xff,0xff,0xff,0xff,0x14,0x6b,0x9c,0xb7,0x12,0x30,0x08,0x00,0x45,0x00,0x00,0x4d,0x00,0x01,0x00,0x00,0x40,0x01,0xb9,0x06,0xc0,0xa8,0x01,0x01,0xff,0xff,0xff,0xff,0x08,0x00,0xfa,0xb1,0x00,0x01,0x00,0x01,0x67,0x6c,0x2d,0x69,0x6e,0x65,0x74,0x2d,0x6f,0x6b,0x20,0x45,0x37,0x35,0x30 };
	memcpy(uip_buf, (const void *)inpkt, len);
	uip_len = len;
    if(update_msg_flag){
        update_msg_flag = 0;
        if(strstr(uip_buf+42,"gl_inet_U_G")){

            p = strstr(uip_buf+42,"gl_inet_U_G");

	    memcpy(firmware_name, (const void *)(p+13),11+4);
            printf("file:%s\n",firmware_name);
            //NetTxPacket = KSEG1ADDR(NetTxPacket);
            //memcpy((void *)NetTxPacket, file_name_ok, 52);
            //eth_send(NetTxPacket, 52);
            file_name_ok_flag = 1;
            s_d_flag = 0;
        }
        else if(strstr(uip_buf+42,"gl_inet_S_D")){
            printf("rec_s_d");
            for(i=0;i<5;i++){
                NetTxPacket = KSEG1ADDR(NetTxPacket);
                memcpy((void *)NetTxPacket,dev_name, 57);
                eth_send(NetTxPacket, 57);
            }
            s_d_flag = 1 ;
        }
        return;
    }
	if(uip_len > 0) {
        // printf("rx a pkt: length %d, type %x\n", uip_len, BUF->type);
        if (uip_len > UIP_BUFSIZE) {
            printf("UIP error: uip_len > UIP_BUFSIZE!!\n");
		    udelay(10 * CLOCK_SECOND);
        }
        if(dhcpd_end){
            return;
        }
        if(BUF->type == htons(UIP_ETHTYPE_IP)) {
		    uip_arp_ipin1();
		    uip_input();
		    /* If the above function invocation resulted in data that
		     should be sent out on the network, the global variable
		     uip_len is set to a value > 0. */
		    if(uip_len > 0) {
		        uip_arp_out1();
		        dev_send();
		    }
        } else if(BUF->type == htons(UIP_ETHTYPE_ARP)) {
		    uip_arp_arpin();
		    /* If the above function invocation resulted in data that
		     should be sent out on the network, the global variable
		     uip_len is set to a value > 0. */
		    if(uip_len > 0) {
	            dev_send();
		    }
        }

    }
}
void eth0_broadcast_rec_on(void)
{
  ath_reg_wr_nf(0x19000058, ath_reg_rd(0x19000058)&(~(0x3<<8)));
}

void eth0_broadcast_rec_off(void)
{
  ath_reg_wr_nf(0x19000058, ath_reg_rd(0x19000058)|(0x3<<8));
}
void eth1_broadcast_rec_on(void)
{
  ath_reg_wr_nf(0x1A000058, ath_reg_rd(0x19000058)&(~(0x3<<8)));
}
void eth1_broadcast_rec_off(void)
{
  ath_reg_wr_nf(0x1A000058, ath_reg_rd(0x19000058)|(0x3<<8));
}

extern char rst_key_5s;

char eth_state = 0;
static void
dev_init(void)
{
    eth_state = 0;
    char eth_ok = 0;
    char eth_no = 1;
    char i = 0;
    DECLARE_GLOBAL_DATA_PTR;

    struct timer eth_timer;
    timer_set(&eth_timer, CLOCK_SECOND*2);
    bd_t *bd = gd->bd;
    if(rst_key_5s){
        mifi_v3_send_msg("{ \"system\": \"gouboot\" }");
    }
    printf("\n");
    while(1){
	    if(ctrlc()){
		    eth_halt();
		    printf("\n eth init aborted!\n\n");
		    return(-1);
	    }
        if(timer_expired(&eth_timer)) {
            timer_reset(&eth_timer);
            eth_ok = eth_init(bd);
            i++;
            if(i>10){
                printf("eth err\n");
                eth_state = 0;
                return;
            }
            if(eth_ok ==1){
                eth_state = 1;
            if(rst_key_5s){
               mifi_v3_send_msg("{ \"system\": \"cable_ok\" }");
            }
            printf("\n");
            break;

            }
            else{
                if(eth_no){
                    eth_state = 0;
                    eth_no = 0;
                if(rst_key_5s){
                    mifi_v3_send_msg("{ \"system\": \"cable_no\" }");
                }
                    printf("\n");
                    printf("Please insert the network cable\n");
                }
            }
        }
    }
    eth0_broadcast_rec_on();
    eth1_broadcast_rec_on();
    return ;
}

char
dev_init_up(void)
{
    char eth_sa = 0;
    DECLARE_GLOBAL_DATA_PTR;
    bd_t *bd = gd->bd;
    eth_sa = eth_init(bd);
    //eth0_broadcast_rec_on();
    //eth1_broadcast_rec_on();
    return eth_sa;
}

char gl_set_uip_info()
{
	//int i=5;
	uip_ipaddr_t ipaddr;
	uip_init();
	uip_ipaddr(ipaddr, 192,168,1,1);  //≈‰÷√IP
	uip_sethostaddr(ipaddr);
	uip_ipaddr(ipaddr, 192,168,1,1);  //≈‰÷√Õ¯πÿ
	uip_setdraddr(ipaddr);
	uip_ipaddr(ipaddr, 255,255,255,0);//≈‰÷√◊”Õ¯—⁄¬Î
 	uip_setnetmask(ipaddr);
	uip_gethostaddr(ipaddr); 

	if(0== dev_init_up()){
		return 0;
	}
	return 1;
}


char one_time=1;
char update_msg(void){

    uip_ipaddr_t ipaddr;
    s_d_flag = 0;
    if(eth_state == 0){
        return 0;
    }                       //Õ¯ø®≥ı ºªØ
    if(one_time){
        one_time = 0;
        uip_init();
        uip_ipaddr(ipaddr, 192,168,1,1);  //≈‰÷√IP
        uip_sethostaddr(ipaddr);
        uip_ipaddr(ipaddr, 192,168,1,1);  //≈‰÷√Õ¯πÿ
        uip_setdraddr(ipaddr);
        uip_ipaddr(ipaddr, 255,255,255,0);//≈‰÷√◊”Õ¯—⁄¬Î
        uip_setnetmask(ipaddr);
        uip_gethostaddr(ipaddr); 

        if(0== dev_init_up()){
            return 0;
        }                       //Õ¯ø®≥ı ºªØ
    }
    S_D:
    update_msg_flag = 1;
/*
    while(1){

    	if(ctrlc()){
			eth_halt();
			NetUipLoop=0;
			printf("\n web failsafe mode aborted!\n\n");
			return(-1);
         } 
        uip_len=eth_rx();
    }
    
    */
    NetUipLoop = 1;
    eth_rx();
    if(s_d_flag){
        goto S_D;
    }
    if(file_name_ok_flag){
        eth_halt();
        file_name_ok_flag = 0;
        NetUipLoop = 0;
        one_time = 1;
        return 1;
    }

    file_name_ok_flag = 0;
    NetUipLoop = 0;
    return 0;

}



int 
uip_main(void)
{
  int i;
  uip_ipaddr_t ipaddr;
  struct timer periodic_timer, arp_timer;

  timer_set(&periodic_timer, CLOCK_SECOND / 2);
  timer_set(&arp_timer, CLOCK_SECOND * 20);

  uip_init();
  uip_ipaddr(ipaddr, 192,168,1,1);//≈‰÷√IP
  uip_sethostaddr(ipaddr);
  uip_ipaddr(ipaddr, 192,168,1,1);//≈‰÷√Õ¯πÿ
  uip_setdraddr(ipaddr);
  uip_ipaddr(ipaddr, 255,255,255,0);//≈‰÷√◊”Õ¯—⁄¬Î
  uip_setnetmask(ipaddr);
  uip_gethostaddr(ipaddr); 

  eth_state = 0;
  dev_init();                      //Õ¯ø®≥ı ºªØ
  if(eth_state == 0){
    return (-1);   
  }
  dhcpd_init();                    //dhcpd≥ı ºªØ

  for ( ; ; ) {

    uip_len=eth_rx();
	if(ctrlc()){
		eth_halt();
		NetUipLoop=0;
		printf("\n web failsafe mode aborted!\n\n");
		return(-1);
	}
   if(timer_expired(&periodic_timer)) {
      timer_reset(&periodic_timer);
      for(i = 0; i < UIP_CONNS; i++) {
		uip_periodic(i);
		/* If the above function invocation resulted in data that
		   should be sent out on the network, the global variable
		   uip_len is set to a value > 0. */
        if(dhcpd_end){
            dhcpd_end = 0;
//	    eth_halt();
	    NetUipLoop=0;
	    printf("dhcpd end\n");
            if(rst_key_5s){
                mifi_v3_send_msg("{ \"system\": \"goweb\" }");
            }
            printf("\n");
            rst_key_5s = 0;
			return(-1);
        }
		if(uip_len > 0) {
		    uip_arp_out1();
		    dev_send();
		}
      }
#if UIP_UDP
      for(i = 0; i < UIP_UDP_CONNS; i++) {
		uip_udp_periodic(i);
		/* If the above function invocation resulted in data that
		   should be sent out on the network, the global variable
		   uip_len is set to a value > 0. */
		if(uip_len > 0) {
		    uip_arp_out1();
            dev_send();
            if(dhcpd_end){
		dhcpd_end = 0;
//		eth_halt();
		NetUipLoop=0;
		printf("dhcpd end\n");
                if(rst_key_5s){
                    mifi_v3_send_msg("{ \"system\": \"goweb\" }");
                }
                rst_key_5s = 0;
                printf("\nHit \"gl\" key to stop\n");
			    return(-1);
            }
    	}
      }
#endif /* UIP_UDP */

      /* Call the ARP timer function every 10 seconds. */
    if(timer_expired(&arp_timer)) {
        timer_reset(&arp_timer);
        uip_arp_timer();
        dhcpd_end = 0;
        eth_halt();
        NetUipLoop=0;
        printf("dhcpd timeout\n");
        return (-1);
    }
  }
 }
  return 0;
}

uint show_kernel( uint addr );
uint show_kernel( uint addr )
{
    char linebuf[4];
    uint *uip = (uint*)linebuf;
    uint k_size = 0;
    uint i = 0;

    for(i=0;i<0xffff;i++){   
        *uip = *((uint *)addr);
        if((*uip)==0x55424923){
          //  printf("%08lx:  %08x  \n",addr, (*uip = *((uint *)addr)));
            k_size=0x10000 * i ;
            break;
                        
        }
        else{
            addr+=0x10000;    
                        
        }
    }
   // printf("kernel size:0x%08lx  \n",k_size);
    return k_size;
}
/*---------------------------------------------------------------------------*/
void
uip_log(char *m)
{
  printf("uIP log : %s\n", m);
}
/*---------------------------------------------------------------------------*/


int gl_uip_command(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
  if(!strncmp(argv[1], "start", 6)) {
    NetUipLoop = 1;
    dhcpd_end  = 0;
    uip_main();
  }
  else
    printf("Usage:\n%s\n use\"help dhcpd\"\n", cmdtp->usage);
  return 0;
}
U_BOOT_CMD(
  dhcpd,	2,	1, 	gl_uip_command,
  "dhcpd	- dhcp server command\n",
  "usage:\n"
  "dhcpd start\n"
);
int update_test(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    NetUipLoop = 1;
    update_msg();
    return 0;
}
/*
U_BOOT_CMD(
  upt,	1,	1, 	update_test,
  "upt	- update test command\n",
  "usage:\n"
  "upt\n"
);
*/
