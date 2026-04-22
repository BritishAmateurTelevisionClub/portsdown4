#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>

#include <lime/LimeSuite.h>

typedef enum
{ 
    WAVFORMAT_I8 = 0, 
    WAVFORMAT_I16 = 1, 
    WAVFORMAT_F32 = 2
} wav_format_t;

static int exit_requested = 0;
static void sighandler(int sig, siginfo_t *siginfo, void *context)
{
    (void)sig;
    (void)siginfo;
    (void)context;

    exit_requested = 1;
}
 
static void print_usage(const char *progname){
    printf("Usage: %s [option]" "\n"
            "\t" "-i <filename> or --input <filename> Filename of input WAV" "\n"
            "\t" "-f <freq in MHz> or --frequency <freq in MHz> Freuqency in MHz (default: 437.000MHz)" "\n"
            "\t" "-g <gain> or --gain <gain> with gain in [0.0 .. 1.0] set the normalized RF gain in LimeSDR (default: 0.8)" "\n"
        "Example:" "\n"
        "\t" "./%s -i SDRsharp.wav -f 2395.0 -g 0.7\n", progname, progname);
    exit(0);
}

typedef struct {
    // RIFF Header
    char riff_header[4]; // Contains "RIFF"
    uint32_t wav_size; // Size of the wav portion of the file, which follows the first 8 bytes. File size - 8
    char wave_header[4]; // Contains "WAVE"
    
    // Format Header
    char fmt_header[4]; // Contains "fmt " (includes trailing space)
    uint32_t fmt_chunk_size; // Should be 16 for PCM
    uint16_t audio_format; // Should be 1 for PCM. 3 for IEEE Float
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate; // Number of bytes per second. sample_rate * num_channels * Bytes Per Sample
    uint16_t sample_alignment; // num_channels * Bytes Per Sample
    uint16_t bit_depth; // Number of bits per sample
    
    // Data
    char data_header[4]; // Contains "data"
    uint32_t data_bytes; // Number of bytes in data. Number of samples * num_channels * sample byte size
    // uint8_t bytes[]; // Remainder of wave file is bytes
} wav_header_t;

int main(int argc, char *const argv[])
{
    struct sigaction control_c;

    memset(&control_c, 0, sizeof(control_c));
    control_c.sa_sigaction = &sighandler;
    control_c.sa_flags = SA_SIGINFO;

    char *filename = NULL;
    double frequency = 437e6;
    double gain = 0.8;

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"input",   required_argument, 0,  'i' },
            {"frequency",   optional_argument, 0,  'f' },
            {"gain",   optional_argument, 0,  'g' },
            {0,         0,                 0,  0 }
        };

        int c = getopt_long(argc, argv, "i:f:g:", long_options, &option_index);
        if (c == -1)
        break;

        switch (c) {
            case 0:
            #if 1
            fprintf(stderr, "option %s", long_options[option_index].name);
            if (optarg)
                fprintf(stderr, " with arg %s", optarg);
            fprintf(stderr, "\n");
            #endif

            break;

            case 'i':
                filename = strdup(optarg);
            break;
            case 'f':
                frequency = (strtod(optarg, NULL) * 1.0e6);
            break;
            case 'g':
                gain = strtod(optarg, NULL);
            break;
            default:
                print_usage(argv[0]);
            break;
        }
    }

    printf("Using filename %s" "\n", filename);

    FILE *fileptr = fopen(filename, "r");
    if(fileptr == NULL) {
        fprintf(stderr,"Cannot open file for reading\n");
        exit(EXIT_FAILURE);
    }

    wav_header_t *wav_header_ptr = malloc(sizeof(wav_header_t));
    if(wav_header_ptr == NULL)
    {
        fprintf(stderr,"Cannot allocate for wav_header\n");
        exit(EXIT_FAILURE);
    }

    //if(fread(wav_header_ptr, sizeof(wav_header_t), 1, fileptr) < sizeof(wav_header_t))
    if(fread(wav_header_ptr, sizeof(wav_header_t), 1, fileptr) < 1)  // returns number of items (1) , not size
    {
        fprintf(stderr,"Failed to read WAV header from input file\n");
        exit(EXIT_FAILURE);
    }

    printf(" - fmt_chunk_size : %d\n", wav_header_ptr->fmt_chunk_size);
    printf(" - audio_format : %d\n", wav_header_ptr->audio_format);
    printf(" - sample_rate : %d\n", wav_header_ptr->sample_rate);
    printf(" - bit_depth : %d\n", wav_header_ptr->bit_depth);
    printf(" - bytes: %d\n", wav_header_ptr->data_bytes);
    printf(" - duration: %d seconds\n", wav_header_ptr->data_bytes / (wav_header_ptr->sample_rate * (wav_header_ptr->bit_depth / 4)));

    double sampleRate = (double)wav_header_ptr->sample_rate;
    double rfBandwidth = (double)wav_header_ptr->sample_rate * 1.5;
    wav_format_t input_format;

    if(wav_header_ptr->audio_format == 3)
    {
        input_format = WAVFORMAT_F32;
    }
    else if(wav_header_ptr->audio_format == 1)
    {
        if(wav_header_ptr->fmt_chunk_size == 8)
        {
            input_format = WAVFORMAT_I8;
        }
        else if(wav_header_ptr->fmt_chunk_size == 16)
        {
            input_format = WAVFORMAT_I16;
        }
        else
        {
            fprintf(stderr,"Unknown WAV PCM bit-depth\n");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        fprintf(stderr,"Unknown WAV format\n");
        exit(EXIT_FAILURE);
    }

    int device_count = LMS_GetDeviceList(NULL);
    if(device_count < 1){
        fprintf(stderr, "Error: No LimeSDR device found\n");
        return -1;
    }
    lms_info_str_t *device_list = malloc(sizeof(lms_info_str_t) * device_count);
    device_count = LMS_GetDeviceList(device_list);

    printf("LimeSDR Devices:\n");
    for(int i = 0; i < device_count; i++)
    {
        printf("  %d: %s\n", i, device_list[i]);
        i++;
    }

    lms_device_t *device = NULL;

    int device_index = 0;
    if(LMS_Open(&device, device_list[device_index], NULL)){
        return -1;
    }

    // Normalized gain shall be in [0.0 .. 1.0]
    if(gain < 0.0){
        gain = 0.0;
    }
    if(gain > 1.0){
        gain = 1.0;
    }
    printf("Using normalized gain %lf (0.0-1.0)\n", gain);

    int lmsReset = LMS_Reset(device);
    if(lmsReset){
        printf("lmsReset %d(%s)" "\n", lmsReset, LMS_GetLastErrorMessage());
    }

    // Query the device details
    const lms_dev_info_t *device_info;
    device_info = LMS_GetDeviceInfo(device);

    int lmsInit = LMS_Init(device);
    if(lmsInit){
        printf("lmsInit %d(%s)" "\n", lmsInit, LMS_GetLastErrorMessage());
    }

    int channel = 0;
    int channel_count = LMS_GetNumChannels(device, LMS_CH_TX);
    printf("Tx channel count %d" "\n", channel_count);
    printf("Using channel %d" "\n", channel);
    
    LMS_SetNormalizedGain(device, LMS_CH_TX, channel, gain);

    // Disable all other channels
    //LMS_EnableChannel(device, LMS_CH_TX, 1 - channel, false);
    LMS_EnableChannel(device, LMS_CH_RX, 0, true); /* LimeSuite bug workaround (needed since LimeSuite git rev 52d6129 - or v18.06.0) */
    //LMS_EnableChannel(device, LMS_CH_RX, 1, false);

    // Enable our Tx channel
    LMS_EnableChannel(device, LMS_CH_TX, channel, true);

    printf("Using frequency %.6lf MHz\n", (frequency / 1.0e6));

    int setLOFrequency = LMS_SetLOFrequency(device, LMS_CH_TX, channel, frequency);
    if(setLOFrequency){
        printf("setLOFrequency(%lf)=%d(%s)" "\n", frequency, setLOFrequency, LMS_GetLastErrorMessage());
    }

    // Set the correct antenna
    if (strcmp(device_info->deviceName, "LimeSDR-USB") == 0)
    {
      if (frequency < 2000000000)
      {
        LMS_SetAntenna(device, LMS_CH_TX, channel, 1);
      }
      else
      {
        LMS_SetAntenna(device, LMS_CH_TX, channel, 2);
      }
    }
    else                                                 // LimeSDR Mini
    {
      if (frequency > 2000000000)
      {
        LMS_SetAntenna(device, LMS_CH_TX, channel, 1);
      }
      else
      {
        LMS_SetAntenna(device, LMS_CH_TX, channel, 2);
      }
    }

    lms_range_t sampleRateRange;
    int getSampleRateRange = LMS_GetSampleRateRange(device, LMS_CH_TX, &sampleRateRange);
    if(getSampleRateRange){
        printf("getSampleRateRange=%d(%s)" "\n", getSampleRateRange, LMS_GetLastErrorMessage());
    }

    printf("Set sample rate to %.6lf MS/s ...\n", (sampleRate / 1.0e6));
    int setSampleRate = LMS_SetSampleRate(device, sampleRate, 0);
    if(setSampleRate){
        printf("setSampleRate=%d(%s)" "\n", setSampleRate, LMS_GetLastErrorMessage());
    }
    double actualHostSampleRate = 0.0;
    double actualRFSampleRate = 0.0;
    int getSampleRate = LMS_GetSampleRate(device, LMS_CH_TX, channel, &actualHostSampleRate, &actualRFSampleRate);
    if(getSampleRate){
        printf("getSampleRate=%d(%s)" "\n", getSampleRate, LMS_GetLastErrorMessage());
    }else{
        printf("Samplerate Host: %.6lf MS/s -> RF: %.6lf MS/s" "\n", (actualHostSampleRate / 1.0e6), (actualRFSampleRate / 1.0e6));
    }

    printf("Calibrating ..." "\n");
    int calibrate = LMS_Calibrate(device, LMS_CH_TX, channel, rfBandwidth, 0);
    if(calibrate){
        printf("calibrate=%d(%s)" "\n", calibrate, LMS_GetLastErrorMessage());
    }

    printf("Setup TX stream ..." "\n");
    lms_stream_t tx_stream = {.channel = channel, .fifoSize = 1024*1024, .throughputVsLatency = 0.5, .isTx = true, .dataFmt = LMS_FMT_I12};
    int setupStream = LMS_SetupStream(device, &tx_stream);
    if(setupStream){
        printf("setupStream=%d(%s)" "\n", setupStream, LMS_GetLastErrorMessage());
    }

    int nSampleSize = 0;
    int nSamples = (int)sampleRate / 100;
    printf("nSamples = %d\n", nSamples);

    struct s8iq_sample_s {
        signed char i;
        signed char q;
    };
    struct s16iq_sample_s {
        int16_t i;
        int16_t q;
    };
    struct f32iq_sample_s {
        float i;
        float q;
    };

    if(input_format == WAVFORMAT_I8)
    {
        nSampleSize = sizeof(struct s8iq_sample_s);
    }
    else if(input_format == WAVFORMAT_I16)
    {
        nSampleSize = sizeof(struct s16iq_sample_s);
    }
    else if(input_format == WAVFORMAT_F32)
    {
        nSampleSize = sizeof(struct f32iq_sample_s);
    }

    void *fileSamples = malloc(nSampleSize * nSamples);

    struct s16iq_sample_s *sdrSamples = malloc(sizeof(struct s16iq_sample_s) * nSamples);

    int nSamplesRead = fread(fileSamples, nSampleSize, nSamples, fileptr);

    int loop = 0;
    int sendStream;
    LMS_StartStream(&tx_stream);

    while((0 == exit_requested) && (nSamplesRead > 0))
    {
        /* Scale input data */
        if(input_format == WAVFORMAT_I8)
        {
            // Up-Scale to 12-bit
            for(int i = 0; i < nSamples; i++)
            {
                sdrSamples[i].i = (((struct s8iq_sample_s*)fileSamples)[i].i << 4);
                sdrSamples[i].q = (((struct s8iq_sample_s*)fileSamples)[i].q << 4);
            }
        }
        else if(input_format == WAVFORMAT_I16)
        {
            // Copy samples
            for(int i = 0; i < nSamples; i++)
            {
                sdrSamples[i].i = (((struct s16iq_sample_s*)fileSamples)[i].i);
                sdrSamples[i].q = (((struct s16iq_sample_s*)fileSamples)[i].q);
                //sdrSamples[i].i = (((struct s16iq_sample_s*)fileSamples)[i].i << 2);
                //sdrSamples[i].q = (((struct s16iq_sample_s*)fileSamples)[i].q << 2);
            }
        }
        else if(input_format == WAVFORMAT_F32)
        {
            // Scale from normalised to 12-bit
            for(int i = 0; i < nSamples; i++)
            {
                sdrSamples[i].i = (int16_t)(((struct f32iq_sample_s*)fileSamples)[i].i * 2048.0);
                sdrSamples[i].q = (int16_t)(((struct f32iq_sample_s*)fileSamples)[i].q * 2048.0);
            }
        }

        /* Output to Lime */
        sendStream = LMS_SendStream(&tx_stream, sdrSamples, nSamples, NULL, 1000);
        if(sendStream < 0){
            printf("sendStream error %d(%s)" "\n", sendStream, LMS_GetLastErrorMessage());
        }

        loop++;
        if(0 == (loop % 100)){
            lms_stream_status_t status;
            LMS_GetStreamStatus(&tx_stream, &status); //Obtain TX stream stats
            printf("Lime TX rate: %.2lf MB/s" "\n", (status.linkRate / 1e6));
        }

        /* Read next data */
        nSamplesRead = fread(fileSamples, nSampleSize, nSamples, fileptr);
    }

    fclose(fileptr);

    LMS_StopStream(&tx_stream);
    LMS_DestroyStream(device, &tx_stream);

    LMS_Reset(device);
    LMS_Init(device);
    int nb_channel = LMS_GetNumChannels( device, LMS_CH_RX );
    int c;
    for( c = 0; c < nb_channel; c++ ) {
        LMS_EnableChannel(device, LMS_CH_RX, c, false);
    }
    nb_channel = LMS_GetNumChannels( device, LMS_CH_TX );
    for( c = 0; c < nb_channel; c++ ) {
        LMS_EnableChannel(device, LMS_CH_TX, c, false);
    }
    LMS_Close(device);

    return(0);
}