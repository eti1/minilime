#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <string.h>
#include <getopt.h>
#include "dev.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(*(a)))
#endif

static int out_fd = -1;
static unsigned long num_samples = 0;
static bool use_float = false;

void open_out(char *name)
{
	if (!strcmp(name,"-"))
		out_fd = 1; 
	else if ((out_fd = open(name, O_WRONLY|O_CREAT|O_TRUNC, 0666))==-1)
	{
		fprintf(stderr, "Cannot write %s\n", name);
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
	fprintf(stderr, "Interrupted.\n");
	close(out_fd);
	dev_cleanup();
	exit(0);
}

static struct option long_options[] = {
	{"num-samples",	required_argument, 0, 'n'}, 
	{"output-cfile",	required_argument, 0, 'o'}, 
	{"save-config", required_argument, 0, 's'}, 
	{"load-config",	no_argument, 0, 'l'}, 
	{"int16",	no_argument, 0, 'I'}, 
	{"get-config",	no_argument, 0, 'G'}, 
	{"rxargs",	no_argument, 0, 'R'}, 
	{"txargs",	no_argument, 0, 'T'}, 
	{"help",	no_argument, 0, 'h'}, 
};

void usage(char*s)
{
	unsigned i;
	fprintf(stderr, "Usage: %s -R/-T freq=2400e6,sr=10e6,lpf=8e6,osr=1,gain=30 [options]\n", s);
	for(i=0;i<ARRAY_SIZE(long_options);i++)
	{
		fprintf(stderr, "  -%c --%s\n", long_options[i].val, long_options[i].name);
	}
	exit(1);
}

int main(int argc, char**argv)
{
	unsigned chan=0;
	char c;
	char *filename = (char*)"/dev/null";
	char *conf_path_out = NULL, *conf_path_in = NULL;
	int get_config = 0, do_sampling = 0;
	unsigned do_rx=0, do_tx=0;
	dev_arg_t rx, tx, *args;

	while ((c = getopt_long(argc, argv, "hn:o:s:l:IGR:T:", long_options, NULL)) != -1)
	{
		switch(c)
		{
		case 'o':
			filename = strdup(optarg);
			break;
		case 'I':
			use_float = false;
			break;
		case 'n':
			do_sampling = 1;
			num_samples = (unsigned long)strtod(optarg, NULL);
			break;
		case 's':
			conf_path_out = strdup(optarg);
			break;
		case 'l':
			conf_path_in = strdup(optarg);
			break;
		case 'G':
			get_config = 1;
			break;
		case 'R':case 'T':
			if(c=='R')
			{
				do_rx=1;
				args=&rx;
			}
			else
			{
				do_tx=1;
				args=&tx;
			}
			if(dev_parse_args(optarg, args))
			{
				usage(*argv);
			}
			
			fprintf(stderr, "%cX args: freq: %.f, bw: %.f, gain: %d, lpf: %f, osr: %d\n",
				c,args->frequency,args->samplerate,args->gain, args->lpfbw, args->osr);
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
	if(do_tx&&do_rx)
	{
		if (tx.samplerate != rx.samplerate)
		{
			fprintf(stderr, "RX & TX Samplerates must match\n");
			return 1;
		}
	}

	signal(2, hdlint);
	if(do_sampling)
		open_out(filename);

	if (dev_open(conf_path_in))
		goto end;

	if(do_tx&&do_rx)
		dev_setup_both(&rx,&tx);
	else{
		if(do_tx)
			dev_setup_tx(tx.frequency, tx.samplerate, chan, tx.osr, tx.lpfbw ? &tx.lpfbw:NULL, tx.gain);
		if(do_rx)
			dev_setup_rx(rx.frequency, rx.samplerate, chan, rx.osr, rx.lpfbw ? &rx.lpfbw:NULL, rx.gain);
	}

	if (get_config)
	{
		dev_get_config();
	}
	if (conf_path_out)
	{
		dev_save_config(conf_path_out);
		fprintf(stderr, "Config saved to %s\n", conf_path_out);
		free(conf_path_out);
	}

	if (do_sampling)
	{
		dev_stream_rx(write_outbuf, use_float);
	}
end:
	if(do_sampling )
		close(out_fd);
	dev_cleanup();

	return 0;
}
