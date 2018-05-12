#ifndef DEF_DEV_H
#define DEF_DEV_H
#include <stdint.h>
#include <stdlib.h>

typedef int (*rx_func_t)(int16_t* sample_buf, size_t count);

void dev_set_rx_freq(double freq);
int dev_cleanup(void);
int dev_open(void);
int dev_setup_rx(float freq, float bw, unsigned chan, unsigned osr, bool lpf, float gain);
void dev_stream_rx(rx_func_t hdlr);
void dev_get_config(void);

#define DEV_BUFCOUNT 4096

#endif
