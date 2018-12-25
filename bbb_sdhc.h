#ifndef __SDHC_H__
#define __SDHC_H__
/*------------------------------------------- Macros --------------------------------------------*/

#define SDHC_ONE_BIT_SUPPORT	0x00000001

#define ZERO 0
#define ONE 1
#define FOUR 4
#define EIGHT 8

#define NO_ARG 0
#define RCA_SHIFT 16
#define CURR_CARD_STATE(r) ((int) ((r) & 0x1E00) >> 9)

#define BYTE_MODE 0
#define SECT_MODE 1

#define BLK_LEN 512

#define CARD_BUSY_BIT 0x80000000
#define SDHC_FIFO_LENGTH (0x80)

/* MMC Defines */
#define MMC_SWITCH_SETBW_ARG(bus_width) (unsigned int)(0x03b70001 | ((bus_width >> 2) << 8))
#define MMC_HV_HC_OCR_VALUE 0x40FF8000
#define MMC_OCR_HC_RESP_VAL 0x40000000
#define MMC_OCR_HC_BIT_MASK 0x60000000

#define MMC_VOLT_VALID_COUNT 3000
#define MMC_VOLT_VALID_DELAY 300

/* SD Defines */
#define SD_VOLT_VALID_COUNT 3000
#define SD_VOLT_VALID_DELAY 300

#define SD_R1_STATUS_APP_CMD_MSK 0x20

#define SD_OCR_VALUE_HV_LC 0x00ff8000
#define SD_OCR_VALUE_HV_HC 0x40ff8000
#define SD_OCR_VALUE_LV_HC 0x40000080
#define SD_OCR_HC_RES      0x40000000
#define SD_IF_HV_COND_ARG  0x000001AA
#define SD_IF_LV_COND_ARG  0x000002AA

#define SD_OCR_VALUE_COUNT  3
#define SD_IF_CMD_ARG_COUNT 2

#define BP_SDHC_CMD_DE		(0)
#define BM_SDHC_CMD_DE		(0x00000001)

#define BP_SDHC_CMD_BCE		(1)
#define BM_SDHC_CMD_BCE		(0x00000002)

#define BP_SDHC_CMD_ACEN	(2)
#define BM_SDHC_CMD_ACEN	(0x00000004)

#define BP_SDHC_CMD_DDIR	(4)
#define BM_SDHC_CMD_DDIR	(0x00000010)

#define BP_SDHC_CMD_MSBS	(5)
#define BM_SDHC_CMD_MSBS	(0x00000020)

#define BP_SDHC_CMD_RSP_TYP	(16)
#define BM_SDHC_CMD_RSP_TYP	(0x00030000)

#define BP_SDHC_CMD_CCCE	(19)
#define BM_SDHC_CMD_CCCE	(0x00080000)

#define BP_SDHC_CMD_CICE	(20)
#define BM_SDHC_CMD_CICE	(0x00100000)

#define BP_SDHC_CMD_DP		(21)
#define BM_SDHC_CMD_DP		(0x00200000)

#define BP_SDHC_CMD_CMD_TYPE	(22)
#define BM_SDHC_CMD_CMD_TYPE	(0x00C00000)

#define BP_SDHC_CMD_INDX	(24)
#define BM_SDHC_CMD_INDX	(0x3F000000)

/*------------------------------------------- Enumerations --------------------------------------------*/
enum card_type_e {
    CARD_SD = 0,
    CARD_MMC = 1,
    CARD_ESD = 2,
    CARD_EMMC = 3
};

enum commands {
    CMD0 = 0,
    CMD1 = 1,
    CMD2 = 2,
    CMD3 = 3,
    CMD5 = 5,
    CMD6 = 6,
    ACMD6 = 6,
    CMD7 = 7,
    CMD8 = 8,
    CMD9 = 9,
    CMD12 = 12,
    CMD13 = 13,
    CMD16 = 16,
    CMD17 = 17,
    CMD18 = 18,
    CMD24 = 24,
    CMD25 = 25,
    CMD26 = 26,
    CMD32 = 32,
    CMD33 = 33,
    CMD35 = 35,
    CMD36 = 36,
    CMD37 = 37,
    CMD38 = 38,
    CMD39 = 39,
    ACMD41 = 41,
    CMD43 = 43,
    ACMD51 = 51,
    CMD55 = 55,
    CMD60 = 60,
    CMD61 = 61,
    CMD62 = 62,
};

enum states {
    IDLE,
    READY,
    IDENT,
    STBY,
    TRAN,
    DATA,
    RCV,
    PRG,
    DIS
};

typedef enum {
    SINGLE = 0,
    MULTIPLE = 1
} multi_single_block_select;

typedef enum {
    DISABLE = 0,
    ENABLE = 1
} crc_check_enable, cmdindex_check_enable, block_count_enable, ddren_enable;

typedef enum {
    DATA_PRESENT_NONE = 0,
    DATA_PRESENT = 1
} data_present_select;

typedef enum {
    RESPONSE_NONE,
    RESPONSE_136,
    RESPONSE_48,
    RESPONSE_48_CHECK_BUSY
} response_format_t;

typedef enum {
	OPERATING_FREQ,
	IDENTIFICATION_FREQ,
	HS_FREQ,
	INIT_FREQ
} sdhc_freq_t;

typedef enum {
    WRITE = 0,
    READ = 1,
    SD_COMMAND = 2
} xfer_type_t;

typedef struct {
    unsigned int command;
    unsigned int arg;
    xfer_type_t data_transfer;
    response_format_t response_format;
    data_present_select data_present;
    crc_check_enable crc_check;
    cmdindex_check_enable cmdindex_check;
    block_count_enable block_count_enable_check;
    multi_single_block_select multi_single_block;
    unsigned int dma_enable;
    unsigned int acmd12_enable;
    ddren_enable ddren;
} command_t;

typedef struct {
    response_format_t format;
    unsigned int cmd_rsp0;
    unsigned int cmd_rsp1;
    unsigned int cmd_rsp2;
    unsigned int cmd_rsp3;
} command_response_t;

typedef struct {
    unsigned int reg_base;      //register base address
    unsigned int adma_ptr;      //ADMA buffer address
    void (*isr) (void);         //interrupt service routine

    unsigned short rca;         //relative card address
    unsigned char addr_mode;    //addressing mode
    unsigned char intr_id;      //interrupt ID
    unsigned char status;       //interrupt status
} sdhc_inst_t;

/* uSDHC device table */
extern sdhc_inst_t sdhc_device;

extern int card_emmc_init(void);
extern void card_cmd_config(command_t * cmd, int index, int argument, xfer_type_t transfer,
			    response_format_t format, data_present_select data,
			    crc_check_enable crc, cmdindex_check_enable cmdindex);
extern int card_get_cid(void);
extern int card_enter_trans(void);
extern int card_trans_status(void);
extern int card_data_read(int *dst_ptr, int length, uint32_t offset);

#endif
