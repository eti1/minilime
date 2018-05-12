#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <lime/LimeSuite.h>
#include "dev.h"

#define CONF_PATH "limesdr.ini"
using namespace std;

static lms_device_t* device = NULL; //Device structure, should be initialize to NULL
static lms_stream_t sc;
static int stream_started = 0;

static int lms_error(void)
{
	cout << "ERROR:" << LMS_GetLastErrorMessage();
	if (device != NULL)
	{
		dev_cleanup();
	}
	exit(1);
}

void dev_set_rx_freq(double freq)
{
	if(LMS_SetLOFrequency(device, LMS_CH_RX, 0, freq) != 0)
	{
		lms_error();
	}
}

void dev_set_samplerate(double sr, size_t oversample=1)
{
	if(LMS_SetSampleRate(device, sr, oversample) != 0)
	{
		lms_error();
	}
}

void dev_get_config(void)
{
	double gain, freq, lpfbw_cur;
	unsigned txrx, chan, numchan, numant, ant, curant;
	char rtc;
	lms_name_t ant_list[10];
	lms_range_t range, lpfbw_range;

	for (txrx=0;txrx<2;txrx++)
	{
		rtc = "RT"[txrx];
		if((numchan=LMS_GetNumChannels(device,txrx))==(unsigned)-1
			||LMS_GetLOFrequencyRange(device, txrx, &range)==-1
			||-1==(LMS_GetLPFBWRange(device, txrx, &lpfbw_range))
		){
			lms_error();
		}
		printf("%cX: %d channel%s\n",
			rtc, numchan, numchan>1?"s":"");
		printf(" LO Freq range (Mhz): %.1f - %.1f, step=%.1f\n",
			range.min/1e6, range.max/1e6, range.step/1e6);
		printf(" LPF BW (MHz): range: %f - %f, step=%f\n",
			lpfbw_range.min/1e6, lpfbw_range.max/1e6, lpfbw_range.step/1e6);
		for(chan=0;chan<numchan;chan++)
		{
			printf("  channel %d:\n", chan);

			if(-1==LMS_GetNormalizedGain(device, txrx, chan, &gain)
			||-1==LMS_GetLOFrequency(device, txrx, chan, &freq)
			||(unsigned)-1==(numant=LMS_GetAntennaList(device, txrx, chan, ant_list))
			||(unsigned)-1==(curant=LMS_GetAntenna(device, txrx,chan))
			||-1==(LMS_GetLPFBW(device, txrx, chan, &lpfbw_cur))
			){
				lms_error();
			}

			printf("    LO Frequency: %f\n", freq);
			printf("    LPF BW (Mhz): %f\n", lpfbw_cur/1e6);
			printf("    Normalized gain: %f\n", gain);
			printf("    Antennas:\n");
			for(ant=0;ant<numant;ant++)
			{
				printf(	"      %s%s:\n", ant_list[ant],ant==curant?" (current)":"");
				if (-1==LMS_GetAntennaBW(device, txrx, chan, ant, &range))
					lms_error();
				printf(	"        range (Mhz): %.1f - %.1f, step=%.1f\n", range.min/1e6,range.max/1e6,range.step/1e6);
			}
		}
	}
}

int dev_setup_rx(float freq, float bw, unsigned chan=0, unsigned osr=1, bool lpf = true, float gain = 0.7)
{
	size_t ant_idx;

	/* Enable channel 0 */
	if(-1==LMS_EnableChannel(device, LMS_CH_RX, chan, true))
		lms_error();
	
	/* Configure LO Frequency */ 
	if(LMS_SetLOFrequency(device, LMS_CH_RX, chan, freq) != 0)
	{
		lms_error();
	}

	/* Configure sample rate*/
	if(LMS_SetSampleRate(device, bw, osr) != 0)
	{
		lms_error();
	}

	if (lpf)
	{
		/* Configure Low-Pass Filter */
		if(	LMS_SetLPFBW(device, LMS_CH_RX, chan, bw) != 0
		||	LMS_SetLPF(device, LMS_CH_RX, chan, true) != 0
		){
			lms_error();
		}
	}

	/* Configure antenna path (FIXME ?)*/
	ant_idx = (freq > 2e9) ? 2 : 1;
	if (LMS_SetAntenna(device, LMS_CH_RX, chan, ant_idx) != 0)
	{
		lms_error();
	}

	/* Configure gain */
	if (LMS_SetNormalizedGain(device, LMS_CH_RX, chan, gain) != 0)
	{
		lms_error();
	}

	/* Calibrate device */
#if 1
	if (LMS_Calibrate(device, LMS_CH_RX, chan, bw, 0) != 0)
	{
		lms_error();
	}
#endif
	printf("Device configured\n");

	return 0;
}

int dev_open(void)
{
	int n;
	struct stat st;

	lms_info_str_t list[8];
	if ((n=LMS_GetDeviceList(list)) < 0)
	{
		lms_error();
	}
	if (!n)
	{
		printf("No device found.\n");
		return 1;
	}
	else if (n > 1)
	{
		printf("%d devices found. Selecting first.\n", n);
	}
	if (LMS_Open(&device, list[0], NULL))
	{
		lms_error();
	}
	if (-1 == stat(CONF_PATH, &st))
	{
		printf("Saving config\n");
		if(0!=LMS_SaveConfig(device, CONF_PATH))
			lms_error();
	}
	else
	{
		printf("Loading config\n");
		if (LMS_LoadConfig(device, CONF_PATH) != 0)
			lms_error();
	}
	printf("Device opened\n");
	return 0;
	
}

int dev_cleanup(void)
{
	if (stream_started)
	{
		LMS_StopStream(&sc);
	}
	printf("Closing device\n");
	return LMS_Close(device);
}

void dev_stream_rx(rx_func_t hdlr)
{
	int rcv;
	int16_t buffer[2*DEV_BUFCOUNT];

	sc.channel = 0;							// channel number
	sc.fifoSize = 1024*1024;				// fifo sample count
	sc.throughputVsLatency = 1.0;			// Optimize throughput
	sc.isTx = false;						// RX channel
	sc.dataFmt = lms_stream_t::LMS_FMT_I12;	// 12 bit integers

	if (LMS_SetupStream(device, &sc) != 0)
	{
		lms_error();
	}

	/* start streaming */
	if (LMS_StartStream(&sc)!= 0)
	{
		lms_error();
	}
	stream_started = 1;

	for(;;)
	{
		if((rcv = LMS_RecvStream(&sc, buffer, DEV_BUFCOUNT, NULL, 1000))==-1)
		{
			lms_error();
		}
		//printf("Readen %d samples\n", rcv);
		if (hdlr(buffer, rcv))
		{
			LMS_StopStream(&sc);
			return;
		}
	}
}