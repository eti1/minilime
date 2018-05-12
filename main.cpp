#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include "dev.h"

int out_fd = -1;

void open_out(char *name)
{
	if ((out_fd = open(name, O_WRONLY|O_CREAT, 0666))==-1)
	{
		printf("Cannot write %s\n", name);
		dev_cleanup();
		exit(1);
	}
}

int write_outbuf(int16_t *inp, size_t cnt)
{
	static float buf[2*DEV_BUFCOUNT];
	size_t i;

	for (i=0;i<cnt*2&&i<DEV_BUFCOUNT*2;i++)
	{
		buf[i] = (float)inp[i];
	}
	write(2, ".", 1);
	fsync(2);
	write(out_fd, buf, sizeof(float)*cnt*2);
	return 0;
}

void (hdlint)(int __attribute__((unused)) n)
{
	printf("Interrupted.\n");
	close(out_fd);
	dev_cleanup();
	exit(0);
}

int main()
{
	double freq, bw, gain;
	unsigned chan, osr;
	bool lpf;

	chan = 0;
	freq = 935e6;
	bw = 20e6;
	lpf = 1;
	osr = 1;
	gain = 0.3;
	
	signal(2, hdlint);
	open_out((char*)"/tmp/out.iq");
	dev_open();
	dev_setup_rx(freq, bw, chan, osr, lpf, gain);
	dev_get_config();

	dev_stream_rx(write_outbuf);
//	close(out_fd);
	dev_cleanup();

	return 0;
}
