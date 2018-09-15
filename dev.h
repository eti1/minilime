#ifndef DEF_DEV_H
#define DEF_DEV_H
#include <stdint.h>
#include <stdlib.h>

#define DEV_BUFCOUNT 8192

#define DEVARG_SEP1 ","
#define DEVARG_SEP2 "="

typedef struct dev_arg_s {
	double frequency;
	double samplerate;
	double gain;
	double lpfbw;
	unsigned osr;
} dev_arg_t;

typedef int (*rx_func_t)(void* sample_buf, size_t count);

void dev_set_rx_freq(double freq);
int dev_cleanup(void);
int dev_open(char *conf_path);
int dev_setup_rx(double freq, double bw, unsigned chan, unsigned osr, double *lpf, double gain);
int dev_setup_tx(double freq, double bw, unsigned chan, unsigned osr, double *lpf, double gain);
void dev_stream_rx(rx_func_t hdlr, bool use_float);
void dev_get_config(void);
int dev_save_config(char *path);
int dev_parse_args(char*str, dev_arg_t *args);
int dev_setup_both(dev_arg_t *tx, dev_arg_t *rx);

#endif
