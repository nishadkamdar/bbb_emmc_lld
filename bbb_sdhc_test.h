#ifndef __BBB_SDHC_TEST__
#define __BBB_SDHC_TEST__

//#define MMC_TEST_BUF_SIZE 8000
//#define MMC_TEST_OFFSET   (1024 * 1024)

#define MMC_TEST_BUF_SIZE 128
#define MMC_TEST_OFFSET   1077338


#define MMC_CARD_SECTOR_BUFFER 0x80

typedef struct {
    const char *name;
    int (*test) (void);
} usdhc_test_t;

#endif

