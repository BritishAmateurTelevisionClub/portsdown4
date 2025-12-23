#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <wiringPi.h>
#include <ads1115.h>
#include <stdbool.h>

#include "adf4351.h"
#include "timing.h"

#define PATH_CONFIG "/home/pi/rpidatv/src/ook48/ook48_config.txt"

#define AD_BASE 200  // The base address for the ADS1115

uint32_t registers[6] =  {0x4580A8, 0x80080C9, 0x4E42, 0x4B3, 0xBC803C, 0x580005};

//REG0 0100 0101 1000 0000 1010 1000
//REG1 1000 0000 0000 1000 0000 1100 1001
//REG2 0100 1110 0100 0010
//REG3 0100 1011 0011
//REG4 1011 1100 1000 0000 0011 1100
//REG5 0101 1000 0000 0000 0000 0101

// Global variables
uint32_t adf4350_requested_ref_freq = 25000000;  // Set between 5 and 100 MHz
uint16_t adf4350_requested_power = 0;            // Set between 0 (min) and 3 (max)

//all valid 4 from 8 values.
uint8_t encode4from8[70] = { 15, 23, 27, 29, 30, 39, 43, 45, 46, 51,
                       53, 54, 57, 58, 60, 71, 75, 77, 78, 83,
                       85, 86, 89, 90, 92, 99, 101, 102, 105, 106,
                       108, 113, 114, 116, 120, 135, 139, 141, 142, 147,
                       149, 150, 153, 154, 156, 163, 165, 166, 169, 170,
                       172, 177, 178, 180, 184, 195, 197, 198, 201, 202,
                       204, 209, 210, 212, 216, 225, 226, 228, 232, 240 };


float startFreq;                                 // Freq for beacon in MHz read from config file

// Function Prototypes
void GetConfigParam(char *PathConfigFile, char *Param, char *Value);
void ReadSavedParams();
int set_adf(float freqMHz);


/***************************************************************************//**
 * @brief Looks up the value of a Param in PathConfigFile and sets value
 *        Used to look up the configuration from portsdown_config.txt
 *
 * @param PatchConfigFile (str) the name of the configuration text file
 * @param Param the string labeling the parameter
 * @param Value the looked-up value of the parameter
 *
 * @return void
*******************************************************************************/

void GetConfigParam(char *PathConfigFile, char *Param, char *Value)
{
  char * line = NULL;
  size_t len = 0;
  int read;
  char ParamWithEquals[255];
  strcpy(ParamWithEquals, Param);
  strcat(ParamWithEquals, "=");

  //printf("Get Config reads %s for %s ", PathConfigFile , Param);

  FILE *fp=fopen(PathConfigFile, "r");
  if(fp != 0)
  {
    while ((read = getline(&line, &len, fp)) != -1)
    {
      if(strncmp (line, ParamWithEquals, strlen(Param) + 1) == 0)
      {
        strcpy(Value, line+strlen(Param)+1);
        char *p;
        if((p=strchr(Value,'\n')) !=0 ) *p=0; //Remove \n
        break;
      }
    }
  }
  else
  {
    printf("Config file not found \n");
  }
  fclose(fp);
}


/***************************************************************************//**
 * @brief Reads the Config file into global variables
 *
 * @param None
 *
 * @return void
 * 
*******************************************************************************/

void ReadSavedParams()
{
  char response[63] = "0";
  char param[63];

  GetConfigParam(PATH_CONFIG, "startfreq", response);
  startFreq = atof(response);


  //strcpy(response, "0");
  //GetConfigParam(PATH_CONFIG, "dwelltime", response);
  //dwellTime = atoi(response);
}


/***************************************************************************//**
 * @brief Powers off or sets the Synth ref freq, output freq and power level.
 *
 * @param float freqMHz between 35 and 4400.  Set 0 to turn off VCO
 *
 * @return 0 on success or 1 if freq out of bounds
*******************************************************************************/

int set_adf(float freqMHz)
{
  // Kick Wiring Pi into life
  if (wiringPiSetup() == -1);

  // set parameter defaults
  uint64_t adf4350_requested_frequency = 1255000000;

  if ( freqMHz >= -0.1 && freqMHz <= 0.1 )
  {
    // Turn VCO Off and return

    adf4350_out_altvoltage0_powerdown(1);
    printf("ADF4351 Powered Down\n");
    return 0;
  }

  if ( freqMHz >= 35.0 && freqMHz <= 4400.0 )
  {
    // Valid freq, so set it
    adf4350_requested_frequency = 1000000 * freqMHz;

    // Valid input so set parameters
    adf4350_init_param MyAdf=
    {
      // Calculation inputs
      .clkin=adf4350_requested_ref_freq,
      .channel_spacing=5000,
      .power_up_frequency=adf4350_requested_frequency,
      .reference_div_factor=0,
      .reference_doubler_enable=0,
      .reference_div2_enable=0,

       // r2_user_settings
      .phase_detector_polarity_positive_enable=1,
      .lock_detect_precision_6ns_enable=0,
      .lock_detect_function_integer_n_enable=0,
      .charge_pump_current=7, // FixMe
      .muxout_select=0,
      .low_spur_mode_enable=1,

      // r3_user_settings
      .cycle_slip_reduction_enable=1,
      .charge_cancellation_enable=0,
      .anti_backlash_3ns_enable=0,
      .band_select_clock_mode_high_enable=1,
      .clk_divider_12bit=0,
      .clk_divider_mode=0,

      // r4_user_settings
      .aux_output_enable=0,
      .aux_output_fundamental_enable=1,
      .mute_till_lock_enable=1,
      .output_power=adf4350_requested_power,
      .aux_output_power=0
    };

    // Send the commands to the ADF4351
    adf4350_setup(0,0,MyAdf);
    return 0;
  }
  else
  {
    // Requested freq out of limits so print error and return 1
    printf("ERROR: Requested Frequency of %0.1f MHz out of limits\n", freqMHz);
    return 1;
  }
}


uint8_t encoder(const char* msg, uint8_t len, uint8_t* symbols) 
{
  uint8_t v;
  for (int i = 0; i < len; i++) 
  {
    v = 69;  //default to null
    switch (msg[i]) 
    {
      case 13:                // Carriage return or line feed = end of message
      case 10:
      v=0;
      break;

      case 32 ... 95:
        v = msg[i] - 31;  //all upper case Letters, numbers and punctuation encoded to 1 - 64
        break;

      case 97 ... 122:
        v = msg[i] - 63;  //lower case letters map to upper case.
        break;
    }
    symbols[i] = encode4from8[v];
  }

  return len;
}


int main()
{
  uint32_t i;
  uint32_t j;
  uint32_t k;
  float loFreq;
  uint64_t startTime_ms;
  uint64_t secondPlusms = 1;
  uint8_t encode[10];
  uint8_t encodedMessage[127];
  uint8_t length;
  uint8_t pattern = 0;
  uint32_t timeAdvance = 225;

  // Read in the desired scan parameters
  ReadSavedParams();

  char message[127] = "CQ DE G8GKQ IO91CC";
  //char message[127] = "3333";   // gives 01010101
  //char message[127] = "PPPP";   // gives 10101010

  // Add a CR to the end of the message
  strcat (message, "\n");

  length = (uint8_t)(strlen(message));

  // Encode the message into 4 of 8 and store in encodedMessage[]
  encoder(message, length, encodedMessage);

  // Check frequency
  if (set_adf(startFreq) != 0)
  {
    printf("ADF Frequency out of limits\n");
  }
  else
  {
    printf("TX set to %0.3f MHz\n", startFreq);
  }

  // Start sending
  //for (k = 0; k < 20; k++)                          // Repeat entire message
  while(true)
  {
    for (j = 0; j < strlen(message); j++)           // For each character
    {
      // Sort out the next character to send

      encode[8] = 0;

      pattern = encodedMessage[j];

      // Wait for the start of the second 
      while (secondPlusms != 0)
      {
        usleep(10);
        startTime_ms = monotonic_ms() + timeAdvance;
        secondPlusms = startTime_ms % 1000;
      }
      
      // Print the character being sent
      if (pattern == 15)              // CR
      {
        printf("Sending cr = %d:  ", pattern);
      }
      else if (pattern < 100)        // formatting option
      {
        printf("Sending %c =  %d:  ", message[j], pattern);
      }
      else
      {
        printf("Sending %c = %d:  ", message[j], pattern);
      }

      for (i = 0; i < 9; i++)                    // For each bit
      {
        if (i != 8)                              // not bit 8 as it is always 0
        {
          //check if bit is on or off
          encode[i] = pattern & 0x80;

          // shift bits for next time round
          pattern = pattern << 1;
        }

        if (i == 0)  // send first bit immediately
        {
          if (encode[0] == 128)
          {
            adf4350_out_altvoltage0_powerdown(0);   // output on
            printf("1 ");
          }
          else
          {
            adf4350_out_altvoltage0_powerdown(1);   // output 0ff
            printf("0 ");
          }
        }
        else       // wait for the correct time to send bits 2 - 9
        {
          // Wait until time for next step
          while (secondPlusms != (uint64_t)(111 * i))
          {
            usleep(10);
            secondPlusms = monotonic_ms() + timeAdvance - startTime_ms;
          }

          if (encode[i] == 128)
          {
            adf4350_out_altvoltage0_powerdown(0);   // output on
            printf("1 ");
          }
          else
          {
            adf4350_out_altvoltage0_powerdown(1);   // output 0ff
            printf("0 ");
          }
        }
      }
      printf("\n");
    }
  }

  // End of scan so power down LO
  set_adf(0);
}
