/*
 * drivers/mtd/nand/omap2.c
 *
 * Copyright (c) 2004 Texas Instruments, Jian Zhang <jzhang@ti.com>
 * Copyright (c) 2004 Micron Technology Inc.
 * Copyright (c) 2004 David Brownell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/io.h>

#include <asm/dma.h>

#include <mach/gpmc.h>
#include <mach/nand.h>

#define GPMC_IRQ_STATUS		0x18
#define GPMC_ECC_CONFIG		0x1F4
#define GPMC_ECC_CONTROL	0x1F8
#define GPMC_ECC_SIZE_CONFIG	0x1FC
#define GPMC_ECC1_RESULT	0x200

#define	DRIVER_NAME	"omap2-nand"
#define	NAND_IO_SIZE	SZ_4K

#define	NAND_WP_ON	1
#define	NAND_WP_OFF	0
#define NAND_WP_BIT	0x00000010
#define WR_RD_PIN_MONITORING	0x00600000

#define	GPMC_BUF_FULL	0x00000001
#define	GPMC_BUF_EMPTY	0x00000000

#define NAND_Ecc_P1e		(1 << 0)
#define NAND_Ecc_P2e		(1 << 1)
#define NAND_Ecc_P4e		(1 << 2)
#define NAND_Ecc_P8e		(1 << 3)
#define NAND_Ecc_P16e		(1 << 4)
#define NAND_Ecc_P32e		(1 << 5)
#define NAND_Ecc_P64e		(1 << 6)
#define NAND_Ecc_P128e		(1 << 7)
#define NAND_Ecc_P256e		(1 << 8)
#define NAND_Ecc_P512e		(1 << 9)
#define NAND_Ecc_P1024e		(1 << 10)
#define NAND_Ecc_P2048e		(1 << 11)

#define NAND_Ecc_P1o		(1 << 16)
#define NAND_Ecc_P2o		(1 << 17)
#define NAND_Ecc_P4o		(1 << 18)
#define NAND_Ecc_P8o		(1 << 19)
#define NAND_Ecc_P16o		(1 << 20)
#define NAND_Ecc_P32o		(1 << 21)
#define NAND_Ecc_P64o		(1 << 22)
#define NAND_Ecc_P128o		(1 << 23)
#define NAND_Ecc_P256o		(1 << 24)
#define NAND_Ecc_P512o		(1 << 25)
#define NAND_Ecc_P1024o		(1 << 26)
#define NAND_Ecc_P2048o		(1 << 27)

#define TF(value)	(value ? 1 : 0)

#define P2048e(a)	(TF(a & NAND_Ecc_P2048e)	<< 0)
#define P2048o(a)	(TF(a & NAND_Ecc_P2048o)	<< 1)
#define P1e(a)		(TF(a & NAND_Ecc_P1e)		<< 2)
#define P1o(a)		(TF(a & NAND_Ecc_P1o)		<< 3)
#define P2e(a)		(TF(a & NAND_Ecc_P2e)		<< 4)
#define P2o(a)		(TF(a & NAND_Ecc_P2o)		<< 5)
#define P4e(a)		(TF(a & NAND_Ecc_P4e)		<< 6)
#define P4o(a)		(TF(a & NAND_Ecc_P4o)		<< 7)

#define P8e(a)		(TF(a & NAND_Ecc_P8e)		<< 0)
#define P8o(a)		(TF(a & NAND_Ecc_P8o)		<< 1)
#define P16e(a)		(TF(a & NAND_Ecc_P16e)		<< 2)
#define P16o(a)		(TF(a & NAND_Ecc_P16o)		<< 3)
#define P32e(a)		(TF(a & NAND_Ecc_P32e)		<< 4)
#define P32o(a)		(TF(a & NAND_Ecc_P32o)		<< 5)
#define P64e(a)		(TF(a & NAND_Ecc_P64e)		<< 6)
#define P64o(a)		(TF(a & NAND_Ecc_P64o)		<< 7)

#define P128e(a)	(TF(a & NAND_Ecc_P128e)		<< 0)
#define P128o(a)	(TF(a & NAND_Ecc_P128o)		<< 1)
#define P256e(a)	(TF(a & NAND_Ecc_P256e)		<< 2)
#define P256o(a)	(TF(a & NAND_Ecc_P256o)		<< 3)
#define P512e(a)	(TF(a & NAND_Ecc_P512e)		<< 4)
#define P512o(a)	(TF(a & NAND_Ecc_P512o)		<< 5)
#define P1024e(a)	(TF(a & NAND_Ecc_P1024e)	<< 6)
#define P1024o(a)	(TF(a & NAND_Ecc_P1024o)	<< 7)

#define P8e_s(a)	(TF(a & NAND_Ecc_P8e)		<< 0)
#define P8o_s(a)	(TF(a & NAND_Ecc_P8o)		<< 1)
#define P16e_s(a)	(TF(a & NAND_Ecc_P16e)		<< 2)
#define P16o_s(a)	(TF(a & NAND_Ecc_P16o)		<< 3)
#define P1e_s(a)	(TF(a & NAND_Ecc_P1e)		<< 4)
#define P1o_s(a)	(TF(a & NAND_Ecc_P1o)		<< 5)
#define P2e_s(a)	(TF(a & NAND_Ecc_P2e)		<< 6)
#define P2o_s(a)	(TF(a & NAND_Ecc_P2o)		<< 7)

#define P4e_s(a)	(TF(a & NAND_Ecc_P4e)		<< 0)
#define P4o_s(a)	(TF(a & NAND_Ecc_P4o)		<< 1)

#ifdef CONFIG_MTD_PARTITIONS
static const char *part_probes[] = { "cmdlinepart", NULL };
#endif

struct omap_nand_info {
	struct nand_hw_control		controller;
	struct omap_nand_platform_data	*pdata;
	struct mtd_info			mtd;
	struct mtd_partition		*parts;
	struct nand_chip		nand;
	struct platform_device		*pdev;

	int				gpmc_cs;
	unsigned long			phys_base;
	void __iomem			*gpmc_cs_baseaddr;
	void __iomem			*gpmc_baseaddr;
};

/*
 * omap_nand_wp - This function enable or disable the Write Protect feature on
 * NAND device
 * @mtd: MTD device structure
 * @mode: WP ON/OFF
 */
static void omap_nand_wp(struct mtd_info *mtd, int mode)
{
	struct omap_nand_info *info = container_of(mtd,
						struct omap_nand_info, mtd);

	unsigned long config = __raw_readl(info->gpmc_baseaddr + GPMC_CONFIG);

	if (mode)
		config &= ~(NAND_WP_BIT);	/* WP is ON */
	else
		config |= (NAND_WP_BIT);	/* WP is OFF */

	__raw_writel(config, (info->gpmc_baseaddr + GPMC_CONFIG));
}

/*
 * hardware specific access to control-lines
 * NOTE: boards may use different bits for these!!
 *
 * ctrl:
 * NAND_NCE: bit 0 - don't care
 * NAND_CLE: bit 1 -> Command Latch
 * NAND_ALE: bit 2 -> Address Latch
 */
static void omap_hwcontrol(struct mtd_info *mtd, int cmd, unsigned int ctrl)
{
	struct omap_nand_info *info = container_of(mtd,
					struct omap_nand_info, mtd);
	switch (ctrl) {
	case NAND_CTRL_CHANGE | NAND_CTRL_CLE:
		info->nand.IO_ADDR_W = info->gpmc_cs_baseaddr +
						GPMC_CS_NAND_COMMAND;
		info->nand.IO_ADDR_R = info->gpmc_cs_baseaddr +
						GPMC_CS_NAND_DATA;
		break;

	case NAND_CTRL_CHANGE | NAND_CTRL_ALE:
		info->nand.IO_ADDR_W = info->gpmc_cs_baseaddr +
						GPMC_CS_NAND_ADDRESS;
		info->nand.IO_ADDR_R = info->gpmc_cs_baseaddr +
						GPMC_CS_NAND_DATA;
		break;

	case NAND_CTRL_CHANGE | NAND_NCE:
		info->nand.IO_ADDR_W = info->gpmc_cs_baseaddr +
						GPMC_CS_NAND_DATA;
		info->nand.IO_ADDR_R = info->gpmc_cs_baseaddr +
						GPMC_CS_NAND_DATA;
		break;
	}

	if (cmd != NAND_CMD_NONE)
		__raw_writeb(cmd, info->nand.IO_ADDR_W);
}

/*
 * omap_read_buf16 - read data from NAND controller into buffer
 * @mtd: MTD device structure
 * @buf: buffer to store date
 * @len: number of bytes to read
 */
static void omap_read_buf16(struct mtd_info *mtd, u_char *buf, int len)
{
	struct nand_chip *nand = mtd->priv;

	__raw_readsw(nand->IO_ADDR_R, buf, len / 2);
}

/*
 * omap_write_buf16 - write buffer to NAND controller
 * @mtd: MTD device structure
 * @buf: data buffer
 * @len: number of bytes to write
 */
static void omap_write_buf16(struct mtd_info *mtd, const u_char * buf, int len)
{
	struct omap_nand_info *info = container_of(mtd,
						struct omap_nand_info, mtd);
	u16 *p = (u16 *) buf;

	/* FIXME try bursts of writesw() or DMA ... */
	len >>= 1;

	while (len--) {
		writew(*p++, info->nand.IO_ADDR_W);

		while (GPMC_BUF_EMPTY == (readl(info->gpmc_baseaddr +
						GPMC_STATUS) & GPMC_BUF_FULL));
	}
}
/*
 * omap_verify_buf - Verify chip data against buffer
 * @mtd: MTD device structure
 * @buf: buffer containing the data to compare
 * @len: number of bytes to compare
 */
static int omap_verify_buf(struct mtd_info *mtd, const u_char * buf, int len)
{
	struct omap_nand_info *info = container_of(mtd, struct omap_nand_info,
							mtd);
	u16 *p = (u16 *) buf;

	len >>= 1;

	while (len--) {

		if (*p++ != cpu_to_le16(readw(info->nand.IO_ADDR_R)))
			return -EFAULT;
	}

	return 0;
}

#ifdef CONFIG_MTD_NAND_OMAP_HWECC
/*
 * omap_hwecc_init-Initialize the Hardware ECC for NAND flash in GPMC controller
 * @mtd: MTD device structure
 */
static void omap_hwecc_init(struct mtd_info *mtd)
{
	struct omap_nand_info *info = container_of(mtd, struct omap_nand_info,
							mtd);
	register struct nand_chip *chip = mtd->priv;
	unsigned long val = 0x0;

	/* Read from ECC Control Register */
	val = __raw_readl(info->gpmc_baseaddr + GPMC_ECC_CONTROL);
	/* Clear all ECC | Enable Reg1 */
	val = ((0x00000001<<8) | 0x00000001);
	__raw_writel(val, info->gpmc_baseaddr + GPMC_ECC_CONTROL);

	/* Read from ECC Size Config Register */
	val = __raw_readl(info->gpmc_baseaddr + GPMC_ECC_SIZE_CONFIG);
	/* ECCSIZE1=512 | Select eccResultsize[0-3] */
	val = ((((chip->ecc.size >> 1) - 1) << 22) | (0x0000000F));
	__raw_writel(val, info->gpmc_baseaddr + GPMC_ECC_SIZE_CONFIG);
}

/*
 * gen_true_ecc - This function will generate true ECC value, which can be used
 * when correcting data read from NAND flash memory core
 * @ecc_buf: buffer to store ecc code
 */
static void gen_true_ecc(u8 *ecc_buf)
{
	u32 tmp = ecc_buf[0] | (ecc_buf[1] << 16) |
		((ecc_buf[2] & 0xF0) << 20) | ((ecc_buf[2] & 0x0F) << 8);

	ecc_buf[0] = ~(P64o(tmp) | P64e(tmp) | P32o(tmp) | P32e(tmp) |
			P16o(tmp) | P16e(tmp) | P8o(tmp) | P8e(tmp));
	ecc_buf[1] = ~(P1024o(tmp) | P1024e(tmp) | P512o(tmp) | P512e(tmp) |
			P256o(tmp) | P256e(tmp) | P128o(tmp) | P128e(tmp));
	ecc_buf[2] = ~(P4o(tmp) | P4e(tmp) | P2o(tmp) | P2e(tmp) | P1o(tmp) |
			P1e(tmp) | P2048o(tmp) | P2048e(tmp));
}

/*
 * omap_compare_ecc - This function compares two ECC's and indicates if there
 * is an error. If the error can be corrected it will be corrected to the
 * buffer
 * @ecc_data1:  ecc code from nand spare area
 * @ecc_data2:  ecc code from hardware register obtained from hardware ecc
 * @page_data:  page data
 */
static int omap_compare_ecc(u8 *ecc_data1,	/* read from NAND memory */
			    u8 *ecc_data2,	/* read from register */
			    u8 *page_data)
{
	uint	i;
	u8	tmp0_bit[8], tmp1_bit[8], tmp2_bit[8];
	u8	comp0_bit[8], comp1_bit[8], comp2_bit[8];
	u8	ecc_bit[24];
	u8	ecc_sum = 0;
	u8	find_bit = 0;
	uint	find_byte = 0;
	int	isEccFF;

	isEccFF = ((*(u32 *)ecc_data1 & 0xFFFFFF) == 0xFFFFFF);

	gen_true_ecc(ecc_data1);
	gen_true_ecc(ecc_data2);

	for (i = 0; i <= 2; i++) {
		*(ecc_data1 + i) = ~(*(ecc_data1 + i));
		*(ecc_data2 + i) = ~(*(ecc_data2 + i));
	}

	for (i = 0; i < 8; i++) {
		tmp0_bit[i]     = *ecc_data1 % 2;
		*ecc_data1	= *ecc_data1 / 2;
	}

	for (i = 0; i < 8; i++) {
		tmp1_bit[i]	 = *(ecc_data1 + 1) % 2;
		*(ecc_data1 + 1) = *(ecc_data1 + 1) / 2;
	}

	for (i = 0; i < 8; i++) {
		tmp2_bit[i]	 = *(ecc_data1 + 2) % 2;
		*(ecc_data1 + 2) = *(ecc_data1 + 2) / 2;
	}

	for (i = 0; i < 8; i++) {
		comp0_bit[i]     = *ecc_data2 % 2;
		*ecc_data2       = *ecc_data2 / 2;
	}

	for (i = 0; i < 8; i++) {
		comp1_bit[i]     = *(ecc_data2 + 1) % 2;
		*(ecc_data2 + 1) = *(ecc_data2 + 1) / 2;
	}

	for (i = 0; i < 8; i++) {
		comp2_bit[i]     = *(ecc_data2 + 2) % 2;
		*(ecc_data2 + 2) = *(ecc_data2 + 2) / 2;
	}

	for (i = 0; i < 6; i++)
		ecc_bit[i] = tmp2_bit[i + 2] ^ comp2_bit[i + 2];

	for (i = 0; i < 8; i++)
		ecc_bit[i + 6] = tmp0_bit[i] ^ comp0_bit[i];

	for (i = 0; i < 8; i++)
		ecc_bit[i + 14] = tmp1_bit[i] ^ comp1_bit[i];

	ecc_bit[22] = tmp2_bit[0] ^ comp2_bit[0];
	ecc_bit[23] = tmp2_bit[1] ^ comp2_bit[1];

	for (i = 0; i < 24; i++)
		ecc_sum += ecc_bit[i];

	switch (ecc_sum) {
	case 0:
		/* Not reached because this function is not called if
		 *  ECC values are equal
		 */
		return 0;

	case 1:
		/* Uncorrectable error */
		DEBUG(MTD_DEBUG_LEVEL0, "ECC UNCORRECTED_ERROR 1\n");
		return -1;

	case 11:
		/* UN-Correctable error */
		DEBUG(MTD_DEBUG_LEVEL0, "ECC UNCORRECTED_ERROR B\n");
		return -1;

	case 12:
		/* Correctable error */
		find_byte = (ecc_bit[23] << 8) +
			    (ecc_bit[21] << 7) +
			    (ecc_bit[19] << 6) +
			    (ecc_bit[17] << 5) +
			    (ecc_bit[15] << 4) +
			    (ecc_bit[13] << 3) +
			    (ecc_bit[11] << 2) +
			    (ecc_bit[9]  << 1) +
			    ecc_bit[7];

		find_bit = (ecc_bit[5] << 2) + (ecc_bit[3] << 1) + ecc_bit[1];

		DEBUG(MTD_DEBUG_LEVEL0, "Correcting single bit ECC error at "
				"offset: %d, bit: %d\n", find_byte, find_bit);

		page_data[find_byte] ^= (1 << find_bit);

		return 0;
	default:
		if (isEccFF) {
			if (ecc_data2[0] == 0 &&
			    ecc_data2[1] == 0 &&
			    ecc_data2[2] == 0)
				return 0;
		}
		DEBUG(MTD_DEBUG_LEVEL0, "UNCORRECTED_ERROR default\n");
		return -1;
	}
}

/*
 * omap_correct_data - Compares the ecc read from nand spare area with ECC
 * registers values and corrects one bit error if it has occured
 * @mtd: MTD device structure
 * @dat: page data
 * @read_ecc: ecc read from nand flash
 * @calc_ecc: ecc read from ECC registers
 */
static int omap_correct_data(struct mtd_info *mtd, u_char * dat,
				u_char * read_ecc, u_char * calc_ecc)
{
	struct omap_nand_info *info = container_of(mtd, struct omap_nand_info,
							mtd);
	int blockCnt = 0, i = 0, ret = 0;

	/* Ex NAND_ECC_HW12_2048 */
	if ((info->nand.ecc.mode == NAND_ECC_HW) &&
			(info->nand.ecc.size  == 2048))
		blockCnt = 4;
	else
		blockCnt = 1;

	for (i = 0; i < blockCnt; i++) {
		if (memcmp(read_ecc, calc_ecc, 3) != 0) {
			ret = omap_compare_ecc(read_ecc, calc_ecc, dat);
			if (ret < 0) return ret;
		}
		read_ecc += 3;
		calc_ecc += 3;
		dat      += 512;
	}
	return 0;
}

/*
 * omap_calcuate_ecc - Generate non-inverted ECC bytes.
 * Using noninverted ECC can be considered ugly since writing a blank
 * page ie. padding will clear the ECC bytes. This is no problem as long
 * nobody is trying to write data on the seemingly unused page. Reading
 * an erased page will produce an ECC mismatch between generated and read
 * ECC bytes that has to be dealt with separately.
 * @mtd: MTD device structure
 * @dat: The pointer to data on which ecc is computed
 * @ecc_code: The ecc_code buffer
 */
static int omap_calculate_ecc(struct mtd_info *mtd, const u_char *dat,
				u_char *ecc_code)
{
	struct omap_nand_info *info = container_of(mtd, struct omap_nand_info,
							mtd);
	unsigned long val = 0x0;
	unsigned long reg;

	/* Start Reading from HW ECC1_Result = 0x200 */
	reg = (unsigned long)(info->gpmc_baseaddr + GPMC_ECC1_RESULT);
	val = __raw_readl(reg);
	*ecc_code++ = val;          /* P128e, ..., P1e */
	*ecc_code++ = val >> 16;    /* P128o, ..., P1o */
	/* P2048o, P1024o, P512o, P256o, P2048e, P1024e, P512e, P256e */
	*ecc_code++ = ((val >> 8) & 0x0f) | ((val >> 20) & 0xf0);
	reg += 4;

	return 0;
}

/*
 * omap_enable_hwecc - This function enables the hardware ecc functionality
 * @mtd: MTD device structure
 * @mode: Read/Write mode
 */
static void omap_enable_hwecc(struct mtd_info *mtd, int mode)
{
	struct omap_nand_info *info = container_of(mtd, struct omap_nand_info,
							mtd);
	register struct nand_chip *chip = mtd->priv;
	unsigned int dev_width = (chip->options & NAND_BUSWIDTH_16) ? 1 : 0;
	unsigned long val = __raw_readl(info->gpmc_baseaddr + GPMC_ECC_CONFIG);

	switch (mode) {
	case NAND_ECC_READ    :
		__raw_writel(0x101, info->gpmc_baseaddr + GPMC_ECC_CONTROL);
		/* (ECC 16 or 8 bit col) | ( CS  )  | ECC Enable */
		val = (dev_width << 7) | (info->gpmc_cs << 1) | (0x1);
		break;
	case NAND_ECC_READSYN :
		 __raw_writel(0x100, info->gpmc_baseaddr + GPMC_ECC_CONTROL);
		/* (ECC 16 or 8 bit col) | ( CS  )  | ECC Enable */
		val = (dev_width << 7) | (info->gpmc_cs << 1) | (0x1);
		break;
	case NAND_ECC_WRITE   :
		__raw_writel(0x101, info->gpmc_baseaddr + GPMC_ECC_CONTROL);
		/* (ECC 16 or 8 bit col) | ( CS  )  | ECC Enable */
		val = (dev_width << 7) | (info->gpmc_cs << 1) | (0x1);
		break;
	default:
		DEBUG(MTD_DEBUG_LEVEL0, "Error: Unrecognized Mode[%d]!\n",
					mode);
		break;
	}

	__raw_writel(val, info->gpmc_baseaddr + GPMC_ECC_CONFIG);
}
#endif

/*
 * omap_wait - Wait function is called during Program and erase
 * operations and the way it is called from MTD layer, we should wait
 * till the NAND chip is ready after the programming/erase operation
 * has completed.
 * @mtd: MTD device structure
 * @chip: NAND Chip structure
 */
static int omap_wait(struct mtd_info *mtd, struct nand_chip *chip)
{
	register struct nand_chip *this = mtd->priv;
	struct omap_nand_info *info = container_of(mtd, struct omap_nand_info,
							mtd);
	int status = 0;

	this->IO_ADDR_W = (void *) info->gpmc_cs_baseaddr +
						GPMC_CS_NAND_COMMAND;
	this->IO_ADDR_R = (void *) info->gpmc_cs_baseaddr + GPMC_CS_NAND_DATA;

	while (!(status & 0x40)) {
		 __raw_writeb(NAND_CMD_STATUS & 0xFF, this->IO_ADDR_W);
		status = __raw_readb(this->IO_ADDR_R);
	}
	return status;
}

/*
 * omap_dev_ready - calls the platform specific dev_ready function
 * @mtd: MTD device structure
 */
static int omap_dev_ready(struct mtd_info *mtd)
{
	struct omap_nand_info *info = container_of(mtd, struct omap_nand_info,
							mtd);
	unsigned int val = __raw_readl(info->gpmc_baseaddr + GPMC_IRQ_STATUS);

	if ((val & 0x100) == 0x100) {
		/* Clear IRQ Interrupt */
		val |= 0x100;
		val &= ~(0x0);
		__raw_writel(val, info->gpmc_baseaddr + GPMC_IRQ_STATUS);
	} else {
		unsigned int cnt = 0;
		while (cnt++ < 0x1FF) {
			if  ((val & 0x100) == 0x100)
				return 0;
			val = __raw_readl(info->gpmc_baseaddr +
							GPMC_IRQ_STATUS);
		}
	}

	return 1;
}

static int __devinit omap_nand_probe(struct platform_device *pdev)
{
	struct omap_nand_info		*info;
	struct omap_nand_platform_data	*pdata;
	int				err;
	unsigned long 			val;


	pdata = pdev->dev.platform_data;
	if (pdata == NULL) {
		dev_err(&pdev->dev, "platform data missing\n");
		return -ENODEV;
	}

	info = kzalloc(sizeof(struct omap_nand_info), GFP_KERNEL);
	if (!info) return -ENOMEM;

	platform_set_drvdata(pdev, info);

	spin_lock_init(&info->controller.lock);
	init_waitqueue_head(&info->controller.wq);

	info->pdev = pdev;

	info->gpmc_cs		= pdata->cs;
	info->gpmc_baseaddr	= pdata->gpmc_baseaddr;
	info->gpmc_cs_baseaddr	= pdata->gpmc_cs_baseaddr;

	info->mtd.priv		= &info->nand;
	info->mtd.name		= dev_name(&pdev->dev);
	info->mtd.owner		= THIS_MODULE;

	err = gpmc_cs_request(info->gpmc_cs, NAND_IO_SIZE, &info->phys_base);
	if (err < 0) {
		dev_err(&pdev->dev, "Cannot request GPMC CS\n");
		goto out_free_info;
	}

	/* Enable RD PIN Monitoring Reg */
	if (pdata->dev_ready) {
		val  = gpmc_cs_read_reg(info->gpmc_cs, GPMC_CS_CONFIG1);
		val |= WR_RD_PIN_MONITORING;
		gpmc_cs_write_reg(info->gpmc_cs, GPMC_CS_CONFIG1, val);
	}

	val  = gpmc_cs_read_reg(info->gpmc_cs, GPMC_CS_CONFIG7);
	val &= ~(0xf << 8);
	val |=  (0xc & 0xf) << 8;
	gpmc_cs_write_reg(info->gpmc_cs, GPMC_CS_CONFIG7, val);

	/* NAND write protect off */
	omap_nand_wp(&info->mtd, NAND_WP_OFF);

	if (!request_mem_region(info->phys_base, NAND_IO_SIZE,
				pdev->dev.driver->name)) {
		err = -EBUSY;
		goto out_free_cs;
	}

	info->nand.IO_ADDR_R = ioremap(info->phys_base, NAND_IO_SIZE);
	if (!info->nand.IO_ADDR_R) {
		err = -ENOMEM;
		goto out_release_mem_region;
	}
	info->nand.controller = &info->controller;

	info->nand.IO_ADDR_W = info->nand.IO_ADDR_R;
	info->nand.cmd_ctrl  = omap_hwcontrol;

	/* REVISIT:  only supports 16-bit NAND flash */

	info->nand.read_buf   = omap_read_buf16;
	info->nand.write_buf  = omap_write_buf16;
	info->nand.verify_buf = omap_verify_buf;

	/*
	* If RDY/BSY line is connected to OMAP then use the omap ready funcrtion
	* and the generic nand_wait function which reads the status register
	* after monitoring the RDY/BSY line.Otherwise use a standard chip delay
	* which is slightly more than tR (AC Timing) of the NAND device and read
	* status register until you get a failure or success
	*/
	if (pdata->dev_ready) {
		info->nand.dev_ready = omap_dev_ready;
		info->nand.chip_delay = 0;
	} else {
		info->nand.waitfunc = omap_wait;
		info->nand.chip_delay = 50;
	}

	info->nand.options  |= NAND_SKIP_BBTSCAN;
	if ((gpmc_cs_read_reg(info->gpmc_cs, GPMC_CS_CONFIG1) & 0x3000)
								== 0x1000)
		info->nand.options  |= NAND_BUSWIDTH_16;

#ifdef CONFIG_MTD_NAND_OMAP_HWECC
	info->nand.ecc.bytes		= 3;
	info->nand.ecc.size		= 512;
	info->nand.ecc.calculate	= omap_calculate_ecc;
	info->nand.ecc.hwctl		= omap_enable_hwecc;
	info->nand.ecc.correct		= omap_correct_data;
	info->nand.ecc.mode		= NAND_ECC_HW;

	/* init HW ECC */
	omap_hwecc_init(&info->mtd);
#else
	info->nand.ecc.mode = NAND_ECC_SOFT;
#endif

	/* DIP switches on some boards change between 8 and 16 bit
	 * bus widths for flash.  Try the other width if the first try fails.
	 */
	if (nand_scan(&info->mtd, 1)) {
		info->nand.options ^= NAND_BUSWIDTH_16;
		if (nand_scan(&info->mtd, 1)) {
			err = -ENXIO;
			goto out_release_mem_region;
		}
	}

#ifdef CONFIG_MTD_PARTITIONS
	err = parse_mtd_partitions(&info->mtd, part_probes, &info->parts, 0);
	if (err > 0)
		add_mtd_partitions(&info->mtd, info->parts, err);
	else if (pdata->parts)
		add_mtd_partitions(&info->mtd, pdata->parts, pdata->nr_parts);
	else
#endif
		add_mtd_device(&info->mtd);

	platform_set_drvdata(pdev, &info->mtd);

	return 0;

out_release_mem_region:
	release_mem_region(info->phys_base, NAND_IO_SIZE);
out_free_cs:
	gpmc_cs_free(info->gpmc_cs);
out_free_info:
	kfree(info);

	return err;
}

static int omap_nand_remove(struct platform_device *pdev)
{
	struct mtd_info *mtd = platform_get_drvdata(pdev);
	struct omap_nand_info *info = mtd->priv;

	platform_set_drvdata(pdev, NULL);
	/* Release NAND device, its internal structures and partitions */
	nand_release(&info->mtd);
	iounmap(info->nand.IO_ADDR_R);
	kfree(&info->mtd);
	return 0;
}

static struct platform_driver omap_nand_driver = {
	.probe		= omap_nand_probe,
	.remove		= omap_nand_remove,
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
};
MODULE_ALIAS(DRIVER_NAME);

static int __init omap_nand_init(void)
{
	printk(KERN_INFO "%s driver initializing\n", DRIVER_NAME);
	return platform_driver_register(&omap_nand_driver);
}

static void __exit omap_nand_exit(void)
{
	platform_driver_unregister(&omap_nand_driver);
}

module_init(omap_nand_init);
module_exit(omap_nand_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Glue layer for NAND flash on TI OMAP boards");
