#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <syslog.h>
#include <sys/time.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <fftw3.h>
#include <stdbool.h>
#include <libwebsockets.h>

#include "transport.h"
#include "protocol.h"
#include "websocket_server.h"

#include "meteorview.h"
#include "timing.h"
#include "sdrplay_api.h"
#include "sdrplayfft.h"

#define PI 3.14159265358979323846
#define PATH_RESTARTS "/home/pi/rpidatv/src/meteorview/restart_counter.txt"

int masterInitialised = 0;
int slaveUninitialised = 0;

int callback_count = 0;

sdrplay_api_DeviceT *chosenDevice = NULL;

#define	SDRPLAY_BUFFER_COPY_SIZE 4096
#define SHORT_EL_SIZE_BYTE (2)
//#define FFT_BUFFER_SIZE 4096



extern bool NewFreq;                 // Set to true to indicate that frequency needs changing
extern uint32_t CentreFreq;          // Frequency in Hz from main
extern bool NewSpan;                 // Set to true to indicate that span needs changing
extern bool prepnewscanwidth;
extern bool readyfornewscanwidth;
extern bool app_exit;
extern bool app_exit;
extern void ShowRemoteCaption();
extern void ShowConnectFail();
extern void ShowStartFail();
extern void UpdateWeb();

extern bool NewGain;                 // Set to true to indicate that gain needs changing
//extern float gain;                   // Gain (0 - 21) from main
extern int RFgain;
extern int IFgain;
extern int remoteRFgain;
extern int remoteIFgain;
extern bool agc;
extern bool Range20dB;
extern int BaseLine20dB;
extern int fft_size;                 // Number of fft samples.  Depends on scan width
extern float fft_time_smooth; 
extern uint8_t decimation_factor;           // decimation applied by SDRPlay api
extern int span;
extern uint8_t clientnumber;

extern uint8_t wfalloverlap;
extern uint8_t wfallsamplefraction;

extern bool transport_OK;

extern bool NewData; 
bool debug = false;    

int fft_offset;

uint16_t y3[4096];               // Histogram values in range 0 - 399

int force_exit = 0;

pthread_t fftThread;

extern char destination[15];
extern char rxName[32];
extern int16_t lat_deg;
extern uint16_t lat_min;
extern uint16_t lat_sec;
extern int16_t lon_deg;
extern uint16_t lon_min;
extern uint16_t lon_sec;


double hanning_window_const[4096];

typedef struct {
	uint32_t index;
	uint32_t size;
	int16_t idata[SDRPLAY_BUFFER_COPY_SIZE * SHORT_EL_SIZE_BYTE];
	int16_t qdata[SDRPLAY_BUFFER_COPY_SIZE * SHORT_EL_SIZE_BYTE];
	pthread_mutex_t mutex;
	pthread_cond_t 	signal;
} rf_buffer2_t;

rf_buffer2_t rf_buffer2 = {
	.index = 0,
	.size = 0,
	.mutex = PTHREAD_MUTEX_INITIALIZER,
	.signal = PTHREAD_COND_INITIALIZER,
	.idata = { 0 },
	.qdata = { 0 }
};

#define FLOAT32_EL_SIZE_BYTE (4)
fftw_complex* fft_in;
fftw_complex*   fft_out;
fftw_plan   fft_plan;

pthread_mutex_t histogram;

static const char *fftw_wisdom_filename = ".fftw_wisdom";
static float fft_output_data[4096];

unsigned long long int total = 0;     /* for debug */
unsigned long long int buf_total = 0; /* for debug */

#define DECIMATION 8      /* we need to do a further factor of 8 decimation to get from 80 KSPS to 10 KSPS */
#define CORRECTION_GAIN 10

long int output_buffer_i[FFT_BUFFER_SIZE];
long int output_buffer_q[FFT_BUFFER_SIZE];
unsigned char transmit_buffer[LWS_PRE + FFT_BUFFER_SIZE*2*sizeof(short)];
unsigned char server_buffer[TCP_DATA_PRE + FFT_BUFFER_SIZE*2*sizeof(short)] = {'H'}; 

sdrplay_api_RxChannelParamsT *chParams;
//sdrplay_api_DeviceT devs[6];
//sdrplay_api_DeviceParamsT *deviceParams = NULL;

int k = 0;
int m = 0;

char restarts[63];
static int restart_count;

void fft_to_buffer();
int legal_gain(int demanded_Gain);

void setup_fft(void)
{
  int i;

  // Set up FFTW
  fft_in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * fft_size);
  fft_out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * fft_size);
  i = fftw_import_wisdom_from_filename(fftw_wisdom_filename);

  // Always calculate fft plan
  i = 0;
  if (i == 0)
  {
    printf("Computing fft plan...\n");
  }
  fft_plan = fftw_plan_dft_1d(fft_size, fft_in, fft_out, FFTW_FORWARD, FFTW_EXHAUSTIVE);

  if (i == 0)
  {
    fftw_export_wisdom_to_filename(fftw_wisdom_filename);
  }
}


/* --------------------------------------------------------------------- */
void fft() {
/* --------------------------------------------------------------------- */
/* display the waterfall on the RPI 7" touch screen                      */
/* --------------------------------------------------------------------- */

    /**********************  DAVE's FFT CODE hooks into here **********************/
    /* fft() gets called when there are 1024 points in the above 2 buffers        */
    /* the buffer size is defined by   fft_size  in case 1024 isn't useful */
    /* note the output buffer is long int (int32_t) sized, can be reduced if needed         */
    /******************************************************************************/

  // Called after fft_size samples are in output_buffer_i.
  // each sample is decimated from 8 samples sent by the SDR to the CIC filter

  int i;

  if (strcmp(destination, "local") == 0)
  {
    pthread_mutex_lock(&rf_buffer2.mutex);
    rf_buffer2.index = 0;

    // Set cal true to to calibrate dBfs
    // It generates a sine wave at -40 dB with varying phase
    bool cal = false;
    //float amplitude = 3276.70;
    float amplitude = 327.670;  // -40 dB
    static float foffset = 2.55;
    static float phaseoffset  = 0;
    phaseoffset = phaseoffset + 0.0002;
    foffset = foffset + 0.000001;

    for (i = 0; i < fft_size; i++)
    {
      if (cal == false)
      {
        rf_buffer2.idata[i]  = (int16_t)(output_buffer_i[i]);  // was / 128 then / 256
        rf_buffer2.qdata[i]  = (int16_t)(output_buffer_q[i]);
      }
      else
      {
        rf_buffer2.idata[i]  = (int16_t)(amplitude * sin((((float)i / foffset) + phaseoffset) * 2.f * PI));
        rf_buffer2.qdata[i]  = (int16_t)(amplitude * cos((((float)i / foffset) + phaseoffset) * 2.f * PI));
      }
    }   

//    rf_buffer2.size = SDRPLAY_BUFFER_COPY_SIZE / (fft_size * 2);

    pthread_cond_signal(&rf_buffer2.signal);
    pthread_mutex_unlock(&rf_buffer2.mutex);
  }
}


/* --------------------------------------------------------------------- */
void buffer_send() {
/* --------------------------------------------------------------------- */
/* send the buffer over the internet to the central server               */
/* --------------------------------------------------------------------- */

  int send_check = 1;
  int fail_count = 0;
  if (strcmp(destination, "remote") == 0)
  {
    //printf("TX\n");
    //printf("IN: %li %li %li %li %li %li %li %li\n", output_buffer_i[0], output_buffer_q[0],
    //                                        output_buffer_i[1], output_buffer_q[1],
    //                                        output_buffer_i[1022], output_buffer_q[1022],
    //                                        output_buffer_i[1023], output_buffer_q[1023]);
    //printf("TX: %i %i %i %i %i %i %i %i\n", transmit_buffer[LWS_PRE], transmit_buffer[LWS_PRE+1],
    //                              transmit_buffer[LWS_PRE+2], transmit_buffer[LWS_PRE+3],
    //                              transmit_buffer[LWS_PRE+4092], transmit_buffer[LWS_PRE+4093],
    //                              transmit_buffer[LWS_PRE+4094], transmit_buffer[LWS_PRE+4095]);

    
    while(send_check != 0)
    {
      send_check = transport_send(server_buffer);

      if (send_check != 0)
      {
        fail_count++;
        if (fail_count == 1)                      // First error
        {
          ShowConnectFail();
          UpdateWeb();
        }
        usleep(1000000);
      }
      if ((fail_count > 1) && (send_check == 0))  // recovered from error
      {
        ShowRemoteCaption();
        UpdateWeb();
      }

      if ((fail_count > 20) && (send_check != 0))     // Restart app if still not working after 20 attempts
      {
        GetConfigParam(PATH_RESTARTS, "restarts", restarts);

        restart_count = atoi(restarts);
        if (restart_count > 180)
        {
          printf("too many restarts in buffer_send, rebooting\n");
          SetConfigParam(PATH_RESTARTS, "restarts", "0");  // reset for next time
          usleep(10000);
          //cleanexit(192);                                  // reboot
          system("sudo reboot now");
        }
        else
        {
          restart_count++;
          snprintf(restarts, 60, "%d", restart_count);
          SetConfigParam(PATH_RESTARTS, "restarts", restarts);
          printf("Restarting MeteorViewer for the %d time in buffer_send\n", restart_count);
          cleanexit(150);
        }
      }

      /* now we tell all our clients that we have data ready for them. That way when they  */
      /* come back and say they are writeable, we will be able to send them the new buffer */
      lws_callback_on_writable_all_protocol(context, protocol);
    }
  }
}


int32_t stream_divisor = 4096;
int32_t stream_factor = 1;       // only used in old code without filter, so not used
int32_t ximax;
int32_t ximin;
int32_t iymax;
int32_t iymin;

/* --------------------------------------------------------------------- */
void cic_filter(short xi, short xq, int reset)
{
  static int32_t decimation_cnt;
  static int32_t buf_ptr;

  static int32_t i_int1_out,     i_int2_out,      i_int3_out;
  static int32_t i_comb1_out,    i_comb2_out,     i_comb3_out;
  static int32_t i_int3_out_old, i_comb2_out_old, i_comb1_out_old;
  static int32_t i_y,            i_w1,            i_w2;

  static int32_t q_int1_out,     q_int2_out,      q_int3_out;
  static int32_t q_comb1_out,    q_comb2_out,     q_comb3_out;
  static int32_t q_int3_out_old, q_comb2_out_old, q_comb1_out_old;
  static int32_t q_y,            q_w1,            q_w2;


  if (true)   // true for new filter code, false for old
  {
    /* --------------------------------------------------------------------- */
    /* implementing a 3 stage CIC filter with R = D = 8                      */
    /* for 14 bits in, and decimation by 8, we need 14 + 3.log2(8) = 23 bits */
    /* this needs a long int on the RPI                                      */
    /* the CIC looks like:                                                   */
    /*                                                                       */
    /*   int1 -> int2 -> int3 -> R(down) -> comb1 -> comb2 -> comb3          */
    /*                                                                       */
    /* we will use a simple correction filter of the form:                   */
    /*   w(n)  Z-1    z-1                                                    */
    /*    |     |      |                                                     */
    /*    |    x10     |                                                     */
    /*      -       +   =yn                                                  */
    /* w3 = w(n) = i_comb3_out = output of CIC                               */
    /*                                                                       */
    /* note we have 14 bit input data so we only need to shift down by 8     */
    /*  bits for the CIC to have valid output data (15 bits + sign bit)      */
    /* but the correction filter adds another 4 bits so shift by 12 bits.    */
    /* --------------------------------------------------------------------- */


    if (reset)               // Called by Callback
    {
      printf("Got reset\n"); /* for debugging */
      /* need to reset all the filter delays and the counters */
      decimation_cnt = DECIMATION;
      buf_ptr = 0;
 
      i_int1_out  = 0;
      i_int2_out  = 0;
      i_int3_out  = 0;
      i_comb1_out = 0;
      i_comb2_out = 0;
      i_comb3_out = 0;
      i_int3_out_old  = 0;
      i_comb2_out_old = 0;
      i_comb1_out_old = 0;
      i_y  = 0;
      i_w2 = 0;
      i_w1 = 0;

      q_int1_out  = 0;
      q_int2_out  = 0;
      q_int3_out  = 0;
      q_comb1_out = 0;
      q_comb2_out = 0;
      q_comb3_out = 0;
      q_int3_out_old  = 0;
      q_comb2_out_old = 0;
      q_comb1_out_old = 0;
      q_y  = 0;
      q_w2 = 0;
      q_w1 = 0;
    }
    else                   // Normal operation
    {
      // for efficiency we do the decimation (by factor R) first
      decimation_cnt--;
      if (decimation_cnt == 0) 
      {
        decimation_cnt = DECIMATION;

        // Decimation for i samples
        /* and then the comb filters (work right to left) */
        i_comb3_out     = i_comb2_out - i_comb2_out_old;
        i_comb2_out_old = i_comb2_out;
        i_comb2_out     = i_comb1_out - i_comb1_out_old;
        i_comb1_out_old = i_comb1_out;
        i_comb1_out     = i_int3_out  - i_int3_out_old;
        i_int3_out_old  = i_int3_out;

        /* now we need a compensation filter       */
        i_y  = i_comb3_out - (CORRECTION_GAIN*i_w2) + i_w1;
        i_w1 = i_w2;           /* z-2 */
        i_w2 = i_comb3_out;    /* z-1 */

        // Decimation for Q samples
        /* since we always do the i and q in sync, no need for separate decimation counts or buffer pointers */
        q_comb3_out     = q_comb2_out - q_comb2_out_old;
        q_comb2_out_old = q_comb2_out;
        q_comb2_out     = q_comb1_out - q_comb1_out_old;
        q_comb1_out_old = q_comb1_out;
        q_comb1_out     = q_int3_out  - q_int3_out_old;
        q_int3_out_old  = q_int3_out;
      
        /* now we need a compensation filter       */
        q_y  = q_comb3_out - (CORRECTION_GAIN*q_w2) + q_w1;
        q_w1 = q_w2;           /* z-2 */
        q_w2 = q_comb3_out;    /* z-1 */

        if (false)  // for debugging set true
        {
          // Print peak values of xi for debugging
          if (xi > ximax)
          {
            ximax = xi;
            printf("Xi Min  = %d, Xi Max = %d\n", ximin, ximax);
          }
          if (xi < ximin)
          {
            ximin = xi;
            printf("Xi Min  = %d, Xi Max = %d\n", ximin, ximax);
          }

          // Print peak values of I in output buffer for debugging for debugging
          if ((i_y / stream_divisor) > iymax)
          {
            iymax = i_y / stream_divisor;
            printf("i_y min = %d, i_y max = %d\n", iymin, iymax);
          }
          if ((i_y / stream_divisor) < iymin)
          {
            iymin = i_y / stream_divisor;
            printf("i_y min = %d, i_y max = %d\n", iymin, iymax);
          }

          // Shout if there is an overflow in the conversion to short
          if ((i_y / stream_divisor) != (short)(i_y / stream_divisor))
          {
            printf("ERROR i_y = %d, sent = %i\n", i_y, (short)(i_y / stream_divisor));
          }
          if ((q_y / stream_divisor) != (short)(q_y / stream_divisor))
          {
            printf("ERROR q_y = %d, sent = %i\n", q_y, (short)(q_y / stream_divisor));
          }
        }

        // Set the Output Buffer values used by the local display
        output_buffer_i[buf_ptr] = (short)(i_y / stream_divisor);
        output_buffer_q[buf_ptr] = (short)(q_y / stream_divisor);

        // Set the Server buffer values to be sent to the central server
        *(short *)(server_buffer + TCP_DATA_PRE + (2 * 2 * buf_ptr) + 2) = (short)(i_y / stream_divisor);
        *(short *)(server_buffer + TCP_DATA_PRE + (2 * 2 * buf_ptr))     = (short)(q_y / stream_divisor);

        // Set the transmit buffer values that would be used for local streaming
        //*(short *)(transmit_buffer + LWS_PRE + (2 * 2*buf_ptr) + 2) = (short)(i_y / stream_divisor);
        //*(short *)(transmit_buffer + LWS_PRE + (2 * 2*buf_ptr))     = (short)(q_y / stream_divisor);

        // if we have filled the output buffer, then we can output it to the local FFT and the internet server
        buf_ptr++;
        if (buf_ptr == FFT_BUFFER_SIZE)   // 1024 for server
        {
          fft();                      // Send to local display
          buffer_send();              // Send to server
          buf_ptr = 0;
        }
      }                               // End of decimation loop

      // for efficiency we do the integrators last, again right to left, so that we don't overwrite any values
      i_int3_out = i_int2_out   + i_int3_out;
      i_int2_out = i_int1_out   + i_int2_out;
      i_int1_out = (uint32_t)xi + i_int1_out;

      q_int3_out = q_int2_out   + q_int3_out;
      q_int2_out = q_int1_out   + q_int2_out;
      q_int1_out = (uint32_t)xq + q_int1_out;
    }
  }
  else  // old filter code
  {

    if (reset) {

        printf("Got reset\n"); /* for debugging */
        /* need to reset all the filter delays and the counters */
        decimation_cnt = DECIMATION;
        buf_ptr = 0;
  
        i_int1_out  = 0;
        i_int2_out  = 0;
        i_int3_out  = 0;
        i_comb1_out = 0;
        i_comb2_out = 0;
        i_comb3_out = 0;
        i_int3_out_old  = 0;
        i_comb2_out_old = 0;
        i_comb1_out_old = 0;

        q_int1_out  = 0;
        q_int2_out  = 0;
        q_int3_out  = 0;
        q_comb1_out = 0;
        q_comb2_out = 0;
        q_comb3_out = 0;
        q_int3_out_old  = 0;
        q_comb2_out_old = 0;
        q_comb1_out_old = 0;

    } else {

        /* for efficiency we do the decimation (by factor R) first */
        decimation_cnt--;
        if (decimation_cnt == 0) {
            decimation_cnt = DECIMATION;

            /* and then the comb filters (work right to left) */
            i_comb3_out     = i_comb2_out - i_comb2_out_old;
            i_comb2_out_old = i_comb2_out;
            i_comb2_out     = i_comb1_out - i_comb1_out_old;
            i_comb1_out_old = i_comb1_out;
            i_comb1_out     = i_int3_out  - i_int3_out_old;
            i_int3_out_old  = i_int3_out;

            /* finally we have a data point to send out so off it goes */
            output_buffer_i[buf_ptr]   = i_comb3_out;

            /* and we fill a transmit buffer with interleaved I,Q samples */
            *(short *)(transmit_buffer + LWS_PRE      + (2 * 2*buf_ptr)+2) = i_comb3_out / stream_factor;
            *(short *)(server_buffer   + TCP_DATA_PRE + (2 * 2*buf_ptr)+2) = i_comb3_out / stream_factor;

            /* since we always do the i and q in sync, no need for separate deimation counts or buffer pointers */
            q_comb3_out     = q_comb2_out - q_comb2_out_old;
            q_comb2_out_old = q_comb2_out;
            q_comb2_out     = q_comb1_out - q_comb1_out_old;
            q_comb1_out_old = q_comb1_out;
            q_comb1_out     = q_int3_out  - q_int3_out_old;
            q_int3_out_old  = q_int3_out;
            
            output_buffer_q[buf_ptr]     = q_comb3_out;
            *(short *)(transmit_buffer + LWS_PRE      + (2 * 2*buf_ptr)) = q_comb3_out / stream_factor;  
            *(short *)(server_buffer   + TCP_DATA_PRE + (2 * 2*buf_ptr)) = q_comb3_out / stream_factor;

            /* if we have filled the output buffer, then we can output it to the FFT and the internet */
            buf_ptr++;
            if (buf_ptr == FFT_BUFFER_SIZE) {
                fft();
                buffer_send();
                buf_ptr = 0;
            }
        }

        /* for efficiency we do the integrators last, again right to left, so that we don't overwrite any values */
        i_int3_out = i_int2_out   + i_int3_out;
        i_int2_out = i_int1_out   + i_int2_out;
        i_int1_out = (long int)xi / 1  + i_int1_out;

        q_int3_out = q_int2_out   + q_int3_out;
        q_int2_out = q_int1_out   + q_int2_out;
        q_int1_out = (long int)xq / 1 + q_int1_out;
    }

  }
}



/* --------------------------------------------------------------------- */
/* the data comes into this callback and we feed it out to the CIC       */
/* and then on to the FFT for display and the internet for server access */
/* --------------------------------------------------------------------- */

/* --------------------------------------------------------------------- */
void StreamACallback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params,
                     unsigned int numSamples, unsigned int reset, void *cbContext) {
/* --------------------------------------------------------------------- */
/* the data comes into this callback and we feed it out to the CIC       */
/* and then on to the FFT for display and the internet for server access */
/* --------------------------------------------------------------------- */
	short *p_xi, *p_xq;
        int count;

        if (reset) {
            printf("sdrplay_api_StreamACallback: numSamples=%d\n", numSamples);
            cic_filter(0,0, true);
        }

        /* note that the API decimation means that we only get 84 bytes each callback                */
	/* We have already done API decimation to get from 2.56 MSPS down to 2.56/32 = 80 KSPS       */
        /* so now we need to do the CIC filtering to get to an audio data stream of 10 kSPS          */
        /* we do this by doing a CIC filter on each I and Q seperately, using factor of 8 decimation */
        total += numSamples;  /* for debug purposes */
        p_xi = xi;
        p_xq = xq;
        
        for (count=0; count<numSamples; count++) {
            /* we may need to reset our CIC fiter to provide a good starting point */
            //non_filter(*p_xi, *p_xq, false);
            cic_filter(*p_xi, *p_xq, false);
            p_xi++; /* pointer maths ... ugggy but efficient */
            p_xq++;
        }
        return;
}


void StreamBCallback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params, unsigned int numSamples, unsigned int reset, void *cbContext)
{
  if (reset) printf("sdrplay_api_StreamBCallback: numSamples=%d\n", numSamples);
  // Process stream callback data here - this callback will only be used in dual tuner mode
  return;
}


void EventCallback(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner, sdrplay_api_EventParamsT *params, void *cbContext)
{
  switch(eventId)
  {
    case sdrplay_api_GainChange:
      printf("sdrplay_api_EventCb: %s, tuner=%s gRdB=%d lnaGRdB=%d systemGain=%.2f\n", "sdrplay_api_GainChange",
				(tuner == sdrplay_api_Tuner_A)? "sdrplay_api_Tuner_A":"sdrplay_api_Tuner_B",
				params->gainParams.gRdB, params->gainParams.lnaGRdB,params->gainParams.currGain);
      break;
    case sdrplay_api_PowerOverloadChange:
      printf("sdrplay_api_PowerOverloadChange: tuner=%s powerOverloadChangeType=%s\n",
				(tuner == sdrplay_api_Tuner_A)? "sdrplay_api_Tuner_A": "sdrplay_api_Tuner_B",
				(params->powerOverloadParams.powerOverloadChangeType == sdrplay_api_Overload_Detected) ? "sdrplay_api_Overload_Detected":"sdrplay_api_Overload_Corrected");
			// Send update message to acknowledge power overload message received
			sdrplay_api_Update(chosenDevice->dev, tuner, sdrplay_api_Update_Ctrl_OverloadMsgAck,sdrplay_api_Update_Ext1_None);
      break;
    case sdrplay_api_RspDuoModeChange:
      printf("sdrplay_api_EventCb: %s, tuner=%s modeChangeType=%s\n",
				"sdrplay_api_RspDuoModeChange", (tuner == sdrplay_api_Tuner_A)?
				"sdrplay_api_Tuner_A": "sdrplay_api_Tuner_B",
				(params->rspDuoModeParams.modeChangeType == sdrplay_api_MasterInitialised)?
				"sdrplay_api_MasterInitialised":
				(params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveAttached)?
				"sdrplay_api_SlaveAttached":
				(params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveDetached)?
				"sdrplay_api_SlaveDetached":
				(params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveInitialised)?
				"sdrplay_api_SlaveInitialised":
				(params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveUninitialised)?
				"sdrplay_api_SlaveUninitialised":
				(params->rspDuoModeParams.modeChangeType == sdrplay_api_MasterDllDisappeared)?
				"sdrplay_api_MasterDllDisappeared":
				(params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveDllDisappeared)?
				"sdrplay_api_SlaveDllDisappeared": "unknown type");
			if (params->rspDuoModeParams.modeChangeType == sdrplay_api_MasterInitialised)
				masterInitialised = 1;
			if (params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveUninitialised)
				slaveUninitialised = 1;
      break;
    case sdrplay_api_DeviceRemoved:
      printf("sdrplay_api_EventCb: %s\n", "sdrplay_api_DeviceRemoved");
      break;
    default:
      printf("sdrplay_api_EventCb: %d, unknown event\n", eventId);
    break;
  }
}


static void close_fftw(void)
{
    /* De-init fftw */
    fftw_free(fft_in);
    fftw_free(fft_out);
    fftw_destroy_plan(fft_plan);
    fftw_forget_wisdom();
}


/* transfer->sample_count is normally 65536 */
#define	AIRSPY_BUFFER_COPY_SIZE	65536

typedef struct {
	uint32_t index;
	uint32_t size;
	char data[AIRSPY_BUFFER_COPY_SIZE * FLOAT32_EL_SIZE_BYTE];
	pthread_mutex_t mutex;
	pthread_cond_t 	signal;
} rf_buffer_t;

rf_buffer_t rf_buffer = {
	.index = 0,
	.size = 0,
	.mutex = PTHREAD_MUTEX_INITIALIZER,
	.signal = PTHREAD_COND_INITIALIZER,
	.data = { 0 }
};


typedef struct {
	float data[4096];
	pthread_mutex_t mutex;
} fft_buffer_t;

fft_buffer_t fft_buffer = {
	.mutex = PTHREAD_MUTEX_INITIALIZER,
};

// FFT Thread
void *thread_fft(void *dummy)
{
  (void) dummy;
  int i;
  int offset;
  uint8_t pass;
  fftw_complex pt;
  double pwr;
  double lpwr;
  double pwr_scale;
  float fft_circ[8182][2];
  int circ_write_index = 0;
  int circ_read_pos = 0;
  int window_open;
  int window_close;

  pwr_scale = 1.0 / ((float)fft_size * (float)fft_size);       // App is restarted on change of fft size

  while (true)
  {
    // Calculate the time definition window
    window_open = fft_size / 2 - fft_size / (2 * wfallsamplefraction);
    window_close = fft_size / 2 + fft_size / (2 * wfallsamplefraction);
    if(debug)
    {
      printf("Window open at %d, Window Close at %d\n", window_open, window_close);
    }

    for(pass = 0; pass < 2 * wfalloverlap; pass++)           // Go round this loop twice the overlap, once round the overlap for each input write
    {
      if ((pass == 0) || (pass == wfalloverlap))          // So this pass needs an input write
      {
        // Lock input buffer
        pthread_mutex_lock(&rf_buffer2.mutex);

        // Wait for signalled input
        pthread_cond_wait(&rf_buffer2.signal, &rf_buffer2.mutex);

        if (pass == 0)                            // First and odd numbered input writes
        {
          circ_write_index = 1;
        }
        else
        {
          circ_write_index = 0;
        }
          

        // offset = rf_buffer2.index * fft_size * 2;
        offset = 0;

        // Copy data out of rf buffer into fft_circ buffer in alternate stripes
        for (i = 0; i < fft_size; i++)
        {
          //fft_circ[i + circ_write_index * fft_size][0] = (float)(rf_buffer2.idata[offset+(i)]) * hanning_window_const[i];
          //fft_circ[i + circ_write_index * fft_size][1] = (float)(rf_buffer2.qdata[offset+(i)]) * hanning_window_const[i];
          fft_circ[i + circ_write_index * fft_size][0] = (float)(rf_buffer2.idata[offset+(i)]);
          fft_circ[i + circ_write_index * fft_size][1] = (float)(rf_buffer2.qdata[offset+(i)]);
        }

        if (debug)
        {
          printf("Write stripe %d\n", circ_write_index);
        }

	    rf_buffer2.index++;

	    // Unlock input buffer
        pthread_mutex_unlock(&rf_buffer2.mutex);
      }                                                       // End of input write

      // Add delay between overlap passes
      if ((pass != 0) && (pass != wfalloverlap))
      {
        usleep((int)((80000 * fft_size) / (wfalloverlap * 1000)));  
      }

      //printf("delay = %d \n", (int)((80000 * fft_size) / (wfalloverlap * 1000)));

      // Now read it out of the buffer 
      for (i = 0; i < fft_size; i++)
      {
        circ_read_pos = i + fft_size * pass / wfalloverlap;
        if (circ_read_pos > 2 * fft_size)
        {
        circ_read_pos = circ_read_pos - 2 * fft_size;
        }

        fft_in[i][0] = fft_circ[circ_read_pos][0] * hanning_window_const[i];
        fft_in[i][1] = fft_circ[circ_read_pos][1] * hanning_window_const[i];

        // debug printing block
        if ((i == 0) && (debug))
        {
          printf("pass %d: ", pass);
        }
        if (((i == 0) || (i == fft_size / 4 - 1 ) || (i == fft_size / 4) || (i == fft_size / 2 - 1 )
          || (i == fft_size / 2) || (i == fft_size * 3 / 4 - 1 ) || (i == fft_size * 3 / 4)) && (debug))
        {
          printf("%d - ", circ_read_pos);
        }
        if ((i == fft_size - 1) && (debug))
        {
          printf("%d\n", circ_read_pos);
        }

        // Apply input time window for increased time definition
        if ((i < window_open) || (i > window_close))
        {
          fft_in[i][0] = 0;
          fft_in[i][1] = 0;
        }
	  }

      if(debug)
      {
        printf("Window open at %d, Window Close at %d\n", window_open, window_close);
      }

      // Run FFT
      fftw_execute(fft_plan);

      // Take the output of the fft, convert it to dB and smooth it.

      // Lock output buffer
      pthread_mutex_lock(&fft_buffer.mutex);

      for (i = 0; i < fft_size; i++)
	  {
	    // shift, normalize and convert to dBFS
	    if (i < fft_size / 2)
	    {
	      pt[0] = fft_out[fft_size / 2 + i][0] / fft_size;
	      pt[1] = fft_out[fft_size / 2 + i][1] / fft_size;
	    }
	    else
	    {
          pt[0] = fft_out[i - fft_size / 2][0] / fft_size;
	      pt[1] = fft_out[i - fft_size / 2][1] / fft_size;
	    }

	    pwr = pwr_scale * ((pt[0] * pt[0]) + (pt[1] * pt[1]));

        lpwr = 10.f * log10(pwr + 1.0e-20);

        fft_buffer.data[i] = (lpwr * (1.f - fft_time_smooth)) + (fft_buffer.data[i] * fft_time_smooth);
      }

      // Unlock output buffer
      pthread_mutex_unlock(&fft_buffer.mutex);

      fft_to_buffer();

    }    // End of the read cycle
  }
  return NULL;
}


// Scale and manage the fft data
void fft_to_buffer()
{
  int32_t j;
  uint32_t average = 0;

  // Lock FFT output buffer for reading
  pthread_mutex_lock(&fft_buffer.mutex);

  for (j = 0; j < fft_size; j++)
  {
    fft_output_data[j] = fft_buffer.data[j];
  }

  // Unlock FFT output buffer
  pthread_mutex_unlock(&fft_buffer.mutex);

  // Calculate the centre of the fft
  // for 512, 1024 2048 etc = fft_size / 2 - 256
  // Was for 500, 1000, 2000 = fft_size / 2 - 250  // 500 -> -7, 1000 -> 250 - 7, 2000 750 - 7
  // Now for 500, 1000, 2000 = fft_size / 2 - 250  // 500 -> -7, 1000 -> 250 - 7, 2000 750 - 7
  if (fft_size == 500)
  {
    fft_offset = -7;
  }
  if (fft_size == 512)
  {
    fft_offset = 0;
  }
  if (fft_size == 1000)
  {
    fft_offset = 243;
  }
  if (fft_size == 1024)
  {
    fft_offset = 256;
  }
  if (fft_size == 2000)
  {
    fft_offset = 743;
  }
  if (fft_size == 2048)
  {
    fft_offset = 768;
  }
  if (fft_size == 4096)
  {
    fft_offset = 1792;
  }


  // Scale and limit the samples
  for(j = 0; j < fft_size; j++)
  {
    // Add a constant to position the baseline
    fft_output_data[j] = fft_output_data[j] + 70.0;   // for 0 dB  = - 20 dBfs
//    fft_output_data[j] = fft_output_data[j] + 75.0;
//    fft_output_data[j] = fft_output_data[j] + 185.0;

    // Multiply by 5 (pixels per dB on display)
    fft_output_data[j] = fft_output_data[j] * 5;

    if (Range20dB) // Range20dB
    {
                
      fft_output_data[j] = fft_output_data[j] - 5 * (80 + BaseLine20dB);  
      fft_output_data[j] = 4 * fft_output_data[j];
    }

    if (fft_output_data[j] > 399)
    {
      //printf("fft output = %f\n", fft_output_data[j] );
      fft_output_data[j] = 399;
    }
    if (fft_output_data[j] < 1)
    {
      //printf("fft output = %f\n", fft_output_data[j] );
      fft_output_data[j] = 1;
    }
    average = average + fft_output_data[j];
  }
  if(debug)
  {
    printf("Average = %d\n", average);
  }

  // y3 needs valid values from [6] to [506]
  // Only [7] through [505] are displayed (499 columns)
  // [6] is lowest cal line
  // [256] is middle cal line
  // [506] is highest cal line

  // Lock the histogram buffer for writing
  pthread_mutex_lock(&histogram);
  
  y3[6] = fft_output_data[7 + fft_offset];

  for (j = 7; j <= 505; j++)
  {
    y3[j] = fft_output_data[j + fft_offset];
    //printf("%d\n", j + fft_offset);
  }

  y3[506] = fft_output_data[505 + fft_offset];

  // Unlock the histogram buffer
  pthread_mutex_unlock(&histogram);

  // Wait here until data has been read
  NewData = true;
  while (NewData == true)
  {
    usleep(100);
  }
}

// Takes demanded gain and returns a valid gain for the device
int legal_gain(int demanded_Gain)
{
  if (chosenDevice->hwVer == SDRPLAY_RSP1_ID)
  {
    if (demanded_Gain > 3)
    {
      return 3;
    }
    else
    {
      return demanded_Gain;
    }
  }
  if (chosenDevice->hwVer == SDRPLAY_RSP1A_ID)
  {
    if (demanded_Gain > 6)
    {
      return 6;
    }
    else
    {
      return demanded_Gain;
    }
  }
  if (chosenDevice->hwVer == SDRPLAY_RSP2_ID)
  {
    if (demanded_Gain > 4)
    {
      return 4;
    }
    else
    {
      return demanded_Gain;
    }
  }
  if (chosenDevice->hwVer == SDRPLAY_RSPduo_ID)
  {
    if (demanded_Gain > 4)
    {
      return 4;
    }
    else
    {
      return demanded_Gain;
    }
  }
  if (chosenDevice->hwVer == SDRPLAY_RSPdx_ID)
  {
    if (demanded_Gain > 18)
    {
      return 18;
    }
    else
    {
      return demanded_Gain;
    }
  }
  return 0;
}


// Main thread
void *sdrplay_fft_thread(void *arg) {

  sdrplay_api_DeviceT devs[6];
  unsigned int ndev;
  int i;
  float ver = 0.0;
  sdrplay_api_ErrT err;
  sdrplay_api_DeviceParamsT *deviceParams = NULL;
  sdrplay_api_CallbackFnsT cbFns;
  sdrplay_api_RxChannelParamsT *chParams;
  int reqTuner = 0;
  int master_slave = 0;
  unsigned int chosenIdx = 0;
  uint64_t last_output;
  bool *exit_requested = (bool *)arg;
  int transport_started = 1;
  int transport_start_attempts = 0;



  // Define the Data header (which includes Lat/Long, RX Freq and Text description here
  // Byte 0       : 'H', packet alignment byte (as per old system)
  // Byte 1       : Latitude Degrees + 90, 0 = south pole, 180 = north pole
  // Byte 2       : Lat Minutes, 0 to 59
  // Byte 3       : Lat Seconds, 0 to 59
  // Byte 4       : most sig 8 bits of Longitude Degrees + 180 i.e.   byte 4 = (uint8_t)(  (  (uint16_t)(lon_deg + 180)  )  >> 1   )
  // Byte 5       : LSB of Lon Degrees in MSB then lower 6 bits are Minutes i.e byte 5 = (uint8_t)(  ((uint16_t)(lon_deg + 180) & 0x100) >> 1  ) + lon_min
  // Byte 6       : Lon Seconds
  // Byte 7       : Freq in KHz (most sig byte of 3 byte value i.e byte 7 = (uint8_t)(freq_khz >> 16)
  // Byte 8       : client number (as per old system)
  // Byte 9       : Freq in KHz (middle byte) i.e  byte 9 = (uint8_t)(freq_khz >> 8)
  // Byte 10     : Freq in KHz (lower byte) i.e  byte 9 = (uint8_t)(freq_khz)
  // Byte 11 to 31:  21 ASCII characters for name of station (with spaces for padding) e.g. "Norman Lockyer Obs.  "

  // Use extern uint32_t CentreFreq;          // Frequency in Hz from main

  // Destination is: unsigned char server_buffer[TCP_DATA_PRE + FFT_BUFFER_SIZE*2*sizeof(short)]= {'H','e','a','t','h','e','r','\0'};

  //extern int16_t lat_deg;
  //extern uint16_t lat_min;
  //extern uint16_t lat_sec;
  //extern int16_t lon_deg;
  //extern uint16_t lon_min;
  //extern uint16_t lon_sec;


  uint32_t freq_khz;
  freq_khz = CentreFreq / 1000;

  server_buffer[0] = 'H';
  server_buffer[1] = (uint8_t)(lat_deg + 90);
  server_buffer[2] = (uint8_t)(lat_min);
  server_buffer[3] = (uint8_t)(lat_sec);
  server_buffer[4] = (uint8_t)(((lon_deg + 180)) >> 1);
  server_buffer[5] = (uint8_t)(((uint16_t)(lon_deg + 180) & 0x0001) << 7) + lon_min;
  server_buffer[6] = (uint8_t)(lon_sec);
  server_buffer[7] = (uint8_t)(freq_khz >> 16);
  server_buffer[8] = clientnumber;
  server_buffer[9] = (uint8_t)(freq_khz >> 8);
  server_buffer[10] = (uint8_t)(freq_khz);

  for (i = 11; i <=31; i++)
  {
    server_buffer[i] = rxName[i - 11];
  }

  // Print out diagnostics:
  //printf("\nData Header\n\n");
  //uint8_t hbyte;
  //for (i = 0; i < 32; i++)
  //{
  //  hbyte = (uint8_t)(server_buffer[i]);
  //  if ((hbyte >= 32) && (hbyte <= 126))
  //  {
  //    printf("Byte %d: Character: %c, Value: %d\n", i, hbyte, hbyte);
  //  }
  //  else
  //  {
  //    printf("Byte %d: No Character, Value: %d\n", i, hbyte);
  //  }
  //
  //("\n");


  while (transport_started != 0)
  {
    transport_started =  transport_init();

    if (transport_started != 0)  // failed to start
    {
      transport_close();
      transport_start_attempts++;
      if (transport_start_attempts == 1)
      {
        ShowStartFail();  // show message
        UpdateWeb();
      }
      usleep(1000000);
      if (transport_start_attempts > 10)
      {
        GetConfigParam(PATH_RESTARTS, "restarts", restarts);

        restart_count = atoi(restarts);
        if (restart_count > 200)
        {
          printf("too many restarts in sdrplay_fft_thread, rebooting\n");
          SetConfigParam(PATH_RESTARTS, "restarts", "0");  // reset for next time
          usleep(10000);
          //cleanexit(192);                                  // reboot
          system("sudo reboot now");
        }
        else
        {
          restart_count++;
          snprintf(restarts, 60, "%d", restart_count);
          SetConfigParam(PATH_RESTARTS, "restarts", restarts);
          printf("Restarting MeteorViewer for the %d time in sdrplay_fft_thread\n", restart_count);
          cleanexit(150);
        }
      }
    }
    else                          // Started OK
    {
      transport_OK = true;
      if (transport_start_attempts > 0)  // Had previously failed
      {
        SetConfigParam(PATH_RESTARTS, "restarts", "0");  // reset for next time
        printf("Writing reset restart attempts to file\n");
        ShowRemoteCaption();
        UpdateWeb();
      }
      transport_start_attempts = 0;
    }
  }

  // Initialise fft
  printf("Initialising FFT (%d bin).. \n", fft_size);
  setup_fft();
  for (i = 0; i < fft_size; i++)
  {
    hanning_window_const[i] = 0.5 * (1.0 - cos(2*M_PI*(((double)i)/fft_size)));
  }
  printf("FFT Intitialised\n");

  printf("requested Tuner=%c Mode=%s\n", (reqTuner == 0)? 'A': 'B', (master_slave == 0)?
		"Single_Tuner": "Master/Slave");

  // Open API
  if ((err = sdrplay_api_Open()) != sdrplay_api_Success)
  {
    printf("sdrplay_api_Open failed %s\n", sdrplay_api_GetErrorString(err));
    cleanexit(150);
  }
  else
  {
    // Enable debug logging output
    //if ((err = sdrplay_api_DebugEnable(NULL, 1)) != sdrplay_api_Success)
    if ((err = sdrplay_api_DebugEnable(NULL, 0)) != sdrplay_api_Success)    // Disable to prevent logs filling
    {
      printf("sdrplay_api_DebugEnable failed %s\n", sdrplay_api_GetErrorString(err));
    }
	
    // Check API versions match
    if ((err = sdrplay_api_ApiVersion(&ver)) != sdrplay_api_Success)
    {
      printf("sdrplay_api_ApiVersion failed %s\n", sdrplay_api_GetErrorString(err));
    }
    if (ver != SDRPLAY_API_VERSION) {
      printf("API version don't match (local=%.2f dll=%.2f)\n", SDRPLAY_API_VERSION, ver);
      // goto CloseApi;
    }
 
    // Lock API while device selection is performed
    sdrplay_api_LockDeviceApi();

    // Fetch list of available devices
    if ((err = sdrplay_api_GetDevices(devs, &ndev, sizeof(devs) / sizeof(sdrplay_api_DeviceT))) != sdrplay_api_Success)
    {
      printf("sdrplay_api_GetDevices failed %s\n", sdrplay_api_GetErrorString(err));
      cleanexit(150);
    }

    // Exit if no devices found
    if (ndev == 0)
    {
      printf("No devices found.  Exiting\n");
      cleanexit(150);
    }

    printf("MaxDevs=%d NumDevs=%d\n", sizeof(devs) / sizeof(sdrplay_api_DeviceT), ndev);
    if (ndev > 0)
    {
      for (i = 0; i < (int)ndev; i++)
      {
        if (devs[i].hwVer == SDRPLAY_RSPduo_ID)
        {
          printf("Dev%d: SerNo=%s hwVer=%d tuner=0x%.2x rspDuoMode=0x%.2x\n", i,devs[i].SerNo, devs[i].hwVer , devs[i].tuner, devs[i].rspDuoMode);
		}
		else
        {
          printf("Dev%d: SerNo=%s hwVer=%d tuner=0x%.2x\n", i, devs[i].SerNo,devs[i].hwVer, devs[i].tuner);
        }
      }
		
      // Choose device
      if ((reqTuner == 1) || (master_slave == 1))  // requires RSPduo
      {
        // Pick first RSPduo
        for (i = 0; i < (int)ndev; i++)
        {
          if (devs[i].hwVer == SDRPLAY_RSPduo_ID)
          {
            chosenIdx = i;
            break;
          }
        }
      }
      else
      {
        // Pick first device of any type
        for (i = 0; i < (int)ndev; i++)
        {
          chosenIdx = i;
          break;
        }
      }
      if (i == ndev)
      {
        printf("Couldn't find a suitable device to open - exiting\n");
        //goto UnlockDeviceAndCloseApi;
      }
      printf("chosenDevice = %d\n", chosenIdx);
      chosenDevice = &devs[chosenIdx];
 
      // If chosen device is an RSPduo, assign additional fields
      if (chosenDevice->hwVer == SDRPLAY_RSPduo_ID)
      {
        // If master device is available, select device as master
        if (chosenDevice->rspDuoMode & sdrplay_api_RspDuoMode_Master)
        {
          // Select tuner based on user input (or default to TunerA)
          chosenDevice->tuner = sdrplay_api_Tuner_A;
          if (reqTuner == 1)
          {
            chosenDevice->tuner = sdrplay_api_Tuner_B;
          }
          // Set operating mode
          if (!master_slave)
          {
            // Single tuner mode
            chosenDevice->rspDuoMode = sdrplay_api_RspDuoMode_Single_Tuner;
            printf("Dev%d: selected rspDuoMode=0x%.2x tuner=0x%.2x\n", chosenIdx,chosenDevice->rspDuoMode, chosenDevice->tuner);
          }
          else
          {
            chosenDevice->rspDuoMode = sdrplay_api_RspDuoMode_Master;
            // Need to specify sample frequency in master/slave mode
            chosenDevice->rspDuoSampleFreq = 6000000.0;
            printf("Dev%d: selected rspDuoMode=0x%.2x tuner=0x%.2x rspDuoSampleFreq=%.1f\n",
              chosenIdx, chosenDevice->rspDuoMode,
              chosenDevice->tuner, chosenDevice->rspDuoSampleFreq);
          }
        }
        else
        {
          // Only slave device available
          // Shouldn't change any parameters for slave device
        }
      }
 
      // Select chosen device
      if ((err = sdrplay_api_SelectDevice(chosenDevice)) != sdrplay_api_Success)
      {
        printf("sdrplay_api_SelectDevice failed %s\n", sdrplay_api_GetErrorString(err));
        //goto UnlockDeviceAndCloseApi;
      }
		
      // Unlock API now that device is selected
      sdrplay_api_UnlockDeviceApi();
			
      // Retrieve device parameters so they can be changed if wanted
      if ((err = sdrplay_api_GetDeviceParams(chosenDevice->dev, &deviceParams)) != sdrplay_api_Success)
      {
        printf("sdrplay_api_GetDeviceParams failed %s\n",sdrplay_api_GetErrorString(err));
        cleanexit(150);
      }
			
      // Check for NULL pointers before changing settings
      if (deviceParams == NULL)
      {
        printf("sdrplay_api_GetDeviceParams returned NULL deviceParams pointer\n");
        cleanexit(150);
      }
		
      // Configure dev parameters
      if (deviceParams->devParams != NULL)
      {
        // This will be NULL for slave devices, only the master can change these parameters
        // Only need to update non-default settings
        if (master_slave == 0)
        {
          // We choose to sample at 256 * 10 KSPS to make decimation to 10 kSPS easier
//          deviceParams->devParams->fsFreq.fsHz = 2560000.0;
          deviceParams->devParams->fsFreq.fsHz = 6000000.0;
        }
        else
        {
          // Can't change Fs in master/slave mode
        }
      }

      // Configure tuner parameters (depends on selected Tuner which parameters to use)
      chParams = (chosenDevice->tuner == sdrplay_api_Tuner_B)? deviceParams->rxChannelB: deviceParams->rxChannelA;
      if (chParams != NULL)
      {
        // Set Frequency
        //chParams->tunerParams.rfFreq.rfHz = 50407000.0;
        chParams->tunerParams.rfFreq.rfHz = (float)CentreFreq;

        // Set the smallest bandwidth as we are going to narrow it down a lot more soon
        chParams->tunerParams.bwType = sdrplay_api_BW_0_200;

        // Change single tuner mode to ZIF
        if (master_slave == 0)
        { 
//          chParams->tunerParams.ifType = sdrplay_api_IF_Zero;
          chParams->tunerParams.ifType = sdrplay_api_IF_1_620;
        }

        // Set the tuner gain
        if (strcmp(destination, "local") == 0)
        {
          chParams->tunerParams.gain.gRdB = IFgain;          // Set between 20 and 59.
          chParams->tunerParams.gain.LNAstate = legal_gain(RFgain);
        }
        else
        {
          chParams->tunerParams.gain.gRdB = remoteIFgain;          // Set between 20 and 59.
          chParams->tunerParams.gain.LNAstate = legal_gain(remoteRFgain);
        }

        //chParams->tunerParams.gain.gRdB = 40;
        //chParams->tunerParams.gain.LNAstate = 5;

        // Set the Decimation to max
        chParams->ctrlParams.decimation.enable = 1;            // default 0 (off), 1=on
        chParams->ctrlParams.decimation.decimationFactor = 32; // default 1, max 32
				
        // wideband = 1 uses better filters but less efficient on cpu usage
        chParams->ctrlParams.decimation.wideBandSignal = 1;    // default 0
			
        // Disable AGC
        chParams->ctrlParams.agc.enable = sdrplay_api_AGC_DISABLE;
      }
      else
      {
        printf("sdrplay_api_GetDeviceParams returned NULL chParams pointer\n");
        // goto CloseApi;
      }
		
      // Assign callback functions to be passed to sdrplay_api_Init()
      cbFns.StreamACbFn = StreamACallback;
      cbFns.StreamBCbFn = StreamBCallback;
      cbFns.EventCbFn = EventCallback;
		
      // Now we're ready to start by calling the initialisation function
      // This will configure the device and start streaming
      if ((err = sdrplay_api_Init(chosenDevice->dev, &cbFns, NULL)) != sdrplay_api_Success)
      {
        printf("sdrplay_api_Init failed %s\n", sdrplay_api_GetErrorString(err));
        if (err == sdrplay_api_StartPending)  // This can happen if we're starting in master/slave mode as a slave and the master is not yet running
        { 
          while(1)
          {
            usleep(1000);
            if (masterInitialised)  // Keep polling flag set in event callback until the master is initialised
            {
              // Redo call - should succeed this time
              if ((err = sdrplay_api_Init(chosenDevice->dev, &cbFns, NULL)) != sdrplay_api_Success)
              {
                printf("sdrplay_api_Init failed %s\n", sdrplay_api_GetErrorString(err));
              }
              // goto CloseApi;
            }
            printf("Waiting for master to initialise\n");
          }
        }
        else
        {
          sdrplay_api_ErrorInfoT *errInfo = sdrplay_api_GetLastError(NULL);
          if (errInfo != NULL) printf("Error in %s: %s(): line %d: %s\n", errInfo->file, errInfo->function, errInfo->line, errInfo->message);
          {
            //goto CloseApi;
          }
        }
      }

      // If required, we start serving the data to the websocket interface
      if (strcmp(destination, "remote") == 0)
      {
        websocket_create();
      }
      else                                 // Local display of waterfall
      {
        // Set initial parameters
        last_output = monotonic_ms();

        fft_offset = (fft_size / 2) - 250;

        // zero all the buffers
        for(i = 0; i < 4096; i++)
        {
          y3[i] = 1;
          //hanning_window_const[i] = 0;
          fft_buffer.data[i] = -20.0;
          fft_output_data[i] = 1;
        }

        // Start fft thread
        printf("Starting FFT Thread.. \n");
        if (pthread_create(&fftThread, NULL, thread_fft, NULL))
        {
          printf("Error creating FFT thread\n");
          //return -1;
        }
        pthread_setname_np(fftThread, "FFT Calculation");
        printf("FFT thread running.\n");

        // Copy fft scaled data to display buffer 
        while ((false == *exit_requested) && (app_exit == false)) 
        {
          if(monotonic_ms() > (last_output + 50))  // so 20 Hz refresh
          {
             // Reset timer for 20 Hz refresh
             last_output = monotonic_ms();

             // Check for parameter changes (loop ends here if none)

             // Change of Frequency
             if (NewFreq == true)
             {
               sdrplay_api_Uninit(chosenDevice->dev);
               chParams->tunerParams.rfFreq.rfHz = (float)CentreFreq;

               if ((err = sdrplay_api_Init(chosenDevice->dev, &cbFns, NULL)) != sdrplay_api_Success)
               {
                 printf("sdrplay_api_Init failed %s\n", sdrplay_api_GetErrorString(err));
               }
               NewFreq = false;
             }

             // Change of gain
             if (NewGain == true)
             {
               sdrplay_api_Uninit(chosenDevice->dev);

               // Set AGC
               if (agc == true)
               {
                 chParams->ctrlParams.agc.enable = sdrplay_api_AGC_100HZ;
               }
               else
               {
                 chParams->ctrlParams.agc.enable = sdrplay_api_AGC_DISABLE;
               }

               // Set RF Gain
               chParams->tunerParams.gain.LNAstate = legal_gain(RFgain);

               // Set IF gain
               chParams->tunerParams.gain.gRdB = IFgain;          // Set between 20 (max gain) and 59 (least)
        
               if ((err = sdrplay_api_Init(chosenDevice->dev, &cbFns, NULL)) != sdrplay_api_Success)
               {
                 printf("sdrplay_api_Init failed %s\n", sdrplay_api_GetErrorString(err));
               }
               NewGain = false;
             }

             // Change of Display Span
             if (prepnewscanwidth == true)
             {

              printf("prepnewscanwidth == true\n");
              // Notify touchscreen that parameters can be changed
              readyfornewscanwidth = true;
              printf("readyfornewscanwidth == true\n");

              // Wait for new parameters to be calculated
              while (NewSpan == false)
              {
                usleep(100);
              }
              printf("NewSpan == true\n");

              if (fft_size == 500)
              {
                fft_offset = -7;
              }
              if (fft_size == 512)
              {
                fft_offset = 0;
              }
              if (fft_size == 1024)
              {
                fft_offset = 256;
              }
              if (fft_size == 2048)
              {
                fft_offset = 768;
              }
              if (fft_size == 4096)
              {
                fft_offset = 1792;
              }
              printf("new fft_size = %d\n", fft_size);
              printf("new fft_offset = %d\n", fft_offset);


              // Reset trigger parameters
              NewSpan = false;
              prepnewscanwidth = false;
              readyfornewscanwidth = false;

              // Initialise fft
              printf("Initialising FFT (%d bin).. \n", fft_size);
              setup_fft();
              for (i = 0; i < fft_size; i++)
              {
                hanning_window_const[i] = 0.5 * (1.0 - cos(2*M_PI*(((double)i)/fft_size)));
              }
              printf("FFT Intitialised\n");
              printf("end of change\n");
            }
          }  // remote/local
          else
          {
           usleep(100);
          }

          if (*exit_requested == true)
          {
            printf("Exit Requested before delay\n");
          }
          sleep_ms(1);
        }
      }

      // Finished with device so uninitialise it
      if ((err = sdrplay_api_Uninit(chosenDevice->dev)) != sdrplay_api_Success)
      {
        printf("sdrplay_api_Uninit failed %s\n", sdrplay_api_GetErrorString(err));

        if (err == sdrplay_api_StopPending) 
        {
          // We’re stopping in master/slave mode as a master and the slave is still running
          while(1)
          {
            usleep(1000);
            if (slaveUninitialised) 
            {
              // Keep polling flag set in event callback until the slave is uninitialised

              // Repeat call - should succeed this time
              if ((err = sdrplay_api_Uninit(chosenDevice->dev)) != sdrplay_api_Success) 
              {
                printf("sdrplay_api_Uninit failed %s\n", sdrplay_api_GetErrorString(err));
              }
              slaveUninitialised = 0;
              // goto CloseApi;
            }
            printf("Waiting for slave to uninitialise\n");
          }
        }
      }
      // Release device (make it available to other applications)
      sdrplay_api_ReleaseDevice(chosenDevice);
    }

    // Unlock API
    sdrplay_api_UnlockDeviceApi();

    // Close API
    sdrplay_api_Close();
  }

  close_fftw();
  closelog();
  printf("Main fft Thread Closed\n");

  // For debug purposes
  // printf("Final total = %llu words, rate=%llu words/second, number of buffers=%llu\n", total, total/10, buf_total);
  return 0;
}





