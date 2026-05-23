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
#include <fftw3.h>

#include "screen.h"
#include "font/font.h"
#include "touch.h"
#include "graphics.h"
#include "timing.h"
//#include "lime.h"
#include "pluto.h"
#include "fft.h"
#include "ffunc.h"

#define PI 3.14159265358979323846

pthread_t thbutton;
pthread_t thMeter_Movement;
pthread_mutex_t text_lock;
pthread_t thwebclick;     //  Listens for mouse clicks from web interface
pthread_t thtouchscreen;  //  listens to the touchscreen   

int fd = 0;
int wscreen, hscreen;
float scaleXvalue, scaleYvalue; // Coeff ratio from Screen/TouchArea

typedef struct
{
  int r,g,b;
} color_t;

typedef struct
{
  char Text[255];
  color_t  Color;
} status_t;


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

#define PATH_CONFIG "/home/pi/rpidatv/src/pluto_nf_meter/pluto_nf_meter_config.txt"
#define PATH_PCONFIG "/home/pi/rpidatv/scripts/portsdown_config.txt"

#define MAX_BUTTON 675
int IndexButtonInArray=0;
button_t ButtonArray[MAX_BUTTON];

int CurrentMenu = 1;
int CallingMenu = 1;
char KeyboardReturn[64];

static bool app_exit = false;

extern double frequency_actual_rx;
extern double bandwidth;
extern int y[260];
extern float rawpwr[260];

static pthread_t screen_thread_obj;
// static pthread_t lime_thread_obj; // to be deleted
static pthread_t sdr_thread_obj;
static pthread_t fft_thread_obj;

bool NewFreq = false;
bool NewGain = false;
bool NewSpan = false;
bool NewCal  = false;

bool Calibrated = false;

int scaledX, scaledY;

int wbuttonsize = 100;
int hbuttonsize = 50;
int rawX, rawY;
int FinishedButton = 0;
bool freeze = false;
bool frozen = false;
bool PeakPlot = false;
bool PortsdownExitRequested = false;

int startfreq = 0;
int stopfreq = 0;
int rbw = 0;
char PlotTitle[63] = "-";
bool ContScan = false;

int centrefreq = 437000;
int span = 5120;
int plutogain = 70;
int nf_bandwidth = 5000;
int pfreq1 = 146500;
int pfreq2 = 437000;
int pfreq3 = 748000;
int pfreq4 = 1255000;
int pfreq5 = 2409000;
char PlutoIP[16];         // Pluto IP address

float MAIN_SPECTRUM_TIME_SMOOTH;

int ScansforLevel = 10;
float ENR = 15.0;
float RF_ENR = 15.0; // Used for transverters
float Tsoff = 290.0;
float Tson;
bool CalibrateRequested = false;
int meter_deflection = 0;
bool ModeChanged = true;
int MeterScale = 1;
float ActiveZero = 30.0;
float ActiveFSD = 0.0;
uint8_t NoiseSourceGPIO = 11;   // Noise source pin 26 wPi 11

int tracecount = 0;  // Used for speed testing
int exit_code;

int yscalenum = 18;       // Numerator for Y scaling fraction
int yscaleden = 30;       // Denominator for Y scaling fraction
int yshift    = 5;        // Vertical shift (pixels) for Y

int xscalenum = 25;       // Numerator for X scaling fraction
int xscaleden = 20;       // Denominator for X scaling fraction

bool webcontrol = false;   // Enables webcontrol on a Portsdown 4
bool LargeNumbers = false;  // Enables large numbers instead of parameters
bool LargeNumbersDisplayed = false;  // Used to clear screen area when large numbers are toggled off
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

///////////////////////////////////////////// FUNCTION PROTOTYPES ///////////////////////////////

void GetConfigParam(char *, char *, char *);
void SetConfigParam(char *, char *, char *);
int CheckWebCtlExists();
void ReadSavedParams();
void *WaitTouchscreenEvent(void * arg);
void *WebClickListener(void * arg);
void parseClickQuerystring(char *query_string, int *x_ptr, int *y_ptr);
FFUNC touchscreenClick(ffunc_session_t * session);
int CheckPlutoConnect();
int CheckPlutoIPConnect();
int PlutoConnectTest();
void do_snapcheck();
int IsImageToBeChanged(int, int);
void MsgBox4(char *, char *, char *, char *);
void UpdateWeb();
void Keyboard(char *, char *, int);
int openTouchScreen(int);
int getTouchScreenDetails(int*, int* ,int* ,int*);
void TransformTouchMap(int, int);
int IsButtonPushed(int, int, int);
int IsMenuButtonPushed(int, int);
void ChangeSmallMeterScale(int);
void ToggleLargeNumbers();
int InitialiseButtons();
int AddButton(int, int, int, int);
int ButtonNumber(int, int);
int CreateButton(int, int);
int AddButtonStatus(int, char *, color_t *);
void AmendButtonStatus(int, int, char *, color_t *);
void DrawButton(int);
void SetButtonStatus(int ,int);
int getTouchSampleThread(int *rawX, int *rawY, int *rawPressure);
int getTouchSample(int *rawX, int *rawY, int *rawPressure);
void UpdateWindow();
void wait_touch();
void SetSpanWidth(int);
void AdjustPlutoGain(int);
void SetENR();
void SetRF_ENR();
void SetFreqPreset(int);
void ShiftFrequency(int);
void CalcSpan();
void ChangeLabel(int);
void RedrawDisplay();
void *WaitButtonEvent(void * arg);
void Define_Menu1();
void Start_Highlights_Menu1();
void Define_Menu2();
void Start_Highlights_Menu2();
void Define_Menu3();
void Define_Menu4();
void Define_Menu5();
void Define_Menu6();
void Start_Highlights_Menu6();
void Define_Menu7();
void Start_Highlights_Menu7();
void Define_Menu8();
void Define_Menu9();
void Define_Menu10();
void Start_Highlights_Menu10();
void Define_Menu41();
void DrawEmptyScreen();  
void DrawYaxisLabels();  
void DrawSettings();
void DrawTrace(int, int, int, int);
void DrawMeterArc();
void DrawMeterTicks(int, int);
void Draw5MeterLabels(float, float);
void *MeterMovement(void * arg);
void CalibrateSystem();
static void cleanexit(int);
static void terminate(int sig);


///////////////////////////////////////////// SCREEN AND TOUCH UTILITIES ////////////////////////

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
 * @brief sets the value of Param in PathConfigFile from a program variable
 *        Used to store the configuration in portsdown_config.txt
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

  GetConfigParam(PATH_CONFIG, "centrefreq", response);
  centrefreq = atoi(response);

  GetConfigParam(PATH_CONFIG, "span", response);
  span = atoi(response);

  CalcSpan();

  GetConfigParam(PATH_CONFIG, "plutogain", response);
  plutogain = atoi(response);

  strcpy(response, "146500");
  GetConfigParam(PATH_CONFIG, "pfreq1", response);
  pfreq1 = atoi(response);

  GetConfigParam(PATH_CONFIG, "pfreq2", response);
  pfreq2 = atoi(response);

  GetConfigParam(PATH_CONFIG, "pfreq3", response);
  pfreq3 = atoi(response);

  GetConfigParam(PATH_CONFIG, "pfreq4", response);
  pfreq4 = atoi(response);

  GetConfigParam(PATH_CONFIG, "pfreq5", response);
  pfreq5 = atoi(response);

  GetConfigParam(PATH_CONFIG, "enr", response);
  ENR = atof(response);
  RF_ENR = ENR;

  strcpy(PlotTitle, "-");  // this is the "do not display" response
  GetConfigParam(PATH_CONFIG, "title", PlotTitle);

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

  // Read Pluto IP from Portsdown Config file
  GetConfigParam(PATH_PCONFIG, "plutoip", PlutoIP);
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


/***************************************************************************//**
 * @brief Checks whether a Pluto responds to ping on pluto.local
 *
 * @param 
 *
 * @return 0 if present, 1 if absent
*******************************************************************************/

int CheckPlutoConnect()
{
  FILE *fp;
  char response[127];

  /* Open the command for reading. */
  fp = popen("timeout 0.2 ping pluto.local -c1 | head -n 5 | tail -n 1 | grep -o \"1 received,\" | head -c 11", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response, 12, fp) != NULL)
  {
    printf("%s", response);
  }
  /* close */
  pclose(fp);
  if (strcmp (response, "1 received,") == 0)
  {
    return 0;
  }
  else
  {
    return 1;
  }
}


/***************************************************************************//**
 * @brief Checks whether a Pluto is connected on the IP address in the Config File
 *
 * @param 
 *
 * @return 0 if present, 1 if absent
*******************************************************************************/

int CheckPlutoIPConnect()
{
  FILE *fp;
  char response[127];
  char plutoping[127];

  strcpy(plutoping, "timeout 0.2 ping ");
  strcat(plutoping, PlutoIP);
  strcat(plutoping, " -c1 | head -n 5 | tail -n 1 | grep -o \"1 received,\" | head -c 11");

  /* Open the command for reading. */
  // fp = popen("timeout 0.2 ping pluto.local -c1 | head -n 5 | tail -n 1 | grep -o \"1 received,\" | head -c 11", "r");

  fp = popen(plutoping, "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response, 12, fp) != NULL)
  {
    //printf("%s", response);
  }

  /* close */
  pclose(fp);
  if (strcmp (response, "1 received,") == 0)
  {
    return 0;
  }
  else
  {
    return 1;
  }
}


/***************************************************************************//**
 * @brief Checks and reports on Pluto Connection
 *
 * @param 
 *
 * @return 0 if OK, 1 if there is a problem
*******************************************************************************/

int PlutoConnectTest()
{

  // Check whether Pluto is connected on stored IP address
  if (CheckPlutoIPConnect() == 0)
  {
    return 0;  // Connected!
  }
  else
  {
    // Check whether Pluto is connected on pluto.local
    if (CheckPlutoConnect() == 0)
    {
      MsgBox4("Pluto Connected, but", "IP Address stored does not match",
              "Please use Settings menu to enter IP Address", "Touch Screen to Continue");
      wait_touch();
      return 1;
    }
    else
    {
      MsgBox4("No Pluto Connected", "Please check connections",
              " ", "Touch Screen to Continue");
      wait_touch();
      return 1;
    }
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


void MsgBox4(char *message1, char *message2, char *message3, char *message4)
{
  // Display a 4-line message
  setForeColour(255, 255, 255);    // White text
  setBackColour(0, 0, 0);          // on Black
  const font_t *font_ptr = &font_dejavu_sans_32;
  int txtht =  font_ptr->ascent;
  int linepitch = (14 * txtht) / 10;

  clearScreen();
  pthread_mutex_lock(&text_lock);
  TextMid2(wscreen / 2, hscreen - (linepitch * 2), message1, font_ptr);
  TextMid2(wscreen / 2, hscreen - 2 * (linepitch * 2), message2, font_ptr);
  TextMid2(wscreen / 2, hscreen - 3 * (linepitch * 2), message3, font_ptr);
  TextMid2(wscreen / 2, hscreen - 4 * (linepitch * 2), message4, font_ptr);
  pthread_mutex_unlock(&text_lock);
  UpdateWeb();

  // printf("MsgBox4 called\n");
}


void UpdateWeb()
{
  // Called after any screen update to update the web page if required.

  if(webcontrol == true)
  {
    system("/home/pi/rpidatv/scripts/single_screen_grab_for_web.sh &");
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
  clearScreen();
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
      pthread_mutex_lock(&text_lock);
      Text2(10, 420 , RequestText, font_ptr);
      pthread_mutex_unlock(&text_lock);

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
        pthread_mutex_lock(&text_lock);
        TextMid2(i * charPitch, 330, thischar, font_ptr);
        pthread_mutex_unlock(&text_lock);
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

        if (strlen(EditText) < 23) // Don't let it overflow
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

  scaledX = x / scaleXvalue;
  scaledY = hscreen - y / scaleYvalue;
}


int IsButtonPushed(int NbButton,int x,int y)
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

  // Check for small meter scale "buttons" if normal screen displayed (not keyboard)
  if (CurrentMenu != 41)
  {
    if ((scaledX <= 600) && (scaledX >= 350)
     && (scaledY <= 470) && (scaledY >= 270))
    {
      ChangeSmallMeterScale(scaledX);
    } 

    // Check for Large Number select/deselct
    if ((scaledX <= 300) && (scaledX >= 100)
     && (scaledY <= 270) && (scaledY >= 70))
    {
      ToggleLargeNumbers();
    } 
  }

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


void ChangeSmallMeterScale(int scaledX)
{
    if (scaledX >= 475)
    {
      MeterScale = MeterScale + 1;
    }
    else
    {
      MeterScale = MeterScale - 1;
    }
    if (MeterScale < 1)
    {
      MeterScale = 1;
    }
    if (MeterScale > 5)
    {
      MeterScale = 5;
    }
    switch (MeterScale)
    {
    case 1:                   // 30 - 0
      ActiveZero = 30.0;
      ActiveFSD = 0.0;
    break;
    case 2:                   // 30 - 20
      ActiveZero = 30.0;
      ActiveFSD = 20.0;
    break;
    case 3:                   // 20 - 10
      ActiveZero = 20.0;
      ActiveFSD = 10.0;
    break;
    case 4:                   // 10 - 0
      ActiveZero = 10.0;
      ActiveFSD = 0.0;
    break;
    case 5:                   // 5 - 0
      ActiveZero = 5.0;
      ActiveFSD = 0.0;
    break;
    }
    ModeChanged = true;
} 


void ToggleLargeNumbers()
{
  if (LargeNumbers == true)
  {
    LargeNumbers = false;
  }
  else
  {
    LargeNumbers = true;
  }
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


int AddButton(int x,int y,int w,int h)
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
  // Returns the Button Number (0 - 674) from the Menu number and the button position
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
  // Provide Menu number (int 1 - 46), Button Position (0 snap, 1 top, 8/9 bottom)
  // return button number

  // Menus 1 - 20 are classic 15-button menus
  // Menus 21 - 40 are undefined and have no button numbers allocated
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

  if ((MenuIndex != 41) && (MenuIndex != 1) && (MenuIndex != 6))   // All except Main, keyboard, Span
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
  else if (MenuIndex == 1)   // Main Menu
  {
    if (ButtonPosition == 0)  // Capture
    {
      x = 0;
      y = 0;
      w = 90;
      h = 50;
    }

    if ((ButtonPosition > 0) && (ButtonPosition < 5))  // 6 right hand buttons
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
  else if (MenuIndex == 6)   // Scan Width Menu
  {
    if (ButtonPosition == 0)  // Capture
    {
      x = 0;
      y = 0;
      w = 90;
      h = 50;
    }

    if ((ButtonPosition > 0) && (ButtonPosition < 6))  // 6 right hand buttons
    {
      x = normal_xpos;
      y = 480 - (ButtonPosition * 60);
      w = normal_width;
      h = 50;
    }
    if (ButtonPosition == 6) // 10
    {
      x = normal_xpos;  
      y = 480 - (6 * 60);
      w = 50;
      h = 50;
    }
    if (ButtonPosition == 7) // 20
    {
      x = 710;  // = normal_xpos + 50 button width + 20 gap
      y = 480 - (6 * 60);
      w = 50;
      h = 50;
    }
    if ((ButtonPosition > 7) && (ButtonPosition < 10))  // Bottom 2 buttons
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


int AddButtonStatus(int ButtonIndex,char *Text,color_t *Color)
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
  button_t *Button = &(ButtonArray[ButtonIndex]);
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
  setBackColour(Button->Status[Button->NoStatus].Color.r,
                Button->Status[Button->NoStatus].Color.g,
                Button->Status[Button->NoStatus].Color.b);

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
    TextMid2(Button->x + Button->w/2, Button->y +Button->h * 11 /16, line1, &font_dejavu_sans_20);	
    TextMid2(Button->x + Button->w/2, Button->y +Button->h * 3 / 16, line2, &font_dejavu_sans_20);	
    pthread_mutex_unlock(&text_lock);
  }
  else                                              // One line only
  {
    pthread_mutex_lock(&text_lock);
    if (CurrentMenu <= 9)
    {
      TextMid2(Button->x + Button->w/2, Button->y + Button->h/2, label, &font_dejavu_sans_20);
    }
    else                // Keyboard
    {
      TextMid2(Button-> x +Button->w/2, Button->y + Button->h/2 - hscreen / 64, label, &font_dejavu_sans_28);
    }
    pthread_mutex_unlock(&text_lock);
  }
}


void SetButtonStatus(int ButtonIndex,int Status)
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


void SetSpanWidth(int button)
{  
  char ValueToSave[63];

  // Stop the scan at the end of the current one and wait for it to stop
  freeze = true;
  while(! frozen)
  {
    usleep(10);                                   // wait till the end of the scan
  }

  switch (button)
  {
    case 2:
      span = 2084;
    break;
    case 3:
      span = 5120;
    break;
    case 4:
      span = 10240;
    break;
    case 5:
      span = 20480;
    break;
  }

  // Store the new span
  snprintf(ValueToSave, 63, "%d", span);
  SetConfigParam(PATH_CONFIG, "span", ValueToSave);
  printf("span set to %d \n", span);

  // Trigger the span change
  CalcSpan();
  NewSpan = true;

  DrawSettings();       // New labels
  freeze = false;
  Calibrated = false;
}


void AdjustPlutoGain(int button)
{  
  char ValueToSave[63];

  // Stop the scan at the end of the current one and wait for it to stop
  freeze = true;
  while(! frozen)
  {
    usleep(10);                                   // wait till the end of the scan
  }

  switch (button)
  {
    case 3:      // up
      if (plutogain >= 75)
      {
        plutogain = 80;
      }
      else
      {
        plutogain = plutogain + 5;
      }
    break;
    case 5:      // down
      if (plutogain <= 5)
      {
        plutogain = 0;
      }
      else
      {
        plutogain = plutogain - 5;
      }
    break;
  }

  // Store the new gain
  snprintf(ValueToSave, 63, "%d", plutogain);
  SetConfigParam(PATH_CONFIG, "plutogain", ValueToSave);
  printf("plutogain set to %d \n", plutogain);

  // Trigger the gain change
  NewGain = true;
  Calibrated = false;
  freeze = false;
}


void SetENR()
{  
  char ValueToSave[63];
  char RequestText[64];
  char InitText[63];
  float newENR;

  // Stop the scan at the end of the current one and wait for it to stop
  freeze = true;
  while(! frozen)
  {
    usleep(10);                                   // wait till the end of the scan
  }

  // Define request string
  strcpy(RequestText, "Enter the new ENR (range 1 to 30) in dB");

  // Define initial value
  snprintf(InitText, 10, "%.2f", ENR);
 
  // Ask for the new value
  do
  {
    Keyboard(RequestText, InitText, 10);
    newENR = atof(KeyboardReturn);
  }
  while ((newENR < 1.0) || (newENR > 30.0));

  ENR = newENR;
  snprintf(ValueToSave, 63, "%.2f", newENR);

  // Store the new ENR and recalculate
  SetConfigParam(PATH_CONFIG, "enr", ValueToSave);
  printf("new ENR set to %.2f \n", ENR);
  Tson = Tsoff * (1 + pow(10, (ENR / 10)));
  RF_ENR = ENR;

  Calibrated = false;

  // Tidy up, paint around the screen and then unfreeze
  clearScreen();
  DrawEmptyScreen();  // Required to set A value, which is not set in DrawTrace
  DrawYaxisLabels();  // dB calibration on LHS
  DrawSettings();     // Start, Stop RBW, Ref level and Title
  ModeChanged = true; // Redraw the meter scale
  freeze = false;
}


void SetRF_ENR()
{  
  char RequestText[64];
  char InitText[63];
  float newENR;

  // Stop the scan at the end of the current one and wait for it to stop
  freeze = true;
  while(! frozen)
  {
    usleep(10);                                   // wait till the end of the scan
  }

  // Define request string
  strcpy(RequestText, "Enter the ENR at the Tvrtr input frequency");

  // Define initial value
  snprintf(InitText, 10, "%.2f", RF_ENR);
 
  // Ask for the new value
  do
  {
    Keyboard(RequestText, InitText, 10);
    newENR = atof(KeyboardReturn);
  }
  while ((newENR < 1.0) || (newENR > 30.0));

  RF_ENR = newENR;

  // Tidy up, paint around the screen and then unfreeze
  clearScreen();
  DrawEmptyScreen();  // Required to set A value, which is not set in DrawTrace
  DrawYaxisLabels();  // dB calibration on LHS
  DrawSettings();     // Start, Stop RBW, Ref level and Title
  ModeChanged = true; // Redraw the meter scale
  freeze = false;
}


void SetFreqPreset(int button)
{  
  char ValueToSave[63];
  char RequestText[64];
  char InitText[63];
  div_t div_10;
  div_t div_100;
  div_t div_1000;
  int amendfreq;

  if (CallingMenu == 7)
  {
    // Stop the scan at the end of the current one and wait for it to stop
    freeze = true;
    while(! frozen)
    {
      usleep(10);                                   // wait till the end of the scan
    }

    switch (button)
    {
      case 2:
        centrefreq = pfreq1;
      break;
      case 3:
        centrefreq = pfreq2;
      break;
      case 4:
        centrefreq = pfreq3;
      break;
      case 5:
        centrefreq = pfreq4;
      break;
      case 6:
        centrefreq = pfreq5;
      break;
    }

    // Store the new frequency
    snprintf(ValueToSave, 63, "%d", centrefreq);
    SetConfigParam(PATH_CONFIG, "centrefreq", ValueToSave);
    printf("Centre Freq set to %d \n", centrefreq);

    // Calculate the new settings
    CalcSpan();

    // Trigger the frequency change
    NewFreq = true;

    DrawSettings();       // New labels
    freeze = false;
    Calibrated = false;
  }
  else if (CallingMenu == 10)  // Amend the preset frequency
  {

    // Stop the scan at the end of the current one and wait for it to stop
    freeze = true;
    while(! frozen)
    {
      usleep(10);                                   // wait till the end of the scan
    }

    switch (button)
    {
      case 2:
        amendfreq = pfreq1;
      break;
      case 3:
        amendfreq = pfreq2;
      break;
      case 4:
        amendfreq = pfreq3;
      break;
      case 5:
        amendfreq = pfreq4;
      break;
      case 6:
        amendfreq = pfreq5;
      break;
    }

    // Define request string
    strcpy(RequestText, "Enter new preset frequency in MHz");

    // Define initial value and convert to MHz
    div_10 = div(amendfreq, 10);
    div_1000 = div(amendfreq, 1000);

    if(div_10.rem != 0)  // last character not zero, so make answer of form xxx.xxx
    {
      snprintf(InitText, 10, "%d.%03d", div_1000.quot, div_1000.rem);
    }
    else
    {
      div_100 = div(amendfreq, 100);
      if(div_100.rem != 0)  // last but one character not zero, so make answer of form xxx.xx
      {
        snprintf(InitText, 10, "%d.%02d", div_1000.quot, div_1000.rem / 10);
      }
      else
      {
        if(div_1000.rem != 0)  // last but two character not zero, so make answer of form xxx.x
        {
          snprintf(InitText, 10, "%d.%d", div_1000.quot, div_1000.rem / 100);
        }
        else  // integer MHz, so just xxx (no dp)
        {
          snprintf(InitText, 10, "%d", div_1000.quot);
        }
      }
    }

    // Ask for the new value
    do
    {
      Keyboard(RequestText, InitText, 10);
    }
    while (strlen(KeyboardReturn) == 0);

    amendfreq = (int)((1000 * atof(KeyboardReturn)) + 0.1);
    snprintf(ValueToSave, 63, "%d", amendfreq);

    switch (button)
    {
      case 2:
        SetConfigParam(PATH_CONFIG, "pfreq1", ValueToSave);
        printf("Preset Freq 1 set to %d \n", amendfreq);
        pfreq1 = amendfreq;
      break;
      case 3:
        SetConfigParam(PATH_CONFIG, "pfreq2", ValueToSave);
        printf("Preset Freq 2 set to %d \n", amendfreq);
        pfreq2 = amendfreq;
      break;
      case 4:
        SetConfigParam(PATH_CONFIG, "pfreq3", ValueToSave);
        printf("Preset Freq 3 set to %d \n", amendfreq);
        pfreq3 = amendfreq;
      break;
      case 5:
         SetConfigParam(PATH_CONFIG, "pfreq4", ValueToSave);
        printf("Preset Freq 4 set to %d \n", amendfreq);
        pfreq4 = amendfreq;
      break;
      case 6:
        SetConfigParam(PATH_CONFIG, "pfreq5", ValueToSave);
        printf("Preset Freq 5 set to %d \n", amendfreq);
        pfreq5 = amendfreq;
      break;
    }
    centrefreq = amendfreq;

    // Store the new frequency
    snprintf(ValueToSave, 63, "%d", centrefreq);
    SetConfigParam(PATH_CONFIG, "centrefreq", ValueToSave);
    printf("Centre Freq set to %d \n", centrefreq);

    // Trigger the frequency change
    NewFreq = true;

    // Tidy up, paint around the screen and then unfreeze
    CalcSpan();         // work out start and stop freqs
    clearScreen();
    DrawEmptyScreen();  // Required to set A value, which is not set in DrawTrace
    DrawYaxisLabels();  // dB calibration on LHS
    DrawSettings();     // Start, Stop RBW, Ref level and Title
    freeze = false;
    Calibrated = false;
  }
  ModeChanged = true;
}


void ShiftFrequency(int button)
{  
  char ValueToSave[63];

  // Stop the scan at the end of the current one and wait for it to stop
  freeze = true;
  while(! frozen)
  {
    usleep(10);                                   // wait till the end of the scan
  }

  switch (button)
  {
    case 5:                                                       // Left 1/10 span
      centrefreq = centrefreq - (span * 125) / 1280;
    break;
    case 6:                                                       // Left 1/10 span
      centrefreq = centrefreq + (span * 125) / 1280;
    break;
  }

  // Store the new frequency
  snprintf(ValueToSave, 63, "%d", centrefreq);
  SetConfigParam(PATH_CONFIG, "centrefreq", ValueToSave);
  printf("Centre Freq set to %d \n", centrefreq);

  // Calculate the new settings
  CalcSpan();

  // Trigger the frequency change
  NewFreq = true;

  DrawSettings();       // New labels
  freeze = false;
  Calibrated = false;
  ModeChanged = true;
}


void CalcSpan()    // takes centre frequency and span and calulates startfreq and stopfreq
{
  startfreq = centrefreq - (span * 125) / 256;
  stopfreq =  centrefreq + (span * 125) / 256;
  frequency_actual_rx = 1000.0 * (float)(centrefreq);
  bandwidth = (float)(span * 1000);
  nf_bandwidth = span * 250 / 256;   //nf bandwidth in kHz

  // set a sensible time constant for the fft display
  if (bandwidth >= 2048000)
  {
    MAIN_SPECTRUM_TIME_SMOOTH =  0.98;
  }
  else
  {
    MAIN_SPECTRUM_TIME_SMOOTH =  0.90;
  }

  // Set levelling time for NF Measurement
  switch (span)
  {
    case 512:                                            // 500 kHz use 15
      ScansforLevel = 15;
      break;
    case 1024:                                            // 1 MHz use 10
      ScansforLevel = 10;
      break;
    case 2048:                                            // 2 MHz use 10
      ScansforLevel = 10;
      break;
    case 5120:                                            // 5 MHz use 10
      ScansforLevel = 10;
      break;
    case 10240:                                            // 10 MHz use 10
      ScansforLevel = 10;
      break;
    case 20480:                                            // 20 MHz use 10
      ScansforLevel = 10;
      break;
    default:
      ScansforLevel = 10;
  }
}


void ChangeLabel(int button)
{
  char RequestText[64];
  char InitText[63];
  div_t div_10;
  div_t div_100;
  div_t div_1000;
  char ValueToSave[63];
  
  // Stop the scan at the end of the current one and wait for it to stop
  freeze = true;
  while(! frozen)
  {
    usleep(10);                                   // wait till the end of the scan
  }

  switch (button)
  {
    case 2:                                                       // Centre Freq
      // Define request string
      strcpy(RequestText, "Enter new centre frequency in MHz");

      // Define initial value and convert to MHz
      div_10 = div(centrefreq, 10);
      div_1000 = div(centrefreq, 1000);

      if(div_10.rem != 0)  // last character not zero, so make answer of form xxx.xxx
      {
        snprintf(InitText, 10, "%d.%03d", div_1000.quot, div_1000.rem);
      }
      else
      {
        div_100 = div(centrefreq, 100);
        if(div_100.rem != 0)  // last but one character not zero, so make answer of form xxx.xx
        {
          snprintf(InitText, 10, "%d.%02d", div_1000.quot, div_1000.rem / 10);
        }
        else
        {
          if(div_1000.rem != 0)  // last but two character not zero, so make answer of form xxx.x
          {
            snprintf(InitText, 10, "%d.%d", div_1000.quot, div_1000.rem / 100);
          }
          else  // integer MHz, so just xxx (no dp)
          {
            snprintf(InitText, 10, "%d", div_1000.quot);
          }
        }
      }

      // Ask for the new value
      do
      {
        Keyboard(RequestText, InitText, 10);
      }
      while (strlen(KeyboardReturn) == 0);

      centrefreq = (int)((1000 * atof(KeyboardReturn)) + 0.1);

      snprintf(ValueToSave, 63, "%d", centrefreq);
      SetConfigParam(PATH_CONFIG, "centrefreq", ValueToSave);
      printf("Centre Freq set to %d \n", centrefreq);
      NewFreq = true;
      ModeChanged = true;
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

      Keyboard(RequestText, InitText, 30);
  
      if(strlen(KeyboardReturn) > 0)
      {
        strcpy(PlotTitle, KeyboardReturn);
      }
      else
      {
        strcpy(PlotTitle, "-");
      }
      SetConfigParam(PATH_CONFIG, "title", PlotTitle);
      printf("Plot Title set to: %s\n", KeyboardReturn);
      break;
  }

  // Tidy up, paint around the screen and then unfreeze
  CalcSpan();         // work out start and stop freqs
  clearScreen();
  DrawEmptyScreen();  // Required to set A value, which is not set in DrawTrace
  DrawYaxisLabels();  // dB calibration on LHS
  DrawSettings();     // Start, Stop RBW, Ref level and Title
  ModeChanged = true;
  freeze = false;
}


void RedrawDisplay()
{
  // Redraw the Y Axis
  DrawYaxisLabels();

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
      printf("Button Event %d, Entering Menu 1 Case Statement\n",i);
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
        case 2:                                            // Select Settings
          printf("Settings Menu 3 Requested\n");
          CurrentMenu=3;
          UpdateWindow();
          break;
        case 3:                                            // Set-up
          printf("Set-up Menu 2 Requested\n");
          CurrentMenu = 2;
          Start_Highlights_Menu2();
          UpdateWindow();
          break;
        case 4:                                            // Mode
          printf("Mode Menu 5 Requested\n");
          CurrentMenu = 5;
          UpdateWindow();
          break;
        case 5:                                            // Left Arrow
        case 6:                                            // Right Arrow
          printf("Shift Frequency Requested\n");
          ShiftFrequency(i);
          break;
        case 7:                                            // System
          printf("System Menu 4 Requested\n");
          CurrentMenu=4;
          UpdateWindow();
          break;
        case 8:                                            // Exit to Portsdown
          if(PortsdownExitRequested)
          {
            freeze = true;
            usleep(100000);
            setBackColour(0, 0, 0);
            clearScreen();
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
        case 9:
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
      if ((i != 8) && (PortsdownExitRequested == true))
      {
        PortsdownExitRequested = false;
        Start_Highlights_Menu1();
        UpdateWindow();
      }
      continue;  // Completed Menu 1 action, go and wait for touch
    }

    if (CurrentMenu == 2)  // Set-up Menu
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
        case 2:                                            // Set ENR
          if (Calibrated == false)
          {
            SetENR();
          }
          else
          {
            SetRF_ENR();
          }
          Start_Highlights_Menu2();
          UpdateWindow();
          break;
        case 3:                                            // Pluto Gain Up
        case 5:                                            // Pluto Gain Down
          if (Calibrated == false)
          {
            AdjustPlutoGain(i);
            Start_Highlights_Menu2();
            UpdateWindow();
          }
          break;
        case 4:                                            // No action, show Pluto Gain
          break;
        case 6:                                            // Calibrate
          if (Calibrated == false)
          {
            CalibrateSystem();
          }
          else
          {
            Calibrated = false;
          }
          Start_Highlights_Menu2();
          UpdateWindow();
          break;
        case 7:                                            // Return to Main Menu
          printf("Main Menu 1 Requested\n");
          CurrentMenu=1;
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 8:
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
        case 2:                                            // Centre Freq
          ChangeLabel(i);
          CurrentMenu = 3;
          UpdateWindow();          
          break;
        case 3:                                            // Frequency Presets
          printf("Frequency Preset Menu 7 Requested\n");
          CurrentMenu = 7;
          Start_Highlights_Menu7();
          UpdateWindow();
          break;
        case 4:                                            // Span Width
          printf("Span Width Menu 6 Requested\n");
          CurrentMenu = 6;
          Start_Highlights_Menu6();
          UpdateWindow();
          break;
        case 5:                                            // Set-up Menu
          printf("Set-up Menu 2 Requested\n");
          CurrentMenu=2;
          Start_Highlights_Menu2();
          UpdateWindow();
          break;
        case 6:                                            // Title
          ChangeLabel(i);
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
          printf("Menu 3 Error\n");
      }
      continue;  // Completed Menu 3 action, go and wait for touch
    }

    if (CurrentMenu == 4)  // System Menu
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
          while (frozen == false)   // Stop writing to screen
          {
            usleep(10);
          }
          do_snapcheck();           // Call the snapcheck

          initScreen();             // Start the screen again
          DrawEmptyScreen();        // Required to set A value, which is not set in DrawTrace
          DrawYaxisLabels();        // dB calibration on LHS
          DrawSettings();           // Start, Stop RBW, Ref level and Title
          DrawMeterArc();
          DrawMeterTicks(5, 1);
          Draw5MeterLabels(ActiveZero, ActiveFSD);
          UpdateWindow();           // Draw the buttons
          freeze = false;           // Restart the scan
          break;
        case 3:                                            // Restart NF Meter
          freeze = true;
          usleep(100000);
          setBackColour(0, 0, 0);
          clearScreen();
          usleep(1000000);
          closeScreen();
          cleanexit(148);
          break;
        case 4:                                            // Exit to Portsdown
          freeze = true;
          usleep(100000);
          setBackColour(0, 0, 0);
          clearScreen();
          usleep(1000000);
          closeScreen();
          cleanexit(207);
          break;
        case 5:                                            // Shutdown
          system("sudo shutdown now");
          break;
        case 6:                                            // ReCal Lime
          //NewCal = true;
          break;
        case 7:                                            // Return to Main Menu
          printf("Main Menu 1 Requested\n");
          CurrentMenu=1;
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
        case 2:                                            // Setup with spectrum and dial

          break;
        case 3:                                            // Full Screen NF

          break;
        case 4:                                            // Full Screen Gain
          break;
        case 5:                                            // 
          break;
        case 6:                                            // Config Menu
          printf("Config Menu 9 Requested\n");
          CurrentMenu = 9;
          UpdateWindow();
          break;
        case 7:                                            // Return to Main Menu
          printf("Main Menu 1 Requested\n");
          CurrentMenu=1;
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

    if (CurrentMenu == 6)  // Span Width Menu
    {
      printf("Button Event %d, Entering Menu 6 Case Statement\n",i);
      CallingMenu = 6;
      switch (i)
      {
        case 0:                                            // Capture Snap
          freeze = true; 
          SetButtonStatus(ButtonNumber(CurrentMenu, 0), 1);
          UpdateWindow();
          while(! frozen);
          system("/home/pi/rpidatv/scripts/snap2.sh");
          SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0);
          Start_Highlights_Menu6();
          UpdateWindow();
          freeze = false;
          break;
        case 2:                                            // 2
        case 3:                                            // 5
        case 4:                                            // 10
        case 5:                                            // 20
          SetSpanWidth(i);
          CurrentMenu = 6;
          Start_Highlights_Menu6();
          UpdateWindow();
          break;
        case 8:                                            // Return to Settings Menu
          CurrentMenu=3;
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
          Start_Highlights_Menu6();
          UpdateWindow();
          break;
        default:
          printf("Menu 6 Error\n");
      }
      continue;  // Completed Menu 6 action, go and wait for touch
    }

    if (CurrentMenu == 7)  // Frequency Preset Menu
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
        case 2:                                            // pfreq1
        case 3:                                            // pfreq2 
        case 4:                                            // pfreq3
        case 5:                                            // pfreq4
        case 6:                                            // pfreq5
          SetFreqPreset(i);
          CurrentMenu = 3;
          UpdateWindow();
          break;
        case 7:                                            // Return to Settings Menu
          printf("Settings Menu 3 requested\n");
          CurrentMenu=3;
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

    if (CurrentMenu == 9)  // Config Menu
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
        case 2:                                            // Freq Presets
          printf("Freq Presets Menu 10 Requested\n");
          CurrentMenu = 10;
          Start_Highlights_Menu10();
          UpdateWindow();
          break;
        case 3:                                            // 
          break;
        case 4:                                            // 
          break;
        case 5:                                            // 
          break;
        case 6:                                            // 
          break;
        case 7:                                            // Return to Main Menu
          printf("Main Menu 1 Requested\n");
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

    if (CurrentMenu == 10)  // Frequency Preset Setting Menu
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
        case 2:                                            // pfreq1
        case 3:                                            // pfreq2 
        case 4:                                            // pfreq3
        case 5:                                            // pfreq4
        case 6:                                            // pfreq5
          SetFreqPreset(i);
          CurrentMenu = 10;
          Start_Highlights_Menu10();
          UpdateWindow();
          break;
        case 7:                                            // Return to Main Menu
          printf("Settings Menu 1 requested\n");
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
          printf("Menu 10 Error\n");
      }
      continue;  // Completed Menu 10 action, go and wait for touch
    }
  }
  return NULL;
}

/////////////////////////////////////////////// DEFINE THE BUTTONS ///////////////////////////////

void Define_Menu1()                                  // Main Menu
{
  int button = 0;

  button = CreateButton(1, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(1, 1);
  AddButtonStatus(button, "Main Menu", &Black);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(1, 2);
  AddButtonStatus(button, "Freq and^Bandwidth", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(1, 3);
  AddButtonStatus(button, "Set-up", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(1, 4);
  AddButtonStatus(button, "Mode", &Blue);
  AddButtonStatus(button, "", &Green);

  button = CreateButton(1, 5);
  AddButtonStatus(button, "<-", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(1, 6);
  AddButtonStatus(button, "->", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(1, 7);
  AddButtonStatus(button, "System", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(1, 8);
  AddButtonStatus(button, "Exit to^Portsdown", &DBlue);
  AddButtonStatus(button, "Exit to^Portsdown", &Red);

  button = CreateButton(1, 9);
  AddButtonStatus(button, "Freeze", &Blue);
  AddButtonStatus(button, "Unfreeze", &Green);
}


void Start_Highlights_Menu1()
{
  if (PortsdownExitRequested)
  {
    SetButtonStatus(ButtonNumber(1, 8), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(1, 8), 0);
  }
}


void Define_Menu2()                                         // Set-up Menu
{
  int button = 0;

  button = CreateButton(2, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(2, 1);
  AddButtonStatus(button, "Set-up^Menu", &Black);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(2, 2);
  AddButtonStatus(button, "Set^ENR", &Blue);
  AddButtonStatus(button, "Set^Input ENR", &Blue);

  button = CreateButton(2, 3);
  AddButtonStatus(button, "Gain Up", &Blue);
  AddButtonStatus(button, "Gain Up", &Grey);

  button = CreateButton(2, 4);
  AddButtonStatus(button, "Gain", &Black);

  button = CreateButton(2, 5);
  AddButtonStatus(button, "Gain Down", &Blue);
  AddButtonStatus(button, "Gain Down", &Grey);

  button = CreateButton(2, 6);
  AddButtonStatus(button, "Calibrate", &Blue);
  AddButtonStatus(button, "Calibrating", &LBlue);
  AddButtonStatus(button, "Calibrated", &Green);

  button = CreateButton(2, 7);
  AddButtonStatus(button, "Return to^Main Menu", &DBlue);

  button = CreateButton(2, 8);
  AddButtonStatus(button, "Freeze", &Blue);
  AddButtonStatus(button, "Unfreeze", &Green);
}


void Start_Highlights_Menu2()
{
  char ButtText[31];
  snprintf(ButtText, 30, "Gain=%d%%", (plutogain + 20));
  AmendButtonStatus(ButtonNumber(CurrentMenu, 4), 0, ButtText, &Black);

  if (Calibrated == true)  // 
  {
    SetButtonStatus(ButtonNumber(2, 2), 1);
    SetButtonStatus(ButtonNumber(2, 3), 1);
    SetButtonStatus(ButtonNumber(2, 5), 1);
    SetButtonStatus(ButtonNumber(2, 6), 2);
  }
  else
  {
    SetButtonStatus(ButtonNumber(2, 2), 0);
    SetButtonStatus(ButtonNumber(2, 3), 0);
    SetButtonStatus(ButtonNumber(2, 5), 0);

    if (CalibrateRequested == true)
    {
      SetButtonStatus(ButtonNumber(2, 6), 1);
    }
    else
    {
      SetButtonStatus(ButtonNumber(2, 6), 0);
    }
  }
}


void Define_Menu3()                                           // Settings Menu
{
  int button = 0;

  button = CreateButton(3, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(3, 1);
  AddButtonStatus(button, "Settings^Menu", &Black);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(3, 2);
  AddButtonStatus(button, "Centre^Freq", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(3, 3);
  AddButtonStatus(button, "Freq^Presets", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(3, 4);
  AddButtonStatus(button, "Span^Width", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(3, 5);
  AddButtonStatus(button, "Set-up^Menu", &Blue);

  button = CreateButton(3, 6);
  AddButtonStatus(button, "Title", &Blue);

  button = CreateButton(3, 7);
  AddButtonStatus(button, "Return to^Main Menu", &DBlue);

  button = CreateButton(3, 8);
  AddButtonStatus(button, "Freeze", &Blue);
  AddButtonStatus(button, "Unfreeze", &Green);
}


void Define_Menu4()                                         // System Menu
{
  int button = 0;

  button = CreateButton(4, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(4, 1);
  AddButtonStatus(button, "System^Menu", &Black);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(4, 2);
  AddButtonStatus(button, "Snap^Viewer", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(4, 3);
  AddButtonStatus(button, "Re-start^NF Meter", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(4, 4);
  AddButtonStatus(button, "Exit to^Portsdown", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(4, 5);
  AddButtonStatus(button, "Shutdown^System", &Blue);
  AddButtonStatus(button, " ", &Green);

  //button = CreateButton(4, 6);
  //AddButtonStatus(button, "Re-cal^LimeSDR", &Blue);
  //AddButtonStatus(button, " ", &Green);

  button = CreateButton(4, 7);
  AddButtonStatus(button, "Return to^Main Menu", &DBlue);

  button = CreateButton(4, 8);
  AddButtonStatus(button, "Freeze", &Blue);
  AddButtonStatus(button, "Unfreeze", &Green);
}


void Define_Menu5()                                          // Mode Menu
{
  int button = 0;

  button = CreateButton(5, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(5, 1);
  AddButtonStatus(button, "Mode^Menu", &Black);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(5, 2);
  AddButtonStatus(button, "Spectrum^and NF", &Blue);

  button = CreateButton(5, 3);
  //AddButtonStatus(button, "NF^Meter", &Blue);
  AddButtonStatus(button, " ", &Blue);

  button = CreateButton(5, 4);
  //AddButtonStatus(button, "Gain^Meter", &Blue);
  AddButtonStatus(button, " ", &Blue);

  button = CreateButton(5, 5);
  AddButtonStatus(button, " ", &Blue);

  button = CreateButton(5, 6);
  AddButtonStatus(button, "Config^Menu", &Blue);

  button = CreateButton(5, 7);
  AddButtonStatus(button, "Return to^Main Menu", &DBlue);

  button = CreateButton(5, 8);
  AddButtonStatus(button, "Freeze", &Blue);
  AddButtonStatus(button, "Unfreeze", &Green);
}


void Define_Menu6()                                           // Span Menu
{
  int button = 0;

  button = CreateButton(6, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(6, 1);
  AddButtonStatus(button, "Span^Menu", &Black);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(6, 2);
  AddButtonStatus(button, "2 MHz", &Blue);
  AddButtonStatus(button, "2 MHz", &Green);

  button = CreateButton(6, 3);
  AddButtonStatus(button, "5 MHz", &Blue);
  AddButtonStatus(button, "5 MHz", &Green);

  button = CreateButton(6, 4);
  AddButtonStatus(button, "10 MHz", &Blue);
  AddButtonStatus(button, "10 MHz", &Green);

  button = CreateButton(6, 5);
  AddButtonStatus(button, "20 MHz", &Blue);
  AddButtonStatus(button, "20 MHz", &Green);

  button = CreateButton(6, 8);
  AddButtonStatus(button, "Back to^Settings", &DBlue);

  button = CreateButton(6, 9);
  AddButtonStatus(button, "Freeze", &Blue);
  AddButtonStatus(button, "Unfreeze", &Green);
}


void Start_Highlights_Menu6()
{
  if (span == 2084)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 2), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 2), 0);
  }
  if (span == 5120)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0);
  }
  if (span == 10240)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 4), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 4), 0);
  }
  if (span == 20480)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 5), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 5), 0);
  }
}


void Define_Menu7()                                            //Presets Menu
{
  int button = 0;

  button = CreateButton(7, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(7, 1);
  AddButtonStatus(button, "Presets^Menu", &Black);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(7, 2);
  AddButtonStatus(button, "146.5 MHz", &Blue);
  AddButtonStatus(button, "146.5 MHz", &Green);

  button = CreateButton(7, 3);
  AddButtonStatus(button, "437 MHz", &Blue);
  AddButtonStatus(button, "437 MHz", &Green);

  button = CreateButton(7, 4);
  AddButtonStatus(button, "748 MHz", &Blue);
  AddButtonStatus(button, "748 MHz", &Green);

  button = CreateButton(7, 5);
  AddButtonStatus(button, "1255 MHz", &Blue);
  AddButtonStatus(button, "1255 MHz", &Green);

  button = CreateButton(7, 6);
  AddButtonStatus(button, "2409 MHz", &Blue);
  AddButtonStatus(button, "2409 MHz", &Green);

  button = CreateButton(7, 7);
  AddButtonStatus(button, "Back to^Settings", &DBlue);

  button = CreateButton(7, 8);
  AddButtonStatus(button, "Freeze", &Blue);
  AddButtonStatus(button, "Unfreeze", &Green);
}


void Start_Highlights_Menu7()
{
  char ButtText[15];
  snprintf(ButtText, 14, "%0.1f MHz", ((float)pfreq1) / 1000);
  AmendButtonStatus(ButtonNumber(CurrentMenu, 2), 0, ButtText, &Blue);
  AmendButtonStatus(ButtonNumber(CurrentMenu, 2), 1, ButtText, &Green);

  snprintf(ButtText, 14, "%0.1f MHz", ((float)pfreq2) / 1000);
  AmendButtonStatus(ButtonNumber(CurrentMenu, 3), 0, ButtText, &Blue);
  AmendButtonStatus(ButtonNumber(CurrentMenu, 3), 1, ButtText, &Green);

  snprintf(ButtText, 14, "%0.1f MHz", ((float)pfreq3) / 1000);
  AmendButtonStatus(ButtonNumber(CurrentMenu, 4), 0, ButtText, &Blue);
  AmendButtonStatus(ButtonNumber(CurrentMenu, 4), 1, ButtText, &Green);

  snprintf(ButtText, 14, "%0.1f MHz", ((float)pfreq4) / 1000);
  AmendButtonStatus(ButtonNumber(CurrentMenu, 5), 0, ButtText, &Blue);
  AmendButtonStatus(ButtonNumber(CurrentMenu, 5), 1, ButtText, &Green);

  snprintf(ButtText, 14, "%0.1f MHz", ((float)pfreq5) / 1000);
  AmendButtonStatus(ButtonNumber(CurrentMenu, 6), 0, ButtText, &Blue);
  AmendButtonStatus(ButtonNumber(CurrentMenu, 6), 1, ButtText, &Green);

  if (centrefreq == pfreq1)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 2), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 2), 0);
  }
  if (centrefreq == pfreq2)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0);
  }
  if (centrefreq == pfreq3)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 4), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 4), 0);
  }
  if (centrefreq == pfreq4)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 5), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 5), 0);
  }
  if (centrefreq == pfreq5)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0);
  }
}


void Define_Menu9()                                          // Config Menu
{
  int button = 0;

  button = CreateButton(9, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(9, 1);
  AddButtonStatus(button, "Config^Menu", &Black);

  button = CreateButton(9, 2);
  AddButtonStatus(button, "Set Freq^Presets", &Blue);

  //button = CreateButton(9, 3);
  //AddButtonStatus(button, "Y Plot", &Blue);
  //AddButtonStatus(button, " ", &Green);

  //button = CreateButton(9, 4);
  //AddButtonStatus(button, " ", &Blue);
  //AddButtonStatus(button, " ", &Green);

  //button = CreateButton(9, 5);
  //AddButtonStatus(button, " ", &Blue);
  //AddButtonStatus(button, " ", &Green);

  //button = CreateButton(9 6);
  //AddButtonStatus(button, "Set^Config", &Blue);

  button = CreateButton(9, 7);
  AddButtonStatus(button, "Return to^Main Menu", &DBlue);

  button = CreateButton(9, 8);
  AddButtonStatus(button, "Freeze", &Blue);
  AddButtonStatus(button, "Unfreeze", &Green);
}


void Define_Menu10()                                          // Set Freq Presets Menu
{
  int button = 0;

  button = CreateButton(10, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(10, 1);
  AddButtonStatus(button, "Set Freq^Presets", &Black);

  button = CreateButton(10, 2);
  AddButtonStatus(button, "Preset 1", &Blue);

  button = CreateButton(10, 3);
  AddButtonStatus(button, "Preset 2", &Blue);

  button = CreateButton(10, 4);
  AddButtonStatus(button, "Preset 3", &Blue);

  button = CreateButton(10, 5);
  AddButtonStatus(button, "Preset 4", &Blue);

  button = CreateButton(10, 6);
  AddButtonStatus(button, "Preset 5", &Blue);

  button = CreateButton(10, 7);
  AddButtonStatus(button, "Return to^Main Menu", &DBlue);

  button = CreateButton(10, 8);
  AddButtonStatus(button, "Freeze", &Blue);
  AddButtonStatus(button, "Unfreeze", &Green);
}


void Start_Highlights_Menu10()
{
  char ButtText[31];
  snprintf(ButtText, 30, "Preset 1^%0.1f MHz", ((float)pfreq1) / 1000);
  AmendButtonStatus(ButtonNumber(CurrentMenu, 2), 0, ButtText, &Blue);

  snprintf(ButtText, 30, "Preset 2^%0.1f MHz", ((float)pfreq2) / 1000);
  AmendButtonStatus(ButtonNumber(CurrentMenu, 3), 0, ButtText, &Blue);

  snprintf(ButtText, 30, "Preset 3^%0.1f MHz", ((float)pfreq3) / 1000);
  AmendButtonStatus(ButtonNumber(CurrentMenu, 4), 0, ButtText, &Blue);

  snprintf(ButtText, 30, "Preset 4^%0.1f MHz", ((float)pfreq4) / 1000);
  AmendButtonStatus(ButtonNumber(CurrentMenu, 5), 0, ButtText, &Blue);

  snprintf(ButtText, 30, "Preset 5^%0.1f MHz", ((float)pfreq5) / 1000);
  AmendButtonStatus(ButtonNumber(CurrentMenu, 6), 0, ButtText, &Blue);
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
    setBackColour(0, 0, 0);
    clearScreen();
    HorizLine(100, 270, 250, 255, 255, 255);
    VertLine(100, 270, 200, 255, 255, 255);
    HorizLine(100, 470, 250, 255, 255, 255);
    VertLine(350, 270, 200, 255, 255, 255);

    HorizLine(101, 320, 249, 63, 63, 63);
    HorizLine(101, 370, 249, 63, 63, 63);
    HorizLine(101, 420, 249, 63, 63, 63);

    VertLine(150, 271, 199, 63, 63, 63);
    VertLine(200, 271, 199, 63, 63, 63);
    VertLine(250, 271, 199, 63, 63, 63);
    VertLine(300, 271, 199, 63, 63, 63);
}


void DrawYaxisLabels()
{
  setForeColour(255, 255, 255);                    // White text
  setBackColour(0, 0, 0);                          // on Black
  const font_t *font_ptr = &font_dejavu_sans_18;   // 18pt

  // Clear the previous scale first
  rectangle(25, 263, 65, 217, 0, 0, 0);

  pthread_mutex_lock(&text_lock);

    Text2(48, 463, "0 dB", font_ptr);
    Text2(30, 416, "-20 dB", font_ptr);
    Text2(30, 366, "-40 dB", font_ptr);
    Text2(30, 316, "-60 dB", font_ptr);
    Text2(30, 266, "-80 dB", font_ptr);

  pthread_mutex_unlock(&text_lock);
}


void DrawSettings()
{
  setForeColour(255, 255, 255);                    // White text
  setBackColour(0, 0, 0);                          // on Black
  const font_t *font_ptr = &font_dejavu_sans_18;   // 18pt
  char DisplayText[63];
  float ParamAsFloat;
  int line1y = 245;
  int line2y = 215;
  int titley = 5;

  // Clear the previous text first
  rectangle(100, 0, 505, 30, 0, 0, 0);
  rectangle(100, line1y - 5, 250, 25, 0, 0, 0);
  rectangle(100, line2y - 5, 250, 25, 0, 0, 0);

  ParamAsFloat = (float)centrefreq / 1000.0;
  snprintf(DisplayText, 63, "Centre Freq %5.2f MHz", ParamAsFloat);
  pthread_mutex_lock(&text_lock);
  Text2(100, line1y, DisplayText, font_ptr);
  pthread_mutex_unlock(&text_lock);

  //nf_bandwidth

  if ((nf_bandwidth > 0) && (nf_bandwidth < 1000))  // valid and less than 1000 kHz
  {
    ParamAsFloat = (float)nf_bandwidth;
    snprintf(DisplayText, 63, "Bandwidth %5.1f kHz", ParamAsFloat);
    pthread_mutex_lock(&text_lock);
    Text2(100, line2y, DisplayText, font_ptr);
    pthread_mutex_unlock(&text_lock);
  }
  else if (nf_bandwidth >= 1000)                  // valid and greater 1000 kHz
  {
    ParamAsFloat = (float)nf_bandwidth / 1000.0;
    snprintf(DisplayText, 63, "Bandwidth %5.1f MHz", ParamAsFloat);
    pthread_mutex_lock(&text_lock);
    Text2(100, line2y, DisplayText, font_ptr);
    pthread_mutex_unlock(&text_lock);
  }

  if (strcmp(PlotTitle, "-") != 0)
  {
    pthread_mutex_lock(&text_lock);
    TextMid2(350, titley, PlotTitle, &font_dejavu_sans_22);
    pthread_mutex_unlock(&text_lock);
  }
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
    for(ypos = 201; ypos < ymin; ypos++)
    {
      setPixelNoAGra(xpos, 479 - (70 + ypos));
    }
    for(ypos = 399; ypos > ymax; ypos--)
    {
      setPixelNoAGra(xpos, 479 - (70 + ypos));
    }
  }
  else  // horizontal graticule and open space
  {
    for(ypos = 201; ypos < ymin; ypos++)  // below the trace
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
    }
  }
}


void DrawMeterArc()
{
  // Draws an anti-aliased meter arc

  float x;
  float y;
//  int previous_x = 129; // Left most position of arc - 1
  int previous_x = 364; // Left most position of arc - 1
  float current_sin;
  float current_cos;
  float arc_angle;
  int arc_deflection;
  float contrast;

  for (arc_deflection = 0; arc_deflection < 998; arc_deflection++)
  {
    arc_angle = (float)(arc_deflection) * 2 * PI / 4000.0 - PI / 4.0;
    current_sin = sin(arc_angle);
    current_cos = cos(arc_angle);


//    x = 350 + 312 * current_sin;
//    y = 90 + 312 * current_cos;
    x = 475 + 156 * current_sin;
    y = 290 + 156 * current_cos;

    if ((x - (float)previous_x) >= 1.0)  // new pixel pair required
    {
      // Work out contrast for lower pixel and paint
      contrast = (y - (int)y) * 255;
      setPixel(previous_x + 1, hscreen - (int)y, contrast, contrast, contrast);

      // work out contrast for upper pixel and paint
      contrast = (1 - (y - (int)y)) * 255;
      setPixel(previous_x + 1, hscreen - ((int)y - 1), contrast, contrast, contrast);

      previous_x = previous_x + 1;
    }
  }
}


void DrawMeterTicks(int major_ticks, int minor_ticks)
{
  float tick_deflection= 0;
  int x1;
  int x2;
  int y1;
  int y2;
  float current_sin;
  float current_cos;
  int major_tick_number;
  int minor_tick_number;

//  float major_tick_length = 20.0;
//  float minor_tick_length = 10.0;
  float major_tick_length = 10.0;
  float minor_tick_length = 5.0;

  // Draw the major ticks.  Start at zero
  for (major_tick_number = 0; major_tick_number <= major_ticks; major_tick_number++)
  {
    tick_deflection = (major_tick_number * 1000) / major_ticks;  // for majors

    current_sin = sin((float)tick_deflection * 2.0 * PI / 4000.0 - PI / 4.0);
    current_cos = cos((float)tick_deflection * 2.0 * PI / 4000.0 - PI / 4.0);

//    x1 = (int)(350.0 + 312.0 * current_sin);
//    y1 = (int)(90.0 + 312.0 * current_cos);
//    x2 = (int)(350.0 + (312.0 + major_tick_length) * current_sin);
//    y2 = (int)(90.0 + (312.0 + major_tick_length) * current_cos);
    x1 = (int)(475.0 + 156.0 * current_sin);
    y1 = (int)(290.0 + 156.0 * current_cos);
    x2 = (int)(475.0 + (156.0 + major_tick_length) * current_sin);
    y2 = (int)(290.0 + (156.0 + major_tick_length) * current_cos);

    DrawAALine(x1, y1, x2, y2, 0, 0, 0, 255, 255, 255);

    // Draw the minor ticks.  Start at one.  Don't draw for last major tick
    if (major_tick_number < major_ticks)
    {
      for (minor_tick_number = 1; minor_tick_number <= minor_ticks; minor_tick_number++)
      {
        tick_deflection = tick_deflection + (minor_tick_number * 1000) / major_ticks / (minor_ticks + 1);

        current_sin = sin((float)tick_deflection * 2.0 * PI / 4000.0 - PI / 4.0);
        current_cos = cos((float)tick_deflection * 2.0 * PI / 4000.0 - PI / 4.0);

//        x1 = (int)(350.0 + 312.0 * current_sin);
//        y1 = (int)(90.0 + 312.0 * current_cos);
//        x2 = (int)(350.0 + (312.0 + minor_tick_length) * current_sin);
//        y2 = (int)(90.0 + (312.0 + minor_tick_length) * current_cos);
        x1 = (int)(475.0 + 156.0 * current_sin);
        y1 = (int)(290.0 + 156.0 * current_cos);
        x2 = (int)(475.0 + (156.0 + minor_tick_length) * current_sin);
        y2 = (int)(290.0 + (156.0 + minor_tick_length) * current_cos);

        DrawAALine(x1, y1, x2, y2, 0, 0, 0, 255, 255, 255);
      }
    }
  }
}


void Draw5MeterLabels(float LH_Value, float RH_Value)
{
  char labeltext[15];
  const font_t *font_ptr = &font_dejavu_sans_18;   // 18pt
  setBackColour(0, 0, 0);

  rectangle(351, 432, 40, 30, 0, 0, 0);
  rectangle(378, 454, 40, 25, 0, 0, 0);
  rectangle(430, 463, 40, 15, 0, 0, 0);
  rectangle(482, 463, 40, 15, 0, 0, 0);
  rectangle(532, 454, 40, 25, 0, 0, 0);
  rectangle(575, 432, 40, 30, 0, 0, 0);

  snprintf(labeltext, 14, "%d", (int)LH_Value);
  pthread_mutex_lock(&text_lock);
  TextMid2(366, 432, labeltext, font_ptr);
  pthread_mutex_unlock(&text_lock);

  snprintf(labeltext, 14, "%d", (int)(LH_Value + (RH_Value -LH_Value) / 5));
  pthread_mutex_lock(&text_lock);
  TextMid2(398, 454, labeltext, font_ptr);
  pthread_mutex_unlock(&text_lock);

  snprintf(labeltext, 14, "%d", (int)(LH_Value + 2 * (RH_Value -LH_Value) / 5));
  pthread_mutex_lock(&text_lock);
  TextMid2(448, 463, labeltext, font_ptr);
  pthread_mutex_unlock(&text_lock);

  snprintf(labeltext, 14, "%d", (int)(LH_Value + 3 * (RH_Value -LH_Value) / 5));
  pthread_mutex_lock(&text_lock);
  TextMid2(500, 463, labeltext, font_ptr);
  pthread_mutex_unlock(&text_lock);

  snprintf(labeltext, 14, "%d", (int)(LH_Value + 4 * (RH_Value -LH_Value) / 5));
  pthread_mutex_lock(&text_lock);
  TextMid2(550, 454, labeltext, font_ptr);
  pthread_mutex_unlock(&text_lock);

  snprintf(labeltext, 14, "%d", (int)RH_Value);
  pthread_mutex_lock(&text_lock);
  TextMid2(593, 432, labeltext, font_ptr);
  pthread_mutex_unlock(&text_lock);
}


void *MeterMovement(void * arg)
{
  bool *exit_requested = (bool *)arg;
  int current_meter_deflection = 0;
  int meter_move;
  float meter_angle;
  float current_sin;
  float current_cos;
  float previous_sin;
  float previous_cos;

  while((false == *exit_requested))
  {
    while (freeze)  // Do not run if display is frozen
    {
      usleep(20000);
    }

    // Check Meter deflection is within bounds
    if (meter_deflection > 1050)
    {
      meter_deflection = 1050;
    }
    if (meter_deflection < -50)
    {
      meter_deflection = -50;
    }

    if (meter_deflection != current_meter_deflection )        
    {
      // Physics
      meter_move = (meter_deflection - current_meter_deflection) / 5;  // Reduce movement speed
      if ((abs(meter_move) < 200) && (abs(meter_move) > 50))           // Slow as nearing position
      {
         meter_move = meter_move / 2;
      }

      if (meter_move > 200)                                            // Limit max speed
      {
        meter_move = 200;
      }

      if ((meter_deflection - current_meter_deflection) != 0)                                     // Only draw if needed
      {
        if (abs(meter_deflection - current_meter_deflection) > 5)                                 // Large move
        {
          meter_angle = (float)(current_meter_deflection + meter_move) * 2.0 * PI / 4000.0 - PI / 4.0;
          current_meter_deflection = current_meter_deflection + meter_move;
        }
        else                                                                                      // last few degrees
        {
          meter_angle = (float)(meter_deflection) * 2.0 * PI / 4000.0 - PI / 4.0;
          current_meter_deflection = meter_deflection;
        }
        current_sin = sin(meter_angle);
        current_cos = cos(meter_angle);

        // Overwrite previous position
        DrawAALine(475, 290, 475 + (int)(153.0 * previous_sin), 290 + (int)(153.0 * previous_cos), 0, 0, 0, 0, 0, 0);

        // Write new position
        DrawAALine(475, 290, 475 + (int)(153.0 * current_sin), 290 + (int)(153.0 * current_cos), 0, 0, 0, 255, 255, 255);

        // Set up for overwrite next time
        previous_sin = current_sin;
        previous_cos = current_cos;
      }
    }
    usleep(20000);  // 50 Hz refresh rate
  }
  return NULL;
}


void CalibrateSystem()
{
  CalibrateRequested = true;
}


static void cleanexit(int calling_exit_code)
{
  exit_code = calling_exit_code;
  app_exit = true;
  printf("Clean Exit Code %d\n", exit_code);
  usleep(1000000);
  char Commnd[255];
  sprintf(Commnd,"stty echo");
  system(Commnd);
  printf("scans = %d\n", tracecount);
  exit(exit_code);
}


static void terminate(int sig)
{
  app_exit = true;
  printf("Terminating\n");
  usleep(1000000);
  char Commnd[255];
  sprintf(Commnd,"stty echo");
  system(Commnd);
  // Turn off Noise Source
  digitalWrite(NoiseSourceGPIO, LOW);
  printf("scans = %d\n", tracecount);
  exit(0);
}


int main(void)
{
  int NoDeviceEvent=0;
  wscreen = 800;
  hscreen = 480;
  int screenXmax, screenXmin;
  int screenYmax, screenYmin;
  int i;
  int pixel;
  float NF = 0;
  float PNF = 0;
  float pwrNF = 0;
  float pwrPNF = 0;
  float Avlpwrcold = -80;
  float Avlpwrhot = -80;
  int nextwebupdate = 10;
  char response[63];

  int NFScans = 0;
  int NFTotalCold = 0;
  int NFTotalHot = 0;
  float pwrTotalHot = 0;
  float pwrTotalCold = 0;
  float lpwrhot;
  float lpwrcold;
  int SampleCount = 0;
  float NoiseDiff;
  float Y2;
  float pwrY2;
  float T2;
  float pwrT2;
  char NFText[31];
  int CalibrateSmoothCount = 0;

  float GDevice = 0;
  float CalpwrTotalHot = 0;
  float CalpwrTotalCold = 0;
  float GDevicedB = 0;
  float NFDevice = 0;
  float NFsys = 0;
  float CalNFsys = 0;
  float NFDevicedB = 0;
  float SmoothGDevicedB = 0;
  float SmoothNFDevicedB = 0;
  float meterNF = 30.0;
  float meterG = 0.0;

  // Catch sigaction and call terminate
  for (i = 0; i < 16; i++)
  {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = terminate;
    sigaction(i, &sa, NULL);
  }

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
  if(NoDeviceEvent != 7)  // Touchscreen detected
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
  Define_Menu9();
  Define_Menu10();
  Define_Menu41();

  // Set up wiringPi module
  if (wiringPiSetup() < 0)
  {
    return 0;
  }

  // Set all nominated pins to outputs
  pinMode(NoiseSourceGPIO, OUTPUT);

  // Set idle conditions
  digitalWrite(NoiseSourceGPIO, LOW);

  ReadSavedParams();

  // Create Wait Button thread
  pthread_create (&thbutton, NULL, &WaitButtonEvent, NULL);

  // Initialise screen and splash

  CurrentMenu = 2;
  Start_Highlights_Menu2();

  screen_init();

  initScreen();

  UpdateWindow();

  MsgBox4("Starting the Band Viewer", "Profiling FFTs on first use", "Please wait 80 seconds", "No delay next time");

  printf("Profiling FFTs..\n");
  fftwf_import_wisdom_from_filename("/home/pi/.fftwf_wisdom");
  printf(" - Main Band FFT\n");
  main_fft_init();
  fftwf_export_wisdom_to_filename("/home/pi/.fftwf_wisdom");
  printf("FFTs Done.\n");

  // Check that Pluto SDR is detected
  PlutoConnectTest();

  /* Pluto SDR Thread */
  if(pthread_create(&sdr_thread_obj, NULL, sdr_thread, &app_exit))
  {
    fprintf(stderr, "Error creating %s pthread\n", "SDR");
    return 1;
  }
  pthread_setname_np(sdr_thread_obj, "sdr");

  printf("Starting FFT Thread\n");

  /* Band FFT Thread */
  if(pthread_create(&fft_thread_obj, NULL, fft_thread, &app_exit))
  {
    fprintf(stderr, "Error creating %s pthread\n", "FFT");
    return 1;
  }
  pthread_setname_np(fft_thread_obj, "FFT");

  // Zero the y value buffer
  for(i = 1; i < 255; i++)
  {
    y[i] = 201;
  }

  DrawEmptyScreen();  // Required to set A value, which is not set in DrawTrace

  DrawYaxisLabels();  // dB calibration on LHS

  DrawSettings();     // Start, Stop RBW, Ref level and Title

  UpdateWindow();     // Draw the buttons

  Tson = Tsoff * (1 + pow(10, (ENR / 10)));
  //printf("ENR = %f dB.  Tson = %f degrees K\n", ENR, Tson);


  // Start the meter movement
  if(pthread_create(&thMeter_Movement, NULL, &MeterMovement, &app_exit))
  {
    fprintf(stderr, "Error creating %s pthread\n", "MeterMovement");
    return 1;
  }

  while(true)                                       // Continuous loop for running
  {
    // Process noise figure calcs and source switching
    NFScans++;
    if (NFScans <= (2 * ScansforLevel))  // NFScans = 1 ( Source on, Settling time) or
                                         // NFScans = 2 (Source on, Measuring time)
    {
      // Turn on
      digitalWrite(NoiseSourceGPIO, HIGH);
    }
    else                                 // NFScans = 3 ( Source off, Settling time) or
                                         // NFScans = 4 (Source off, Measuring time)
    {
      // Turn off
      digitalWrite(NoiseSourceGPIO, LOW);
    }

    if (NFScans >= (4 * ScansforLevel))  // End of Source off measuring time, so calculate
    {
      NFScans = 0;

      // Draw Meter Surround
      
      if (ModeChanged == true)
      {
        setBackColour(0, 0, 0);
        setForeColour(255, 255, 255);

        DrawMeterArc();
        DrawMeterTicks(5, 1);
        Draw5MeterLabels(ActiveZero, ActiveFSD);
        
        ModeChanged = false;
      }


      // Calculate Noise difference in dB.  Totals are 4 dB per div
      NoiseDiff = (float)(NFTotalHot - NFTotalCold) * 0.4 / (float)SampleCount;  //  NoiseDiff 4 div/dB and 10 samples averaged
      // Noise Diff is Y expressed in dB as derived from spectrum display

      Y2 = pow(10, (NoiseDiff / 10));  // Y2 is a pure ratio (= N2/N1)

      T2 = (Tson - Y2 * Tsoff)/(Y2 - 1);
      NFsys = 1 + (T2 / 290);
      NF = (10 * log10(NFsys)) * 0.1 + (PNF * 0.9);
      if (NF < 50)
      {
        PNF = NF;
      }

      //printf("%d Samples. Hot Average = %d, Cold Average = %d, Noise Diff = %f\n",
      //  SampleCount, (2 * NFTotalHot) / SampleCount, (2 * NFTotalCold) / SampleCount, NoiseDiff);


      lpwrhot = 10.f * log10((0.5 * pwrTotalHot / SampleCount) + 1.0e-20);
      lpwrcold = 10.f * log10((0.5 * pwrTotalCold / SampleCount) + 1.0e-20);

      pwrY2 = pwrTotalHot / pwrTotalCold;
      pwrT2 = (Tson - pwrY2 * Tsoff)/(pwrY2 - 1);
      pwrNF = (10 * log10(1 + (pwrT2 / 290))) * 0.1 + (pwrPNF * 0.9);
      if (pwrNF < 50)
      {
        pwrPNF = pwrNF;
      }

      Avlpwrhot = lpwrhot * 0.1 + Avlpwrhot * 0.9;
      Avlpwrcold = lpwrcold * 0.1 + Avlpwrcold * 0.9;

      //printf("%d Samples. Hot dB Power Average = %f, Cold dB Power Average = %f, Pwr noise diff = %f\n",
      //  SampleCount, Avlpwrhot, Avlpwrcold, pwrNoiseDiff);

      meterNF = pwrNF;

      // Display Lower left quadrant (detailed parameters or large numbers)
      if (LargeNumbers == false)   // display detailed parameters
      {
        // Clear the screen area if required
        if (LargeNumbersDisplayed == true)
        {
          rectangle(100, 55, 200, 185, 0, 0, 0);  // Blank area
          DrawSettings();                         // Redraw Bandwidth
          LargeNumbersDisplayed = false;
        }

        // Display On Power Level
        rectangle(100, 120, 250, 22, 0, 0, 0);  // Blank area
        snprintf(NFText, 20, "Noise on %0.1f dB", Avlpwrhot + 20.0);
        setBackColour(0, 0, 0);
        pthread_mutex_lock(&text_lock);
        Text2(100, 125, NFText, &font_dejavu_sans_18);
        pthread_mutex_unlock(&text_lock);

        // Display Off Power Level
        rectangle(100, 90, 250, 22, 0, 0, 0);  // Blank area
        snprintf(NFText, 20, "Noise off %0.1f dB", Avlpwrcold + 20.0);
        setBackColour(0, 0, 0);
        pthread_mutex_lock(&text_lock);
        if (Avlpwrcold + 20.0 < -52.0)
        {
          setForeColour(255, 127, 127);
        }
        Text2(100, 95, NFText, &font_dejavu_sans_18);
        setForeColour(255, 255, 255);
        pthread_mutex_unlock(&text_lock);
        rectangle(300, 90, 250, 22, 0, 0, 0);  // Blank area
        if (Avlpwrcold + 20.0 < -52.0)
        {
          pthread_mutex_lock(&text_lock);
          Text2(300, 95, "Increase Pluto Gain", &font_dejavu_sans_18);
          pthread_mutex_unlock(&text_lock);
        }
      }

      if (CalibrateRequested)
      {
        if (CalibrateSmoothCount == 0)
        {
          // Initialise calibrate variables with first value
          CalpwrTotalHot = pwrTotalHot;
          CalpwrTotalCold = pwrTotalCold;
          CalNFsys = NFsys;
        }
        if ((CalibrateSmoothCount > 0) &&  (CalibrateSmoothCount <= 19))
        {
          // Add values to average
          CalpwrTotalHot = CalpwrTotalHot + pwrTotalHot;
          CalpwrTotalCold = CalpwrTotalCold + pwrTotalCold;
          CalNFsys = CalNFsys + NFsys;     
        }
        if (CalibrateSmoothCount >= 19)
        {
          // Calculate constants for device cal

          CalpwrTotalHot =  CalpwrTotalHot / 20.0;
          CalpwrTotalCold =  CalpwrTotalCold / 20.0;
          CalNFsys = CalNFsys / 20.0;

          Calibrated = true;
          CalibrateRequested = false;
          CalibrateSmoothCount = -1;
          Start_Highlights_Menu2();
          UpdateWindow();
        }
        CalibrateSmoothCount++;
      }

      if (Calibrated)
      {
        // Calculate and display device NF and gain

        // Gain

        GDevice = (pwrTotalHot - pwrTotalCold) / (CalpwrTotalHot - CalpwrTotalCold);
        GDevicedB = 10 * log10(GDevice);

        // NF

        NFDevice = NFsys - (CalNFsys -1) / GDevice;
        NFDevicedB = 10 * log10(NFDevice);

        //  Add smoothing for displayed parameters in here?

        // Constrain Parameters before smoothing

        if (GDevicedB != GDevicedB)  // Test for NAN
        {
          GDevicedB = 0.0;
          //printf("NaN\n");
        }
        if (GDevicedB > 100.0)
        {
          GDevicedB = 50.0;
        }
        if (GDevicedB < -50.0)
        {
          GDevicedB = -49.9;
        }
        if (NFDevicedB != NFDevicedB)  // Test for NAN
        {
          NFDevicedB = 0.0;
          //printf("NaN\n");
        }

        if (NFDevicedB > 30.0)
        {
          NFDevicedB = 30.0;
        }
        if (NFDevicedB < -5.0)
        {
          NFDevicedB = -4.9;
        }

        SmoothGDevicedB = GDevicedB * 0.1 + SmoothGDevicedB * 0.9;
        SmoothNFDevicedB = NFDevicedB * 0.1 + SmoothNFDevicedB * 0.9;

        meterG = SmoothGDevicedB;
        meterNF = SmoothNFDevicedB;

        // Apply Transverter ENR if required
        if ((Calibrated == true) && (ENR != RF_ENR))
        {
          meterNF = meterNF - ENR + RF_ENR;
          meterG = meterG + ENR - RF_ENR;
        }

        //printf("NFDevicedB = %0.1f dB, SmoothNFDevicedB = %0.1f dB\n", NFDevicedB, SmoothNFDevicedB);

      }

      if (meterNF != meterNF)
      {
        meterNF = 30.0;
      }

      switch (MeterScale)
      {
      case 1:                   // 30 - 0
        meter_deflection = (int)(1000 - meterNF * 1000 / 30);
      break;
      case 2:                   // 30 - 20
        meter_deflection = (int)(1000 - meterNF * 1000 / 10) + 2000;
      break;
      case 3:                   // 20 - 10
        meter_deflection = (int)(1000 - meterNF * 1000 / 10) + 1000;
      break;
      case 4:                   // 10 - 0
        meter_deflection = (int)(1000 - meterNF * 1000 / 10);
      break;
      case 5:                   // 5 - 0
        meter_deflection = (int)(1000 - meterNF * 1000 / 5);
      break;
      }

      if (LargeNumbers == true)                                       // Clear lower left quadrant and Display large numbers
      {
        snprintf(NFText, 20, "%0.1f", meterNF);
        rectangle(100, 55, 200, 185, 0, 0, 0);  // Blank area
        pthread_mutex_lock(&text_lock);
        Text2(100, 110, NFText, &font_dejavu_sans_72);
        pthread_mutex_unlock(&text_lock);
        LargeNumbersDisplayed = true;
      }
      
      if(Calibrated == false)
      {
        snprintf(NFText, 20, "System NF %0.1f dB", meterNF);
      }
      else
      {
        if (ENR != RF_ENR)
        {
          snprintf(NFText, 20, "Tvtr NF %0.1f dB", meterNF);
        }
        else
        {
          snprintf(NFText, 20, "Device NF %0.1f dB", meterNF);
        }
      }

      rectangle(360, 250, 280, 40, 0, 0, 0);  // Blank area
      setBackColour(0, 0, 0);
      pthread_mutex_lock(&text_lock);
      Text2(360, 260, NFText, &font_dejavu_sans_28);
      pthread_mutex_unlock(&text_lock);

      rectangle(330, 190, 300, 40, 0, 0, 0);  // Blank area
      if(Calibrated == true)
      {
        if (ENR != RF_ENR)
        {
          snprintf(NFText, 20, "Tvtr gain %0.1f dB", meterG);
        }
        else
        {
          snprintf(NFText, 20, "Device gain %0.1f dB", meterG);
        }
        setBackColour(0, 0, 0);
        pthread_mutex_lock(&text_lock);
        Text2(330, 200, NFText, &font_dejavu_sans_28);
        pthread_mutex_unlock(&text_lock);
      }

      // printf("Noise Diff = %0.2f  Y2 =  %0.2f.  T2 = %0.1f, NF = %0.2f\n", NoiseDiff, Y2, T2, NF);
      NFTotalHot = 0;
      NFTotalCold = 0;
      pwrTotalHot = 0;
      pwrTotalCold = 0;
      SampleCount = 0;
    }

    for (pixel = 4; pixel < 253; pixel++)
    {
      DrawTrace((pixel - 2), y[pixel - 2], y[pixel - 1], y[pixel]);
      //printf("pixel=%d, prev2=%d, prev1=%d, current=%d\n", pixel, y[pixel - 2], y[pixel - 1], y[pixel]);

      if (((pixel > 3) && (pixel < 122)) || ((pixel > 133) && (pixel < 253)))
      {
        if ((NFScans >= ScansforLevel) && (NFScans <= (2 * ScansforLevel)))
        {
          NFTotalHot = NFTotalHot + 2 * (y[pixel - 1] - 200);
          pwrTotalHot = pwrTotalHot + rawpwr[pixel - 1];
          SampleCount++;
        }
        if (((NFScans >= (3 * ScansforLevel)) && (NFScans <= (4 * ScansforLevel - 1))) || (NFScans == 0))
        {
          NFTotalCold = NFTotalCold + 2 * (y[pixel - 1] - 200);
          pwrTotalCold = pwrTotalCold + rawpwr[pixel - 1];
          SampleCount++;
        }
      }

      while (freeze)
      {
        frozen = true;
      }
      frozen = false;
    }
    tracecount++;
    if (tracecount >= nextwebupdate)
    {
      // printf("tracecount = %d, Time ms = %llu \n", tracecount, monotonic_ms());
      UpdateWeb();
      usleep(10000);
      nextwebupdate = tracecount + 200;  // About 800 ms between updates
    }
    //usleep(1000);
  }

  printf("Waiting for Lime Thread to exit..\n");
  pthread_join(sdr_thread_obj, NULL);
  printf("Waiting for FFT Thread to exit..\n");
  pthread_join(fft_thread_obj, NULL);
  printf("Waiting for Screen Thread to exit..\n");
  pthread_join(screen_thread_obj, NULL);
  printf("Waiting for Meter Thread to exit..\n");
  pthread_join(thMeter_Movement, NULL);
  printf("All threads caught, exiting..\n");
  pthread_join(thbutton, NULL);
  pthread_mutex_destroy(&text_lock);
}

