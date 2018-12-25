#ifndef __SDHC_HOST_H__
#define __SDHC_HOST_H__

#define ESDHC_BLKATTR_WML_BLOCK       (0x80)

int host_data_read(int *dst_ptr, int length, int wml);
void host_read_response(command_response_t *response);
int host_send_cmd(command_t * cmd);
void host_init_active(void);
void host_cfg_clock(int frequency);
void host_set_bus_width(int bus_width);
void host_reset(int bus_width);
void host_cfg_block(int blk_len, int nob);

#endif
