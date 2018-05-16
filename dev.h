#ifndef DEF_DEV_H
#define DEF_DEV_H
#include <stdint.h>
#include <stdlib.h>

typedef int (*rx_func_t)(void* sample_buf, size_t count);

void dev_set_rx_freq(double freq);
int dev_cleanup(void);
int dev_open(void);
int dev_setup_rx(double freq, double bw, unsigned chan, unsigned osr, double *lpf, double gain);
void dev_stream_rx(rx_func_t hdlr, bool use_float);
void dev_get_config(void);
int dev_save_config(char *path);

#define DEV_BUFCOUNT 8192

#endif
