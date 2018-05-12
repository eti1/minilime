#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <string.h>
#include <getopt.h>
#include "dev.h"

using namespace std;

static int out_fd = -1;
static unsigned long num_samples = 0;

void open_out(char *name)
{
	if ((out_fd = open(name, O_WRONLY|O_CREAT|O_TRUNC, 0666))==-1)
	{
		printf("Cannot write %s\n", name);
		dev_cleanup();
		exit(1);
	}
}

int write_outbuf(int16_t *inp, size_t cnt)
{
	static unsigned long samples_count = 0;
	static float buf[2*DEV_BUFCOUNT];
	unsigned mx = DEV_BUFCOUNT*2;
	size_t i;

	if (cnt < DEV_BUFCOUNT)
		mx = 2*cnt;

	for (i=0;i<mx;i++)
	{
		buf[i] = (float)inp[i]/2048. ;
	}
	write(out_fd, buf, sizeof(float)*cnt*2);

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
	{"output-file",	required_argument, 0, 'o'}, 
	{"gain",	required_argument, 0, 'g'}, 
	{"num-samples",	required_argument, 0, 'n'}, 
	{"lpf-bandwith",	required_argument, 0, 'l'}, 
	{"decimation",	required_argument, 0, 'd'}, 
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

	chan = 0;
	freq = 935e6;
	bw = 20e6;
	osr = 1;
	gain = 0.5;

	while ((c = getopt_long(argc, argv, "f:s:g:n:o:d:l:h", long_options, NULL)) != -1)
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
		case 'n':
			num_samples = (unsigned long)strtod(optarg, NULL);
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

	dev_stream_rx(write_outbuf);
	close(out_fd);
	dev_cleanup();

	return 0;
}
