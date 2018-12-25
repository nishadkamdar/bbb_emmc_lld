#include <bbb_types.h>
#include <bbb_sdhc.h>
#include <bbb_sdhc_mmc.h>
#include <bbb_sdhc_host.h>
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

static int sdhc_check_transfer(void);
int host_data_read(int *dst_ptr, int length, int wml);
void host_read_response(command_response_t *response);
static int sdhc_check_response(void);
static void sdhc_wait_end_cmd_resp_intr(void);
static void sdhc_cmd_cfg(command_t *cmd);
static int sdhc_wait_cmd_data_lines(int data_present);
int host_send_cmd(command_t * cmd);
void host_init_active(void);
void host_cfg_clock(int frequency);
static void sdhc_set_data_transfer_width(int dat_width);
void host_set_bus_width(int bus_width);
void host_reset(int bus_width);
void host_cfg_block(int blk_len, int nob);

/*!
 * @brief uSDHC Controller Checks transfer
 *
 * @param instance     Instance number of the uSDHC module.
 * 
 * @return             0 if successful; 1 otherwise
 */
static int sdhc_check_transfer(void)
{
	int status = FAIL;

	if ((__raw_readl(0x481D8230) & 0x00000002) &&
	     !(__raw_readl(0x481D8230) & 0x00080000) &&
	     !(__raw_readl(0x481D8230) & 0x00100000))
	{
		status = SUCCESS;
	}
	else
	{
		printf("Error transfer status: 0x%x\n", __raw_readl(0x481D8230));
	}

	return status;
}


/*!
 * @brief uSDHC Controller reads data
 * 
 * @param instance     Instance number of the uSDHC module.
 * @param dst_ptr      Pointer for data destination
 * @param length       Data length to be reading
 * @param wml          Watermark for data reading
 * 
 * @return             0 if successful; 1 otherwise
 */
int host_data_read(int *dst_ptr, int length, int wml)
{
	int idx, itr, loop;
	unsigned int val = 0;

	/* Enable Interrupt */
	val = __raw_readl(0x481D8234);
	val |= 0x007F013F;
	__raw_writel(val, 0x481D8234);

	/* Read data to dst_ptr */
	loop = length / (4 * wml);
	for(idx = 0; idx < loop; idx++)
	{
		/* Wait until buffer ready */
		while (!(__raw_readl(0x481D8224) & 0x00000800))
		{
			;
		}
		printf("Buffer ready.\n");

		/* Read from FIFO watermark words */
		for(itr = 0; itr < wml; itr++)
		{
			*dst_ptr = __raw_readl(0x481D8220);
			dst_ptr++;
		}
	}

	/* Read left data that not WML aligned */
	loop = (length % (4 * wml)) / 4;
	if (loop != 0)
	{
		/* Wait until buffer ready */
		while (!(__raw_readl(0x481D8224) & 0x00000800))
		{
			;
		}
		printf("Buffer ready 2.\n");

		/* Read the left to destination buffer */
		for (itr = 0; itr < loop; itr++)
		{
			*dst_ptr = __raw_readl(0x481D8220);
			dst_ptr++;
		}

		/* Clear FIFO */
		for(; itr < wml; itr++)
		{
			idx = __raw_readl(0x481D8220);
		}
	}

	/* Wait until transfer complete */
	while (!(__raw_readl(0x481D8230) & 0x00700002));

	/* Check if error happened */
	return sdhc_check_transfer();
}

/*!
 * @brief uSDHC Controller reads responses
 * 
 * @param instance     Instance number of the uSDHC module.
 * @param response     Responses from card
 */
void host_read_response(command_response_t *response)
{
	/* Read response from registers */
	response->cmd_rsp0 = __raw_readl(0x481D8210);
	printf("resp0: %x\n", response->cmd_rsp0);
	
	response->cmd_rsp1 = __raw_readl(0x481D8214);
	printf("resp1: %x\n", response->cmd_rsp1);

	response->cmd_rsp2 = __raw_readl(0x481D8218);
	printf("resp2: %x\n", response->cmd_rsp2);

	response->cmd_rsp3 = __raw_readl(0x481D821C);
	printf("resp3: %x\n", response->cmd_rsp3);
}

void host_cfg_block(int blk_len, int nob)
{
	int sd_blk = (nob << 16) | blk_len;
	printf("SD_BLK register is %x\n", sd_blk);
	/*__raw_writew(blk_len, 0x481D8204);
	__raw_writew(nob, 0x481D8206);*/
	__raw_writel(sd_blk, 0x481D8204);
}

/*!
 * @brief uSDHC Controller Checks response
 *
 * @param instance     Instance number of the uSDHC module.
 * 
 * @return             0 if successful; 1 otherwise
 */
static int sdhc_check_response(void)
{
	int status = FAIL;
	int val;

	if ((readl(0x481D8230) & 0x0000001) &&
	    (!(readl(0x481D8230) & 0x0000100)) &&
	    (!(readl(0x481D8230) & 0x0000200)) &&
	    (!(readl(0x481D8230) & 0x0000400)) &&
	    (!(readl(0x481D8230) & 0x0000800)))
	   {
	   	status = SUCCESS; 
	   }
	else
	{
		printf("Error status: 0x%x\n", __raw_readl(0x481D8230));
		/* Clear CIHB and CDIHB status */
		if ((__raw_readl(0x481D8224) & 0x00000001) ||
		   (__raw_readl(0x481D8224) & 0x00000002))
		   {
			val = (__raw_readl(0x481D822C) | 0x01000000);
		   	writel(val, 0x481D822F);
		   }
	}
	    	
	return status;
}

/*!
 * @brief Wait for command complete and without error
 *
 * @param instance     Instance number of the uSDHC module.
 * 
 */
static void sdhc_wait_end_cmd_resp_intr(void)
{
	int count = ZERO;
	unsigned int val = 0x00000000;

	while (!(__raw_readl(0x481D8230) & 0x020F0001))
	//while (!(__raw_readl(0x481D8230) & 0x00010000))
	{
		if (count == 1000)
		{
			printf("Command Timeout\n");
			
			val = __raw_readl(0x481D8224);
			printf("The SD_PSTATE: %x\n", val);
			return;
		}

		count++;
		udelay(1000);
	}
	//printf("Command Failed\n");
}


/*!
 * @brief Command configuration
 *
 * @param instance     Instance number of the uSDHC module.
 * @param cmd          The command to be configured
 * 
 */
static void sdhc_cmd_cfg(command_t *cmd)
{
	unsigned int cmd0 = 0;
	unsigned int cmd2 = 0;
	unsigned int cmd3 = 0;

	unsigned int val = 0;

	/* Write Command Argument in Command Argument Register */
	__raw_writel(cmd->arg, 0x481D8208);

	/* Clear the DMAS field */
//	val = __raw_readl(0x481D8228) & ~0x00000018;
//	__raw_writel(val, 0x481D8228);

	cmd0 = __raw_readl(0x481D820C) & ~0x3FFB0037;
	cmd0 = (cmd0 | ( ((cmd->dma_enable) << BP_SDHC_CMD_DE) |
		 ((cmd->block_count_enable_check) << BP_SDHC_CMD_BCE) |
	 	 ((cmd->acmd12_enable) << BP_SDHC_CMD_ACEN) |
		 ((cmd->data_transfer) << BP_SDHC_CMD_DDIR) |
		 ((cmd->multi_single_block) << BP_SDHC_CMD_MSBS)));

	printf("cmd0 = %x\n", cmd0);
	//__raw_writeb(cmd0, 0x481D820C);

	//cmd2 = __raw_readl(0x481D820C) & ~0x3FFB0000;
	cmd2 = (cmd0 |
		((cmd->response_format) << BP_SDHC_CMD_RSP_TYP) |
		((cmd->crc_check) << BP_SDHC_CMD_CCCE) |
		((cmd->cmdindex_check) << BP_SDHC_CMD_CICE) |
		((cmd->data_present) << BP_SDHC_CMD_DP)	|
		((cmd->command) << BP_SDHC_CMD_INDX));

	//__raw_writew(cmd2, 0x481D820E);
	printf("cmd2 = %x\n", cmd2);

	//cmd3 = cmd2 | cmd0;
	__raw_writel(cmd2, 0x481D820C);

	//printf("cmd3 = %x\n", cmd3);
	//__raw_writeb(cmd0, 0x481D820C);
	//cmd3 = (cmd->command) << BP_SDHC_CMD_INDX;
	//__raw_writeb(cmd3, 0x481D820F);
}

/*!
 * @brief Wait for command inhibit(CMD) and command inhibit(DAT) idle for 
 * issuing next SD/MMC command. 
 *
 * @param instance     Instance number of the uSDHC module.
 * @param data_present check command inhibit(DAT)
 * 
 * @return             0 if successful; 1 otherwise
 */
static int sdhc_wait_cmd_data_lines(int data_present)
{
	int count = ZERO;

	/* Wait for release of CMD line */
	while(__raw_readl(0x481D8224) & 0x00000001)
	{
		if (count == 10)
			return FAIL;
	
		count++;
		udelay(1000);
	}

	/* If data present with command, wait for release of Data lines */
	if (data_present == DATA_PRESENT)
	{
		count = ZERO;

		while(__raw_readl(0x481D8224) & 0x00000002)
		{
			if (count == 10)
				return FAIL;

			count++;
			udelay(1000);
		}
	}

	return SUCCESS;
}
 

/*!
 * @brief uSDHC Controller sends command
 *
 * @param instance     Instance number of the uSDHC module.
 * @param cmd          the command to be sent
 * 
 * @return             0 if successful; 1 otherwise
 */
int host_send_cmd(command_t * cmd)
{
	unsigned int val = 0;

	/* Clear Interrupt status register */
//	val = __raw_readl(0x481D8230) & ~0x037F01FF;
//	val |= 0x037F01FF;

	/* Enable Interrupt */
//	val = __raw_readl(0x481D8234) & ~0x007F013F;
//	val |= 0x007F013F;

	/* Wait for CMD/DATA lines to be free */
	if (sdhc_wait_cmd_data_lines(cmd->data_present) == FAIL)
	{
		printf("Data/Command lines busy.\n");
		return FAIL;
	}

	/* Clear interrupt status */
//	val = __raw_readl(0x481D8230) | 0x020F0001;
//	__raw_writel(val, 0x481D8230);

	writel(0xFFFFFFFF, 0x481D8230);

	while (readl(0x481D8230))
	{
		udelay(1000);
		printf("timedout waiting for stat to clear\n");	
	}

	
	/*Set appropriate bits in SD_IE register*/
	__raw_writel(0x327f0033, 0x481D8234);
	
	sdhc_cmd_cfg(cmd);

	sdhc_wait_end_cmd_resp_intr();

	/* Mask all interrupts */
//	__raw_writel(0x00000000, 0x481D8238);

	/* Check if an error occured */
	return sdhc_check_response();
}

void host_init_active(void)
{
	unsigned int val = 0;

	/* Send 80 clock ticks for card to power up */
	val = __raw_readl(0x481D812C) & ~0x00000002;
	val |= 0x00000002;
	__raw_writel(val, 0x481D812C);

	/*Write 0x00000000 to SD_CMD register*/
	__raw_writel(0x00000000, 0x481D820C);

	/*Wait for 10ms*/
	udelay(10000);

	/* Set CC bit to 1 in SD_STAT[0] register */
	val = __raw_readl(0x481D8230) & ~0x00000001;
	val |= 0x00000001;
	__raw_writel(val, 0x481D8230);

	/* End initialization sequence */
	val = __raw_readl(0x481D812C) & ~0x00000002;
	__raw_writel(val, 0x481D812C);

	/* Clear SD_STATregister */
	__raw_writel(0xFFFFFFFF, 0x481D8230);

	 /*Wait until command is complete */
	/*(while (!(__raw_readl(0x481D8230) & 0x00000001))
	{
		;
	}*/
}

void host_cfg_clock(int frequency)
{
	unsigned int cap = 0;
	unsigned int val = 0;

	cap = __raw_readl(0x481D8240);
	printf ("The capability register is %u\n", cap);

	/*Enable internal clock*/
	val = __raw_readl(0x481D822C) & ~0x00000001;
	val |= 0x00000001;
	__raw_writel(val, 0x481D822C);

	/*Wait until clock stable*/	
	while(!(__raw_readl(0x481D822C) & 0x00000002))
	{
		;
	}
	printf("Internal clock stable_1\n");

	/*Clear DTO, CLKD and CEN*/
	val = __raw_readl(0x481D822C) & ~0x000FFFC4;
	__raw_writel(val, 0x481D822C);
	
	/*Wait until clock stable*/
	while(!(__raw_readl(0x481D822C) & 0x00000002))
	{
		;
	}
	printf("Internal clock stable_2\n");


	/*Find out base clock freq and find out MHz and KHz*/
	cap = __raw_readl(0x481D8240);
	printf ("The capability register is %u\n", cap);

	/*Set frequency dividers*/
	if (frequency == IDENTIFICATION_FREQ)
	{
		val = __raw_readl(0x481D822C) & ~0x0000FFC0;
		val |=0x000003C0;
		__raw_writel(val, 0x481D822C);		
	}
	else if (frequency == OPERATING_FREQ)
	{
		val = __raw_readl(0x481D822C) & ~0x0000FFC0;
		val |= 0x00000000;
		__raw_writel(val, 0x481D822C);			
	}
	else if (frequency == HS_FREQ)
	{
		val = __raw_readl(0x481D822C) & ~0x0000FFC0;
		val |= 0x00000000;
		__raw_writel(val, 0x481D822C);		
	}
	else if (frequency == INIT_FREQ)
	{
		val = __raw_readl(0x481D822C) & ~0x0000FFC0;
		val |= 0x000FFC00;
		__raw_writel(val, 0x481D822C);		
	}

	/*Wait until clock stable*/
	while(!(__raw_readl(0x481D822C) & 0x00000002))
	{
		;
	}
	printf("Internal clock stable after setting frequency\n");
	
	/*Set Data timeout frequency*/
	val = __raw_readl(0x481D822C) & 0x000F0000;
	val |= 0x0000000E;
	__raw_writel(val, 0x481D822C);

	val = __raw_readl(0x481D822C) & ~0x00000004;
	val |= 0x00000004;
	__raw_writel(val, 0x481D822C);
}

/* SD/MMC bus configuration */
static void host_configure_bus(int dat_width)
{
	unsigned int val = 0;
	unsigned int cap = 0;
	
	cap = __raw_readl(0x481D8240);
	printf ("The capability register is %x\n", cap);
	
	cap = __raw_readl(0x481D8248);
	printf ("The cur capability register is %x\n", cap);

	cap = __raw_readl(0x481D8228);
	printf ("The hctl register is %x\n", cap);
	
	cap = __raw_readl(0x481D823C);
	printf ("The ac12 register is %x\n", cap);
	
	//val = __raw_readl(0x481D8228) & ~0x00000A02;
	/*Set SDVS to 5h*/
	val = 0x00000A00;
	__raw_writel(val, 0x481D8228);
	
	cap = __raw_readl(0x481D8228);
	printf ("The hctl register is %x\n", cap);
	
	/*if ((dat_width == 1) || (dat_width == 4))
	{
		val = __raw_readl(0x481D812C) & ~0x00001021;
		val |= 0x00000001;
		__raw_writel(val, 0x481D812C);
		printf("DW8 and CETA set to zero, OD set to 1\n");
	}*/
	
	/*Set SD_CON register*/
	/* CTPL = 0x0
	 * DVAL = 0x3
	 * WPP  = 0x0
	 * MIT  = 0x0
	 * DW8  = 0x0
	 * MODE = 0x0
	 * STR  = 0x0
	 * HR   = 0x0
	 * INIT = 0x0
	 * OD   = 0x0
	 */
	__raw_writel(0x00000600, 0x481D812C);

	/*Set SD_SYSCTL register*/
	/* DTO  = 0xE 
	 */
	__raw_writel(0x000e0000, 0x481D822C);
	
	/*Set SD_SYSCTL register*/
	/* DTO  = 0xE
	 * CLKD = 0x3C
	 * ICE  = 0x1
	 */
	__raw_writel(0x000e3c01, 0x481D822C);

	while(! (__raw_readl(0x481D822C) & 0x00000002));
	{
		;
	}
	printf("Internal Clock Stable\n");

	/*Set SD_SYSCTL register*/
	/* DTO  = 0xE
	 * CLKD = 0x3C
	 * ICE  = 0x5
	 */
	__raw_writel(0x000e3c05, 0x481D822C);
	
	/*Set SD_HCTL register*/
	/* SDVS  = 0x5
	 * SDBP  = 0x1
	 */
	__raw_writel(0x00000b00, 0x481D8228);
	
	/*Set appropriate bits in SD_IE register*/
	__raw_writel(0x327f0033, 0x481D8234);
	
	/*if(dat_width == 1)
	{	

		val = __raw_readl(0x481D8228) & ~0x00000100;
		val |= 0x00000100;
		__raw_writel(val, 0x481D8228);

		printf("Card voltage power mode and bus width set\n");
	}

	while (!(__raw_readl(0x481D8228) & 0x00000100))
	{
		;
	}
	printf("Power setting correct\n");

	val = __raw_readl(0x481D822C);
	printf("The SD_SYSCTL register is %x\n", val);
	
	val = __raw_readl(0x481D822C) & ~0x00000005;
	val |= 0x00000005;
	__raw_writel(val, 0x481D822C);
	printf("Internal Clock Enabled, Output clock Disabled\n");
	
	val = __raw_readl(0x481D822C) & ~0x0000FFC0;
	val |= 0x00003C00;
	__raw_writel(val, 0x481D822C);
	printf("Frequency <80 KHz set\n");
	
	while(! (__raw_readl(0x481D822C) & 0x00000002));
	{
		;
	}
	printf("Internal Clock Stable\n");
	
	val = __raw_readl(0x481D822C) & ~0x00000004;
	val |= 0x00000004;
	__raw_writel(val, 0x481D822C);
	printf("Output clock Enabled\n");
	
	val = __raw_readl(0x481D8110) & ~0x00000319;
	val |= 0x00000011;
	__raw_writel(val, 0x481D8110);
	printf ("The SYSCONFIG settings done\n");*/

	printf("Bus configuration done\n");
	


	/*if(dat_width == 4)
	{	
		val = __raw_readl(0x481D8228) & ~0x00000002;
		val |= 0x00000002;
		__raw_writel(val, 0x481D8228);
		printf("DTW set to 1\n");
	}
	
	if(dat_width == 8)
	{	
		val = __raw_readl(0x481D812C) & ~0x00000020;
		val |= 0x00000020;
		__raw_writel(val, 0x481D812C);
		printf("DW8 set to 1\n");
	}*/
}

static void sdhc_set_data_transfer_width(int dat_width)
{
	unsigned int val = 0;

	switch (dat_width) {
	case 8:
		val = readl(0x481D812C) | 0x00000020;
		writel(val, 0x481D812C);
		break;

	case 4:
		val = readl(0x481D812C) & ~0x00000020;
		writel(val, 0x481D812C);
		val = readl(0x481D8228) | 0x00000002;
		writel(val, 0x481D8228);
		break;
	
	case 1:
		val = readl(0x481D812C) & ~0x00000020;
		writel(val, 0x481D812C);
		val = readl(0x481D8228) & ~0x00000002;
		writel(val, 0x481D8228);
		break;
	}	
}

void host_set_bus_width(int bus_width)
{
	//int width = bus_width >> ONE;

	sdhc_set_data_transfer_width(bus_width);
}

void host_reset(int bus_width)
{
	unsigned int val = 0;

	/*sysconfig softreset*/
	val = __raw_readl(0x481D8110) | 0x00000002;
	__raw_writel(val, 0x481D8110);

	while (!(__raw_readl(0x481D8114) & 0x00000001))
	{
		;
	}
	
	/*sysctl resetall*/
	val = __raw_readl(0x481D822C) | 0x01000000;
	__raw_writel(val, 0x481D822C);

	while (__raw_readl(0x481D822C) & 0x01000000)
	{
		;
	}

	printf("Software reset done\n");

	host_configure_bus(bus_width);
	//host_set_bus_width(bus_width);	
}
