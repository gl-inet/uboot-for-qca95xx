/*
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
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
#	define board_str(a)	do {						\
	char ver[] = "0";							\
	uint32_t        revid;							\
	if(((revid=ath_reg_rd(RST_REVISION_ID_ADDRESS))&0xff0)==0x140) 		\
		strcpy(s, a " - Honey Bee 1.");					\
	else			 						\
		strcpy(s, a " - Honey Bee 2.");					\
	ver[0] +=  (revid & 0xf) ;		 				\
	strcat(s, ver);								\
} while (0)
#else
#	define prmsg	printf
#	define args		void
#	define board_str(a)							\
	uint32_t        revid;							\
	if(((revid=ath_reg_rd(RST_REVISION_ID_ADDRESS))&0xff0)==0x140) 		\
	printf(a " - Honey Bee 1.%d", revid & 0xf);				\
	else									\
	printf(a " - Honey Bee 2.%d", revid & 0xf);
#endif

void
ath_usb_initial_config(void)
{
#define unset(a)	(~(a))

	if (ath_reg_rd(RST_BOOTSTRAP_ADDRESS) & RST_BOOTSTRAP_TESTROM_ENABLE_MASK) {

		ath_reg_rmw_set(RST_RESET_ADDRESS, RST_RESET_USB_HOST_RESET_SET(1));
		udelay(1000);
		ath_reg_rmw_set(RST_RESET_ADDRESS, RST_RESET_USB_PHY_RESET_SET(1));
		udelay(1000);

		ath_reg_wr(PHY_CTRL5_ADDRESS, PHY_CTRL5_RESET_1);
		udelay(1000);

		ath_reg_rmw_set(RST_RESET_ADDRESS, RST_RESET_USB_PHY_PLL_PWD_EXT_SET(1));
		udelay(1000);
		ath_reg_rmw_set(RST_RESET_ADDRESS, RST_RESET_USB_PHY_ARESET_SET(1));
		udelay(1000);

		ath_reg_rmw_clear(RST_CLKGAT_EN_ADDRESS, RST_CLKGAT_EN_USB1_SET(1));

		return;
	}

	ath_reg_wr_nf(SWITCH_CLOCK_SPARE_ADDRESS,
		ath_reg_rd(SWITCH_CLOCK_SPARE_ADDRESS) |
		SWITCH_CLOCK_SPARE_USB_REFCLK_FREQ_SEL_SET(2));
	udelay(1000);

	ath_reg_rmw_set(RST_RESET_ADDRESS,
				RST_RESET_USB_PHY_SUSPEND_OVERRIDE_SET(1));
	udelay(1000);
	ath_reg_rmw_clear(RST_RESET_ADDRESS, RST_RESET_USB_PHY_ARESET_SET(1));
	udelay(1000);
	ath_reg_rmw_clear(RST_RESET_ADDRESS, RST_RESET_USB_PHY_RESET_SET(1));
	udelay(1000);
	ath_reg_rmw_clear(RST_RESET_ADDRESS, RST_RESET_USB_HOST_RESET_SET(1));
	udelay(1000);

	ath_reg_rmw_clear(RST_RESET_ADDRESS, RST_RESET_USB_PHY_PLL_PWD_EXT_SET(1));
	udelay(10);
}

void ath_gpio_config(void)
{
	/* disable the CLK_OBS on GPIO_4 and set GPIO4 as input */
	ath_reg_rmw_clear(GPIO_OE_ADDRESS, (1 << 4));
	ath_reg_rmw_clear(GPIO_OUT_FUNCTION1_ADDRESS, GPIO_OUT_FUNCTION1_ENABLE_GPIO_4_MASK);
	ath_reg_rmw_set(GPIO_OUT_FUNCTION1_ADDRESS, GPIO_OUT_FUNCTION1_ENABLE_GPIO_4_SET(0x80));
	ath_reg_rmw_set(GPIO_OE_ADDRESS, (1 << 4));
	/* Turn off JUMPST_LED and 5Gz LED during bootup */
	ath_reg_rmw_set(GPIO_OE_ADDRESS, (1 << 15));

        /* set gpio 12 13 14 to output */
	ath_reg_rmw_clear(GPIO_OE_ADDRESS, (1 << 12));
        ath_reg_rmw_clear(GPIO_OE_ADDRESS, (1 << 13));
        ath_reg_rmw_clear(GPIO_OE_ADDRESS, (1 << 14));
	/* turn off led 13 and 14, turn on 12 */
        ath_reg_wr_nf(AR7240_GPIO_CLEAR, (1 << 12));
        ath_reg_wr_nf(AR7240_GPIO_SET, (1 << 13));
        ath_reg_wr_nf(AR7240_GPIO_SET, (1 << 14));
	/* set reset button as input */
	ath_reg_rmw_set(GPIO_OE_ADDRESS, (1 << 3));
}

int
ath_mem_config(void)
{
	unsigned int type, reg32, *tap;
	extern uint32_t *ath_ddr_tap_cal(void);

#if !defined(CONFIG_ATH_EMULATION)
	type = ath_ddr_initial_config(CFG_DDR_REFRESH_VAL);

	tap = ath_ddr_tap_cal();
	/* prmsg("tap = 0x%p\n", tap); */

	tap = (uint32_t *)0xbd001f10;
	/* prmsg("Tap (low, high) = (0x%x, 0x%x)\n", tap[0], tap[1]); */

	tap = (uint32_t *)TAP_CONTROL_0_ADDRESS;
	/* prmsg("Tap values = (0x%x, 0x%x, 0x%x, 0x%x)\n", */
	/* 	tap[0], tap[2], tap[2], tap[3]); */

	/* Take WMAC out of reset */
	reg32 = ath_reg_rd(RST_RESET_ADDRESS);
	reg32 = reg32 & ~RST_RESET_RTC_RESET_SET(1);
	ath_reg_wr_nf(RST_RESET_ADDRESS, reg32);

	ath_usb_initial_config();

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
	/* board_str(CONFIG_BOARD_NAME); */
	return 0;
}


int reset_button_status(void)
{
	unsigned int gpio;

        gpio = ath_reg_rd(AR7240_GPIO_IN);

#define GL_AR300M_GPIO_BTN_RESET 3
	if (gpio & (1 << GL_AR300M_GPIO_BTN_RESET)) {
		return(0);
	} else {
		return(1);
	}
}

#define GPIO_LED_STATUS	(1 << 12)
#define GPIO_LED_GREEN	(1 << 13)
#define GPIO_LED_RED	(1 << 14)

void status_led_on(void)
{
        ath_reg_wr_nf(AR7240_GPIO_CLEAR, GPIO_LED_STATUS);
}

void status_led_off(void)
{
        ath_reg_wr_nf(AR7240_GPIO_SET, GPIO_LED_STATUS);
}

void status_led_toggle(void)
{
        if (ath_reg_rd(AR7240_GPIO_OUT) & GPIO_LED_STATUS)
                status_led_on();
        else
                status_led_off();
}

void green_led_on(void)
{
        ath_reg_wr_nf(AR7240_GPIO_CLEAR, GPIO_LED_GREEN);
}

void green_led_off(void)
{
        ath_reg_wr_nf(AR7240_GPIO_SET, GPIO_LED_GREEN);
}

void green_led_toggle(void)
{
        if (ath_reg_rd(AR7240_GPIO_OUT) & GPIO_LED_GREEN)
                green_led_on();
        else
                green_led_off();
}

void red_led_on(void)
{
        ath_reg_wr_nf(AR7240_GPIO_CLEAR, GPIO_LED_RED);
}

void red_led_off(void)
{
        ath_reg_wr_nf(AR7240_GPIO_SET, GPIO_LED_RED);
}

void red_led_toggle(void)
{
        if (ath_reg_rd(AR7240_GPIO_OUT) & GPIO_LED_RED)
                red_led_on();
        else
                red_led_off();
}


void all_led_on(void)
{
        status_led_on();
        green_led_on();
        red_led_on();
}

void all_led_off(void)
{
        status_led_off();
        green_led_off();
        red_led_off();
}

void gpio17_select_out()
{
unsigned int tmp=0;
tmp = ath_reg_rd(AR7240_GPIO_FUNC4);
ath_reg_wr_nf(AR7240_GPIO_FUNC4, (tmp & 0x00ff) );

}

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

