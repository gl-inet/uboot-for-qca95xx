/* 
 * Copyright (c) 2014 Qualcomm Atheros, Inc.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * 
 */

#include <common.h>
#include <command.h>
#include <asm/mipsregs.h>
#include <asm/addrspace.h>
#include <config.h>
#include <version.h>
#include <atheros.h>
#include "ar7240_soc.h"
extern int ath_ddr_initial_config(uint32_t refresh);
extern int ath_ddr_find_size(void);

#ifdef COMPRESSED_UBOOT
#	define prmsg(...)
#	define args		char *s
#	define board_str(a)	do {			\
	char ver[] = "0";				\
	strcpy(s, a " - Dragonfly 1.");			\
	ver[0] += ath_reg_rd(RST_REVISION_ID_ADDRESS)	\
						& 0xf;	\
	strcat(s, ver);					\
} while (0)
#else
#	define prmsg	printf
#	define args		void
#	define board_str(a)				\
	printf(a " - Dragonfly 1.%d", ath_reg_rd		\
			(RST_REVISION_ID_ADDRESS) & 0xf)
#endif

void
ath_usb1_initial_config(void)
{
#define unset(a)	(~(a))

	ath_reg_wr_nf(SWITCH_CLOCK_SPARE_ADDRESS,
		ath_reg_rd(SWITCH_CLOCK_SPARE_ADDRESS) |
		SWITCH_CLOCK_SPARE_USB_REFCLK_FREQ_SEL_SET(5));
	udelay(1000);

	ath_reg_rmw_set(RST_RESET_ADDRESS,
				RST_RESET_USB_PHY_SUSPEND_OVERRIDE_SET(1));
	udelay(1000);
	ath_reg_rmw_clear(RST_RESET_ADDRESS, RST_RESET_USB_PHY_RESET_SET(1));
	udelay(1000);
	ath_reg_rmw_clear(RST_RESET_ADDRESS, RST_RESET_USB_PHY_ARESET_SET(1));
	udelay(1000);
	ath_reg_rmw_clear(RST_RESET_ADDRESS, RST_RESET_USB_HOST_RESET_SET(1));
	udelay(1000);

	ath_reg_rmw_clear(RST_RESET_ADDRESS, RST_RESET_USB_PHY_PLL_PWD_EXT_SET(1));
	udelay(10);

	ath_reg_rmw_set(RST_RESET2_ADDRESS, RST_RESET2_USB1_EXT_PWR_SEQ_SET(1));
	udelay(10);
}

void
ath_usb2_initial_config(void)
{
	if (is_drqfn()) {
		return;
	}

	ath_reg_rmw_set(RST_RESET2_ADDRESS, RST_RESET2_USB2_MODE_SET(1));
	udelay(10);
	ath_reg_rmw_set(RST_RESET2_ADDRESS,
				RST_RESET2_USB_PHY2_SUSPEND_OVERRIDE_SET(1));
	udelay(1000);
	ath_reg_rmw_clear(RST_RESET2_ADDRESS, RST_RESET2_USB_PHY2_RESET_SET(1));
	udelay(1000);
	ath_reg_rmw_clear(RST_RESET2_ADDRESS, RST_RESET2_USB_PHY2_ARESET_SET(1));
	udelay(1000);
	ath_reg_rmw_clear(RST_RESET2_ADDRESS, RST_RESET2_USB_HOST2_RESET_SET(1));
	udelay(1000);

	ath_reg_rmw_clear(RST_RESET2_ADDRESS, RST_RESET2_USB_PHY2_PLL_PWD_EXT_SET(1));
	udelay(10);

	ath_reg_rmw_set(RST_RESET2_ADDRESS, RST_RESET2_USB2_EXT_PWR_SEQ_SET(1));
	udelay(10);
}


void ath_gpio_config(void)
{
#if defined(CONFIG_CUS249)
    /* Turn on System LED GPIO18 for CUS249 */
    ath_reg_rmw_clear(GPIO_OUT_ADDRESS, (1 << 18));
#endif
	/* Turn off JUMPST_LED and 5Gz LED during bootup */
//	ath_reg_rmw_set(GPIO_OE_ADDRESS, (1 << 15));
//	ath_reg_rmw_set(GPIO_OE_ADDRESS, (1 << 12));
}

int
ath_mem_config(void)
{
	unsigned int type, reg32, *tap;
	extern uint32_t *ath_ddr_tap_cal(void);

#if !defined(CONFIG_ATH_EMULATION)

#if !defined(CONFIG_ATH_NAND_BR)
	type = ath_ddr_initial_config(CFG_DDR_REFRESH_VAL);
	tap = ath_ddr_tap_cal();
//	tap = (uint32_t *)0xbd001f10;
//	prmsg("Tap (low, high) = (0x%x, 0x%x)\n", tap[0], tap[1]);

	tap = (uint32_t *)TAP_CONTROL_0_ADDRESS;
	prmsg("Tap values = (0x%x, 0x%x, 0x%x, 0x%x)\n",
		tap[0], tap[1], tap[2], tap[3]);

	/* Take WMAC out of reset */
	reg32 = ath_reg_rd(RST_RESET_ADDRESS);
	reg32 = reg32 & ~RST_RESET_RTC_RESET_SET(1);
	ath_reg_wr_nf(RST_RESET_ADDRESS, reg32);
#endif

#if defined(CONFIG_USB)
	ath_usb1_initial_config();
	ath_usb2_initial_config();
#else
    //turn off not support interface register
    reg32 = ath_reg_rd(RST_RESET_ADDRESS);
    reg32 = reg32 | RST_RESET_USB_PHY_PLL_PWD_EXT_SET(1);
    ath_reg_wr_nf(RST_RESET_ADDRESS, reg32);
    reg32 = ath_reg_rd(RST_CLKGAT_EN_ADDRESS);
    reg32 = reg32 & ~(RST_CLKGAT_EN_PCIE_EP_SET(1) | RST_CLKGAT_EN_PCIE_RC_SET(1) |
            RST_CLKGAT_EN_PCIE_RC2_SET(1) | RST_CLKGAT_EN_CLK100_PCIERC_SET(1) | 
            RST_CLKGAT_EN_CLK100_PCIERC2_SET(1) | RST_CLKGAT_EN_USB1_SET(1) |
            RST_CLKGAT_EN_USB2_SET(1));
    ath_reg_wr_nf(RST_CLKGAT_EN_ADDRESS, reg32);
    reg32 = ath_reg_rd(RST_RESET2_ADDRESS);
    reg32 = reg32 | RST_RESET2_USB_PHY2_PLL_PWD_EXT_SET(1);
    ath_reg_wr_nf(RST_RESET2_ADDRESS, reg32);

    ath_reg_wr_nf(BIAS4_ADDRESS, 0x6df6ffe0);
    ath_reg_wr_nf(BIAS5_ADDRESS, 0x7ffffffe);
#endif

	ath_gpio_config();
#endif /* !defined(CONFIG_ATH_EMULATION) */

	return ath_ddr_find_size();
}

long int initdram(int board_type)
{
	return (ath_mem_config());
}

int	checkboard(args)
{
	board_str(CONFIG_BOARD_NAME);
	return 0;
}

#if defined(GPIO_RESET)
int reset_button_status(void)
{
	unsigned int gpio;

        gpio = ath_reg_rd(AR7240_GPIO_IN);

	if (gpio & (1 << GPIO_RESET)) {
		return(0);
	} else {
		return(1);
	}
}
#else
int reset_button_status(){return 1;}
#endif

#if defined(GPIO_LED_STATUS)
void status_led_on(void)
{
	unsigned int led_GPIO_OE  = ath_reg_rd(AR7240_GPIO_OE);
        if(led_GPIO_OE & (1<<GPIO_LED_STATUS))
        {
                led_GPIO_OE  &= ~(1<<GPIO_LED_STATUS);
                ath_reg_wr(AR7240_GPIO_OE,led_GPIO_OE);
        }
        ath_reg_wr_nf(AR7240_GPIO_CLEAR, 1<<GPIO_LED_STATUS);
}

void status_led_off(void)
{
        unsigned int led_GPIO_OE  = ath_reg_rd(AR7240_GPIO_OE);
        if(led_GPIO_OE & (1<<GPIO_LED_STATUS))
        {
                led_GPIO_OE  &= ~(1<<GPIO_LED_STATUS);
                ath_reg_wr(AR7240_GPIO_OE,led_GPIO_OE);
        }
        ath_reg_wr_nf(AR7240_GPIO_SET, 1<<GPIO_LED_STATUS);
}

void status_led_toggle(void)
{
        if (ath_reg_rd(AR7240_GPIO_OUT) & (1<<GPIO_LED_STATUS))
                status_led_on();
        else
                status_led_off();
}
#else
void status_led_on(){}
void status_led_off(){}
void status_led_toggle(){}
#endif

#if defined(GPIO_LED_GREEN)
void green_led_on(void)
{
        unsigned int led_GPIO_OE  = ath_reg_rd(AR7240_GPIO_OE);
        if(led_GPIO_OE & (1<<GPIO_LED_GREEN))
        {       
                led_GPIO_OE  &= ~(1<<GPIO_LED_GREEN);
                ath_reg_wr(AR7240_GPIO_OE,led_GPIO_OE);
        }
        ath_reg_wr_nf(AR7240_GPIO_CLEAR,1<< GPIO_LED_GREEN);
}

void green_led_off(void)
{
        unsigned int led_GPIO_OE  = ath_reg_rd(AR7240_GPIO_OE);
        if(led_GPIO_OE & (1<<GPIO_LED_GREEN))
        {
                led_GPIO_OE  &= ~(1<<GPIO_LED_GREEN);
                ath_reg_wr(AR7240_GPIO_OE,led_GPIO_OE);
        }
        ath_reg_wr_nf(AR7240_GPIO_SET, 1<<GPIO_LED_GREEN);
}

void green_led_toggle(void)
{
        if (ath_reg_rd(AR7240_GPIO_OUT) & (1<<GPIO_LED_GREEN))
                green_led_on();
        else
                green_led_off();
}
#else
void green_led_on(){}
void green_led_off(){}
void green_led_toggle(){}
#endif

#if defined(GPIO_LED_RED)
void red_led_off(void)
{
        unsigned int led_GPIO_OE  = ath_reg_rd(AR7240_GPIO_OE);
        if(led_GPIO_OE & (1<<GPIO_LED_RED))
        {
                led_GPIO_OE  &= ~(1<<GPIO_LED_RED);
                ath_reg_wr(AR7240_GPIO_OE,led_GPIO_OE);
        }
        ath_reg_wr_nf(AR7240_GPIO_CLEAR, 1<<GPIO_LED_RED);
}

void red_led_on(void)
{
        unsigned int led_GPIO_OE  = ath_reg_rd(AR7240_GPIO_OE);
        if(led_GPIO_OE & (1<<GPIO_LED_RED))
        {       
                led_GPIO_OE  &= ~(1<<GPIO_LED_RED);
                ath_reg_wr(AR7240_GPIO_OE,led_GPIO_OE);
        }
        ath_reg_wr_nf(AR7240_GPIO_SET, 1<<GPIO_LED_RED);
}

void red_led_toggle(void)
{
        if (ath_reg_rd(AR7240_GPIO_OUT) & (1<<GPIO_LED_RED))
                red_led_off();
        else
                red_led_on();
}
#else
void red_led_on(){}
void red_led_off(){}
void red_led_toggle(){}
#endif

#if defined(GPIO_LED_LAN)
void lan_led_on(void)
{
        unsigned int led_GPIO_OE  = ath_reg_rd(AR7240_GPIO_OE);
        if(led_GPIO_OE & (1<<GPIO_LED_LAN))
        {
                led_GPIO_OE  &= ~(1<<GPIO_LED_LAN);
                ath_reg_wr(AR7240_GPIO_OE,led_GPIO_OE);
        }
        ath_reg_wr_nf(AR7240_GPIO_CLEAR, 1<<GPIO_LED_LAN);
}

void lan_led_off(void)
{
        unsigned int led_GPIO_OE  = ath_reg_rd(AR7240_GPIO_OE);
        if(led_GPIO_OE & (1<<GPIO_LED_LAN))
        {
                led_GPIO_OE  &= ~(1<<GPIO_LED_LAN);
                ath_reg_wr(AR7240_GPIO_OE,led_GPIO_OE);
        }
        ath_reg_wr_nf(AR7240_GPIO_SET, 1<<GPIO_LED_LAN);
}

void lan_led_toggle(void)
{
        if (ath_reg_rd(AR7240_GPIO_OUT) & (1<<GPIO_LED_LAN))
                red_led_on();
        else
                red_led_off();
}
#else
void lan_led_on(){}
void lan_led_off(){}
void lan_led_toggle(){}
#endif

#if defined(GPIO_LED_4G)
void g4_led_on(void)
{
	 unsigned int led_GPIO_OE  = ath_reg_rd(AR7240_GPIO_OE);
        if(led_GPIO_OE & (1<<GPIO_LED_4G))
        {
                led_GPIO_OE  &= ~(1<<GPIO_LED_4G);
                ath_reg_wr(AR7240_GPIO_OE,led_GPIO_OE);
        }
        ath_reg_wr_nf(AR7240_GPIO_CLEAR, 1<<GPIO_LED_4G);
}

void g4_led_off(void)
{
         unsigned int led_GPIO_OE  = ath_reg_rd(AR7240_GPIO_OE);
        if(led_GPIO_OE & (1<<GPIO_LED_4G))
        {
                led_GPIO_OE  &= ~(1<<GPIO_LED_4G);
                ath_reg_wr(AR7240_GPIO_OE,led_GPIO_OE);
        }
        ath_reg_wr_nf(AR7240_GPIO_SET, 1<<GPIO_LED_4G);
}

void g4_led_toggle(void)
{
        if (ath_reg_rd(AR7240_GPIO_OUT) & (1<<GPIO_LED_4G))
                red_led_on();
        else
                red_led_off();
}
#else
void g4_led_on(){}
void g4_led_off(){}
void g4_led_toggle(){}
#endif
void all_led_on(void)
{
        status_led_on();
        green_led_on();
        red_led_on();
	lan_led_on();
	g4_led_on();
}

void all_led_off(void)
{
        status_led_off();
        green_led_off();
        red_led_off();
	lan_led_off();
	g4_led_off();
}


void gpio17_select_out()
{
unsigned int tmp=0;
tmp = ath_reg_rd(AR7240_GPIO_FUNC4);
ath_reg_wr_nf(AR7240_GPIO_FUNC4, (tmp & 0x00ff) );

}
#if defined(GPIO_4G1_POWER)  || defined(GPIO_4G2_POWER) || defined(GPIO_WATCHDOG1) || defined(GPIO_SIM_SELECT)
/*void get_gpio_status()
{
	unsigned int gpio_oe = ath_reg_rd(AR7240_GPIO_OE);
	unsigned int gpio_in = ath_reg_rd(AR7240_GPIO_IN);
	unsigned int gpio_out = ath_reg_rd(AR7240_GPIO_OUT);
	printf("Function:%s Line:%d gpio_oe=%x\n",__func__, __LINE__, gpio_oe);
	printf("Function:%s Line:%d gpio_in=%x\n",__func__, __LINE__, gpio_in);
	printf("Function:%s Line:%d gpio_out=%x\n",__func__, __LINE__, gpio_out);
}*/
void set_gpio_value(char gpionum,char value)
{
	//printf("Function:%s Line:%d GPIO Number:%d GPIO Value:%d\n", __func__, __LINE__, gpionum,value);
	unsigned int led_GPIO_OE  = ath_reg_rd(AR7240_GPIO_OE);
	//printf("Function:%s Line:%d ath_reg_rd(AR7240_GPIO_OE)=%x\n",__func__, __LINE__, led_GPIO_OE);
	if(led_GPIO_OE & (1<<gpionum))
	{
		led_GPIO_OE  &= ~(1<<gpionum);
		ath_reg_wr(AR7240_GPIO_OE,led_GPIO_OE);//set output
	}
	if(value == 0)
		ath_reg_wr_nf(AR7240_GPIO_CLEAR, 1<<gpionum);
	else
		ath_reg_wr_nf(AR7240_GPIO_SET, 1<<gpionum);
	//printf("Function:%s Line:%d ath_reg_rd(AR7240_GPIO_OE)=%x\n",__func__, __LINE__, ath_reg_rd(AR7240_GPIO_OE));
}
#endif

#if defined(GPIO_WATCHDOG2)
void disable_jtag(void)
{
	ath_reg_wr_nf(GPIO_FUNCTION_ADDRESS, ath_reg_rd(GPIO_FUNCTION_ADDRESS) | GPIO_FUNCTION_DISABLE_JTAG_MASK);
}

void gpio_watchdog_up()
{	
	unsigned int led_GPIO_OE  = ath_reg_rd(AR7240_GPIO_OE);
	if(led_GPIO_OE & (1<<GPIO_WATCHDOG2))
	{
		led_GPIO_OE  &= ~(1<<GPIO_WATCHDOG2);
		ath_reg_wr(AR7240_GPIO_OE,led_GPIO_OE);
	}
	ath_reg_wr_nf(AR7240_GPIO_SET, 1<<GPIO_WATCHDOG2);
}

void gpio_watchdog_down()
{
	unsigned int led_GPIO_OE  = ath_reg_rd(AR7240_GPIO_OE);
	if(led_GPIO_OE & (1<<GPIO_WATCHDOG2))
	{
		led_GPIO_OE  &= ~(1<<GPIO_WATCHDOG2);
		ath_reg_wr(AR7240_GPIO_OE,led_GPIO_OE);
	}
	ath_reg_wr_nf(AR7240_GPIO_CLEAR, 1<<GPIO_WATCHDOG2);
}

void gpio_watchdog_toggle(int count)
{
	disable_jtag();
	while(count--)
	{
		gpio_watchdog_up();
		udelay(10000);
		gpio_watchdog_down();
		udelay(10000);
	}
}
#endif

#define GPIO_SWITCH_LOAD 0x3 //GPIO 0 and 1
unsigned  int switch_boot_load()
{
  char val1=0,val2=0;
  val1 =ath_reg_rd(AR7240_GPIO_IN) & GPIO_SWITCH_LOAD;
        udelay(20000);//20ms
  val2 =ath_reg_rd(AR7240_GPIO_IN) & GPIO_SWITCH_LOAD;
  if(val1 == val2) return val1;
  else{
        printf("is default boot device \n");
        return 0;
        }
}

