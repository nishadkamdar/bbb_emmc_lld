#include <bbb_types.h>
#include <bbb_sdhc.h>
#include <bbb_sdhc_host.h>
#include <bbb_sdhc_mmc.h>
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

static struct csd_struct csd_reg;
static uint32_t ext_csd_data[BLK_LEN / FOUR];
static uint32_t mmc_version = MMC_CARD_INV;

static int mmc_read_esd(void);
static int mmc_switch(uint32_t arg);
static int mmc_set_bus_width(int bus_width);
static int mmc_read_csd(void);
static uint32_t mmc_get_spec_ver(void);
static int mmc_set_rca(void);
int emmc_init(void);
int mmc_voltage_validation(void);
void emmc_print_cfg_info(void);

/*!
 * @brief Send CMD8 to get EXT_CSD value of MMC;
 * 
 * @param instance     Instance number of the uSDHC module.
 * 
 * @return             0 if successful; 1 otherwise
 */
static int mmc_read_esd(void)
{
	command_t cmd;
	unsigned int i = 0;
	int status = FAIL;

	/* Set block length */
	card_cmd_config(&cmd, CMD16, BLK_LEN, READ, RESPONSE_48, DATA_PRESENT_NONE, TRUE, TRUE);

	printf("Send CMD16.\n");

	/* Send CMD16 */
	if (SUCCESS == host_send_cmd(&cmd))
	{
		/* Configure block attribute */
		host_cfg_block(BLK_LEN, ONE);
		printf("Host block configured\n");
		
		/* Read extended CSD */
		card_cmd_config(&cmd, CMD8, NO_ARG, READ, RESPONSE_48, DATA_PRESENT, TRUE, TRUE);

		printf("Send CMD8.\n");

		/* Send CMD8 */
		if (SUCCESS == host_send_cmd(&cmd))
		{
			status = host_data_read((int*) ext_csd_data, BLK_LEN, SDHC_BLKATTR_WML_BLOCK);
			for (i = 0; i < (BLK_LEN / FOUR); i++)
			{
				printf("CSD[%d] = %x\n", i, ext_csd_data[i]);
			}
		}
	}

	return status;
}

/*!
 * @brief Check switch ability and switch function 
 * 
 * @param instance     Instance number of the uSDHC module.
 * @param arg          Argument to command 6 
 * 
 * @return             0 if successful; 1 otherwise
 */
static int mmc_switch(uint32_t arg)
{
	command_t cmd;
	int status = FAIL;

	/* Configure MMC Switch Command */
	card_cmd_config(&cmd, CMD6, arg, READ, RESPONSE_48, DATA_PRESENT_NONE, TRUE, TRUE);

	printf("Send CMD6.\n");

	/* Send CMD6 */
	if (SUCCESS == host_send_cmd(&cmd))
	{
		status = card_trans_status();
	}

	return status;
}

static int mmc_set_bus_width(int bus_width)
{
	return mmc_switch(MMC_SWITCH_SETBW_ARG(bus_width));
}

/*!
 * @brief Read card specified data (CSD)
 * 
 * @param instance     Instance number of the uSDHC module.
 * 
 * @return             0 if successful; 1 otherwise
 */
static int mmc_read_csd(void)
{
	command_t cmd;
	command_response_t response;

	int status = SUCCESS;

	/* Configure read CSD command */
	card_cmd_config(&cmd, CMD9, ONE << RCA_SHIFT, READ, RESPONSE_136, DATA_PRESENT_NONE, TRUE, FALSE);
	printf("Send CMD9.\n");

	/* Send CMD9 */
	if (host_send_cmd(&cmd) == FAIL)
	{
		status = FAIL;
	}
	else
	{
		/* Read response */
		response.format = RESPONSE_136;
		host_read_response(&response);

		csd_reg.response[0] = response.cmd_rsp0;
		csd_reg.response[1] = response.cmd_rsp1;
		csd_reg.response[2] = response.cmd_rsp2;
		csd_reg.response[3] = response.cmd_rsp3;

		csd_reg.csds = (csd_reg.response[3] & 0xC0000000) >> 30;
		csd_reg.ssv = (csd_reg.response[3] & 0x3C000000) >> 26;
	}

	return status;
}

/*!
 * @brief Read CSD and EXT_CSD value of MMC;
 * 
 * @param instance     Instance number of the uSDHC module.
 * 
 * @return             CSD value if successful; 0 otherwise
 */
static uint32_t mmc_get_spec_ver(void)
{
	int retv = 0;

	/* Read CSD */
	if (SUCCESS == mmc_read_csd())
	{
		//retv = csd_reg.ssv | (csd_reg.csds << 8);
		printf("CSD structure = %d\n", csd_reg.csds);
		printf("SSV is = %d\n", csd_reg.ssv);
		printf("CSD read succes\n");
	}

	/* Enter transfer mode */
	if (SUCCESS == card_enter_trans())
	{
		printf("Card entered trans state successfully\n");
		/* Set bus width */
		if (mmc_set_bus_width(ONE) == SUCCESS)
		{
			printf("MMC bus width set\n");
			host_set_bus_width(ONE);
			printf("MMC host bus width set\n");
		}

		/* Read Extened CSD */
		if (SUCCESS == mmc_read_esd())
		{
			printf("esd read success\n");
			retv |= (ext_csd_data[48] & 0x00FF0000) | ((ext_csd_data[57] & 0xFF) << 24);
		}
	}

	return retv;
}

/********************************************* Static Function ******************************************/
/*!
 * @brief Set the mmc card a RCA 
 * 
 * @param instance     Instance number of the uSDHC module.
 * 
 * @return             0 if successful; 1 otherwise
 */
static int mmc_set_rca(void)
{
	command_t cmd;
	int card_state, status = FAIL;
	command_response_t response;

	/* Set RCA to ONE */
	sdhc_device.rca = ONE;

	/* Configure CMD3 */
	card_cmd_config(&cmd, CMD3, (sdhc_device.rca << RCA_SHIFT), READ, RESPONSE_48, DATA_PRESENT_NONE, TRUE, TRUE);

	printf("Send CMD3.\n");

	/* Send CMD3 */
	if (host_send_cmd(&cmd) == SUCCESS)
	{
		response.format = RESPONSE_48;
		host_read_response(&response);

		/* Check the IDENT card state */
		card_state = CURR_CARD_STATE(response.cmd_rsp0);

		if (card_state == IDENT)
		{
			status = SUCCESS;
		}
	}

	return status;
}

void emmc_print_cfg_info(void)
{
	uint8_t byte, *ptr;

	if (mmc_version == MMC_CARD_INV) {
		printf("Invalid or uinitialized card.\n");
		return;
	}

	if (FAIL == mmc_read_esd()) {
		printf("Read extended CSD failed.\n");
		return;
	}

	ptr = (uint8_t *) ext_csd_data;

	byte = ptr[MMC_ESD_OFF_PRT_CFG] & BP_MASK;

	printf("\t%s enabled for boot.\n", (byte == BP_USER) ? "User Partition" :
	       (byte == BP_BT1) ? "Boot partition #1" :
	       (byte == BP_BT2) ? "Boot partition #2" : "No partition");

	if (mmc_version == MMC_CARD_4_4) {
		byte = ptr[MMC_ESD_OFF_PRT_CFG] & BT_ACK;

		printf("\tFast boot acknowledgement %s\n", (byte == 0) ? "disabled" : "enabled");
	}

	byte = ptr[MMC_ESD_OFF_BT_BW] & BBW_BUS_MASK;
	printf("\tFast boot bus width: %s\n", (byte == BBW_1BIT) ? "1 bit" :
	       (byte == BBW_4BIT) ? "4 bit" : (byte == BBW_8BIT) ? "8 bit" : "unknowni");

	byte = ptr[MMC_ESD_OFF_BT_BW] & BBW_DDR_MASK;
	printf("\tDDR boot mode %s\n", (byte == BBW_DDR) ? "enabled" : "disabled");

	byte = ptr[MMC_ESD_OFF_BT_BW] & BBW_SAVE;
	printf("\t%s boot bus width settings.\n\n", (byte == 0) ? "Discard" : "Retain");
}	

/*!
 * @brief Initialize eMMC - Get Card ID, Set RCA, Frequency and bus width.
 * 
 * @param instance     Instance number of the uSDHC module.
 * 
 * @return             0 if successful; 1 otherwise
 */
int emmc_init(void)
{
	uint8_t byte;
	uint32_t retv;
	int status = FAIL;

	/* Init MMC version */
	mmc_version = MMC_CARD_INV;

	/* Get CID */
	if (card_get_cid() == SUCCESS)
	{
		printf("Reveived card CID\n");
		/* Set RCA */
		if (mmc_set_rca() == SUCCESS)
		{
			printf("Successfully set relative card address\n");
			status = SUCCESS;

			retv = mmc_get_spec_ver();
			printf("retv is %x\n", retv);		

			/* Obtain CSD structure */
			byte = (retv >> 16) & 0xFF;

			if (byte == 3) {
                	/* Obtain system spec version in CSD */
                		byte = (retv >> 16) & 0xFF;
            		}

            		if (byte == 2) {
                	/* If support DDR mode */
                		byte = retv >> 24;
                		if (byte & 0x2) {
                    			mmc_version = MMC_CARD_4_4;
                    			printf("\teMMC 4.4 card.\n");
                		} else {
                    			mmc_version = MMC_CARD_4_X;
                    			printf("\teMMC 4.X (X<4) card.\n");
                		}
            		} else {
                		mmc_version = MMC_CARD_3_X;
                		printf("\tMMC 3.X or older cards.\n");
            		}
		}
	}
}

/*!
 * @brief Omap Card identification and selection.
 * 
 * @param instance     Instance number of the uSDHC module.
 * 
 * @return             0 if successful; 1 otherwise
 */
int mmc_omap_validation(void)
{
	command_t cmd;
	command_response_t response;
	int count = ZERO;
	int status = FAIL;
	unsigned int val = 0;

	/* Configure CMD5 */
	card_cmd_config(&cmd, CMD5, ((sdhc_device.rca << RCA_SHIFT) | 0x00008000), WRITE, RESPONSE_48, DATA_PRESENT_NONE, TRUE, TRUE);

	/* Wait for CMD/DATA lines to be free */
	if (sdhc_wait_cmd_data_lines(cmd.data_present) == FAIL)
	{
		printf("Data/Command lines busy.\n");
		return FAIL;
	}
	
	writel(0xFFFFFFFF, 0x481D8230);

	while (readl(0x481D8230))
	{
		udelay(1000);
		printf("timedout waiting for stat to clear\n");	
	}
	
	/*Set appropriate bits in SD_IE register*/
	__raw_writel(0x327f0033, 0x481D8234);
	
	sdhc_cmd_cfg(&cmd);

	if (!(__raw_readl(0x481D8230) & 0x00000001))
	{
		printf("CMD5 CC = 0\n");
		udelay(10000);
		if((__raw_readl(0x481D8230) & 0x00010000))
		{
			printf("CMD5 CTO = 1\n");
			printf("CMD5 Success\n");
		}
		else
		{
			printf("CMD5 fail\n");
		}
	}
	else
	{
		printf("CMD5 fail\n");
	}
	
	/* Software reset */
	val = __raw_readl(0x481D822C) & ~0x02000000;
	val |= 0x02000000;
	__raw_writel(val, 0x481D822C);
	
	while (__raw_readl(0x481D822C) & 0x02000000)
	{
		;
	}
	printf("Software reset done\n");

		
	/* Configure CMD8 */
	//card_cmd_config(&cmd, CMD8, NO_ARG, WRITE, RESPONSE_48, DATA_PRESENT_NONE, TRUE, TRUE);

	/* Send CMD1 */
	//if (host_send_cmd(&cmd) == FAIL)
	//{
	//	printf("Send CMD8 failed\n");
	//	break;
	//}

//	while ((count < MMC_VOLT_VALID_COUNT) && (status == FAIL))
//	{
		/* Configure CMD8 */
//		card_cmd_config(&cmd, CMD8, NO_ARG, WRITE, RESPONSE_48, DATA_PRESENT_NONE, TRUE, TRUE);

		/* Send CMD1 */
//		if (host_send_cmd(&cmd) == FAIL)
//		{
//			printf("Send CMD8 failed\n");
//			break;
//		}
//		else
//		{
			/* Check Response */
//			response.format = RESPONSE_48;
//			host_read_response(&response);

			/* Check Busy Bit Cleared or NOT */
//			if (response.cmd_rsp0 & CARD_BUSY_BIT)
//			{
				/* Check Address Mode */
//				if ((response.cmd_rsp0 & MMC_OCR_HC_BIT_MASK) == MMC_OCR_HC_RESP_VAL)
//				{
//					sdhc_device.addr_mode = SECT_MODE;
//				}
//				else
//				{
//					sdhc_device.addr_mode = BYTE_MODE;
//				}

//				status = SUCCESS;
//			}
//			else
//			{
//				count++;
//				udelay(MMC_VOLT_VALID_DELAY);
//			}
//		}
//	}

	return status;
}

/*!
 * @brief Valid the voltage.
 * 
 * @param instance     Instance number of the uSDHC module.
 * 
 * @return             0 if successful; 1 otherwise
 */
int mmc_voltage_validation(void)
{
	command_t cmd;
	command_response_t response;
	int count = ZERO;
	int status = FAIL;
	unsigned int ocr_val = MMC_HV_HC_OCR_VALUE;

	while ((count < MMC_VOLT_VALID_COUNT) && (status == FAIL))
	{
		/* Configure CMD1 */
		card_cmd_config(&cmd, CMD1, ocr_val, WRITE, RESPONSE_48, DATA_PRESENT_NONE, FALSE, FALSE);

		/* Send CMD1 */
		if (host_send_cmd(&cmd) == FAIL)
		{
			printf("Send CMD1 failed\n");
			break;
		}
		else
		{
			/* Check Response */
			response.format = RESPONSE_48;
			host_read_response(&response);

			/* Check Busy Bit Cleared or NOT */
			if (response.cmd_rsp0 & CARD_BUSY_BIT)
			{
				/* Check Address Mode */
				if ((response.cmd_rsp0 & MMC_OCR_HC_BIT_MASK) == MMC_OCR_HC_RESP_VAL)
				{
					sdhc_device.addr_mode = SECT_MODE;
				}
				else
				{
					sdhc_device.addr_mode = BYTE_MODE;
				}

				status = SUCCESS;
			}
			else
			{
				count++;
				udelay(MMC_VOLT_VALID_DELAY);
			}
		}
	}

	return status;
}
