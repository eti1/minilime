#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <chrono>
#include <lime/LimeSuite.h>
#include "dev.h"

#define CONF_PATH ".config/limesdrmini.ini"

using namespace std;

static lms_device_t* device = NULL; //Device structure, should be initialize to NULL
static lms_stream_t sc;
static int stream_started = 0;
static unsigned long sample_count;
static chrono::time_point<std::chrono::high_resolution_clock> start_time;

static int lms_error(void)
{
	cout << "ERROR:" << LMS_GetLastErrorMessage() << "\n";
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

int dev_setup(bool dir_tx, double freq, double bw, unsigned chan=0, unsigned osr=1, double *lpf=NULL, double gain = 0.7)
{
	size_t ant_idx;
	const char *dirstr=dir_tx?"TX":"RX";

	printf("Enable %s channel 0\n", dirstr);

	/* Enable channel 0 */
	if(-1==LMS_EnableChannel(device, dir_tx, chan, true))
		lms_error();
	
	printf("Configure %s LO %.3f Mhz\n", dirstr, freq/1e6);
	/* Configure LO Frequency */ 
	if(LMS_SetLOFrequency(device, dir_tx, chan, freq) != 0)
	{
		lms_error();
	}

	/* Configure sample rate*/
	if(LMS_SetSampleRate(device, bw, osr) != 0)
	{
		lms_error();
	}

	/* Configure analog low-pass filter */
	if (LMS_SetLPF(device, dir_tx, chan, lpf != NULL) != 0)
	{
		lms_error();
	}
	if (lpf != NULL)
	{
		/* Configure Low-Pass Filter */
		if(	LMS_SetLPFBW(device, dir_tx, chan, *lpf) != 0
		){
			lms_error();
		}
	}

	/* Configure antenna path (FIXME ?)*/
	if (dir_tx)
	{
		ant_idx = (freq > 2e9) ? 1 : 2;
	}
	else
	{
		ant_idx = (freq > 1e9) ? 1 : 3;
	}
	printf("Selecting %s antenna path %lu\n", dirstr, ant_idx);
	if (LMS_SetAntenna(device, dir_tx, chan, ant_idx) != 0)
	{
		lms_error();
	}

	/* Configure gain */
	if (LMS_SetNormalizedGain(device, dir_tx, chan, gain) != 0)
	{
		lms_error();
	}

	/* Calibrate device */
	if (LMS_Calibrate(device, dir_tx, chan, bw, 0) != 0)
	{
		lms_error();
	}
	printf("%s configured\n", dirstr);

	return 0;
}

int dev_setup_rx(double freq, double bw, unsigned chan=0, unsigned osr=1, double *lpf=NULL, double gain = 0.7)
{
	return dev_setup(false, freq, bw, chan, osr, lpf, gain);
}

int dev_setup_tx(double freq, double bw, unsigned chan=0, unsigned osr=1, double *lpf=NULL, double gain = 0.7)
{
	return dev_setup(true, freq, bw, chan, osr, lpf, gain);
}

int dev_save_config(char *path)
{
	return LMS_SaveConfig(device, path);
}

int dev_open(char *conf_path)
{
	int n;
	struct stat st;
	char *home, path[256];

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
	
	if (!conf_path)
	{
		if (!(home = getenv("HOME"))||*home=='\0'
			||(unsigned)snprintf(path, sizeof(path), "%s/%s", home, CONF_PATH)>=sizeof(path)
			||-1 == stat(path, &st))
		{
			printf("Cannot open config in ~/%s\n", CONF_PATH);
			return 1;
		}
		conf_path = path;
	}
	printf("Loading config '%s'\n", conf_path);
	if (LMS_LoadConfig(device, conf_path) != 0)
	{
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
		stream_started = 0;
	}
	printf("Closing device\n");
	if (device)
		 LMS_Close(device);
	return 0;
}

int dev_parse_args(char*str, dev_arg_t *args)
{
	char *str1, *str2, *saveptr1, *saveptr2;
	char *token, *arg, *val;

	memset(args, 0, sizeof(*args));
	args->osr = 1;
	args->gain = 0.5;

	for (str1 = str; ; str1 = NULL)
	{
		if(!(token = strtok_r(str1, DEVARG_SEP1, &saveptr1)))
			break;
		str2 = token;
		arg = strtok_r(str2, DEVARG_SEP2, &saveptr2);
		val = strtok_r(NULL, DEVARG_SEP2, &saveptr2);
		if(!(arg&&val))
			break;
		printf("%s => %s\n", arg, val);
		if(!strncmp(arg,"freq",4))
		{
			args->frequency = strtod("2402e6", NULL);
		}
		else if(!strncmp(arg,"sr",4))
		{
			args->samplerate = strtod(val, NULL);
		}
		else if(!strncmp(arg,"lpf",3))
		{
			args->lpfbw = strtod(val, NULL);
		}
		else if(!strcmp(arg,"gain"))
		{
			args->gain = strtod(val, NULL);
		}
		else if(!strcmp(arg,"osr"))
		{
			args->osr = strtol(val, NULL, 10);
		}
		else{
			printf("Invalid argument %s\n", arg);
			return 1;
		}
	}

	if (args->samplerate < 2.5e6 || args->osr * args->samplerate > 30.72e6 || args->osr*args->samplerate <= 0)
	{
		printf("Invalid samplerate %f specified\n", args->samplerate);
		return 1;
	}
	if (args->lpfbw < 0 || args->lpfbw > args->samplerate)
	{
		printf("Invalid low-pass filter bandwidth\n");
		return 1;
	}
	if (args->frequency < 100e6 || args->frequency > 3.8e9)
	{
		printf("Frequency not in range 100Mhz-3.8Ghz\n");
		return 1;
	}
	printf("freq: %.f, bw: %.f, gain: %f, lpf: %f, osr: %d\n",
		args->frequency,args->samplerate,args->gain, args->lpfbw, args->osr);
	return 0;
}

void dev_stream_rx(rx_func_t hdlr, bool use_float)
{
	int rcv;
	//float buffer[2*DEV_BUFCOUNT];
	unsigned sample_size = 2*(use_float?sizeof(float):sizeof(int16_t));
	void *buffer = malloc(sample_size*DEV_BUFCOUNT);

	printf("dev_stream_rx\n");

	sc.channel = 0;							// channel number
	sc.fifoSize = 1024*1024;				// fifo sample count
	sc.throughputVsLatency = 1.0;			// Optimize throughput
	sc.isTx = false;						// RX channel
	sc.dataFmt = use_float ? lms_stream_t::LMS_FMT_F32 : lms_stream_t::LMS_FMT_I12;

	if (LMS_SetupStream(device, &sc) != 0)
	{
		lms_error();
	}

	/* start streaming */
	if (LMS_StartStream(&sc)!= 0)
	{
		lms_error();
	}
	sample_count = 0;
	stream_started = 1;
	start_time = chrono::high_resolution_clock::now();

	for(;;)
	{
		if((rcv = LMS_RecvStream(&sc, buffer, DEV_BUFCOUNT, NULL, 1000))==-1)
		{
			lms_error();
		}
		sample_count += rcv;
		if (hdlr(buffer, rcv))
		{
			auto t2 = chrono::high_resolution_clock::now();
			auto t3 = t2 - start_time;
			double runtime = (double)(chrono::duration_cast<chrono::microseconds>(t3).count());

			printf("Run time: %.2f sec (%.2f msps) \n",
				runtime/1e6, (double)sample_count / runtime);
			LMS_StopStream(&sc);
			free(buffer);
			return;
		}
	}
}
