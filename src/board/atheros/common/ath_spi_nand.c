/*
 * Copyright (c) 2014 The Linux Foundation. All rights reserved.
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
/*
 * linux/drivers/mtd/nand/ath_spi_nand.c
 * vim: tabstop=8 : noexpandtab
 * Derived from atheros nand driver.
 */
#include <common.h>
#include <command.h>
#include <asm/addrspace.h>
#include <asm/io.h>
#include <asm/types.h>
#include <config.h>
#include <atheros.h>
#include <malloc.h>

#include <linux/types.h>
#include <linux/string.h>
#include <linux/bitops.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include "ath_spi.h"

#define ENOMEM		12
#define EINVAL		22
#define EIO		5
#define ETIMEDOUT       110
#define EUCLEAN         117


#define writesize	oobblock

#define DRV_NAME	"ath-spi-nand"
#define DRV_VERSION	"0.1"
#define DRV_AUTHOR	"Qualcomm Atheros"
#define DRV_DESC	"Qualcomm Atheros SPI NAND Driver"

#define ATH_SPI_NAND_CMD_WRITE_ENABLE		0x06
#define ATH_SPI_NAND_CMD_WRITE_DISABLE		0x04
#define ATH_SPI_NAND_CMD_GET_FEATURE		0x0f
#define ATH_SPI_NAND_CMD_SET_FEATURE		0x1f
#define ATH_SPI_NAND_CMD_PAGE_READ_TO_CACHE	0x13
#define ATH_SPI_NAND_CMD_READ_FROM_CACHE	0x03
#define ATH_SPI_NAND_CMD_READ_ID		0x9f
#define ATH_SPI_NAND_CMD_BLOCK_ERASE		0xd8
#define ATH_SPI_NAND_CMD_RESET			0xff
#define ATH_SPI_NAND_CMD_PROGRAM_LOAD		0x02
#define ATH_SPI_NAND_CMD_PROGRAM_EXECUTE	0x10

#define ATH_SPI_NAND_BLK_PROT			0xa0
#define ATH_SPI_NAND_BP_BRWD			(1 << 7)
#define ATH_SPI_NAND_BP_BP2			(1 << 5)
#define ATH_SPI_NAND_BP_BP1			(1 << 4)
#define ATH_SPI_NAND_BP_BP0			(1 << 3)
#define ATH_SPI_NAND_BP_INV			(1 << 2)
#define ATH_SPI_NAND_BP_CMP			(1 << 1)
#define ATH_SPI_NAND_BP_MASK			(ATH_SPI_NAND_BP_BP2 |	\
						 ATH_SPI_NAND_BP_BP1 |	\
						 ATH_SPI_NAND_BP_BP0 |	\
						 ATH_SPI_NAND_BP_INV |	\
						 ATH_SPI_NAND_BP_CMP)

#define ATH_SPI_NAND_FEAT			0xb0
#define ATH_SPI_NAND_FEAT_OTP_PRT		(1 << 7)
#define ATH_SPI_NAND_FEAT_OTP_EN		(1 << 6)
#define ATH_SPI_NAND_FEAT_ECC_EN		(1 << 4)
#define ATH_SPI_NAND_FEAT_QE			(1 << 0)

#define ATH_SPI_NAND_STATUS			0xc0
#define ATH_SPI_NAND_STATUS_ECCS2		(1 << 6)
#define ATH_SPI_NAND_STATUS_ECCS1		(1 << 5)
#define ATH_SPI_NAND_STATUS_ECCS0		(1 << 4)
#define ASNS_ECC_ERR_NONE			0
#define ASNS_ECC_ERR_CORR_1_7			1
#define ASNS_ECC_ERR_CORR_8			3
#define ATH_SPI_NAND_STATUS_P_FAIL		(1 << 3)
#define ATH_SPI_NAND_STATUS_E_FAIL		(1 << 2)
#define ATH_SPI_NAND_STATUS_WEL			(1 << 1)
#define ATH_SPI_NAND_STATUS_OIP			(1 << 0)

#define ATH_NAND_IO_DBG				0
#define ATH_NAND_OOB_DBG			0
#define ATH_NAND_IN_DBG				0

#define ATH_SPI_NAND_GET_ECCS(sc, status)	(((status) >> 4) & \
						 ((sc)->info.ecc_mask))
#define ATH_SPI_NAND_UNCORR(sc)			((sc)->info.uncorr_code)

#define ATH_NAND_MID				0xc8
/* HeYang Tek spi nand */
#define HYT_NAND_MID				0xc9
/*Etron nand*/
#define ETH_NAND_MID				0xd5
#define MX_NAND_MID				0xA1
#define FM_NAND_MID                             0XA1
enum {
	ATH_NAND_VER_1 = 0,
	ATH_NAND_VER_2,
	ATH_NAND_VER_MAX
};

#if ATH_NAND_IO_DBG
#	define iodbg	printk
#	define oobdbg	printk
#else
#	define iodbg(...)
#	define oobdbg(...)
#endif

#if ATH_NAND_IN_DBG
#	define indbg(a, ...)					\
	do {							\
		printk("--- %s(%d):" a "\n",			\
			__func__, __LINE__, ## __VA_ARGS__);	\
	} while (0)
#else
#	define indbg(...)
#	define indbg1(a, ...)					\
	do {							\
		printk("--- %s(%d):" a "\n",			\
			__func__, __LINE__, ## __VA_ARGS__);	\
	} while (0)
#endif

#define ATH_SPI_NAND_CE_LOW		(ATH_SPI_CS_ENABLE_1 & ~ATH_SPI_CLK_HIGH)
#define ATH_SPI_NAND_CE_HIGH		(ATH_SPI_CS_ENABLE_1 | ATH_SPI_CLK_HIGH)

#define  bit_banger 0

#if bit_banger
#define ath_spi_nand_bit_banger(__b)	\
	ath_spi_bit_banger(ATH_SPI_NAND_CE_LOW, ATH_SPI_NAND_CE_HIGH, __b)
#define ath_spi_nand_send_addr(__a)	\
	ath_spi_send_addr(ATH_SPI_NAND_CE_LOW, ATH_SPI_NAND_CE_HIGH, __a)
#else
/* By Default :: Use word banging */
#define ath_spi_nand_bit_banger(__b)	\
	ath_spi_word_banger(__b, 8, 0xa0000000);
#define ath_spi_nand_send_addr(__b)	\
	ath_spi_word_banger(__b, 24, 0xa0000000)
#define ath_spi_nand_send_2byte_addr(__b)	\
	ath_spi_word_banger(__b, 16, 0xa0000000)
#endif

#define ath_spi_nand_go()	ath_spi_go(ATH_SPI_NAND_CE_LOW)
#define ath_spi_nand_read_byte()	\
	ath_spi_bit_banger(ATH_SPI_NAND_CE_LOW, ATH_SPI_NAND_CE_HIGH, 0)

#define ath_spi_nand_start()					\
do {								\
	ath_reg_wr(ATH_SPI_WRITE, ATH_SPI_CS_DIS);		\
	ath_reg_wr(ATH_SPI_WRITE, ATH_SPI_NAND_CE_LOW);		\
} while (0)

#define ath_spi_nand_end()					\
do {								\
	ath_reg_wr(ATH_SPI_WRITE, ATH_SPI_NAND_CE_LOW);		\
	ath_reg_wr(ATH_SPI_WRITE, ATH_SPI_CS_DIS);		\
} while (0)

struct ath_spi_nand_info {
	int 		version;
	uint8_t		ecc_mask;
	uint8_t		uncorr_code;
};

/* ath nand info */
typedef struct {
	/* mtd info */
	struct mtd_info		*mtd;

	unsigned int		page_size;

	/* NAND MTD partition information */
	int			nr_partitions;
	struct mtd_partition	*partitions;
	unsigned		*bbt;
#define oob_support	1
	struct nand_ecclayout *ecclayout;
#if oob_support
	uint8_t			*raw;
	int			rawlen;
#endif
	struct ath_spi_nand_info info;
} ath_spi_nand_sc_t;

ath_spi_nand_sc_t ath_spi_nand_sc;


#define	bbt_index				(sizeof(*sc->bbt) * 8 / 2)
#define ATH_NAND_BLK_DONT_KNOW			0x0
#define ATH_NAND_BLK_GOOD			0x1
#define ATH_NAND_BLK_BAD			0x2
#define ATH_NAND_BLK_ERASED			0x3

/* lifted from linux */
typedef enum {
	MTD_OOB_PLACE,
	MTD_OOB_AUTO,
	MTD_OOB_RAW,
} mtd_oob_mode_t;

struct mtd_oob_ops {
	mtd_oob_mode_t  mode;
	size_t          len;
	size_t          retlen;
	size_t          ooblen;
	size_t          oobretlen;
	uint32_t        ooboffs;
	uint8_t         *datbuf;
	uint8_t         *oobbuf;
};

struct nand_oobfree {
	uint32_t offset;
	uint32_t length;
};

#define MTD_MAX_OOBFREE_ENTRIES 8
/*
 * ECC layout control structure. Exported to userspace for
 * diagnosis and to allow creation of raw images
 */
struct nand_ecclayout {
	uint32_t eccbytes;
	uint32_t eccpos[96];
	uint32_t oobavail;
	struct nand_oobfree oobfree[MTD_MAX_OOBFREE_ENTRIES];
};


struct mtd_info nand_info[CFG_MAX_NAND_DEVICE];
int nand_curr_device = 0;

inline unsigned
ath_spi_nand_get_blk_state(struct mtd_info *mtd, loff_t b)
{
	unsigned		x, y;
	ath_spi_nand_sc_t	*sc = mtd->priv;

	if (!sc->bbt)	return ATH_NAND_BLK_DONT_KNOW;

	b = b >> mtd->erasesize_shift;

	x = b / bbt_index;
	y = b % bbt_index;

	return (sc->bbt[x] >> (y * 2)) & 0x3;
}

inline void
ath_spi_nand_set_blk_state(struct mtd_info *mtd, loff_t b, unsigned state)
{
	unsigned		x, y;
	ath_spi_nand_sc_t	*sc = mtd->priv;

	if (!sc->bbt)	return;

	b = b >> mtd->erasesize_shift;

	x = b / bbt_index;
	y = b % bbt_index;

	sc->bbt[x] = (sc->bbt[x] & ~(3 << (y * 2))) | (state << (y * 2));
}

inline int
ath_spi_nand_read_id(int version)
{
	uint32_t id;

	ath_spi_nand_start();
	ath_spi_nand_bit_banger(0x9F);

	if (version == ATH_NAND_VER_1)
		ath_spi_nand_bit_banger(0x00);

	ath_spi_nand_read_byte();
	ath_spi_nand_read_byte();

	if (version == ATH_NAND_VER_2)
		ath_spi_nand_read_byte();

	id = ath_reg_rd(ATH_SPI_RD_STATUS);

	ath_spi_nand_end();

	if (version == ATH_NAND_VER_2)
		id >>= 8;
	printf("manu id %X\n",id);
	return id;
}

static int
ath_parse_read_id(ath_spi_nand_sc_t *sc, uint8_t *did)
{
	uint32_t version, id;

#define get_ath_spi_nand_vendor(x)		(((x) >> 8) & 0xff)
#define get_ath_spi_nand_device(x)		((x) & 0xff)

	for (version = ATH_NAND_VER_1; version < ATH_NAND_VER_MAX; version++) {
		id = ath_spi_nand_read_id(version);

		if (get_ath_spi_nand_vendor(id) != ATH_NAND_MID &&
		    get_ath_spi_nand_vendor(id) != HYT_NAND_MID &&
		    get_ath_spi_nand_vendor(id) != ETH_NAND_MID &&
		    get_ath_spi_nand_vendor(id) != MX_NAND_MID)
			continue;

		*did = get_ath_spi_nand_device(id);
		return 0;
	}

	return -1;
}

static void
ath_gpio_config_output(int gpio)
{
#if defined(CONFIG_MACH_AR934x) || \
    defined(CONFIG_MACH_QCA955x) || \
    defined(CONFIG_MACH_QCA953x) || \
        defined(CONFIG_MACH_QCA956x)
        ath_reg_rmw_clear(ATH_GPIO_OE, (1 << gpio));
#else
        ath_reg_rmw_set(ATH_GPIO_OE, (1 << gpio));
#endif
}

static void
ath_gpio_set_fn(int gpio, int fn)
{
#define gpio_fn_reg(x)  ((x) / 4)
#define gpio_lsb(x)     (((x) % 4) * 8)
#define gpio_msb(x)     (gpio_lsb(x) + 7)
#define gpio_mask(x)    (0xffu << gpio_lsb(x))
#define gpio_set(x, f)  (((f) & 0xffu) << gpio_lsb(x))

        uint32_t *reg = ((uint32_t *)GPIO_OUT_FUNCTION0_ADDRESS) +
                                        gpio_fn_reg(gpio);

        ath_reg_wr(reg, (ath_reg_rd(reg) & ~gpio_mask(gpio)) | gpio_set(gpio, fn));
}


int
ath_spi_nand_set_feature(uint32_t addr, uint8_t val)
{
	if ((addr != ATH_SPI_NAND_BLK_PROT) &&
	    (addr != ATH_SPI_NAND_FEAT) &&
	    (addr != ATH_SPI_NAND_STATUS))
		return -EINVAL;

	ath_spi_nand_start();
	ath_spi_nand_bit_banger(ATH_SPI_NAND_CMD_SET_FEATURE);
	ath_spi_nand_bit_banger(addr);
	ath_spi_nand_bit_banger(val);
	ath_spi_nand_end();
	return 0;
}

int
ath_spi_nand_get_feature(uint8_t addr)
{
	uint32_t ret;

	if ((addr != ATH_SPI_NAND_BLK_PROT) &&
	    (addr != ATH_SPI_NAND_FEAT) &&
	    (addr != ATH_SPI_NAND_STATUS))
		return -EINVAL;

	ath_spi_nand_start();
	ath_spi_nand_bit_banger(ATH_SPI_NAND_CMD_GET_FEATURE);
	ath_spi_nand_bit_banger(addr);
	ath_spi_nand_read_byte();
	ret = ath_reg_rd(ATH_SPI_RD_STATUS);
	ath_spi_nand_end();
	return (ret & 0xff);
}

static int
__ath_spi_nand_status(ath_spi_nand_sc_t *sc, const char *f)
{
	int i, status;
	/*
	 * Per the Giga Device GD5F1GQ4UAYIG data sheet, time taken for the
	 * following operations are:
	 *	Page Program time: 400us typical
	 *	Block Erase time: 3ms typical
	 *	Page read time: 120us maximum(w/I ECC)
	 *
	 * Set 'max' to maximum of the above
	 */
	/* HeYang Tek spi nand flash need 60ms */
	const int max = 6000;

	ath_spi_nand_start();
	ath_spi_nand_bit_banger(ATH_SPI_NAND_CMD_GET_FEATURE);
	ath_spi_nand_bit_banger(ATH_SPI_NAND_STATUS);

	for (i = 0; i < max; i++) {
		ath_spi_nand_read_byte();
		status = ath_reg_rd(ATH_SPI_RD_STATUS);
		if (status & ATH_SPI_NAND_STATUS_OIP)
			udelay(10);
		else
			break;
	}

	ath_spi_nand_end();

	if (max == i) {
		printk("%s: Operation timed out\n", __func__);
		return -ETIMEDOUT;
	}

	return (status & 0xff);
}
#define ath_spi_nand_status(sc) __ath_spi_nand_status(sc, __func__)

static int
__ath_spi_set_nand_status(ath_spi_nand_sc_t *sc, const char *f)
{
	int i, status;

	status=ath_spi_nand_status(sc);
	status = status & (~ATH_SPI_NAND_STATUS_E_FAIL);
	ath_spi_nand_set_feature(ATH_SPI_NAND_STATUS,status);
	udelay(60000);
	status=ath_spi_nand_status(sc);
	if(status & ATH_SPI_NAND_STATUS){
	printf("bad black\n");
	}
	return (0);
}
#define ath_spi_set_nand_status(sc) __ath_spi_set_nand_status(sc, __func__)

static int
ath_spi_nand_ecc(ath_spi_nand_sc_t *sc, int on)
{
	uint32_t feat;
	char *msg[] = { "dis", "en" };

	feat = ath_spi_nand_get_feature(ATH_SPI_NAND_FEAT);

	if (on) {
		/* Enable internal ECC */
		
		ath_spi_nand_set_feature(ATH_SPI_NAND_FEAT,
					feat | ATH_SPI_NAND_FEAT_ECC_EN);
		feat = ATH_SPI_NAND_FEAT_ECC_EN;
	} else {
		feat &= ~ATH_SPI_NAND_FEAT_ECC_EN;
		ath_spi_nand_set_feature(ATH_SPI_NAND_FEAT, feat);
	}

	udelay(60000);

	if ((ath_spi_nand_get_feature(ATH_SPI_NAND_FEAT) & feat) != feat) {
		printk("%s: Couldn't %sable internal ECC\n", __func__, msg[on]);
		return -EIO;
	}

	return 0;
}

inline int
ath_spi_nand_reset(ath_spi_nand_sc_t *sc)
{
	ath_spi_nand_start();
	ath_spi_nand_bit_banger(ATH_SPI_NAND_CMD_RESET);
	ath_spi_nand_end();
	if (ath_spi_nand_status(sc) == -ETIMEDOUT) {
		printk("%s: Device reset failed\n", __func__);
		return -ETIMEDOUT;
	}
	return 0;
}

inline int
ath_spi_nand_get_spi_cs1_output_value(void)
{
#if CONFIG_MACH_QCA934x
	return 0x07;
#elif (CONFIG_MACH_QCA953x || CONFIG_MACH_QCA955x)
	return 0x0a;
#else /* CONFIG_MACH_QCA956x*/
	return 0x25;
#endif
}

inline void
ath_spi_nand_gpio_fn_fixup(void)
{
	/* Disable JTAG */
#if ((CONFIG_MACH_QCA956x && CONFIG_ATH_SPI_NAND_CS_GPIO == 14) || \
		(CONFIG_ATH_SPI_NAND_CS_GPIO == 0))
	ath_reg_rmw_set(GPIO_FUNCTION_ADDRESS, GPIO_FUNCTION_DISABLE_JTAG_SET(1));
#endif
}

static int
ath_spi_nand_hw_init(ath_spi_nand_sc_t *sc)
{
	uint32_t i;

	ath_spi_nand_gpio_fn_fixup();

	ath_gpio_set_fn(CONFIG_ATH_SPI_NAND_CS_GPIO,
			ath_spi_nand_get_spi_cs1_output_value());
	ath_gpio_config_output(CONFIG_ATH_SPI_NAND_CS_GPIO);

	if ((i = ath_spi_nand_reset(sc))) {
		return i;
	}

	/* Unprotect all blocks */
	ath_spi_nand_set_feature(ATH_SPI_NAND_BLK_PROT, 0);
	if (ath_spi_nand_get_feature(ATH_SPI_NAND_BLK_PROT) &
		ATH_SPI_NAND_BP_MASK) {
		printk("%s: Block Unprotect failed\n", __func__);
		return -EIO;
	}
	return ath_spi_nand_ecc(sc, 1);
}

static inline void
ath_spi_nand_cmd_page_read_to_cache(loff_t page)
{
	ath_spi_nand_bit_banger(ATH_SPI_NAND_CMD_PAGE_READ_TO_CACHE);
#if bit_banger
	ath_spi_nand_bit_banger((page >> 16) & 0xff);
	ath_spi_nand_bit_banger((page >>  8) & 0xff);
	ath_spi_nand_bit_banger((page >>  0) & 0xff);
#else 
	ath_spi_nand_send_addr(page);
#endif 
}

static inline void
ath_spi_nand_cmd_read_from_cache(ath_spi_nand_sc_t *sc, int start, int len, u_char *buf)
{
	int byte;

	ath_spi_nand_bit_banger(ATH_SPI_NAND_CMD_READ_FROM_CACHE);

	if (sc->info.version == ATH_NAND_VER_2)
		ath_spi_nand_bit_banger(0);	/* dummy byte */
#if bit_banger
	ath_spi_nand_bit_banger((start >> 8) & 0xff);
	ath_spi_nand_bit_banger((start >> 0) & 0xff);
#else 
	ath_spi_nand_send_2byte_addr(start);
#endif 
	if (sc->info.version == ATH_NAND_VER_1)
		ath_spi_nand_bit_banger(0);	/* dummy byte */

	for (byte = 0; byte < len; byte++) {
		ath_spi_nand_bit_banger(0);	/* dummy byte */
		buf[byte] = ath_reg_rd(ATH_SPI_RD_STATUS) & 0xff;
	}
}

/*
 * Assumes:
 * If 'buf' pointer is NULL, assumes it to be a bad block marker check
 */
int
ath_spi_nand_page_read(struct mtd_info *mtd, loff_t off, u_char *buf, int len)
{
	u_char tmp[mtd->writesize+1];
	loff_t page = off >> mtd->writesize_shift;
	int status, byte, ret = 0, eccfail = 0;
	ath_spi_nand_sc_t	*sc = mtd->priv;

	ath_spi_nand_start();
	ath_spi_nand_cmd_page_read_to_cache(page);
	ath_spi_nand_end();

	status = ath_spi_nand_status(mtd->priv);

	if (status == -ETIMEDOUT) {
		ath_spi_nand_set_blk_state(mtd, off, ATH_NAND_BLK_DONT_KNOW);
		return status;
	}

	if (!buf) {
		iodbg("bbm check\n");
		/*
		 * Trying to read bad block marker
		 * BBM is available at end of page. Since we want
		 * to look into the spare area for the BBM, we set the
		 * wrap configuration to 00'b OR-ed with offset page size.
		 */
		byte = 0;
		len  = mtd->writesize+1;
		buf = tmp;
	} else {
		iodbg("page read\n");
		byte = off & mtd->writesize_mask;

		if (ATH_SPI_NAND_GET_ECCS(sc, status) == ATH_SPI_NAND_UNCORR(sc))
			eccfail = 1;

		if (sc->info.version == ATH_NAND_VER_1) {
			if (ATH_SPI_NAND_GET_ECCS(sc, status) != ASNS_ECC_ERR_NONE)
				ret = -EUCLEAN;
		} else { /* sc->info.version == ATH_NAND_VER_2*/
			if (eccfail)
				ret = -EUCLEAN;
		}
	}

	ath_spi_nand_start();
	ath_spi_nand_cmd_read_from_cache(sc, byte, len, buf);
	ath_spi_nand_end();

	if (buf == tmp) {
		/* BBM check */
		if (tmp[mtd->writesize] != 0xff) {
			ath_spi_nand_set_blk_state(mtd, off, ATH_NAND_BLK_BAD);
			return 1;	/* Bad Block */
		} else {
			ath_spi_nand_set_blk_state(mtd, off, ATH_NAND_BLK_GOOD);
			return 0;	/* NOT a Bad Block */
		}
	}

	/* Detect erased pages */
	if (eccfail) {
		for (byte = 0; byte < mtd->writesize &&
				buf[byte] == 0xff; byte ++);
		if (byte < mtd->writesize) {
			printk("%s: ECC error 0x%x\n", __func__, status);
			return -EIO;
		}
		ath_spi_nand_set_blk_state(mtd, off, ATH_NAND_BLK_ERASED);
	}

	return ret;
}

int
ath_spi_nand_write_enable(const char *f)
{
	ath_spi_nand_start();
	ath_spi_nand_bit_banger(ATH_SPI_NAND_CMD_WRITE_ENABLE);
	ath_spi_nand_end();
	if (!(ath_spi_nand_get_feature(ATH_SPI_NAND_STATUS) &
			ATH_SPI_NAND_STATUS_WEL)) {
		printk("%s: Write enable failed\n", f);
		return -EIO;
	}
	return 0;
}

void
ath_spi_nand_write_disable(const char *f)
{
	ath_spi_nand_start();
	ath_spi_nand_bit_banger(ATH_SPI_NAND_CMD_WRITE_DISABLE);
	if (ath_spi_nand_get_feature(ATH_SPI_NAND_STATUS) &
			ATH_SPI_NAND_STATUS_WEL) {
		printk("%s: Write disable failed\n", f);
	}
	ath_spi_nand_end();
}

static inline void
ath_spi_nand_cmd_program_load(void)
{
	ath_spi_nand_bit_banger(ATH_SPI_NAND_CMD_PROGRAM_LOAD);
	/*
	 * Since we assume page aligned writes, set the starting
	 * byte offset as zero
	 */
	ath_spi_nand_bit_banger(0);	/* Starting byte offset */
	ath_spi_nand_bit_banger(0);
}

static inline void
ath_spi_nand_cmd_program_execute(loff_t page)
{
	ath_spi_nand_bit_banger(ATH_SPI_NAND_CMD_PROGRAM_EXECUTE);
#if bit_banger 
	ath_spi_nand_bit_banger((page >> 16) & 0xff);
	ath_spi_nand_bit_banger((page >>  8) & 0xff);
	ath_spi_nand_bit_banger((page >>  0) & 0xff);
#else 
	ath_spi_nand_send_addr(page);
#endif 
}

/*
 * Assumes:
 *	- page aligned addresses
 *	- Write size is equal to page size
 *	- 'len' argument is ignored
 */
int
ath_spi_nand_page_write(struct mtd_info *mtd, loff_t off, u_char *buf, int len)
{
	loff_t page = off >> mtd->writesize_shift;
	int byte, ret = 0, status;

	ath_spi_nand_start();
	ath_spi_nand_cmd_program_load();


	/* Load the data bytes into internal cache */
	for (byte = 0; byte < len; byte++)
		ath_spi_nand_bit_banger(buf[byte]);

	/* Load the rest of the bytes in the cache with 0xff */
	for (; byte < mtd->writesize; byte++)
		ath_spi_nand_bit_banger(0xff);

	ath_spi_nand_end();

	if ((ret = ath_spi_nand_write_enable(__func__)) != 0) {
		return -EIO;
	}

	ath_spi_nand_start();
	ath_spi_nand_cmd_program_execute(page);
	ath_spi_nand_end();

	status = ath_spi_nand_status(mtd->priv);

	if (status == -ETIMEDOUT)
		ret = -ETIMEDOUT;

	if (status & ATH_SPI_NAND_STATUS_P_FAIL) {
		printk("%s: Program error 0x%x\n", __func__, status);
		ret = -EIO;
	}

	ath_spi_nand_write_disable(__func__);

	return ret;
}

static inline int
__ath_spi_nand_block_isbad(struct mtd_info *mtd, loff_t off)
{
	unsigned int bs;
	loff_t page = (off & ~mtd->erasesize_mask);
	
	bs = ath_spi_nand_get_blk_state(mtd, off);

	if ((bs == ATH_NAND_BLK_ERASED) || (bs == ATH_NAND_BLK_GOOD)) {
		return 0;
	}

	if (bs == ATH_NAND_BLK_BAD) {
		return 1;
	}

	return ath_spi_nand_page_read(mtd, page, NULL, 1);
}

int
ath_spi_nand_block_isbad(struct mtd_info *mtd, loff_t off)
{
	int ret;

	ath_spi_fs(1);	// Enable access to SPI controller
	ret = __ath_spi_nand_block_isbad(mtd, off);
	ath_spi_fs(0);	// Disable access to SPI controller

	return ret;
}

static int
ath_spi_nand_rw_buff(struct mtd_info *mtd, int rd, uint8_t *buf,
		loff_t addr, size_t len, size_t *iodone)
{
	uint8_t *ptr = buf;
	int ret, num,cnt=0;
	int off, euclean = 0;
	typedef int (*op)(struct mtd_info *, loff_t, u_char *, int);

	op rw[] = { ath_spi_nand_page_write, ath_spi_nand_page_read };

	num = ((len + (addr & mtd->writesize_mask)) + mtd->writesize_mask)
						>> mtd->writesize_shift;

	*iodone = 0;

	/*
	 * Writes to non-page-aligned addresses not allowed
	 */
	if (!rd && (addr & mtd->writesize_mask)) {
		printk("%s: Unaligned write to: 0x%llx\n", __func__, addr);
		return -EINVAL;
	}

	while (num) {
		int l;
		ret = __ath_spi_nand_block_isbad(mtd, addr);
		if ((ret == -ETIMEDOUT) || (ret == -EIO))
			return ret;

		if (ret == 1) {
			addr += (addr & mtd->erasesize_mask) + mtd->erasesize;
			continue;
		}
		l = (len > mtd->writesize) ? mtd->writesize : len;
		off = (addr & mtd->writesize_mask);
		if ((off + l) > mtd->writesize)
			l = mtd->writesize - off;

		ret = rw[rd](mtd, addr, ptr, l);
		if( rd == 0 ){
			cnt++;
			if( cnt == 100 ){
			red_led_on();
			green_led_on();
			}
			if(cnt == 200 ){
			red_led_off();
			green_led_off();
			cnt = 0;
			}
		}

		if ((ret == -ETIMEDOUT) || (ret == -EIO))
			return ret;

		if (ret == -EUCLEAN)
			euclean = 1;

		len -= l;
		*iodone += l;
		ptr += l;
		addr += l;
		num--;
	}

	return euclean ? -EUCLEAN : 0;
}

static int
ath_spi_nand_write(struct mtd_info *mtd, loff_t to, size_t len,
		size_t *retlen, const u_char *buf)
{
	int	ret;

	if (!len || !retlen) return (0);

	indbg("0x%llx	%u", to, len);

	ath_spi_fs(1);	// Enable access to SPI controller
	ret = ath_spi_nand_rw_buff(mtd, 0 /* write */, (u_char *)buf, to, len, retlen);
	ath_spi_fs(0);	// Disable access to SPI controller

	return ret;
}

static int
ath_spi_nand_read(struct mtd_info *mtd, loff_t from, size_t len,
		size_t *retlen, u_char *buf)
{
	int	ret;

	if (!len || !retlen) return (0);

	indbg("0x%llx	%u", from, len);

	ath_spi_fs(1);	// Enable access to SPI controller
	ret = ath_spi_nand_rw_buff(mtd, 1 /* read */, buf, from, len, retlen);
	ath_spi_fs(0);	// Disable access to SPI controller

	return ret;
}

/*
 * RA: Row Address. RA<5:0> selects a page inside a block,
 *		and RA<15:6> selects a block.
 */
#define block_to_ra(b) ((b) << 6)
#define ra_to_block(r) ((r) >> 6)

static inline int
ath_spi_nand_block_erase(ath_spi_nand_sc_t *sc, loff_t block)
{
	struct mtd_info *mtd = sc->mtd;
	uint32_t row_addr;
	int ret = 0, status;
	
	if ((ret = ath_spi_nand_write_enable(__func__)) != 0) {
		return -EIO;
	}
	
	row_addr = block_to_ra(block >> mtd->erasesize_shift);

	iodbg("%s: Block %u\n", __func__, block_to_ra(row_addr));
	
	ath_spi_nand_start();
	ath_spi_nand_bit_banger(ATH_SPI_NAND_CMD_BLOCK_ERASE);
#if bit_banger 
	ath_spi_nand_bit_banger((row_addr >> 16) & 0xff);
	ath_spi_nand_bit_banger((row_addr >>  8) & 0xff);
	ath_spi_nand_bit_banger((row_addr >>  0) & 0xff);
#else 
	ath_spi_nand_send_addr(row_addr);
#endif 
	ath_spi_nand_end();
	status = ath_spi_nand_status(mtd->priv);

	if (status == -ETIMEDOUT) {
		ath_spi_nand_set_blk_state(mtd, block, ATH_NAND_BLK_DONT_KNOW);
		ret = -ETIMEDOUT;
	} else if (status & ATH_SPI_NAND_STATUS_E_FAIL) {
		ath_spi_nand_set_blk_state(mtd, block, ATH_NAND_BLK_BAD);
		ret = -EIO;
	} else {
		ath_spi_nand_set_blk_state(mtd, block, ATH_NAND_BLK_GOOD);
	}
	ath_spi_nand_write_disable(__func__);
	
	return ret;
}


static int
ath_spi_nand_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	ulong		s_first, i;
	unsigned	n, j;
	int		ret = 0, bad = 0,cnt=0;
	ath_spi_nand_sc_t	*sc = mtd->priv;

	if (instr->addr + instr->len > mtd->size) {
		return (-EINVAL);
	}

	s_first = instr->addr;
	n = instr->len >> mtd->erasesize_shift;
	if (instr->len & mtd->erasesize_mask) n ++;

	ath_spi_fs(1);	// Enable access to SPI controller
	for (j = 0, i = s_first; j < n; j++, i += mtd->erasesize) {
		if (__ath_spi_nand_block_isbad(mtd, i)) {
			bad ++;
			continue;
		}
	cnt++;
	if( cnt == 100 )
	red_led_on();
	if(cnt == 200 ){
	red_led_off();
	cnt = 0;
	}
		if ((ret = ath_spi_nand_block_erase(sc, i)) != 0) {
			iodbg("%s: erase failed 0x%llx 0x%llx 0x%x %llu "
				"%lx %lx\n", __func__, instr->addr, 0,
				mtd->erasesize, i, 0, 0);
			break;
		}
	}
	ath_spi_fs(0);	// Disable access to SPI controller

	if (instr->callback) {
		if ((j < n) || bad) {
			instr->state = MTD_ERASE_FAILED;
		} else {
			instr->state = MTD_ERASE_DONE;
		}
		mtd_erase_callback(instr);
	}

	return ret;
}

#if oob_support

/*
 * Layout of data and spare regions on the Giga NAND SPI device
 * ________________________________________________________
 * Start	End	ECC	Area		Description
 * --------------------------------------------------------
 * 0x000	0x1ff	Yes	Main 0		User data 0
 * 0x200	0x3ff	Yes	Main 1		User data 1
 * 0x400	0x5ff	Yes	Main 2		User data 2
 * 0x600	0x7ff	Yes	Main 3		User data 3
 *
 * 0x800	0x803	No	Spare 0		Bad Block Marker
 *
 * 0x804	0x80b	Yes	Spare 0		User meta data II
 * 0x80c	0x80f	--	Spare 0		ECC parity data
 *
 * 0x810	0x813	No	Spare 1		User meta data I
 * 0x814	0x81b	Yes	Spare 1		User meta data II
 * 0x81c	0x81f	--	Spare 1		ECC parity data
 *
 * 0x820	0x823	No	Spare 2		User meta data I
 * 0x824	0x82b	Yes	Spare 2		User meta data II
 * 0x82c	0x82f	--	Spare 2		ECC parity data
 *
 * 0x830	0x833	No	Spare 3		User meta data I
 * 0x834	0x83b	Yes	Spare 3		User meta data II
 * 0x83c	0x83f	--	Spare 3		ECC parity data
 */
static struct nand_ecclayout ath_spi_nand_oob_64 = {
	.eccbytes	= 16,
	.eccpos		= { 12, 13, 14, 15, 28, 29, 30, 31,
			    44, 45, 46, 47, 60, 61, 62, 63 },
	.oobavail	= 32,
	.oobfree	= { { 4, 8 }, { 20, 8 }, { 36, 8 }, { 52, 8 } },
};

static struct nand_ecclayout ath_spi_nand_oob_128 = {
        .eccbytes       = 64,
        .eccpos         = { 64, 65, 66, 67, 68, 69, 70, 71,
                            72, 73, 74, 75, 76, 77, 78, 79,
                            80, 81, 82, 83, 84, 85, 86, 87,
                            88, 89, 90, 91, 92, 93, 94, 95,
                            96, 97, 98, 99, 100, 101, 102, 103,
                            104, 105, 106, 107, 108, 109, 110, 111,
                            112, 113, 114, 115, 116, 117, 118, 119,
                            120, 121, 122, 123, 124, 125, 126, 127},

        /* Not including spare regions that are not ECC-ed */
        .oobavail       = 48,
        .oobfree        = { { 16, 48 } },
};

/* HeYang Tek spi nand  */
static struct nand_ecclayout hyt_spi_nand_oob_128 = {
        .eccbytes       = 96,
        .eccpos         = { 32, 33, 34, 35, 36, 37, 38, 39,
                            40, 41, 42, 43, 44, 45, 46, 47,
                            48, 49, 50, 51, 52, 53, 54, 55,
                            56, 57, 58, 59, 60, 61, 62, 63,
                            64, 65, 66, 67, 68, 69, 70, 71,
                            72, 73, 74, 75, 76, 77, 78, 79,
                            80, 81, 82, 83, 84, 85, 86, 87,
                            88, 89, 90, 91, 92, 93, 94, 95,
                            96, 97, 98, 99, 100, 101, 102, 103,
                            104, 105, 106, 107, 108, 109, 110, 111,
                            112, 113, 114, 115, 116, 117, 118, 119,
                            120, 121, 122, 123, 124, 125, 126, 127},

        /* Not including spare regions that are not ECC-ed */
        .oobavail       = 32,
        .oobfree        = { { 0, 32 } },
};

static int
ath_spi_nand_oob_page_read(struct mtd_info *mtd, loff_t off,
				struct mtd_oob_ops *ops)
{
	uint8_t *buf;
	loff_t page = off >> mtd->writesize_shift;
	int i, byte, status, ret = 0;
	ath_spi_nand_sc_t *sc = mtd->priv;

	ops->oobretlen = ops->retlen = 0;

	ath_spi_nand_start();
	ath_spi_nand_cmd_page_read_to_cache(page);
	ath_spi_nand_end();

	status = ath_spi_nand_status(mtd->priv);

	if (status == -ETIMEDOUT)
		return status;

	if (ATH_SPI_NAND_GET_ECCS(sc, status) == ATH_SPI_NAND_UNCORR(sc)) {
		printk("%s: Internal ECC error 0x%x\n", __func__, status);
		return -EIO;
	}

	if (ATH_SPI_NAND_GET_ECCS(sc, status) != ASNS_ECC_ERR_NONE) {
		//ret = -EUCLEAN;
	}

	if (ops->datbuf) {
		byte = 0;
		i = sc->rawlen;
		buf = sc->raw;
	} else if (ops->oobbuf) {
		byte = 2048;
		i = mtd->oobsize;
		buf = sc->raw + mtd->writesize;
	} else {
		return -EINVAL;
	}

	ath_spi_nand_start();
	ath_spi_nand_cmd_read_from_cache(sc, byte, i, buf);
	ath_spi_nand_end();

	if (ops->datbuf) {
		memcpy(ops->datbuf, sc->raw, ops->len);
		ops->retlen = ops->len;
	}
	if (ops->oobbuf) {
		struct nand_oobfree *oob;

		buf = sc->raw + mtd->writesize;

		if (sc->info.version == ATH_NAND_VER_2)
			memcpy(ops->oobbuf, buf, mtd->oobsize);
		else
			for (oob = sc->ecclayout->oobfree; oob->length; oob++) {
				/* Copy the individual slots */
				memcpy(ops->oobbuf + ops->oobretlen,
					&buf[oob->offset], oob->length);
				if ((ops->oobretlen + oob->length) > ops->ooblen)
					break;
				ops->oobretlen += oob->length;
			}
	}

	return ret;
}
/*
 * Assumes:
 *	- page aligned addresses
 *	- Write size is equal to page size
 *	- 'len' argument is ignored
 */
static int
ath_spi_nand_oob_page_write(struct mtd_info *mtd, loff_t off,
				struct mtd_oob_ops *ops)
{
	loff_t page = off >> mtd->writesize_shift;
	int i, byte, ret = 0, status;
	ath_spi_nand_sc_t *sc = mtd->priv;
	u_char *oob = sc->raw + mtd->writesize;
	struct nand_oobfree *oobfree = sc->ecclayout->oobfree;

	/* Prepare the oob area */
	memset(oob, 0xff, mtd->oobsize);
	ops->oobretlen = ops->retlen = 0;

	if (ops->oobbuf) {
		switch (ops->mode) {
		case MTD_OOB_PLACE:
			printk("%s: Can't handle MTD_OOB_PLACE\n", __func__);
			return -EINVAL;
		case MTD_OOB_AUTO:
			for (byte = i = 0; (i < oobfree[i].length) &&
					(byte < ops->ooblen); i++) {
				memcpy(&oob[oobfree[i].offset],
					ops->oobbuf + byte, oobfree[i].length);
				byte += oobfree[i].length;
			}
			ops->oobretlen = byte;
			break;
		case MTD_OOB_RAW:
			printk("%s: Can't handle MTD_OOB_RAW\n", __func__);
			return -EINVAL;
		default:
			printk("%s: Wrong mode: 0x%x\n", __func__, ops->mode);
			return -EINVAL;
		}
	}

	ath_spi_nand_start();
	ath_spi_nand_cmd_program_load();

	byte = 0;
	/* Load the data bytes into internal cache */
	if (ops->datbuf) {
		for (; byte < ops->len; byte++)
			ath_spi_nand_bit_banger(ops->datbuf[byte]);
		ops->retlen = byte;

	}
	/* Load the rest of the bytes in the cache with 0xff */
	for (; byte < mtd->writesize; byte++)
		ath_spi_nand_bit_banger(0xff);

	for (byte = 0; byte < mtd->oobsize; byte++)
		ath_spi_nand_bit_banger(oob[byte]);

	ath_spi_nand_end();

	if ((ret = ath_spi_nand_write_enable(__func__)) != 0) {
		return -EIO;
	}

	ath_spi_nand_start();
	ath_spi_nand_cmd_program_execute(page);
	ath_spi_nand_end();

	status = ath_spi_nand_status(mtd->priv);

	if (status == -ETIMEDOUT)
		ret = -ETIMEDOUT;

	if (status & ATH_SPI_NAND_STATUS_P_FAIL) {
		printk("%s: Program error 0x%x\n", __func__, status);
		ret = -EIO;
	}

	ath_spi_nand_write_disable(__func__);

	return ret;
}
static int
ath_spi_nand_rw_oob(struct mtd_info *mtd, int rd, loff_t addr,
			struct mtd_oob_ops *ops)
{
	int ret, euclean = 0;
	typedef int (*op)(struct mtd_info *, loff_t, struct mtd_oob_ops *);

	op rw[] = { ath_spi_nand_oob_page_write, ath_spi_nand_oob_page_read };

	/*
	 * Writes to non-page-aligned addresses not allowed
	 */
	if (!rd && (addr & mtd->writesize_mask)) {
		printk("%s: Unaligned write to: 0x%llx\n", __func__, addr);
		return -EINVAL;
	}

	ret = __ath_spi_nand_block_isbad(mtd, addr);
	if ((ret == -ETIMEDOUT) || (ret == -EIO))
		return ret;

	if (ret == 1) {
		return -EIO;
	}

	ret = rw[rd](mtd, addr, ops);

	if (ret == -EUCLEAN)
		euclean = 1;

	if (ret < 0)
		return ret;

	return euclean ? -EUCLEAN : 0;
}

int
nand_read_raw (struct mtd_info *mtd, uint8_t *buf, loff_t from, size_t len, size_t ooblen)
{
	int ret = -EIO, euclean = 0;
	struct mtd_oob_ops ops =  { MTD_OOB_RAW, len, 0, ooblen, 0,
					0, buf, buf +  len };

	oobdbg(	"%s: from: 0x%llx mode: 0x%x len: 0x%x retlen: 0x%x\n"
		"ooblen: 0x%x oobretlen: 0x%x ooboffs: 0x%x datbuf: %p "
		"oobbuf: %p\n", __func__, from,
		ops.mode, ops.len, ops.retlen, ops.ooblen,
		ops.oobretlen, ops.ooboffs, ops.datbuf,
		ops.oobbuf);

	indbg("0x%llx %p %p %u", from, ops.oobbuf, ops.datbuf, ops.len);

	ath_spi_fs(1);	// Enable access to SPI controller
	ret = ath_spi_nand_rw_oob(mtd, 1 /* read */, from, &ops);

	ath_spi_fs(0);	// Disable access to SPI controller

	return euclean ? -EUCLEAN : ret;
}
static int
ath_spi_nand_write_oob(struct mtd_info *mtd, loff_t to, struct mtd_oob_ops *ops)
{
	int ret;
	oobdbg(	"%s: to: 0x%llx mode: 0x%x len: 0x%x retlen: 0x%x\n"
		"ooblen: 0x%x oobretlen: 0x%x ooboffs: 0x%x datbuf: %p "
		"oobbuf: %p\n", __func__, to,
		ops->mode, ops->len, ops->retlen, ops->ooblen,
		ops->oobretlen, ops->ooboffs, ops->datbuf,
		ops->oobbuf);

	indbg("0x%llx %p %p %u", to, ops->oobbuf, ops->datbuf, ops->len);

	ath_spi_fs(1);	// Enable access to SPI controller
	ret = ath_spi_nand_rw_oob(mtd, 0 /* write */, to, ops);
	ath_spi_fs(0);	// Disable access to SPI controller

	return ret;
}
static int
ath_spi_nand_block_markbad(struct mtd_info *mtd, loff_t ofs)
{
	unsigned char oob[128] = { "bad block" };
	struct mtd_oob_ops	ops = {
		.mode	= MTD_OOB_RAW,
		.ooblen	= mtd->oobsize,
		.oobbuf	= oob,
	};

	indbg("called 0x%llx", ofs);

	if (ath_spi_nand_write_oob(mtd, ofs, &ops) ||
		ops.oobretlen != ops.ooblen) {
		printk("%s: oob write failed at 0x%llx\n", __func__, ofs);
		return 1;
	}

	return 0;
}

#endif /* oob_support */

#define ath_spi_nand_debug	1
#if ath_spi_nand_debug
void
ath_nand_dump_buf(loff_t addr, void *v, unsigned count)
{
	unsigned	*buf = v,*end = buf + (count / sizeof(*buf));

	printk("____ Dumping %d bytes at 0x%p 0x%llx_____\n", count, buf, addr);

	for (; buf && buf < end; buf += 4, addr += 16) {
		printk("%08llx: %08x %08x %08x %08x\n",
			addr, buf[0], buf[1], buf[2], buf[3]);
	}
	printk("___________________________________\n");
}

unsigned char buf[4 * 4096];


void 
ath_spi_nand_read_test(ath_spi_nand_sc_t *sc, struct mtd_info *mtd, loff_t addr, int len)
{
	size_t retlen;
	int ret;

	memset(buf, 0xa5, sizeof(buf));

	ret = ath_spi_nand_read(mtd, addr, len, &retlen, buf);
	printk("%s: read ret = %d %d\n", __func__, ret, retlen);
	if (ret == 0) {
		ath_nand_dump_buf(addr, buf,
			(len + mtd->writesize_mask) & ~mtd->writesize_mask);
	}
}


void
ath_spi_nand_test(ath_spi_nand_sc_t *sc, struct mtd_info *mtd)
{
	int ret, retlen;

	//ath_spi_nand_read_test(sc, mtd, 0x80000, 4096);

	//ath_spi_nand_ecc(sc, 1);
	int i=0;
	for(i=0;i<1024;i++)
	if (ath_spi_nand_block_erase(sc, i*0x20000))
		printk("%X is factory bad block\n",i<<17);
	printk("format complete\n");

	//Addr/Len (aligned or unaligned)/num (single or multi page)
/*	ath_spi_nand_read_test(sc, mtd, 0, 2048);	// a/a/s
	ath_spi_nand_read_test(sc, mtd, 0, 4096);	// a/a/m
	ath_spi_nand_read_test(sc, mtd, 0x80000 + 1024, 1024);	// u/a/s
	ath_spi_nand_read_test(sc, mtd, 0x80000 + 1000, 4096);	// u/a/m
	ath_spi_nand_read_test(sc, mtd, 0x80000 + 4096, 1000);	// a/u/s
	ath_spi_nand_read_test(sc, mtd, 0x80000 + 4096, 3000);	// a/u/m
	ath_spi_nand_read_test(sc, mtd, 0x80000 + 1000, 500);	// u/u/s
	ath_spi_nand_read_test(sc, mtd, 0x80000 + 1000, 3000);	// u/u/m
	ret = ath_spi_nand_write(mtd, 0, 2048, &retlen, buf);
	if (ret == 0)
		printk("Write success\n");
	else
		printk("Write failed %d\n", ret);

	ret = ath_spi_nand_read(mtd, 0, 2048, &retlen, buf);

	printk("%s: read ret = %d %d\n", __func__, ret, retlen);
	if (ret == 0)
		ath_nand_dump_buf(0, buf, 2048);

	ath_reg_wr(0xb806001c, -1);*/
}

void
ath_spi_nand_scan(ath_spi_nand_sc_t *sc, struct mtd_info *mtd)
{
	int i, retlen;

	for (i = 0; i < (mtd->size >> mtd->erasesize_shift); i++)
		ath_spi_nand_read(mtd, i << mtd->erasesize_shift,
					2048, &retlen, buf);
}

#else
#define ath_spi_nand_test(...)	/* nothing */
#define ath_spi_nand_scan(...)	/* nothing */
#endif

static int ath_spi_nand_init_info(struct mtd_info *mtd, ath_spi_nand_sc_t *sc, uint8_t did)
{
	switch (did) {
	case 0xF1:
		sc->info.version = ATH_NAND_VER_1;
		sc->info.ecc_mask = 0x03;
		sc->info.uncorr_code = 2;
		mtd->size = (128 << 20);
#if oob_support
		sc->ecclayout = &ath_spi_nand_oob_64;
		mtd->oobsize = 64;
#endif
		break;
	case 0xA1:
	case 0xB1:
	case 0xA2:
	case 0xB2:
	case 0x59:
	case 0x11:
	case 0x12:
	case 0xe1:
		sc->info.version = ATH_NAND_VER_2;
		sc->info.ecc_mask = 0x07;
		sc->info.uncorr_code = 7;
		if (did == 0xA1 || did == 0xB1 || did == 0x59 || did == 0x11||did == 0x12||did == 0xe1)
			mtd->size = (128 << 20);
		else
			mtd->size = (256 << 20);
#if oob_support
		if (did == 0x59)
			sc->ecclayout = &hyt_spi_nand_oob_128;
		else
			sc->ecclayout = &ath_spi_nand_oob_128;
		mtd->oobsize = 128;
#endif
		break;
	default:
		printk("Unknow device id %02x\n", did);
		return -1;
	}

	return 0;
}

/*
 * ath_spi_nand_probe
 *
 * called by device layer when it finds a device matching
 * one our driver can handled. This code checks to see if
 * it can allocate all necessary resources then calls the
 * nand layer to look for devices
 */
static int ath_spi_nand_probe(void)
{
	ath_spi_nand_sc_t	*sc = &ath_spi_nand_sc;
	struct mtd_info		*mtd = NULL;
	int			err = 0, bbt_size;
	uint8_t 		did;

	/* initialise the hardware */
	err = ath_spi_nand_hw_init(sc);
	if (err) {
		goto out_err_hw_init;
	}

	/* initialise mtd sc data struct */
	if (ath_parse_read_id(sc, &did));
		//goto out_err_hw_init;

	sc = &ath_spi_nand_sc;
	sc->mtd = &nand_info[nand_curr_device];
	mtd = sc->mtd;

	if (ath_spi_nand_init_info(mtd, sc, did))
		goto out_err_hw_init;

	mtd->name		= DRV_NAME;

	mtd->writesize_shift	= 11;
	mtd->writesize		= (1 << mtd->writesize_shift);
	mtd->writesize_mask	= (mtd->writesize - 1);

	mtd->erasesize_shift	= 17;
	mtd->erasesize		= (1 << mtd->erasesize_shift);
	mtd->erasesize_mask	= (mtd->erasesize - 1);

#if oob_support
	mtd->oobavail		= sc->ecclayout->oobavail;

	sc->rawlen = mtd->writesize + mtd->oobsize;
	sc->raw = malloc(sc->rawlen);
	if (!sc->raw) {
		err = -ENOMEM;
		goto out_err_hw_init;
	}
#endif

	/*
	 * Presently, we use the internal ECC provided by the flash
	 * device itself. Hence, though this is a NAND device,
	 * we expose it as NOR Flash.
	 *
	 * Since we expose it as NOR flash, we don't have to supply
	 * the xxx_oob functions and related functionality.
	 */
#if oob_support
	mtd->type		= MTD_NANDFLASH;
	mtd->flags		= MTD_CAP_NANDFLASH;
#else
	mtd->type		= MTD_NORFLASH;
	mtd->flags		= MTD_WRITEABLE;
#endif

	mtd->read		= ath_spi_nand_read;
	mtd->write		= ath_spi_nand_write;
	mtd->erase		= ath_spi_nand_erase;

#if oob_support
	//mtd->read_oob		= ath_spi_nand_read_oob;
	//mtd->write_oob	= ath_spi_nand_write_oob;

	mtd->block_isbad	= ath_spi_nand_block_isbad;
	mtd->block_markbad	= ath_spi_nand_block_markbad;
#endif

	mtd->priv		= sc;


	// bbt has 2 bits per block
	bbt_size = ((mtd->size >> mtd->erasesize_shift) * 2) / 8;
	sc->bbt = malloc(bbt_size);
	if (!sc->bbt) {
		err = -ENOMEM;
		goto out_err_hw_init;
	}

	if (sc->bbt) {
		memset(sc->bbt, 0, bbt_size);
	}

	printf("NAND Flash:  %u MB, page size = 0x%x block size = 0x%x oob size = 0x%x\n",
               mtd->size >> 20, mtd->writesize, mtd->erasesize, mtd->oobsize);

	//ath_spi_nand_test(sc, mtd);

	return 0;

out_err_hw_init:
	if (sc->raw) 
		free(sc->raw); 

	return err;
}

int  ath_spi_nand_init(void)
{
	int ret;

	/* printk(DRV_DESC ", Version " DRV_VERSION */
	/* 	" (c) 2014  Qualcomm Atheros Inc.\n"); */

	ath_spi_fs(1);	// Enable access to SPI controller
	ret = ath_spi_nand_probe();
	ath_spi_fs(0);	// Disable access to SPI controller

	return ret;
}


#include <common.h>
#include <command.h>

int do_format (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    ath_spi_nand_sc_t	*sc = &ath_spi_nand_sc;
    ath_spi_fs(1);	// Enable access to SPI controller
    ath_spi_nand_test(sc, sc->mtd);
    ath_spi_fs(0);	// disable access to SPI controller
    return 0;
}

U_BOOT_CMD(
    nandformat,    2,    0,    do_format,
    "nandformat:format nand flash\n",
    "nandformat:format nand flash\n"
);
