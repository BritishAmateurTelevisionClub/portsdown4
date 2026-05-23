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

//#include <wiringPi.h>
#include <signal.h>
#include <dirent.h>
#include <pthread.h>
#include <ctype.h>
#include <stdbool.h>

#include "font/font.h"
#include "touch.h"
#include "Graphics.h"
#include "timing.h"
#include "ffunc.h"
#include "mcp3002.h"
#include "power_meter.h"

#define PI 3.14159265358979323846

pthread_t thbutton;
pthread_t thMeter_Movement;
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
#define PATH_XYCONFIG "/home/pi/rpidatv/src/power_meter/pm_config.txt"

#define MAX_BUTTON 675
int IndexButtonInArray=0;
button_t ButtonArray[MAX_BUTTON];
int CurrentMenu = 6;
int CallingMenu = 6;
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
bool normalised = false;
bool normalising = false;
bool activescan = false;
bool finishednormalising = false;
int y[501];               // Actual displayed values on the chart
int norm[501];            // Normalisation corrections
int scaledadresult[501];  // Sensed AD Result
int normleveloffset = 300;

int startfreq = 0;
int stopfreq = 0;
int rbw = 0;
int reflevel = 99;
int normlevel = -20;
char PlotTitle[63] = "-";
bool ContScan = false;
bool Meter = true;
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

bool webcontrol = false;   // Enables webcontrol on a Portsdown 4

int tracecount = 0;  // Used for speed testing
int exit_code;

int yscalenum = 18;       // Numerator for Y scaling fraction
int yscaleden = 30;       // Denominator for Y scaling fraction
int yshift    = 5;        // Vertical shift (pixels) for Y

int xmin = 30;            // Trigger value for start of scan
int xscalenum = 26;       // Numerator for X scaling fraction
int xscaleden = 20;       // Denominator for X scaling fraction

int meter_deflection = -110;
char MeterSensor[63];     // ad8318-3
char MeterTitle[63] = "-";
char MeterSource[63] = "dbm" ;     // dbm mw volts a-d db
char MeterRange[63];
char MetermWRange[63];
char MeterZero[63];
char MeterAtten[63];
char MeterCalFactor[63];
int Meterrange;
float Metermwrange;
int Meterzero;
float Meteratten;
int Metercalfactor;
char MeterUnits[63] = "dBm";
float ActiveZero = -80.0;
float ActiveFSD = 20.0;

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
void do_snapcheck();
int IsImageToBeChanged(int x, int y);
void UpdateWeb();
void Keyboard(char RequestText[64], char InitText[64], int MaxLength);
int openTouchScreen(int);
int getTouchScreenDetails(int *screenXmin, int *screenXmax,int *screenYmin,int *screenYmax);
void TransformTouchMap(int x, int y);
int IsButtonPushed(int NbButton, int x, int y);
int InitialiseButtons();
int AddButton(int x, int y, int w, int h);
int ButtonNumber(int MenuIndex, int Button);
int CreateButton(int MenuIndex, int ButtonPosition);
int AddButtonStatus(int ButtonIndex, char *Text, color_t *Color);
void AmendButtonStatus(int ButtonIndex, int ButtonStatusIndex, char *Text, color_t *Color);
void DrawButton(int ButtonIndex);
void SetButtonStatus(int ButtonIndex, int Status);
int GetButtonStatus(int ButtonIndex);
int getTouchSampleThread(int *rawX, int *rawY, int *rawPressure);
int getTouchSample(int *rawX, int *rawY, int *rawPressure);
void UpdateWindow();
void wait_touch();
void CalculateMarkers();
void Normalise();
void ChangeLabel(int button);
void ChangeRange(int button);
void ChangeSensor(int button);
void *WaitButtonEvent(void * arg);
void Define_Menu1();
void Start_Highlights_Menu1();
void Define_Menu2();
void Define_Menu3();
void Define_Menu4();
void Start_Highlights_Menu4();
void Define_Menu5();
void Start_Highlights_Menu5();
void Define_Menu6();
void Start_Highlights_Menu6();
void Define_Menu7();
void Define_Menu8();
void Start_Highlights_Menu8();
void Define_Menu9();
void Start_Highlights_Menu9();
void Define_Menu41();
void DrawEmptyScreen();
void DrawYaxisLabels();
void DrawSettings();
void DrawTrace(int xoffset, int prev2, int prev1, int current);
void DrawMeterBox();
void DrawMeterTicks(int major_ticks, int minor_ticks);
void DrawMeterSettings();
void Draw5MeterLabels(float LH_Value, float RH_Value);
void *MeterMovement(void * arg);
void ShowdBm(float dBm);
void ShowmW(float mW);
void Showraw(int raw);
void Showvolts(float raw);
void CheckWithinRange(int advalue);
static void cleanexit(int calling_exit_code);
static void terminate(int dummy);


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
  GetConfigParam(PATH_XYCONFIG, "startfreq", response);
  startfreq = atoi(response);

  strcpy(response, "0");
  GetConfigParam(PATH_XYCONFIG, "stopfreq", response);
  stopfreq = atoi(response);

  strcpy(response, "99");  // this is the "do not display" level
  GetConfigParam(PATH_XYCONFIG, "reflevel", response);
  reflevel = atoi(response);

  strcpy(response, "-20");  // this is the default level
  GetConfigParam(PATH_XYCONFIG, "normlevel", response);
  normlevel = atoi(response);

  strcpy(response, "0");  // this is the "do not display" level
  GetConfigParam(PATH_XYCONFIG, "rbw", response);
  rbw = atoi(response);

  strcpy(PlotTitle, "-");  // this is the "do not display" response
  GetConfigParam(PATH_XYCONFIG, "title", PlotTitle);

  strcpy(MeterSensor, "-");  // like ad8318-3
  GetConfigParam(PATH_XYCONFIG, "metersensor", MeterSensor);

  strcpy(MeterTitle, "-");  // this is the "do not display" response
  GetConfigParam(PATH_XYCONFIG, "metertitle", MeterTitle);

  strcpy(MeterSource, "-");  // like dbm or mw
  GetConfigParam(PATH_XYCONFIG, "metersource", MeterSource);

  strcpy(MeterRange, "10");  // integer units from 0 to FSD min 5 max 100
  GetConfigParam(PATH_XYCONFIG, "meterrange", MeterRange);
  Meterrange = atoi(MeterRange);

  strcpy(MeterZero, "0");  // Integer Meter zero value
  GetConfigParam(PATH_XYCONFIG, "meterzero", MeterZero);
  Meterzero = atoi(MeterZero);

  strcpy(MetermWRange, "1.0");  // 
  GetConfigParam(PATH_XYCONFIG, "metermwrange", MetermWRange);
  Metermwrange = atof(MetermWRange);


  strcpy(MeterAtten, "0");  // dB of ext atten
  GetConfigParam(PATH_XYCONFIG, "meteratten", MeterAtten);
  Meteratten = atof(MeterAtten);

  strcpy(MeterCalFactor, "100");  // %
  GetConfigParam(PATH_XYCONFIG, "metercalfactor", MeterCalFactor);
  Metercalfactor = atoi(MeterCalFactor);

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

int IsImageToBeChanged(int x, int y)
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
  // Provide Menu number (int 1 - 46), Button Position (0 bottom left, 23 top right)
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

  if ((MenuIndex != 41) && (MenuIndex != 2) && (MenuIndex != 6))   // All except keyboard, Markers, Main Power
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
  else if ((MenuIndex == 2) || (MenuIndex == 6))  // Marker or Power(main) Menu
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


int getTouchSampleOLD(int *rawX, int *rawY, int *rawPressure)
{
	int i;
        /* how many bytes were read */
        size_t rb;
        /* the events (up to 64 at once) */
        struct input_event ev[64];
	//static int Last_event=0; //not used?
	rb=read(fd,ev,sizeof(struct input_event)*64);
	*rawX=-1;*rawY=-1;
	int StartTouch=0;
        for (i = 0;  i <  (rb / sizeof(struct input_event)); i++){
              if (ev[i].type ==  EV_SYN)
		{
                         //printf("Event type is %s%s%s = Start of New Event\n",KYEL,events[ev[i].type],KWHT);
		}
                else if (ev[i].type == EV_KEY && ev[i].code == 330 && ev[i].value == 1)
		{
			StartTouch=1;
                        //printf("Event type is %s%s%s & Event code is %sTOUCH(330)%s & Event value is %s1%s = Touch Starting\n", KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,KWHT);
		}
                else if (ev[i].type == EV_KEY && ev[i].code == 330 && ev[i].value == 0)
		{
			//StartTouch=0;
			//printf("Event type is %s%s%s & Event code is %sTOUCH(330)%s & Event value is %s0%s = Touch Finished\n", KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,KWHT);
		}
                else if (ev[i].type == EV_ABS && ev[i].code == 0 && ev[i].value > 0){
                        //printf("Event type is %s%s%s & Event code is %sX(0)%s & Event value is %s%d%s\n", KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,ev[i].value,KWHT);
			*rawX = ev[i].value;
		}
                else if (ev[i].type == EV_ABS  && ev[i].code == 1 && ev[i].value > 0){
                        //printf("Event type is %s%s%s & Event code is %sY(1)%s & Event value is %s%d%s\n", KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,ev[i].value,KWHT);
			*rawY = ev[i].value;
		}
                else if (ev[i].type == EV_ABS  && ev[i].code == 24 && ev[i].value > 0){
                        //printf("Event type is %s%s%s & Event code is %sPressure(24)%s & Event value is %s%d%s\n", KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,ev[i].value,KWHT);
			*rawPressure = ev[i].value;
		}
		if((*rawX!=-1)&&(*rawY!=-1)&&(StartTouch==1))
		{
			/*if(Last_event-mymillis()>500)
			{
				Last_event=mymillis();
				return 1;
			}*/
			//StartTouch=0;
			return 1;
		}

	}
	return 0;
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
      for (i = 50; i < 450; i++)
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
      for (i = 50; i < 450; i++)
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
  markerlev = ((float)(410 - markery) / 5) - 80.0;
  rectangle(620, 420, 160, 60, 0, 0, 0);  // Blank the Menu title
  snprintf(markerlevel, 31, "Mkr %0.1f dB", markerlev);
  TextMid2(700, 450, markerlevel, &font_dejavu_sans_18);

  if((startfreq != -1 ) && (stopfreq != -1)) // "Do not display" values
  {

    markerf = (float)((((markerx - 100) * (stopfreq - startfreq)) / 500 + startfreq)) / 1000.0; 
    snprintf(markerfreq, 31, "%0.2f MHz", markerf);
    setBackColour(0, 0, 0);
    TextMid2(700, 425, markerfreq, &font_dejavu_sans_18);
  }
}


void Normalise()
{
  normleveloffset = 400 + normlevel * 5;  // 0 = screen bottom, 400 = screen top

  finishednormalising = false;            // Make sure that it is not normalising (ie measuring the baseline)

  while (activescan == true)              // Wait for the end of the current scan
  {
    usleep(10);
  }

  normalising = true;                     // Set the flag for it to measure the baseline

  while (finishednormalising == false)    // Wait for the end of the measurement scan
  {
    usleep(10);
  }

  normalising = false;                    // Stop it measuring

  normalised = true;                      // and start it calculating for subsequent scans
}

void ChangeLabel(int button)
{

  char RequestText[64];
  char InitText[63];
  bool IsValid = false;
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
    case 2:                                                       // Start Freq
      // Define request string
      strcpy(RequestText, "Enter new start frequency in MHz");

      // Define initial value and convert to MHz
      if(startfreq == -1 ) // "Do not display" value
      {
        InitText[0] = '\0';
      }
      else
      {
        div_10 = div(startfreq, 10);
        div_1000 = div(startfreq, 1000);

        if(div_10.rem != 0)  // last character not zero, so make answer of form xxx.xxx
        {
          snprintf(InitText, 10, "%d.%03d", div_1000.quot, div_1000.rem);
        }
        else
        {
          div_100 = div(startfreq, 100);

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
      }

      // Ask for the new value
      Keyboard(RequestText, InitText, 10);
      if (strlen(KeyboardReturn) == 0)
      {
        startfreq = -1;
      }
      else
      {
        startfreq = (int)((1000 * atof(KeyboardReturn)) + 0.1);
      }
      snprintf(ValueToSave, 63, "%d", startfreq);
      SetConfigParam(PATH_XYCONFIG, "startfreq", ValueToSave);
      printf("StartFreq set to %d \n", startfreq);
      break;

    case 3:                                                       // Stop Freq
      // Define request string
      strcpy(RequestText, "Enter new stop frequency in MHz");

      // Define initial value and convert to MHz
      if(stopfreq == -1 ) // "Do not display" value
      {
        InitText[0] = '\0';
      }
      else
      {
        div_10 = div(stopfreq, 10);
        div_1000 = div(stopfreq, 1000);

        if(div_10.rem != 0)  // last character not zero, so make answer of form xxx.xxx
        {
          snprintf(InitText, 10, "%d.%03d", div_1000.quot, div_1000.rem);
        }
        else
        {
          div_100 = div(stopfreq, 100);

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
      }

      // Ask for the new value
      Keyboard(RequestText, InitText, 10);
      if (strlen(KeyboardReturn) == 0)
      {
        stopfreq = -1;
      }
      else
      {
        stopfreq = (int)((1000 * atof(KeyboardReturn)) + 0.1);
      }
      snprintf(ValueToSave, 63, "%d", stopfreq);
      SetConfigParam(PATH_XYCONFIG, "stopfreq", ValueToSave);
      printf("stopFreq set to %d \n", stopfreq);
      break;
    case 4:                                                       // Ref Level
      // Define request string
      strcpy(RequestText, "Enter new Reference Level in dBm");
      // Define initial value 
      if(reflevel == 99 ) // "Do not display" value
      {
        InitText[0] = '\0';
      }
      else
      {
        snprintf(InitText, 10, "%d", reflevel);
      }

      while (IsValid == false)
      {
        Keyboard(RequestText, InitText, 4);
        if (strlen(KeyboardReturn) == 0)
        {
          reflevel = 99;  // "Do not display" value
        }
        else
        {
          reflevel = atoi(KeyboardReturn);
        }
        if ((reflevel > -200) && (reflevel < 100))
        {
          IsValid = true;
        }
      }
      snprintf(ValueToSave, 63, "%d", reflevel);
      SetConfigParam(PATH_XYCONFIG, "reflevel", ValueToSave);
      printf("Ref Level set to: %d\n", reflevel);
      break;
    case 5:                                                       // RBW
      // Define request string
      strcpy(RequestText, "Enter new Resolution Bandwidth in kHz");
      // Define initial value 
      if(rbw == 0) // "Do not display" value
      {
        InitText[0] = '\0';
      }
      else
      {
        snprintf(InitText, 10, "%d", rbw);
      }

      while (IsValid == false)
      {
        Keyboard(RequestText, InitText, 5);
        if (strlen(KeyboardReturn) == 0)
        {
          rbw = 0;  // "Do not display" value
        }
        else
        {
          rbw = atoi(KeyboardReturn);
        }
        if ((rbw >= 0) && (rbw < 10000))
        {
          IsValid = true;
        }
      }
      snprintf(ValueToSave, 63, "%d", rbw);
      SetConfigParam(PATH_XYCONFIG, "rbw", ValueToSave);
      printf("Resolution Bandwidth set to: %d\n", rbw);
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
      SetConfigParam(PATH_XYCONFIG, "title", PlotTitle);
      printf("Plot Title set to: %s\n", KeyboardReturn);
      break;
    case 7:                                                       // Set Normalise Level
      // Define request string
      strcpy(RequestText, "Enter new Normalise Level (Range 0 to -80)");

      // Define initial value 
      snprintf(InitText, 10, "%d", normlevel);

      while (IsValid == false)
      {
        Keyboard(RequestText, InitText, 4);
        if (strlen(KeyboardReturn) == 0)
        {
          IsValid = false;
        }
        else
        {
          normlevel = atoi(KeyboardReturn);
          if ((normlevel >= -80) && (normlevel <= 0))
          {
            IsValid = true;
          }
          else
          {
            IsValid = false;
          }
        }
      }
      snprintf(ValueToSave, 63, "%d", normlevel);
      SetConfigParam(PATH_XYCONFIG, "normlevel", ValueToSave);
      printf("Normalisation Level set to: %d\n", normlevel);
      break;

    case 16:                                                       // Power Meter Title
      strcpy(RequestText, "Enter the title to be displayed on the Meter");
      if(strcmp(PlotTitle, "-") == 0)           // Active Blank
      {
        InitText[0] = '\0';
      }
      else
      {
        snprintf(InitText, 63, "%s", MeterTitle);
      }

      Keyboard(RequestText, InitText, 40);
  
      if(strlen(KeyboardReturn) > 0)
      {
        strcpy(MeterTitle, KeyboardReturn);
      }
      else
      {
        strcpy(MeterTitle, "-");
      }
      SetConfigParam(PATH_XYCONFIG, "metertitle", MeterTitle);
      printf("Meter Title set to: %s\n", KeyboardReturn);
      break;

    case 12:                                                       // External Attenuator
      // Define request string
      strcpy(RequestText, "Enter value (dB) of External Attenuator");

      // Ask for the new value
      Keyboard(RequestText, MeterAtten, 10);
      if (strlen(KeyboardReturn) == 0)
      {
        strcpy(MeterAtten, "0");
      }
      else
      {
        strcpy(MeterAtten, KeyboardReturn);
      }

      // Convert and store
      Meteratten = 0.0;  // In case atof fails
      Meteratten = atof(MeterAtten);
      SetConfigParam(PATH_XYCONFIG, "meteratten", MeterAtten);
      printf("Attenuator set to set to %f \n", Meteratten);
      break;

    case 13:                                                       // Cal Factor
      // Define request string
      strcpy(RequestText, "Enter Calibration Factor (between 5 and 500 %)");

      // Ask for the new value
      Keyboard(RequestText, MeterCalFactor, 10);
      if (strlen(KeyboardReturn) == 0)
      {
        strcpy(MeterCalFactor, "100");
      }
      else
      {
        strcpy(MeterCalFactor, KeyboardReturn);
      }

      // Convert and store
      Metercalfactor = 100;  // In case atoi fails
      Metercalfactor = atoi(MeterCalFactor);
      SetConfigParam(PATH_XYCONFIG, "metercalfactor", MeterCalFactor);
      printf("Cal Factor set to %d%%\n", Metercalfactor);
      break;
  }

  // Tidy up, paint around the screen and then unfreeze
  wipeScreen(0, 0, 0);
  if (Meter == false)
  {
    DrawEmptyScreen();  // Required to set A value, which is not set in DrawTrace
    DrawYaxisLabels();  // dB calibration on LHS
    DrawSettings();     // Start, Stop RBW, Ref level and Title
  }
  else
  {
    DrawMeterBox();
    DrawMeterArc();
    DrawMeterTicks(10, 1);
    Draw5MeterLabels(-80, +20);
    DrawMeterSettings();
  }
  freeze = false;
}

void ChangeRange(int button)
{
  const font_t *font_ptr = &font_dejavu_sans_18;   // 18pt

  float meterfullscalenow = 1;
  float meterfullscaledown = 5;
  float meterfullscaleup = 2;

  float rangenow = 1.0;
  float rangeup = 2.0;
  float rangedown = 0.5;

  char MeterUnitsNow[63];
  char MeterUnitsUp[63];
  char MeterUnitsDown[63];

  char metermwrangeToSave[63];
  char meterrangeToSave[63];
  char meterzeroToSave[63];

  if (strcmp(MeterSource, "mw") == 0)
  {
    ActiveZero = 0.0;

    if ((Metermwrange > 90000) && (Metermwrange < 110000))  // 100 W = 50 dBm (max)
    {
      rangenow = 100000.0;
      rangeup =  100000.0;
      rangedown = 50000.0;

      strcpy(MeterUnitsNow, "W");
      strcpy(MeterUnitsUp, "W");
      strcpy(MeterUnitsDown, "W");

      meterfullscalenow = 100.0;
      meterfullscaleup =  100.0;
      meterfullscaledown = 50.0;
    }

    if ((Metermwrange > 45000) && (Metermwrange < 55000))  // 50 W
    {
      rangenow =  50000.0;
      rangeup =  100000.0;
      rangedown = 20000.0;

      strcpy(MeterUnitsNow, "W");
      strcpy(MeterUnitsUp, "W");
      strcpy(MeterUnitsDown, "W");

      meterfullscalenow = 50.0;
      meterfullscaleup = 100.0;
      meterfullscaledown = 20.0;
    }

    if ((Metermwrange > 18000) && (Metermwrange < 22000))  // 20 W
    {
      rangenow =  20000.0;
      rangeup =   50000.0;
      rangedown = 10000.0;

      strcpy(MeterUnitsNow, "W");
      strcpy(MeterUnitsUp, "W");
      strcpy(MeterUnitsDown, "W");

      meterfullscalenow = 20.0;
      meterfullscaleup = 50.0;
      meterfullscaledown = 10.0;
    }

    if ((Metermwrange > 9000) && (Metermwrange < 11000))  // 10 W = 40 dBm
    {
      rangenow = 10000.0;
      rangeup = 20000.0;
      rangedown = 5000.0;

      strcpy(MeterUnitsNow, "W");
      strcpy(MeterUnitsUp, "W");
      strcpy(MeterUnitsDown, "W");

      meterfullscalenow = 10.0;
      meterfullscaleup = 20.0;
      meterfullscaledown = 5.0;
    }

    if ((Metermwrange > 4500) && (Metermwrange < 5500))  // 5 W
    {
      rangenow = 5000.0;
      rangeup = 10000.0;
      rangedown = 2000.0;

      strcpy(MeterUnitsNow, "W");
      strcpy(MeterUnitsUp, "W");
      strcpy(MeterUnitsDown, "W");

      meterfullscalenow = 5.0;
      meterfullscaleup = 10.0;
      meterfullscaledown = 2.0;
    }

    if ((Metermwrange > 1800) && (Metermwrange < 2200))  // 2 W
    {
      rangenow = 2000.0;
      rangeup = 5000.0;
      rangedown = 1000.0;

      strcpy(MeterUnitsNow, "W");
      strcpy(MeterUnitsUp, "W");
      strcpy(MeterUnitsDown, "W");

      meterfullscalenow = 2.0;
      meterfullscaleup = 5.0;
      meterfullscaledown = 1.0;
    }

    if ((Metermwrange > 900) && (Metermwrange < 1100))  // 1 W = 30 dBm
    {
      rangenow = 1000.0;
      rangeup = 2000.0;
      rangedown = 500.0;

      strcpy(MeterUnitsNow, "W");
      strcpy(MeterUnitsUp, "W");
      strcpy(MeterUnitsDown, "W");

      meterfullscalenow = 1.0;
      meterfullscaleup = 2.0;
      meterfullscaledown = 0.5;
    }

    if ((Metermwrange > 450) && (Metermwrange < 550))  // 500 mW
    {
      rangenow = 500.0;
      rangeup = 1000.0;
      rangedown = 200.0;

      strcpy(MeterUnitsNow, "W");
      strcpy(MeterUnitsUp, "W");
      strcpy(MeterUnitsDown, "mW");

      meterfullscalenow = 0.5;
      meterfullscaleup = 1.0;
      meterfullscaledown = 200.0;
    }

    if ((Metermwrange > 180) && (Metermwrange < 220))  // 200 mW
    {
      rangenow = 200.0;
      rangeup = 500.0;
      rangedown = 100.0;

      strcpy(MeterUnitsNow, "mW");
      strcpy(MeterUnitsUp, "W");
      strcpy(MeterUnitsDown, "mW");

      meterfullscalenow = 200.0;
      meterfullscaleup = 0.5;
      meterfullscaledown = 100.0;
    }

    if ((Metermwrange > 90) && (Metermwrange < 110))  // 100 mW = 20 dBm
    {
      rangenow = 100.0;
      rangeup = 200.0;
      rangedown = 50.0;

      strcpy(MeterUnitsNow, "mW");
      strcpy(MeterUnitsUp, "mW");
      strcpy(MeterUnitsDown, "mW");

      meterfullscalenow = 100.0;
      meterfullscaleup = 200.0;
      meterfullscaledown = 50.0;
    }

    if ((Metermwrange > 45) && (Metermwrange < 55))  // 50 mW
    {
      rangenow = 50.0;
      rangeup = 100.0;
      rangedown = 20.0;

      strcpy(MeterUnitsNow, "mW");
      strcpy(MeterUnitsUp, "mW");
      strcpy(MeterUnitsDown, "mW");

      meterfullscalenow = 50.0;
      meterfullscaleup = 100.0;
      meterfullscaledown = 20.0;
    }

    if ((Metermwrange > 18) && (Metermwrange < 22))  // 20 mW
    {
      rangenow = 20.0;
      rangeup = 50.0;
      rangedown = 10.0;

      strcpy(MeterUnitsNow, "mW");
      strcpy(MeterUnitsUp, "mW");
      strcpy(MeterUnitsDown, "mW");

      meterfullscalenow = 20.0;
      meterfullscaleup = 50.0;
      meterfullscaledown = 10.0;
    }

    if ((Metermwrange > 9.0) && (Metermwrange < 11))  // 10 mW = 10 dBm
    {
      rangenow = 10.0;
      rangeup = 20.0;
      rangedown = 5.0;

      strcpy(MeterUnitsNow, "mW");
      strcpy(MeterUnitsUp, "mW");
      strcpy(MeterUnitsDown, "mW");

      meterfullscalenow = 10.0;
      meterfullscaleup = 20.0;
      meterfullscaledown = 5.0;
    }

    if ((Metermwrange > 4.5) && (Metermwrange < 5.5))  // 5 mW
    {
      rangenow = 5.0;
      rangeup = 10.0;
      rangedown = 2.0;

      strcpy(MeterUnitsNow, "mW");
      strcpy(MeterUnitsUp, "mW");
      strcpy(MeterUnitsDown, "mW");

      meterfullscalenow = 5.0;
      meterfullscaleup = 10.0;
      meterfullscaledown = 2.0;
    }

    if ((Metermwrange > 1.8) && (Metermwrange < 2.2))  // 2 mW
    {
      rangenow = 2.0;
      rangeup = 5.0;
      rangedown = 1.0;

      strcpy(MeterUnitsNow, "mW");
      strcpy(MeterUnitsUp, "mW");
      strcpy(MeterUnitsDown, "mW");

      meterfullscalenow = 2.0;
      meterfullscaleup = 5.0;
      meterfullscaledown = 1.0;
    }

    if ((Metermwrange > 0.9) && (Metermwrange < 1.1))  // 1 mW = 0 dBm
    {
      rangenow = 1.0;
      rangeup = 2.0;
      rangedown = 0.5;

      strcpy(MeterUnitsNow, "mW");
      strcpy(MeterUnitsUp, "mW");
      strcpy(MeterUnitsDown, "mW");

      meterfullscalenow = 1.0;
      meterfullscaleup = 2.0;
      meterfullscaledown = 0.5;
    }

    if ((Metermwrange > 0.45) && (Metermwrange < 0.55))  // 500 uW
    {
      rangenow = 0.5;
      rangeup = 1.0;
      rangedown = 0.2;

      strcpy(MeterUnitsNow, "mW");
      strcpy(MeterUnitsUp, "mW");
      strcpy(MeterUnitsDown, "uW");

      meterfullscalenow = 0.5;
      meterfullscaleup = 1.0;
      meterfullscaledown = 200;
    }

    if ((Metermwrange > 0.18) && (Metermwrange < 0.22))  // 200 uW
    {
      rangenow = 0.2;
      rangeup = 0.5;
      rangedown = 0.1;

      strcpy(MeterUnitsNow, "uW");
      strcpy(MeterUnitsUp, "mW");
      strcpy(MeterUnitsDown, "uW");

      meterfullscalenow = 200;
      meterfullscaleup = 0.5;
      meterfullscaledown = 100;
    }

    if ((Metermwrange > 0.09) && (Metermwrange < 0.11))  // 100 uW = -10 dBm
    {
      rangenow = 0.1;
      rangeup = 0.2;
      rangedown = 0.05;

      strcpy(MeterUnitsNow, "uW");
      strcpy(MeterUnitsUp, "uW");
      strcpy(MeterUnitsDown, "uW");

      meterfullscalenow = 100;
      meterfullscaleup = 200;
      meterfullscaledown = 50;
    }

    if ((Metermwrange > 0.045) && (Metermwrange < 0.055))  // 50 uW
    {
      rangenow = 0.05;
      rangeup = 0.1;
      rangedown = 0.02;

      strcpy(MeterUnitsNow, "uW");
      strcpy(MeterUnitsUp, "uW");
      strcpy(MeterUnitsDown, "uW");

      meterfullscalenow = 50;
      meterfullscaleup = 100;
      meterfullscaledown = 20;
    }

    if ((Metermwrange > 0.018) && (Metermwrange < 0.022))  // 20 uW
    {
      rangenow = 0.02;
      rangeup = 0.05;
      rangedown = 0.01;

      strcpy(MeterUnitsNow, "uW");
      strcpy(MeterUnitsUp, "uW");
      strcpy(MeterUnitsDown, "uW");

      meterfullscalenow = 20;
      meterfullscaleup = 50;
      meterfullscaledown = 10;
    }

    if ((Metermwrange > 0.009) && (Metermwrange < 0.011))  // 10 uW = -20 dBm
    {
      rangenow = 0.01;
      rangeup = 0.02;
      rangedown = 0.005;

      strcpy(MeterUnitsNow, "uW");
      strcpy(MeterUnitsUp, "uW");
      strcpy(MeterUnitsDown, "uW");

      meterfullscalenow = 10;
      meterfullscaleup = 20;
      meterfullscaledown = 5;
    }

    if ((Metermwrange > 0.0045) && (Metermwrange < 0.0055))  // 5 uW
    {
      rangenow = 0.005;
      rangeup = 0.01;
      rangedown = 0.002;

      strcpy(MeterUnitsNow, "uW");
      strcpy(MeterUnitsUp, "uW");
      strcpy(MeterUnitsDown, "uW");

      meterfullscalenow = 5;
      meterfullscaleup = 10;
      meterfullscaledown = 2;
    }

    if ((Metermwrange > 0.0018) && (Metermwrange < 0.0022))  // 2 uW
    {
      rangenow = 0.002;
      rangeup = 0.005;
      rangedown = 0.001;

      strcpy(MeterUnitsNow, "uW");
      strcpy(MeterUnitsUp, "uW");
      strcpy(MeterUnitsDown, "uW");

      meterfullscalenow = 2;
      meterfullscaleup = 5;
      meterfullscaledown = 1;
    }

    if ((Metermwrange > 0.0009) && (Metermwrange < 0.0011))  // 1 uW = -30 dBm
    {
      rangenow = 0.001;
      rangeup = 0.002;
      rangedown = 0.0005;

      strcpy(MeterUnitsNow, "uW");
      strcpy(MeterUnitsUp, "uW");
      strcpy(MeterUnitsDown, "uW");

      meterfullscalenow = 1;
      meterfullscaleup = 2;
      meterfullscaledown = 0.5;
    }

    if ((Metermwrange > 0.00045) && (Metermwrange < 0.00055))  // 0.5 uW
    {
      rangenow = 0.0005;
      rangeup = 0.001;
      rangedown = 0.0002;

      strcpy(MeterUnitsNow, "uW");
      strcpy(MeterUnitsUp, "uW");
      strcpy(MeterUnitsDown, "nW");

      meterfullscalenow = 0.5;
      meterfullscaleup = 1.0;
      meterfullscaledown = 200;
    }

    if ((Metermwrange > 0.00018) && (Metermwrange < 0.00022))  // 200 nW
    {
      rangenow = 0.0002;
      rangeup = 0.0005;
      rangedown = 0.0001;

      strcpy(MeterUnitsNow, "nW");
      strcpy(MeterUnitsUp, "uW");
      strcpy(MeterUnitsDown, "nW");

      meterfullscalenow = 200;
      meterfullscaleup = 0.5;
      meterfullscaledown = 100;
    }

    if ((Metermwrange > 0.00009) && (Metermwrange < 0.00011))  // 100 nW = -40 dBm
    {
      rangenow = 0.0001;
      rangeup = 0.0002;
      rangedown = 0.00005;

      strcpy(MeterUnitsNow, "nW");
      strcpy(MeterUnitsUp, "nW");
      strcpy(MeterUnitsDown, "nW");

      meterfullscalenow = 100;
      meterfullscaleup = 200;
      meterfullscaledown = 50;
    }

    if ((Metermwrange > 0.000045) && (Metermwrange < 0.000055))  // 50 nW
    {
      rangenow = 0.00005;
      rangeup = 0.0001;
      rangedown = 0.00002;

      strcpy(MeterUnitsNow, "nW");
      strcpy(MeterUnitsUp, "nW");
      strcpy(MeterUnitsDown, "nW");

      meterfullscalenow = 50;
      meterfullscaleup = 100;
      meterfullscaledown = 20;
    }

    if ((Metermwrange > 0.000018) && (Metermwrange < 0.000022))  // 20 nW
    {
      rangenow =  0.00002;
      rangeup =   0.00005;
      rangedown = 0.00001;

      strcpy(MeterUnitsNow, "nW");
      strcpy(MeterUnitsUp, "nW");
      strcpy(MeterUnitsDown, "nW");

      meterfullscalenow = 20;
      meterfullscaleup = 50;
      meterfullscaledown = 10;
    }

    if ((Metermwrange > 0.000009) && (Metermwrange < 0.000011))  // 10 nW = -50 dBm
    {
      rangenow = 0.00001;
      rangeup = 0.00002;
      rangedown = 0.000005;

      strcpy(MeterUnitsNow, "nW");
      strcpy(MeterUnitsUp, "nW");
      strcpy(MeterUnitsDown, "nW");

      meterfullscalenow = 10;
      meterfullscaleup = 20;
      meterfullscaledown = 5;
    }

    if ((Metermwrange > 0.0000045) && (Metermwrange < 0.0000055))  // 5 nW
    {
      rangenow = 0.000005;
      rangeup = 0.00001;
      rangedown = 0.000002;

      strcpy(MeterUnitsNow, "nW");
      strcpy(MeterUnitsUp, "nW");
      strcpy(MeterUnitsDown, "nW");

      meterfullscalenow = 5;
      meterfullscaleup = 10;
      meterfullscaledown = 2;
    }

    if ((Metermwrange > 0.0000018) && (Metermwrange < 0.0000022))  // 2 nW
    {
      rangenow = 0.000002;
      rangeup = 0.000005;
      rangedown = 0.000001;

      strcpy(MeterUnitsNow, "nW");
      strcpy(MeterUnitsUp, "nW");
      strcpy(MeterUnitsDown, "nW");

      meterfullscalenow = 2.0;
      meterfullscaleup = 5.0;
      meterfullscaledown = 1.0;
    }

    if ((Metermwrange > 0.0000009) && (Metermwrange < 0.0000011))  // 1 nW = -60 dBm (lowest)
    {
      rangenow = 0.000001;
      rangeup = 0.000002;
      rangedown = 0.000001;

      strcpy(MeterUnitsNow, "nW");
      strcpy(MeterUnitsUp, "nW");
      strcpy(MeterUnitsDown, "nW");

      meterfullscalenow = 1.0;
      meterfullscaleup = 2.0;
      meterfullscaledown = 1.0;
    }

    switch (button)
    {
      case 0:                                            // Just set the meter scale
        Metermwrange = rangenow;
        strcpy(MeterUnits, MeterUnitsNow);
        ActiveFSD = meterfullscalenow;
        break;   
      case 2:                                            // Power up
        Metermwrange = rangeup;
        strcpy(MeterUnits, MeterUnitsUp);
        ActiveFSD = meterfullscaleup;

        // Store the new setting in the config file
        snprintf(metermwrangeToSave, 63, "%f", Metermwrange);
        SetConfigParam(PATH_XYCONFIG, "metermwrange", metermwrangeToSave);
        break;
      case 3:                                            // Power down
        Metermwrange = rangedown;
        strcpy(MeterUnits, MeterUnitsDown);
        ActiveFSD = meterfullscaledown;

        // Store the new setting in the config file
        snprintf(metermwrangeToSave, 63, "%f", Metermwrange);
        SetConfigParam(PATH_XYCONFIG, "metermwrange", metermwrangeToSave);
        break;
      case 6:                                            // Reset
        Metermwrange = 1.0;
        strcpy(MeterUnits, "mW");
        ActiveFSD = 1.0;

        // Store the new setting in the config file
        snprintf(metermwrangeToSave, 63, "%f", Metermwrange);
        SetConfigParam(PATH_XYCONFIG, "metermwrange", metermwrangeToSave);
        break;
    }
  }

  if (strcmp(MeterSource, "dbm") == 0)
  {
    strcpy(MeterUnits, "dBm");
 
    switch (button)
    {
      case 0:                                            // Just set the meter scale to value from file
        ActiveZero = (float)Meterzero;
        ActiveFSD = (float)(Meterzero + Meterrange);
        break;   
      case 2:                                            // Power up
        if (Meterzero <= 45)  
        {
          Meterzero = Meterzero + 5;
          ActiveZero = (float)Meterzero;
          ActiveFSD = (float)(Meterzero + Meterrange);
          snprintf(meterzeroToSave, 63, "%d", Meterzero);
          SetConfigParam(PATH_XYCONFIG, "meterzero", meterzeroToSave);
        }
        break;
      case 3:                                            // Power down
        if (Meterzero >= -75)  
        {
          Meterzero = Meterzero - 5;
          ActiveZero = (float)Meterzero;
          ActiveFSD = (float)(Meterzero + Meterrange);
          snprintf(meterzeroToSave, 63, "%d", Meterzero);
          SetConfigParam(PATH_XYCONFIG, "meterzero", meterzeroToSave);
        }
        break;
      case 4:                                            // Wider Range
        if (Meterrange < 100)       // 100 is max range for dBm
        {
          if (Meterrange == 10)
          {
            Meterrange = (5 * Meterrange) / 2;  // int maths!
          }
          else  // 5, 25, 50
          {
            Meterrange = 2 * Meterrange;
          }
          ActiveZero = (float)Meterzero;
          ActiveFSD = (float)(Meterzero + Meterrange);
          snprintf(meterrangeToSave, 63, "%d", Meterrange);
          SetConfigParam(PATH_XYCONFIG, "meterrange", meterrangeToSave);
        }
        break;
      case 5:                                            // Smaller Range
        if (Meterrange > 5)       // 5 is min range for dBm
        {
          if (Meterrange == 25)
          {
            Meterrange = (2 * Meterrange) / 5;  // int maths!
          }
          else  // 10, 50, 100
          {
            Meterrange = Meterrange / 2;
          }
          ActiveZero = (float)Meterzero;
          ActiveFSD = (float)(Meterzero + Meterrange);
          snprintf(meterrangeToSave, 63, "%d", Meterrange);
          SetConfigParam(PATH_XYCONFIG, "meterrange", meterrangeToSave);
        }
        break;
      case 6:                                            // Reset
        Meterzero = -45;
        Meterrange = 50;
        ActiveZero = (float)Meterzero;
        ActiveFSD = (float)(Meterzero + Meterrange);
        snprintf(meterzeroToSave, 63, "%d", Meterzero);
        SetConfigParam(PATH_XYCONFIG, "meterzero", meterzeroToSave);
        snprintf(meterrangeToSave, 63, "%d", Meterrange);
        SetConfigParam(PATH_XYCONFIG, "meterrange", meterrangeToSave);
        break;
    }
  }

  if (strcmp(MeterSensor, "raw") == 0)
  {
    ActiveZero = 0;
    ActiveFSD = 1000.0;
    strcpy(MeterUnits, " ");
  }

  if (strcmp(MeterSensor, "voltage") == 0)
  {
    ActiveZero = 0;
    ActiveFSD = 5.0;
    strcpy(MeterUnits, "Volts");
  }

  // Overwrite the Meter Units
  rectangle(120, 180, 50, 30, 0, 0, 0);
  pthread_mutex_lock(&text_lock);
  setBackColour(0, 0, 0);
  setForeColour(63, 255, 63);
  Text2(120, 180, MeterUnits, font_ptr);
  setForeColour(255, 255, 255);
  pthread_mutex_unlock(&text_lock);


}

void ChangeSensor(int button)
{
    switch (button)
    {
      case 2:                                         // ad8318-3
        strcpy(MeterSensor, "ad8318-3");
        SetConfigParam(PATH_XYCONFIG, "metersensor", "ad8318-3");
      break;
      case 3:                                         // ad8318-5
        strcpy(MeterSensor, "ad8318-5");
        SetConfigParam(PATH_XYCONFIG, "metersensor", "ad8318-5");
      break;
      case 5:
        strcpy(MeterSensor, "voltage");
        SetConfigParam(PATH_XYCONFIG, "metersensor", "voltage");
        break;
      case 6:
        strcpy(MeterSensor, "raw");
        SetConfigParam(PATH_XYCONFIG, "metersensor", "raw");
        break;
    }
    ChangeRange(0); // calculate and draw the meter units
    Draw5MeterLabels(ActiveZero, ActiveFSD);
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
        case 2:                                            // Select Mode
          printf("Mode Menu 5 Requested\n");
          CurrentMenu = 5;
          Start_Highlights_Menu5();
          UpdateWindow();
          break;
        case 3:                                            // Markers
          printf("Markers Menu 2 Requested\n");
          CurrentMenu=2;
          UpdateWindow();
          break;
        case 4:                                            // Normalise
          if (normalised == false)
          {
            Normalise();
            SetButtonStatus(ButtonNumber(CurrentMenu, 4), 1);
            UpdateWindow();
            normalised = true;
          }
          else
          {
            SetButtonStatus(ButtonNumber(CurrentMenu, 4), 0);
            UpdateWindow();
            normalised = false;
          }
          break;
        case 5:                                            // Labels
          printf("Labels Menu 3 Requested\n");
          CurrentMenu=3;
          UpdateWindow();
          break;
        case 6:                                            // System
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
      if ((i != 7) && (PortsdownExitRequested == true))
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

    if (CurrentMenu == 3)  // XY Labels Menu
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
        case 2:                                            // Start
        case 3:                                            // Stop
        case 4:                                            // Ref Level
        case 5:                                            // RBW
        case 6:                                            // Title
          ChangeLabel(i);
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

    if (CurrentMenu == 4)  //  System Menu (Power and XY)
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
          if (Meter == true)
          {
            setBackColour(0, 0, 0);
            DrawMeterBox();
            DrawMeterArc();
            DrawMeterTicks(10, 1);
            Draw5MeterLabels(ActiveZero, ActiveFSD);
            DrawMeterSettings();
          }
          else
          {
            DrawEmptyScreen();
            DrawYaxisLabels();  // dB calibration on LHS
            DrawSettings();     // Start, Stop RBW, Ref level and Title
          }
          UpdateWindow();
          freeze = false;
          break;
        case 3:                                            // Set Normalise Level
          if (Meter == false)                              // Only for XY
          {
            ChangeLabel(7);
            CurrentMenu=1;
            UpdateWindow();
          }
          break;
        case 4:                                            // Shutdown
          system("sudo shutdown now");
          break;
        case 5:                                            // Exit to Portsdown
          freeze = true;
          usleep(100000);
          wipeScreen(0, 0, 0);
          UpdateWeb();
          usleep(1000000);
          closeScreen();
          cleanexit(207);
          break;
        case 6:                                            // Restart this App
          freeze = true;
          usleep(100000);
          wipeScreen(0, 0, 0);
          usleep(1000000);
          closeScreen();
          cleanexit(137);
          break;
        case 7:                                            // Return to Main Menu
          if (Meter == false)
          {
            printf("Main Menu 1 Requested\n");
            CurrentMenu = 1;
            Start_Highlights_Menu1();
          }
          else  // Go to Meter Main Menu
          {
            printf("Main Menu 6 Requested\n");
            CurrentMenu = 6;
            Start_Highlights_Menu6();
          }
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

    if (CurrentMenu == 5)  // Mode Menu (XY and Power)
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
        case 2:                                            // Power Meter Mode
          Meter = true;
          ModeChanged = true;
          Start_Highlights_Menu5();
          UpdateWindow();
          break;
        case 3:                                            // XY Mode
          if ((ContScan == true) || (Meter == true))
          {
            ContScan = false;
            Meter = false;
          }
          ModeChanged = true;
          Start_Highlights_Menu5();
          UpdateWindow();
          break;
        case 4:                                            // Continuous Scan Mode
          if ((ContScan == false)  || (Meter == true))
          {
            ContScan = true;
            Meter = false;
          }
          ModeChanged = true;
          Start_Highlights_Menu5();
          UpdateWindow();
          break;
        case 5:                                            // 
          break;
        case 6:                                            // 
          break;
        case 7:                                            // Return to Main Menu
          if (Meter == false)
          {
            printf("Main Menu 1 Requested\n");
            CurrentMenu = 1;
            Start_Highlights_Menu1();
         }
          else  // Go to Meter Main Menu
          {
            printf("Main Menu 6 Requested\n");
            CurrentMenu = 6;
            Start_Highlights_Menu6();
          }
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

    if (CurrentMenu == 6)  // Meter (Main) Menu
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
        case 2:                                            // Select Meter Settings
          printf("Settings Menu 7 Requested\n");
          CurrentMenu=7;
          UpdateWindow();
          break;
        case 3:                                            // Toggle dBm/mw
          if (strcmp(MeterSource, "dbm") == 0)
          {
            strcpy(MeterSource, "mw");
          }
          else
          {
            strcpy(MeterSource, "dbm");
          }
          ChangeRange(0);
          Draw5MeterLabels(ActiveZero, ActiveFSD);
          SetConfigParam(PATH_XYCONFIG, "metersource", MeterSource);
          CurrentMenu=6;
          UpdateWindow();
          break;
        case 4:                                            // Mode
          printf("Mode Menu 5 Requested\n");
          CurrentMenu = 5;
          Start_Highlights_Menu5();
          UpdateWindow();
          break;
        case 5:                                            // Left Arrow
          ChangeRange(3);
          Draw5MeterLabels(ActiveZero, ActiveFSD);
          UpdateWindow();
          break;
        case 6:                                            // Right Arrow
          ChangeRange(2);
          Draw5MeterLabels(ActiveZero, ActiveFSD);
          UpdateWindow();
          break;
        case 7:                                            // System
          printf("System Menu 4 Requested\n");
          CurrentMenu=4;
          Start_Highlights_Menu4();
          UpdateWindow();
          break;
        case 8:                                            // Exit to Portsdown
          if(PortsdownExitRequested)
          {
            freeze = true;
            usleep(100000);
            setBackColour(0, 0, 0);
            wipeScreen(0, 0, 0);
            usleep(1000000);
            closeScreen();
            cleanexit(207);
          }
          else
          {
            PortsdownExitRequested = true;
            Start_Highlights_Menu6();
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
          printf("Menu 6 Error\n");
      }
      if(i != 8)
      {
        PortsdownExitRequested = false;
        Start_Highlights_Menu6();
        UpdateWindow();
      }
      continue;  // Completed Menu 6 action, go and wait for touch
    }
    if (CurrentMenu == 7)  // Settings Menu
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
        case 2:                                            // Ext Atten
          ChangeLabel(i + 10);
          UpdateWindow();          
          break;
        case 3:                                            // Cal Factor
          ChangeLabel(i + 10);
          UpdateWindow();          
          break;
        case 4:                                            // Range
          printf("Range Menu 8 Requested\n");
          CurrentMenu = 8;
          Start_Highlights_Menu8();
          UpdateWindow();
          break;
        case 5:                                            // Sensor
          printf("Sensor Menu 9 Requested\n");
          CurrentMenu = 9;
          Start_Highlights_Menu9();
          UpdateWindow();
          break;
        case 6:                                            // Meter Title
          ChangeLabel(i + 10);
          UpdateWindow();          
          break;
        case 7:                                            // Return to Main Menu
          printf("Meter Main Menu 6 Requested\n");
          Start_Highlights_Menu6();
          CurrentMenu=6;
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

    if (CurrentMenu == 8)  // Meter Range Menu
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
        case 2:                                            // Power Up
          ChangeRange(i);
          Draw5MeterLabels(ActiveZero, ActiveFSD);
          UpdateWindow();          
          break;
        case 3:                                            // Power Down
          ChangeRange(i);
          Draw5MeterLabels(ActiveZero, ActiveFSD);
          UpdateWindow();          
          break;
        case 4:                                            // dbm expand
          ChangeRange(i);
          Draw5MeterLabels(ActiveZero, ActiveFSD);
          UpdateWindow();
          break;
        case 5:                                            // dbm contract
          ChangeRange(i);
          Draw5MeterLabels(ActiveZero, ActiveFSD);
          UpdateWindow();
          break;
        case 6:                                            // Reset
          ChangeRange(i);
          Draw5MeterLabels(ActiveZero, ActiveFSD);
          UpdateWindow();          
          break;
        case 7:                                            // Return to Main Menu
          printf("Meter Main Menu 6 Requested\n");
          Start_Highlights_Menu6();
          CurrentMenu=6;
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
      continue;  // Completed Menu 7 action, go and wait for touch
    }
    if (CurrentMenu == 9)  // Sensor Menu
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
        case 2:                                            // ad8318-3
          ChangeSensor(i);
          Start_Highlights_Menu9();
          UpdateWindow();          
          break;
        case 3:                                            // ad8318-5
          ChangeSensor(i);
          Start_Highlights_Menu9();
          UpdateWindow();          
          break;
        case 4:                                            // 
          //ChangeSensor(i);
          //UpdateWindow();
          break;
        case 5:                                            // voltage
          ChangeSensor(i);
          Start_Highlights_Menu9();
          UpdateWindow();
          break;
        case 6:                                            // raw a-d
          ChangeSensor(i);
          Start_Highlights_Menu9();
          UpdateWindow();          
          break;
        case 7:                                            // Return to Main Menu
          printf("Meter Main Menu 6 Requested\n");
          Start_Highlights_Menu6();
          CurrentMenu=6;
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
  }
  return NULL;
}


/////////////////////////////////////////////// DEFINE THE BUTTONS ///////////////////////////////

void Define_Menu1()  // XY Main Menu
{
  int button = 0;

  button = CreateButton(1, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(1, 1);
  AddButtonStatus(button, "Main Menu", &Black);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(1, 2);
  AddButtonStatus(button, "Mode", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(1, 3);
  AddButtonStatus(button, "Markers", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(1, 4);
  AddButtonStatus(button, "Normalise", &Blue);
  AddButtonStatus(button, "Normalised", &Green);

  button = CreateButton(1, 5);
  AddButtonStatus(button, "Labels", &Blue);
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


void Define_Menu2()  // XY Markers
{
  int button = 0;

  button = CreateButton(2, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(2, 1);
  AddButtonStatus(button, "Marker^Menu", &Black);
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

void Define_Menu3()  // XY Settings
{
  int button = 0;

  button = CreateButton(3, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(3, 1);
  AddButtonStatus(button, "Labels^Menu", &Black);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(3, 2);
  AddButtonStatus(button, "Start^Freq", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(3, 3);
  AddButtonStatus(button, "Stop^Freq", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(3, 4);
  AddButtonStatus(button, "Reference^Level", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(3, 5);
  AddButtonStatus(button, "Resolution^Bandwidth", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(3, 6);
  AddButtonStatus(button, "Title", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(3, 7);
  AddButtonStatus(button, "Return to^Main Menu", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(3, 8);
  AddButtonStatus(button, "Freeze", &Blue);
  AddButtonStatus(button, "Unfreeze", &Green);
}

void Define_Menu4()  // System (XY and Meter)
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
  AddButtonStatus(button, "Normalise^Set Level", &Blue);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(4, 4);
  AddButtonStatus(button, "Shutdown^System", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(4, 5);
  AddButtonStatus(button, "Exit to^Portsdown", &Blue);
  AddButtonStatus(button, " ", &Green);

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
  if (Meter == true)  // Hide Normalise button
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 1);
  printf("Meter true\n");

  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0);
  }
}


void Define_Menu5()  // Mode (XY and Power)
{
  int button = 0;

  button = CreateButton(5, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(5, 1);
  AddButtonStatus(button, "Mode^Menu", &Black);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(5, 2);
  AddButtonStatus(button, "Power^Meter", &Blue);
  AddButtonStatus(button, "Power^Meter", &Green);

  button = CreateButton(5, 3);
  AddButtonStatus(button, "XY^Display", &Blue);
  AddButtonStatus(button, "XY^Display", &Green);

  button = CreateButton(5, 4);
  AddButtonStatus(button, "Y Plot", &Blue);
  AddButtonStatus(button, "Y Plot", &Green);

  //button = CreateButton(5, 5);
  //AddButtonStatus(button, " ", &Blue);
  //AddButtonStatus(button, " ", &Green);

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
  if (Meter == true)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 2), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 2), 0);
  }

  if ((Meter == false) && (ContScan == false))
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0);
  }

  if ((Meter == false) && (ContScan == true)) 
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 4), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 4), 0);
  }
}


void Define_Menu6()                                  // Meter (Main) Menu
{
  int button = 0;

  button = CreateButton(6, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(6, 1);
  AddButtonStatus(button, "Main Menu", &Black);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(6, 2);
  AddButtonStatus(button, "Settings", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(6, 3);
  AddButtonStatus(button, "dBm", &Blue);
  AddButtonStatus(button, "mW", &Blue);

  button = CreateButton(6, 4);
  AddButtonStatus(button, "Mode", &Blue);
  AddButtonStatus(button, "", &Green);

  button = CreateButton(6, 5);
  AddButtonStatus(button, "<-", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(6, 6);
  AddButtonStatus(button, "->", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(6, 7);
  AddButtonStatus(button, "System", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(6, 8);
  AddButtonStatus(button, "Exit to^Portsdown", &DBlue);
  AddButtonStatus(button, "Exit to^Portsdown", &Red);

  button = CreateButton(6, 9);
  AddButtonStatus(button, "Freeze", &Blue);
  AddButtonStatus(button, "Unfreeze", &Green);
}

void Start_Highlights_Menu6()
{

  if (strcmp(MeterSource, "dbm") == 0)
  {
    SetButtonStatus(ButtonNumber(6, 3), 0);
  }
  else
  {
    SetButtonStatus(ButtonNumber(6, 3), 1);
  }

  if (PortsdownExitRequested)
  {
    SetButtonStatus(ButtonNumber(6, 8), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(6, 8), 0);
  }
}


void Define_Menu7()  // Power Meter Settings
{
  int button = 0;

  button = CreateButton(7, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(7, 1);
  AddButtonStatus(button, "Settings^Menu", &Black);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(7, 2);
  AddButtonStatus(button, "External^Atten", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(7, 3);
  AddButtonStatus(button, "Cal^Factor", &Blue);

  button = CreateButton(7, 4);
  AddButtonStatus(button, "Range", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(7, 5);
  AddButtonStatus(button, "Sensor", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(7, 6);
  AddButtonStatus(button, "Title", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(7, 7);
  AddButtonStatus(button, "Return to^Main Menu", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(7, 8);
  AddButtonStatus(button, "Freeze", &Blue);
  AddButtonStatus(button, "Unfreeze", &Green);
}

void Define_Menu8()  // Power Meter Range
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
  if (strcmp(MeterSource, "dbm") == 0)
  {
    SetButtonStatus(ButtonNumber(8, 4), 0);
    SetButtonStatus(ButtonNumber(8, 5), 0);
  }
  else
  {
    SetButtonStatus(ButtonNumber(8, 4), 1);
    SetButtonStatus(ButtonNumber(8, 5), 1);
  }
}


void Define_Menu9()  // Power Meter Sensor
{
  int button = 0;

  button = CreateButton(9, 0);
  AddButtonStatus(button, "Capture^Snap", &DGrey);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(9, 1);
  AddButtonStatus(button, "Sensor^Menu", &Black);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(9, 2);
  AddButtonStatus(button, "AD8318 3v", &Blue);
  AddButtonStatus(button, "AD8318 3v", &Green);

  button = CreateButton(9, 3);
  AddButtonStatus(button, "AD8318  5v", &Blue);
  AddButtonStatus(button, "AD8318  5v", &Green);

  //button = CreateButton(9, 4);
  //AddButtonStatus(button, "Range", &Blue);
  //AddButtonStatus(button, " ", &Green);

  button = CreateButton(9, 5);
  AddButtonStatus(button, "Voltage", &Blue);
  AddButtonStatus(button, "Voltage", &Green);

  button = CreateButton(9, 6);
  AddButtonStatus(button, "Raw A-D", &Blue);
  AddButtonStatus(button, "Raw A-D", &Green);

  button = CreateButton(9, 7);
  AddButtonStatus(button, "Return to^Main Menu", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(9, 8);
  AddButtonStatus(button, "Freeze", &Blue);
  AddButtonStatus(button, "Unfreeze", &Green);
}


void Start_Highlights_Menu9()
{
  if (strcmp(MeterSensor, "ad8318-3") == 0)
  {
    SetButtonStatus(ButtonNumber(9, 2), 1);
    SetButtonStatus(ButtonNumber(9, 3), 0);
    SetButtonStatus(ButtonNumber(9, 5), 0);
    SetButtonStatus(ButtonNumber(9, 6), 0);
  }
  else if (strcmp(MeterSensor, "ad8318-5") == 0)
  {
    SetButtonStatus(ButtonNumber(9, 2), 0);
    SetButtonStatus(ButtonNumber(9, 3), 1);
    SetButtonStatus(ButtonNumber(9, 5), 0);
    SetButtonStatus(ButtonNumber(9, 6), 0);
  }
  else if (strcmp(MeterSensor, "voltage") == 0)
  {
    SetButtonStatus(ButtonNumber(9, 2), 0);
    SetButtonStatus(ButtonNumber(9, 3), 0);
    SetButtonStatus(ButtonNumber(9, 5), 1);
    SetButtonStatus(ButtonNumber(9, 6), 0);
  }
  else
  {
    SetButtonStatus(ButtonNumber(9, 2), 0);
    SetButtonStatus(ButtonNumber(9, 3), 0);
    SetButtonStatus(ButtonNumber(9, 5), 0);
    SetButtonStatus(ButtonNumber(9, 6), 1);
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
  setForeColour(255, 255, 255);                    // White text
  setBackColour(0, 0, 0);                          // on Black
  const font_t *font_ptr = &font_dejavu_sans_18;   // 18pt

  Text2(48, 463, "0 dB", font_ptr);
  Text2(30, 416, "-10 dB", font_ptr);
  Text2(30, 366, "-20 dB", font_ptr);
  Text2(30, 316, "-30 dB", font_ptr);
  Text2(30, 266, "-40 dB", font_ptr);
  Text2(30, 216, "-50 dB", font_ptr);
  Text2(30, 166, "-60 dB", font_ptr);
  Text2(30, 116, "-70 dB", font_ptr);
  Text2(30,  66, "-80 dB", font_ptr);
}

void DrawSettings()
{
  setForeColour(255, 255, 255);                    // White text
  setBackColour(0, 0, 0);                          // on Black
  const font_t *font_ptr = &font_dejavu_sans_18;   // 18pt
  char DisplayText[63];
  float ParamAsFloat;
  int line1y = 45;
  int line2y = 15;
  int titley = 5;

  // Clear the previous text first
  rectangle(100, 0, 505, 69, 0, 0, 0);
 
  if ((startfreq >= 0) && (startfreq < 10000000))  // valid and less than 10 GHz
  {
    ParamAsFloat = (float)startfreq / 1000.0;
    snprintf(DisplayText, 63, "Start %5.1f MHz", ParamAsFloat);
    Text2(100, line1y, DisplayText, font_ptr);
  }
  else if (startfreq >= 10000000)                  // valid and greater than 10 GHz
  {
    ParamAsFloat = (float)startfreq / 1000000.0;
    snprintf(DisplayText, 63, "Start %5.3f GHz", ParamAsFloat);
    Text2(100, line1y, DisplayText, font_ptr);
  }

  if ((stopfreq > 0) && (stopfreq < 10000000))  // valid and less than 10 GHz
  {
    ParamAsFloat = (float)stopfreq / 1000.0;
    snprintf(DisplayText, 63, "Stop %5.1f MHz", ParamAsFloat);
    Text2(440, line1y, DisplayText, font_ptr);
  }
  else if (stopfreq >= 10000000)                  // valid and greater than 10 GHz
  {
    ParamAsFloat = (float)stopfreq / 1000000.0;
    snprintf(DisplayText, 63, "Stop %5.3f GHz", ParamAsFloat);
    Text2(440, line1y, DisplayText, font_ptr);
  }

  if (reflevel != 99)                           // valid
  {
    snprintf(DisplayText, 63, "Ref %d dBm", reflevel);
    Text2(290, line1y, DisplayText, font_ptr);
  }

  if (rbw != 0)                                 // valid
  {
    snprintf(DisplayText, 63, "RBW %d kHz", rbw);
    Text2(100, line2y, DisplayText, font_ptr);
  }

  if (strcmp(PlotTitle, "-") != 0)
  {
    TextMid2(350, titley, PlotTitle, &font_dejavu_sans_22);
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


void DrawMeterBox()
{
    setBackColour(0, 0, 0);
    wipeScreen(0, 0, 0);
    HorizLine(100, 70, 500, 255, 255, 255);
    VertLine(100, 70, 400, 255, 255, 255);
    HorizLine(100, 470, 500, 255, 255, 255);
    VertLine(600, 70, 400, 255, 255, 255);
}


void DrawMeterArc()
{
  // Draws an anti-aliased meter arc

  float x;
  float y;
  int previous_x = 129; // Left most position of arc - 1
  float current_sin;
  float current_cos;
  float arc_angle;
//  float arc_deflection;
  int arc_deflection;
  float contrast;

//  for (arc_deflection = -1.0; arc_deflection < 999.0; arc_deflection++)
  for (arc_deflection = 0; arc_deflection < 998; arc_deflection++)
  {
    arc_angle = (float)(arc_deflection) * 2 * PI / 4000.0 - PI / 4.0;
    current_sin = sin(arc_angle);
    current_cos = cos(arc_angle);

if (arc_deflection == 999)
{
  printf("sin = %f, cos = %f for arc\n", current_sin, current_cos);
}


    x = 350 + 312 * current_sin;
    y = 90 + 312 * current_cos;

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

  float major_tick_length = 20.0;
  float minor_tick_length = 10.0;

  // Draw the major ticks.  Start at zero
  for (major_tick_number = 0; major_tick_number <= major_ticks; major_tick_number++)
  {
    tick_deflection = (major_tick_number * 1000) / major_ticks;  // for majors

    current_sin = sin((float)tick_deflection * 2.0 * PI / 4000.0 - PI / 4.0);
    current_cos = cos((float)tick_deflection * 2.0 * PI / 4000.0 - PI / 4.0);

    x1 = (int)(350.0 + 312.0 * current_sin);
    y1 = (int)(90.0 + 312.0 * current_cos);
    x2 = (int)(350.0 + (312.0 + major_tick_length) * current_sin);
    y2 = (int)(90.0 + (312.0 + major_tick_length) * current_cos);

    DrawAALine(x1, y1, x2, y2, 0, 0, 0, 255, 255, 255);

    // Draw the minor ticks.  Start at one.  Don't draw for last major tick
    if (major_tick_number < major_ticks)
    {
      for (minor_tick_number = 1; minor_tick_number <= minor_ticks; minor_tick_number++)
      {
        tick_deflection = tick_deflection + (minor_tick_number * 1000) / major_ticks / (minor_ticks + 1);

        current_sin = sin((float)tick_deflection * 2.0 * PI / 4000.0 - PI / 4.0);
        current_cos = cos((float)tick_deflection * 2.0 * PI / 4000.0 - PI / 4.0);

        x1 = (int)(350.0 + 312.0 * current_sin);
        y1 = (int)(90.0 + 312.0 * current_cos);
        x2 = (int)(350.0 + (312.0 + minor_tick_length) * current_sin);
        y2 = (int)(90.0 + (312.0 + minor_tick_length) * current_cos);

        DrawAALine(x1, y1, x2, y2, 0, 0, 0, 255, 255, 255);
      }
    }
  }
}


void DrawMeterSettings()
{
  setForeColour(255, 255, 255);                    // White text
  setBackColour(0, 0, 0);                          // on Black
  const font_t *font_ptr = &font_dejavu_sans_18;   // 18pt
  char DisplayText[511];
  int line1y = 45;
  int titley = 5;

  // Clear the previous text first
  rectangle(100, 0, 505, 69, 0, 0, 0);
 
  if ((Meteratten > 0.05) || ((int)Metercalfactor != 100))
  {
    snprintf(DisplayText, 127, "Sensor %s. Attenuator %0.1f dB. Cal Factor %d%%", MeterSensor, Meteratten, (int)Metercalfactor);
  }
  else
  {
    snprintf(DisplayText, 127, "Sensor: %s", MeterSensor);
  }
  pthread_mutex_lock(&text_lock);
  Text2(100, line1y, DisplayText, font_ptr);
  pthread_mutex_unlock(&text_lock);

  if (strcmp(MeterTitle, "-") != 0)
  {
    pthread_mutex_lock(&text_lock);
    TextMid2(350, titley, MeterTitle, &font_dejavu_sans_22);
    pthread_mutex_unlock(&text_lock);
  }
}


void Draw5MeterLabels(float LH_Value, float RH_Value)
{
  char labeltext[15];
  const font_t *font_ptr = &font_dejavu_sans_18;   // 18pt
  setBackColour(0, 0, 0);

  // Clear the previous labels
  rectangle(102, 345, 40, 30, 0, 0, 0);
  rectangle(175, 395, 40, 30, 0, 0, 0);
  rectangle(277, 430, 40, 30, 0, 0, 0);
  rectangle(387, 430, 40, 30, 0, 0, 0);
  rectangle(487, 395, 40, 30, 0, 0, 0);
  rectangle(562, 345, 38, 30, 0, 0, 0);

  if (abs(LH_Value - RH_Value) < 10.0)  // Display DP
  {
    snprintf(labeltext, 14, "%0.1f", LH_Value);
    pthread_mutex_lock(&text_lock);
    TextMid2(120, 345, labeltext, font_ptr);
    pthread_mutex_unlock(&text_lock);

    snprintf(labeltext, 14, "%0.1f", LH_Value + (RH_Value -LH_Value) / 5);
    pthread_mutex_lock(&text_lock);
    TextMid2(195, 395, labeltext, font_ptr);
    pthread_mutex_unlock(&text_lock);

    snprintf(labeltext, 14, "%0.1f", LH_Value + 2 * (RH_Value -LH_Value) / 5);
    pthread_mutex_lock(&text_lock);
    TextMid2(295, 430, labeltext, font_ptr);
    pthread_mutex_unlock(&text_lock);

    snprintf(labeltext, 14, "%0.1f", LH_Value + 3 * (RH_Value -LH_Value) / 5);
    pthread_mutex_lock(&text_lock);
    TextMid2(405, 430, labeltext, font_ptr);
    pthread_mutex_unlock(&text_lock);

    snprintf(labeltext, 14, "%0.1f", LH_Value + 4 * (RH_Value -LH_Value) / 5);
    pthread_mutex_lock(&text_lock);
    TextMid2(505, 395, labeltext, font_ptr);
    pthread_mutex_unlock(&text_lock);

    snprintf(labeltext, 14, "%0.1f", RH_Value);
    pthread_mutex_lock(&text_lock);
    TextMid2(580, 345, labeltext, font_ptr);
    pthread_mutex_unlock(&text_lock);
  }
  else
  //No DP
  {
    snprintf(labeltext, 14, "%d", (int)LH_Value);
    pthread_mutex_lock(&text_lock);
    TextMid2(120, 345, labeltext, font_ptr);
    pthread_mutex_unlock(&text_lock);

    snprintf(labeltext, 14, "%d", (int)(LH_Value + (RH_Value -LH_Value) / 5));
    pthread_mutex_lock(&text_lock);
    TextMid2(195, 395, labeltext, font_ptr);
    pthread_mutex_unlock(&text_lock);

    snprintf(labeltext, 14, "%d", (int)(LH_Value + 2 * (RH_Value -LH_Value) / 5));
    pthread_mutex_lock(&text_lock);
    TextMid2(295, 430, labeltext, font_ptr);
    pthread_mutex_unlock(&text_lock);

    snprintf(labeltext, 14, "%d", (int)(LH_Value + 3 * (RH_Value -LH_Value) / 5));
    pthread_mutex_lock(&text_lock);
    TextMid2(405, 430, labeltext, font_ptr);
    pthread_mutex_unlock(&text_lock);

    snprintf(labeltext, 14, "%d", (int)(LH_Value + 4 * (RH_Value -LH_Value) / 5));
    pthread_mutex_lock(&text_lock);
    TextMid2(505, 395, labeltext, font_ptr);
    pthread_mutex_unlock(&text_lock);

    snprintf(labeltext, 14, "%d", (int)RH_Value);
    pthread_mutex_lock(&text_lock);
    TextMid2(580, 345, labeltext, font_ptr);
    pthread_mutex_unlock(&text_lock);
  }
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
        DrawAALine(350, 90, 350 + (int)(309.0 * previous_sin), 90 + (int)(309.0 * previous_cos), 0, 0, 0, 0, 0, 0);

        // Write new position
        DrawAALine(350, 90, 350 + (int)(309.0 * current_sin), 90 + (int)(309.0 * current_cos), 0, 0, 0, 255, 255, 255);

        // Set up for overwrite next time
        previous_sin = current_sin;
        previous_cos = current_cos;
      }
    }
    usleep(20000);  // 50 Hz refresh rate
  }
  return NULL;
}


void ShowdBm(float dBm)
{
  char dBmtext[15];
  const font_t *font_ptr = &font_dejavu_sans_36;   // 36pt

  snprintf(dBmtext, 20, "%0.1f dBm", dBm);
  if (strlen(dBmtext) < 8)
  {
    strcat(dBmtext, "   ");
  }
  if (strlen(dBmtext) < 9)
  {
    strcat(dBmtext, "  ");
  }
  if (strlen(dBmtext) < 10)
  {
    strcat(dBmtext, " ");
  }

  setBackColour(0, 0, 0);
  pthread_mutex_lock(&text_lock);
  Text2(110, 80, dBmtext, font_ptr);
  pthread_mutex_unlock(&text_lock);
}


void ShowmW(float mW)
{
  char mWtext[15];
  const font_t *font_ptr = &font_dejavu_sans_36;   // 36pt

  if (mW > 100)  // Display Watts
  {
    snprintf(mWtext, 20, "%0.1f W", mW / 1000.0);
  }
  if ((mW <= 100) && (mW > 0.1)) // Display mW
  {
    snprintf(mWtext, 20, "%0.1f mW", mW);
  }
  if ((mW <= 0.1) && (mW > 0.0001))// Display uW
  {
    snprintf(mWtext, 20, "%0.1f uW", mW * 1000.0);
  }
  if (mW <= 0.0001) // Display nW
  {
    snprintf(mWtext, 20, "%0.1f nW", mW * 1000000.0);
  }

  if (strlen(mWtext) < 8)
  {
    strcat(mWtext, "   ");
  }
  if (strlen(mWtext) < 9)
  {
    strcat(mWtext, "  ");
  }
  if (strlen(mWtext) < 10)
  {
    strcat(mWtext, " ");
  }

  setBackColour(0, 0, 0);
  pthread_mutex_lock(&text_lock);
  Text2(400, 80, mWtext, font_ptr);
  pthread_mutex_unlock(&text_lock);
}


void Showraw(int raw)
{
  char rawtext[63];
  const font_t *font_ptr = &font_dejavu_sans_36;   // 36pt

  snprintf(rawtext, 30, "%d", raw);
  if (strlen(rawtext) == 1)
  {
    strcat(rawtext, "          ");
  }
  if (strlen(rawtext) == 2)
  {
    strcat(rawtext, "          ");
  }
  if (strlen(rawtext) == 3)
  {
    strcat(rawtext, "            ");
  }
  if (strlen(rawtext) == 4)
  {
    strcat(rawtext, "             ");
  }

  setBackColour(0, 0, 0);
  pthread_mutex_lock(&text_lock);
  Text2(110, 80, rawtext, font_ptr);
  pthread_mutex_unlock(&text_lock);

  // And overwrite power text
  rectangle(400, 80, 150, 50, 0, 0, 0);
}


void Showvolts(float raw)
{
  char rawtext[63];
  const font_t *font_ptr = &font_dejavu_sans_36;   // 36pt

  snprintf(rawtext, 30, "%0.2f", raw);
  if (strlen(rawtext) == 3)
  {
    strcat(rawtext, " v        ");
  }
  if (strlen(rawtext) >= 4)
  {
    strcat(rawtext, " v         ");
  }

  setBackColour(0, 0, 0);
  pthread_mutex_lock(&text_lock);
  Text2(110, 80, rawtext, font_ptr);
  pthread_mutex_unlock(&text_lock);

  // And overwrite power text
  rectangle(400, 80, 150, 50, 0, 0, 0);
}


void CheckWithinRange(int advalue)
{
  const font_t *font_ptr = &font_dejavu_sans_28;   // 28pt

  // Check for each sensor
  if (strcmp(MeterSensor, "ad8318-5") == 0)
  {
    if (advalue > ad8318_5_underrange)
    {
      pthread_mutex_lock(&text_lock);
      setBackColour(0, 0, 0);
      setForeColour(255, 63, 36);
      Text2(110, 120,"Under-range", font_ptr);
      setForeColour(255, 255, 255);
      pthread_mutex_unlock(&text_lock);
    }
    else
    {
      pthread_mutex_lock(&text_lock);
      setBackColour(0, 0, 0);
      setForeColour(0, 0, 0);
      Text2(110, 120,"Under-range", font_ptr);
      setForeColour(255, 255, 255);
      pthread_mutex_unlock(&text_lock);
    }
    if (advalue < ad8318_5_overrange)
    {
      pthread_mutex_lock(&text_lock);
      setBackColour(0, 0, 0);
      setForeColour(255, 63, 36);
      Text2(430, 120,"Over-range", font_ptr);
      setForeColour(255, 255, 255);
      pthread_mutex_unlock(&text_lock);
    }
    else
    {
      pthread_mutex_lock(&text_lock);
      setBackColour(0, 0, 0);
      setForeColour(0, 0, 0);
      Text2(430, 120,"Over-range", font_ptr);
      setForeColour(255, 255, 255);
      pthread_mutex_unlock(&text_lock);
    }
  }

  if (strcmp(MeterSensor, "ad8318-3") == 0)
  {
    if (advalue > ad8318_3_underrange)
    {
      pthread_mutex_lock(&text_lock);
      setBackColour(0, 0, 0);
      setForeColour(255, 63, 36);
      Text2(110, 120,"Under-range", font_ptr);
      setForeColour(255, 255, 255);
      pthread_mutex_unlock(&text_lock);
    }
    else
    {
      pthread_mutex_lock(&text_lock);
      setBackColour(0, 0, 0);
      setForeColour(0, 0, 0);
      Text2(110, 120,"Under-range", font_ptr);
      setForeColour(255, 255, 255);
      pthread_mutex_unlock(&text_lock);
    }
    if (advalue < ad8318_3_overrange)
    {
      pthread_mutex_lock(&text_lock);
      setBackColour(0, 0, 0);
      setForeColour(255, 63, 36);
      Text2(430, 120,"Over-range", font_ptr);
      setForeColour(255, 255, 255);
      pthread_mutex_unlock(&text_lock);
    }
    else
    {
      pthread_mutex_lock(&text_lock);
      setBackColour(0, 0, 0);
      setForeColour(0, 0, 0);
      Text2(430, 120,"Over-range", font_ptr);
      setForeColour(255, 255, 255);
      pthread_mutex_unlock(&text_lock);
    }
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
  int pixel;
  int x;
  int dispvalue;
  int rawvalue;  // 0 to 1023
  int previous_rawvalue = -1;  // 0 to 1023
  int nextwebupdate = 10;
  char response[63];

  float power_dBm;
  float power_mW;
  char valtext[15];

  bool Flyback = false;
  int x250 = -1000;

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
  
  Define_Menu41();

  ReadSavedParams();

  // Check if starting in Power Meter or XY mode
  if( argc == 1)
  {
    printf("No arguments, Starting with Power meter\n");
  }  
  else if( argc == 2 )
  {
    //printf("%d arguments, Starting with Power meter %s\n", argc, argv[1]);

    // 1 Argument provided
    if(strcmp(argv[1], "xy") == 0)
    {
      ContScan = false;
      Meter = false;
      ModeChanged = true;
      CurrentMenu = 1;
      CallingMenu = 1;
    }
  }
  else
  {
    printf("ERROR: Incorrect number of parameters!\n");
    return 0;
  }

  // Create Wait Button thread
  pthread_create (&thbutton, NULL, &WaitButtonEvent, NULL);

  // Initialise direct access to the 7 inch screen
  initScreen();
  
  normalised = false;
  while(app_exit == false)
  {
    if (Meter == true)
    {
      if (ModeChanged == true)
      {
        setBackColour(0, 0, 0);
        setForeColour(255, 255, 255);
        DrawMeterBox();
        DrawMeterArc();
        ChangeRange(0); // calculate and draw the meter units
        DrawMeterTicks(10, 1);
        Draw5MeterLabels(ActiveZero, ActiveFSD);
        DrawMeterSettings();
        Start_Highlights_Menu6();
        UpdateWindow();     // Draw the buttons

        // Start the meter movement
        if(pthread_create(&thMeter_Movement, NULL, &MeterMovement, &app_exit))
        {
          fprintf(stderr, "Error creating %s pthread\n", "MeterMovement");
          return 1;
        }
        UpdateWeb();
        nextwebupdate = 10;
        tracecount = 0;
        ModeChanged = false;
      }

      // Read the MCP3002 second input
      rawvalue = mcp3002_value(1);

      // Show A-D output for testing
      //const font_t *font_ptr = &font_dejavu_sans_18;   // 18pt
      snprintf(valtext, 8, "%d", rawvalue);

      //pthread_mutex_lock(&text_lock);
      // Text2(0, 80, valtext, font_ptr);
      //pthread_mutex_unlock(&text_lock);

      // Function here to translate MCP3002 output to useful value in dBm, mW or volts

      if (strcmp(MeterSensor, "ad8318-5") == 0)
      {
        power_dBm = ad8318_5[rawvalue].pwr_dBm;
      }

      if (strcmp(MeterSensor, "ad8318-3") == 0)
      {
        power_dBm = ad8318_3[rawvalue].pwr_dBm;
      }

      // Apply Calibration Factor correction if needed
      if (Metercalfactor != 100)
      {
        if (Metercalfactor < 5)
        {
          Metercalfactor = 100;
        }
        power_dBm = power_dBm + log10(100.0 / (float)Metercalfactor) * 10.0;
      }

      // Apply correction for external attenuator
      power_dBm = power_dBm + Meteratten;

      // Convert to linear power
      power_mW = pow(10.0, power_dBm / 10.0);

      // Now work out meter deflection (-50 to +1050) for this value

      if (strcmp(MeterSource, "dbm") == 0)
      {
       meter_deflection = (int)((power_dBm - ActiveZero) * 1000.0 / (ActiveFSD - ActiveZero));
      }
      if (strcmp(MeterSource, "mw") == 0)
      {
         meter_deflection = (int)(power_mW * 1000 / Metermwrange);
      }
      if (strcmp(MeterSensor, "raw") == 0)
      {
         meter_deflection = rawvalue;
      }
      if (strcmp(MeterSensor, "voltage") == 0)
      {
         meter_deflection = (rawvalue * 660) / 1024;
      }

      if (rawvalue != previous_rawvalue)           // Update numeric display
      {
        if (strcmp(MeterSensor, "raw") == 0)
        {
          Showraw(rawvalue);
        }
        else if (strcmp(MeterSensor, "voltage") == 0)
        {
          Showvolts(((float)(rawvalue) * 3.3) / 1024.0);
        }
        else
        {
          ShowdBm(power_dBm);
          ShowmW(power_mW);

          // And check within measurement range
          CheckWithinRange(rawvalue);
        }
      }

      while (freeze)
      {
        frozen = true;
      }
      frozen = false;

      tracecount++;
      if (tracecount >= nextwebupdate)
      {
        //printf("tracecount = %d,  \n", tracecount);
        UpdateWeb();
        usleep(10000);
        nextwebupdate = tracecount + 75;  // 
      }

      usleep(10000);
    }
    else   // XY display
    {
      if (ModeChanged == true)
      {
        //pthread_join(thMeter_Movement, NULL);
        //printf("After Join\n");
        setBackColour(0, 0, 0);
        DrawEmptyScreen();
        DrawYaxisLabels();  // dB calibration on LHS
        DrawSettings();     // Start, Stop RBW, Ref level and Title
        CurrentMenu = 1;
        Start_Highlights_Menu1();
        UpdateWindow();     // Draw the buttons
        UpdateWeb();
        tracecount = 0;
        nextwebupdate = 10;
        ModeChanged = false;
      }

      do  // Wait here for flyback in XY Display Mode (ContScan = false)
      {
        x = mcp3002_value(0) - xmin;
        //printf("X value awaiting 0 = %d\n", x);
        if (Meter == true) break;                 // Don't wait if meter selected
      }
      while (((x > 0) && (ContScan == false)) || (Meter == true));

      Flyback = false;
      do  // Wait here for first sample -------------------------------------------------
      {
        x = mcp3002_value(0) - xmin;
        //printf("X value awaiting sample 1 = %d\n", x);
        if (Meter == true) break;                 // Don't wait if meter selected
      }
      while (((x <= 1) && (ContScan == false)) || (Meter == true));

      activescan = true;
      scaledadresult[0] = ((mcp3002_value(1) * yscalenum)/ yscaleden) + yshift;
      if (normalised == false)
      {
        y[0] = scaledadresult[0];
      }
      else
      {
        y[0] = scaledadresult[0] + norm[0];
      }

      if (normalising == true)
      {
        norm[0] = normleveloffset - scaledadresult[0];
      }

      if (y[0] < 1)
      {
        y[0] = 1;
      }
      if (y[0] > 399)
      {
      y[0] = 399;
      }

      do  // Wait here for second sample ------------------------------------------------
      {
        x = mcp3002_value(0) - xmin;
        //printf("X value awaiting sample 2 = %d\n", x);
        if (Meter == true) break;                 // Don't wait if meter selected
      }
      while (((x <= 2) && (ContScan == false)) || (Meter == true));

      scaledadresult[1] = ((mcp3002_value(1) * yscalenum)/ yscaleden) + yshift;
      if (normalised == false)
      {
        y[1] = scaledadresult[1];
      }
      else
      {
        y[1] = scaledadresult[1] + norm[1];
      }

      if (normalising == true)
      {
        norm[1] = normleveloffset - scaledadresult[1];
      }

      if (y[1] < 1)
      {
        y[1] = 1;
      }
      if (y[1] > 399)
      {
        y[1] = 399;
      }
      for (pixel = 2; pixel < 500; pixel++)  // Subsequent Samples -----------------------
      {

        do  // Wait here for numbered sample
        {
          x = mcp3002_value(0) - xmin;
          //printf("X value awaiting sample %d = %d\n", pixel, x);
          if (Meter == true) break;                                 // Don't wait if meter selected

          // Deal with X ramp that does not quite make the scan end
          if (pixel == 250)
          {
            x250 = x;       // Note half-way X value
          }
          if (pixel > 400)
          {
            if (x < x250)   // So flyback has started
            {
              Flyback = true;
              //printf("Flyback = true, pixel  = %d, x = %d, x250 = %d\n", pixel, x, x250);
              break;
            }
          }
        }
        while (((((pixel * xscalenum) > (x * xscaleden)) && (ContScan == false))) || (Meter == true));

        if (Flyback == true)  // Set the trace to the baseline for the rest of the scan
        {
          y[pixel] = 1;
        }
        else                               // Measure and possibly normalise the value
        {
          dispvalue = mcp3002_value(1);
          scaledadresult[pixel] = ((dispvalue * yscalenum)/ yscaleden) + yshift;

          if (normalised == false)        
          {
            y[pixel] = scaledadresult[pixel];
          }
          else                           // Apply normalisation if required
          {
            y[pixel] = scaledadresult[pixel] + norm[pixel];
          }

          if (normalising == true)         // Measure for normalisation (One scan only)
          {
            norm[pixel] = normleveloffset - scaledadresult[pixel];
          }

          if (y[pixel] < 1)                // Constrain y
          {
            y[pixel] = 1;
          }
          if (y[pixel] > 399)
          {
            y[pixel] = 399;
          }
        }
        DrawTrace(pixel, y[pixel - 2], y[pixel - 1], y[pixel]);

	    //printf("pixel=%d, prev2=%d, prev1=%d, current=%d\n", pixel, y[pixel - 2], y[pixel - 1], y[pixel]);
        while (freeze)
        {
          frozen = true;
        }
        frozen = false;
        if (Meter == true) break;                  // Don't scan if meter selected
      }
      activescan = false;

      if (normalising == true)
      {
        finishednormalising = true;
      }

      if (markeron == true)
      {
        CalculateMarkers();
      }
      tracecount++;

      if (tracecount >= nextwebupdate)
      {
        //printf("tracecount = %d,  \n", tracecount);
        UpdateWeb();
        usleep(10000);
        nextwebupdate = tracecount + 20;  // 
      }
    }
  }
  printf("Waiting for Meter Thread to exit..\n");
  pthread_join(thMeter_Movement, NULL);
  printf("Waiting for Button Thread to exit..\n");
  pthread_join(thbutton, NULL);
  pthread_mutex_destroy(&text_lock);
}

