#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <linux/input.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <math.h>

#include <wiringPi.h>
#include <signal.h>
#include <dirent.h>
#include <pthread.h>
#include <ctype.h>
#include <stdbool.h>

#include <assert.h>
#include <time.h>

#include "ffunc.h"
#include "font/font.h"
#include "touch.h"
#include "Graphics.h"
#include "dmm.h"
#include "timing.h"

#define PI 3.14159265358979323846

pthread_t thbutton;
pthread_t thSerial_Listener;
pthread_t thSerial_Decoder;
pthread_t thwebclick;     //  Listens for mouse clicks from web interface
pthread_t thtouchscreen;  //  listens to the touchscreen   

pthread_mutex_t text_lock;

int fd = 0;
int wscreen, hscreen;
float scaleXvalue, scaleYvalue; // Coeff ratio from Screen/TouchArea

#define MAX_STATUS 6
typedef struct
{
  int x, y, w, h;                // Position, width and height of button
  status_t Status[MAX_STATUS];   // Array of text and required colour for each status
  int IndexStatus;               // The number of valid status definitions.  0 = do not display
  int NoStatus;                  // This is the active status (colour and text)
} button_t;

// Set the Colours up front
color_t Green = {.r = 0  , .g = 128, .b = 0  };
color_t Blue  = {.r = 0  , .g = 0  , .b = 128};
color_t LBlue = {.r = 64 , .g = 64 , .b = 192};
color_t DBlue = {.r = 0  , .g = 0  , .b = 64 };
color_t Grey  = {.r = 127, .g = 127, .b = 127};
color_t DGrey = {.r = 32 , .g = 32 , .b = 32 };
color_t Red   = {.r = 255, .g = 0  , .b = 0  };
color_t Black = {.r = 0  , .g = 0  , .b = 0  };

#define PATH_PCONFIG "/home/pi/rpidatv/scripts/portsdown_config.txt"
#define PATH_DMMCONFIG "/home/pi/rpidatv/src/dmm/dmm_config.txt"

#define MAX_BUTTON 675
int IndexButtonInArray=0;
button_t ButtonArray[MAX_BUTTON];
int CurrentMenu = 1;
int CallingMenu = 1;
char KeyboardReturn[64];

int Inversed = 0;
int  scaledX, scaledY;
char DisplayType[31] = "Element14_7";
int wbuttonsize = 100;
int hbuttonsize = 50;
int rawX, rawY;
int FinishedButton = 0;
bool freeze = false;
bool frozen = false;
bool activescan = false;
int y[501];                  // Actual displayed values on the chart
float reading[501];          // value passed from meter
char readingtime[501][31];   //Time tag for value
int NewestReading;           // Position (0 - 501) of most recent reading
time_t t;      // = time(NULL);
struct tm *tm; // = localtime(&t);

char PlotTitle[63] = "-";
bool ContScan = false;

bool ModeChanged = true;
bool PortsdownExitRequested = false;
bool app_exit = false;

int markerx = 250;
int markery = 15;
bool markeron = false;
int markermode = 7;       // 2 peak, 3, null, 4 man, 7 off
int historycount = 0;
int markerxhistory[10];
int markeryhistory[10];
int manualmarkerx = 250;

int tracecount = 0;  // Used for speed testing
int exit_code;

int yscalenum = 18;       // Numerator for Y scaling fraction
int yscaleden = 30;       // Denominator for Y scaling fraction
int yshift    = 5;        // Vertical shift (pixels) for Y

int xmin = 30;            // Trigger value for start of scan
int xscalenum = 26;       // Numerator for X scaling fraction
int xscaleden = 20;       // Denominator for X scaling fraction

char DMMMode[31];                  // Mode: numeric, graph, battery
char MeterSensor[63];              // pdm300 or maplin
float PlotBase;                    // baseline
float PlotMax;                     // max
int PlotDuration;                  // Plot full width in minutes
float Discharged;                  // Discharged voltage
float InitialCurrent;              // Initial discharge current
float InitialVoltage;              // Initial battery voltage
uint64_t Cumulative_ma_ms;         // battery capacity supplied so far in mA*mS
float CumulativeAH;                // battery capacity supplied so far in AH
bool ConstantCurrent;              // False means resistive load (config constant/resistive)
bool SerialPolarity;               // normal=true, false=inverted
bool PlotDurationChanged = false;  // Redraw trigger
bool SingleScan = false;           // single scan for recording events
bool DischargeInProgress = false;  // 
bool DischargePaused = false;      //
bool DischargeComplete;            //

// Serial data
uint8_t nibble1 = 0;
uint8_t nibble2 = 0;
uint8_t readbyte = 0;
bool new_serial_data = false;
bool new_serial_sentence = false;
uint8_t measuremode = 0;
uint16_t exponentbyte = 0;
float MeterReading = 0;
float BasicMeterReading = -10000.0;
bool webcontrol = false;   // Enables webcontrol on a Portsdown 4
bool stale_data = false;
uint8_t LoadSwitchGPIO = 11;   // Load Switch pin 26 wPi 11
bool LoadSwitchOn = false;

bool webclicklistenerrunning = false; // Used to only start thread if required
char WebClickForAction[7] = "no";  // no/yes
char ProgramName[255];             // used to pass prog name char string to listener
int *web_x_ptr;                // pointer
int *web_y_ptr;                // pointer
int web_x;                     // click x 0 - 799 from left
int web_y;                     // click y 0 - 480 from top
int TouchX;
int TouchY;
int TouchPressure;
int TouchTrigger = 0;
bool touchneedsinitialisation = true;

char DisplayType[31];
bool touchscreen_present = false;

///////////////////////////////////////////// SCREEN AND TOUCH UTILITIES ////////////////////////


/***************************************************************************//**
 * @brief Looks up the value of a Param in PathConfigFile and sets value
 *        Used to look up the configuration from dmm_config.txt
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
 * @brief sets the value of Param in PathConfigFile from a program variable
 *        Used to store the configuration in dmm_config.txt
 *
 * @param PatchConfigFile (str) the name of the configuration text file
 * @param Param the string labeling the parameter
 * @param Value the looked-up value of the parameter
 *
 * @return void
*******************************************************************************/


void SetConfigParam(char *PathConfigFile, char *Param, char *Value)
{
  char * line = NULL;
  size_t len = 0;
  int read;
  char Command[511];
  char BackupConfigName[240];
  strcpy(BackupConfigName,PathConfigFile);
  strcat(BackupConfigName,".bak");
  FILE *fp=fopen(PathConfigFile,"r");
  FILE *fw=fopen(BackupConfigName,"w+");
  char ParamWithEquals[255];
  strcpy(ParamWithEquals, Param);
  strcat(ParamWithEquals, "=");

  if(fp!=0)
  {
    while ((read = getline(&line, &len, fp)) != -1)
    {
      if(strncmp (line, ParamWithEquals, strlen(Param) + 1) == 0)
      {
        fprintf(fw, "%s=%s\n", Param, Value);
      }
      else
      {
        fprintf(fw, line);
      }
    }
    fclose(fp);
    fclose(fw);
    snprintf(Command, 511, "cp %s %s", BackupConfigName, PathConfigFile);
    system(Command);
  }
  else
  {
    printf("Config file not found \n");
    fclose(fp);
    fclose(fw);
  }
}


/***************************************************************************//**
 * @brief Checks to see if webcontrol exists in Portsdown Config file
 *
 * @param None
 *
 * @return 0 = Exists, so Portsdown 4
 *         1 = Not
*******************************************************************************/
 
int CheckWebCtlExists()
{
  char shell_command[255];
  FILE *fp;
  int r;

  sprintf(shell_command, "grep -q 'webcontrol' %s", PATH_PCONFIG);
  fp = popen(shell_command, "r");
  r = pclose(fp);

  if (WEXITSTATUS(r) == 0)
  {
    printf("webcontrol detected\n");
    return 0;
  }
  else
  {
    printf("webcontrol not detected\n");
    return 1;
  } 
}


void ReadSavedParams()
{
  char response[63]="0";

  strcpy(DMMMode, "numeric");
  GetConfigParam(PATH_DMMCONFIG, "dmmmode", DMMMode);

  strcpy(MeterSensor, "pdm300");  // pdm300 or maplin
  GetConfigParam(PATH_DMMCONFIG, "metersensor", MeterSensor);

  strcpy(response, "0");
  GetConfigParam(PATH_DMMCONFIG, "plotbase", response);
  PlotBase = atof(response);

  strcpy(response, "0");
  GetConfigParam(PATH_DMMCONFIG, "plotmax", response);
  PlotMax = atof(response);

  strcpy(response, "0");
  GetConfigParam(PATH_DMMCONFIG, "plotduration", response);
  PlotDuration = atoi(response);

  strcpy(response, "false");
  SingleScan = false;
  GetConfigParam(PATH_DMMCONFIG, "singlescan", response);
  if (strcmp(response, "true") == 0)
  {
    SingleScan = true;
  }

  strcpy(response, "0");
  GetConfigParam(PATH_DMMCONFIG, "discharged", response);
  Discharged = atof(response);

  strcpy(response, "0");
  GetConfigParam(PATH_DMMCONFIG, "initialcurrent", response);
  InitialCurrent = atof(response);

  strcpy(response, "resistive");
  GetConfigParam(PATH_DMMCONFIG, "load", response);
  ConstantCurrent = false;
  if (strcmp(response, "constant") == 0)
  {
    ConstantCurrent = true;
  }

  strcpy(response, "normal");
  GetConfigParam(PATH_DMMCONFIG, "serialpolarity", response);
  SerialPolarity = true;
  if (strcmp(response, "inverted") == 0)
  {
    SerialPolarity = false;
  }

  strcpy(PlotTitle, "-");  // this is the "do not display" response
  GetConfigParam(PATH_DMMCONFIG, "title", PlotTitle);

  if (CheckWebCtlExists() == 0)  // Stops the GetConfig thowing an error on Portsdown 2020
  {
    GetConfigParam(PATH_PCONFIG, "webcontrol", response);
    if (strcmp(response, "enabled") == 0)
    {
      webcontrol = true;
      pthread_create (&thwebclick, NULL, &WebClickListener, NULL);
      webclicklistenerrunning = true;
    }
    else
    {
      webcontrol = false;
      system("cp /home/pi/rpidatv/scripts/images/web_not_enabled.png /home/pi/tmp/screen.png");
    }
  }
}


void *WaitTouchscreenEvent(void * arg)
{
  int TouchTriggerTemp;
  int rawX;
  int rawY;
  int rawPressure;
  while (true)
  {
    TouchTriggerTemp = getTouchSampleThread(&rawX, &rawY, &rawPressure);
    TouchX = rawX;
    TouchY = rawY;
    TouchPressure = rawPressure;
    TouchTrigger = TouchTriggerTemp;
  }
  return NULL;
}


void *WebClickListener(void * arg)
{
  while (webcontrol)
  {
    //(void)argc;
	//return ffunc_run(ProgramName);
	ffunc_run(ProgramName);
  }
  webclicklistenerrunning = false;
  return NULL;
}


void parseClickQuerystring(char *query_string, int *x_ptr, int *y_ptr)
{
  char *query_ptr = strdup(query_string),
  *tokens = query_ptr,
  *p = query_ptr;

  while ((p = strsep (&tokens, "&\n")))
  {
    char *var = strtok (p, "="),
         *val = NULL;
    if (var && (val = strtok (NULL, "=")))
    {
      if(strcmp("x", var) == 0)
      {
        *x_ptr = atoi(val);
      }
      else if(strcmp("y", var) == 0)
      {
        *y_ptr = atoi(val);
      }
    }
  }
}


FFUNC touchscreenClick(ffunc_session_t * session)
{
  ffunc_str_t payload;

  if( (webcontrol == false) || ffunc_read_body(session, &payload) )
  {
    if( webcontrol == false)
    {
      return;
    }

    ffunc_write_out(session, "Status: 200 OK\r\n");
    ffunc_write_out(session, "Content-Type: text/plain\r\n\r\n");
    ffunc_write_out(session, "%s\n", "click received.");
    fprintf(stderr, "Received click POST: %s (%d)\n", payload.data?payload.data:"", payload.len);

    int x = -1;
    int y = -1;
    parseClickQuerystring(payload.data, &x, &y);
    printf("After Parse: x: %d, y: %d\n", x, y);

    if((x >= 0) && (y >= 0))
    {
      web_x = x;                 // web_x is a global int
      web_y = y;                 // web_y is a global int
      strcpy(WebClickForAction, "yes");
      printf("Web Click Event x: %d, y: %d\n", web_x, web_y);
    }
  }
  else
  {
    ffunc_write_out(session, "Status: 400 Bad Request\r\n");
    ffunc_write_out(session, "Content-Type: text/plain\r\n\r\n");
    ffunc_write_out(session, "%s\n", "payload not found.");
  }
}


void UpdateWeb()
{
  // Called after any screen update to update the web page if required.

  if(webcontrol == true)
  {
    system("/home/pi/rpidatv/scripts/single_screen_grab_for_web.sh &");
  }
}


void do_snapcheck()
{
  FILE *fp;
  char SnapIndex[256];
  int SnapNumber;
  int Snap;
  int LastDisplayedSnap = -1;
  int rawX, rawY, rawPressure;
  int TCDisplay = -1;

  char fbicmd[256];

  // Fetch the Next Snap serial number
  fp = popen("cat /home/pi/snaps/snap_index.txt", "r");
  if (fp == NULL) 
  {
    printf("Failed to run command\n" );
    exit(1);
  }
  // Read the output a line at a time - output it. 
  while (fgets(SnapIndex, 20, fp) != NULL)
  {
    printf("%s", SnapIndex);
  }

  pclose(fp);

  SnapNumber=atoi(SnapIndex);
  Snap = SnapNumber - 1;

  while (((TCDisplay == 1) || (TCDisplay == -1)) && (SnapNumber != 0))
  {
    if(LastDisplayedSnap != Snap)  // only redraw if not already there
    {
      sprintf(SnapIndex, "%d", Snap);
      strcpy(fbicmd, "sudo fbi -T 1 -noverbose -a /home/pi/snaps/snap");
      strcat(fbicmd, SnapIndex);
      strcat(fbicmd, ".jpg >/dev/null 2>/dev/null");
      system(fbicmd);
      UpdateWeb();
      LastDisplayedSnap = Snap;
    }

    if (getTouchSample(&rawX, &rawY, &rawPressure)==0) continue;

    system("sudo killall fbi >/dev/null 2>/dev/null");  // kill instance of fbi

    TCDisplay = IsImageToBeChanged(rawX, rawY);  // check if touch was previous snap, next snap or exit
    if (TCDisplay != 0)
    {
      Snap = Snap + TCDisplay;
      if (Snap >= SnapNumber)
      {
        Snap = 0;
      }
      if (Snap < 0)
      {
        Snap = SnapNumber - 1;
      }
    }
  }

  // Tidy up and display touch menu
  system("sudo killall fbi >/dev/null 2>/dev/null");  // kill any instance of fbi
}


int IsImageToBeChanged(int x,int y)
{
  // Returns -1 for LHS touch, 0 for centre and 1 for RHS

  TransformTouchMap(x,y);       // Sorts out orientation and approx scaling of the touch map

  if (scaledY >= hscreen/2)
  {
    return 0;
  }
  if (scaledX <= wscreen/8)
  {
    return -1;
  }
  else if (scaledX >= 7 * wscreen/8)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}


void Keyboard(char RequestText[64], char InitText[64], int MaxLength)
{
  char EditText[64];
  strcpy (EditText, InitText);
  int token;
  int PreviousMenu;
  int i;
  int CursorPos;
  char thischar[2];
  int KeyboardShift = 1;
  int ShiftStatus = 0;
  char KeyPressed[2];
  char PreCuttext[63];
  char PostCuttext[63];
  bool refreshed;
  
  // Store away currentMenu
  PreviousMenu = CurrentMenu;

  // Trim EditText to MaxLength
  if (strlen(EditText) > MaxLength)
  {
    strncpy(EditText, &EditText[0], MaxLength);
    EditText[MaxLength] = '\0';
  }

  // Set cursor position to next character after EditText
  CursorPos = strlen(EditText);

  // On initial call set Menu to 41
  CurrentMenu = 41;
  int rawX, rawY, rawPressure;
  refreshed = false;

  // Set up text
  setForeColour(255, 255, 255);    // White text
  setBackColour(0, 0, 0);          // on Black
  wipeScreen(0, 0, 0);
  const font_t *font_ptr = &font_dejavu_sans_28;

  // Look up pitch for W (87), the widest caharcter
  int charPitch = font_ptr->characters[87].render_width;

  for (;;)
  {
    if (!refreshed)
    {
      // Sort Shift changes
      ShiftStatus = 2 - (2 * KeyboardShift); // 0 = Upper, 2 = lower
      for (i = ButtonNumber(CurrentMenu, 0); i <= ButtonNumber(CurrentMenu, 49); i = i + 1)
      {
        if (ButtonArray[i].IndexStatus > 0)  // If button needs to be drawn
        {
          SetButtonStatus(i, ShiftStatus);
        }
      }  

      // Display the keyboard here as it would overwrite the text later
      UpdateWindow();

      // Display Instruction Text
      setForeColour(255, 255, 255);    // White text
      setBackColour(0, 0, 0);          // on Black
      Text2(10, 420 , RequestText, font_ptr);

      // Blank out the text line to erase the previous text and cursor
      rectangle(10, 320, 780, 40, 0, 0, 0);

      // Display Text for Editing
      for (i = 1; i <= strlen(EditText); i = i + 1)
      {
        strncpy(thischar, &EditText[i-1], 1);
        thischar[1] = '\0';
        if (i > MaxLength)
        {
          setForeColour(255, 63, 63);    // Red text
        }
        else
        {
          setForeColour(255, 255, 255);    // White text
        }
        TextMid2(i * charPitch, 330, thischar, font_ptr);
      }

      // Draw the cursor
      rectangle(12 + charPitch * CursorPos, 320, 2, 40, 255, 255, 255);

      refreshed = true;
    }
    UpdateWeb();

    // Wait for key press
    if (getTouchSample(&rawX, &rawY, &rawPressure)==0) continue;
    refreshed = false;

    token = IsMenuButtonPushed(rawX, rawY);
    printf("Keyboard Token %d\n", token);

    // Highlight special keys when touched
    if ((token == 5) || (token == 8) || (token == 9) || (token == 2) || (token == 3)) // Clear, Enter, backspace, L and R arrows
    {
      SetButtonStatus(ButtonNumber(41, token), 1);
      DrawButton(ButtonNumber(41, token));
      UpdateWindow();
      usleep(300000);
      SetButtonStatus(ButtonNumber(41, token), 0);
      DrawButton(ButtonNumber(41, token));
      UpdateWindow();
    }

    if (token == 8)  // Enter pressed
    {
      if (strlen(EditText) > MaxLength) 
      {
        strncpy(KeyboardReturn, &EditText[0], MaxLength);
        KeyboardReturn[MaxLength] = '\0';
      }
      else
      {
        strcpy(KeyboardReturn, EditText);
      }
      CurrentMenu = PreviousMenu;
      printf("Exiting KeyBoard with String \"%s\"\n", KeyboardReturn);
      break;
    }
    else
    {    
      if (KeyboardShift == 1)     // Upper Case
      {
        switch (token)
        {
        case 0:  strcpy(KeyPressed, " "); break;
        case 4:  strcpy(KeyPressed, "-"); break;
        case 10: strcpy(KeyPressed, "Z"); break;
        case 11: strcpy(KeyPressed, "X"); break;
        case 12: strcpy(KeyPressed, "C"); break;
        case 13: strcpy(KeyPressed, "V"); break;
        case 14: strcpy(KeyPressed, "B"); break;
        case 15: strcpy(KeyPressed, "N"); break;
        case 16: strcpy(KeyPressed, "M"); break;
        case 17: strcpy(KeyPressed, ","); break;
        case 18: strcpy(KeyPressed, "."); break;
        case 19: strcpy(KeyPressed, "/"); break;
        case 20: strcpy(KeyPressed, "A"); break;
        case 21: strcpy(KeyPressed, "S"); break;
        case 22: strcpy(KeyPressed, "D"); break;
        case 23: strcpy(KeyPressed, "F"); break;
        case 24: strcpy(KeyPressed, "G"); break;
        case 25: strcpy(KeyPressed, "H"); break;
        case 26: strcpy(KeyPressed, "J"); break;
        case 27: strcpy(KeyPressed, "K"); break;
        case 28: strcpy(KeyPressed, "L"); break;
        case 30: strcpy(KeyPressed, "Q"); break;
        case 31: strcpy(KeyPressed, "W"); break;
        case 32: strcpy(KeyPressed, "E"); break;
        case 33: strcpy(KeyPressed, "R"); break;
        case 34: strcpy(KeyPressed, "T"); break;
        case 35: strcpy(KeyPressed, "Y"); break;
        case 36: strcpy(KeyPressed, "U"); break;
        case 37: strcpy(KeyPressed, "I"); break;
        case 38: strcpy(KeyPressed, "O"); break;
        case 39: strcpy(KeyPressed, "P"); break;
        case 40: strcpy(KeyPressed, "1"); break;
        case 41: strcpy(KeyPressed, "2"); break;
        case 42: strcpy(KeyPressed, "3"); break;
        case 43: strcpy(KeyPressed, "4"); break;
        case 44: strcpy(KeyPressed, "5"); break;
        case 45: strcpy(KeyPressed, "6"); break;
        case 46: strcpy(KeyPressed, "7"); break;
        case 47: strcpy(KeyPressed, "8"); break;
        case 48: strcpy(KeyPressed, "9"); break;
        case 49: strcpy(KeyPressed, "0"); break;
        }
      }
      else                          // Lower Case
      {
        switch (token)
        {
        case 0:  strcpy(KeyPressed, " "); break;
        case 4:  strcpy(KeyPressed, "_"); break;
        case 10: strcpy(KeyPressed, "z"); break;
        case 11: strcpy(KeyPressed, "x"); break;
        case 12: strcpy(KeyPressed, "c"); break;
        case 13: strcpy(KeyPressed, "v"); break;
        case 14: strcpy(KeyPressed, "b"); break;
        case 15: strcpy(KeyPressed, "n"); break;
        case 16: strcpy(KeyPressed, "m"); break;
        case 17: strcpy(KeyPressed, "!"); break;
        case 18: strcpy(KeyPressed, "."); break;
        case 19: strcpy(KeyPressed, "?"); break;
        case 20: strcpy(KeyPressed, "a"); break;
        case 21: strcpy(KeyPressed, "s"); break;
        case 22: strcpy(KeyPressed, "d"); break;
        case 23: strcpy(KeyPressed, "f"); break;
        case 24: strcpy(KeyPressed, "g"); break;
        case 25: strcpy(KeyPressed, "h"); break;
        case 26: strcpy(KeyPressed, "j"); break;
        case 27: strcpy(KeyPressed, "k"); break;
        case 28: strcpy(KeyPressed, "l"); break;
        case 30: strcpy(KeyPressed, "q"); break;
        case 31: strcpy(KeyPressed, "w"); break;
        case 32: strcpy(KeyPressed, "e"); break;
        case 33: strcpy(KeyPressed, "r"); break;
        case 34: strcpy(KeyPressed, "t"); break;
        case 35: strcpy(KeyPressed, "y"); break;
        case 36: strcpy(KeyPressed, "u"); break;
        case 37: strcpy(KeyPressed, "i"); break;
        case 38: strcpy(KeyPressed, "o"); break;
        case 39: strcpy(KeyPressed, "p"); break;
        case 40: strcpy(KeyPressed, "1"); break;
        case 41: strcpy(KeyPressed, "2"); break;
        case 42: strcpy(KeyPressed, "3"); break;
        case 43: strcpy(KeyPressed, "4"); break;
        case 44: strcpy(KeyPressed, "5"); break;
        case 45: strcpy(KeyPressed, "6"); break;
        case 46: strcpy(KeyPressed, "7"); break;
        case 47: strcpy(KeyPressed, "8"); break;
        case 48: strcpy(KeyPressed, "9"); break;
        case 49: strcpy(KeyPressed, "0"); break;
        }
      }

      // If shift key has been pressed Change Case
      if ((token == 6) || (token == 7))
      {
        if (KeyboardShift == 1)
        {
          KeyboardShift = 0;
        }
        else
        {
          KeyboardShift = 1;
        }
      }
      else if (token == 9)   // backspace
      {
        if (CursorPos == 1)
        {
           // Copy the text to the right of the deleted character
          strncpy(PostCuttext, &EditText[1], strlen(EditText) - 1);
          PostCuttext[strlen(EditText) - 1] = '\0';
          strcpy (EditText, PostCuttext);
          CursorPos = 0;
        }
        if (CursorPos > 1)
        {
          // Copy the text to the left of the deleted character
          strncpy(PreCuttext, &EditText[0], CursorPos - 1);
          PreCuttext[CursorPos-1] = '\0';

          if (CursorPos == strlen(EditText))
          {
            strcpy (EditText, PreCuttext);
          }
          else
          {
            // Copy the text to the right of the deleted character
            strncpy(PostCuttext, &EditText[CursorPos], strlen(EditText));
            PostCuttext[strlen(EditText)-CursorPos] = '\0';

            // Now combine them
            strcat(PreCuttext, PostCuttext);
            strcpy (EditText, PreCuttext);
          }
          CursorPos = CursorPos - 1;
        }
      }
      else if (token == 5)   // Clear
      {
        EditText[0]='\0';
        CursorPos = 0;
      }
      else if (token == 2)   // left arrow
      {
        CursorPos = CursorPos - 1;
        if (CursorPos < 0)
        {
          CursorPos = 0;
        }
      }
      else if (token == 3)   // right arrow
      {
        CursorPos = CursorPos + 1;
        if (CursorPos > strlen(EditText))
        {
          CursorPos = strlen(EditText);
        }
      }
      else if ((token == 0) || (token == 4) || ((token >=10) && (token <= 49)))
      {
        // character Key has been touched, so highlight it for 300 ms
 
        ShiftStatus = 3 - (2 * KeyboardShift); // 1 = Upper, 3 = lower
        SetButtonStatus(ButtonNumber(41, token), ShiftStatus);
        DrawButton(ButtonNumber(41, token));
        //UpdateWindow();
        usleep(300000);
        ShiftStatus = 2 - (2 * KeyboardShift); // 0 = Upper, 2 = lower
        SetButtonStatus(ButtonNumber(41, token), ShiftStatus);
        DrawButton(ButtonNumber(41, token));
        //UpdateWindow();

        if (strlen(EditText) < 33) // Don't let it overflow
        {
        // Copy the text to the left of the insert point
        strncpy(PreCuttext, &EditText[0], CursorPos);
        PreCuttext[CursorPos] = '\0';
          
        // Append the new character to the pre-insert string
        strcat(PreCuttext, KeyPressed);

        // Append any text to the right if it exists
        if (CursorPos == strlen(EditText))
        {
          strcpy (EditText, PreCuttext);
        }
        else
        {
          // Copy the text to the right of the inserted character
          strncpy(PostCuttext, &EditText[CursorPos], strlen(EditText));
          PostCuttext[strlen(EditText)-CursorPos] = '\0';

          // Now combine them
          strcat(PreCuttext, PostCuttext);
          strcpy (EditText, PreCuttext);
        }
        CursorPos = CursorPos+1;
        }
      }
    }
  }
}


int openTouchScreen(int NoDevice)
{
  char sDevice[255];

  sprintf(sDevice, "/dev/input/event%d", NoDevice);
  if(fd != 0) close(fd);
  if ((fd = open(sDevice, O_RDONLY)) > 0)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}


int getTouchScreenDetails(int *screenXmin, int *screenXmax,int *screenYmin,int *screenYmax)
{
  unsigned long bit[EV_MAX][NBITS(KEY_MAX)];
  char name[256] = "Unknown";
  int abs[6] = {0};

  ioctl(fd, EVIOCGNAME(sizeof(name)), name);
  //printf("Input device name: \"%s\"\n", name);

  memset(bit, 0, sizeof(bit));
  ioctl(fd, EVIOCGBIT(0, EV_MAX), bit[0]);
  //printf("Supported events:\n");

  int i,j,k;
  int IsAtouchDevice=0;
  for (i = 0; i < EV_MAX; i++)
  {
    if (test_bit(i, bit[0]))
    {
      //printf("  Event type %d (%s)\n", i, events[i] ? events[i] : "?");
      if (!i) continue;
      ioctl(fd, EVIOCGBIT(i, KEY_MAX), bit[i]);
      for (j = 0; j < KEY_MAX; j++)
      {
        if (test_bit(j, bit[i]))
        {
          //printf("    Event code %d (%s)\n", j, names[i] ? (names[i][j] ? names[i][j] : "?") : "?");
          if(j==330) IsAtouchDevice=1;
          if (i == EV_ABS)
          {
            ioctl(fd, EVIOCGABS(j), abs);
            for (k = 0; k < 5; k++)
            {
              if ((k < 3) || abs[k])
              {
                //printf("     %s %6d\n", absval[k], abs[k]);
                if (j == 0)
                {
                  if ((strcmp(absval[k],"Min  ")==0)) *screenXmin =  abs[k];
                  if ((strcmp(absval[k],"Max  ")==0)) *screenXmax =  abs[k];
                }
                if (j == 1)
                {
                  if ((strcmp(absval[k],"Min  ")==0)) *screenYmin =  abs[k];
                  if ((strcmp(absval[k],"Max  ")==0)) *screenYmax =  abs[k];
                }
              }
            }
          }
        }
      }
    }
  }
  return IsAtouchDevice;
}


void TransformTouchMap(int x, int y)
{
  // This function takes the raw (0 - 4095 on each axis) touch data x and y
  // and transforms it to approx 0 - wscreen and 0 - hscreen in globals scaledX 
  // and scaledY prior to final correction by CorrectTouchMap  

  int shiftX, shiftY;
  double factorX, factorY;

  // Adjust registration of touchscreen for Waveshare
  shiftX=30; // move touch sensitive position left (-) or right (+).  Screen is 700 wide
  shiftY=-5; // move touch sensitive positions up (-) or down (+).  Screen is 480 high

  factorX=-0.4;  // expand (+) or contract (-) horizontal button space from RHS. Screen is 5.6875 wide
  factorY=-0.3;  // expand or contract vertical button space.  Screen is 8.53125 high

  // Switch axes for normal and waveshare displays
  if(Inversed==0) // Tontec35 or Element14_7
  {
    scaledX = x/scaleXvalue;
    scaledY = hscreen-y/scaleYvalue;
  }
  else //Waveshare (inversed)
  {
    scaledX = shiftX+wscreen-y/(scaleXvalue+factorX);

    if(strcmp(DisplayType, "Waveshare4") != 0) //Check for Waveshare 4 inch
    {
      scaledY = shiftY+hscreen-x/(scaleYvalue+factorY);
    }
    else  // Waveshare 4 inch display so flip vertical axis
    {
      scaledY = shiftY+x/(scaleYvalue+factorY); // Vertical flip for 4 inch screen
    }
  }
}


int IsButtonPushed(int NbButton, int x, int y)
{
  TransformTouchMap(x,y);  // Sorts out orientation and approx scaling of the touch map

  //printf("x=%d y=%d scaledx %d scaledy %d sxv %f syv %f Button %d\n",x,y,scaledX,scaledY,scaleXvalue,scaleYvalue, NbButton);

  int margin=10;  // was 20

  if((scaledX<=(ButtonArray[NbButton].x+ButtonArray[NbButton].w-margin))&&(scaledX>=ButtonArray[NbButton].x+margin) &&
    (scaledY<=(ButtonArray[NbButton].y+ButtonArray[NbButton].h-margin))&&(scaledY>=ButtonArray[NbButton].y+margin))
  {
    // ButtonArray[NbButton].LastEventTime=mymillis(); No longer used
    return 1;
  }
  else
  {
    return 0;
  }
}


int IsMenuButtonPushed(int x, int y)
{
  int  i, NbButton, cmo, cmsize;
  NbButton = -1;
  int margin=1;  // was 20 then 10
  cmo = ButtonNumber(CurrentMenu, 0); // Current Menu Button number Offset
  cmsize = ButtonNumber(CurrentMenu + 1, 0) - ButtonNumber(CurrentMenu, 0);
  TransformTouchMap(x,y);       // Sorts out orientation and approx scaling of the touch map

  //printf("x=%d y=%d scaledx %d scaledy %d sxv %f syv %f Button %d\n",x,y,scaledX,scaledY,scaleXvalue,scaleYvalue, NbButton);

  // For each button in the current Menu, check if it has been pushed.
  // If it has been pushed, return the button number.  If nothing valid has been pushed return -1
  // If it has been pushed, do something with the last event time

  for (i = 0; i <cmsize; i++)
  {
    if (ButtonArray[i + cmo].IndexStatus > 0)  // If button has been defined
    {
      //printf("Button %d, ButtonX = %d, ButtonY = %d\n", i, ButtonArray[i + cmo].x, ButtonArray[i + cmo].y);

      if  ((scaledX <= (ButtonArray[i + cmo].x + ButtonArray[i + cmo].w - margin))
        && (scaledX >= ButtonArray[i + cmo].x + margin)
        && (scaledY <= (ButtonArray[i + cmo].y + ButtonArray[i + cmo].h - margin))
        && (scaledY >= ButtonArray[i + cmo].y + margin))  // and touched
      {
        // ButtonArray[NbButton].LastEventTime=mymillis(); No longer used
        NbButton = i;          // Set the button number to return
        break;                 // Break out of loop as button has been found
      }
    }
  }
  return NbButton;
}


int InitialiseButtons()
{
  // Writes 0 to IndexStatus of each button to signify that it should not
  // be displayed.  As soon as a status (text and color) is added, IndexStatus > 0
  int i;
  for (i = 0; i <= MAX_BUTTON; i = i + 1)
  {
    ButtonArray[i].IndexStatus = 0;
  }
  return 1;
}


int AddButton(int x, int y, int w, int h)
{
  button_t *NewButton=&(ButtonArray[IndexButtonInArray]);
  NewButton->x=x;
  NewButton->y=y;
  NewButton->w=w;
  NewButton->h=h;
  NewButton->NoStatus=0;
  NewButton->IndexStatus=0;
  return IndexButtonInArray++;
}


int ButtonNumber(int MenuIndex, int Button)
{
  // Returns the Button Number (0 - 350) from the Menu number and the button position
  int ButtonNumb = 0;

  if (MenuIndex <= 20)  // 20 x 15-button main menus
  {
    ButtonNumb = (MenuIndex - 1) * 15 + Button;
  }
  if ((MenuIndex >= 41) && (MenuIndex <= 41))  // keyboard
  {
    ButtonNumb = 300 + (MenuIndex - 41) * 50 + Button;
  }
  if (MenuIndex == 42) //Edge Case
  {
    ButtonNumb = 350;
  }
  return ButtonNumb;
}


int CreateButton(int MenuIndex, int ButtonPosition)
{
  // Provide Menu number (int 1 - 41), Button Position (0 bottom left, 2 top right down to 11)
  // return button number

  // Menus 1 - 20 are classic 15-button menus
  // Menus 21 - 40 are undefined
  // Menu 41 is a keyboard
  // Menu 42 only exists as a boundary for Menu 41

  int ButtonIndex;
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  int normal_width = 120;
  int normal_xpos = 640;

  ButtonIndex = ButtonNumber(MenuIndex, ButtonPosition);

  if ((MenuIndex != 41) && (MenuIndex != 2) && (MenuIndex != 10) && (MenuIndex != 11))   // All except keyboard, Markers, Span
  {
    if (ButtonPosition == 0)  // Capture
    {
      x = 0;
      y = 0;
      w = 90;
      h = 55;
    }

    if ((ButtonPosition > 0) && (ButtonPosition < 9))  // 8 right hand buttons
    {
      x = normal_xpos;
      y = 480 - (ButtonPosition * 60);
      w = normal_width;
      h = 50;
    }
  }
  else if ((MenuIndex == 2) || (MenuIndex == 10) || (MenuIndex == 11))  // Marker Menu
  {
    if (ButtonPosition == 0)  // Capture
    {
      x = 0;
      y = 0;
      w = 90;
      h = 50;
    }

    if ((ButtonPosition > 0) && (ButtonPosition < 5))  // 4 right hand buttons
    {
      x = normal_xpos;
      y = 480 - (ButtonPosition * 60);
      w = normal_width;
      h = 50;
    }
    if (ButtonPosition == 5) // Left hand arrow
    {
      x = normal_xpos;  
      y = 480 - (5 * 60);
      w = 50;
      h = 50;
    }
    if (ButtonPosition == 6) // Right hand arrow
    {
      x = 710;  // = normal_xpos + 50 button width + 20 gap
      y = 480 - (5 * 60);
      w = 50;
      h = 50;
    }
    if ((ButtonPosition > 6) && (ButtonPosition < 10))  // Bottom 3 buttons
    {
      x = normal_xpos;
      y = 480 - ((ButtonPosition - 1) * 60);
      w = normal_width;
      h = 50;
    }
  }
  else  // Keyboard
  {
    w = 65;
    h = 59;

    if (ButtonPosition <= 9)  // Space bar and < > - Enter, Bkspc
    {
      switch (ButtonPosition)
      {
      case 0:                   // Space Bar
          y = 0;
          x = 165; // wscreen * 5 / 24;
          w = 362; // wscreen * 11 /24;
          break;
      case 1:                  // Not used
          y = 0;
          x = 528; //wscreen * 8 / 12;
          break;
      case 2:                  // <
          y = 0;
          x = 594; //wscreen * 9 / 12;
          break;
      case 3:                  // >
          y = 0;
          x = 660; // wscreen * 10 / 12;
          break;
      case 4:                  // -
          y = 0;
          x = 726; // wscreen * 11 / 12;
          break;
      case 5:                  // Clear
          y = 0;
          x = 0;
          w = 131; // 2 * wscreen/12;
          break;
      case 6:                 // Left Shift
          y = hscreen/8;
          x = 0;
          break;
      case 7:                 // Right Shift
          y = hscreen/8;
          x = 726; // wscreen * 11 / 12;
          break;
      case 8:                 // Enter
          y = 2 * hscreen/8;
          x = 660; // wscreen * 10 / 12;
          w = 131; // 2 * wscreen/12;
          h = 119;
          break;
      case 9:                 // Backspace
          y = 4 * hscreen / 8;
          x = 693; // wscreen * 21 / 24;
          w = 98; // 3 * wscreen/24;
          break;
      }
    }
    if ((ButtonPosition >= 10) && (ButtonPosition <= 19))  // ZXCVBNM,./
    {
      y = hscreen/8;
      x = (ButtonPosition - 9) * 66;
    }
    if ((ButtonPosition >= 20) && (ButtonPosition <= 29))  // ASDFGHJKL
    {
      y = 2 * hscreen / 8;
      x = ((ButtonPosition - 19) * 66) - 33;
    }
    if ((ButtonPosition >= 30) && (ButtonPosition <= 39))  // QWERTYUIOP
    {
      y = 3 * hscreen / 8;
      x = (ButtonPosition - 30) * 66;
    }
    if ((ButtonPosition >= 40) && (ButtonPosition <= 49))  // 1234567890
    {
      y = 4 * hscreen / 8;
      x = ((ButtonPosition - 39) * 66) - 33;
    }
  }

  button_t *NewButton=&(ButtonArray[ButtonIndex]);
  NewButton->x=x;
  NewButton->y=y;
  NewButton->w=w;
  NewButton->h=h;
  NewButton->NoStatus=0;
  NewButton->IndexStatus=0;

  return (ButtonIndex);
}


int AddButtonStatus(int ButtonIndex, char *Text, color_t *Color)
{
  button_t *Button=&(ButtonArray[ButtonIndex]);
  strcpy(Button->Status[Button->IndexStatus].Text,Text);
  Button->Status[Button->IndexStatus].Color=*Color;
  return Button->IndexStatus++;
}


void AmendButtonStatus(int ButtonIndex, int ButtonStatusIndex, char *Text, color_t *Color)
{
  button_t *Button=&(ButtonArray[ButtonIndex]);
  strcpy(Button->Status[ButtonStatusIndex].Text, Text);
  Button->Status[ButtonStatusIndex].Color=*Color;
}


void DrawButton(int ButtonIndex)
{
  button_t *Button=&(ButtonArray[ButtonIndex]);
  char label[255];
  char line1[15];
  char line2[15];

  // Look up the label
  strcpy(label, Button->Status[Button->NoStatus].Text);

  // Draw the basic button
  rectangle(Button->x, Button->y + 1, Button->w, Button->h, 
    Button->Status[Button->NoStatus].Color.r,
    Button->Status[Button->NoStatus].Color.g,
    Button->Status[Button->NoStatus].Color.b);

  // Set text and background colours
  setForeColour(255, 255, 255);				   // White text
  //setBackColour(Button->Status[Button->NoStatus].Color.r,
  //              Button->Status[Button->NoStatus].Color.g,
  //              Button->Status[Button->NoStatus].Color.b);

  // Separate button text into 2 lines if required  
  char find = '^';                                  // Line separator is ^
  const char *ptr = strchr(label, find);            // pointer to ^ in string

  if((ptr) && (CurrentMenu != 41))                  // if ^ found then
  {                                                 // 2 lines
    int index = ptr - label;                        // Position of ^ in string
    snprintf(line1, index+1, label);                // get text before ^
    snprintf(line2, strlen(label) - index, label + index + 1);  // and after ^

    // Display the text on the button
    pthread_mutex_lock(&text_lock);
    TextMid3(Button->x+Button->w/2, Button->y+Button->h*11/16, line1, &font_dejavu_sans_20,
      Button->Status[Button->NoStatus].Color.r,
      Button->Status[Button->NoStatus].Color.g,
      Button->Status[Button->NoStatus].Color.b);	
    TextMid3(Button->x+Button->w/2, Button->y+Button->h*3/16, line2, &font_dejavu_sans_20,
      Button->Status[Button->NoStatus].Color.r,
      Button->Status[Button->NoStatus].Color.g,
      Button->Status[Button->NoStatus].Color.b);	
    pthread_mutex_unlock(&text_lock);
  }
  else                                              // One line only
  {
    pthread_mutex_lock(&text_lock);
    if ((CurrentMenu <= 9) && (CurrentMenu != 4))
    {
      TextMid3(Button->x+Button->w/2, Button->y+Button->h/2, label, &font_dejavu_sans_20,
      Button->Status[Button->NoStatus].Color.r,
      Button->Status[Button->NoStatus].Color.g,
      Button->Status[Button->NoStatus].Color.b);
    }
    else if (CurrentMenu == 41)  // Keyboard
    {
      TextMid3(Button->x+Button->w/2, Button->y+Button->h/2 - hscreen / 64, label, &font_dejavu_sans_28,
      Button->Status[Button->NoStatus].Color.r,
      Button->Status[Button->NoStatus].Color.g,
      Button->Status[Button->NoStatus].Color.b);
    }
    else // fix text size at 20
    {
      TextMid3(Button->x+Button->w/2, Button->y+Button->h/2, label, &font_dejavu_sans_20,
      Button->Status[Button->NoStatus].Color.r,
      Button->Status[Button->NoStatus].Color.g,
      Button->Status[Button->NoStatus].Color.b);
    }
    pthread_mutex_unlock(&text_lock);
  }
  setBackColour(0, 0, 0);  // Back to black!
}


void SetButtonStatus(int ButtonIndex, int Status)
{
  button_t *Button=&(ButtonArray[ButtonIndex]);
  Button->NoStatus=Status;
}


int GetButtonStatus(int ButtonIndex)
{
  button_t *Button=&(ButtonArray[ButtonIndex]);
  return Button->NoStatus;
}


int getTouchSampleThread(int *rawX, int *rawY, int *rawPressure)
{
  int i;
  static bool awaitingtouchstart;
  static bool touchfinished;

  if (touchneedsinitialisation == true)
  {
    awaitingtouchstart = true;
    touchfinished = true;
    touchneedsinitialisation = false;
  }

  /* how many bytes were read */
  size_t rb;

  /* the events (up to 64 at once) */
  struct input_event ev[64];

  if (((strcmp(DisplayType, "Element14_7") == 0) || (strcmp(DisplayType, "Browser") == 0))
      && (strcmp(DisplayType, "dfrobot5") != 0))   // Browser or Element14_7, but not dfrobot5
  {
    // Thread flow blocks here until there is a touch event
    rb = read(fd, ev, sizeof(struct input_event) * 64);

    *rawX = -1;
    *rawY = -1;
    int StartTouch = 0;

    for (i = 0;  i <  (rb / sizeof(struct input_event)); i++)
    {
      if (ev[i].type ==  EV_SYN)
      {
        //printf("Event type is %s%s%s = Start of New Event\n",
        //        KYEL, events[ev[i].type], KWHT);
      }

      else if (ev[i].type == EV_KEY && ev[i].code == 330 && ev[i].value == 1)
      {
        StartTouch = 1;
        //printf("Event type is %s%s%s & Event code is %sTOUCH(330)%s & Event value is %s1%s = Touch Starting\n",
        //        KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,KWHT);
      }

      else if (ev[i].type == EV_KEY && ev[i].code == 330 && ev[i].value == 0)
      {
        //StartTouch=0;
        //printf("Event type is %s%s%s & Event code is %sTOUCH(330)%s & Event value is %s0%s = Touch Finished\n",
        //        KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,KWHT);
      }

      else if (ev[i].type == EV_ABS && ev[i].code == 0 && ev[i].value > 0)
      {
        //printf("Event type is %s%s%s & Event code is %sX(0)%s & Event value is %s%d%s\n",
        //        KYEL, events[ev[i].type], KWHT, KYEL, KWHT, KYEL, ev[i].value, KWHT);
	    *rawX = ev[i].value;
      }

      else if (ev[i].type == EV_ABS  && ev[i].code == 1 && ev[i].value > 0)
      {
        //printf("Event type is %s%s%s & Event code is %sY(1)%s & Event value is %s%d%s\n",
        //        KYEL, events[ev[i].type], KWHT, KYEL, KWHT, KYEL, ev[i].value, KWHT);
        *rawY = ev[i].value;
      }

      else if (ev[i].type == EV_ABS  && ev[i].code == 24 && ev[i].value > 0)
      {
        //printf("Event type is %s%s%s & Event code is %sPressure(24)%s & Event value is %s%d%s\n",
        //        KYEL, events[ev[i].type], KWHT, KYEL, KWHT, KYEL, ev[i].value,KWHT);
        *rawPressure = ev[i].value;
      }

      if((*rawX != -1) && (*rawY != -1) && (StartTouch == 1))  // 1a
      {
        printf("7 inch Touchscreen Touch Event: rawX = %d, rawY = %d, rawPressure = %d\n", 
                *rawX, *rawY, *rawPressure);
        return 1;
      }
    }
  }

  if (strcmp(DisplayType, "dfrobot5") == 0)
  {
    // Program flow blocks here until there is a touch event
    rb = read(fd, ev, sizeof(struct input_event) * 64);

    if (awaitingtouchstart == true)
    {    
      *rawX = -1;
      *rawY = -1;
      touchfinished = false;
    }

    for (i = 0;  i <  (rb / sizeof(struct input_event)); i++)
    {
      //printf("rawX = %d, rawY = %d, rawPressure = %d, \n\n", *rawX, *rawY, *rawPressure);

      if (ev[i].type ==  EV_SYN)
      {
        //printf("Event type is %s%s%s = Start of New Event\n",
        //        KYEL, events[ev[i].type], KWHT);
      }

      else if (ev[i].type == EV_KEY && ev[i].code == 330 && ev[i].value == 1)
      {
        awaitingtouchstart = false;
        touchfinished = false;

        //printf("Event type is %s%s%s & Event code is %sTOUCH(330)%s & Event value is %s1%s = Touch Starting\n",
        //        KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,KWHT);
      }

      else if (ev[i].type == EV_KEY && ev[i].code == 330 && ev[i].value == 0)
      {
        awaitingtouchstart = false;
        touchfinished = true;

        //printf("Event type is %s%s%s & Event code is %sTOUCH(330)%s & Event value is %s0%s = Touch Finished\n",
        //        KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,KWHT);
      }

      else if (ev[i].type == EV_ABS && ev[i].code == 0 && ev[i].value > 0)
      {
        //printf("Event type is %s%s%s & Event code is %sX(0)%s & Event value is %s%d%s\n",
        //        KYEL, events[ev[i].type], KWHT, KYEL, KWHT, KYEL, ev[i].value, KWHT);
        *rawX = ev[i].value;
      }

      else if (ev[i].type == EV_ABS  && ev[i].code == 1 && ev[i].value > 0)
      {
        //printf("Event type is %s%s%s & Event code is %sY(1)%s & Event value is %s%d%s\n",
        //        KYEL, events[ev[i].type], KWHT, KYEL, KWHT, KYEL, ev[i].value, KWHT);
        *rawY = ev[i].value;
      }

      else if (ev[i].type == EV_ABS  && ev[i].code == 24 && ev[i].value > 0)
      {
        //printf("Event type is %s%s%s & Event code is %sPressure(24)%s & Event value is %s%d%s\n",
        //        KYEL, events[ev[i].type], KWHT, KYEL, KWHT, KYEL, ev[i].value,KWHT);
        *rawPressure = ev[i].value;
      }

      if((*rawX != -1) && (*rawY != -1) && (touchfinished == true))  // 1a
      {
        printf("DFRobot Touch Event: rawX = %d, rawY = %d, rawPressure = %d\n", 
                *rawX, *rawY, *rawPressure);
        awaitingtouchstart = true;
        touchfinished = false;
        return 1;
      }
    }
  }
  return 0;
}


int getTouchSample(int *rawX, int *rawY, int *rawPressure)
{
  while (true)
  {
    if (TouchTrigger == 1)
    {
      *rawX = TouchX;
      *rawY = TouchY;
      *rawPressure = TouchPressure;
      printf("Touchtrigger was 1\n");
      TouchTrigger = 0;
      return 1;
    }
    else if ((webcontrol == true) && (strcmp(WebClickForAction, "yes") == 0))
    {
      *rawX = web_x;
      *rawY = web_y;
      *rawPressure = 0;
      strcpy(WebClickForAction, "no");
      printf("Web rawX = %d, rawY = %d, rawPressure = %d\n", *rawX, *rawY, *rawPressure);
      return 1;
    }
    else
    {
      usleep(1000);
    }
  }
  return 0;
}


void UpdateWindow()    // Paint each defined button
{
  int i;
  int first;
  int last;

  // Draw a black rectangle where the buttons are to erase them
  rectangle(620, 0, 160, 480, 0, 0, 0);
  
  // Draw each button in turn
  first = ButtonNumber(CurrentMenu, 0);
  last = ButtonNumber(CurrentMenu + 1 , 0) - 1;

  for(i = first; i <= last; i++)
  {
    if (ButtonArray[i].IndexStatus > 0)  // If button needs to be drawn
    {
      DrawButton(i);                     // Draw the button
    }
  }
  UpdateWeb();
}


void wait_touch()
// Wait for Screen touch, ignore position, but then move on
// Used to let user acknowledge displayed text
{
  int rawX, rawY, rawPressure;
  printf("wait_touch called\n");

  // Check if screen touched, if not, wait 0.1s and check again
  while(getTouchSample(&rawX, &rawY, &rawPressure)==0)
  {
    usleep(100000);
  }
  // Screen has been touched
  printf("wait_touch exit\n");
}


void CalculateMarkers()
{
//int markerx = 250;
//int markery = 15;
//bool markeron = false;
//int markermode = 7;       // 2 peak, 3, null, 4 man, 7 off

  int i;
  int maxy;
  int xformaxy = 0;
  int xsum = 0;
  int ysum = 0;
  int markersamples = 10;
  char markerlevel[31];
  char markerfreq[31];
  float markerlev;
  float markerf;

  switch (markermode)
  {
    case 2:  // peak
      maxy = 0;
      for (i = 1; i < 500; i++)
      {
         if((y[i] + 1) > maxy)
         {
           maxy = y[i] + 1;
           xformaxy = i;
         }
      }
    break;

    case 3:  // null
      maxy = 400;
      for (i = 1; i < 500; i++)
      {
         if((y[i] + 1) < maxy)
         {
           maxy = y[i] + 1;
           xformaxy = i;
         }
      }
    break;

    case 4:  // manual
      maxy = 0;
      maxy = y[manualmarkerx] + 1;
      xformaxy = manualmarkerx;
    break;
  }

  // Now smooth the marker 
  markerxhistory[historycount] = xformaxy;
  markeryhistory[historycount] = maxy;

  for (i = 0; i < markersamples; i++)
  {
    xsum = xsum + markerxhistory[i];
    ysum = ysum + markeryhistory[i];
  }

  historycount++;
  if (historycount > (markersamples - 1))
  {
    historycount = 0;
  }
  markerx = 100 + (xsum / markersamples);
  markery = 410 - (ysum / markersamples);

  // And display it
  markerlev = ((float)(410 - markery) / 5) * (PlotMax - PlotBase) + PlotBase;
  rectangle(620, 420, 160, 60, 0, 0, 0);  // Blank the Menu title
  snprintf(markerlevel, 31, "Mkr %0.3f", markerlev);
  pthread_mutex_lock(&text_lock);
  TextMid2(700, 450, markerlevel, &font_dejavu_sans_18);
  pthread_mutex_unlock(&text_lock);

  markerf = (float)(markerx - 100); 
  snprintf(markerfreq, 31, "%d/500", (uint16_t)markerf);
  setBackColour(0, 0, 0);
  pthread_mutex_lock(&text_lock);
  TextMid2(700, 425, markerfreq, &font_dejavu_sans_18);
  pthread_mutex_unlock(&text_lock);
}


void ChangeLabel(int button)
{
  char RequestText[64];
  char InitText[64];
  
  // Stop the scan at the end of the current one and wait for it to stop
  freeze = true;
  while(! frozen)
  {
    usleep(10);                                   // wait till the end of the scan
  }

  switch (button)
  {
    case 2:                                                       // Start Freq
      break;

    case 3:                                                       // Stop Freq
      break;
    case 4:                                                       // Ref Level
      break;
    case 5:                                                       // 
      break;
    case 6:                                                       // Plot Title
      strcpy(RequestText, "Enter the title to be displayed on this plot");
      if(strcmp(PlotTitle, "-") == 0)           // Active Blank
      {
        InitText[0] = '\0';
      }
      else
      {
        snprintf(InitText, 63, "%s", PlotTitle);
      }

      Keyboard(RequestText, InitText, 40);
  
      if(strlen(KeyboardReturn) > 0)
      {
        strcpy(PlotTitle, KeyboardReturn);
      }
      else
      {
        strcpy(PlotTitle, "-");
      }
      SetConfigParam(PATH_DMMCONFIG, "title", PlotTitle);
      printf("Plot Title set to: %s\n", PlotTitle);
      break;
    case 7:                                                       // Set Normalise Level
      // Define request string
      break;

    case 16:                                                       // Not used
      break;

    case 12:                                                       // External Attenuator
      // Define request string
      break;

    case 13:                                                       // Cal Factor
      // Define request string
      break;
  }

  ModeChanged = true;
  freeze = false;
}


void ChangeSpan(int menu, int button)
{
  int PreviousPlotDuration;
  PreviousPlotDuration = PlotDuration;
  char PlotDurationText[15];

  if (menu == 11)
  {
    button = button + 10;
  }

  switch (button)
  {
    case 2:                                            // Min
      PlotDuration = 1;
      break;
    case 3:                                            // 5 minutes
      PlotDuration = 5;
      break;
    case 4:                                            // 10 minutes
      PlotDuration = 10;
      break;
    case 5:                                            // 20 minutes
      PlotDuration = 20;
      break;
    case 6:                                            // 50 minutes
      PlotDuration = 50;
      break;
    case 7:                                            // 100 minutes
      PlotDuration = 100;
      break;
    case 12:                                            // 1 hour
      PlotDuration = 60;
      break;
    case 13:                                            // 2 hours
      PlotDuration = 120;
      break;
    case 14:                                            // 5 hours
      PlotDuration = 300;
      break;
    case 15:                                            // 10 hours
      PlotDuration = 600;
      break;
    case 16:                                            // 20 hours
      PlotDuration = 1200;
      break;
    case 17:                                            // 50 hours
      PlotDuration = 3000;
      break;
   }

  if (PreviousPlotDuration != PlotDuration)
  {
    snprintf(PlotDurationText, 10, "%d", PlotDuration);
    SetConfigParam(PATH_DMMCONFIG, "plotduration", PlotDurationText);
    printf("Plot Duration set to %d minutes\n", PlotDuration);
  }
}


void ChangePlot(int button)
{
  char RequestText[63];
  char InitText[31];
  char ValueToSave[31];
  float ReturnedPlotBase;
  float ReturnedPlotMax = 0;

  freeze = true;
  while(! frozen)
  {
    usleep(10);                                   // wait till the end of the scan
  }

  switch (button)
  {
    case 2:                                                       // Plot Baseline
      // Define request string
      strcpy(RequestText, "Enter Value for Plot Baseline");

      // Define initial value
      snprintf(InitText, 10, "%.3f", PlotBase);

      // set up invalid returned value (nan)
      ReturnedPlotBase = 0.0 / 0.0;

      // Ask for the new value
      while (ReturnedPlotBase != ReturnedPlotBase)
      {
        Keyboard(RequestText, InitText, 10);
        ReturnedPlotBase = atof(KeyboardReturn);
      }
      PlotBase = ReturnedPlotBase;
      snprintf(ValueToSave, 10, "%.3f", PlotBase);
      SetConfigParam(PATH_DMMCONFIG, "plotbase", ValueToSave);
      printf("PlotBase set to %.3f\n", PlotBase);
      break;
    case 3:                                                       // Plot Max
      // Define request string
      strcpy(RequestText, "Enter value for Plot Maximum");

      // Define initial value
      snprintf(InitText, 10, "%.3f", PlotMax);

      // set up invalid returned value (nan)
      ReturnedPlotMax= 0.0 / 0.0;

      // Ask for the new value
      while (ReturnedPlotMax != ReturnedPlotMax)
      {
        Keyboard(RequestText, InitText, 10);
        ReturnedPlotMax = atof(KeyboardReturn);
      }
      PlotMax = ReturnedPlotMax;
      snprintf(ValueToSave, 10, "%.3f", PlotMax);
      SetConfigParam(PATH_DMMCONFIG, "plotmax", ValueToSave);
      printf("plot Max set to %.3f\n", PlotMax);
      break;
  }
  ModeChanged = true;
  freeze = false;
}

void ChangeDischarge(int button)
{
  char RequestText[63];
  char InitText[31];
  char ValueToSave[31];
  float ReturnedInitialCurrent = 0;
  float ReturnedInitialDischarge = 0;

  freeze = true;
  while(! frozen)
  {
    usleep(10);                                   // wait till the end of the scan
  }

  switch (button)
  {
    case 2:                                                       // Initial Current
      // Define request string
      strcpy(RequestText, "Enter Initial Discharge Current in Amps");

      // Define initial value
      snprintf(InitText, 10, "%.3f", InitialCurrent);

      // Ask for the new value
      while ((ReturnedInitialCurrent < 0.001) || (ReturnedInitialCurrent > 100.0))
      {
        Keyboard(RequestText, InitText, 10);
        ReturnedInitialCurrent = atof(KeyboardReturn);
      }
      InitialCurrent = ReturnedInitialCurrent;
      snprintf(ValueToSave, 10, "%.3f", InitialCurrent);
      SetConfigParam(PATH_DMMCONFIG, "initialcurrent", ValueToSave);
      printf("Initial Current set to %.3f Amps \n", InitialCurrent);
      break;
    case 4:                                                       // Discharged Voltage
      // Define request string
      strcpy(RequestText, "Enter Lowest Discharge Voltage");

      // Define initial value
      snprintf(InitText, 10, "%.3f", Discharged);

      // Ask for the new value
      while ((ReturnedInitialDischarge < 0.001) || (ReturnedInitialDischarge > 300.0))
      {
        Keyboard(RequestText, InitText, 10);
        ReturnedInitialDischarge = atof(KeyboardReturn);
      }
      Discharged = ReturnedInitialDischarge;
      snprintf(ValueToSave, 10, "%.3f", Discharged);
      SetConfigParam(PATH_DMMCONFIG, "discharged", ValueToSave);
      printf("Full discharge set to %.3f volts \n", Discharged);
      break;
  }
  ModeChanged = true;
  freeze = false;
}


void CalculateCapacityUsed(float InstantaneousVoltage, uint64_t MeasurementPeriod)  // period in mS
{
  float InstantaneousCurrent;

  if (ConstantCurrent == false)           // Resistive load
  {
    InstantaneousCurrent = InitialCurrent * (InstantaneousVoltage / InitialVoltage);
  }
  else
  {
    InstantaneousCurrent = InitialCurrent;
  }

  Cumulative_ma_ms = Cumulative_ma_ms + (uint64_t)(1000.0 * InstantaneousCurrent) * MeasurementPeriod;
  CumulativeAH = ((float)Cumulative_ma_ms)/3600000000.0;

  //printf("ma_ms = %llu, AH = %f\n", Cumulative_ma_ms, CumulativeAH);
}


void CheckDischargeLimit(float InstantaneousVoltage)
{
  if ((InstantaneousVoltage < Discharged) && (InstantaneousVoltage > 0.5))
  {
    DischargeComplete = true;                         // Set flag
    DischargeInProgress = false;                      // and another
    system("/home/pi/rpidatv/scripts/snap2.sh");      // Take a snap
    usleep(100000);                                   // Wait for the snap
    digitalWrite(LoadSwitchGPIO, LOW);                // Turn the load off
    LoadSwitchOn = false;                             // Tell the system that the load is off
  }
}


void SaveCSV()
{
  int FirstSample;
  int LastSample;
  char SampleLine[63];
  char CopyCommand[127];
  int i;
  FILE *fPtr;

  system("rm /home/pi/tmp/dmmdata.csv >/dev/null 2>/dev/null");
  
  if ((NewestReading < 0) || (strcmp(DMMMode, "battery") == 0))
  {
    FirstSample = 0;
    LastSample = 500;
  }
  else                           // continuous plot
  {
    FirstSample = NewestReading + 1;
    LastSample = NewestReading;
  }

  fPtr = fopen("/home/pi/tmp/dmmdata.csv", "a");

  for (i = FirstSample; i <= 500; i++)
  {
    snprintf(SampleLine, 15, "%.3f, ", reading[i]);
    strcat(SampleLine, readingtime[i]);
    strcat(SampleLine, "\r\n");
    fputs(SampleLine, fPtr);
  }

  if (FirstSample > 0)
  {
    for (i = 0; i <= LastSample; i++)
    {
      snprintf(SampleLine, 15, "%.3f, ", reading[i]);
      strcat(SampleLine, readingtime[i]);
      strcat(SampleLine, "\r\n");
      fputs(SampleLine, fPtr);
    }
  }
  fclose(fPtr);

  t = time(NULL);
  tm = localtime(&t);
  char s[64];
  size_t ret;
  ret = strftime(s, sizeof(s), "%Y%m%d-%H%M%S", tm);
  assert(ret);

  strcpy(CopyCommand, "cp /home/pi/tmp/dmmdata.csv /home/pi/snaps/dmmdata-");
  strcat(CopyCommand, s);
  strcat(CopyCommand, ".csv");

  //printf("%s\n", CopyCommand);
  system(CopyCommand);
}

void ResetCSVFile()
{
  int i;

  for (i = 0; i <= 500; i++)
  {
    reading[i] = 0.0;
    strcpy(readingtime[i], "\0");
  }
}


//////////////////////////////////////// DEAL WITH TOUCH EVENTS //////////////////////////////////////


void *WaitButtonEvent(void * arg)
{
  int i;
  int  rawPressure;

  for (;;)
  {
    while(getTouchSample(&rawX, &rawY, &rawPressure)==0)
    {
      usleep(10);                                   // wait without burnout
    }

    printf("x=%d y=%d\n", rawX, rawY);
    FinishedButton = 1;
    i = IsMenuButtonPushed(rawX, rawY);
    if (i == -1)
    {
      continue;  //Pressed, but not on a button so wait for the next touch
    }
    // Now do the reponses for each Menu in turn

    if (CurrentMenu == 1)  // Main Menu
    {
      printf("Button Event %d, Entering Menu 1 Case Statement\n", i);
      CallingMenu = 1;
      switch (i)
      {
        case 0:                                            // Capture Snap
          freeze = true; 
          SetButtonStatus(ButtonNumber(CurrentMenu, 0), 1);  // hide the capture button 
          UpdateWindow();                                    // paint the hide
          while(! frozen);                                   // wait till the end of the scan
          system("/home/pi/rpidatv/scripts/snap2.sh");
          SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0);
          UpdateWindow();
          freeze = false;
          break;
        case 2:                                            // Select Mode Menu
          printf("Mode Menu 5 Requested\n");
          CurrentMenu = 5;
          Start_Highlights_Menu5();
          UpdateWindow();
          break;
        case 3:                                            // Select Action Menu
          printf("Action Menu 9 Requested\n");
          CurrentMenu=9;
          Start_Highlights_Menu9();
          UpdateWindow();
          break;
        case 4:                                            // Select Settings Menu
          printf("Settings Menu 3 Requested\n");
          CurrentMenu=3;
          Start_Highlights_Menu3();
          UpdateWindow();
          break;
        case 5:                                            // Select Discharge Set-up Menu
          printf("Discharge Set-up Menu 7 Requested\n");
          CurrentMenu = 7;
          Start_Highlights_Menu7();
          UpdateWindow();
          break;
        case 6:                                            // Select System Menu
          printf("System Menu 4 Requested\n");
          CurrentMenu=4;
          Start_Highlights_Menu4();
          UpdateWindow();
          break;
        case 7:                                            // Exit to Portsdown
          if(PortsdownExitRequested)
          {
            freeze = true;
            usleep(100000);
            setBackColour(0, 0, 0);
            wipeScreen(0, 0, 0);
            UpdateWeb();
            usleep(1000000);
            closeScreen();
            cleanexit(207);
          }
          else
          {
            PortsdownExitRequested = true;
            Start_Highlights_Menu1();
            UpdateWindow();
          }
          break;
        case 8:
          if (freeze)
          {
            SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0);
            freeze = false;
          }
          else
          {
            SetButtonStatus(ButtonNumber(CurrentMenu, 8), 1);
            freeze = true;
          }
          UpdateWindow();
          break;
        default:
          printf("Menu 1 Error\n");
      }
      if ((i != 7) && (PortsdownExitRequested == true)) // Cancel exit request
      {
        PortsdownExitRequested = false;
        Start_Highlights_Menu1();
        UpdateWindow();
      }
      continue;  // Completed Menu 1 action, go and wait for touch
    }

    if (CurrentMenu == 2)  // XY Marker Menu
    {
      printf("Button Event %d, Entering Menu 2 Case Statement\n",i);
      CallingMenu = 2;
      switch (i)
      {
        case 0:                                            // Capture Snap
          freeze = true; 
          SetButtonStatus(ButtonNumber(CurrentMenu, 0), 1);
          UpdateWindow();
          while(! frozen);
          system("/home/pi/rpidatv/scripts/snap2.sh");
          SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0);
          UpdateWindow();
          freeze = false;
          break;
        case 2:                                            // Peak
          if ((markeron == false) || (markermode != 2))
          {
            markeron = true;
            markermode = i;
            SetButtonStatus(ButtonNumber(CurrentMenu, 2), 1);
            SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0);
            SetButtonStatus(ButtonNumber(CurrentMenu, 4), 0);
          }
          else
          {
            markeron = false;
            markermode = 7;
            SetButtonStatus(ButtonNumber(CurrentMenu, 2), 0);
            SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0);
            SetButtonStatus(ButtonNumber(CurrentMenu, 4), 0);
          }
          UpdateWindow();
          break;
        case 3:                                            // Null
          if ((markeron == false) || (markermode != 3))
          {
            markeron = true;
            markermode = i;
            SetButtonStatus(ButtonNumber(CurrentMenu, 2), 0);
            SetButtonStatus(ButtonNumber(CurrentMenu, 3), 1);
            SetButtonStatus(ButtonNumber(CurrentMenu, 4), 0);
          }
          else
          {
            markeron = false;
            markermode = 7;
            SetButtonStatus(ButtonNumber(CurrentMenu, 2), 0);
            SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0);
            SetButtonStatus(ButtonNumber(CurrentMenu, 4), 0);
          }
          UpdateWindow();
          break;
        case 4:                                            // Manual
          if ((markeron == false) || (markermode != 4))
          {
            markeron = true;
            markermode = i;
            SetButtonStatus(ButtonNumber(CurrentMenu, 2), 0);
            SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0);
            SetButtonStatus(ButtonNumber(CurrentMenu, 4), 1);
          }
          else
          {
            markeron = false;
            markermode = 7;
            SetButtonStatus(ButtonNumber(CurrentMenu, 2), 0);
            SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0);
            SetButtonStatus(ButtonNumber(CurrentMenu, 4), 0);
          }
          UpdateWindow();
          break;
        case 5:                                            // Left Arrow
          if(manualmarkerx > 10)
          {
            manualmarkerx = manualmarkerx - 10;
          }
          break;
        case 6:                                            // Right Arrow
          if(manualmarkerx < 490)
          {
            manualmarkerx = manualmarkerx + 10;
          }
          break;
        case 7:                                            // No Markers
          markeron = false;
          markermode = i;
          SetButtonStatus(ButtonNumber(CurrentMenu, 2), 0);
          SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0);
          SetButtonStatus(ButtonNumber(CurrentMenu, 4), 0);
          UpdateWindow();
          break;
        case 8:                                            // Return to Main Menu
          printf("Main Menu 1 Requested\n");
          CurrentMenu = 1;
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 9:
          if (freeze)
          {
            SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0);
            freeze = false;
          }
          else
          {
            SetButtonStatus(ButtonNumber(CurrentMenu, 9), 1);
            freeze = true;
          }
          UpdateWindow();
          break;
        default:
          printf("Menu 2 Error\n");
      }
      continue;  // Completed Menu 2 action, go and wait for touch
    }

    if (CurrentMenu == 3)  // Settings Menu
    {
      printf("Button Event %d, Entering Menu 3 Case Statement\n",i);
      CallingMenu = 3;
      switch (i)
      {
        case 0:                                            // Capture Snap
          freeze = true; 
          SetButtonStatus(ButtonNumber(CurrentMenu, 0), 1);
          UpdateWindow();
          while(! frozen);
          system("/home/pi/rpidatv/scripts/snap2.sh");
          SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0);
          UpdateWindow();
          freeze = false;
          break;
        case 2:                                            // Plot Baseline
        case 3:                                            // Plot max
          ChangePlot(i);
          UpdateWindow();
          break;
        case 4:                                            // Plot Duration Minutes
          printf("Plot Duration Minutes Menu 10 Requested\n");
          CurrentMenu = 10;
          Start_Highlights_Menu10();
          UpdateWindow();
          break;
        case 5:                                            // Plot Duration Hours
          printf("Plot Duration Hours Menu 11 Requested\n");
          CurrentMenu = 11;
          Start_Highlights_Menu11();
          UpdateWindow();
          break;
        case 6:                                            // More Settings
          printf("Settings 2 Menu 6 Requested\n");
          CurrentMenu = 6;
          Start_Highlights_Menu6();
          UpdateWindow();
          break;
        case 7:                                            // Return to Main Menu
          printf("Main Menu 1 Requested\n");
          CurrentMenu = 1;
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 8:
          if (freeze)
          {
            SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0);
            freeze = false;
          }
          else
          {
            SetButtonStatus(ButtonNumber(CurrentMenu, 8), 1);
            freeze = true;
          }
          UpdateWindow();
          break;
        default:
          printf("Menu 3 Error\n");
      }
      continue;  // Completed Menu 3 action, go and wait for touch
    }

    if (CurrentMenu == 4)  //  System Menu (DMM)
    {
      printf("Button Event %d, Entering Menu 4 Case Statement\n",i);
      CallingMenu = 4;
      switch (i)
      {
        case 0:                                            // Capture Snap
          freeze = true; 
          SetButtonStatus(ButtonNumber(CurrentMenu, 0), 1);
          UpdateWindow();
          while(! frozen);
          system("/home/pi/rpidatv/scripts/snap2.sh");
          SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0);
          UpdateWindow();
          freeze = false;
          break;
        case 2:                                            // Snap Viewer
          freeze = true; 
          while (frozen == false)
          {
            usleep(10);
          }
          do_snapcheck();
          initScreen();
          ModeChanged = true;                              // redraw after snap viewer
          UpdateWindow();
          freeze = false;
          break;
        case 3:                                            // Markers Menu
          printf("Markers Menu 2 Requested\n");
          CurrentMenu = 2;
          Start_Highlights_Menu2();
          UpdateWindow();
          break;
        case 4:                                            // Shutdown
          system("sudo shutdown now");
          break;
        //case 5:                                            // Spare
          //break;
        case 6:                                            // Restart this App
          freeze = true;
          usleep(100000);
          wipeScreen(0, 0, 0);
          usleep(1000000);
          closeScreen();
          cleanexit(142);
          break;
        case 7:                                            // Return to Main Menu
          printf("Main Menu 1 Requested\n");
          CurrentMenu = 1;
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 8:
          if (freeze)
          {
            SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0);
            freeze = false;
          }
          else
          {
            SetButtonStatus(ButtonNumber(CurrentMenu, 8), 1);
            freeze = true;
          }
          UpdateWindow();
          break;
        default:
          printf("Menu 4 Error\n");
      }
      continue;  // Completed Menu 4 action, go and wait for touch
    }

    if (CurrentMenu == 5)  // Mode Menu
    {
      printf("Button Event %d, Entering Menu 5 Case Statement\n",i);
      CallingMenu = 5;
      Start_Highlights_Menu5();
      UpdateWindow();
      switch (i)
      {
        case 0:                                            // Capture Snap
          freeze = true; 
          SetButtonStatus(ButtonNumber(CurrentMenu, 0), 1);
          UpdateWindow();
          while(! frozen);
          system("/home/pi/rpidatv/scripts/snap2.sh");
          SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0);
          UpdateWindow();
          freeze = false;
          break;
        case 2:                                            // DMM Digital Mode
          strcpy(DMMMode, "numeric");
          SetConfigParam(PATH_DMMCONFIG, "dmmmode", "numeric");
          Start_Highlights_Menu5();
          UpdateWindow();
          usleep(500000);
          printf("Returning to MENU 1 from Menu 5\n");
          CurrentMenu = 1;
          Start_Highlights_Menu1();
          ModeChanged = true;
          UpdateWindow();
          break;
        case 3:                                            // Continuous Graph Mode
          strcpy(DMMMode, "graph");
          SetConfigParam(PATH_DMMCONFIG, "dmmmode", "graph");
          Start_Highlights_Menu5();
          UpdateWindow();
          usleep(500000);
          printf("Returning to MENU 3 from Menu 5\n");
          CurrentMenu = 3;
          Start_Highlights_Menu3();
          ModeChanged = true;
          UpdateWindow();
          break;
        case 4:                                            // Battery Plot Mode
          strcpy(DMMMode, "battery");
          SetConfigParam(PATH_DMMCONFIG, "dmmmode", "battery");
          Start_Highlights_Menu5();
          UpdateWindow();
          usleep(500000);
          printf("Returning to MENU 7 from Menu 5\n");
          CurrentMenu = 7;
          Start_Highlights_Menu7();
          ModeChanged = true;
          UpdateWindow();
          break;
        //case 5:                                            // Not used
          //break;
        //case 6:                                            // 
        //  break;
        case 7:                                            // Return to Main Menu
          printf("Main Menu 1 Requested\n");
          CurrentMenu = 1;
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 8:
          if (freeze)
          {
            SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0);
            freeze = false;
          }
          else
          {
            SetButtonStatus(ButtonNumber(CurrentMenu, 8), 1);
            freeze = true;
          }
          UpdateWindow();
          break;
        default:
          printf("Menu 5 Error\n");
      }
      continue;  // Completed Menu 5 action, go and wait for touch
    }

    if (CurrentMenu == 6)  // More Settings
    {
      printf("Button Event %d, Entering Menu 6 Case Statement\n",i);
      CallingMenu = 6;
      switch (i)
      {
        case 0:                                            // Capture Snap
          freeze = true; 
          SetButtonStatus(ButtonNumber(CurrentMenu, 0), 1);  // hide the capture button 
          UpdateWindow();                                    // paint the hide
          while(! frozen);                                   // wait till the end of the scan
          system("/home/pi/rpidatv/scripts/snap2.sh");
          SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0);
          UpdateWindow();
          freeze = false;
          break;
        case 2:                                            // Single/Multi scan
          if (SingleScan == true)
          {
            SingleScan = false;
            SetConfigParam(PATH_DMMCONFIG, "singlescan", "false");
          }
          else
          {
            SingleScan = true;
            SetConfigParam(PATH_DMMCONFIG, "singlescan", "true");
          }
          Start_Highlights_Menu6();
          UpdateWindow();
          break;
        case 3:                                            // Toggle meter type
          if (strcmp(MeterSensor, "pdm300") == 0)
          {
            strcpy(MeterSensor, "maplin");
            SetConfigParam(PATH_DMMCONFIG, "metersensor", "maplin");
          }
          else
          {
            strcpy(MeterSensor, "pdm300");
            SetConfigParam(PATH_DMMCONFIG, "metersensor", "pdm300");
          }
          Start_Highlights_Menu6();
          UpdateWindow();
          usleep(500000);
          ModeChanged = true;
          break;
        case 4:                                            // Serial normal or inverted
          if (SerialPolarity == true)
          {
            SetConfigParam(PATH_DMMCONFIG, "serialpolarity", "inverted");
            SerialPolarity = false;
          }
          else
          {
            SetConfigParam(PATH_DMMCONFIG, "serialpolarity", "normal");
            SerialPolarity = true;
          }
          Start_Highlights_Menu6();
          UpdateWindow();
          break;
        case 5:                                            // Title
          ChangeLabel(6);
          UpdateWindow();          
          break;
        case 6:                                            // Back to main Settings menu
          printf("Settings Menu 1 Requested\n");
          CurrentMenu = 3;
          Start_Highlights_Menu3();
          UpdateWindow();
          break;
        case 7:                                            // Main Menu
          printf("Main Menu 1 Requested\n");
          CurrentMenu = 1;
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 8:
          if (freeze)
          {
            SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0);
            freeze = false;
          }
          else
          {
            SetButtonStatus(ButtonNumber(CurrentMenu, 8), 1);
            freeze = true;
          }
          UpdateWindow();
          break;
        default:
          printf("Menu 6 Error\n");
      }
      continue;  // Completed Menu 6 action, go and wait for touch
    }
    if (CurrentMenu == 7)  // Discharge Set-up Menu
    {
      printf("Button Event %d, Entering Menu 7 Case Statement\n",i);
      CallingMenu = 7;
      switch (i)
      {
        case 0:                                            // Capture Snap
          freeze = true; 
          SetButtonStatus(ButtonNumber(CurrentMenu, 0), 1);
          UpdateWindow();
          while(! frozen);
          system("/home/pi/rpidatv/scripts/snap2.sh");
          SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0);
          UpdateWindow();
          freeze = false;
          break;
        case 2:                                            // Initial Current
          ChangeDischarge(i);
          UpdateWindow();          
          break;
        case 3:                                            // Constant/Resistive load
          if (ConstantCurrent ==true)
          {
            ConstantCurrent = false;
            SetConfigParam(PATH_DMMCONFIG, "load", "resistive");
          }
          else
          {
            ConstantCurrent = true;
            SetConfigParam(PATH_DMMCONFIG, "load", "constant");
          }
          Start_Highlights_Menu7();
          UpdateWindow();          
          break;
        case 4:                                            // Discharged Voltage
          ChangeDischarge(i);
          UpdateWindow();
          break;
        case 5:                                            // Span Hours
          printf("Plot Duration Hours Menu 11 Requested\n");
          CurrentMenu = 11;
          Start_Highlights_Menu11();
          UpdateWindow();
          break;
        case 6:                                            // Load on/off
          if (LoadSwitchOn == false)
          {
            digitalWrite(LoadSwitchGPIO, HIGH);
            LoadSwitchOn = true;
          }
          else
          {
            digitalWrite(LoadSwitchGPIO, LOW);
            LoadSwitchOn = false;
          }
          Start_Highlights_Menu7();
          UpdateWindow();
          break;
        case 7:                                            // Return to Main Menu
          printf("Main Menu  Requested\n");
          Start_Highlights_Menu1();
          CurrentMenu = 1;
          UpdateWindow();
          break;
        case 8:
          if (freeze)
          {
            SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0);
            freeze = false;
          }
          else
          {
            SetButtonStatus(ButtonNumber(CurrentMenu, 8), 1);
            freeze = true;
          }
          UpdateWindow();
          break;
        default:
          printf("Menu 7 Error\n");
      }
      continue;  // Completed Menu 7 action, go and wait for touch
    }

    if (CurrentMenu == 8)  // Not used
    {
      printf("Button Event %d, Entering Menu 7 Case Statement\n",i);
      CallingMenu = 8;
      Start_Highlights_Menu8();
      switch (i)
      {
        case 0:                                            // Capture Snap
          freeze = true; 
          SetButtonStatus(ButtonNumber(CurrentMenu, 0), 1);
          UpdateWindow();
          while(! frozen);
          system("/home/pi/rpidatv/scripts/snap2.sh");
          SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0);
          UpdateWindow();
          freeze = false;
          break;
        //case 2:                                            //      
        //  break;
        //case 3:                                            // 
        //  break;
        //case 4:                                            // 
        //  break;
        //case 5:                                            // 
        //  break;
        //case 6:                                            // 
        //  break;
        case 7:                                            // Return to Main Menu
          printf("Main Menu 1 Requested\n");
          Start_Highlights_Menu1();
          CurrentMenu = 1;
          UpdateWindow();
          break;
        case 8:
          if (freeze)
          {
            SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0);
            freeze = false;
          }
          else
          {
            SetButtonStatus(ButtonNumber(CurrentMenu, 8), 1);
            freeze = true;
          }
          UpdateWindow();
          break;
        default:
          printf("Menu 8 Error\n");
      }
      continue;  // Completed Menu 8 action, go and wait for touch
    }
    if (CurrentMenu == 9)  // Actions Menu
    {
      printf("Button Event %d, Entering Menu 9 Case Statement\n",i);
      CallingMenu = 9;
      switch (i)
      {
        case 0:                                            // Capture Snap
          freeze = true; 
          SetButtonStatus(ButtonNumber(CurrentMenu, 0), 1);
          UpdateWindow();
          while(! frozen);
          system("/home/pi/rpidatv/scripts/snap2.sh");
          SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0);
          UpdateWindow();
          freeze = false;
          break;
        case 2:                                            // Load on/off
          if (LoadSwitchOn == false)
          {
            digitalWrite(LoadSwitchGPIO, HIGH);
            LoadSwitchOn = true;
          }
          else
          {
            digitalWrite(LoadSwitchGPIO, LOW);
            LoadSwitchOn = false;
          }
          Start_Highlights_Menu9();
          UpdateWindow();          
          break;
        case 3:                                                                  // Start/pause plot
          if ((DischargeInProgress == false) && (DischargePaused == false))      // First start      
          {
            digitalWrite(LoadSwitchGPIO, HIGH);
            LoadSwitchOn = true;
            DischargeInProgress = true;
            DischargeComplete = false;
          }
          else if ((DischargeInProgress == true) && (DischargePaused == false))      // Pause selected
          {
            digitalWrite(LoadSwitchGPIO, LOW);
            LoadSwitchOn = false;
            DischargePaused = true;
          }
          else if ((DischargeInProgress == true) && (DischargePaused == true))      // Pause de-selected
          {
            digitalWrite(LoadSwitchGPIO, HIGH);
            LoadSwitchOn = true;
            DischargePaused = false;
          }
          Start_Highlights_Menu9();
          UpdateWindow();          
          break;
        case 4:                                            // Reset plot
          digitalWrite(LoadSwitchGPIO, LOW);
          LoadSwitchOn = false;
          DischargeInProgress = false;
          DischargePaused = false;
          DischargeComplete = false;
          Cumulative_ma_ms = 0;
          CumulativeAH = 0;
          ModeChanged = true;
          usleep(100000);
          CurrentMenu = 9;
          Start_Highlights_Menu9();
          UpdateWindow();
          break;
        case 5:                                            // State Indicator - do nothing
          break;
        case 6:                                            // save .csv
          SetButtonStatus(ButtonNumber(9, 6), 1);
          Start_Highlights_Menu9();
          UpdateWindow();
          SaveCSV();
          usleep(500000);
          SetButtonStatus(ButtonNumber(9, 6), 0);
          Start_Highlights_Menu9();
          UpdateWindow();          
          break;
        case 7:                                            // Return to Main Menu
          printf("Main Menu 1 Requested\n");
          Start_Highlights_Menu1();
          CurrentMenu=1;
          UpdateWindow();
          break;
        case 8:
          if (freeze)
          {
            SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0);
            freeze = false;
          }
          else
          {
            SetButtonStatus(ButtonNumber(CurrentMenu, 8), 1);
            freeze = true;
          }
          UpdateWindow();
          break;
        default:
          printf("Menu 9 Error\n");
      }
      continue;  // Completed Menu 9 action, go and wait for touch
    }
    if (CurrentMenu == 10)  // Span Minutes Menu
    {
      printf("Button Event %d, Entering Menu 10 Case Statement\n",i);
      CallingMenu = 10;
      switch (i)
      {
        case 0:                                            // Capture Snap
          freeze = true; 
          SetButtonStatus(ButtonNumber(CurrentMenu, 0), 1);
          UpdateWindow();
          while(! frozen);
          system("/home/pi/rpidatv/scripts/snap2.sh");
          SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0);
          UpdateWindow();
          freeze = false;
          break;
        case 2:                                            // Minimum
        case 3:                                            // 5 Minutes
        case 4:                                            // 10 Minutes
        case 5:                                            // 20 Minutes
        case 6:                                            // 50 Minutes
        case 7:                                            // 100 Minutes
          ChangeSpan(10, i);
          PlotDurationChanged = true;
          Start_Highlights_Menu10();
          UpdateWindow();
          usleep(500000);
          printf("Returning to MENU 3 from Menu 10\n");
          CurrentMenu = 3;
          Start_Highlights_Menu3();
          UpdateWindow();
          break;
        case 8:                                            // Return to Main Menu
          printf("Main Menu 1 Requested\n");
          Start_Highlights_Menu1();
          CurrentMenu = 1;
          UpdateWindow();
          break;
        case 9:
          if (freeze)
          {
            SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0);
            freeze = false;
          }
          else
          {
            SetButtonStatus(ButtonNumber(CurrentMenu, 9), 1);
            freeze = true;
          }
          UpdateWindow();
          break;
        default:
          printf("Menu 10 Error\n");
      }
      continue;  // Completed Menu 10 action, go and wait for touch
    }

    if (CurrentMenu == 11)  // Span Hours Menu
    {
      printf("Button Event %d, Entering Menu 11 Case Statement\n",i);
      CallingMenu = 11;
      switch (i)
      {
        case 0:                                            // Capture Snap
          freeze = true; 
          SetButtonStatus(ButtonNumber(CurrentMenu, 0), 1);
          UpdateWindow();
          while(! frozen);
          system("/home/pi/rpidatv/scripts/snap2.sh");
          SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0);
          UpdateWindow();
          freeze = false;
          break;
        case 2:                                            // 1 Hour
        case 3:                                            // 2 Hours
        case 4:                                            // 5 Hours
        case 5:                                            // 10 Hours
        case 6:                                            // 20 Hours
        case 7:                                            // 50 Hours
          ChangeSpan(11, i);
          PlotDurationChanged = true;
          Start_Highlights_Menu11();
          UpdateWindow();
          usleep(500000);
          printf("Returning to MENU 3 from Menu 11\n");
          CurrentMenu = 3;
          Start_Highlights_Menu3();
          UpdateWindow();
          break;
        case 8:                                            // Return to Main Menu
          printf("Main Menu 1 Requested\n");
          Start_Highlights_Menu1();
          CurrentMenu = 1;
          UpdateWindow();
          break;
        case 9:
          if (freeze)
          {
            SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0);
            freeze = false;
          }
          else
          {
            SetButtonStatus(ButtonNumber(CurrentMenu, 9), 1);
            freeze = true;
          }
          UpdateWindow();
          break;
        default:
          printf("Menu 11 Error\n");
      }
      continue;  // Completed Menu 11 action, go and wait for touch
    }
  }
  return NULL;
}


/////////////////////////////////////////////// DEFINE THE BUTTONS ///////////////////////////////

void Define_Menu1()  // DMM Main Menu
{
  int button = 0;

  button = CreateButton(1, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(1, 1);
  AddButtonStatus(button, "Main^Menu 1", &Black);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(1, 2);
  AddButtonStatus(button, "Mode", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(1, 3);
  AddButtonStatus(button, "Actions", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(1, 4);
  AddButtonStatus(button, "Settings", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(1, 5);
  AddButtonStatus(button, "Discharge^Set-up", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(1, 6);
  AddButtonStatus(button, "System", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(1, 7);
  AddButtonStatus(button, "Exit to^Portsdown", &DBlue);
  AddButtonStatus(button, "Exit to^Portsdown", &Red);

  button = CreateButton(1, 8);
  AddButtonStatus(button, "Freeze", &Blue);
  AddButtonStatus(button, "Unfreeze", &Green);
}


void Start_Highlights_Menu1()
{
  if (PortsdownExitRequested)
  {
    SetButtonStatus(ButtonNumber(1, 7), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(1, 7), 0);
  }
}


void Define_Menu2()  // Plot Markers
{
  int button = 0;

  button = CreateButton(2, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(2, 1);
  AddButtonStatus(button, "Marker^Menu 2", &Black);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(2, 2);
  AddButtonStatus(button, "Peak", &Blue);
  AddButtonStatus(button, "Peak", &Green);

  button = CreateButton(2, 3);
  AddButtonStatus(button, "Null", &Blue);
  AddButtonStatus(button, "Null", &Green);

  button = CreateButton(2, 4);
  AddButtonStatus(button, "Manual", &Blue);
  AddButtonStatus(button, "Manual", &Green);

  button = CreateButton(2, 5);
  AddButtonStatus(button, "<-", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(2, 6);
  AddButtonStatus(button, "->", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(2, 7);
  AddButtonStatus(button, "Markers^Off", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(2, 8);
  AddButtonStatus(button, "Return to^Main Menu", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(2, 9);
  AddButtonStatus(button, "Freeze", &Blue);
  AddButtonStatus(button, "Unfreeze", &Green);
}


void Start_Highlights_Menu2()
{
}


void Define_Menu3()  // DMM Settings
{
  int button = 0;

  button = CreateButton(3, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(3, 1);
  AddButtonStatus(button, "Settings (1)^Menu 3", &Black);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(3, 2);
  AddButtonStatus(button, "Set Plot^Baseline", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(3, 3);
  AddButtonStatus(button, "Set Plot^Max", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(3, 4);
  AddButtonStatus(button, "Plot Span^Minutes", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(3, 5);
  AddButtonStatus(button, "Plot Span^Hours", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(3, 6);
  AddButtonStatus(button, "More^Settings", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(3, 7);
  AddButtonStatus(button, "Return to^Main Menu", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(3, 8);
  AddButtonStatus(button, "Freeze", &Blue);
  AddButtonStatus(button, "Unfreeze", &Green);
}


void Start_Highlights_Menu3()
{
}


void Define_Menu4()  // DMM System
{
  int button = 0;

  button = CreateButton(4, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(4, 1);
  AddButtonStatus(button, "System^Menu 4", &Black);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(4, 2);
  AddButtonStatus(button, "Snap^Viewer", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(4, 3);                  // Markers Menu
  AddButtonStatus(button, "Marker^Menu", &Blue);

  button = CreateButton(4, 4);
  AddButtonStatus(button, "Shutdown^System", &Blue);
  AddButtonStatus(button, " ", &Green);

  //button = CreateButton(4, 5);                   // Spare
  //AddButtonStatus(button, " ", &Blue);
  //AddButtonStatus(button, " ", &Green);

  button = CreateButton(4, 6);
  AddButtonStatus(button, "Re-start^App", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(4, 7);
  AddButtonStatus(button, "Return to^Main Menu", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(4, 8);
  AddButtonStatus(button, "Freeze", &Blue);
  AddButtonStatus(button, "Unfreeze", &Green);
}


void Start_Highlights_Menu4()
{
}


void Define_Menu5()  // DMM Mode
{
  int button = 0;

  button = CreateButton(5, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(5, 1);
  AddButtonStatus(button, "Mode^Menu 5", &Black);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(5, 2);
  AddButtonStatus(button, "Digital^Meter", &Blue);
  AddButtonStatus(button, "Digital^Meter", &Green);

  button = CreateButton(5, 3);
  AddButtonStatus(button, "Running^Plot", &Blue);
  AddButtonStatus(button, "Running^Plot", &Green);

  button = CreateButton(5, 4);
  AddButtonStatus(button, "Battery^Discharge", &Blue);
  AddButtonStatus(button, "Battery^Discharge", &Green);

  //button = CreateButton(5, 5);
  //AddButtonStatus(button, "Analog^Meter", &Blue);
  //AddButtonStatus(button, "Analog^Meter", &Green);

  //button = CreateButton(5, 6);
  //AddButtonStatus(button, " ", &Blue);
  //AddButtonStatus(button, " ", &Green);

  button = CreateButton(5, 7);
  AddButtonStatus(button, "Return to^Main Menu", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(5, 8);
  AddButtonStatus(button, "Freeze", &Blue);
  AddButtonStatus(button, "Unfreeze", &Green);
}

void Start_Highlights_Menu5()
{
  printf("Entered start highlights 5\n");
  if (strcmp(DMMMode, "numeric") == 0)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 2), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 2), 0);
  }

  if (strcmp(DMMMode, "graph") == 0)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0);
  }

  if (strcmp(DMMMode, "battery") == 0) 
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 4), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 4), 0);
  }

  if (strcmp(DMMMode, "meter") == 0)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 5), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 5), 0);
  }
}


void Define_Menu6()                  // More Settings Menu
{
  int button = 0;

  button = CreateButton(6, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(6, 1);
  AddButtonStatus(button, "Settings (2)^Menu 6", &Black);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(6, 2);
  AddButtonStatus(button, "Multi^Scan", &Blue);
  AddButtonStatus(button, "Single^Scan", &Blue);

  button = CreateButton(6, 3);
  AddButtonStatus(button, "Parkside^DMM", &Blue);
  AddButtonStatus(button, "Maplin^DMM", &Blue);

  button = CreateButton(6, 4);
  AddButtonStatus(button, "Serial Pol^Normal", &Blue);
  AddButtonStatus(button, "Serial Pol^Inverted", &Blue);

  button = CreateButton(6, 5);
  AddButtonStatus(button, "Title", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(6, 6);
  AddButtonStatus(button, "Less^Settings", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(6, 7);
  AddButtonStatus(button, "Return to^Main Menu", &Blue);

  button = CreateButton(6, 8);
  AddButtonStatus(button, "Freeze", &Blue);
  AddButtonStatus(button, "Unfreeze", &Green);
}

void Start_Highlights_Menu6()
{
  if (SingleScan)
  {
    SetButtonStatus(ButtonNumber(6, 2), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(6, 2), 0);
  }

  if (strcmp(MeterSensor, "pdm300") == 0)
  {
    SetButtonStatus(ButtonNumber(6, 3), 0);
  }
  else
  {
    SetButtonStatus(ButtonNumber(6, 3), 1);
  }

  if (SerialPolarity)
  {
    SetButtonStatus(ButtonNumber(6, 4), 0);
  }
  else
  {
    SetButtonStatus(ButtonNumber(6, 4), 1);
  }
}


void Define_Menu7()  //  Discharge Set-up
{
  int button = 0;

  button = CreateButton(7, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(7, 1);
  AddButtonStatus(button, "Discharge^Set-up Menu 7", &Black);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(7, 2);
  AddButtonStatus(button, "Initial^Current", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(7, 3);
  AddButtonStatus(button, "Load^Resistive", &Blue);
  AddButtonStatus(button, "Load^Constant I", &Blue);

  button = CreateButton(7, 4);
  AddButtonStatus(button, "Discharge^Voltage", &Blue);

  button = CreateButton(7, 5);
  AddButtonStatus(button, "Discharge^Duration", &Blue);

  button = CreateButton(7, 6);
  AddButtonStatus(button, "Load OFF", &Blue);
  AddButtonStatus(button, "Load ON", &Red);

  button = CreateButton(7, 7);
  AddButtonStatus(button, "Return to^Main Menu", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(7, 8);
  AddButtonStatus(button, "Freeze", &Blue);
  AddButtonStatus(button, "Unfreeze", &Green);
}


void Start_Highlights_Menu7()
{
  printf("Entered start highlights 7\n");
  if (ConstantCurrent == true)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0);
  }

  if (LoadSwitchOn == false)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 1);
  }
}


void Define_Menu8()  // Power Meter Range  Not used in DMM
{
  int button = 0;

  button = CreateButton(8, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(8, 1);
  AddButtonStatus(button, "Range^Menu", &Black);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(8, 2);
  AddButtonStatus(button, "Up", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(8, 3);
  AddButtonStatus(button, "Down", &Blue);

  button = CreateButton(8, 4);
  AddButtonStatus(button, "Wider", &Blue);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(8, 5);
  AddButtonStatus(button, "Narrower", &Blue);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(8, 6);
  AddButtonStatus(button, "Reset", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(8, 7);
  AddButtonStatus(button, "Return to^Main Menu", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(8, 8);
  AddButtonStatus(button, "Freeze", &Blue);
  AddButtonStatus(button, "Unfreeze", &Green);
}

void Start_Highlights_Menu8()
{
}


void Define_Menu9()  // DMM Actions
{
  int button = 0;

  button = CreateButton(9, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(9, 1);
  AddButtonStatus(button, "Actions^Menu 9", &Black);

  button = CreateButton(9, 2);
  AddButtonStatus(button, "Load OFF", &Blue);
  AddButtonStatus(button, "Load ON", &Red);

  button = CreateButton(9, 3);
  AddButtonStatus(button, "Start^Test", &Blue);
  AddButtonStatus(button, "Pause^Test", &Green);
  AddButtonStatus(button, "Resume^Test", &Green);

  button = CreateButton(9, 4);
  AddButtonStatus(button, "Reset^Test", &Blue);

  button = CreateButton(9, 5);
  AddButtonStatus(button, "Test not^Started", &Grey);
  AddButtonStatus(button, "Test in^Progress", &DBlue);
  AddButtonStatus(button, "Test ^Paused", &LBlue);
  AddButtonStatus(button, "Test^Complete", &Red);

  button = CreateButton(9, 6);
  AddButtonStatus(button, "Save^.csv file", &Blue);
  AddButtonStatus(button, "Saving^.csv file", &Green);

  button = CreateButton(9, 7);
  AddButtonStatus(button, "Return to^Main Menu", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(9, 8);
  AddButtonStatus(button, "Freeze", &Blue);
  AddButtonStatus(button, "Unfreeze", &Green);
}


void Start_Highlights_Menu9()  // Amend for load on/off
{
  if (LoadSwitchOn == false)
  {
    SetButtonStatus(ButtonNumber(9, 2), 0);
  }
  else
  {
    SetButtonStatus(ButtonNumber(9, 2), 1);
  }

  if ((DischargeInProgress == false) && (DischargePaused == false))
  {
    SetButtonStatus(ButtonNumber(9, 3), 0);
  }
  else if ((DischargeInProgress == true) && (DischargePaused == false))
  {
    SetButtonStatus(ButtonNumber(9, 3), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(9, 3), 2);
  }

  if (DischargeComplete == true)
  {
    SetButtonStatus(ButtonNumber(9, 5), 3);
  }
  else if (DischargePaused == true)
  {
    SetButtonStatus(ButtonNumber(9, 5), 2);
  }
  else if ((DischargePaused == false) && (DischargeInProgress == true))
  {
    SetButtonStatus(ButtonNumber(9, 5), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(9, 5), 0);
  }
}


void Define_Menu10()  // Span Minutes
{
  int button = 0;

  button = CreateButton(10, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(10, 1);
  AddButtonStatus(button, "Full Span^Minutes", &Black);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(10, 2);
  AddButtonStatus(button, "500^Samples", &Blue);
  AddButtonStatus(button, "500^Samples", &Green);

  button = CreateButton(10, 3);
  AddButtonStatus(button, "5", &Blue);
  AddButtonStatus(button, "5", &Green);

  button = CreateButton(10, 4);
  AddButtonStatus(button, "10", &Blue);
  AddButtonStatus(button, "10", &Green);

  button = CreateButton(10, 5);
  AddButtonStatus(button, "20", &Blue);
  AddButtonStatus(button, "20", &Green);

  button = CreateButton(10, 6);
  AddButtonStatus(button, "50", &Blue);
  AddButtonStatus(button, "50", &Green);

  button = CreateButton(10, 7);
  AddButtonStatus(button, "100", &Blue);
  AddButtonStatus(button, "100", &Green);

  button = CreateButton(10, 8);
  AddButtonStatus(button, "Return to^Main Menu", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(10, 9);
  AddButtonStatus(button, "Freeze", &Blue);
  AddButtonStatus(button, "Unfreeze", &Green);
}


void Start_Highlights_Menu10()  // 
{
  if (PlotDuration == 1)
  {
    SetButtonStatus(ButtonNumber(10, 2), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(10, 2), 0);
  }
  if (PlotDuration == 5)
  {
    SetButtonStatus(ButtonNumber(10, 3), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(10, 3), 0);
  }
  if (PlotDuration == 10)
  {
    SetButtonStatus(ButtonNumber(10, 4), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(10, 4), 0);
  }
  if (PlotDuration == 20)
  {
    SetButtonStatus(ButtonNumber(10, 5), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(10, 5), 0);
  }
  if (PlotDuration == 50)
  {
    SetButtonStatus(ButtonNumber(10, 6), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(10, 6), 0);
  }
  if (PlotDuration == 100)
  {
    SetButtonStatus(ButtonNumber(10, 7), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(10, 7), 0);
  }
}


void Define_Menu11()  // Span Hours
{
  int button = 0;

  button = CreateButton(11, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(11, 1);
  AddButtonStatus(button, "Full Span^Hours", &Black);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(11, 2);
  AddButtonStatus(button, "1", &Blue);
  AddButtonStatus(button, "1", &Green);

  button = CreateButton(11, 3);
  AddButtonStatus(button, "2", &Blue);
  AddButtonStatus(button, "2", &Green);

  button = CreateButton(11, 4);
  AddButtonStatus(button, "5", &Blue);
  AddButtonStatus(button, "5", &Green);

  button = CreateButton(11, 5);
  AddButtonStatus(button, "10", &Blue);
  AddButtonStatus(button, "10", &Green);

  button = CreateButton(11, 6);
  AddButtonStatus(button, "20", &Blue);
  AddButtonStatus(button, "20", &Green);

  button = CreateButton(11, 7);
  AddButtonStatus(button, "50", &Blue);
  AddButtonStatus(button, "50", &Green);

  button = CreateButton(11, 8);
  AddButtonStatus(button, "Return to^Main Menu", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(11, 9);
  AddButtonStatus(button, "Freeze", &Blue);
  AddButtonStatus(button, "Unfreeze", &Green);
}


void Start_Highlights_Menu11()  // 
{
  if (PlotDuration == 60)
  {
    SetButtonStatus(ButtonNumber(11, 2), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(11, 2), 0);
  }
  if (PlotDuration == 120)
  {
    SetButtonStatus(ButtonNumber(11, 3), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(11, 3), 0);
  }
  if (PlotDuration == 300)
  {
    SetButtonStatus(ButtonNumber(11, 4), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(11, 4), 0);
  }
  if (PlotDuration == 600)
  {
    SetButtonStatus(ButtonNumber(11, 5), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(11, 5), 0);
  }
  if (PlotDuration == 1200)
  {
    SetButtonStatus(ButtonNumber(11, 6), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(11, 6), 0);
  }
  if (PlotDuration == 3000)
  {
    SetButtonStatus(ButtonNumber(11, 7), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(11, 7), 0);
  }
}


void Define_Menu41()
{
  int button;

  button = CreateButton(41, 0);
  AddButtonStatus(button, "SPACE", &Blue);
  AddButtonStatus(button, "SPACE", &LBlue);
  AddButtonStatus(button, "SPACE", &Blue);
  AddButtonStatus(button, "SPACE", &LBlue);
  //button = CreateButton(41, 1);
  //AddButtonStatus(button, "X", &Blue);
  //AddButtonStatus(button, "X", &LBlue);
  button = CreateButton(41, 2);
  AddButtonStatus(button, "<", &Blue);
  AddButtonStatus(button, "<", &LBlue);
  AddButtonStatus(button, "<", &Blue);
  AddButtonStatus(button, "<", &LBlue);
  button = CreateButton(41, 3);
  AddButtonStatus(button, ">", &Blue);
  AddButtonStatus(button, ">", &LBlue);
  AddButtonStatus(button, ">", &Blue);
  AddButtonStatus(button, ">", &LBlue);
  button = CreateButton(41, 4);
  AddButtonStatus(button, "-", &Blue);
  AddButtonStatus(button, "-", &LBlue);
  AddButtonStatus(button, "_", &Blue);
  AddButtonStatus(button, "_", &LBlue);
  button = CreateButton(41, 5);
  AddButtonStatus(button, "Clear", &Blue);
  AddButtonStatus(button, "Clear", &LBlue);
  AddButtonStatus(button, "Clear", &Blue);
  AddButtonStatus(button, "Clear", &LBlue);
  button = CreateButton(41, 6);
  AddButtonStatus(button, "^", &LBlue);
  AddButtonStatus(button, "^", &Blue);
  AddButtonStatus(button, "^", &Blue);
  AddButtonStatus(button, "^", &LBlue);
  button = CreateButton(41, 7);
  AddButtonStatus(button, "^", &LBlue);
  AddButtonStatus(button, "^", &Blue);
  AddButtonStatus(button, "^", &Blue);
  AddButtonStatus(button, "^", &LBlue);
  button = CreateButton(41, 8);
  AddButtonStatus(button, "Enter", &Blue);
  AddButtonStatus(button, "Enter", &LBlue);
  AddButtonStatus(button, "Enter", &Blue);
  AddButtonStatus(button, "Enter", &LBlue);
  button = CreateButton(41, 9);
  AddButtonStatus(button, "<=", &Blue);
  AddButtonStatus(button, "<=", &LBlue);
  AddButtonStatus(button, "<=", &Blue);
  AddButtonStatus(button, "<=", &LBlue);
  button = CreateButton(41, 10);
  AddButtonStatus(button, "Z", &Blue);
  AddButtonStatus(button, "Z", &LBlue);
  AddButtonStatus(button, "z", &Blue);
  AddButtonStatus(button, "z", &LBlue);
  button = CreateButton(41, 11);
  AddButtonStatus(button, "X", &Blue);
  AddButtonStatus(button, "X", &LBlue);
  AddButtonStatus(button, "x", &Blue);
  AddButtonStatus(button, "x", &LBlue);
  button = CreateButton(41, 12);
  AddButtonStatus(button, "C", &Blue);
  AddButtonStatus(button, "C", &LBlue);
  AddButtonStatus(button, "c", &Blue);
  AddButtonStatus(button, "c", &LBlue);
  button = CreateButton(41, 13);
  AddButtonStatus(button, "V", &Blue);
  AddButtonStatus(button, "V", &LBlue);
  AddButtonStatus(button, "v", &Blue);
  AddButtonStatus(button, "v", &LBlue);
  button = CreateButton(41, 14);
  AddButtonStatus(button, "B", &Blue);
  AddButtonStatus(button, "B", &LBlue);
  AddButtonStatus(button, "b", &Blue);
  AddButtonStatus(button, "b", &LBlue);
  button = CreateButton(41, 15);
  AddButtonStatus(button, "N", &Blue);
  AddButtonStatus(button, "N", &LBlue);
  AddButtonStatus(button, "n", &Blue);
  AddButtonStatus(button, "n", &LBlue);
  button = CreateButton(41, 16);
  AddButtonStatus(button, "M", &Blue);
  AddButtonStatus(button, "M", &LBlue);
  AddButtonStatus(button, "m", &Blue);
  AddButtonStatus(button, "m", &LBlue);
  button = CreateButton(41, 17);
  AddButtonStatus(button, ",", &Blue);
  AddButtonStatus(button, ",", &LBlue);
  AddButtonStatus(button, "!", &Blue);
  AddButtonStatus(button, "!", &LBlue);
  button = CreateButton(41, 18);
  AddButtonStatus(button, ".", &Blue);
  AddButtonStatus(button, ".", &LBlue);
  AddButtonStatus(button, ".", &Blue);
  AddButtonStatus(button, ".", &LBlue);
  button = CreateButton(41, 19);
  AddButtonStatus(button, "/", &Blue);
  AddButtonStatus(button, "/", &LBlue);
  AddButtonStatus(button, "?", &Blue);
  AddButtonStatus(button, "?", &LBlue);
  button = CreateButton(41, 20);
  AddButtonStatus(button, "A", &Blue);
  AddButtonStatus(button, "A", &LBlue);
  AddButtonStatus(button, "a", &Blue);
  AddButtonStatus(button, "a", &LBlue);
  button = CreateButton(41, 21);
  AddButtonStatus(button, "S", &Blue);
  AddButtonStatus(button, "S", &LBlue);
  AddButtonStatus(button, "s", &Blue);
  AddButtonStatus(button, "s", &LBlue);
  button = CreateButton(41, 22);
  AddButtonStatus(button, "D", &Blue);
  AddButtonStatus(button, "D", &LBlue);
  AddButtonStatus(button, "d", &Blue);
  AddButtonStatus(button, "d", &LBlue);
  button = CreateButton(41, 23);
  AddButtonStatus(button, "F", &Blue);
  AddButtonStatus(button, "F", &LBlue);
  AddButtonStatus(button, "f", &Blue);
  AddButtonStatus(button, "f", &LBlue);
  button = CreateButton(41, 24);
  AddButtonStatus(button, "G", &Blue);
  AddButtonStatus(button, "G", &LBlue);
  AddButtonStatus(button, "g", &Blue);
  AddButtonStatus(button, "g", &LBlue);
  button = CreateButton(41, 25);
  AddButtonStatus(button, "H", &Blue);
  AddButtonStatus(button, "H", &LBlue);
  AddButtonStatus(button, "h", &Blue);
  AddButtonStatus(button, "h", &LBlue);
  button = CreateButton(41, 26);
  AddButtonStatus(button, "J", &Blue);
  AddButtonStatus(button, "J", &LBlue);
  AddButtonStatus(button, "j", &Blue);
  AddButtonStatus(button, "j", &LBlue);
  button = CreateButton(41, 27);
  AddButtonStatus(button, "K", &Blue);
  AddButtonStatus(button, "K", &LBlue);
  AddButtonStatus(button, "k", &Blue);
  AddButtonStatus(button, "k", &LBlue);
  button = CreateButton(41, 28);
  AddButtonStatus(button, "L", &Blue);
  AddButtonStatus(button, "L", &LBlue);
  AddButtonStatus(button, "l", &Blue);
  AddButtonStatus(button, "l", &LBlue);
  //button = CreateButton(41, 29);
  //AddButtonStatus(button, "/", &Blue);
  //AddButtonStatus(button, "/", &LBlue);
  button = CreateButton(41, 30);
  AddButtonStatus(button, "Q", &Blue);
  AddButtonStatus(button, "Q", &LBlue);
  AddButtonStatus(button, "q", &Blue);
  AddButtonStatus(button, "q", &LBlue);
  button = CreateButton(41, 31);
  AddButtonStatus(button, "W", &Blue);
  AddButtonStatus(button, "W", &LBlue);
  AddButtonStatus(button, "w", &Blue);
  AddButtonStatus(button, "w", &LBlue);
  button = CreateButton(41, 32);
  AddButtonStatus(button, "E", &Blue);
  AddButtonStatus(button, "E", &LBlue);
  AddButtonStatus(button, "e", &Blue);
  AddButtonStatus(button, "e", &LBlue);
  button = CreateButton(41, 33);
  AddButtonStatus(button, "R", &Blue);
  AddButtonStatus(button, "R", &LBlue);
  AddButtonStatus(button, "r", &Blue);
  AddButtonStatus(button, "r", &LBlue);
  button = CreateButton(41, 34);
  AddButtonStatus(button, "T", &Blue);
  AddButtonStatus(button, "T", &LBlue);
  AddButtonStatus(button, "t", &Blue);
  AddButtonStatus(button, "t", &LBlue);
  button = CreateButton(41, 35);
  AddButtonStatus(button, "Y", &Blue);
  AddButtonStatus(button, "Y", &LBlue);
  AddButtonStatus(button, "y", &Blue);
  AddButtonStatus(button, "y", &LBlue);
  button = CreateButton(41, 36);
  AddButtonStatus(button, "U", &Blue);
  AddButtonStatus(button, "U", &LBlue);
  AddButtonStatus(button, "u", &Blue);
  AddButtonStatus(button, "u", &LBlue);
  button = CreateButton(41, 37);
  AddButtonStatus(button, "I", &Blue);
  AddButtonStatus(button, "I", &LBlue);
  AddButtonStatus(button, "i", &Blue);
  AddButtonStatus(button, "i", &LBlue);
  button = CreateButton(41, 38);
  AddButtonStatus(button, "O", &Blue);
  AddButtonStatus(button, "O", &LBlue);
  AddButtonStatus(button, "o", &Blue);
  AddButtonStatus(button, "o", &LBlue);
  button = CreateButton(41, 39);
  AddButtonStatus(button, "P", &Blue);
  AddButtonStatus(button, "P", &LBlue);
  AddButtonStatus(button, "p", &Blue);
  AddButtonStatus(button, "p", &LBlue);

  button = CreateButton(41, 40);
  AddButtonStatus(button, "1", &Blue);
  AddButtonStatus(button, "1", &LBlue);
  AddButtonStatus(button, "1", &Blue);
  AddButtonStatus(button, "1", &LBlue);
  button = CreateButton(41, 41);
  AddButtonStatus(button, "2", &Blue);
  AddButtonStatus(button, "2", &LBlue);
  AddButtonStatus(button, "2", &Blue);
  AddButtonStatus(button, "2", &LBlue);
  button = CreateButton(41, 42);
  AddButtonStatus(button, "3", &Blue);
  AddButtonStatus(button, "3", &LBlue);
  AddButtonStatus(button, "3", &Blue);
  AddButtonStatus(button, "3", &LBlue);
  button = CreateButton(41, 43);
  AddButtonStatus(button, "4", &Blue);
  AddButtonStatus(button, "4", &LBlue);
  AddButtonStatus(button, "4", &Blue);
  AddButtonStatus(button, "4", &LBlue);
  button = CreateButton(41, 44);
  AddButtonStatus(button, "5", &Blue);
  AddButtonStatus(button, "5", &LBlue);
  AddButtonStatus(button, "5", &Blue);
  AddButtonStatus(button, "5", &LBlue);
  button = CreateButton(41, 45);
  AddButtonStatus(button, "6", &Blue);
  AddButtonStatus(button, "6", &LBlue);
  AddButtonStatus(button, "6", &Blue);
  AddButtonStatus(button, "6", &LBlue);
  button = CreateButton(41, 46);
  AddButtonStatus(button, "7", &Blue);
  AddButtonStatus(button, "7", &LBlue);
  AddButtonStatus(button, "7", &Blue);
  AddButtonStatus(button, "7", &LBlue);
  button = CreateButton(41, 47);
  AddButtonStatus(button, "8", &Blue);
  AddButtonStatus(button, "8", &LBlue);
  AddButtonStatus(button, "8", &Blue);
  AddButtonStatus(button, "8", &LBlue);
  button = CreateButton(41, 48);
  AddButtonStatus(button, "9", &Blue);
  AddButtonStatus(button, "9", &LBlue);
  AddButtonStatus(button, "9", &Blue);
  AddButtonStatus(button, "9", &LBlue);
  button = CreateButton(41, 49);
  AddButtonStatus(button, "0", &Blue);
  AddButtonStatus(button, "0", &LBlue);
  AddButtonStatus(button, "0", &Blue);
  AddButtonStatus(button, "0", &LBlue);
}


/////////////////////////////////////////// APPLICATION DRAWING //////////////////////////////////


void DrawEmptyScreen()
{
  int div = 0;
  
    setBackColour(0, 0, 0);
    wipeScreen(0, 0, 0);
    HorizLine(100, 70, 500, 255, 255, 255);
    VertLine(100, 70, 400, 255, 255, 255);
    HorizLine(100, 470, 500, 255, 255, 255);
    VertLine(600, 70, 400, 255, 255, 255);

    HorizLine(101, 120, 499, 63, 63, 63);
    HorizLine(101, 170, 499, 63, 63, 63);
    HorizLine(101, 220, 499, 63, 63, 63);
    HorizLine(101, 270, 499, 63, 63, 63);
    HorizLine(101, 320, 499, 63, 63, 63);
    HorizLine(101, 370, 499, 63, 63, 63);
    HorizLine(101, 420, 499, 63, 63, 63);

    VertLine(150, 71, 399, 63, 63, 63);
    VertLine(200, 71, 399, 63, 63, 63);
    VertLine(250, 71, 399, 63, 63, 63);
    VertLine(300, 71, 399, 63, 63, 63);
    VertLine(350, 71, 399, 63, 63, 63);
    VertLine(400, 71, 399, 63, 63, 63);
    VertLine(450, 71, 399, 63, 63, 63);
    VertLine(500, 71, 399, 63, 63, 63);
    VertLine(550, 71, 399, 63, 63, 63);

  for(div = 0; div < 10; div++)
  {
    VertLine(100 + div * 50 + 10, 265, 10, 63, 63, 63);
    VertLine(100 + div * 50 + 20, 265, 10, 63, 63, 63);
    VertLine(100 + div * 50 + 30, 265, 10, 63, 63, 63);
    VertLine(100 + div * 50 + 40, 265, 10, 63, 63, 63);
  }

  for(div = 0; div < 8; div++)
  {
    HorizLine(345, 70 + div * 50 + 10, 10, 63, 63, 63);
    HorizLine(345, 70 + div * 50 + 20, 10, 63, 63, 63);
    HorizLine(345, 70 + div * 50 + 30, 10, 63, 63, 63);
    HorizLine(345, 70 + div * 50 + 40, 10, 63, 63, 63);
  }
}

void DrawYaxisLabels()
{
  pthread_mutex_lock(&text_lock);
  setForeColour(255, 255, 255);                    // White text
  setBackColour(0, 0, 0);                          // on Black
  const font_t *font_ptr = &font_dejavu_sans_18;   // 18pt
  char y_label[15];

  snprintf( y_label, 14, "%.4g", PlotMax);
  Text2(30, 463, y_label, font_ptr);

  snprintf( y_label, 14, "%.4g", PlotBase + 3 * (PlotMax - PlotBase) / 4);
  Text2(30, 366, y_label, font_ptr);

  snprintf( y_label, 14, "%.4g", PlotBase + 2 * (PlotMax - PlotBase) / 4);
  Text2(30, 266, y_label, font_ptr);

  snprintf( y_label, 14, "%.4g", PlotBase + 1 * (PlotMax - PlotBase) / 4);
  Text2(30, 166, y_label, font_ptr);

  snprintf( y_label, 14, "%.4g", PlotBase);
  Text2(30,  66, y_label, font_ptr);
  pthread_mutex_unlock(&text_lock);
}


void DrawTitle()
{
  setForeColour(255, 255, 255);                    // White text
  setBackColour(0, 0, 0);                          // on Black
  int titley = 5;

  // Clear the previous text first
  rectangle(100, 0, 505, 35, 0, 0, 0);
 
  if (strcmp(PlotTitle, "-") != 0)
  {
    TextMid2(350, titley, PlotTitle, &font_dejavu_sans_22);
  }
}


void DrawBaselineText()
{
  setForeColour(255, 255, 255);                    // White text
  setBackColour(0, 0, 0);                          // on Black
  int BaselineTexty = 45;
  char BaselineText[255];
  char ReadingText[255];
  char AHText[31];

  // Clear the previous text first
  rectangle(100, 36, 505, 33, 0, 0, 0);

  if (BasicMeterReading < -9999.0) // No valid data yet
  {
    strcpy(BaselineText, "No Data");
  }
  else
  {
    snprintf(ReadingText, 30, "%f", MeterReading);
    if (ReadingText[0] == '-')
    {
      if (ReadingText[1] == '1')
      {
        ReadingText[6] = '\0';
      }
      else
      {
        ReadingText[5] = '\0';
      }
    }
    else
    {
      if (ReadingText[0] == '1')
      {
        ReadingText[5] = '\0';
      }
      else
      {
        ReadingText[4] = '\0';
      }
    }
    strcpy(BaselineText, "Reading: ");
    strcat(BaselineText, ReadingText);
    if (stale_data == true)
    {
      strcpy(BaselineText, "Stale Data");
    } 
  }

  switch(PlotDuration)
  {
    case 1:
      strcat(BaselineText, "     50 samples/div");
      break;
    case 5:
      strcat(BaselineText, "     30 sec/div");
      break;
    case 10:
      strcat(BaselineText, "     1 min/div");
      break;
    case 20:
      strcat(BaselineText, "     2 min/div");
      break;
    case 50:
      strcat(BaselineText, "     5 min/div");
      break;
    case 100:
      strcat(BaselineText, "     10 min/div");
      break;
    case 60:
      strcat(BaselineText, "     6 min/div");
      break;
    case 120:
      strcat(BaselineText, "     12 min/div");
      break;
    case 300:
      strcat(BaselineText, "     30 min/div");
      break;
    case 600:
      strcat(BaselineText, "     1 hour/div");
      break;
    case 1200:
      strcat(BaselineText, "     2 hour/div");
      break;
    case 3000:
      strcat(BaselineText, "     5 hour/div");
      break;
  }

  if (strcmp(DMMMode, "battery") == 0)
  {
    snprintf(AHText, 20, "  Cap %.3f AH", CumulativeAH);
    strcat(BaselineText, AHText);
  }
  pthread_mutex_lock(&text_lock);
  Text2(100, BaselineTexty, BaselineText, &font_dejavu_sans_22);
  pthread_mutex_unlock(&text_lock);
}


void DrawTrace(int xoffset, int prev2, int prev1, int current)
{
  int xpos;
  int previousStep;
  int thisStep;
  int column[401];
  int ypos;
  int ypospix;  // ypos corrected for pixel map  
  int ymax;     // inclusive upper limit of this plot
  int ymin;     // inclusive lower limit of this plot

  for (ypos = 0; ypos < 401; ypos++)
  {
    column[ypos] = 0;
  }

  xpos = 99 + xoffset;  // we are going to draw the column before the current one

  previousStep = prev1 - prev2;  // positive is going up, ie below prev1
  thisStep = current - prev1; //    positive is going up, ie above prev1

  // Calculate contribution from previous2:

  if (previousStep == 0)
  {
    column[prev1] = 255;
    ymax = prev1;
    ymin = prev1;
  }
   else if (previousStep > 0)  // prev1 higher than prev2
  {
    for (ypos = prev2; ypos < prev1; ypos++)
    {
      column[ypos] = (255 * (ypos - prev2)) / previousStep;
    }
    ymax = prev1;
    ymin = prev2;
  }
  else // previousStep < 0  // prev2 lower than prev1
  {
    for (ypos = prev2; ypos > prev1; ypos--)
    {
      column[ypos] = (255 * (ypos - prev2 )) / previousStep;
    }
    ymax = prev2;
    ymin = prev1;
  }

  // Calculate contribution from current reading:

  if (thisStep == 0)
  {
    column[prev1] = 255;
  }
   else if (thisStep > 0)
  {
    for (ypos = prev1; ypos < current; ypos++)
    {
      column[ypos] = ((255 * (current - ypos)) / thisStep) + column[ypos];
    }
  }
  else // thisStep < 0
  {
    for (ypos = prev1; ypos > current; ypos--)
    {
      column[ypos] = ((255 * (current - ypos )) / thisStep) + column[ypos];
    }
  }

  if (current > ymax)  // Decide the highest extent of the trace
  {
    ymax = current;
  }
  if (current < ymin)  // Decide the lowest extent of the trace
  {
    ymin = current;
  }

  if (column[ypos] > 255)  // Limit the max brightness of the trace to 255
  {
    column[ypos] = 255;
  }

  // Draw the trace in the column

  for(ypos = ymin; ypos <= ymax; ypos++)
  
  {
    ypospix = 409 - ypos;  //409 = 479 - 70
    setPixelNoA(xpos, ypospix, column[ypos], column[ypos], 0);  
  }

  // Draw the background and grid (except in the active trace) to erase the previous scan

  if (xpos % 50 == 0)  // vertical int graticule
  {
    for(ypos = 1; ypos < ymin; ypos++)
    {
      setPixelNoAGra(xpos, 479 - (70 + ypos));
    }
    for(ypos = 399; ypos > ymax; ypos--)
    {
      setPixelNoAGra(xpos, 479 - (70 + ypos));
    }
  }
  else if ((xpos > 344) && (xpos < 356))  // centre vertical graticule marks
  {
    for(ypos = 1; ypos < ymin; ypos++)
    {
      ypospix = 409 - ypos;  // 409 = 479 - 70

      if (ypos % 10 == 0)  // tick mark
      {
        setPixelNoAGra(xpos, ypospix);
      }
      else
      {
        setPixelNoABlk(xpos, ypospix);
      }
    }
    for(ypos = 399; ypos > ymax; ypos--)
    {
      ypospix = 409 - ypos;  // 409 = 479 - 70

      if (ypos % 10 == 0)  // tick mark
      {
        setPixelNoAGra(xpos, ypospix);
      }
      else
      {
        setPixelNoABlk(xpos, ypospix);
      }
    }
  }
  else  // horizontal graticule and open space
  {
    for(ypos = 1; ypos < ymin; ypos++)  // below the trace
    {
      ypospix = 409 - ypos;  // 409 = 479 - 70

      if (ypos % 50 == 0)  // graticule line
      {
        setPixelNoAGra(xpos, ypospix);
      }
      else
      {
        setPixelNoABlk(xpos, ypospix);
      }
      if ((xpos % 10 == 0) && (ypos > 195) && (ypos < 205)) // tick mark on x axis
      {
        setPixelNoAGra(xpos, ypospix);
      }
    }
    for(ypos = 399; ypos > ymax; ypos--)  // above the trace
    {
      ypospix = 409 - ypos;  // 409 = 479 - 70
      if (ypos % 50 == 0)  // graticule line
      {
        setPixelNoAGra(xpos, ypospix);
      }
      else
      {
        setPixelNoABlk(xpos, ypospix);
      }
      if ((xpos % 10 == 0) && (ypos > 195) && (ypos < 205)) // tick mark on x axis
      {
        setPixelNoAGra(xpos, ypospix);
      }
    }
  }

  // Draw the markers

  if (markeron == true)
  {
    MarkerGrn(markerx, xpos, markery);
  }
}


void displayByte(uint8_t input)
{
  uint8_t bit[9];  // only 1 - 8 used
  uint8_t nib1 = 0;
  uint8_t nib2 = 0;
  // char hex1[2];
  // char hex2[2];
  uint8_t k;

  for (k = 1; k <= 8; k++)
  {
    bit[k] = 0;
  }

  if (input > 127)
  {
    bit[8] = 1;
    input = input - 128;
  }
  if (input > 63)
  {
    bit[7] = 1;
    input = input - 64;
  }
  if (input > 31)
  {
    bit[6] = 1;
    input = input - 32;
  }
  if (input > 15)
  {
    bit[5] = 1;
    input = input - 16;
  }
  if (input > 7)
  {
    bit[4] = 1;
    input = input - 8;
  }
  if (input > 3)
  {
    bit[3] = 1;
    input = input - 4;
  }
  if (input > 1)
  {
    bit[2] = 1;
    input = input - 2;
  }
  if (input > 0)
  {
    bit[1] = 1;
  }

  nib1 = bit[1] + 2 * bit[2] + 4 * bit[3] + 8 * bit[4]; 
  nib2 = bit[5] + 2 * bit[6] + 4 * bit[7] + 8 * bit[8]; 
  readbyte = nib1 + 16 * nib2;
  printf("%d%d%d%d %d%d%d%d %d %d %d\n", bit[8], bit[7], bit[6], bit[5], bit[4], bit[3], bit[2], bit[1], nib2, nib1, nib1 + 16 * nib2);
}

/***************************************************************************//**
 * @brief Run as a thread to monitor the serial input
 *
 * Listens to set serial port and sets 
 * globals new_serial_data true and nibble2, nibble1, readbyte 
 * when data received.
 *
 * Consuming function should set new_serial_data false when it has been read
 *
 * @return void
*******************************************************************************/

void *serial_listener(void * arg)
{
  uint8_t SERIAL_IN_GPIO = 8;  // Use WiringPi number

  uint8_t mark_level;
  uint8_t space_level;

  uint64_t baud_time;
  uint32_t baud_rate;

  int m;

  bool awaiting_start = true;

  uint64_t start_transition_time;
  uint64_t start_centre_time;
  uint64_t bit_centre_time[10];  // only 1 - 8 used for data.  9 is stop bit
  uint64_t last_valid_data_time;
 
  uint8_t bit[9];  // only 1 - 8 used

  // Idle RS232 is "Mark", normally -15 V (data 1).  Space is normally +15 V (data 0)
  // one start bit (space), 8 data bits and one stop bit (mark)
  // Set levels for GPIO hardware interface here

  if (SerialPolarity == true)  // normal
  {
    mark_level = 0;
    space_level = 1;
  }
  else    // Inverted
  {
    mark_level = 1;
    space_level = 0;
  }

  pinMode(SERIAL_IN_GPIO, INPUT);

  // Calculate baud timings from baud rate
  baud_rate = 2400;
  baud_time = (uint64_t)(1000000 / baud_rate); // data bit length in us

  last_valid_data_time = monotonic_us();
  new_serial_data = false;

  while (app_exit == false)
  {
    usleep(1);  // Don't consume all the resources!

    if (last_valid_data_time < monotonic_us() - 1000000)
    {
      stale_data = true;
    }
    else
    {
      stale_data = false;
    }  

    // check if start bit is found
    if ((digitalRead(SERIAL_IN_GPIO) == space_level) && (awaiting_start))
    {
      start_transition_time = monotonic_us();
      start_centre_time = start_transition_time + baud_time / 2;

      //printf("Start bit detected\n");
      awaiting_start = false;

      // Wait for centre of start bit
      while (monotonic_us() < start_centre_time)
      {
        usleep(1);
      }

      if (digitalRead(SERIAL_IN_GPIO) == space_level) // still in start bit so progress
      {
        for (m = 1; m <= 9; m++)  // Set center time for each bit and the stop bit
        {
          bit_centre_time[m] = start_centre_time + m * baud_time;
        }

        for (m = 1; m <= 8; m++)  // wait then read each bit
        {
          while (monotonic_us() < bit_centre_time[m])
          {
            usleep(1);
          }

          if (digitalRead(SERIAL_IN_GPIO) == mark_level)
          {
            bit[m] = 1;
          }
          else
          {
            bit[m] = 0;
          }
        }

        // Now wait for the centre of the stop bit
        while (monotonic_us() < bit_centre_time[9])
        {
          usleep(1);
        }
        awaiting_start = true;

        nibble1 = bit[1] + 2 * bit[2] + 4 * bit[3] + 8 * bit[4]; 
        nibble2 = bit[5] + 2 * bit[6] + 4 * bit[7] + 8 * bit[8]; 
        readbyte = nibble1 + 16 * nibble2;
        //printf("%d%d%d%d %d%d%d%d %d %d %d\n", bit[8], bit[7], bit[6], bit[5], bit[4], bit[3], bit[2], bit[1], nibble2, nibble1, readbyte);
        last_valid_data_time = monotonic_us();
        new_serial_data = true;
      }
      else
      {
        printf("False Start Bit detected; waiting for end of data\n");
        while (monotonic_us() < start_centre_time + 9 * baud_time)
        {
          usleep(1);
        }
        awaiting_start = true;
      }
    }
  }
  return NULL;
}


void *serial_decoder(void * arg)
{
  bool preamble1 = false;
  bool preamble2 = false;
  bool preamble3 = false;
  uint16_t preamble3byte = 0;
  char textmode[31] = "";

  bool CaptureExponent = false;
  bool CaptureUnknown = false;

  uint16_t unknownbyte = 0;
  bool CaptureValueHiByte = false;
  bool CaptureValueLoByte = false;
  bool CaptureCsumHiByte = false;
  bool CaptureCsumLoByte = false;
  uint16_t hibytecs = 0;
  uint16_t lobyte = 0;
  int16_t hibyte = 0;
  uint16_t CsumLoByte = 0;
  uint16_t CsumHiByte = 0;
  uint16_t measuremodebyte = 0;
  uint16_t CalcCheckSum = 0;
  new_serial_sentence = false;

  while (app_exit == false)
  {
    usleep(100);  // Don't consume all the resources

    if (new_serial_data == true)  // So new byte to decode
    {
      new_serial_data = false;
      //printf("Readbyte = %d\n", readbyte);

      // Start the decode

      if ((preamble1 == false) && (readbyte == 220))
      {
        preamble1 = true;
      }
      else if ((preamble1 == true) && (preamble2 == false) && (readbyte == 186))
      {
        preamble2 = true;
      }
      else if ((preamble2 == true) && (preamble3 == false) && (readbyte == 1))
      {
        preamble3 = true;
        preamble3byte = readbyte;
        printf("Preamble Complete\n");
      }
      else if ((preamble3 == true) && (nibble2 == 1) && (strlen(textmode) == 0))
      {
        measuremode = nibble1;
        measuremodebyte = readbyte;
        switch(measuremode)
        {
          case 6:
            strcpy(textmode, "DC V");
            break;
          case 5:
            strcpy(textmode, "AC V");
            break;
          case 13:
            strcpy(textmode, "Resistance");
            break;
        }
        displayByte(readbyte);                                           // for debug
        printf("Measure Mode %d which is %s\n", measuremode, textmode);
        CaptureExponent = true;
      }
      else if (CaptureExponent == true)
      {
        exponentbyte = readbyte;
        CaptureExponent = false;
        printf("Exponent = %d\n", exponentbyte); 
        CaptureUnknown = true;
      }
      else if (CaptureUnknown == true)
      {
        printf("Unknown = %d\n", readbyte);
        unknownbyte = readbyte;
        CaptureUnknown = false;
        CaptureValueHiByte = true;
      }
      else if (CaptureValueHiByte == true)
      {
        printf("HiByte = %d\n", readbyte);
        hibytecs = readbyte;
        if (readbyte < 128)
        {
          hibyte = readbyte;
        }
        else
        {
          hibyte = readbyte - 255;
        }
        CaptureValueHiByte = false;
        CaptureValueLoByte = true;
      }
      else if (CaptureValueLoByte == true)
      {
        printf("LoByte = %d\n", readbyte);
        lobyte = readbyte;

        if (hibyte >= 0)
        {
          BasicMeterReading = (float)(lobyte + 256 * hibyte);
        }
        else
        {
          BasicMeterReading = (float)(256 * hibyte - (255 - lobyte));
        }

        CaptureValueLoByte = false;
        CaptureCsumHiByte = true;
      }
      else if (CaptureCsumHiByte == true)
      {
       // printf("CsumHiByte = %d\n", readbyte);
        CsumHiByte = readbyte;
             
        CaptureCsumHiByte = false;
        CaptureCsumLoByte = true;
      }
      else if (CaptureCsumLoByte == true)
      {
        //printf("CsumLoByte = %d\n", readbyte);
        CsumLoByte = readbyte;
        CaptureCsumLoByte = false;

        CalcCheckSum = preamble3byte + measuremodebyte + exponentbyte + unknownbyte +  + hibytecs + lobyte;
        //printf("CheckSum =  %d\n", CalcCheckSum);
        if (CalcCheckSum == (CsumLoByte + (256 * CsumHiByte)))
        {
          new_serial_sentence = true;
          printf("CheckSum valid\n");
        }
        else
        {
          printf("CheckSum invalid: CsumHiByte = %d, CsumLoByte = %d, CalcCheckSum = %d\n", CsumHiByte, CsumLoByte, CalcCheckSum);
        }
      }
      else
      {
        preamble1 = false;
        preamble2 = false;
        preamble3 = false;
        measuremode = 0;
        strcpy(textmode, "");
        CaptureExponent = false;
        CaptureUnknown = false;
        CaptureValueHiByte = false;
        CaptureValueLoByte = false;
        CaptureCsumHiByte = false;
        CaptureCsumLoByte = false;
        new_serial_sentence = false;
         //printf("\n");
      }
    }
  }
  return NULL;
}

void DecodeSerialSentence()
{
  switch(measuremode)
  {
    case 6:                  // DC Volts
      switch(exponentbyte)
      {
        case 2:
          MeterReading = BasicMeterReading * 0.0001;
          break;
        case 4:
          MeterReading = BasicMeterReading * 0.001;
          break;
        case 8:
          MeterReading = BasicMeterReading * 0.01;
          break;
        case 16:
          MeterReading = BasicMeterReading * 0.1;
          break;
        case 32:
          MeterReading = BasicMeterReading * 1.0;
          break;
      }
      printf("DC V Reading = %f\n", MeterReading);
      break;
    case 5:                           // AC Volts
      switch(exponentbyte)
      {
        case 4:
          MeterReading = BasicMeterReading * 0.001;
          break;
        case 8:
          MeterReading = BasicMeterReading * 0.01;
          break;
        case 16:
          MeterReading = BasicMeterReading * 0.1;
          break;
        case 32:
          MeterReading = BasicMeterReading * 1.0;
          break;
      }
      printf("AC V Reading = %f\n", MeterReading);
      break;
    case 13:
      switch(exponentbyte)
      {
        case 1:
          MeterReading = BasicMeterReading * 0.1;
          break;
        case 2:
          MeterReading = BasicMeterReading * 1.0;
          break;
        case 4:
          MeterReading = BasicMeterReading * 0.001;
          break;
        case 8:
          MeterReading = BasicMeterReading * 0.01;
          break;
        case 16:
          MeterReading = BasicMeterReading * 0.1;
          break;
        case 32:
          MeterReading = BasicMeterReading * 1.0;
          break;
      }
      printf("Resistance Reading = %f ohms\n", MeterReading);
      break;
    default:
      BasicMeterReading = -10000.0;       // indicate invalid value
      break;
  }
}



static void cleanexit(int calling_exit_code)
{
  app_exit = true;
  usleep(100000);
  exit_code = calling_exit_code;
  printf("Clean Exit Code %d\n", exit_code);
  usleep(1000000);
  char Commnd[255];
  sprintf(Commnd,"stty echo");
  system(Commnd);
  exit(exit_code);
}


static void terminate(int dummy)
{
  app_exit = true;
  printf("Terminate\n");
  digitalWrite(LoadSwitchGPIO, LOW);
  char Commnd[255];
  sprintf(Commnd,"stty echo");
  system(Commnd);
  sprintf(Commnd,"reset");
  system(Commnd);
  printf("scans = %d\n", tracecount);
  exit(207);
}


int main(int argc, char **argv)
{
  int NoDeviceEvent=0;
  wscreen = 800;
  hscreen = 480;
  int screenXmax, screenXmin;
  int screenYmax, screenYmin;
  int i;
  int xpos = 0;
  float previous_MeterReading = 10000.0;
  bool previous_stale_data = false;
  bool new_reading = false;
  char MeterReadingString[31];
  uint64_t StartTime;
  uint64_t NextPlotTime;
  char response[63];

  // Catch sigaction and call terminate
  for (i = 0; i < 16; i++)
  {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = terminate;
    sigaction(i, &sa, NULL);
  }

  // Make sure that text writes do not interfere with each other
  pthread_mutex_init(&text_lock, NULL);

  // Set up wiringPi module
  if (wiringPiSetup() < 0)
  {
    return 0;
  }

  // Set nominated pin to output
  pinMode(LoadSwitchGPIO, OUTPUT);

  // Set idle conditions
  digitalWrite(LoadSwitchGPIO, LOW);

  // Check the display type in the config file
  strcpy(response, "Element14_7");
  GetConfigParam(PATH_PCONFIG, "display", response);
  strcpy(DisplayType, response);

  // Check for presence of touchscreen
  for(NoDeviceEvent = 0; NoDeviceEvent < 7; NoDeviceEvent++)
  {
    if (openTouchScreen(NoDeviceEvent) == 1)
    {
      if(getTouchScreenDetails(&screenXmin, &screenXmax, &screenYmin, &screenYmax) == 1) break;
    }
  }
  if(NoDeviceEvent != 7) 
  {
    // Create Touchscreen thread
    pthread_create (&thtouchscreen, NULL, &WaitTouchscreenEvent, NULL);
  }
  else // No touchscreen detected
  {
    touchscreen_present = false;

    if ((strcmp(DisplayType, "Browser") != 0) && (strcmp(DisplayType, "hdmi") != 0)
     && (strcmp(DisplayType, "hdmi480") != 0) && (strcmp(DisplayType, "hdmi720") != 0)
     && (strcmp(DisplayType, "hdmi1080") != 0))
    {  
      SetConfigParam(PATH_PCONFIG, "webcontrol", "enabled");
      SetConfigParam(PATH_PCONFIG, "display", "Browser");
      system ("/home/pi/rpidatv/scripts/set_display_config.sh");
      system ("sudo reboot now");
    }

    // Set Screen parameters
    screenXmax = 799;
    screenXmin = 0;
    wscreen = 800;
    screenYmax = 479;
    screenYmin = 0;
    hscreen = 480;
  }

  // Calculate screen parameters
  scaleXvalue = ((float)screenXmax-screenXmin) / wscreen;
  printf ("X Scale Factor = %f\n", scaleXvalue);
  printf ("wscreen = %d\n", wscreen);
  printf ("screenXmax = %d\n", screenXmax);
  printf ("screenXmim = %d\n", screenXmin);
  scaleYvalue = ((float)screenYmax-screenYmin) / hscreen;
  printf ("Y Scale Factor = %f\n", scaleYvalue);
  printf ("hscreen = %d\n", hscreen);

  Define_Menu1();
  Define_Menu2();
  Define_Menu3();
  Define_Menu4();
  Define_Menu5();
  Define_Menu6();
  Define_Menu7();
  Define_Menu8();
  Define_Menu9();
  Define_Menu10();
  Define_Menu11();
  
  Define_Menu41();

  ReadSavedParams();

  // Create Wait Button thread
  pthread_create (&thbutton, NULL, &WaitButtonEvent, NULL);

  // Initialise direct access to the 7 inch screen
  initScreen();
  
  // Create the Serial Listener thread
  pthread_create (&thSerial_Listener, NULL, &serial_listener, NULL);

  // Create the Serial Decoder thread
  pthread_create (&thSerial_Decoder, NULL, &serial_decoder, NULL);

  Start_Highlights_Menu1();

  clearScreen();
  UpdateWindow();     // Draw the buttons

  UpdateWeb();

  usleep(700000);     // Give meter time to start

  bool CaptionDisplayed = false;
  while(app_exit == false)
  {

    if (new_serial_sentence == true)  // New valid meter reading
    {
      DecodeSerialSentence();
      new_serial_sentence = false;
      new_reading = true;
    }

   usleep(10000);

    while (freeze)
    {
      frozen = true;
    }
    frozen = false;

    if (strcmp(DMMMode, "numeric") == 0)                 ///////////////////////  DMM Large Numbers
    {
      if (ModeChanged == true)
      {
        setBackColour(0, 0, 0);
        clearScreen();
        DrawTitle();
        CurrentMenu = 1;
        Start_Highlights_Menu1();
        UpdateWindow();     // Draw the buttons
        previous_MeterReading = previous_MeterReading - 1.0; // to ensure data painted
        ModeChanged = false;
      }

      while (freeze)
      {
        frozen = true;
      }
      frozen = false;

      usleep(10);  // Control resource use!

      if ((previous_MeterReading > 9999.0) && (BasicMeterReading < -9999.0)) // No valid data yet
      {
        if (CaptionDisplayed == false)
        {
          LargeText2(wscreen * 1 / 40, 200 , 2, "No Data", &font_dejavu_sans_36);
          UpdateWeb();
          CaptionDisplayed = true;
        }
      }
      else
      {
        if ((MeterReading != previous_MeterReading) || (previous_stale_data != stale_data))
        {
          snprintf(MeterReadingString, 30, "%f", MeterReading);
          if (MeterReadingString[0] == '-')
          {
            if (MeterReadingString[1] == '1')
            {
             MeterReadingString[6] = '\0';
            }
            else
            {
             MeterReadingString[5] = '\0';
            }
          }
          else
          {
            if (MeterReadingString[0] == '1')
            {
             MeterReadingString[5] = '\0';
            }
            else
            {
             MeterReadingString[4] = '\0';
            }
          }
          rectangle(wscreen * 1 / 40, 180 , 620, 200, 0, 0, 0);
          if (stale_data == true)
          {
            strcat(MeterReadingString, " (stale)");
            LargeText2(wscreen * 1 / 40, 200 , 1, MeterReadingString, &font_dejavu_sans_72);
          }
          else
          {
            LargeText2(wscreen * 1 / 40, 200 , 2, MeterReadingString, &font_dejavu_sans_72);
          }

          UpdateWeb();
          previous_MeterReading = MeterReading;
          previous_stale_data = stale_data;
        }
      }
    }
    else if (strcmp(DMMMode, "graph") == 0)                  ///////////////////////  DMM Rolling Graph ////
    {
      if (ModeChanged == true)
      {
        setBackColour(0, 0, 0);
        DrawEmptyScreen();
        DrawYaxisLabels();
        DrawTitle();
        DrawBaselineText();
        CurrentMenu = 1;
        Start_Highlights_Menu1();
        UpdateWindow();                // Draw the buttons
        xpos = 0;                      // always start at the LHS
        StartTime = monotonic_ms();
        NextPlotTime = StartTime;
        NewestReading = -1;           // for the duration of the first scan
        ResetCSVFile();
        ModeChanged = false;
      }

      if (PlotDurationChanged == true)
      {
        setBackColour(0, 0, 0);
        DrawEmptyScreen();
        DrawYaxisLabels();  // calibration on LHS
        DrawTitle();
        DrawBaselineText();
        UpdateWindow();     // Draw the buttons
        ModeChanged = false;
        xpos = 0;               // always start at the LHS
        StartTime = monotonic_ms();
        NextPlotTime = StartTime;
        NewestReading = -1;           // for the duration of the first scan
        ResetCSVFile();
        PlotDurationChanged = false;
      }

      // Now only enter plot code if the time is right, or we are plotting every reading

      if (((NextPlotTime < monotonic_ms()) && (PlotDuration != 1)) || ((new_reading == true)  && (PlotDuration == 1)))
      {
        new_reading = false;

        if ((xpos == 0) || (xpos == 1))                   // if first or second reading
        {
          y[xpos] = (int)(400 * (MeterReading - PlotBase) / (PlotMax - PlotBase));

          if (y[xpos] < 1)
          {
            y[xpos] = 1;
          }
          if (y[xpos] > 399)
          {
            y[xpos] = 399;
          }
        }

        if ((xpos > 1) && (xpos < 501))     // intermediate readings
        {
          y[xpos] = (int)(400 * (MeterReading - PlotBase) / (PlotMax - PlotBase));

         if (y[xpos] < 1)
          {
            y[xpos] = 1;
          }
          if (y[xpos] > 399)
          {
            y[xpos] = 399;
          }

          DrawTrace(xpos, y[xpos - 2], y[xpos - 1], y[xpos]);
          DrawBaselineText();
          UpdateWeb();
        }

        if (xpos > 500)               // End of trace
        {
          xpos = -1;
          NewestReading = 0;          // End of first trace so initiate rolling 501 sample buffer
        }
        else                          // Readings 0 through 500
        {
          reading[xpos] = MeterReading;

          // set reading time here
          t = time(NULL);
          tm = localtime(&t);
          char s[64];
          size_t ret;
          ret = strftime(s, sizeof(s), "%T %a %d %b %Y", tm);
          assert(ret);
          //printf("%s\n", s);
          strcpy (readingtime[xpos], s);

          if (NewestReading >= 0)    // second or subsequent trace so use a rolling buffer
          {
            NewestReading = xpos;
          }
        }
        xpos = xpos + 1;
        NextPlotTime = NextPlotTime + (PlotDuration * 120); // = NextPlotTime + (PlotDuration * 60000) / 500
      }

      while (freeze)
      {
        frozen = true;
        NextPlotTime = monotonic_ms();
      }
      frozen = false;

      if (markeron == true)
      {
        CalculateMarkers();
      }
      tracecount++;     
    }
    else if (strcmp(DMMMode, "battery") == 0)           ///////////////// Battery Capacity Plot //////////
    {
      if (ModeChanged == true)
      {
        setBackColour(0, 0, 0);
        DrawEmptyScreen();
        DrawYaxisLabels();  // calibration on LHS
        DrawTitle();
        DrawBaselineText();
        CurrentMenu = 1;
        Start_Highlights_Menu1();
        UpdateWindow();     // Draw the buttons
        xpos = 0;               // always start at the LHS
        StartTime = monotonic_ms();
        NextPlotTime = StartTime;
        Cumulative_ma_ms = 0;
        DischargeInProgress = false;
        DischargeComplete = false;
        DischargePaused = false;
        NewestReading = -1;
        ResetCSVFile();
        ModeChanged = false;
      }

      if (PlotDurationChanged == true)
      {
        setBackColour(0, 0, 0);
        DrawEmptyScreen();
        DrawYaxisLabels();  // calibration on LHS
        DrawTitle();
        DrawBaselineText();
        UpdateWindow();     // Draw the buttons
        ModeChanged = false;
        xpos = 0;               // always start at the LHS
        StartTime = monotonic_ms();
        NextPlotTime = StartTime;
        Cumulative_ma_ms = 0;
        NewestReading = -1;
        ResetCSVFile();
        PlotDurationChanged = false;
      }

      if ((NextPlotTime < monotonic_ms()) && (DischargeInProgress == true) && (DischargePaused == false))
      {
        if (xpos == 0)  // reset clock at start of scan
        {
          StartTime = monotonic_ms();
          NextPlotTime = StartTime;
          InitialVoltage = MeterReading;
        }

        if (xpos == 1)                   // if second reading
        {
          CalculateCapacityUsed(MeterReading, PlotDuration * 120);
        }

        if ((xpos == 0) || (xpos == 1))                   // if first or second reading
        {
          y[xpos] = (int)(400 * (MeterReading - PlotBase) / (PlotMax - PlotBase));

          if (y[xpos] < 1)
          {
            y[xpos] = 1;
          }
          if (y[xpos] > 399)
          {
            y[xpos] = 399;
          }
        }

        if ((xpos > 1) && (xpos < 501))     // intermediate readings
        {
          y[xpos] = (int)(400 * (MeterReading - PlotBase) / (PlotMax - PlotBase));

         if (y[xpos] < 1)
          {
            y[xpos] = 1;
          }
          if (y[xpos] > 399)
          {
            y[xpos] = 399;
          }

          DrawTrace(xpos, y[xpos - 2], y[xpos - 1], y[xpos]);



          CalculateCapacityUsed(MeterReading, PlotDuration * 120);
          CheckDischargeLimit(MeterReading);
          if (DischargeComplete == true)
          {
            if (CurrentMenu == 9)
            {
              Start_Highlights_Menu9();
              UpdateWindow();            // Display that the Load is off
            }
            if (CurrentMenu == 7)
            {
              Start_Highlights_Menu7();
              UpdateWindow();            // Display that the Load is off
            }
          }
        }

        DrawBaselineText();
        UpdateWeb();

        if (xpos > 500)               // End of trace
        {
          DischargeInProgress = false;                              // single span only
          xpos = -1;
        }
        else                                                        // Readings 0 - 500
        {
          reading[xpos] = MeterReading;

          // set reading time here
          t = time(NULL);
          tm = localtime(&t);
          char s[64];
          size_t ret;
          ret = strftime(s, sizeof(s), "%T %a %d %b %Y", tm);
          assert(ret);
          //printf("%s\n", s);
          strcpy (readingtime[xpos], s);
        }

        if (DischargePaused == false)
        {
          xpos = xpos + 1;
          NextPlotTime = NextPlotTime + (PlotDuration * 120); // = NextPlotTime + (PlotDuration * 60000) / 500
        }
      }

      if ((new_reading == true) && (strcmp(DMMMode, "battery") == 0))
      {
        DrawBaselineText();
        new_reading = false;
      }

      while (freeze)
      {
        frozen = true;
        NextPlotTime = monotonic_ms();
      }
      frozen = false;

      if (markeron == true)
      {
        CalculateMarkers();
      }
      tracecount++;     
    }
  }

  printf("Waiting for Serial Listener Thread to exit..\n");
  pthread_join(thSerial_Listener, NULL);
  printf("Waiting for Serial Decoder Thread to exit..\n");
  pthread_join(thSerial_Decoder, NULL);
  printf("Waiting for Button Thread to exit..\n");
  pthread_join(thbutton, NULL);
  pthread_mutex_destroy(&text_lock);
}

