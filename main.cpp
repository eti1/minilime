#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <string.h>
#include <getopt.h>
#include "dev.h"

static int out_fd = -1;
static unsigned long num_samples = 0;
static bool use_float = false;

void open_out(char *name)
{
	if ((out_fd = open(name, O_WRONLY|O_CREAT|O_TRUNC, 0666))==-1)
	{
		printf("Cannot write %s\n", name);
		dev_cleanup();
		exit(1);
	}
}

int write_outbuf(void *inp, size_t cnt)
{
	static unsigned long samples_count = 0;
	unsigned sample_size = 2*(use_float?sizeof(float):sizeof(int16_t));

	write(out_fd, inp, sample_size*cnt);

	if (num_samples != 0 && (samples_count += cnt) >= num_samples)
	{
		return 1;
	}
	return 0;
}

void (hdlint)(int __attribute__((unused)) n)
{
	printf("Interrupted.\n");
	close(out_fd);
	dev_cleanup();
	exit(0);
}

static struct option long_options[] = {
	{"frequency",	required_argument, 0, 'f'}, 
	{"samplerate",	required_argument, 0, 's'}, 
	{"gain",	required_argument, 0, 'g'}, 
	{"num-samples",	required_argument, 0, 'n'}, 
	{"output-file",	required_argument, 0, 'o'}, 
	{"decimation",	required_argument, 0, 'd'}, 
	{"lpf-bandwith",	required_argument, 0, 'l'}, 
	{"config-output", required_argument, 0, 'c'}, 
	{"int16",	no_argument, 0, 'I'}, 
	{"help",	no_argument, 0, 'h'}, 
};


#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(*(a)))
#endif
void usage(char*s)
{
	unsigned i;
	printf("Usage: %s [options]\n", s);
	for(i=0;i<ARRAY_SIZE(long_options);i++)
	{
		printf("  -%c --%s\n", long_options[i].val, long_options[i].name);
	}
	exit(1);
}

int main(int argc, char**argv)
{
	double freq, bw, gain, lpf_bw;
	unsigned chan, osr;
	bool lpf = false;
	char c;
	char *filename = (char*)"/tmp/out.iq";
	char *conf_path = NULL;

	chan = 0;
	freq = 935e6;
	bw = 20e6;
	osr = 1;
	gain = 0.5;

	while ((c = getopt_long(argc, argv, "f:s:g:n:o:d:l:c:Ih", long_options, NULL)) != -1)
	{
		switch(c)
		{
		case 'f':
			freq = strtod(optarg, NULL);
			break;
		case 's':
			bw = strtod(optarg, NULL);
			break;
		case 'o':
			filename = strdup(optarg);
			break;
		case 'g':
			gain = strtod(optarg, NULL);
			break;
		case 'l':
			lpf_bw = strtod(optarg, NULL);
			lpf = true;
			break;
		case 'd':
			osr = strtol(optarg, NULL, 10);
			break;
		case 'I':
			use_float = false;
			break;
		case 'n':
			num_samples = (unsigned long)strtod(optarg, NULL);
			break;
		case 'c':
			conf_path = strdup(optarg);
			break;
		case '?': case 'h':
			usage(*argv);
			break;
		default:
			break;
		}
	}
	if(optind != argc)
	{
		usage(*argv);
	}

	if (bw < 0 || osr * bw > 30.72e6 || osr*bw <= 0)
	{
		printf("Invalid samplerate specified\n");
		return 1;
	}
	if (lpf_bw < 0 || lpf_bw > bw)
	{
		printf("Invalid low-pass filter bandwidth\n");
		return 1;
	}
	if (freq < 100e6 || freq > 3.8e9)
	{
		printf("Frequency not in range 100Mhz-3.8Ghz\n");
		return 1;
	}

	printf("freq: %.f, bw: %.f, o: %s, g: %f, n: %ld\n",
		freq,bw,filename,gain,num_samples);

	signal(2, hdlint);
	open_out(filename);
	dev_open();
	dev_setup_rx(freq, bw, chan, osr, lpf ? &lpf_bw: NULL, gain);
	dev_get_config();
	if (conf_path)
	{
		dev_save_config(conf_path);
		printf("Config saved to %s\n", conf_path);
		free(conf_path);
	}

	dev_stream_rx(write_outbuf, use_float);
	close(out_fd);
	dev_cleanup();

	return 0;
}
