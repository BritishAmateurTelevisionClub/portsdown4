#include <stdio.h>
#include <cstring>
#include <lime/LimeSuite.h>
#include "dvb_t.h"
#ifdef __USE_LIME__
#include "lime.h"

//Device structure, should be initialise to NULL
lms_device_t* m_device = NULL;
const lms_dev_info_t *device_info;
lms_stream_t m_streamId; //stream structure
lms_stream_meta_t m_metadata;

int lime_open(DVBTFormat *fmt){
    //Find devices
    int n;

    lms_info_str_t list[8]; //should be large enough to hold all detected devices
    if ((n = LMS_GetDeviceList(list)) > 0) //NULL can be passed to only get number of devices
    {
    	printf("Lime Devices found: %d\n",n); //print number of devices
    	if (n < 1) return -1;

    	//open the first device
    	if (LMS_Open(&m_device, list[0], NULL)){
    		printf("LMS_Open error\n");
    		return -1;
    	}

        device_info = LMS_GetDeviceInfo(m_device);

    	//Initialise device with default configuration
    	//Do not use if you want to keep existing configuration
    	//Use LMS_LoadConfig(device, "/path/to/file.ini") to load config from INI
    	if (LMS_Init(m_device) != 0){
    		printf("LMS_Init error\n");
    		return -1;
    	}

    	//Enable TX channel
    	//Channels are numbered starting at 0
    	if (LMS_EnableChannel(m_device, LMS_CH_TX, 0, true) != 0){
    		printf("LMS_EnableChannel error\n");
    		return -1;
    	}

    	//Set centre frequency to default
    	if (LMS_SetLOFrequency(m_device, LMS_CH_TX, 0, fmt->freq) != 0){
    		printf("LMS_SetLOFrequency error\n");
    		return -1;
    	}
        int ratio = 32;
        if(fmt->tx_sample_rate > 1e6 )  ratio = 16;
        if(fmt->tx_sample_rate > 2e6 )  ratio = 8;
        if(fmt->tx_sample_rate > 4e6 )  ratio = 4;
        if(fmt->tx_sample_rate > 8e6 )  ratio = 2;
        if(fmt->tx_sample_rate > 16e6 ) ratio = 1;

    	if (LMS_SetSampleRate(m_device, fmt->tx_sample_rate, ratio) != 0){
    		printf("LMS_SetSampleRate error\n");
    		return -1;
    	}

//    	if(LMS_SetNormalizedGain(m_device, LMS_CH_TX, 0, 1.0) != 0){  // gkq correction
    	if(LMS_SetNormalizedGain(m_device, LMS_CH_TX, 0, fmt->level) != 0){
    		printf("LMS_SetNormalisedGain error\n");
    		return -1;
    	}

        // Set the correct antenna port
    	int port = 2;
        if (strcmp(device_info->deviceName, "LimeSDR-USB") == 0)
        {
          if(fmt->freq < 2e9) port = 1;
        }
        else                    // Lime SDR Mini
        {
          if(fmt->freq > 2e9) port = 1;
        }
    	if(LMS_SetAntenna(m_device,LMS_CH_TX,0,port)!= 0){
    		printf("LMS_SetAntenna error\n");
    		return -1;
    	}

    	// Set up the GPIO
    	uint8_t buffer;

    	LMS_GPIODirRead( m_device, &buffer, 1);// Read the directions
    	// Set all the GPIO Pins to output
    	buffer = 0xFF;
    	LMS_GPIODirWrite(m_device, &buffer, 1);
    	// Set all the outputs to zero
    	buffer = 0;
    	LMS_GPIOWrite(m_device, &buffer, 1);

    	//Streaming Setup

    	//Initialise stream
    	m_streamId.channel = 0; //channel number
    	m_streamId.fifoSize = 1024 * 1024 * 10; //FIFO size in samples
    	m_streamId.throughputVsLatency = 1.0; //optimise for max throughput
    	m_streamId.isTx = true; //TX channel
    	m_streamId.dataFmt = lms_stream_t::LMS_FMT_I16; //16-bit integers

    	m_metadata.flushPartialPacket = false;
    	m_metadata.waitForTimestamp   = false;

    	if (LMS_SetupStream(m_device, &m_streamId) != 0){
    		printf("LMS_SetupStream error\n");
    		return -1;
    	}
    	//Start streaming
    	if(LMS_StartStream(&m_streamId) != 0){
    		printf("LMS_StartStream error\n");
    		return -1;
    	}
    	if(LMS_Calibrate(m_device,LMS_CH_TX,0,2500000,0)!=0){
    		printf("LMS_Calibrate error\n");
    		return -1;
    	}
    }
    return 0;
}
void lime_set_gpio(int num, bool status) {
	uint8_t buffer;
	LMS_GPIORead(m_device, &buffer, 1);

	if (status == true)
		buffer |= (1 << num);
	else
		buffer &= ~(1 << num);

	LMS_GPIOWrite(m_device, &buffer, 1);
}
void lime_close(void){
	LMS_StopStream(&m_streamId); //stream is stopped but can be started again with LMS_StartStream()
	LMS_DestroyStream(m_device, &m_streamId); //stream is deallocated and can no longer be used
	//Close device
	LMS_Close(m_device);
}
void lime_set_gain(float level){
	if(LMS_SetNormalizedGain(m_device, LMS_CH_TX, 0, level) != 0){
		printf("LMS_SetNormalisedGain error\n");
	}
}
void lime_set_freq(uint64_t freq){
	LMS_SetLOFrequency(m_device, LMS_CH_TX, 0, freq);
}
void lime_set_port(int port){
	LMS_SetAntenna(m_device,LMS_CH_TX,0,port);
}
void lime_transmit(void){
	lime_set_gpio(1, true);
}
void lime_receive(void){
	lime_set_gpio(1, false);
}
void lime_transmit_samples( scmplx *in, int len){
	LMS_SendStream(&m_streamId, in, len, &m_metadata, 1000000);
}
#endif
