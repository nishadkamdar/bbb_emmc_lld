#include <bbb_types.h>
#include <bbb_sdhc.h>
#include <bbb_sdhc_test.h>
#include <common.h>
#include <command.h>
#include <errno.h>
#include <autoboot.h>
#include <bootretry.h>
#include <cli.h>
#include <console.h>
#include <fdtdec.h>
#include <menu.h>
#include <post.h>
#include <u-boot/sha256.h>

/* Buffer Definition */
static int mmc_test_src[MMC_TEST_BUF_SIZE + MMC_CARD_SECTOR_BUFFER];
static int mmc_test_dst[MMC_TEST_BUF_SIZE + MMC_CARD_SECTOR_BUFFER];
static int mmc_test_tmp[MMC_TEST_BUF_SIZE + MMC_CARD_SECTOR_BUFFER];

static int emmc_test_dump(void)
{
	emmc_print_cfg_info();

	return TRUE;
}

static test_return_t mmc_test(unsigned int bus_width)
{
	int status, idx, result;
	int i;
	
	printf("1. Card -> TMP.\n");

	memset(mmc_test_src, 0x5A, MMC_TEST_BUF_SIZE);
	memset(mmc_test_dst, 0xA5, MMC_TEST_BUF_SIZE);

	status = card_data_read(mmc_test_tmp, MMC_TEST_BUF_SIZE * sizeof(int), MMC_TEST_OFFSET);
	if (status == FAIL) {
		printf("%d: SD/MMC data read failed.\n", __LINE__);
		return TEST_FAILED;
	}

	for (i = 0; i < (MMC_TEST_BUF_SIZE); i++)
	{
		printf("mmc_test_tmp[%d] = %x\n", i, mmc_test_tmp[i]);
	}
}

static int do_cmd(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	int status = FAIL;
	unsigned int cap = 0;
	printf ("\n\tInitializing eMMC chip.\n");

	printf("MMC1 registers\n");	
	cap = __raw_readl(0x481D8110);
	printf ("The SYSCONFIG register is %x\n", cap);

	cap = __raw_readl(0x481D8114);
	printf ("The SYSSTATUS register is %x\n", cap);

	cap = __raw_readl(0x481D8228);
	printf ("The SD_HCTL register is %x\n", cap);

	cap = __raw_readl(0x481D8240);
	printf ("The SD_CAPA register is %x\n", cap);

	cap = __raw_readl(0x481D8248);
	printf ("The SD_CUR_CAPA register is %x\n", cap);

	cap = __raw_readl(0x481D822C);
	printf ("The SD_STSCTL register is %x\n", cap);
	
	if (FAIL == card_emmc_init())
	{
		printf("Initializing eMMC failed.\n");
		goto out;
	}
	printf("Initialized eMMC successfully\n");

	emmc_test_dump();

	mmc_test(1);
out:
	return -1;
}

U_BOOT_CMD(test_cmd, 4, 0, do_cmd, "test command", "prints names wrt switches.\n" "simple test command to check the functionality of u-boot command\n" "valid arguments, [n,m,p,a]");
