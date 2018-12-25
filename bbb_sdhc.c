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

int card_trans_status(void);
int card_enter_trans(void);
int card_get_cid(void);
void card_cmd_config(command_t * cmd, int index, int argument, xfer_type_t transfer,
                     response_format_t format, data_present_select data,
                     crc_check_enable crc, cmdindex_check_enable cmdindex);
static int card_software_reset(void);
int card_emmc_init(void);
int card_data_read(int *dst_ptr, int length, uint32_t offset);

/* Global Variables */

sdhc_inst_t sdhc_device = {
     0x481D8000,        //register base
     0x00000000,        //ADMA base
     NULL,              //ISR
     0,                 //RCA
     0,                 //addressing mode
     0x00000000,        //interrupt ID
     1,                 //status
};

void host_clear_fifo(void)
{
	unsigned int val, idx;

	if (__raw_readl(0x481D8230) & 0x00000020)
	{
		for (idx = 0; idx < SDHC_FIFO_LENGTH; idx++)
		{	
			val = __raw_readl(0x481D8220);
		}
	}

	val = val * 2;

	val = __raw_readl(0x481D8230) & ~0x00000020;
	val |= 0x00000020;
	__raw_writel(val, 0x481D8230);	
}

int card_set_blklen(int len)
{
	command_t cmd;
	int status = FAIL;

	card_cmd_config(&cmd, CMD16, len, READ, RESPONSE_48, DATA_PRESENT_NONE, TRUE, TRUE);
	printf("Send CMD16.\n");

	if (host_send_cmd(&cmd) == SUCCESS)
	{
		status = SUCCESS;
	}

	return status;
}

/* Whether to enable ADMA */
//static int SDHC_ADMA_mode = FALSE;

/* Whether to enable Interrupt */
//static int SDHC_INTR_mode = FALSE;

/*!
 * @brief Addressed card send its status register
 *
 * @param instance     Instance number of the uSDHC module.
 * 
 * @return             0 if successful; 1 otherwise
 */
int card_trans_status(void)
{
	command_t cmd;
	command_response_t response;
	int card_state, card_address, status = FAIL;

	/* Get RCA */
	card_address = sdhc_device.rca << RCA_SHIFT;

	/* Configure CMD13 */
	card_cmd_config(&cmd, CMD13, card_address, READ, RESPONSE_48, DATA_PRESENT_NONE, TRUE, TRUE);

	printf("Send CMD13.\n");

	/* Send CMD13 */
	if (host_send_cmd(&cmd) == SUCCESS)
	{
		/* Get Response */
		response.format = RESPONSE_48;
		host_read_response(&response);

		/* Read card state from response */
		card_state = CURR_CARD_STATE(response.cmd_rsp0);
		if (card_state == TRAN)
		{
			status = SUCCESS;
		}
	}

	return status;
}

/*!
 * @brief Toggle the card between the standby and transfer states
 *
 * @param instance     Instance number of the uSDHC module.
 * 
 * @return             0 if successful; 1 otherwise
 */
int card_enter_trans(void)
{
	command_t cmd;
	int card_address, status = FAIL;

	/* Get RCA */
	card_address = sdhc_device.rca << RCA_SHIFT;

	/* Configure CMD7 */
	card_cmd_config(&cmd, CMD7, card_address, READ, RESPONSE_48_CHECK_BUSY, DATA_PRESENT_NONE, TRUE, TRUE);

	printf("Send CMD7");

	/* Send CMD7 */
	if (host_send_cmd(&cmd) == SUCCESS)
	{
		/* Check of the card is in TRAN state */
		if (card_trans_status() == SUCCESS)
		{
			status = SUCCESS;
		}
	}

	return status;
}

/*!
 * @brief Get Card CID
 *
 * @param instance     Instance number of the uSDHC module.
 * 
 * @return             0 if successful; 1 otherwise
 */
int card_get_cid(void)
{
	command_t cmd;
	int status = FAIL;
	command_response_t response;

	/* Configure CMD2 */
	card_cmd_config(&cmd, CMD2, NO_ARG, READ, RESPONSE_136, DATA_PRESENT_NONE, TRUE, FALSE);

	printf("Send CMD2.\n");

	/* Send CMD2 */
	if (host_send_cmd(&cmd) == SUCCESS)
	{
		response.format = RESPONSE_136;
		host_read_response(&response);

		/* No Need to Save CID */

		status = SUCCESS;
	}

	return status;
}

/*!
 * @brief Build up command
 *
 * @param cmd      IPointer of command to be build up.
 * @param index    Command index.
 * @param argument Argument of the command.
 * @param transfer Command transfer type - Read, Write or SD command.
 * @param format   Command response format
 * @param data     0 - no data present, 1 - data present.
 * @param src      0 - no CRC check, 1 - do CRC check
 * @param cmdindex 0 - no check on command index, 1 - Check comamnd index
 */
void card_cmd_config(command_t * cmd, int index, int argument, xfer_type_t transfer,
                     response_format_t format, data_present_select data,
                     crc_check_enable crc, cmdindex_check_enable cmdindex)
{
    cmd->command = index;
    cmd->arg = argument;
    cmd->data_transfer = transfer;
    cmd->response_format = format;
    cmd->data_present = data;
    cmd->crc_check = crc;
    cmd->cmdindex_check = cmdindex;
    cmd->dma_enable = FALSE;
    cmd->block_count_enable_check = FALSE;
    cmd->multi_single_block = SINGLE;
    cmd->acmd12_enable = FALSE;
    cmd->ddren = FALSE;

    /* Multi Block R/W Setting */
    if ((CMD18 == index) || (CMD25 == index)) {
        /*if (SDHC_ADMA_mode == TRUE) {
            cmd->dma_enable = TRUE;
        }*/

        cmd->block_count_enable_check = TRUE;
        cmd->multi_single_block = MULTIPLE;
        cmd->acmd12_enable = TRUE;
    }
}

static int card_software_reset(void)
{
	command_t cmd;
	int response = FAIL;

	/* Configure CMD0 */
	card_cmd_config(&cmd, CMD0, NO_ARG, WRITE, RESPONSE_NONE, DATA_PRESENT_NONE, FALSE, FALSE);

	printf("Send CMD0.\n");

	/* Issue CMD0 to Card */
	if (host_send_cmd(&cmd) == SUCCESS)
	{
		response = SUCCESS;
	}

	return response;
}

int card_data_read(int *dst_ptr, int length, uint32_t offset)
{
	int port, sector;
	command_t cmd;

	printf("card_data_read: Read 0x%x bytes from SD%d offset 0x%x to 0x%x.\n",
	       length, port + 1, offset, (int)dst_ptr);

	sector = length / BLK_LEN;

	if ((length % BLK_LEN) != 0) {
		sector++;
	}

	if (sdhc_device.addr_mode == SECT_MODE) {
		offset = offset / BLK_LEN;
		printf("The offset is %x.\n", offset);
	}

	if (card_set_blklen(BLK_LEN) == FAIL) {
		printf("Fail to set block length to card in reading sector %d");
		return FAIL;
	}

	host_clear_fifo();

	host_cfg_block(BLK_LEN, sector);

	card_cmd_config(&cmd, CMD18, offset, READ, RESPONSE_48, DATA_PRESENT, TRUE, TRUE);

	printf("card_data_read: Send CMD18.\n");

	if (host_send_cmd(&cmd) == FAIL)
	{
		printf("Fail to send CMD18.\n");
		return FAIL;
	}
	else
	{
		printf("Read data from FIFO.\n");

		if (host_data_read(dst_ptr, length, ESDHC_BLKATTR_WML_BLOCK) == FAIL)
		{
			printf("Fail to read data from card.\n");
			return FAIL;
		}
	}

	printf("card_data_read: Data read successful.\n");

	return SUCCESS;
}

int card_emmc_init(void)
{
	int init_status = FAIL;
	unsigned int val = 0x00000000;

	/* Software reset to host controller */
	host_reset(SDHC_ONE_BIT_SUPPORT);

	/* Enable Init Frequency */
//	host_cfg_clock(INIT_FREQ);
//	printf("Init frequency set.\n");

	/* Send Init 80 Clock */
	host_init_active();
	printf("80 clocks sent.\n");
	
	/* Enable Identification Frequency */
//	host_cfg_clock(IDENTIFICATION_FREQ);
//	printf("Ident frequency set.\n");

	/* Issue Software Reset to card */
	if (card_software_reset() == FAIL)
	{
		return init_status;
		printf("CMD0 fail\n");
	}
	printf("Card reset Successfully\n");
	
	/* Software reset */
	val = __raw_readl(0x481D822C) & ~0x02000000;
	val |= 0x02000000;
	__raw_writel(val, 0x481D822C);
	
	while (__raw_readl(0x481D822C) & 0x02000000)
	{
		;
	}
	printf("Software reset done\n");
	
	/* MMC Voltage Validation */
	//if (mmc_voltage_validation() == SUCCESS)
	if (mmc_voltage_validation() == SUCCESS)
	{
		/* MMC Initialization */

		printf("card voltage validation success\n");
		init_status = emmc_init();
	}

	return init_status;
}
