/*
 * Copyright (c) 2014 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#ifndef __BOARD_953X_H
#define __BOARD_953X_H

#include <config.h>
/* ethernet debug */
/* #define ET_DEBUG */

#define CFG_CONSOLE_INFO_QUIET
#define CONFIG_DELAY_TO_AUTORUN_HTTPD 5

#ifdef CONFIG_AR300M
	#define CONFIG_BOOTCOUNT_LIMIT
#endif

/*
 * Web Failsafe configuration
 */
#define CONFIG_LOADADDR         0x80800000
#define WEBFAILSAFE_UPLOAD_RAM_ADDRESS                                  CONFIG_LOADADDR

// U-Boot partition size and offset
#define WEBFAILSAFE_UPLOAD_UBOOT_ADDRESS                                CFG_FLASH_BASE
#define CONFIG_FOR_GL_BOARD
#if defined(CONFIG_FOR_DLINK_DIR505_A1)
        #define WEBFAILSAFE_UPLOAD_UBOOT_SIZE_IN_BYTES          (64 * 1024)
        #define UPDATE_SCRIPT_UBOOT_SIZE_IN_BYTES                       "0x10000"
#elif defined(CONFIG_FOR_8DEVICES_CARAMBOLA2)
        #define WEBFAILSAFE_UPLOAD_UBOOT_SIZE_IN_BYTES          (256 * 1024)
        #define UPDATE_SCRIPT_UBOOT_SIZE_IN_BYTES                       "0x40000"
#elif defined(CONFIG_FOR_DOMINO)
        #define WEBFAILSAFE_UPLOAD_UBOOT_SIZE_IN_BYTES          (256 * 1024)
        #define UPDATE_SCRIPT_UBOOT_SIZE_IN_BYTES                       "0x40000"
#elif defined(CONFIG_FOR_DRAGINO_V2)
        #define WEBFAILSAFE_UPLOAD_UBOOT_SIZE_IN_BYTES          (192 * 1024)
        #define UPDATE_SCRIPT_UBOOT_SIZE_IN_BYTES                       "0x30000"
#elif defined(CONFIG_FOR_GL_BOARD)
	#define WEBFAILSAFE_UPLOAD_UBOOT_SIZE_IN_BYTES          (256 * 1024)
        #define UPDATE_SCRIPT_UBOOT_SIZE_IN_BYTES                       "0x40000"
#else
        #define WEBFAILSAFE_UPLOAD_UBOOT_SIZE_IN_BYTES          (256 * 1024)
        #define UPDATE_SCRIPT_UBOOT_SIZE_IN_BYTES                       "0x40000"
#endif

#define WEBFAILSAFE_ERASE_UBOOT_SIZE_IN_BYTES                          (320 * 1024)
// Firmware partition offset
#if defined(CONFIG_FOR_DLINK_DIR505_A1)
        #define WEBFAILSAFE_UPLOAD_KERNEL_ADDRESS                       WEBFAILSAFE_UPLOAD_UBOOT_ADDRESS + 0x80000
#elif defined(CONFIG_FOR_8DEVICES_CARAMBOLA2)
        #define WEBFAILSAFE_UPLOAD_KERNEL_ADDRESS                       WEBFAILSAFE_UPLOAD_UBOOT_ADDRESS + 0x50000
#elif defined(CONFIG_FOR_DOMINO)
        #define WEBFAILSAFE_UPLOAD_KERNEL_ADDRESS                       WEBFAILSAFE_UPLOAD_UBOOT_ADDRESS + 0x50000
#elif defined(CONFIG_FOR_DRAGINO_V2)
        #define WEBFAILSAFE_UPLOAD_KERNEL_ADDRESS                       WEBFAILSAFE_UPLOAD_UBOOT_ADDRESS + 0x40000
#elif defined(CONFIG_FOR_GL_BOARD)					
	#define WEBFAILSAFE_UPLOAD_KERNEL_ADDRESS			GL_BOOT_ADDR
#else
        #define WEBFAILSAFE_UPLOAD_KERNEL_ADDRESS                       WEBFAILSAFE_UPLOAD_UBOOT_ADDRESS + 0x20000
#endif

// ART partition size and offset
#if defined(CONFIG_FOR_DLINK_DIR505_A1)
        #define WEBFAILSAFE_UPLOAD_ART_ADDRESS                          WEBFAILSAFE_UPLOAD_UBOOT_ADDRESS + 0x10000
#endif
#if defined(CONFIG_FOR_GL_BOARD)
        #define WEBFAILSAFE_UPLOAD_ART_ADDRESS                          WEBFAILSAFE_UPLOAD_UBOOT_ADDRESS + 0x50000
	#define WEBFAILSAFE_UPLOAD_NAND_KERNEL_SIZE			0x200000
#endif
#define WEBFAILSAFE_UPLOAD_ART_SIZE_IN_BYTES                    (64 * 1024)

// max. firmware size <= (FLASH_SIZE -  WEBFAILSAFE_UPLOAD_LIMITED_AREA_IN_BYTES)
#if defined(CONFIG_FOR_DLINK_DIR505_A1)
        // D-Link DIR-505: 64k(U-Boot),64k(ART),64k(MAC),64k(NVRAM),256k(Language)
        #define WEBFAILSAFE_UPLOAD_LIMITED_AREA_IN_BYTES        (512 * 1024)
#elif defined(CONFIG_FOR_8DEVICES_CARAMBOLA2)
        // Carambola 2: 256k(U-Boot),64k(U-Boot env),64k(ART)
        #define WEBFAILSAFE_UPLOAD_LIMITED_AREA_IN_BYTES        (384 * 1024)
#elif defined(CONFIG_FOR_DOMINO)
        // DOMINO 2: 256k(U-Boot),64k(U-Boot env),64k(ART)
        #define WEBFAILSAFE_UPLOAD_LIMITED_AREA_IN_BYTES        (384 * 1024)
#elif defined(CONFIG_FOR_DRAGINO_V2)
        // Dragino 2: 192k(U-Boot),64k(U-Boot env),64k(ART)
        #define WEBFAILSAFE_UPLOAD_LIMITED_AREA_IN_BYTES        (320 * 1024)
#elif defined(CONFIG_FOR_GS_OOLITE_V1_DEV)
        // GS-Oolite v1: 128k(U-Boot + MAC),64k(ART)
        #define WEBFAILSAFE_UPLOAD_LIMITED_AREA_IN_BYTES        (192 * 1024)
#else
        // TP-Link: 64k(U-Boot),64k(MAC/model/WPS pin block),64k(ART)
        #define WEBFAILSAFE_UPLOAD_LIMITED_AREA_IN_BYTES        (192 * 1024)
#endif

// progress state info
#define WEBFAILSAFE_PROGRESS_START                              0
#define WEBFAILSAFE_PROGRESS_TIMEOUT                    1
#define WEBFAILSAFE_PROGRESS_UPLOAD_READY               2
#define WEBFAILSAFE_PROGRESS_UPGRADE_READY              3
#define WEBFAILSAFE_PROGRESS_UPGRADE_FAILED             4

// update type
#define WEBFAILSAFE_UPGRADE_TYPE_FIRMWARE               0
#define WEBFAILSAFE_UPGRADE_TYPE_UBOOT                  1
#define WEBFAILSAFE_UPGRADE_TYPE_ART                    2
#define WEBFAILSAFE_UPGRADE_TYPE_NOR_FIRMWARE           3


#include <config.h>

#undef MTDPARTS_DEFAULT

#undef CFG_HZ

#include <atheros.h>


#ifndef FLASH_SIZE
#define FLASH_SIZE 8
#endif

#if (FLASH_SIZE > 16)
#define ATH_DUAL_NOR		1
#endif

#define _TO_STR(a) #a
#define TO_STR(a) _TO_STR(a)

#define CONFIG_SYS_VSNPRINTF

/*
 *define gl environment
*/
#ifndef CONFIG_AR300M
#define COMMAND_LF "if ping $serverip; then tftp $loadaddr $firmware_name && erase $firmware_addr +$filesize && cp.b $fileaddr $firmware_addr $filesize && echo OK!; else echo ERROR! Server not reachable!; fi" 
#else
#define COMMAND_LF "if ping $serverip; then tftp 0x80060000 $firmware_name && erase $firmware_addr +$filesize && cp.b $fileaddr $firmware_addr $filesize; if nand bad; then run dlf; fi; else echo ping $serverip failed; fi"
#endif
#define VAR_FIRMWARE_NAME "openwrt-gl-"CONFIG_BOARD_NAME".bin"

#define COMMAND_LU "if ping $serverip; then tftp $loadaddr $uboot_name && erase $uboot_addr +$uboot_size && cp.b $fileaddr $uboot_addr $filesize && echo OK!; else echo ERROR! Server not reachable!; fi"

#if 0
#define COMMAND_LC "tftp 0x80100000 config.bin && cp.b "TO_STR(GL_ART_ADDR)" 0x81000000 0xffff && cp.b 0x80100000 0x81000000 0x40 && cp.b 0x80100000 0x81001002 0x06 \
&& wmac 0x80100000 0x81001002 2 && wmac 0x80100000 0x81005006 3 && erase "TO_STR(GL_ART_ADDR)" +0xffff && cp.b 0x81000000 "TO_STR(GL_ART_ADDR)" 0xffff"
#else
#define COMMAND_LC "tftp 0x80100000 config.bin && cp.b "TO_STR(GL_ART_ADDR)" 0x81000000 0xffff && cp.b 0x80100000 0x81000000 0x40 && cp.b 0x80100000 0x81001002 0x06 && erase "TO_STR(GL_ART_ADDR)\
" +0xffff && cp.b 0x81000000 "TO_STR(GL_ART_ADDR)" 0xffff"
#endif

#define COMMAND_RLF "if ping $serverip; then tftp $loadaddr $firmware_name && erase $firmware_addr +$filesize && cp.b $fileaddr $firmware_addr $filesize && echo OK!; else echo ERROR! Server not reachable!; fi"

#ifdef CONFIG_AR300M
#define COMMAND_DLF "if ping 192.168.1.2; then echo ok; elif ping 192.168.1.2; then echo ok; elif ping 192.168.1.2; then echo ok; elif echo ping 192.168.1.2; then echo ok; elif echo ping 192.168.1.2; then \
echo ok; else echo ping finally failed; fi; tftp 0x81000000 openwrt-gl-"CONFIG_BOARD_NAME".img && nand erase && nand write $fileaddr 0 $filesize"
#else
#define COMMAND_DLF "if ping 192.168.1.2; then echo ok; elif ping 192.168.1.2; then echo ok; elif ping 192.168.1.2; then echo ok; elif echo ping 192.168.1.2; then echo ok; elif echo ping 192.168.1.2; then \
echo ok; else echo ping finally failed; fi; tftp 0x81000000 openwrt-gl-"CONFIG_BOARD_NAME".img && erase $firmware_addr +0x200000 && cp.b $fileaddr $firmware_addr 0x200000 && nand erase && nand write 0x81200000 0 $filesize"	
#endif

#define VAR_UBOOT_ADDR  0x9f000000
#define VAR_UBOOT_SIZE  0x00050000
#define VAR_UBOOT_NAME  "uboot-gl-"CONFIG_BOARD_NAME".bin"
#define VAR_FIRMWARE_ADDR GL_BOOT_ADDR

/*****************gl environment end************/
/*-----------------------------------------------------------------------
 * FLASH and environment organization
 */
#define CFG_MAX_FLASH_BANKS	1	/* max number of memory banks */
#if (FLASH_SIZE == 32)
#define CFG_MAX_FLASH_SECT	512	/* max number of sectors on one chip */
#define ATH_MTDPARTS_MIB0	"64k(mib0)"
#define ATH_ROOTFS_SIZE		"14528k(rootfs)"
#elif (FLASH_SIZE == 16)
#define CFG_MAX_FLASH_SECT	256	/* max number of sectors on one chip */
#define ATH_MTDPARTS_MIB0	"64k(mib0)"
#define ATH_ROOTFS_SIZE		"14528k(rootfs)"
#elif (FLASH_SIZE == 8)
#define CFG_MAX_FLASH_SECT	128	/* max number of sectors on one chip */
#define ATH_MTDPARTS_MIB0	"64k(mib0)"
#define ATH_ROOTFS_SIZE		"6336k(rootfs)"
#elif (FLASH_SIZE == 1/2)
#define CFG_MAX_FLASH_SECT	8	/* max number of sectors on one chip */
#else
#define CFG_MAX_FLASH_SECT	64	/* max number of sectors on one chip */
#define ATH_MTDPARTS_MIB0	"64k(mib0)"
#define ATH_ROOTFS_SIZE		"6336k(rootfs)"
#endif

#define CFG_FLASH_SECTOR_SIZE	(64*1024)
#if (FLASH_SIZE == 32)
#define CFG_FLASH_SIZE		0x02000000	/* Total flash size */
#elif (FLASH_SIZE == 16)
#define CFG_FLASH_SIZE		0x01000000	/* Total flash size */
#elif (FLASH_SIZE == 8)
#define CFG_FLASH_SIZE		0x00800000	/* max number of sectors on one chip */
#elif (FLASH_SIZE == 1/2)
#define CFG_FLASH_SIZE		0x00080000	/* Total flash size */
#else
#define CFG_FLASH_SIZE		0x00400000	/* Total flash size */
#endif

#ifndef COMPRESSED_UBOOT
#define ENABLE_DYNAMIC_CONF	1
#endif

#undef CFG_ATHRS26_PHY

#if (CFG_MAX_FLASH_SECT * CFG_FLASH_SECTOR_SIZE) != CFG_FLASH_SIZE
#	error "Invalid flash configuration"
#endif

#define CFG_FLASH_WORD_SIZE	unsigned short

#if defined(CONFIG_ATH_NAND_BR) && defined(COMPRESSED_UBOOT)
#define CFG_FLASH_BASE			0xa0100000
#else
/* NOR Flash start address */
#define CFG_FLASH_BASE			0x9f000000
#endif

#ifdef COMPRESSED_UBOOT
#define BOOTSTRAP_TEXT_BASE		CFG_FLASH_BASE
#define BOOTSTRAP_CFG_MONITOR_BASE	BOOTSTRAP_TEXT_BASE
#endif

#define CONFIG_PCI_CONFIG_DATA_IN_OTP

/*
 * Defines to change flash size on reboot
 */
#ifdef ENABLE_DYNAMIC_CONF
#define UBOOT_FLASH_SIZE		(256 * 1024)
#define UBOOT_ENV_SEC_START		(CFG_FLASH_BASE + UBOOT_FLASH_SIZE)

#define CFG_FLASH_MAGIC			0xaabacada
#define CFG_FLASH_MAGIC_F		(UBOOT_ENV_SEC_START + CFG_FLASH_SECTOR_SIZE - 0x20)
#define CFG_FLASH_SECTOR_SIZE_F		*(volatile int *)(CFG_FLASH_MAGIC_F + 0x4)
#define CFG_FLASH_SIZE_F		*(volatile int *)(CFG_FLASH_MAGIC_F + 0x8) /* Total flash size */
#define CFG_MAX_FLASH_SECT_F		(CFG_FLASH_SIZE / CFG_FLASH_SECTOR_SIZE) /* max number of sectors on one chip */
#else
#define CFG_FLASH_SIZE_F		CFG_FLASH_SIZE
#define CFG_FLASH_SECTOR_SIZE_F		CFG_FLASH_SECTOR_SIZE
#endif


#define CFG_DDR_REFRESH_VAL		0x40c2

/*
 * The following #defines are needed to get flash environment right
 */
#define	CFG_MONITOR_BASE	TEXT_BASE
#define	CFG_MONITOR_LEN		(192 << 10)

#undef CONFIG_BOOTARGS

#define __gen_cmd(n, a, f, ec, cc, el)		\
	#n "=tftp 0x80060000 ${dir}" #f "&&"	\
	#ec " " #a " " #el "&&"			\
	#cc " $fileaddr " #a " $filesize\0"

#if (FLASH_SIZE == 32)
#define __gen_cmd2(n, a1, a2, f, ec, cc, el)	\
	#n "=tftp 0x80060000 ${dir}" #f "&&"	\
	#ec " " #a1 " " #el "&&"		\
	#cc " $fileaddr " #a1 " $filesize &&"	\
	#ec " " #a2 " " #el "&&"		\
	#cc " $fileaddr " #a2 " $filesize\0"
#define gen_cmd2(n, a1, a2, f)			\
	__gen_cmd2(n, a1, a2, f, erase, cp.b, +$filesize)
#endif

#define gen_cmd(n, a, f)			\
	__gen_cmd(n, a, f, erase, cp.b, +$filesize)

#define gen_cmd_el(n, a, f, el)			\
	__gen_cmd(n, a, f, erase, cp.b, +el)

#define nand_gen_cmd(n, a, f, s)		\
	__gen_cmd(n, a, f, nand erase, nand write, s)

#define __fs_name(x, y)		x ## y
#define _fs_name(x, y)		__fs_name(x, y)
#define fs_name(y)		_fs_name(__CONFIG_BOARD_NAME, y)

#ifdef COMPRESSED_UBOOT
#	define ATH_U_FILE	tuboot.bin
#else
#	define ATH_U_FILE	u-boot.bin
#endif

#define ATH_UBI_ARGS	" "

#ifdef CONFIG_ATH_NAND_SUPPORT
#	ifdef CONFIG_ATH_NAND_BR	// nand boot rom
#		if defined(COMPRESSED_UBOOT)
#			define ATH_U_CMD	nand_gen_cmd(lu, 0x0, 2fw.bin, 0x20000)
#			define MTDPARTS_DEFAULT	"mtdparts=ath-nand:128k(u-boot),384k(free),1280k(uImage),7m(rootfs),128k(dummy),128k(caldata)"
#		else
#		define ATH_U_CMD	nand_gen_cmd(lu, 0x0, 2fw.bin, 0x40000)
#		define MTDPARTS_DEFAULT "mtdparts=ath-nand:256k(u-boot),256k(u-boot-env),1280k(uImage),7m(rootfs),128k(dummy),128k(caldata)"
#		endif
#		define ATH_ROOT_DEV	"31:03"
#		define CFG_ENV_ADDR	0x00040000
#		define ATH_F_LEN	0x700000
#		define ATH_F_ADDR	0x1c0000
#		define ATH_K_ADDR	0x80000
#		define ATH_K_LEN	0x140000
#	else //dual flash
#		ifdef ATH_SPI_NAND
#			if CONFIG_ATH_DUAL_IMAGE_SUPPORT
#                               define MTDPARTS_DEFAULT "mtdparts=spi0.0:256k(u-boot)ro,64k(u-boot-env),128k(reserved),64k(art);spi0.1:2m(kernel),20m(rootfs),2m(k-2),20m(r-2),84m(user),22m@0x0(firmware),22m@0x1600000(fw-2)"
#				define ATH_UBI_ARGS	" ubi.mtd=5,2048 root=/dev/mtdblock11 "
#                       else
#				define MTDPARTS_DEFAULT "mtdparts=spi0.0:256k(u-boot)ro,64k(u-boot-env),128k(reserved),64k(art);spi0.1:2m(kernel),20m(rootfs),106m(data),22m@0x0(firmware)"
#				define ATH_UBI_ARGS	" ubi.mtd=5,2048 root=/dev/mtdblock8 "
#			endif
#			define ATH_F_LEN        0x1400000
#			define ATH_F_ADDR       0x200000
#			define ATH_K_ADDR       0x0
#			define ATH_K_LEN        0x200000
#			define CFG_ENV_ADDR	0x9f040000
#		else
#			error "Invalid NAND flash configuration"
#		endif
#	endif
#	define ATH_F_FILE		fs_name(${bc}-nand-jffs2)
#	define ATH_K_FILE		vmlinux${bc}.lzma.uImage
#	define ATH_F_CMD		nand_gen_cmd(lf, ATH_F_ADDR, ATH_F_FILE, ATH_F_LEN)
#	define ATH_K_CMD		nand_gen_cmd(lk, ATH_K_ADDR, ATH_K_FILE, ATH_K_LEN)
#	define ATH_EXTRA_ENV		"bootdevice=0\0"
#else
#	if defined(COMPRESSED_UBOOT)
#		define ATH_U_FILE	tuboot.bin
#		define ATH_F_FILE	fs_name(${bc}-jffs2)
#		define ATH_F_LEN	$filesize
#		define ATH_F_ADDR	0x9f010000
#		define ATH_K_FILE	vmlinux${bc}.lzma.uImage
#		define ATH_K_ADDR	0x9f300000
		/*
		 * For compressed uboot, environment sector is not used.
		 * Hence the mtd partition indices get reduced by 1.
		 * This conflicts with
		 *	- minor no. for /dev/caldata in
		 *		build/scripts/{board}/dev.txt
		 *	- root=<rooot dev> kernel cmdline parameter
		 * Hence, doing a dummy split of the u-boot partition
		 * to maintain the same minor no. as in the normal u-boot.
		 */
#		define MTDPARTS_DEFAULT	"mtdparts=ath-nor0:32k(u-boot1),32k(u-boot2),3008k(rootfs),896k(uImage),64k(mib0),64k(ART)"
#	else
#ifdef CONFIG_FOR_GL_BOARD
#       define ATH_F_FILE               fs_name(${bc}-jffs2)
#       define ATH_F_LEN                0xE30000
#       define ATH_F_ADDR               0x9f050000
#       define ATH_K_FILE               vmlinux${bc}.lzma.uImage
#       define ATH_K_ADDR               0x9f060000
#       define MTDPARTS_DEFAULT         "mtdparts=ath-nor0:256k(u-boot),64k(u-boot-env),14528k(rootfs),1408k(uImage)," ATH_MTDPARTS_MIB0 ",64k(ART)"
#	elif (FLASH_SIZE == 16 || FLASH_SIZE == 32)
#		define ATH_F_LEN		0xE30000
#		define ATH_K_ADDR		0x9fE80000
#	else
#		define ATH_F_LEN		0x630000
#		define ATH_K_ADDR		0x9f680000
# 	endif
#		define ATH_F_ADDR		0x9f050000
#		define ATH_K_FILE		vmlinux${bc}.lzma.uImage

#		if CONFIG_ATH_DUAL_IMAGE_SUPPORT
#			if (FLASH_SIZE == 32)
#				define MTDPARTS_DEFAULT	"mtdparts=spi0.0:256k(u-boot)ro,64k(u-boot-env)," ATH_ROOTFS_SIZE ",1472k(kernel),64k(art),256k(reserved),14528k(r-2),1472k(k-2),128k(reserved2),16000k@0x50000(firmware),16000k@0x1040000(fw-2)"
#			else
#				if defined(CONFIG_ATH_SPI_CS1_GPIO)
#					define MTDPARTS_DEFAULT	"mtdparts=spi0.0:256k(u-boot)ro,64k(u-boot-env)," ATH_ROOTFS_SIZE ",1472k(kernel),64k(art),16000k@0x50000(firmware);spi0.1:14528k(r-2),1472k(k-2),384k(reserved),16000k@0x0(fw-2)"
#				else /* FLASH_SIZE == 16 */
#					define ATH_F_LEN		0x660000
#					define ATH_K_ADDR		0x9f6B0000
#					define MTDPARTS_DEFAULT	"mtdparts=spi0.0:256k(u-boot),64k(u-boot-env),6528k(rootfs),1472k(kernel),6528k(r-2),1472k(k-2),64k(art),8000k@0x50000(firmware),8000k@0x820000(fw-2)"
#				endif
#			endif
#		else
#			if (FLASH_SIZE == 32)
#				define MTDPARTS_DEFAULT	"mtdparts=ath-nor0:256k(u-boot),64k(u-boot-env)," ATH_ROOTFS_SIZE ",1472k(uImage)," "64k(ART),256k(reserved),16128k(usr)"
#			else
#				define MTDPARTS_DEFAULT	"mtdparts=ath-nor0:256k(u-boot),64k(u-boot-env)," ATH_ROOTFS_SIZE ",1408k(uImage)," ATH_MTDPARTS_MIB0 ",64k(ART)"
#			endif
#		endif
#	endif
#endif /*CONFIG_MI124*/

#ifndef ATH_ROOT_DEV
#	define ATH_ROOT_DEV	"31:02"
#endif

#ifndef ATH_EXTRA_ENV
#	define ATH_EXTRA_ENV
#endif

#ifndef ATH_U_CMD
#	if (FLASH_SIZE == 32)
#		define ATH_U_CMD	gen_cmd2(lu, 0x9f000000, 0xa0000000, ATH_U_FILE)
#	else
#		define ATH_U_CMD	gen_cmd(lu, 0x9f000000, ATH_U_FILE)
#	endif
#endif

#ifndef ATH_F_CMD
#	define ATH_F_CMD	gen_cmd_el(lf, ATH_F_ADDR, ATH_F_FILE, ATH_F_LEN)
#endif

#ifndef ATH_K_CMD
#	define ATH_K_CMD	gen_cmd(lk, ATH_K_ADDR, ATH_K_FILE)
#endif

#define CONFIG_EXTRA_ENV_SETTINGS	\
	"dir=\0" ATH_U_CMD ATH_F_CMD ATH_K_CMD ""

#ifdef BOARD_NAME
	#define CONFIG_BOOTARGS "board=" BOARD_NAME " console=ttyS0,115200 " ATH_UBI_ARGS MTDPARTS_DEFAULT " rootfstype=squashfs,jffs2 noinitrd"
#else
	#define	CONFIG_BOOTARGS	"console=ttyS0,115200 root=" ATH_ROOT_DEV " rootfstype=jffs2 init=/sbin/init " MTDPARTS_DEFAULT
#endif
//#define CFG_PLL_FREQ    CFG_PLL_720_600_200

/*
 * timeout values are in ticks
 */
#define CFG_FLASH_ERASE_TOUT	(2 * CFG_HZ) /* Timeout for Flash Erase */
#define CFG_FLASH_WRITE_TOUT	(2 * CFG_HZ) /* Timeout for Flash Write */

/*
 * Cache lock for stack
 */
#define CFG_INIT_SP_OFFSET	0x1000
#define CFG_INIT_SRAM_SP_OFFSET	0xbd001800

#ifdef CONFIG_ATH_NAND_SUPPORT
#ifdef ATH_SPI_NAND
#	define CONFIG_BOOTCOMMAND       "nboot 0x81000000 0 0"
#else
#	define CONFIG_BOOTCOMMAND	"nboot 0x81000000 0 0x80000"
#endif
#else
#	define CFG_ENV_ADDR		0x9f040000
#ifdef CONFIG_FOR_GL_BOARD
#	define CONFIG_BOOTCOMMAND       "bootm 0x9f060000"
#else
#		if (FLASH_SIZE == 16 || FLASH_SIZE == 32)
#			define CONFIG_BOOTCOMMAND	"bootm 0x9fE80000"
#		else
#			define CONFIG_BOOTCOMMAND	"bootm 0x9f680000"
#		endif
#	endif
#endif



#ifdef ENABLE_DYNAMIC_CONF
#define CFG_DDR_MAGIC		0xaabacada
#define CFG_DDR_MAGIC_F		(UBOOT_ENV_SEC_START + CFG_FLASH_SECTOR_SIZE - 0x30)
#define CFG_DDR_CONFIG_VAL_F	*(volatile int *)(CFG_DDR_MAGIC_F + 4)
#define CFG_DDR_CONFIG2_VAL_F	*(volatile int *)(CFG_DDR_MAGIC_F + 8)
#define CFG_DDR_EXT_MODE_VAL_F	*(volatile int *)(CFG_DDR_MAGIC_F + 12)
#endif

#define CONFIG_NET_MULTI
#define CONFIG_MEMSIZE_IN_BYTES
#define CONFIG_PCI 1

/*-----------------------------------------------------------------------
 * Cache Configuration
 */
#ifndef COMPRESSED_UBOOT
#define ATH_CFG_COMMANDS	((			\
				CONFIG_CMD_DFL	|	\
				CFG_CMD_DHCP	|	\
				CFG_CMD_ELF	|	\
				CFG_CMD_PCI	|	\
				CFG_CMD_FLS	|	\
				CFG_CMD_MII	|	\
				CFG_CMD_PING	|	\
				CFG_CMD_NET	|	\
				CFG_CMD_ENV	|	\
				CFG_CMD_PLL	|	\
				CFG_CMD_FLASH	|	\
				CFG_CMD_RUN	|	\
				CFG_CMD_ELF	|	\
				CFG_CMD_DDR	|	\
				CFG_CMD_ETHREG		\
				) & ~(			\
				CFG_CMD_IMLS	|	\
				CFG_CMD_FLASH		\
				))
#else
#	ifdef CONFIG_ATH_NAND_BR
#		define ATH_CFG_COMMANDS		((			\
						CONFIG_CMD_DFL	|	\
						CFG_CMD_PING	|	\
						CFG_CMD_NET) & ~(	\
						CFG_CMD_FLASH		\
						))
#	else
#		define ATH_CFG_COMMANDS		(CONFIG_CMD_DFL	|	\
				CFG_CMD_PING	|	\
				CFG_CMD_NET)
#	endif
#endif /* #ifndef COMPRESSED_UBOOT */

#ifdef CONFIG_ATH_NAND_SUPPORT
#	ifdef CONFIG_ATH_NAND_BR
#		define CFG_ENV_IS_IN_NAND	1
#		define CFG_ENV_OFFSET		0x40000u
#		define CFG_ENV_SIZE		0x40000u
#		define ATH_EXTRA_CMD		CFG_CMD_NAND
#	else
#		define CFG_ENV_IS_IN_FLASH	1
#		define CFG_ENV_SIZE		CFG_FLASH_SECTOR_SIZE
#		define ATH_EXTRA_CMD		(CFG_CMD_NAND | CFG_CMD_FLASH)
#	endif
#	define NAND_MAX_CHIPS			1
#	define CFG_MAX_NAND_DEVICE		1
#else
#	define ATH_EXTRA_CMD			CFG_CMD_FLASH
#	define CFG_ENV_IS_IN_FLASH		1
#	define CFG_ENV_SIZE			CFG_FLASH_SECTOR_SIZE
#endif

#define DEBUG

#ifdef COMPRESSED_UBOOT
#undef  CFG_ENV_IS_IN_FLASH
#undef  CFG_ENV_IS_IN_NAND
#define CFG_ENV_IS_NOWHERE		1
#endif

#define CONFIG_COMMANDS			(ATH_CFG_COMMANDS | ATH_EXTRA_CMD)

#define CONFIG_IPADDR			192.168.1.1
#define CONFIG_SERVERIP			192.168.1.2
#define CONFIG_ETHADDR			0x00:0xaa:0xbb:0xcc:0xdd:0xee
#define CFG_FAULT_ECHO_LINK_DOWN	1

#define CFG_PHY_ADDR			0
#define CFG_GMII			0
#define CFG_MII0_RMII			1
#define CFG_AG7100_GE0_RMII		1

#define CFG_BOOTM_LEN			(16 << 20) /* 16 MB */
#define CFG_HUSH_PARSER
#define CFG_PROMPT_HUSH_PS2		"hush>"

/*
** Parameters defining the location of the calibration/initialization
** information for the two Merlin devices.
** NOTE: **This will change with different flash configurations**
*/
#if (FLASH_SIZE == 32)
#define BOARDCAL			(CFG_FLASH_BASE + 0x01000000 - 0x10000)
#else
#define BOARDCAL                        (CFG_FLASH_BASE + CFG_FLASH_SIZE - 0x10000)
#endif

#define WLANCAL				(BOARDCAL + 0x1000)
#define ATHEROS_PRODUCT_ID		137
#define CAL_SECTOR			(CFG_MAX_FLASH_SECT - 1)

/* For Merlin, both PCI, PCI-E interfaces are valid */
#define ATH_ART_PCICFG_OFFSET		12

#include <cmd_confdefs.h>

#endif	/* __BOARD_953X_H */
