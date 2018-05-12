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
		buf[i] = (float)inp[i];
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
	double freq, bw, gain;
	unsigned chan, osr;
	int optidx = 0;
	bool lpf;
	char c;
	char *filename = (char*)"/tmp/out.iq";

	chan = 0;
	freq = 935e6;
	bw = 20e6;
	lpf = 1;
	osr = 1;
	if (bw < 15e6)
		osr = 2;
	gain = 0.5;

	while ((c = getopt_long(argc, argv, "f:s:g:n:o:", long_options, &optidx)) != -1)
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
		case 'n':
			num_samples = (unsigned long)strtod(optarg, NULL);
			break;
		case '?':
			usage(*argv);
			break;
		default:
			break;
		}
	}

	printf("freq: %.f, bw: %.f, o: %s, g: %.f, n: %ld\n",
		freq,bw,filename,gain,num_samples);

	signal(2, hdlint);
	open_out(filename);
	dev_open();
	dev_setup_rx(freq, bw, chan, osr, lpf, gain);
	dev_get_config();

	dev_stream_rx(write_outbuf);
	close(out_fd);
	dev_cleanup();

	return 0;
}
