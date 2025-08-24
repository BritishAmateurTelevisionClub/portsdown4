// rpidatvtouch4.c
/*
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

Initial code by Evariste F5OEO
Rewitten by Dave, G8GKQ
*/
//

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <fftw3.h>
#include <getopt.h>
#include <linux/input.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <math.h>
#include <wiringPi.h>
#include <sys/stat.h> 
#include <sys/types.h> 
#include <time.h>
#include <lime/LimeSuite.h>
#include <lime/limeRFE.h>

#include "font/font.h"
#include "touch.h"
#include "Graphics.h"
#include "lmrx_utils.h"
#include "ffunc.h"
#include "timing.h"

#define KWHT  "\x1B[37m"
#define KYEL  "\x1B[33m"

#define PATH_PCONFIG "/home/pi/rpidatv/scripts/portsdown_config.txt"
#define PATH_PPRESETS "/home/pi/rpidatv/scripts/portsdown_presets.txt"
#define PATH_SGCONFIG "/home/pi/rpidatv/src/siggen/siggenconfig.txt"
#define PATH_RTLPRESETS "/home/pi/rpidatv/scripts/rtl-fm_presets.txt"
#define PATH_LOCATORS "/home/pi/rpidatv/scripts/portsdown_locators.txt"
#define PATH_RXPRESETS "/home/pi/rpidatv/scripts/rx_presets.txt"
#define PATH_STREAMPRESETS "/home/pi/rpidatv/scripts/stream_presets.txt"
#define PATH_JCONFIG "/home/pi/rpidatv/scripts/jetson_config.txt"
#define PATH_LMCONFIG "/home/pi/rpidatv/scripts/longmynd_config.txt"
#define PATH_LIME_CAL "/home/pi/rpidatv/scripts/limecalfreq.txt"
#define PATH_C_NUMBERS "/home/pi/rpidatv/scripts/portsdown_C_codes.txt"
#define PATH_BV_CONFIG "/home/pi/rpidatv/src/bandview/bandview_config.txt"
#define PATH_AS_CONFIG "/home/pi/rpidatv/src/airspyview/airspyview_config.txt"
#define PATH_RS_CONFIG "/home/pi/rpidatv/src/rtlsdrview/rtlsdrview_config.txt"
#define PATH_PB_CONFIG "/home/pi/rpidatv/src/plutoview/plutoview_config.txt"
#define PATH_SV_CONFIG "/home/pi/rpidatv/src/sdrplayview/sdrplayview_config.txt"
#define PATH_TC_CONFIG "/home/pi/rpidatv/scripts/images/testcard_config.txt"

#define PI 3.14159265358979323846
#define deg2rad(DEG) ((DEG)*((PI)/(180.0)))
#define rad2deg(RAD) ((RAD)*180/PI)
#define DELIM "."

char ImageFolder[63]="/home/pi/rpidatv/image/";

int fd = 0;                           // File Descriptor for touchscreen touch messages
int wscreen, hscreen;
float scaleXvalue, scaleYvalue;       // Coeff ratio from Screen/TouchArea
int wbuttonsize;
int hbuttonsize;

typedef struct {
	int r,g,b;
} color_t;

typedef struct {
	char Text[255];
	color_t  Color;
} status_t;

#define MAX_STATUS 10
typedef struct {
	int x,y,w,h;                   // Position and size of button
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
color_t Red   = {.r = 255, .g = 0  , .b = 0  };
color_t Black = {.r = 0  , .g = 0  , .b = 0  };

#define MAX_BUTTON 690
int IndexButtonInArray=0;
button_t ButtonArray[MAX_BUTTON];
#define TIME_ANTI_BOUNCE 500
int CurrentMenu = 1;
int CallingMenu = 1;

//GLOBAL PARAMETERS

int fec;
int SR;
char ModeInput[255];
// char freqtxt[255]; not global
char ModeAudio[255];
char ModeOutput[255];
char ModeSTD[255];
char ModeVidIP[255];
char ModeOP[255];
char Caption[255];
char CallSign[255];
char Locator[15];
char PIDstart[15];
char PIDvideo[15];
char PIDaudio[15];
char PIDpmt[15];
char ADFRef[3][15];
char CurrentADFRef[15];
char UpdateStatus[31] = "NotAvailable";
char ADF5355Ref[15];
char DisplayType[31];
char LinuxCommand[511];
int  scaledX, scaledY;

// GPIO numbers are Wiring Pi Numbers
int GPIO_PTT = 29;
int GPIO_SPI_CLK = 21;
int GPIO_SPI_DATA = 22;
// int GPIO_4351_LE = 30;  Changed to prevent confict with RPi Hat EEPROM
int GPIO_4351_LE = 23;
int GPIO_Atten_LE = 16;
int GPIO_5355_LE = 15;
// int GPIO_Band_LSB = 31;   Changed to prevent confict with RPi Hat EEPROM
int GPIO_Band_LSB = 26;
int GPIO_Band_MSB = 24;
int GPIO_Tverter = 7;
int GPIO_SD_LED = 2;

int debug_level = 0; // 0 minimum, 1 medium, 2 max
int MicLevel = 26;   // 1 to 30.  default 26

char ScreenState[255] = "NormalMenu";  // NormalMenu SpecialMenu TXwithMenu TXwithImage RXwithImage VideoOut SnapView VideoView Snap SigGen
char MenuTitle[50][127];

// Band details to be over-written by values from from portsdown_config.txt:
char TabBand[16][3] = {"d1", "d2", "d3", "d4", "d5", "t1", "t2", "t3", "t4", "t0", "t5", "t6", "t7", "t8", "d0", "d6"};
char TabBandLabel[16][15] = {"71_MHz", "146_MHz", "70_cm", "23_cm", "13_cm", "9_cm_T", "6_cm_T", "3_cm_T",
     "24_GHz_T", "50_MHz_T", "13_cm_T", "47_GHz_T", "76_GHz_T", "Spare_T", "50_MHz", "9_cm"};
float TabBandAttenLevel[16] = {-10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10};
int TabBandExpLevel[16] = {30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30};
int TabBandLimeGain[16]  = {90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90};
int TabBandMuntjacGain[16]  = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10};
int TabBandPlutoLevel[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int TabBandExpPorts[16] = {0, 1, 2, 3, 11, 4, 5, 6, 7, 12, 15, 9, 10, 14, 8, 13};
float TabBandLO[16] = {0, 0, 0, 0, 0, 2970, 5330, 9930, 23610, -94.8, 2248.5, 469440, 758320, 0, 0, 0};
char TabBandNumbers[16][10]={"1111", "2222", "3333", "4444", "5555", "2111", "0222", "2333",
                             "2444", "2000", "2555", "2666", "2777", "2888", "0000", "6666"};

// Defaults for Menus
int TabSR[9]={125,333,1000,2000,4000, 88, 250, 500, 3000};
char SRLabel[9][255]={"SR 125","SR 333","SR1000","SR2000","SR4000", "SR 88", "SR 250", "SR 500", "SR3000"};
int TabFec[5]={1, 2, 3, 5, 7};
int TabS2Fec[9]={14, 13, 12, 35, 23, 34, 56, 89, 91};
char S2FECLabel[9][7]={"1/4", "1/3", "1/2", "3/5", "2/3", "3/4", "5/6", "8/9", "9/10"};
char TabFreq[9][255]={"71", "146.5", "437", "1249", "1255", "436", "436.5", "437.5", "438"};
char FreqLabel[31][255];
char TabPresetLabel[4][15]={"-", "-", "-", "-"};
char TabModeAudio[6][15]={"auto", "mic", "video", "bleeps", "no_audio", "webcam"};
char TabModeSTD[2][7]={"6","0"};
char TabModeVidIP[2][7]={"0","1"};
char TabModeOP[15][31]={"IQ", "QPSKRF", "DATVEXPRESS", "LIMEUSB", "STREAMER", "COMPVID", \
  "DTX1", "IP", "LIMEMINI", "JLIME", "JSTREAM", "MUNTJAC", "LIMEDVB", "PLUTO"};
char TabModeOPtext[15][31]={"Portsdown", " Ugly ", "Express", "Lime USB", "BATC^Stream", "Comp Vid", \
  " DTX1 ", "IPTS out", "Lime Mini", "Jetson^Lime", "Jetson^Stream", "Muntjac", "Lime DVB", "Pluto"};
char TabAtten[4][15] = {"NONE", "PE4312", "PE43713", "HMC1119"};
char CurrentModeOP[31] = "QPSKRF";
char CurrentModeOPtext[31] = " UGLY ";
char TabTXMode[7][255] = {"DVB-S", "Carrier", "S2QPSK", "8PSK", "16APSK", "32APSK", "DVB-T"};
char CurrentTXMode[255] = "DVB-S";
char CurrentPilots[7] = "off";
char CurrentFrames[7] = "long";
//char CurrentModeInput[255] = "DESKTOP";
char TabEncoding[7][15] = {"MPEG-2", "H264", "H265", "IPTS in", "TS File", "IPTS in H264", "IPTS in H265"};
char CurrentEncoding[255] = "H264";
char TabSource[10][15] = {"Pi Cam", "CompVid", "TCAnim", "TestCard", "PiScreen", "Contest", "Webcam", "C920", "HDMI", "PC"};
char CurrentSource[15] = "PiScreen";
char TabFormat[4][15] = {"4:3", "16:9", "720p", "1080p"};
char CurrentFormat[15] = "4:3";
char CurrentCaptionState[15] = "on";
char CurrentAudioState[255] = "auto";
char CurrentAtten[255] = "NONE";
int  CurrentBand = 2; // 0 thru 15
char KeyboardReturn[64];
char FreqBtext[31];
char MenuText[5][63];
char Guard[7];
char DVBTQAM[7];
char CurrentPiCamOrientation[15] = "normal";

// Valid Input Modes:
// "CAMMPEG-2", "CAMH264", "PATERNAUDIO", "ANALOGCAM" ,"CARRIER" ,"CONTEST"
// "IPTSIN","ANALOGMPEG-2", "CARDMPEG-2", "CAMHDMPEG-2", "DESKTOP", "FILETS"
//  NOT "C920"
// "JHDMI", "JCAM", "JPC", "JCARD", "JWEBCAM", "HDMI"
// "IPTSIN264", "IPTSIN265"

// Composite Video Output variables
char TabVidSource[8][15] = {"Pi Cam", "CompVid", "TCAnim", "TestCard", "Snap", "Contest", "Webcam", "Movie"};
char CurrentVidSource[15] = "TestCard";
int VidPTT = 0;             // 0 is off, 1 is on
int CompVidBand = 2;        // Band used for Comp Vid Contest numbers
int ImageIndex = 0;         // Test Card selection number
int ImageRange = 5;         // Number of Test Cards

// Test Card switching variables
int CurrentTestCard = 0;   // Test Card F
char TestCardName[12][63] = {"tcfm", "tcc", "pm5544", "75cb", "11g"}; 
char TestCardTitle[12][63];

// RTL FM Parameters. [0] is current
char RTLfreq[10][15];       // String with frequency in MHz
char RTLlabel[10][15];      // String for label
char RTLmode[10][5];       // String for mode: fm, wbfm, am, usb, lsb
int RTLsquelch[10];         // between 0 and 1000.  0 is off
int RTLgain[10];            // between 0 (min) and 50 (max).
int RTLsquelchoveride = 1;  // if 0, squelch has been over-riden
int RTLppm = 0;             // RTL frequency correction in integer ppm
int RTLStoreTrigger = 0;    // Set to 1 if ready to store RTL preset
int RTLdetected = 0;        // Set to 1 at first entry to Menu 6 if RTL detected
int RTLactive = 0;          // Set to 1 if RTL_FM receiver running

// LeanDVB RX Parameters. [0] is current. 1-4 are presets
char RXfreq[5][15];         // String with frequency in MHz
char RXlabel[5][15];        // String for label
int RXsr[5];                // Symbol rate in K
char RXfec[5][7];           // FEC as String
int RXsamplerate[5];        // Samplerate in K. 0 = auto
int RXgain[5];              // Gain
char RXmodulation[5][15];   // Modulation
char RXencoding[5][15];     // Encoding
char RXsdr[5][15];          // SDR Type
char RXgraphics[5][7];      // Graphics on/off
char RXparams[5][7];        // Parameters on/off
char RXsound[5][7];         // Sound on/off
char RXfastlock[5][7];      // Fastlock on/off
int RXStoreTrigger = 0;     // Set to 1 if ready to store RX preset
int FinishedButton2 = 1;    // Used to control FFT
fftwf_complex *fftout=NULL; // FFT for RX
#define FFT_SIZE 256        // for RX display

// LongMynd RX Parameters. [0] is current.
int LMRXfreq[22];           // Integer frequency in kHz 0 current, 1-10 q, 11-20 t, 21 second tuner current
int LMRXsr[14];             // Symbol rate in K. 0 current, 1-6 q, 6-12 t, 13 second tuner current
int LMRXqoffset;            // Offset in kHz
char LMRXinput[2];          // Input a or b
char LMRXudpip[20];         // UDP IP address
char LMRXudpport[10];       // UDP IP port
char LMRXmode[10];          // sat or terr
char LMRXaudio[15];         // rpi or usb or hdmi
char LMRXvolts[7];          // off, v or h
char RXmod[7];              // DVB-S or DVB-T
bool VLCResetRequest = false; // Set on touchsscreen request for VLC to be reset  
int  CurrentVLCVolume = 256;  // Read from config file                   

// LongMynd RX Received Parameters for display
bool timeOverlay = false;    // Display time overlay on received metadata and snaps
time_t t;                    // current time

// Stream Display Parameters. [0] is current
char StreamAddress[9][127];  // Full rtmp address of stream
char StreamLabel[9][31];     // Button Label for stream
int IQAvailable = 1;         // Flag set to 0 if RPi audio output has been used
int StreamStoreTrigger = 0;  // Set to 1 if stream amendment needed
char StreamPlayer[7] = "OMX";// set to VLC if VLC viewer button is pressed

// Stream Output Parameters. [0] is current
char StreamURL[9][127];      // Full rtmp address of stream server (except for key)
char StreamKey[9][31];       // streamname-key for stream
int StreamerStoreTrigger = 0;   // Set to 1 if streamer amendment needed

// TS in/out parameters
char UDPOutAddr[31];
char UDPOutPort[31];
char UDPInPort[31];
char TSVideoFile[63];

// File menu parameters
char CurrentPathSelection[255] = "/home/pi/";
char CurrentFileSelection[255] = "";
char YesButtonCaption[63] = "Yes";
char NoButtonCaption[63] = "No";
bool ValidIQFileSelected = false;

// Range and Bearing Calculator Parameters
int GcBearing(const float, const float, const float, const float);
float GcDistance(const float, const float, const float, const float, const char *);
float Locator_To_Lat(char *);
float Locator_To_Lon(char *);
int CalcBearing(char *, char *);
int CalcRange(char *, char *);
bool CheckLocator(char *);

// Contest Number Management
char Site1Locator10[10];
char Site2Locator10[10];
char Site3Locator10[10];
char Site4Locator10[10];
char Site5Locator10[10];
bool AwaitingContestNumberEditSeln = false;
bool AwaitingContestNumberLoadSeln = false;
bool AwaitingContestNumberSaveSeln = false;
bool AwaitingContestNumberViewSeln = false;

// Lime Control
float LimeCalFreq = 0;    // -2 cal never, -1 = cal every time, 0 = cal next time, freq = no cal if no change
int LimeRFEState = 0;     // 0 = disabled, 1 = enabled
int LimeRFEPort  = 1;     // 1 = txrx, 2 = tx, 3 = 30MHz
int LimeRFERXAtt = 0;     // 0-7 representing 0-14dB 
int LimeNETMicroDet = 0;  // 0 = Not detected, 1 = detected.  Tested on entry to Lime Config menu
int LimeRFEMode = 0;      // 0 is RX , 1 is TX
rfe_dev_t* rfe = NULL;    // handle for LimeRFE
int RFEHWVer = -1;        // hardware version
int LimeUpsample = 1;     // Upsample setting for Lime

// QO-100 Transmit Freqs
char QOFreq[10][31] = {"2405.25", "2405.75", "2406.25", "2406.75", "2407.25", "2407.75", "2408.25", "2408.75", "2409.25", "2409.75"};
char QOFreqButts[10][31] = {"10494.75^2405.25", "10495.25^2405.75", "10495.75^2406.25", "10496.25^2406.75", "10496.75^2407.25", \
                            "10497.25^2407.75", "10497.75^2408.25", "10498.25^2408.75", "10498.75^2409.25", "10499.25^2409.75"};

// Langstone Integration variables
char StartApp[63];            // Startup app on boot
char PlutoIP[63];             // Portsdown Pluto IP address
char LangstonePlutoIP[63];    // Langstone Pluto IP address
char langstone_version[31] = "none";


// Touch display variables
int Inversed=0;               //Display is inversed (Waveshare=1)
int PresetStoreTrigger = 0;   //Set to 1 if awaiting preset being stored
int FinishedButton = 0;       // Used to indicate screentouch during TX or RX
int touch_response = 0;       // set to 1 on touch and used to reboot display if it locks up
int TouchX;
int TouchY;
int TouchPressure;
int TouchTrigger = 0;
bool touchneedsinitialisation = true;
bool FalseTouch = false;     // used to simulate a screen touch if a monitored event finishes
bool boot_to_tx = false;
bool boot_to_rx = false;

// Web Control globals
bool webcontrol = false;               // Enables remote control of touchscreen functions
char ProgramName[255];                 // used to pass rpidatvgui char string to listener
int *web_x_ptr;                        // pointer
int *web_y_ptr;                        // pointer
int web_x;                             // click x 0 - 799 from left
int web_y;                             // click y 0 - 480 from top
bool webclicklistenerrunning = false;  // Used to only start thread if required
char WebClickForAction[7] = "no";      // no/yes
bool touchscreen_present = true;       // detected on startup; used to control menu availability
bool reboot_required = false;          // used after hdmi display change
bool mouse_active = false;             // set true after first movement of mouse
bool MouseClickForAction = false;      // set true on left click of mouse
int mouse_x;                           // click x 0 - 799 from left
int mouse_y;                           // click y 0 - 479 from top
bool image_complete = true;            // prevents mouse image buffer from being copied until image is complete
bool mouse_connected = false;          // Set true if mouse detected at startup


// Threads for Touchscreen monitoring

pthread_t thbutton;         //
pthread_t thview;           //
pthread_t thwait3;          //  Used to count 3 seconds for WebCam reset after transmit
pthread_t thwebclick;       //  Listens for clicks from web interface
pthread_t thtouchscreen;    //  listens to the touchscreen   
pthread_t thrfe15;          //  Turns LimeRFE on after 15 seconds
pthread_t thbuttonFileVLC;  //  Handles touches during VLC play from file
pthread_t thbuttonIQPlay;   //  Handles touches to stop IQ player
pthread_t thmouse;          //  Listens to the mouse

// ************** Function Prototypes **********************************//

void GetConfigParam(char *PathConfigFile, char *Param, char *Value);
void SetConfigParam(char *PathConfigFile, char *Param, char *Value);
void strcpyn(char *outstring, char *instring, int n);
void DisplayHere(char *DisplayCaption);
void GetPiAudioCard(char card[15]);
void GetMicAudioCard(char mic[15]);
void GetHDMIAudioCard(char hdmi[15]);
void GetPiCamDev(char picamdev[15]);
void togglescreentype();
int GetLinuxVer();
void GetIPAddr(char IPAddress[256]);
void GetIPAddr2(char IPAddress[256]);
void Get_wlan0_IPAddr(char IPAddress[255]);
void GetSWVers(char SVersion[256]);
void GetLatestVers(char LatestVersion[256]);
int CheckGoogle();
int CheckJetson();
int valid_digit(char *ip_str);
int is_valid_ip(char *ip_str);
void DisplayUpdateMsg(char* Version, char* Step);
void PrepSWUpdate();
void ExecuteUpdate(int NoButton);
void LimeFWUpdate(int button);
int CheckMouse();
void GetGPUTemp(char GPUTemp[256]);
void GetCPUTemp(char CPUTemp[256]);
void GetThrottled(char Throttled[256]);
void SetAudioLevels();
void ReadModeInput(char coding[256], char vsource[256]);
void ReadModeOutput(char Moutput[256]);
int file_exist (char *filename);
void ReadModeEasyCap();
void ReadPiCamOrientation();
void ReadCaptionState();
void ReadTestCardState();
void ReadAudioState();
void ReadWebControl();
void ReadAttenState();
void ReadBand();
void ReadBandDetails();
void ReadCallLocPID();
void ReadLangstone();
void ReadTSConfig();
void ReadADFRef();
void GetSerNo(char SerNo[256]);
int CalcTSBitrate();
void GetDevices(char DeviceName1[256], char DeviceName2[256]);
void GetUSBVidDev(char VidDevName[256]);
int CheckCamLink4K();
int DetectLogitechWebcam();
int CheckC920();
int CheckC920Type();
int CheckLangstonePlutoIP();
void InitialiseGPIO();
void ReadPresets();
int CheckRTL();
int CheckMuntjac();
int RegisterMuntjac();
int CheckTuner();
void ChangeMicGain();
void SaveRTLPreset(int PresetButton);
void RecallRTLPreset(int PresetButton);
void ChangeRTLFreq();
void ChangeRTLSquelch();
void ChangeRTLGain();
void ChangeRTLppm();
int DetectLimeNETMicro();
void ReadRTLPresets();
void ReadRXPresets();
void ReadLMRXPresets();
void ChangeLMRXIP();
void ChangeLMRXPort();
void ChangeLMRXOffset();
void ChangeLMTST();
void ChangeLMSW();
void ChangeLMChan();
void AutosetLMRXOffset();
void SaveCurrentRX();
void SelectRTLmode(int NoButton);
void RTLstart();
void RTLstop();
void ReadStreamPresets();
int CheckExpressConnect();
int CheckExpressRunning();
int StartExpressServer();
int DetectUSBAudio();
void CheckExpress();
int CheckAirspyConnect();
int CheckLimeMiniConnect();
int CheckLimeMiniV2Connect();
int CheckLimeUSBConnect();
int CheckPlutoConnect();
int CheckPlutoIPConnect();
int CheckPlutoUSBConnect();
int GetPlutoXO();
int GetPlutoAD();
int GetPlutoCPU();
void CheckPlutoReady();
void CheckLimeReady();
void LimeInfo();
int LimeGWRev();
int LimeGWVer();
int LimeFWVer();
int LimeHWVer();
void LimeMiniTest();
void LimeUtilInfo();
void SetLimeUpsample();
int CheckSDRPlay();
void ClearMenuMessage();
void *WaitButtonFileVLC(void * arg);
void ShowVideoFile(char *VideoPath, char *VideoFile);
void ShowImageFile(char *ImagePath, char *ImageFile);
void ListText(char *TextPath, char *TextFile);
void FileOperation(int button);
void *WaitButtonIQPlay(void * arg);
void IQFileOperation(int button);
void ListUSBDevices();
int USBmounted();
int USBDriveDevice();
void ListNetDevices();
void ListNetPis();
void DisplayLogo();
void TransformTouchMap(int x, int y);
int IsMenuButtonPushed(int x,int y);
int IsImageToBeChanged(int x,int y);
int InitialiseButtons();
int ButtonNumber(int MenuIndex, int Button);
int CreateButton(int MenuIndex, int ButtonPosition);
int AddButtonStatus(int ButtonIndex,char *Text,color_t *Color);
void AmendButtonStatus(int ButtonIndex, int ButtonStatusIndex, char *Text, color_t *Color);
void DrawButton(int ButtonIndex);
void SetButtonStatus(int ButtonIndex,int Status);
int openTouchScreen(int NoDevice);
int getTouchScreenDetails(int *screenXmin,int *screenXmax,int *screenYmin,int *screenYmax);
int getTouchSampleThread(int *rawX, int *rawY, int *rawPressure);
int getTouchSample(int *rawX, int *rawY, int *rawPressure);
void *WaitTouchscreenEvent(void * arg);
void *WaitMouseEvent(void * arg);
void handle_mouse();
void *WebClickListener(void * arg);
void parseClickQuerystring(char *query_string, int *x_ptr, int *y_ptr);
FFUNC touchscreenClick(ffunc_session_t * session);
void togglewebcontrol();
void UpdateWeb();
void ShowMenuText();
void ShowTitle();
void UpdateWindow();
void ApplyTXConfig();
void EnforceValidTXMode();
void EnforceValidFEC();
int ListFilestoArray(char Path[255], int FirstFile, int LastFile, char FileArray[100][255], char FileTypeArray[101][2]);
int SelectFileUsingList(char *InitialPath, char *InitialFile, char *SelectedPath, char *SelectedFile, int directory);
int SelectFromList(int CurrentSelection, char ListEntry[100][63], int ListLength);
int CheckWifiEnabled();
int CheckWifiConnection(char Network_SSID[63]);
void WiFiConfig(int NoButton);
void GreyOut1();
void GreyOutReset11();
void GreyOut11();
void GreyOut12();
void GreyOut15();
void GreyOutReset25();
void GreyOut25();
void GreyOutReset42();
void GreyOut42();
void GreyOutReset44();
void GreyOut44();
void GreyOut45();
void SelectInGroup(int StartButton,int StopButton,int NoButton,int Status);
void SelectInGroupOnMenu(int Menu, int StartButton, int StopButton, int NumberButton, int Status);
void SelectTX(int NoButton);
void SelectPilots();
void SelectFrames();
void SelectEncoding(int NoButton);
void SelectOP(int NoButton);
void SelectFormat(int NoButton);
void SelectSource(int NoButton);
void SelectFreq(int NoButton);
void SelectSR(int NoButton);
void SelectFec(int NoButton);
void SelectLMSR(int NoButton);
void SelectLMFREQ(int NoButton);
void ResetLMParams();
void SelectS2Fec(int NoButton);
void SelectPTT(int NoButton,int Status);
void ChangeTestCard(int NoButton);
void SelectSTD(int NoButton);
void SelectGuard(int NoButton);
void SelectQAM(int NoButton);
void ChangeBandDetails(int NoButton);
void DoFreqChange();
void SelectBand(int NoButton);
void SelectVidIP(int NoButton);
void SelectAudio(int NoButton);
void SelectAtten(int NoButton);
void SetAttenLevel();
void SetDeviceLevel();
void AdjustVLCVolume(int adjustment);
void SetReceiveLOFreq(int NoButton);
void SavePreset(int PresetButton);
void RecallPreset(int PresetButton);
void SelectVidSource(int NoButton);
void ReceiveLOStart();
void CompVidInitialise();
void CompVidStart();
void CompVidStop();
void TransmitStart();
void *Wait3Seconds(void * arg);
void TransmitStop();
void *WaitButtonEvent(void * arg);
void *WaitButtonVideo(void * arg);
void *WaitButtonSnap(void * arg);
void *WaitButtonLMRX(void * arg);
void *WaitButtonStream(void * arg);
int CheckStream();
int CheckVLCStream();
void DisplayStream(int NoButton);
void DisplayStreamVLC(int NoButton);
void AmendStreamPreset(int NoButton);
void SelectStreamer(int NoButton);
void SeparateStreamKey(char streamkey[127], char streamname[63], char key[63]);
void AmendStreamerPreset(int NoButton);
void checkTunerSettings();
void LMRX(int NoButton);
void CycleLNBVolts();
void wait_touch();
void MsgBox(char *message);
void MsgBox2(char *message1, char *message2);
void MsgBox4(char *message1, char *message2, char *message3, char *message4);
void YesNo(int i);
void InfoScreen();
void RangeBearing();
void BeaconBearing();
void AmendBeacon(int i);
int CalcBearing(char *myLocator, char *remoteLocator);
int CalcRange(char *myLocator, char *remoteLocator);
bool CheckLocator(char *Loc);
float Locator_To_Lat(char *Loc);
float Locator_To_Lon(char *Loc);
float GcDistance( const float lat1, const float lon1, const float lat2, const float lon2, const char *unit );
int GcBearing( const float lat1, const float lon1, const float lat2, const float lon2 );
void rtl_tcp();
void do_snap();
void do_snapcheck();
static void cleanexit(int exit_code);
void do_Langstone();
void do_video_monitor(int button);
void MonitorStop();
void IPTSConfig(int NoButton);
void Keyboard(char RequestText[64], char InitText[64], int MaxLength);
void ChangePresetFreq(int NoButton);
void ChangeLMPresetFreq(int NoButton);
void ChangePresetSR(int NoButton);
void ChangeLMPresetSR(int NoButton);
void ChangeCall();
void ChangeLocator();
void ReadContestSites();
void ManageContestCodes(int NoButton);
void ChangeStartApp(int NoButton);
void ChangePlutoIP();
void ChangePlutoIPLangstone();
void ChangePlutoXO();
void ChangePlutoAD();
void ChangePlutoCPU();
void RebootPluto(int context);
void CheckPlutoFirmware();
void UpdateLangstone(int version_number);
void InstallLangstone(int NoButton);
void ChangeADFRef(int NoButton);
void ChangePID(int NoButton);
void ToggleLimeRFE();
void SetLimeRFERXAtt();
void LimeRFEInit();
void LimeRFETX();
void LimeRFERX();
void LimeRFEClose();
void ChangeJetsonIP();
void ChangeLKVIP();
void ChangeLKVPort();
void ChangeJetsonUser();
void ChangeJetsonPW();
void ChangeJetsonRPW();
void waituntil(int w,int h);
void Define_Menu1();
void Start_Highlights_Menu1();
void Define_Menu2();
void Start_Highlights_Menu2();
void Define_Menu3();
void Start_Highlights_Menu3();
void Define_Menu4();
void Start_Highlights_Menu4();
void Define_Menu5();
void Start_Highlights_Menu5();
void Define_Menu6();
void Start_Highlights_Menu6();
void Define_Menu7();
void Start_Highlights_Menu7();
void Define_Menu8();
void Start_Highlights_Menu8();
void Define_Menu9();
void Start_Highlights_Menu9();
void Define_Menu10();
void Start_Highlights_Menu10();
void Define_Menu11();
void Start_Highlights_Menu11();
void Define_Menu12();
void Start_Highlights_Menu12();
void Define_Menu13();
void Start_Highlights_Menu13();
void Define_Menu14();
void Start_Highlights_Menu14();
void Define_Menu15();
void Start_Highlights_Menu15();
void Define_Menu16();
void MakeFreqText(int index);
void Start_Highlights_Menu16();
void Define_Menu17();
void Start_Highlights_Menu17();
void Define_Menu18();
void Start_Highlights_Menu18();
void Define_Menu19();
void Start_Highlights_Menu19();
void Define_Menu20();
void Start_Highlights_Menu20();
void Define_Menu21();
void Start_Highlights_Menu21();
void Define_Menu22();
void Start_Highlights_Menu22();
void Define_Menu23();
void Start_Highlights_Menu23();
void Define_Menu24();
void Start_Highlights_Menu24();
void Define_Menu25();
void Start_Highlights_Menu25();
void Define_Menu26();
void Start_Highlights_Menu26();
void Define_Menu27();
void Start_Highlights_Menu27();
void Define_Menu28();
void Start_Highlights_Menu28();
void Define_Menu29();
void Start_Highlights_Menu29();
void Define_Menu30();
void Start_Highlights_Menu30();
void Define_Menu31();
void Start_Highlights_Menu31();
void Define_Menu32();
void Start_Highlights_Menu32();
void Define_Menu33();
void Start_Highlights_Menu33();
void Define_Menu34();
void Start_Highlights_Menu34();
void Define_Menu35();
void Start_Highlights_Menu35();
void Define_Menu36();
void Start_Highlights_Menu36();
void Define_Menu37();
void Start_Highlights_Menu37();
void Define_Menu38();
void Start_Highlights_Menu38();
void Define_Menu39();
void Start_Highlights_Menu39();
void Define_Menu40();
void Start_Highlights_Menu40();
void Define_Menu42();
void Start_Highlights_Menu42();
void Define_Menu43();
void Start_Highlights_Menu43();
void Define_Menu44();
void Start_Highlights_Menu44();
void Define_Menu45();
void Start_Highlights_Menu45();
void Define_Menu46();
void Start_Highlights_Menu46();
void Define_Menu47();
void Start_Highlights_Menu47();
void Define_Menu41();

// **************************************************************************** //

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
  char ErrorMessage1[255];
  strcpy(ParamWithEquals, Param);
  strcat(ParamWithEquals, "=");
  int file_test = 0;

  file_test = file_exist(PathConfigFile);  // Display error if file does not exist
  if (file_test == 1)
  {
    usleep(100000);           // Pause to let screen initialise
    setBackColour(0, 0, 0);   // Overwrite Portsdown Logo
    MsgBox4("Error: Config File not found:", PathConfigFile, "Restore manually or by factory reset", 
            "Touch Screen to Continue");
    wait_touch();
    strcpy(Value, " ");
    return;
  }

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
    if (debug_level == 2)
    {
      printf("Get Config reads %s for %s and returns %s\n", PathConfigFile, Param, Value);
    }
  }
  else
  {
    printf("Config file not found \n");
  }
  fclose(fp);

  if (strlen(Value) == 0)  // Display error if parameter undefined
  {  
    usleep(100000);           // Pause to let screen initialise
    setBackColour(0, 0, 0);   // Overwrite Portsdown Logo

    snprintf(ErrorMessage1, 63, "Error: Undefined parameter %s in file:", Param);

    MsgBox4(ErrorMessage1, PathConfigFile, "Restore manually or by factory reset", 
            "Touch Screen to Continue");
    wait_touch();
  }
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
  char ParamWithEquals[255];
  char ErrorMessage1[255];

  if (debug_level == 2)
  {
    printf("Set Config called %s %s %s\n", PathConfigFile , ParamWithEquals, Value);
  }

  if (strlen(Value) == 0)  // Don't write empty values
  {
    setBackColour(0, 0, 0);   // Overwrite Portsdown Logo
    snprintf(ErrorMessage1, 63, "Error: Parameter %s in file:", Param);
    MsgBox4(ErrorMessage1, PathConfigFile, "Would have no value. Try again.", 
            "Touch Screen to Continue");
    wait_touch();
    return;
  }

  strcpy(BackupConfigName, PathConfigFile);
  strcat(BackupConfigName, ".bak");
  FILE *fp=fopen(PathConfigFile, "r");
  FILE *fw=fopen(BackupConfigName, "w+");
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
 * @brief safely copies n characters of instring to outstring without overflow
 *
 * @param *outstring
 * @param *instring
 * @param n int number of characters to copy.  Max value is the outstring array size -1
 *
 * @return void
*******************************************************************************/
void strcpyn(char *outstring, char *instring, int n)
{
  //printf("\ninstring= -%s-, instring length = %d, desired length = %d\n", instring, strlen(instring), strnlen(instring, n));
  
  n = strnlen(instring, n);
  int i;
  for (i = 0; i < n; i = i + 1)
  {
    //printf("i = %d input character = %c\n", i, instring[i]);
    outstring[i] = instring[i];
  }
  outstring[n] = '\0'; // Terminate the outstring
  //printf("i = %d input character = %c\n", n, instring[n]);
  //printf("i = %d input character = %c\n\n", (n + 1), instring[n + 1]);

  //for (i = 0; i < n; i = i + 1)
  //{
  //  printf("i = %d output character = %c\n", i, outstring[i]);
  //}  
  //printf("i = %d output character = %c\n", n, outstring[n]);
  //printf("i = %d output character = %c\n", (n + 1), outstring[n + 1]);

  //printf("outstring= -%s-, length = %d\n\n", outstring, strlen(outstring));
}

void DisplayHere(char *DisplayCaption)
{
  // Displays a caption on top of the standard logo
  char ConvertCommand[255];

  system("sudo killall fbi >/dev/null 2>/dev/null");  // kill any previous fbi processes
  system("rm /home/pi/tmp/captionlogo.png >/dev/null 2>/dev/null");
  system("rm /home/pi/tmp/streamcaption.png >/dev/null 2>/dev/null");

  strcpy(ConvertCommand, "convert -font \"FreeSans\" -size 720x80 xc:transparent -fill white -gravity Center -pointsize 40 -annotate 0 \"");
  strcat(ConvertCommand, DisplayCaption);
  strcat(ConvertCommand, "\" /home/pi/tmp/captionlogo.png");
  system(ConvertCommand);

  strcpy(ConvertCommand, "convert /home/pi/rpidatv/scripts/images/BATC_Black.png /home/pi/tmp/captionlogo.png ");
  strcat(ConvertCommand, "-geometry +0+475 -composite /home/pi/tmp/streamcaption.png");
  system(ConvertCommand);

  system("sudo fbi -T 1 -noverbose -a /home/pi/tmp/streamcaption.png  >/dev/null 2>/dev/null");  // Add logo image
  refreshMouseBackground();
  draw_cursor_foreground(mouse_x, mouse_y);
  UpdateWeb();
}


/***************************************************************************//**
 * @brief Looks up the card number for the RPi Audio Card
 *
 * @param card (str) as a single character string with no <CR>
 *
 * @return void
*******************************************************************************/

void GetPiAudioCard(char card[15])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("aplay -l | grep bcm2835 | head -1 | cut -c6-6", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(card, 7, fp) != NULL)
  {
    sprintf(card, "%d", atoi(card));
  }

  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief Looks up the card number for the USB Microphone Card
 *
 * @param mic (str) as a single character string with no <CR>
 *
 * @return void
*******************************************************************************/

void GetMicAudioCard(char mic[15])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("cat /proc/asound/modules | grep \"usb_audio\" | head -c 2 | tail -c 1", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(mic, 7, fp) != NULL)
  {
    sprintf(mic, "%d", atoi(mic));
  }

  /* close */
  pclose(fp);
}


/***************************************************************************//**
 * @brief Looks up the card number for the first HDMI audio output
 *
 * @param hdmi (str) as a single character string with no <CR>
 *
 * @return void
*******************************************************************************/

void GetHDMIAudioCard(char hdmi[15])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("cat /proc/asound/modules | grep \"snd_bcm2835\" | head -c 2 | tail -c 1", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(hdmi, 7, fp) != NULL)
  {
    sprintf(hdmi, "%d", atoi(hdmi));
  }

  /* close */
  pclose(fp);
}


/***************************************************************************//**
 * @brief Looks up the Pi Cam device name
 *
 * @param picamdev(str) Device name with no CR (/dev/videon)
 *
 * @return void
*******************************************************************************/
void GetPiCamDev(char picamdev[15])
{
  FILE *fp;
  int linecount = 0;
  char result[15];

  /* Open the command for reading. */
  fp = popen("v4l2-ctl --list-devices 2> /dev/null |sed -n '/mmal/,/dev/p' | grep 'dev' | tr -d '\t'", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while ((fgets(result, 12, fp) != NULL) && (linecount == 0))
  {
    strcpy(picamdev, result);
    //printf("%s\n", picamdev);
    linecount = 1;
  }

  /* close */
  pclose(fp);
}

void togglescreentype()
{
  if (strcmp(DisplayType, "Element14_7") == 0)
  {
    strcpy(DisplayType, "dfrobot5");
  }
  else
  {
    strcpy(DisplayType, "Element14_7");
  }
  SetConfigParam(PATH_PCONFIG, "display", DisplayType);
}

/***************************************************************************//**
 * @brief Looks up the current Linux Version
 *
 * @param nil
 *
 * @return 8 for Jessie, 9 for Stretch, 10 for Buster
*******************************************************************************/

int GetLinuxVer()
{
  FILE *fp;
  char version[7];
  int ver = 0;

  /* Open the command for reading. */
  fp = popen("cat /etc/issue | grep -E -o \"8|9|10\"", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(version, 6, fp) != NULL)
  {
    //printf("%s", version);
  }

  /* close */
  pclose(fp);

  if ((atoi(version) == 8) || (atoi(version) == 9) || (atoi(version) == 10))
  {
    ver = atoi(version);
  }
  return ver;
}


/***************************************************************************//**
 * @brief Looks up the current IPV4 address
 *
 * @param IPAddress (str) IP Address to be passed as a string
 *
 * @return void
*******************************************************************************/

void GetIPAddr(char IPAddress[256])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("ifconfig | grep -Eo \'inet (addr:)?([0-9]*\\.){3}[0-9]*\' | grep -Eo \'([0-9]*\\.){3}[0-9]*\' | grep -v \'127.0.0.1\' | head -1", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(IPAddress, 17, fp) != NULL)
  {
    //printf("%s", IPAddress);
  }

  /* close */
  pclose(fp);
}


/***************************************************************************//**
 * @brief Looks up the current second IPV4 address
 *
 * @param IPAddress (str) IP Address to be passed as a string
 *
 * @return void
*******************************************************************************/

void GetIPAddr2(char IPAddress[256])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("ifconfig | grep -Eo \'inet (addr:)?([0-9]*\\.){3}[0-9]*\' | grep -Eo \'([0-9]*\\.){3}[0-9]*\' | grep -v \'127.0.0.1\' | sed -n '2 p'", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(IPAddress, 17, fp) != NULL)
  {
    //printf("%s", IPAddress);
  }

  /* close */
  pclose(fp);
}


/***************************************************************************//**
 * @brief Looks up the current wireless (wlan0) IPV4 address
 *
 * @param IPAddress (str) IP Address to be passed as a string
 *
 * @return void
*******************************************************************************/

void Get_wlan0_IPAddr(char IPAddress[255])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("ifconfig | grep -A1 \'wlan0\' | grep -Eo \'inet (addr:)?([0-9]*\\.){3}[0-9]*\' | grep -Eo \'([0-9]*\\.){3}[0-9]*\' | grep -v \'127.0.0.1\' ", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(IPAddress, 16, fp) != NULL)
  {
    //printf("%s", IPAddress);
  }

  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief Looks up the current Software Version
 *
 * @param SVersion (str) IP Address to be passed as a string
 *
 * @return void
*******************************************************************************/

void GetSWVers(char SVersion[256])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("cat /home/pi/rpidatv/scripts/installed_version.txt", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(SVersion, 16, fp) != NULL)
  {
    //printf("%s", SVersion);
  }

  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief Looks up the latest Software Version from file
 *
 * @param SVersion (str) IP Address to be passed as a string
 *
 * @return void
*******************************************************************************/

void GetLatestVers(char LatestVersion[256])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("cat /home/pi/rpidatv/scripts/latest_version.txt", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(LatestVersion, 16, fp) != NULL)
  {
    //printf("%s", LatestVersion);
  }

  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief Checks whether a ping to google on 8.8.4.4 works
 *
 * @param nil
 *
 * @return 0 if it pings OK, 1 if it doesn't
*******************************************************************************/

int CheckGoogle()
{
  FILE *fp;
  char response[127];

  /* Open the command for reading. */
  fp = popen("ping 8.8.4.4 -c1 | head -n 5 | tail -n 1 | grep -o \"1 received,\" | head -c 11", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response, 12, fp) != NULL)
  {
    printf("%s", response);
  }
  //  printf("%s", response);
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
 * @brief Checks whether a ping to a connected Jetson works
 *
 * @param nil
 *
 * @return 0 if it pings OK, 1 if it doesn't
*******************************************************************************/

int CheckJetson()
{
  FILE *fp;
  char response[127];
  char pingcommand[127];
  
  strcpy(pingcommand, "timeout 0.1 ping ");
  GetConfigParam(PATH_JCONFIG, "jetsonip", response);
  strcat(pingcommand, response);
  strcat(pingcommand, " -c1 | head -n 5 | tail -n 1 | grep -o \"1 received,\" | head -c 11");

  /* Open the command for reading. */
  fp = popen(pingcommand, "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response, 12, fp) != NULL)
  {
    printf("%s", response);
  }
  //  printf("%s", response);
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
 * @brief int is_valid_ip(char *ip_str) Checks whether an IP address is valid
 *
 * @param char *ip_str (which gets mangled)
 *
 * @return 1 if IP string is valid, else return 0
*******************************************************************************/

/* return 1 if string contain only digits, else return 0 */
int valid_digit(char *ip_str) 
{ 
  while (*ip_str) 
  { 
    if (*ip_str >= '0' && *ip_str <= '9')
    { 
      ++ip_str; 
    }
    else
    {
      return 0; 
    }
  } 
  return 1; 
} 

int is_valid_ip(char *ip_str) 
{ 
  int num, dots = 0; 
  char *ptr; 
  
  if (ip_str == NULL) 
  {
    return 0; 
  }

  ptr = strtok(ip_str, DELIM); 
  if (ptr == NULL) 
  {
    return 0; 
  }
  
  while (ptr)
  { 
    // after parsing string, it must contain only digits
    if (!valid_digit(ptr)) 
    {
      return 0; 
    }  
    num = atoi(ptr); 
  
    // check for valid numbers
    if (num >= 0 && num <= 255)
    { 
      // parse remaining string
      ptr = strtok(NULL, DELIM); 
      if (ptr != NULL)
      {
        ++dots; 
      }
    }
    else
    {
      return 0; 
    }
  }

  // valid IP string must contain 3 dots
  if (dots != 3)
  { 
    return 0;
  }
  return 1;
} 

/***************************************************************************//**
 * @brief Displays a splash screen with update progress
 *
 * @param char Version (Latest or Developement), char Step (centre message)
 *
 * @return void
*******************************************************************************/

void DisplayUpdateMsg(char* Version, char* Step)
{
  // Delete any old image
  strcpy(LinuxCommand, "rm /home/pi/tmp/update.jpg >/dev/null 2>/dev/null");
  system(LinuxCommand);

  // Build and run the convert command for the image
  strcpy(LinuxCommand, "convert -font \"FreeSans\" -size 800x480 xc:white ");

  strcat(LinuxCommand, "-gravity North -pointsize 40 -annotate 0 ");
  strcat(LinuxCommand, "\"Updating Portsdown Software\\nTo ");
  strcat(LinuxCommand, Version);
  strcat(LinuxCommand, " Version\" ");
 
  strcat(LinuxCommand, "-gravity Center -pointsize 50 -annotate 0 \"");
  strcat(LinuxCommand, Step);
  strcat(LinuxCommand, "\\n\\nPlease wait\" ");

  strcat(LinuxCommand, "-gravity South -pointsize 50 -annotate 0 ");
  strcat(LinuxCommand, "\"DO NOT TURN POWER OFF\" ");

  strcat(LinuxCommand, "/home/pi/tmp/update.jpg");

  system(LinuxCommand);

  // Display the image on the desktop
  strcpy(LinuxCommand, "sudo fbi -T 1 -noverbose -a /home/pi/tmp/update.jpg ");
  strcat(LinuxCommand, ">/dev/null 2>/dev/null");
  system(LinuxCommand);

  // Kill fbi
  strcpy(LinuxCommand, "(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
  system(LinuxCommand);
}

/***************************************************************************//**
 * @brief checks whether software update available
 *
 * @param 
 *
 * @return void
*******************************************************************************/

void PrepSWUpdate()
{
  char CurrentVersion[255];
  char LatestVersion[255];
  char CurrentVersion9[10];
  char LatestVersion9[10];

  strcpy(UpdateStatus, "NotAvailable");

  // delete old latest version file
  system("rm /home/pi/rpidatv/scripts/latest_version.txt  >/dev/null 2>/dev/null");

  // Download new latest version file
  strcpy(LinuxCommand, "wget -4 --timeout=2 https://raw.githubusercontent.com/BritishAmateurTelevisionClub/");
  strcat(LinuxCommand, "portsdown4/master/scripts/latest_version.txt ");
  strcat(LinuxCommand, "-O /home/pi/rpidatv/scripts/latest_version.txt  >/dev/null 2>/dev/null");
  system(LinuxCommand);

  // Fetch the current and latest versions and make sure we have 9 characters
  GetSWVers(CurrentVersion);
  strcpyn(CurrentVersion9, CurrentVersion, 9);
  GetLatestVers(LatestVersion);
  strcpyn(LatestVersion9, LatestVersion, 9);
  snprintf(MenuText[0], 40, "Current Software Version: %s", CurrentVersion9);

  // Clear the message lines
  strcpy(MenuText[1], " ");
  strcpy(MenuText[2], " ");
  strcpy(MenuText[3], " ");
  strcpy(MenuText[4], " ");

  // Check latest version starts with 20*
  if ( !((LatestVersion9[0] == 50) && (LatestVersion9[1] == 48)) )
  {
    // Invalid response from GitHub.  Check Google ping
    if (CheckGoogle() == 0)
    {
      strcpy(MenuText[2], "Unable to contact GitHub for update");
      strcpy(MenuText[3], "Internet connection to Google seems OK");
      strcpy(MenuText[4], "Please check BATC Wiki FAQ for advice");
    }
    else
    {
      strcpy(MenuText[2], "Unable to contact GitHub for update");
      strcpy(MenuText[3], "There appears to be no Internet connection");
      strcpy(MenuText[4], "Please check your connection and try again");
    }
  }
  else
  {
    snprintf(MenuText[1], 40, "Latest Software Version:   %s", LatestVersion9);

    // Compare versions
    if (atoi(LatestVersion9) > atoi(CurrentVersion9))
    {
      strcpy(MenuText[3], "A software update is available");
      strcpy(MenuText[4], "Do you want to update now?");
      strcpy(UpdateStatus, "NormalUpdate");
    }
    if (atoi(LatestVersion9) == atoi(CurrentVersion9))
    {
      strcpy(MenuText[2], "Your software is the latest version");
      strcpy(MenuText[3], "There is no need to update");
      strcpy(MenuText[4], "Do you want to force an update now?");
      strcpy(UpdateStatus, "ForceUpdate");
    }
    if (atoi(LatestVersion9) < atoi(CurrentVersion9))
    {
      strcpy(MenuText[2], " ");
      strcpy(MenuText[3], "Your software is newer than the latest version.");
      strcpy(MenuText[4], "Do you want to force an update now?");
      strcpy(UpdateStatus, "ForceUpdate");
    }
  }
}

/***************************************************************************//**
 * @brief Acts on Software Update buttons
 *
 * @param int NoButton, which is 5, 6 or 7
 *
 * @return void
*******************************************************************************/

void ExecuteUpdate(int NoButton)
{
  char Step[255];

  switch(NoButton)
  {
  case 5:
    if ((strcmp(UpdateStatus, "NormalUpdate") == 0) || (strcmp(UpdateStatus, "ForceUpdate") == 0))

    {
      // code for normal update

      // Display the updating message
      strcpy(Step, "Step 1 of 10\\nDownloading Update\\n\\nX---------");
      DisplayUpdateMsg("Latest" , Step);
      refreshMouseBackground();
      draw_cursor_foreground(mouse_x, mouse_y);
      UpdateWeb();

      // Delete any old update
      strcpy(LinuxCommand, "rm /home/pi/update.sh >/dev/null 2>/dev/null");
      system(LinuxCommand);

      printf("Downloading Normal Update Portsdown 4 Version\n");
      strcpy(LinuxCommand, "wget https://raw.githubusercontent.com/BritishAmateurTelevisionClub/portsdown4/master/update.sh");
      strcat(LinuxCommand, " -O /home/pi/update.sh");
      system(LinuxCommand);

      strcpy(Step, "Step 2 of 10\\nLoading Update Script\\n\\nXX--------");
      DisplayUpdateMsg("Latest Portsdown 4", Step);
      UpdateWeb();

      strcpy(LinuxCommand, "chmod +x /home/pi/update.sh");   
      system(LinuxCommand);
      system("reset");
      exit(132);  // Pass control back to scheduler
    }
    break;
  case 6:
    if (strcmp(UpdateStatus, "ForceUpdate") == 0)
    {
        strcpy(UpdateStatus, "DevUpdate");
    }
    break;
  case 7:
    if (strcmp(UpdateStatus, "DevUpdate") == 0)
    {
      // Code for Dev Update
      // Display the updating message
      strcpy(Step, "Step 1 of 10\\nDownloading Update\\n\\nX---------");
      DisplayUpdateMsg("Development", Step);
      UpdateWeb();

      // Delete any old update
      strcpy(LinuxCommand, "rm /home/pi/update.sh >/dev/null 2>/dev/null");
      system(LinuxCommand);

      printf("Downloading Development Update Portsdown 4 Version\n");
      strcpy(LinuxCommand, "wget https://raw.githubusercontent.com/davecrump/portsdown4/master/update.sh");
      strcat(LinuxCommand, " -O /home/pi/update.sh");
      system(LinuxCommand);

      strcpy(Step, "Step 2 of 10\\nLoading Update Script\\n\\nXX--------");
      DisplayUpdateMsg("Development", Step);
      UpdateWeb();

      strcpy(LinuxCommand, "chmod +x /home/pi/update.sh");   
      system(LinuxCommand);
      system("reset");
      exit(133);  // Pass control back to scheduler for Dev Load
    }
    break;
  default:
    break;
  }
}


/***************************************************************************//**
 * @brief Detects if a mouse is currently connected
 *
 * @param nil
 *
 * @return 0 if connected, 1 if not connected
*******************************************************************************/

int CheckMouse()
{
  FILE *fp;
  char response_line[255];

  // Read the Webcam address if it is present

  fp = popen("ls -l /dev/input | grep 'mouse'", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  // Response is "crw-rw---- 1 root input 13, 32 Apr 29 17:02 mouse0" if present, null if not
  // So, if there is a response, return 0.

  /* Read the output a line at a time - output it. */
  while (fgets(response_line, 250, fp) != NULL)
  {
    if (strlen(response_line) > 1)
    {
      pclose(fp);
      return 0;
    }
  }
  pclose(fp);
  return 1;
}


/***************************************************************************//**
 * @brief Performs Lime firmware update and checks GW revision
 *
 * @param nil
 *
 * @return void
*******************************************************************************/

void LimeFWUpdate(int button)
{
  // Portsdown 4 and selectable FW.  1 = 1.30, 2 = Custom, 0 is basic update
  if (CheckLimeUSBConnect() == 0)
  {
    MsgBox4("Upgrading Lime USB", "To latest standard", "Using LimeUtil 22.09", "Please Wait");
    system("LimeUtil --update");
    usleep(250000);
    MsgBox4("Upgrade Complete", " ", "Touch Screen to Continue" ," ");
  }
  else if (CheckLimeMiniConnect() == 0)
  {
    switch (button)
    {
    case 0:
      MsgBox4("Upgrading Lime Mini", "To latest standard", "Using LimeUtil 22.09", "Please Wait");
      system("LimeUtil --update");
      usleep(250000);
      MsgBox4("Upgrade Complete", " ", "Touch Screen to Continue" ," ");
      break;
    case 1:
      if (CheckLimeMiniV2Connect() != 0)    // Don't do it if a LimeSDR V2
      {
        MsgBox4("Upgrading Lime Firmware", "to 1.30", " ", " ");
        system("sudo LimeUtil --fpga=/home/pi/.local/share/LimeSuite/images/22.09/LimeSDR-Mini_HW_1.2_r1.30.rpd");
        if (LimeGWRev() == 30)
        {
          MsgBox4("Firmware Upgrade Successful", "Now at Gateware 1.30", "Touch Screen to Continue" ," ");
        }
        else
        {
          MsgBox4("Firmware Upgrade Unsuccessful", "Further Investigation required", "Touch Screen to Continue" ," ");
        }
      }
      break;
    case 2:
      if (CheckLimeMiniV2Connect() != 0)    // Don't do it if a LimeSDR V2
      {
        MsgBox4("Upgrading Lime Firmware", "to Custom DVB", " ", " ");
        system("sudo LimeUtil --force --fpga=/home/pi/.local/share/LimeSuite/images/v0.3/LimeSDR-Mini_lms7_trx_HW_1.2_auto.rpd");

        MsgBox4("Firmware Upgrade Complete", "DVB", "Touch Screen to Continue" ," ");
      }
      break;
    default:
      printf("Lime Update button selection error\n");
      break;
    }
  }
  else
  {
    MsgBox4("No LimeSDR Detected", " ", "Touch Screen to Continue", " ");
  }
  wait_touch();
}


/***************************************************************************//**
 * @brief Looks up the GPU Temp
 *
 * @param GPUTemp (str) GPU Temp to be passed as a string max 20 char
 *
 * @return void
*******************************************************************************/

void GetGPUTemp(char GPUTemp[256])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("/opt/vc/bin/vcgencmd measure_temp", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(GPUTemp, 20, fp) != NULL)
  {
    printf("%s", GPUTemp);
  }

  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief Looks up the CPU Temp
 *
 * @param CPUTemp (str) CPU Temp to be passed as a string
 *
 * @return void
*******************************************************************************/

void GetCPUTemp(char CPUTemp[256])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("cat /sys/class/thermal/thermal_zone0/temp", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(CPUTemp, 20, fp) != NULL)
  {
    printf("%s", CPUTemp);
  }

  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief Checks the CPU Throttling Status
 *
 * @param Throttled (str) Throttle status to be passed as a string
 *
 * @return void
*******************************************************************************/

void GetThrottled(char Throttled[256])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("vcgencmd get_throttled", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(Throttled, 20, fp) != NULL)
  {
    printf("%s", Throttled);
  }

  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief Sets the System Audio Levels
 *
 * @param None yet, but there will be in future
 *
 * @return void
*******************************************************************************/

void SetAudioLevels()
{
  char MicGain[15];
  char aMixerCmd[127];
  int MicLevelPercent;

  // Read the mic gain (may not be defined)
  GetConfigParam(PATH_PCONFIG,"micgain", MicGain);
  MicLevel = atoi(MicGain);

  // Error check the Mic Gain
  if ((MicLevel <= 0) || (MicLevel > 30))
  {
    MicLevel = 26;
  }

  // Apply as apercentage as some cards are not 0 - 30 but 0 - 17
  MicLevelPercent = (MicLevel * 10) / 3;

  // Apply to cards 1 and 2 in case the EasyCap has grabbed the card 1 ID
  snprintf(aMixerCmd, 126, "amixer -c 1 -- sset Mic Capture %d%% > /dev/null 2>&1", MicLevelPercent);
  system(aMixerCmd);

  snprintf(aMixerCmd, 126, "amixer -c 2 -- sset Mic Capture %d%% > /dev/null 2>&1", MicLevelPercent);
  system(aMixerCmd);

  // And set all the output levels to Max
  system("amixer -c 0 -- sset Headphone 100% > /dev/null 2>&1");
  system("amixer -c 1 -- sset Speaker 100% > /dev/null 2>&1");
  system("amixer -c 2 -- sset Speaker 100% > /dev/null 2>&1");
}


/***************************************************************************//**
 * @brief Reads the input source from portsdown_config.txt
 *        and determines coding and video source
 * @param sets strings: coding, source
 *
 * @return void
*******************************************************************************/

void ReadModeInput(char coding[256], char vsource[256])
{
  char ModeInput[31];
  char value[31];
  char ModulationMode[31];

  // Read the current Modulation
  GetConfigParam(PATH_PCONFIG,"modulation", ModulationMode);
  if (strlen(ModulationMode) < 1)
  {
    strcpy(CurrentTXMode, "DVB-S");
  }
  else
  {
    strcpy(CurrentTXMode, ModulationMode);
  }

  // Read the current pilots and frames
  GetConfigParam(PATH_PCONFIG,"pilots", value);
  if (strlen(value) > 1)
  {
    strcpy(CurrentPilots, value);
  }
  GetConfigParam(PATH_PCONFIG,"frames", value);
  if (strlen(value) > 1)
  {
    strcpy(CurrentFrames, value);
  }

  strcpy(coding, "notset");
  strcpy(vsource, "notset");

  // Read the current vision source and encoding
  GetConfigParam(PATH_PCONFIG,"modeinput", ModeInput);
  GetConfigParam(PATH_PCONFIG,"modeoutput", ModeOutput);
  GetConfigParam(PATH_PCONFIG,"format", CurrentFormat);
  GetConfigParam(PATH_PCONFIG,"encoding", CurrentEncoding);

  // Correct Jetson modes if Jetson not selected
  printf ("Mode Output in ReadModeInput() is %s\n", ModeOutput);
  if ((strcmp(ModeOutput, "JLIME") != 0) && (strcmp(ModeOutput, "JEXPRESS") != 0))
  {
    // If H265 encoding selected, set Encoding to H264
    if (strcmp(CurrentEncoding, "H265") == 0)
    {
      strcpy(CurrentEncoding, "H264");
      strcpy(coding, "H264");
      SetConfigParam(PATH_PCONFIG, "encoding", CurrentEncoding);
    }

    // Read ModeInput from Config and correct if required
    if (strcmp(ModeInput, "JHDMI") == 0)
    {
      strcpy(vsource, "Screen");
      strcpy(CurrentSource, TabSource[4]); // Desktop
      SetConfigParam(PATH_PCONFIG, "modeinput", "DESKTOP");
    }
    if (strcmp(ModeInput, "JCAM") == 0)
    {
      strcpy(vsource, "RPi Camera");
      strcpy(CurrentSource, TabSource[0]); // Pi Cam
      SetConfigParam(PATH_PCONFIG, "modeinput", "DESKTOP");
    } 
    if (strcmp(ModeInput, "JPC") == 0)
    {
      strcpy(vsource, "Screen");
      strcpy(CurrentSource, TabSource[4]); // Desktop
      SetConfigParam(PATH_PCONFIG, "modeinput", "DESKTOP");
    }
    if (strcmp(ModeInput, "JWEBCAM") == 0)
    {
      strcpy(vsource, "Webcam");
      strcpy(CurrentSource, TabSource[6]); // Webcam
      SetConfigParam(PATH_PCONFIG, "modeinput", "WEBCAMH264");
    }      
    if (strcmp(ModeInput, "JCARD") == 0)
    {
      strcpy(vsource, "Static Test Card F");
      strcpy(CurrentSource, TabSource[3]); // TestCard
      SetConfigParam(PATH_PCONFIG, "modeinput", "CARDH264");
    }      
  }

  if (strcmp(ModeInput, "CAMH264") == 0) 
  {
    strcpy(coding, "H264");
    strcpy(vsource, "RPi Camera");
    strcpy(CurrentEncoding, "H264");
    strcpy(CurrentSource, TabSource[0]); // Pi Cam
  } 
  else if (strcmp(ModeInput, "ANALOGCAM") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "Ext Video Input");
    strcpy(CurrentEncoding, "H264");
    if(strcmp(CurrentFormat, "16:9") !=0)  // Allow 16:9
    {
      strcpy(CurrentFormat, "4:3");
    }
    strcpy(CurrentSource, TabSource[1]); // EasyCap
  }
  else if (strcmp(ModeInput, "WEBCAMH264") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "Webcam");
    strcpy(CurrentEncoding, "H264");
    strcpy(CurrentSource, TabSource[6]); // Webcam
  }
  else if (strcmp(ModeInput, "CARDH264") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "Static Test Card F");
    strcpy(CurrentEncoding, "H264");
    if ((strcmp(CurrentFormat, "720p") == 0) || (strcmp(CurrentFormat, "1080p") == 0))  // Go 16:9
    {
      strcpy(CurrentFormat, "16:9");
    }
    if(strcmp(CurrentFormat, "16:9") != 0)  // Allow 16:9
    {
      strcpy(CurrentFormat, "4:3");
    }
    SetConfigParam(PATH_PCONFIG, "format", CurrentFormat);
    strcpy(CurrentSource, TabSource[3]); // TestCard
  }
  else if (strcmp(ModeInput, "CONTEST") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "Contest Numbers");
    strcpy(CurrentEncoding, "H264");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[5]); // Contest
  }
  else if (strcmp(ModeInput, "DESKTOP") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "Screen");
    strcpy(CurrentEncoding, "H264");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[4]); // Desktop
  }
  else if (strcmp(ModeInput, "CARRIER") == 0)
  {
    strcpy(coding, "DC");
    strcpy(vsource, "Plain Carrier");
    strcpy(CurrentTXMode, "Carrier");
  }
  else if (strcmp(ModeInput, "TESTMODE") == 0)
  {
    strcpy(coding, "Square Wave");
    strcpy(vsource, "Test");
    strcpy(CurrentEncoding, "H264");
  }
  else if (strcmp(ModeInput, "FILETS") == 0)
  {
    strcpy(coding, "Native");
    strcpy(vsource, "TS File");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentEncoding, "TS File");
  }
  else if (strcmp(ModeInput, "IPTSIN") == 0)
  {
    strcpy(coding, "Native");
    strcpy(vsource, "IP Transport Stream");
    strcpy(CurrentEncoding, "IPTS in");
  }
  else if (strcmp(ModeInput, "IPTSIN264") == 0)
  {
    strcpy(coding, "Native");
    strcpy(vsource, "Raw IP Transport Stream H264");
    strcpy(CurrentEncoding, "IPTS in H264");
  }
  else if (strcmp(ModeInput, "IPTSIN265") == 0)
  {
    strcpy(coding, "Native");
    strcpy(vsource, "Raw IP Transport Stream H265");
    strcpy(CurrentEncoding, "IPTS in H265");
  }
  else if (strcmp(ModeInput, "VNC") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "VNC");
    strcpy(CurrentEncoding, "H264");
    strcpy(CurrentFormat, "4:3");
  }
  else if (strcmp(ModeInput, "CAMMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "RPi Camera");
    strcpy(CurrentEncoding, "MPEG-2");
    if (strcmp(ModeOutput, "STREAMER") != 0)  // Allow 16:9 for streamer
    {
      strcpy(CurrentFormat, "4:3");
    }
    strcpy(CurrentSource, TabSource[0]); // Pi Cam
  }
  else if (strcmp(ModeInput, "ANALOGMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Ext Video Input");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[1]); // EasyCap
  }
  else if (strcmp(ModeInput, "WEBCAMMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Webcam");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentSource, TabSource[6]); // Webcam
  }
  else if (strcmp(ModeInput, "CARDMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Static Test Card");
    strcpy(CurrentEncoding, "MPEG-2");
    if (strcmp(ModeOutput, "STREAMER") != 0)  // Allow 16:9 for streamer
    {
      strcpy(CurrentFormat, "4:3");
    }
    strcpy(CurrentSource, TabSource[3]); // Desktop
  }
  else if (strcmp(ModeInput, "CONTESTMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Contest Numbers");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[5]); // Contest
  }
  else if (strcmp(ModeInput, "CAM16MPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "RPi Cam 16:9");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "16:9");
    strcpy(CurrentSource, TabSource[0]); // Pi Cam
  }
  else if (strcmp(ModeInput, "CAMHDMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "RPi Cam HD");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "720p");
    strcpy(CurrentSource, TabSource[0]); // Pi Cam
  }
  else if (strcmp(ModeInput, "ANALOG16MPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Ext Video Input");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "16:9");
    strcpy(CurrentSource, TabSource[1]); // EasyCap
  }
  else if (strcmp(ModeInput, "WEBCAM16MPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Webcam");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "16:9");
    strcpy(CurrentSource, TabSource[6]); // Webcam
  }
  else if (strcmp(ModeInput, "WEBCAMHDMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Webcam");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "720p");
    strcpy(CurrentSource, TabSource[6]); // Webcam
  }
  else if (strcmp(ModeInput, "CARD16MPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Static Test Card");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "16:9");
    strcpy(CurrentSource, TabSource[3]); // TestCard
  }
  else if (strcmp(ModeInput, "CARDHDMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Static Test Card");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "720p");
    strcpy(CurrentSource, TabSource[3]); // TestCard
  }
  else if (strcmp(ModeInput, "HDMI") == 0)
  {
    GetConfigParam(PATH_PCONFIG, "format", CurrentFormat);
    GetConfigParam(PATH_PCONFIG, "encoding", CurrentEncoding);
    strcpy(coding, CurrentEncoding);
    strcpy(vsource, "HDMI");
    strcpy(CurrentSource, TabSource[8]); // HDMI
  }
  else
  {
    strcpy(coding, "notset");
    strcpy(vsource, "notset");
  }

  // Override all of the above for Jetson modes
  if ((strcmp(ModeOutput, "JLIME") == 0) || (strcmp(ModeOutput, "JEXPRESS") == 0))
  {
    // Read format from config and set
    GetConfigParam(PATH_PCONFIG, "format", CurrentFormat);

    // Read Encoding from Config and set
    GetConfigParam(PATH_PCONFIG, "encoding", CurrentEncoding);
    strcpy(coding, CurrentEncoding);

    // Read ModeInput from Config and set
    if (strcmp(ModeInput, "JHDMI") == 0) 
    {
      strcpy(vsource, "Jetson HDMI");
      strcpy(CurrentSource, "HDMI");
    }      
    if (strcmp(ModeInput, "JCAM") == 0)
    {
      strcpy(vsource, "Jetson Pi Camera");
      strcpy(CurrentSource, "Pi Cam");
    }      
    if (strcmp(ModeInput, "JPC") == 0)
    {
      strcpy(vsource, "Jetson PC Input");
      strcpy(CurrentSource, "PC");
    }      
    if (strcmp(ModeInput, "JWEBCAM") == 0)
    {
      strcpy(vsource, "Jetson Webcam");
      strcpy(CurrentSource, "Webcam");
    }      
    if (strcmp(ModeInput, "JCARD") == 0)
    {
      strcpy(vsource, "Jetson Test Card");
      strcpy(CurrentSource, "TestCard");
    }      
  }
}

/***************************************************************************//**
 * @brief Reads the output mode from portsdown_config.txt
 *        and determines the user-friendly string for display
 * @param sets strings: Moutput
 *
 * @return void
*******************************************************************************/

void ReadModeOutput(char Moutput[256])
{
  char ModeOutput[255];
  // char LimeCalFreqText[63];
  char LimeRFEStateText[63];
  char LimeUpsampleText[63];

  GetConfigParam(PATH_PCONFIG,"modeoutput", ModeOutput);
  strcpy(CurrentModeOP, ModeOutput);
  strcpy(Moutput, "notset");

  if (strcmp(ModeOutput, "DATVEXPRESS") == 0) 
  {
    strcpy(Moutput, "DATV Express DVB-S");
    strcpy(CurrentModeOPtext, TabModeOPtext[2]);
  } 
  else if (strcmp(ModeOutput, "LIMEUSB") == 0) 
  {
    strcpy(Moutput, "LimeSDR USB");
    strcpy(CurrentModeOPtext, TabModeOPtext[3]);
  } 
  else if (strcmp(ModeOutput, "LIMEMINI") == 0) 
  {
    strcpy(Moutput, "LimeSDR Mini");
    strcpy(CurrentModeOPtext, TabModeOPtext[8]);
  } 
  else if (strcmp(ModeOutput, "STREAMER") == 0) 
  {
    strcpy(Moutput, "BATC Streaming");
    strcpy(CurrentModeOPtext, TabModeOPtext[4]);
  } 
  else if (strcmp(ModeOutput, "COMPVID") == 0) 
  {
    strcpy(Moutput, "Composite Video");
    strcpy(CurrentModeOPtext, TabModeOPtext[5]);
  } 
  else if (strcmp(ModeOutput, "IP") == 0) 
  {
    strcpy(Moutput, "IP Stream");
    strcpy(CurrentModeOPtext, TabModeOPtext[7]);
  } 
  else if (strcmp(ModeOutput, "JLIME") == 0) 
  {
    strcpy(Moutput, "Jetson with Lime");
    strcpy(CurrentModeOPtext, TabModeOPtext[9]);
  } 
  else if (strcmp(ModeOutput, "JEXPRESS") == 0) 
  {
    strcpy(Moutput, "Jetson with DATV Express");
    strcpy(CurrentModeOPtext, TabModeOPtext[10]);
  } 
  else if (strcmp(ModeOutput, "MUNTJAC") == 0) 
  {
    strcpy(Moutput, "Muntjac");
    strcpy(CurrentModeOPtext, TabModeOPtext[11]);
  } 
  else if (strcmp(ModeOutput, "LIMEDVB") == 0) 
  {
    strcpy(Moutput, "LimeSDR Mini with custom DVB FW");
    strcpy(CurrentModeOPtext, TabModeOPtext[12]);
  } 
  else if (strcmp(ModeOutput, "PLUTO") == 0) 
  {
    strcpy(Moutput, "Pluto");
    strcpy(CurrentModeOPtext, TabModeOPtext[13]);
  } 
  else  // Possibly Ugly or IQ, so set to Lime Mini
  {
    strcpy(Moutput, "LimeSDR Mini");
    strcpy(CurrentModeOPtext, TabModeOPtext[8]);
    SetConfigParam(PATH_PCONFIG,"modeoutput", "LIMEMINI");
    strcpy(CurrentModeOP, "LIMEMINI");
  }

  // Read LimeCal freq
  LimeCalFreq  = -1;  // Always Cal is the only option

  // Read LimeCal freq
  //GetConfigParam(PATH_LIME_CAL, "limecalfreq", LimeCalFreqText);
  //LimeCalFreq = atof(LimeCalFreqText);
  LimeCalFreq  = -1;  // Always Cal is the only option

  // And read LimeRFE state
  GetConfigParam(PATH_PCONFIG, "limerfe", LimeRFEStateText);
  if (strcmp(LimeRFEStateText, "enabled") == 0)
  {
    LimeRFEState = 1;
  }
  else
  {
    LimeRFEState = 0;
  }

  // LimeRFE TX Port
  GetConfigParam(PATH_PCONFIG, "limerfeport", LimeRFEStateText);
  if (strcmp(LimeRFEStateText, "txrx") == 0)
  {
    LimeRFEPort = 1;
  }
  else if (strcmp(LimeRFEStateText, "tx") == 0)
  {
    LimeRFEPort = 2;
  }
  else
  {
    LimeRFEPort = 3;
  }

  // LimeRFE RX Attenuator
  GetConfigParam(PATH_PCONFIG, "limerferxatt", LimeRFEStateText);
  LimeRFERXAtt = atoi(LimeRFEStateText);

  // Read DVB-T Guard Interval and QAM
  GetConfigParam(PATH_PCONFIG, "guard", Guard);
  GetConfigParam(PATH_PCONFIG, "qam", DVBTQAM);

  // Lime Upsample
  GetConfigParam(PATH_PCONFIG, "upsample", LimeUpsampleText);
  LimeUpsample = atoi(LimeUpsampleText);
}


/***************************************************************************//**
 * @brief Checks if a file exists
 *
 * @param nil
 *
 * @return 0 if exists, 1 if not
*******************************************************************************/

int file_exist (char *filename)
{
  if (access(filename, R_OK) == 0) 
  {
    // file exists
    return 0;
  }
  else
  {
    // file doesn't exist
    return 1;
  }
}

/***************************************************************************//**
 * @brief Reads the EasyCap modes from portsdown_config.txt
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/

void ReadModeEasyCap()
{
  GetConfigParam(PATH_PCONFIG,"analogcaminput", ModeVidIP);
  GetConfigParam(PATH_PCONFIG,"analogcamstandard", ModeSTD);
}

/***************************************************************************//**
 * @brief Reads the PiCam Orientation from portsdown_config.txt
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/

void ReadPiCamOrientation()
{
  GetConfigParam(PATH_PCONFIG,"picam", CurrentPiCamOrientation);
}

/***************************************************************************//**
 * @brief Reads the Caption State from portsdown_config.txt
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/

void ReadCaptionState()
{
  GetConfigParam(PATH_PCONFIG,"caption", CurrentCaptionState);
}
/***************************************************************************//**
 * @brief Reads the TestCard State from testcard_config.txt
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/

void ReadTestCardState()
{
  char Value[63];
  GetConfigParam(PATH_TC_CONFIG, "card", Value);
  CurrentTestCard = atoi(Value);
  if ((CurrentTestCard < 0) || (CurrentTestCard > 11))
  {
    CurrentTestCard = 0;
  }

  GetConfigParam(PATH_TC_CONFIG, "card5name", TestCardName[5]);
  GetConfigParam(PATH_TC_CONFIG, "card5title", TestCardTitle[5]);
  GetConfigParam(PATH_TC_CONFIG, "card6name", TestCardName[6]);
  GetConfigParam(PATH_TC_CONFIG, "card6title", TestCardTitle[6]);
  GetConfigParam(PATH_TC_CONFIG, "card7name", TestCardName[7]);
  GetConfigParam(PATH_TC_CONFIG, "card7title", TestCardTitle[7]);
  GetConfigParam(PATH_TC_CONFIG, "card8name", TestCardName[8]);
  GetConfigParam(PATH_TC_CONFIG, "card8title", TestCardTitle[8]);
  GetConfigParam(PATH_TC_CONFIG, "card9name", TestCardName[9]);
  GetConfigParam(PATH_TC_CONFIG, "card9title", TestCardTitle[9]);
  GetConfigParam(PATH_TC_CONFIG, "card10name", TestCardName[10]);
  GetConfigParam(PATH_TC_CONFIG, "card10title", TestCardTitle[10]);
  GetConfigParam(PATH_TC_CONFIG, "card11name", TestCardName[11]);
  GetConfigParam(PATH_TC_CONFIG, "card11title", TestCardTitle[11]);
}

/***************************************************************************//**
 * @brief Reads the Audio State from portsdown_config.txt
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/

void ReadAudioState()
{
  GetConfigParam(PATH_PCONFIG,"audio", CurrentAudioState);
}

/***************************************************************************//**
 * @brief Reads the VLC Volume from portsdown_config.txt
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/

void ReadVLCVolume()
{
  char VLCVolumeText[15];
  GetConfigParam(PATH_PCONFIG, "vlcvolume", VLCVolumeText);
  CurrentVLCVolume = atoi(VLCVolumeText);
  if (CurrentVLCVolume < 0)
  {
    CurrentVLCVolume = 0;
  }
  if (CurrentVLCVolume > 512)
  {
    CurrentVLCVolume = 512;
  }
}


/***************************************************************************//**
 * @brief Reads webcontrol state from portsdown_config.txt
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/

void ReadWebControl()
{
  char WebControlText[15];

  GetConfigParam(PATH_PCONFIG, "webcontrol", WebControlText);

  if (strcmp(WebControlText, "enabled") == 0)
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

/***************************************************************************//**
 * @brief Reads the current Attenuator from portsdown_config.txt
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/

void ReadAttenState()
{
  GetConfigParam(PATH_PCONFIG,"attenuator", CurrentAtten);
}

/***************************************************************************//**
 * @brief Reads the current band from portsdown_config.txt
 * and checks and rewrites it if required
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/

void ReadBand()
{
  int i;
  char Param[15];
  char Value[15]="";
  char BandFromFile[15];
  float CurrentFreq;

  // Look up the current frequency
  strcpy(Param,"freqoutput");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  CurrentFreq = atof(Value);
  strcpy(Value,"");

  // Look up the current band designator and derive current band index
  strcpy(Param, "band");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  strcpy(BandFromFile, Value);

  for (i = 0; i < 16; i++)
  {
    if (strcmp(Value, TabBand[i]) == 0)
    {
      CurrentBand = i;
    }
  }

  if ((TabBandLO[CurrentBand] < 0.5) && (TabBandLO[CurrentBand] > -0.5)) // LO freq = 0 so not a transverter
  {
    // Set band based on the current frequency

    if (CurrentFreq < 60)                            // 50 MHz
    {
       CurrentBand = 14;
       strcpy(Value, "d0");
    }
    if ((CurrentFreq >= 60) && (CurrentFreq < 100))  // 71 MHz
    {
       CurrentBand = 0;
       strcpy(Value, "d1");
    }
    if ((CurrentFreq >= 100) && (CurrentFreq < 250))  // 146 MHz
    {
      CurrentBand = 1;
      strcpy(Value, "d2");
    }
    if ((CurrentFreq >= 250) && (CurrentFreq < 950))  // 437 MHz
    {
      CurrentBand = 2;
      strcpy(Value, "d3");
    }
    if ((CurrentFreq >= 950) && (CurrentFreq < 2000))  // 1255 MHz
    {
      CurrentBand = 3;
      strcpy(Value, "d4");
    }
    if ((CurrentFreq >= 2000) && (CurrentFreq < 3000))  // 2400 MHz
    {
      CurrentBand = 4;
      strcpy(Value, "d5");
    }
    if (CurrentFreq >= 3000)                          // 3400 MHz
    {
      CurrentBand = 15;
      strcpy(Value, "d6");
    }

    // And set the band correctly if required
    if (strcmp(BandFromFile, Value) != 0)
    {
      strcpy(Param,"band");
      SetConfigParam(PATH_PCONFIG, Param, Value);
    }
  }
    printf("In ReadBand, CurrentFreq = %f, CurrentBand = %d and band desig = %s\n", CurrentFreq, CurrentBand, Value);
}

/***************************************************************************//**
 * @brief Reads all 16 band details from portsdown_presets.txt
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/
void ReadBandDetails()
{
  int i;
  char Param[31];
  char Value[31]="";
  for( i = 0; i < 16; i = i + 1)
  {
    strcpy(Param, TabBand[i]);
    strcat(Param, "label");
    GetConfigParam(PATH_PPRESETS, Param, Value);
    strcpy(TabBandLabel[i], Value);
    
    strcpy(Param, TabBand[i]);
    strcat(Param, "attenlevel");
    GetConfigParam(PATH_PPRESETS, Param, Value);
    TabBandAttenLevel[i] = atoi(Value);

    strcpy(Param, TabBand[i]);
    strcat(Param, "explevel");
    GetConfigParam(PATH_PPRESETS, Param, Value);
    TabBandExpLevel[i] = atoi(Value);

    strcpy(Param, TabBand[i]);
    strcat(Param, "limegain");
    GetConfigParam(PATH_PPRESETS, Param, Value);
    TabBandLimeGain[i] = atoi(Value);

    strcpy(Param, TabBand[i]);
    strcat(Param, "muntjacgain");
    GetConfigParam(PATH_PPRESETS, Param, Value);
    TabBandMuntjacGain[i] = atoi(Value);

    strcpy(Param, TabBand[i]);
    strcat(Param, "expports");
    GetConfigParam(PATH_PPRESETS, Param, Value);
    TabBandExpPorts[i] = atoi(Value);

    strcpy(Param, TabBand[i]);
    strcat(Param, "plutopwr");
    GetConfigParam(PATH_PPRESETS, Param, Value);
    TabBandPlutoLevel[i] = -1 * atoi(Value);  // Pluto level is saved as 0 - 71 but used as 0 to -71

    strcpy(Param, TabBand[i]);
    strcat(Param, "lo");
    GetConfigParam(PATH_PPRESETS, Param, Value);
    TabBandLO[i] = atof(Value);

    strcpy(Param, TabBand[i]);
    strcat(Param, "numbers");
    GetConfigParam(PATH_PPRESETS, Param, Value);
    strcpy(TabBandNumbers[i], Value);
    //printf("Index = %d, Band = %s, Name = %s, LO = %f\n", i, TabBand[i], TabBandLabel[i], TabBandLO[i]);
  }
}

/***************************************************************************//**
 * @brief Reads the current Call, Locator and PIDs from portsdown_config.txt
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/
void ReadCallLocPID()
{
  char Param[31];
  char Value[255]="";

  strcpy(Param, "call");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  strcpy(CallSign, Value);

  strcpy(Param, "locator");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  strcpy(Locator, Value);

  strcpy(Param, "pidstart");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  strcpy(PIDstart, Value);

  strcpy(Param, "pidvideo");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  strcpy(PIDvideo, Value);

  strcpy(Param, "pidaudio");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  strcpy(PIDaudio, Value);

  strcpy(Param, "pidpmt");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  strcpy(PIDpmt, Value);
}

/***************************************************************************//**
 * @brief Reads the Langstone parameters from portsdown_config.txt
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/
void ReadLangstone()
{
  char Param[31];
  char Value[255]="";

  strcpy(Param, "startup");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  strcpy(StartApp, Value);

  strcpy(Param, "plutoip");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  strcpy(PlutoIP, Value);

  strcpy(Param, "langstone");
  strcpy(Value, "none");   // default
  GetConfigParam(PATH_PCONFIG, Param, Value);
  strcpy(langstone_version, Value);
}

/***************************************************************************//**
 * @brief Reads the TS Config parameters from portsdown_config.txt
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/
void ReadTSConfig()
{
  char Param[31];
  char Value[255]="";

  strcpy(Param, "udpoutaddr");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  strcpy(UDPOutAddr, Value);

  strcpy(Param, "udpoutport");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  strcpy(UDPOutPort, Value);

  strcpy(Param, "udpinport");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  strcpy(UDPInPort, Value);

  strcpy(Param, "tsvideofile");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  strcpy(TSVideoFile, Value);
}


/***************************************************************************//**
 * @brief Reads the current ADF Ref Freqs from portsdown_presets.txt
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/
void ReadADFRef()
{
  char Param[31];
  char Value[255]="";

  strcpy(Param, "adfref1");
  GetConfigParam(PATH_PPRESETS, Param, Value);
  strcpy(ADFRef[0], Value);

  strcpy(Param, "adfref2");
  GetConfigParam(PATH_PPRESETS, Param, Value);
  strcpy(ADFRef[1], Value);

  strcpy(Param, "adfref3");
  GetConfigParam(PATH_PPRESETS, Param, Value);
  strcpy(ADFRef[2], Value);

  strcpy(Param, "adfref");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  strcpy(CurrentADFRef, Value);

  strcpy(Param, "adf5355ref");
  GetConfigParam(PATH_SGCONFIG, Param, Value);
  strcpy(ADF5355Ref, Value);
}

/***************************************************************************//**
 * @brief Looks up the SD Card Serial Number
 *
 * @param SerNo (str) Serial Number to be passed as a string
 *
 * @return void
*******************************************************************************/

void GetSerNo(char SerNo[256])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("cat /sys/block/mmcblk0/device/serial", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(SerNo, 20, fp) != NULL)
  {
    printf("%s", SerNo);
  }

  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief Calculates the TS bitrate for the current settings
 *
 * @param nil.  Looks up globals
 *
 * @return int Bitrate.  -1  indicates calculation error
*******************************************************************************/

int CalcTSBitrate()
{
  char Value[127] = " ";
  float Bitrate = 0;
  int BitrateK = 0;
  int guard = 32;
  int BitsPerSymbol = 2;
  int fec = 99;
  int fecden = 100;
  char Constellation[15];
  FILE *fp;
  char BitRateCommand[255];

  // Look up raw SR or bandwidth
  GetConfigParam(PATH_PCONFIG, "symbolrate", Value);
  BitrateK = atoi(Value);
  Bitrate = 1000 * atof(Value);

  strcpy(Value, "99");
  GetConfigParam(PATH_PCONFIG, "fec", Value);
  fec = atoi(Value);

  if ((strcmp(CurrentTXMode, "S2QPSK") == 0) || (strcmp(CurrentTXMode, "8PSK") == 0)
   || (strcmp(CurrentTXMode, "16APSK") == 0) || (strcmp(CurrentTXMode, "32APSK") == 0))
  {
    // First determine FEC parameters
    if((fec == 14) || (fec == 13) || (fec == 12)) // 1/4, 1/3, or 1/2
    {
      fecden = fec - 10;
      fec = 1;
    }
    else if (fec == 23)
    {
      fec = 2;
      fecden = 3;
    }
    else if ((fec == 34) || (fec == 35))
    {
      fecden = fec - 30;
      fec = 3;
    }
    else if (fec == 56)
    {
      fec = 5;
      fecden = 6;
    }
    else if (fec == 89)
    {
      fec = 8;
      fecden = 9;
    }
    else if (fec == 91)
    {
      fec = 9;
      fecden = 10;
    }
    else
    {
      return -1; // invalid FEC
    }

    // Now determine Bits per symbol
    if (strcmp(CurrentTXMode, "S2QPSK") == 0)
    {
      strcpy(Constellation, "QPSK");
    }
    else if (strcmp(CurrentTXMode, "8PSK") == 0)
    {
      strcpy(Constellation, "8PSK");
    }
    else if (strcmp(CurrentTXMode, "16APSK") == 0)
    {
      strcpy(Constellation, "16APSK");
    }
    else if (strcmp(CurrentTXMode, "32APSK") == 0)
    {
      strcpy(Constellation, "32APSK");
    }

    strcpy(BitRateCommand, "/home/pi/rpidatv/bin/dvb2iq");
    snprintf(Value, 15, " -s %d", BitrateK);
    strcat(BitRateCommand, Value);
    snprintf(Value, 63, " -f %d/%d -d -r 2 -m DVBS2", fec, fecden);
    strcat(BitRateCommand, Value);
    snprintf(Value, 63, " -c %s", Constellation);
    strcat(BitRateCommand, Value);

    //printf("Command is %s\n", BitRateCommand);

    fp = popen(BitRateCommand, "r");
    if (fp == NULL)
    {
      printf("Failed to run command\n" );
      exit(1);
    }

    while (fgets(Value, 10, fp) != NULL)
    {
      //printf("Calculated DVB-S2 bitrate is: %s", Value);
    }
    pclose(fp);
    return atoi(Value);  
  }

  if (strcmp(CurrentTXMode, "DVB-S") == 0)
  {
    // bitrate = SR * 2 * FEC * 188 / 204

    if ((fec == 1) || (fec == 2) || (fec == 3) || (fec == 5) || (fec ==7))  // valid FEC
    {
      Bitrate = Bitrate *  2 * fec *    188 ;       // top line
      Bitrate = Bitrate /  ((fec + 1) * 204);       // bottom line    
    }
    else
    {
      return -1;
    }
  }

  if (strcmp(CurrentTXMode, "DVB-T") == 0)
  {
    // bitrate = 423/544 * bandwidth * FEC * (bits per symbol) * (Guard Factor)

    if (strcmp(DVBTQAM, "qpsk") == 0)
    {
      BitsPerSymbol = 2;
    }
    if (strcmp(DVBTQAM, "16qam") == 0)
    {
      BitsPerSymbol = 4;
    }
    if (strcmp(DVBTQAM, "64qam") == 0)
    {
      BitsPerSymbol = 6;
    }

    if (strcmp(Guard, "4") == 0)
    {
      guard = 4;
    }
    if (strcmp(Guard, "8") == 0)
    {
      guard = 8;
    }
    if (strcmp(Guard, "16") == 0)
    {
      guard = 16;
    }
    if (strcmp(Guard, "32") == 0)
    {
      guard = 32;
    }

    if ((fec == 1) || (fec == 2) || (fec == 3) || (fec == 5) || (fec ==7))  // valid FEC
    {
      Bitrate = Bitrate *  423 * fec * BitsPerSymbol * guard;       // top line
      Bitrate = Bitrate / (544 * (fec + 1)       *   (guard + 1));  // bottom line    
    }
    else
    {
      return -1;
    }
  } 
  return (int)Bitrate;
}


/***************************************************************************//**
 * @brief Looks up the Audio Input Devices
 *
 * @param DeviceName1 and DeviceName2 (str) First 40 char of device names
 *
 * @return void
*******************************************************************************/

void GetDevices(char DeviceName1[256], char DeviceName2[256])
{
  FILE *fp;
  char arecord_response_line[256];
  int card = 1;

  /* Open the command for reading. */
  fp = popen("arecord -l", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(arecord_response_line, 250, fp) != NULL)
  {
    if (arecord_response_line[0] == 'c')
    {
      if (card == 2)
      {
        strcpy(DeviceName2, arecord_response_line);
        card = card + 1;
      }
      if (card == 1)
      {
        strcpy(DeviceName1, arecord_response_line);
        card = card + 1;
      }
    }
    printf("%s", arecord_response_line);
  }
  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief Looks up the USB Video Input Device address
 *
 * @param DeviceName1 and DeviceName2 (str) First 40 char of device names
 *
 * @return void
*******************************************************************************/

void GetUSBVidDev(char VidDevName[256])
{
  FILE *fp;
  char response_line[255];
  char WebcamName[255] = "none";

  strcpy(VidDevName, "nil");

  // Read the Webcam address if it is present

  fp = popen("v4l2-ctl --list-devices 2> /dev/null | sed -n '/Webcam/,/dev/p' | grep 'dev' | tr -d '\t'", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response_line, 250, fp) != NULL)
  {
    if (strlen(response_line) > 1)
    {
      strcpy(WebcamName, response_line);
    }
  }
  pclose(fp);

  if (strcmp(WebcamName, "none") == 0)  // not detected previously, so try "046d:0825" for C270
  {
    fp = popen("v4l2-ctl --list-devices 2> /dev/null | sed -n '/046d:0825/,/dev/p' | grep 'dev' | tr -d '\t'", "r");
    if (fp == NULL)
    {
      printf("Failed to run command\n" );
      exit(1);
    }

    /* Read the output a line at a time - output it. */
    while (fgets(response_line, 250, fp) != NULL)
    {
      if (strlen(response_line) > 1)
       {
        strcpy(WebcamName, response_line);
      }
    }
    pclose(fp);
  }

  if (strcmp(WebcamName, "none") == 0)  // not detected previously, so try "046d:081b" for C310
  {
    fp = popen("v4l2-ctl --list-devices 2> /dev/null | sed -n '/046d:081b/,/dev/p' | grep 'dev' | tr -d '\t'", "r");
    if (fp == NULL)
    {
      printf("Failed to run command\n" );
      exit(1);
    }

    /* Read the output a line at a time - output it. */
    while (fgets(response_line, 250, fp) != NULL)
    {
      if (strlen(response_line) > 1)
       {
        strcpy(WebcamName, response_line);
      }
    }
    pclose(fp);
  }

  if (strcmp(WebcamName, "none") == 0)  // not detected previously, so try "046d:0821" for C910
  {
    fp = popen("v4l2-ctl --list-devices 2> /dev/null | sed -n '/046d:0821/,/dev/p' | grep 'dev' | tr -d '\t'", "r");
    if (fp == NULL)
    {
      printf("Failed to run command\n" );
      exit(1);
    }

    /* Read the output a line at a time - output it. */
    while (fgets(response_line, 250, fp) != NULL)
    {
      if (strlen(response_line) > 1)
       {
        strcpy(WebcamName, response_line);
      }
    }
    pclose(fp);
  }

  printf("Webcam device name is %s\n", WebcamName);

  // Now look for USB devices, but reject any lines that match the Webcam address
  fp = popen("v4l2-ctl --list-devices 2> /dev/null | sed -n '/usb/,/dev/p' | grep 'dev' | tr -d '\t'", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response_line, 250, fp) != NULL)
  {
    if (strlen(response_line) <= 1)
    {
        strcpy(VidDevName, "nil");
    }
    else
    {
      // printf("This time response was %s\n", response_line);
      
      if (strcmp(WebcamName, response_line) != 0) // If they don't match
      {
        strcpy(VidDevName, response_line);
      }
    }
  }

  printf("Video capture device name is %s\n", VidDevName);

  /* close */
  pclose(fp);
}


/***************************************************************************//**
 * @brief Detects if an Elgato CamLink 4K HDMI Dongle is currently connected
 *
 * @param nil
 *
 * @return 1 if connected, 0 if not connected
*******************************************************************************/

int CheckCamLink4K()
{
  FILE *fp;
  char response_line[255];

  // Read the CamLink address if it is present

  fp = popen("v4l2-ctl --list-devices 2> /dev/null | sed -n '/Cam Link 4K/,/dev/p' | grep 'dev' | tr -d '\t'", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  // Response is /dev/videox if present, null if not
  // So, if there is a response, return 1.

  /* Read the output a line at a time - output it. */
  while (fgets(response_line, 250, fp) != NULL)
  {
    if (strlen(response_line) > 1)
    {
      pclose(fp);
      return 1;
    }
  }
  pclose(fp);
  return 0;
}


/***************************************************************************//**
 * @brief Detects if a Logitech C 910, C525 C310 or C270 webcam was connected since last restart
 *
 * @param None
 *
 * @return 0 = webcam not detected
 *         1 = webcam detected
 *         2 = shell returned unexpected exit status
*******************************************************************************/
 
int DetectLogitechWebcam()
{
  char shell_command[255];
  // Pattern for C270, C310, C525 and C910
  char DMESG_PATTERN[63] = "046d:0825|046d:081b|Webcam C525|046d:0821";
  FILE * shell;
  sprintf(shell_command, "dmesg | grep -E -q \"%s\"", DMESG_PATTERN);
  shell = popen(shell_command, "r");
  int r = pclose(shell);
  if (WEXITSTATUS(r) == 0)
  {
    //printf("Logitech: webcam detected\n");
    return 1;
  }
  else if (WEXITSTATUS(r) == 1)
  {
    //printf("Logitech: webcam not detected\n");
    return 0;
  } 
  else 
  {
    //printf("Logitech: unexpected exit status %d\n", WEXITSTATUS(r));
    return 2;
  }
}


/***************************************************************************//**
 * @brief Detects if a Logitech C920 webcam is currently connected
 *
 * @param nil
 *
 * @return 1 if connected, 0 if not connected
*******************************************************************************/

int CheckC920()
{
  FILE *fp;
  char response_line[255];

  // Read the Webcam address if it is present

  fp = popen("v4l2-ctl --list-devices 2> /dev/null | sed -n '/C920/,/dev/p' | grep 'dev' | tr -d '\t'", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  // Response is /dev/videox if present, null if not
  // So, if there is a response, return 1.

  /* Read the output a line at a time - output it. */
  while (fgets(response_line, 250, fp) != NULL)
  {
    if (strlen(response_line) > 1)
    {
      pclose(fp);
      return 1;
    }
  }
  pclose(fp);
  return 0;
}

/***************************************************************************//**
 * @brief Checks the type of C920 Webcam
 *
 * @param nil
 *
 * @return 0 if not connected, 1 if old with H264 Encoder and 2 if Orbicam without encoder
 * and return 3 if newer C920 without encoder
*******************************************************************************/

int CheckC920Type()
{
  FILE *fp;
  char response_line[255];

  // lsusb response is Bus 00x Device 00x: ID 046d:082d Logitech, Inc. HD Pro Webcam C920 (old)
  // or          Bus 001 Device 00x: ID 046d:0892 Logitech, Inc. OrbiCam            (new)
  // or null

  fp = popen("lsusb | grep '046d:082d'", "r");  // Old C920
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response_line, 250, fp) != NULL)
  {
    if (strlen(response_line) > 1)
    {
      pclose(fp);
      return 1;
    }
  }

  fp = popen("lsusb | grep '046d:0892'", "r");  // Orbicam C920
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response_line, 250, fp) != NULL)
  {
    if (strlen(response_line) > 1)
    {
      pclose(fp);
      return 2;
    }
  }

  fp = popen("lsusb | grep '046d:08e5'", "r");  // Newer C920
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response_line, 250, fp) != NULL)
  {
    if (strlen(response_line) > 1)
    {
      pclose(fp);
      return 3;
    }
  }

  fp = popen("lsusb | grep '095d:3001'", "r");  // Polycom EagleEye Mini
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response_line, 250, fp) != NULL)
  {
    if (strlen(response_line) > 1)
    {
      pclose(fp);
      return 4;
    }
  }

  pclose(fp);
  return 0;
}

/***************************************************************************//**
 * @brief Looks Up the IP address that the Langstone is using for Pluto
 *
 * @param LangstonePlutoIP
 *
 * @return 0 if successful, 1 if not found
*******************************************************************************/

int CheckLangstonePlutoIP()
{
  FILE *fp;
  char response_line[255];

  if (strcmp(langstone_version, "v1pluto") == 0)
  {
    fp = popen("grep '^export PLUTO_IP=' /home/pi/Langstone/run | cut -c17-", "r");
    if (fp == NULL)
    {
      printf("Failed to run command\n" );
      return 1;
    }

    /* Read the output a line at a time - output it. */
    while (fgets(response_line, 250, fp) != NULL)
    {
      if (strlen(response_line) > 1)
      {
        response_line[strlen(response_line) - 1] = '\0';  // Strip trailing cr
        strcpy(LangstonePlutoIP, response_line);
      }
    }
    pclose(fp);
  }
  else if ((strcmp(langstone_version, "v2pluto") == 0)
        || (strcmp(langstone_version, "v2lime") == 0))
  {
    fp = popen("grep '^export PLUTO_IP=' /home/pi/Langstone/run_pluto | cut -c17-", "r");
    if (fp == NULL)
    {
      printf("Failed to run command\n" );
      return 1;
    }

    /* Read the output a line at a time - output it. */
    while (fgets(response_line, 250, fp) != NULL)
    {
      if (strlen(response_line) > 1)
      {
        response_line[strlen(response_line) - 1] = '\0';  // Strip trailing cr
        strcpy(LangstonePlutoIP, response_line);
      }
    }
    pclose(fp);
  }
  else
  {
    MsgBox2("Langstone not installed", "Please install Langstone, then set Pluto IP");
    wait_touch();
    return 1;
  }
  return 0;
}



/***************************************************************************//**
 * @brief Initialises all the GPIOs at startup
 *
 * @param None
 *
 * @return void
*******************************************************************************/

void InitialiseGPIO()
{
  // Called on startup to initialise the GPIO so that it works correctly
  // for spi commands when first used (it didn't always before)
  pinMode(GPIO_PTT, OUTPUT);
  digitalWrite(GPIO_PTT, LOW);
  pinMode(GPIO_SPI_CLK, OUTPUT);
  digitalWrite(GPIO_SPI_CLK, LOW);
  pinMode(GPIO_SPI_DATA, OUTPUT);
  digitalWrite(GPIO_SPI_DATA, LOW);
  pinMode(GPIO_4351_LE, OUTPUT);
  digitalWrite(GPIO_4351_LE, HIGH);
  pinMode(GPIO_Atten_LE, OUTPUT);
  digitalWrite(GPIO_Atten_LE, HIGH);
  pinMode(GPIO_5355_LE, OUTPUT);
  digitalWrite(GPIO_5355_LE, HIGH);
  pinMode(GPIO_Band_LSB, OUTPUT);
  pinMode(GPIO_Band_MSB, OUTPUT);
  pinMode(GPIO_Tverter, OUTPUT);
  digitalWrite(GPIO_Tverter, LOW);
}

/***************************************************************************//**
 * @brief Reads the Presets from portsdown_presets.txt and formats them for
 *        Display and switching
 *
 * @param None.  Works on global variables
 *
 * @return void
*******************************************************************************/

void ReadPresets()
{
  int n;
  char Param[255];
  char Value[255];
  char SRTag[9][255]={"psr1","psr2","psr3","psr4","psr5","psr6","psr7","psr8","psr9"};
  char FreqTag[9][255]={"pfreq1","pfreq2","pfreq3","pfreq4","pfreq5","pfreq6","pfreq7","pfreq8","pfreq9"};
  char SRValue[9][255];
  int len;

  // Read SRs
  for(n = 0; n < 9; n = n + 1)
  {
    strcpy(Param, SRTag[ n ]);
    GetConfigParam(PATH_PPRESETS,Param,Value);
    strcpy(SRValue[ n ], Value);
    TabSR[n] = atoi(SRValue[n]);
    if (TabSR[n] > 999)
    {
      strcpy(SRLabel[n], "SR");
      strcat(SRLabel[n], SRValue[n]);
    }
    else
    {
      strcpy(SRLabel[n], "SR ");
      strcat(SRLabel[n], SRValue[n]);
    }
  }

  // Read Frequencies
  for(n = 0; n < 9; n = n + 1)
  {
    strcpy(Param, FreqTag[ n ]);
    GetConfigParam(PATH_PPRESETS, Param, Value);
    strcpy(TabFreq[ n ], Value);
    len  = strlen(TabFreq[n]);
    switch (len)
    {
      case 2:
        strcpy(FreqLabel[n], " ");
        strcat(FreqLabel[n], TabFreq[n]);
        strcat(FreqLabel[n], " MHz ");
        break;
      case 3:
        strcpy(FreqLabel[n], TabFreq[n]);
        strcat(FreqLabel[n], " MHz ");
        break;
      case 4:
        strcpy(FreqLabel[n], TabFreq[n]);
        strcat(FreqLabel[n], " MHz");
        break;
      case 5:
        strcpy(FreqLabel[n], TabFreq[n]);
        strcat(FreqLabel[n], "MHz");
        break;
      default:
        strcpy(FreqLabel[n], TabFreq[n]);
        strcat(FreqLabel[n], " M");
        break;
    }
  }

  // Read Preset Config button labels
  for(n = 0; n < 4; n = n + 1)
  {
    strcpy(Value, "");
    snprintf(Param, 10, "p%dlabel", n + 1);
    GetConfigParam(PATH_PPRESETS, Param, Value);
    strcpy(TabPresetLabel[n], Value);
  }
}

/***************************************************************************//**
 * @brief Checks for the presence of an RTL-SDR
 *        
 * @param None
 *
 * @return 0 if present, 1 if not present
*******************************************************************************/

int CheckRTL()
{
  char RTLStatus[256];
  FILE *fp;
  int rtlstat = 1;

  /* Open the command for reading. */
  fp = popen("/home/pi/rpidatv/scripts/check_rtl.sh", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  // Read the output a line at a time - output it
  while (fgets(RTLStatus, sizeof(RTLStatus)-1, fp) != NULL)
  {
    if (RTLStatus[0] == '0')
    {
      printf("RTL Detected\n" );
      rtlstat = 0;
    }
    else
    {
      printf("No RTL Detected\n" );
    }
  }
  pclose(fp);
  return(rtlstat);
}


/***************************************************************************//**
 * @brief Checks for the presence of a Muntjac
 *        
 * @param None
 *
 * @return 0 if present, 1 if not present
*******************************************************************************/

int CheckMuntjac()
{
  char MuntjacStatus[255];
  FILE *fp;
  int muntjac_stat = 1;

  /* Open the command for reading. */
  fp = popen("/home/pi/rpidatv/scripts/check_muntjac.sh", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  // Read the output a line at a time - output it
  while (fgets(MuntjacStatus, sizeof(MuntjacStatus)-1, fp) != NULL)
  {
    if (MuntjacStatus[0] == '0')
    {
      //printf("Muntjac Detected\n" );
      muntjac_stat = 0;
    }
    else
    {
      //printf("No Muntjac Detected\n" );
    }
  }
  pclose(fp);
  return(muntjac_stat);
}


/***************************************************************************//**
 * @brief Creates Muntjac entry in /etc/udev/rules.d/99-usbserial.rules
 *        
 * @param None
 *
 * @return 0 if no error, 1 on error
*******************************************************************************/

int RegisterMuntjac()
{
  char MuntjacSerial[127];
  FILE *fp;
  char GrepCommand[127];
  char GrepResponse[255];
  char AppendCommand[300];

  if (CheckMuntjac() == 1)
  {
    return 1;
  }

  // Look up Muntjac Pico Serial Number
  fp = popen("dmesg | grep -A4 'idVendor=2e8a, idProduct=000a' | tail --lines=1 | sed -n -e 's/^.*SerialNumber: //p'", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    return 1;
  }

  // Read the output a line at a time - output it
  while (fgets(MuntjacSerial, sizeof(MuntjacSerial) - 1, fp) != NULL)
  {
    MuntjacSerial[strlen(MuntjacSerial) - 1] = '\0';
    //printf("\n Muntjac Serial is -%s-\n\n", MuntjacSerial );
  }

  pclose(fp);

  // Check if Muntjac serial is already in /etc/udev/rules.d/99-usbserial.rules

  snprintf(GrepCommand, 250, "grep %s /etc/udev/rules.d/99-usbserial.rules", MuntjacSerial);

  fp = popen(GrepCommand, "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    return 1;
  }

  // Read the output a line at a time - output it
  while (fgets(GrepResponse, sizeof(GrepResponse) - 1, fp) != NULL)
  {
    if (strlen(GrepResponse) > 5)
    {
      // File does not need amending
      //printf("Grep response %s, Serial %s found in 99-usbserial.rules file\n", GrepResponse, MuntjacSerial);
      return 0;
    }
  }

  // file needs new line appending and a reboot

  snprintf(AppendCommand, 300, "sudo sh -c \"echo ACTION==\\\\\\\"add\\\\\\\",ENV{ID_BUS}==\\\\\\\"usb\\\\\\\",ENV{ID_SERIAL_SHORT}==\\\\\\\"%s\\\\\\\",SYMLINK+=\\\\\\\"ttyMJ0\\\\\\\" >> /etc/udev/rules.d/99-usbserial.rules\"",  MuntjacSerial);
  //printf("\n%s\n\n", AppendCommand);
  system(AppendCommand);
  MsgBox4("System will reboot now", "to register new Muntjac", " ", "Touch screen to continue");
  wait_touch();
  system("sudo reboot now");

  return 0;
}



/***************************************************************************//**
 * @brief Checks for the presence of an FTDI Device
 *        
 * @param None
 *
 * @return 0 if present, 1 if not present
*******************************************************************************/

int CheckTuner()
{
  char FTDIStatus[256];
  FILE *fp;
  int ftdistat = 1;
  char response_line[255];

  /* Open the command for reading. */
  fp = popen("/home/pi/rpidatv/scripts/check_ftdi.sh", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  // Read the output a line at a time - output it
  while (fgets(FTDIStatus, sizeof(FTDIStatus)-1, fp) != NULL)
  {
    if (FTDIStatus[0] == '0')
    {
      printf("FTDI Detected\n" );
      ftdistat = 0;
    }
    else
    {
      printf("No FTDI Detected\n" );
    }
  }
  pclose(fp);

  // Check for PicoTuner if no MiniTiouner

  if (ftdistat != 0)
  {
    fp = popen("lsusb | grep '2e8a:ba2c'", "r");  // RPi Pico interface
    if (fp == NULL)
    {
      printf("Failed to run command\n" );
      exit(1);
    }

    /* Read the output a line at a time - output it. */
    while (fgets(response_line, 250, fp) != NULL)
    {
      if (strlen(response_line) > 1)
      {
        printf("PicoTuner Detected\n" );
        ftdistat = 0;
      }
    }
    pclose(fp);
  }

  return(ftdistat);
}

/***************************************************************************//**
 * @brief Uses keyboard to ask for a new Mic Gain setting
 *        
 * @param none
 *
 * @return void.  Sets global int RTLgain[0] in range 0 - 50
*******************************************************************************/

void ChangeMicGain()
{
  char RequestText[64];
  char InitText[64];

  //Define request string 
  strcpy(RequestText, "Enter new Mic gain setting 1 (min) to 30 (max):");
  snprintf(InitText, 3, "%d", MicLevel);

  // Ask for response and check validity
  strcpy(KeyboardReturn, "31");
  while ((atoi(KeyboardReturn) < 1) || (atoi(KeyboardReturn) > 30))
  {
    Keyboard(RequestText, InitText, 3);
  }

  // Store Response
  MicLevel = atoi(KeyboardReturn);

  // Write response to file
  SetConfigParam(PATH_PCONFIG, "micgain", KeyboardReturn);
}


/***************************************************************************//**
 * @brief Saves RTL-FM Freq, Mode, squelch and label
 *        
 * @param Preset button number 0-9, but not 4
 *
 * @return void
*******************************************************************************/

void SaveRTLPreset(int PresetButton)
{
  char Param[255];
  char Value[255];
  char Prompt[63];
  int index;
  int Spaces;
  int j;

  // Transform button number into preset index
  index = PresetButton + 6;  // works for bottom row
  if (PresetButton > 4)      // second row
  {
    index = PresetButton - 4;
  }

  if (index != 0)
  {
    // Read the current preset label and ask for a new value
    snprintf(Prompt, 62, "Enter the new label for RTL Preset %d (no spaces):", index);

    // Check that there are no spaces
    while (Spaces >= 1)
    {
      Keyboard(Prompt, RTLlabel[index], 10);

      // Check that there are no spaces
      Spaces = 0;
      for (j = 0; j < strlen(KeyboardReturn); j = j + 1)
      {
        if (isspace(KeyboardReturn[j]))
        {
          Spaces = Spaces +1;
        }
      }
    }
    strcpy(RTLlabel[index], KeyboardReturn);
    snprintf(Param, 10, "r%dtitle", index);
    SetConfigParam(PATH_RTLPRESETS, Param, KeyboardReturn);
  }

  // Copy the current values into the presets arrays

  strcpy(RTLfreq[index], RTLfreq[0]);
  strcpy(RTLmode[index], RTLmode[0]);
  RTLsquelch[index] = RTLsquelch[0];
  RTLgain[index] = RTLgain[0];

  // Save the current values into the presets File

  snprintf(Param, 10, "r%dfreq", index);
  SetConfigParam(PATH_RTLPRESETS, Param, RTLfreq[0]);

  snprintf(Param, 10, "r%dmode", index);
  SetConfigParam(PATH_RTLPRESETS, Param, RTLmode[index]);

  snprintf(Param, 10, "r%dsquelch", index);
  snprintf(Value, 4, "%d", RTLsquelch[0]);
  SetConfigParam(PATH_RTLPRESETS, Param, Value);

  snprintf(Param, 10, "r%dgain", index);
  snprintf(Value, 4, "%d", RTLgain[0]);
  SetConfigParam(PATH_RTLPRESETS, Param, Value);
}

/***************************************************************************//**
 * @brief Loads RTL-FM Freq, Mode, squelch and label from in-use variables
 *        
 * @param Preset button number 0-9, but not 4
 *
 * @return void
*******************************************************************************/

void RecallRTLPreset(int PresetButton)
{
  int index;

  // Transform button number into preset index
  index = PresetButton + 6;  // works for bottom row
  if (PresetButton > 4)      // second row
  {
    index = PresetButton - 4;
  }

  // Copy stored parameters into in-use parameters
  strcpy(RTLfreq[0], RTLfreq[index]);
  strcpy(RTLlabel[0], RTLlabel[index]);
  strcpy(RTLmode[0], RTLmode[index]);
  RTLsquelch[0] = RTLsquelch[index];
  RTLgain[0] = RTLgain[index];
}

/***************************************************************************//**
 * @brief Uses keyboard to ask for a new RTL-FM frequency
 *        
 * @param none
 *
 * @return void.  Sets global RTLfreq[0]
*******************************************************************************/

void ChangeRTLFreq()
{
  char RequestText[64];
  char InitText[64];

  //Define request string 
  strcpy(RequestText, "Enter new frequency in MHz:");
  strcpy(InitText, RTLfreq[0]);

  // Ask for response and check validity
  strcpy(KeyboardReturn, "2500");
  while ((atof(KeyboardReturn) < 1) || (atof(KeyboardReturn) > 2000))
  {
    Keyboard(RequestText, InitText, 11);
  }

  // Store Response
  strcpy(RTLfreq[0], KeyboardReturn);
}

/***************************************************************************//**
 * @brief Uses keyboard to ask for a new RTL-FM Squelch setting
 *        
 * @param none
 *
 * @return void.  Sets global int RTLsquelch[0] in range 0 - 1000
*******************************************************************************/

void ChangeRTLSquelch()
{
  char RequestText[64];
  char InitText[64];

  //Define request string 
  strcpy(RequestText, "Enter new squelch setting 0 (off) to 1000 (fully on):");
  snprintf(InitText, 4, "%d", RTLsquelch[0]);

  // Ask for response and check validity
  strcpy(KeyboardReturn, "2000");
  while ((atoi(KeyboardReturn) < 0) || (atoi(KeyboardReturn) > 1000))
  {
    Keyboard(RequestText, InitText, 3);
  }

  // Store Response
  RTLsquelch[0] = atoi(KeyboardReturn);
}

/***************************************************************************//**
 * @brief Uses keyboard to ask for a new RTL-FM Gain setting
 *        
 * @param none
 *
 * @return void.  Sets global int RTLgain[0] in range 0 - 50
 *                or if gain set to auto returns -1
*******************************************************************************/

void ChangeRTLGain()
{
  char RequestText[64];
  char InitText[64];

  //Define request string 
  strcpy(RequestText, "Enter new gain setting 0 (min) to 50 (max) or auto:");

  if (RTLgain[0] < 0)
  {
    strcpy(InitText, "auto");
  }
  else
  {
    snprintf(InitText, 3, "%d", RTLgain[0]);
  }

  // Ask for response and check validity
  strcpy(KeyboardReturn, "60");
  while ((atoi(KeyboardReturn) < 0) || (atoi(KeyboardReturn) > 50))  // 0 to 50 or alpha text
  {
    Keyboard(RequestText, InitText, 4);
  }

  // Store Response
  if ((KeyboardReturn[0] == 'a') || (KeyboardReturn[0] == 'A')) // auto
  {
    RTLgain[0] = -1;
  }
  else
  {
    RTLgain[0] = atoi(KeyboardReturn);
  }
}

/***************************************************************************//**
 * @brief Uses keyboard to ask for a new RTL-FM ppm setting
 *        
 * @param none
 *
 * @return void.  Sets global int RTLppm in range -1000 - 1000
*******************************************************************************/

void ChangeRTLppm()
{
  char RequestText[64];
  char InitText[64];
  char Value[15];
  char Param[15];

  //Define request string 
  strcpy(RequestText, "Enter new ppm setting -1000 to 1000:");
  snprintf(InitText, 4, "%d", RTLppm);

  // Ask for response and check validity
  strcpy(KeyboardReturn, "2000");
  while ((atoi(KeyboardReturn) < -1000) || (atoi(KeyboardReturn) > 1000))
  {
    Keyboard(RequestText, InitText, 4);
  }

  // Store Response in global variable
  RTLppm = atoi(KeyboardReturn);

  // Save Response in file
  strcpy(Param, "roffset");
  snprintf(Value, 6, "%d", RTLppm);
  SetConfigParam(PATH_RTLPRESETS, Param, Value);
}

/***************************************************************************//**
 * @brief Detects if a LimeNET Micro is connected
 *
 * @param None
 *
 * @return 0 = LimeNET Micro not detected
 *         1 = LimeNET Micro detected
 *         2 = shell returned unexpected exit status
*******************************************************************************/
 
int DetectLimeNETMicro()
{
  int r;

  r = WEXITSTATUS(system("cat /proc/device-tree/model | grep 'Raspberry Pi Compute Module 3'"));

  if (r == 0)
  {
    // printf("LimeNET Micro detected\n");
    return 1;
  }
  else if (r == 1)
  {
    // printf("LimeNET Micro not detected\n");
    return 0;
  } 
  else 
  {
    // printf("LimeNET Micro unexpected exit status %d\n", r);
    return 2;
  }
}


/***************************************************************************//**
 * @brief Reads the Presets from rtl-fm_config.txt and formats them for
 *        Display and switching
 *
 * @param None.  Works on global variables
 *
 * @return void
*******************************************************************************/

void ReadRTLPresets()
{
  int n;
  char Value[15] = "";
  char Param[15];

  for(n = 0; n < 10; n = n + 1)
  {
    // Title
    snprintf(Param, 10, "r%dtitle", n);
    GetConfigParam(PATH_RTLPRESETS, Param, Value);
    strcpy(RTLlabel[n], Value);

    // Frequency
    snprintf(Param, 10, "r%dfreq", n);
    GetConfigParam(PATH_RTLPRESETS, Param, Value);
    strcpy(RTLfreq[n], Value);

    // Mode
    snprintf(Param, 10, "r%dmode", n);
    GetConfigParam(PATH_RTLPRESETS, Param, Value);
    strcpyn(RTLmode[n], Value, 4);

    // Squelch
    snprintf(Param, 10, "r%dsquelch", n);
    GetConfigParam(PATH_RTLPRESETS, Param, Value);
    RTLsquelch[n] = atoi(Value);

    // Gain
    snprintf(Param, 10, "r%dgain", n);
    GetConfigParam(PATH_RTLPRESETS, Param, Value);
    RTLgain[n] = atoi(Value);
  }

  // Frequency correction ppm
  strcpy(Param, "roffset");
  GetConfigParam(PATH_RTLPRESETS, Param, Value);
  RTLppm = atoi(Value);
}

/***************************************************************************//**
 * @brief Reads the Presets from rx_config.txt and formats them for
 *        Display and switching
 *
 * @param None.  Works on global variables
 *
 * @return void
*******************************************************************************/

void ReadRXPresets()
{
  int n;
  char Value[15] = "";
  char Param[20];

  for(n = 0; n < 5; n = n + 1)
  {
    // Frequency
    snprintf(Param, 15, "rx%dfrequency", n);
    GetConfigParam(PATH_RXPRESETS, Param, Value);
    strcpy(RXfreq[n], Value);

    // SR
    snprintf(Param, 15, "rx%dsr", n);
    GetConfigParam(PATH_RXPRESETS, Param, Value);
    RXsr[n] = atoi(Value);

    // FEC
    snprintf(Param, 15, "rx%dfec", n);
    GetConfigParam(PATH_RXPRESETS, Param, Value);
    strcpy(RXfec[n], Value);

    // Sample Rate
    snprintf(Param, 15, "rx%dsamplerate", n);
    GetConfigParam(PATH_RXPRESETS, Param, Value);
    RXsamplerate[n] = atoi(Value);

    // Gain
    snprintf(Param, 15, "rx%dgain", n);
    GetConfigParam(PATH_RXPRESETS, Param, Value);
    RXgain[n] = atoi(Value);

    // Modulation
    snprintf(Param, 15, "rx%dmodulation", n);
    GetConfigParam(PATH_RXPRESETS, Param, Value);
    strcpy(RXmodulation[n], Value);

    // Encoding
    snprintf(Param, 15, "rx%dencoding", n);
    GetConfigParam(PATH_RXPRESETS, Param, Value);
    strcpy(RXencoding[n], Value);

    // SDR Type
    snprintf(Param, 15, "rx%dsdr", n);
    GetConfigParam(PATH_RXPRESETS, Param, Value);
    strcpy(RXsdr[n], Value);

    // Graphics on/off
    snprintf(Param, 15, "rx%dgraphics", n);
    GetConfigParam(PATH_RXPRESETS, Param, Value);
    strcpy(RXgraphics[n], Value);

    // Parameters on/off
    snprintf(Param, 15, "rx%dparameters", n);
    GetConfigParam(PATH_RXPRESETS, Param, Value);
    strcpy(RXparams[n], Value);

    // Sound on/off
    snprintf(Param, 15, "rx%dsound", n);
    GetConfigParam(PATH_RXPRESETS, Param, Value);
    strcpy(RXsound[n], Value);

    // Fastlock on/off
    snprintf(Param, 15, "rx%dfastlock", n);
    GetConfigParam(PATH_RXPRESETS, Param, Value);
    strcpy(RXfastlock[n], Value);

    // Label
    snprintf(Param, 12, "rx%dlabel", n);
    GetConfigParam(PATH_RXPRESETS, Param, Value);
    strcpy(RXlabel[n], Value);
  }
}

/***************************************************************************//**
 * @brief Reads the Presets from longmynd_config.txt and formats them for
 *        Display and switching.  Only called on entry to Menu 8
 *
 * @param None.  Works on global variables
 *
 * @return void
*******************************************************************************/

void ReadLMRXPresets()
{
  int n;
  char Value[15] = "";
  char Param[20];

  // Mode: sat or terr
  GetConfigParam(PATH_LMCONFIG, "mode", LMRXmode);

  // UDP output IP address:
  GetConfigParam(PATH_LMCONFIG, "udpip", LMRXudpip);
  
  // UDP output port:
  GetConfigParam(PATH_LMCONFIG, "udpport", LMRXudpport);

  // Audio output port: (rpi or usb or hdmi)
  GetConfigParam(PATH_LMCONFIG, "audio", LMRXaudio);

  // QO-100 LNB Offset:
  GetConfigParam(PATH_LMCONFIG, "qoffset", Value);
  LMRXqoffset = atoi(Value);

  if (strcmp(LMRXmode, "sat") == 0)
  {
    // Input: a or b
    GetConfigParam(PATH_LMCONFIG, "input", LMRXinput);

    // Start up frequency
    GetConfigParam(PATH_LMCONFIG, "freq0", Value);
    LMRXfreq[0] = atoi(Value);

    // Start up SR
    GetConfigParam(PATH_LMCONFIG, "sr0", Value);
    LMRXsr[0] = atoi(Value);
  }
  else    // Terrestrial
  {
    // Input: a or b
    GetConfigParam(PATH_LMCONFIG, "input1", LMRXinput);

    // Start up frequency
    GetConfigParam(PATH_LMCONFIG, "freq1", Value);
    LMRXfreq[0] = atoi(Value);

    // Start up SR
    GetConfigParam(PATH_LMCONFIG, "sr1", Value);
    LMRXsr[0] = atoi(Value);
  }

  // Frequencies
  for(n = 1; n < 11; n = n + 1)
  {
    // QO-100
    snprintf(Param, 15, "qfreq%d", n);
    GetConfigParam(PATH_LMCONFIG, Param, Value);
    LMRXfreq[n] = atoi(Value);

    // Terrestrial
    snprintf(Param, 15, "tfreq%d", n);
    GetConfigParam(PATH_LMCONFIG, Param, Value);
    LMRXfreq[n + 10] = atoi(Value);
  }

  // Symbol Rates
  for(n = 1; n < 7; n = n + 1)
  {
    // QO-100
    snprintf(Param, 15, "qsr%d", n);
    GetConfigParam(PATH_LMCONFIG, Param, Value);
    LMRXsr[n] = atoi(Value);

    // Terrestrial
    snprintf(Param, 15, "tsr%d", n);
    GetConfigParam(PATH_LMCONFIG, Param, Value);
    LMRXsr[n + 6] = atoi(Value);
  }

  // Modulation
  GetConfigParam(PATH_LMCONFIG, "rxmod", Value);
  if (strcmp(Value, "dvbt") == 0)
  {
    strcpy(RXmod, "DVB-T");
  }
  else
  {
    strcpy(RXmod, "DVB-S");
  }

  // Time Overlay
  strcpy(Value, "off");
  GetConfigParam(PATH_PCONFIG, "timeoverlay", Value);
  if (strcmp(Value, "off") == 0)
  {
    timeOverlay = false;
  }
  else
  {
    timeOverlay = true;
  }
}

void ChangeLMRXIP()
{
  char RequestText[64];
  char InitText[64];
  bool IsValid = FALSE;
  char LMRXIP[31];
  char LMRXIPCopy[31];

  //Retrieve (17 char) Current IP from Config file
  GetConfigParam(PATH_LMCONFIG, "udpip", LMRXIP);

  while (IsValid == FALSE)
  {
    strcpy(RequestText, "Enter the new UDP IP Destination for the RX TS");
    //snprintf(InitText, 17, "%s", LMRXIP);
    strcpyn(InitText, LMRXIP, 17);
    Keyboard(RequestText, InitText, 17);
  
    strcpy(LMRXIPCopy, KeyboardReturn);
    if(is_valid_ip(LMRXIPCopy) == 1)
    {
      IsValid = TRUE;
    }
  }
  printf("Receiver UDP IP Destination set to: %s\n", KeyboardReturn);

  // Save IP to Local copy and Config File
  strcpy(LMRXudpip, KeyboardReturn);
  SetConfigParam(PATH_LMCONFIG, "udpip", LMRXudpip);
}

void ChangeLMRXPort()
{
  char RequestText[63];
  char InitText[63];
  bool IsValid = FALSE;
  char LMRXPort[15];

  //Retrieve (10 char) Current port from Config file
  GetConfigParam(PATH_LMCONFIG, "udpport", LMRXPort);

  while (IsValid == FALSE)
  {
    strcpy(RequestText, "Enter the new UDP Port Number for the RX TS");
    //snprintf(InitText, 10, "%s", LMRXPort);
    strcpyn(InitText, LMRXPort, 10);
    Keyboard(RequestText, InitText, 10);
  
    if(strlen(KeyboardReturn) > 0)
    {
      IsValid = TRUE;
    }
  }
  printf("LMRX UDP Port set to: %s\n", KeyboardReturn);

  // Save port to local copy and Config File
  strcpy(LMRXudpport, KeyboardReturn);
  SetConfigParam(PATH_LMCONFIG, "udpport", LMRXudpport);
}

void ChangeLMRXOffset()
{
  char RequestText[64];
  char InitText[64];
  bool IsValid = FALSE;
  char LMRXOffset[15];

  //Retrieve (10 char) Current offset from Config file
  GetConfigParam(PATH_LMCONFIG, "qoffset", LMRXOffset);

  while (IsValid == FALSE)
  {
    strcpy(RequestText, "Enter the new QO-100 LNB Offset in kHz");
    //snprintf(InitText, 10, "%s", LMRXOffset);
    strcpyn(InitText, LMRXOffset, 10);
    Keyboard(RequestText, InitText, 10);
  
    if((atoi(KeyboardReturn) > 1000000) && (atoi(KeyboardReturn) < 76000000))
    {
      IsValid = TRUE;
    }
  }
  printf("LMRXOffset set to: %s kHz\n", KeyboardReturn);

  // Save offset to Config File
  LMRXqoffset = atoi(KeyboardReturn);
  SetConfigParam(PATH_LMCONFIG, "qoffset", KeyboardReturn);
}

void ChangeLMTST()
{
  char RequestText[64];
  char InitText[64];
  bool IsValid = FALSE;
  char LMTST[15];

  //  Retrieve (6 char) Current Sat or Terr Timeout from Config file
  if (strcmp(LMRXmode, "sat") == 0)
  {
    GetConfigParam(PATH_LMCONFIG, "tstimeout", LMTST);
    strcpy(RequestText, "Enter the new Tuner Timeout for QO-100 in ms");
  }
  else  //Terrestrial
  {
    GetConfigParam(PATH_LMCONFIG, "tstimeout1", LMTST);
    strcpy(RequestText, "Enter the new Tuner Timeout for terrestrial in ms");
  }

  while (IsValid == FALSE)
  {
    strcpyn(InitText, LMTST, 10);
    Keyboard(RequestText, InitText, 10);
  
    if (((atoi(KeyboardReturn) >= 500) && (atoi(KeyboardReturn) <= 60000)) || (atoi(KeyboardReturn) == -1))
    {
      IsValid = TRUE;
    }
  }
  printf("Tuner Timeout set to: %s ms\n", KeyboardReturn);

  // Save offset to Config File
  if (strcmp(LMRXmode, "sat") == 0)
  {
    SetConfigParam(PATH_LMCONFIG, "tstimeout", KeyboardReturn);
  }
  else
  {
    SetConfigParam(PATH_LMCONFIG, "tstimeout1", KeyboardReturn);
  }
}

void ChangeLMSW()
{
  char RequestText[64];
  char InitText[64];
  bool IsValid = FALSE;
  char LMSW[15];

  //  Retrieve (5 char) Current Sat or Terr Scan Width from Config file
  if (strcmp(LMRXmode, "sat") == 0)
  {
    GetConfigParam(PATH_LMCONFIG, "scanwidth", LMSW);
    strcpy(RequestText, "Enter scan half-width as % of SR for QO-100");
  }
  else  //Terrestrial
  {
    GetConfigParam(PATH_LMCONFIG, "scanwidth1", LMSW);
    strcpy(RequestText, "Enter scan half-width as % of SR for terrestrial");
  }

  while (IsValid == FALSE)
  {
    strcpyn(InitText, LMSW, 10);
    Keyboard(RequestText, InitText, 10);
  
    if((atoi(KeyboardReturn) >= 10) && (atoi(KeyboardReturn) <= 500))
    {
      IsValid = TRUE;
    }
  }
  printf("Scan Width set to: %s %% of SR\n", KeyboardReturn);

  // Save scan width to Config File
  if (strcmp(LMRXmode, "sat") == 0)
  {
    SetConfigParam(PATH_LMCONFIG, "scanwidth", KeyboardReturn);
  }
  else
  {
    SetConfigParam(PATH_LMCONFIG, "scanwidth1", KeyboardReturn);
  }
}

void ChangeLMChan()
{
  char RequestText[64];
  char InitText[64];
  bool IsValid = FALSE;
  char LMChan[7];
  char IntCheck[64];

  //  Retrieve (1 char) Current Sat or Terr channel from Config file
  if (strcmp(LMRXmode, "sat") == 0)
  {
    GetConfigParam(PATH_LMCONFIG, "chan", LMChan);
    strcpy(RequestText, "Enter TS Video Channel, 0 for default");
  }
  else  //Terrestrial
  {
    GetConfigParam(PATH_LMCONFIG, "chan1", LMChan);
    strcpy(RequestText, "Enter TS Video Channel, 0 for default");
  }

  while (IsValid == FALSE)
  {
    strcpyn(InitText, LMChan, 10);
    Keyboard(RequestText, InitText, 10);

    if(strcmp(KeyboardReturn, "") == 0)  // Set null to 0
    {
      strcpy(KeyboardReturn, "0");
    }

    snprintf(IntCheck, 9, "%d", atoi(KeyboardReturn));
    if(strcmp(IntCheck, KeyboardReturn) == 0)           // Check answer is an integer
    {
      if((atoi(KeyboardReturn) >= 0) && (atoi(KeyboardReturn) <= 64000))
      {
        IsValid = TRUE;
      }
    }
  }
  printf("Video Channel %s selected\n", KeyboardReturn);

  // Save channel to Config File
  if (strcmp(LMRXmode, "sat") == 0)
  {
    SetConfigParam(PATH_LMCONFIG, "chan", KeyboardReturn);
  }
  else
  {
    SetConfigParam(PATH_LMCONFIG, "chan1", KeyboardReturn);
  }
}

void AutosetLMRXOffset()
{
  LMRX(5);
}


/***************************************************************************//**
 * @brief Saves Current LeanDVB Config to file as Preset 0
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/

void SaveCurrentRX()
{
  char Param[255];
  char Value[255];
  int index = 0;

  // Save the current values into the presets File
  snprintf(Param, 15, "rx%dfrequency", index);          // Frequency
  SetConfigParam(PATH_RXPRESETS, Param, RXfreq[0]);

  snprintf(Param, 15, "rx%dlabel", index);              // Label
  SetConfigParam(PATH_RXPRESETS, Param, RXlabel[0]);
    
  snprintf(Param, 15, "rx%dsr", index);                 // SR
  snprintf(Value, 5, "%d", RXsr[0]);
  SetConfigParam(PATH_RXPRESETS, Param, Value);

  snprintf(Param, 15, "rx%dfec", index);                // FEC
  SetConfigParam(PATH_RXPRESETS, Param, RXfec[0]);

  snprintf(Param, 15, "rx%dsamplerate", index);         // Sample Rate
  snprintf(Value, 5, "%d", RXsamplerate[0]);
  SetConfigParam(PATH_RXPRESETS, Param, Value);

  snprintf(Param, 15, "rx%dgain", index);               // Gain
  snprintf(Value, 5, "%d", RXgain[0]);
  SetConfigParam(PATH_RXPRESETS, Param, Value);

  snprintf(Param, 15, "rx%dmodulation", index);         // Modulation
  SetConfigParam(PATH_RXPRESETS, Param, RXmodulation[0]);

  snprintf(Param, 15, "rx%dencoding", index);           // Encoding
  SetConfigParam(PATH_RXPRESETS, Param, RXencoding[0]);

  snprintf(Param, 15, "rx%dsdr", index);                // SDR Type
  SetConfigParam(PATH_RXPRESETS, Param, RXsdr[0]);

  snprintf(Param, 15, "rx%dgraphics", index);           // Graphics on/off
  SetConfigParam(PATH_RXPRESETS, Param, RXgraphics[0]);

  snprintf(Param, 15, "rx%dparameters", index);         // Parameters on/off
  SetConfigParam(PATH_RXPRESETS, Param, RXparams[0]);

  snprintf(Param, 15, "rx%dsound", index);              // Sound on/off
  SetConfigParam(PATH_RXPRESETS, Param, RXsound[0]);

  snprintf(Param, 15, "rx%dfastlock", index);           // Fastlock on/off
  SetConfigParam(PATH_RXPRESETS, Param, RXfastlock[0]);
}


/***************************************************************************//**
 * @brief Sets the RTLmode after a button press
 *
 * @param NoButton = button pressed in range 10 - 14  
 *
 * @return void. Works on the global variable RTLmode[0]
*******************************************************************************/

void SelectRTLmode(int NoButton)
{
  switch (NoButton)
  {
  case 10:
    strcpy(RTLmode[0], "am");
    break;
  case 11:
    strcpy(RTLmode[0], "fm");
    break;
  case 12:
    strcpy(RTLmode[0], "wbfm");
    break;
  case 13:
    strcpy(RTLmode[0], "usb");
    break;
  case 14:
    strcpy(RTLmode[0], "lsb");
    break;
  }
}

/***************************************************************************//**
 * @brief Starts the RTL Receiver using the stored parameters
 *
 * @param nil.  Works on Globals
 *
 * @return void. 
*******************************************************************************/

void RTLstart()
{
  char fragment[31];
  char fragment11[12];
  
  if(RTLdetected == 1)
  {
    char rtlcall[255];
    char card[15];
    char mic[15];
    char hdmi[15];

    if (strcmp(LMRXaudio, "rpi") == 0)
    {
      GetPiAudioCard(card);
    }
    else if (strcmp(LMRXaudio, "usb") == 0)
    {
      GetMicAudioCard(mic);
      strcpy(card, mic);
    }
    else  // hdmi 0
    {
      GetHDMIAudioCard(hdmi);
      strcpy(card, hdmi);
    }

    strcpy(rtlcall, "(rtl_fm");
    snprintf(fragment, 12, " -M %s", RTLmode[0]);  // -M mode
    strcat(rtlcall, fragment);
    strcpyn(fragment11, RTLfreq[0], 11);
    snprintf(fragment, 18, " -f %sM", fragment11); // -f frequencyM
    strcat(rtlcall, fragment);
    if (strcmp(RTLmode[0], "am") == 0)
    {
      strcat(rtlcall, " -s 12k");
    }
    if (strcmp(RTLmode[0], "fm") == 0)
    {
      strcat(rtlcall, " -s 12k");
    }
    if (RTLgain[0] >= 0)                           // default is auto (-1) so only set gain if positive
    {
      snprintf(fragment, 12, " -g %d", RTLgain[0]); // -g gain
      strcat(rtlcall, fragment);
    }
    if (RTLsquelchoveride == 1)
    {
      snprintf(fragment, 12, " -l %d", RTLsquelch[0]); // -l squelch
      strcat(rtlcall, fragment);
    }
    snprintf(fragment, 12, " -p %d", RTLppm); // -p ppm_error
    strcat(rtlcall, fragment);
    strcpy(fragment, " -E pad"); // -E pad so that aplay does not crash
    strcat(rtlcall, fragment);
    strcat(rtlcall, " | aplay -D plughw:");
    strcat(rtlcall, card);
    if (strcmp(RTLmode[0], "am") == 0)
    {
      strcat(rtlcall, ",0 -f S16_LE -r12) &"); // 12 KHz for AM
    }
    if (strcmp(RTLmode[0], "fm") == 0)
    {
      strcat(rtlcall, ",0 -f S16_LE -r12) &"); // 12 KHz for FM
    }
    if (strcmp(RTLmode[0], "wbfm") == 0)
    {
      strcat(rtlcall, ",0 -f S16_LE -r32) &"); // 32 KHz for WBFM
    }
    if ((strcmp(RTLmode[0], "usb") == 0) || (strcmp(RTLmode[0], "lsb") == 0))
    {
      strcat(rtlcall, ",0 -f S16_LE -r6) &"); // 6 KHz for SSB
    }
    printf("RTL_FM called with: %s\n", rtlcall);
    system(rtlcall);
  }
  else
  {
    MsgBox("No RTL-SDR Connected");
    wait_touch();
  }
}

/***************************************************************************//**
 * @brief Stops the RTL Receiver and cleans up
 *
 * @param nil
 *
 * @return void. 
*******************************************************************************/

void RTLstop()
{
  system("sudo killall rtl_fm >/dev/null 2>/dev/null");
  system("sudo killall aplay >/dev/null 2>/dev/null");
  usleep(1000);
  system("sudo killall -9 rtl_fm >/dev/null 2>/dev/null");
  system("sudo killall -9 aplay >/dev/null 2>/dev/null");
}

/***************************************************************************//**
 * @brief Reads the Presets from stream_presets.txt and formats them for
 *        Display and switching
 *
 * @param None.  Works on global variables
 *
 * @return void
*******************************************************************************/

void ReadStreamPresets()
{
  int n;
  char Value[127] = "";
  char Param[20];

  // Read in Stream Receive presets
  for(n = 0; n < 9; n = n + 1)
  {
    // Stream Address
    snprintf(Param, 15, "stream%d", n);
    GetConfigParam(PATH_STREAMPRESETS, Param, Value);
    strcpy(StreamAddress[n], Value);

    // Stream Button Label
    snprintf(Param, 12, "label%d", n);
    GetConfigParam(PATH_STREAMPRESETS, Param, Value);
    strcpy(StreamLabel[n], Value);
  }

  // Read in Streamer Transmit presets
  for(n = 1; n < 9; n = n + 1)
  {
    // Streamer URL
    snprintf(Param, 15, "streamurl%d", n);
    GetConfigParam(PATH_STREAMPRESETS, Param, Value);
    strcpy(StreamURL[n], Value);

    // Streamname-key
    snprintf(Param, 15, "streamkey%d", n);
    GetConfigParam(PATH_STREAMPRESETS, Param, Value);
    strcpy(StreamKey[n], Value);
  }

  // Read in current setting
  GetConfigParam(PATH_PCONFIG, "streamurl", Value);
  strcpy(StreamURL[0], Value);

  GetConfigParam(PATH_PCONFIG, "streamkey", Value);
  strcpy(StreamKey[0], Value);

  // If preset 1 undefined, and current setting is defined,
  // read current settings into Preset 1
  if (((strcmp(StreamKey[1], "callsign-keykey") == 0) && (strcmp(StreamKey[0], "callsign-keykey") != 0)))
  {
    strcpy(StreamURL[1], StreamURL[0]);
    SetConfigParam(PATH_STREAMPRESETS, "streamurl1", StreamURL[0]);
    strcpy(StreamKey[1], StreamKey[0]);
    SetConfigParam(PATH_STREAMPRESETS, "streamkey1", StreamKey[0]);
  }
}


/***************************************************************************//**
 * @brief Checks whether the DATV Express is connected
 *
 * @param 
 *
 * @return 0 if present, 1 if absent
*******************************************************************************/

int CheckExpressConnect()
{
  FILE *fp;
  char response[255];
  int responseint;

  /* Open the command for reading. */
  fp = popen("lsusb | grep -q 'CY7C68013' ; echo $?", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response, 7, fp) != NULL)
  {
    responseint = atoi(response);
  }

  /* close */
  pclose(fp);
  return responseint;
}

/***************************************************************************//**
 * @brief Checks whether the DATV Express Server is Running
 *
 * @param 
 *
 * @return 0 if running, 1 if not running
*******************************************************************************/

int CheckExpressRunning()
{
  FILE *fp;
  char response[255];
  int responseint;

  /* Open the command for reading. */
  fp = popen("pgrep -x 'express_server' ; echo $?", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response, 7, fp) != NULL)
  {
    responseint = atoi(response);
  }

  /* close */
  pclose(fp);
  return responseint;
}


/***************************************************************************//**
 * @brief Called to start the DATV Express Server
 *
 * @param 
 *
 * @return 0 if running OK, 1 if not running
*******************************************************************************/

int StartExpressServer()
{
  char BashText[255];
  int responseint;
  // Check if DATV Express is connected
  if (CheckExpressConnect() == 1)   // Not connected
  {
    MsgBox2("DATV Express Not connected", "Connect it or select another mode");
    wait_touch();
  }

  // Check if the server is already running
  if (CheckExpressRunning() == 1)   // Not running, so start it
  {
    // Make sure the control file is not locked by deleting it
    strcpy(BashText, "sudo rm /tmp/expctrl >/dev/null 2>/dev/null");
    system(BashText);

    // Start the server
    strcpy(BashText, "cd /home/pi/express_server; ");
    strcat(BashText, "sudo nice -n -40 /home/pi/express_server/express_server  >/dev/null 2>/dev/null &");
    system(BashText);
    strcpy(BashText, "cd /home/pi");
    system(BashText);
    MsgBox4("Please wait 5 seconds", "while the DATV Express firmware", "is loaded", " ");
    usleep(5000000);
    responseint = CheckExpressRunning();
    if (responseint == 0)  // Running OK
    {
      MsgBox4(" ", " ", " ", "DATV Express Firmware Loaded");
      usleep(1000000);
    }
    else
    {
      MsgBox4("Failed to load", "DATV Express firmware.", "Please check connections", "and try again");
      wait_touch();
    }
    setBackColour(0, 0, 0);
    clearScreen();
  }
  else
  {
    responseint = 0;
  }
  return responseint;
}

/***************************************************************************//**
 * @brief Detects if a USB Audio device is connected
 *
 * @param None
 *
 * @return 0 = USB Audio detected
 *         1 = USB Audio not detected
 *         2 = shell returned unexpected exit status
*******************************************************************************/
 
int DetectUSBAudio()
{
  char shell_command[255];
  // Pattern for USB Audio Dongle; others can be added with |xxxx
  char DMESG_PATTERN[63] = "USB Audio";
  FILE * shell;
  sprintf(shell_command, "aplay -l | grep -E -q \"%s\"", DMESG_PATTERN);
  shell = popen(shell_command, "r");
  int r = pclose(shell);
  if (WEXITSTATUS(r) == 0)
  {
    //printf("USB Audio detected\n");
    return 0;
  }
  else if (WEXITSTATUS(r) == 1)
  {
    //printf("USB Audio not detected\n");
    return 1;
  } 
  else 
  {
    //printf("USB Audio: unexpected exit status %d\n", WEXITSTATUS(r));
    return 2;
  }
}


/***************************************************************************//**
 * @brief Called on GUI start to check if the DATV Express Server needs to be started
 *
 * @param nil
 *
 * @return nil
*******************************************************************************/

void CheckExpress()
{
  //Check if DATV Express Required
  if (strcmp(CurrentModeOP, TabModeOP[2]) == 0)  // Startup mode is DATV Express
  {
    if (CheckExpressConnect() == 1)   // Not connected
    {
      MsgBox2("DATV Express Not connected", "Connect it now or select another mode");
      wait_touch();
      setBackColour(0, 0, 0);
      clearScreen();
    }
    if (CheckExpressConnect() != 1)   // Connected
    {
      StartExpressServer();
    }
  }
}


/***************************************************************************//**
 * @brief Checks whether an Airspy is connected
 *
 * @param 
 *
 * @return 0 if present, 1 if absent
*******************************************************************************/

int CheckAirspyConnect()
{
  FILE *fp;
  char response[255];
  int responseint;

  /* Open the command for reading. */
  fp = popen("lsusb | grep -q 'Airspy' ; echo $?", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response, 7, fp) != NULL)
  {
    responseint = atoi(response);
  }

  /* close */
  pclose(fp);
  return responseint;
}


/***************************************************************************//**
 * @brief Checks whether a Lime Mini (V1 or V2) is connected
 *
 * @param 
 *
 * @return 0 if present, 1 if absent
*******************************************************************************/

int CheckLimeMiniConnect()
{
  FILE *fp;
  char response[255];
  int responseint;

  /* Open the command for reading. */
  fp = popen("lsusb | grep -q '0403:601f' ; echo $?", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response, 7, fp) != NULL)
  {
    responseint = atoi(response);
  }

  /* close */
  pclose(fp);
  return responseint;
}

/***************************************************************************//**
 * @brief Checks whether a Lime Mini V2 is connected
 *
 * @param 
 *
 * @return 0 if present, 1 if absent (or V1)
*******************************************************************************/

int CheckLimeMiniV2Connect()
{
  FILE *fp;
  char response[255];
  int responseint = 1;

  /* Open the command for reading. */
  fp = popen("LimeUtil --make | grep -q 'LimeSDR-Mini_v2' ; echo $?", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response, 7, fp) != NULL)
  {
    responseint = atoi(response);
  }

  /* close */
  pclose(fp);
  return responseint;
}

/***************************************************************************//**
 * @brief Checks whether a Lime USB is connected
 *
 * @param 
 *
 * @return 0 if present, 1 if absent
*******************************************************************************/

int CheckLimeUSBConnect()
{
  FILE *fp;
  char response[255];
  int responseint;

  /* Open the command for reading. */
  fp = popen("lsusb | grep -q '1d50:6108' ; echo $?", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response, 7, fp) != NULL)
  {
    responseint = atoi(response);
  }

  /* close */
  pclose(fp);
  return responseint;
}

/***************************************************************************//**
 * @brief Checks whether a Pluto is connected
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
  //  printf("%s", response);
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
 * @brief Checks whether a Pluto is connected to USB
 *
 * @param 
 *
 * @return 0 if present, 1 if absent
*******************************************************************************/

int CheckPlutoUSBConnect()
{
  FILE *fp;
  char response[255];
  int responseint;

  /* Open the command for reading. */
  fp = popen("lsusb | grep -q '0456:b673' ; echo $?", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response, 7, fp) != NULL)
  {
    responseint = atoi(response);
  }

  /* close */
  pclose(fp);
  return responseint;
}


/***************************************************************************//**
 * @brief Looks up the current xo_correction on the Pluto
 *
 * @param nil
 *
 * @return int which is value read from Pluto (typically 40000000)
*******************************************************************************/

int GetPlutoXO()
{
  FILE *fp;
  char response[127];
  char XOtext[63];
  int XO = 40000000;

  /* Open the command for reading. */
  fp = popen("/home/pi/rpidatv/scripts/pluto_get_ref.sh", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    pclose(fp);
    
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response, 63, fp) != NULL)
  {
    strcpyn(XOtext, response, 13);
    if (strcmp(XOtext, "xo_correction") == 0)
    {
      size_t len = strlen(response);
      if (len > 14)
      {
        memmove(response, response + 14, len - 14 + 1);
        XO = atoi(response);
      }
      else // This means that it has not been set, so return 40 MHz.
      {
        XO = 40000000; 
      }
    }
  }

  /* close */
  pclose(fp);
  return XO;
}


/***************************************************************************//**
 * @brief Looks up the current frequency expansion state on the Pluto
 *
 * @param nil
 *
 * @return int 9364 if expanded
*******************************************************************************/

int GetPlutoAD()
{
  FILE *fp;
  char response[127];
  char ADtext[63];
  int AD = 0;

  /* Open the command for reading. */
  fp = popen("/home/pi/rpidatv/scripts/pluto_get_ad.sh", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    pclose(fp);
    
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response, 63, fp) != NULL)
  {
    strcpyn(ADtext, response, 8);
    if (strcmp(ADtext, "attr_val") == 0)
    {
      size_t len = strlen(response);
      if (len > 11)
      {
        memmove(response, response + 11, len - 11 + 1);
        AD = atoi(response);
      }
      else // This means that it has not been set, so return 40 MHz.
      {
        AD = 0; 
      }
    }
  }

  /* close */
  pclose(fp);
  return AD;
}

/***************************************************************************//**
 * @brief Looks up the current CPU state on the Pluto
 *
 * @param nil
 *
 * @return int 0 on fail, 1 for single CPU, 2 for both
*******************************************************************************/

int GetPlutoCPU()
{
  FILE *fp;
  char response[127];
  char CPUtext[15];
  int cpu = 0;

  if (CheckPlutoIPConnect() == 1)
  {
    MsgBox4("No Pluto Detected", "Check Config and Connections", "or disconnect/connect Pluto", "Touch screen to continue");
    wait_touch();
    return 0;
  }

  // Open the command for reading
  fp = popen("/home/pi/rpidatv/scripts/pluto_get_cpu.sh", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    pclose(fp);
    return 0;
  }

  // Read the output a line at a time - output it
  while (fgets(response, 126, fp) != NULL)
  {
    strcpyn(CPUtext, response, 9);
    if (strcmp(CPUtext, "processor") == 0)
    {
      cpu = cpu + 1;
    }
  }

  pclose(fp);
  // printf("\n%d cpus detected\n", cpu);
  return cpu;
}


/***************************************************************************//**
 * @brief Checks whether a Pluto is connected if selected on the IP address
 *        and displays error message if not
 * @param 
 *
 * @return void
*******************************************************************************/

void CheckPlutoReady()
{
  if (strcmp(CurrentModeOP, TabModeOP[13]) == 0)  // Pluto Output selected
  {
    if (CheckPlutoIPConnect() == 1)
    {
      MsgBox4("No Pluto Detected", "Check Config and Connections", "or disconnect/connect Pluto", "Touch Screen to Continue");
      wait_touch();
    }
  }
}


/***************************************************************************//**
 * @brief Checks whether a Lime Mini or Lime USB is connected if selected
 *        and displays error message if not
 * @param 
 *
 * @return void
*******************************************************************************/

void CheckLimeReady()
{
  if ((strcmp(CurrentModeOP, TabModeOP[8]) == 0)
    || (strcmp(CurrentModeOP, TabModeOP[12]) == 0))  // Lime mini or FPGA Output selected
  {
    if (CheckLimeMiniConnect() == 1)
    {
      if (CheckLimeUSBConnect() == 0)
      {
        MsgBox2("Lime USB Detected, Lime Mini Selected", "Please select LimeSDR USB");
      }
      else
      {
        MsgBox4("No LimeSDR Mini Detected", "Check connections", "or select another output device.", "Touch Screen to Continue");
      }
      wait_touch();
    }
  }
  if (strcmp(CurrentModeOP, TabModeOP[3]) == 0)  // Lime USB Output selected
  {
    if (CheckLimeUSBConnect() == 1)
    {
      if (CheckLimeMiniConnect() == 0)
      {
        MsgBox2("Lime Mini Detected, Lime USB Selected", "Please select LimeSDR Mini");
      }
      else
      {
        MsgBox4("No LimeSDR USB Detected", "Check connections - use USB3", "or select another output device.", "Touch Screen to Continue");
      }
      wait_touch();
    }
  }
}

/***************************************************************************//**
 * @brief Displays Info about a connected Lime
 *        
 * @param 
 *
 * @return void
*******************************************************************************/

void LimeInfo()
{
  MsgBox4("Please wait", " ", " ", " ");

  // Initialise and calculate the text display
  setForeColour(255, 255, 255);    // White text
  setBackColour(0, 0, 0);          // on Black
  const font_t *font_ptr = &font_dejavu_sans_22;
  int txtht =  font_ptr->ascent;
  int linepitch = (14 * txtht) / 10;

  FILE *fp;
  char response[255];
  int line = 0;

  /* Open the command for reading. */
  fp = popen("LimeUtil --make", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  clearScreen();
  Text2(wscreen/12, hscreen - 1 * linepitch, "Lime Firmware Information", font_ptr);

  /* Read the output a line at a time - output it. */
  while (fgets(response, 50, fp) != NULL)
  {
    if (line > 0)    //skip first line
    {
      Text2(wscreen/12, hscreen - (1.2 * line + 2) * linepitch, response, font_ptr);
    }
    line = line + 1;
  }

  /* close */
  pclose(fp);
  Text2(wscreen/12, 1.2 * linepitch, "Touch Screen to Continue", font_ptr);
  refreshMouseBackground();
  draw_cursor_foreground(mouse_x, mouse_y);
  UpdateWeb();
}

/***************************************************************************//**
 * @brief Returns Lime Gateware Revision
 *        
 * @param 
 *
 * @return int Lime Gateware revision 26, 27 or 28
*******************************************************************************/

int LimeGWRev()
{
  FILE *fp;
  char response[255];
  char test_string[255];
  int line = 0;
  int GWRev = 0;

  // Open the command for reading
  fp = popen("LimeUtil --make", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  // Read the output a line at a time
  while (fgets(response, 50, fp) != NULL)
  {
    if (line > 0)    //skip first line
    {
      strcpy(test_string, response);
      test_string[19] = '\0';
      if (strcmp(test_string, "  Gateware revision") == 0)
      {
        strncpy(test_string, &response[21], strlen(response));
        test_string[strlen(response)-21] = '\0';
        GWRev = atoi(test_string);
      }
    }
    line = line + 1;
  }

  pclose(fp);
  return GWRev;
}

/***************************************************************************//**
 * @brief Returns Lime Gateware Version
 *        
 * @param 
 *
 * @return int Lime Gateware Version 1?
*******************************************************************************/
int LimeGWVer()
{
  FILE *fp;
  char response[255];
  char test_string[255];
  int line = 0;
  int GWVer = 0;

  fp = popen("LimeUtil --make", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  // Read the output a line at a time
  while (fgets(response, 50, fp) != NULL)
  {
    if (line > 0)    //skip first line
    {
      strcpy(test_string, response);
      test_string[18] = '\0';
      if (strcmp(test_string, "  Gateware version") == 0)
      {
        // Copy the text to the right of the deleted character
        strncpy(test_string, &response[20], strlen(response));
        test_string[strlen(response)-20] = '\0';
        GWVer = atoi(test_string);
      }
    }
    line = line + 1;
  }

  pclose(fp);
  return GWVer;
}

/***************************************************************************//**
 * @brief Returns Lime Firmware Version
 *        
 * @param 
 *
 * @return int Lime Firmware version 5?
*******************************************************************************/
int LimeFWVer()
{
  FILE *fp;
  char response[255];
  char test_string[255];
  int line = 0;
  int FWVer = 0;

  fp = popen("LimeUtil --make", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  // Read the output a line at a time
  while (fgets(response, 50, fp) != NULL)
  {
    if (line > 0)    //skip first line
    {
      strcpy(test_string, response);
      test_string[18] = '\0';
      if (strcmp(test_string, "  Firmware version") == 0)
      {
        // Copy the text to the right of the deleted character
        strncpy(test_string, &response[20], strlen(response));
        test_string[strlen(response)-20] = '\0';
        FWVer = atoi(test_string);
      }
    }
    line = line + 1;
  }

  pclose(fp);
  return FWVer;
}

/***************************************************************************//**
 * @brief Returns Lime Hardware version
 *        
 * @param 
 *
 * @return int Lime Hardware version 1 or 2
*******************************************************************************/
int LimeHWVer()
{
  FILE *fp;
  char response[255];
  char test_string[255];
  int line = 0;
  int HWVer = 0;

  fp = popen("LimeUtil --make", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  // Read the output a line at a time
  while (fgets(response, 50, fp) != NULL)
  {
    if (line > 0)    //skip first line
    {
      strcpy(test_string, response);
      test_string[18] = '\0';
      if (strcmp(test_string, "  Hardware version") == 0)
      {
        // Copy the text to the right of the deleted character
        strncpy(test_string, &response[20], strlen(response));
        test_string[strlen(response)-20] = '\0';
        HWVer = atoi(test_string);
      }
    }
    line = line + 1;
  }

  pclose(fp);
  return HWVer;
}

/***************************************************************************//**
 * @brief Checks Lime Mini version and runs LimeQuickTest
 *        
 * @param 
 *
 * @return null, but output is displayed
*******************************************************************************/
void LimeMiniTest()
{
  char version_info[51];
  int LHWVer = 0;
  int LFWVer = 0;
  int LGWVer = 0;
  int LGWRev = 0;
  FILE *fp;
  char response[255];
  char test_string[255];
  int line = 0;
  int rct = 1;      // REF clock test
  int vctcxot = 1;  // VCTCXO test
  int cnt  = 1;     // Clock Network Test
  int fpgae = 1;    // FPGA EEPROM Test
  int lmst = 1;     // LMS7002M Test
  int tx2lnawt = 1; // TX_2 -> LNA_W Test
  int tx1lnaht = 1; // TX_1 -> LNA_H
  int rflt = 1;     // RF Loopback Test
  int bt = 1;       // Board tests

  setForeColour(255, 255, 255);    // White text
  setBackColour(0, 0, 0);          // on Black
  const font_t *font_ptr = &font_dejavu_sans_22;
  int txtht =  font_ptr->ascent;
  int th = (14 * txtht) / 10;

  clearScreen();
  Text2(wscreen/12, hscreen - 1 * th, "Portsdown LimeSDR Mini Test Report", font_ptr);

  // First check that a LimeSDR Mini (not a USB version) is connected
  if ((CheckLimeMiniConnect() != 0) || (CheckExpressConnect() == 0))  // No Lime Mini Connected or express
  {
    if (CheckExpressConnect() == 0)
    {
      Text2(wscreen/12, hscreen - 3 * th, "DATV Express Connected", font_ptr);
      Text2(wscreen/12, hscreen - 5 * th, "Please retry without DATV Express", font_ptr);
      Text2(wscreen/12, hscreen - 6.1 * th, "and only LimeSDR Mini connected", font_ptr);
    }
    else
    {
      if (CheckLimeUSBConnect() == 0)  // Lime USB Connnected
      {
        Text2(wscreen/12, hscreen - 3 * th, "LimeSDR USB Connected", font_ptr);
        Text2(wscreen/12, hscreen - 5 * th, "This test only works for the LimeSDR Mini", font_ptr);
      }
      else  // Nothing connected
      {
        Text2(wscreen/12, hscreen - 3 * th, "No LimeSDR Connected", font_ptr);
        Text2(wscreen/12, hscreen - 5 * th, "Please check connections", font_ptr);
      }
    }
  }
  else  // LimeSDR Mini connected, so check HW, FW and GW versions
  {
    MsgBox4("Testing...", " ", "Please Wait", " ");

    fp = popen("LimeUtil --make", "r");
    if (fp == NULL)
    {
      printf("Failed to run command\n" );
      exit(1);
    }

    while (fgets(response, 50, fp) != NULL)
    {
      if (line > 0)    //skip first line
      {
        strcpy(test_string, response);
        test_string[18] = '\0';
        if (strcmp(test_string, "  Hardware version") == 0)
        {
          strncpy(test_string, &response[20], strlen(response));
          test_string[strlen(response)-20] = '\0';
          LHWVer = atoi(test_string);
        }

        strcpy(test_string, response);
        test_string[18] = '\0';
        if (strcmp(test_string, "  Firmware version") == 0)
        {
          strncpy(test_string, &response[20], strlen(response));
          test_string[strlen(response)-20] = '\0';
          LFWVer = atoi(test_string);
        }

        strcpy(test_string, response);
        test_string[18] = '\0';
        if (strcmp(test_string, "  Gateware version") == 0)
        {
          strncpy(test_string, &response[20], strlen(response));
          test_string[strlen(response)-20] = '\0';
          LGWVer = atoi(test_string);
        }

        strcpy(test_string, response);
        test_string[19] = '\0';
        if (strcmp(test_string, "  Gateware revision") == 0)
        {
          strncpy(test_string, &response[21], strlen(response));
          test_string[strlen(response)-21] = '\0';
          LGWRev = atoi(test_string);
        }
      }
      line = line + 1;
    }
    pclose(fp);

    clearScreen();
    Text2(wscreen/12, hscreen - 1 * th, "Portsdown LimeSDR Mini Test Report", font_ptr);

    if (LHWVer <= 3)  // LimeSDR Mini V1.x
    {
      snprintf(version_info, 50, "Hardware V1.%d, Firmware V%d, Gateware V%d.%d", LHWVer, LFWVer, LGWVer, LGWRev);
    }
    else              // V2.x
    {
      snprintf(version_info, 50, "Hardware V2.%d, Firmware V%d, Gateware V%d.%d", LHWVer - 3, LFWVer, LGWVer, LGWRev);
    }
    Text2(wscreen/48, hscreen - (2.5 * th), version_info, font_ptr);

    fp = popen("LimeQuickTest", "r");
    if (fp == NULL)
    {
      printf("Failed to run command\n" );
      exit(1);
    }

    // Read the output a line at a time
    while (fgets(response, 100, fp) != NULL)
    {
      if (line > 4)    //skip initial lines
      {
        strcpy(test_string, response);
        test_string[15] = '\0';
        if (strcmp(test_string, "  Test results:") == 0)
        {
          Text2(wscreen/12, hscreen - (4.5 * th), "REF clock test", font_ptr);
          strncpy(test_string, &response[strlen(response) - 7], strlen(response));
          test_string[6] = '\0';
          Text2(wscreen*6/12, hscreen - (4.5 * th), test_string, font_ptr);
          rct = strcmp(test_string, "PASSED");
        }

        strcpy(test_string, response);
        test_string[11] = '\0';
        if (strcmp(test_string, "  Results :") == 0)
        {
          Text2(wscreen/12, hscreen - (5.6 * th), "VCTCXO test", font_ptr);
          strncpy(test_string, &response[strlen(response) - 7], strlen(response));
          test_string[6] = '\0';
          Text2(wscreen*6/12, hscreen - (5.6 * th), test_string, font_ptr);
          vctcxot = strcmp(test_string, "PASSED");
        }
        strcpy(test_string, response);
        test_string[11] = '\0';
        if (strcmp(test_string, "->Clock Net") == 0)
        {
          Text2(wscreen/12, hscreen - (6.7 * th), "Clock Network Test", font_ptr);
          strncpy(test_string, &response[strlen(response) - 7], strlen(response));
          test_string[6] = '\0';
          Text2(wscreen*6/12, hscreen - (6.7 * th), test_string, font_ptr);
          cnt = strcmp(test_string, "PASSED");
        }
        strcpy(test_string, response);
        test_string[12] = '\0';
        if (strcmp(test_string, "->FPGA EEPRO") == 0)
        {
          Text2(wscreen/12, hscreen - (7.8 * th), "FPGA EEPROM Test", font_ptr);
          strncpy(test_string, &response[strlen(response) - 7], strlen(response));
          test_string[6] = '\0';
          Text2(wscreen*6/12, hscreen - (7.8 * th), test_string, font_ptr);
          fpgae = strcmp(test_string, "PASSED");
        }
        strcpy(test_string, response);
        test_string[12] = '\0';
        if (strcmp(test_string, "->LMS7002M T") == 0)
        {
          Text2(wscreen/12, hscreen - (8.9 * th), "LMS7002M Test", font_ptr);
          strncpy(test_string, &response[strlen(response) - 7], strlen(response));
          test_string[6] = '\0';
          Text2(wscreen*6/12, hscreen - (8.9 * th), test_string, font_ptr);
          lmst = strcmp(test_string, "PASSED");
        }
        strcpy(test_string, response);
        test_string[12] = '\0';
        if (strcmp(test_string, "  CH0 (SXR=1") == 0)
        {
          Text2(wscreen/12, hscreen - (10 * th), "TX_2 -> LNA_W Test", font_ptr);
          strncpy(test_string, &response[strlen(response) - 7], strlen(response));
          test_string[6] = '\0';
          Text2(wscreen*6/12, hscreen - (10 * th), test_string, font_ptr);
          tx2lnawt = strcmp(test_string, "PASSED");
        }
        strcpy(test_string, response);
        test_string[12] = '\0';
        if (strcmp(test_string, "  CH0 (SXR=2") == 0)
        {
          Text2(wscreen/12, hscreen - (11.1 * th), "TX_1 -> LNA_H Test", font_ptr);
          strncpy(test_string, &response[strlen(response) - 7], strlen(response));
          test_string[6] = '\0';
          Text2(wscreen*6/12, hscreen - (11.1 * th), test_string, font_ptr);
          tx1lnaht = strcmp(test_string, "PASSED");
        }
        strcpy(test_string, response);
        test_string[12] = '\0';
        if (strcmp(test_string, "->RF Loopbac") == 0)
        {
          Text2(wscreen/12, hscreen - (12.2 * th), "RF Loopback Test", font_ptr);
          strncpy(test_string, &response[strlen(response) - 7], strlen(response));
          test_string[6] = '\0';
          Text2(wscreen*6/12, hscreen - (12.2 * th), test_string, font_ptr);
          rflt = strcmp(test_string, "PASSED");
        }
        strcpy(test_string, response);
        test_string[12] = '\0';
        if (strcmp(test_string, "=> Board tes") == 0)
        {
          Text2(wscreen/12, hscreen - (13.3 * th), "Board tests", font_ptr);
          strncpy(test_string, &response[strlen(response) - 10], strlen(response));
          test_string[6] = '\0';
          Text2(wscreen*6/12, hscreen - (13.3 * th), test_string, font_ptr);
          bt = strcmp(test_string, "PASSED");
        }
      }
      line = line + 1;
    }
    pclose(fp);
    if ((rct + vctcxot + cnt + fpgae + lmst + tx2lnawt + tx1lnaht + rflt + bt) == 0)       // All passed
    {
      setForeColour(127, 255, 127);    // Green text
      Text2(wscreen/12, 1.0 * th, "All tests passed", font_ptr);
    }
    else
    {
      setForeColour(255, 63, 63);    // Red text
      Text2(wscreen/12, hscreen - (14.4 * th), "Further investigation required", font_ptr);
      Text2(wscreen/12, hscreen - (15.5 * th), "Note that the custom FPGA nearly always fails", font_ptr);
    }
  }
  setForeColour(255, 255, 255);    // White text
  Text2(wscreen*5/12, 1, "Touch Screen to Continue", font_ptr);
  refreshMouseBackground();
  draw_cursor_foreground(mouse_x, mouse_y);
  UpdateWeb();
}


/***************************************************************************//**
 * @brief Displays Info about the installed LimeUtil
 *        
 * @param 
 *
 * @return void
*******************************************************************************/

void LimeUtilInfo()
{
  // Initialise and calculate the text display
  setForeColour(255, 255, 255);    // White text
  setBackColour(0, 0, 0);          // on Black
  const font_t *font_ptr = &font_dejavu_sans_22;
  int txtht =  font_ptr->ascent;
  int th = (14 * txtht) / 10;

  clearScreen();
  Text2(wscreen/12, hscreen - 1 * th, "LimeSuite Version Information", font_ptr);

  FILE *fp;
  char response[255];
  int line = 0;

  /* Open the command for reading. */
  fp = popen("LimeUtil --info", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response, 50, fp) != NULL)
  {
    if ((line > 4) && (line < 11))    // Select lines for display
    {
      Text2(wscreen/12, hscreen - (1.5 * line - 2) * th, response, font_ptr);
    }
    line = line + 1;
  }

  /* close */
  pclose(fp);
  Text2(wscreen/12, 1.2 * th, "Touch Screen to Continue", font_ptr);
  refreshMouseBackground();
  draw_cursor_foreground(mouse_x, mouse_y);
  UpdateWeb();
}


void SetLimeUpsample()
{
  char Prompt[63];
  char CurrentValue[63];
  char Value[63];
  int EnteredValue = 0;

  snprintf(CurrentValue, 30, "%d", LimeUpsample);
  snprintf(Prompt, 63, "Set the Lime Upsample to 1, 2, 4 or 8:");

  while ((EnteredValue != 1) && (EnteredValue != 2) && (EnteredValue != 4) && (EnteredValue != 8))
  {
    Keyboard(Prompt, CurrentValue, 3);
    EnteredValue = atoi(KeyboardReturn);
  }
  LimeUpsample = EnteredValue;
  snprintf(Value, 3, "%d", LimeUpsample);
  SetConfigParam(PATH_PCONFIG, "upsample", Value);
}


/***************************************************************************//**
 * @brief Checks whether an SDRPlay SDR is connected
 *
 * @param 
 *
 * @return 0 if present, 1 if absent
*******************************************************************************/

int CheckSDRPlay()
{
  FILE *fp;
  char response[255];
  int responseint;

  // Open the command for reading
  fp = popen("lsusb | grep -q '1df7:' ; echo $?", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  // Read the output a line at a time - output it
  while (fgets(response, 7, fp) != NULL)
  {
    responseint = atoi(response);
  }

  pclose(fp);
  return responseint;
}


void ClearMenuMessage()
{
  MenuText[0][0] = '\0';
  MenuText[1][0] = '\0';
  MenuText[2][0] = '\0';
  MenuText[3][0] = '\0';
  MenuText[4][0] = '\0';
}


/***************************************************************************//**
 * @brief Monitors touchscreen during VLC play.  Waits for touch
 *        Controls volume, snap and sets FinishedButton = 0 at end  
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/

void *WaitButtonFileVLC(void * arg)
{
  int rawX, rawY, rawPressure;
  FinishedButton = 1; // Start with Parameters on

  while (FinishedButton == 1)
  {
    while(getTouchSample(&rawX, &rawY, &rawPressure)==0);  // Wait here for touch

    TransformTouchMap(rawX, rawY);  // Sorts out orientation and approx scaling of the touch map

    if((scaledX <= 5 * wscreen / 40)  &&  (scaledY <= 2 * hscreen / 12)) // Bottom left
    {
      printf("In snap zone, so take snap.\n");
      system("/home/pi/rpidatv/scripts/snap2.sh");
    }
    else if((scaledX >= 35 * wscreen / 40)  && (scaledY >= 6 * hscreen / 12))  // Top Right
    {
      printf("Volume Up.\n");
      AdjustVLCVolume(51);
    }
    else if((scaledX >= 35 * wscreen / 40)  && (scaledY < 6 * hscreen / 12))  // Top Right
    {
      printf("Volume Down.\n");
      AdjustVLCVolume(-51);
    }
    else
    {
      printf("Out of zone.  End VLC play requested.\n");
      FinishedButton = 0;  // Not in the zone, so exit receive
      system("/home/pi/rpidatv/scripts/lmvlcsd.sh &");
      return NULL;
    }
  }
  return NULL;
}


/***************************************************************************//**
 * @brief Plays a video for file explorer until screen is touched
 *        while FinishedButton == 1  
 *        
 * @param char *VideoPath, char *VideoFile - file path and name
 *
 * @return void
*******************************************************************************/

void ShowVideoFile(char *VideoPath, char *VideoFile)
{
  char PlayCommand[1023];
  char LinuxCommand[511];

  // Show the touchmap
  strcpy(LinuxCommand, "sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/VLC_overlay.png ");
  strcat(LinuxCommand, ">/dev/null 2>/dev/null");
  system(LinuxCommand);
  strcpy(LinuxCommand, "(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
  system(LinuxCommand);

  // Create thread to monitor touchscreen
  pthread_create (&thbuttonFileVLC, NULL, &WaitButtonFileVLC, NULL);

  FinishedButton = 1;
  snprintf(PlayCommand, 1023, "/home/pi/rpidatv/scripts/play_video_file.sh \"%s%s\" &", VideoPath, VideoFile);
  system(PlayCommand);

  while (FinishedButton == 1)
  {
    usleep(1000);
  }

  system("sudo killall vlc >/dev/null 2>/dev/null");
  pthread_join(thbuttonFileVLC, NULL);
}


/***************************************************************************//**
 * @brief Displays an image for file explorer until screen is touched
 *        
 * @param char *TextPath, char *TextFile - file path and name
 *
 * @return void
*******************************************************************************/

void ShowImageFile(char *ImagePath, char *ImageFile)
{
  int rawX, rawY, rawPressure;
  char fbicmd[1023];
  bool NotWaitingforTouchYet = true;

  snprintf(fbicmd, 1023, "sudo fbi -T 1 -noverbose -a \"%s%s\" >/dev/null 2>/dev/null", ImagePath, ImageFile);

  system(fbicmd);

  usleep(100000);  // Delay to allow fbi to finish

  refreshMouseBackground();
  draw_cursor_foreground(mouse_x, mouse_y);
  UpdateWeb();

  while(NotWaitingforTouchYet)
  {
    NotWaitingforTouchYet = false;
    if (getTouchSample(&rawX, &rawY, &rawPressure) == 0) continue;
  }
  system("sudo killall fbi >/dev/null 2>/dev/null");  // kill instance of fbi
}


/***************************************************************************//**
 * @brief Displays up to 100 lines of text using the SelectFormList function
 *        
 * @param char *TextPath, char *TextFile - file path and name
 *
 * @return void
*******************************************************************************/

void ListText(char *TextPath, char *TextFile)
{
  int i;
  FILE *fp;
  char ListCommand[1023];
  char response[255];
  char TextArray[101][63];
  int LineCount = 1;           // Start at 1, as 0 is the title

  // Clear the TextArray
  for (i = 0; i <= 100; i++)
  {
    strcpy(TextArray[i], "");
  }

  snprintf(ListCommand, 1023, "head -n 100 \"%s%s\"", TextPath, TextFile);
  fp = popen(ListCommand, "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  // Read the output a line at a time - store it
  while ((fgets(response, 255, fp) != NULL) && (LineCount < 101))
  {
    //response[strlen(response) - 1] = '\0';  // Don't Strip trailing cr
    // printf("Line %d %s\n", LineCount, response);
    strcpyn(TextArray[LineCount], response, 63);  // Read response and limit to 63 characters
    LineCount++;
  }
  pclose(fp);

  strcpy(TextArray[0], "Text file listing:");   // Title
  SelectFromList(-1, TextArray, LineCount - 1);    // Display in list
}


/***************************************************************************//**
 * @brief Handles button presses from the file menu 
 *        and acts on buttons 0 (copy), 1 (paste) and 5 (Explorer)
 * @param int NoButton
 *
 * @return void
*******************************************************************************/

void FileOperation(int NoButton)
{
  int response = 0;
  char NewPathSelection[255] = "/home/pi/";
  char NewFileSelection[255] = "";
  char FileCommand[1279];
  char MangleText[1023];
  char DummyFileSelection[255] = "";
  char FilePathShort[255];
  char FileExtension[15];
  char RequestText[64];
  char InitText[64];
  bool IsValid = false;

  switch (NoButton)
  {
    case 0:                                                    // Copy file for paste

      // Clear the message lines
      ClearMenuMessage();

      response = SelectFileUsingList(CurrentPathSelection, CurrentFileSelection, NewPathSelection, NewFileSelection, 0);
      if (response == 1)    // File has been changed
      {
        strcpy(CurrentPathSelection, NewPathSelection);
        strcpy(CurrentFileSelection, NewFileSelection);
      }

      if (strlen(NewFileSelection) > 0)                // filename has been selected
      {
        strcpy(MenuText[0], "Selected File:");
      }
      else                                             // Only directory selected
      {
        strcpy(MenuText[0], "Current Directory:");
      }

      if (strlen(CurrentPathSelection) + strlen(CurrentFileSelection) < 63)  // all on one line
      {
        strcpy(MenuText[1], CurrentPathSelection);
        strcat(MenuText[1], CurrentFileSelection);
      }
      else if (strlen(CurrentPathSelection) < 63 )                           // path on one line
      {
        strcpy(MenuText[1], CurrentPathSelection);
        strcpy(MenuText[2], CurrentFileSelection);
      }
      else if ((strlen(CurrentPathSelection) >= 63 ) && (strlen(CurrentPathSelection) < 127 ))  // path on 2 lines                                                              
      {
        strcpyn(MenuText[1], CurrentPathSelection, 63);
        strcpy(MenuText[2], CurrentPathSelection + 63);
        strcpy(MenuText[3], CurrentFileSelection);
      }
      else                                                                                // Use all available space for path
      {
        strcpyn(MenuText[1], CurrentPathSelection, strlen(CurrentPathSelection) - 126);
        strcpyn(MenuText[2], CurrentPathSelection + strlen(CurrentPathSelection) - 126, 63);
        strcpyn(MenuText[3], CurrentPathSelection + strlen(CurrentPathSelection) - 63, 63);
        strcpy(MenuText[4], CurrentFileSelection);
      }
      break;

    case 1:                                                                               // Paste file to Directory

      if (strcmp(MenuText[0], "Selected File:") == 0)                                     // Only action if a file is selected
      {
        SelectFileUsingList(CurrentPathSelection, DummyFileSelection, NewPathSelection, NewFileSelection, 1);
        if (strcmp(CurrentPathSelection, NewPathSelection) != 0)  // New path
        {
          printf("Old Path %s\nNew Path %s\n", CurrentPathSelection, NewPathSelection);
          strcpyn(FilePathShort, NewPathSelection, 7);

          if (strcmp(FilePathShort, "/media/") == 0)
          {
            snprintf(FileCommand, 1028,"sudo cp %s%s %s%s &", CurrentPathSelection, CurrentFileSelection, NewPathSelection, CurrentFileSelection);
          }
          else
          {
            snprintf(FileCommand, 1025, "cp %s%s %s%s &", CurrentPathSelection, CurrentFileSelection, NewPathSelection, CurrentFileSelection);
          }

          printf ("%s\n", FileCommand);
          system(FileCommand);

          snprintf(MangleText, 1023, "File %s copied to:", CurrentFileSelection);
          strcpyn(MenuText[0], MangleText, 63);
          strcpy(MenuText[2], "");
          strcpy(MenuText[3], "");
          strcpy(MenuText[4], "");

          if (strlen(NewPathSelection) < 63 )                           // path on one line
          {
            strcpy(MenuText[1], NewPathSelection);
          }
          else if ((strlen(NewPathSelection) >= 63 ) && (strlen(NewPathSelection) < 127 ))  // path on 2 lines                                                              
          {
            strcpyn(MenuText[1], NewPathSelection, 63);
            strcpy(MenuText[2], NewPathSelection + 63);
          }
          else                                                                                // Use all available space for path
          {
            strcpyn(MenuText[1], NewPathSelection, strlen(NewPathSelection) - 126);
            strcpyn(MenuText[2], NewPathSelection + strlen(NewPathSelection) - 126, 63);
            strcpyn(MenuText[3], NewPathSelection + strlen(NewPathSelection) - 63, 63);
          }

          // Deselect the file, but stay in the folder
          strcpy(CurrentFileSelection, "");
        }
      }
      break;

    case 2:                                                                           // Rename

      // Clear the message lines
      ClearMenuMessage();

      response = SelectFileUsingList(CurrentPathSelection, CurrentFileSelection, NewPathSelection, NewFileSelection, 0);
      if (response == 1)    // File has been changed
      {
        strcpy(CurrentPathSelection, NewPathSelection);
        strcpy(CurrentFileSelection, NewFileSelection);

        while (IsValid == FALSE)
        {
          strcpy(RequestText, "Enter new filename");
          strcpyn(InitText, CurrentFileSelection, 25);
          Keyboard(RequestText, InitText, 25);

          if (strlen(KeyboardReturn) >= 0)
          {
            IsValid = TRUE;
          }
        }

        snprintf(FileCommand, 1025, "mv %s%s %s%s &", CurrentPathSelection, CurrentFileSelection, CurrentPathSelection, KeyboardReturn);
        // printf("%s\n", FileCommand);
        system(FileCommand);

        strcpy(MenuText[0], "File renamed as:");

        if (strlen(CurrentPathSelection) + strlen(KeyboardReturn) < 63)  // all on one line
        {
          strcpy(MenuText[1], CurrentPathSelection);
          strcat(MenuText[1], KeyboardReturn);
        }
        else if (strlen(CurrentPathSelection) < 63 )                           // path on one line
        {
          strcpy(MenuText[1], CurrentPathSelection);
          strcpy(MenuText[2], KeyboardReturn);
        }
        else if ((strlen(CurrentPathSelection) >= 63 ) && (strlen(CurrentPathSelection) < 127 ))  // path on 2 lines                                                              
        {
          strcpyn(MenuText[1], CurrentPathSelection, 63);
          strcpy(MenuText[2], CurrentPathSelection + 63);
          strcpy(MenuText[3], KeyboardReturn);
        }
        else                                                                                // Use all available space for path
        {
          strcpyn(MenuText[1], CurrentPathSelection, strlen(CurrentPathSelection) - 126);
          strcpyn(MenuText[2], CurrentPathSelection + strlen(CurrentPathSelection) - 126, 63);
          strcpyn(MenuText[3], CurrentPathSelection + strlen(CurrentPathSelection) - 63, 63);
          strcpy(MenuText[4], KeyboardReturn);
        }
      }
      else
      {
        ClearMenuMessage();
        strcpy(MenuText[0], "Filename not changed");
      }
      break;

    case 3:
      break;

    case 5:                                                                                // File Explorer
      
      ClearMenuMessage();

      response = SelectFileUsingList(CurrentPathSelection, CurrentFileSelection, NewPathSelection, NewFileSelection, 0);
      printf("File explorer to display %s%s\n", NewPathSelection, NewFileSelection);

      // Copy new selection back so that it is ready if reselected
      strcpy(CurrentPathSelection, NewPathSelection);
      strcpy(CurrentFileSelection, NewFileSelection);

      strcpy(FileExtension, "");  // will include . like .txt up to 7 chars (.factory)

      // Ascertain file type by file extension

      if ((NewFileSelection[strlen(NewFileSelection) - 1] == '.') && (strlen(FileExtension) == 0))
      {
        strcpy(FileExtension, NewFileSelection + strlen(NewFileSelection) - 1);
      }
      if ((NewFileSelection[strlen(NewFileSelection) - 2] == '.') && (strlen(FileExtension) == 0))
      {
        strcpy(FileExtension, NewFileSelection + strlen(NewFileSelection) - 2);
      }
      if ((NewFileSelection[strlen(NewFileSelection) - 3] == '.') && (strlen(FileExtension) == 0))
      {
        strcpy(FileExtension, NewFileSelection + strlen(NewFileSelection) - 3);
      }
      if ((NewFileSelection[strlen(NewFileSelection) - 4] == '.') && (strlen(FileExtension) == 0))
      {
        strcpy(FileExtension, NewFileSelection + strlen(NewFileSelection) - 4);
      }
      if ((NewFileSelection[strlen(NewFileSelection) - 5] == '.') && (strlen(FileExtension) == 0))
      {
        strcpy(FileExtension, NewFileSelection + strlen(NewFileSelection) - 5);
      }
      if ((NewFileSelection[strlen(NewFileSelection) - 6] == '.') && (strlen(FileExtension) == 0))
      {
        strcpy(FileExtension, NewFileSelection + strlen(NewFileSelection) - 6);
      }
      if ((NewFileSelection[strlen(NewFileSelection) - 7] == '.') && (strlen(FileExtension) == 0))
      {
        strcpy(FileExtension, NewFileSelection + strlen(NewFileSelection) - 7);
      }
      if ((NewFileSelection[strlen(NewFileSelection) - 8] == '.') && (strlen(FileExtension) == 0))
      {
        strcpy(FileExtension, NewFileSelection + strlen(NewFileSelection) - 8);
      }

      printf("Extension: -%s-\n", FileExtension);

      if ((strcmp(FileExtension, ".txt"    ) == 0)
       || (strcmp(FileExtension, ".bak"    ) == 0)
       || (strcmp(FileExtension, ".factory") == 0)
       || (strcmp(FileExtension, ".sh"     ) == 0)
       || (strcmp(FileExtension, ".c"      ) == 0)
       || (strcmp(FileExtension, ".h"      ) == 0)
       || (strcmp(FileExtension, ".md"     ) == 0)
       || (strcmp(FileExtension, ".config" ) == 0)
       || (strcmp(FileExtension, ".cpp"    ) == 0)
       || (strcmp(FileExtension, ".yaml"   ) == 0)
       || (strcmp(FileExtension, ".yml"    ) == 0))
      {
        // Call Text reader
        ListText(NewPathSelection, NewFileSelection);
      }

      if ((strcmp(FileExtension, ".PhotoCD") == 0)
       || (strcmp(FileExtension, ".jpg"    ) == 0)
       || (strcmp(FileExtension, ".JPG"    ) == 0)
       || (strcmp(FileExtension, ".jpeg"   ) == 0)
       || (strcmp(FileExtension, ".ppm"    ) == 0)
       || (strcmp(FileExtension, ".gif"    ) == 0)
       || (strcmp(FileExtension, ".tiff"   ) == 0)
       || (strcmp(FileExtension, ".xpm"    ) == 0)
       || (strcmp(FileExtension, ".xwd"    ) == 0)
       || (strcmp(FileExtension, ".bmp"    ) == 0)
       || (strcmp(FileExtension, ".webp"   ) == 0)
       || (strcmp(FileExtension, ".png"    ) == 0))
      {
        // Call Image Viewer
        ShowImageFile(NewPathSelection, NewFileSelection);
      }

      if ((strcmp(FileExtension, ".ts"     ) == 0)
       || (strcmp(FileExtension, ".avi"    ) == 0)
       || (strcmp(FileExtension, ".mp3"    ) == 0)
       || (strcmp(FileExtension, ".mp4"    ) == 0)
       || (strcmp(FileExtension, ".mkv"    ) == 0)
       || (strcmp(FileExtension, ".mov"    ) == 0)
       || (strcmp(FileExtension, ".ogg"    ) == 0)
       || (strcmp(FileExtension, ".MTS"    ) == 0))
      {
        // Call VLC
        ShowVideoFile(NewPathSelection, NewFileSelection);
      }

      break;
    case 13:                                                                                // Mount/unmount USB
      // Check drive device name
      
      if (USBmounted() == 0) // mounted, so unmount
      {
        if (USBDriveDevice() == 1)
        {
          system("sudo umount /dev/sda1");
        }
        else
        {
          system("sudo umount /dev/sdb1");
        }
      }
      else                  // not mounted, so mount and display file explorer
      {
        if (USBDriveDevice() == 1)
        {
          system("sudo mount /dev/sda1 /mnt");
        }
        else if (USBDriveDevice() == 2)
        {
          system("sudo mount /dev/sdb1 /mnt");
        }
        else
        {
          MsgBox4("Failed to mount USB Drive", "Check Connections", "", "Touch screen to continue");
          wait_touch();
          return;
        }

        if (USBmounted() == 0)
        {
          strcpy(CurrentPathSelection, "/mnt/");
          strcpy(CurrentFileSelection, "");
          FileOperation(5);
        }
        else
        {
          MsgBox4("Failed to mount USB Drive", "Check Connections", "", "Touch screen to continue");
          wait_touch();
        }
      }
      break;
  }
}


/***************************************************************************//**
 * @brief Detects if a USB Drive is currently mounted at /mnt
 *
 * @param nil
 *
 * @return 0 if mounted, 1 if not mounted
*******************************************************************************/

int USBmounted()
{
  FILE *fp;
  char response_line[255];

  // Check the mountpoint

  fp = popen("mountpoint -d /mnt | grep 'not a mountpoint'", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  // Response is "not a mountpoint" if not mounted
  // So, if there is a response, return 1.

  /* Read the output a line at a time - output it. */
  while (fgets(response_line, 250, fp) != NULL)
  {
    if (strlen(response_line) > 1)
    {
      pclose(fp);
      return 1;
    }
  }
  pclose(fp);
  return 0;
}


/***************************************************************************//**
 * @brief Checks the device name for a USB drive
 *
 * @param nil
 *
 * @return 0 if none, 1 if sda1, 2 if sdb1
*******************************************************************************/

int USBDriveDevice()
{
  FILE *fp;
  char response_line[255];
  int return_value = 0;

  // Check the mountpoint

  fp = popen("/home/pi/rpidatv/scripts/check_usb_storage.sh", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  // Response is 0, 1 or 2

  /* Read the output a line at a time - output it. */
  while (fgets(response_line, 250, fp) != NULL)
  {
    if (strlen(response_line) >= 1)
    {
      return_value = atoi(response_line);
    }
  }
  pclose(fp);
  //printf("Driver return value = %d\n", return_value);
  return return_value;
}


void *WaitButtonIQPlay(void * arg)
{
  int rawX, rawY, rawPressure;
  FinishedButton = 1;

  while (FinishedButton == 1)
  {
    while (getTouchSample(&rawX, &rawY, &rawPressure) == 0);  // Wait here for touch

    FinishedButton = 0;
  }
  system("sudo killall wav2lime");
  return NULL;
}


/***************************************************************************//**
 * @brief Handles button presses from the IQ file menu 
 *        and acts on buttons 10 (sample rate), 11 (sing/loop) and 12 (Select file) and 13 (play)
 * @param int NoButton
 *
 * @return void
*******************************************************************************/

void IQFileOperation(int NoButton)
{
  int response = 0;
  char NewPathSelection[255] = "";
  char NewFileSelection[255] = "";
  char FileCommand[1279];
  char Value[63];
  char Value2[63];
  char ShortValue[7];
  char ShortValue2[7];
  char FrequencyLine[127];

  strcpy(CurrentPathSelection, "/home/pi/iqfiles/");

  switch (NoButton)
  {
    case 10:                                                    // Select File

      // Clear the message lines
      ClearMenuMessage();
      ValidIQFileSelected = false;

      response = SelectFileUsingList(CurrentPathSelection, CurrentFileSelection, NewPathSelection, NewFileSelection, 0);

      if (response == 1)    // File has been changed
      {
        strcpy(CurrentPathSelection, NewPathSelection);
        strcpy(CurrentFileSelection, NewFileSelection);
      }

      if (strlen(NewFileSelection) > 0)                // filename has been selected
      {
        strcpy(MenuText[0], "Selected File:");
        ValidIQFileSelected = true;
      }
      else                                             // Only directory selected
      {
        strcpy(MenuText[0], "Current Directory:");
        ValidIQFileSelected = false;
      }

      strcpy(MenuText[1], CurrentPathSelection);
      strcat(MenuText[1], CurrentFileSelection);
      GetConfigParam(PATH_PCONFIG, "freqoutput", Value);
      GetConfigParam(PATH_PCONFIG, "limegain", Value2);
      strcpyn(ShortValue, Value, 7);
      strcpyn(ShortValue2, Value2, 7);
      snprintf(MenuText[2], 127, "Frequency: %s MHz, Lime Gain %s%%", ShortValue, ShortValue2);

      //strcpy(MenuText[3], "Info 2");

      break;

    case 11:                                                                            // Play file single
      
      snprintf(FileCommand, 1000, "/home/pi/rpidatv/scripts/playiqfile.sh -i %s%s", CurrentPathSelection, CurrentFileSelection);

      // Create thread to monitor touchscreen

      // thread kills wav2lime when touched
      pthread_create (&thbuttonIQPlay, NULL, &WaitButtonIQPlay, NULL);

      GetConfigParam(PATH_PCONFIG, "freqoutput", Value);
      strcpyn(ShortValue, Value, 7);
      snprintf(FrequencyLine, 127, "on %s MHz", ShortValue);

      MsgBox4("Playing", CurrentFileSelection, FrequencyLine, "Touch Screen to Cancel");

      system(FileCommand);
      FinishedButton = 0;
      FalseTouch = true;   // simulate screen touch to take away waiting message

      pthread_join(thbuttonIQPlay, NULL);

      // Reset the LimeSDR
      system("/home/pi/rpidatv/bin/limesdr_stopchannel &");

      break;

    case 12:                                                                            // Play file loop
      
      snprintf(FileCommand, 1000, "/home/pi/rpidatv/scripts/playiqfile.sh -i %s%s", CurrentPathSelection, CurrentFileSelection);

      // Create thread to monitor touchscreen

      // thread kills wav2lime when touched
      pthread_create (&thbuttonIQPlay, NULL, &WaitButtonIQPlay, NULL);

      GetConfigParam(PATH_PCONFIG, "freqoutput", Value);
      strcpyn(ShortValue, Value, 7);
      snprintf(FrequencyLine, 127, "on %s MHz in a loop", ShortValue);

      MsgBox4("Playing", CurrentFileSelection, FrequencyLine, "Touch Screen to Cancel");

      while (FinishedButton == 1)
      {
        system(FileCommand);
      }

      pthread_join(thbuttonIQPlay, NULL);

      // Reset the LimeSDR
      system("/home/pi/rpidatv/bin/limesdr_stopchannel &");

      break;
  }
}


/***************************************************************************//**
 * @brief Uses the "SelectFromList" dialogue to display a list of 
 * connected USB devices as shown by lsusb.  Truncates names to 63 chars.
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/

void ListUSBDevices()
{
  int i;
  FILE *fp;
  char response[255];
  char DeviceArray[101][63];
  char DeviceTest[63];
  int LineCount = 1;           // Start at 1, as 0 is the title

  // Clear the DeviceArray
  for (i = 0; i <= 100; i++)
  {
    strcpy(DeviceArray[i], "");
  }

  fp = popen("lsusb", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  // Read the output a line at a time - store it
  while ((fgets(response, 255, fp) != NULL) && (LineCount < 99))
  {
    response[strlen(response) - 1] = '\0';  // Strip trailing cr
    //printf("Line %d %s\n", LineCount, response);
    strcpyn(DeviceArray[LineCount], response + 23, 63);  // Read response from 23rd character and limit to 63

    // Highlight BATC PicoTuner
    strcpyn(DeviceTest, response + 23, 9);  // Read response from 23rd character and limit to 9
    if (strcmp(DeviceTest, "2e8a:ba2c") == 0)
    {
      strcpy(DeviceArray[LineCount], "2e8a:ba2c BATC PicoTuner");
    } 
    LineCount++;
  }
  pclose(fp);

  strcpy(DeviceArray[0], "USB Devices Detected:");   // Title
  SelectFromList(-1, DeviceArray, LineCount - 1);    // Display in list
}


/***************************************************************************//**
 * @brief Uses the "SelectFromList" dialogue to display a list of 
 * connected Network devices as shown by the list_rpi.sh script.  Truncates entriess to 63 chars.
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/

void ListNetDevices()
{
  int i;
  FILE *fp;
  char response[255];
  char NetworkArray[101][63];
  int LineCount = 1;           // Start at 1, as 0 is the title

  // Clear the NetworkArray
  for (i = 0; i <= 100; i++)
  {
    strcpy(NetworkArray[i], "");
  }

  fp = popen("/home/pi/rpidatv/scripts/list_net_devices.sh", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  // Read the output a line at a time - store it
  while ((fgets(response, 255, fp) != NULL) && (LineCount < 99))
  {
    response[strlen(response) - 1] = '\0';  // Strip trailing cr
    //printf("Line %d %s\n", LineCount, response);
    strcpyn(NetworkArray[LineCount], response, 63);  // Read response and limit to 63
    LineCount++;
  }
  pclose(fp);

  strcpy(NetworkArray[0], "Network Devices Detected:");   // Title
  SelectFromList(-1, NetworkArray, LineCount - 1);    // Display in list
}


/***************************************************************************//**
 * @brief Uses the "SelectFromList" dialogue to display a list of 
 * connected Raspberry Pis as shown by the list_rpi.sh script.  Truncates entriess to 63 chars.
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/

void ListNetPis()
{
  int i;
  FILE *fp;
  char response[255];
  char NetworkArray[101][63];
  int LineCount = 1;           // Start at 1, as 0 is the title

  // Clear the NetworkArray
  for (i = 0; i <= 100; i++)
  {
    strcpy(NetworkArray[i], "");
  }

  fp = popen("/home/pi/rpidatv/scripts/list_rpi.sh", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  // Read the output a line at a time - store it
  while ((fgets(response, 255, fp) != NULL) && (LineCount < 99))
  {
    response[strlen(response) - 1] = '\0';  // Strip trailing cr
    //printf("Line %d %s\n", LineCount, response);
    strcpyn(NetworkArray[LineCount], response, 63);  // Read response and limit to 63
    LineCount++;
  }
  pclose(fp);

  strcpy(NetworkArray[0], "Raspberry Pis Detected:");   // Title
  SelectFromList(-1, NetworkArray, LineCount - 1);    // Display in list
}


/***************************************************************************//**
 * @brief Stops the graphics system and displays the Portdown Logo
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/

void DisplayLogo()
{
  system("sudo fbi -T 1 -noverbose -a \"/home/pi/rpidatv/scripts/images/BATC_Black.png\" >/dev/null 2>/dev/null");
  UpdateWeb();
  refreshMouseBackground();
  draw_cursor_foreground(mouse_x, mouse_y);
  system("(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
}


void TransformTouchMap(int x, int y)
{
  // This function takes the raw (0 - 4095 on each axis) touch data x and y
  // and transforms it to approx 0 - wscreen and 0 - hscreen in globals scaledX 
  // and scaledY 

  int shiftX, shiftY;
  double factorX, factorY;

  if (touchscreen_present == true)      // Touchscreen
  {
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
  else                                         // Browser control without touchscreen
  {
    scaledX = x;
    scaledY = 480 - y;
  }
}


int IsMenuButtonPushed(int x,int y)
{
  int  i, NbButton, cmo, cmsize;
  NbButton = -1;
  int margin=10;  // was 20
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
        NbButton = i;          // Set the button number to return
        break;                 // Break out of loop as button has been found
      }
    }
  }
  return NbButton;
}

int IsImageToBeChanged(int x,int y)
{
  // Returns -1 for LHS touch, 0 for centre and 1 for RHS

  TransformTouchMap(x,y);       // Sorts out orientation and approx scaling of the touch map

  //if (scaledY >= hscreen/2)
  //{
  //  return 0;
  //}
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


int ButtonNumber(int MenuIndex, int Button)
{
  // Returns the Button Number (0 - 689) from the Menu number and the button position
  int ButtonNumb = 0;

  if (MenuIndex <= 10)  // 10 x 25-button main menus
  {
    ButtonNumb = (MenuIndex - 1) * 25 + Button;
  }
  if ((MenuIndex >= 11) && (MenuIndex <= 40))  // 30 x 10-button submenus
  {
    ButtonNumb = 250 + (MenuIndex - 11) * 10 + Button;
  }
  if ((MenuIndex >= 41) && (MenuIndex <= 41))  // keyboard
  {
    ButtonNumb = 550 + (MenuIndex - 41) * 50 + Button;
  }
  if (MenuIndex >= 42)  // 6 x 15-button submenus
  {
    ButtonNumb = 600 + (MenuIndex - 42) * 15 + Button;
  }
  return ButtonNumb;
}

int CreateButton(int MenuIndex, int ButtonPosition)
{
  // Provide Menu number (int 1 - 47), Button Position (0 bottom left, 23 top right)
  // return button number

  // Menus 1 - 10 are classic 25-button menus
  // Menus 11 - 40 are 10-button menus
  // Menu 41 is a keyboard
  // Menus 42 - 47 are 15-button menus

  int ButtonIndex;
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;

  ButtonIndex = ButtonNumber(MenuIndex, ButtonPosition);

  if ((MenuIndex != 41) && (MenuIndex != 8))  // All except keyboard and RX Menu
  {
    if (ButtonPosition < 20)  // Bottom 4 rows
    {
      x = (ButtonPosition % 5) * wbuttonsize + 20;  // % operator gives the remainder of the division
      y = (ButtonPosition / 5) * hbuttonsize + 20;
      w = wbuttonsize * 0.9;
      h = hbuttonsize * 0.9;
    }
    else if (ButtonPosition == 20)  // TX button
    {
      x = (ButtonPosition % 5) * wbuttonsize *1.7 + 20;    // % operator gives the remainder of the division
      y = (ButtonPosition / 5) * hbuttonsize + 20;
      w = wbuttonsize * 1.2;
      h = hbuttonsize * 1.2;
    }
    else if ((ButtonPosition == 21) || (ButtonPosition == 22) || (ButtonPosition == 23)) // RX/M1, M2 and M3 buttons
    {
      x = ((ButtonPosition + 1) % 5) * wbuttonsize + 20;  // % operator gives the remainder of the division
      y = (ButtonPosition / 5) * hbuttonsize + 20;
      w = wbuttonsize * 0.9;
      h = hbuttonsize * 1.2;
    }
  }
  else if (MenuIndex == 8)  // RX Menu
  {
    if (ButtonPosition < 15)  // Bottom 3 rows
    {
      x = (ButtonPosition % 5) * wbuttonsize + 20;  // % operator gives the remainder of the division
      y = (ButtonPosition / 5) * hbuttonsize + 20;
      w = wbuttonsize * 0.9;
      h = hbuttonsize * 0.9;
    }
    if ((ButtonPosition >= 15) && (ButtonPosition < 21))  // SR Buttons (6)
    {
      x = (ButtonPosition - 15) * 0.83 * wbuttonsize + 20;  // % operator gives the remainder of the division
      y = 3 * hbuttonsize + 20;
      w = wbuttonsize * 0.75;
      h = hbuttonsize * 0.9;
    }
    else if (ButtonPosition == 21)  // QO-100/T button
    {
      x = ((ButtonPosition - 1) % 5) * wbuttonsize * 1.7 + 20;    // % operator gives the remainder of the division
      y = ((ButtonPosition - 1) / 5) * hbuttonsize + 20;
      w = wbuttonsize * 1.2;
      h = hbuttonsize * 1.2;
    }
    else if ((ButtonPosition == 22) || (ButtonPosition == 23) || (ButtonPosition == 24)) // Exit, Config and [blank] buttons
    {
      x = ((ButtonPosition ) % 5) * wbuttonsize + 20;  // % operator gives the remainder of the division
      y = ((ButtonPosition - 1) / 5) * hbuttonsize + 20;
      w = wbuttonsize * 0.9;
      h = hbuttonsize * 1.2;
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
  setBackColour(Button->Status[Button->NoStatus].Color.r,
                Button->Status[Button->NoStatus].Color.g,
                Button->Status[Button->NoStatus].Color.b);
  
  if (label[0] == '\0')  // Deal with empty string
  {
    strcpy(label, " ");
  }

  // Separate button text into 2 lines if required  
  char find = '^';                                  // Line separator is ^
  const char *ptr = strchr(label, find);            // pointer to ^ in string

  if((ptr) && (CurrentMenu != 41))                  // if ^ found then
  {                                                 // 2 lines
    int index = ptr - label;                        // Position of ^ in string
    snprintf(line1, index+1, label);                // get text before ^
    snprintf(line2, strlen(label) - index, label + index + 1);  // and after ^

    // Display the text on the button
    TextMid2(Button->x+Button->w/2, Button->y+Button->h*11/16, line1, &font_dejavu_sans_20);	
    TextMid2(Button->x+Button->w/2, Button->y+Button->h*3/16, line2, &font_dejavu_sans_20);	
  
    // Draw green overlay half-button.  Menu 1, 2 lines and Button status = 0 only
    if ((CurrentMenu == 1) && (Button->NoStatus == 0))
    {
      // Draw the green box
      rectangle(Button->x, Button->y + 1, Button->w, Button->h/2,
        Button->Status[1].Color.r,
        Button->Status[1].Color.g,
        Button->Status[1].Color.b);

      // Set the background colour and then display the text
      setBackColour(Button->Status[1].Color.r,
                    Button->Status[1].Color.g,
                    Button->Status[1].Color.b);
      TextMid2(Button->x+Button->w/2, Button->y+Button->h*3/16, line2, &font_dejavu_sans_20);	
    }
  }
  else                                              // One line only
  {
    if ((CurrentMenu <= 9) && (CurrentMenu != 4))
    {
      TextMid2(Button->x+Button->w/2, Button->y+Button->h/2, label, &font_dejavu_sans_28);
    }
    else if (CurrentMenu == 41)  // Keyboard
    {
      TextMid2(Button->x+Button->w/2, Button->y+Button->h/2 - hscreen / 64, label, &font_dejavu_sans_28);
    }
    else // fix text size at 20
    {
      TextMid2(Button->x+Button->w/2, Button->y+Button->h/2, label, &font_dejavu_sans_20);
    }
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

/*
Input device name: "ADS7846 Touchscreen"
Supported events:
  Event type 0 (Sync)
  Event type 1 (Key)
    Event code 330 (Touch)
  Event type 3 (Absolute)
    Event code 0 (X)
     Value      0
     Min        0
     Max     4095
    Event code 1 (Y)
     Value      0
     Min        0
     Max     4095
    Event code 24 (Pressure)
     Value      0
     Min        0
     Max      255
*/

int getTouchScreenDetails(int *screenXmin,int *screenXmax,int *screenYmin,int *screenYmax)
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

  if (((strcmp(DisplayType, "Element14_7") == 0) || (touchscreen_present == true))
      && (strcmp(DisplayType, "dfrobot5") != 0))   // Browser or Element14_7, but not dfrobot5
  {
    // Program flow blocks here until there is a touch event
    rb = read(fd, ev, sizeof(struct input_event) * 64);

    *rawX = -1;
    *rawY = -1;
    int StartTouch = 0;

    for (i = 0;  i <  (rb / sizeof(struct input_event)); i++)
    {
      if (ev[i].type ==  EV_SYN)
      {
        //printf("Event type is %s%s%s = Start of New Event\n", KYEL, events[ev[i].type], KWHT);
      }

      else if (ev[i].type == EV_KEY && ev[i].code == 330 && ev[i].value == 1)
      {
        StartTouch = 1;
        //printf("Event type is %s%s%s & Event code is %sTOUCH(330)%s & Event value is %s1%s = Touch Starting\n",
        //        KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,KWHT);
      }

      else if (ev[i].type == EV_KEY && ev[i].code == 330 && ev[i].value == 0)
      {
        StartTouch=0;
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
        printf("7 inch Touchscreen Touch Event: rawX = %d, rawY = %d\n", *rawX, *rawY);
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
        printf("DFRobot Touch Event: rawX = %d, rawY = %d, rawPressure = %d\n", *rawX, *rawY, *rawPressure);
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
      TouchTrigger = 0;
      printf("Touch rawX = %d, rawY = %d, rawPressure = %d\n", *rawX, *rawY, *rawPressure);
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
    else if (MouseClickForAction == true)
    {
      *rawX = mouse_x;
      *rawY = mouse_y;
      *rawPressure = 0;
      MouseClickForAction = false;
      printf("Mouse rawX = %d, rawY = %d, rawPressure = %d\n", *rawX, *rawY, *rawPressure);
      return 1;
    }
    else if (FalseTouch == true)
    {
      *rawX = web_x;
      *rawY = web_y;
      *rawPressure = 0;
      FalseTouch = false;
      printf("False Touch prompted by other event\n");
      return 1;
    }
    else
    {
      usleep(1000);
    }
  }
  return 0;
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


void *WaitMouseEvent(void * arg)
{
  int x = 0;
  int y = 0;
  int scroll = 0;
  int fd;

  bool left_button_action = false;

  if ((fd = open("/dev/input/event0", O_RDONLY)) < 0)
  {
    perror("evdev open");
    exit(1);
  }
  struct input_event ev;

  while(1)
  {
    read(fd, &ev, sizeof(struct input_event));

    if (ev.type == 2)  // EV_REL
    {
      if (ev.code == 0) // x
      {
        x = x + ev.value;
        if (x < 0)
        {
          x = 0;
        }
        if (x > 799)
        {
          x = 799;
        }
        //printf("value %d, type %d, code %d, x_pos %d, y_pos %d\n",ev.value,ev.type,ev.code, x, y);
        //printf("x_pos %d, y_pos %d\n", x, y);
        mouse_active = true;
        draw_cursor2(x, y);
      }
      else if (ev.code == 1) // y
      {
        y = y - ev.value;
        if (y < 0)
        {
          y = 0;
        }
        if (y > 479)
        {
          y = 479;
        }
        //printf("value %d, type %d, code %d, x_pos %d, y_pos %d\n",ev.value,ev.type,ev.code, x, y);
        //printf("x_pos %d, y_pos %d\n", x, y);
        mouse_active = true;
        while (image_complete == false)  // Wait for menu to be drawn
        {
          usleep(1000);
        }
        draw_cursor2(x, y);
      }
      else if (ev.code == 8) // scroll wheel
      {
        scroll = scroll + ev.value;
        //printf("value %d, type %d, code %d, scroll %d\n",ev.value,ev.type,ev.code, scroll);
      }
      else
      {
        //printf("value %d, type %d, code %d\n", ev.value, ev.type, ev.code);
      }
    }

    else if (ev.type == 4)  // EV_MSC
    {
      if (ev.code == 4) // ?
      {
        if (ev.value == 589825)
        { 
          //printf("value %d, type %d, code %d, left mouse click \n", ev.value, ev.type, ev.code);
          //printf("Waiting for up or down signal\n");
          left_button_action = true;
        }
        if (ev.value == 589826)
        { 
          printf("value %d, type %d, code %d, right mouse click \n", ev.value, ev.type, ev.code);
        }
      }
    }
    else if (ev.type == 1)
    {
      //printf("value %d, type %d, code %d\n", ev.value, ev.type, ev.code);

      if ((left_button_action == true) && (ev.code == 272) && (ev.value == 1) && (mouse_active == true))
      {
        mouse_x = x;
        mouse_y = 479 - y;
        MouseClickForAction = true;
      }
      left_button_action = false;
    }
    else
    { 
      //printf("value %d, type %d, code %d\n", ev.value, ev.type, ev.code);
    }
  }
}


void handle_mouse()
{
  // First check if mouse is connected
  if (CheckMouse() != 0)    // Mouse not connected
  {
    return;
  }
  mouse_connected = true;
  printf("Starting Mouse Thread\n");
  pthread_create (&thmouse, NULL, &WaitMouseEvent, NULL);
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
  printf("Exiting WebClickListener\n");
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


void togglewebcontrol()
{
  if(webcontrol == false)
  {
    SetConfigParam(PATH_PCONFIG, "webcontrol", "enabled");
    webcontrol = true;
    if (webclicklistenerrunning == false)
    {
      printf("Creating thread as webclick listener is not running\n");
      pthread_create (&thwebclick, NULL, &WebClickListener, NULL);
      printf("Created webclick listener thread\n");
    }
  }
  else
  {
    system("cp /home/pi/rpidatv/scripts/images/web_not_enabled.png /home/pi/tmp/screen.png");
    SetConfigParam(PATH_PCONFIG, "webcontrol", "disabled");
    webcontrol = false;
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


void ShowMenuText()
{
  // Called to display software update information in Menu 33 and file information on Menu 4

  // Initialise and calculate the text display
  int line;
  const font_t *font_ptr = &font_dejavu_sans_28;
  const font_t *font_ptr20 = &font_dejavu_sans_20;
  int txtht =  font_ptr->ascent;
  int linepitch = (14 * txtht) / 10;

  setForeColour(255, 255, 255);    // White text
  setBackColour(0, 0, 0);          // on Black

  // Display Text
  if (CurrentMenu != 4)
  {
    for (line = 0; line < 5; line = line + 1)
    {
      // printf("%s-%d\n", MenuText[line], strlen(MenuText[line]));
      Text2(wscreen / 12, hscreen - (3 + line) * linepitch, MenuText[line], font_ptr);
    }
  }
  else                            // Smaller text for Menu 4
  {
    for (line = 0; line < 5; line = line + 1)
    {
      // printf("%s-%d\n", MenuText[line], strlen(MenuText[line]));
      Text2(wscreen / 40, hscreen - (3 + line) * linepitch, MenuText[line], font_ptr20);
    }
  }
  UpdateWeb();
}

void ShowTitle()
{
  // Set the colours
  if (CurrentMenu == 1)               // Main menu
  {
    setForeColour(0, 0, 0);          // Black text
    setBackColour(255, 255, 255);    // on White
  }
  else                               // All others
  {
    setForeColour(255, 255, 255);    // White text
    setBackColour(0, 0, 0);          // on Black
  }

  const font_t *font_ptr = &font_dejavu_sans_28;
  int txtht =  font_ptr->ascent;
  int linepitch = (12 * txtht) / 10;
  int linenumber = 1;

  // Display Text
  TextMid2(wscreen / 2.0, hscreen - linenumber * linepitch, MenuTitle[CurrentMenu], &font_dejavu_sans_28);
}

void UpdateWindow()
// Paint each defined button and the title on the current Menu
{
  int i;
  int first;
  int last;

  image_complete = false;

  // Set the background colour
  if (CurrentMenu == 1)           // Main Menu White
  {
    setBackColour(255, 255, 255);
  }
  else                            // All others Black
  {
    setBackColour(0, 0, 0);
  }
  
  if ((CurrentMenu != 38) && (CurrentMenu != 41)) // If not yes/no or the keyboard
  {
    clearScreen();
    clearScreen();  // Second clear screen sometimes required on return from fbi images
  }
  // Draw the backgrounds for the smaller menus
  if ((CurrentMenu >= 11) && (CurrentMenu <= 40))  // 10-button menus
  {
    rectangle(10, 12, wscreen - 18, hscreen * 2 / 6 + 12, 127, 127, 127);
  }

  if ((CurrentMenu >= 42) && (CurrentMenu <= 47))  // 15-button menus
  {
    rectangle(10, 12, wscreen - 18, hscreen * 3 / 6 + 12, 127, 127, 127);
  }

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

  // Show the title and any required text
  ShowTitle();
  if ((CurrentMenu == 4) || (CurrentMenu == 33))  // File or update menus
  {
    ShowMenuText();
  }
  refreshMouseBackground();
  draw_cursor_foreground(mouse_x, mouse_y);
  UpdateWeb();
  image_complete = true;
}

void ApplyTXConfig()
{
  // Called after any top row button changes to work out config required for a.sh
  // Based on decision tree
  if (strcmp(CurrentTXMode, "Carrier") == 0)
  {
    strcpy(ModeInput, "CARRIER");
  }
  else if (((strcmp(CurrentModeOP, "LIMEMINI") == 0) || (strcmp(CurrentModeOP, "LIMEDVB") == 0)
         || (strcmp(CurrentModeOP, "LIMEUSB") == 0)) && (strcmp(CurrentSource, "HDMI") == 0))
  {
    // Don't correct Elgato HDMI Capture for Lime
    return;
  }
  else if ((strcmp(CurrentModeOP, "JLIME") == 0) || (strcmp(CurrentModeOP, "JEXPRESS") == 0))
  {
    if (strcmp(CurrentSource, "HDMI") == 0)
    {
      strcpy(ModeInput, "JHDMI");
    }
    else if (strcmp(CurrentSource, "Pi Cam") == 0)
    {
      strcpy(ModeInput, "JCAM");
    }
    else if (strcmp(CurrentSource, "Webcam") == 0)
    {
      strcpy(ModeInput, "JWEBCAM");
    }
    else if (strcmp(CurrentSource, "TestCard") == 0)
    {
      strcpy(ModeInput, "JCARD");
    }
    else                             // Default
    {
      strcpy(ModeInput, "JCARD");
    }
  }
  else if (strcmp(CurrentModeOP, "STREAMER") == 0) //          STREAMER Modes
  {
    if (strcmp(CurrentSource, "HDMI") == 0)
    {
      strcpy(ModeInput, "HDMI");
    }
    else if (strcmp(CurrentSource, "CompVid") == 0)
    {
      strcpy(ModeInput, "ANALOGCAM");
    }
    else if (strcmp(CurrentSource, "Pi Cam") == 0)
    {
      strcpy(ModeInput, "CAMMPEG-2");
    }
    else if (strcmp(CurrentSource, "Webcam") == 0)
    {
      strcpy(ModeInput, "WEBCAMMPEG-2");
    }
    else if (strcmp(CurrentSource, "TestCard") == 0)
    {
      strcpy(ModeInput, "CARDMPEG-2");
    }
    else                             // Default
    {
      strcpy(ModeInput, "CARDMPEG-2");
    }
  }
  else if (strcmp(CurrentModeOP, "PLUTO") == 0) //          PLUTO Modes
  {
    if (strcmp(CurrentEncoding, "IPTS in") == 0)
    {
      strcpy(ModeInput, "IPTSIN");
    }
    else if (strcmp(CurrentEncoding, "TS File") == 0)
    {
      strcpy(ModeInput, "FILETS");
    }

    if ((strcmp(CurrentEncoding, "IPTS in") != 0)      // Only check if not IPTS, or not TS File input
    &&  (strcmp(CurrentEncoding, "IPTS in H264") != 0)
    &&  (strcmp(CurrentEncoding, "IPTS in H265") != 0)
    &&  (strcmp(CurrentEncoding, "TS File") != 0))
    {
      if (strcmp(CurrentEncoding, "MPEG-2") == 0)
      {
        MsgBox2("MPEG-2 encoding not available with Pluto", "Selecting H264");
        wait_touch();
        strcpy(CurrentEncoding, "H264");
        SetConfigParam(PATH_PCONFIG, "encoding", CurrentEncoding);
      }
      if (strcmp(CurrentSource, "HDMI") == 0)
      {
        strcpy(ModeInput, "HDMI");
      }
      else if (strcmp(CurrentSource, "CompVid") == 0)
      {
        strcpy(ModeInput, "ANALOGCAM");
      }
      else if (strcmp(CurrentSource, "Pi Cam") == 0)
      {
        strcpy(ModeInput, "CAMH264");
      }
      else if (strcmp(CurrentSource, "Webcam") == 0)
      {
        strcpy(ModeInput, "WEBCAMH264");
      }
      else if (strcmp(CurrentSource, "TestCard") == 0)
      {
        strcpy(ModeInput, "CARDH264");
      }
      else if (strcmp(CurrentSource, "PiScreen") == 0)
      {
        strcpy(ModeInput, "DESKTOP");
      }
      else if (strcmp(CurrentSource, "Contest") == 0)
      {
        strcpy(ModeInput, "CONTEST");
      }
      else                             // Default
      {
        strcpy(ModeInput, "CARDH264");
      }
    }
  }
  else  // For all modes except Carrier, Jetson, Pluto and Streaming
  {
    if (strcmp(CurrentEncoding, "IPTS in") == 0)
    {
      strcpy(ModeInput, "IPTSIN");
    }
    else if (strcmp(CurrentEncoding, "IPTS in H264") == 0)
    {
      strcpy(ModeInput, "IPTSIN264");
    }
    else if (strcmp(CurrentEncoding, "IPTS in H265") == 0)
    {
      strcpy(ModeInput, "IPTSIN265");
    }
    else if (strcmp(CurrentEncoding, "TS File") == 0)
    {
      strcpy(ModeInput, "FILETS");
    }
    else if (strcmp(CurrentEncoding, "H264") == 0)   // H264, so define definition by format (max 720p)
    {
//      if (strcmp(CurrentFormat, "1080p") == 0)
//      {
//        strcpy(CurrentFormat, "720p");
//      }
//      if (strcmp(CurrentFormat, "720p") == 0)
//      {
//        if (CheckC920() == 1)
//        {
//          if (strcmp(CurrentSource, "C920") == 0)
//          {
//            strcpy(ModeInput, "C920HDH264");
//          }
//          else
//          {
//            strcpy(CurrentFormat, "16:9");
//          }
//        }
//      }
      if (strcmp(CurrentFormat, "16:9") == 0)
      {
        if (strcmp(CurrentSource, "CompVid") == 0)
        {
          strcpy(ModeInput, "ANALOGCAM");
        }
        else if (strcmp(CurrentSource, "Webcam") == 0)
        {
          strcpy(ModeInput, "WEBCAMH264");
        }
        else if (strcmp(CurrentSource, "TestCard") == 0)
        {
          strcpy(ModeInput, "CARDH264");
        }
        else if (strcmp(CurrentSource, "Pi Cam") == 0)
        {
          strcpy(ModeInput, "CAMH264");
        }
        else
        {
//          strcpy(CurrentFormat, "4:3");
        }        
      }
//      if (strcmp(CurrentFormat, "4:3") == 0)
      {
        if (strcmp(CurrentSource, "Pi Cam") == 0)
        {
          strcpy(ModeInput, "CAMH264");
        }
        else if (strcmp(CurrentSource, "CompVid") == 0)
        {
          strcpy(ModeInput, "ANALOGCAM");
        }
        else if (strcmp(CurrentSource, "TestCard") == 0)
        {
          strcpy(ModeInput, "CARDH264");
        }
        else if (strcmp(CurrentSource, "PiScreen") == 0)
        {
          strcpy(ModeInput, "DESKTOP");
        }
        else if (strcmp(CurrentSource, "Contest") == 0)
        {
          strcpy(ModeInput, "CONTEST");
        }
        else if (strcmp(CurrentSource, "Webcam") == 0)
        {
          strcpy(ModeInput, "WEBCAMH264");
        }
        else // Shouldn't happen
        {
          strcpy(ModeInput, "DESKTOP");
        }
      }
    }
    else  // MPEG-2.  Check for C920 first
    {
      if ((strcmp(CurrentSource, "C920") == 0) && (CheckC920() == 1))
      {
        if (strcmp(CurrentFormat, "1080p") == 0)
        {
          strcpy(ModeInput, "C920FHDH264");
        }
        else if (strcmp(CurrentFormat, "720p") == 0)
        {
          strcpy(ModeInput, "C920HDH264");
        }
        else
        {
          strcpy(ModeInput, "C920H264");
        }
      }
      else  // Not C920
      {
        if (strcmp(CurrentFormat, "1080p") == 0)
        {
          MsgBox2("1080p only available with C920 Webcam"
            , "Please select another mode");
          wait_touch();
        }
        if (strcmp(CurrentFormat, "720p") == 0)
        {
          if (strcmp(CurrentSource, "Pi Cam") == 0)
          {
            strcpy(ModeInput, "CAMHDMPEG-2");
          }
          else if (strcmp(CurrentSource, "CompVid") == 0)
          {
            MsgBox2("720p not available with Comp Vid", "Selecting the test card");
            wait_touch();
            strcpy(ModeInput, "CARDHDMPEG-2");
          }
          else if (strcmp(CurrentSource, "TCAnim") == 0)
          {
            MsgBox2("720p not available with TCAnim", "Selecting the test card");
            wait_touch();
            strcpy(ModeInput, "CARDHDMPEG-2");
          }
          else if (strcmp(CurrentSource, "TestCard") == 0)
          {
            strcpy(ModeInput, "CARDHDMPEG-2");
          }
          else if (strcmp(CurrentSource, "PiScreen") == 0)
          {
            MsgBox2("720p not available with PiScreen", "Selecting the test card");
            wait_touch();
            strcpy(ModeInput, "CARDHDMPEG-2");
          }
          else if (strcmp(CurrentSource, "Contest") == 0)
          {
            MsgBox2("720p not available with Contest", "Selecting the test card");
            wait_touch();
            strcpy(ModeInput, "CARDHDMPEG-2");
          }
          else if (strcmp(CurrentSource, "Webcam") == 0)
          {
            strcpy(ModeInput, "WEBCAMHDMPEG-2");
          }
          else if (strcmp(CurrentSource, "C920") == 0)
          {
            strcpy(ModeInput, "WEBCAMHDMPEG-2");
          }
          else  // shouldn't happen
          {
            strcpy(ModeInput, "CARDHDMPEG-2");
          }
        }

        if (strcmp(CurrentFormat, "16:9") == 0)
        {
          if (strcmp(CurrentSource, "Pi Cam") == 0)
          {
            strcpy(ModeInput, "CAM16MPEG-2");
          }
          else if (strcmp(CurrentSource, "CompVid") == 0)
          {
            strcpy(ModeInput, "ANALOG16MPEG-2");
          }
          else if (strcmp(CurrentSource, "TCAnim") == 0)
          {
            MsgBox2("TCAnim not available with MPEG-2", "Selecting the test card");
            wait_touch();
            strcpy(ModeInput, "CARD16MPEG-2");
          }
          else if (strcmp(CurrentSource, "TestCard") == 0)
          {
            strcpy(ModeInput, "CARD16MPEG-2");
          }
          else if (strcmp(CurrentSource, "PiScreen") == 0)
          {
            MsgBox2("16:9 not available with PiScreen", "Selecting the test card");
            wait_touch();
            strcpy(ModeInput, "CARD16MPEG-2");
          }
          else if (strcmp(CurrentSource, "Contest") == 0)
          {
            MsgBox2("16:9 not available with Contest", "Selecting 4:3");
            wait_touch();
            strcpy(ModeInput, "CONTESTMPEG-2");
          }
          else if (strcmp(CurrentSource, "Webcam") == 0)
          {
            strcpy(ModeInput, "WEBCAM16MPEG-2");
          }
          else if (strcmp(CurrentSource, "C920") == 0)
          {
            strcpy(ModeInput, "WEBCAM16MPEG-2");
          }
          else  // shouldn't happen
          {
            strcpy(ModeInput, "CARD16MPEG-2");
          }
        }

        if (strcmp(CurrentFormat, "4:3") == 0)
        {
          if (strcmp(CurrentSource, "Pi Cam") == 0)
          {
            strcpy(ModeInput, "CAMMPEG-2");
          }
          else if (strcmp(CurrentSource, "CompVid") == 0)
          {
            strcpy(ModeInput, "ANALOGMPEG-2");
          }
          else if (strcmp(CurrentSource, "TCAnim") == 0)
          {
            strcpy(ModeInput, "CARDMPEG-2");
            MsgBox2("TCAnim not available with MPEG-2", "Selecting Test Card F instead");
            wait_touch();
          }
          else if (strcmp(CurrentSource, "TestCard") == 0)
          {
            strcpy(ModeInput, "CARDMPEG-2");
          }
          else if (strcmp(CurrentSource, "PiScreen") == 0)
          {
            strcpy(ModeInput, "CARDMPEG-2");
            MsgBox2("PiScreen not available with MPEG-2", "Selecting Test Card F instead");
            wait_touch();
          }
          else if (strcmp(CurrentSource, "Contest") == 0)
          {
            strcpy(ModeInput, "CONTESTMPEG-2");
          }
          else if (strcmp(CurrentSource, "Webcam") == 0)
          {
            strcpy(ModeInput, "WEBCAMMPEG-2");
          }
          else if (strcmp(CurrentSource, "C920") == 0)
          {
            strcpy(ModeInput, "WEBCAMMPEG-2");
          }
          else  // Shouldn't happen but give them Test Card F
          {
            strcpy(ModeInput, "CARDMPEG-2");
          }
        }
      }
    }
  }
  // Now save the change and make sure that all the config is correct
  char Param[15] = "modeinput";
  SetConfigParam(PATH_PCONFIG, Param, ModeInput);
  printf("a.sh will be called with %s\n", ModeInput);

  strcpy(Param, "format");
  SetConfigParam(PATH_PCONFIG, Param, CurrentFormat);
  printf("a.sh will be called with format %s\n", CurrentFormat);

  // Load the Pi Cam driver for CAMMPEG-2 and Streaming modes
  //printf("TESTING FOR STREAMER\n");
  if ((strcmp(ModeInput,"CAMMPEG-2")==0)
    ||(strcmp(ModeInput,"CAM16MPEG-2")==0)
    ||(strcmp(ModeInput,"CAMHDMPEG-2")==0))
  {
    system("sudo modprobe bcm2835_v4l2");
  }
  // Replace Contest Numbers or Test Card with BATC Logo
  system("sudo fbi -T 1 -noverbose -a \"/home/pi/rpidatv/scripts/images/BATC_Black.png\" >/dev/null 2>/dev/null");
  system("(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
}


void EnforceValidTXMode()
{
  char Param[15]="modulation";

  if ((strcmp(CurrentModeOP, "LIMEUSB") != 0)
       && (strcmp(CurrentModeOP, "LIMEMINI") != 0)
       && (strcmp(CurrentModeOP, "LIMEDVB") != 0)
       && (strcmp(CurrentModeOP, "MUNTJAC") != 0)
       && (strcmp(CurrentModeOP, "STREAMER") != 0)
       && (strcmp(CurrentModeOP, "COMPVID") != 0)
       && (strcmp(CurrentModeOP, "IP") != 0)
       && (strcmp(CurrentModeOP, "JLIME") != 0)
       && (strcmp(CurrentModeOP, "JEXPRESS") != 0)
       && (strcmp(CurrentModeOP, "PLUTO") != 0)) // If not any of these, then not DVB-S2-capable
  {
    if ((strcmp(CurrentTXMode, TabTXMode[0]) != 0) && (strcmp(CurrentTXMode, TabTXMode[1]) != 0))  // Not DVB-S and not Carrier
    {
      strcpy(CurrentTXMode, TabTXMode[0]);
      SetConfigParam(PATH_PCONFIG, Param, CurrentTXMode);
    }
  }
}

void EnforceValidFEC()
{
  int FECChanged = 0;
  char Param[7]="fec";
  char Value[7];

  if ((strcmp(CurrentTXMode, TabTXMode[0]) == 0) || (strcmp(CurrentTXMode, TabTXMode[1]) == 0)
   || (strcmp(CurrentTXMode, TabTXMode[6]) == 0)) // Carrier, DVB-S or DVB-T
  {
    if (fec > 10)  // DVB-S2 FEC selected for DVB-S or DVB-T transmit mode
    {
      if((fec == 14) || (fec == 13) || (fec == 12)) // 1/4, 1/3, or 1/2
      {
        fec = 1; // 1/2
      }
      else if((fec == 35) || (fec == 23)) // 3/5 or 2/3
      {
        fec = 2; // 2/3
      }
      else if(fec == 34) // 3/4
      {
        fec = 3; // 3/4
      }
      else if(fec == 56) // 5/6
      {
        fec = 5; // 5/6
      }
      else
      {
        fec = 7; // 7/8
      }
      FECChanged = 1;
    }
  }
  if (strcmp(CurrentTXMode, TabTXMode[2]) == 0)  // DVB-S2 QPSK
  {
    if (fec < 9)  // DVB-S FEC selected for DVB-S2 QPSK
    {
      if(fec == 1)       // 1/2
      {
        fec = 12;        // 1/2
      }
      else if(fec == 2)  // 2/3
      {
        fec = 23;        // 2/3
      }
      else if(fec == 3)  // 3/4
      {
        fec = 34;        // 3/4
      }
      else if(fec == 5)  // 5/6
      {
        fec = 56;        // 5/6
      }
      else               // 7/8
      {
        fec = 91;        // 9/10
      }
      FECChanged = 1;
    }
  }
  if (strcmp(CurrentTXMode, TabTXMode[3]) == 0)  // 8PSK
  {
    if (fec < 20)
    {
      if((fec == 1) || (fec == 14) || (fec == 13) || (fec == 12))       // 1/3, 1/4 or 1/2
      {
        fec = 35;        // 3/5
      }
      else if(fec == 2)  // 2/3
      {
        fec = 23;        // 2/3
      }
      else if(fec == 3)  // 3/4
      {
        fec = 34;        // 3/4
      }
      else if(fec == 5)  // 5/6
      {
        fec = 56;        // 5/6
      }
      else               // 7/8
      {
        fec = 91;        // 9/10
      }
      FECChanged = 1;
    }
  }
  if (strcmp(CurrentTXMode, TabTXMode[4]) == 0)  // 16APSK
  {
    if ((fec < 20) || (fec == 35))
    {
      if((fec == 1) || (fec == 2) || (fec == 14) || (fec == 13) || (fec == 12) || (fec == 35))   // 1/3, 1/4, 1/2 or 2/3 or 3/5
      {
        fec = 23;        // 2/3
      }
      else if(fec == 3)  // 3/4
      {
        fec = 34;        // 3/4
      }
      else if(fec == 5)  // 5/6
      {
        fec = 56;        // 5/6
      }
      else               // 7/8
      {
        fec = 91;        // 9/10
      }
      FECChanged = 1;
    }
  }
  if (strcmp(CurrentTXMode, TabTXMode[5]) == 0)  // 32APSK
  {
    if ((fec < 30) || (fec == 35))
    {
      if((fec == 1) || (fec == 2) || (fec == 3) || (fec == 14) || (fec == 13) 
        || (fec == 12) || (fec == 35) || (fec == 23))   // 1/3, 1/4, 1/2, 2/3, 3/4 or 3/5
      {
        fec = 34;        // 3/4
      }
      else if(fec == 5)  // 5/6
      {
        fec = 56;        // 5/6
      }
      else               // 7/8
      {
        fec = 91;        // 9/10
      }
      FECChanged = 1;
    }
  }
  // and save to config
  if (FECChanged == 1)
  {
    sprintf(Value, "%d", fec);
    SetConfigParam(PATH_PCONFIG, Param, Value);
  }
}


/***************************************************************************//**
 * @brief Lists (ls -l) files in a directory and puts up to 98 entries in an array
 *
 * @param Path is directory path.  Should start and finish with /
 *        FirstFile is the alpabetical list position of the first file to be returned.  Starts with 1
 *        Last File must be between Firstfile and FirstFile + 97
 *        FileArray puts the results in [2] to [2 + 97]
 *
 * @return (int) total number of file entries in directory
*******************************************************************************/

int ListFilestoArray(char *Path, int FirstFile, int LastFile, char FileArray[101][255], char FileTypeArray[101][2]) 
{
  int i;
  FILE *fp;
  char response[300];
  char lscommand[511];
  int LineCount = 1;      // First line of ls response not used
  int FileIndexCount = 2; // 0 is title, 1 is parent
  int TypeLineCount = 0;  // Used for check of File Types

  // Clear the FileArray
  for (i = 0; i <= 100; i++)
  {
    strcpy(FileArray[i], "");
    strcpy(FileTypeArray[i], "");
  }

  // Check that 98 or less files have been requested
  if (LastFile >= FirstFile + 97)
  {
    LastFile = FirstFile + 97;
  }

  // Read the filenames in the directory
  strcpy(lscommand, "ls -1 ");   // filenames only
  strcat(lscommand, Path);


  fp = popen(lscommand, "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  // Read the output a line at a time - store it
  while (fgets(response, 300, fp) != NULL)
  {
    response[strlen(response) - 1] = '\0';  // Strip trailing cr
    //printf("Line %d %s\n", LineCount, response);
    if ((LineCount >= FirstFile) && (LineCount <= LastFile))
    {
      strcpy(FileArray[FileIndexCount], response);
      FileIndexCount++;
    }
    LineCount++;
  }
  pclose(fp);

  // Read the filetypes in the directory
  FileIndexCount = 2;
  strcpy(lscommand, "ls -l -L ");  // Long listing and dereference symbolic links
  strcat(lscommand, Path);

  fp = popen(lscommand, "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  // Read the output a line at a time - store it
  while (fgets(response, 300, fp) != NULL)
  {
    response[strlen(response) - 1] = '\0';  // Strip trailing cr
    response[1] = '\0';                     //  Limit length to 1 character
    //printf("Line %d file type %s\n", TypeLineCount, response);
    if ((TypeLineCount >= FirstFile) && (TypeLineCount <= LastFile))
    {
      //printf("FileIndexCount = %d\n", FileIndexCount);
      strcpy(FileTypeArray[FileIndexCount], response);
      FileIndexCount++;
    }
    TypeLineCount++;
  }
  pclose(fp);

  if (TypeLineCount != LineCount)
  {
    printf("FILE INDEXING ERROR\n");
  }
  return LineCount - 1;
}


/***************************************************************************//**
 * @brief Displays a File Dialogue to allow file selection
 *
 * @param InitialPath is directory path.  Should start and finish with /
 *        InitialFile is file to be highlighted on entry
 *        SelectedPath is path selected
 *        SelectedFile is file selected
 *        if directory == 1, then output is a directory
 *        All params should be declared [255] in calling function

 * @return (int) 0 if no change, 1 if changed
*******************************************************************************/

int SelectFileUsingList(char *InitialPath, char *InitialFile, char *SelectedPath, char *SelectedFile, int directory)
{
  int NumberofFiles;
  char DisplayFileList[101][63];
  char FileArray[101][255];
  char FileTypeArray[101][2];
  char shortfilename[255];
  int i;
  bool choosing = true;
  bool noMatch = true;
  int ListingSection = 1;
  int response = 0;
  int MatchTries;
  int InitialSelection = 0;
  int ChooseListLength;
  int ThisFileListLength;
  bool deleting = true;

  strcpy(SelectedFile, InitialFile);

  // Check InitialPath exists.  If not, set path to /home/pi
  DIR* dir = opendir(InitialPath);
  if (dir)                               // directory exists
  {
    closedir(dir);
    strcpy(SelectedPath, InitialPath);
  }
  else if (ENOENT == errno)              // directory does not exist
  {
    printf("Directory does not exist, setting to /home/pi\n");
    strcpy(SelectedPath, "/home/pi/");
  }
  else                                   // Directory Check Error
  {
    printf("Directory Check Error\n");
  }

  // Read the file list to check for a match with the initial file
  if (strlen(InitialFile) != 0)
  {
    while (noMatch)
    {
      NumberofFiles = ListFilestoArray(SelectedPath, ((ListingSection - 1) * 98) + 1, ListingSection * 98, FileArray, FileTypeArray);

      // Calculate the number of lines to check for a match within this 98
      if (NumberofFiles > (ListingSection * 98))  // Another section to check after this
      {
         MatchTries = 98;
      }
      else
      {
        MatchTries = NumberofFiles - ((ListingSection - 1) * 98);
      }

      // Check for a match on each line
      for (i = 2; i <= (MatchTries + 1); i++)
      {
        if (FileTypeArray[i][0] == '-')    // Only check if a file not a directory
        {
          if (strcmp(InitialFile, FileArray[i]) == 0)
          {
            InitialSelection = i;
            noMatch = false;
          }
        }
      }

      if ((noMatch) && (NumberofFiles > ListingSection * 98))  // End of this 98, but there are more
      {
        ListingSection++;
      }
      else                                                     // End of the list
      {
        if (noMatch)
        {
          noMatch = false;
          InitialSelection = 0;
          ListingSection = 1;
        }
      }
    }
  }
  else
  {
    InitialSelection = 0;
  }

  // At this point ListingSection is valid and InitialSelection is set.

  while (choosing)
  {
    NumberofFiles = ListFilestoArray(SelectedPath, ((ListingSection - 1) * 98) + 1, ListingSection * 98, FileArray, FileTypeArray);
    deleting = true;

    if (NumberofFiles > (ListingSection * 98))  // Another section to check after this
    {
      ChooseListLength = 100;
      ThisFileListLength = 98;
      strcpy(DisplayFileList[100], "More files...");
    }
    else
    {
      ChooseListLength = NumberofFiles - ((ListingSection - 1) * 98) + 1;
      ThisFileListLength = NumberofFiles - ((ListingSection - 1) * 98);
    }

    if (ListingSection == 1)
    {
      strcpy(DisplayFileList[1], "Up to Parent Directory");
    }
    else
    {
      strcpy(DisplayFileList[1], "Back to Previous Files");
    }

    for (i = 2; i <= (ThisFileListLength + 1); i++)
    {
      strcpyn(shortfilename, FileArray[i], 55);

      if (FileTypeArray[i][0] == 'd')                    // This is a directory
      {
        strcpy(DisplayFileList[i], "<DIR> ");
        strcat(DisplayFileList[i], shortfilename);
      }
      else                                           // Normal File
      {
        strcpy(DisplayFileList[i], shortfilename);
      }
    }

    snprintf(DisplayFileList[0], 63, "%s", SelectedPath);
    response = SelectFromList(InitialSelection, DisplayFileList, ChooseListLength);

    if ((NumberofFiles > 98) && (response == 100))  // next 98 files requested
    {
      ListingSection++;
    }
    else if ((response >= 2) && (response <= (ChooseListLength + 1)))  // valid choice
    {
        printf("Response for analysis is -%s-\n", FileArray[response]);

      printf("Chosen File was %s\n", DisplayFileList[response]);
      if (FileTypeArray[response][0] == 'd')                           // Directory selected
      {
        strcat(SelectedPath, FileArray[response]);
        strcat(SelectedPath, "/");
        ListingSection = 1;
        InitialSelection = 0;
        printf("New selected path is %s\n", SelectedPath);
      }
      else
      {
        strcpy(SelectedFile, DisplayFileList[response]);
        printf("Chosen File was copied as %s\n", DisplayFileList[response]);
        choosing = false;
      }
    }
    else if (response == 0)                                      // Cancel or Select with no highlight
    {
      if ((strcmp(SelectedPath, InitialPath) != 0) && (directory == 1))     // Path has changed
      {                                                                    // and we are accepting directory answers
        // Selected path will be correct
        strcpy(SelectedFile, "");
      }
      else
      {
        strcpy(SelectedFile, InitialFile);
        strcpy(SelectedPath, InitialPath);
      }
      choosing = false;
    }

    if((response == 1) && (ListingSection == 1))                // Go to parent folder
    {
      if (strlen(SelectedPath) > 1 )                            // Only change if path is not currently "/"
      {
        InitialSelection = 0;                                   // Clear any selection

        for (i = strlen(SelectedPath); i > 1; i = i - 1)        // From the right hand end of the path
        {
          if (deleting)                                         // if the slash has not been reached
          {
            SelectedPath[i - 1] = '\0';                         // delete the character
          }
          if (SelectedPath[i - 2] == '/')                       // Slash has been reached
          {
            deleting = false;                                   // So stop deleting
          }
        }
      }
    }
  }
  printf("Selected File = %s\n", SelectedFile);
  printf("Selected Path = %s\n", SelectedPath);

  if ((strcmp(SelectedFile, InitialFile) == 0) && (strcmp(SelectedPath, InitialPath) == 0))
  {
    // No change
    return 0;
  }
  else
  {
    return 1;
  }
  return 0;
}


/***************************************************************************//**
 * @brief Displays a Dialogue to allow selection from an array
 *
 * @param (int) CurrentSelection:  -1 list only, selection not possible
 *                                 0  No current selection, returns 0
 *                                 1 - 100 initial selection for highlighting
 *        char ListEntry[101][63]: [0] is title 
 *                                 [1] - [100] are list entries
 *        int ListLength:          Number of entries to display
 *
 * @return (int) Cancel returns "Current Selection" (1 - 100) or 0 if none
 *               Select returns new selection (1 - 100)
 *               Select with no highlighted selection returns 0
*******************************************************************************/

int SelectFromList(int CurrentSelection, char ListEntry[101][63], int ListLength)
{
  char TableNumberText[7];
  int ButtonWidth = 160;
  int ButtonHeight = 65;
  char Button1Caption[15] = "Previous Page";
  char Button2Caption[15] = "Cancel";
  char Button3Caption[15] = "Select";
  char Button4Caption[15] = "Next Page";
  char Button5Caption[15] = "Exit";
  int ButtonY = 10;
  int Button1X = 10;
  int Button2X = 217;
  int Button3X = 423;
  int Button4X = 630;
  int Button5X = 320;
  color_t Button1_Color = Blue;
  color_t Button2_Color = Blue;
  color_t Button3_Color = Blue;
  color_t Button4_Color = Blue;
  color_t Button5_Color = Blue;
  int margin = 10;

  const font_t *font_ptr = &font_dejavu_sans_24;
  int txtht =  font_ptr->ascent;
  int linepitch;

  int rawX, rawY, rawPressure;

  bool NotExit = true;

  int i;
  int j;
  int CurrentPage;
  int PageCount;
  char PageText[63];
  int SelectedEntry;
  int TopEntry;
  int BottomEntry;
  int ReturnValue;

  linepitch = (14 * txtht) / 10;  // =32

  SelectedEntry = CurrentSelection;
  if ((CurrentSelection == 0) || (CurrentSelection == -1))  // No highlighted entry on initial display
  {
    CurrentPage = 1;
  }
  else
  {
    CurrentPage = (CurrentSelection + 9) / 10;
  }

  linepitch = (14 * txtht) / 10;  // 32
  
  if (ListLength < 1)
  {
    PageCount = 1;
  }
  else if (ListLength > 100)
  {
    return 0;
  }
  else
  {
    PageCount = (ListLength + 9) / 10;
  }

  while (NotExit)  // Repeat from here for each refresh
  {
    setForeColour(255, 255, 255);    // White text
    setBackColour(0, 0, 0);          // on Black

    clearScreen();

    TopEntry = (CurrentPage - 1) * 10 + 1;
    if (TopEntry + 9 > ListLength)
    {
      BottomEntry = ListLength;
    }
    else
    {
      BottomEntry = TopEntry + 9;
    }

    // Display the Title Line
    Text2(wscreen / 40, hscreen - linepitch, ListEntry[0], font_ptr);
    snprintf(PageText, 63, "Page %d of %d", CurrentPage, PageCount);
    Text2((wscreen * 30) / 40, hscreen - linepitch, PageText, font_ptr);

    // Display each row of the list in turn
    for(i = TopEntry; i <= BottomEntry ; i++)
    {
      j = i - TopEntry + 1;
      snprintf(TableNumberText, 4, "%d", i);
      if (SelectedEntry != i)
      {
        // printf("%s %s\n", TableNumberText, ListEntry[i]);
        Text2(wscreen / 40, hscreen - (j + 2) * linepitch, TableNumberText, font_ptr);
        Text2((wscreen * 5) / 40, hscreen - (j + 2) * linepitch, ListEntry[i], font_ptr);
      }
      else
      {
        setForeColour(0, 0, 0);    // Black text
        setBackColour(255, 255, 255);          // on White
        Text2(wscreen / 40, hscreen - (j + 2) * linepitch, TableNumberText, font_ptr);
        Text2((wscreen * 5) / 40, hscreen - (j + 2) * linepitch, ListEntry[i], font_ptr);
        setForeColour(255, 255, 255);    // White text
        setBackColour(0, 0, 0);          // on Black
      }
    }

    // Set Button Colours
    if (CurrentPage > 1)
    {
      Button1_Color = Blue;
    }
    else
    {
      Button1_Color = Grey;
    }
    if (CurrentPage < PageCount)
    {
      Button4_Color = Blue;
    }
    else
    {
      Button4_Color = Grey;
    }

    // Draw the basic button
    rectangle(Button1X, ButtonY, ButtonWidth, ButtonHeight, 
              Button1_Color.r,
              Button1_Color.g,
              Button1_Color.b);

    // Set text and background colours
    setForeColour(255, 255, 255);				   // White text
    setBackColour(Button1_Color.r,
                Button1_Color.g,
                Button1_Color.b);
  
    TextMid2(Button1X + ButtonWidth / 2, ButtonY + ButtonHeight / 2, Button1Caption, &font_dejavu_sans_20);

    if( CurrentSelection != -1)  // Draw Cancel and Select Buttons
    {
      rectangle(Button2X, ButtonY, ButtonWidth, ButtonHeight, 
                Button2_Color.r,
                Button2_Color.g,
                Button2_Color.b);

      // Set text and background colours
      setForeColour(255, 255, 255);				   // White text
      setBackColour(Button2_Color.r,
                    Button2_Color.g,
                    Button2_Color.b);
  
      TextMid2(Button2X + ButtonWidth / 2, ButtonY + ButtonHeight / 2, Button2Caption, &font_dejavu_sans_20);

      rectangle(Button3X, ButtonY, ButtonWidth, ButtonHeight, 
                Button3_Color.r,
                Button3_Color.g,
                Button3_Color.b);

      // Set text and background colours
      setForeColour(255, 255, 255);				   // White text
      setBackColour(Button3_Color.r,
                    Button3_Color.g,
                    Button3_Color.b);
  
      TextMid2(Button3X + ButtonWidth / 2, ButtonY + ButtonHeight / 2, Button3Caption, &font_dejavu_sans_20);
    }
    else  // Draw Exit Button
    {
      rectangle(Button5X, ButtonY, ButtonWidth, ButtonHeight, 
                Button5_Color.r,
                Button5_Color.g,
                Button5_Color.b);

      // Set text and background colours
      setForeColour(255, 255, 255);				   // White text
      setBackColour(Button5_Color.r,
                    Button5_Color.g,
                    Button5_Color.b);
  
      TextMid2(Button5X + ButtonWidth / 2, ButtonY + ButtonHeight / 2, Button5Caption, &font_dejavu_sans_20);
    }

    rectangle(Button4X, ButtonY, ButtonWidth, ButtonHeight, 
              Button4_Color.r,
              Button4_Color.g,
              Button4_Color.b);

    // Set text and background colours
    setForeColour(255, 255, 255);				   // White text
    setBackColour(Button4_Color.r,
                  Button4_Color.g,
                  Button4_Color.b);
  
    TextMid2(Button4X + ButtonWidth / 2, ButtonY + ButtonHeight / 2, Button4Caption, &font_dejavu_sans_20);

    printf("List displayed and waiting for touch\n");
    refreshMouseBackground();
    draw_cursor_foreground(mouse_x, mouse_y);
    UpdateWeb();

    // Wait for key press
    if (getTouchSample(&rawX, &rawY, &rawPressure) == 0) continue;

    TransformTouchMap(rawX, rawY);  // returns scaledX, scaledY with bottom left origin to 800, 480
    //printf("X::: %d Y::: %d\n", scaledX, scaledY);

    // Check if a line has been highlighted
    for (j = 1; j <= 10; j++)
    {
      if ((scaledY <= (418 - 32 * j) + 16)  && (scaledY > (418 - 32 * j) - 16) && (CurrentSelection != -1))  // Line selection
      {
        SelectedEntry = TopEntry + j - 1;
      }
    }

    if ((scaledX <= (Button1X + ButtonWidth - margin)) && (scaledX >= Button1X + margin) &&
        (scaledY <= (ButtonY + ButtonHeight - margin)) && (scaledY >= ButtonY + margin))     // Previous Page
    {
      if (CurrentPage > 1)
      {
        CurrentPage = CurrentPage - 1;
      }
    }

    if ((scaledX <= (Button4X + ButtonWidth - margin)) && (scaledX >= Button4X + margin) &&
        (scaledY <= (ButtonY + ButtonHeight - margin)) && (scaledY >= ButtonY + margin))    // Next page
    {
      if (CurrentPage < PageCount)
      {
        CurrentPage = CurrentPage + 1;
      }
    }
    if (CurrentSelection != -1)  // Display list and select value
    {
      if ((scaledX <= (Button2X + ButtonWidth - margin)) && (scaledX >= Button2X + margin) &&
          (scaledY <= (ButtonY + ButtonHeight - margin)) && (scaledY >= ButtonY + margin))    // Cancel
      {
        ReturnValue = CurrentSelection;
        NotExit = false;
      }

      if ((scaledX <= (Button3X + ButtonWidth - margin)) && (scaledX >= Button3X + margin) &&
          (scaledY <= (ButtonY + ButtonHeight - margin)) && (scaledY >= ButtonY + margin))    // Select
      {
        ReturnValue = SelectedEntry;
        NotExit = false;
      }
    }
    else  // Display list only, no selection
    {
      if ((scaledX <= (Button5X + ButtonWidth - margin)) && (scaledX >= Button5X + margin) &&
          (scaledY <= (ButtonY + ButtonHeight - margin)) && (scaledY >= ButtonY + margin))
      {
        ReturnValue = -1;
        NotExit = false;
      }
    }
  }
  printf("Returning Value %d from Select From List\n", ReturnValue);
  return ReturnValue;
}


int CheckWifiEnabled()
{
  // Returns 0 if WiFi enabled, 1 if not

  FILE *fp;
  char response[255];
  int responseint;

  // Open the command for reading
  fp = popen("ifconfig | grep -q 'wlan0' ; echo $?", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response, 7, fp) != NULL)
  {
    responseint = atoi(response);
    // printf("WiFi response %s, %d\n", response, responseint);
  }

  pclose(fp);
  return responseint;
}


int CheckWifiConnection(char Network_SSID[63])
{
  // Returns 0 if WiFi connected with SSID, 1 if not

  FILE *fp;
  char response[255];
  int responseint;

  // Open the command for reading
  fp = popen("iwconfig 2>/dev/null | grep 'ESSID' ", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response, 63, fp) != NULL)
  {
    response[strlen(response) - 2] = '\0';  // Remove trailing CR

    //printf("Response = -%s-\n", response + 29);

    if (strncmp(response + 29, "off/any", 7) == 0)
    {
      responseint = 1;
      strcpy(Network_SSID, "Not connected");
    }
    else
    {
      responseint = 0;
      strcpy(Network_SSID, response + 29);
    }
  }
  pclose(fp);
  return responseint;
}


void WiFiConfig(int NoButton)
{
  char ListEntry[101][63];
  int CurrentSelection = 0;
  int ListLength = 0;
  FILE *fp;
  char response_line[255];
  int j;
  int NewSelection = 0;
  char PassPhrase[63];
  char Prompt[127];
  char SystemCommand[255];
  char Network_SSID[63];
  char wlan0IPAddress[255];

  strcpy(ListEntry[0], "Empty List Title");

  switch (NoButton)
  {
    case 5:                               //
      strcpy(ListEntry[0], "WiFi Networks Detected:");
      CurrentSelection = -1;
      j = 0;

      fp = popen("sudo iwlist wlan0 scan | grep 'ESSID'", "r");
      if (fp == NULL)
      {
        printf("Failed to run command\n" );
        exit(1);
      }

      // Read the output a line at a time - output it
      while (fgets(response_line, 250, fp) != NULL)
      {
        if ((strlen(response_line) > 1) && (j >= 0) && (j < 100))
        {
          j = j + 1;
          if (strlen(response_line) > 90)
          {
            response_line[89] = '\0';  // Limit line length to 62 + 27
          }

          response_line[strlen(response_line) - 2] = '\0';  // Remove trailing " and CR

          if (strlen(response_line) == 27) 
          {
            strcpy(ListEntry[j], "Hidden Network");
          }
          else if (strncmp(response_line + 28, "x00", 3) == 0)  // Check for null byte associated with hidden networks
          {
            strcpy(ListEntry[j], "Hidden Network");
          }
          else
          { 
            strcpy(ListEntry[j], response_line + 27);
          }
          //printf("%s\n", ListEntry[j]);
        }
      }
      pclose(fp);
      ListLength = j;
      if (j == 0)
      {
        strcpy(ListEntry[0], "No WiFi Networks Detected");
      }
      SelectFromList(CurrentSelection, ListEntry, ListLength);
      break;

    case 6:                               //
      strcpy(ListEntry[0], "Select one of these WiFi Networks:");
      CurrentSelection = 0;
      j = 0;

      fp = popen("sudo iwlist wlan0 scan | grep 'ESSID'", "r");
      if (fp == NULL)
      {
        printf("Failed to run command\n" );
        exit(1);
      }

      // Read the output a line at a time - output it
      while (fgets(response_line, 250, fp) != NULL)
      {
        if ((strlen(response_line) > 1) && (j >= 0) && (j < 100))
        {
          j = j + 1;
          if (strlen(response_line) > 90)
          {
            response_line[89] = '\0';  // Limit line length to 62 + 27
          }

          response_line[strlen(response_line) - 2] = '\0';  // Remove trailing " and CR

          if (strlen(response_line) == 27) 
          {
            strcpy(ListEntry[j], "Hidden Network");
          }
          else if (strncmp(response_line + 28, "x00", 3) == 0)  // Check for null byte associated with hidden networks
          {
            strcpy(ListEntry[j], "Hidden Network");
          }
          else
          { 
            strcpy(ListEntry[j], response_line + 27);
          }
          //printf("%s\n", ListEntry[j]);
        }
      }
      pclose(fp);
      ListLength = j;
      if (j == 0)
      {
        strcpy(ListEntry[0], "No WiFi Networks Detected");
      }
      NewSelection = SelectFromList(CurrentSelection, ListEntry, ListLength);

      if (NewSelection == 0)
      {
        return;
      }

      if (strcmp(ListEntry[NewSelection], "Hidden Network") == 0)
      {
        return;  // Maybe react better to this later
      }

      snprintf(Prompt, 94, "Enter the PassPhrase for SSID %s", ListEntry[NewSelection]);
      KeyboardReturn[0] = '\0';
      while (strlen(KeyboardReturn) < 1)
      {
        Keyboard(Prompt, "", 23);
      }
      strcpy(PassPhrase, KeyboardReturn);

      snprintf(SystemCommand, 254, "~/rpidatv/scripts/AddWifiNetwork.sh %s %s", ListEntry[NewSelection], PassPhrase);
      system(SystemCommand);
      // printf("SystemCommand %s\n", SystemCommand);

      break;

    case 7:                               // Check Wifi Connection
      if (CheckWifiEnabled() == 1)
      {
        MsgBox4("Wifi Not Enabled", "", "", "Touch Screen to Continue");
        wait_touch();
      }
      else
      {
        if (CheckWifiConnection(Network_SSID) == 1)
        {
          MsgBox4("Wifi Enabled", "but not connected", "", "Touch Screen to Continue");
          wait_touch();
        }
        else
        {
          Get_wlan0_IPAddr(wlan0IPAddress);
          MsgBox4("Wifi Enabled and connected to", Network_SSID, wlan0IPAddress, "Touch Screen to Continue");
          wait_touch();
        }
      }
      break;

    case 8:                                                                                 // Enable Wifi
      system("sudo ifconfig wlan0 down >/dev/null 2>/dev/null");                            // First, Disable it
      system("sudo ifconfig wlan0 up >/dev/null 2>/dev/null");                              // Enable it
      system("rm ~/.wifi_off >/dev/null 2>/dev/null");                                      // Enable at start-up
      system("wpa_cli -i wlan0 reconfigure >/dev/null 2>/dev/null");                        // Make it Connect
      usleep (1000000);                                                                     // Pause
      system("sudo rfkill unblock 0 >/dev/null 2>/dev/null");                               // Unblock the RF
      break;

    case 3:                                                                                     // Disable Wifi
      system("sudo ifconfig wlan0 down >/dev/null 2>/dev/null");                                // Disable it now
      system("cp ~/rpidatv/scripts/configs/text.wifi_off ~/.wifi_off >/dev/null 2>/dev/null");  // Disable at start-up
      break;

    case 9:                                                                                 // Reset Wifi

      system("sudo ifconfig wlan0 down >/dev/null 2>/dev/null");                            // First, Disable it
      system("sudo raspi-config nonint do_wifi_country GB>/dev/null 2>/dev/null");          // Make sure country is set

      system("sudo rm /etc/wpa_supplicant/wpa_supplicant.conf >/dev/null 2>/dev/null");     // Delete Config

      system("sudo cp ~/rpidatv/scripts/configs/wpa_supplicant.conf /etc/wpa_supplicant/wpa_supplicant.conf >/dev/null 2>/dev/null");
                                                                                            // Copy in new config

      system("sudo chown root /etc/wpa_supplicant/wpa_supplicant.conf >/dev/null 2>/dev/null");
                                                                                            // and set ownership

      system("sudo ifconfig wlan0 up >/dev/null 2>/dev/null");                              // Enable it
      system("rm ~/.wifi_off >/dev/null 2>/dev/null");                                      // Enable at start-up
      system("wpa_cli -i wlan0 reconfigure >/dev/null 2>/dev/null");                        // Load the config
      usleep (1000000);                                                                     // Pause
      system("sudo rfkill unblock 0 >/dev/null 2>/dev/null");                               // Unblock the RF
      break;

    case 1:                               //
  CurrentSelection = 11;
  ListLength = 12;

  strcpy(ListEntry[0], "List Title");
  strcpy(ListEntry[1], "List Entry 1");
  strcpy(ListEntry[2], "List Entry 2");
  strcpy(ListEntry[3], "List Entry 3");
  strcpy(ListEntry[4], "List Entry 4");
  strcpy(ListEntry[5], "List Entry 5");
  strcpy(ListEntry[6], "List Entry 6");
  strcpy(ListEntry[7], "List Entry 7");
  strcpy(ListEntry[8], "List Entry 8");
  strcpy(ListEntry[9], "List Entry 9");
  strcpy(ListEntry[10], "List Entry 10");
  strcpy(ListEntry[11], "List Entry 11");
  strcpy(ListEntry[12], "List Entry 12");

   
      SelectFromList(CurrentSelection, ListEntry, ListLength);
      break;
  }
}


void GreyOut1()
{
  // Called at the top of any StartHighlight1 to grey out inappropriate selections
  if (CurrentMenu == 1)
  {
    if (strcmp(CurrentTXMode, "Carrier") == 0)
    {
      SetButtonStatus(ButtonNumber(CurrentMenu, 16), 2); // Encoder
      SetButtonStatus(ButtonNumber(CurrentMenu, 18), 2); // Format
      SetButtonStatus(ButtonNumber(CurrentMenu, 19), 2); // Source
      SetButtonStatus(ButtonNumber(CurrentMenu, 11), 2); // SR
      SetButtonStatus(ButtonNumber(CurrentMenu, 12), 2); // FEC
      SetButtonStatus(ButtonNumber(CurrentMenu, 7), 2);  // Audio
      SetButtonStatus(ButtonNumber(CurrentMenu, 6), 2);  // Caption
      SetButtonStatus(ButtonNumber(CurrentMenu, 5), 2);  // EasyCap
    }
    else
    {
      SetButtonStatus(ButtonNumber(CurrentMenu, 16), 0); // Encoder
      SetButtonStatus(ButtonNumber(CurrentMenu, 11), 0); // SR
      SetButtonStatus(ButtonNumber(CurrentMenu, 12), 0); // FEC
      SetButtonStatus(ButtonNumber(CurrentMenu, 7), 0); // Audio
      SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0); // Caption
      SetButtonStatus(ButtonNumber(CurrentMenu, 5), 0); // EasyCap

      if ((strcmp(CurrentEncoding, "IPTS in") == 0) || (strcmp(CurrentEncoding, "TS File") == 0))
      {
        SetButtonStatus(ButtonNumber(CurrentMenu, 18), 2); // Format
        SetButtonStatus(ButtonNumber(CurrentMenu, 19), 2); // Source
        SetButtonStatus(ButtonNumber(CurrentMenu, 7), 2);  // Audio
        SetButtonStatus(ButtonNumber(CurrentMenu, 6), 2); // Caption
        SetButtonStatus(ButtonNumber(CurrentMenu, 5), 2); // EasyCap
      }
      else
      {
        SetButtonStatus(ButtonNumber(CurrentMenu, 18), 0); // Format
        SetButtonStatus(ButtonNumber(CurrentMenu, 19), 0); // Source
        SetButtonStatus(ButtonNumber(CurrentMenu, 7), 0); // Audio
        SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0); // Caption
        SetButtonStatus(ButtonNumber(CurrentMenu, 5), 0); // EasyCap
      }
      if ((strcmp(CurrentEncoding, "MPEG-2") == 0) || (strcmp(CurrentEncoding, "H264") == 0))
      {
        SetButtonStatus(ButtonNumber(CurrentMenu, 7), 0); // Blue/Green Audio
      }
      else if (strcmp(CurrentSource, "C920") == 0)
      {
        SetButtonStatus(ButtonNumber(CurrentMenu, 7), 0); // Blue/Green Audio
      }
      else  // IPTS in or TS File
      {
        SetButtonStatus(ButtonNumber(CurrentMenu, 7), 2); // Grey Audio
      }
      // Grey out Caption Button if not MPEG-2 or Streamer or test card
      if ((strcmp(CurrentEncoding, "MPEG-2") != 0) 
        && (strcmp(CurrentModeOP, "STREAMER") != 0)
        && (strcmp(CurrentSource, "TestCard") != 0))
      {
        SetButtonStatus(ButtonNumber(CurrentMenu, 6), 2); // Caption
      }
    }
    if ((strcmp(CurrentModeOP, "STREAMER") == 0)\
      || (strcmp(CurrentModeOP, "COMPVID") == 0) || (strcmp(CurrentModeOP, "DTX1") == 0)\
      || (strcmp(CurrentModeOP, "IP") == 0))
    {
      if (strcmp(CurrentModeOP, "STREAMER") != 0)
      {
        SetButtonStatus(ButtonNumber(CurrentMenu, 10), 2); // Don't grey out freq button for stream
      }
      else
      {
        SetButtonStatus(ButtonNumber(CurrentMenu, 10), 0);
      }
      SetButtonStatus(ButtonNumber(CurrentMenu, 13), 2); // Band
      SetButtonStatus(ButtonNumber(CurrentMenu, 14), 2); // Device Level
      SetButtonStatus(ButtonNumber(CurrentMenu, 8), 2);  // Attenuator Type
      SetButtonStatus(ButtonNumber(CurrentMenu, 9), 2);  // Attenuator Level
    }
    else
    {
      SetButtonStatus(ButtonNumber(CurrentMenu, 10), 0); // Frequency
      SetButtonStatus(ButtonNumber(CurrentMenu, 13), 0); // Band
      SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0);  // Attenuator Type
      SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0); // Attenuator Level
      // If no attenuator then Grey out Atten Level
      if (strcmp(CurrentAtten, "NONE") == 0)
      {
        SetButtonStatus(ButtonNumber(CurrentMenu, 9), 2); // Attenuator Level
      }

      // If not DATV Express, Lime, JLIME or Pluto then Grey out Device Level
      if ((strcmp(CurrentModeOP, "DATVEXPRESS") != 0) 
        && (strcmp(CurrentModeOP, "PLUTO") != 0) 
        && (strcmp(CurrentModeOP, TabModeOP[3]) != 0) 
        && (strcmp(CurrentModeOP, TabModeOP[8]) != 0) 
        && (strcmp(CurrentModeOP, TabModeOP[9]) != 0)
        && (strcmp(CurrentModeOP, TabModeOP[11]) != 0)
        && (strcmp(CurrentModeOP, TabModeOP[12]) != 0))
      {
        SetButtonStatus(ButtonNumber(CurrentMenu, 14), 2); // Device Level Grey
      }
      else
      {
        SetButtonStatus(ButtonNumber(CurrentMenu, 14), 0); // Device Level Blue
      }
    }
  }
}

void GreyOutReset11()
{
  SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0); // S2 QPSK
  SetButtonStatus(ButtonNumber(CurrentMenu, 1), 0); // 8PSK
  SetButtonStatus(ButtonNumber(CurrentMenu, 2), 0); // 16 APSK
  SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0); // 32APSK
  SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0); // carrier
  SetButtonStatus(ButtonNumber(CurrentMenu, 7), 0); // Show DVB-T
}

void GreyOut11()
{
  if ((strcmp(CurrentModeOP, "LIMEUSB") != 0)
   && (strcmp(CurrentModeOP, "LIMEMINI") != 0)
   && (strcmp(CurrentModeOP, "LIMEDVB") != 0)
   && (strcmp(CurrentModeOP, "MUNTJAC") != 0)
   && (strcmp(CurrentModeOP, "PLUTO") != 0)
   && (strcmp(CurrentModeOP, "STREAMER") != 0)
   && (strcmp(CurrentModeOP, "COMPVID") != 0)
   && (strcmp(CurrentModeOP, "IP") != 0)
   && (strcmp(CurrentModeOP, "JLIME") != 0)
   && (strcmp(CurrentModeOP, "JEXPRESS") != 0)) // so selection is not DVB-S2-capable
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 0), 2); // grey-out S2 QPSK
    SetButtonStatus(ButtonNumber(CurrentMenu, 1), 2); // grey-out 8PSK
    SetButtonStatus(ButtonNumber(CurrentMenu, 2), 2); // grey-out 16 APSK
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 2); // grey-out 32APSK
    SetButtonStatus(ButtonNumber(CurrentMenu, 8), 2); // grey-out Pilots on/off
    SetButtonStatus(ButtonNumber(CurrentMenu, 9), 2); // grey-out Frames long/short
  }
  else
  {
    if(strcmp(CurrentTXMode, "DVB-S") == 0) // DVB-S selected
    {
      SetButtonStatus(ButtonNumber(CurrentMenu, 8), 2); // grey-out Pilots on/off
      SetButtonStatus(ButtonNumber(CurrentMenu, 9), 2); // grey-out Frames long/short
    }
    else
    {
      SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0); // Show Pilots on/off
      SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0); // Show Frames long/short
    }

    // Until short/long frames working, grey out frames
    SetButtonStatus(ButtonNumber(CurrentMenu, 9), 2); // grey-out Frames long/short
  }
  // For Pluto
  if (strcmp(CurrentModeOP, "PLUTO") == 0)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 2); // grey-out 32APSK
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 2); // grey-out carrier
    SetButtonStatus(ButtonNumber(CurrentMenu, 8), 2); // grey-out Pilots on/off
    SetButtonStatus(ButtonNumber(CurrentMenu, 9), 2); // grey-out Frames long/short
    SetButtonStatus(ButtonNumber(CurrentMenu, 7), 0); // Show DVB-T
  }

  // For Muntjac
  if (strcmp(CurrentModeOP, "MUNTJAC") == 0)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 2), 2); // grey-out 16APSK
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 2); // grey-out 32APSK
    SetButtonStatus(ButtonNumber(CurrentMenu, 5), 2); // grey-out DVB-S
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 2); // grey-out Carrier
    SetButtonStatus(ButtonNumber(CurrentMenu, 7), 2); // grey-out DVB-T
    SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0); // Show Pilots on/off
    SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0); // Show Frames long/short
  }

  if (strcmp(CurrentModeOP, "EXPRESS") == 0
   || (strcmp(CurrentModeOP, "LIMEDVB") == 0)
   || (strcmp(CurrentModeOP, "JLIME") == 0)
   || (strcmp(CurrentModeOP, "JEXPRESS") == 0)) // so selection is not DVB-T-capable
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 7), 2); // Grey-out DVB-T
  }
}

void GreyOut12()
{
  if(strcmp(CurrentModeOP, "JLIME") == 0) // Jetson Selected
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 7), 0); // Show H265 button
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 7), 2); // Grey-out H265 Button
  }
  if(strcmp(CurrentModeOP, "PLUTO") == 0) // Pluto Selected
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 5), 2); // Grey-out MPEG-2 button
    SetButtonStatus(ButtonNumber(CurrentMenu, 2), 2); // Grey-out Raw IPTS H265 button button
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 5), 0); // Show MPEG-2 Button
  }
}

void GreyOut15()
{
}

void GreyOutReset25()
{
  SetButtonStatus(ButtonNumber(CurrentMenu, 5), 0); // FEC 1/4
  SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0); // FEC 1/3
  SetButtonStatus(ButtonNumber(CurrentMenu, 7), 0); // FEC 1/2
  SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0); // FEC 3/5
  SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0); // FEC 2/3
}

void GreyOut25()
{
  if (strcmp(CurrentTXMode, TabTXMode[3]) == 0)  // 8PSK
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 5), 2); // FEC 1/4
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 2); // FEC 1/3
    SetButtonStatus(ButtonNumber(CurrentMenu, 7), 2); // FEC 1/2
  }
  if (strcmp(CurrentTXMode, TabTXMode[4]) == 0)  // 16APSK
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 5), 2); // FEC 1/4
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 2); // FEC 1/3
    SetButtonStatus(ButtonNumber(CurrentMenu, 7), 2); // FEC 1/2
    SetButtonStatus(ButtonNumber(CurrentMenu, 8), 2); // FEC 3/5
  }
  if (strcmp(CurrentTXMode, TabTXMode[5]) == 0)  // 32APSK
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 5), 2); // FEC 1/4
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 2); // FEC 1/3
    SetButtonStatus(ButtonNumber(CurrentMenu, 7), 2); // FEC 1/2
    SetButtonStatus(ButtonNumber(CurrentMenu, 8), 2); // FEC 3/5
    SetButtonStatus(ButtonNumber(CurrentMenu, 9), 2); // FEC 2/3
  }
}


void GreyOutReset42()
{
  SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0); // Lime Mini
  SetButtonStatus(ButtonNumber(CurrentMenu, 7), 0); // DATV Express
  SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0); // Lime USB
  SetButtonStatus(ButtonNumber(CurrentMenu, 10), 0); // Jetson
  SetButtonStatus(ButtonNumber(CurrentMenu, 13), 0); // LimeMini DVB
  SetButtonStatus(ButtonNumber(CurrentMenu, 14), 0); // Pluto
}

void GreyOut42()
{
  if (CheckExpressConnect() == 1)   // DATV Express not connected so GreyOut
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 7), 2); // DATV Express
  }
  if (CheckLimeMiniConnect() == 1)  // Lime Mini not connected so GreyOut
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 2); // Lime Mini
    SetButtonStatus(ButtonNumber(CurrentMenu, 13), 2); // Lime Mini DVB
  }
  if (CheckLimeUSBConnect() == 1)  // Lime USB not connected so GreyOut
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 8), 2); // Lime USB
  }
  if (CheckMuntjac() == 1)  // Muntjac not connected so GreyOut
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 12), 2); // Muntjac
  }
  if (CheckJetson() == 1)  // Jetson not connected so GreyOut
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 10), 2); // Jetson Lime
    SetButtonStatus(ButtonNumber(CurrentMenu, 11), 2); // Jetson Stream
  }
  // Check Pluto
  if ((CheckPlutoIPConnect() == 1) || (CheckPlutoUSBConnect() != 0))   // Pluto not connected, so GreyOut
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 14), 2); // Pluto
  }
}

void GreyOutReset44()
{
  SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0); // Shutdown Jetson
  SetButtonStatus(ButtonNumber(CurrentMenu, 1), 0); // Reboot Jetson
}

void GreyOut44()
{
  if (CheckJetson() == 1)  // Jetson not connected so GreyOut
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 0), 2); // Shutdown Jetson
    SetButtonStatus(ButtonNumber(CurrentMenu, 1), 2); // Reboot Jetson
  }
}

void GreyOut45()
{
  if ((strcmp(CurrentModeOP, "JLIME") == 0) || (strcmp(CurrentModeOP, "JEXPRESS") == 0)) // Jetson
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 0), 2); // Contest
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 2); // Comp Vid
    SetButtonStatus(ButtonNumber(CurrentMenu, 9), 2); // PiScreen
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0); // HDMI
  }
  else if (strcmp(CurrentModeOP, "STREAMER") == 0) // Streamer
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 0), 2); // Contest
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0); // Comp Vid
    SetButtonStatus(ButtonNumber(CurrentMenu, 9), 2); // PiScreen
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0); // HDMI
  }
  else if (strcmp(CurrentModeOP, "PLUTO") == 0) // Pluto
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0); // Contest
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0); // Comp Vid
    SetButtonStatus(ButtonNumber(CurrentMenu, 9), 2); // PiScreen
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0); // HDMI
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0); // HDMI
    SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0); // Contest
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0); // Comp Vid
    SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0); // PiScreen

    if (strcmp(CurrentFormat, "1080p") == 0)
    {
      SetButtonStatus(ButtonNumber(CurrentMenu, 5), 2); // Pi Cam
      SetButtonStatus(ButtonNumber(CurrentMenu, 6), 2); // CompVid
      SetButtonStatus(ButtonNumber(CurrentMenu, 8), 2); // TestCard
      SetButtonStatus(ButtonNumber(CurrentMenu, 9), 2); // PiScreen
      SetButtonStatus(ButtonNumber(CurrentMenu, 0), 2); // Contest
      SetButtonStatus(ButtonNumber(CurrentMenu, 1), 2); // Webcam
    }
    else
    {
      SetButtonStatus(ButtonNumber(CurrentMenu, 5), 0); // Pi Cam
      SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0); // CompVid
      SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0); // TestCard
      SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0); // PiScreen
      SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0); // Contest
      SetButtonStatus(ButtonNumber(CurrentMenu, 1), 0); // Webcam

      if (strcmp(CurrentEncoding, "H264") == 0)
      {
        SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0); // PiScreen
        SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0); // CompVid
      }
      else //MPEG-2
      {
        SetButtonStatus(ButtonNumber(CurrentMenu, 9), 2); // PiScreen
        SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0); // CompVid

        if (strcmp(CurrentFormat, "720p") == 0)
        {
          SetButtonStatus(ButtonNumber(CurrentMenu, 6), 2); // CompVid
        }
      }
    }
    // Highlight HDMI if Lime connected and Elgato Available
    if (((strcmp(CurrentModeOP, "LIMEMINI") == 0) || (strcmp(CurrentModeOP, "LIMEUSB") == 0)) && (CheckCamLink4K() == 1))
    {
      SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0); // HDMI
    }
  }
}


void SelectInGroup(int StartButton,int StopButton,int NoButton,int Status)
{
  int i;
  for(i = StartButton ; i <= StopButton ; i++)
  {
    if(i == NoButton)
      SetButtonStatus(i, Status);
    else
      SetButtonStatus(i, 0);
  }
}

void SelectInGroupOnMenu(int Menu, int StartButton, int StopButton, int NumberButton, int Status)
{
  int i;
  int StartButtonAll;
  int StopButtonAll;
  int NumberButtonAll;

  StartButtonAll = ButtonNumber(Menu, StartButton); 
  StopButtonAll = ButtonNumber(Menu, StopButton); 
  NumberButtonAll = ButtonNumber(Menu, NumberButton); 
  for(i = StartButtonAll ; i <= StopButtonAll ; i++)
  {
    if(i == NumberButtonAll)
    {
      SetButtonStatus(i, Status);
    }
    else
    {
      SetButtonStatus(i, 0);
    }
  }
}

void SelectTX(int NoButton)  // TX RF Output Mode
{
  SelectInGroupOnMenu(CurrentMenu, 5, 7, NoButton, 1);
  SelectInGroupOnMenu(CurrentMenu, 0, 3, NoButton, 1);
  if (NoButton == 7) // DVB-T
  {
    NoButton = 6;
  }
  else
  {
    if (NoButton > 3)  // Correct numbering
    {
      NoButton = NoButton - 5;
    }
    else
    {
      NoButton = NoButton + 2;
    }
  }
  strcpy(CurrentTXMode, TabTXMode[NoButton]);
  char Param[15]="modulation";
  SetConfigParam(PATH_PCONFIG, Param, CurrentTXMode);
  EnforceValidTXMode();
  EnforceValidFEC();
  ApplyTXConfig();
}

void SelectPilots()  // Toggle pilots on/off
{
  if (strcmp(CurrentPilots, "off") == 0)  // Currently off
  {
    strcpy(CurrentPilots, "on");
    SetConfigParam(PATH_PCONFIG, "pilots", "on");
  }
  else                                   // Currently on
  {
    strcpy(CurrentPilots, "off");
    SetConfigParam(PATH_PCONFIG, "pilots", "off");
  }
}

void SelectFrames()  // Toggle frames long/short
{
  if (strcmp(CurrentFrames, "long") == 0)  // Currently long
  {
    strcpy(CurrentFrames, "short");
    SetConfigParam(PATH_PCONFIG, "frames", "short");
  }
  else                                   // Currently short
  {
    strcpy(CurrentFrames, "long");
    SetConfigParam(PATH_PCONFIG, "frames", "long");
  }
}

void SelectEncoding(int NoButton)  // Encoding
{
  SelectInGroupOnMenu(CurrentMenu, 5, 9, NoButton, 1);
  SelectInGroupOnMenu(CurrentMenu, 1, 2, NoButton, 1);

  // Encoding is stored as modeinput, so deal with IPTS in and TS File first

  if (NoButton == 8)       // IPTS in 
  {
    SetConfigParam(PATH_PCONFIG, "modeinput", "IPTSIN");
    strcpy(CurrentEncoding, "IPTS in");
  }
  else if (NoButton == 1)       // IPTS in H264
  {
    SetConfigParam(PATH_PCONFIG, "modeinput", "IPTSIN264");
    strcpy(CurrentEncoding, "IPTS in H264");
  }
  else if (NoButton == 2)       // IPTS in H265
  {
    SetConfigParam(PATH_PCONFIG, "modeinput", "IPTSIN265");
    strcpy(CurrentEncoding, "IPTS in H265");
  }
  else if (NoButton == 9) // TS File  
  {
    SetConfigParam(PATH_PCONFIG, "modeinput", "FILETS");
    strcpy(CurrentEncoding, "TS File");
    IPTSConfig(3);       // and confirm source file
  }
  else                   // MPEG-2, H264 or H265
  {
    strcpy(CurrentEncoding, TabEncoding[NoButton - 5]);
    char Param[15]="encoding";
    SetConfigParam(PATH_PCONFIG, Param, CurrentEncoding);
  }
  ApplyTXConfig();
}

void SelectOP(int NoButton)      // Output device
{
  int index;
  // Stop or reset DATV Express Server if required
  if (strcmp(CurrentModeOP, TabModeOP[2]) == 0)  // mode was DATV Express
  {
    system("sudo killall express_server >/dev/null 2>/dev/null");
    system("sudo rm /tmp/expctrl >/dev/null 2>/dev/null");
  }

  SelectInGroupOnMenu(CurrentMenu, 5, 13, NoButton, 1);
  SelectInGroupOnMenu(CurrentMenu, 0, 3, NoButton, 1);
  if (NoButton > 9 ) // 3rd row up
  {
    index = NoButton - 1;
  }
  if ((NoButton > 4 ) && (NoButton < 10 )) // 2nd row up
  {
    index = NoButton - 5;
  }
  if (NoButton < 4) // Bottom row
  {
    index = NoButton + 5;
  }
  strcpy(ModeOP, TabModeOP[index]);
  char Param[15]="modeoutput";
  SetConfigParam(PATH_PCONFIG, Param, ModeOP);

  // Set the Current Mode Output variable
  strcpy(CurrentModeOP, TabModeOP[index]);
  strcpy(CurrentModeOPtext, TabModeOPtext[index]);

  // Start DATV Express if required
  if (strcmp(CurrentModeOP, TabModeOP[2]) == 0)  // new mode is DATV Express
  {
    StartExpressServer();
  }
  EnforceValidTXMode();
  EnforceValidFEC();

  // Check Lime Connected if selected
  CheckLimeReady();
  // and check Pluto connected if selected
  CheckPlutoReady();
}

void SelectFormat(int NoButton)  // Video Format
{
  SelectInGroupOnMenu(CurrentMenu, 5, 8, NoButton, 1);
  strcpy(CurrentFormat, TabFormat[NoButton - 5]);
  SetConfigParam(PATH_PCONFIG, "format", CurrentFormat);
  ApplyTXConfig();
}

void SelectSource(int NoButton)  // Video Source
{
  SelectInGroupOnMenu(CurrentMenu, 5, 9, NoButton, 1);
  SelectInGroupOnMenu(CurrentMenu, 0, 3, NoButton, 1);
  if (NoButton < 4) // allow for reverse numbering of rows
  {
    NoButton = NoButton + 10;
  }
  strcpy(CurrentSource, TabSource[NoButton - 5]);
  printf("Current Source before ApplyTXConfig in SelectSource is %s\n",  CurrentSource);

  // Bodge for HDMI input from Elgato
  if (NoButton == 13)  // HDMI Selected
  {
    SetConfigParam(PATH_PCONFIG, "modeinput", "HDMI");
  } 
  ApplyTXConfig();
  printf("Current Source afer ApplyTXConfig in SelectSource is %s\n",  CurrentSource);
}

void SelectFreq(int NoButton)  //Frequency
{
  char freqtxt[255];

  SelectInGroupOnMenu(CurrentMenu, 5, 19, NoButton, 1);
  SelectInGroupOnMenu(CurrentMenu, 0, 3, NoButton, 1);
  if (NoButton < 4)
  {
    NoButton = NoButton + 5;
  }
  else if ((NoButton > 4) && (NoButton < 10))
  {
    NoButton = NoButton - 5;
  }

  printf("NoButton = %d\n", NoButton);

  if (NoButton < 10) // Normal presets
  {
    strcpy(freqtxt, TabFreq[NoButton]);
  }
  else              // QO-100 freqs
  {
    strcpy(freqtxt, QOFreq[NoButton - 10]);
  }
  printf("CallingMenu = %d\n", CallingMenu);

  printf ("freqtxt = %s\n", freqtxt);

  if (CallingMenu == 1)  // Transmit Frequency
  {
    char Param[] = "freqoutput";
    SetConfigParam(PATH_PCONFIG, Param, freqtxt);

    DoFreqChange();
  }
  else                    // Lean DVB Receive frequency
  {
    strcpy(RXfreq[0], freqtxt);              
    SetConfigParam(PATH_RXPRESETS, "rx0frequency", RXfreq[0]);
  }
}

void SelectSR(int NoButton)  // Symbol Rate
{
  char Value[255];

  SelectInGroupOnMenu(CurrentMenu, 5, 9, NoButton, 1);
  SelectInGroupOnMenu(CurrentMenu, 0, 3, NoButton, 1);
  if (NoButton < 4)
  {
    NoButton = NoButton + 10;
  }

  if (CallingMenu == 1)  // Transmit SR
  {
    SR = TabSR[NoButton - 5];
    sprintf(Value, "%d", SR);
    SetConfigParam(PATH_PCONFIG, "symbolrate", Value);
  }
  else                    // Lean DVB Receive SR
  {
    RXsr[0] = TabSR[NoButton - 5];
    sprintf(Value, "%d", RXsr[0]);
    SetConfigParam(PATH_RXPRESETS, "rx0sr", Value);
  }
}

void SelectFec(int NoButton)  // FEC
{
  char Param[7]="fec";
  char Value[255];
  SelectInGroupOnMenu(CurrentMenu, 5, 9, NoButton, 1);
  if (CallingMenu == 1)  // Transmit FEC
  {
    fec = TabFec[NoButton - 5];
    sprintf(Value, "%d", fec);
    SetConfigParam(PATH_PCONFIG, Param, Value);
  }
  else                    // Lean DVB Receive SR
  {
    sprintf(Value, "%d", TabFec[NoButton - 5]);
    strcpy(RXfec[0], Value);
    SetConfigParam(PATH_RXPRESETS, "rx0fec", Value);
  }
}

void SelectLMSR(int NoButton)  // LongMynd Symbol Rate
{
  char Value[255];

  NoButton = NoButton - 14;  // Translate to 1 - 6

  if (strcmp(LMRXmode, "terr") == 0)
  {
    NoButton = NoButton + 6;
    LMRXsr[0] = LMRXsr[NoButton];
    snprintf(Value, 15, "%d", LMRXsr[0]);
    SetConfigParam(PATH_LMCONFIG, "sr1", Value);
  }
  else // Sat
  {
    LMRXsr[0] = LMRXsr[NoButton];
    snprintf(Value, 15, "%d", LMRXsr[0]);
    SetConfigParam(PATH_LMCONFIG, "sr0", Value);
  }
}

void SelectLMFREQ(int NoButton)  // LongMynd Frequency
{
  char Value[255];

  NoButton = NoButton - 9; // top row 1 - 5 bottom -4 - 0

  if (NoButton < 1)
  {
    NoButton = NoButton + 10;
  }
  // Buttons 1 - 10

  if (strcmp(LMRXmode, "terr") == 0)
  {
    NoButton = NoButton + 10;
    LMRXfreq[0] = LMRXfreq[NoButton];
    snprintf(Value, 25, "%d", LMRXfreq[0]);
    SetConfigParam(PATH_LMCONFIG, "freq1", Value);
  }
  else // Sat
  {
    LMRXfreq[0] = LMRXfreq[NoButton];
    snprintf(Value, 25, "%d", LMRXfreq[0]);
    SetConfigParam(PATH_LMCONFIG, "freq0", Value);
  }
}

void ResetLMParams()  // Called after switch between Terrestrial and Sat
{
  char Value[255];

  if (strcmp(LMRXmode, "terr") == 0)
  {
    GetConfigParam(PATH_LMCONFIG, "freq1", Value);
    LMRXfreq[0] = atoi(Value);
    GetConfigParam(PATH_LMCONFIG, "sr1", Value);
    LMRXsr[0] = atoi(Value);
    GetConfigParam(PATH_LMCONFIG, "input1", Value);
    strcpy(LMRXinput, Value);
  }
  else // Sat
  {
    GetConfigParam(PATH_LMCONFIG, "freq0", Value);
    LMRXfreq[0] = atoi(Value);
    GetConfigParam(PATH_LMCONFIG, "sr0", Value);
    LMRXsr[0] = atoi(Value);
    GetConfigParam(PATH_LMCONFIG, "input", Value);
    strcpy(LMRXinput, Value);
  }
}


void SelectS2Fec(int NoButton)  // DVB-S2 FEC
{
  SelectInGroupOnMenu(CurrentMenu, 5, 9, NoButton, 1);
  SelectInGroupOnMenu(CurrentMenu, 0, 3, NoButton, 1);
  if (NoButton > 3)  // Correct numbering
  {
    NoButton = NoButton - 5;
  }
  else
  {
    NoButton = NoButton + 5;
  }
  fec = TabS2Fec[NoButton];
  EnforceValidFEC();
  char Param[7]="fec";
  char Value[255];
  sprintf(Value, "%d", fec);
  SetConfigParam(PATH_PCONFIG, Param, Value);
}

void SelectPTT(int NoButton,int Status)  // TX/RX
{
  SelectInGroup(20, 21, NoButton, Status);
}

void SelectCaption(int NoButton)  // Caption on or off
{
  char Param[10]="caption";
  if(NoButton == 5)
  {
    strcpy(CurrentCaptionState, "off");
    SetConfigParam(PATH_PCONFIG, Param, "off");
  }
  if(NoButton == 6)
  {
    strcpy(CurrentCaptionState, "on");
    SetConfigParam(PATH_PCONFIG, Param, "on");
  }
  SelectInGroupOnMenu(CurrentMenu, 5, 6, NoButton, 1);
}

void ChangeTestCard(int NoButton)  // Change Test Card Selection
{
  int NewTestCard;
  char NewFileName[127];
  char NewFileNamew16[127];
  char NewFileNamew[127];
  char CopyCommand[255];
  char Value[63];

  // Convert button number into test card number
  switch(NoButton)
  {
    case 0:
      NewTestCard = 8;
      break;
    case 1:
      NewTestCard = 9;
      break;
    case 2:
      NewTestCard = 10;
      break;
    case 3:
      NewTestCard = 11;
      break;
    case 5:
      NewTestCard = 4;
      break;
    case 6:
      NewTestCard = 5;
      break;
    case 7:
      NewTestCard = 6;
      break;
    case 8:
      NewTestCard = 7;
      break;
    case 10:
      NewTestCard = 0;
      break;
    case 11:
      NewTestCard = 1;
      break;
    case 12:
      NewTestCard = 2;
      break;
    case 13:
      NewTestCard = 3;
      break;
  }

  if (NewTestCard == 0) // Add other cards in here for caption?
  {
    strcpy(CurrentCaptionState, "on");
    SetConfigParam(PATH_PCONFIG, "caption", "on");
  }
  else
  {
    strcpy(CurrentCaptionState, "off");
    SetConfigParam(PATH_PCONFIG, "caption", "off");
  }

  strcpy(NewFileName, "/home/pi/rpidatv/scripts/images/");
  strcat(NewFileName, TestCardName[NewTestCard]);
  strcpy(NewFileNamew, NewFileName);
  strcpy(NewFileNamew16, NewFileName);
  strcat(NewFileName, ".jpg");
  strcat(NewFileNamew, "w.jpg");
  strcat(NewFileNamew16, "w16.jpg");

  if(access(NewFileName, F_OK ) == 0)
  {
    // Copy across SD Image
    strcpy(CopyCommand, "cp ");
    strcat(CopyCommand, NewFileName);
    strcat(CopyCommand, " /home/pi/rpidatv/scripts/images/tcf.jpg");
    system(CopyCommand);
    CurrentTestCard = NewTestCard;
    snprintf(Value, 10, "%d", NewTestCard);
    SetConfigParam(PATH_TC_CONFIG, "card", Value);

    // Check if 720p image exists
    if(access(NewFileNamew, F_OK ) == 0)
    {
      strcpy(CopyCommand, "cp ");
      strcat(CopyCommand, NewFileNamew);
      strcat(CopyCommand, " /home/pi/rpidatv/scripts/images/tcfw.jpg");
      system(CopyCommand);
    }

    // Check if 1024x576 image exists
    if(access(NewFileNamew16, F_OK ) == 0)
    {
      strcpy(CopyCommand, "cp ");
      strcat(CopyCommand, NewFileNamew16);
      strcat(CopyCommand, " /home/pi/rpidatv/scripts/images/tcfw16.jpg");
      system(CopyCommand);
    }
  }
  else
  {
    MsgBox2("Test Card File Doesn't Exist\n", "Test Card F Selected");
    wait_touch();
    strcpy(CopyCommand, "cp  /home/pi/rpidatv/scripts/images/tcfm.jpg");
    strcat(CopyCommand, " /home/pi/rpidatv/scripts/images/tcf.jpg");
    system(CopyCommand);
    CurrentTestCard = 0;
    strcpy(CurrentCaptionState, "on");
    SetConfigParam(PATH_PCONFIG, "caption", "on");
  }  
}

void SelectSTD(int NoButton)  // PAL or NTSC
{
  char USBVidDevice[255];
  char Param[255];
  char SetStandard[255];
  SelectInGroupOnMenu(CurrentMenu, 8, 9, NoButton, 1);
  strcpy(ModeSTD, TabModeSTD[NoButton - 8]);
  strcpy(Param, "analogcamstandard");
  SetConfigParam(PATH_PCONFIG, Param, ModeSTD);

  // Now Set the Analog Capture (input) Standard
  GetUSBVidDev(USBVidDevice);
  if (strlen(USBVidDevice) == 12)  // /dev/video* with a new line
  {
    USBVidDevice[strcspn(USBVidDevice, "\n")] = 0;  //remove the newline
    strcpy(SetStandard, "v4l2-ctl -d ");
    strcat(SetStandard, USBVidDevice);
    strcat(SetStandard, " --set-standard=");
    strcat(SetStandard, ModeSTD);
    printf("Video Standard set with command %s\n", SetStandard);
    system(SetStandard);
  }
}

void SelectGuard(int NoButton)
{
  char OldGuard[7];

  strcpy(OldGuard, Guard);
  switch (NoButton)
  {
  case 0:                               //   Guard 1/4
    strcpy(Guard, "4");
    break;
  case 1:                               //   Guard 1/8
    strcpy(Guard, "8");
    break;
  case 2:                               //   Guard 1/16
    strcpy(Guard, "16");
    break;
  case 3:                               //   Guard 1/32
    strcpy(Guard, "32");
    break;
  }

  if (strcmp(OldGuard, Guard) != 0)  // if changed, write to config file
  {
    SetConfigParam(PATH_PCONFIG, "guard", Guard);
  }
}

void SelectQAM(int NoButton)
{
  char OldDVBTQAM[7];

  strcpy(OldDVBTQAM, DVBTQAM);
  switch (NoButton)
  {
  case 5:                               //   QPSK
    strcpy(DVBTQAM, "qpsk");
    break;
  case 6:                               //   16-QAM
    strcpy(DVBTQAM, "16qam");
    break;
  case 7:                               //   64-QAM
    strcpy(DVBTQAM, "64qam");
    break;
  }

  if (strcmp(OldDVBTQAM, DVBTQAM) != 0)  // if changed, write to config file
  {
    SetConfigParam(PATH_PCONFIG, "qam", DVBTQAM);
  }
}



void ChangeBandDetails(int NoButton)
{
  char Param[31];
  char Value[31];
  char Prompt[63];
  float AttenLevel = 1;
  int ExpLevel = -1;
  int ExpPorts = -1;
  int LimeGain = -1;
  int MuntjacGain = -1;
  int RFEState = -1;
  int RFEAtt = 1;
  int RFEPort = -1;
  int PlutoLevel = 1;
  float LO = 1000001;
  char Numbers[10] ="";
  char TexttoSave[63];
  char ActualValue[31];
  int band = 0;

  // Convert button number to band number
  switch (NoButton)
  {
    case 0:                               //   24 GHz T
      band = 8;
      break;
    case 1:                               //   47 GHz T
      band = 11;
      break;
    case 2:                               //   76 GHz T
      band = 12;
      break;
    case 3:                               //   Spare
      band = 13;
      break;
    case 5:                               //   50 MHz T
      band = 9;
      break;
    case 6:                               //   13 cm T
      band = 10;
      break;
    case 7:                               //   9 cm T
      band = 5;
      break;
    case 8:                               //   6 cm T
      band = 6;
      break;
    case 9:                               //   10 GHz T
      band = 7;
      break;
    case 10:                               //   50 MHz
      band = 14;
      break;
    case 11:                               //   Invalid Selection
      return;
      break;
    case 14:                               //   9 cm
      band = 15;
      break;
    case 15:                               //   71 MHz
      band = 0;
      break;
    case 16:                               //   146 MHz
      band = 1;
      break;
    case 17:                               //   70 cm
      band = 2;
      break;
    case 18:                               //   23 cm
      band = 3;
      break;
    case 19:                               //   13 cm
      band = 4;
      break;
  }

  // Label
  snprintf(Prompt, 63, "Enter the title for Band with GPIO Code %d", TabBandExpPorts[band]);
  KeyboardReturn[0] = '\0';
  while (strlen(KeyboardReturn) < 1)
  {
    Keyboard(Prompt, TabBandLabel[band], 15);
  }
  strcpy(Param, TabBand[band]);
  strcat(Param, "label");
  SetConfigParam(PATH_PPRESETS ,Param, KeyboardReturn);
  strcpy(TabBandLabel[band], KeyboardReturn);

  // Attenuator Level
  snprintf(Value, 30, "%.2f", TabBandAttenLevel[band]);
  while ((AttenLevel > 0) || (AttenLevel < -31.75) || (strlen(KeyboardReturn) < 1))  // AttenLevel init at 1
  {
    snprintf(Prompt, 63, "Set the Attenuator Level for the %s Band:", TabBandLabel[band]);
    Keyboard(Prompt, Value, 6);
    AttenLevel = atof(KeyboardReturn);
  }
  TabBandAttenLevel[band] = AttenLevel;
  strcpy(Param, TabBand[band]);
  strcat(Param, "attenlevel");
  SetConfigParam(PATH_PPRESETS ,Param, KeyboardReturn);

  // Express Level
  snprintf(Value, 30, "%d", TabBandExpLevel[band]);
  while ((ExpLevel < 0) || (ExpLevel > 44) || (strlen(KeyboardReturn) < 1))  // ExpLevel init at -1
  {
    snprintf(Prompt, 63, "Set the Express Level for the %s Band:", TabBandLabel[band]);
    Keyboard(Prompt, Value, 2);
    ExpLevel = atoi(KeyboardReturn);
  }
  TabBandExpLevel[band] = ExpLevel;
  strcpy(Param, TabBand[band]);
  strcat(Param, "explevel");
  SetConfigParam(PATH_PPRESETS ,Param, KeyboardReturn);

  // GPIO ports
  snprintf(Value, 30, "%d", TabBandExpPorts[band]);
  while ((ExpPorts < 0) || (ExpPorts > 31) || (strlen(KeyboardReturn) < 1))  // ExpPorts init at -1
  {
    snprintf(Prompt, 63, "Enter the GPIO Port Settings for %s:", TabBandLabel[band]);
    Keyboard(Prompt, Value, 2);
    ExpPorts = atoi(KeyboardReturn);
  }
  TabBandExpPorts[band] = ExpPorts;
  snprintf(TexttoSave, 30, "%d", TabBandExpPorts[band]);
  strcpy(Param, TabBand[band]);
  strcat(Param, "expports");
  SetConfigParam(PATH_PPRESETS ,Param, TexttoSave);

  // Lime Gain
  snprintf(Value, 30, "%d", TabBandLimeGain[band]);
  while ((LimeGain < 1) || (LimeGain > 100))    // Do not allow zero or blank Lime Gain
  {
    snprintf(Prompt, 63, "Set the Lime Gain for the %s Band:", TabBandLabel[band]);
    Keyboard(Prompt, Value, 3);
    LimeGain = atoi(KeyboardReturn);
  }
  TabBandLimeGain[band] = LimeGain;
  strcpy(Param, TabBand[band]);
  strcat(Param, "limegain");
  SetConfigParam(PATH_PPRESETS ,Param, KeyboardReturn);

  // Muntjac Gain
  snprintf(Value, 30, "%d", TabBandMuntjacGain[band]);
  while ((MuntjacGain < 1) || (MuntjacGain > 20))    // Do not allow zero or blank Muntjac Gain
  {
    snprintf(Prompt, 63, "Set the Muntjac Gain for the %s Band:", TabBandLabel[band]);
    Keyboard(Prompt, Value, 3);
    MuntjacGain = atoi(KeyboardReturn);
  }
  TabBandMuntjacGain[band] = MuntjacGain;
  strcpy(Param, TabBand[band]);
  strcat(Param, "muntjacgain");
  SetConfigParam(PATH_PPRESETS ,Param, KeyboardReturn);

  // Lime RFE Enable
  strcpy(Param, TabBand[band]);
  strcat(Param, "limerfe");
  GetConfigParam(PATH_PPRESETS, Param, Value);
  if (strcmp(Value, "enabled") == 0)
  {
    RFEState = 1;
  }
  else
  {
    RFEState = 0;
  }

  snprintf(Value, 30, "%d", RFEState);
  RFEState = -1;
  while ((RFEState < 0 ) || (RFEState > 1) || (strlen(KeyboardReturn) < 1))  
  {
    snprintf(Prompt, 63, "Lime RFE Enable for %s. 0=off, 1=on:", TabBandLabel[band]);
    Keyboard(Prompt, Value, 6);
    RFEState = atof(KeyboardReturn);
  }
  strcpy(Param, TabBand[band]);
  strcat(Param, "limerfe");
  if (RFEState == 1)
  {
    strcpy(Value, "enabled");
  }
  else
  {
    strcpy(Value, "disabled");
  }
  SetConfigParam(PATH_PPRESETS ,Param, Value);

  // Lime RFE Tx Port
  strcpy(Param, TabBand[band]);
  strcat(Param, "limerfeport");
  GetConfigParam(PATH_PPRESETS, Param, Value);
  if (strcmp(Value, "txrx") == 0)
  {
    RFEPort = 1;
  }
  else
  {
    RFEPort = 2;
  }
  snprintf(Value, 30, "%d", RFEPort);
  RFEPort = 0;
  while ((RFEPort < 1 ) || (RFEPort > 2) || (strlen(KeyboardReturn) < 1))  
  {
    snprintf(Prompt, 63, "LimeRFE Tx Port for %s Band. 1=txrx, 2=tx:", TabBandLabel[band]);
    Keyboard(Prompt, Value, 6);
    RFEPort = atof(KeyboardReturn);
  }
  strcpy(Param, TabBand[band]);
  strcat(Param, "limerfeport");
  if (RFEPort == 1)
  {
    strcpy(Value, "txrx");
  }
  else
  {
    strcpy(Value, "tx");
  }  
  SetConfigParam(PATH_PPRESETS ,Param, Value);

  // Lime RFE Attenuator Level
  strcpy(Param, TabBand[band]);
  strcat(Param, "limerferxatt");
  GetConfigParam(PATH_PPRESETS, Param, Value);
  RFEAtt=atoi(Value);
  snprintf(Value, 30, "%d", RFEAtt * 2);
  RFEAtt= - 1;
  while ((RFEAtt < 0) || (RFEAtt > 14) || (strlen(KeyboardReturn) < 1))  
  {
    snprintf(Prompt, 63, "Set LimeRFE Rx Atten for %s. 0-14 (dB):", TabBandLabel[band]);
    Keyboard(Prompt, Value, 6);
    RFEAtt = atof(KeyboardReturn);
  }
  strcpy(Param, TabBand[band]);
  strcat(Param, "limerferxatt");
  snprintf(Value, 30, "%d", RFEAtt / 2);
  SetConfigParam(PATH_PPRESETS ,Param, Value);

  // Pluto Power
  snprintf(ActualValue, 30, "%d", TabBandPlutoLevel[band]);
  while ((PlutoLevel < -71) || (PlutoLevel > 0) || (strlen(KeyboardReturn) < 1))  // PlutoLevel init at 1
  {
    snprintf(Prompt, 63, "Set the Pluto Power for the %s Band (0 to -71):", TabBandLabel[band]);
    Keyboard(Prompt, ActualValue, 3);
    PlutoLevel = atoi(KeyboardReturn);
  }
  TabBandPlutoLevel[band] = PlutoLevel;
  snprintf(Value, 3, "%d", -1 * PlutoLevel);
  strcpy(Param, TabBand[band]);
  strcat(Param, "plutopwr");
  SetConfigParam(PATH_PPRESETS, Param, Value);

  // LO frequency
  snprintf(Value, 30, "%.2f", TabBandLO[band]);
  while ((LO > 1000000) || (strlen(KeyboardReturn) < 1))  // LO init at 1000001
  {
    snprintf(Prompt, 63, "Enter LO frequency in MHz for the %s Band:", TabBandLabel[band]);
    Keyboard(Prompt, Value, 10);
    LO = atof(KeyboardReturn);
  }
  TabBandLO[band] = LO;
  strcpy(Param, TabBand[band]);
  strcat(Param, "lo");
  SetConfigParam(PATH_PPRESETS ,Param, KeyboardReturn);

  // Contest Numbers
  while (strlen(Numbers) < 1)
  {
    snprintf(Prompt, 63, "Enter Contest Numbers for the %s Band:", TabBandLabel[band]);
    Keyboard(Prompt, TabBandNumbers[band], 10);
    strcpy(Numbers, KeyboardReturn);
  }
  strcpy(TabBandNumbers[band], Numbers);
  strcpy(Param, TabBand[band]);
  strcat(Param, "numbers");
  SetConfigParam(PATH_PPRESETS, Param, Numbers);

  // Then, if it is the current band, call DoFreqChange
  if (band == CurrentBand)
  {
    DoFreqChange();
  }
}


void DoFreqChange()
{
  // Called after any frequency or band change or band parameter change
  // to set the correct band levels and numbers in portsdown_config.txt
  // Transverter band must be correct on entry.
  // However, this changes direct bands based on the new frequency

  char Param[15];
  char Value[15];
  //char Freqtext[255];
  float CurrentFreq;

  if ((TabBandLO[CurrentBand] < 0.5) && (TabBandLO[CurrentBand] > -0.5)) // LO freq = 0 so not a transverter
  {
    // So look up the current frequency
    strcpy(Param,"freqoutput");
    GetConfigParam(PATH_PCONFIG, Param, Value);
    CurrentFreq = atof(Value);

    printf("DoFreqChange thinks freq is %f\n", CurrentFreq);

    // Set band based on the current frequency

    if (CurrentFreq < 60)                            // 50 MHz
    {
       CurrentBand = 14;
       strcpy(Value, "d0");
    }
    if ((CurrentFreq >= 60) && (CurrentFreq < 100))  // 71 MHz
    {
       CurrentBand = 0;
       strcpy(Value, "d1");
    }
    if ((CurrentFreq >= 100) && (CurrentFreq < 250))  // 146 MHz
    {
      CurrentBand = 1;
      strcpy(Value, "d2");
    }
    if ((CurrentFreq >= 250) && (CurrentFreq < 950))  // 437 MHz
    {
      CurrentBand = 2;
      strcpy(Value, "d3");
    }
    if ((CurrentFreq >= 950) && (CurrentFreq < 2000))  // 1255 MHz
    {
      CurrentBand = 3;
      strcpy(Value, "d4");
    }
    if ((CurrentFreq >= 2000) && (CurrentFreq < 3000))  // 2400 MHz
    {
      CurrentBand = 4;
      strcpy(Value, "d5");
    }
    if (CurrentFreq >= 3000)                          // 3400 MHz
    {
      CurrentBand = 15;
      strcpy(Value, "d6");
    }

    // Set the correct band in portsdown_config.txt
    SetConfigParam(PATH_PCONFIG, "band", Value);
  }

  // CurrentBand now holds the in-use band 

  // Read each Band value in turn from PPresets
  // Then set Current variables and
  // write correct values to portsdown_config.txt

  // Set the Modulation in portsdown_config.txt
  strcpy(Param,"modulation");    
  SetConfigParam(PATH_PCONFIG, Param, CurrentTXMode);

  // Band Label
  strcpy(Param, TabBand[CurrentBand]);
  strcat(Param, "label");
  GetConfigParam(PATH_PPRESETS, Param, Value);

  strcpy(Param, "labelofband");
  SetConfigParam(PATH_PCONFIG, Param, Value);
  
  // Attenuator Level
  strcpy(Param, TabBand[CurrentBand]);
  strcat(Param, "attenlevel");
  GetConfigParam(PATH_PPRESETS, Param, Value);

  TabBandAttenLevel[CurrentBand] = atof(Value);

  strcpy(Param, "attenlevel");
  SetConfigParam(PATH_PCONFIG ,Param, Value);
  
  // Express Level
  strcpy(Param, TabBand[CurrentBand]);
  strcat(Param, "explevel");
  GetConfigParam(PATH_PPRESETS, Param, Value);
 
  TabBandExpLevel[CurrentBand] = atoi(Value);

  strcpy(Param, "explevel");
  SetConfigParam(PATH_PCONFIG ,Param, Value);

  // Express Ports
  strcpy(Param, TabBand[CurrentBand]);
  strcat(Param, "expports");
  GetConfigParam(PATH_PPRESETS, Param, Value);
 
  TabBandExpPorts[CurrentBand] = atoi(Value);

  strcpy(Param, "expports");
  SetConfigParam(PATH_PCONFIG ,Param, Value);

  // Lime Gain
  strcpy(Param, TabBand[CurrentBand]);
  strcat(Param, "limegain");
  GetConfigParam(PATH_PPRESETS, Param, Value);
 
  TabBandLimeGain[CurrentBand] = atoi(Value);

  strcpy(Param, "limegain");
  SetConfigParam(PATH_PCONFIG ,Param, Value);

  // Muntjac Gain
  strcpy(Param, TabBand[CurrentBand]);
  strcat(Param, "muntjacgain");
  GetConfigParam(PATH_PPRESETS, Param, Value);
 
  TabBandMuntjacGain[CurrentBand] = atoi(Value);

  strcpy(Param, "muntjacgain");
  SetConfigParam(PATH_PCONFIG ,Param, Value);

  // Pluto Level
  strcpy(Param, TabBand[CurrentBand]);
  strcat(Param, "plutopwr");
  GetConfigParam(PATH_PPRESETS, Param, Value);
  TabBandPlutoLevel[CurrentBand] = -1 * atoi(Value);
  strcpy(Param, "plutopwr");
  SetConfigParam(PATH_PCONFIG ,Param, Value);

  // LO frequency
  strcpy(Param, TabBand[CurrentBand]);
  strcat(Param, "lo");
  GetConfigParam(PATH_PPRESETS, Param, Value);

  TabBandLO[CurrentBand] = atof(Value);

  strcpy(Param, "lo");
  SetConfigParam(PATH_PPRESETS, Param, Value);

  // Contest Numbers
  strcpy(Param, TabBand[CurrentBand]);
  strcat(Param, "numbers");
  GetConfigParam(PATH_PPRESETS, Param, Value);

  strcpy(TabBandNumbers[CurrentBand], Value);

  strcpy(Param, "numbers");
  SetConfigParam(PATH_PCONFIG ,Param, Value);

  // LimeRFE enable/disable
  strcpy(Param, TabBand[CurrentBand]);
  strcat(Param, "limerfe");
  GetConfigParam(PATH_PPRESETS, Param, Value);
  if (strcmp(Value, "enabled") == 0)
  {
    LimeRFEState = 1;
  }
  else
  {
    LimeRFEState = 0;
  }
  strcpy(Param, "limerfe");
  SetConfigParam(PATH_PCONFIG ,Param, Value);

  // LimeRFE TX Port
  strcpy(Param, TabBand[CurrentBand]);
  strcat(Param, "limerfeport");
  GetConfigParam(PATH_PPRESETS, Param, Value);
  if (strcmp(Value, "txrx") == 0)
  {
    LimeRFEPort = 1;
  }
  else if (strcmp(Value, "tx") == 0)
  {
    LimeRFEPort = 2;
  }
  else
  {
    LimeRFEPort = 3;
  }
  strcpy(Param, "limerfeport");
  SetConfigParam(PATH_PCONFIG ,Param, Value);

  // LimeRFE RX Attenuator
  strcpy(Param, TabBand[CurrentBand]);
  strcat(Param, "limerferxatt");
  GetConfigParam(PATH_PPRESETS, Param, Value);
  LimeRFERXAtt = atoi(Value);
  strcpy(Param, "limerferxatt");
  SetConfigParam(PATH_PCONFIG ,Param, Value);

  if (LimeRFEState == 1)
  {
    LimeRFEInit();
  }
  else
  {
    LimeRFEClose();
  }

  // Set the Band (and filter) Switching
  printf("CTLfilter called\n");
  system ("sudo /home/pi/rpidatv/scripts/ctlfilter.sh");
  // And wait for it to finish using portsdown_config.txt
  usleep(100000);

  // Now check if the Receive upconverter LO needs to be started or stopped
  ReceiveLOStart();  
}

void SelectBand(int NoButton)  // Set the Band  Now from Menu 9
{
  char Param[15];
  char Value[15];
  float CurrentFreq;
  strcpy(Param,"freqoutput");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  CurrentFreq = atof(Value);
  int band;
  bool direct = false;

  // Convert button number to band number
  switch (NoButton)
  {
    case 0:                               //   24 GHz T
      band = 8;
      break;
    case 1:                               //   47 GHz T
      band = 11;
      break;
    case 2:                               //   76 GHz T
      band = 12;
      break;
    case 3:                               //   Spare
      band = 13;
      break;
    case 5:                               //   50 MHz T
      band = 9;
      break;
    case 6:                               //   13 cm T
      band = 10;
      break;
    case 7:                               //   9 cm T
      band = 5;
      break;
    case 8:                               //   6 cm T
      band = 6;
      break;
    case 9:                               //   10 GHz T
      band = 7;
      break;
    case 10:                               //   50 MHz
      band = 14;
      break;
    case 11:                               //   Direct Button
      direct = true;
      break;
    case 14:                               //   9 cm
      band = 15;
      break;
    case 15:                               //   71 MHz
      band = 0;
      break;
    case 16:                               //   146 MHz
      band = 1;
      break;
    case 17:                               //   70 cm
      band = 2;
      break;
    case 18:                               //   23 cm
      band = 3;
      break;
    case 19:                               //   13 cm
      band = 4;
      break;
  }

  if (((TabBandLO[band] < 0.5) && (TabBandLO[band] > -0.5)) && (NoButton != 11))// LO freq = 0 so not a transverter
  {
    // Hidden button pressed, so do nothing
    return;
  }

  if (direct == true)  //Look up correct band
  {
    // Set band based on the current frequency

    if (CurrentFreq < 60)                            // 50 MHz
    {
       band = 14;
       strcpy(Value, "d0");
    }
    if ((CurrentFreq >= 60) && (CurrentFreq < 100))  // 71 MHz
    {
       band = 0;
       strcpy(Value, "d1");
    }
    if ((CurrentFreq >= 100) && (CurrentFreq < 250))  // 146 MHz
    {
      band = 1;
      strcpy(Value, "d2");
    }
    if ((CurrentFreq >= 250) && (CurrentFreq < 950))  // 437 MHz
    {
      band = 2;
      strcpy(Value, "d3");
    }
    if ((CurrentFreq >= 950) && (CurrentFreq < 2000))  // 1255 MHz
    {
      band = 3;
      strcpy(Value, "d4");
    }
    if ((CurrentFreq >= 2000) && (CurrentFreq < 3000))  // 2400 MHz
    {
      band = 4;
      strcpy(Value, "d5");
    }
    if (CurrentFreq >= 3000)                          // 3400 MHz
    {
      band = 15;
      strcpy(Value, "d6");
    }

    // Set the correct band in portsdown_config.txt
    SetConfigParam(PATH_PCONFIG, "band", Value);
  }

  CurrentBand = band;

  // Look up the name of the new Band
  strcpy(Value, TabBand[CurrentBand]);

  // Store the new band
  strcpy(Param, "band");
  SetConfigParam(PATH_PCONFIG, Param, Value);

  // Make all the changes required after a band change
  DoFreqChange();
}

void SelectVidIP(int NoButton)  // Comp Vid or S-Video
{
  char USBVidDevice[255];
  char Param[255];
  char SetVidIP[255];

  SelectInGroupOnMenu(CurrentMenu, 5, 6, NoButton, 1);
  strcpy(ModeVidIP, TabModeVidIP[NoButton - 5]);
  strcpy(Param, "analogcaminput");
  SetConfigParam(PATH_PCONFIG, Param, ModeVidIP);

  // Now Set the Analog Capture (input) Socket
  // command format: v4l2-ctl -d $ANALOGCAMNAME --set-input=$ANALOGCAMINPUT

  GetUSBVidDev(USBVidDevice);
  if (strlen(USBVidDevice) == 12)  // /dev/video* with a new line
  {
    USBVidDevice[strcspn(USBVidDevice, "\n")] = 0;  //remove the newline
    strcpy(SetVidIP, "v4l2-ctl -d ");
    strcat(SetVidIP, USBVidDevice);
    strcat(SetVidIP, " --set-input=");
    strcat(SetVidIP, ModeVidIP);
    printf(SetVidIP);
    system(SetVidIP);
  }
}

void SelectAudio(int NoButton)  // Audio Input
{
  int AudioIndex;
  if (NoButton < 5)
  {
    AudioIndex = NoButton + 5;
  }
  else
  {
    AudioIndex = NoButton - 5;
  }

  SelectInGroupOnMenu(CurrentMenu, 5, 9, NoButton, 1);
  SelectInGroupOnMenu(CurrentMenu, 0, 0, NoButton, 1);
  strcpy(ModeAudio, TabModeAudio[AudioIndex]);
  char Param[]="audio";
  SetConfigParam(PATH_PCONFIG, Param, ModeAudio);
}

void SelectAtten(int NoButton)  // Attenuator Type
{
  SelectInGroupOnMenu(CurrentMenu, 5, 8, NoButton, 1);
  strcpy(CurrentAtten, TabAtten[NoButton - 5]);
  char Param[]="attenuator";
  SetConfigParam(PATH_PCONFIG, Param, CurrentAtten);
}

void SetAttenLevel()
{
  char Prompt[63];
  char Value[31];
  char Param[15];
  float AttenLevel = 1;

  if (strcmp(CurrentAtten, "NONE") !=0)
  {
    while ((AttenLevel > 0) || (AttenLevel < -31.75) || (strlen(KeyboardReturn) < 1))
    {
      snprintf(Prompt, 62, "Set the Attenuator Level for the %s Band:", TabBandLabel[CurrentBand]);
      snprintf(Value, 7, "%.2f", TabBandAttenLevel[CurrentBand]);
      Keyboard(Prompt, Value, 6);
      AttenLevel = atof(KeyboardReturn);
    }

    TabBandAttenLevel[CurrentBand] = AttenLevel;
    strcpy(Param, TabBand[CurrentBand]);
    strcat(Param, "attenlevel");
    SetConfigParam(PATH_PPRESETS, Param, KeyboardReturn);
    strcpy(Param, "attenlevel");
    SetConfigParam(PATH_PCONFIG, Param, KeyboardReturn);

    // Now set the new level
    system ("sudo /home/pi/rpidatv/scripts/ctlfilter.sh");
  }
}

void SetDeviceLevel()
{
  char Prompt[63];
  char Value[31];
  char Param[15];
  int ExpLevel = -1;
  int LimeGain = -1;
  int MuntjacGain = -1;
  int PlutoLevel = 1; // valid range -71 to 0, bit stored as 71 to 0 in config file

  if (strcmp(CurrentModeOP, TabModeOP[2]) == 0)  // DATV Express
  {
    while ((ExpLevel < 0) || (ExpLevel > 44) || (strlen(KeyboardReturn) < 1))
    {
      snprintf(Prompt, 62, "Set the Express Output Level for the %s Band:", TabBandLabel[CurrentBand]);
      snprintf(Value, 3, "%d", TabBandExpLevel[CurrentBand]);
      Keyboard(Prompt, Value, 2);
      ExpLevel = atoi(KeyboardReturn);
    }
    TabBandExpLevel[CurrentBand] = ExpLevel;
    strcpy(Param, TabBand[CurrentBand]);
    strcat(Param, "explevel");
    SetConfigParam(PATH_PPRESETS, Param, KeyboardReturn);
    strcpy(Param, "explevel");
    SetConfigParam(PATH_PCONFIG, Param, KeyboardReturn);
  }
  else if ((strcmp(CurrentModeOP, TabModeOP[3]) == 0) || (strcmp(CurrentModeOP, TabModeOP[8]) == 0)
        || (strcmp(CurrentModeOP, TabModeOP[9]) == 0) || (strcmp(CurrentModeOP, TabModeOP[12]) == 0))  
        // Lime Mini or USB or JLIME or LIMEDVB
  {
    while ((LimeGain < 0) || (LimeGain > 100) || (strlen(KeyboardReturn) < 1))
    {
      snprintf(Prompt, 62, "Set the Lime Gain for the %s Band:", TabBandLabel[CurrentBand]);
      snprintf(Value, 4, "%d", TabBandLimeGain[CurrentBand]);
      Keyboard(Prompt, Value, 3);
      LimeGain = atoi(KeyboardReturn);
    }
    TabBandLimeGain[CurrentBand] = LimeGain;
    strcpy(Param, TabBand[CurrentBand]);
    strcat(Param, "limegain");
    SetConfigParam(PATH_PPRESETS, Param, KeyboardReturn);
    strcpy(Param, "limegain");
    SetConfigParam(PATH_PCONFIG, Param, KeyboardReturn);
  }
  else if (strcmp(CurrentModeOP, TabModeOP[11]) == 0) // Muntjac
  {
    while ((MuntjacGain < 0) || (MuntjacGain > 20) || (strlen(KeyboardReturn) < 1))
    {
      snprintf(Prompt, 62, "Set the Muntjac Gain for the %s Band:", TabBandLabel[CurrentBand]);
      snprintf(Value, 4, "%d", TabBandMuntjacGain[CurrentBand]);
      Keyboard(Prompt, Value, 3);
      MuntjacGain = atoi(KeyboardReturn);
    }
    TabBandMuntjacGain[CurrentBand] = MuntjacGain;
    strcpy(Param, TabBand[CurrentBand]);
    strcat(Param, "muntjacgain");
    SetConfigParam(PATH_PPRESETS, Param, KeyboardReturn);
    strcpy(Param, "muntjacgain");
    SetConfigParam(PATH_PCONFIG, Param, KeyboardReturn);
  }
  else if (strcmp(CurrentModeOP, "PLUTO") == 0)  // Pluto
  {
    while ((PlutoLevel < -71) || (PlutoLevel > 0) || (strlen(KeyboardReturn) < 1))
    {
      snprintf(Prompt, 62, "Set the Pluto Power for the %s Band (0 to -71):", TabBandLabel[CurrentBand]);
      snprintf(Value, 5, "%d", TabBandPlutoLevel[CurrentBand]);
      Keyboard(Prompt, Value, 3);
      PlutoLevel = atoi(KeyboardReturn);
    }
    TabBandPlutoLevel[CurrentBand] = PlutoLevel;
    snprintf(Value, 3, "%d", -1 * PlutoLevel);
    strcpy(Param, TabBand[CurrentBand]);
    strcat(Param, "plutopwr");
    SetConfigParam(PATH_PPRESETS, Param, Value);
    strcpy(Param, "plutopwr");
    SetConfigParam(PATH_PCONFIG, Param, Value);
  }

  else
  {
  // Do nothing
  }
}


void AdjustVLCVolume(int adjustment)
{
  int VLCVolumePerCent;
  static int premuteVLCVolume;
  static bool muted;
  char VLCVolumeText[63];
  char VolumeMessageText[63];
  char VLCVolumeCommand[255];

  if (adjustment == -512) // toggle mute
  {
    if (muted  == true)
    {
      CurrentVLCVolume = premuteVLCVolume;
      muted = false;
    }
    else
    {
      premuteVLCVolume = CurrentVLCVolume;
      CurrentVLCVolume = 0;
      muted = true;
    }
  }
  else                    // normal up or down
  {
    CurrentVLCVolume = CurrentVLCVolume + adjustment;
    if (CurrentVLCVolume < 0)
    {
      CurrentVLCVolume = 0;
    }
    if (CurrentVLCVolume > 512)
    {
      CurrentVLCVolume = 512;
    }
  }

  snprintf(VLCVolumeText, 62, "%d", CurrentVLCVolume);
  SetConfigParam(PATH_PCONFIG, "vlcvolume", VLCVolumeText);

  snprintf(VLCVolumeCommand, 254, "/home/pi/rpidatv/scripts/setvlcvolume.sh %d", CurrentVLCVolume);
  system(VLCVolumeCommand); 

  VLCVolumePerCent = (100 * CurrentVLCVolume) / 512;
  snprintf(VolumeMessageText, 62, "VOLUME %d%%", VLCVolumePerCent);
  // printf("%s\n", VolumeMessageText);
  
  FILE *fw=fopen("/home/pi/tmp/vlc_temp_overlay.txt","w+");
  if(fw!=0)
  {
    fprintf(fw, "%s\n", VolumeMessageText);
  }
  fclose(fw);

  // Copy temp file to file to be read by VLC to prevent file collisions
  system("cp /home/pi/tmp/vlc_temp_overlay.txt /home/pi/tmp/vlc_overlay.txt");
  // Clear the volume caption after 1 second
  system("(sleep 1; echo " " > /home/pi/tmp/vlc_overlay.txt) &");
}


void SetReceiveLOFreq(int NoButton)
{
  char Param[31];
  char Value[31];
  char Value14[15];
  char Prompt[63];
  int band;

  // Convert button number to band number
  switch (NoButton)
  {
    case 0:                               //   50 MHz
      band = 14;
      break;
    case 1:                               //   71 MHz
      band = 0;
      break;
    case 2:                               //   146 MHz
      band = 1;
      break;
    case 3:                               //   70 cm
      band = 2;
      break;
    case 5:                               //   50 MHz T
      band = 9;
      break;
    case 6:                               //   Spare
      band = 13;
      break;
  }

  // Compose the prompt
  strcpy(Param, TabBand[band]);
  strcat(Param, "label");
  GetConfigParam(PATH_PPRESETS, Param, Value);
  strcpyn(Value14, Value, 14);
  snprintf(Prompt, 63, "Enter RX LO Freq (MHz) for %s Band (0 for off)", Value14);
  
  // Look up the current value
  strcpy(Param, TabBand[band]);
  strcat(Param, "lo");
  GetConfigParam(PATH_RXPRESETS, Param, Value);

  Keyboard(Prompt, Value, 15);

  SetConfigParam(PATH_RXPRESETS ,Param, KeyboardReturn);
}


void SavePreset(int PresetButton)
{
  char Param[255];
  char Value[255];
  char Prompt[63];

  // Read the Preset Label and ask for a new value
  snprintf(Prompt, 62, "Enter the new label for Preset %d:", PresetButton + 1);
  strcpy(Value, "");
  snprintf(Param, 10, "p%dlabel", PresetButton + 1);
  GetConfigParam(PATH_PPRESETS, Param, Value);
  Keyboard(Prompt, Value, 10);
  SetConfigParam(PATH_PPRESETS, Param, KeyboardReturn);
  strcpy(TabPresetLabel[PresetButton], KeyboardReturn);

  // Save the current frequency to Preset
  strcpy(Value, "");
  strcpy(Param, "freqoutput");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  snprintf(Param, 10, "p%dfreq", PresetButton + 1);
  SetConfigParam(PATH_PPRESETS, Param, Value);

  // Save the current band to Preset
  strcpy(Value, "");
  strcpy(Param, "band");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  snprintf(Param, 10, "p%dband", PresetButton + 1);
  SetConfigParam(PATH_PPRESETS, Param, Value);

  // Save the current modulation to Preset
  strcpy(Value, CurrentTXMode);
  snprintf(Param, 10, "p%dmode", PresetButton + 1);
  SetConfigParam(PATH_PPRESETS, Param, Value);

  // Save the current encoder to Preset
  strcpy(Value, CurrentEncoding);
  snprintf(Param, 10, "p%dencoder", PresetButton + 1);
  SetConfigParam(PATH_PPRESETS, Param, Value);

  // Save the current format to Preset
  strcpy(Value, CurrentFormat);
  snprintf(Param, 10, "p%dformat", PresetButton + 1);
  SetConfigParam(PATH_PPRESETS, Param, Value);

  // Save the current source to Preset
  strcpy(Value, CurrentSource);
  snprintf(Param, 10, "p%dsource", PresetButton + 1);
  SetConfigParam(PATH_PPRESETS, Param, Value);

  // Save the current output device to Preset
  strcpy(Value, "");
  strcpy(Param, "modeoutput");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  snprintf(Param, 10, "p%doutput", PresetButton + 1);
  SetConfigParam(PATH_PPRESETS, Param, Value);

  // Save the current SR to Preset
  strcpy(Value, "");
  strcpy(Param, "symbolrate");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  snprintf(Param, 10, "p%dsr", PresetButton + 1);
  SetConfigParam(PATH_PPRESETS, Param, Value);

  // Save the current FEC to Preset
  strcpy(Value, "");
  strcpy(Param, "fec");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  snprintf(Param, 10, "p%dfec", PresetButton + 1);
  SetConfigParam(PATH_PPRESETS, Param, Value);

  // Save the current audio setting to Preset
  strcpy(Value, "");
  strcpy(Param, "audio");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  snprintf(Param, 10, "p%daudio", PresetButton + 1);
  SetConfigParam(PATH_PPRESETS, Param, Value);
}

void RecallPreset(int PresetButton)
{
  char Param[255];
  char Value[255];

  // Read Preset Frequency and store in portsdown_config (not in memory)
  strcpy(Value, "");
  snprintf(Param, 10, "p%dfreq", PresetButton + 1);
  GetConfigParam(PATH_PPRESETS, Param, Value);
  SetConfigParam(PATH_PCONFIG, "freqoutput", Value);

  // Read Preset Band and store in portsdown_config and in memory
  strcpy(Value, "");
  snprintf(Param, 10, "p%dband", PresetButton + 1);
  GetConfigParam(PATH_PPRESETS, Param, Value);
  SetConfigParam(PATH_PCONFIG, "band", Value);
  ReadBand();   //Sets CurrentBand consistently from saved freqoutput and band

  // Read modulation, encoding, format and source

  strcpy(Value, "");
  snprintf(Param, 10, "p%dmode", PresetButton + 1);
  GetConfigParam(PATH_PPRESETS, Param, Value);
  strcpy(CurrentTXMode, Value);

  strcpy(Value, "");
  snprintf(Param, 15, "p%dencoder", PresetButton + 1);
  GetConfigParam(PATH_PPRESETS, Param, Value);
  strcpy(CurrentEncoding, Value);

  strcpy(Value, "");
  snprintf(Param, 15, "p%dformat", PresetButton + 1);
  GetConfigParam(PATH_PPRESETS, Param, Value);
  strcpy(CurrentFormat, Value);

  strcpy(Value, "");
  snprintf(Param, 15, "p%dsource", PresetButton + 1);
  GetConfigParam(PATH_PPRESETS, Param, Value);
  strcpy(CurrentSource, Value);

  // Decode them and set modeinput in memory and portsdown_config
  ApplyTXConfig();

  // Read Output Device
  strcpy(Value, "");
  snprintf(Param, 15, "p%doutput", PresetButton + 1);
  GetConfigParam(PATH_PPRESETS, Param, Value);
  SetConfigParam(PATH_PCONFIG, "modeoutput", Value);
  strcpy(Value, "");
  ReadModeOutput(Value);  // Set CurrentModeOP and CurrentModeOPtest

  // Read SR
  strcpy(Value, "");
  snprintf(Param, 15, "p%dsr", PresetButton + 1);
  GetConfigParam(PATH_PPRESETS, Param, Value);
  SetConfigParam(PATH_PCONFIG, "symbolrate", Value);
  SR = atoi(Value);

  // Read FEC
  strcpy(Value, "");
  snprintf(Param, 15, "p%dfec", PresetButton + 1);
  GetConfigParam(PATH_PPRESETS, Param, Value);
  SetConfigParam(PATH_PCONFIG, "fec", Value);
  fec = atoi(Value);
  
  // Read Audio setting
  strcpy(Value, "");
  snprintf(Param, 15, "p%daudio", PresetButton + 1);
  GetConfigParam(PATH_PPRESETS, Param, Value);
  SetConfigParam(PATH_PCONFIG, "audio", Value);
  strcpy(ModeAudio, Value);

  // Make sure that changes are applied
  DoFreqChange();
}

void SelectVidSource(int NoButton)  // Video Source
{
  //SelectInGroupOnMenu(CurrentMenu, 5, 9, NoButton, 1);
  //SelectInGroupOnMenu(CurrentMenu, 0, 2, NoButton, 1);
  if (NoButton < 4) // allow for reverse numbering of rows
  {
    NoButton = NoButton + 10;
  }
  strcpy(CurrentVidSource, TabVidSource[NoButton - 5]);
  CompVidStart();
}


void ReceiveLOStart()
{
  char Param[15];
  char Value[15];
  char bashCall[127];

  strcpy(Param, TabBand[CurrentBand]);
  strcat(Param, "lo");
  GetConfigParam(PATH_RXPRESETS, Param, Value);

  if(strcmp(Value, "0") != 0)  // Start the LO
  {
    strcpy(bashCall, "sudo /home/pi/rpidatv/bin/adf4351 ");
    strcat(bashCall, Value);
    strcat(bashCall, " ");
    strcat(bashCall, CurrentADFRef);
    strcat(bashCall, " 3"); //max level
  }
  else                        // Stop the LO
  {
    strcpy(bashCall, "sudo /home/pi/rpidatv/bin/adf4351 off");
  }
  //printf("bash:-%s\n", bashCall);
  system(bashCall);
}

void CompVidInitialise()
{
  VidPTT = 1;
  digitalWrite(GPIO_Band_LSB, LOW);
  digitalWrite(GPIO_Band_MSB, LOW);
  digitalWrite(GPIO_Tverter, LOW);
  digitalWrite(GPIO_PTT, HIGH);
  char mic[15] = "xx";
  // char card[15];
  char commnd[255];

  // Start the audio pipe from mic to USB Audio out socket
  GetMicAudioCard(mic);
  if (strlen(mic) == 1)
  {
    // GetPiAudioCard(card);
    strcpy(commnd, "arecord -D plughw:");
    strcat(commnd, mic);
  //strcat(commnd, ",0 -f cd - | /dev/null &");
  strcat(commnd, ",0 -f cd - | aplay -D plughw:");
  //strcat(commnd, card);
  strcat(commnd, mic);
  strcat(commnd, ",0 - &");
  system(commnd);
  //printf("%s\n", commnd);
  }
  else
  {
    printf("mic = %s, so not starting arecord\n", mic);
  }
}

void CompVidStart()
{
  char fbicmd[255];
  int TCDisplay = 1;
  int rawX, rawY, rawPressure;
  FILE *fp;
  char SnapIndex[255];
  int SnapNumber;
  int Snap;
  char picamdev1[15];
  char bashcmd[255];

  if (strcmp(CurrentVidSource, "Pi Cam") == 0)
  {
    system("sudo modprobe bcm2835_v4l2");
    GetPiCamDev(picamdev1);         
    if (strlen(picamdev1) > 1)
    {
      strcpy(bashcmd, "v4l2-ctl -d ");
      strcat(bashcmd, picamdev1);
      strcat(bashcmd, " --set-fmt-overlay=left=0,top=0,width=736,height=416 --overlay=1");
      system(bashcmd);
      strcpy(ScreenState, "VideoOut");
      wait_touch();
      strcpy(bashcmd, "v4l2-ctl -d ");  // Now turn the overlay off
      strcat(bashcmd, picamdev1);
      strcat(bashcmd, " --overlay=0");
      system(bashcmd);
    }
    else
    {
      MsgBox("Pi Cam Not Connected");
      wait_touch();
    }
  }

  if (strcmp(CurrentVidSource, "TCAnim") == 0)
  {
    strcpy(fbicmd, "/home/pi/rpidatv/bin/tcanim1v16 \"/home/pi/rpidatv/video/*10\" ");
    strcat(fbicmd, " \"720\" \"576\" \"48\" \"72\" \"CQ\" \"CQ CQ CQ de ");
    strcat(fbicmd, CallSign);
    strcat(fbicmd, " - ATV on ");
    strcat(fbicmd, TabBandLabel[CompVidBand]);
    strcat(fbicmd, " \" &");
    system(fbicmd);
    strcpy(ScreenState, "VideoOut");
    wait_touch();
    system("sudo killall tcanim1v16");
  }

  if (strcmp(CurrentVidSource, "Contest") == 0)
  {
    // Delete any previous Contest image
    system("rm /home/pi/tmp/contest.jpg >/dev/null 2>/dev/null");

    // Create the new image
    strcpy(fbicmd, "convert -font \"FreeSans\" -size 720x576 xc:white ");
    strcat(fbicmd, "-gravity North -pointsize 125 -annotate 0,0,0,20 ");
    strcat(fbicmd, CallSign); 
    strcat(fbicmd, " -gravity Center -pointsize 200 -annotate 0,0,0,20 ");
    strcat(fbicmd, TabBandNumbers[CompVidBand]);
    strcat(fbicmd, " -gravity South -pointsize 75 -annotate 0,0,0,20 \"");
    strcat(fbicmd, Locator);
    strcat(fbicmd, "    ");
    strcat(fbicmd, TabBandLabel[CompVidBand]);
    strcat(fbicmd, "\" /home/pi/tmp/contest.jpg");
    system(fbicmd);

    // Make the display ready
    strcpy(ScreenState, "VideoOut");

    strcpy(fbicmd, "sudo fbi -T 1 -noverbose -a /home/pi/tmp/contest");
    strcat(fbicmd, ".jpg >/dev/null 2>/dev/null");
    system(fbicmd);
    wait_touch();
  }

  if (strcmp(CurrentVidSource, "TestCard") == 0)
  {
    // Make the display ready
    strcpy(ScreenState, "VideoOut");

    // Delete any old test card with caption
    system("rm /home/pi/tmp/caption.png >/dev/null 2>/dev/null");
    system("rm /home/pi/tmp/tcf2.jpg >/dev/null 2>/dev/null");

    if (strcmp(CurrentCaptionState, "on") == 0)
    {
      // Compose the new card
      strcpy(fbicmd, "convert -font \"FreeSans\" -size 720x80 xc:transparent -fill white -gravity Center -pointsize 40 -annotate 0 ");
      strcat(fbicmd, CallSign); 
      strcat(fbicmd, " /home/pi/tmp/caption.png");
      system(fbicmd);

      strcpy(fbicmd, "convert /home/pi/rpidatv/scripts/images/tcf.jpg /home/pi/tmp/caption.png ");
      strcat(fbicmd, "-geometry +0+475 -composite /home/pi/tmp/tcf2.jpg");
      system(fbicmd);
    }
    else
    {
      system("cp /home/pi/rpidatv/scripts/images/tcf.jpg /home/pi/tmp/tcf2.jpg");
    }

    while ((TCDisplay == 1) || (TCDisplay == -1))
    {
      switch(ImageIndex)
      {
      case 0:
        system("sudo fbi -T 1 -noverbose -a /home/pi/tmp/tcf2.jpg >/dev/null 2>/dev/null");
        break;
      case 1:
        system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/tcc.jpg >/dev/null 2>/dev/null");
        break;
      case 2:
        system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/pm5544.jpg >/dev/null 2>/dev/null");
        break;
      case 3:
        system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/75cb.jpg >/dev/null 2>/dev/null");
        break;
      case 4:
        system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/11g.jpg >/dev/null 2>/dev/null");
        break;
      case 5:
        system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/pb.jpg >/dev/null 2>/dev/null");
        break;
      }
      if (getTouchSample(&rawX, &rawY, &rawPressure)==0) continue;
      TCDisplay = IsImageToBeChanged(rawX, rawY);
      if (TCDisplay != 0)
      {
        ImageIndex = ImageIndex + TCDisplay;
        if (ImageIndex > ImageRange)
        {
          ImageIndex = 0;
        }
        if (ImageIndex < 0)
        {
          ImageIndex = ImageRange;
        }
      }
    }
  }

  if (strcmp(CurrentVidSource, "Snap") == 0)
  {
    // Make the display ready
    strcpy(ScreenState, "VideoOut");

    // Fetch the Next Snap serial number
    fp = popen("cat /home/pi/snaps/snap_index.txt", "r");
    if (fp == NULL) 
    {
      printf("Failed to run command\n" );
      exit(1);
    }
    /* Read the output a line at a time - output it. */
    while (fgets(SnapIndex, 20, fp) != NULL)
    {
      printf("%s", SnapIndex);
    }
    /* close */
    pclose(fp);

    SnapNumber=atoi(SnapIndex);
    Snap = SnapNumber - 1;

    while (((TCDisplay == 1) || (TCDisplay == -1)) && (SnapNumber != 0))
      {
        sprintf(SnapIndex, "%d", Snap);
        strcpy(fbicmd, "sudo fbi -T 1 -noverbose -a /home/pi/snaps/snap");
        strcat(fbicmd, SnapIndex);
        strcat(fbicmd, ".jpg >/dev/null 2>/dev/null");
        system(fbicmd);

        if (getTouchSample(&rawX, &rawY, &rawPressure)==0) continue;

        TCDisplay = IsImageToBeChanged(rawX, rawY);
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
  }
  system("(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
}

void CompVidStop()
{
  // Set PTT low
  VidPTT = 0;
  digitalWrite(GPIO_PTT, LOW);

  // Stop the audio relay
  system("killall arecord >/dev/null 2>/dev/null");
  system("killall aplay >/dev/null 2>/dev/null");

  // Reset the Band Switching
  system ("sudo /home/pi/rpidatv/scripts/ctlfilter.sh");
  // and wait for it to finish using rpidatvconfig.txt
  usleep(100000);
}

void TransmitStart()
{
  // printf("Transmit Start\n");

  char Param[255];
  char Value[255];
  #define PATH_SCRIPT_A "/home/pi/rpidatv/scripts/a.sh >/dev/null 2>/dev/null"

  // Turn the VCO off in case it has been used for receive
  system("sudo /home/pi/rpidatv/bin/adf4351 off");

  // Check Lime connected if selected
  CheckLimeReady();
  // and Check Pluto connected if selected
  CheckPlutoReady();

  strcpy(Param,"modeinput");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  strcpy(ModeInput,Value);

  // If Pi Cam source, and not IPTS or TS File input, clear the screen
  if ((strcmp(CurrentSource, "Pi Cam") == 0) && (strcmp(CurrentEncoding, "IPTS in") != 0)
   && (strcmp(CurrentEncoding, "IPTS in H264") != 0) && (strcmp(CurrentEncoding, "IPTS in H265") != 0)
   && (strcmp(CurrentEncoding, "TS File") != 0))
  {
    setBackColour(0, 0, 0);
    clearScreen();
  }

  // Check if MPEG-2 camera mode selected, or streaming PiCam
  if ((strcmp(ModeInput, "CAMMPEG-2")==0)
    ||(strcmp(ModeInput, "CAM16MPEG-2")==0)
    ||(strcmp(ModeInput, "CAMHDMPEG-2")==0)
    ||((strcmp(CurrentModeOP, TabModeOP[4]) == 0) && (strcmp(CurrentSource, "Pi Cam") == 0)))
  {
    // Load the camera driver and start the viewfinder
    system("sudo modprobe bcm2835_v4l2");
    system("v4l2-ctl --overlay=1 >/dev/null 2>/dev/null");
    strcpy(ScreenState, "TXwithImage");
  }

  // Check if H264 Camera selected
  if(strcmp(ModeInput,"CAMH264") == 0)
  {
    strcpy(ScreenState, "TXwithImage");
  }

  // Check if a desktop mode is selected; if so, display desktop

  if  ((strcmp(ModeInput,"CARDH264")==0)
    || (strcmp(ModeInput,"CONTEST") == 0) 
    || (strcmp(ModeInput,"DESKTOP") == 0)
    || (strcmp(ModeInput,"CARDMPEG-2")==0)
    || (strcmp(ModeInput,"CONTESTMPEG-2")==0)
    || (strcmp(ModeInput,"CARD16MPEG-2")==0)
    || (strcmp(ModeInput,"CARDHDMPEG-2")==0))
  {
    strcpy(ScreenState, "TXwithImage");
  }

  // Check if non-display input mode selected.  If so, turn off response to buttons.

  if ((strcmp(ModeInput,"ANALOGCAM") == 0)
    || (strcmp(ModeInput,"PATERNAUDIO") == 0)
    ||(strcmp(ModeInput,"WEBCAMH264") == 0)
    ||(strcmp(ModeInput,"ANALOGMPEG-2") == 0)
    ||(strcmp(ModeInput,"CARRIER") == 0)
    ||(strcmp(ModeInput,"TESTMODE") == 0)
    ||(strcmp(ModeInput,"IPTSIN") == 0)
    ||(strcmp(ModeInput,"IPTSIN264") == 0)
    ||(strcmp(ModeInput,"IPTSIN265") == 0)
    ||(strcmp(ModeInput,"FILETS") == 0)
    ||(strcmp(ModeInput,"WEBCAMMPEG-2") == 0)
    ||(strcmp(ModeInput,"ANALOG16MPEG-2") == 0)
    ||(strcmp(ModeInput,"WEBCAM16MPEG-2") == 0)
    ||(strcmp(ModeInput,"WEBCAMHDMPEG-2") == 0)
    ||(strcmp(ModeInput,"C920H264") == 0)
    ||(strcmp(ModeInput,"C920HDH264") == 0)
    ||(strcmp(ModeInput,"C920FHDH264") == 0)
    ||(strcmp(ModeInput,"JHDMI") == 0)
    ||(strcmp(ModeInput,"JCAM") == 0)
    ||(strcmp(ModeInput,"JPC") == 0)
    ||(strcmp(ModeInput,"JWEBCAM") == 0)
    ||(strcmp(ModeInput,"JCARD") == 0)
    ||(strcmp(ModeInput,"HDMI") == 0)
    ||(strcmp(CurrentEncoding, "TS File") == 0)
    ||(strcmp(CurrentEncoding, "IPTS in") == 0))
  {
     strcpy(ScreenState, "TXwithMenu");
  }

  // Run the Extra script for TX start
  system("/home/pi/rpidatv/scripts/TXstartextras.sh &");

  if (LimeRFEState == 1)
  {
    LimeRFETX();
  }

  // Call a.sh to transmit
  system(PATH_SCRIPT_A);
}

void *Wait3Seconds(void * arg)
{
  system ("/home/pi/rpidatv/scripts/webcam_reset.sh");
  strcpy(ScreenState, "NormalMenu");
  return NULL;
}

void TransmitStop()
{
  char Param[255];
  char Value[255];
  int WebcamPresent = 0;
  char bashcmd[255];
  char picamdev1[15];

  printf("\nStopping all transmit processes, even if they weren't running\n");

  // If transmit menu is displayed, blue-out the TX button here
  // code to be added

  if (LimeRFEState == 1)
  {
    LimeRFERX();
  }

  // Turn the VCO off
  system("sudo /home/pi/rpidatv/bin/adf4351 off");

  // Run the Extra scripts for TX stop
  system("/home/pi/rpidatv/scripts/TXstop.sh &");
  system("/home/pi/rpidatv/scripts/TXstopextras.sh &");

  // Check for C910, C525, C310 or C270 webcam
  WebcamPresent = DetectLogitechWebcam();

  // Stop DATV Express transmitting
  char expressrx[50];
  strcpy(Param,"modeoutput");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  strcpy(ModeOutput,Value);
  if(strcmp(ModeOutput,"DATVEXPRESS")==0)
  {
    strcpy( expressrx, "echo \"set ptt rx\" >> /tmp/expctrl" );
    system(expressrx);
    strcpy( expressrx, "echo \"set car off\" >> /tmp/expctrl" );
    system(expressrx);
    system("sudo killall netcat >/dev/null 2>/dev/null");
  }

  // Stop Lime transmitting
  if((strcmp(ModeOutput, "LIMEMINI") == 0) || (strcmp(ModeOutput, "LIMEUSB") == 0)
    || (strcmp(ModeOutput, "LIMEDVB") == 0))
  {
    system("sudo killall dvb2iq >/dev/null 2>/dev/null");
    system("sudo killall dvb2iq2 >/dev/null 2>/dev/null");
    system("sudo killall limesdr_send >/dev/null 2>/dev/null");
    system("sudo killall limesdr_dvb >/dev/null 2>/dev/null");
    system("(sleep 0.5; /home/pi/rpidatv/bin/limesdr_stopchannel) &");
  }

  // Stop Muntjac transmitting
  if (strcmp(ModeOutput, "MUNTJAC") == 0)
  {
    system("sudo killall dvb2iq >/dev/null 2>/dev/null");
    system("sudo killall dvb2iq2 >/dev/null 2>/dev/null");
    system("sudo killall muntjacsdr_dvb >/dev/null 2>/dev/null");
  }

  // Kill the key processes as nicely as possible
  system("sudo killall rpidatv >/dev/null 2>/dev/null");
  system("sudo killall ffmpeg >/dev/null 2>/dev/null");
  system("sudo killall tcanim1v16 >/dev/null 2>/dev/null");
  system("sudo killall avc2ts >/dev/null 2>/dev/null");
  system("sudo killall sox >/dev/null 2>/dev/null");
  system("sudo killall arecord >/dev/null 2>/dev/null");
  system("sudo killall dvb_t_stack >/dev/null 2>/dev/null");
  system("sudo killall /home/pi/rpidatv/bin/dvb_t_stack_lime > /dev/null 2>/dev/null");
  system("sudo killall /home/pi/rpidatv/bin/dvb_t_stack_limeusb > /dev/null 2>/dev/null");

  if((strcmp(ModeOutput, "IQ") == 0) || (strcmp(ModeOutput, "QPSKRF") == 0))
  {
    //  Ensure that Transverter line does not float
    //  As it is released when rpidatv terminates
    pinMode(GPIO_Tverter, OUTPUT);
  }

  // Look up the Pi Cam device name and turn the Viewfinder off
  GetPiCamDev(picamdev1);
  if (strlen(picamdev1) > 1)
  {
    strcpy(bashcmd, "v4l2-ctl -d ");
    strcat(bashcmd, picamdev1);
    strcat(bashcmd, " --overlay=0 >/dev/null 2>/dev/null");
    system(bashcmd);
  }

  // Stop the audio relay in CompVid mode
  system("sudo killall arecord >/dev/null 2>/dev/null");

  // Then pause and make sure that avc2ts has really been stopped (needed at high SRs)
  usleep(1000);
  system("sudo killall -9 avc2ts >/dev/null 2>/dev/null");
  system("sudo killall -9 limesdr_send >/dev/null 2>/dev/null");
  system("sudo killall -9 limesdr_dvb >/dev/null 2>/dev/null");
  system("sudo killall -9 sox >/dev/null 2>/dev/null");

  // And make sure rpidatv has been stopped (required for brief transmit selections)
  system("sudo killall -9 rpidatv >/dev/null 2>/dev/null");
  system("sudo killall -9 dvb_t_stack >/dev/null 2>/dev/null");

  // Ensure PTT off.  Required for carrier mode
  pinMode(GPIO_PTT, OUTPUT);
  digitalWrite(GPIO_PTT, LOW);

  // Re-enable SR selection which might have been set all high by a LimeSDR
  system("/home/pi/rpidatv/scripts/ctlSR.sh");

  // Make sure that a.sh has stopped
  system("sudo killall a.sh >/dev/null 2>/dev/null");

  // Delete the transmit fifos
  system("sudo rm videoes >/dev/null 2>/dev/null");
  system("sudo rm videots >/dev/null 2>/dev/null");
  system("sudo rm netfifo >/dev/null 2>/dev/null");
  system("sudo rm audioin.wav >/dev/null 2>/dev/null");

  // Start the Receive LO Here
  ReceiveLOStart();

  // Wait a further 3 seconds and reset v42l-ctl if Logitech C910, C270, C310 or C525 present
  if (WebcamPresent == 1)
  {
    // Check if Webcam was in use
    if ((strcmp(ModeInput,"WEBCAMH264") == 0)
      ||(strcmp(ModeInput,"WEBCAMMPEG-2") == 0)
      ||(strcmp(ModeInput,"WEBCAM16MPEG-2") == 0)
      ||(strcmp(ModeInput,"WEBCAMHDMPEG-2") == 0))
    {
      strcpy(ScreenState, "WebcamWait");
      // Create Wait 3 seconds timer thread
      pthread_create (&thwait3, NULL, &Wait3Seconds, NULL);
    }
  }
}


void *WaitButtonEvent(void * arg)
{
  int rawX, rawY, rawPressure;
  while(getTouchSample(&rawX, &rawY, &rawPressure)==0);
  FinishedButton=1;
  return NULL;
}

void *WaitButtonVideo(void * arg)
{
  int rawX, rawY, rawPressure;
  FinishedButton = 0;
  while(FinishedButton == 0)
  {
    while(getTouchSample(&rawX, &rawY, &rawPressure)==0);  // Wait here for touch

    TransformTouchMap(rawX, rawY);  // Sorts out orientation and approx scaling of the touch map

    // printf("wscreen = %d, hscreen = %d, scaledX = %d, scaledY = %d\n", wscreen, hscreen, scaledX, scaledY);

    if((scaledX <= 5 * wscreen / 40)  && (scaledY <= 2 * hscreen / 12))
    {
      printf("snap\n");
      system("/home/pi/rpidatv/scripts/snap2.sh");
    }
    else
    {
      FinishedButton=1;
    }
  }
  return NULL;
}

void *WaitButtonSnap(void * arg)
{
  int rawX, rawY, rawPressure;
  int count_time_ms;
  FinishedButton = 1; // Start with Parameters on

  while((FinishedButton == 1) || (FinishedButton = 2))
  {
    while(getTouchSample(&rawX, &rawY, &rawPressure)==0);  // Wait here for touch

    TransformTouchMap(rawX, rawY);  // Sorts out orientation and approx scaling of the touch map

    if((scaledX <= 15 * wscreen / 40) && (scaledX >= wscreen / 40) && (scaledY <= hscreen) && (scaledY >= 2 * hscreen / 12))
    {
      printf("in zone\n");
      if (FinishedButton == 2)  // Toggle parameters on/off 
      {
        FinishedButton = 1; // graphics on
      }
      else
      {
        FinishedButton = 2; // graphics off
      }
    }
    else if((scaledX <= 5 * wscreen / 40)  && (scaledY <= hscreen) && (scaledY <= 2 * hscreen / 12))
    {
      printf("snap\n");
      system("/home/pi/rpidatv/scripts/snap2.sh");
    }

    else
    {
      printf("Out of zone\n");
      FinishedButton = 0;  // Not in the zone, so exit receive
      touch_response = 1;
      count_time_ms = 0;

      // wait here to make sure that touch_response is set back to 0
      // If not, restart GUI
      printf("Entering Delay\n");
      while ((touch_response == 1) && (count_time_ms < 3000))
      {
        usleep(1000);
        count_time_ms = count_time_ms + 1;
      }
      printf("Leaving Delay\n");
      if (touch_response == 1) // count_time has elapsed and still no reponse
      {
        exit(129);
      }
      return NULL;
    }
  }
  return NULL;
}


void *WaitButtonLMRX(void * arg)
{
  int rawX, rawY, rawPressure;
  int count_time_ms;
  FinishedButton = 1; // Start with Parameters on

  while((FinishedButton == 1) || (FinishedButton = 2))
  {
    while(getTouchSample(&rawX, &rawY, &rawPressure)==0);  // Wait here for touch

    TransformTouchMap(rawX, rawY);  // Sorts out orientation and approx scaling of the touch map

    if((scaledX <= 5 * wscreen / 40)  &&  (scaledY <= 2 * hscreen / 12)) // Bottom left
    {
      printf("In snap zone, so take snap.\n");
      system("/home/pi/rpidatv/scripts/snap2.sh");
    }
    else if((scaledX <= 5 * wscreen / 40)  && (scaledY >= 10 * hscreen / 12))  // Top left
    {
      printf("In restart VLC zone, so set for reset.\n");
      VLCResetRequest = true;
    }
    else if ((scaledX <= 15 * wscreen / 40) && (scaledY <= 10 * hscreen / 12) && (scaledY >= 2 * hscreen / 12))
    {
      printf("In parameter zone, so toggle parameter view.\n");
      if (FinishedButton == 2)  // Toggle parameters on/off 
      {
        FinishedButton = 1; // graphics on
      }
      else
      {
        FinishedButton = 2; // graphics off
      }
    }
    else if((scaledX >= 35 * wscreen / 40) && (scaledY >= 8 * hscreen / 12))  // Top Right
    {
      //printf("Volume Up.\n");
      AdjustVLCVolume(51);
    }
    else if((scaledX >= 35 * wscreen / 40) && (scaledY >= 4 * hscreen / 12) && (scaledY < 8 * hscreen / 12))  // mid Right
    {
      //printf("Volume Mute.\n");
      AdjustVLCVolume(-512);
    }
    else if((scaledX >= 35 * wscreen / 40) && (scaledY < 4 * hscreen / 12))  // Bottom Right
    {
      //printf("Volume Down.\n");
      AdjustVLCVolume(-51);
    }
    else
    {
      // Close VLC to reduce processor load first
      system("/home/pi/rpidatv/scripts/lmvlcclose.sh");

      // kill the receive processes
      system("sudo killall longmynd > /dev/null 2>/dev/null");
      system("sudo killall CombiTunerExpress > /dev/null 2>/dev/null");
      system("sudo killall /usr/bin/omxplayer.bin > /dev/null 2>/dev/null");  // killed last otherwise fifo will block

      printf("Receive processes stopped.\n");
      FinishedButton = 0;  // Exit receive
      touch_response = 1;
      count_time_ms = 0;

      // wait here to make sure that touch_response is set back to 0
      // If not, restart GUI
      while ((touch_response == 1) && (count_time_ms < 500))
      {
        usleep(1000);
        count_time_ms = count_time_ms + 1;
      }
      count_time_ms = 0;
      while ((touch_response == 1) && (count_time_ms < 2500))
      {
        usleep(1000);
        count_time_ms = count_time_ms + 1;
      }

      if (touch_response == 1) // count_time has elapsed and still no reponse
      {
        setBackColour(0, 0, 0);
        clearScreen();
        exit(129);                 // Restart the GUI
      }
      return NULL;
    }
  }
  return NULL;
}

void *WaitButtonStream(void * arg)
{
  int rawX, rawY, rawPressure;
  FinishedButton = 0;

  while(FinishedButton == 0)
  {
    while(getTouchSample(&rawX, &rawY, &rawPressure)==0);  // Wait here for touch

    TransformTouchMap(rawX, rawY);  // Sorts out orientation and approx scaling of the touch map

    if((scaledX <= 5 * wscreen / 40)  && (scaledY <= hscreen) && (scaledY <= 2 * hscreen / 12)) // Bottom left
    {
      printf("In snap zone, so take snap.\n");
      system("/home/pi/rpidatv/scripts/snap2.sh");
    }
    else if((scaledX >= 35 * wscreen / 40)  && (scaledY >= 6 * hscreen / 12))  // Top Right
    {
      //printf("Volume Up.\n");
      AdjustVLCVolume(51);
    }
    else if((scaledX >= 35 * wscreen / 40)  && (scaledY < 6 * hscreen / 12))  // Top Right
    {
      //printf("Volume Down.\n");
      AdjustVLCVolume(-51);
    }
    else
    {
      printf("End stream receive requested\n");
      FinishedButton = 1;
    }
  }
  return NULL;
}


int CheckStream()
{
  // first check file exists, if not, return 1
  // then read first 5 characters of file
  // if Video, return 0
  // if Audio, return 2.
  // else return 3

  FILE *fp;
  char response[127] = "";

  fp = popen("cat /home/pi/tmp/stream_status.txt 2>/dev/null", "r");
  if (fp == NULL)
  {
    //printf("Failed to run command\n" );
    return 1;
  }

  // Read the output a line at a time
  while (fgets(response, 127, fp) != NULL)
  {
    //printf("Response was %s", response);
  }
  pclose(fp);

  if (strlen(response) < 5)  // Null string or single <CR>
  {
    return 1;
  }
 
  response[5] = '\0'; // Truncate response to 5 characters

  if (strcmp(response, "Video") == 0)
  {
    return 0;
  }
  else if (strcmp(response, "Audio") == 0)
  {
    return 2;
  }
  else
  {
    return 3;
  }
}

int CheckVLCStream()
{
  // first check file exists, if not, return 1
  // then read first 5 characters of file
  // If less than 5 characters (invalid) return 1
  // if Video, return 0
  // else return 3

  FILE *fp;
  char response[127] = "";

  fp = popen("cat /home/pi/tmp/stream_status.txt 2>/dev/null", "r");
  if (fp == NULL)
  {
    //printf("Failed to run command\n" );
    return 1;
  }

  // Read the output a line at a time
  while (fgets(response, 127, fp) != NULL)
  {
    //printf("Response was %s", response);
  }
  pclose(fp);

  if (strlen(response) < 5)  // Null string or single <CR>
  {
    return 1;
  }
 
  response[5] = '\0'; // Truncate response to 5 characters

  if (strcmp(response, "Video") == 0)
  {
    return 0;
  }
  else
  {
    return 3;
  }
}


void DisplayStream(int NoButton)
{
  int NoPreset;
  int StreamStatus;
  int count;
  char startCommand[255];
  char WaitMessage[63];

  if(NoButton < 5) // bottom row
  {
    NoPreset = NoButton + 5;
  }
  else  // top row
  {
    NoPreset = NoButton - 4;
  }

  strcpy(startCommand, "/home/pi/rpidatv/scripts/omx.sh ");
  strcat(startCommand, StreamAddress[NoPreset]);
  strcat(startCommand, " &");

  strcpy(WaitMessage, "Waiting for Stream ");
  strcat(WaitMessage, StreamLabel[NoPreset]);

  printf("Starting Stream receiver ....\n");
  IQAvailable = 0;           // Set flag to prompt user reboot before transmitting
  FinishedButton = 0;
  setBackColour(0, 0, 0);
  clearScreen();
  DisplayHere(WaitMessage);

  // Create Wait Button thread
  pthread_create (&thbutton, NULL, &WaitButtonEvent, NULL);

  while (FinishedButton == 0)
  {
    // With no stream, this loop is executed about once every 10 seconds

    // first make sure that the stream status is not stale
    system("rm /home/pi/tmp/stream_status.txt >/dev/null 2>/dev/null");
    usleep(500000);

    // run the omxplayer script
 
    system(startCommand);

    StreamStatus = CheckStream();

    // = 0 Stream running
    // = 1 Not started yet
    // = 2 started but audio only
    // = 3 terminated
    
    // Now wait 10 seconds for omxplayer to respond
    // checking every 0.5 seconds.  It will time out at 5 seconds

    count = 0;
    while ((StreamStatus == 1) && (count < 20) && (FinishedButton == 0))
    {
      usleep(500000); 
      count = count + 1;
      StreamStatus = CheckStream();
    }

    // If it is running properly, wait here
    if (StreamStatus == 0)
    {
      DisplayHere("Valid Stream Detected");

      while (StreamStatus == 0 && (FinishedButton == 0))
      {
        // Wait in this loop while the stream is running
        usleep(500000); // Check every 0.5 seconds
        StreamStatus = CheckStream();
      }

      if (FinishedButton == 0)
      {
        DisplayHere("Stream Dropped Out");
        usleep(500000); // Display dropout message for 0.5 sec
      }
      else
      {
        DisplayHere(""); // Clear messages
      }
    }     

    if (StreamStatus == 2)  // Audio only
    {
      DisplayHere("Audio Stream Detected, Trying for Video");
    }

    if ((StreamStatus == 3) || (StreamStatus == 1))  // Nothing detected
    {
      DisplayHere(WaitMessage);
    }

    // Make sure that omxplayer is no longer running
    system("killall -9 omxplayer.bin >/dev/null 2>/dev/null");
  }
  DisplayHere("");
  //init(&wscreen, &hscreen);  // Restart the graphics
  pthread_join(thbutton, NULL);
}

void DisplayStreamVLC(int NoButton)
{
  int NoPreset;
  int StreamStatus;
  char startCommand[255];
  char WaitMessage[63];

  if(NoButton < 5) // bottom row
  {
    NoPreset = NoButton + 5;
  }
  else  // top row
  {
    NoPreset = NoButton - 4;
  }

  strcpy(startCommand, "/home/pi/rpidatv/scripts/vlc_stream_player.sh ");
  strcat(startCommand, StreamAddress[NoPreset]);
  strcat(startCommand, " &");

  strcpy(WaitMessage, "Waiting for Stream ");
  strcat(WaitMessage, StreamLabel[NoPreset]);

  printf("Starting Stream receiver ....\n");
  IQAvailable = 0;           // Set flag to prompt user reboot before transmitting
  FinishedButton = 0;
  setBackColour(0, 0, 0);
  clearScreen();
  DisplayHere(WaitMessage);

  // Create Wait Button thread
  pthread_create (&thbutton, NULL, &WaitButtonStream, NULL);

  // first make sure that the stream status is not stale
  system("rm /home/pi/tmp/stream_status.txt >/dev/null 2>/dev/null");
  usleep(500000);

  while (FinishedButton == 0)
  {
    // first make sure that the stream status is not stale
    //system("rm /home/pi/tmp/stream_status.txt >/dev/null 2>/dev/null");
    //usleep(500000);

    // run the VLC player script
    system(startCommand);

    StreamStatus = CheckVLCStream();
    //printf("Stream Status File value = %d\n", StreamStatus);

    // = 0 Stream running
    // = 1 Not started yet
    // = 3 terminated
    
    // Now wait for VLC to respond
    // checking every 0.5 seconds. 

    while ((StreamStatus == 1) && (FinishedButton == 0))
    {
      usleep(500000); 
      StreamStatus = CheckStream();
      //printf("Stream Status File value = %d\n", StreamStatus);
    }

    // If it is running properly, wait here
    if (StreamStatus == 0)
    {
      DisplayHere("Valid Stream Detected");

      while (StreamStatus == 0 && (FinishedButton == 0))
      {
        // Wait in this loop while the stream is running
        usleep(500000); // Check every 0.5 seconds
        StreamStatus = CheckStream();
        // printf("Stream Status File value = %d\n", StreamStatus);
      }

      if (FinishedButton == 0)
      {
        DisplayHere("Stream Dropped Out");
        usleep(500000); // Display dropout message for 0.5 sec
      }
      else
      {
        DisplayHere(""); // Clear messages
      }
    }     

    if ((StreamStatus == 3) || (StreamStatus == 1))  // Nothing detected
    {
      DisplayHere(WaitMessage);
    }

    // Make sure that VLC player is no longer running
    system("/home/pi/rpidatv/scripts/vlc_stream_player_stop.sh");
    
  }
  DisplayHere("");

  //init(&wscreen, &hscreen);  // Restart the graphics
  pthread_join(thbutton, NULL);
}

void AmendStreamPreset(int NoButton)
{
  int NoPreset;
  char Param[255];
  char Value[255];
  char TestValue[255];
  char Prompt[63];

  if(NoButton < 5) // bottom row
  {
    NoPreset = NoButton + 5;
  }
  else  // top row
  {
    NoPreset = NoButton - 4;
  }

  // Read the Preset address and ask for a new address
  strcpy(TestValue, StreamAddress[NoPreset]);  // Read the full rtmp address
  TestValue[29] = '\0';                        // Shorten to 29 characters
  if (strcmp(TestValue, "rtmp://rtmp.batc.org.uk/live/") == 0) // BATC Address
  {
    snprintf(Prompt, 62, "Enter the new lower case StreamName for Preset %d:", NoPreset);
    strcpy(Value, StreamAddress[NoPreset]);  // Read the full rtmp address

    // Copy the text to the right of character 29 (the last /)
    strncpy(TestValue, &Value[29], strlen(Value));
    TestValue[strlen(Value) - 29] = '\0';
    // Ask user to edit streamname
    Keyboard(Prompt, TestValue, 15);
    // Put the full url back together
    strcpy(Value, "rtmp://rtmp.batc.org.uk/live/");
    strcat(Value, KeyboardReturn);
    snprintf(Param, 10, "stream%d", NoPreset);
    SetConfigParam(PATH_STREAMPRESETS, Param, Value);
    strcpy(StreamAddress[NoPreset], Value);

    // Read the Preset Label and ask for a new value
    snprintf(Prompt, 62, "Enter the new label for Preset %d:", NoPreset);
    strcpy(Value, StreamLabel[NoPreset]);  // Read the old label
    Keyboard(Prompt, Value, 15);
    snprintf(Param, 16, "label%d", NoPreset);
    SetConfigParam(PATH_STREAMPRESETS, Param, KeyboardReturn);
    strcpy(StreamLabel[NoPreset], KeyboardReturn);
  }
  else  // not a BATC Address
  {
    MsgBox4("Not a BATC Streamer Address", "Please edit it directly in", "rpidatv/scripts/stream_presets.txt", "Touch Screen to Continue");
    wait_touch();
  }
  setBackColour(0, 0, 0);
  clearScreen();
}

void SelectStreamer(int NoButton)
{
  int NoPreset;
  //char Param[255];
  //char Value[255];

  // Map button numbering
  if(NoButton < 5) // bottom row
  {
    NoPreset = NoButton + 5;
  }
  else  // top row
  {
    NoPreset = NoButton - 4;
  }

  // Copy in the Streamer URL
  SetConfigParam(PATH_PCONFIG, "streamurl", StreamURL[NoPreset]);
  strcpy(StreamURL[0], StreamURL[NoPreset]);

  // Copy in Streamname-key
  SetConfigParam(PATH_PCONFIG, "streamkey", StreamKey[NoPreset]);
  strcpy(StreamKey[0], StreamKey[NoPreset]);
}

/* Test code
  SeparateStreamKey("g8gkq-abcxyz", Param, Value);
  printf("\nStream separation test **************************\n\n");
  printf("Test input = \'g8gkq-abcxyz\'\n");
  printf("Streamname Output = \'%s\'\n", Param);
  printf("Key Output = \'%s\'\n\n", Value);void SeparateStreamKey(char streamkey[127], char streamname[63], char key[63])
*/

void SeparateStreamKey(char streamkey[127], char streamname[63], char key[63])
{
  int n;
  char delimiter[1] = "-";
  int AfterDelimiter = 0;
  int keystart;
  int stringkeylength;
  strcpy(streamname, "null");
  strcpy(key, "null");

  stringkeylength = strlen(streamkey);

  for(n = 0; n < stringkeylength ; n = n + 1)  // for each character
  {
    if (AfterDelimiter == 0)                   // if streamname
    {
      streamname[n] = streamkey[n];            // copy character into streamname

      if (n == stringkeylength - 1)            // if no delimiter found
      {
        streamname[n + 1] = '\0';                  // terminate streamname to prevent overflow
      }
    }
    else
    {
      AfterDelimiter = AfterDelimiter + 1;     // if not streamname jump over delimiter
    }
    if (streamkey[n] == delimiter[0])          // if delimiter
    {
      streamname[n] = '\0';                    // end streamname
      AfterDelimiter = 1;                      // set flag
      keystart = n;                            // and note key start point
    }
    if (AfterDelimiter > 1)                    // if key
    {
      key[n - keystart - 1] = streamkey[n];    // copy character into key
    }
    if (n == stringkeylength - 2)              // if end of input string
    {
      key[n - keystart + 1] = '\0';            // end key
    }
  }
}

void AmendStreamerPreset(int NoButton)
{
  int NoPreset;
  char Param[255];
  char Value[255];
  char streamname[63];
  char key[63];
  char Prompt[63];

  // Map button numbering
  if(NoButton < 5) // bottom row
  {
    NoPreset = NoButton + 5;
  }
  else  // top row
  {
    NoPreset = NoButton - 4;
  }

  // streamurl is unchanged
  // can be changed in file manually if required

  snprintf(Param, 15, "streamkey%d", NoPreset);
  GetConfigParam(PATH_STREAMPRESETS, Param, Value);
  SeparateStreamKey(Value, streamname, key);

  sprintf(Prompt, "Enter the streamname (lower case)");
  Keyboard(Prompt, streamname, 15);
  strcpy(streamname, KeyboardReturn);

  sprintf(Prompt, "Enter the stream key (6 characters)");
  Keyboard(Prompt, key, 15);
  strcpy(key, KeyboardReturn);
  
  snprintf(Value, 127, "%s-%s", streamname, key);
  SetConfigParam(PATH_STREAMPRESETS, Param, Value);
  strcpy(StreamKey[NoPreset], Value);

  // Select this new streamer as the in-use streamer
  SetConfigParam(PATH_PCONFIG, "streamurl", StreamURL[NoPreset]);
  strcpy(StreamURL[0], StreamURL[NoPreset]);
  SetConfigParam(PATH_PCONFIG, "streamkey", StreamKey[NoPreset]);
  strcpy(StreamKey[0], StreamKey[NoPreset]);

  setBackColour(0, 0, 0);
  clearScreen();
}


void checkTunerSettings()
{
  int basefreq;

  // First check that an FTDI device or PicoTuner is connected
  if (CheckTuner() != 0)
  {
    MsgBox4("No tuner connected", "Please check that you have either", "a MiniTiouner, PicoTuner or a Knucker connected", "Touch Screen to Continue");
    wait_touch();
  }

  if (strcmp(LMRXmode, "sat") == 0)  // Correct for Sat freqs
  {
    basefreq = LMRXfreq[0] - LMRXqoffset;
  }
  else
  {
    basefreq = LMRXfreq[0];
  }

  if (strcmp(RXmod, "DVB-S") == 0)  // Check DVB-S Settings
  {
    // Frequency
    if ((basefreq < 143500 ) || (basefreq > 2650000))
    {
      MsgBox4("Receive Frequency outside normal range", "of 143.5 MHz to 2650 MHz", "Tuner may not operate", "Touch Screen to Continue");
      wait_touch();
    }
    if ((LMRXsr[0] < 66 ) || (LMRXsr[0] > 8000))
    {
      MsgBox4("Receive SR outside normal range", "of 66 kS to 8 MS", "Tuner may not operate", "Touch Screen to Continue");
      wait_touch();
    }
  }
  else                              // Check DVB-T settings
  {
    // Frequency
    if ((basefreq < 50000 ) || (basefreq > 1000000))
    {
      MsgBox4("Receive Frequency outside normal range", "of 50 MHz to 1000 MHz", "Tuner may not operate", "Touch Screen to Continue");
      wait_touch();
    }
    if ((LMRXsr[0] > 500 ) && (LMRXsr[0] != 1000) && (LMRXsr[0] != 1700) && (LMRXsr[0] != 2000) && (LMRXsr[0] != 4000)
                           && (LMRXsr[0] != 5000) && (LMRXsr[0] != 6000) && (LMRXsr[0] != 7000) && (LMRXsr[0] != 8000))
    {
      MsgBox4("Receive bandwidth outside normal range", "of < 501 kHz or 1, 1.7, 2, 4, 5, 6, 7, or 8 MHz", 
              "Tuner may not operate", "Touch Screen to Continue");
      wait_touch();
    }
  }
}


void LMRX(int NoButton)
{
  #define PATH_SCRIPT_LMRXMER "/home/pi/rpidatv/scripts/lmmer.sh 2>&1"
  #define PATH_SCRIPT_LMRXUDP "/home/pi/rpidatv/scripts/lmudp.sh 2>&1"
  #define PATH_SCRIPT_LMRXOMX "/home/pi/rpidatv/scripts/lmomx.sh 2>&1"
  #define PATH_SCRIPT_LMRXVLCFFUDP "/home/pi/rpidatv/scripts/lmvlcffudp.sh"
  #define PATH_SCRIPT_LMRXVLCFFUDP2 "/home/pi/rpidatv/scripts/lmvlcffudp2.sh"

  //Local parameters:

  FILE *fp;
  int num;
  int ret;
  int fd_status_fifo;
  int TUNEFREQ;

  float MER;
  float refMER;
  int MERcount = 0;
  float FREQ;
  int STATE = 0;
  int SR;
  char Value[63];
  bool Overlay_displayed = false;

  // To be Global Paramaters:

  char status_message_char[14];
  char stat_string[255];
  char udp_string[63];
  char MERtext[63];
  char MERNtext[63];
  char STATEtext[63];
  char FREQtext[63];
  char SRtext[63];
  char ServiceProvidertext[255] = " ";
  char Servicetext[255] = " ";

  char FECtext[63] = " ";
  char Modulationtext[63] = " ";
  char Encodingtext[63] = " ";
  char vlctext[255];
  char AGCtext[255];
  char AGC1text[255];
  char AGC2text[255];
  uint16_t AGC1 = 0;
  uint16_t AGC2 = 3200;
  float MERThreshold = 0;
  int Parameters_currently_displayed = 1;  // 1 for displayed, 0 for blank
  float previousMER = 0;
  int FirstLock = 0;  // set to 1 on first lock, and 2 after parameter fade
  clock_t LockTime;
  bool webupdate_this_time = true;   // Only update web on alternate MER changes
  char LastServiceProvidertext[255] = " ";
  bool FirstReceive = true;
  char TIMEtext[63];

  // DVB-T parameters

  char line3[31] = "";
  char line4[31] = "";
  char line5[127] = "";
  char line6[31] = "";
  char line7[31] = "";
  char line8[31] = "";
  char line9[31] = "";
  char line10[31] = "";
  char line11[31] = "";
  char line12[31] = "";
  char line13[31] = "";
  char line14[31] = "";
  char linex[127] = "";
  uint16_t TunerPollCount = 0;
  bool TunerFound = FALSE;

  char ExtraText[63];

  // Set globals
  FinishedButton = 1;
  VLCResetRequest = false;

  // Display the correct background
  if ((NoButton == 1) || (NoButton == 5))   // OMX and LNB autoset modes
  {
    strcpy(LinuxCommand, "sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/RX_Black.png ");
    strcat(LinuxCommand, ">/dev/null 2>/dev/null");
    system(LinuxCommand);
    strcpy(LinuxCommand, "(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
    system(LinuxCommand);
  }
  else if ((NoButton == 0) || (NoButton == 2))   // VLC modes
  {
    strcpy(LinuxCommand, "sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/RX_overlay.png ");
    Overlay_displayed = true;
    strcat(LinuxCommand, ">/dev/null 2>/dev/null");
    system(LinuxCommand);
    strcpy(LinuxCommand, "(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
    system(LinuxCommand);
    refreshMouseBackground();
    draw_cursor_foreground(mouse_x, mouse_y);
    UpdateWeb();
  }
  else // MER display modes
  {
    setBackColour(0, 0, 0);
    clearScreen();
  }

  // Initialise and calculate the text display
  setForeColour(255, 255, 255);    // White text
  setBackColour(0, 0, 0);          // on Black
  const font_t *font_ptr = &font_dejavu_sans_28;
  int txtht =  font_ptr->ascent;
  int txttot =  font_ptr->height;
  int txtdesc = font_ptr->height - font_ptr->ascent;
  int linepitch = (14 * txtht) / 10;

  // Initialise the MER display
  int bar_height;
  int bar_centre = hscreen * 0.30;
  int ls = wscreen * 38 / 40;
  int wdth = (wscreen * 2 / 40) - 1;

  // Create Wait Button thread
  pthread_create (&thbutton, NULL, &WaitButtonLMRX, NULL);

  // case 0 = DVB-S/S2 VLCFFUDP
  // case 1 = DVB-S/S2 OMXPlayer
  // case 2 = DVB-S/S2 VLCFFUDP2 No scan
  // case 3 = DVB-S/S2 UDP
  // case 4 = Beacon MER
  // case 5 = Autoset LNB LO Freq
  // case 6 = DVB-T VLC
  // case 7 = not used
  // case 8 = not used
  // case 9 = DVB-T UDP

  // Switch for DVB-T
  if (strcmp(RXmod, "DVB-T") == 0)
  {
    if (NoButton == 3)  // UDP
    {
      NoButton = 9;
    }
    else
    {
      NoButton = 6;   // VLC with ffmpeg
    }
  }

  switch (NoButton)
  {
  case 0:
  case 2:

    if (NoButton == 0)
    {
      fp=popen(PATH_SCRIPT_LMRXVLCFFUDP, "r");
    }
    
    if (NoButton == 2)
    {
      fp=popen(PATH_SCRIPT_LMRXVLCFFUDP2, "r");
    }
    
    if(fp==NULL) printf("Process error\n");

    printf("STARTING VLC with FFMPEG RX\n");

    /* Open status FIFO for read only  */
    ret = mkfifo("longmynd_status_fifo", 0666);
    fd_status_fifo = open("longmynd_status_fifo", O_RDONLY); 

    // Set the status fifo to be non-blocking on empty reads
    fcntl(fd_status_fifo, F_SETFL, O_NONBLOCK);

    if (fd_status_fifo < 0)
    {
      printf("Failed to open status fifo\n");
    }
    printf("Listening, ret = %d\n", ret);

    while ((FinishedButton == 1) || (FinishedButton == 2)) // 1 is captions on, 2 is off
    {
      if (VLCResetRequest == true)
      {
        system("/home/pi/rpidatv/scripts/lmvlcreset.sh &");
        VLCResetRequest = false;
        FirstLock = 2;
        FinishedButton = 1;
      }

      num = read(fd_status_fifo, status_message_char, 1);

      if (num < 0)  // no character to read
      {
        usleep(500);
        if (TunerFound == FALSE)
        {
          TunerPollCount = TunerPollCount + 1;

          if (TunerPollCount == 15)  // Tuner not responding
          {
            strcpy(line5, "Waiting for Tuner to Respond");
            Text2(wscreen * 6 / 40, hscreen - 1 * linepitch, line5, font_ptr);
          }       
          // printf("TPC = %d\n", TunerPollCount);
          if (TunerPollCount > 2500)  // Maybe PicoTuner has locked up so reset USB bus power
          {
            strcpy(line5, "Resetting USB Bus                            ");
            Text2(wscreen * 6 / 40, hscreen - 1 * linepitch, line5, font_ptr);
            system("sudo killall vlc");
            system("sudo uhubctl -R -a 2");
            MsgBox("Touch Centre of Screen to Return to Receiver");
          }
        }
      }
      else // there was a character to read
      {
        status_message_char[num]='\0';
        // if (num>0) printf("%s\n",status_message_char);
        
        if (strcmp(status_message_char, "$") == 0)
        {
          TunerFound = TRUE;

          if (Overlay_displayed == true)
          {
            clearScreen();
            Overlay_displayed = false;
          }

          if ((stat_string[0] == '1') && (stat_string[1] == ','))  // Decoder State
          {
            // Return State as an integer and a string
            STATE = LMDecoderState(stat_string, STATEtext);
          }

          if ((stat_string[0] == '6') && (stat_string[1] == ','))  // Frequency
          {
            strcpy(FREQtext, stat_string);
            chopN(FREQtext, 2);
            FREQ = atof(FREQtext);
            if (strcmp(LMRXmode, "sat") == 0)
            {
              FREQ = FREQ + LMRXqoffset;
            }
            FREQ = FREQ / 1000;
            snprintf(FREQtext, 15, "%.3f MHz", FREQ);
            if ((TabBandLO[CurrentBand] < -0.5) || (TabBandLO[CurrentBand] > 0.5))      // band not direct
            {
              strcat(FREQtext, " ");
              strcat(FREQtext, TabBandLabel[CurrentBand]);                             // so add band label
            }
          }

          if ((stat_string[0] == '9') && (stat_string[1] == ','))  // SR in S
          {
            strcpy(SRtext, stat_string);
            chopN(SRtext, 2);
            SR = atoi(SRtext) / 1000;
            snprintf(SRtext, 15, "%d kS", SR);
          }

          if ((stat_string[0] == '1') && (stat_string[1] == '3'))  // Service Provider
          {
            strcpy(ServiceProvidertext, stat_string);
            chopN(ServiceProvidertext, 3);

            // Force VLC reset if service provider has changed
            if ((strlen(ServiceProvidertext) > 1) && (strlen(LastServiceProvidertext) > 1))
            {
              if (strcmp(ServiceProvidertext, LastServiceProvidertext) != 0)  // Service provider has changed
              {
                system("/home/pi/rpidatv/scripts/lmvlcreset.sh &");           // so reset VLC
                strcpy(LastServiceProvidertext, ServiceProvidertext);
              }
            }

            // deal with first receive case (no reset required)
            if ((FirstReceive == true) && (strlen(ServiceProvidertext) > 1))
            {
              strcpy(LastServiceProvidertext, ServiceProvidertext);
              FirstReceive = false;
            }
          }

          if ((stat_string[0] == '1') && (stat_string[1] == '4'))  // Service
          {
            strcpy(Servicetext, stat_string);
            chopN(Servicetext, 3);
          }

          if ((stat_string[0] == '1') && (stat_string[1] == '8'))  // MODCOD
          {
            MERThreshold = LMLookupMODCOD(stat_string, STATE, FECtext, Modulationtext);
          }

          if ((stat_string[0] == '1') && (stat_string[1] == '7'))  // Video and audio encoding
          {
            if (LMLookupVidEncoding(stat_string, ExtraText) == 0)
            {
              strcpy(Encodingtext, ExtraText);
            }
            strcpy(ExtraText, "");

            if (LMLookupAudEncoding(stat_string, ExtraText) == 0)
            {
              strcat(Encodingtext, ExtraText);
            }
          }

          if ((stat_string[0] == '2') && (stat_string[1] == '6'))  // AGC1 Setting
          {
            strcpy(AGC1text, stat_string);
            chopN(AGC1text, 3);
            AGC1 = atoi(AGC1text);
          }

          if ((stat_string[0] == '2') && (stat_string[1] == '7'))  // AGC2 Setting
          {
            strcpy(AGC2text, stat_string);
            chopN(AGC2text, 3);
            AGC2 = atoi(AGC2text);
          }

          if ((stat_string[0] == '1') && (stat_string[1] == '2'))  // MER
          {
            if (FinishedButton == 1)  // Parameters requested to be displayed
            {

              // If they weren't displayed before, set the previousMER to 0 
              // so they get displayed and don't have to wait for an MER change
              if (Parameters_currently_displayed != 1)
              {
                previousMER = 0;
              }
              Parameters_currently_displayed = 1;
              strcpy(MERtext, stat_string);
              chopN(MERtext, 3);
              MER = atof(MERtext)/10;
              if (MER > 51)  // Trap spurious MER readings
              {
                MER = 0;
              }
              snprintf(MERtext, 24, "MER %.1f (%.1f needed)", MER, MERThreshold);
              snprintf(AGCtext, 24, "RF Input Level %d dB", CalcInputPwr(AGC1, AGC2));

              rectangle(wscreen * 1 / 40, hscreen - 1 * linepitch - txtdesc, wscreen * 30 / 40, txttot, 0, 0, 0);
              Text2(wscreen * 1 / 40, hscreen - 1 * linepitch, STATEtext, font_ptr);
              rectangle(wscreen * 1 / 40, hscreen - 2 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
              Text2(wscreen * 1 / 40, hscreen - 2 * linepitch, FREQtext, font_ptr);
              rectangle(wscreen * 1 / 40, hscreen - 3 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
              Text2(wscreen * 1 / 40, hscreen - 3 * linepitch, SRtext, font_ptr);
              rectangle(wscreen * 1 / 40, hscreen - 4 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
              Text2(wscreen * 1 / 40, hscreen - 4 * linepitch, Modulationtext, font_ptr);
              rectangle(wscreen * 1 / 40, hscreen - 5 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
              Text2(wscreen * 1 / 40, hscreen - 5 * linepitch, FECtext, font_ptr);
              rectangle(wscreen * 1 / 40, hscreen - 6 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
              Text2(wscreen * 1 / 40, hscreen - 6 * linepitch, ServiceProvidertext, font_ptr);
              rectangle(wscreen * 1 / 40, hscreen - 7 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
              Text2(wscreen * 1 / 40, hscreen - 7 * linepitch, Servicetext, font_ptr);
              rectangle(wscreen * 1 / 40, hscreen - 8 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
              Text2(wscreen * 1 / 40, hscreen - 8 * linepitch, Encodingtext, font_ptr);
              if (AGC1 < 1)  // Low input level
              {
                setForeColour(255, 63, 63); // Set foreground colour to red
              }
              rectangle(wscreen * 1 / 40, hscreen - 10 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
              Text2(wscreen * 1 / 40, hscreen - 10 * linepitch, AGCtext, font_ptr);
              setForeColour(255, 255, 255);  // Set foreground colour to white

              if (MER < MERThreshold + 0.1)
              {
                setForeColour(255, 63, 63); // Set foreground colour to red
              }
              else  // Auto-hide the parameter display after 5 seconds
              {
                if (FirstLock == 0) // This is the first time MER has exceeded threshold
                {
                  FirstLock = 1;
                  LockTime = clock();  // Set first lock time
                }
                if ((clock() > LockTime + 600000) && (FirstLock == 1))  // About 5s since first lock
                {
                  FinishedButton = 2; // Hide parameters
                  FirstLock = 2;      // and stop it trying to hide them again
                }
              }

              rectangle(wscreen * 1 / 40, hscreen - 9 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
              Text2(wscreen * 1 / 40, hscreen - 9 * linepitch, MERtext, font_ptr);
              setForeColour(255, 255, 255);  // Set foreground colour to white

              // Only change VLC overlayfile if MER has changed
              if (MER != previousMER)
              {

                // Strip trailing line feeds from text strings
                ServiceProvidertext[strlen(ServiceProvidertext) - 1] = '\0';
                Servicetext[strlen(Servicetext) - 1] = '\0';

                // Build string for VLC
                strcpy(vlctext, STATEtext);
                strcat(vlctext, "%n");
                strcat(vlctext, FREQtext);
                strcat(vlctext, "%n");
                strcat(vlctext, SRtext);
                strcat(vlctext, "%n");
                strcat(vlctext, Modulationtext);
                strcat(vlctext, "%n");
                strcat(vlctext, FECtext);
                strcat(vlctext, "%n");
                strcat(vlctext, ServiceProvidertext);
                strcat(vlctext, "%n");
                strcat(vlctext, Servicetext);
                strcat(vlctext, "%n");
                strcat(vlctext, Encodingtext);
                strcat(vlctext, "%n");
                strcat(vlctext, MERtext);
                strcat(vlctext, "%n");
                strcat(vlctext, AGCtext);
                strcat(vlctext, "%n");
                if (timeOverlay == true)
                {
                  // Retrieve the current time
                  t = time(NULL);
                  strftime(TIMEtext, sizeof(TIMEtext), "%H:%M %d %b %Y", gmtime(&t));
                  strcat(vlctext, TIMEtext);
                }
                else
                {
                  strcat(vlctext, ".");
                }
                strcat(vlctext, "%nTouch Left to Hide Overlay%nTouch Centre to Exit");

                FILE *fw=fopen("/home/pi/tmp/vlc_temp_overlay.txt","w+");
                if(fw!=0)
                {
                  fprintf(fw, "%s\n", vlctext);
                }
                fclose(fw);

                // Copy temp file to file to be read by VLC to prevent file collisions
                system("cp /home/pi/tmp/vlc_temp_overlay.txt /home/pi/tmp/vlc_overlay.txt");

                previousMER = MER;
              }

              Text2(wscreen * 1 / 40, hscreen - 11 * linepitch, "Touch Centre to exit", font_ptr);
              Text2(wscreen * 1 / 40, hscreen - 12 * linepitch, "Touch Lower left for image capture", font_ptr);
            
            }
            else
            {
              if (Parameters_currently_displayed == 1)
              {
                setBackColour(0, 0, 0);
                clearScreen();
                Parameters_currently_displayed = 0;

                FILE *fw=fopen("/home/pi/tmp/vlc_overlay.txt","w+");
                if(fw!=0)
                {
                  fprintf(fw, " ");
                }
                fclose(fw);
              }
            }
            // Update web on alternate MER changes (to reduce processor workload)
            if (webupdate_this_time == true)
            {
              refreshMouseBackground();
              draw_cursor_foreground(mouse_x, mouse_y);
              UpdateWeb();
              webupdate_this_time = false;
            }
            else
            {
              webupdate_this_time = true;
            }
          }
          stat_string[0] = '\0';
        }
        else
        {
          strcat(stat_string, status_message_char);
        }
      }
    }

    close(fd_status_fifo); 
    usleep(1000);
    pclose(fp);
    touch_response = 0; 
    break;

  case 6:
    printf("STARTING VLC with FFMPEG DVB-T RX\n");

    // Create DVB-T Receiver thread
    system("/home/pi/rpidatv/scripts/ctvlcff.sh");

    // Open status FIFO for read only
    fd_status_fifo = open("longmynd_status_fifo", O_RDONLY); 

    // Set the status fifo to be non-blocking on empty reads
    fcntl(fd_status_fifo, F_SETFL, O_NONBLOCK);

    if (fd_status_fifo < 0)  // failed to open
    {
      printf("Failed to open status fifo\n");
    }

    if (Overlay_displayed == true)
    {
      clearScreen();
      Overlay_displayed = false;
      strcpy(line5, "Waiting for a Knucker Tuner to Respond");
    }

    // Flush status message string
    stat_string[0]='\0';

    while ((FinishedButton == 1) || (FinishedButton == 2)) // 1 is captions on, 2 is off
    {
      // Read the next character from the fifo
      num = read(fd_status_fifo, status_message_char, 1);

      if (num < 0)  // no character to read
      {
        usleep(500);
        if (TunerFound == FALSE)
        {
          TunerPollCount = TunerPollCount + 1;
          if (TunerPollCount > 30)
          {
            strcpy(line5, "Knucker Tuner Not Responding");
            Text2(wscreen * 1 / 40, hscreen - 1 * linepitch, line5, font_ptr);
            TunerPollCount = 0;
          }
        }
      }
      else // there was a character to read
      {
        status_message_char[num]='\0';  // Make sure that it is a single character (when num=1)
        //printf("%s\n", status_message_char);

        if (strcmp(status_message_char, "\n") == 0)  // If end of line, process info
        {
          printf("%s\n", stat_string);  // for test

          if (strcmp(stat_string, "[GetChipId] chip id:AVL6862") == 0)
          {
            strcpy(line5, "Found Knucker Tuner");
            TunerFound = TRUE;
            Text2(wscreen * 1 / 40, hscreen - 1 * linepitch, line5, font_ptr);
          }
          else
          {
            if (strcmp(stat_string, "Tuner not found") == 0)
            {
              strcpy(line5, "Please connect a Knucker Tuner");
              Text2(wscreen * 1 / 40, hscreen - 1 * linepitch, line5, font_ptr);
            }
          }
        
          if (strcmp(stat_string, "[GetFamilyId] Family ID:0x4955") == 0)
          {
            strcpy(line5, "Initialising Tuner, Please Wait");
          }

          if (strcmp(stat_string, "[AVL_Init] ok") == 0)
          {
            strcpy(line5, "Tuner Initialised");
          }

          if ((stat_string[0] == '=') && (stat_string[5] == 'F'))  // Frequency
          {
            line3[0] = stat_string[13];
            line3[1] = stat_string[14];
            line3[2] = stat_string[15];
            line3[3] = stat_string[16];
            line3[4] = stat_string[17];
            line3[5] = stat_string[18];
            line3[6] = stat_string[19];
            line3[7] = '\0';
            strcat(line3, " MHz");
          }

          if ((stat_string[0] == '=') && (stat_string[5] == 'B'))  // Bandwidth
          {
            if (stat_string[18] != '0')
            {
              line4[0] = stat_string[18];
              line4[1] = stat_string[20];
              line4[2] = stat_string[21];
              line4[3] = stat_string[22];
              line4[4] = '\0';
            }
            else
            {
              line4[0] = stat_string[20];
              line4[1] = stat_string[21];
              line4[2] = stat_string[22];
              line4[3] = '\0';
            }
            strcat(line4, " kHz");
          }

          // Now detect start of signal search
          strcpyn(linex, stat_string, 25);  // for destructive test
          if (strcmp(linex, "[AVL_ChannelScan_Tx] Freq") == 0)
          {
            strcpy(line5, "Searching for signal");
          }

          // And detect failed search
          if (strcmp(stat_string, "[DVBTx_Channel_ScanLock_Example] DVBTx channel scan is fail,Err.") == 0)
          {
            strcpy(line5, "Search failed, resetting for another search");
          }

          // Notify signal detection (linex is already the first 25 chars of stat_string)
          if (strcmp(linex, "[AVL_LockChannel_T] Freq ") == 0)
          {
            strcpy(line5, "Signal detected, attempting to lock");
          }

          // Notify lock
          if (strcmp(stat_string, "locked") == 0)
          {
            strcpy(line5, "Signal locked");
          }

          // Notify unlocked
          if (strcmp(stat_string, "Unlocked") == 0)
          {
            strcpy(line5, "Tuner Unlocked");
            strcpy(line10, "|");
            strcpy(line11, "|");
            strcpy(line12, "|");
            strcpy(line13, "|");
            strcpy(line14, "|");
            FinishedButton = 1;
            if(FirstLock == 0)
            {
              FirstLock = 1;
            }
          }

          // Display reported modulation
          strcpyn(linex, stat_string, 6);  // for destructive test
          if (strcmp(linex, "MOD  :") == 0)
          {
            strcpy(line6, stat_string);
          }

          // Display reported FFT
          strcpyn(linex, stat_string, 6);  // for destructive test
          if (strcmp(linex, "FFT  :") == 0)
          {
            strcpy(line7, stat_string);
          }

          // Display reported constellation
          strcpyn(linex, stat_string, 6);  // for destructive test
          if (strcmp(linex, "Const:") == 0)
          {
            strcpy(line8, stat_string);
          }

          // Display reported FEC
          strcpyn(linex, stat_string, 6);  // for destructive test
          if (strcmp(linex, "FEC  :") == 0)
          {
            strcpy(line9, stat_string);
          }
          
          // Display reported Guard
          strcpyn(linex, stat_string, 6);  // for destructive test
          if (strcmp(linex, "Guard:") == 0)
          {
            strcpy(line10, stat_string);
          }

          // Display reported SSI
          strcpyn(linex, stat_string, 6);  // for destructive test
          if (strcmp(linex, "SSI is") == 0)
          {
            strcpy(line11, stat_string);
          }

          // Display reported SQI
          strcpyn(linex, stat_string, 6);  // for destructive test
          if (strcmp(linex, "SQI is") == 0)
          {
            strcpy(line12, stat_string);
          }

          // Display reported SNR
          strcpyn(linex, stat_string, 6);  // for destructive test
          if (strcmp(linex, "SNR is") == 0)
          {
            strcpy(line13, stat_string);
          }

          // Display reported PER
          strcpyn(linex, stat_string, 6);  // for destructive test
          if (strcmp(linex, "PER is") == 0)
          {
            strcpy(line14, stat_string);
            strcpy(line5, "|");            // Clear any old text from line 5
          }

          stat_string[0] = '\0';   // Finished processing this info, so clear the stat_string

          if (FinishedButton == 1)  // Parameters requested to be displayed
          {

            Parameters_currently_displayed = 1;

            rectangle(wscreen * 1 / 40, hscreen - 1 * linepitch - txtdesc, wscreen * 39 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 1 * linepitch, line5, font_ptr);
            rectangle(wscreen * 1 / 40, hscreen - 2 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 2 * linepitch, line3, font_ptr);
            Text2(wscreen * 14 / 40, hscreen - 2 * linepitch, line4, font_ptr);
            rectangle(wscreen * 1 / 40, hscreen - 3 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 3 * linepitch, line6, font_ptr);
            Text2(wscreen * 14 / 40, hscreen - 3 * linepitch, line7, font_ptr);
            rectangle(wscreen * 1 / 40, hscreen - 4 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 4 * linepitch, line8, font_ptr);
            Text2(wscreen * 14 / 40, hscreen - 4 * linepitch, line9, font_ptr);
            rectangle(wscreen * 1 / 40, hscreen - 5 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 5 * linepitch, line10, font_ptr);
            rectangle(wscreen * 1 / 40, hscreen - 6 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 6 * linepitch, line11, font_ptr);
            rectangle(wscreen * 1 / 40, hscreen - 7 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 7 * linepitch, line12, font_ptr);
            rectangle(wscreen * 1 / 40, hscreen - 8 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 8 * linepitch, line13, font_ptr);

            if (strlen(line14) > 2)  // Locked, so Auto-hide the parameter display after 5 seconds
            {
              if (FirstLock == 0) // This is the first time MER has exceeded threshold
              {
                FirstLock = 1;
                LockTime = clock();  // Set first lock time
              }
              if ((clock() > LockTime + 600000) && (FirstLock == 1))  // About 5s since first lock
              {
                FinishedButton = 2; // Hide parameters
                FirstLock = 2;      // and stop it trying to hide them again
              }
            }

            rectangle(wscreen * 1 / 40, hscreen - 9 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 9 * linepitch, line14, font_ptr);

            // Only change VLC overlayfile if Display has changed
            if (TRUE)
            {
              // Build string for VLC
              strcpy(vlctext, line5);
              strcat(vlctext, "%n");
              strcat(vlctext, line3);
              strcat(vlctext, ",  ");
              strcat(vlctext, line4);
              strcat(vlctext, "%n");
              strcat(vlctext, line6);
              strcat(vlctext, ",  ");
              strcat(vlctext, line7);
              strcat(vlctext, "%n");
              strcat(vlctext, line8);
              strcat(vlctext, ",  ");
              strcat(vlctext, line9);
              strcat(vlctext, "%n");
              strcat(vlctext, line10);
              strcat(vlctext, "%n");
              strcat(vlctext, line11);
              strcat(vlctext, "%n");
              strcat(vlctext, line12);
              strcat(vlctext, "%n");
              strcat(vlctext, line13);
              strcat(vlctext, "%n");
              strcat(vlctext, line14);
              strcat(vlctext, "%n.%nTouch Left to Hide Overlay%nTouch Centre to Exit");

              FILE *fw=fopen("/home/pi/tmp/vlc_temp_overlay.txt","w+");
              if(fw!=0)
              {
                fprintf(fw, "%s\n", vlctext);
              }
              fclose(fw);

              // Copy temp file to file to be read by VLC to prevent file collisions
              system("cp /home/pi/tmp/vlc_temp_overlay.txt /home/pi/tmp/vlc_overlay.txt");

            }
            Text2(wscreen * 1 / 40, hscreen - 11 * linepitch, "Touch Centre to exit", font_ptr);
            Text2(wscreen * 1 / 40, hscreen - 12 * linepitch, "Touch Lower left for image capture", font_ptr);
          }
          else
          {
            if (Parameters_currently_displayed == 1)
            {
              setBackColour(0, 0, 0);
              clearScreen();
              Parameters_currently_displayed = 0;

              FILE *fw=fopen("/home/pi/tmp/vlc_overlay.txt","w+");
              if(fw!=0)
              {
                fprintf(fw, " ");
              }
              fclose(fw);
            }
          }
        }
        else
        {
          strcat(stat_string, status_message_char);  // Not end of line, so append the character to the stat string
        }
      }
    }
    
    // Shutdown VLC if it has not stolen the graphics
    system("/home/pi/rpidatv/scripts/lmvlcsd.sh &");

    close(fd_status_fifo); 
    usleep(1000);

    printf("Stopping receive process\n");
    system("sudo killall /home/pi/rpidatv/scripts/ctvlcff.sh >/dev/null 2>/dev/null");
    system("/home/pi/rpidatv/scripts/lmstop.sh");
    touch_response = 0; 
    printf("Stopped receive process\n");
    break;

  case 9:
    printf("STARTING UDP Output DVB-T RX\n");

    // Create DVB-T Receiver thread
    system("/home/pi/rpidatv/scripts/ctudp.sh");

    // Open status FIFO for read only
    fd_status_fifo = open("longmynd_status_fifo", O_RDONLY); 


    // Set the status fifo to be non-blocking on empty reads
    fcntl(fd_status_fifo, F_SETFL, O_NONBLOCK);

    if (fd_status_fifo < 0)  // failed to open
    {
      printf("Failed to open status fifo\n");
    }

    // Flush status message string
    stat_string[0]='\0';

    while ((FinishedButton == 1) || (FinishedButton == 2)) // 1 is captions on, 2 is off
    {
      // Read the next character from the fifo
      num = read(fd_status_fifo, status_message_char, 1);

      if (num < 0)  // no character to read
      {
        usleep(500);
        if (TunerFound == FALSE)
        {
          TunerPollCount = TunerPollCount + 1;
          if (TunerPollCount > 30)
          {
            strcpy(line5, "Knucker Tuner Not Responding");
            Text2(wscreen * 1 / 40, hscreen - 1 * linepitch, line5, font_ptr);
            TunerPollCount = 0;
          }
        }
      }
      else // there was a character to read
      {
        status_message_char[num]='\0';  // Make sure that it is a single character (when num=1)
        //printf("%s\n", status_message_char);

        if (strcmp(status_message_char, "\n") == 0)  // If end of line, process info
        {
          printf("%s\n", stat_string);  // for test

          if (strcmp(stat_string, "[GetChipId] chip id:AVL6862") == 0)
          {
            strcpy(line5, "Found Knucker Tuner");
            TunerFound = TRUE;
          }
          else
          {
            if (strcmp(stat_string, "Tuner not found") == 0)
            {
              strcpy(line5, "Please connect a Knucker Tuner");
            }
          }
        
          if (strcmp(stat_string, "[GetFamilyId] Family ID:0x4955") == 0)
          {
            strcpy(line5, "Initialising Tuner, Please Wait");
          }

          if (strcmp(stat_string, "[AVL_Init] ok") == 0)
          {
            strcpy(line5, "Tuner Initialised");
          }

          if ((stat_string[0] == '=') && (stat_string[5] == 'F'))  // Frequency
          {
            line3[0] = stat_string[13];
            line3[1] = stat_string[14];
            line3[2] = stat_string[15];
            line3[3] = stat_string[16];
            line3[4] = stat_string[17];
            line3[5] = stat_string[18];
            line3[6] = stat_string[19];
            line3[7] = '\0';
            strcat(line3, " MHz");
          }

          if ((stat_string[0] == '=') && (stat_string[5] == 'B'))  // Bandwidth
          {
            if (stat_string[18] != '0')
            {
              line4[0] = stat_string[18];
              line4[1] = stat_string[20];
              line4[2] = stat_string[21];
              line4[3] = stat_string[22];
              line4[4] = '\0';
            }
            else
            {
              line4[0] = stat_string[20];
              line4[1] = stat_string[21];
              line4[2] = stat_string[22];
              line4[3] = '\0';
            }
            strcat(line4, " kHz");
          }

          // Now detect start of signal search
          strcpyn(linex, stat_string, 25);  // for destructive test
          if (strcmp(linex, "[AVL_ChannelScan_Tx] Freq") == 0)
          {
            strcpy(line5, "Searching for signal");
          }

          // And detect failed search
          if (strcmp(stat_string, "[DVBTx_Channel_ScanLock_Example] DVBTx channel scan is fail,Err.") == 0)
          {
            strcpy(line5, "Search failed, resetting for another search");
          }

          // Notify signal detection (linex is already the first 25 chars of stat_string)
          if (strcmp(linex, "[AVL_LockChannel_T] Freq ") == 0)
          {
            strcpy(line5, "Signal detected, attempting to lock");
          }

          // Notify lock
          if (strcmp(stat_string, "locked") == 0)
          {
            strcpy(line5, "Signal locked");
          }

          // Notify unlocked
          if (strcmp(stat_string, "Unlocked") == 0)
          {
            strcpy(line5, "Tuner Unlocked");
            strcpy(line10, "|");
            strcpy(line11, "|");
            strcpy(line12, "|");
            strcpy(line13, "|");
            strcpy(line14, "|");
            FinishedButton = 1;
            if(FirstLock == 0)
            {
              FirstLock = 1;
            }
          }

          // Display reported modulation
          strcpyn(linex, stat_string, 6);  // for destructive test
          if (strcmp(linex, "MOD  :") == 0)
          {
            strcpy(line6, stat_string);
          }

          // Display reported FFT
          strcpyn(linex, stat_string, 6);  // for destructive test
          if (strcmp(linex, "FFT  :") == 0)
          {
            strcpy(line7, stat_string);
          }

          // Display reported constellation
          strcpyn(linex, stat_string, 6);  // for destructive test
          if (strcmp(linex, "Const:") == 0)
          {
            strcpy(line8, stat_string);
          }

          // Display reported FEC
          strcpyn(linex, stat_string, 6);  // for destructive test
          if (strcmp(linex, "FEC  :") == 0)
          {
            strcpy(line9, stat_string);
          }
          
          // Display reported Guard
          strcpyn(linex, stat_string, 6);  // for destructive test
          if (strcmp(linex, "Guard:") == 0)
          {
            strcpy(line10, stat_string);
          }

          // Display reported SSI
          strcpyn(linex, stat_string, 6);  // for destructive test
          if (strcmp(linex, "SSI is") == 0)
          {
            strcpy(line11, stat_string);
          }

          // Display reported SQI
          strcpyn(linex, stat_string, 6);  // for destructive test
          if (strcmp(linex, "SQI is") == 0)
          {
            strcpy(line12, stat_string);
          }

          // Display reported SNR
          strcpyn(linex, stat_string, 6);  // for destructive test
          if (strcmp(linex, "SNR is") == 0)
          {
            strcpy(line13, stat_string);
          }

          // Display reported PER
          strcpyn(linex, stat_string, 6);  // for destructive test
          if (strcmp(linex, "PER is") == 0)
          {
            strcpy(line14, stat_string);
            strcpy(line5, "|");            // Clear any old text from line 5
          }

          stat_string[0] = '\0';   // Finished processing this info, so clear the stat_string

          if (FinishedButton == 1)  // Parameters requested to be displayed
          {

            Parameters_currently_displayed = 1;

            rectangle(wscreen * 1 / 40, hscreen - 1 * linepitch - txtdesc, wscreen * 39 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 1 * linepitch, line5, font_ptr);
            rectangle(wscreen * 1 / 40, hscreen - 2 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 2 * linepitch, line3, font_ptr);
            Text2(wscreen * 14 / 40, hscreen - 2 * linepitch, line4, font_ptr);
            rectangle(wscreen * 1 / 40, hscreen - 3 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 3 * linepitch, line6, font_ptr);
            Text2(wscreen * 14 / 40, hscreen - 3 * linepitch, line7, font_ptr);
            rectangle(wscreen * 1 / 40, hscreen - 4 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 4 * linepitch, line8, font_ptr);
            Text2(wscreen * 14 / 40, hscreen - 4 * linepitch, line9, font_ptr);
            rectangle(wscreen * 1 / 40, hscreen - 5 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 5 * linepitch, line10, font_ptr);
            rectangle(wscreen * 1 / 40, hscreen - 6 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 6 * linepitch, line11, font_ptr);
            rectangle(wscreen * 1 / 40, hscreen - 7 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 7 * linepitch, line12, font_ptr);
            rectangle(wscreen * 1 / 40, hscreen - 8 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 8 * linepitch, line13, font_ptr);
            rectangle(wscreen * 1 / 40, hscreen - 9 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 9 * linepitch, line14, font_ptr);

            Text2(wscreen * 1 / 40, hscreen - 11 * linepitch, "UDP Output", font_ptr);
            Text2(wscreen * 1 / 40, hscreen - 12 * linepitch, "Touch Centre to Exit", font_ptr);
          }
          else
          {
            if (Parameters_currently_displayed == 1)
            {
              setBackColour(0, 0, 0);
              clearScreen();
              Parameters_currently_displayed = 0;
            }
          }
        }
        else
        {
          strcat(stat_string, status_message_char);  // Not end of line, so append the character to the stat string
        }
      }
    }
    
    close(fd_status_fifo); 
    usleep(1000);

    printf("Stopping receive process\n");
    system("sudo killall /home/pi/rpidatv/scripts/ctudp.sh >/dev/null 2>/dev/null");
    system("/home/pi/rpidatv/scripts/lmstop.sh");
    touch_response = 0; 
    printf("Stopped receive process\n");
    break;

  case 1:
    fp=popen(PATH_SCRIPT_LMRXOMX, "r");
    if(fp==NULL) printf("Process error\n");

    printf("STARTING omxplayer RX\n");

    /* Open status FIFO for read only  */
    ret=mkfifo("longmynd_status_fifo", 0666);
    fd_status_fifo = open("longmynd_status_fifo", O_RDONLY); 
    if (fd_status_fifo < 0)
    {
      printf("Failed to open status fifo\n");
    }

    printf("Listening\n");

    while ((FinishedButton == 1) || (FinishedButton == 2)) 
    {
      num = read(fd_status_fifo, status_message_char, 1);
      // printf("%s Num= %d \n", "End Read", num);
      if (num >= 0 )
      {
        status_message_char[num]='\0';
        //if (num>0) printf("%s\n",status_message_char);
        
        if (strcmp(status_message_char, "$") == 0)
        {

          if ((stat_string[0] == '1') && (stat_string[1] == ','))  // Decoder State
          {
            // Return State as an integer and a string
            STATE = LMDecoderState(stat_string, STATEtext);
          }

          if ((stat_string[0] == '6') && (stat_string[1] == ','))  // Frequency
          {
            strcpy(FREQtext, stat_string);
            chopN(FREQtext, 2);
            FREQ = atof(FREQtext);
            if (strcmp(LMRXmode, "sat") == 0)
            {
              FREQ = FREQ + LMRXqoffset;
            }
            FREQ = FREQ / 1000;
            snprintf(FREQtext, 15, "%.3f MHz", FREQ);
          }

          if ((stat_string[0] == '9') && (stat_string[1] == ','))  // SR in S
          {
            strcpy(SRtext, stat_string);
            chopN(SRtext, 2);
            SR = atoi(SRtext) / 1000;
            snprintf(SRtext, 15, "%d kS", SR);
          }

          if ((stat_string[0] == '1') && (stat_string[1] == '3'))  // Service Provider
          {
            strcpy(ServiceProvidertext, stat_string);
            chopN(ServiceProvidertext, 3);
          }

          if ((stat_string[0] == '1') && (stat_string[1] == '4'))  // Service
          {
            strcpy(Servicetext, stat_string);
            chopN(Servicetext, 3);
          }

          if ((stat_string[0] == '1') && (stat_string[1] == '8'))  // MODCOD
          {
            MERThreshold = LMLookupMODCOD(stat_string, STATE, FECtext, Modulationtext);
          }

          if ((stat_string[0] == '1') && (stat_string[1] == '7'))  // Video and audio encoding
          {
            if (LMLookupVidEncoding(stat_string, ExtraText) == 0)
            {
              strcpy(Encodingtext, ExtraText);
            }
            strcpy(ExtraText, "");

            if (LMLookupAudEncoding(stat_string, ExtraText) == 0)
            {
              strcat(Encodingtext, ExtraText);
            }
          }

          if ((stat_string[0] == '2') && (stat_string[1] == '6'))  // AGC1 Setting
          {
            strcpy(AGC1text, stat_string);
            chopN(AGC1text, 3);
            AGC1 = atoi(AGC1text);
          }

          if ((stat_string[0] == '2') && (stat_string[1] == '7'))  // AGC2 Setting
          {
            strcpy(AGC2text, stat_string);
            chopN(AGC2text, 3);
            AGC2 = atoi(AGC2text);
          }

          if ((stat_string[0] == '1') && (stat_string[1] == '2'))  // MER
          {
            if (FinishedButton == 1)  // Parameters displayed
            {
              Parameters_currently_displayed = 1;
              strcpy(MERtext, stat_string);
              chopN(MERtext, 3);
              MER = atof(MERtext)/10;
              if (MER > 51)  // Trap spurious MER readings
              {
                MER = 0;
              }
              snprintf(MERtext, 24, "MER %.1f (%.1f needed)", MER, MERThreshold);
              snprintf(AGCtext, 24, "RF Input Level %d dB", CalcInputPwr(AGC1, AGC2));

              rectangle(wscreen * 1 / 40, hscreen - 1 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
              Text2(wscreen * 1 / 40, hscreen - 1 * linepitch, STATEtext, font_ptr);
              rectangle(wscreen * 1 / 40, hscreen - 2 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
              Text2(wscreen * 1 / 40, hscreen - 2 * linepitch, FREQtext, font_ptr);
              rectangle(wscreen * 1 / 40, hscreen - 3 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
              Text2(wscreen * 1 / 40, hscreen - 3 * linepitch, SRtext, font_ptr);
              rectangle(wscreen * 1 / 40, hscreen - 4 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
              Text2(wscreen * 1 / 40, hscreen - 4 * linepitch, Modulationtext, font_ptr);
              rectangle(wscreen * 1 / 40, hscreen - 5 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
              Text2(wscreen * 1 / 40, hscreen - 5 * linepitch, FECtext, font_ptr);
              rectangle(wscreen * 1 / 40, hscreen - 6 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
              Text2(wscreen * 1 / 40, hscreen - 6 * linepitch, ServiceProvidertext, font_ptr);
              rectangle(wscreen * 1 / 40, hscreen - 7 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
              Text2(wscreen * 1 / 40, hscreen - 7 * linepitch, Servicetext, font_ptr);
              rectangle(wscreen * 1 / 40, hscreen - 8 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
              Text2(wscreen * 1 / 40, hscreen - 8 * linepitch, Encodingtext, font_ptr);
              if (AGC1 < 1)  // Low input level
              {
                setForeColour(255, 63, 63); // Set foreground colour to red
              }
              rectangle(wscreen * 1 / 40, hscreen - 10 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
              Text2(wscreen * 1 / 40, hscreen - 10 * linepitch, AGCtext, font_ptr);
              setForeColour(255, 255, 255);  // Set foreground colour to white
              if (MER < MERThreshold + 0.1)
              {
                setForeColour(255, 63, 63); // Set foreground colour to red
              }
              rectangle(wscreen * 1 / 40, hscreen - 9 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
              Text2(wscreen * 1 / 40, hscreen - 9 * linepitch, MERtext, font_ptr);
              setForeColour(255, 255, 255);  // Set foreground colour to white
              Text2(wscreen * 1 / 40, hscreen - 11 * linepitch, "Touch Centre to exit", font_ptr);
              Text2(wscreen * 1 / 40, hscreen - 12 * linepitch, "Touch Lower left for image capture", font_ptr);
            }
            else
            {
              if (Parameters_currently_displayed == 1)
              {
                setBackColour(0, 0, 0);
                clearScreen();
                Parameters_currently_displayed = 0;
              }
            }
          }
          stat_string[0] = '\0';
        }
        else
        {
          strcat(stat_string, status_message_char);
        }
      }
      else
      {
        FinishedButton = 0;
      }
    } 
    close(fd_status_fifo); 
    usleep(1000);

    printf("Stopping receive process\n");
    pclose(fp);
    system("sudo killall lmomx.sh >/dev/null 2>/dev/null");
    touch_response = 0; 
    break;

  case 3:  // DVB-S/S2 UDP Output
    snprintf(udp_string, 63, "UDP Output to %s:%s", LMRXudpip, LMRXudpport);
    fp=popen(PATH_SCRIPT_LMRXUDP, "r");
    if(fp==NULL) printf("Process error\n");

    printf("STARTING UDP Player RX\n");

    // Open status FIFO
    ret = mkfifo("longmynd_status_fifo", 0666);
    fd_status_fifo = open("longmynd_status_fifo", O_RDONLY); 
    if (fd_status_fifo < 0)
    {
      printf("Failed to open status fifo\n");
    }

    printf("FinishedButton = %d\n", FinishedButton);

    while ((FinishedButton == 1) || (FinishedButton == 2)) // 1 is captions on, 2 is off
    {
      // printf("%s", "Start Read\n");

      num = read(fd_status_fifo, status_message_char, 1);
      // printf("%s Num= %d \n", "End Read", num);
      if (num >= 0 )
      {
        status_message_char[num]='\0';
        //if (num>0) printf("%s\n",status_message_char);
 
        if (strcmp(status_message_char, "$") == 0)
        {

          if ((stat_string[0] == '1') && (stat_string[1] == ','))  // Decoder State
          {
            // Return State as an integer and a string
            STATE = LMDecoderState(stat_string, STATEtext);
          }

          if ((stat_string[0] == '6') && (stat_string[1] == ','))  // Frequency
          {
            strcpy(FREQtext, stat_string);
            chopN(FREQtext, 2);
            FREQ = atof(FREQtext);
            if (strcmp(LMRXmode, "sat") == 0)
            {
              FREQ = FREQ + LMRXqoffset;
            }
            FREQ = FREQ / 1000;
            snprintf(FREQtext, 15, "%.3f MHz", FREQ);
          }

          if ((stat_string[0] == '9') && (stat_string[1] == ','))  // SR in S
          {
            strcpy(SRtext, stat_string);
            chopN(SRtext, 2);
            SR = atoi(SRtext) / 1000;
            snprintf(SRtext, 15, "%d kS", SR);
          }

          if ((stat_string[0] == '1') && (stat_string[1] == '3'))  // Service Provider
          {
            strcpy(ServiceProvidertext, stat_string);
            chopN(ServiceProvidertext, 3);
          }

          if ((stat_string[0] == '1') && (stat_string[1] == '4'))  // Service
          {
            strcpy(Servicetext, stat_string);
            chopN(Servicetext, 3);
          }

          if ((stat_string[0] == '1') && (stat_string[1] == '8'))  // MODCOD
          {
            MERThreshold = LMLookupMODCOD(stat_string, STATE, FECtext, Modulationtext);
          }

          if ((stat_string[0] == '1') && (stat_string[1] == '7'))  // Video and audio encoding
          {
            if (LMLookupVidEncoding(stat_string, ExtraText) == 0)
            {
              strcpy(Encodingtext, ExtraText);
            }
            strcpy(ExtraText, "");

            if (LMLookupAudEncoding(stat_string, ExtraText) == 0)
            {
              strcat(Encodingtext, ExtraText);
            }
          }

          if ((stat_string[0] == '2') && (stat_string[1] == '6'))  // AGC1 Setting
          {
            strcpy(AGC1text, stat_string);
            chopN(AGC1text, 3);
            AGC1 = atoi(AGC1text);
          }

          if ((stat_string[0] == '2') && (stat_string[1] == '7'))  // AGC2 Setting
          {
            strcpy(AGC2text, stat_string);
            chopN(AGC2text, 3);
            AGC2 = atoi(AGC2text);
          }

          if ((stat_string[0] == '1') && (stat_string[1] == '2'))  // MER
          {
            strcpy(MERtext, stat_string);
            chopN(MERtext, 3);
            MER = atof(MERtext)/10;

            if (MER > 51)  // Trap spurious MER readings
            {
              MER = 0;
              strcpy(MERNtext, " ");
            }
            else if (MER >= 10)
            {
              snprintf(MERNtext, 10, "%.1f", MER);
            }
            else
            {
              snprintf(MERNtext, 10, "%.1f  ", MER);
            }
            snprintf(MERtext, 24, "MER %.1f (%.1f needed)", MER, MERThreshold);
            snprintf(AGCtext, 24, "RF Input Level %d dB", CalcInputPwr(AGC1, AGC2));

            rectangle(wscreen * 1 / 40, hscreen - 1 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 1 * linepitch, STATEtext, font_ptr);
            rectangle(wscreen * 1 / 40, hscreen - 2 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 2 * linepitch, FREQtext, font_ptr);
            rectangle(wscreen * 1 / 40, hscreen - 3 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 3 * linepitch, SRtext, font_ptr);
            rectangle(wscreen * 1 / 40, hscreen - 4 * linepitch - txtdesc, wscreen * 17 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 4 * linepitch, Modulationtext, font_ptr);
            rectangle(wscreen * 1 / 40, hscreen - 5 * linepitch - txtdesc, wscreen * 17 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 5 * linepitch, FECtext, font_ptr);
            rectangle(wscreen * 1 / 40, hscreen - 6 * linepitch - txtdesc, wscreen * 17 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 6 * linepitch, ServiceProvidertext, font_ptr);
            rectangle(wscreen * 1 / 40, hscreen - 7 * linepitch - txtdesc, wscreen * 17 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 7 * linepitch, Servicetext, font_ptr);
            rectangle(wscreen * 1 / 40, hscreen - 8 * linepitch - txtdesc, wscreen * 17 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 8 * linepitch, Encodingtext, font_ptr);
            if (AGC1 < 1)  // Low input level
            {
              setForeColour(255, 63, 63); // Set foreground colour to red
            }
            rectangle(wscreen * 1 / 40, hscreen - 10 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 10 * linepitch, AGCtext, font_ptr);
            setForeColour(255, 255, 255);  // Set foreground colour to white
            if (MER < MERThreshold + 0.1)
            {
              setForeColour(255, 63, 63); // Set foreground colour to red
            }
            rectangle(wscreen * 1 / 40, hscreen - 9 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 9 * linepitch, MERtext, font_ptr);
            setForeColour(255, 255, 255);  // Set foreground colour to white
            Text2(wscreen * 1 / 40, hscreen - 11 * linepitch, udp_string, font_ptr);
            Text2(wscreen * 1 / 40, hscreen - 12 * linepitch, "Touch Centre to Exit", font_ptr);

            // Display large MER number
            LargeText2(wscreen * 18 / 40, hscreen * 19 / 48, 5, MERNtext, &font_dejavu_sans_32);
            refreshMouseBackground();
            draw_cursor_foreground(mouse_x, mouse_y);
            UpdateWeb();
          }
          stat_string[0] = '\0';
        }
        else
        {
          strcat(stat_string, status_message_char);
        }
      }
      else
      {
        FinishedButton = 0;
      }
    } 
    close(fd_status_fifo); 
    usleep(1000);

    printf("Stopping receive process\n");
    pclose(fp);

    system("sudo killall lmudp.sh >/dev/null 2>/dev/null");
    touch_response = 0; 
    break;

  case 4:  // MER Display
    snprintf(udp_string, 63, "UDP Output to %s:%s", LMRXudpip, LMRXudpport);
    fp=popen(PATH_SCRIPT_LMRXMER, "r");
    if(fp==NULL) printf("Process error\n");

    printf("STARTING MER Display\n");

    // Open status FIFO
    ret = mkfifo("longmynd_status_fifo", 0666);
    fd_status_fifo = open("longmynd_status_fifo", O_RDONLY); 
    if (fd_status_fifo < 0)
    {
      printf("Failed to open status fifo\n");
    }

    while ((FinishedButton == 1) || (FinishedButton == 2)) // 1 is captions on, 2 is off
    {
      num = read(fd_status_fifo, status_message_char, 1);

      if (num >= 0 )
      {
        status_message_char[num]='\0';
        if (strcmp(status_message_char, "$") == 0)
        {

          if ((stat_string[0] == '1') && (stat_string[1] == ','))  // Decoder State
          {
            // Return State as an integer and a string
            STATE = LMDecoderState(stat_string, STATEtext);
            strcat(STATEtext, " MER:");
          }

          if ((stat_string[0] == '2') && (stat_string[1] == '6'))  // AGC1 Setting
          {
            strcpy(AGC1text, stat_string);
            chopN(AGC1text, 3);
            AGC1 = atoi(AGC1text);
          }

          if ((stat_string[0] == '2') && (stat_string[1] == '7'))  // AGC2 Setting
          {
            strcpy(AGC2text, stat_string);
            chopN(AGC2text, 3);
            AGC2 = atoi(AGC2text);
          }

          if ((stat_string[0] == '1') && (stat_string[1] == '2'))  // MER
          {
            strcpy(MERtext, stat_string);
            chopN(MERtext, 3);
            MER = atof(MERtext)/10;
            if (MER > 51)  // Trap spurious MER readings
            {
              MER = 0;
              strcpy(MERtext, " ");
            }
            else if (MER >= 10)
            {
              snprintf(MERtext, 10, "%.1f", MER);
            }
            else
            {
              snprintf(MERtext, 10, "%.1f  ", MER);
            }

            snprintf(AGCtext, 24, "RF Input Level %d dB", CalcInputPwr(AGC1, AGC2));

            // Set up for the tuning bar

            if ((MER > 0.2) && (MERcount < 9))  // Wait for a valid MER
            {
              MERcount = MERcount + 1;
            }
            if (MERcount == 9)                 // Third valid MER
            {
              refMER = MER;
              MERcount = 10;
            }

            if (AGC1 < 1)  // Low input level
            {
              setForeColour(255, 63, 63); // Set foreground colour to red
            }
            rectangle(wscreen * 1 / 40, hscreen - 11 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 11 * linepitch, AGCtext, font_ptr);
            setForeColour(255, 255, 255);  // Set foreground colour to white

            rectangle(wscreen * 1 / 40, hscreen - 1 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 1 * linepitch, STATEtext, font_ptr);
            rectangle(wscreen * 1 / 40, hscreen - 9 * linepitch - txtdesc, 470, 8, 0, 0, 0);  // line at base of text
            rectangle(490, hscreen - 9 * linepitch - txtdesc, 160, 230, 0, 0, 0);             // poss 3rd digit
            LargeText2(wscreen * 1 / 40, hscreen - 9 * linepitch, 4, MERtext, &font_dejavu_sans_72);
            Text2(wscreen * 1 / 40, hscreen - 12 * linepitch, "Touch Centre to Exit", font_ptr);

            if (MERcount == 10)
            {
              bar_height = hscreen * (MER - refMER) / 8; // zero is reference
              // valid pixels are 1 to wscreen (800) and 1 to hscreen (480)

              rectangle(ls - wdth, bar_centre -1, wdth, 2, 255, 255, 255); // White Reference line
              if (MER > refMER)  // Green rectangle
              {
                if ((bar_centre + bar_height ) > hscreen)  // off the top
                {
                  bar_height = hscreen - bar_centre;
                }
                rectangle(ls, bar_centre, wdth, bar_height, 0, 255, 0); // Green bar
                rectangle(ls, bar_centre + bar_height, wdth, hscreen - bar_centre - bar_height, 0, 0, 0); // Black above green
                rectangle(ls, 1, wdth, bar_centre, 0, 0, 0); // Black below centre
              }
              else              // Red rectangle
              {
                if ((bar_centre + bar_height ) < 1)  // off the bottom
                {
                  bar_height = 1 - bar_centre;
                }
                rectangle(ls, bar_centre + bar_height, wdth, 0 - bar_height, 255, 0, 0); // Red bar
                rectangle(ls, 1, wdth, bar_centre + bar_height, 0, 0, 0);  // Black below red
                rectangle(ls, bar_centre, wdth, hscreen - bar_centre, 0, 0, 0); // Black above centre
              }
            }
            refreshMouseBackground();
            draw_cursor_foreground(mouse_x, mouse_y);
            UpdateWeb();
          }
          stat_string[0] = '\0';
        }
        else
        {
          strcat(stat_string, status_message_char);
        }
      }
      else
      {
        FinishedButton = 0;
      }
    } 
    close(fd_status_fifo); 
    usleep(1000);

    printf("Stopping receive process\n");
    pclose(fp);

    system("sudo killall lmmer.sh >/dev/null 2>/dev/null");
    touch_response = 0; 
    break;

  case 5:  // AutoSet LNB Frequency

    setBackColour(0, 0, 0);
    clearScreen();
    fp=popen(PATH_SCRIPT_LMRXMER, "r");
    if(fp==NULL) printf("Process error\n");

    printf("STARTING Autoset LNB LO Freq\n");
    int oldLMRXqoffset = LMRXqoffset;
    LMRXqoffset = 0;

    // Open status FIFO
    ret = mkfifo("longmynd_status_fifo", 0666);
    fd_status_fifo = open("longmynd_status_fifo", O_RDONLY); 
    if (fd_status_fifo < 0)
    {
      printf("Failed to open status fifo\n");
    }
    while ((FinishedButton == 1) || (FinishedButton == 2)) // 1 is captions on, 2 is off
    {
      num = read(fd_status_fifo, status_message_char, 1);
      if (num >= 0 )
      {
        status_message_char[num]='\0';
        if (strcmp(status_message_char, "$") == 0)
        {

          if ((stat_string[0] == '1') && (stat_string[1] == ','))  // Decoder State
          {
            // Return State as an integer and a string
            STATE = LMDecoderState(stat_string, STATEtext);
          }

          if ((stat_string[0] == '6') && (stat_string[1] == ','))  // Frequency
          {
            strcpy(FREQtext, stat_string);
            chopN(FREQtext, 2);
            FREQ = atof(FREQtext);
            TUNEFREQ = atoi(FREQtext);
            if (strcmp(LMRXmode, "sat") == 0)
            {
              FREQ = FREQ + LMRXqoffset;
            }
            FREQ = FREQ / 1000;
            snprintf(FREQtext, 15, "%.3f MHz", FREQ);
          }

          if ((stat_string[0] == '1') && (stat_string[1] == '2'))  // MER
          {
            strcpy(MERtext, stat_string);
            chopN(MERtext, 3);
            MER = atof(MERtext)/10;
            if (MER > 51)  // Trap spurious MER readings
            {
              MER = 0;
            }
            snprintf(MERtext, 24, "MER %.1f", MER);

            // Set up for Frequency capture

            if ((MER > 0.2) && (MERcount < 9))  // Wait for a valid MER
            {
              MERcount = MERcount + 1;
            }
            if (MERcount == 9)                 // Third valid MER
            {
              refMER = MER;
              MERcount = 10;
            }

            rectangle(wscreen * 1 / 40, hscreen - 1 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 1 * linepitch, STATEtext, font_ptr);
            rectangle(wscreen * 1 / 40, hscreen - 3 * linepitch - txtdesc, wscreen * 19 / 40, txttot, 0, 0, 0);
            Text2(wscreen * 1 / 40, hscreen - 3 * linepitch, MERtext, font_ptr);

            Text2(wscreen * 1.0 / 40.0, hscreen - 4.5 * linepitch, "Previous LNB Offset", font_ptr);
            snprintf(FREQtext, 15, "%d kHz", oldLMRXqoffset);
            Text2(wscreen * 1.0 / 40.0, hscreen - 5.5 * linepitch, FREQtext, font_ptr);
       
            // Make sure that the Tuner frequency is sensible
            if ((TUNEFREQ < 143000) || (TUNEFREQ > 2650000))
            {
              TUNEFREQ = 0;
            }

            if ((MERcount == 10) && (LMRXqoffset == 0) && (TUNEFREQ != 0))
            {
              Text2(wscreen * 1.0 / 40.0, hscreen - 7.0 * linepitch, "Calculated New LNB Offset", font_ptr);
              LMRXqoffset = 10491500 - TUNEFREQ;
              snprintf(FREQtext, 15, "%d kHz", LMRXqoffset);
              Text2(wscreen * 1.0 / 40.0, hscreen - 8.0 * linepitch, FREQtext, font_ptr);
              Text2(wscreen * 1.0 / 40.0, hscreen - 9.0 * linepitch, "Saved to memory card", font_ptr);
              snprintf(Value, 15, "%d", LMRXqoffset);
              SetConfigParam(PATH_LMCONFIG, "qoffset", Value);
            }
            if ((MERcount == 10) && (LMRXqoffset != 0)) // Done, so just display results
            {
              Text2(wscreen * 1.0 / 40.0, hscreen - 7.0 * linepitch, "Calculated New LNB Offset", font_ptr);
              snprintf(FREQtext, 15, "%d kHz", LMRXqoffset);
              Text2(wscreen * 1.0 / 40.0, hscreen - 8.0 * linepitch, FREQtext, font_ptr);
            }
            Text2(wscreen * 1.0 / 40.0, hscreen - 11.5 * linepitch, "Touch Centre of screen to exit", font_ptr);
            refreshMouseBackground();
            draw_cursor_foreground(mouse_x, mouse_y);
            UpdateWeb();
          }
          stat_string[0] = '\0';
        }
        else
        {
          strcat(stat_string, status_message_char);
        }
      }
      else
      {
        FinishedButton = 0;
      }
    } 
    close(fd_status_fifo); 
    usleep(1000);

    printf("Stopping receive process\n");
    pclose(fp);

    system("sudo killall lmmer.sh >/dev/null 2>/dev/null");
    touch_response = 0; 
    break;
  }
  system("sudo killall longmynd >/dev/null 2>/dev/null");
  system("sudo killall CombiTunerExpress >/dev/null 2>/dev/null");
  system("sudo killall omxplayer.bin >/dev/null 2>/dev/null");
  system("sudo killall mplayer >/dev/null 2>/dev/null");
  system("sudo killall vlc >/dev/null 2>/dev/null");
  pthread_join(thbutton, NULL);
}

void CycleLNBVolts()
{
  if (strcmp(LMRXvolts, "h") == 0)
  {
    strcpy(LMRXvolts, "v");
  }
  else
  {
    if (strcmp(LMRXvolts, "off") == 0)
    {
      strcpy(LMRXvolts, "h");
    }
    else  // All other cases
    {
      strcpy(LMRXvolts, "off");
    }
  }
  SetConfigParam(PATH_LMCONFIG, "lnbvolts", LMRXvolts);
  strcpy(LMRXvolts, "off");
  GetConfigParam(PATH_LMCONFIG, "lnbvolts", LMRXvolts);
}


void wait_touch()
// Wait for Screen touch, ignore position, but then move on
// Used to let user acknowledge displayed text
{
  int rawX, rawY, rawPressure;
  printf("wait_touch called\n");

  // Check if screen touched, if not, wait 0.1s and check again
  while(getTouchSample(&rawX, &rawY, &rawPressure) == 0)
  {
    usleep(100000);
  }
  // Screen has been touched
  printf("wait_touch exit\n");
}


void MsgBox(char *message)
{
  // Display a one-line message and wait for touch
  setForeColour(255, 255, 255);    // White text
  setBackColour(0, 0, 0);          // on Black
  const font_t *font_ptr = &font_dejavu_sans_32;

  clearScreen();
  TextMid2(wscreen / 2, hscreen /2, message, font_ptr);
  TextMid2(wscreen / 2, 20, "Touch Screen to Continue", font_ptr);
  refreshMouseBackground();
  draw_cursor_foreground(mouse_x, mouse_y);
  UpdateWeb();

  printf("MsgBox called and waiting for touch\n");
}


void MsgBox2(char *message1, char *message2)
{
  // Display a 2-line message and wait for touch
  setForeColour(255, 255, 255);    // White text
  setBackColour(0, 0, 0);          // on Black
  const font_t *font_ptr = &font_dejavu_sans_32;
  int txtht =  font_ptr->ascent;
  int linepitch = (14 * txtht) / 10;

  clearScreen();
  TextMid2(wscreen / 2, hscreen / 2 + linepitch, message1, font_ptr);
  TextMid2(wscreen / 2, hscreen / 2 - linepitch, message2, font_ptr);
  TextMid2(wscreen / 2, 20, "Touch Screen to Continue", font_ptr);
  refreshMouseBackground();
  draw_cursor_foreground(mouse_x, mouse_y);
  UpdateWeb();

  printf("MsgBox2 called and waiting for touch\n");
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
  TextMid2(wscreen / 2, hscreen - (linepitch * 2), message1, font_ptr);
  TextMid2(wscreen / 2, hscreen - 2 * (linepitch * 2), message2, font_ptr);
  TextMid2(wscreen / 2, hscreen - 3 * (linepitch * 2), message3, font_ptr);
  TextMid2(wscreen / 2, hscreen - 4 * (linepitch * 2), message4, font_ptr);
  refreshMouseBackground();
  draw_cursor_foreground(mouse_x, mouse_y);
  UpdateWeb();

  // printf("MsgBox4 called\n");
}


void YesNo(int i)  // i == 6 Yes, i == 8 No
{
  // First switch on what was calling the Yes/No question
  switch(CallingMenu)
  {
  case 414:         // Download ISS file??
    switch (i)
    {
    case 6:     // Yes
      MsgBox4("Downloading a 1.5 GB file", "This may take some time", "At least 3 minutes", " ");
      system("wget https://live.ariss.org/media/HAMTV%20Recordings/IQ%20Files/SDRSharp_20160423_121611Z_731000000Hz_IQ.wav.gz -O /home/pi/iqfiles/SDRSharp_20160423_121611Z_731000000Hz_IQ.wav.gz");
      MsgBox4("Completed Download", "Unzipping File", "This will take 5 minutes or so", " ");
      system("gzip -d /home/pi/iqfiles/SDRSharp_20160423_121611Z_731000000Hz_IQ.wav.gz");
      MsgBox("File ready for use");
      wait_touch();
      setBackColour(0, 0, 0);
      clearScreen();
      break;
    case 8:     // No
      MsgBox("File not Downloaded");
      wait_touch();
      setBackColour(0, 0, 0);
      clearScreen();
      break;
    }
    CurrentMenu = 4;
    UpdateWindow();
    break;
  case 430:         // Restore Factory Settings?
    switch (i)
    {
    case 6:     // Yes
      // Run script
      system("/home/pi/rpidatv/scripts/restore_factory.sh");
      
      // Correct the display back to original
      SetConfigParam(PATH_PCONFIG, "display", DisplayType);
      MsgBox2("Restored to Factory Settings", "Display will restart after touch");
      wait_touch();

      // Exit and restart display application to load settings
      cleanexit(129);
      break;
    case 8:     // No
      MsgBox("Current settings retained");
      wait_touch();
      setBackColour(0, 0, 0);
      clearScreen();
      break;
    }
    CurrentMenu = 43;
    UpdateWindow();
    break;

  case 431:         // Restore Settings from USB?
    switch (i)
    {
    case 6:     // Yes
      // Run script
      system("/home/pi/rpidatv/scripts/restore_from_USB.sh");
      
      // Correct the display back to original
      SetConfigParam(PATH_PCONFIG, "display", DisplayType);
      MsgBox2("Settings restored from USB", "Display will restart after touch");
      wait_touch();

      // Exit and restart display application to load settings
      cleanexit(129);
      break;
    case 8:     // No
      MsgBox("Current settings retained");
      wait_touch();
      setBackColour(0, 0, 0);
      clearScreen();
      break;
    }
    CurrentMenu = 43;
    UpdateWindow();
    break;

  case 432:         // Restore Settings from /boot?
    switch (i)
    {
    case 6:     // Yes
      // Run script
      system("/home/pi/rpidatv/scripts/restore_from_boot_folder.sh");
      
      // Correct the display back to original
      SetConfigParam(PATH_PCONFIG, "display", DisplayType);
      MsgBox2("Settings restored from /boot", "Display will restart after touch");
      wait_touch();

      // Exit and restart display application to load settings
      cleanexit(129);
      break;
    case 8:     // No
      MsgBox("Current settings retained");
      wait_touch();
      setBackColour(0, 0, 0);
      clearScreen();
      break;
    }
    CurrentMenu = 43;
    UpdateWindow();
    break;

  case 436:         // save Settings to USB?
    switch (i)
    {
    case 6:     // Yes
      // Run script
      system("/home/pi/rpidatv/scripts/copy_settings_to_usb.sh");
      MsgBox("Current settings saved to USB");
      wait_touch();
      setBackColour(0, 0, 0);
      clearScreen();
      break;
    case 8:     // No
      MsgBox("Settings not saved");
      wait_touch();
      setBackColour(0, 0, 0);
      clearScreen();
      break;
    }
    CurrentMenu = 43;
    UpdateWindow();
    break;

  case 437:         // save Settings to /boot?
    switch (i)
    {
    case 6:     // Yes
      // Run script
      system("/home/pi/rpidatv/scripts/copy_settings_to_boot_folder.sh");
      MsgBox("Current settings saved to /boot");
      wait_touch();
      setBackColour(0, 0, 0);
      clearScreen();
      break;
    case 8:     // No
      MsgBox("Settings not saved");
      wait_touch();
      setBackColour(0, 0, 0);
      clearScreen();
      break;
    }
    CurrentMenu = 43;
    UpdateWindow();
    break;

  case 4314:         // Invert 7 inch
    switch (i)
    {
    case 6:     // Yes
      // Run script which corrects config and reboots immediately
      MsgBox4(" ", "Rebooting now", " ", " ");
      UpdateWindow();
      system("sudo killall express_server >/dev/null 2>/dev/null");
      system("sudo rm /tmp/expctrl >/dev/null 2>/dev/null");
      sync();            // Prevents shutdown hang in Stretch
      usleep(1000000);
      cleanexit(193);    // Commands scheduler to rotate and reboot
      break;
    case 8:     // No
      MsgBox("Current settings retained");
      wait_touch();
      setBackColour(0, 0, 0);
      clearScreen();
      break;
    }
    CurrentMenu = 43;
    UpdateWindow();
    break;
  }
}

void InfoScreen()
{
  char result[255];
  char result2[255] = " ";
  int fec = 0;
  char FECtext[127];
  char TIMEtext[63];

  // Look up and format all the parameters to be displayed

  char swversion[255] = "Software Version: P4 ";
  GetSWVers(result);
  strcat(swversion, result);

  char dateTime[255];
  t = time(NULL);
  strftime(TIMEtext, sizeof(TIMEtext), "%H:%M:%S UTC %d %b %Y", gmtime(&t));
  strcpy(dateTime, TIMEtext);

  char ipaddress[255] = "IP: ";
  strcpy(result, "Not connected");
  GetIPAddr(result);
  strcat(ipaddress, result);
  strcat(ipaddress, "    ");
  GetIPAddr2(result2);
  strcat(ipaddress, result2);

  char CPUTemp[255];
  GetCPUTemp(result);
  sprintf(CPUTemp, "CPU temp=%.1f\'C      GPU ", atoi(result)/1000.0);
  GetGPUTemp(result);
  strcat(CPUTemp, result);

  char PowerText[255] = "Temperature has been or is too high";
  GetThrottled(result);
  result[strlen(result) - 1]  = '\0';
  if(strcmp(result,"throttled=0x0")==0)
  {
    strcpy(PowerText,"Temperatures and Supply voltage OK");
  }
  if(strcmp(result,"throttled=0x50000")==0)
  {
    strcpy(PowerText,"Low supply voltage event since start-up");
  }
  if(strcmp(result,"throttled=0x50005")==0)
  {
    strcpy(PowerText,"Low supply voltage now");
  }

  char TXParams1[255] = "TX ";
  GetConfigParam(PATH_PCONFIG,"freqoutput",result);
  strcat(TXParams1, result);
  strcat(TXParams1, " MHz  SR ");
  GetConfigParam(PATH_PCONFIG,"symbolrate",result);
  strcat(TXParams1, result);
  strcat(TXParams1, "  FEC ");
  GetConfigParam(PATH_PCONFIG, "fec", result);
  fec = atoi(result);
  switch(fec)
  {
    case 1:strcpy(FECtext,  "1/2") ; break;
    case 2:strcpy(FECtext,  "2/3") ; break;
    case 3:strcpy(FECtext,  "3/4") ; break;
    case 5:strcpy(FECtext,  "5/6") ; break;
    case 7:strcpy(FECtext,  "7/8") ; break;
    case 14:strcpy(FECtext, "1/4") ; break;
    case 13:strcpy(FECtext, "1/3") ; break;
    case 12:strcpy(FECtext, "1/2") ; break;
    case 35:strcpy(FECtext, "3/5") ; break;
    case 23:strcpy(FECtext, "2/3") ; break;
    case 34:strcpy(FECtext, "3/4") ; break;
    case 56:strcpy(FECtext, "5/6") ; break;
    case 89:strcpy(FECtext, "8/9") ; break;
    case 91:strcpy(FECtext, "9/10") ; break;
    default:strcpy(FECtext, "Not known") ; break;
  }
  strcat(TXParams1, FECtext);

  char TXParams2[255];
  char vcoding[255];
  char vsource[255];
  ReadModeInput(vcoding, vsource);
  strcpy(TXParams2, vcoding);
  strcat(TXParams2, " coding from ");
  strcat(TXParams2, vsource);
  
  char TXParams3[255];
  char ModeOutput[255];
  ReadModeOutput(ModeOutput);
  strcpy(TXParams3, "Output to ");
  strcat(TXParams3, ModeOutput);

  char SerNo[255];
  char CardSerial[255] = "SD Card Serial: ";
  GetSerNo(SerNo);
  strcat(CardSerial, SerNo);

  char DeviceTitle[255] = "Audio Devices:";

  char Device1[255]=" ";
  char Device2[255]=" ";
  GetDevices(Device1, Device2);

  char BitRate[255];
  sprintf(BitRate, "TS Bitrate Required = %d", CalcTSBitrate());

  if (CheckC920Type() == 1)
  {
    strcat(BitRate, "  Webcam: C920 (with H264 Encoder)");
  }

  if (CheckC920Type() == 2)
  {
    strcat(BitRate, "  Webcam: C920 (no H264 encoder)");
  }

  if (CheckC920Type() == 3)
  {
    strcat(BitRate, "  Webcam: C920 (new with no H264)");
  }

  if (CheckC920Type() == 4)
  {
    strcat(BitRate, "  Webcam: Polycom EagleEye Mini");
  }

  // Initialise and calculate the text display
  setForeColour(255, 255, 255);    // White text
  setBackColour(0, 0, 0);          // on Black
  const font_t *font_ptr = &font_dejavu_sans_22;
  int txtht =  font_ptr->ascent;
  int linepitch = (14 * txtht) / 10;
  int linenumber = 1;
 
  // Display Text
  clearScreen();

  TextMid2(wscreen / 2.0, hscreen - linenumber * linepitch, "BATC Portsdown 4 Information Screen", font_ptr);
  linenumber = linenumber + 2;

  Text2(wscreen/25, hscreen - linenumber * linepitch, swversion, font_ptr);

  Text2(15 * wscreen/25, hscreen - linenumber * linepitch, dateTime, font_ptr);
  linenumber = linenumber + 1;

  Text2(wscreen/25, hscreen - linenumber * linepitch, ipaddress, font_ptr);
  linenumber = linenumber + 1;

  Text2(wscreen/25, hscreen - linenumber * linepitch, CPUTemp, font_ptr);
  linenumber = linenumber + 1;

  Text2(wscreen/25, hscreen - linenumber * linepitch, PowerText, font_ptr);
  linenumber = linenumber + 1;

  Text2(wscreen/25, hscreen - linenumber * linepitch, TXParams1, font_ptr);
  linenumber = linenumber + 1;

  Text2(wscreen/25, hscreen - linenumber * linepitch, TXParams2, font_ptr);
  linenumber = linenumber + 1;

  Text2(wscreen/25, hscreen - linenumber * linepitch, TXParams3, font_ptr);
  linenumber = linenumber + 1;

  Text2(wscreen/25, hscreen - linenumber * linepitch, CardSerial, font_ptr);
  linenumber = linenumber + 1;

  Text2(wscreen/25, hscreen - linenumber * linepitch, DeviceTitle, font_ptr);
  linenumber = linenumber + 1;

  Text2(wscreen/25, hscreen - linenumber * linepitch, Device1, font_ptr);
  linenumber = linenumber + 1;

  Text2(wscreen/25, hscreen - linenumber * linepitch, Device2, font_ptr);
  linenumber = linenumber + 1;

  Text2(wscreen/25, hscreen - linenumber * linepitch, BitRate, font_ptr);
  linenumber = linenumber + 1;

  TextMid2(wscreen / 2, hscreen - linenumber * linepitch, "Touch Screen to Continue", font_ptr);

  printf("Info Screen called and waiting for touch\n");
  refreshMouseBackground();
  draw_cursor_foreground(mouse_x, mouse_y);
  UpdateWeb();

  // Create Wait Button thread
  pthread_create (&thbutton, NULL, &WaitButtonEvent, NULL);    // Show changing time while waiting for touch

  struct tm *tm;
  int seconds;
  tm =  gmtime(&t);
  seconds = tm->tm_sec;

  while (FinishedButton == 0)
  {
    //This loop is executed at about 100 Hz

    usleep(10000);
    t = time(NULL);                           // Look up current time
    tm =  gmtime(&t);                         // Convert to UTC

    if (seconds != tm->tm_sec)                // Seconds have clicked over
    {

      strftime(TIMEtext, sizeof(TIMEtext), "%H:%M:%S UTC %d %b %Y", gmtime(&t));   // so form new time string
      strcpy(dateTime, TIMEtext);

      Text2(15 * wscreen/25, hscreen - 3 * linepitch, dateTime, font_ptr);         // and display it       

      seconds = tm->tm_sec;                                                        // reset check time

      refreshMouseBackground();
      draw_cursor_foreground(mouse_x, mouse_y);
      UpdateWeb();
    }
  }
  FinishedButton = 0;
  pthread_join(thbutton, NULL);
}

void RangeBearing()
{
  char Param[31];
  char Value[31];
  char IntValue[31];
  char DispName[10][20];
  char Locator[10][11];
  char MyLocator[11]="IO90LU";
  char FromCall[25];
  int Bearing[10];
  int Range[10];
  int i, j;
  int offset;
  char Prompt[63];
  bool IsValid = FALSE;

  // read which entry is currently top of the list
  GetConfigParam(PATH_LOCATORS, "index", Value);
  offset = atoi(Value);
  
  // Calculate the bottom of the list
  offset = offset - 1;
  if (offset < 0)
  {
    offset = offset + 10;
  }

  sprintf(Prompt, "Enter the callsign (select enter to view list)");
  Keyboard(Prompt, "", 19);

  if (strlen(KeyboardReturn) > 0)
  {
    sprintf(Param, "callsign%d", offset);
    SetConfigParam(PATH_LOCATORS, Param, KeyboardReturn);

    strcpy(KeyboardReturn, "");
    while (IsValid == FALSE)
    {
      sprintf(Prompt, "Enter the Locator (6, 8 or 10 chars)");
      Keyboard(Prompt, KeyboardReturn, 10);
      IsValid = CheckLocator(KeyboardReturn);
    }

    sprintf(Param, "locator%d", offset);
    SetConfigParam(PATH_LOCATORS, Param, KeyboardReturn);

    snprintf(Value, 2, "%d", offset);
    SetConfigParam(PATH_LOCATORS, "index", Value);
  }
  else
  {
    offset = offset + 1;
    if (offset > 9)
    {
      offset = offset - 10;
    }
  }

  // Initialise and calculate the text display
  setForeColour(255, 255, 255);    // White text
  setBackColour(0, 0, 0);          // on Black
  const font_t *font_ptr = &font_dejavu_sans_24;
  int txtht =  font_ptr->ascent;
  int linepitch = (14 * txtht) / 10;
  //int linenumber = 1;

  // Read my locator
  strcpy(Param,"mylocator");
  GetConfigParam(PATH_LOCATORS, Param, MyLocator);

  // Read Callsigns and Locators from file, and calculate each r/b
  for(i = 0; i < 10 ;i++)
  {
    sprintf(Param, "callsign%d", i);
    GetConfigParam(PATH_LOCATORS, Param, DispName[i]);
    sprintf(Param, "locator%d", i);
    GetConfigParam(PATH_LOCATORS, Param, Locator[i]);

    Bearing[i] = CalcBearing(MyLocator, Locator[i]);
    Range[i] = CalcRange(MyLocator, Locator[i]);
  }

  // Display Title Text
  strcpyn(FromCall, CallSign, 24);
  sprintf(Value, "From %s", FromCall);

  clearScreen();
  Text2(wscreen / 40, hscreen - linepitch, Value, font_ptr);
  Text2((wscreen * 15) / 40, hscreen - linepitch, MyLocator, font_ptr);
  Text2((wscreen * 27) / 40, hscreen - linepitch, "Bearing", font_ptr);
  Text2((wscreen * 34) / 40, hscreen - linepitch, "Range", font_ptr);

  // Display each row in turn
  for(i = 0; i < 10 ; i++)
  {
    // Correct for offset
    j = i + offset;
    if (j > 9)
    {
      j = j - 10;
    }

    Text2(wscreen / 40, hscreen - (i + 3) * linepitch, DispName[j], font_ptr);
    Text2((wscreen * 15) / 40, hscreen - (i + 3) * linepitch, Locator[j], font_ptr);
    snprintf(IntValue, 4, "%d", Bearing[j]);
    if (strlen(IntValue) == 3)
    {
       strcpy(Value, IntValue);
    }
    if (strlen(IntValue) == 2)
    {
      strcpy(Value, "0");
      strcat(Value, IntValue);
    }
    if (strlen(IntValue) == 1)
    {
      strcpy(Value, "00");
      strcat(Value, IntValue);
    }
    strcat(Value, " deg");
    Text2((wscreen * 27) / 40, hscreen - (i + 3) * linepitch, Value, font_ptr);
    sprintf(Value, "%d km", Range[j]);
    Text2((wscreen * 34) / 40, hscreen - (i + 3) * linepitch, Value, font_ptr);
  }

  TextMid2(wscreen/2, 20, "Touch Screen to Continue",  font_ptr);

  printf("Locator Bearing called and waiting for touch\n");
  refreshMouseBackground();
  draw_cursor_foreground(mouse_x, mouse_y);
  UpdateWeb();

  wait_touch();
}

void BeaconBearing()
{
  char Param[31];
  char Value[31];
  char IntValue[31];
  char DispName[10][20];
  char Locator[10][11];
  char MyLocator[11]="IO90LU";
  char FromCall[24];
  int Bearing[10];
  int Range[10];
  int i;

  // Initialise and calculate the text display
  setForeColour(255, 255, 255);    // White text
  setBackColour(0, 0, 0);          // on Black
  const font_t *font_ptr = &font_dejavu_sans_24;
  int txtht =  font_ptr->ascent;
  int linepitch = (14 * txtht) / 10;
  //int linenumber = 1;

  // Read Callsigns and Locators from file, and calculate each r/b
  strcpy(Param,"mylocator");
  GetConfigParam(PATH_LOCATORS, Param, MyLocator);
  for(i = 0; i < 10 ; i++)
  {
    sprintf(Param, "bcallsign%d", i);
    GetConfigParam(PATH_LOCATORS, Param, DispName[i]);
    sprintf(Param, "blocator%d", i);
    GetConfigParam(PATH_LOCATORS, Param, Locator[i]);

    Bearing[i] = CalcBearing(MyLocator, Locator[i]);
    Range[i] = CalcRange(MyLocator, Locator[i]);
  }

  // Display Title Text
  strcpyn(FromCall, CallSign, 24);
  sprintf(Value, "From %s", FromCall);

  clearScreen();
  Text2(wscreen / 40, hscreen - linepitch, Value, font_ptr);
  Text2((wscreen * 15) / 40, hscreen - linepitch, MyLocator, font_ptr);
  Text2((wscreen * 27) / 40, hscreen - linepitch, "Bearing", font_ptr);
  Text2((wscreen * 34) / 40, hscreen - linepitch, "Range", font_ptr);

  // Display each row in turn
  for(i = 0; i < 10 ; i++)
  {
    Text2(wscreen / 40, hscreen - (i + 3) * linepitch, DispName[i], font_ptr);
    Text2((wscreen * 15) / 40, hscreen - (i + 3) * linepitch, Locator[i], font_ptr);
    snprintf(IntValue, 4, "%d", Bearing[i]);
    if (strlen(IntValue) == 3)
    {
       strcpy(Value, IntValue);
    }
    if (strlen(IntValue) == 2)
    {
      strcpy(Value, "0");
      strcat(Value, IntValue);
    }
    if (strlen(IntValue) == 1)
    {
      strcpy(Value, "00");
      strcat(Value, IntValue);
    }
    strcat(Value, " deg");
    Text2((wscreen * 27) / 40, hscreen - (i + 3) * linepitch, Value, font_ptr);
    sprintf(Value, "%d km", Range[i]);
    Text2((wscreen * 34) / 40, hscreen - (i + 3) * linepitch, Value, font_ptr);
  }

  TextMid2(wscreen/2, 20, "Touch Screen to Continue",  font_ptr);
  refreshMouseBackground();
  draw_cursor_foreground(mouse_x, mouse_y);
  UpdateWeb();

  printf("Beacon Bearing called and waiting for touch\n");
  wait_touch();
}

void AmendBeacon(int i)
{
  char Param[15];
  char Value[31];
  char Prompt[63];
  bool IsValid = FALSE;

  // Correct button number to site number 
  i = i - 5;
  if (i < 0)
  {
    i = i + 10;
  }
  printf("Amend Beacon %d\n", i);

  // Retrieve amend and save the details
  sprintf(Param, "bcallsign%d", i);
  GetConfigParam(PATH_LOCATORS, Param, Value);
  sprintf(Prompt, "Enter the new name for the site/beacon (no spaces)");
  Keyboard(Prompt, Value, 19);
  SetConfigParam(PATH_LOCATORS, Param, KeyboardReturn);

  sprintf(Param, "blocator%d", i);
  GetConfigParam(PATH_LOCATORS, Param, Value);
  sprintf(Prompt, "Enter the locator for this new Site/beacon");

  while (IsValid == FALSE)
  {
    Keyboard(Prompt, Value, 10);
    IsValid = CheckLocator(KeyboardReturn);
  }
  SetConfigParam(PATH_LOCATORS, Param, KeyboardReturn);
}

int CalcBearing(char *myLocator, char *remoteLocator)
{
  float myLat;
  float myLong;
  float remoteLat;
  float remoteLong;
  int Bearing;

  myLat = Locator_To_Lat(myLocator);
  myLong = Locator_To_Lon(myLocator);
  remoteLat = Locator_To_Lat(remoteLocator);
  remoteLong = Locator_To_Lon(remoteLocator);

  //printf(" Calc Bearing: My Locator %s, myLat %f, myLong %f\n", myLocator, myLat, myLong);

  Bearing =  GcBearing(myLat, myLong, remoteLat, remoteLong);
  return Bearing;
}

int CalcRange(char *myLocator, char *remoteLocator)
{
  float myLat;
  float myLong;
  float remoteLat;
  float remoteLong;
  int Range;

  myLat = Locator_To_Lat(myLocator);
  myLong = Locator_To_Lon(myLocator);
  remoteLat = Locator_To_Lat(remoteLocator);
  remoteLong = Locator_To_Lon(remoteLocator);
  Range = (int)GcDistance(myLat, myLong, remoteLat, remoteLong, "K");

  return Range;
}

bool CheckLocator(char *Loc)
{
  // Returns false if locator not valid format

  char szLoc[32] = {0};
  int V;
  int n;
  bool bRet = TRUE;
  int LocLength;

  strcpy(szLoc, Loc);
  LocLength = strlen(szLoc);
  if ((LocLength < 4) || (LocLength > 10))
  {
    bRet = FALSE;
    return bRet;
  }
  switch(LocLength)
  {
  case 4:
    strcat(szLoc, "LL55AA");
    break;
  case 6:
    strcat(szLoc, "55AA");
    break;
  case 8:
    strcat(szLoc, "LL");
    break;
  case 5:
  case 7:
  case 9:
    bRet = FALSE;
    return bRet;
  }

  for( n = 0; n <= strlen(szLoc) ; n++ )
  {
    V = szLoc[n];

    // Check Values
    if ((n==0) || (n==1) || (n==4) || (n==5) || (n==8) || (n==9))
    {
      if( (V < 'A') || (V > 'X') )
      {
        bRet = FALSE;
      }
    }
    else
    {
      if( V > '9' )
      {
        bRet = FALSE;
      }
    }
  }
  return bRet;
}

float Locator_To_Lat(char *Loc)
{
  char szLoc[32] = {0};
  int V;
  float P[10] = {0};
  float Lat;
  unsigned int n;
  bool bRet = TRUE;

  strcpy(szLoc,Loc);

  if (strlen(szLoc) < 4 )
  {
    bRet = FALSE;
    return bRet;
  }
  if( (strlen(szLoc) == 4) )
  {
    strcat(szLoc, "LL55AA");
  }
  if( (strlen(szLoc) == 6) )
  {
    strcat(szLoc, "55AA");
  }
  if( (strlen(szLoc) == 8) )
  {
    strcat(szLoc, "LL");
  }

  bRet = CheckLocator(szLoc);

  for( n = 0; n <= strlen(szLoc) ; n++ )
  {
    V = szLoc[n];
    if( V < 'A' )
    {
      P[n] = V - '0';
    }
    else
    {
      P[n] = V - 'A';
    }
  }

  if ( bRet )
  {
    Lat = (P[1]*10) + P[3] + (P[5]/24) + (P[7]/240) + (P[9]/5760) - 90;
  }
  return Lat;
}

float Locator_To_Lon(char *Loc)
{
  char szLoc[32] = {0};
  int V;
  float P[10] = {0};
  float Lon;
  unsigned int n;
  bool bRet = TRUE;

  strcpy(szLoc, Loc);

  if (strlen(szLoc) < 4 )
  {
    bRet = FALSE;
    return bRet;
  }
  if( (strlen(szLoc) == 4) )
  {
    strcat(szLoc, "LL55AA");
  }
  if( (strlen(szLoc) == 6) )
  {
    strcat(szLoc, "55AA");
  }
  if( (strlen(szLoc) == 8) )
  {
    strcat(szLoc, "LL");
  }

  bRet = CheckLocator(szLoc);

  for( n = 0; n <= strlen(szLoc) ; n++ )
  {
    V = szLoc[n];
    if( V < 'A' )
    {
      P[n] = V - '0';
    }
    else
    {
      P[n] = V - 'A';
    }
  }

  if ( bRet )
  {
    Lon = (P[0]*20) + (P[2]*2) + (P[4]/12) + (P[6]/120) + (P[8]/2880) - 180;
  }
  return Lon;
}


float GcDistance( const float lat1, const float lon1, const float lat2, const float lon2, const char *unit )
{
  // Units:
  // "K" = Kilometers, "N" = Nautical Miles, "M" = Miles

  float theta;
  float dist;
  float miles;

  theta = lon1 - lon2;
  dist = sin(deg2rad(lat1)) * sin(deg2rad(lat2)) +  cos(deg2rad(lat1)) * cos(deg2rad(lat2)) * cos(deg2rad(theta));
  dist = acos(dist);
  dist = rad2deg(dist);
  miles = dist * 60 * 1.1515;

  if( strcmp(unit, "K") == 0 )
  {
    return (miles * 1.609344);
  }
  else if (strcmp(unit, "N") == 0 )
  {
    return (miles * 0.8684);
  }
  else
  {
    return miles;
  }

    return 0;
}

int GcBearing( const float lat1, const float lon1, const float lat2, const float lon2 )
{
  float theta;
  float x, y;
  float brng;

  // printf("lat %f lon %f, lat %f lon %f\n", lat1, lon1, lat2, lon2);

  theta = lon2 - lon1;
  y = sin(deg2rad(theta)) * cos(deg2rad(lat2));
  x = cos(deg2rad(lat1)) * sin(deg2rad(lat2)) - sin(deg2rad(lat1)) * cos(deg2rad(lat2)) * cos(deg2rad(theta));

  brng = atan2(y, x);
  brng = rad2deg(brng);

  return ((int) brng + 360) % 360;
}

void rtl_tcp()
{
  if(CheckRTL()==0)
  {
    char rtl_tcp_start[256];
    char current_IP[256];
    char message1[256];
    char message2[256];
    char message3[256];
    char message4[256];
    GetIPAddr(current_IP);
    strcpy(rtl_tcp_start,"(rtl_tcp -a ");
    strcat(rtl_tcp_start, current_IP);
    strcat(rtl_tcp_start, ") &");
    system(rtl_tcp_start);

    strcpy(message1, "RTL-TCP server running on");
    strcpy(message2, current_IP);
    strcat(message2, ":1234");
    strcpy(message3, "Touch screen again to");
    strcpy(message4, "stop the RTL-TCP Server");
    MsgBox4(message1, message2, message3, message4);
    wait_touch();

    system("sudo killall rtl_tcp >/dev/null 2>/dev/null");
    usleep(500);
    system("sudo killall -9 rtl_tcp >/dev/null 2>/dev/null");
  }
  else
  {
    MsgBox("No RTL-SDR Connected");
    wait_touch();
  }
}

void do_snap()
{
  char USBVidDevice[255];

  GetUSBVidDev(USBVidDevice);
  if (strlen(USBVidDevice) != 12)  // /dev/video* with a new line
  {
    MsgBox("No EasyCap Found");
    wait_touch();
    UpdateWindow();
    setBackColour(0, 0, 0);
    clearScreen();
  }
  else
  {
    printf("do_snap\n");
    system("/home/pi/rpidatv/scripts/snap.sh >/dev/null 2>/dev/null");
    wait_touch();
    system("sudo killall fbi >/dev/null 2>/dev/null");  // kill any previous images
    system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png  >/dev/null 2>/dev/null");  // Add logo image

    setBackColour(0, 0, 0);
    clearScreen();
    UpdateWindow();
    system("sudo killall fbi >/dev/null 2>/dev/null");  // kill fbi now
  }
}


void do_snapcheck()
{
  FILE *fp;
  char SnapIndex[255];
  int SnapNumber;
  int Snap;
  int LastDisplayedSnap = -1;
  int rawX, rawY, rawPressure;
  int TCDisplay = -1;

  char fbicmd[511];

  // Fetch the Next Snap serial number
  fp = popen("cat /home/pi/snaps/snap_index.txt", "r");
  if (fp == NULL) 
  {
    printf("Failed to run command\n" );
    exit(1);
  }
  /* Read the output a line at a time - output it. */
  while (fgets(SnapIndex, 20, fp) != NULL)
  {
    printf("%s", SnapIndex);
  }
  /* close */
  pclose(fp);

  SnapNumber=atoi(SnapIndex);
  Snap = SnapNumber - 1;

  while (((TCDisplay == 1) || (TCDisplay == -1)) && (SnapNumber != 0))
  {
    if(LastDisplayedSnap != Snap)  // only redraw if not already there
    {
      sprintf(fbicmd, "convert /home/pi/snaps/snap%d.jpg -font \"FreeSans\" -size 800x480 -gravity South -pointsize 15 -fill grey80 -annotate 0,0,0,25 snap%d /home/pi/tmp/nsnap.jpg", Snap, Snap);
      system(fbicmd);
      strcpy(fbicmd, "sudo fbi -T 1 -noverbose -a /home/pi/tmp/nsnap.jpg >/dev/null 2>/dev/null");
      system(fbicmd);
      LastDisplayedSnap = Snap;
      refreshMouseBackground();
      draw_cursor_foreground(mouse_x, mouse_y);
      UpdateWeb();
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
      printf("Displaying snap %d\n", Snap);
    }
  }

  // Tidy up and display touch menu
  system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png  >/dev/null 2>/dev/null");  // Add logo image
  UpdateWindow();
  system("sudo killall fbi >/dev/null 2>/dev/null");  // kill instance of fbi
}


static void cleanexit(int exit_code)
{
  strcpy(ModeInput, "DESKTOP"); // Set input so webcam reset script is not called
  TransmitStop();
  setBackColour(0, 0, 0);
  clearScreen();
  closeScreen();
  printf("Clean Exit Code %d\n", exit_code);
  char Commnd[255];
  sprintf(Commnd,"stty echo");
  system(Commnd);
  sprintf(Commnd,"reset");
  system(Commnd);
  exit(exit_code);
}


void do_Langstone()
{
  // Check that audio dongle exists before exit, otherwise display error message
  if (DetectUSBAudio() == 0)
  {
    if (strcmp(langstone_version, "v1pluto") == 0)
    {
      // Check that Pluto IP is set correctly otherwise display error message
      if (CheckPlutoIPConnect() == 0)
      {
        cleanexit(135);  // Start Langstone
      }
      else
      {
        MsgBox4("Pluto IP not set in Pluto Config", "or Pluto not connected",
                "Please correct and try again", "Touch Screen to Continue");
        wait_touch();
      } 
    }
    if (strcmp(langstone_version, "v2pluto") == 0)
    {
      // Check that Pluto IP is set correctly otherwise display error message
      if (CheckPlutoIPConnect() == 0)
      {
        cleanexit(146);  // Start Langstone v2 for Pluto
      }
      else
      {
        MsgBox4("Pluto IP not set in Pluto Config", "or Pluto not connected",
                "Please correct and try again", "Touch Screen to Continue");
        wait_touch();
      } 
    }
    if (strcmp(langstone_version, "v2lime") == 0)
    {
      // Check that Pluto IP is set correctly otherwise display error message
      if ((CheckLimeMiniConnect() == 0) || (CheckLimeUSBConnect() == 0))
      {
        cleanexit(145);  // Start Langstone V2 for Lime
      }
      else
      {
        MsgBox4("No LimeSDR connected", " ",
                "Please correct and try again", "Touch Screen to Continue");
        wait_touch();
      } 
    }
  }
  else
  {
    MsgBox2("No USB Audio Dongle Detected", "Connect one before selecting Langstone");
    wait_touch();
  }
}


void do_video_monitor(int button)
{
  char startCommand[255];

  switch(button)
  {
  case 10:
    printf("Starting Video Monitor, calling av2.sh\n");
    strcpy(startCommand, "/home/pi/rpidatv/scripts/av2.sh");
    strcat(startCommand, " &");
    break;
  case 11:
    printf("Starting Pi Cam Monitor, calling av1.sh\n");
    strcpy(startCommand, "/home/pi/rpidatv/scripts/av1.sh");
    strcat(startCommand, " &");
    break;
  case 12:
    printf("Starting C920 Monitor, calling av3.sh\n");
    strcpy(startCommand, "/home/pi/rpidatv/scripts/av3.sh");
    strcat(startCommand, " &");
    break;
  case 13:
    printf("Starting IPTS Monitor, calling av5.sh\n");
    strcpy(startCommand, "/home/pi/rpidatv/scripts/av5.sh");
    strcat(startCommand, " &");
    break;
  case 14:
    printf("Starting HDMI Monitor, calling av4.sh\n");
    strcpy(startCommand, "/home/pi/rpidatv/scripts/av4.sh");
    strcat(startCommand, " &");
    break;
  }

  FinishedButton = 0;
  setBackColour(0, 0, 0);
  clearScreen();

  // Create Wait Button thread
  pthread_create (&thbutton, NULL, &WaitButtonVideo, NULL);

  system(startCommand);

  while (FinishedButton == 0)
  {
    usleep(500000); 
  }

  MonitorStop();

  pthread_join(thbutton, NULL);
  printf("Exiting Video Monitor\n");
}

void MonitorStop()
{
  char bashcmd[255];
  char picamdev1[15];

  // Kill the key processes as nicely as possible
  system("sudo killall ffmpeg >/dev/null 2>/dev/null");
  system("sudo killall avc2ts >/dev/null 2>/dev/null");
  system("sudo killall netcat >/dev/null 2>/dev/null");
  system("sudo killall mplayer >/dev/null 2>/dev/null");

  // Shutdown VLC
  system("/home/pi/rpidatv/scripts/lmvlcsd.sh &");


  // Turn the Viewfinder off
  GetPiCamDev(picamdev1);
  if (strlen(picamdev1) > 1)
  {
    strcpy(bashcmd, "v4l2-ctl -d ");
    strcat(bashcmd, picamdev1);
    strcat(bashcmd, " --overlay=0 >/dev/null 2>/dev/null");
    system(bashcmd);
  }

  // Stop the audio relay in CompVid mode
  system("sudo killall arecord >/dev/null 2>/dev/null");
}

void IPTSConfig(int NoButton)
{
  char RequestText[64];
  char InitText[64];
  bool IsValid = FALSE;
  char KRCopy[63];
  char TSFile[255];
  char TSPath[255];
  char NewTSFile[255];
  char NewTSPath[255];
  bool deleting;
  int i;

  switch (NoButton)
  {
  case 0:
    while (IsValid == FALSE)
    {
      strcpy(RequestText, "Enter IP address for TS Out");
      strcpyn(InitText, UDPOutAddr, 17);
      Keyboard(RequestText, InitText, 17);
  
      strcpy(KRCopy, KeyboardReturn);
      if(is_valid_ip(KRCopy) == 1)
      {
        IsValid = TRUE;
      }
    }
    printf("UDP Out IP set to: %s\n", KeyboardReturn);

    // Save IP to config file
    SetConfigParam(PATH_PCONFIG, "udpoutaddr", KeyboardReturn);
    strcpy(UDPOutAddr, KeyboardReturn);
    break;
  case 1:
    while (IsValid == FALSE)
    {
      strcpy(RequestText, "Enter IP Port for TS Out");
      strcpyn(InitText, UDPOutPort, 17);
      Keyboard(RequestText, InitText, 17);
  
      strcpy(KRCopy, KeyboardReturn);
      if(atoi(KRCopy) <= 65353)
      {
        IsValid = TRUE;
      }
    }
    printf("UDP Port for TS Out set to: %s\n", KeyboardReturn);

    // Save IP to config file
    SetConfigParam(PATH_PCONFIG, "udpoutport", KeyboardReturn);
    strcpy(UDPOutPort, KeyboardReturn);
    break;
  case 2:
    while (IsValid == FALSE)
    {
      strcpy(RequestText, "Enter IP Port for TS Input");
      strcpyn(InitText, UDPInPort, 17);
      Keyboard(RequestText, InitText, 17);
  
      strcpy(KRCopy, KeyboardReturn);
      if(atoi(KRCopy) <= 65353)
      {
        IsValid = TRUE;
      }
    }
    printf("UDP Port for TS in set to: %s\n", KeyboardReturn);

    // Save IP to config file
    SetConfigParam(PATH_PCONFIG, "udpinport", KeyboardReturn);
    strcpy(UDPInPort, KeyboardReturn);
    break;

  case 3:                            // Edit TS Filename
    printf("stored TSVideoFile is %s\n", TSVideoFile);

    // Separate out the Path
    deleting = true;
    strcpy(TSPath, TSVideoFile);
    for (i = strlen(TSVideoFile); i > 1; i = i - 1)
    {
      if (deleting)                                   // if the slash has not been reached
      {
        TSPath[i - 1] = '\0';                         // delete the character
      }
      if (TSPath[i - 2] == '/')                       // If Next character is a slash 
      {
        deleting = false;                             // stop deleting
      }
    }
    printf("TSPath is %s\n", TSPath);

    // Separate out the filename
    strcpy(TSFile, TSVideoFile);
    chopN(TSFile, strlen(TSPath));                   // Chop off the path characters from the full string
    printf("TSFile is %s\n", TSFile);


    // Choose the new file (or not)
    if (SelectFileUsingList(TSPath, TSFile, NewTSPath, NewTSFile, 0) == 1)  // file has changed
    {
      strcat(NewTSPath, NewTSFile);                                      // Combine path and filename
      printf("New TS filename: %s\n", NewTSPath);

      SetConfigParam(PATH_PCONFIG, "tsvideofile", NewTSPath);            // Save filename to config file   
      strcpy(TSVideoFile, NewTSPath);
    }
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
      //UpdateWindow();
      usleep(300000);
      SetButtonStatus(ButtonNumber(41, token), 0);
      DrawButton(ButtonNumber(41, token));
      //UpdateWindow();
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

void ChangePresetFreq(int NoButton)
{
  char RequestText[64];
  char InitText[64];
  char PresetNo[2];
  int FreqIndex;
  //float TvtrFreq;
  //float PDfreq;
  char Param[15] = "pfreq";

  // Convert button number to frequency array index
  if (NoButton < 4)
  {
    FreqIndex = NoButton + 5;
  }
  else
  {
    FreqIndex = NoButton - 5;
  }

  // Define request string depending on transverter or not
  if (((TabBandLO[CurrentBand] < 0.1) && (TabBandLO[CurrentBand] > -0.1)) || (CallingMenu == 5))
  {
    strcpy(RequestText, "Enter new frequency for Button ");
  }
  else
  {
    strcpy(RequestText, "Enter new IF Drive frequency for Button ");
  }
  snprintf(PresetNo, 2, "%d", FreqIndex + 1);
  strcat(RequestText, PresetNo);
  strcat(RequestText, " in MHz:");

  // Calculate initial value
  //if (((TabBandLO[CurrentBand] < 0.1) && (TabBandLO[CurrentBand] > -0.1)) || (CallingMenu == 5))
  //{
    //snprintf(InitText, 10, "%s", TabFreq[FreqIndex]);
    strcpyn(InitText, TabFreq[FreqIndex], 10);
  //}
  //else
  //{
  //  TvtrFreq = atof(TabFreq[FreqIndex]) + TabBandLO[CurrentBand];
  //  if (TvtrFreq < 0)
  //  {
  //    TvtrFreq = TvtrFreq * -1;
  //  }
  //  snprintf(InitText, 10, "%.2f", TvtrFreq);
  //}
  
  Keyboard(RequestText, InitText, 10);

  // Correct freq for transverter offset
  //if (((TabBandLO[CurrentBand] < 0.1) && (TabBandLO[CurrentBand] > -0.1)) || (CallingMenu == 5))
  //{
  //  ; // No transverter offset required
  //}
  //else  // Calculate transverter offset
  //{
  //  if (TabBandLO[CurrentBand] > 0)  // Low side LO
  //  {
  //    PDfreq = atof(KeyboardReturn) - TabBandLO[CurrentBand];
  //  }
  //  else  // High side LO
  //  {
  //    PDfreq = -1 * (atof(KeyboardReturn) + TabBandLO[CurrentBand]);
  //  }
  //  snprintf(KeyboardReturn, 10, "%.2f", PDfreq);
  //}
  
  // Write freq to tabfreq
  strcpy(TabFreq[FreqIndex], KeyboardReturn);

  // write freq to Presets file
  strcat(Param, PresetNo); 
  printf("Store Preset %s %s\n", Param, KeyboardReturn);
  SetConfigParam(PATH_PPRESETS, Param, KeyboardReturn);

  // Compose Button Text
  switch (strlen(TabFreq[FreqIndex]))
  {
  case 2:
    strcpy(FreqLabel[FreqIndex], " ");
    strcat(FreqLabel[FreqIndex], TabFreq[FreqIndex]);
    strcat(FreqLabel[FreqIndex], " MHz ");
    break;
  case 3:
    strcpy(FreqLabel[FreqIndex], TabFreq[FreqIndex]);
    strcat(FreqLabel[FreqIndex], " MHz ");
    break;
  case 4:
    strcpy(FreqLabel[FreqIndex], TabFreq[FreqIndex]);
    strcat(FreqLabel[FreqIndex], " MHz");
    break;
  case 5:
    strcpy(FreqLabel[FreqIndex], TabFreq[FreqIndex]);
    strcat(FreqLabel[FreqIndex], "MHz");
    break;
  default:
    strcpy(FreqLabel[FreqIndex], TabFreq[FreqIndex]);
    strcat(FreqLabel[FreqIndex], " M");
    break;
  }
}

void ChangeLMPresetFreq(int NoButton)
{
  char RequestText[63];
  char InitText[63] = " ";
  char PresetNo[3];
  char Param[63];
  div_t div_10;
  div_t div_100;
  div_t div_1000;
  int FreqIndex;
  int CheckValue = 0;
  int Offset_to_Apply = 0;
  char FreqkHz[63];

  // Convert button number to frequency array index
  if (CallingMenu == 8)  // Called from receive Menu
  {
    FreqIndex = 10;
  }
  else  // called from LM freq Presets menu 
  {
    if (NoButton < 4)
    {
      FreqIndex = NoButton + 6;
    }
    else
    {
      FreqIndex = NoButton - 4;
    }
  }
  if (strcmp(LMRXmode, "terr") == 0) // Add index for second set of freqs
  {
    FreqIndex = FreqIndex + 10;
    strcpy(Param, "tfreq");
  }
  else
  {
    Offset_to_Apply = LMRXqoffset;
    strcpy(Param, "qfreq");
  }

  // Define request string
  strcpy(RequestText, "Enter new receive frequency in MHz");

  // Define initial value and convert to MHz

  if(LMRXfreq[FreqIndex] < 50000)  // below 50 MHz, so set to 146.5
  {
    strcpy(InitText, "146.5");
  }
  else
  {
    div_10 = div(LMRXfreq[FreqIndex], 10);
    div_1000 = div(LMRXfreq[FreqIndex], 1000);

    if(div_10.rem != 0)  // last character not zero, so make answer of form xxx.xxx
    {
      snprintf(InitText, 10, "%d.%03d", div_1000.quot, div_1000.rem);
    }
    else
    {
      div_100 = div(LMRXfreq[FreqIndex], 100);

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

  // Ask for new value
  while ((CheckValue - Offset_to_Apply < 50000) || (CheckValue - Offset_to_Apply > 2600000))
  {
    Keyboard(RequestText, InitText, 10);
    CheckValue = (int)((1000 * atof(KeyboardReturn)) + 0.1);
    printf("CheckValue = %d Offset = %d\n", CheckValue, Offset_to_Apply);
  }

  // Write freq to memory
  LMRXfreq[FreqIndex] = CheckValue;

  // Convert to string in kHz
  snprintf(FreqkHz, 10, "%d", CheckValue);

  // Write to Presets File as in-use frequency
  if (strcmp(LMRXmode, "terr") == 0) // Terrestrial
  {
    SetConfigParam(PATH_LMCONFIG, "freq1", FreqkHz);        // Set in-use freq
    FreqIndex = FreqIndex - 10;                             // subtract index for terrestrial freqs
  }
  else                               // Sat
  {
    SetConfigParam(PATH_LMCONFIG, "freq0", FreqkHz);        // Set in-use freq
  }

  // write freq to Stored Presets file
  snprintf(PresetNo, 3, "%d", FreqIndex);
  strcat(Param, PresetNo); 
  SetConfigParam(PATH_LMCONFIG, Param, FreqkHz);
}


void ChangePresetSR(int NoButton)
{
  char RequestText[64];
  char InitText[64];
  char PresetNo[2];
  int SRIndex;
  int SRCheck = 0;

  if (NoButton < 4)
  {
    SRIndex = NoButton + 5;
  }
  else
  {
    SRIndex = NoButton - 5;
  }

  while ((SRCheck < 30) || (SRCheck > 9999))
  {
    strcpy(RequestText, "Enter new Symbol Rate for Button ");
    snprintf(PresetNo, 2, "%d", SRIndex + 1);
    strcat(RequestText, PresetNo);
    strcat(RequestText, " in KS/s:");
    snprintf(InitText, 5, "%d", TabSR[SRIndex]);
    Keyboard(RequestText, InitText, 4);
  
    // Check valid value
    SRCheck = atoi(KeyboardReturn);
  }

  // Update stored value
  TabSR[SRIndex] = SRCheck;

  // write SR to Presets file
  char Param[8] = "psr";
  strcat(Param, PresetNo); 
  printf("Store Preset %s %s\n", Param, KeyboardReturn);
  SetConfigParam(PATH_PPRESETS, Param, KeyboardReturn);

  // Compose and update Button Text
  switch (strlen(KeyboardReturn))
  {
  case 2:
  case 3:
    strcpy(SRLabel[SRIndex], "SR ");
    strcat(SRLabel[SRIndex], KeyboardReturn);
    break;
  default:
    strcpy(SRLabel[SRIndex], "SR");
    strcat(SRLabel[SRIndex], KeyboardReturn);
    break;
  }

  // refresh Menu 28 buttons

  AmendButtonStatus(ButtonNumber(28, NoButton), 0, SRLabel[SRIndex], &Blue);
  AmendButtonStatus(ButtonNumber(28, NoButton), 1, SRLabel[SRIndex], &Green);

  // refresh Menu 17 buttons
  AmendButtonStatus(ButtonNumber(17, NoButton), 0, SRLabel[SRIndex], &Blue);
  AmendButtonStatus(ButtonNumber(17, NoButton), 1, SRLabel[SRIndex], &Green);

  // Undo button highlight
  // SetButtonStatus(ButtonNumber(28, NoButton), 0);
}


void ChangeLMPresetSR(int NoButton)
{
  char RequestText[64];
  char InitText[64];
  char PresetNo[4];
  char Param[7];
  int SRIndex;
  int SRCheck = 0;

  // Correct button numbers to index numbers
  if (NoButton < 4)          // bottom row
  {
    SRIndex = NoButton + 6;  
  }
  else if (NoButton < 10)    // second row
  {
    SRIndex = NoButton - 4; 
  }
  else if (NoButton == 20)  // Change preset 6 directly from Menu 8
  {
    SRIndex = 6;
  }

  if (SRIndex <= 6)  // Only act on valid buttons
  {
    snprintf(PresetNo, 5, "%d", SRIndex);
    if (strcmp(LMRXmode, "terr") == 0) // Add index for second set of SRs
    {
      SRIndex = SRIndex + 6;
      strcpy(Param, "tsr");
    }
    else
    {
      strcpy(Param, "qsr");
    }

    while ((SRCheck < 30) || (SRCheck > 29999))
    {
      strcpy(RequestText, "Enter new Symbol Rate");

      snprintf(InitText, 5, "%d", LMRXsr[SRIndex]);
      Keyboard(RequestText, InitText, 4);
  
      // Check valid value
      SRCheck = atoi(KeyboardReturn);
    }

    // Update stored preset value and in-use value
    LMRXsr[SRIndex] = SRCheck;
    LMRXsr[0] = SRCheck;

    // write SR to Config file for current
    if (strcmp(LMRXmode, "terr") == 0) // terrestrial
    {
      SetConfigParam(PATH_LMCONFIG, "sr1", KeyboardReturn);
      SRIndex = SRIndex - 6;
    }
    else
    {
      SetConfigParam(PATH_LMCONFIG, "sr0", KeyboardReturn);
    }

    // write SR to Presets file for preset
    snprintf(PresetNo, 3, "%d", SRIndex);
    strcat(Param, PresetNo); 
    SetConfigParam(PATH_LMCONFIG, Param, KeyboardReturn);
  }
}


void ChangeCall()
{
  char RequestText[64];
  char InitText[64];
  int Spaces = 1;
  int j;

  while (Spaces >= 1)
  {
    strcpy(RequestText, "Enter new Callsign (NO SPACES ALLOWED)");
    //snprintf(InitText, 24, "%s", CallSign);
    strcpyn(InitText, CallSign, 24);
    Keyboard(RequestText, InitText, 23);
  
    // Check that there are no spaces
    Spaces = 0;
    for (j = 0; j < strlen(KeyboardReturn); j = j + 1)
    {
      if (isspace(KeyboardReturn[j]))
      {
        Spaces = Spaces +1;
      }
    }

    // Check call length which must be > 0
    if (strlen(KeyboardReturn) == 0)
    {
        Spaces = Spaces +1;
    }
  }
  strcpy(CallSign, KeyboardReturn);
  printf("Callsign set to: %s\n", CallSign);
  SetConfigParam(PATH_PCONFIG, "call", KeyboardReturn);
}

void ChangeLocator()
{
  char RequestText[64];
  char InitText[64];
  bool IsValid = FALSE;
  char Locator10[10];
  char Locator6[7];

  //Retrieve (10 char) Current Locator from locator file
  GetConfigParam(PATH_LOCATORS, "mylocator", Locator10);

  while (IsValid == FALSE)
  {
    strcpy(RequestText, "Enter new Locator (6, 8 or 10 char)");
    snprintf(InitText, 11, "%s", Locator10);
    Keyboard(RequestText, InitText, 10);
  
    // Check locator is valid
    IsValid = CheckLocator(KeyboardReturn);
  }
  strcpy(Locator, KeyboardReturn);
  printf("Locator set to: %s\n", Locator);

  // Save Full locator to Locators file
  SetConfigParam(PATH_LOCATORS, "mylocator", KeyboardReturn);

  //Truncate to 6 Characters for Contest display
  strcpyn(Locator6, KeyboardReturn, 6);
  SetConfigParam(PATH_PCONFIG, "locator", Locator6);
  strcpy(Locator, Locator6);
}

void ReadContestSites()
{
  GetConfigParam(PATH_C_NUMBERS, "site1locator", Site1Locator10);
  GetConfigParam(PATH_C_NUMBERS, "site2locator", Site2Locator10);
  GetConfigParam(PATH_C_NUMBERS, "site3locator", Site3Locator10);
  GetConfigParam(PATH_C_NUMBERS, "site4locator", Site4Locator10);
  GetConfigParam(PATH_C_NUMBERS, "site5locator", Site5Locator10);
}

void ManageContestCodes(int NoButton)
{
  setForeColour(255, 255, 255);    // White text
  setBackColour(0, 0, 0);          // on Black
  const font_t *font_ptr = &font_dejavu_sans_28;
  int txtht =  font_ptr->ascent;
  int linepitch = (14 * txtht) / 10;
  int linenumber = 1;
  char SiteText[7];
  int band;
  char Param[31];
  char Value[25];
  char EntryText[127];
  char RequestText[64];
  char InitText[64];
  bool IsValid = FALSE;
  char Locator10[10];
  char Locator6[7];

  switch(NoButton)
  {
  case 0:                                     // edit
    if ((AwaitingContestNumberEditSeln) || (AwaitingContestNumberLoadSeln)
     || (AwaitingContestNumberSaveSeln) || (AwaitingContestNumberViewSeln))
    {                                                                        // hanging selection
      AwaitingContestNumberEditSeln = false;
      AwaitingContestNumberLoadSeln = false;
      AwaitingContestNumberSaveSeln = false;
      AwaitingContestNumberViewSeln = false;
    }
    else                                                                     // valid selection
    {
      AwaitingContestNumberEditSeln = true;
    }
    break;                         
  case 1:                                     // load
    if ((AwaitingContestNumberEditSeln) || (AwaitingContestNumberLoadSeln)
     || (AwaitingContestNumberSaveSeln) || (AwaitingContestNumberViewSeln))
    {                                                                        // hanging selection
      AwaitingContestNumberEditSeln = false;
      AwaitingContestNumberLoadSeln = false;
      AwaitingContestNumberSaveSeln = false;
      AwaitingContestNumberViewSeln = false;
    }
    else                                                                     // valid selection
    {
      AwaitingContestNumberLoadSeln = true;
    }
    break;                         
  case 2:                                     // save
    if ((AwaitingContestNumberEditSeln) || (AwaitingContestNumberLoadSeln)
     || (AwaitingContestNumberSaveSeln))
    {                                                                        // hanging selection
      AwaitingContestNumberEditSeln = false;
      AwaitingContestNumberLoadSeln = false;
      AwaitingContestNumberSaveSeln = false;
      AwaitingContestNumberViewSeln = false;
    }
    else                                                                     // valid selection
    {
      if (AwaitingContestNumberViewSeln == false)
      {
        AwaitingContestNumberSaveSeln = true;
      }
      else
      {
        snprintf(SiteText, 7, "site%d", NoButton - 4);    // create the base for the parameter label
        clearScreen();

        strcpy(Param, "locator");
        GetConfigParam(PATH_PCONFIG, Param, Value);
        snprintf(EntryText, 90, "Contest Numbers for Current Site at %s", Value);
        TextMid2(wscreen / 2.0, hscreen - linenumber * linepitch, EntryText, font_ptr);
        linenumber = linenumber + 2;

        GetConfigParam(PATH_PPRESETS, "d0numbers", Value);                         // Band d0 50 MHz
        snprintf(EntryText, 90, "%d", TabBandExpPorts[14]);
        Text2(wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
        Text2(3 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[14], font_ptr);
        Text2(8 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);

        GetConfigParam(PATH_PPRESETS, "t0numbers", Value);                         // Band t0 50 MHz
        snprintf(EntryText, 90, "%d", TabBandExpPorts[9]);
        Text2(14 * wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
        Text2(16 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[9], font_ptr);
        Text2(21 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);
        linenumber = linenumber + 1;

        GetConfigParam(PATH_PPRESETS, "d1numbers", Value);                         // Band d1 71 MHz
        snprintf(EntryText, 90, "%d", TabBandExpPorts[0]);
        Text2(wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
        Text2(3 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[0], font_ptr);
        Text2(8 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);

        GetConfigParam(PATH_PPRESETS, "t5numbers", Value);                         // Band t5 13 cm
        snprintf(EntryText, 90, "%d", TabBandExpPorts[10]);
        Text2(14 * wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
        Text2(16 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[10], font_ptr);
        Text2(21 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);
        linenumber = linenumber + 1;

        GetConfigParam(PATH_PPRESETS, "d2numbers", Value);                         // Band d2 146 MHz
        snprintf(EntryText, 90, "%d", TabBandExpPorts[1]);
        Text2(wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
        Text2(3 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[1], font_ptr);
        Text2(8 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);

        GetConfigParam(PATH_PPRESETS, "t1numbers", Value);                         // Band t1 9 cm
        snprintf(EntryText, 90, "%d", TabBandExpPorts[5]);
        Text2(14 * wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
        Text2(16 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[5], font_ptr);
        Text2(21 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);
        linenumber = linenumber + 1;

        GetConfigParam(PATH_PPRESETS, "d3numbers", Value);                         // Band d3 437 MHz
        snprintf(EntryText, 90, "%d", TabBandExpPorts[2]);
        Text2(wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
        Text2(3 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[2], font_ptr);
        Text2(8 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);

        GetConfigParam(PATH_PPRESETS, "t2numbers", Value);                         // Band t2 6 cm
        snprintf(EntryText, 90, "%d", TabBandExpPorts[6]);
        Text2(14 * wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
        Text2(16 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[6], font_ptr);
        Text2(21 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);
        linenumber = linenumber + 1;

        GetConfigParam(PATH_PPRESETS, "d4numbers", Value);                         // Band d4 1255 MHz
        snprintf(EntryText, 90, "%d", TabBandExpPorts[3]);
        Text2(wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
        Text2(3 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[3], font_ptr);
        Text2(8 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);

        GetConfigParam(PATH_PPRESETS, "t3numbers", Value);                         // Band t3 3 cm
        snprintf(EntryText, 90, "%d", TabBandExpPorts[7]);
        Text2(14 * wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
        Text2(16 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[7], font_ptr);
        Text2(21 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);
        linenumber = linenumber + 1;

        GetConfigParam(PATH_PPRESETS, "d5numbers", Value);                         // Band d5 2400 MHz
        snprintf(EntryText, 90, "%d", TabBandExpPorts[4]);
        Text2(wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
        Text2(3 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[4], font_ptr);
        Text2(8 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);

        GetConfigParam(PATH_PPRESETS, "t4numbers", Value);                         // Band t4 24 GHz
        snprintf(EntryText, 90, "%d", TabBandExpPorts[8]);
        Text2(14 * wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
        Text2(16 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[8], font_ptr);
        Text2(21 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);
        linenumber = linenumber + 1;

        GetConfigParam(PATH_PPRESETS, "d6numbers", Value);                         // Band d6 3400 MHz
        snprintf(EntryText, 90, "%d", TabBandExpPorts[15]);
        Text2(wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
        Text2(3 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[15], font_ptr);
        Text2(8 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);

        GetConfigParam(PATH_PPRESETS, "t6numbers", Value);                         // Band t6 47 GHz
        snprintf(EntryText, 90, "%d", TabBandExpPorts[11]);
        Text2(14 * wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
        Text2(16 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[11], font_ptr);
        Text2(21 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);
        linenumber = linenumber + 1;

        GetConfigParam(PATH_PPRESETS, "t7numbers", Value);                         // Band t7 76 GHz
        snprintf(EntryText, 90, "%d", TabBandExpPorts[12]);
        Text2(14 * wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
        Text2(16 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[12], font_ptr);
        Text2(21 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);
        linenumber = linenumber + 1;

        GetConfigParam(PATH_PPRESETS, "t8numbers", Value);                         // Band t8 Spare
        snprintf(EntryText, 90, "%d", TabBandExpPorts[13]);
        Text2(14 * wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
        Text2(16 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[13], font_ptr);
        Text2(21 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);
        linenumber = linenumber + 1;

        TextMid2(wscreen / 2, hscreen - linenumber * linepitch, "Touch Screen to Continue", font_ptr);

        printf("Contest number Screen called and waiting for touch\n");
        wait_touch();
        AwaitingContestNumberViewSeln = false;
      }
    }
    break;                         
  case 3:                                     // show
    if ((AwaitingContestNumberEditSeln) || (AwaitingContestNumberLoadSeln)
     || (AwaitingContestNumberSaveSeln) || (AwaitingContestNumberViewSeln))
    {                                                                        // hanging selection
      AwaitingContestNumberEditSeln = false;
      AwaitingContestNumberLoadSeln = false;
      AwaitingContestNumberSaveSeln = false;
      AwaitingContestNumberViewSeln = false;
    }
    else                                                                     // valid selection
    {
      AwaitingContestNumberViewSeln = true;
    }
    break;
  case 5:                                     //Selected Site
  case 6:
  case 7:
  case 8:
  case 9:
    snprintf(SiteText, 7, "site%d", NoButton - 4);    // create the base for the parameter label

    if (AwaitingContestNumberViewSeln)
    {
      clearScreen();

      strcpy(Param, SiteText);                          // Title Line
      strcat(Param, "locator");
      GetConfigParam(PATH_C_NUMBERS, Param, Value);
      snprintf(EntryText, 90, "Contest Numbers for Site %d at %s", NoButton - 4, Value);
      TextMid2(wscreen / 2.0, hscreen - linenumber * linepitch, EntryText, font_ptr);
      linenumber = linenumber + 2;

      strcpy(Param, SiteText);                         // Band d0 50 MHz
      strcat(Param, "d0numbers");
      GetConfigParam(PATH_C_NUMBERS, Param, Value);
      snprintf(EntryText, 90, "%d", TabBandExpPorts[14]);
      Text2(wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
      Text2(3 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[14], font_ptr);
      Text2(8 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);

      strcpy(Param, SiteText);                         // Band t0 50 MHz
      strcat(Param, "t0numbers");
      GetConfigParam(PATH_C_NUMBERS, Param, Value);
      snprintf(EntryText, 90, "%d", TabBandExpPorts[9]);
      Text2(14 * wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
      Text2(16 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[9], font_ptr);
      Text2(21 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);
      linenumber = linenumber + 1;

      strcpy(Param, SiteText);                         // Band d1 71 MHz
      strcat(Param, "d1numbers");
      GetConfigParam(PATH_C_NUMBERS, Param, Value);
      snprintf(EntryText, 90, "%d", TabBandExpPorts[0]);
      Text2(wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
      Text2(3 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[0], font_ptr);
      Text2(8 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);

      strcpy(Param, SiteText);                         // Band t5 13 cm
      strcat(Param, "t5numbers");
      GetConfigParam(PATH_C_NUMBERS, Param, Value);
      snprintf(EntryText, 90, "%d", TabBandExpPorts[10]);
      Text2(14 * wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
      Text2(16 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[10], font_ptr);
      Text2(21 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);
      linenumber = linenumber + 1;

      strcpy(Param, SiteText);                         // Band d2 146 MHz
      strcat(Param, "d2numbers");
      GetConfigParam(PATH_C_NUMBERS, Param, Value);
      snprintf(EntryText, 90, "%d", TabBandExpPorts[1]);
      Text2(wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
      Text2(3 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[1], font_ptr);
      Text2(8 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);

      strcpy(Param, SiteText);                         // Band t1 9 cm
      strcat(Param, "t1numbers");
      GetConfigParam(PATH_C_NUMBERS, Param, Value);
      snprintf(EntryText, 90, "%d", TabBandExpPorts[5]);
      Text2(14 * wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
      Text2(16 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[5], font_ptr);
      Text2(21 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);
      linenumber = linenumber + 1;

      strcpy(Param, SiteText);                         // Band d3 437 MHz
      strcat(Param, "d3numbers");
      GetConfigParam(PATH_C_NUMBERS, Param, Value);
      snprintf(EntryText, 90, "%d", TabBandExpPorts[2]);
      Text2(wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
      Text2(3 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[2], font_ptr);
      Text2(8 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);

      strcpy(Param, SiteText);                         // Band t2 6 cm
      strcat(Param, "t2numbers");
      GetConfigParam(PATH_C_NUMBERS, Param, Value);
      snprintf(EntryText, 90, "%d", TabBandExpPorts[6]);
      Text2(14 * wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
      Text2(16 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[6], font_ptr);
      Text2(21 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);
      linenumber = linenumber + 1;

      strcpy(Param, SiteText);                         // Band d4 1255 MHz
      strcat(Param, "d4numbers");
      GetConfigParam(PATH_C_NUMBERS, Param, Value);
      snprintf(EntryText, 90, "%d", TabBandExpPorts[3]);
      Text2(wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
      Text2(3 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[3], font_ptr);
      Text2(8 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);

      strcpy(Param, SiteText);                         // Band t3 3 cm
      strcat(Param, "t3numbers");
      GetConfigParam(PATH_C_NUMBERS, Param, Value);
      snprintf(EntryText, 90, "%d", TabBandExpPorts[7]);
      Text2(14 * wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
      Text2(16 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[7], font_ptr);
      Text2(21 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);
      linenumber = linenumber + 1;

      strcpy(Param, SiteText);                         // Band d5 2400 MHz
      strcat(Param, "d5numbers");
      GetConfigParam(PATH_C_NUMBERS, Param, Value);
      snprintf(EntryText, 90, "%d", TabBandExpPorts[4]);
      Text2(wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
      Text2(3 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[4], font_ptr);
      Text2(8 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);

      strcpy(Param, SiteText);                         // Band t4 24 GHz
      strcat(Param, "t4numbers");
      GetConfigParam(PATH_C_NUMBERS, Param, Value);
      snprintf(EntryText, 90, "%d", TabBandExpPorts[8]);
      Text2(14 * wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
      Text2(16 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[8], font_ptr);
      Text2(21 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);
      linenumber = linenumber + 1;

      strcpy(Param, SiteText);                         // Band d6 3400 MHz
      strcat(Param, "d6numbers");
      GetConfigParam(PATH_C_NUMBERS, Param, Value);
      snprintf(EntryText, 90, "%d", TabBandExpPorts[15]);
      Text2(wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
      Text2(3 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[15], font_ptr);
      Text2(8 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);

      strcpy(Param, SiteText);                         // Band t6 47 GHz
      strcat(Param, "t6numbers");
      GetConfigParam(PATH_C_NUMBERS, Param, Value);
      snprintf(EntryText, 90, "%d", TabBandExpPorts[11]);
      Text2(14 * wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
      Text2(16 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[11], font_ptr);
      Text2(21 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);
      linenumber = linenumber + 1;

      strcpy(Param, SiteText);                         // Band t7 76 GHz
      strcat(Param, "t7numbers");
      GetConfigParam(PATH_C_NUMBERS, Param, Value);
      snprintf(EntryText, 90, "%d", TabBandExpPorts[12]);
      Text2(14 * wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
      Text2(16 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[12], font_ptr);
      Text2(21 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);
      linenumber = linenumber + 1;

      strcpy(Param, SiteText);                         // Band t8 Spare
      strcat(Param, "t8numbers");
      GetConfigParam(PATH_C_NUMBERS, Param, Value);
      snprintf(EntryText, 90, "%d", TabBandExpPorts[13]);
      Text2(14 * wscreen/25, hscreen - linenumber * linepitch, EntryText, font_ptr);
      Text2(16 * wscreen/25, hscreen - linenumber * linepitch, TabBandLabel[13], font_ptr);
      Text2(21 * wscreen/25, hscreen - linenumber * linepitch, Value, font_ptr);
      linenumber = linenumber + 1;

      TextMid2(wscreen / 2, hscreen - linenumber * linepitch, "Touch Screen to Continue", font_ptr);

      draw_cursor_foreground(mouse_x, mouse_y);
      UpdateWeb();

      printf("Contest number Screen called and waiting for touch\n");
      wait_touch();
      AwaitingContestNumberViewSeln = false;
    }

    if (AwaitingContestNumberEditSeln)
    {
      snprintf(RequestText, 63, "Enter or confirm locator for Site %d", NoButton - 4);
      strcpy(Param, SiteText);
      strcat(Param, "locator");
      GetConfigParam(PATH_C_NUMBERS, Param, InitText);
      while (IsValid == FALSE)
      {
        Keyboard(RequestText, InitText, 10);
        // Check locator is valid
        IsValid = CheckLocator(KeyboardReturn);
      }

      // Save Full locator to Contest Numbers file
      SetConfigParam(PATH_C_NUMBERS, Param, KeyboardReturn);
      // and save to Site*Locator10
      switch(NoButton)
      {
      case 5:
        strcpy(Site1Locator10, KeyboardReturn);
        break;
      case 6:
        strcpy(Site2Locator10, KeyboardReturn);
        break;
      case 7:
        strcpy(Site3Locator10, KeyboardReturn);
        break;
      case 8:
        strcpy(Site4Locator10, KeyboardReturn);
        break;
      case 9:
        strcpy(Site5Locator10, KeyboardReturn);
        break;
      }

      for (band = 0; band < 16; band++)
      {
        snprintf(RequestText, 63, "Enter or confirm Contest Numbers for the %s band:", TabBandLabel[band]);
        strcpy(Param, SiteText);
        strcat(Param, TabBand[band]);
        strcat(Param, "numbers");
        GetConfigParam(PATH_C_NUMBERS, Param, InitText);
        strcpy(KeyboardReturn, "");
        while (strlen(KeyboardReturn) < 1)
        {
          snprintf(RequestText, 63, "Enter Contest Numbers for the %s Band:", TabBandLabel[band]);
          Keyboard(RequestText, InitText, 10);
        }
        SetConfigParam(PATH_C_NUMBERS, Param, KeyboardReturn);
      }
      AwaitingContestNumberEditSeln = false;
    }

    if (AwaitingContestNumberLoadSeln)
    {
      // Sort the locator first
      strcpy(Param, SiteText);
      strcat(Param, "locator");
      GetConfigParam(PATH_C_NUMBERS, Param, Locator10);

      // Save Full locator to Locators file
      SetConfigParam(PATH_LOCATORS, "mylocator", Locator10);

      //Truncate to 6 Characters for Contest display
      strcpyn(Locator6, Locator10, 6);
      SetConfigParam(PATH_PCONFIG, "locator", Locator6);
      strcpy(Locator, Locator6);

      // Contest Numbers
      for (band = 0; band < 16; band++)
      {
        // Read the value
        strcpy(Param, SiteText);
        strcat(Param, TabBand[band]);
        strcat(Param, "numbers");
        GetConfigParam(PATH_C_NUMBERS, Param, Value);

        // And write it to the Presets file
        strcpy(Param, TabBand[band]);
        strcat(Param, "numbers");
        SetConfigParam(PATH_PPRESETS, Param, Value);
        strcpy(TabBandNumbers[band], Value);

        if (band == CurrentBand) // Write to the current file
        {
          SetConfigParam(PATH_PCONFIG, "numbers", Value);
        }
      }
      AwaitingContestNumberLoadSeln = false;
    }

    if (AwaitingContestNumberSaveSeln)
    {
      // Sort the locator first
      // Get the Full locator from the Locators file
      GetConfigParam(PATH_LOCATORS, "mylocator", Locator10);

      // Write it to the Contest File
      strcpy(Param, SiteText);
      strcat(Param, "locator");
      SetConfigParam(PATH_C_NUMBERS, Param, Locator10);

      // and save it to Site*Locator10
      switch(NoButton)
      {
      case 5:
        strcpy(Site1Locator10, Locator10);
        break;
      case 6:
        strcpy(Site2Locator10, Locator10);
        break;
      case 7:
        strcpy(Site3Locator10, Locator10);
        break;
      case 8:
        strcpy(Site4Locator10, Locator10);
        break;
      case 9:
        strcpy(Site5Locator10, Locator10);
        break;
      }

      // Contest Numbers
      for (band = 0; band < 16; band++)
      {
        // Read the value from the Presets file
        strcpy(Param, TabBand[band]);
        strcat(Param, "numbers");
        GetConfigParam(PATH_PPRESETS, Param, Value);

        // And write it to the Contests file
        strcpy(Param, SiteText);
        strcat(Param, TabBand[band]);
        strcat(Param, "numbers");
        SetConfigParam(PATH_C_NUMBERS, Param, Value);
      }
      AwaitingContestNumberSaveSeln = false;
    }
    break;                   
  }
}

void ChangeStartApp(int NoButton)
{
  switch(NoButton)
  {
  case 0:                          
    SetConfigParam(PATH_PCONFIG, "startup", "Testmenu_boot");
    strcpy(StartApp, "Testmenu_boot");
    break;
  case 1:                          
    SetConfigParam(PATH_PCONFIG, "startup", "Transmit_boot");
    strcpy(StartApp, "Transmit_boot");
    break;
  case 2:                          
    SetConfigParam(PATH_PCONFIG, "startup", "Receive_boot");
    strcpy(StartApp, "Receive_boot");
    break;
  case 3:                          
    SetConfigParam(PATH_PCONFIG, "startup", "Ryde_boot");
    strcpy(StartApp, "Ryde_boot");
    break;
  case 5:                          
    SetConfigParam(PATH_PCONFIG, "startup", "Display_boot");
    strcpy(StartApp, "Display_boot");
    break;
  case 6:
    if ((strcmp(langstone_version, "v1pluto") == 0) || (strcmp(langstone_version, "v2lime") == 0)
     || (strcmp(langstone_version, "v2pluto") == 0))
    {
      SetConfigParam(PATH_PCONFIG, "startup", "Langstone_boot");
      strcpy(StartApp, "Langstone_boot");
    }
    break;
  case 7:
    SetConfigParam(PATH_PCONFIG, "startup", "Bandview_boot");
    strcpy(StartApp, "Bandview_boot");
    break;
  case 8:
    SetConfigParam(PATH_PCONFIG, "startup", "Meteorview_boot");
    strcpy(StartApp, "Meteorview_boot");
    break;
  default:
    break;
  }
}

void ChangePlutoIP()  // For Portsdown
{
  char RequestText[64];
  char InitText[64];
  bool IsValid = FALSE;
  char PlutoIPCopy[31];

  while (IsValid == FALSE)
  {
    strcpy(RequestText, "Enter IP address for Portsdown Pluto");
    strcpyn(InitText, PlutoIP, 17);
    Keyboard(RequestText, InitText, 17);
  
    strcpy(PlutoIPCopy, KeyboardReturn);
    if(is_valid_ip(PlutoIPCopy) == 1)
    {
      IsValid = TRUE;
    }
  }
  printf("portsdown Pluto IP set to: %s\n", KeyboardReturn);

  // Save IP to config file
  SetConfigParam(PATH_PCONFIG, "plutoip", KeyboardReturn);
  strcpy(PlutoIP, KeyboardReturn);
}

void ChangePlutoIPLangstone()  // For Langstone
{
  char RequestText[64];
  char InitText[64];
  bool IsValid = FALSE;
  char PlutoIPCopy[31];
  char LinuxCommand[255];

  // Check if Langstone is not loaded
  if ((strcmp(langstone_version, "v1pluto") != 0)
  &&  (strcmp(langstone_version, "v2lime") != 0)
  &&  (strcmp(langstone_version, "v2pluto") != 0))
  {
    MsgBox2("Langstone not installed", "Please install Langstone, then set Pluto IP");
    wait_touch();
    return;
  }
  // then need to read in Pluto Langstone IP
  CheckLangstonePlutoIP();

  while (IsValid == FALSE)
  {
    strcpy(RequestText, "Enter IP address for Pluto");
    strcpyn(InitText, LangstonePlutoIP, 17);
    Keyboard(RequestText, InitText, 17);
  
    strcpy(PlutoIPCopy, KeyboardReturn);
    if((is_valid_ip(PlutoIPCopy) == 1) || (strcmp(KeyboardReturn, "pluto.local") == 0))
    {
      IsValid = TRUE;
    }
  }

  if(strcmp(LangstonePlutoIP, KeyboardReturn) == 0)   // Unchanged
  {
    printf("Langstone Pluto IP unchanged as %s\n", KeyboardReturn);
  }
  else                                              // Changed
  {
    printf("Langstone Pluto IP set to: %s\n", KeyboardReturn);
    strcpy(LangstonePlutoIP, KeyboardReturn);

    if (strcmp(langstone_version, "v1pluto") == 0)
    {
      // Save IP to Langstone/run file
      snprintf(LinuxCommand, 254, "sed -i \"s/^export PLUTO_IP=.*/export PLUTO_IP=%s/\" /home/pi/Langstone/run", LangstonePlutoIP);
      system(LinuxCommand);

      // Save IP to Langstone/stop file
      snprintf(LinuxCommand, 254, "sed -i \"s/PLUTO_IP=.*/PLUTO_IP=%s/\" /home/pi/Langstone/stop", LangstonePlutoIP);
      system(LinuxCommand);
    }
    if ((strcmp(langstone_version, "v2pluto") == 0)
     || (strcmp(langstone_version, "v2lime") == 0))
    {
      // Save IP to Langstone/run_pluto file
      snprintf(LinuxCommand, 254, "sed -i \"s/^export PLUTO_IP=.*/export PLUTO_IP=%s/\" /home/pi/Langstone/run_pluto", LangstonePlutoIP);
      system(LinuxCommand);

      // Save IP to Langstone/stop_pluto file
      snprintf(LinuxCommand, 254, "sed -i \"s/PLUTO_IP=.*/PLUTO_IP=%s/\" /home/pi/Langstone/stop_pluto", LangstonePlutoIP);
      system(LinuxCommand);
    }
  }
}

void ChangePlutoXO()
{
  char RequestText[63];
  char InitText[63];
  char cmdText[63];
  char msgText[63];
  int Spaces = 1;
  int j;
  int newXO = 0;
  int oldXO;
  int checkXO;

  oldXO = GetPlutoXO();

  snprintf(InitText, 10, "%d", oldXO);
  snprintf(RequestText, 45, "Enter new Pluto Reference Frequency in Hz");

  while ((Spaces >= 1) || (newXO < 9000000) || (newXO > 110000000))
  {
    Keyboard(RequestText, InitText, 9);
  
    // Check that there are no spaces or other characters
    Spaces = 0;
    for (j = 0; j < strlen(KeyboardReturn); j = j + 1)
    {
      if ( !(isdigit(KeyboardReturn[j])) )
      {
        Spaces = Spaces + 1;
      }
    }
    newXO = atoi(KeyboardReturn);
  }
  
  if (oldXO != newXO)
  {
    snprintf(cmdText, 62, "/home/pi/rpidatv/scripts/pluto_set_ref.sh %d", newXO);
    system(cmdText);
    printf("Pluto Ref set to: %d\n", newXO);

    checkXO = GetPlutoXO();
    snprintf(msgText, 62, "Pluto Ref Freq Changed to %d", checkXO);
    MsgBox(msgText);
  }
  else
  {
    snprintf(msgText, 62, "Pluto Ref Freq unchanged at %d", oldXO);
    MsgBox(msgText);
  }
  wait_touch();
}


void ChangePlutoAD()
{
  // Checks Whether Pluto has been expanded to AD9364 status and does it if required
  int oldAD;

  oldAD = GetPlutoAD();

  if (oldAD == 9364)
  {
    MsgBox("Pluto range is expanded to AD 9364 status");
    wait_touch();
  }
  else
  {
    MsgBox2("Pluto range is not expanded to AD 9364 status", "Expand from next Menu if required");
    wait_touch();
    // Set the status of the button to 1
    SetButtonStatus(ButtonNumber(15, 8), 1);
  }
}


void ChangePlutoCPU()
{
  // Checks whether Pluto has been expanded to use 2 cpus and does it if required
  int oldCPU;

  oldCPU = GetPlutoCPU();

  if (oldCPU == 2)
  {
    MsgBox("Both Pluto CPUs enabled");
    wait_touch();
  }

  if (oldCPU == 1)
  {
    MsgBox2("Only a single CPU is enabled", "Enable both from next Menu if required");
    wait_touch();
    // Set the status of the change action button to 1
    SetButtonStatus(ButtonNumber(15, 6), 1);
  }
}


// Pluto reboot
// context 0, straight reboot, waits for touch
// context 1, Portsdown to Langstone reboot with message
// context 2, Langstone (or SigGen) to Portsdown reboot with message
// context 3, Portsdown to SigGen reboot with message

void RebootPluto(int context)
{
  int test = 1;
  int count = 0;
  int touchcheckcount = 0;
  char timetext[63];
  FinishedButton = 0;

  if (context == 0)  // Straight reboot
  {
    system("/home/pi/rpidatv/scripts/reboot_pluto.sh");
    MsgBox4("Pluto Rebooting", "Wait for reconnection", "Touch to cancel wait", "Timeout in 24 seconds");
    usleep(500000);  // Give time for selecting touch to clear
    while(test == 1)
    {
      snprintf(timetext, 62, "Timeout in %d seconds", 24 - count);
      MsgBox4("Pluto Rebooting", "Wait for reconnection", "Touch to cancel wait", timetext);
      test = CheckPlutoIPConnect();
      count = count + 1;

      // Now monitor screen and web for cancel touch
      for (touchcheckcount = 0; touchcheckcount < 1000; touchcheckcount++)
      {
        if (TouchTrigger == 1)
        {
          TouchTrigger = 0;
          test = 9;
          break;
        }
        else if ((webcontrol == true) && (strcmp(WebClickForAction, "yes") == 0))
        {
          strcpy(WebClickForAction, "no");
          test = 9;
          break;
        }
        else
        {
          usleep(1000);
        }
      }

      if (count > 24)
      {
        MsgBox4("Failed to Reconnect","to Pluto", " ", "Touch Screen to Continue");
        wait_touch();
        return;
      }
    }
    if (test == 9)
    {
      MsgBox4("Pluto Reboot Monitoring Cancelled"," ", " ", " ");
      usleep(1000000);
    }
    else
    {
      MsgBox4("Pluto Rebooted"," ", " ", "Touch Screen to Continue");
      wait_touch();
    }
    return;
  }

  if (context == 1)  // Switching to Langstone
  {
    system("/home/pi/rpidatv/scripts/reboot_pluto.sh");

    while(test == 1)
    {
      snprintf(timetext, 62, "Timeout in %d seconds", 24 - count);
      MsgBox4("Pluto Rebooting", "Langstone will be selected", "once Pluto has rebooted", timetext);
      usleep(1000000);
      test = CheckPlutoIPConnect();
      count = count + 1;
      if (count > 24)
      {
        MsgBox4("Failed to Reconnect","to Pluto", "You may need to recycle the power", "Touch Screen to Continue");
        wait_touch();
        return;
      }
    }
    return;
  }

  if ((context == 2) && (CheckPlutoIPConnect() == 1))  // On start or entry from Langstone or SigGen with no Pluto
  {
    MsgBox4("Pluto may be rebooting", "Portsdown will start once Pluto has rebooted", "Touch to cancel wait", "Timeout in 24 seconds");
    usleep(500000);  // Give time for selecting touch to clear

    while(test == 1)
    {
      snprintf(timetext, 62, "Timeout in %d seconds", 24 - count);
      MsgBox4("Pluto may be rebooting", "Portsdown will start once Pluto has rebooted", 
              "Touch to cancel wait", timetext);
      //usleep(1000000);
      test = CheckPlutoIPConnect();
      count = count + 1;

      // Now monitor screen and web for cancel touch
      for (touchcheckcount = 0; touchcheckcount < 1000; touchcheckcount++)
      {
        if (TouchTrigger == 1)
        {
          TouchTrigger = 0;
          test = 9;
          break;
        }
        else if ((webcontrol == true) && (strcmp(WebClickForAction, "yes") == 0))
        {
          strcpy(WebClickForAction, "no");
          test = 9;
          break;
        }
        else
        {
          usleep(1000);
        }
      }
      if (count > 24)
      {
        MsgBox4("Failed to reconnect to the Pluto", "You may need to recycle the power", " ", "Touch Screen to Continue");
        wait_touch();
        return;
      }
    }
    if (test == 9)
    {
      MsgBox4("Pluto Reboot Monitoring Cancelled"," ", " ", " ");
      usleep(1000000);
    }
  }
}


void CheckPlutoFirmware()
{
  FILE *fp;
  char firmware_response_line[127]=" ";
  char firmware_version[127];

  MsgBox4("Checking Pluto", "", "Please wait", "");

  if (CheckPlutoIPConnect() == 1)
  {
    MsgBox4("No Pluto Detected", "Check Config and Connections", "or disconnect/connect Pluto", "Touch screen to continue");
    wait_touch();
    return;
  }

  // Open the command for reading
  fp = popen("/home/pi/rpidatv/scripts/pluto_fw_version.sh", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  // Read the output a line at a time
  while (fgets(firmware_response_line, 250, fp) != NULL)
  {
    if (firmware_response_line[0] == 'P')
    {
      strcpyn(firmware_version, firmware_response_line, 45);
    }
  }
  pclose(fp);

  if (strcmp(firmware_version, "Pluto Firmware Version is v0.31-4-g9ceb-dirty") == 0)
  {
    MsgBox4(firmware_version, "This is correct for Portsdown operation", " ", "Touch Screen to Continue");
  }
  else
  {
    MsgBox4(firmware_version, "This may not work with Portsdown", "Please update to v0.31-4-g9ceb-dirty", "Touch Screen to Continue");
  }
  wait_touch();
}


void UpdateLangstone(int version_number)
{
  if (CheckGoogle() == 0)  // First check internet conection
  {
    if (version_number == 1)
    {
      system("/home/pi/rpidatv/scripts/update_langstone.sh");
      usleep(1000000);
      if (file_exist("/home/pi/Langstone/GUI") == 0)
      {
        MsgBox4("Langstone V1 Successfully Updated"," ", " ", "Touch Screen to Continue");
      }
      else
      {
        MsgBox4("Langstone not Updated","Try again, ", "or try manual update", " ");
      }
    }
    if (version_number == 2)
    {
      system("/home/pi/rpidatv/scripts/update_langstone2.sh");
      usleep(1000000);
      if (file_exist("/home/pi/Langstone/LangstoneGUI_Lime.c") == 0)
      {
        MsgBox4("Langstone V2 Successfully Updated"," ", " ", "Touch Screen to Continue");
      }
      else
      {
        MsgBox4("Langstone not Updated","Try again, ", "or try manual update", " ");
      }
    }
  }
  else
  {
    MsgBox4("Unable to contact GitHub for update", "There appears to be no Internet connection", \
     "Please check your connection and try again", "Touch Screen to Continue");
  }
  wait_touch();
  UpdateWindow();
}


void InstallLangstone(int NoButton)
{
  int i;

  if (CheckGoogle() == 0)  // First check internet conection
  {
    if (NoButton == 5)
    {
      system("/home/pi/rpidatv/add_langstone.sh &");
      MsgBox4("Installing Langstone V1 Software", "This can take up to 15 minutes", " ", "Please wait for Reboot");
    }
    if (NoButton == 6)
    {
      system("/home/pi/rpidatv/add_langstone2.sh &");
      MsgBox4("Installing Langstone V2 Software", "This can take up to 15 minutes", " ", "Please wait for Reboot");
    }

    for (i = 0; i < 900; i++)
    {
      usleep(1000000);
    }
  }
  else
  {
    MsgBox4("Unable to contact GitHub for Install", "There appears to be no Internet connection", \
     "Please check your connection and try again", "Touch Screen to Continue");
    wait_touch();
    UpdateWindow();
  }
}


void ChangeADFRef(int NoButton)
{
  char RequestText[64];
  char InitText[64];
  char Param[64];
  int Spaces = 1;
  int j;

  switch(NoButton)
  {
  case 5:
    strcpy(CurrentADFRef, ADFRef[0]);
    SetConfigParam(PATH_PCONFIG, "adfref", CurrentADFRef);
    break;
  case 6:
    strcpy(CurrentADFRef, ADFRef[1]);
    SetConfigParam(PATH_PCONFIG, "adfref", CurrentADFRef);
    break;
  case 7:
    strcpy(CurrentADFRef, ADFRef[2]);
    SetConfigParam(PATH_PCONFIG, "adfref", CurrentADFRef);
    break;
  case 0:
  case 1:
  case 2:
    snprintf(RequestText, 45, "Enter new ADF4351 Reference Frequncy %d in Hz", NoButton + 1);
    //snprintf(InitText, 10, "%s", ADFRef[NoButton]);
    strcpyn(InitText, ADFRef[NoButton], 10);
    while (Spaces >= 1)
    {
      Keyboard(RequestText, InitText, 9);
  
      // Check that there are no spaces or other characters
      Spaces = 0;
      for (j = 0; j < strlen(KeyboardReturn); j = j + 1)
      {
        if ( !(isdigit(KeyboardReturn[j])) )
        {
          Spaces = Spaces + 1;
        }
      }
    }
    strcpy(ADFRef[NoButton], KeyboardReturn);
    snprintf(Param, 8, "adfref%d", NoButton + 1);
    SetConfigParam(PATH_PPRESETS, Param, KeyboardReturn);
    strcpy(CurrentADFRef, KeyboardReturn);
    SetConfigParam(PATH_PCONFIG, "adfref", KeyboardReturn);
    break;
  case 3:
    snprintf(RequestText, 45, "Enter new ADF5355 Reference Frequncy in Hz");
    //snprintf(InitText, 10, "%s", ADF5355Ref);
    strcpyn(InitText, ADF5355Ref, 10);
    while (Spaces >= 1)
    {
      Keyboard(RequestText, InitText, 9);
  
      // Check that there are no spaces or other characters
      Spaces = 0;
      for (j = 0; j < strlen(KeyboardReturn); j = j + 1)
      {
        if ( !(isdigit(KeyboardReturn[j])) )
        {
          Spaces = Spaces + 1;
        }
      }
    }
    strcpy(ADF5355Ref, KeyboardReturn);
    SetConfigParam(PATH_SGCONFIG, "adf5355ref", KeyboardReturn);
    break;
  default:
    break;
  }
  printf("ADF4351Ref set to: %s\n", CurrentADFRef);
  printf("ADF5355Ref set to: %s\n", ADF5355Ref);
}

void ChangePID(int NoButton)
{
  char RequestText[64];
  char InitText[64];
  int PIDValid = 1;  // 0 represents valid
  int j;

  while (PIDValid >= 1)
  {
    switch (NoButton)
    {
    case 0:
      strcpy(RequestText, "Enter new Video PID (Range 16 - 8190, Rec: 256)");
      //snprintf(InitText, 5, "%s", PIDvideo);
      strcpyn(InitText, PIDvideo, 5);
      break;
    case 1:
      strcpy(RequestText, "Enter new Audio PID (Range 16 - 8190 Rec: 257)");
      //snprintf(InitText, 5, "%s", PIDaudio);
      strcpyn(InitText, PIDaudio, 5);
      break;
    case 2:
      strcpy(RequestText, "Enter new PMT PID (Range 16 - 8190 Rec: 4095)");
      //snprintf(InitText, 5, "%s", PIDpmt);
      strcpyn(InitText, PIDpmt, 5);
      break;
    default:
      return;
    }

    Keyboard(RequestText, InitText, 4);
  
    // Check that there are no spaces and it is valid
    PIDValid = 0;
    for (j = 0; j < strlen(KeyboardReturn); j = j + 1)
    {
      if (isspace(KeyboardReturn[j]))
      {
        PIDValid = PIDValid + 1;
      }
    }
    if ( PIDValid == 0)
    {
      if ((atoi(KeyboardReturn) < 16 ) || (atoi(KeyboardReturn) > 8190 ))
      {
        PIDValid = 1;   // Out of bounds
      }
    }
  }
  switch (NoButton)
  {
  case 0:
    strcpy(PIDvideo, KeyboardReturn);
    SetConfigParam(PATH_PCONFIG, "pidvideo", KeyboardReturn);
    strcpy(PIDstart, KeyboardReturn);
    SetConfigParam(PATH_PCONFIG, "pidstart", KeyboardReturn);
    break;
  case 1:
    strcpy(PIDaudio, KeyboardReturn);
    SetConfigParam(PATH_PCONFIG, "pidaudio", KeyboardReturn);
    break;
  case 2:
    strcpy(PIDpmt, KeyboardReturn);
    SetConfigParam(PATH_PCONFIG, "pidpmt", KeyboardReturn);
    break;
  }
  printf("PID set to: %s\n", KeyboardReturn);
}


void ToggleLimeRFE()
{
 // Set the correct band in portsdown_config.txt
  char bandtext [15];   
  GetConfigParam(PATH_PCONFIG, "band", bandtext);
  strcat(bandtext, "limerfe");

  if (LimeRFEState == 1)  // Enabled
  {
    LimeRFEState = 0;
    SetConfigParam(PATH_PCONFIG, "limerfe", "disabled");
    SetConfigParam(PATH_PPRESETS, bandtext, "disabled");
    LimeRFEClose();
  }
  else                    // Disabled
  {
    LimeRFEState = 1;
    SetConfigParam(PATH_PCONFIG, "limerfe", "enabled");
    SetConfigParam(PATH_PPRESETS, bandtext, "enabled");
    LimeRFEInit();
  }
}


void SetLimeRFERXAtt()
{
  char bandtext [15];
  char Param[31];
  char Prompt[127];
  char Value[31];
  int  RFEAtt;
   
  // Get the current band from portsdown_config.txt
  //GetConfigParam(PATH_PCONFIG, "band", bandtext);
  //strcat(bandtext, "limerfe");

  // Lime RFE Attenuator Level
  //strcpy(Param, TabBand[band]);
  GetConfigParam(PATH_PCONFIG, "band", bandtext);
  strcpy(Param, bandtext);
  strcat(Param, "limerferxatt");
  GetConfigParam(PATH_PPRESETS, Param, Value);
  RFEAtt=atoi(Value);
  snprintf(Value, 30, "%d", RFEAtt * 2);
  RFEAtt= - 1;
  while ((RFEAtt < 0) || (RFEAtt > 14) || (strlen(KeyboardReturn) < 1))  
  {
    snprintf(Prompt, 63, "Set LimeRFE Rx Atten for %s. 0-14 (dB):", TabBandLabel[CurrentBand]);
    Keyboard(Prompt, Value, 6);
    RFEAtt = atoi(KeyboardReturn);
  }
  LimeRFERXAtt = RFEAtt / 2;
  snprintf(Value, 30, "%d", RFEAtt / 2);
  SetConfigParam(PATH_PCONFIG, "limerferxatt", Value);
  SetConfigParam(PATH_PPRESETS, Param, Value);
  LimeRFEInit();
}


void LimeRFEInit()
{
  FILE *fp;
  char response[127];

  if(rfe == NULL)             // don't do this if the port is already open
  {
    // Don't initialise if not required
    if(LimeRFEState == 0)
    {
      return;
    }

    // List the ttyUSBs (should return /dev/ttyUSB0)
    fp = popen("ls --format=single-column /dev/ttyUSB*", "r");
    if (fp == NULL)
    {
      printf("Failed to run command ls --format=single-column /dev/ttyUSB* \n" );
      return;
    }

    // Read the output a line at a time and see if it works
    while ((fgets(response, 16, fp) != NULL)  && (rfe == NULL))
    {
      response[strcspn(response, "\n")] = 0;                 // strip off the trailing \n
      printf("Attempting to open %s for LimeRFE\n", response);
      rfe = RFE_Open(response, NULL);
      if (rfe != NULL)
      {
        printf("RFE on %s opened\n", response);
      }
    }

    // Close the File
    pclose(fp);

    if (rfe == NULL)
    {
      printf("Failed to Open LimeRFE for use\n");
    }
  }
  int RFE_CID = 1;
  int RFE_PORT_TX = 2;                    // TX port. 1 is TX/RX, 2 is TX, 3 is HF
  int RFE_PORT_RX = 1;                    // RX port can only be 1 (TX/RX) or 3 (HF)
  int RFE_RX_ATT = 7;                     // RX attenuator setting.  0-7 representing 0-14dB
  float RealFreq = 146.5;
  char Value[127] = "146.5";
  unsigned char RFEInfo[7];

  // Look up the current transmit frequency and other parameters
  GetConfigParam(PATH_PCONFIG, "freqoutput", Value);
  RealFreq = atof(Value);

  RFE_PORT_TX = LimeRFEPort;              // Get the configured Tx port
  RFE_RX_ATT = LimeRFERXAtt;              // Get the rx attenuator setting.   0-7 representing 0-14dB

  if (RFEHWVer == -1)   // Hardware version unknown so look it up
  {
    RFE_GetInfo(rfe, RFEInfo);
    RFEHWVer = RFEInfo[1];
  }

  if (RFEHWVer == 3)    // Development Version 0.3  NOTE Firmware needs changing to make this happen
  {
    if (RealFreq <= 30)
    {
      RFE_CID = 3;      // HAM_0030
      RFE_PORT_TX = 3;  // Force HF output Port  for this band
      RFE_PORT_RX = 3;
    }
    else if ((RealFreq > 30) && (RealFreq <= 140))
    {
      RFE_CID = 1;      // WB_1000
    }
    else if ((RealFreq > 140) && (RealFreq <= 150))
    {
      RFE_CID = 5;      // HAM_0145
    }
    else if ((RealFreq > 150) && (RealFreq <= 425))
    {
      RFE_CID = 1;      // WB_1000
    }
    else if ((RealFreq > 425) && (RealFreq <= 445))
    {
      RFE_CID = 7;      // HAM_0435
    }
    else if ((RealFreq > 445) && (RealFreq <= 1000))
    {
      RFE_CID = 1;      // WB_1000
    }
    else if ((RealFreq > 1000) && (RealFreq <= 1230))
    {
      RFE_CID = 2;      // WB_4000
    }
    else if ((RealFreq > 1230) && (RealFreq <= 1330))
    {
      RFE_CID = 9;      // HAM_1280
    }
    else if ((RealFreq > 1330) && (RealFreq <= 2300))
    {
      RFE_CID = 2;      // WB_4000
    }
    else if ((RealFreq > 2300) && (RealFreq <= 2500))
    {
      RFE_CID = 10;      // HAM2400
    }
    else if ((RealFreq > 2500) && (RealFreq <= 3350))
    {
      RFE_CID = 2;      // WB_4000
    }
    else if ((RealFreq > 3350) && (RealFreq <= 3500))
    {
      RFE_CID = 11;      // HAM3500
    }
    else
    {
      RFE_CID = 2;      // WB_4000
    }
  }
  else                 // Production Version or unknown
  {
    if (RealFreq < 50)
    {
      RFE_CID = 3;      // HAM_0030
      RFE_PORT_TX = 3;  // Force HF output Port for this band
      RFE_PORT_RX = 3;
    }
    else if ((RealFreq >= 50) && (RealFreq < 85))
    {
      RFE_CID = 4;      // HAM_0070
      RFE_PORT_TX = 3;  // Force HF output Port for this band
      RFE_PORT_RX = 3;
    }
    else if ((RealFreq >= 85) && (RealFreq < 140))
    {
      RFE_CID = 1;      // WB_1000
    }
    else if ((RealFreq >= 140) && (RealFreq < 150))
    {
      RFE_CID = 5;      // HAM_0145
    }
    else if ((RealFreq >= 150) && (RealFreq < 190))
    {
      RFE_CID = 1;      // WB_1000
    }
    else if ((RealFreq >= 190) && (RealFreq < 260))
    {
      RFE_CID = 6;      // HAM_0220
    }
    else if ((RealFreq >= 260) && (RealFreq < 400))
    {
      RFE_CID = 1;      // WB_1000
    }
    else if ((RealFreq >= 400) && (RealFreq < 500))
    {
      RFE_CID = 7;      // HAM_0435
    }
    else if ((RealFreq >= 500) && (RealFreq < 900))
    {
      RFE_CID = 1;      // WB_1000
    }
    else if ((RealFreq >= 900) && (RealFreq < 930))
    {
      RFE_CID = 8;      // HAM_0920
    }
    else if ((RealFreq >= 930) && (RealFreq < 1000))
    {
      RFE_CID = 1;      // WB_1000
    }
    else if ((RealFreq >= 1000) && (RealFreq < 1200))
    {
      RFE_CID = 2;      // WB_4000
    }
    else if ((RealFreq >= 1200) && (RealFreq < 1500))
    {
      RFE_CID = 9;      // HAM_1280
    }
    else if ((RealFreq >= 1500) && (RealFreq < 2200))
    {
      RFE_CID = 2;      // WB_4000
    }
    else if ((RealFreq >= 2200) && (RealFreq < 2600))
    {
      RFE_CID = 10;      // HAM2400
    }
    else if ((RealFreq >= 2600) && (RealFreq < 3200))
    {
      RFE_CID = 2;      // WB_4000
    }
    else if ((RealFreq >= 3200) && (RealFreq < 3500))
    {
      RFE_CID = 11;      // HAM3500
    }
    else
    {
      RFE_CID = 2;      // WB_4000
    }
  }
  RFE_Configure(rfe, RFE_CID, RFE_CID, RFE_PORT_RX, RFE_PORT_TX, RFE_MODE_RX, RFE_NOTCH_OFF, RFE_RX_ATT, 0, 0);
  LimeRFEMode = 0;

  printf("LimeRFE Version %d Configured for freq band %d\n Tx output port %d Rx input port %d\n Rx Attenuator = %d dB\n",
          RFEHWVer, RFE_CID, RFE_PORT_TX, RFE_PORT_RX, RFE_RX_ATT * 2);
}


void *LimeRFEPTTDelay(void * arg)
{
  int seconds = 0;

  do	
  {
    usleep(1000000);  // Sleep 1 second
    seconds = seconds + 1;
    // printf ("LimeRFE Waiting for transmit %d\n", seconds);
  }
  while((seconds < 15) && (GetButtonStatus(20) == 1));  // Wait 15 seconds if still on TX

  if((GetButtonStatus(20) == 1) && (digitalRead(GPIO_PTT) == 1))  // Still on TX and PTT enabled
  {
    LimeRFEMode = 1;
    RFE_Mode(rfe, RFE_MODE_TX);
    printf ("LimeRFE switching to transmit \n\n");
  }
  return NULL;
}


void LimeRFETX()
{
  if (rfe != NULL)  // Don't do this if we don't have a handle for the LimeRFE
  {

    // Create thread with 12 second delay then TX
    pthread_create (&thrfe15, NULL, &LimeRFEPTTDelay, NULL);
    //pthread_detach(thrfe15);  // It will then free its resources when it exits
    //pthread_join(thrfe15, NULL);
 }
}


void LimeRFERX()
{
  if (rfe != NULL)  // Don't do this if we don't have a handle for the LimeRFE
  {
    LimeRFEMode = 0;
	RFE_Mode(rfe, RFE_MODE_RX);
    printf("LimeRFE switched to Receive Mode\n");
  }
}


void LimeRFEClose()
{
  if (rfe != NULL)  // Don't do this if we don't have a handle for the LimeRFE
  {
    LimeRFEMode = 0;

	//Reset LimeRFE
	RFE_Reset(rfe);
    printf("LimeRFE Reset\n");

	//Close port
	RFE_Close(rfe);
	printf("LimeRFE Port closed\n");
    rfe = NULL;
  }
}


void ChangeJetsonIP()
{
  char RequestText[64];
  char InitText[64];
  bool IsValid = FALSE;
  char JetsonIP[31];
  char JetsonIPCopy[31];

  //Retrieve (17 char) Current IP from Config file
  GetConfigParam(PATH_JCONFIG, "jetsonip", JetsonIP);

  while (IsValid == FALSE)
  {
    strcpy(RequestText, "Enter new IP address for Jetson Nano");
    //snprintf(InitText, 17, "%s", JetsonIP);
    strcpyn(InitText, JetsonIP, 17);
    Keyboard(RequestText, InitText, 17);
  
    strcpy(JetsonIPCopy, KeyboardReturn);
    if(is_valid_ip(JetsonIPCopy) == 1)
    {
      IsValid = TRUE;
    }
  }
  printf("Jetson IP set to: %s\n", KeyboardReturn);

  // Save IP to config file
  SetConfigParam(PATH_JCONFIG, "jetsonip", KeyboardReturn);
}

void ChangeLKVIP()
{
  char RequestText[64];
  char InitText[64];
  bool IsValid = FALSE;
  char LKVIP[31];
  char LKVIPCopy[31];

  //Retrieve (17 char) Current IP from Config file
  GetConfigParam(PATH_JCONFIG, "lkvudp", LKVIP);

  while (IsValid == FALSE)
  {
    strcpy(RequestText, "Enter new UDP IP address for LKV373A");
    //snprintf(InitText, 17, "%s", LKVIP);
    strcpyn(InitText, LKVIP, 17);
    Keyboard(RequestText, InitText, 17);
  
    strcpy(LKVIPCopy, KeyboardReturn);
    if(is_valid_ip(LKVIPCopy) == 1)
    {
      IsValid = TRUE;
    }
  }
  printf("LKV UDP IP set to: %s\n", KeyboardReturn);

  // Save IP to Config File
  SetConfigParam(PATH_JCONFIG, "lkvudp", KeyboardReturn);
}

void ChangeLKVPort()
{
  char RequestText[64];
  char InitText[64];
  bool IsValid = FALSE;
  char LKVPort[15];

  //Retrieve (10 char) Current port from Config file
  GetConfigParam(PATH_JCONFIG, "lkvport", LKVPort);

  while (IsValid == FALSE)
  {
    strcpy(RequestText, "Enter the new UDP port for the LKV373A");
    //snprintf(InitText, 10, "%s", LKVPort);
    strcpyn(InitText, LKVPort, 15);
    Keyboard(RequestText, InitText, 10);
  
    if(strlen(KeyboardReturn) > 0)
    {
      IsValid = TRUE;
    }
  }
  printf("LKV UDP Port set to: %s\n", KeyboardReturn);

  // Save port to Config File
  SetConfigParam(PATH_JCONFIG, "lkvport", KeyboardReturn);
}

void ChangeJetsonUser()
{
  char RequestText[64];
  char InitText[64];
  bool IsValid = FALSE;
  char JetsonUser[15];

  //Retrieve (15 char) Username from Config file
  GetConfigParam(PATH_JCONFIG, "jetsonuser", JetsonUser);

  while (IsValid == FALSE)
  {
    strcpy(RequestText, "Enter the username for the Jetson Nano");
    snprintf(InitText, 15, "%s", JetsonUser);
    Keyboard(RequestText, InitText, 10);
  
    if(strlen(KeyboardReturn) > 0)
    {
      IsValid = TRUE;
    }
  }
  printf("Jetson Username set to: %s\n", KeyboardReturn);

  // Save username to Config File
  SetConfigParam(PATH_JCONFIG, "jetsonuser", KeyboardReturn);
}

void ChangeJetsonPW()
{
  char RequestText[64];
  char InitText[64];
  bool IsValid = FALSE;
  char JetsonPW[31];

  //Retrieve (31 char)Password from Config file
  GetConfigParam(PATH_JCONFIG, "jetsonpw", JetsonPW);

  while (IsValid == FALSE)
  {
    strcpy(RequestText, "Enter the password for the Jetson Nano");
    snprintf(InitText, 31, "%s", JetsonPW);
    Keyboard(RequestText, InitText, 10);
  
    if(strlen(KeyboardReturn) > 0)
    {
      IsValid = TRUE;
    }
  }
  printf("Jetson password set to: %s\n", KeyboardReturn);

  // Save username to Config File
  SetConfigParam(PATH_JCONFIG, "jetsonpw", KeyboardReturn);
}

void ChangeJetsonRPW()
{
  char RequestText[64];
  char InitText[64];
  bool IsValid = FALSE;
  char JetsonPW[31];

  //Retrieve (31 char) root password from Config file
  GetConfigParam(PATH_JCONFIG, "jetsonrootpw", JetsonPW);

  while (IsValid == FALSE)
  {
    strcpy(RequestText, "Enter the root password for the Jetson Nano");
    snprintf(InitText, 31, "%s", JetsonPW);
    Keyboard(RequestText, InitText, 10);
  
    if(strlen(KeyboardReturn) > 0)
    {
      IsValid = TRUE;
    }
  }
  printf("Jetson root password set to: %s\n", KeyboardReturn);

  // Save username to Config File
  SetConfigParam(PATH_JCONFIG, "jetsonrootpw", KeyboardReturn);
}


void waituntil(int w,int h)
{
  // Wait for a screen touch and act on its position

  int rawX, rawY, rawPressure, i;
  rawX = 0;
  rawY = 0;
  char ValueToSave[63];
  char device_name[63];
  char linux_cmd[127];

  // Start the main loop for the Touchscreen
  for (;;)
  {
    if ((strcmp(ScreenState, "RXwithImage") != 0) && (strcmp(ScreenState, "VideoOut") != 0)
     && (boot_to_tx == false) && (boot_to_rx == false))  // Don't wait for touch if returning from recieve or booting to TX or rx
    {
      // Wait here until screen touched
      if (getTouchSample(&rawX, &rawY, &rawPressure)==0) continue;
    }

    // Screen has been touched or returning from recieve or booting to tx or rx
    // printf("Actioning Event in waituntil x=%d y=%d\n", rawX, rawY);

    // React differently depending on context: char ScreenState[255]

      // Menu (normal)                              NormalMenu  (implemented)
      // Menu (Specials)                            SpecialMenu (not implemented yet)
      // Transmitting
        // with image displayed                     TXwithImage (implemented)
        // with menu displayed but not active       TXwithMenu  (implemented)
      // Receiving                                  RXwithImage (implemented)
      // Video Output                               VideoOut    (implemented)
      // Snap View                                  SnapView    (not implemented yet)
      // VideoView                                  VideoView   (not implemented yet)
      // Snap                                       Snap        (not implemented yet)
      // SigGen?                                    SigGen      (not implemented yet)
      // WebcamWait                                 Waiting for Webcam reset. Touch listens but does not respond

      // printf("Screenstate is %s \n", ScreenState);

     // Sort TXwithImage first:
    if (strcmp(ScreenState, "TXwithImage") == 0)
    {
      TransmitStop();
      system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png  >/dev/null 2>/dev/null");  // Add logo image
      system("(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
      setBackColour(255, 255, 255);
      clearScreen();
      setBackColour(0, 0, 0);
      SelectPTT(20,0);
      strcpy(ScreenState, "NormalMenu");
      UpdateWindow();
      continue;  // All reset, and Menu displayed so go back and wait for next touch
     }

    // Now Sort TXwithMenu:
    if (strcmp(ScreenState, "TXwithMenu") == 0)
    {
      SelectPTT(20, 0);  // Update screen first
      UpdateWindow();
      TransmitStop();
      if (strcmp(ScreenState, "WebcamWait") != 0)
      {
        strcpy(ScreenState, "NormalMenu");
      }
      continue;
    }

    if (strcmp(ScreenState, "WebcamWait") == 0)
    {
      // Do nothing
      printf("In WebcamWait \n");
      continue;
    }

    // Now deal with return from receiving
    if (strcmp(ScreenState, "RXwithImage") == 0)
    {
      system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png  >/dev/null 2>/dev/null");  // Add logo image
      system("(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
      if (CallingMenu == 1)
      {
      setBackColour(255, 255, 255);
      clearScreen();
      setBackColour(0, 0, 0);
        SelectPTT(21,0);
      }
      else
      {
        setBackColour(0, 0, 0);
        clearScreen();
      }
      strcpy(ScreenState, "NormalMenu");
      UpdateWindow();
      continue;
    }

    if (strcmp(ScreenState, "VideoOut") == 0)
    {
      system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png  >/dev/null 2>/dev/null");  // Add logo image
      system("(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
      setBackColour(127, 127, 127);
      clearScreen();
      setBackColour(0, 0, 0);
      strcpy(ScreenState, "NormalMenu");
      UpdateWindow();
      continue;
    }

    if (strcmp(ScreenState, "SnapView") == 0)
    {
      system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png  >/dev/null 2>/dev/null");  // Add logo image
      system("(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
      setBackColour(0, 0, 0);
      clearScreen();
      strcpy(ScreenState, "NormalMenu");
      UpdateWindow();
      continue;
    }

    // Not transmitting or receiving, so sort NormalMenu
    if (strcmp(ScreenState, "NormalMenu") == 0)
    {
      // For Menu (normal), check which button has been pressed (Returns 0 - 23)

      i = IsMenuButtonPushed(rawX, rawY);

      // Deal with boot to tx or boot to rx
      if (boot_to_tx == true)
      {
        CurrentMenu = 1;
        i = 20;
        boot_to_tx = false;
      }
      if (boot_to_rx == true)
      {
        CurrentMenu = 8;
        i = 0;
        boot_to_rx = false;
      }

      if (i == -1)
      {
        continue;  //Pressed, but not on a button so wait for the next touch
      }

      // Now do the reponses for each Menu in turn
      if (CurrentMenu == 1)  // Main Menu
      {
        printf("Button Event %d, Entering Menu 1 Case Statement\n",i);
        CallingMenu = 1;

        // Clear Preset store trigger if not a preset
        if ((i > 4) && (PresetStoreTrigger == 1))
        {
          PresetStoreTrigger = 0;
          SetButtonStatus(4,0);
          UpdateWindow();
          continue;
        }
        switch (i)
        {
        case 0:
        case 1:
        case 2:
        case 3:
          if (PresetStoreTrigger == 0)
          {
            RecallPreset(i);  // Recall preset
            // and make sure that everything has been refreshed?
          }
          else
          {
            SavePreset(i);  // Set preset
            PresetStoreTrigger = 0;
            SetButtonStatus(4,0);
            setBackColour(255, 255, 255);
            clearScreen();
            setBackColour(0, 0, 0);
          }
          SetButtonStatus(i, 1);
          Start_Highlights_Menu1();    // Refresh button labels
          UpdateWindow();
          usleep(500000);
          SetButtonStatus(i, 0); 
          UpdateWindow();
          break;
        case 4:                        // Set up to store preset
          if (PresetStoreTrigger == 0)
          {
            PresetStoreTrigger = 1;
            SetButtonStatus(4,2);
          }
          else
          {
            PresetStoreTrigger = 0;
            SetButtonStatus(4,0);
          }
          UpdateWindow();
          break;
        case 5:
          printf("MENU 21 \n");       // EasyCap
          CurrentMenu=21;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu21();
          UpdateWindow();
          break;
        case 6:
          printf("MENU 22 \n");       // Caption
          CurrentMenu=22;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu22();
          UpdateWindow();
          break;
        case 7:
          printf("MENU 23 \n");       // Audio
          CurrentMenu=23;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu23();
          UpdateWindow();
          break;
        case 8:
          printf("MENU 24 \n");       // Attenuator
          CurrentMenu=24;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu24();
          UpdateWindow();
          break;
        case 9:                          // Attenuator
          printf("Set Attenuator Level \n");
          SetAttenLevel();
          setBackColour(255, 255, 255);
          clearScreen();
          setBackColour(0, 0, 0);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 10:
          if (strcmp(CurrentModeOP, TabModeOP[4]) != 0)  // not Streaming
          {
            printf("MENU 10 \n");        // Set Frequency
            CurrentMenu=10;
            setBackColour(0, 0, 0);
            clearScreen();
            Start_Highlights_Menu10();
          }
          else
          {
            printf("MENU 35 \n");        // Select Stream
            CurrentMenu=35;
            setBackColour(0, 0, 0);
            clearScreen();
            Start_Highlights_Menu35();
          }
          UpdateWindow();
          break;
        case 11:
          printf("MENU 17 \n");       // SR
          CurrentMenu=17;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu17();
          UpdateWindow();
          break;
        case 12:
          setBackColour(0, 0, 0);
          clearScreen();
          if ((strcmp(CurrentTXMode, TabTXMode[0]) == 0) || (strcmp(CurrentTXMode, TabTXMode[1]) == 0)
           || (strcmp(CurrentTXMode, TabTXMode[6]) == 0)) // Carrier, DVB-S or DVB-T
          {
            printf("MENU 18 \n");       // FEC
            CurrentMenu=18;
            Start_Highlights_Menu18();
          }
          else  // DVB-S2
          {
            printf("MENU 25 \n");       // DVB-S2 FEC
            CurrentMenu=25;
            Start_Highlights_Menu25();
          }
          UpdateWindow();
          break;
        case 13:                      // Transverter
          CallingMenu = 113;
          printf("MENU 9 \n");
          CurrentMenu = 9;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu9();
          UpdateWindow();
          break;
        case 14:                         // Lime/Express/Pluto/Muntjac Level
          printf("Set Device Output Level \n");
          SetDeviceLevel();
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 15:
          printf("MENU 11 \n");        // Modulation
          CurrentMenu=11;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu11();
          UpdateWindow();
          break;
        case 16:
          printf("MENU 12 \n");        // Encoding
          CurrentMenu=12;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu12();
          UpdateWindow();
          break;
        case 17:
          printf("MENU 42 \n");        // Output Device
          CurrentMenu=42;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu42();
          UpdateWindow();
          break;
        case 18:
          printf("MENU 14 \n");        // Format
          CurrentMenu=14;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu14();
          UpdateWindow();
          break;
        case 19:
          printf("MENU 45 \n");        // Source
          CurrentMenu=45;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu45();
          UpdateWindow();
          break;
        case 20:                       // TX PTT
          if (strcmp(CurrentModeOP, "COMPVID") == 0) // Comp Vid OP
          {
            printf("MENU 4 \n");        // Source
            CurrentMenu=4;
            setBackColour(127, 127, 127);
            clearScreen();
            setBackColour(0, 0, 0);
            CompVidInitialise();
            Start_Highlights_Menu4();
            UpdateWindow();
            //CompVidStart();
          }
          else     // Transmit 
          {
            if ((strcmp(CurrentModeOP, "LIMEMINI") == 0) || (strcmp(CurrentModeOP, "LIMEUSB") == 0) || (strcmp(CurrentModeOP, "LIMEDVB") == 0))
            {  
              system("/home/pi/rpidatv/scripts/lime_ptt.sh &");
            }
            if (strcmp(CurrentModeOP, "PLUTO") == 0)
            {  
              system("/home/pi/rpidatv/scripts/pluto_ptt.sh &");
            }
            if (strcmp(CurrentModeOP, "MUNTJAC") == 0)
            {  
              system("/home/pi/rpidatv/scripts/muntjac_ptt.sh &");
            }
            SelectPTT(i, 1);
            UpdateWindow();
            TransmitStart();
          }
          break;
        case 21:                       // LongMynd RX
          if (CheckTuner() == 1)  // No MiniTiouner or PicoTuner
          {
            MsgBox4("No Tuner Connected", "Connect a MiniTiouner, PicoTuner or a Knucker", "for the receiver to use", "Touch Screen to Continue");
            wait_touch();
          }
          printf("MENU 8 \n");  //  LongMynd
          CurrentMenu=8;
          setBackColour(0, 0, 0);
          clearScreen();
          ReadLMRXPresets();
          Start_Highlights_Menu8();
          UpdateWindow();
          break;
        case 22:                      // Select Menu 2
          printf("MENU 2 \n");
          CurrentMenu=2;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu2();
          UpdateWindow();
          break;
        case 23:                      // Select Menu 3
          printf("MENU 3 \n");
          CurrentMenu=3;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu3();
          UpdateWindow();
          break;
        default:
          printf("Menu 1 Error\n");
        }
        continue;  // Completed Menu 1 action, go and wait for touch
      }

      if (CurrentMenu == 2)  // Menu 2
      {
        printf("Button Event %d, Entering Menu 2 Case Statement\n",i);
        switch (i)
        {
        case 0:                               // Shutdown
          MsgBox4(" ", "Shutting down now", " ", " ");
          system("/home/pi/rpidatv/scripts/s_jetson.sh &");  // Shutdown the Jetson
          system("sudo killall express_server >/dev/null 2>/dev/null");
          system("sudo rm /tmp/expctrl >/dev/null 2>/dev/null");
          sync();            // Prevents shutdown hang in Stretch
          LimeRFEClose();
          usleep(1000000);
          cleanexit(160);    // Commands scheduler to initiate shutdown
          break;
        case 1:                               // Reboot
          MsgBox4(" ", "Rebooting now", " ", " ");
          system("sudo killall express_server >/dev/null 2>/dev/null");
          system("sudo rm /tmp/expctrl >/dev/null 2>/dev/null");
          sync();            // Prevents shutdown hang in Stretch
          usleep(1000000);
          cleanexit(192);    // Commands scheduler to initiate reboot
          break;
        case 2:                               // Display Info Page
          InfoScreen();
          setBackColour(0, 0, 0);
          clearScreen();
          UpdateWindow();
          break;
        case 3:                               // File Menu
          printf("MENU 4 \n");
          CurrentMenu=4;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu4();
          UpdateWindow();
          break;
        case 4:                              // VLC Stream Viewer
          printf("MENU 20 \n");
          CurrentMenu = 20;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu20();
          UpdateWindow();
          strcpy(StreamPlayer, "VLC");
          break;
        case 5:                               // Locator Bearings
          RangeBearing();
          setBackColour(0, 0, 0);
          clearScreen();
          UpdateWindow();
          break;
        case 6:                               // Sites and Beacons Bearing
          BeaconBearing();
          setBackColour(0, 0, 0);
          clearScreen();
          UpdateWindow();
          break;
        case 7:                              // Check Snaps
          do_snapcheck();
          UpdateWindow();
          break;
        case 8:                               // Test Equipment Menu
          printf("MENU 7 \n");
          CurrentMenu = 7;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu7();
          UpdateWindow();
          break;
        case 9:                              // OMX Stream Viewer
          printf("MENU 20 \n");
          CurrentMenu = 20;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu20();
          UpdateWindow();
          strcpy(StreamPlayer, "OMX");
          break;
        case 10:                              // Video Monitor (EasyCap)
        case 11:                              // Pi Cam Monitor
        case 12:                              // C920
        case 13:                              // IPTS Viewer
        case 14:                              // HDMI
          do_video_monitor(i);
          setBackColour(0, 0, 0);
          clearScreen();
          UpdateWindow();
          break;
        case 15:                                     // Select Langstone (Or go to menu for install)
          if (strcmp(langstone_version, "v1pluto") == 0)
          {
            SetButtonStatus(ButtonNumber(2, 15), 1);  // and highlight button
            UpdateWindow();
            RebootPluto(1);                           // Reboot Pluto and wait for recovery                          
            do_Langstone();
            SetButtonStatus(ButtonNumber(2, 15), 0);  // unhighlight button
            setBackColour(0, 0, 0);
            clearScreen();
            UpdateWindow();
          }
          else if (strcmp(langstone_version, "v2pluto") == 0)
          {
            SetButtonStatus(ButtonNumber(2, 15), 1);  // and highlight button
            UpdateWindow();
            RebootPluto(1);                           // Reboot Pluto and wait for recovery                          
            do_Langstone();
            SetButtonStatus(ButtonNumber(2, 15), 0);  // unhighlight button
            setBackColour(0, 0, 0);
            clearScreen();
            UpdateWindow();
          }
          else if (strcmp(langstone_version, "v2lime") == 0)
          {
            SetButtonStatus(ButtonNumber(2, 15), 1);  // and highlight button
            UpdateWindow();
            do_Langstone();
            SetButtonStatus(ButtonNumber(2, 15), 0);  // unhighlight button
            setBackColour(0, 0, 0);
            clearScreen();
            UpdateWindow();
          }
          else                                                    // Langstone not installed yet
          {
            printf("MENU 39 \n");
            CurrentMenu=39;
            Start_Highlights_Menu39();
            UpdateWindow();
          }
          break;
        case 16:                               // Start Sig Gen and Exit
          DisplayLogo();
          cleanexit(130);
          break;
        case 17:                              //  Band Viewer.  Check for Airspy, SDRPlay, LimeSDR, RTL-SDR then Pluto
          if (CheckAirspyConnect() == 0)
          {
            DisplayLogo();
            cleanexit(140);
          }
          else 
          { 
            if(CheckSDRPlay() == 0)
            {
              DisplayLogo();
              cleanexit(144);
            }
            else 
            { 
              if((CheckLimeMiniConnect() == 0) || (CheckLimeUSBConnect() == 0))
              {
                DisplayLogo();
                cleanexit(136);
              }
              else
              { 
                if(CheckRTL() == 0)
                {
                  DisplayLogo();
                  cleanexit(141);
                }
                else
                {
                  if(CheckPlutoIPConnect() == 0)
                  {
                    DisplayLogo();
                    cleanexit(143);
                  }
                  else
                  {
                    MsgBox2("No LimeSDR, Airspy, SDRplay,", "Pluto or RTL-SDR Connected");
                    wait_touch();
                  }
                }
              }
            }
          }
          UpdateWindow();
          break;
        case 18:                               // Start RTL-TCP server or Ryde if installed
          if (file_exist("/home/pi/ryde/config.yaml") == 1)       // No Ryde
          {
            if(CheckRTL()==0)
            {
              rtl_tcp();
            }
            else
            {
              MsgBox("No RTL-SDR Connected");
              wait_touch();
            }
            setBackColour(0, 0, 0);
            clearScreen();
            UpdateWindow();
          }
          else
          {
            MsgBox2("Starting the Ryde Receiver on HDMI", "Touch Screen to return to Portsdown");
            cleanexit(197);
          }
          break;
        case 19:                              // RTL-FM Receiver
          if(CheckRTL()==0)
          {
            RTLdetected = 1;
          }
          else
          {
            RTLdetected = 0;
            MsgBox2("No RTL-SDR Connected", "Connect RTL-SDR to enable RX");
            wait_touch();
          }
          printf("MENU 6 \n");
          CurrentMenu=6;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu6();
          UpdateWindow();
          break;
        case 20:                              // Not shown
          break;
        case 21:                              // Menu 1
          printf("MENU 1 \n");
          CurrentMenu=1;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 22:                              // Not shown
          break;
        case 23:                              // Menu 3
          printf("MENU 3 \n");
          CurrentMenu=3;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu3();
          UpdateWindow();
          break;
        default:
          printf("Menu 2 Error\n");
        }
        continue;   // Completed Menu 2 action, go and wait for touch
      }

      if (CurrentMenu == 3)  // Menu 3
      {
        printf("Button Event %d, Entering Menu 3 Case Statement\n",i);
        CallingMenu = 3;
        switch (i)
        {
        case 0:                              // Check for Update
          SetButtonStatus(ButtonNumber(3, 0), 1);  // and highlight button
          UpdateWindow();
          SetButtonStatus(ButtonNumber(3, 0), 0);
          printf("MENU 33 \n"); 
          CurrentMenu=33;
          setBackColour(0, 0, 0);
          clearScreen();
          PrepSWUpdate();
          Start_Highlights_Menu33();
          UpdateWindow();
          break;
        case 1:                               // System Config
          printf("MENU 43 \n"); 
          CurrentMenu=43;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu43();
          UpdateWindow();
          break;
        case 2:                               // Wifi Config
          printf("MENU 36 \n"); 
          CurrentMenu=36;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu36();
          UpdateWindow();
          break;
        case 3:                               // TS IP Config
          printf("MENU 40 \n"); 
          CurrentMenu=40;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu40();
          UpdateWindow();
          break;
        case 4:                              // 
          break;
        case 5:                              // Lime Config
          printf("MENU 37 \n"); 
          CurrentMenu=37;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu37();
          UpdateWindow();
          break;
       case 6:                              // Jetson Config 
          printf("MENU 44 \n"); 
          CurrentMenu=44;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu44();
          UpdateWindow();
          break;
        case 7:                              // Langstone Config
          printf("MENU 39 \n"); 
          CurrentMenu=39;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu39();
          UpdateWindow();
          break;
        case 8:                              // Pluto Config
          printf("MENU 15 \n"); 
          CurrentMenu=15;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu15();
          UpdateWindow();
          break;
        case 9:                              // Test card Select Menu
          printf("MENU 47 \n"); 
          CurrentMenu=47;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu47();
          UpdateWindow();
          break;
        case 10:                               // Amend Sites/Beacons
          printf("MENU 31 \n"); 
          CurrentMenu=31;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu31();
          UpdateWindow();
          break;
        case 11:                               // Set Receive LO
          CallingMenu = 302;
          printf("MENU 26 \n"); 
          CurrentMenu=26;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu26();
          UpdateWindow();
          break;
        case 12:                               // Set Stream Outputs
          printf("MENU 35 \n"); 
          CurrentMenu=35;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu35();
          UpdateWindow();
          break;
        case 13:                               // Set Audio output
          if (strcmp(LMRXaudio, "rpi") == 0)
          {
            strcpy(LMRXaudio, "usb");
          }
          else if (strcmp(LMRXaudio, "usb") == 0)
          {
            strcpy(LMRXaudio, "hdmi");
          }
          else
          {
            strcpy(LMRXaudio, "rpi");
          }
          SetConfigParam(PATH_LMCONFIG, "audio", LMRXaudio);
          if (file_exist("/home/pi/ryde/config.yaml") == 0)       // Ryde installed
          {
            system("/home/pi/rpidatv/scripts/set_ryde_audio.sh");  // So synchronise Ryde audio
          }
          Start_Highlights_Menu3();
          UpdateWindow();
          break;
        case 14:                               // Set Mic Gain
          ChangeMicGain();
          SetAudioLevels();
          Start_Highlights_Menu3();
          UpdateWindow();
          break;
        case 15:                              // Set Band Details
          CallingMenu = 315;
          printf("MENU 9 \n"); 
          CurrentMenu = 9;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu9();
          UpdateWindow();
          break;
        case 16:                              // Set Preset Frequencies
          printf("MENU 27 \n"); 
          CurrentMenu=27;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu27();
          UpdateWindow();
          break;
        case 17:                              // Set Preset SRs
          printf("MENU 28 \n"); 
          CurrentMenu=28;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu28();
          UpdateWindow();
          break;
        case 18:                              // Set Call, Loc and PIDs
          printf("MENU 29 \n"); 
          CurrentMenu=29;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu29();
          UpdateWindow();
          break;
        case 19:                               // Set ADF Reference Frequency
          printf("MENU 32 \n"); 
          CurrentMenu=32;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu32();
          UpdateWindow();
          break;
        case 20:                              // Not shown
          break;
        case 21:                              // Menu 1
          printf("MENU 1 \n");
          CurrentMenu=1;
          setBackColour(255, 255, 255);
          clearScreen();
          setBackColour(0, 0, 0);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 22:                              // Menu 2
          printf("MENU 2 \n");
          CurrentMenu=2;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu2();
          UpdateWindow();
          break;
        case 23:                              // Not Shown
          break;
        default:
          printf("Menu 3 Error\n");
        }
        continue;   // Completed Menu 3 action, go and wait for touch
      }

      if (CurrentMenu == 4)  // Menu 4 File Menu
      {
        printf("Button Event %d, Entering Menu 4 Case Statement\n",i);
        switch (i)
        {
        case 0:                               // Copy file
        case 1:                               // Paste file
        case 2:                               // Rename File
        case 3:                               // Delete file or directory
          FileOperation(i);
          Start_Highlights_Menu4();           // Refresh button labels
          UpdateWindow();
          break;
        case 4:
          ClearMenuMessage(); 
          printf("MENU 1 \n");
          CurrentMenu=1;
          setBackColour(255, 255, 255);
          clearScreen();
          setBackColour(0, 0, 0);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 5:                               // Open File Explorer
          FileOperation(i);
          Start_Highlights_Menu4();
          UpdateWindow();
          break;
        case 6:                               // Install or update Ryde
          if (file_exist("/home/pi/ryde/config.yaml") == 1)       // Ryde Install or Update
          {
            MsgBox4("Installing Ryde", "Portsdown will reboot", "when complete", " ");
            system("/home/pi/rpidatv/add_ryde.sh");
          }
          else
          {
            MsgBox4("Updating Ryde", "Portsdown will reboot", "when complete", " ");
            system("/home/pi/rpidatv/update_ryde.sh");
          }
          Start_Highlights_Menu4();
          UpdateWindow();
          break;
        case 7:                               // List Raspberry Pis on network
          MsgBox4("Please wait", "Scanning Networks", ".....", " ");
          ListNetPis();
          Start_Highlights_Menu4();
          UpdateWindow();
          break;
        case 8:                               // List Network Devices
          MsgBox4("Please wait", "Scanning Networks", ".....", " ");
          ListNetDevices();
          Start_Highlights_Menu4();
          UpdateWindow();
          break;
        case 9:                               // List USB Devices
          ClearMenuMessage(); 
          ListUSBDevices();
          Start_Highlights_Menu4();
          UpdateWindow();
          break;
        case 10:                               // Select IQ file
        case 11:                               // Play single
        case 12:                               // Play loop
          IQFileOperation(i);
          Start_Highlights_Menu4();           // Refresh button labels
          UpdateWindow();
          break;
        case 13:                              // mount/unmount USB drive
          FileOperation(i);
          Start_Highlights_Menu4();           // Refresh button labels
          UpdateWindow();
          break;
        case 14:                             // Download HamTV file
          if (file_exist("/home/pi/iqfiles/SDRSharp_20160423_121611Z_731000000Hz_IQ.wav") == 1)  // so file not present
          {
            CallingMenu = 414;
            CurrentMenu = 38;
            MsgBox4("File is 1.5 GB and will take some time", "to download and uncompress", "Continue?", " ");
            UpdateWindow();
          }
          break;
        case 20:                             // 
          break;
        case 22:                             // 
          break;
        default:
          printf("Menu 4 Error\n");
        }
        continue;   // Completed Menu 4 action, go and wait for touch
      }

      if (CurrentMenu == 5)  // Menu 5 LeanDVB
      {
        printf("Button Event %d, Entering Menu 5 Case Statement\n",i);
        CallingMenu = 5;

        switch (i)
        {
        case 0:                              // Preset 1
        case 1:                              // Preset 2
        case 2:                              // Preset 3
        case 3:                              // Preset 4
          break;
        case 4:                              // Store RX Preset
          break;
        case 5:                              // Fastlock on/off
          break;
        case 7:                              // Audio on/off
          break;
        case 9:                              // SetRXLikeTX
          break;
        case 10:
          break;
        case 11:
          break;
        case 12:
          break;
        case 13:                       // Sample Rate
          break;
        case 14:                       // Gain
          break;
        case 16:                       // Encoding
          break;
        case 17:                       // SDR Selection
          break;
        case 18:                                            // Constellation on/off
          break;
        case 19:                                            // Parameters on/off
          break;
        case 21:                       // RX
          break;
        case 22:                                          // Back to Menu 1
          printf("MENU 1 \n");
          CurrentMenu=1;
          setBackColour(255, 255, 255);
          clearScreen();
          setBackColour(0, 0, 0);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        default:
          printf("Menu 5 Error\n");
        }
        continue;   // Completed Menu 5 action, go and wait for touch
      }

      if (CurrentMenu == 6)  // Menu 6 RTL-FM
      {
        printf("Button Event %d, Entering Menu 6 Case Statement\n",i);

        // Clear RTL Preset store trigger if not a preset
        if ((i > 9) && (RTLStoreTrigger == 1))
        {
          RTLStoreTrigger = 0;
          SetButtonStatus(ButtonNumber(CurrentMenu, 4), 0);
          UpdateWindow();
          continue;
        }
        switch (i)
        {
          case 4:                              // Store RTL Preset
          if (RTLStoreTrigger == 0)
          {
            RTLStoreTrigger = 1;
            SetButtonStatus(ButtonNumber(CurrentMenu, 4),1);
          }
          else
          {
            RTLStoreTrigger = 0;
            SetButtonStatus(ButtonNumber(CurrentMenu, 4),0);
          }
          UpdateWindow();
          break;
        case 5:                              // Preset 1
        case 6:                              // Preset 2
        case 7:                              // Preset 3
        case 8:                              // Preset 4
        case 9:                              // Preset 5
        case 0:                              // Preset 6
        case 1:                              // Preset 7
        case 2:                              // Preset 8
        case 3:                              // Preset 9
          if (RTLStoreTrigger == 0)
          {
            RecallRTLPreset(i);  // Recall preset
            // and start/restart RX
          }
          else
          {
            SaveRTLPreset(i);  // Set preset
            RTLStoreTrigger = 0;
            SetButtonStatus(ButtonNumber(CurrentMenu, 4), 0);
          setBackColour(0, 0, 0);
          clearScreen();
          }
          RTLstop();
          RTLstart();
          RTLactive = 1;
          //SetButtonStatus(ButtonNumber(CurrentMenu, i), 1);
          //Start_Highlights_Menu6();    // Refresh button labels
          //UpdateWindow();
          //usleep(500000);
          //SetButtonStatus(ButtonNumber(CurrentMenu, i), 0); 
          Start_Highlights_Menu6();          // Refresh button labels
          UpdateWindow();
          break;
        case 10:                             // AM
        case 11:                             // FM
        case 12:                             // WBFM
        case 13:                             // USB
        case 14:                             // LSB
          SelectRTLmode(i);
          if (RTLactive == 1)
          {
            RTLstop();
            RTLstart();
          }
          Start_Highlights_Menu6();          // Refresh button labels
          UpdateWindow();
          break;
        case 15:                             // Frequency
          ChangeRTLFreq();
          if (RTLactive == 1)
          {
            RTLstop();
            RTLstart();
          }
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu6();          // Refresh button labels
          UpdateWindow();
          break;
        case 16:                             // Set Squelch Level
          ChangeRTLSquelch();
          if (RTLactive == 1)
          {
            RTLstop();
            RTLstart();
          }
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu6();          // Refresh button labels
          UpdateWindow();
          break;
        case 17:                             // Squelch on/off
          if (RTLsquelchoveride == 1)
          {
            RTLsquelchoveride = 0;
            SetButtonStatus(ButtonNumber(CurrentMenu, 17),1);
          }
          else
          {
            RTLsquelchoveride = 1;
            SetButtonStatus(ButtonNumber(CurrentMenu, 17),0);
          }
          if (RTLactive == 1)
          {
            RTLstop();
            RTLstart();
          }
          UpdateWindow();
          break;
        case 18:                             // Save
          SaveRTLPreset(-6);                 // Saves current state
          SetButtonStatus(ButtonNumber(CurrentMenu, i), 1);
          Start_Highlights_Menu6();    // Refresh button labels
          UpdateWindow();
          usleep(500000);
          SetButtonStatus(ButtonNumber(CurrentMenu, i), 0); 
          UpdateWindow();
          break;
        case 19:                             // Set gain
          ChangeRTLGain();
          if (RTLactive == 1)
          {
            RTLstop();
            RTLstart();
          }
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu6();          // Refresh button labels
          UpdateWindow();
          break;
        case 20:                            // Not shown
          break;
        case 21:                            // RX on
          if (RTLactive == 1)
          {
            RTLstop();
            RTLactive = 0;
          }
          else
          {
            RTLstop();
            RTLactive = 1;
            RTLstart();
          }
          Start_Highlights_Menu6();          // Refresh button labels
          UpdateWindow();
          break;
         case 22:                            // Exit
          RTLstop();
          RTLactive = 0;
          printf("MENU 1 \n");
          CurrentMenu=1;
          setBackColour(255, 255, 255);
          clearScreen();
          setBackColour(0, 0, 0);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 23:                            // Blank
          break;
        default:
          printf("Menu 6 Error\n");
        }
        continue;   // Completed Menu 6 action, go and wait for touch
      }

      if (CurrentMenu == 7)  // Menu 7
      {
        printf("Button Event %d, Entering Menu 7 Case Statement\n",i);
        switch (i)
        {
        case 0:                                                 // Airspy BandViewer
          if (CheckAirspyConnect() == 0)
          {
            DisplayLogo();
            cleanexit(140);
          }
          else 
          {
            MsgBox("No Airspy Connected");
            wait_touch();
          }
          UpdateWindow();
          break;
        case 1:                                                 // LimeSDR BandViewer
          if((CheckLimeMiniConnect() == 0) || (CheckLimeUSBConnect() == 0))
          {
            DisplayLogo();
            cleanexit(136);
          }
          else
          {
            MsgBox("No LimeSDR Connected");
            wait_touch();
          }
          UpdateWindow();
          break;
        case 2:                                                 // Pluto BandViewer
          if(CheckPlutoIPConnect() == 0)
          {
            DisplayLogo();
            cleanexit(143);
          }
          else
          {
            MsgBox("No Pluto Connected");
            wait_touch();
          }
          UpdateWindow();
          break;
        case 3:                                                 // RTL-SDR BandViewer
          if(CheckRTL() == 0)
          {
            DisplayLogo();
            cleanexit(141);
          }
          else
          {
            MsgBox("No RTL-SDR Connected");
            wait_touch();
          }
          UpdateWindow();
          break;
        case 4:                                                 // SDR Play BandViewer
          if(CheckSDRPlay() == 0)
          {
            DisplayLogo();
            cleanexit(144);
          }
          else
          {
            MsgBox("No SDR Play Connected");
            wait_touch();
          }
          UpdateWindow();
          break;
        case 5:                                                 // Frequency Sweeper
          if((CheckLimeMiniConnect() == 0) || (CheckLimeUSBConnect() == 0))
          {
            DisplayLogo();
            cleanexit(139);
          }
          else
          {
            MsgBox("No LimeSDR Connected");
            wait_touch();
          }
          UpdateWindow();
          break;
        case 6:                                                 // Lime Noise Figure Meter
          if((CheckLimeMiniConnect() == 0) || (CheckLimeUSBConnect() == 0))
          {
            DisplayLogo();
            cleanexit(138);
          }
          else
          {
            MsgBox("No LimeSDR Connected");
            wait_touch();
          }
          break;
        case 7:                                                 // Pluto Noise Figure Meter
          if(CheckPlutoIPConnect() == 0)
          {
            DisplayLogo();
            cleanexit(148);
          }
          else
          {
            MsgBox("No Pluto Connected");
            wait_touch();
          }
          UpdateWindow();
          break;
        case 8:                                                 // DMM Display
          DisplayLogo();
          cleanexit(142);
          break;
        case 9:                                                 // SDRPlay MeteorViewer
          DisplayLogo();
          cleanexit(150);
          break;
        case 10:                                                 // Signal Generator
          DisplayLogo();
          cleanexit(130);
          break;
        case 11:                                                 // Lime Noise Meter
          if((CheckLimeMiniConnect() == 0) || (CheckLimeUSBConnect() == 0))
          {
            DisplayLogo();
            cleanexit(147);
          }
          else
          {
            MsgBox("No LimeSDR Connected");
            wait_touch();
          }
          UpdateWindow();
          break;
        case 12:                                                 // Pluto Noise Meter
          if(CheckPlutoIPConnect() == 0)
          {
            DisplayLogo();
            cleanexit(149);
          }
          else
          {
            MsgBox("No Pluto Connected");
            wait_touch();
          }
          UpdateWindow();
          break;
        case 13:                                                 // Power Meter
          DisplayLogo();
          cleanexit(137);
          break;
        case 14:                                                 // XY Display
          DisplayLogo();
          cleanexit(134);
          break;
        case 21:                              // Menu 1
          printf("MENU 1 \n");
          CurrentMenu=1;
          setBackColour(255, 255, 255);
          clearScreen();
          setBackColour(0, 0, 0);
          Start_Highlights_Menu1();
          break;
        default:
          printf("Menu 7 Error\n");
        }
        SelectInGroupOnMenu(CurrentMenu, 0, 4, 21, 1);
        Start_Highlights_Menu7();
        UpdateWindow();
        continue;   // Completed Menu 7 action, go and wait for touch
      }

      if (CurrentMenu == 8)  // Menu 8
      {
        printf("Button Event %d, Entering Menu 8 Case Statement\n",i);
        CallingMenu = 8;
        switch (i)
        {
        case 0:                                           // VLC with ffmpeg
        case 1:                                           // OMXPlayer
        case 2:                                           // VLC
        case 3:                                           // UDP Output
          checkTunerSettings();
          setBackColour(0, 0, 0);
          clearScreen();
          LMRX(i);
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu8();
          UpdateWindow();
          break;
        case 4:                                           // Beacon MER
          if (strcmp(LMRXmode, "sat") == 0)
          {
            setBackColour(0, 0, 0);
            clearScreen();
            LMRX(i);
            setBackColour(0, 0, 0);
            clearScreen();
            Start_Highlights_Menu8();
          }
          else                                           // Terrestrial, so set band viewer freq and exit to bandviewer
          {
            if (CheckAirspyConnect() == 0)
            {
              snprintf(ValueToSave, 63, "%d", LMRXfreq[0]);
              SetConfigParam(PATH_AS_CONFIG, "centrefreq", ValueToSave);
              DisplayLogo();
              cleanexit(140);
            }
            else
            {
              if(CheckSDRPlay() == 0)
              {
                snprintf(ValueToSave, 63, "%d", LMRXfreq[0] * 1000);
                SetConfigParam(PATH_SV_CONFIG, "centrefreq", ValueToSave);
                DisplayLogo();
                cleanexit(144);
              }
              else
              {
                if((CheckLimeMiniConnect() == 0) || (CheckLimeUSBConnect() == 0))
                {
                  snprintf(ValueToSave, 63, "%d", LMRXfreq[0]);
                  SetConfigParam(PATH_BV_CONFIG, "centrefreq", ValueToSave);
                  DisplayLogo();
                  cleanexit(136);
                }
                else
                { 
                  if(CheckRTL() == 0)
                  {
                    snprintf(ValueToSave, 63, "%d", LMRXfreq[0]);
                    SetConfigParam(PATH_RS_CONFIG, "centrefreq", ValueToSave);
                    DisplayLogo();
                    cleanexit(141);
                  }
                  else
                  {
                    if(CheckPlutoIPConnect() == 0)
                    {
                      snprintf(ValueToSave, 63, "%d", LMRXfreq[0]);
                      SetConfigParam(PATH_PB_CONFIG, "centrefreq", ValueToSave);
                      DisplayLogo();
                      cleanexit(143);
                    }
                    else
                    {
                      MsgBox2("No LimeSDR, Airspy, SDRplay,", "Pluto or RTL-SDR Connected");
                      wait_touch();
                    }
                  }
                }
              }
            }
          }
          UpdateWindow();
          break;
        case 5:                                          // Change Freq
        case 6:
        case 7:
        case 8:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
          SelectLMFREQ(i);
          Start_Highlights_Menu8();
          UpdateWindow();
          break;
       case 9:                                         // Change freq from keyboard
          ChangeLMPresetFreq(i);
          setBackColour(0, 0, 0);
          clearScreen();
          SelectLMFREQ(i);
          Start_Highlights_Menu8();
          UpdateWindow();
          break;
        case 15:                                          // Change SR
        case 16:
        case 17:
        case 18:
        case 19:
          SelectLMSR(i);
          Start_Highlights_Menu8();
          UpdateWindow();
          break;
        case 20:                                         // Change SR from keyboard
          ChangeLMPresetSR(i);
          setBackColour(0, 0, 0);
          clearScreen();
          SelectLMSR(i);
          Start_Highlights_Menu8();
          UpdateWindow();
          break;
        case 21:
          if (strcmp(LMRXmode, "sat") == 0)
          {
            strcpy(LMRXmode, "terr");
          }
          else
          {
            strcpy(LMRXmode, "sat");
          }
          SetConfigParam(PATH_LMCONFIG, "mode", LMRXmode);
          ResetLMParams();
          Start_Highlights_Menu8();
          UpdateWindow();
          break;
        case 22:                                          // Back to Menu 1
          printf("MENU 1 \n");
          // Revert to the Normal Desktop Image
          strcpy(LinuxCommand, "sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png ");
          strcat(LinuxCommand, ">/dev/null 2>/dev/null");
          system(LinuxCommand);
          strcpy(LinuxCommand, "(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
          system(LinuxCommand);
          CurrentMenu=1;
          setBackColour(255, 255, 255);
          clearScreen();
          setBackColour(0, 0, 0);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 23:                                          // Config Menu 46
          printf("MENU 46\n");
          CurrentMenu=46;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu46();
          UpdateWindow();
          break;
        case 24:                                          // Switch between S/S2 and T/T2
          if (strcmp(RXmod, "DVB-S") == 0)
          {
            strcpy(RXmod, "DVB-T");
          }
          else
          {
            strcpy(RXmod, "DVB-S");
          }
          if (strcmp(RXmod, "DVB-S") == 0)
          {
            SetConfigParam(PATH_LMCONFIG, "rxmod", "dvbs");
          }
          else
          {
            SetConfigParam(PATH_LMCONFIG, "rxmod", "dvbt");
          }
          Start_Highlights_Menu8();
          UpdateWindow();
          break;

        default:
          printf("Menu 8 Error\n");
        }
        continue;   // Completed Menu 8 action, go and wait for touch
      }

      if (CurrentMenu == 9)  // Menu 9 Band Details
      {
        printf("Button Event %d, Entering Menu 9 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Set Band Details Cancel\n");
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 1 from Menu 9\n");
          CurrentMenu=1;
          setBackColour(255, 255, 255);
          clearScreen();
          setBackColour(0, 0, 0);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 0:
        case 1:
        case 2:
        case 3:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
          if (CallingMenu == 315)
          {
            printf("Changing Band Details %d\n", i);
            ChangeBandDetails(i);
            CurrentMenu=9;
            setBackColour(0, 0, 0);
            clearScreen();
            Start_Highlights_Menu9();
          }
          else
          {
            printf("Selecting Band %d\n", i);
            SelectBand(i);
            CurrentMenu=9;
            setBackColour(0, 0, 0);
            clearScreen();
            Start_Highlights_Menu9();
            UpdateWindow();
            usleep(500000);
            printf("Returning to MENU 1 from Menu 9\n");
            CurrentMenu = 1;
            setBackColour(255, 255, 255);
            clearScreen();
            //setBackColour(0, 0, 0);
            Start_Highlights_Menu1();
          }
          UpdateWindow();
          break;
        default:
          printf("Menu 9 Error\n");
        }
        continue;   // Completed Menu 9 action, go and wait for touch
      }

      if (CurrentMenu == 10)  // Menu 10 New TX Frequency
      {
        printf("Button Event %d, Entering Menu 10 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("SR Cancel\n");
          break;
        case 0:                               // Freq 6
        case 1:                               // Freq 7
        case 2:                               // Freq 8
        case 5:                               // Freq 1
        case 6:                               // Freq 2
        case 7:                               // Freq 3
        case 8:                               // Freq 4
        case 9:                               // Freq 5
        case 10:                              // Preset Freq 6
        case 11:                              // Preset Freq 7
        case 12:                              // Preset Freq 8
        case 13:                              // Preset Freq 9
        case 14:                              // Preset Freq 10
        case 15:                              // Preset Freq 1
        case 16:                              // Preset Freq 2
        case 17:                              // Preset Freq 3
        case 18:                              // Preset Freq 4
        case 19:                              // Preset Freq 5
          SelectFreq(i);
          printf("Frequency Button %d\n", i);
          break;
        case 3:                               // Freq 9 Direct Entry
          ChangePresetFreq(i);
          SelectFreq(i);
          printf("Frequency Button %d\n", i);
          break;
        default:
          printf("Menu 10 Error\n");
        }
        if(i != 3)  // Don't pause if frequency has been set on keyboard
        {
          UpdateWindow();
          usleep(500000);
        }
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        if (CallingMenu == 1)
        {
          printf("Returning to MENU 1 from Menu 10\n");
          CurrentMenu=1;
          setBackColour(255, 255, 255);
          clearScreen();
          setBackColour(0, 0, 0);
          Start_Highlights_Menu1();
          UpdateWindow();
        }
        else
        {
          printf("Returning to MENU 5 from Menu 10\n");
          printf("This should not happen!\n");
          CurrentMenu=5;
          clearScreen();
          setBackColour(0, 0, 0);
          Start_Highlights_Menu5();
        }
        UpdateWindow();
        continue;   // Completed Menu 10 action, go and wait for touch
      }

      if (CurrentMenu == 11)  // Menu 11 TX RF Output Mode
      {
        printf("Button Event %d, Entering Menu 11 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("SR Cancel\n");
          break;
        case 5:                               // DVB-S
          SelectTX(i);
          printf("DVB-S\n");
          break;
        case 6:                               // Carrier
          SelectTX(i);
          printf("Carrier\n");
          break;
        case 7:                               // DVB-T
          SelectTX(i);
          printf("DVB-T\n");
          CurrentMenu = 16;                  // Set the guard interval and QAM
          printf("MENU 16 \n");              // on DVB-T selection
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu16();
          UpdateWindow();
          break;
        case 0:                               // QPSK
          SelectTX(i);
          printf("S2 QPSK\n");
          break;
        case 1:                               // S2 8PSK
          SelectTX(i);
          printf("S2 8PSK\n");
          break;
        case 2:                               // S2 16APSK
          SelectTX(i);
          printf("S2 16APSK\n");
          break;
        case 3:                               // S2 32APSK
          SelectTX(i);
          printf("S2 32APSK\n");
          break;
        case 8:                               // Pilots off/on
          SelectPilots();
          printf("Toggle Pilots\n");
          break;
        case 9:                               // Frames Long/short
          //SelectFrames();                   // Not yet working
          printf("Toggle Frames\n");
          break;

        default:
          printf("Menu 11 Error\n");
        }
        if (i != 7)   // Skip if DVB-T guard/QAM needs to be set
        {
          Start_Highlights_Menu11();
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 1 from Menu 11\n");
          CurrentMenu=1;
          setBackColour(255, 255, 255);
          clearScreen();
          Start_Highlights_Menu1();
          UpdateWindow();
        }
        continue;   // Completed Menu 11 action, go and wait for touch
      }

      if (CurrentMenu == 12)  // Menu 12 Encoding
      {
        printf("Button Event %d, Entering Menu 12 Case Statement\n",i);
        switch (i)
        {
        case 1:                               // Raw IPTS in H264
          SelectEncoding(i);
          printf("Raw IPTS in H264\n");
          break;
        case 2:                               // Raw IPTS in H265
          SelectEncoding(i);
          printf("Raw IPTS in H265\n");
          break;
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Encoding Cancel\n");
          break;
        case 5:                               // MPEG-2
          SelectEncoding(i);
          printf("MPEG-2\n");
          break;
        case 6:                               // H264
          SelectEncoding(i);
          printf("H264\n");
          break;
        case 7:                               // H265
          SelectEncoding(i);
          printf("H265\n");
          break;
        case 8:                               // IPTS in
          SelectEncoding(i);
          printf("IPTS in\n");
          break;
        case 9:                               // TS File
          SelectEncoding(i);
          printf("TS File\n");
          break;
        default:
          printf("Menu 12 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 12\n");
        CurrentMenu=1;
        setBackColour(255, 255, 255);
        clearScreen();
        setBackColour(0, 0, 0);
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 12 action, go and wait for touch
      }

      if (CurrentMenu == 13)  // Menu 13 Contest Number Management
      {
        printf("Button Event %d, Entering Menu 13 Case Statement\n",i);
        CallingMenu = 13;
        switch (i)
        {
        case 0:                                         // Edit Site
        case 1:                                         // Load Site
        case 2:                                         // save in-use site or show in-use site
        case 3:                                         // Show Site
        case 5:                                         // Site 1
        case 6:                                         // Site 2
        case 7:                                         // Site 3
        case 8:                                         // Site 4
        case 9:                                         // Site 5
          ManageContestCodes(i);
          Start_Highlights_Menu13();  // Update Menu appearance
          UpdateWindow();             // 
          break;
        case 4:                                         // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          AwaitingContestNumberEditSeln = false;
          AwaitingContestNumberLoadSeln = false;
          AwaitingContestNumberSaveSeln = false;
          AwaitingContestNumberViewSeln = false;
          printf("Menu 13 Cancel\n");
          Start_Highlights_Menu13();  // Update Menu appearance
          UpdateWindow();             // and display for half a second
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 3 from Menu 13\n");
          CurrentMenu=3;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu3();
          UpdateWindow();
          break;
        default:
          printf("Menu 13 Error\n");
        }
        continue;   // Completed Menu 13 action, go and wait for touch
      }

      if (CurrentMenu == 14)  // Menu 14 Video Format
      {
        printf("Button Event %d, Entering Menu 14 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Vid Format Cancel\n");
          break;
        case 5:                               // 4:3
          SelectFormat(i);
          printf("4:3\n");
          break;
        case 6:                               // 16:9
          SelectFormat(i);
          printf("16:9\n");
          break;
        case 7:                               // 720p
          SelectFormat(i);
          printf("720p\n");
          break;
        case 8:                               // 1080p
          SelectFormat(i);
          printf("1080p\n");
          break;
        default:
          printf("Menu 14 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 14\n");
        CurrentMenu=1;
        setBackColour(255, 255, 255);
        clearScreen();
        setBackColour(0, 0, 0);
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 14 action, go and wait for touch
      }

      if (CurrentMenu == 15)  // Menu 15 Pluto Config
      {
        printf("Button Event %d, Entering Menu 15 Case Statement\n",i);
        switch (i)
        {
        case 0:                               // Enter Pluto IP
          ChangePlutoIP();
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu15();
          UpdateWindow();
          break;
        case 1:                               // Reboot Pluto
          RebootPluto(0);
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu15();
          UpdateWindow();
          break;
        case 2:                               // Set Pluto Ref
          printf("Changing Pluto XO\n");
          ChangePlutoXO();
          Start_Highlights_Menu15();
          UpdateWindow();
          break;
        case 3:                               // Check Pluto Expansion
          ChangePlutoAD();
          Start_Highlights_Menu15();
          UpdateWindow();
          break;
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Menu 15 Cancel\n");
          UpdateWindow();
          usleep(500000);
          break;
        case 5:                               // Check Pluto CPUs
          ChangePlutoCPU();
          Start_Highlights_Menu15();
          UpdateWindow();
          break;
        case 6:                               // Enable 2nd Pluto CPU
          if (GetButtonStatus(ButtonNumber(15, 6)) == 1)
          {
            system("/home/pi/rpidatv/scripts/pluto_set_cpu.sh");
            MsgBox2("Pluto 2nd CPU enabled", "You must reboot the Pluto now");
            wait_touch();
            SetButtonStatus(ButtonNumber(15, 6), 0);
          }
          UpdateWindow();
          break;
        case 7:                               // Check Pluto Firmware
          CheckPlutoFirmware();
          Start_Highlights_Menu15();
          UpdateWindow();
          break;
        case 8:                               // Perform Pluto Expansion
          if (GetButtonStatus(ButtonNumber(15, 8)) == 1)
          {
            system("/home/pi/rpidatv/scripts/pluto_set_ad.sh");
            MsgBox2("Pluto expanded to AD9364 status", "You must reboot the Pluto now");
            wait_touch();
            SetButtonStatus(ButtonNumber(15, 8), 0);
          }
          UpdateWindow();
          break;
        case 9:                               // Enter Pluto IP for Langstone
          ChangePlutoIPLangstone();
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu15();
          UpdateWindow();
          break;
        default:
          printf("Menu 15 Error\n");
        }
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)

        if (i != 4) // Stay in Menu 15 unless cancel pressed
        {
          printf("Staying in Menu 15\n");
          CurrentMenu=15;
          Start_Highlights_Menu15();
        }
        else
        {
          printf("Returning to MENU 1 from Menu 15\n");
          CurrentMenu=1;
          Start_Highlights_Menu1();
        }
        UpdateWindow();
        continue;   // Completed Menu 15 action, go and wait for touch
      }

      if (CurrentMenu == 16)  // Menu 16 Guard Interval (was frequency)
      {
        printf("Button Event %d, Entering Menu 16 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel DVBTQAM
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Guard Cancel\n");
          break;
        case 0:                               //   Guard 1/4
        case 1:                               //   Guard 1/8
        case 2:                               //   Guard 1/16
        case 3:                               //   Guard 1/32
          SelectInGroupOnMenu(CurrentMenu, 0, 3, i, 1);
          SelectGuard(i);
          printf("Guard Interval Button %d\n", i);
          break;
        case 5:                               //   qpsk
        case 6:                               //   16-QAM
        case 7:                               //   64-QAM
          SelectInGroupOnMenu(CurrentMenu, 5, 7, i, 1);
          SelectQAM(i);
          printf("DVB-T QAM Button %d\n", i);
          break;
        default:
          printf("Menu 16 Error\n");
        }
        UpdateWindow();
        usleep(1000000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)

        printf("Returning to MENU 1 from Menu 16\n");
        CurrentMenu=1;
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 16 action, go and wait for touch
      }

      if (CurrentMenu == 17)  // Menu 17 Symbol Rate
      {
        printf("Button Event %d, Entering Menu 17 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("SR Cancel\n");
          break;
        case 5:                               // SR 1
          SelectSR(i);
          printf("SR 1\n");
          break;
        case 6:                               // SR 2
          SelectSR(i);
          printf("SR 2\n");
          break;
        case 7:                               // SR 3
          SelectSR(i);
          printf("SR 3\n");
          break;
        case 8:                               // SR 4
          SelectSR(i);
          printf("SR 4\n");
          break;
        case 9:                               // SR 5
          SelectSR(i);
          printf("SR 5\n");
          break;
        case 0:                               // SR 6
          SelectSR(i);
          printf("SR 6\n");
          break;
        case 1:                               // SR 7
          SelectSR(i);
          printf("SR 7\n");
          break;
        case 2:                               // SR 8
          SelectSR(i);
          printf("SR 8\n");
          break;
        case 3:                               // SR 9
          SelectSR(i);
          printf("SR 8\n");
          break;
        default:
          printf("Menu 17 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 17\n");
        CurrentMenu=1;
        setBackColour(255, 255, 255);
        clearScreen();
        setBackColour(0, 0, 0);
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 17 action, go and wait for touch
      }

      if (CurrentMenu == 18)  // Menu 18 FEC
      {
        printf("Button Event %d, Entering Menu 18 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("FEC Cancel\n");
          break;
        case 5:                               // FEC 1/2
          SelectFec(i);
          printf("FEC 1/2\n");
          break;
        case 6:                               // FEC 2/3
          SelectFec(i);
          printf("FEC 2/3\n");
          break;
        case 7:                               // FEC 3/4
          SelectFec(i);
          printf("FEC 3/4\n");
          break;
        case 8:                               // FEC 5/6
          SelectFec(i);
          printf("FEC 5/6\n");
          break;
        case 9:                               // FEC 7/8
          SelectFec(i);
          printf("FEC 7/8\n");
          break;
        default:
          printf("Menu 18 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 18\n");
        CurrentMenu=1;
        setBackColour(255, 255, 255);
        clearScreen();
        setBackColour(0, 0, 0);
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 18 action, go and wait for touch
      }
      if (CurrentMenu == 19)  // Menu 19 Transverter
      {
        printf("Button Event %d, Entering Menu 19 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("FEC Cancel\n");
          break;
        case 5:                               // Band Direct
          SelectBand(i);
          printf("Band Direct\n");
          break;
        case 0:                               // Band t1
          SelectBand(i);
          printf("Band t1\n");
          break;
        case 1:                               // Band t2
          SelectBand(i);
          printf("Band t2\n");
          break;
        case 2:                               // Band t3
          SelectBand(i);
          printf("Band t3\n");
          break;
        case 3:                               // Band t4
          SelectBand(i);
          printf("Band t4\n");
          break;
        default:
          printf("Menu 19 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 19\n");
        CurrentMenu=1;
        setBackColour(255, 255, 255);
        clearScreen();
        setBackColour(0, 0, 0);
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 19 action, go and wait for touch
      }
      if (CurrentMenu == 20)  // Menu 20 Stream Display
      {
        printf("Button Event %d, Entering Menu 20 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Stream Select Cancel\n");
          StreamStoreTrigger = 0;
          SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0);
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 1 from Menu 20\n");
          CurrentMenu=1;
          setBackColour(255, 255, 255);
          clearScreen();
          setBackColour(0, 0, 0);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 5:                               // Preset 1
        case 6:                               // Preset 2
        case 7:                               // Preset 3
        case 8:                               // Preset 4
        case 0:                               // Preset 5
        case 1:                               // Preset 6
        case 2:                               // Preset 7
          if (StreamStoreTrigger == 0)        // Normal
          {
            if (strcmp(StreamPlayer, "OMX") == 0)
            {
              DisplayStream(i);
              setBackColour(0, 0, 0);
              clearScreen();
            }
            else
            {
              DisplayStreamVLC(i);
              setBackColour(0, 0, 0);
              clearScreen();
            }
          }
          else                                // Amend the Preset
          {
            AmendStreamPreset(i);
            StreamStoreTrigger = 0;
            SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0);
            Start_Highlights_Menu20();        // Refresh the button labels
          }
          UpdateWindow();                     // Stay in Menu 20
          break;
        case 3:                               // Preset 8  and direct entry
          AmendStreamPreset(i);
          StreamStoreTrigger = 0;
          SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0);
          if (strcmp(StreamPlayer, "OMX") == 0)
          {
            DisplayStream(i);
            setBackColour(0, 0, 0);
            clearScreen();
          }
          else
          {
            DisplayStreamVLC(i);
            setBackColour(0, 0, 0);
            clearScreen();
          }
          UpdateWindow();                     // Stay in Menu 20
          break;
        case 9:                               // Amend Preset
          if (StreamStoreTrigger == 0)
          {
            StreamStoreTrigger = 1;
            SetButtonStatus(ButtonNumber(CurrentMenu, 9), 1);
          }
          else
          {
            StreamStoreTrigger = 0;
            SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0);
          }
          UpdateWindow();
          break;
          printf("Amend Preset\n");
          break;
        default:
          printf("Menu 20 Error\n");
        }
        continue;   // Completed Menu 20 action, go and wait for touch
      }
      if (CurrentMenu == 21)  // Menu 21 EasyCap
      {
        printf("Button Event %d, Entering Menu 21 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("EasyCap Cancel\n");
          break;
        case 5:                               // Comp Vid
          SelectVidIP(i);
          printf("EasyCap Comp Vid\n");
          break;
        case 6:                               // S-Video
          SelectVidIP(i);
          printf("EasyCap S-Video\n");
          break;
         case 8:                               // PAL
          SelectSTD(i);
          printf("EasyCap PAL\n");
          break;
        case 9:                               // NTSC
          SelectSTD(i);
          printf("EasyCap NTSC\n");
          break;
        default:
          printf("Menu 21 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 21\n");
        CurrentMenu=1;
        setBackColour(255, 255, 255);
        clearScreen();
        setBackColour(0, 0, 0);
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 21 action, go and wait for touch
      }
      if (CurrentMenu == 22)  // Menu 22 Caption
      {
        printf("Button Event %d, Entering Menu 22 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Caption Cancel\n");
          break;
        case 5:                               // Caption Off
          SelectCaption(i);
          printf("Caption Off\n");
          break;
        case 6:                               // Caption On
          SelectCaption(i);
          printf("Caption On\n");
          break;
        default:
          printf("Menu 22 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 22\n");
        CurrentMenu=1;
        setBackColour(255, 255, 255);
        clearScreen();
        setBackColour(0, 0, 0);
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 22 action, go and wait for touch
      }
      if (CurrentMenu == 23)  // Menu 23 Audio
      {
        printf("Button Event %d, Entering Menu 23 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Audio Cancel\n");
          break;
        case 5:                               // Audio Auto
          SelectAudio(i);
          printf("Audio Auto\n");
          break;
        case 6:                               // Audio Mic
          SelectAudio(i);
          printf("Audio Mic\n");
          break;
        case 7:                               // Audio EasyCap
          SelectAudio(i);
          printf("Audio EasyCap\n");
          break;
        case 8:                               // Audio Bleeps
          SelectAudio(i);
          printf("Audio Bleeps\n");
          break;
        case 9:                               // Audio Off
          SelectAudio(i);
          printf("Audio Off\n");
          break;
        case 0:                               // Webcam
          SelectAudio(i);
          printf("Audio Webcam\n");
          break;
        default:
          printf("Menu 23 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 23\n");
        CurrentMenu=1;
        setBackColour(255, 255, 255);
        clearScreen();
        setBackColour(0, 0, 0);
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 23 action, go and wait for touch
      }
      if (CurrentMenu == 24)  // Menu 24 Attenuator Type
      {
        printf("Button Event %d, Entering Menu 24 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Attenuator Cancel\n");
          break;
        case 5:                               // Attenuator NONE
          SelectAtten(i);
          printf("Attenuator None\n");
          break;
        case 6:                               // Attenuator PE4312
          SelectAtten(i);
          printf("Attenuator PE4312\n");
          break;
        case 7:                               // Attenuator PE43713
          SelectAtten(i);
          printf("Attenuator PE43713\n");
          break;
        case 8:                               // Attenuator HMC1119
          SelectAtten(i);
          printf("Attenuator HMC1119\n");
          break;
         default:
          printf("Menu 24 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 24\n");
        CurrentMenu=1;
        setBackColour(255, 255, 255);
        clearScreen();
        setBackColour(0, 0, 0);
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 24 action, go and wait for touch
      }

      if (CurrentMenu == 25)  // Menu 25 DVB-S2 FEC
      {
        printf("Button Event %d, Entering Menu 25 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("FEC Cancel\n");
          break;
        case 5:                               // FEC 1/4
          SelectS2Fec(i);
          printf("FEC 1/4\n");
          break;
        case 6:                               // FEC 1/3
          SelectS2Fec(i);
          printf("FEC 1/3\n");
          break;
        case 7:                               // FEC 1/2
          SelectS2Fec(i);
          printf("FEC 1/2\n");
          break;
        case 8:                               // FEC 3/5
          SelectS2Fec(i);
          printf("FEC 3/5\n");
          break;
        case 9:                               // FEC 2/3
          SelectS2Fec(i);
          printf("FEC 2/3\n");
          break;
        case 0:                               // FEC 3/4
          SelectS2Fec(i);
          printf("FEC 3/4\n");
          break;
        case 1:                               // FEC 5/6
          SelectS2Fec(i);
          printf("FEC 5/6\n");
          break;
        case 2:                               // FEC 8/9
          SelectS2Fec(i);
          printf("FEC 8/9\n");
          break;
        case 3:                               // FEC 9/10
          SelectS2Fec(i);
          printf("FEC 9/10\n");
          break;
        default:
          printf("Menu 25 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 25\n");
        CurrentMenu=1;
        setBackColour(255, 255, 255);
        clearScreen();
        setBackColour(0, 0, 0);
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 25 action, go and wait for touch
      }

      if (CurrentMenu == 26)  // Menu 26 Band Details
      {
        printf("Button Event %d, Entering Menu 26 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Set Band Details Cancel\n");
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 1 from Menu 26\n");
          CurrentMenu=1;
          setBackColour(255, 255, 255);
          clearScreen();
          setBackColour(0, 0, 0);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 0:
        case 1:
        case 2:
        case 3:
        case 5:
        case 6:
          // Set Receive LO
          SetReceiveLOFreq(i);      // Set the LO frequency
          ReceiveLOStart();         // Start the LO if it is required
          printf("Returning to MENU 1 from Menu 26\n");
          CurrentMenu=1;
          setBackColour(255, 255, 255);
          clearScreen();
          setBackColour(0, 0, 0);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        default:
          printf("Menu 26 Error\n");
        }
        // stay in Menu 26 to set another band
        continue;   // Completed Menu 26 action, go and wait for touch
      }
      if (CurrentMenu == 27)  // Menu 27 Preset TX or RX Frequencies
      {
        printf("Button Event %d, Entering Menu 27 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Set Preset Frequency Cancel\n");
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          if (CallingMenu == 3) // TX presets
          {
            printf("Returning to MENU 1 from Menu 27\n");
            CurrentMenu=1;
            setBackColour(255, 255, 255);
            clearScreen();
            setBackColour(0, 0, 0);
            Start_Highlights_Menu1();
          }
          else if (CallingMenu == 46) // RX presets
          {
            printf("Returning to MENU 8 from Menu 27\n");
            CurrentMenu=8;
            setBackColour(0, 0, 0);
            clearScreen();
            Start_Highlights_Menu8();
          }
          UpdateWindow();
          break;
        case 0:
        case 1:
        case 2:
        case 3:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
          printf("Changing Preset Freq Button %d\n", i);
          if (CallingMenu == 3) // TX presets
          {
            ChangePresetFreq(i);
          }
          else if (CallingMenu == 46) // RX presets
          {
            ChangeLMPresetFreq(i);
          }
          CurrentMenu=27;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu27();
          UpdateWindow();
          break;
        default:
          printf("Menu 27 Error\n");
        }
        // stay in Menu 27 if freq changed
        continue;   // Completed Menu 27 action, go and wait for touch
      }
      if (CurrentMenu == 28)  // Menu 28 Preset SRs
      {
        printf("Button Event %d, Entering Menu 28 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Set Preset SR Cancel\n");
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          if (CallingMenu == 3) // TX presets
          {
            printf("Returning to MENU 1 from Menu 28\n");
            CurrentMenu=1;
            setBackColour(255, 255, 255);
            clearScreen();
            setBackColour(0, 0, 0);
            Start_Highlights_Menu1();
          }
          else if (CallingMenu == 46) // RX presets
          {
            printf("Returning to MENU 8 from Menu 28\n");
            CurrentMenu=8;
            setBackColour(0, 0, 0);
            clearScreen();
            Start_Highlights_Menu8();
          }
          UpdateWindow();
          break;
        case 0:
        case 1:
        case 2:
        case 3:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
          printf("Changing Preset SR Button %d\n", i);
          if (CallingMenu == 3) // TX presets
          {
            ChangePresetSR(i);
          }
          else if (CallingMenu == 46) // RX presets
          {
            ChangeLMPresetSR(i);
          }
          CurrentMenu=28;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu28();
          UpdateWindow();
          break;
        default:
          printf("Menu 28 Error\n");
        }
        // stay in Menu 28 if SR changed
        continue;   // Completed Menu 28 action, go and wait for touch
      }
      if (CurrentMenu == 29)  // Menu 29 Call, Locator and PIDs
      {
        printf("Button Event %d, Entering Menu 29 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Set Call/locator/PID Cancel\n");
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 1 from Menu 29\n");
          CurrentMenu=1;
          setBackColour(255, 255, 255);
          clearScreen();
          setBackColour(0, 0, 0);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 0:
        case 1:
        case 2:
          printf("Changing PID\n");
          ChangePID(i);
          CurrentMenu=29;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu29();
          UpdateWindow();
          break;
        case 3:
          break;  // PCR PID can't be changed
        case 5:
          printf("Changing Call\n");
          ChangeCall();
          CurrentMenu=29;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu29();
          UpdateWindow();
          break;
        case 6:
          printf("Changing Locator\n");
          ChangeLocator();
          CurrentMenu=29;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu29();
          UpdateWindow();
          break;
        case 7:
          printf("Going to MENU 13 from Menu 29\n");
          CurrentMenu=13;
          Start_Highlights_Menu13();
          UpdateWindow();
          break;
        default:
          printf("Menu 29 Error\n");
        }
        // stay in Menu 29 if parameter changed
        continue;   // Completed Menu 29 action, go and wait for touch
      }

      if (CurrentMenu == 30)  // Menu 30 HDMI Screen Type
      {
        printf("Button Event %d, Entering Menu 30 Case Statement\n",i);
        switch (i)
        {
        case 3:
          if (reboot_required == true)
          {
            cleanexit(192);    // Commands scheduler to initiate reboot
          }
          break;
        case 4:                                         // Cancel so return to System Config Menu
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Screen Type Cancel\n");
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 43 from Menu 30\n");
          CurrentMenu = 43;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu43();
          UpdateWindow();
          break;
        case 5:
          SetConfigParam(PATH_PCONFIG, "display", "hdmi480");
          system("/home/pi/rpidatv/scripts/set_display_config.sh");
          reboot_required = true;
          Start_Highlights_Menu30();
          UpdateWindow();
          break;
        case 6:
          SetConfigParam(PATH_PCONFIG, "display", "hdmi720");
          system("/home/pi/rpidatv/scripts/set_display_config.sh");
          reboot_required = true;
          Start_Highlights_Menu30();
          UpdateWindow();
          break;
        case 7:
          SetConfigParam(PATH_PCONFIG, "display", "hdmi1080");
          system("/home/pi/rpidatv/scripts/set_display_config.sh");
          reboot_required = true;
          Start_Highlights_Menu30();
          UpdateWindow();
          break;
        case 8:
          SetConfigParam(PATH_PCONFIG, "display", "hdmi");
          system("/home/pi/rpidatv/scripts/set_display_config.sh");
          reboot_required = true;
          Start_Highlights_Menu30();
          UpdateWindow();
          break;
        case 9:
          SetConfigParam(PATH_PCONFIG, "display", "Browser");
          system("/home/pi/rpidatv/scripts/set_display_config.sh");
          reboot_required = true;
          Start_Highlights_Menu30();
          UpdateWindow();
          break;
        default:
          printf("Menu 30 Error\n");
        }
        // stay in Menu 30 
        continue;
      }

      if (CurrentMenu == 31)  // Menu 31 Change Beacon/site details
      {
        printf("Button Event %d, Entering Menu 31 Case Statement\n",i);
        AmendBeacon(i);
        // No exit button, so go straight to menu 3
        printf("Completed band/site change, going to Menu 3 %d\n", i);
        CurrentMenu=3;
        setBackColour(0, 0, 0);
        clearScreen();
        Start_Highlights_Menu3();
        UpdateWindow();
        continue;   // Completed Menu 31 action, go and wait for touch
      }

 
      if (CurrentMenu == 32)  // Menu 32 Select and Change RTL Reference Frequency
      {
        printf("Button Event %d, Entering Menu 32 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Set or change ADFRef\n");
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 1 from Menu 32\n");
          CurrentMenu=1;
          setBackColour(255, 255, 255);
          clearScreen();
          setBackColour(0, 0, 0);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        //case 0:                               // Use ADF4351 Ref 1
        //case 1:                               // Use ADF4351 Ref 2
        //case 2:                               // Use ADF4351 Ref 3
        //case 3:                               // Set ADF5355 Ref
        //case 5:                               // Set ADF4351 Ref 1
        //case 6:                               // Set ADF4351 Ref 2
        //case 7:                               // Set ADF4351 Ref 3
          printf("Changing ADFRef\n");
          ChangeADFRef(i);
          CurrentMenu=32;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu32();
          UpdateWindow();
          break;
        case 9:                               // Set RTL-SDR ppm offset
          ChangeRTLppm();
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu32();          // Refresh button labels
          UpdateWindow();
          break;
        default:
          printf("Menu 32 Error\n");
        }
        // stay in Menu 32 if parameter changed
        continue;   // Completed Menu 32 action, go and wait for touch
      }
      if (CurrentMenu == 33)  // Menu 33 Check for Update
      {
        printf("Button Event %d, Entering Menu 33 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Cancelling Check for Update\n");
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 1 from Menu 33\n");
          CurrentMenu=1;
          setBackColour(255, 255, 255);
          clearScreen();
          setBackColour(0, 0, 0);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 5:                                 // All defined in ExecuteUpdate(i)
        case 6:
        case 7:
          printf("Checking for Update\n");
          ExecuteUpdate(i);
          CurrentMenu=33;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu33();
          UpdateWindow();
          break;
        default:
          printf("Menu 33 Error\n");
        }
        // stay in Menu 33 if parameter changed
        continue;   // Completed Menu 33 action, go and wait for touch
      }

      if (CurrentMenu == 34)  // Menu 34 Start-up App Menu
      {
        printf("Button Event %d, Entering Menu 34 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Cancelling Boot Menu\n");
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 1 from Menu 34\n");
          CurrentMenu=1;
          setBackColour(255, 255, 255);
          clearScreen();
          setBackColour(0, 0, 0);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 0:                               // Boot to Portsdown Test Equip Menu (7)
        case 1:                               // Boot to Transmit Menu 1, button 20
        case 2:                               // Boot to Portsdown Receive Menu 8, button 0
        case 3:                               // Boot to Ryde
        case 5:                               // Boot to Portsdown Menu 1
        case 6:                               // Boot to Langstone
        case 7:                               // Boot to Band Viewer
        case 8:                               // Boot to Meteor Viewer
          ChangeStartApp(i);
          //wait_touch();
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu34();
          UpdateWindow();
          break;
        default:
          printf("Menu 34 Error\n");
        }
        continue;   // Completed Menu 34 action, go and wait for touch
      }

      if (CurrentMenu == 35)  // Menu 35 Stream output Selector
      {
        printf("Button Event %d, Entering Menu 35 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Streamer Select Cancel\n");
          StreamerStoreTrigger = 0;
          SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0);
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 1 from Menu 35\n");
          CurrentMenu=1;
          setBackColour(255, 255, 255);
          clearScreen();
          setBackColour(0, 0, 0);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 5:                               // Preset 1
        case 6:                               // Preset 2
        case 7:                               // Preset 3
        case 8:                               // Preset 4
        case 0:                               // Preset 5
        case 1:                               // Preset 6
        case 2:                               // Preset 7
        case 3:                               // Preset 8
          if (StreamerStoreTrigger == 0)      // Normal
          {
            SelectStreamer(i);
            setBackColour(0, 0, 0);
            clearScreen();
            Start_Highlights_Menu35();        // Refresh the button labels
          }
          else                                // Amend the Preset
          {
            AmendStreamerPreset(i);
            StreamerStoreTrigger = 0;
            SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0);
            Start_Highlights_Menu35();        // Refresh the button labels
          }
          UpdateWindow();                     // Stay in Menu 35
          break;
        case 9:                               // Amend Preset
          if (StreamerStoreTrigger == 0)
          {
            StreamerStoreTrigger = 1;
            SetButtonStatus(ButtonNumber(CurrentMenu, 9), 1);
          }
          else
          {
            StreamerStoreTrigger = 0;
            SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0);
          }
          UpdateWindow();
          break;
          printf("Amend Streamer Preset\n");
          break;
        default:
          printf("Menu 35 Error\n");
        }
        continue;   // Completed Menu 35 action, go and wait for touch
      }

      if (CurrentMenu == 36)  // Menu 36  Configuration
      {
        printf("Button Event %d, Entering Menu 36 Case Statement\n",i);
        switch (i)
        {
        case 3:                               // Disable Wifi
        case 7:                               // Check connection
        case 8:                               // Enable Wifi
        case 9:                               // List Networks
        case 5:                               // Initialise Wifi
        case 6:                               // Set-up Network
          SetButtonStatus(ButtonNumber(36, i), 1);  // Set button green while waiting
          UpdateWindow();
          WiFiConfig(i);
          SetButtonStatus(ButtonNumber(36, i), 0);
          CurrentMenu = 36;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu36();
          UpdateWindow();
          break;
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Cancelling WiFi Config Menu\n");
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 1 from Menu 36\n");
          CurrentMenu=1;
          setBackColour(255, 255, 255);
          clearScreen();
          setBackColour(0, 0, 0);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        default:
          printf("Menu 36 Error\n");
        }
        // stay in Menu 36 if parameter changed
        continue;   // Completed Menu 36 action, go and wait for touch
      }

      if (CurrentMenu == 37)  // Menu 37 Lime Menu for Buster
      {
        printf("Button Event %d, Entering Menu 37 Case Statement\n",i);
        switch (i)
        {
        case 0:                               // Default FW Update
        case 1:                               // Lime FW Update 1.30
        case 2:                               // Lime FW Update DVB
          printf("Lime Firmware Update %d\n", i);
          LimeFWUpdate(i);
          CurrentMenu=37;
          setBackColour(0, 0, 0);
          clearScreen();
          UpdateWindow();
          break;
        case 3:                               // Toggle LimeRFE
          ToggleLimeRFE();
          Start_Highlights_Menu37();
          UpdateWindow();
          break;
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Cancelling Buster Lime Menu\n");
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 1 from Menu 37\n");
          CurrentMenu = 1;
          setBackColour(255, 255, 255);
          clearScreen();
          setBackColour(0, 0, 0);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 5:                               // Set Lime Upsample
          SetLimeUpsample();
          Start_Highlights_Menu37();

          //was                               // Display LimeSuite Info Page
          //LimeUtilInfo();
          //wait_touch();
          //setBackColour(0, 0, 0);
          //clearScreen();

          UpdateWindow();
          break;
        case 6:                               // Display Lime FW Info Page
          LimeInfo();
          wait_touch();
          setBackColour(0, 0, 0);
          clearScreen();
          UpdateWindow();
          break;
        case 7:                               // Display Lime Report Page
          LimeMiniTest();
          wait_touch();
          system("/home/pi/rpidatv/bin/limesdr_stopchannel"); // reset Lime
          setBackColour(0, 0, 0);
          clearScreen();
          UpdateWindow();
          break;
        case 8:                               // Toggle LimeRFE between TX and RX
          if ((LimeRFEMode == 0) && (LimeRFEState == 1))    // RX and enabled
          {
            LimeRFEMode = 1;
            RFE_Mode(rfe, RFE_MODE_TX);
          }
          else if ((LimeRFEMode == 1) && (LimeRFEState == 1))    // TX and enabled
          {
            LimeRFEMode = 0;
            LimeRFERX();
          }
          Start_Highlights_Menu37();
          UpdateWindow();
          break;
        case 9:                               // Set LimeRFE RX Attenuator
          SetLimeRFERXAtt();
          Start_Highlights_Menu37();
          UpdateWindow();
          break;
        default:
          printf("Menu 37 Error\n");
        }
        // stay in Menu 37 if parameter changed
        continue;   // Completed Menu 37 action, go and wait for touch
      }

      if (CurrentMenu == 38)  // Menu 38 Yes/No
      {
        printf("Button Event %d, Entering Menu 38 Case Statement\n",i);
        switch (i)
        {
        case 6:                               // Yes
        case 8:                               // No
          YesNo(i);                            // Decide what to do next
          break;
        default:
          printf("Menu 38 Error\n");
        }
        continue;   // Completed Menu 38 action, go and wait for touch
      }

      if (CurrentMenu == 39)  // Menu 39 Langstone Config
      {
        printf("Button Event %d, Entering Menu 39 Case Statement\n",i);
        switch (i)
        {
        case 0:                               // Enter Pluto IP for Portsdown
          ChangePlutoIP();
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu39();
          UpdateWindow();
          break;
        case 1:                               // Enter Pluto IP for Langstone
          ChangePlutoIPLangstone();
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu39();
          UpdateWindow();
          break;
        case 2:                               // Update Langstone
          if (strcmp(langstone_version, "v1pluto") == 0)  // Langstone V1 installed
          {
            printf("Updating Langstone V1\n");
            SetButtonStatus(ButtonNumber(39, 2), 1);
            UpdateWindow();
            usleep(200000);
            setBackColour(0, 0, 0);
            clearScreen();
            MsgBox4("Updating Langstone V1", " ", "Please Wait", " ");
            UpdateLangstone(1);
          }
          if ((strcmp(langstone_version, "v2pluto") == 0)
           || (strcmp(langstone_version, "v2lime") == 0))   // Langstone V2 installed
          {
            printf("Updating Langstone V2\n");
            SetButtonStatus(ButtonNumber(39, 2), 1);
            UpdateWindow();
            usleep(200000);
            setBackColour(0, 0, 0);
            clearScreen();
            MsgBox4("Updating Langstone V2", " ", "Please Wait", " ");
            UpdateLangstone(2);
          }
          setBackColour(0, 0, 0);
          clearScreen();
          SetButtonStatus(ButtonNumber(39, 2), 0);
          Start_Highlights_Menu39();
          UpdateWindow();
          break;
        case 5:                               // Install Langstone V1
        case 6:                               // Install Langstone V2
          printf("Installing Langstone V1 or V2\n");
          SetButtonStatus(ButtonNumber(39, 5), 1);
          UpdateWindow();
          usleep(200000);
          InstallLangstone(i);
          Start_Highlights_Menu39();
          UpdateWindow();
          break;
        case 7:                               // Toggle between Langstone V2 Lime and Langstone V2 Pluto 
          printf("Changing Langstone V2 between Pluto and Lime\n");
          if (strcmp(langstone_version, "v2lime") == 0)
          {
            strcpy(langstone_version, "v2pluto");
            SetConfigParam(PATH_PCONFIG, "langstone", "v2pluto");
          }
          else if (strcmp(langstone_version, "v2pluto") == 0)
          {
            strcpy(langstone_version, "v2lime");
            SetConfigParam(PATH_PCONFIG, "langstone", "v2lime");
          }
          Start_Highlights_Menu39();
          UpdateWindow();
          break;
        case 4:                               // Exit
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Cancelling Langstone Config Menu\n");
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 1 from Menu 39\n");
          CurrentMenu=1;
          setBackColour(255, 255, 255);
          clearScreen();
          setBackColour(0, 0, 0);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 9:                                     // Select Langstone (Or go to menu for install)
          if (strcmp(langstone_version, "v1pluto") == 0)
          {
            SetButtonStatus(ButtonNumber(39, 9), 1);  // and highlight button
            UpdateWindow();
            RebootPluto(1);                           // Reboot Pluto and wait for recovery                          
            do_Langstone();
            SetButtonStatus(ButtonNumber(39, 9), 0);  // unhighlight button
            setBackColour(0, 0, 0);
            clearScreen();
            UpdateWindow();
          }
          else if (strcmp(langstone_version, "v2pluto") == 0)
          {
            SetButtonStatus(ButtonNumber(39, 9), 1);  // and highlight button
            UpdateWindow();
            RebootPluto(1);                           // Reboot Pluto and wait for recovery                          
            do_Langstone();
            SetButtonStatus(ButtonNumber(39, 9), 0);  // unhighlight button
            setBackColour(0, 0, 0);
            clearScreen();
            UpdateWindow();
          }
          else if (strcmp(langstone_version, "v2lime") == 0)
          {
            SetButtonStatus(ButtonNumber(39, 9), 1);  // and highlight button
            UpdateWindow();
            do_Langstone();
            SetButtonStatus(ButtonNumber(39, 9), 0);  // unhighlight button
            setBackColour(0, 0, 0);
            clearScreen();
            UpdateWindow();
          }
          else                                                    // Langstone not installed yet
          {
            printf("MENU 39 \n");
            CurrentMenu=39;
            Start_Highlights_Menu39();
            UpdateWindow();
          }
          break;
        default:
          printf("Menu 39 Error\n");
        }
        continue;   // Completed Menu 39 action, go and wait for touch
      }

      if (CurrentMenu == 40)  // Menu 40 TS IP Config
      {
        printf("Button Event %d, Entering Menu 40 Case Statement\n",i);
        switch (i)
        {
        case 0:                               // IP TS Out Address
        case 1:                               // IP TS Out Port
        case 2:                               // IP TS In Port
        case 3:                               // TS Filename
          printf("IPTS Config %d\n", i);
          IPTSConfig(i);
          CurrentMenu=40;
          setBackColour(0, 0, 0);
          clearScreen();
          UpdateWindow();
          break;
        case 4:                               // Exit
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Cancelling TS IP Config Menu\n");
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 1 from Menu 40\n");
          CurrentMenu = 1;
          setBackColour(255, 255, 255);
          clearScreen();
          setBackColour(0, 0, 0);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 5:                               // RX UDP Out IP Address
          ChangeLMRXIP();
          CurrentMenu = 40;
          setBackColour(0, 0, 0);
          clearScreen();
          UpdateWindow();
          break;
        case 6:                               // RX UDP Out IP Port
          ChangeLMRXPort();
          CurrentMenu = 40;
          setBackColour(0, 0, 0);
          clearScreen();
          UpdateWindow();
          break;
        default:
          printf("Menu 40 Error\n");
        }
        continue;   // Completed Menu 40 action, go and wait for touch
      }

      if (CurrentMenu == 42)  // Menu 42 Output Device
      {
        printf("Button Event %d, Entering Menu 42 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Output Device Selection Cancelled\n");
          break;
        case 5:                               // IQ
          //SelectOP(i);
          //printf("IQ\n");
          break;
        case 6:                               // QPSKRF
          //SelectOP(i);
          //printf("QPSKRF\n");
          break;
        case 7:                               // EXPRESS
          SelectOP(i);
          printf("EXPRESS\n");
          break;
        case 8:                               // Lime USB
          SelectOP(i);
          printf("LIME USB\n");
          break;
        case 9:                               // STREAMER
          SelectOP(i);
          printf("STREAMER\n");
          break;
        case 0:                               // COMPVID
          //SelectOP(i);
          //printf("COMPVID\n");
          break;
        case 1:                               // DTX-1
          //SelectOP(i);
          //printf("DTX-1\n");
          break;
        case 2:                               // IPTS
          SelectOP(i);
          printf("IPTS\n");
          break;
        case 3:                               // LIME Mini
          SelectOP(i);
          printf("LIME Mini\n");
          break;
        case 10:                              // Jetson Lime
          SelectOP(i);
          printf("Jetson Lime\n");
          break;
        case 12:                              // Muntjac
          RegisterMuntjac();
          SelectOP(i);
          printf("Muntjac\n");
          break;
        case 13:                              // Lime Mini FPGA
          SelectOP(i);
          printf("Lime FPGA\n");
          break;
        case 14:                              // Pluto
          SelectOP(i);
          printf("Pluto\n");
          break;
        default:
          printf("Menu 42 Error\n");
        }
        Start_Highlights_Menu42();  // Update Menu appearance
        UpdateWindow();             // and display for half a second
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 42\n");
        CurrentMenu=1;
        setBackColour(255, 255, 255);
        clearScreen();
        setBackColour(0, 0, 0);
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 42 action, go and wait for touch
      }

      if (CurrentMenu == 43)  // Menu 43 System Configuration
      {
        printf("Button Event %d, Entering Menu 43 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Cancelling System Config Menu\n");
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 1 from Menu 43\n");
          CurrentMenu=1;
          setBackColour(255, 255, 255);
          clearScreen();
          setBackColour(0, 0, 0);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 0:                               // Restore Factory Settings
          CallingMenu = 430;
          CurrentMenu = 38;
          MsgBox4("Are you sure that you want to", "overwrite all the current settings", "with the factory settings?", " ");
          UpdateWindow();
          break;
        case 1:                               // Restore Settings from USB
          if (USBDriveDevice() == 1)
          {
            strcpy(device_name, "/dev/sda1");
          }
          else
          {
            strcpy(device_name, "/dev/sdb1");
          }
          sprintf(linux_cmd, "sudo mount %s /mnt", device_name);

          // mount the USB drive to check drive and files exist first
          system(linux_cmd);
          if (file_exist("/mnt/portsdown_settings/portsdown_config.txt") == 1) // no file found
          {
            MsgBox4("Portsdown configuration files", "not found on USB drive.", "Please check the USB drive and", "try reconnecting it");
            sprintf(linux_cmd, "sudo umount %s", device_name);
            system("sudo umount /dev/sdb1");
            wait_touch();
          }
          else  // file exists
          {
            sprintf(linux_cmd, "sudo umount %s", device_name);
            system("sudo umount /dev/sdb1");
            CallingMenu = 431;
            CurrentMenu = 38;
            MsgBox4("Are you sure that you want to", "overwrite all the current settings", "with the settings from USB?", " ");
          }
          UpdateWindow();
          break;
        case 2:                               // Restore Settings from /boot
          if (file_exist("/boot/portsdown_settings/portsdown_config.txt") == 1) // no file found
          {
            MsgBox4("Portsdown configuration files", "not found in /boot folder.", " ", "Touch screen to exit");
            wait_touch();
          }
          else  // file exists
          {
            CallingMenu = 432;
            CurrentMenu = 38;
            MsgBox4("Are you sure that you want to", "overwrite all the current settings", "with the settings from /boot?", " ");
          }
          UpdateWindow();
          break;
        case 3:                               // Restart the GUI
          cleanexit(129);
          break;
        case 5:                               // Unmount USB drive
          system("pumount /media/usb");
          MsgBox2("USB drive unmounted", "USB drive can safely be removed");
          wait_touch();
          setBackColour(0, 0, 0);
          clearScreen();
          UpdateWindow();
          break;
        case 6:                               // Back-up to USB
          // Test writing file to usb
          system("sudo rm -rf /media/usb/portsdown_write_test.txt");
          if (file_exist("/media/usb/portsdown_write_test.txt") == 0) // File still there
          {
            MsgBox4("USB Drive found, but", "Portsdown is unable to delete files from it", "Please check the USB drive", " ");
            setBackColour(0, 0, 0);
            clearScreen();
            wait_touch();
          }
          else  // test file not present, so try creating it
          {
            system("sudo touch /media/usb/portsdown_write_test.txt");
            if (file_exist("/media/usb/portsdown_write_test.txt") == 1) // File not created
            {
              MsgBox4("Unable to write to USB drive", "Please check the USB drive", "and try reconnecting it", " ");
              setBackColour(0, 0, 0);
              clearScreen();
              wait_touch();
            }
            else  // all good
            {
              system("sudo rm -rf /media/usb/portsdown_write_test.txt");
              CallingMenu = 436;
              CurrentMenu = 38;
              MsgBox4("Are you sure that you want to overwrite", "any stored settings on the USB drive", "with the current settings?", " ");

            }
          }
          UpdateWindow();
          break;
        case 7:                               // Back-up to /boot
          CallingMenu = 437;
          CurrentMenu = 38;
          MsgBox4("Are you sure that you want to overwrite", "any stored settings on the boot drive", "with the current settings?", " ");
          UpdateWindow();
          break;
        case 8:                               // Time overlay on/off
          if (timeOverlay == false)
          {
            timeOverlay = true;
            SetConfigParam(PATH_PCONFIG, "timeoverlay", "on");
          }
          else
          {
            timeOverlay = false;
            SetConfigParam(PATH_PCONFIG, "timeoverlay", "off");
          }
          Start_Highlights_Menu43();
          UpdateWindow();
          break;
        case 9:                               // Toggle enable of hardware shutdown button
          if (file_exist ("/home/pi/.pi-sdn") == 0)  // If enabled
          {
            system("rm /home/pi/.pi-sdn");  // Stop it being loaded at log-on
            system("sudo pkill -x pi-sdn"); // kill the current process
            pinMode(GPIO_SD_LED, OUTPUT);   // Turn the LED Off
            digitalWrite(GPIO_SD_LED, LOW);
          }
          else
          {
            system("cp /home/pi/rpidatv/scripts/configs/text.pi-sdn /home/pi/.pi-sdn");  // load it at logon
            system("/home/pi/.pi-sdn");                                                  // load it now
          }
          Start_Highlights_Menu43();
          UpdateWindow();
          break;
        case 10:                               // Web Control Enable/disable
          togglewebcontrol();
          Start_Highlights_Menu43();
          UpdateWindow();
          break;
        case 11:                               // Select Start-up App
          printf("MENU 34 \n"); 
          CurrentMenu = 34;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu34();
          UpdateWindow();
          break;
        case 12:                               // Screen Type
          if (touchscreen_present == true)     // 5 inch or 7 inch touchscreen
          {
            togglescreentype();
            Start_Highlights_Menu43();
          }
          else                                 // Web control with/without hdmi
          {
            printf("MENU 30 \n");              // Call hdmi display menu
            CurrentMenu = 30;
            setBackColour(0, 0, 0);
            clearScreen();
            Start_Highlights_Menu30();
          }
          UpdateWindow();
          break;
        case 13:                               // Invert Pi Cam
          if(strcmp(CurrentPiCamOrientation, "normal") != 0)  // Not normal, so set to normal
          {
            SetConfigParam(PATH_PCONFIG, "picam", "normal");
            strcpy(CurrentPiCamOrientation, "normal");
          }
          else                                                // normal, so invert
          {
            SetConfigParam(PATH_PCONFIG, "picam", "inverted");
            strcpy(CurrentPiCamOrientation, "inverted");
          }
          Start_Highlights_Menu43();
          UpdateWindow();
          break;
        case 14:                               // Invert 7 Inch
          if (touchscreen_present == true)
          {
            CallingMenu = 4314;
            CurrentMenu = 38;
            MsgBox4("System will reboot immediately", "and restart with inverted display", "Are you sure?", " ");
          }
          UpdateWindow();
          break;
        default:
          printf("Menu 43 Error\n");
        }
        // stay in Menu 43 if parameter changed
        continue;   // Completed Menu 43 action, go and wait for touch
      }

      if (CurrentMenu == 44)  // Menu 44 Jetson Configuration
      {
        printf("Button Event %d, Entering Menu 44 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Cancelling System Config Menu\n");
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 1 from Menu 44\n");
          CurrentMenu=1;
          setBackColour(255, 255, 255);
          clearScreen();
          setBackColour(0, 0, 0);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 0:
          system("/home/pi/rpidatv/scripts/s_jetson.sh");
          SetButtonStatus(ButtonNumber(CurrentMenu, 0), 2); // Greyout Shutdown Jetson
          SetButtonStatus(ButtonNumber(CurrentMenu, 1), 2); // Greyout Reboot Jetson
          UpdateWindow();
          break;
        case 1:
          system("/home/pi/rpidatv/scripts/r_jetson.sh");
          SetButtonStatus(ButtonNumber(CurrentMenu, 0), 2); // Greyout Shutdown Jetson
          SetButtonStatus(ButtonNumber(CurrentMenu, 1), 2); // Greyout Reboot Jetson
          UpdateWindow();
          break;
        case 2:
          printf("Changing Jetson IP\n");
          ChangeJetsonIP();
          CurrentMenu=44;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu44();
          UpdateWindow();
          break;
        case 3:
          printf("Changing LKV373A UDP IP\n");
          ChangeLKVIP();
          CurrentMenu=44;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu44();
          UpdateWindow();
          break;
        case 5:
          printf("Changing Jetson User name\n");
          ChangeJetsonUser();
          CurrentMenu=44;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu44();
          UpdateWindow();
          break;
        case 6:
          printf("Changing Jetson passord\n");
          ChangeJetsonPW();
          CurrentMenu=44;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu44();
          UpdateWindow();
          break;
        case 7:
          printf("Changing Jetson root password\n");
          ChangeJetsonRPW();
          CurrentMenu=44;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu44();
          UpdateWindow();
          break;
        case 8:
          printf("Changing LKV373A UDP Port\n");
          ChangeLKVPort();
          CurrentMenu=44;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu44();
          UpdateWindow();
          break;
        default:
          printf("Menu 44 Error\n");
        }
      }

      if (CurrentMenu == 45)  // Menu 45 Video Source
      {
        printf("Button Event %d, Entering Menu 45 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Vid Format Cancel\n");
          break;
        case 5:                               // PiCam
          SelectSource(i);
          printf("PiCam\n");
          break;
        case 6:                               // CompVid
          SelectSource(i);
          printf("CompVid\n");
          break;
        // case 7:                               // TCAnim No tcanim on RPi 4
        case 8:                               // TestCard
          SelectSource(i);
          printf("TestCard\n");
          break;
        case 9:                               // PiScreen
          SelectSource(i);
          printf("PiScreen\n");
          break;
        case 0:                               // Contest
          SelectSource(i);
          printf("Contest\n");
          break;
        case 1:                               // Webcam
          SelectSource(i);
          printf("Webcam\n");
          break;
        // case 2:                               // Raw C920 not used
        case 3:                               // HDMI
          SelectSource(i);
          printf("HDMI\n");
          break;
        default:
          printf("Menu 45 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 45\n");
        CurrentMenu=1;
        setBackColour(255, 255, 255);
        clearScreen();
        setBackColour(0, 0, 0);
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 45 action, go and wait for touch
      }

      if (CurrentMenu == 46)  // Menu 46 New Receiver Config Menu
      {
        printf("Button Event %d, Entering Menu 46 Case Statement\n",i);
        CallingMenu = 46;
        switch (i)
        {
        case 0:                                         // Output UDP IP
          ChangeLMRXIP();
          CurrentMenu=46;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu46();
          UpdateWindow();
          break;
        case 1:                                         // Output UDP port 
          ChangeLMRXPort();
          CurrentMenu=46;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu46();
          UpdateWindow();
          break;
        case 2:                                         // Change Receive Preset freqss
          printf("MENU 27 \n");
          CurrentMenu=27;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu27();
          UpdateWindow();
          break;
        case 3:                                         // Change Receive Preset SRs 
          printf("MENU 28 \n");
          CurrentMenu=28;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu28();
          UpdateWindow();
          break;
        case 4:                                         // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Menu 46 Cancel\n");
          Start_Highlights_Menu46();  // Update Menu appearance
          UpdateWindow();             // and display for half a second
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 8 from Menu 46\n");
          CurrentMenu=8;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu8();
          UpdateWindow();
          break;
        case 5:                                         // QO-100 Offset
          ChangeLMRXOffset();
          CurrentMenu=46;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu46();
          UpdateWindow();
          break;
        case 6:                                         // Autoset QO-100 Offset
          if (strcmp(LMRXmode, "sat") == 0)
          {
            AutosetLMRXOffset();
          }
          CurrentMenu=46;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu46();
          UpdateWindow();
          break;
        case 7:                                        // Input Socket
          if (strcmp(LMRXinput, "a") == 0)
          {
            strcpy(LMRXinput, "b");
          }
          else
          {
            strcpy(LMRXinput, "a");
          }
          if (strcmp(LMRXmode, "sat") == 0)
          {
            SetConfigParam(PATH_LMCONFIG, "input", LMRXinput);
          }
          else
          {
            SetConfigParam(PATH_LMCONFIG, "input1", LMRXinput);
          }
          Start_Highlights_Menu46();
          UpdateWindow();
          break;
        case 8:                                        // LNB Volts
          CycleLNBVolts();
          Start_Highlights_Menu46();
          UpdateWindow();
          break;
        case 9:                                        // Audio Output
          if (strcmp(LMRXaudio, "rpi") == 0)
          {
            strcpy(LMRXaudio, "usb");
          }
          else if (strcmp(LMRXaudio, "usb") == 0)
          {
            strcpy(LMRXaudio, "hdmi");
          }
          else
          {
            strcpy(LMRXaudio, "rpi");
          }
          SetConfigParam(PATH_LMCONFIG, "audio", LMRXaudio);
          if (file_exist("/home/pi/ryde/config.yaml") == 0)       // Ryde installed
          {
            system("/home/pi/rpidatv/scripts/set_ryde_audio.sh");  // So synchronise Ryde audio
          }
          Start_Highlights_Menu46();
          UpdateWindow();
          break;
        case 10:                                         // Tuner TS Timeout
          ChangeLMTST();
          CurrentMenu=46;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu46();
          UpdateWindow();
          break;
        case 11:                                         // Tuner Scan Width
          ChangeLMSW();
          CurrentMenu=46;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu46();
          UpdateWindow();
          break;
        case 12:                                         // Video Channel for MCPC TS
          ChangeLMChan();
          CurrentMenu=46;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu46();
          UpdateWindow();
          break;
        case 13:                                        // DVB-S/S2 or DVB-T/T2
          if (strcmp(RXmod, "DVB-S") == 0)
          {
            strcpy(RXmod, "DVB-T");
          }
          else
          {
            strcpy(RXmod, "DVB-S");
          }
          if (strcmp(RXmod, "DVB-S") == 0)
          {
            SetConfigParam(PATH_LMCONFIG, "rxmod", "dvbs");
          }
          else
          {
            SetConfigParam(PATH_LMCONFIG, "rxmod", "dvbt");
          }
          Start_Highlights_Menu46();
          UpdateWindow();
          break;
        case 14:                                         // Show overlay
          strcpy(LinuxCommand, "sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/RX_overlay.png ");
          strcat(LinuxCommand, ">/dev/null 2>/dev/null");
          system(LinuxCommand);
          strcpy(LinuxCommand, "(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
          system(LinuxCommand);
          wait_touch();
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu46();
          UpdateWindow();
          break;
        default:
          printf("Menu 46 Error\n");
        }
        continue;   // Completed Menu 46 action, go and wait for touch
      }

      if (CurrentMenu == 47)  // Menu 47 Test Card Selection Menu
      {
        printf("Button Event %d, Entering Menu 47 Case Statement\n",i);
        CallingMenu = 47;
        switch (i)
        {
        case 0:                                         // Change the Test Card
        case 1:
        case 2:
        case 3:
        case 5:
        case 6:
        case 7:
        case 8:
        case 10:
        case 11:
        case 12:
        case 13:
          ChangeTestCard(i);
          Start_Highlights_Menu47();
          UpdateWindow();
          break;
        case 4:                                         // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Menu 47 Cancel\n");
          Start_Highlights_Menu47();  // Update Menu appearance
          UpdateWindow();             // and display for half a second
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 1 from Menu 47\n");
          CurrentMenu = 1;
          setBackColour(255, 255, 255);
          clearScreen();
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 14:                                         // Toggle Caption State 
          if (strcmp(CurrentCaptionState, "off") == 0)
          {
            strcpy(CurrentCaptionState, "on");
            SetConfigParam(PATH_PCONFIG, "caption", "on");
          }
          else
          {
            strcpy(CurrentCaptionState, "off");
            SetConfigParam(PATH_PCONFIG, "caption", "off");
          }
          Start_Highlights_Menu47();
          UpdateWindow();
          break;
        default:
          printf("Menu 47 Error\n");
        }
        continue;   // Completed Menu 47 action, go and wait for touch
      }


      if (CurrentMenu == 41)  // Menu 41 Keyboard (should not get here)
      {
        //break;
      }
    }   
  }
}

void Define_Menu1()
{
  int button = 0;
  strcpy(MenuTitle[1], "BATC Portsdown 4 DATV Transceiver Main Menu"); 

  // Frequency - Bottom Row, Menu 1

  button = CreateButton(1, 0);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(1, 1);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(1, 2);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(1, 3);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(1, 4);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  // 2nd Row, Menu 1.  EasyCap, Caption, Audio and Attenuator type and level

  button = CreateButton(1, 5);                        // EasyCap
  AddButtonStatus(button, "EasyCap^not set", &Blue);
  AddButtonStatus(button, "EasyCap^not set", &Green);
  AddButtonStatus(button, "EasyCap^not set", &Grey);

  button = CreateButton(1, 6);                        // Caption
  AddButtonStatus(button, "Caption^not set", &Blue);
  AddButtonStatus(button, "Caption^not set", &Green);
  AddButtonStatus(button, "Caption^not set", &Grey);

  button = CreateButton(1, 7);                        // Audio
  AddButtonStatus(button, "Audio^not set", &Blue);
  AddButtonStatus(button, "Audio^not set", &Green);
  AddButtonStatus(button, "Audio^not set", &Grey);

  button = CreateButton(1, 8);                       // Attenuator Type
  AddButtonStatus(button, "Atten^not set", &Blue);
  AddButtonStatus(button, "Atten^not set", &Green);
  AddButtonStatus(button, "Atten^not set", &Grey);

  button = CreateButton(1, 9);                        // Atten Level
  AddButtonStatus(button,"Att Level^-",&Blue);
  AddButtonStatus(button,"Att Level^-",&Green);
  AddButtonStatus(button,"Att Level^-",&Grey);

  // Freq, SR, FEC, Transverter and Level - 3rd line up Menu 1

  button = CreateButton(1, 10);                        // Freq
  AddButtonStatus(button, " Freq ^not set", &Blue);
  AddButtonStatus(button, " Freq ^not set", &Green);
  AddButtonStatus(button, " Freq ^not set", &Grey);

  button = CreateButton(1, 11);                        // SR
  AddButtonStatus(button, "Sym Rate^not set", &Blue);
  AddButtonStatus(button, "Sym Rate^not set", &Green);
  AddButtonStatus(button, "Sym Rate^not set", &Grey);

  button = CreateButton(1, 12);                        // FEC
  AddButtonStatus(button, "  FEC  ^not set", &Blue);
  AddButtonStatus(button, "  FEC  ^not set", &Green);
  AddButtonStatus(button, "  FEC  ^not set", &Grey);

  button = CreateButton(1, 13);                        // Transverter
  AddButtonStatus(button,"Band/Tvtr^None",&Blue);
  AddButtonStatus(button,"Band/Tvtr^None",&Green);
  AddButtonStatus(button,"Band/Tvtr^None",&Grey);

  button = CreateButton(1, 14);                        // Level
  AddButtonStatus(button," Level^-",&Blue);
  AddButtonStatus(button," Level^-",&Green);
  AddButtonStatus(button," Level^-",&Grey);

  // Modulation, Vid Encoder, Output Device, Format and Source. 4th line up Menu 1

  button = CreateButton(1, 15);
  AddButtonStatus(button, "TX Mode ^not set", &Blue);
  AddButtonStatus(button, "TX Mode ^not set", &Green);
  AddButtonStatus(button, "TX Mode ^not set", &Grey);

  button = CreateButton(1, 16);
  AddButtonStatus(button, "Encoding^not set", &Blue);
  AddButtonStatus(button, "Encoding^not set", &Green);
  AddButtonStatus(button, "Encoder^not set", &Grey);

  button = CreateButton(1, 17);
  AddButtonStatus(button, "Output to^not set", &Blue);
  AddButtonStatus(button, "Output to^not set", &Green);
  AddButtonStatus(button, "Output to^not set", &Grey);

  button = CreateButton(1, 18);
  AddButtonStatus(button, "Format^not set", &Blue);
  AddButtonStatus(button, "Format^not set", &Green);
  AddButtonStatus(button, "Format^not set", &Grey);

  button = CreateButton(1, 19);
  AddButtonStatus(button, "Source^not set", &Blue);
  AddButtonStatus(button, "Source^not set", &Green);
  AddButtonStatus(button, "Source^not set", &Grey);

  //TRANSMIT RECEIVE BLANK MENU2 - Top of Menu 1

  button = CreateButton(1, 20);
  AddButtonStatus(button," TX  ",&Blue);
  AddButtonStatus(button,"TX ON",&Red);

  button = CreateButton(1, 21);
  AddButtonStatus(button," RX  ",&Blue);
  AddButtonStatus(button,"RX ON",&Green);

  button = CreateButton(1, 22);
  AddButtonStatus(button," M2  ",&Blue);
  AddButtonStatus(button," M2  ",&Green);

  button = CreateButton(1, 23);
  AddButtonStatus(button," M3  ",&Blue);
  AddButtonStatus(button," M3  ",&Green);
}

void Start_Highlights_Menu1()
// Retrieves stored value for each group of buttons
// and then sets the correct highlight and text
{
  char Param[255];
  char Value[255];
  char Leveltext[15];

  // Read the Config from file
  char vcoding[256];
  char vsource[256];
  ReadModeInput(vcoding, vsource);

  printf("Entering Start_Highlights_Menu1\n");

  // Call to Check Grey buttons
  GreyOut1();

  // Presets Buttons 0 - 3
  
  char Presettext[63];

  strcpy(Presettext, "Preset 1^");
  strcat(Presettext, TabPresetLabel[0]);
  AmendButtonStatus(0, 0, Presettext, &Blue);
  AmendButtonStatus(0, 1, Presettext, &Green);
  AmendButtonStatus(0, 2, Presettext, &Grey);

  strcpy(Presettext, "Preset 2^");
  strcat(Presettext, TabPresetLabel[1]);
  AmendButtonStatus(1, 0, Presettext, &Blue);
  AmendButtonStatus(1, 1, Presettext, &Green);
  AmendButtonStatus(1, 2, Presettext, &Grey);

  strcpy(Presettext, "Preset 3^");
  strcat(Presettext, TabPresetLabel[2]);
  AmendButtonStatus(2, 0, Presettext, &Blue);
  AmendButtonStatus(2, 1, Presettext, &Green);
  AmendButtonStatus(2, 2, Presettext, &Grey);

  strcpy(Presettext, "Preset 4^");
  strcat(Presettext, TabPresetLabel[3]);
  AmendButtonStatus(3, 0, Presettext, &Blue);
  AmendButtonStatus(3, 1, Presettext, &Green);
  AmendButtonStatus(3, 2, Presettext, &Grey);

  // Set Preset Button 4

  strcpy(Presettext, "Store^Preset");
  AmendButtonStatus(4, 0, Presettext, &Blue);
  AmendButtonStatus(4, 1, Presettext, &Blue);
  AmendButtonStatus(4, 2, Presettext, &Red);
  AmendButtonStatus(4, 3, Presettext, &Red);

  // EasyCap Button 5

  char EasyCaptext[255];
  strcpy(Param,"analogcaminput");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  printf("Value=%s %s\n",Value," EasyCap Input");
  strcpy(EasyCaptext, "EasyCap^");
  if (strcmp(Value, "0") == 0)
  {
    strcat(EasyCaptext, "Comp Vid");
  }
  else
  {
    strcat(EasyCaptext, "S-Video");
  }
  AmendButtonStatus(5, 0, EasyCaptext, &Blue);
  AmendButtonStatus(5, 1, EasyCaptext, &Green);
  AmendButtonStatus(5, 2, EasyCaptext, &Grey);

  // Caption Button 6

  char Captiontext[255];
  strcpy(Param,"caption");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  printf("Value=%s %s\n", Value, " Caption State");
  strcpy(Captiontext, "Caption^");
  if (strcmp(Value, "off") == 0)
  {
    strcat(Captiontext, "Off");
  }
  else
  {
    strcat(Captiontext, "On");
  }
  AmendButtonStatus(6, 0, Captiontext, &Blue);
  AmendButtonStatus(6, 1, Captiontext, &Green);
  AmendButtonStatus(6, 2, Captiontext, &Grey);

  // Audio Button 7

  char Audiotext[255];
  strcpy(Param,"audio");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  printf("Value=%s %s\n",Value," Audio Selection");
  if (strcmp(Value, "auto") == 0)
  {
    strcpy(Audiotext, "Audio^Auto");
  }
  else if (strcmp(Value, "mic") == 0)
  {
    strcpy(Audiotext, "Audio^USB Mic");
  }
  else if (strcmp(Value, "video") == 0)
  {
    strcpy(Audiotext, "Audio^EasyCap");
  }
  else if (strcmp(Value, "bleeps") == 0)
  {
    strcpy(Audiotext, "Audio^Bleeps");
  }
  else if (strcmp(Value, "webcam") == 0)
  {
    strcpy(Audiotext, "Audio^Webcam");
  }
  else
  {
    strcpy(Audiotext, "Audio^Off");
  }
  AmendButtonStatus(7, 0, Audiotext, &Blue);
  AmendButtonStatus(7, 1, Audiotext, &Green);
  AmendButtonStatus(7, 2, Audiotext, &Grey);

  // Attenuator Type Button 8

  char Attentext[255];
  strcpy(Value, "None");
  strcpy(Param,"attenuator");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  printf("Value=%s %s\n", Value, " Attenuator Selection");
  strcpy (Attentext, "Atten^");
  strcat (Attentext, Value);

  AmendButtonStatus(8, 0, Attentext, &Blue);
  AmendButtonStatus(8, 1, Attentext, &Green);
  AmendButtonStatus(8, 2, Attentext, &Grey);

  // Attenuator Level Button 9
  snprintf(Leveltext, 20, "Att Level^%.2f", TabBandAttenLevel[CurrentBand]);
  AmendButtonStatus(9, 0, Leveltext, &Blue);
  AmendButtonStatus(9, 1, Leveltext, &Green);
  AmendButtonStatus(9, 2, Leveltext, &Grey);

  // Frequency Button 10

  char Freqtext[255];
  char streamname_[31];
  char key_[15];
  float TvtrFreq;
  float RealFreq;
  float DLFreq;

  if (strcmp(CurrentModeOPtext, "BATC^Stream") != 0)             // Not a stream
  {
    // Not Streaming, so display Frequency
    strcpy(Param,"freqoutput");
    GetConfigParam(PATH_PCONFIG, Param, Value);
    if ((TabBandLO[CurrentBand] < 0.1) && (TabBandLO[CurrentBand] > -0.1))  // Direct
    {
      // Check if QO-100
      RealFreq = atof(Value);
      if ((RealFreq > 2400) && (RealFreq < 2410))  // is QO-100
      {
        DLFreq = RealFreq + 8089.50;
        strcpy(Freqtext, "QO-100^");
        snprintf(Value, 10, "%.2f", DLFreq);
        strcat(Freqtext, Value);
      }
      else                                          // Not QO-100
      {
        if (strlen(Value) > 5)
        {
          strcpy(Freqtext, "Freq^");
          strcat(Freqtext, Value);
          strcat(Freqtext, " M");
        }
        else
        {
          strcpy(Freqtext, "Freq^");
          strcat(Freqtext, Value);
          strcat(Freqtext, " MHz");
        }
      }
    }
    else                                                   // Transverter
    {
      strcpy(Freqtext, "IF: ");
      strcat(Freqtext, Value);
      strcat(Freqtext, "^TX:");
      TvtrFreq = atof(Value) + TabBandLO[CurrentBand];
      if (TvtrFreq < 0)
      {
        TvtrFreq = TvtrFreq * -1;
      }
      snprintf(Value, 10, "%.2f", TvtrFreq);
      strcat(Freqtext, Value);
    }
  }
  else
  {
    // Streaming, so display streamname
    strcpy(Freqtext, "Stream to^");
    SeparateStreamKey(StreamKey[0], streamname_, key_);
    strcat(Freqtext, streamname_);
  }
  AmendButtonStatus(10, 0, Freqtext, &Blue);
  AmendButtonStatus(10, 1, Freqtext, &Green);
  AmendButtonStatus(10, 2, Freqtext, &Grey);

  // Symbol Rate Button 11

  char SRtext[255];
  strcpy(Param,"symbolrate");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  printf("Value=%s %s\n",Value,"SR");
  if (strcmp(CurrentTXMode, "DVB-T") != 0)  //not DVB-T
  {
    strcpy(SRtext, "Sym Rate^ ");
  }
  else
  {
    strcpy(SRtext, "Bandwidth^ ");
  }
  strcat(SRtext, Value);
  strcat(SRtext, " ");
  AmendButtonStatus(11, 0, SRtext, &Blue);
  AmendButtonStatus(11, 1, SRtext, &Green);
  AmendButtonStatus(11, 2, SRtext, &Grey);

  // FEC Button 12

  char FECtext[255];
  strcpy(Param,"fec");
  strcpy(Value,"");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  printf("Value=%s %s\n",Value,"Fec");
  fec=atoi(Value);
  switch(fec)
  {
    case 1:strcpy(FECtext, "  FEC  ^  1/2 ") ;break;
    case 2:strcpy(FECtext, "  FEC  ^  2/3 ") ;break;
    case 3:strcpy(FECtext, "  FEC  ^  3/4 ") ;break;
    case 5:strcpy(FECtext, "  FEC  ^  5/6 ") ;break;
    case 7:strcpy(FECtext, "  FEC  ^  7/8 ") ;break;
    case 14:strcpy(FECtext, "  FEC  ^  1/4 ") ;break;
    case 13:strcpy(FECtext, "  FEC  ^  1/3 ") ;break;
    case 12:strcpy(FECtext, "  FEC  ^  1/2 ") ;break;
    case 35:strcpy(FECtext, "  FEC  ^  3/5 ") ;break;
    case 23:strcpy(FECtext, "  FEC  ^  2/3 ") ;break;
    case 34:strcpy(FECtext, "  FEC  ^  3/4 ") ;break;
    case 56:strcpy(FECtext, "  FEC  ^  5/6 ") ;break;
    case 89:strcpy(FECtext, "  FEC  ^  8/9 ") ;break;
    case 91:strcpy(FECtext, "  FEC  ^  9/10 ") ;break;
    default:strcpy(FECtext, "  FEC  ^Error") ;break;
  }
  AmendButtonStatus(12, 0, FECtext, &Blue);
  AmendButtonStatus(12, 1, FECtext, &Green);
  AmendButtonStatus(12, 2, FECtext, &Grey);

  // Band, Button 13
  char Bandtext[31] = "Band/Tvtr^";
  strcat(Bandtext, TabBandLabel[CurrentBand]);
  AmendButtonStatus(13, 0, Bandtext, &Blue);
  AmendButtonStatus(13, 1, Bandtext, &Green);
  AmendButtonStatus(13, 2, Bandtext, &Grey);

  // Device Level, Button 14
  if (strcmp(CurrentModeOP, TabModeOP[2]) == 0)  // DATV Express
  {
    snprintf(Leveltext, 20, "Exp Level^%d", TabBandExpLevel[CurrentBand]);
  }
  else if ((strcmp(CurrentModeOP, TabModeOP[3]) == 0)
        || (strcmp(CurrentModeOP, TabModeOP[8]) == 0)
        || (strcmp(CurrentModeOP, TabModeOP[9]) == 0)
        || (strcmp(CurrentModeOP, TabModeOP[12]) == 0))  // Lime
  {
    snprintf(Leveltext, 20, "Lime Gain^%d", TabBandLimeGain[CurrentBand]);
  }
  else if (strcmp(CurrentModeOP, TabModeOP[11]) == 0)  // Muntjac
  {
    snprintf(Leveltext, 20, "Muntjac Gain^%d", TabBandMuntjacGain[CurrentBand]);
  }
  else if (strcmp(CurrentModeOP, "PLUTO") == 0)  // Pluto
  {
    snprintf(Leveltext, 20, "Pluto Pwr^%d", TabBandPlutoLevel[CurrentBand]);
  }
  else
  {
    snprintf(Leveltext, 20, "OP Level^Fixed");
  }
  AmendButtonStatus(14, 0, Leveltext, &Blue);
  AmendButtonStatus(14, 1, Leveltext, &Green);
  AmendButtonStatus(14, 2, Leveltext, &Grey);

  // TX Modulation Button 15

  char TXModetext[255];
  strcpy(TXModetext, "Modulation^");
  strcat(TXModetext, CurrentTXMode);
  AmendButtonStatus(15, 0, TXModetext, &Blue);
  AmendButtonStatus(15, 1, TXModetext, &Green);
  AmendButtonStatus(15, 2, TXModetext, &Grey);

  // Encoding Button 16

  char Encodingtext[255];
  strcpy(Encodingtext, "Encoder^ ");
  strcat(Encodingtext, CurrentEncoding);
  strcat(Encodingtext, " ");
  AmendButtonStatus(16, 0, Encodingtext, &Blue);
  AmendButtonStatus(16, 1, Encodingtext, &Green);
  AmendButtonStatus(16, 2, Encodingtext, &Grey);

  // Output Device Button 17

  char Outputtext[255];
  strcpy(Outputtext, "Output to^");
  if (strcmp(CurrentModeOPtext, "BATC^Stream") == 0)
  {
    strcpy(Outputtext, "Output to^BATC");
    AmendButtonStatus(17, 1, Outputtext, &Green);
  }
  else if (strcmp(CurrentModeOPtext, "Jetson^Lime") == 0)
  {
    strcpy(Outputtext, "Output to^Jtsn Lime");
    AmendButtonStatus(17, 1, Outputtext, &Green);
  }
  else if ((strcmp(CurrentModeOPtext, "Lime Mini") == 0) || (strcmp(CurrentModeOPtext, "Lime DVB") == 0))
  {
    strcat(Outputtext, CurrentModeOPtext);
    if ((CheckLimeMiniConnect() != 0) && (LimeNETMicroDet != 1))
    {
      AmendButtonStatus(17, 1, Outputtext, &Grey);
    }
    else
    {
      AmendButtonStatus(17, 1, Outputtext, &Green);
    }
  }
  else if (strcmp(CurrentModeOPtext, "Lime USB") == 0)
  {
    strcpy(Outputtext, "Output to^Lime USB");
    if (CheckLimeUSBConnect() != 0)
    {
      AmendButtonStatus(17, 1, Outputtext, &Grey);
    }
    else
    {
      AmendButtonStatus(17, 1, Outputtext, &Green);
    }
  }
  else if (strcmp(CurrentModeOPtext, "Pluto") == 0)
  {
    strcpy(Outputtext, "Output to^Pluto");
    if (CheckPlutoUSBConnect() != 0)
    {
      AmendButtonStatus(17, 1, Outputtext, &Grey);
    }
    else
    {
      AmendButtonStatus(17, 1, Outputtext, &Green);
    }
  }
  else if (strcmp(CurrentModeOPtext, "Express") == 0)
  {
    strcpy(Outputtext, "Output to^Express");
    if (CheckExpressConnect() != 0)
    {
      AmendButtonStatus(17, 1, Outputtext, &Grey);
    }
    else
    {
      AmendButtonStatus(17, 1, Outputtext, &Green);
    }
  }
  else
  {
    strcat(Outputtext, CurrentModeOPtext);
    AmendButtonStatus(17, 1, Outputtext, &Green);
  }
  AmendButtonStatus(17, 0, Outputtext, &Blue);

  AmendButtonStatus(17, 2, Outputtext, &Grey);

  // Video Format Button 18

  char Formattext[255];
  strcpy(Formattext, "Format^ ");
  strcat(Formattext, CurrentFormat);
  strcat(Formattext, " ");
  AmendButtonStatus(18, 0, Formattext, &Blue);
  AmendButtonStatus(18, 1, Formattext, &Green);
  AmendButtonStatus(18, 2, Formattext, &Grey);

  // Video Source Button 19

  char Sourcetext[255];
  char SourceFile[63];
  strcpy(Sourcetext, "Source^");
  if (strcmp(CurrentEncoding, "TS File") == 0)
  {
    strcpy(SourceFile, TSVideoFile);
    chopN(SourceFile, 23);              // cut off /home/pi/rpidatv/video/
    strcat(Sourcetext, SourceFile);
  }
  else
  {
    strcat(Sourcetext, CurrentSource);
  }
  AmendButtonStatus(19, 0, Sourcetext, &Blue);
  AmendButtonStatus(19, 1, Sourcetext, &Green);
  AmendButtonStatus(19, 2, Sourcetext, &Grey);
}

void Define_Menu2()
{
  int button;

  strcpy(MenuTitle[2], "BATC Portsdown 4 DATV Transceiver Menu 2"); 

  // Bottom Row, Menu 2

  button = CreateButton(2, 0);
  AddButtonStatus(button, "Shutdown^ ", &Blue);
  AddButtonStatus(button, "Shutdown^ ", &Green);

  button = CreateButton(2, 1);
  AddButtonStatus(button, "Reboot^ ", &Blue);
  AddButtonStatus(button, "Reboot^ ", &Green);

  button = CreateButton(2, 2);
  AddButtonStatus(button, "Info^ ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(2, 3);
  AddButtonStatus(button, "File^Menu", &Blue);
  AddButtonStatus(button, "File^Menu", &Green);

  button = CreateButton(2, 4);
  AddButtonStatus(button, "Stream^Viewer VLC", &Blue);
  AddButtonStatus(button, "Stream^Viewer VLC", &Green);

  // 2nd line up Menu 2

  button = CreateButton(2, 5);
  AddButtonStatus(button, "Locator^Bearings", &Blue);

  button = CreateButton(2, 6);
  AddButtonStatus(button, "Sites/Bcns^Bearings", &Blue);

  button = CreateButton(2, 7);
  AddButtonStatus(button, "Snap^Check", &Blue);

  button = CreateButton(2, 8);
  AddButtonStatus(button, "Test^Equipment", &Blue);

  button = CreateButton(2, 9);
  AddButtonStatus(button, "Stream^Viewer OMX", &Blue);
  AddButtonStatus(button, "Stream^Viewer OMX", &Green);

  // 3rd line up Menu 2

  button = CreateButton(2, 10);
  AddButtonStatus(button, "Video^Monitor", &Blue);

  button = CreateButton(2, 11);
  AddButtonStatus(button, "Pi Cam^Monitor", &Blue);

  button = CreateButton(2, 12);
  AddButtonStatus(button, "C920^Monitor", &Blue);

  button = CreateButton(2, 13);
  AddButtonStatus(button, "IPTS^Monitor", &Blue);

  button = CreateButton(2, 14);
  AddButtonStatus(button, "HDMI^Monitor", &Blue);

  button = CreateButton(2, 15);
  AddButtonStatus(button, "Switch to^Langstone", &Blue);
  AddButtonStatus(button, "Switch to^Langstone", &Green);

  button = CreateButton(2, 16);
  AddButtonStatus(button, "Sig Gen^ ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(2, 17);
  AddButtonStatus(button, "Band Viewer^ ", &Blue);

  button = CreateButton(2, 18);
  AddButtonStatus(button, "RTL-TCP^Server", &Blue);
  AddButtonStatus(button, "Ryde^Receiver", &Blue);

  button = CreateButton(2, 19);
  AddButtonStatus(button, "RTL-FM^Receiver", &Blue);

  // Top of Menu 2

  button = CreateButton(2, 21);
  AddButtonStatus(button," M1  ",&Blue);
  AddButtonStatus(button," M1  ",&Green);

  button = CreateButton(2, 23);
  AddButtonStatus(button," M3  ",&Blue);
  AddButtonStatus(button," M3  ",&Green);
}

void Start_Highlights_Menu2()
{
  // Check Langstone status
  if ((strcmp(langstone_version, "v1pluto") == 0)
   || (strcmp(langstone_version, "v2pluto") == 0)
   || (strcmp(langstone_version, "v2lime") == 0))
  {
    AmendButtonStatus(ButtonNumber(2, 15), 0, "Switch to^Langstone", &Blue);
    AmendButtonStatus(ButtonNumber(2, 15), 1, "Switch to^Langstone", &Green);
  }
  else
  {
    AmendButtonStatus(ButtonNumber(2, 15), 0, "Langstone^Menu", &Blue);
    AmendButtonStatus(ButtonNumber(2, 15), 1, "Langstone^Menu", &Green);
  }

  if (file_exist("/home/pi/ryde/config.yaml") == 1)       // TCP Server or Ryde
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 18), 0);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 18), 1);
  }
}

void Define_Menu3()
{
  int button;

  strcpy(MenuTitle[3], "Portsdown DATV Transceiver Configuration Menu 3");

  // Bottom Line Menu 3: Check for Update

  button = CreateButton(3, 0);
  AddButtonStatus(button, "Check for^Update", &Blue);
  AddButtonStatus(button, "Checking^for Update", &Green);

  button = CreateButton(3, 1);
  AddButtonStatus(button, "System^Config", &Blue);

  button = CreateButton(3, 2);
  AddButtonStatus(button, "WiFi^Config", &Blue);

  button = CreateButton(3, 3);
  AddButtonStatus(button, "TS IP^Config", &Blue);

  // 2nd line up Menu 3: Lime Config 

  button = CreateButton(3, 5);
  AddButtonStatus(button, "Lime^Config", &Blue);

  button = CreateButton(3, 6);
  AddButtonStatus(button, "Jetson/LKV^Config", &Blue);

  button = CreateButton(3, 7);
  AddButtonStatus(button, "Langstone^Config", &Blue);

  button = CreateButton(3, 8);
  AddButtonStatus(button, "Pluto^Config", &Blue);

  button = CreateButton(3, 9);
  AddButtonStatus(button, "Select^Test Card", &Blue);

  // 3rd line up Menu 3: Amend Sites/Beacons, Set Receive LOs and set Stream Outputs 

  button = CreateButton(3, 10);
  AddButtonStatus(button, "Amend^Sites/Bcns", &Blue);

  button = CreateButton(3, 11);
  AddButtonStatus(button, "Set RX^LOs", &Blue);
  AddButtonStatus(button, "Set RX^LOs", &Green);

  button = CreateButton(3, 12);
  AddButtonStatus(button, "Set Stream^Outputs", &Blue);
  AddButtonStatus(button, "Set Stream^Outputs", &Green);

  button = CreateButton(3, 13);
  AddButtonStatus(button, "Audio out^RPi Jack", &Blue);

  button = CreateButton(3, 14);
  AddButtonStatus(button, "Set USB^Mic Gain", &Blue);

  // 4th line up Menu 3: Band Details, Preset Freqs, Preset SRs, Call and ADFRef

  button = CreateButton(3, 15);
  AddButtonStatus(button, "Set Band^Details", &Blue);
  AddButtonStatus(button, "Set Band^Details", &Green);

  button = CreateButton(3, 16);
  AddButtonStatus(button, "Set Preset^Freqs", &Blue);
  AddButtonStatus(button, "Set Preset^Freqs", &Green);

  button = CreateButton(3, 17);
  AddButtonStatus(button, "Set Preset^SRs", &Blue);
  AddButtonStatus(button, "Set Preset^SRs", &Green);

  button = CreateButton(3, 18);
  AddButtonStatus(button, "Set Call,^Loc & PIDs", &Blue);
  AddButtonStatus(button, "Set Call,^Loc & PIDs", &Green);

  button = CreateButton(3, 19);
  AddButtonStatus(button, "Set ADF^Ref Freq", &Blue);
  AddButtonStatus(button, "Set ADF^Ref Freq", &Green);

  // Top of Menu 3

  button = CreateButton(3, 21);
  AddButtonStatus(button," M1  ",&Blue);

  button = CreateButton(3, 22);
  AddButtonStatus(button," M2  ",&Blue);
}

void Start_Highlights_Menu3()
{
  if (strcmp(LMRXaudio, "rpi") == 0)
  {
    AmendButtonStatus(ButtonNumber(3, 13), 0, "Audio out^RPi Jack", &Blue);
  }
  else if (strcmp(LMRXaudio, "usb") == 0)
  {
    AmendButtonStatus(ButtonNumber(3, 13), 0, "Audio out^USB dongle", &Blue);
  }
  else
  {
    AmendButtonStatus(ButtonNumber(3, 13), 0, "Audio out^HDMI", &Blue);
  }
}

void Define_Menu4()
{
  int button;

  strcpy(MenuTitle[4], "File Menu (4)"); 

  // Bottom Row, Menu 4
  button = CreateButton(4, 0);
  AddButtonStatus(button, "Copy File^for Paste", &Blue);
  AddButtonStatus(button, "Copy File^for Paste", &Green);
  AddButtonStatus(button, "Copy File^for Paste", &Grey);

  button = CreateButton(4, 1);
  AddButtonStatus(button, "Paste File^to Directory", &Blue);
  AddButtonStatus(button, "Paste File^to Directory", &Green);
  AddButtonStatus(button, "Paste File^to Directory", &Grey);

  button = CreateButton(4, 2);
  AddButtonStatus(button, "Rename File", &Blue);
  AddButtonStatus(button, "Rename File", &Green);
  AddButtonStatus(button, "Rename File", &Grey);

  button = CreateButton(4, 3);
  //AddButtonStatus(button, "Delete File^or Directory", &Blue);
  //AddButtonStatus(button, "Delete File^or Directory", &Green);
  //AddButtonStatus(button, "Delete File^or Directory", &Grey);

  button = CreateButton(4, 4);
  AddButtonStatus(button, "Exit", &DBlue);
  AddButtonStatus(button, "Exit", &LBlue);


  // 2nd Row, Menu 4

  button = CreateButton(4, 5);
  AddButtonStatus(button, "File^Explorer", &Blue);
  AddButtonStatus(button, "File^Explorer", &Green);
  AddButtonStatus(button, "File^Explorer", &Grey);

  button = CreateButton(4, 6);
  AddButtonStatus(button, "Install^Ryde RX", &Blue);
  AddButtonStatus(button, "Update^Ryde RX", &Blue);

  button = CreateButton(4, 7);
  AddButtonStatus(button, "List Network^Raspberry Pis", &Blue);

  button = CreateButton(4, 8);
  AddButtonStatus(button, "List Network^Devices", &Blue);

  button = CreateButton(4, 9);
  AddButtonStatus(button, "List USB^Devices", &Blue);
  AddButtonStatus(button, "List USB^Devices", &Green);


  // 3rd Row, Menu 4

  button = CreateButton(4, 10);
  AddButtonStatus(button, "Select^IQ File", &Blue);

  button = CreateButton(4, 11);
  AddButtonStatus(button, "Play IQ^File", &Blue);
  AddButtonStatus(button, "Play IQ^File", &Grey);

  button = CreateButton(4, 12);
  AddButtonStatus(button, "Play IQ^File in loop", &Blue);
  AddButtonStatus(button, "Play IQ^File in loop", &Grey);

  button = CreateButton(4, 13);
  AddButtonStatus(button, "Mount USB^Drive", &Blue);
  AddButtonStatus(button, "Unmount USB^Drive", &Blue);

  button = CreateButton(4, 14);
  AddButtonStatus(button, "Download^HamTV IQ File", &Blue);
  AddButtonStatus(button, " ", &Black);

  // Top of Menu 4

  //button = CreateButton(4, 20);
  //AddButtonStatus(button,"PTT", &Blue);
  //AddButtonStatus(button,"PTT ON", &Red);

  //button = CreateButton(4, 22);
  //AddButtonStatus(button," Exit ", &DBlue);
  //AddButtonStatus(button," Exit ", &LBlue);
}

void Start_Highlights_Menu4()
{
  if (strcmp(MenuText[0], "Selected File:") == 0)       // Grey out if no file is selected
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 1), 0);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 1), 2);
  }

  if (file_exist("/home/pi/ryde/config.yaml") == 1)       // Ryde Install or Update
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 1);
  }

  if (ValidIQFileSelected == true)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 11), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 12), 0);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 11), 1);
    SetButtonStatus(ButtonNumber(CurrentMenu, 12), 1);
  }

  if (USBmounted() == 1)       // USB Drive not mounted
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 13), 0);
  }
  else  // mounted
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 13), 1);
  }

  if (file_exist("/home/pi/iqfiles/SDRSharp_20160423_121611Z_731000000Hz_IQ.wav") == 0)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 14), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 14), 0);
  }
}

void Define_Menu5()
{
  // int button = 0;

  strcpy(MenuTitle[5], "Menu (5)"); 

  // Menu 5

  // 2nd Row, Menu 5.  

  // 3rd line up Menu 5


  // 4th line up Menu 5


  // Top of Menu 5

}

void Start_Highlights_Menu5()
{
}

void Define_Menu6()
{
  int button = 0;
  strcpy(MenuTitle[6], "RTL-FM Audio Receiver Menu (6)"); 

  // Presets - Bottom Row, Menu 6

  button = CreateButton(6, 0);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(6, 1);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(6, 2);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(6, 3);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(6, 4);
  AddButtonStatus(button, "Store^Preset", &Blue);
  AddButtonStatus(button, "Store^Preset", &Red);

  // 2nd Row, Menu 6.  Presets

  button = CreateButton(6, 5);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(6, 6);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(6, 7);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(6, 8);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(6, 9);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  // AM, FM, WBFM, USB, LSB - 3rd line up Menu 6

  button = CreateButton(6, 10);                        // AM
  AddButtonStatus(button, "  AM  ", &Blue);
  AddButtonStatus(button, "  AM  ", &Green);

  button = CreateButton(6, 11);                        // FM
  AddButtonStatus(button, " NBFM ", &Blue);
  AddButtonStatus(button, " NBFM ", &Green);

  button = CreateButton(6, 12);                        // WBFM
  AddButtonStatus(button, " WBFM ", &Blue);
  AddButtonStatus(button, " WBFM ", &Green);

  button = CreateButton(6, 13);                        // USB
  AddButtonStatus(button," USB  ",&Blue);
  AddButtonStatus(button," USB  ",&Green);

  button = CreateButton(6, 14);                        // LSB
  AddButtonStatus(button," LSB  ",&Blue);
  AddButtonStatus(button," LSB  ",&Green);

  // Freq, Squelch setting, Squelch on/off, blank, freq ppm. 4th line up Menu 6

  button = CreateButton(6, 15);
  AddButtonStatus(button, " Freq ^not set", &Blue);
  AddButtonStatus(button, " Freq ^not set", &Green);

  button = CreateButton(6, 16);
  AddButtonStatus(button, "Squelch^Level   ", &Blue);
  AddButtonStatus(button, "Squelch^Level   ", &Green);
  AddButtonStatus(button, "Squelch^Level   ", &Grey);

  button = CreateButton(6, 17);
  AddButtonStatus(button, "Squelch^  ON  ", &Blue);
  AddButtonStatus(button, "Squelch^  OFF  ", &Blue);

  button = CreateButton(6, 18);
  AddButtonStatus(button, "Save^Settings", &Blue);
  AddButtonStatus(button, "Save^Settings", &Green);

  button = CreateButton(6, 19);
  AddButtonStatus(button, "Gain", &Blue);
  AddButtonStatus(button, "Gain", &Green);

  //RECEIVE and Exit - Top of Menu 6

  button = CreateButton(6, 21);
  AddButtonStatus(button," RX  ",&Blue);
  AddButtonStatus(button,"RX ON",&Red);

  button = CreateButton(6, 22);
  AddButtonStatus(button,"EXIT",&Blue);
  AddButtonStatus(button,"EXIT",&Green);
}

void Start_Highlights_Menu6()
{
  int index;
  char RTLBtext[63];
  int NoButton;
  int Match = 0;

  // Display the frequency
  strcpy(RTLBtext, " Freq ^");
  strcat(RTLBtext, RTLfreq[0]);
  AmendButtonStatus(ButtonNumber(6, 15), 0, RTLBtext, &Blue);
  AmendButtonStatus(ButtonNumber(6, 15), 1, RTLBtext, &Green);

  // Display the Squelch level
  snprintf(RTLBtext, 50, "Squelch^Level %d", RTLsquelch[0]);
  AmendButtonStatus(ButtonNumber(6, 16), 0, RTLBtext, &Blue);
  AmendButtonStatus(ButtonNumber(6, 16), 1, RTLBtext, &Green);

  // Display Squelch off/on
  if (RTLsquelchoveride == 1) // squelch on
  {
      SelectInGroupOnMenu(6, 17, 17, 17, 0);
  }
  else // squelch off
  {
      SelectInGroupOnMenu(6, 17, 17, 17, 1);
  }

  // Display the Gain
  if (RTLgain[0] >= 0)
  {
    snprintf(RTLBtext, 20, "Gain^%d", RTLgain[0]);
  }
  else        // gain auto
  {
    strcpy(RTLBtext, "Gain^Auto");
  }
  AmendButtonStatus(ButtonNumber(6, 19), 0, RTLBtext, &Blue);
  AmendButtonStatus(ButtonNumber(6, 19), 1, RTLBtext, &Green);

  // Highlight the current mode
  if (strcmp(RTLmode[0], "am") == 0)
  {
    SelectInGroupOnMenu(6, 10, 14, 10, 1);
  }
  else if (strcmp(RTLmode[0], "fm") == 0)
  {
    SelectInGroupOnMenu(6, 10, 14, 11, 1);
  }
  else if (strcmp(RTLmode[0], "wbfm") == 0)
  {
    SelectInGroupOnMenu(6, 10, 14, 12, 1);
  }
  else if (strcmp(RTLmode[0], "usb") == 0)
  {
    SelectInGroupOnMenu(6, 10, 14, 13, 1);
  }
  else if (strcmp(RTLmode[0], "lsb") == 0)
  {
    SelectInGroupOnMenu(6, 10, 14, 14, 1);
  }
 
  // Highlight current preset by comparing frequency
  for(index = 1; index < 10 ; index = index + 1)
  {
    // Define the button text
    snprintf(RTLBtext, 35, "%s^%s", RTLlabel[index], RTLfreq[index]);

    NoButton = index + 4;   // Valid for top row
    if (index > 5)          // Overwrite for bottom row
    {
      NoButton = index - 6;
    }
    AmendButtonStatus(ButtonNumber(6, NoButton), 0, RTLBtext, &Blue);
    AmendButtonStatus(ButtonNumber(6, NoButton), 1, RTLBtext, &Green);

    if (atof(RTLfreq[index]) == atof(RTLfreq[0]))
    {
      SelectInGroupOnMenu(6, 0, 3, NoButton, 1);
      SelectInGroupOnMenu(6, 5, 9, NoButton, 1);
      Match = 1;
    }
  }

  // If current freq not a preset, unhighlight all buttons
  if (Match == 0)
  {
    for(index = 1; index < 10 ; index = index + 1)
    {
      NoButton = index + 4;   // Valid for top row
      if (index > 5)          // Overwrite for bottom row
      {
        NoButton = index - 6;
      }

      SelectInGroupOnMenu(6, 0, 3, NoButton, 0);
      SelectInGroupOnMenu(6, 5, 9, NoButton, 0);
    }
  }

  // Make the RX button red if RX on
  SetButtonStatus(ButtonNumber(6, 21), RTLactive); 
}

void Define_Menu7()
{
  int button;

  strcpy(MenuTitle[7], "BATC Portsdown 4 Test Equipment Menu (7)");

  // Bottom Line Menu 7: User Buttons

  button = CreateButton(7, 0);
  AddButtonStatus(button, "Airspy^BandViewer", &Blue);

  button = CreateButton(7, 1);
  AddButtonStatus(button, "LimeSDR^BandViewer", &Blue);

  button = CreateButton(7, 2);
  AddButtonStatus(button, "Pluto^BandViewer", &Blue);

  button = CreateButton(7, 3);
  AddButtonStatus(button, "RTL-SDR^BandViewer", &Blue);

  button = CreateButton(7, 4);
  AddButtonStatus(button, "SDR Play^BandViewer", &Blue);

  // 2nd line up Menu 7:  

  button = CreateButton(7, 5);
  AddButtonStatus(button, "Frequency^Sweeper", &Blue);

  button = CreateButton(7, 6);
  AddButtonStatus(button, "Noise Figure^Meter (Lime)", &Blue);

  button = CreateButton(7, 7);
  AddButtonStatus(button, "Noise Figure^Meter (Pluto)", &Blue);

  button = CreateButton(7, 8);
  AddButtonStatus(button, "DMM^Display", &Blue);

  button = CreateButton(7, 9);
  AddButtonStatus(button, "Meteor^Viewer", &Blue);

  // 3rd line up Menu 7:

  button = CreateButton(7, 10);
  AddButtonStatus(button, "Signal^Generator", &Blue);

  button = CreateButton(7, 11);
  AddButtonStatus(button, "Noise^Meter (Lime)", &Blue);

  button = CreateButton(7, 12);
  AddButtonStatus(button, "Noise^Meter (Pluto)", &Blue);

  button = CreateButton(7, 13);
  AddButtonStatus(button, "Power^Meter", &Blue);

  button = CreateButton(7, 14);
  AddButtonStatus(button, "XY^Display", &Blue);

  // 4th line up Menu 7: 

  //button = CreateButton(7, 18);
  //AddButtonStatus(button, "Noise Figure^Meter (Pluto)", &Blue);

  // Top of Menu 7

  button = CreateButton(7, 21);
  AddButtonStatus(button," M1  ",&Blue);
}


void Start_Highlights_Menu7()
{
}


void Define_Menu8()
{
  int button = 0;

  strcpy(MenuTitle[8], "Portsdown Receiver Menu (8)"); 

  // Bottom Row, Menu 8

  button = CreateButton(8, 0);
  AddButtonStatus(button, "RECEIVE", &Blue);
  AddButtonStatus(button, "RECEIVE", &Grey);

  button = CreateButton(8, 1);
  AddButtonStatus(button, "RX with^OMX Player", &Blue);
  AddButtonStatus(button, "RX with^OMX Player", &Grey);

  button = CreateButton(8, 2);
  AddButtonStatus(button, "RX DVB-S2^No Scan", &Blue);
  AddButtonStatus(button, "RX DVB-S2^No Scan", &Grey);

  button = CreateButton(8, 3);
  AddButtonStatus(button, "Play to^UDP Stream", &Blue);
  AddButtonStatus(button, "Play to^UDP Stream", &Grey);

  button = CreateButton(8, 4);
  AddButtonStatus(button, "Beacon^MER", &Blue);
  AddButtonStatus(button, "Beacon^MER", &Grey);
  AddButtonStatus(button, "Band Viewer^on RX freq", &Blue);
  AddButtonStatus(button, "Band Viewer^on RX freq", &Grey);

  // 2nd Row, Menu 8.  

  button = CreateButton(8, 5);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(8, 6);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(8, 7);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(8, 8);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(8, 9);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  // 3rd line up Menu 8

  button = CreateButton(8, 10);    
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(8, 11);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(8, 12);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(8, 13);
  AddButtonStatus(button," ",&Blue);
  AddButtonStatus(button," ",&Green);

  button = CreateButton(8, 14);
  AddButtonStatus(button," ",&Blue);
  AddButtonStatus(button," ",&Green);

  // 4th line up Menu 8

  button = CreateButton(8, 15);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(8, 16);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);
  AddButtonStatus(button, " ", &Grey);

  button = CreateButton(8, 17);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Blue);

  button = CreateButton(8, 18);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(8, 19);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(8, 20);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  // - Top of Menu 8

  button = CreateButton(8, 21);
  AddButtonStatus(button, "QO-100", &Blue);
  AddButtonStatus(button, "Terre^strial", &Blue);

  button = CreateButton(8, 22);
  AddButtonStatus(button, "EXIT", &Blue);
  AddButtonStatus(button, "EXIT", &Red);

  button = CreateButton(8, 23);
  AddButtonStatus(button, "Config", &Blue);
  AddButtonStatus(button, "Config", &Green);

  button = CreateButton(8, 24);
  AddButtonStatus(button, "DVB-S/S2", &Blue);
  AddButtonStatus(button, "DVB-T/T2", &Blue);
}


void Start_Highlights_Menu8()
{
  int indexoffset = 0;
  char LMBtext[11][21];
  char LMBStext[31];
  int i;
  int FreqIndex;
  div_t div_10;
  div_t div_100;
  div_t div_1000;

  if (strcmp(RXmod, "DVB-S") == 0)
  {
    strcpy(MenuTitle[8], "Portsdown DVB-S/S2 Receiver Menu (8)"); 
    SetButtonStatus(ButtonNumber(CurrentMenu, 1), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 2), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 24), 0);
  }
  else
  {
    strcpy(MenuTitle[8], "Portsdown DVB-T/T2 Receiver Menu (8)"); 
    SetButtonStatus(ButtonNumber(CurrentMenu, 1), 1);
    SetButtonStatus(ButtonNumber(CurrentMenu, 2), 1);
    SetButtonStatus(ButtonNumber(CurrentMenu, 24), 1);
  }

  // Sort Beacon MER Button / BandViewer Button

  if (strcmp(LMRXmode, "terr") == 0)
  {
    indexoffset = 10;
    if((CheckLimeMiniConnect() == 0) || (CheckLimeUSBConnect() == 0) || (CheckAirspyConnect() == 0) || (CheckRTL() == 0)
        || (CheckSDRPlay() == 0) || (CheckPlutoIPConnect() == 0))
    {
      SetButtonStatus(ButtonNumber(CurrentMenu, 4), 2);
    }
    else  // Grey out BandViewer Button
    {
      SetButtonStatus(ButtonNumber(CurrentMenu, 4), 3);
    }
  }
  else     // QO-100
  {
    if (strcmp(RXmod, "DVB-T") == 0)  // Grey out for DVB-T
    {
      SetButtonStatus(ButtonNumber(CurrentMenu, 4), 1);
    }
    else                             // DVB-S or S2
    {
      SetButtonStatus(ButtonNumber(CurrentMenu, 4), 0);
    }
  }

  // Freq buttons

  for(i = 1; i <= 10; i = i + 1)
  {
    if (i <= 5)
    {
      FreqIndex = i + 5 + indexoffset;
    }
    else
    {
      FreqIndex = i + indexoffset - 5;    
    }
    div_10 = div(LMRXfreq[FreqIndex], 10);
    div_1000 = div(LMRXfreq[FreqIndex], 1000);

    if(div_10.rem != 0)  // last character not zero, so make answer of form xxx.xxx
    {
      snprintf(LMBtext[i], 15, "%d.%03d", div_1000.quot, div_1000.rem);
    }
    else
    {
      div_100 = div(LMRXfreq[FreqIndex], 100);

      if(div_100.rem != 0)  // last but one character not zero, so make answer of form xxx.xx
      {
        snprintf(LMBtext[i], 15, "%d.%02d", div_1000.quot, div_1000.rem / 10);
      }
      else
      {
        if(div_1000.rem != 0)  // last but two character not zero, so make answer of form xxx.x
        {
          snprintf(LMBtext[i], 15, "%d.%d", div_1000.quot, div_1000.rem / 100);
        }
        else  // integer MHz, so just xxx.0
        {
          snprintf(LMBtext[i], 15, "%d.0", div_1000.quot);
        }
      }
    }
    if (i == 5)
    {
      strcat(LMBtext[i], "^Keyboard");
    }
    else
    {
      strcat(LMBtext[i], "^MHz");
    }
  }

  AmendButtonStatus(ButtonNumber(8, 5), 0, LMBtext[1], &Blue);
  AmendButtonStatus(ButtonNumber(8, 5), 1, LMBtext[1], &Green);
  
  AmendButtonStatus(ButtonNumber(8, 6), 0, LMBtext[2], &Blue);
  AmendButtonStatus(ButtonNumber(8, 6), 1, LMBtext[2], &Green);
  
  AmendButtonStatus(ButtonNumber(8, 7), 0, LMBtext[3], &Blue);
  AmendButtonStatus(ButtonNumber(8, 7), 1, LMBtext[3], &Green);
  
  AmendButtonStatus(ButtonNumber(8, 8), 0, LMBtext[4], &Blue);
  AmendButtonStatus(ButtonNumber(8, 8), 1, LMBtext[4], &Green);
  
  AmendButtonStatus(ButtonNumber(8, 9), 0, LMBtext[5], &Blue);
  AmendButtonStatus(ButtonNumber(8, 9), 1, LMBtext[5], &Green);
  
  AmendButtonStatus(ButtonNumber(8, 10), 0, LMBtext[6], &Blue);
  AmendButtonStatus(ButtonNumber(8, 10), 1, LMBtext[6], &Green);
  
  AmendButtonStatus(ButtonNumber(8, 11), 0, LMBtext[7], &Blue);
  AmendButtonStatus(ButtonNumber(8, 11), 1, LMBtext[7], &Green);
  
  AmendButtonStatus(ButtonNumber(8, 12), 0, LMBtext[8], &Blue);
  AmendButtonStatus(ButtonNumber(8, 12), 1, LMBtext[8], &Green);
  
  AmendButtonStatus(ButtonNumber(8, 13), 0, LMBtext[9], &Blue);
  AmendButtonStatus(ButtonNumber(8, 13), 1, LMBtext[9], &Green);
  
  AmendButtonStatus(ButtonNumber(8, 14), 0, LMBtext[10], &Blue);
  AmendButtonStatus(ButtonNumber(8, 14), 1, LMBtext[10], &Green);

  if ( LMRXfreq[0] == LMRXfreq[6 + indexoffset] )
  {
    SelectInGroupOnMenu(8, 5, 14, 5, 1);
  }
  else if ( LMRXfreq[0] == LMRXfreq[7 + indexoffset] )
  {
    SelectInGroupOnMenu(8, 5, 14, 6, 1);
  }
  else if ( LMRXfreq[0] == LMRXfreq[8 + indexoffset] )
  {
    SelectInGroupOnMenu(8, 5, 14, 7, 1);
  }
  else if ( LMRXfreq[0] == LMRXfreq[9 + indexoffset] )
  {
    SelectInGroupOnMenu(8, 5, 14, 8, 1);
  }
  else if ( LMRXfreq[0] == LMRXfreq[10 + indexoffset] )
  {
    SelectInGroupOnMenu(8, 5, 14, 9, 1);
  }
  else if ( LMRXfreq[0] == LMRXfreq[1 + indexoffset] )
  {
    SelectInGroupOnMenu(8, 5, 14, 10, 1);
  }
  else if ( LMRXfreq[0] == LMRXfreq[2 + indexoffset] )
  {
    SelectInGroupOnMenu(8, 5, 14, 11, 1);
  }
  else if ( LMRXfreq[0] == LMRXfreq[3 + indexoffset] )
  {
    SelectInGroupOnMenu(8, 5, 14, 12, 1);
  }
  else if ( LMRXfreq[0] == LMRXfreq[4 + indexoffset] )
  {
    SelectInGroupOnMenu(8, 5, 14, 13, 1);
  }
  else if ( LMRXfreq[0] == LMRXfreq[5 + indexoffset] )
  {
    SelectInGroupOnMenu(8, 5, 14, 14, 1);
  }

  // SR buttons

  if (strcmp(LMRXmode, "terr") == 0)
  {
    indexoffset = 6;
  }

  if (strcmp(RXmod, "DVB-S") == 0)
  {
    snprintf(LMBStext, 15, "SR^%d", LMRXsr[1 + indexoffset]);
    AmendButtonStatus(ButtonNumber(8, 15), 0, LMBStext, &Blue);
    AmendButtonStatus(ButtonNumber(8, 15), 1, LMBStext, &Green);
  
    snprintf(LMBStext, 15, "SR^%d", LMRXsr[2 + indexoffset]);
    AmendButtonStatus(ButtonNumber(8, 16), 0, LMBStext, &Blue);
    AmendButtonStatus(ButtonNumber(8, 16), 1, LMBStext, &Green);
  
    snprintf(LMBStext, 15, "SR^%d", LMRXsr[3 + indexoffset]);
    AmendButtonStatus(ButtonNumber(8, 17), 0, LMBStext, &Blue);
    AmendButtonStatus(ButtonNumber(8, 17), 1, LMBStext, &Green);
  
    snprintf(LMBStext, 15, "SR^%d", LMRXsr[4 + indexoffset]);
    AmendButtonStatus(ButtonNumber(8, 18), 0, LMBStext, &Blue);
    AmendButtonStatus(ButtonNumber(8, 18), 1, LMBStext, &Green);
  
    snprintf(LMBStext, 15, "SR^%d", LMRXsr[5 + indexoffset]);
    AmendButtonStatus(ButtonNumber(8, 19), 0, LMBStext, &Blue);
    AmendButtonStatus(ButtonNumber(8, 19), 1, LMBStext, &Green);
  
    snprintf(LMBStext, 25, "SR %d^Keyboard", LMRXsr[6 + indexoffset]);
    AmendButtonStatus(ButtonNumber(8, 20), 0, LMBStext, &Blue);
    AmendButtonStatus(ButtonNumber(8, 20), 1, LMBStext, &Green);
  }
  else
  {
    snprintf(LMBStext, 21, "Bandwidth^%d kHz", LMRXsr[1 + indexoffset]);
    AmendButtonStatus(ButtonNumber(8, 15), 0, LMBStext, &Blue);
    AmendButtonStatus(ButtonNumber(8, 15), 1, LMBStext, &Green);
  
    snprintf(LMBStext, 21, "Bandwidth^%d kHz", LMRXsr[2 + indexoffset]);
    AmendButtonStatus(ButtonNumber(8, 16), 0, LMBStext, &Blue);
    AmendButtonStatus(ButtonNumber(8, 16), 1, LMBStext, &Green);
  
    snprintf(LMBStext, 21, "Bandwidth^%d kHz", LMRXsr[3 + indexoffset]);
    AmendButtonStatus(ButtonNumber(8, 17), 0, LMBStext, &Blue);
    AmendButtonStatus(ButtonNumber(8, 17), 1, LMBStext, &Green);
  
    snprintf(LMBStext, 21, "Bandwidth^%d kHz", LMRXsr[4 + indexoffset]);
    AmendButtonStatus(ButtonNumber(8, 18), 0, LMBStext, &Blue);
    AmendButtonStatus(ButtonNumber(8, 18), 1, LMBStext, &Green);
  
    snprintf(LMBStext, 21, "Bandwidth^%d kHz", LMRXsr[5 + indexoffset]);
    AmendButtonStatus(ButtonNumber(8, 19), 0, LMBStext, &Blue);
    AmendButtonStatus(ButtonNumber(8, 19), 1, LMBStext, &Green);
  
    snprintf(LMBStext, 25, "BW %d kHz^Keyboard", LMRXsr[6 + indexoffset]);
    AmendButtonStatus(ButtonNumber(8, 20), 0, LMBStext, &Blue);
    AmendButtonStatus(ButtonNumber(8, 20), 1, LMBStext, &Green);
  }

  if ( LMRXsr[0] == LMRXsr[1 + indexoffset] )
  {
    SelectInGroupOnMenu(8, 15, 20, 15, 1);
  }
  else if ( LMRXsr[0] == LMRXsr[2 + indexoffset] )
  {
    SelectInGroupOnMenu(8, 15, 20, 16, 1);
  }
  else if ( LMRXsr[0] == LMRXsr[3 + indexoffset] )
  {
    SelectInGroupOnMenu(8, 15, 20, 17, 1);
  }
  else if ( LMRXsr[0] == LMRXsr[4 + indexoffset] )
  {
    SelectInGroupOnMenu(8, 15, 20, 18, 1);
  }
  else if ( LMRXsr[0] == LMRXsr[5 + indexoffset] )
  {
    SelectInGroupOnMenu(8, 15, 20, 19, 1);
  }
  else if ( LMRXsr[0] == LMRXsr[6 + indexoffset] )
  {
    SelectInGroupOnMenu(8, 15, 20, 20, 1);
  }

  if (strcmp(LMRXmode, "sat") == 0)
  {
    strcpy(LMBStext, "QO-100 (");
    strcat(LMBStext, LMRXinput);
    strcat(LMBStext, ")^ ");
    AmendButtonStatus(ButtonNumber(8, 21), 0, LMBStext, &Blue);
  }
  else
  {
    strcpy(LMBStext, " ^Terrestrial (");
    strcat(LMBStext, LMRXinput);
    strcat(LMBStext, ")");
    AmendButtonStatus(ButtonNumber(8, 21), 0, LMBStext, &Blue);
  }
}

void Define_Menu9()
{
  int button;
  char BandLabel[31];

  // Bottom Row, Menu 9

  button = CreateButton(9, 0);                  // 24G T
  strcpy(BandLabel, "Transvtr^");
  strcat(BandLabel, TabBandLabel[8]);
  AddButtonStatus(button, BandLabel, &Blue);
  AddButtonStatus(button, BandLabel, &Green);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(9, 1);                  // 47G T
  strcpy(BandLabel, "Transvtr^");
  strcat(BandLabel, TabBandLabel[11]);
  AddButtonStatus(button, BandLabel, &Blue);
  AddButtonStatus(button, BandLabel, &Green);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(9, 2);                  // 76G T
  strcpy(BandLabel, "Transvtr^");
  strcat(BandLabel, TabBandLabel[12]);
  AddButtonStatus(button, BandLabel, &Blue);
  AddButtonStatus(button, BandLabel, &Green);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(9, 3);                  // Spare T
  strcpy(BandLabel, "Transvtr^");
  strcat(BandLabel, TabBandLabel[13]);
  AddButtonStatus(button, BandLabel, &Blue);
  AddButtonStatus(button, BandLabel, &Green);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(9, 4);
  AddButtonStatus(button, "Exit", &DBlue);
  AddButtonStatus(button, "Exit", &LBlue);

  // 2nd Row, Menu 9

  button = CreateButton(9, 5);                  // 50 T
  strcpy(BandLabel, "Transvtr^");
  strcat(BandLabel, TabBandLabel[9]);
  AddButtonStatus(button, TabBandLabel[0], &Blue);
  AddButtonStatus(button, TabBandLabel[0], &Green);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(9, 6);                  // 13cm T
  strcpy(BandLabel, "Transvtr^");
  strcat(BandLabel, TabBandLabel[10]);
  AddButtonStatus(button, TabBandLabel[1], &Blue);
  AddButtonStatus(button, TabBandLabel[1], &Green);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(9, 7);                  // 9 cm T
  strcpy(BandLabel, "Transvtr^");
  strcat(BandLabel, TabBandLabel[5]);
  AddButtonStatus(button, TabBandLabel[2], &Blue);
  AddButtonStatus(button, TabBandLabel[2], &Green);
  AddButtonStatus(button, "", &Black);

  button = CreateButton(9, 8);                  // 6 cm T
  strcpy(BandLabel, "Transvtr^");
  strcat(BandLabel, TabBandLabel[6]);
  AddButtonStatus(button, TabBandLabel[3], &Blue);
  AddButtonStatus(button, TabBandLabel[3], &Green);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(9, 9);                  // 3 cm T
  strcpy(BandLabel, "Transvtr^");
  strcat(BandLabel, TabBandLabel[7]);
  AddButtonStatus(button, TabBandLabel[4], &Blue);
  AddButtonStatus(button, TabBandLabel[4], &Green);
  AddButtonStatus(button, " ", &Black);

  // 3rd Row, Menu 9

  button = CreateButton(9, 10);                  // 50 MHz
  AddButtonStatus(button, TabBandLabel[14], &Blue);
  AddButtonStatus(button, TabBandLabel[14], &Green);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(9, 11);                  // 50 MHz
  AddButtonStatus(button, "Direct", &Blue);
  AddButtonStatus(button, "Direct", &Green);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(9, 14);                  // 9 cm
  AddButtonStatus(button, TabBandLabel[15], &Blue);
  AddButtonStatus(button, TabBandLabel[15], &Green);
  AddButtonStatus(button, " ", &Black);

  // 4th Row Menu 9

  button = CreateButton(9, 15);                  // 71 MHz
  AddButtonStatus(button, TabBandLabel[0], &Blue);
  AddButtonStatus(button, TabBandLabel[0], &Green);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(9, 16);                  // 146 MHz
  AddButtonStatus(button, TabBandLabel[1], &Blue);
  AddButtonStatus(button, TabBandLabel[1], &Green);
  AddButtonStatus(button, "", &Black);

  button = CreateButton(9, 17);                  // 70 cm
  AddButtonStatus(button, TabBandLabel[2], &Blue);
  AddButtonStatus(button, TabBandLabel[2], &Green);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(9, 18);                  // 23 cm
  AddButtonStatus(button, TabBandLabel[3], &Blue);
  AddButtonStatus(button, TabBandLabel[3], &Green);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(9, 19);                  // 13cm
  AddButtonStatus(button, TabBandLabel[4], &Blue);
  AddButtonStatus(button, TabBandLabel[4], &Green);
  AddButtonStatus(button, " ", &Black);
}


void Start_Highlights_Menu9()
{
  // Set Band Select Labels
  char BandLabel[31];
  int i;
  printf("Entering Start Highlights Menu 9\n");

  if (CallingMenu == 315)
  {
    strcpy(MenuTitle[9], "Band Details Setting Menu (9)");
    //SetButtonStatus(ButtonNumber(9, 11), 2);                      // Hide direct button
    AmendButtonStatus(ButtonNumber(9, 11), 2, " ", &Black);
    SetButtonStatus(ButtonNumber(9, 11), 2);
    // Put a loop in here to show hide or highlight
    for (i = 0; i < 4 ; i++)
    {
      SetButtonStatus(ButtonNumber(9, i), 0);
    }

    for (i = 5; i < 11 ; i++)
    {
      SetButtonStatus(ButtonNumber(9, i), 0);
    }

    for (i = 14; i < 20 ; i++)
    {
      SetButtonStatus(ButtonNumber(9, i), 0);
    }
  }
  else
  {
    strcpy(MenuTitle[9], "Band/Transverter Selection Menu (9)");

    for (i = 0; i < 12 ; i++)  // Set buttons to blue (including direct)
    {
      SetButtonStatus(ButtonNumber(9, i), 0);
    }
    for (i = 14; i < 20 ; i++)  // Set buttons to blue
    {
      SetButtonStatus(ButtonNumber(9, i), 0);
    }
    if ((TabBandLO[CurrentBand] < 0.5) && (TabBandLO[CurrentBand] > -0.5))  // If not a transverter
    {
      SetButtonStatus(ButtonNumber(9, 11), 1);  // Set Direct Button Green
    }

  }

  strcpy(BandLabel, "Transvtr^");                                  // 24G T
  if ((TabBandLO[8] < 0.5) && (TabBandLO[8] > -0.5))  // If not a transverter
  {
    strcpy(BandLabel, "");
    if (CallingMenu == 113)
    {
      SetButtonStatus(ButtonNumber(9, 0), 2);        // set button to black
    }
  }
  else  // is a transverter
  {
    if (CurrentBand == 8)                           // This band selected
    {
      SetButtonStatus(ButtonNumber(9, 0), 1);        // set button to green
    }
  }
  if ((TabBandLO[8] > 0.5) || (TabBandLO[8] < -0.5) || (CallingMenu == 315))
  {
    strcat(BandLabel, TabBandLabel[8]);
  }
  AmendButtonStatus(ButtonNumber(9, 0), 0, BandLabel, &Blue);
  AmendButtonStatus(ButtonNumber(9, 0), 1, BandLabel, &Green);
  AmendButtonStatus(ButtonNumber(9, 0), 2, BandLabel, &Black);

  strcpy(BandLabel, "Transvtr^");                                  // 47G T
  if ((TabBandLO[11] < 0.5) && (TabBandLO[11] > -0.5))
  {
    strcpy(BandLabel, "");
    if (CallingMenu == 113)
    {
      SetButtonStatus(ButtonNumber(9, 1), 2);
    }
  }
  else  // is a transverter
  {
    if (CurrentBand == 11)                           // This band selected
    {
      SetButtonStatus(ButtonNumber(9, 1), 1);        // set button to green
    }
  }
  if ((TabBandLO[11] > 0.5) || (TabBandLO[11] < -0.5) || (CallingMenu == 315))
  {
    strcat(BandLabel, TabBandLabel[11]);
  }
  AmendButtonStatus(ButtonNumber(9, 1), 0, BandLabel, &Blue);
  AmendButtonStatus(ButtonNumber(9, 1), 1, BandLabel, &Green);
  AmendButtonStatus(ButtonNumber(9, 1), 2, BandLabel, &Black);

  strcpy(BandLabel, "Transvtr^");                                  // 76G T
  if ((TabBandLO[12] < 0.5) && (TabBandLO[12] > -0.5))
  {
    strcpy(BandLabel, "");
    if (CallingMenu == 113)
    {
      SetButtonStatus(ButtonNumber(9, 2), 2);
    }
  }
  else  // is a transverter
  {
    if (CurrentBand == 12)                           // This band selected
    {
      SetButtonStatus(ButtonNumber(9, 2), 1);        // set button to green
    }
  }
  if ((TabBandLO[12] > 0.5) || (TabBandLO[12] < -0.5) || (CallingMenu == 315))
  {
    strcat(BandLabel, TabBandLabel[12]);
  }
  AmendButtonStatus(ButtonNumber(9, 2), 0, BandLabel, &Blue);
  AmendButtonStatus(ButtonNumber(9, 2), 1, BandLabel, &Green);
  AmendButtonStatus(ButtonNumber(9, 2), 2, BandLabel, &Black);

  strcpy(BandLabel, "Transvtr^");                                  // Spare T
  if ((TabBandLO[13] < 0.5) && (TabBandLO[13] > -0.5))
  {
    strcpy(BandLabel, "");
    if (CallingMenu == 113)
    {
      SetButtonStatus(ButtonNumber(9, 3), 2);
    }
  }
  else  // is a transverter
  {
    if (CurrentBand == 13)                           // This band selected
    {
      SetButtonStatus(ButtonNumber(9, 3), 1);        // set button to green
    }
  }
  if ((TabBandLO[13] > 0.5) || (TabBandLO[13] < -0.5) || (CallingMenu == 315))
  {
    strcat(BandLabel, TabBandLabel[13]);
  }
  AmendButtonStatus(ButtonNumber(9, 3), 0, BandLabel, &Blue);
  AmendButtonStatus(ButtonNumber(9, 3), 1, BandLabel, &Green);
  AmendButtonStatus(ButtonNumber(9, 3), 2, BandLabel, &Black);

  strcpy(BandLabel, "Transvtr^");                                  // 50 T
  if ((TabBandLO[9] < 0.5) && (TabBandLO[9] > -0.5))
  {
    strcpy(BandLabel, "");
    if (CallingMenu == 113)
    {
      SetButtonStatus(ButtonNumber(9, 5), 2);
    }
  }
  else  // is a transverter
  {
    if (CurrentBand == 9)                           // This band selected
    {
      SetButtonStatus(ButtonNumber(9, 5), 1);        // set button to green
    }
  }
  if ((TabBandLO[9] > 0.5) || (TabBandLO[9] < -0.5) || (CallingMenu == 315))
  {
    strcat(BandLabel, TabBandLabel[9]);
  }
  AmendButtonStatus(ButtonNumber(9, 5), 0, BandLabel, &Blue);
  AmendButtonStatus(ButtonNumber(9, 5), 1, BandLabel, &Green);
  AmendButtonStatus(ButtonNumber(9, 5), 2, BandLabel, &Black);

  strcpy(BandLabel, "Transvtr^");                                  // 13cm T
  if ((TabBandLO[10] < 0.5) && (TabBandLO[10] > -0.5))
  {
    strcpy(BandLabel, "");
    if (CallingMenu == 113)
    {
      SetButtonStatus(ButtonNumber(9, 6), 2);
    }
  }
  else  // is a transverter
  {
    if (CurrentBand == 10)                           // This band selected
    {
      SetButtonStatus(ButtonNumber(9, 6), 1);        // set button to green
    }
  }
  if ((TabBandLO[10] > 0.5) || (TabBandLO[10] < -0.5) || (CallingMenu == 315))
  {
    strcat(BandLabel, TabBandLabel[10]);
  }
  AmendButtonStatus(ButtonNumber(9, 6), 0, BandLabel, &Blue);
  AmendButtonStatus(ButtonNumber(9, 6), 1, BandLabel, &Green);
  AmendButtonStatus(ButtonNumber(9, 6), 2, BandLabel, &Black);

  strcpy(BandLabel, "Transvtr^");                                  // 9cm T
  if ((TabBandLO[5] < 0.5) && (TabBandLO[5] > -0.5))
  {
    strcpy(BandLabel, "");
    if (CallingMenu == 113)
    {
      SetButtonStatus(ButtonNumber(9, 7), 2);
    }
  }
  else  // is a transverter
  {
    if (CurrentBand == 5)                           // This band selected
    {
      SetButtonStatus(ButtonNumber(9, 7), 1);        // set button to green
    }
  }
  if ((TabBandLO[5] > 0.5) || (TabBandLO[5] < -0.5) || (CallingMenu == 315))
  {
    strcat(BandLabel, TabBandLabel[5]);
  }
  AmendButtonStatus(ButtonNumber(9, 7), 0, BandLabel, &Blue);
  AmendButtonStatus(ButtonNumber(9, 7), 1, BandLabel, &Green);
  AmendButtonStatus(ButtonNumber(9, 7), 2, BandLabel, &Black);

  strcpy(BandLabel, "Transvtr^");                                  // 6cm T
  if ((TabBandLO[6] < 0.5) && (TabBandLO[6] > -0.5))
  {
    strcpy(BandLabel, "");
    if (CallingMenu == 113)
    {
      SetButtonStatus(ButtonNumber(9, 8), 2);
    }
  }
  else  // is a transverter
  {
    if (CurrentBand == 6)                           // This band selected
    {
      SetButtonStatus(ButtonNumber(9, 8), 1);        // set button to green
    }
  }
  if ((TabBandLO[6] > 0.5) || (TabBandLO[6] < -0.5) || (CallingMenu == 315))
  {
    strcat(BandLabel, TabBandLabel[6]);
  }
  AmendButtonStatus(ButtonNumber(9, 8), 0, BandLabel, &Blue);
  AmendButtonStatus(ButtonNumber(9, 8), 1, BandLabel, &Green);
  AmendButtonStatus(ButtonNumber(9, 8), 2, BandLabel, &Black);
 
  strcpy(BandLabel, "Transvtr^");                                  // 3cm T
  if ((TabBandLO[7] < 0.5) && (TabBandLO[7] > -0.5))
  {
    strcpy(BandLabel, "");
    if (CallingMenu == 113)
    {
      SetButtonStatus(ButtonNumber(9, 9), 2);
    }
  }
  else  // is a transverter
  {
    if (CurrentBand == 7)                           // This band selected
    {
      SetButtonStatus(ButtonNumber(9, 9), 1);        // set button to green
    }
  }
  if ((TabBandLO[7] > 0.5) || (TabBandLO[7] < -0.5) || (CallingMenu == 315))
  {
    strcat(BandLabel, TabBandLabel[7]);
  }
  AmendButtonStatus(ButtonNumber(9, 9), 0, BandLabel, &Blue);
  AmendButtonStatus(ButtonNumber(9, 9), 1, BandLabel, &Green);
  AmendButtonStatus(ButtonNumber(9, 9), 2, BandLabel, &Black);

  strcpy(BandLabel, "Transvtr^");                                  // 50 MHz
  if ((TabBandLO[14] < 0.5) && (TabBandLO[14] > -0.5))
  {
    strcpy(BandLabel, "");
    if (CallingMenu == 113)
    {
      SetButtonStatus(ButtonNumber(9, 10), 2);
    }
  }
  else  // is a transverter
  {
    if (CurrentBand == 14)                           // This band selected
    {
      SetButtonStatus(ButtonNumber(9, 10), 1);        // set button to green
    }
  }
  if ((TabBandLO[14] > 0.5) || (TabBandLO[14] < -0.5) || (CallingMenu == 315))
  {
    strcat(BandLabel, TabBandLabel[14]);
  }
  AmendButtonStatus(ButtonNumber(9, 10), 0, BandLabel, &Blue);
  AmendButtonStatus(ButtonNumber(9, 10), 1, BandLabel, &Green);
  AmendButtonStatus(ButtonNumber(9, 10), 2, BandLabel, &Black);

  strcpy(BandLabel, "Transvtr^");                                  // 9 cm
  if ((TabBandLO[15] < 0.5) && (TabBandLO[15] > -0.5))
  {
    strcpy(BandLabel, "");
    if (CallingMenu == 113)
    {
      SetButtonStatus(ButtonNumber(9, 14), 2);
    }
  }
  else  // is a transverter
  {
    if (CurrentBand == 15)                           // This band selected
    {
      SetButtonStatus(ButtonNumber(9, 14), 1);        // set button to green
    }
  }
  if ((TabBandLO[15] > 0.5) || (TabBandLO[15] < -0.5) || (CallingMenu == 315))
  {
    strcat(BandLabel, TabBandLabel[15]);
  }
  AmendButtonStatus(ButtonNumber(9, 14), 0, BandLabel, &Blue);
  AmendButtonStatus(ButtonNumber(9, 14), 1, BandLabel, &Green);
  AmendButtonStatus(ButtonNumber(9, 14), 2, BandLabel, &Black);
 
  strcpy(BandLabel, "Transvtr^");                                  // 71 MHz
  if ((TabBandLO[0] < 0.5) && (TabBandLO[0] > -0.5))
  {
    strcpy(BandLabel, "");
    if (CallingMenu == 113)
    {
      SetButtonStatus(ButtonNumber(9, 15), 2);
    }
  }
  else  // is a transverter
  {
    if (CurrentBand == 0)                           // This band selected
    {
      SetButtonStatus(ButtonNumber(9, 15), 1);        // set button to green
    }
  }
  if ((TabBandLO[0] > 0.5) || (TabBandLO[0] < -0.5) || (CallingMenu == 315))
  {
    strcat(BandLabel, TabBandLabel[0]);
  }
  AmendButtonStatus(ButtonNumber(9, 15), 0, BandLabel, &Blue);
  AmendButtonStatus(ButtonNumber(9, 15), 1, BandLabel, &Green);
  AmendButtonStatus(ButtonNumber(9, 15), 2, BandLabel, &Black);
 
  strcpy(BandLabel, "Transvtr^");                                  // 146 MHz
  if ((TabBandLO[1] < 0.5) && (TabBandLO[1] > -0.5))
  {
    strcpy(BandLabel, " ");
  }
    if (CallingMenu == 113)
    {
      SetButtonStatus(ButtonNumber(9, 16), 2);
    }
  else  // is a transverter
  {
    if (CurrentBand == 1)                           // This band selected
    {
      SetButtonStatus(ButtonNumber(9, 16), 1);        // set button to green
    }
  }
  if ((TabBandLO[1] > 0.5) || (TabBandLO[1] < -0.5) || (CallingMenu == 315))
  {
    strcat(BandLabel, TabBandLabel[1]);
  }
  AmendButtonStatus(ButtonNumber(9, 16), 0, BandLabel, &Blue);
  AmendButtonStatus(ButtonNumber(9, 16), 1, BandLabel, &Green);
  AmendButtonStatus(ButtonNumber(9, 16), 2, BandLabel, &Black);
 
  strcpy(BandLabel, "Transvtr^");                                  // 70 cm
  if ((TabBandLO[2] < 0.5) && (TabBandLO[2] > -0.5))
  {
    strcpy(BandLabel, "");
    if (CallingMenu == 113)
    {
      SetButtonStatus(ButtonNumber(9, 17), 2);
    }
  }
  else  // is a transverter
  {
    if (CurrentBand == 2)                           // This band selected
    {
      SetButtonStatus(ButtonNumber(9, 17), 1);        // set button to green
    }
  }
  if ((TabBandLO[2] > 0.5) || (TabBandLO[2] < -0.5) || (CallingMenu == 315))
  {
    strcat(BandLabel, TabBandLabel[2]);
  }
  AmendButtonStatus(ButtonNumber(9, 17), 0, BandLabel, &Blue);
  AmendButtonStatus(ButtonNumber(9, 17), 1, BandLabel, &Green);
  AmendButtonStatus(ButtonNumber(9, 17), 2, BandLabel, &Black);
 
  strcpy(BandLabel, "Transvtr^");                                  // 23 cm
  if ((TabBandLO[3] < 0.5) && (TabBandLO[3] > -0.5))
  {
    strcpy(BandLabel, "");
    if (CallingMenu == 113)
    {
      SetButtonStatus(ButtonNumber(9, 18), 2);
    }
  }
  else  // is a transverter
  {
    if (CurrentBand == 3)                           // This band selected
    {
      SetButtonStatus(ButtonNumber(9, 18), 1);        // set button to green
    }
  }
  if ((TabBandLO[3] > 0.5) || (TabBandLO[3] < -0.5) || (CallingMenu == 315))
  {
    strcat(BandLabel, TabBandLabel[3]);
  }
  AmendButtonStatus(ButtonNumber(9, 18), 0, BandLabel, &Blue);
  AmendButtonStatus(ButtonNumber(9, 18), 1, BandLabel, &Green);
  AmendButtonStatus(ButtonNumber(9, 18), 2, BandLabel, &Black);

  strcpy(BandLabel, "Transvtr^");                                  // 13 cm
  if ((TabBandLO[4] < 0.5) && (TabBandLO[4] > -0.5))
  {
    strcpy(BandLabel, "");
    if (CallingMenu == 113)
    {
      SetButtonStatus(ButtonNumber(9, 19), 2);
    }
  }
  else  // is a transverter
  {
    if (CurrentBand == 4)                           // This band selected
    {
      SetButtonStatus(ButtonNumber(9, 19), 1);        // set button to green
    }
  }
  if (! ((TabBandLO[4] < 0.5) && (TabBandLO[4] > -0.5) && (CallingMenu == 113)))
  {
    strcat(BandLabel, TabBandLabel[4]);
  }
  AmendButtonStatus(ButtonNumber(9, 19), 0, BandLabel, &Blue);
  AmendButtonStatus(ButtonNumber(9, 19), 1, BandLabel, &Green);
  AmendButtonStatus(ButtonNumber(9, 19), 2, BandLabel, &Black);
}


void Define_Menu10()
{
  int button;

  float TvtrFreq;
  char Freqtext[31];
  char Value[31];

  strcpy(MenuTitle[10], "Transmit Frequency Selection Menu (10)"); 

  // Bottom Row, Menu 10

  if ((TabBandLO[5] < 0.1) && (TabBandLO[5] > -0.1))
  {
    strcpy(Freqtext, FreqLabel[5]);
  }
  else
  {
    strcpy(Freqtext, "F: ");
    strcat(Freqtext, TabFreq[5]);
    strcat(Freqtext, "^T:");
    TvtrFreq = atof(TabFreq[5]) + TabBandLO[CurrentBand];
    if (TvtrFreq < 0)
    {
      TvtrFreq = TvtrFreq * -1;
    }
    snprintf(Value, 10, "%.2f", TvtrFreq);
    strcat(Freqtext, Value);
  }

  button = CreateButton(10, 0);
  AddButtonStatus(button, FreqLabel[5], &Blue);
  AddButtonStatus(button, FreqLabel[5], &Green);

  button = CreateButton(10, 1);
  AddButtonStatus(button, FreqLabel[6], &Blue);
  AddButtonStatus(button, FreqLabel[6], &Green);

  button = CreateButton(10, 2);
  AddButtonStatus(button, FreqLabel[7], &Blue);
  AddButtonStatus(button, FreqLabel[7], &Green);

  button = CreateButton(10, 3);
  AddButtonStatus(button, FreqLabel[8], &Blue);
  AddButtonStatus(button, FreqLabel[8], &Green);

  button = CreateButton(10, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  // 2nd Row, Menu 16

  button = CreateButton(10, 5);
  AddButtonStatus(button, FreqLabel[0], &Blue);
  AddButtonStatus(button, FreqLabel[0], &Green);

  button = CreateButton(10, 6);
  AddButtonStatus(button, FreqLabel[1], &Blue);
  AddButtonStatus(button, FreqLabel[1], &Green);

  button = CreateButton(10, 7);
  AddButtonStatus(button, FreqLabel[2], &Blue);
  AddButtonStatus(button, FreqLabel[2], &Green);

  button = CreateButton(10, 8);
  AddButtonStatus(button, FreqLabel[3], &Blue);
  AddButtonStatus(button, FreqLabel[3], &Green);

  button = CreateButton(10, 9);
  AddButtonStatus(button, FreqLabel[4], &Blue);
  AddButtonStatus(button, FreqLabel[4], &Green);

  button = CreateButton(10, 10);
  AddButtonStatus(button, QOFreqButts[0], &Blue);
  AddButtonStatus(button, QOFreqButts[0], &Green);

  button = CreateButton(10, 11);
  AddButtonStatus(button, QOFreqButts[1], &Blue);
  AddButtonStatus(button, QOFreqButts[1], &Green);

  button = CreateButton(10, 12);
  AddButtonStatus(button, QOFreqButts[2], &Blue);
  AddButtonStatus(button, QOFreqButts[2], &Green);

  button = CreateButton(10, 13);
  AddButtonStatus(button, QOFreqButts[3], &Blue);
  AddButtonStatus(button, QOFreqButts[3], &Green);

  button = CreateButton(10, 14);
  AddButtonStatus(button, QOFreqButts[4], &Blue);
  AddButtonStatus(button, QOFreqButts[4], &Green);

  button = CreateButton(10, 15);
  AddButtonStatus(button, QOFreqButts[5], &Blue);
  AddButtonStatus(button, QOFreqButts[5], &Green);

  button = CreateButton(10, 16);
  AddButtonStatus(button, QOFreqButts[6], &Blue);
  AddButtonStatus(button, QOFreqButts[6], &Green);

  button = CreateButton(10, 17);
  AddButtonStatus(button, QOFreqButts[7], &Blue);
  AddButtonStatus(button, QOFreqButts[7], &Green);

  button = CreateButton(10, 18);
  AddButtonStatus(button, QOFreqButts[8], &Blue);
  AddButtonStatus(button, QOFreqButts[8], &Green);

  button = CreateButton(10, 19);
  AddButtonStatus(button, QOFreqButts[9], &Blue);
  AddButtonStatus(button, QOFreqButts[9], &Green);
}

void Start_Highlights_Menu10()
{
  // Frequency
  char Param[255];
  char Value[255];
  int index;
  int NoButton;

  // Update info in memory
  ReadPresets();

  // Look up current transmit frequency for highlighting
  strcpy(Param,"freqoutput");
  GetConfigParam(PATH_PCONFIG, Param, Value);

  for(index = 0; index <= 8 ; index = index + 1)  // 9 would be cancel button
  {
    // Define the button text
    MakeFreqText(index);
    NoButton = index + 5; // Valid for bottom row
    if (index > 4)          // Overwrite for top row
    {
      NoButton = index - 5;
    }

    AmendButtonStatus(ButtonNumber(10, NoButton), 0, FreqBtext, &Blue);
    AmendButtonStatus(ButtonNumber(10, NoButton), 1, FreqBtext, &Green);

    //Highlight the Current Button
    if(strcmp(Value, TabFreq[index]) == 0)
    {
      SelectInGroupOnMenu(10, 5, 9, NoButton, 1);
      SelectInGroupOnMenu(10, 0, 3, NoButton, 1);
    }
  }

  for(index = 10; index <= 19 ; index = index + 1)
  {
    //Highlight the Current Button
    if (strcmp(Value, QOFreq[index - 10]) == 0)
    {
      SelectInGroupOnMenu(10, 0, 3, index, 1);
      SelectInGroupOnMenu(10, 5, 19, index, 1);
    }
  }
}

void Define_Menu11()
{
  int button;

  strcpy(MenuTitle[11], "Modulation Selection Menu (11)"); 

  // Bottom Row, Menu 11

  button = CreateButton(11, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  button = CreateButton(11, 0);
  AddButtonStatus(button, "DVB-S2^QPSK", &Blue);
  AddButtonStatus(button, "DVB-S2^QPSK", &Green);
  AddButtonStatus(button, "DVB-S2^QPSK", &Grey);

  button = CreateButton(11, 1);
  AddButtonStatus(button, "DVB-S2^8 PSK", &Blue);
  AddButtonStatus(button, "DVB-S2^8 PSK", &Green);
  AddButtonStatus(button, "DVB-S2^8 PSK", &Grey);

  button = CreateButton(11, 2);
  AddButtonStatus(button, "DVB-S2^16 APSK", &Blue);
  AddButtonStatus(button, "DVB-S2^16 APSK", &Green);
  AddButtonStatus(button, "DVB-S2^16 APSK", &Grey);

  button = CreateButton(11, 3);
  AddButtonStatus(button, "DVB-S2^32 APSK", &Blue);
  AddButtonStatus(button, "DVB-S2^32 APSK", &Green);
  AddButtonStatus(button, "DVB-S2^32 APSK", &Grey);

  // 2nd Row, Menu 11

  button = CreateButton(11, 5);
  AddButtonStatus(button, "DVB-S^QPSK", &Blue);
  AddButtonStatus(button, "DVB-S^QPSK", &Green);
  AddButtonStatus(button, "DVB-S^QPSK", &Grey);

  button = CreateButton(11, 6);
  AddButtonStatus(button, "Carrier", &Blue);
  AddButtonStatus(button, "Carrier", &Green);
  AddButtonStatus(button, "Carrier", &Grey);

  button = CreateButton(11, 7);
  AddButtonStatus(button, "DVB-T", &Blue);
  AddButtonStatus(button, "DVB-T", &Green);
  AddButtonStatus(button, "DVB-T", &Grey);

  button = CreateButton(11, 8);
  AddButtonStatus(button, "Pilots^Off", &LBlue);
  AddButtonStatus(button, "Pilots^Off", &Green);
  AddButtonStatus(button, "Pilots^Off", &Grey);

  button = CreateButton(11, 9);
  AddButtonStatus(button, "Frames^Long", &LBlue);
  AddButtonStatus(button, "Frames^Long", &Green);
  AddButtonStatus(button, "Frames^Long", &Grey);
}

void Start_Highlights_Menu11()
{
  char vcoding[256];
  char vsource[256];

  ReadModeInput(vcoding, vsource);

  GreyOutReset11();
  if(strcmp(CurrentTXMode, TabTXMode[0])==0)
  {
    SelectInGroupOnMenu(11, 5, 7, 5, 1);
    SelectInGroupOnMenu(11, 0, 3, 5, 1);
  }
  if(strcmp(CurrentTXMode, TabTXMode[1])==0)
  {
    SelectInGroupOnMenu(11, 5, 7, 6, 1);
    SelectInGroupOnMenu(11, 0, 3, 6, 1);
  }
  if(strcmp(CurrentTXMode, TabTXMode[2])==0)
  {
    SelectInGroupOnMenu(11, 5, 7, 0, 1);
    SelectInGroupOnMenu(11, 0, 3, 0, 1);
  }
  if(strcmp(CurrentTXMode, TabTXMode[3])==0)
  {
    SelectInGroupOnMenu(11, 5, 7, 1, 1);
    SelectInGroupOnMenu(11, 0, 3, 1, 1);
  }
  if(strcmp(CurrentTXMode, TabTXMode[4])==0)
  {
    SelectInGroupOnMenu(11, 5, 7, 2, 1);
    SelectInGroupOnMenu(11, 0, 3, 2, 1);
  }
  if(strcmp(CurrentTXMode, TabTXMode[5])==0)
  {
    SelectInGroupOnMenu(11, 5, 7, 3, 1);
    SelectInGroupOnMenu(11, 0, 3, 3, 1);
  }
  if(strcmp(CurrentTXMode, TabTXMode[6])==0)
  {
    SelectInGroupOnMenu(11, 5, 7, 7, 1);
    SelectInGroupOnMenu(11, 0, 3, 7, 1);
  }
  if(strcmp(CurrentPilots, "on") == 0)
  {
    AmendButtonStatus(ButtonNumber(11, 8), 0, "Pilots^On", &LBlue);
    AmendButtonStatus(ButtonNumber(11, 8), 2, "Pilots^On", &Grey);
  }
  else
  {
    AmendButtonStatus(ButtonNumber(11, 8), 0, "Pilots^Off", &LBlue);
    AmendButtonStatus(ButtonNumber(11, 8), 2, "Pilots^Off", &Grey);
  }
  if(strcmp(CurrentFrames, "short") == 0)
  {
    AmendButtonStatus(ButtonNumber(11, 9), 0, "Frames^Short", &LBlue);
    AmendButtonStatus(ButtonNumber(11, 9), 2, "Frames^Short", &Grey);
  }
  else
  {
    AmendButtonStatus(ButtonNumber(11, 9), 0, "Frames^Long", &LBlue);
    AmendButtonStatus(ButtonNumber(11, 9), 2, "Frames^Long", &Grey);
  }
  GreyOut11();
}

void Define_Menu12()
{
  int button;

  strcpy(MenuTitle[12], "Encoding Selection Menu (12)"); 

  // Bottom Row, Menu 12

  button = CreateButton(12, 1);
  AddButtonStatus(button, "Raw IPTS in^H264", &Blue);
  AddButtonStatus(button, "Raw IPTS in^H264", &Green);
  AddButtonStatus(button, "Raw IPTS in^H264", &Grey);

  button = CreateButton(12, 2);
  AddButtonStatus(button, "Raw IPTS in^H265", &Blue);
  AddButtonStatus(button, "Raw IPTS in^H265", &Green);
  AddButtonStatus(button, "Raw IPTS in^H265", &Grey);

  button = CreateButton(12, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  // 2nd Row, Menu 12

  button = CreateButton(12, 5);
  AddButtonStatus(button, TabEncoding[0], &Blue);
  AddButtonStatus(button, TabEncoding[0], &Green);
  AddButtonStatus(button, TabEncoding[0], &Grey);

  button = CreateButton(12, 6);
  AddButtonStatus(button, TabEncoding[1], &Blue);
  AddButtonStatus(button, TabEncoding[1], &Green);

  button = CreateButton(12, 7);
  AddButtonStatus(button, TabEncoding[2], &Blue);
  AddButtonStatus(button, TabEncoding[2], &Green);
  AddButtonStatus(button, TabEncoding[2], &Grey);

  button = CreateButton(12, 8);
  AddButtonStatus(button, TabEncoding[3], &Blue);
  AddButtonStatus(button, TabEncoding[3], &Green);

  button = CreateButton(12, 9);
  AddButtonStatus(button, TabEncoding[4], &Blue);
  AddButtonStatus(button, TabEncoding[4], &Green);
}

void Start_Highlights_Menu12()
{
  char vcoding[256];
  char vsource[256];
  ReadModeInput(vcoding, vsource);

  if(strcmp(CurrentEncoding, TabEncoding[0]) == 0)
  {
    SelectInGroupOnMenu(12, 5, 9, 5, 1);
    SelectInGroupOnMenu(12, 1, 2, 5, 1);
  }
  if(strcmp(CurrentEncoding, TabEncoding[1]) == 0)
  {
    SelectInGroupOnMenu(12, 5, 9, 6, 1);
    SelectInGroupOnMenu(12, 1, 2, 6, 1);
  }
  if(strcmp(CurrentEncoding, TabEncoding[2]) == 0)
  {
    SelectInGroupOnMenu(12, 5, 9, 7, 1);
    SelectInGroupOnMenu(12, 1, 2, 7, 1);
  }
  if(strcmp(CurrentEncoding, TabEncoding[3]) == 0)
  {
    SelectInGroupOnMenu(12, 5, 9, 8, 1);
    SelectInGroupOnMenu(12, 1, 2, 8, 1);
  }
  if(strcmp(CurrentEncoding, TabEncoding[4]) == 0)
  {
    SelectInGroupOnMenu(12, 5, 9, 9, 1);
    SelectInGroupOnMenu(12, 1, 2, 9, 1);
  }
  if(strcmp(CurrentEncoding, TabEncoding[5]) == 0)
  {
    SelectInGroupOnMenu(12, 5, 9, 1, 1);
    SelectInGroupOnMenu(12, 1, 2, 1, 1);
  }
  if(strcmp(CurrentEncoding, TabEncoding[6]) == 0)
  {
    SelectInGroupOnMenu(12, 5, 9, 2, 1);
    SelectInGroupOnMenu(12, 1, 2, 2, 1);
  }
  GreyOut12();  // Grey out H265 if not available
}

void Define_Menu13()
{
  int button;

  strcpy(MenuTitle[13], "Manage Contest Codes (13)"); 

  // Bottom Row, Menu 13

  button = CreateButton(13, 0);
  AddButtonStatus(button, "Edit^Site", &Blue);
  AddButtonStatus(button, "Edit^Site", &Red);

  button = CreateButton(13, 1);
  AddButtonStatus(button, "Load^Site", &Blue);
  AddButtonStatus(button, "Load^Site", &Red);

  button = CreateButton(13, 2);
  AddButtonStatus(button, "Save^In-use Site", &Blue);
  AddButtonStatus(button, "Save^In-use Site", &Red);
  AddButtonStatus(button, "Show^In-use Site", &Blue);

  button = CreateButton(13, 3);
  AddButtonStatus(button, "Show^Site Codes", &Blue);
  AddButtonStatus(button, "Show^Site Codes", &Red);

  button = CreateButton(13, 4);
  AddButtonStatus(button, "Exit", &DBlue);
  AddButtonStatus(button, "Exit", &LBlue);


  // 2nd Row, Menu 13

  button = CreateButton(13, 5);
  AddButtonStatus(button, "Site 1", &Blue);

  button = CreateButton(13, 6);
  AddButtonStatus(button, "Site 2", &Blue);

  button = CreateButton(13, 7);
  AddButtonStatus(button, "Site 3", &Blue);

  button = CreateButton(13, 8);
  AddButtonStatus(button, "Site 4", &Blue);

  button = CreateButton(13, 9);
  AddButtonStatus(button, "Site 5", &Blue);

}

void Start_Highlights_Menu13()
{
  char buttontext[63];

  if (AwaitingContestNumberEditSeln)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 0), 1);  // Red
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0);  // Blue
  }

  if (AwaitingContestNumberLoadSeln)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 1), 1);  // Red
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 1), 0);  // Blue
  }

  if (AwaitingContestNumberViewSeln)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 1);  // Red
    SetButtonStatus(ButtonNumber(CurrentMenu, 2), 2);  // Change Caption on button 2
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0);  // Blue
    if (AwaitingContestNumberSaveSeln)
    {
      SetButtonStatus(ButtonNumber(CurrentMenu, 2), 1);  // Red
    }
    else
    {
      SetButtonStatus(ButtonNumber(CurrentMenu, 2), 0);  // Blue
    }
  }

  strcpy(buttontext, "Site 1^");
  strcat(buttontext, Site1Locator10);
  AmendButtonStatus(ButtonNumber(13, 5), 0, buttontext, &Blue);

  strcpy(buttontext, "Site 2^");
  strcat(buttontext, Site2Locator10);
  AmendButtonStatus(ButtonNumber(13, 6), 0, buttontext, &Blue);

  strcpy(buttontext, "Site 3^");
  strcat(buttontext, Site3Locator10);
  AmendButtonStatus(ButtonNumber(13, 7), 0, buttontext, &Blue);

  strcpy(buttontext, "Site 4^");
  strcat(buttontext, Site4Locator10);
  AmendButtonStatus(ButtonNumber(13, 8), 0, buttontext, &Blue);

  strcpy(buttontext, "Site 5^");
  strcat(buttontext, Site5Locator10);
  AmendButtonStatus(ButtonNumber(13, 9), 0, buttontext, &Blue);
}

void Define_Menu14()
{
  int button;

  strcpy(MenuTitle[14], "Video Format Menu (14)"); 

  // Bottom Row, Menu 14

  button = CreateButton(14, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  // 2nd Row, Menu 14

  button = CreateButton(14, 5);
  AddButtonStatus(button, TabFormat[0], &Blue);
  AddButtonStatus(button, TabFormat[0], &Green);

  button = CreateButton(14, 6);
  AddButtonStatus(button, TabFormat[1], &Blue);
  AddButtonStatus(button, TabFormat[1], &Green);

  button = CreateButton(14, 7);
  AddButtonStatus(button, TabFormat[2], &Blue);
  AddButtonStatus(button, TabFormat[2], &Green);

  button = CreateButton(14, 8);
  AddButtonStatus(button, TabFormat[3], &Blue);
  AddButtonStatus(button, TabFormat[3], &Green);
}

void Start_Highlights_Menu14()
{
  char vcoding[256];
  char vsource[256];
  ReadModeInput(vcoding, vsource);

  if(strcmp(CurrentFormat, TabFormat[0]) == 0)
  {
    SelectInGroupOnMenu(14, 5, 8, 5, 1);
  }
  if(strcmp(CurrentFormat, TabFormat[1]) == 0)
  {
    SelectInGroupOnMenu(14, 5, 8, 6, 1);
  }
  if(strcmp(CurrentFormat, TabFormat[2]) == 0)
  {
    SelectInGroupOnMenu(14, 5, 8, 7, 1);
  }
  if(strcmp(CurrentFormat, TabFormat[3]) == 0)
  {
    SelectInGroupOnMenu(14, 5, 8, 8, 1);
  }
}

void Define_Menu15()
{
  int button;

  strcpy(MenuTitle[15], "Pluto Configuration Menu (15)"); 

  // Bottom Row, Menu 15

  button = CreateButton(15, 0);
  AddButtonStatus(button, "Set Pluto IP^for Portsdown", &Blue);

  button = CreateButton(15, 1);
  AddButtonStatus(button, "Reboot^Pluto", &Blue);

  button = CreateButton(15, 2);
  AddButtonStatus(button, "Set Pluto^Ref Freq", &Blue);

  button = CreateButton(15, 3);
  AddButtonStatus(button, "Check Pluto^AD9364", &Blue);

  button = CreateButton(15, 4);
  AddButtonStatus(button, "Exit", &DBlue);
  AddButtonStatus(button, "Exit", &LBlue);

 // Second Row, Menu 15

  button = CreateButton(15, 5);
  AddButtonStatus(button, "Check Pluto^CPUs", &Blue);

  button = CreateButton(15, 6);
  AddButtonStatus(button, " ", &Grey);
  AddButtonStatus(button, "Enable 2nd^Pluto CPU", &Red);

  button = CreateButton(15, 7);
  AddButtonStatus(button, "Check Pluto^Firmware", &Blue);

  button = CreateButton(15, 8);
  AddButtonStatus(button, " ", &Grey);
  AddButtonStatus(button, "Update Pluto^to AD9364", &Red);

  button = CreateButton(15, 9);
  AddButtonStatus(button, "Set Pluto IP^for Langstone", &Blue);

}

void Start_Highlights_Menu15()
{
}

void Define_Menu16()
{
  int button;

  strcpy(MenuTitle[16], "DVB-T Parameters Menu (16)"); 

  // Bottom Row, Menu 16

  button = CreateButton(16, 0);
  AddButtonStatus(button, "Guard^1/4", &Blue);
  AddButtonStatus(button, "Guard^1/4", &Green);

  button = CreateButton(16, 1);
  AddButtonStatus(button, "Guard^1/8", &Blue);
  AddButtonStatus(button, "Guard^1/8", &Green);

  button = CreateButton(16, 2);
  AddButtonStatus(button, "Guard^1/16", &Blue);
  AddButtonStatus(button, "Guard^1/16", &Green);

  button = CreateButton(16, 3);
  AddButtonStatus(button, "Guard^1/32", &Blue);
  AddButtonStatus(button, "Guard^1/32", &Green);

  button = CreateButton(16, 4);
  AddButtonStatus(button, "Continue", &DBlue);
  AddButtonStatus(button, "Continue", &LBlue);

  // 2nd Row, Menu 16

  button = CreateButton(16, 5);
  AddButtonStatus(button, "QPSK", &Blue);
  AddButtonStatus(button, "QPSK", &Green);

  button = CreateButton(16, 6);
  AddButtonStatus(button, "16-QAM", &Blue);
  AddButtonStatus(button, "16-QAM", &Green);

  button = CreateButton(16, 7);
  AddButtonStatus(button, "64-QAM", &Blue);
  AddButtonStatus(button, "64-QAM", &Green);

}

void MakeFreqText(int index)
{
  char Param[255];
  char Value[255];
  float TvtrFreq;

  strcpy(Param,"freqoutput");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  if ((TabBandLO[CurrentBand] < 0.1) && (TabBandLO[CurrentBand] > -0.1))
  {
    if (index == 8)
    {
      strcpy(FreqBtext, "Keyboard^");
      strcat(FreqBtext, FreqLabel[index]);
    }
    else
    {
      strcpy(FreqBtext, FreqLabel[index]);
    }
  }
  else
  {
    if (index == 8)
    {
      //strcpy(FreqBtext, "Keyboard^TX:");
      //TvtrFreq = atof(TabFreq[index]) + TabBandLO[CurrentBand];
      //if (TvtrFreq < 0)
      //{
      //  TvtrFreq = TvtrFreq * -1;
      //}
      strcpy(FreqBtext, "Keyboard^IF:");
      TvtrFreq = atof(TabFreq[index]); // 
      snprintf(Value, 10, "%.2f", TvtrFreq);
      strcat(FreqBtext, Value);
    }
    else
    {
      strcpy(FreqBtext, "IF: ");
      strcat(FreqBtext, TabFreq[index]);
      strcat(FreqBtext, "^TX:");
      TvtrFreq = atof(TabFreq[index]) + TabBandLO[CurrentBand];
      if (TvtrFreq < 0)
      {
        TvtrFreq = TvtrFreq * -1;
      }
      snprintf(Value, 10, "%.2f", TvtrFreq);
      strcat(FreqBtext, Value);
    }
  }
}

void Start_Highlights_Menu16()
{
  int NoButton;

  if (strcmp(Guard, "4") == 0)
  {
    NoButton = 0;
  }
  else if (strcmp(Guard, "8") == 0)
  {
    NoButton = 1;
  }
  else if (strcmp(Guard, "16") == 0)
  {
    NoButton = 2;
  }
  else if (strcmp(Guard, "32") == 0)
  {
    NoButton = 3;
  }
  SelectInGroupOnMenu(16, 0, 3, NoButton, 1);

  if (strcmp(DVBTQAM, "qpsk") == 0)
  {
    NoButton = 5;
  }
  else if (strcmp(DVBTQAM, "16qam") == 0)
  {
    NoButton = 6;
  }
  else if (strcmp(DVBTQAM, "64qam") == 0)
  {
    NoButton = 7;
  }
  SelectInGroupOnMenu(16, 5, 7, NoButton, 1);
}

void Define_Menu17()
{
  int button;

  // Bottom Row, Menu 17

  button = CreateButton(17, 0);
  AddButtonStatus(button, SRLabel[5], &Blue);
  AddButtonStatus(button, SRLabel[5], &Green);

  button = CreateButton(17, 1);
  AddButtonStatus(button, SRLabel[6], &Blue);
  AddButtonStatus(button, SRLabel[6], &Green);

  button = CreateButton(17, 2);
  AddButtonStatus(button, SRLabel[7], &Blue);
  AddButtonStatus(button, SRLabel[7], &Green);

  button = CreateButton(17, 3);
  AddButtonStatus(button, SRLabel[8], &Blue);
  AddButtonStatus(button, SRLabel[8], &Green);

  button = CreateButton(17, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  // 2nd Row, Menu 17

  button = CreateButton(17, 5);
  AddButtonStatus(button, SRLabel[0], &Blue);
  AddButtonStatus(button, SRLabel[0], &Green);

  button = CreateButton(17, 6);
  AddButtonStatus(button, SRLabel[1], &Blue);
  AddButtonStatus(button, SRLabel[1], &Green);

  button = CreateButton(17, 7);
  AddButtonStatus(button, SRLabel[2], &Blue);
  AddButtonStatus(button, SRLabel[2], &Green);

  button = CreateButton(17, 8);
  AddButtonStatus(button, SRLabel[3], &Blue);
  AddButtonStatus(button, SRLabel[3], &Green);

  button = CreateButton(17, 9);
  AddButtonStatus(button, SRLabel[4], &Blue);
  AddButtonStatus(button, SRLabel[4], &Green);
}

void Start_Highlights_Menu17()
{
  // Symbol Rate or Bandwidth (DVB-T)
  char Param[255];
  char Value[255];
  int SR;

  if (strcmp(CurrentTXMode, "DVB-T") != 0)  //not DVB-T
  {
    strcpy(MenuTitle[17], "Transmit Symbol Rate Selection Menu (17)");
  }
  else
  {
    strcpy(MenuTitle[17], "DVB-T Transmit Bandwidth Selection Menu (17)");
  }

  strcpy(Param,"symbolrate");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  SR=atoi(Value);
  printf("Value=%s %s\n",Value,"SR");

  if ( SR == TabSR[0] )
  {
    SelectInGroupOnMenu(17, 0, 3, 5, 1);
    SelectInGroupOnMenu(17, 5, 9, 5, 1);
  }
  else if ( SR == TabSR[1] )
  {
    SelectInGroupOnMenu(17, 0, 3, 6, 1);
    SelectInGroupOnMenu(17, 5, 9, 6, 1);
  }
  else if ( SR == TabSR[2] )
  {
    SelectInGroupOnMenu(17, 0, 3, 7, 1);
    SelectInGroupOnMenu(17, 5, 9, 7, 1);
  }
  else if ( SR == TabSR[3] )
  {
    SelectInGroupOnMenu(17, 0, 3, 8, 1);
    SelectInGroupOnMenu(17, 5, 9, 8, 1);
  }
  else if ( SR == TabSR[4] )
  {
    SelectInGroupOnMenu(17, 0, 3, 9, 1);
    SelectInGroupOnMenu(17, 5, 9, 9, 1);
  }
  else if ( SR == TabSR[5] )
  {
    SelectInGroupOnMenu(17, 0, 3, 0, 1);
    SelectInGroupOnMenu(17, 5, 9, 0, 1);
  }
  else if ( SR == TabSR[6] )
  {
    SelectInGroupOnMenu(17, 0, 3, 1, 1);
    SelectInGroupOnMenu(17, 5, 9, 1, 1);
  }
  else if ( SR == TabSR[7] )
  {
    SelectInGroupOnMenu(17, 0, 3, 2, 1);
    SelectInGroupOnMenu(17, 5, 9, 2, 1);
  }
  else if ( SR == TabSR[8] )
  {
    SelectInGroupOnMenu(17, 0, 3, 3, 1);
    SelectInGroupOnMenu(17, 5, 9, 3, 1);
  }
}

void Define_Menu18()
{
  int button;

  // Bottom Row, Menu 18

  button = CreateButton(18, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  // 2nd Row, Menu 18

  button = CreateButton(18, 5);
  AddButtonStatus(button,"FEC 1/2",&Blue);
  AddButtonStatus(button,"FEC 1/2",&Green);

  button = CreateButton(18, 6);
  AddButtonStatus(button,"FEC 2/3",&Blue);
  AddButtonStatus(button,"FEC 2/3",&Green);

  button = CreateButton(18, 7);
  AddButtonStatus(button,"FEC 3/4",&Blue);
  AddButtonStatus(button,"FEC 3/4",&Green);

  button = CreateButton(18, 8);
  AddButtonStatus(button,"FEC 5/6",&Blue);
  AddButtonStatus(button,"FEC 5/6",&Green);

  button = CreateButton(18, 9);
  AddButtonStatus(button,"FEC 7/8",&Blue);
  AddButtonStatus(button,"FEC 7/8",&Green);
}

void Start_Highlights_Menu18()
{
  // FEC
  char Param[255];
  char Value[255];
  int fec;

  strcpy(MenuTitle[18], "Transmit FEC Selection Menu (18)"); 
  strcpy(Param,"fec");
  strcpy(Value,"");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  printf("Value=%s %s\n",Value,"Fec");
  fec=atoi(Value);

  switch(fec)
  {
    case 1:SelectInGroupOnMenu(18, 5, 9, 5, 1);
    break;
    case 2:SelectInGroupOnMenu(18, 5, 9, 6, 1);
    break;
    case 3:SelectInGroupOnMenu(18, 5, 9, 7, 1);
    break;
    case 5:SelectInGroupOnMenu(18, 5, 9, 8, 1);
    break;
    case 7:SelectInGroupOnMenu(18, 5, 9, 9, 1);
    break;
  }
}

void Define_Menu19()
{
  int button;

  strcpy(MenuTitle[19], "Transverter Selection Menu (19)"); 

  // Bottom Row, Menu 19

  button = CreateButton(19, 0);
  AddButtonStatus(button, TabBandLabel[5], &Blue);
  AddButtonStatus(button, TabBandLabel[5], &Green);

  button = CreateButton(19, 1);
  AddButtonStatus(button, TabBandLabel[6], &Blue);
  AddButtonStatus(button, TabBandLabel[6], &Green);

  button = CreateButton(19, 2);
  AddButtonStatus(button, TabBandLabel[7], &Blue);
  AddButtonStatus(button, TabBandLabel[7], &Green);

  button = CreateButton(19, 3);
  AddButtonStatus(button, TabBandLabel[8], &Blue);
  AddButtonStatus(button, TabBandLabel[8], &Green);

  button = CreateButton(19, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  // 2nd Row, Menu 19

  button = CreateButton(19, 5);
  AddButtonStatus(button,"Direct",&Blue);
  AddButtonStatus(button,"Direct",&Green);
}

void Start_Highlights_Menu19()
{
  // Band/Transverter Select
  char Param[255];
  char Value[255];
  int NoButton;
  char BandLabel[31];

  strcpy(Param,"band");
  strcpy(Value,"");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  printf("Value=%s %s\n",Value,"Band");

  //Set default of Direct and then test for transverters
  // and overwrite if required
  SelectInGroupOnMenu(19, 5, 5, 5, 1);
  SelectInGroupOnMenu(19, 0, 3, 5, 1);

  if (strcmp(Value, "t1") == 0)
  {
    SelectInGroupOnMenu(19, 5, 5, 0, 1);
    SelectInGroupOnMenu(19, 0, 3, 0, 1);
  }
  if (strcmp(Value, "t2") == 0)
  {
    SelectInGroupOnMenu(19, 5, 5, 1, 1);
    SelectInGroupOnMenu(19, 0, 3, 1, 1);
  }
  if (strcmp(Value, "t3") == 0)
  {
    SelectInGroupOnMenu(19, 5, 5, 2, 1);
    SelectInGroupOnMenu(19, 0, 3, 2, 1);
  }
  if (strcmp(Value, "t4") == 0)
  {
    SelectInGroupOnMenu(19, 5, 5, 3, 1);
    SelectInGroupOnMenu(19, 0, 3, 3, 1);
  }

  // Set transverter Labels

  for (NoButton = 0; NoButton < 4; NoButton = NoButton + 1)
  {
    strcpy(BandLabel, "Transvtr^");
    strcat(BandLabel, TabBandLabel[NoButton + 5]);
    AmendButtonStatus(ButtonNumber(19, NoButton), 0, BandLabel, &Blue);
    AmendButtonStatus(ButtonNumber(19, NoButton), 1, BandLabel, &Green);
  }
}

// Menu 20 Stream Viewer

void Define_Menu20()
{
  int button;
  int n;
  char TempLabel[31] = "Keyboard^";

  strcpy(MenuTitle[20], "Stream Viewer Selection Menu (20)"); 

  // Top Row, Menu 20
  button = CreateButton(20, 9);
  AddButtonStatus(button, "Amend^Preset", &Blue);
  AddButtonStatus(button, "Amend^Preset", &Red);

  // Bottom Row, Menu 20
  button = CreateButton(20, 4);
  AddButtonStatus(button, "Exit to^Main Menu", &DBlue);
  AddButtonStatus(button, "Exit to^Main Menu", &LBlue);

  for(n = 1; n < 9; n = n + 1)
  {
    if (n < 5)  // top row
    {
      button = CreateButton(20, n + 4);
      AddButtonStatus(button, StreamLabel[n], &Blue);
      AddButtonStatus(button, StreamLabel[n], &Green);
    }
    else       // Bottom Row
    {
      button = CreateButton(20, n - 5);
      if (n == 20)
      {
        strcat(TempLabel, StreamLabel[n]);
        AddButtonStatus(button, TempLabel, &Blue);
        AddButtonStatus(button, TempLabel, &Green);
      }
      else
      {
        AddButtonStatus(button, StreamLabel[n], &Blue);
        AddButtonStatus(button, StreamLabel[n], &Green);
      }
    }
  }
}

void Start_Highlights_Menu20()
{
  // Stream Display Menu

  int n;
  char TempLabel[31] = "Keyboard^";

  for(n = 1; n < 9; n = n + 1)
  {
    if (n < 5)  // top row
    {
      AmendButtonStatus(ButtonNumber(20, n + 4), 0, StreamLabel[n], &Blue);
      AmendButtonStatus(ButtonNumber(20, n + 4), 1, StreamLabel[n], &Green);
    }
    else       // Bottom Row
    {
      if (n == 8)
      {
        strcat(TempLabel, StreamLabel[n]);
        AmendButtonStatus(ButtonNumber(20, n - 5), 0, TempLabel, &Blue);
        AmendButtonStatus(ButtonNumber(20, n - 5), 1, TempLabel, &Green);
      }
      else
      {
        AmendButtonStatus(ButtonNumber(20, n - 5), 0, StreamLabel[n], &Blue);
        AmendButtonStatus(ButtonNumber(20, n - 5), 1, StreamLabel[n], &Green);
      }
    }
  }
}


void Define_Menu21()
{
  int button;

  strcpy(MenuTitle[21], "EasyCap Video Input Menu (21)"); 

  // Bottom Row, Menu 21

  button = CreateButton(21, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  // 2nd Row, Menu 21

  button = CreateButton(21, 5);
  AddButtonStatus(button,"Comp Vid",&Blue);
  AddButtonStatus(button,"Comp Vid",&Green);

  button = CreateButton(21, 6);
  AddButtonStatus(button,"S-Video",&Blue);
  AddButtonStatus(button,"S-Video",&Green);

  button = CreateButton(21, 8);
  AddButtonStatus(button," PAL ",&Blue);
  AddButtonStatus(button," PAL ",&Green);

  button = CreateButton(21, 9);
  AddButtonStatus(button," NTSC ",&Blue);
  AddButtonStatus(button," NTSC ",&Green);
}

void Start_Highlights_Menu21()
{
  // EasyCap
  ReadModeEasyCap();
  if (strcmp(ModeVidIP, "0") == 0)
  {
    SelectInGroupOnMenu(21, 5, 6, 5, 1);
  }
  else
  {
    SelectInGroupOnMenu(21, 5, 6, 6, 1);
  }
  if (strcmp(ModeSTD, "6") == 0)
  {
    SelectInGroupOnMenu(21, 8, 9, 8, 1);
  }
  else
  {
    SelectInGroupOnMenu(21, 8, 9, 9, 1);
  }
}

void Define_Menu22()
{
  int button;

  strcpy(MenuTitle[22], "Caption Selection Menu (22)"); 

  // Bottom Row, Menu 22

  button = CreateButton(22, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  // 2nd Row, Menu 22

  button = CreateButton(22, 5);
  AddButtonStatus(button,"Caption^Off",&Blue);
  AddButtonStatus(button,"Caption^Off",&Green);

  button = CreateButton(22, 6);
  AddButtonStatus(button,"Caption^On",&Blue);
  AddButtonStatus(button,"Caption^On",&Green);
}

void Start_Highlights_Menu22()
{
  // Caption
  ReadCaptionState();
  if (strcmp(CurrentCaptionState, "off") == 0)
  {
    SelectInGroupOnMenu(22, 5, 6, 5, 1);
  }
  else
  {
    SelectInGroupOnMenu(22, 5, 6, 6, 1);
  }
}

void Define_Menu23()
{
  int button;

  strcpy(MenuTitle[23], "Audio Input Selection Menu (23)"); 

  // Bottom Row, Menu 23

  button = CreateButton(23, 0);
  AddButtonStatus(button,"Webcam",&Blue);
  AddButtonStatus(button,"Webcam",&Green);

  button = CreateButton(23, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  // 2nd Row, Menu 23

  button = CreateButton(23, 5);
  AddButtonStatus(button,"Auto",&Blue);
  AddButtonStatus(button,"Auto",&Green);

  button = CreateButton(23, 6);
  AddButtonStatus(button,"USB Mic",&Blue);
  AddButtonStatus(button,"USB Mic",&Green);

  button = CreateButton(23, 7);
  AddButtonStatus(button,"EasyCap",&Blue);
  AddButtonStatus(button,"EasyCap",&Green);

  button = CreateButton(23, 8);
  AddButtonStatus(button,"Bleeps",&Blue);
  AddButtonStatus(button,"Bleeps",&Green);

  button = CreateButton(23, 9);
  AddButtonStatus(button,"Audio^Off",&Blue);
  AddButtonStatus(button,"Audio^Off",&Green);
}

void Start_Highlights_Menu23()
{
  // Audio
  ReadAudioState();
  if (strcmp(CurrentAudioState, "auto") == 0)
  {
    SelectInGroupOnMenu(23, 5, 9, 5, 1);
    SelectInGroupOnMenu(23, 0, 0, 5, 1);
  }
  else if (strcmp(CurrentAudioState, "mic") == 0)
  {
    SelectInGroupOnMenu(23, 5, 9, 6, 1);
    SelectInGroupOnMenu(23, 0, 0, 6, 1);
  }
  else if (strcmp(CurrentAudioState, "video") == 0)
  {
    SelectInGroupOnMenu(23, 5, 9, 7, 1);
    SelectInGroupOnMenu(23, 0, 0, 7, 1);
  }
  else if (strcmp(CurrentAudioState, "bleeps") == 0)
  {
    SelectInGroupOnMenu(23, 5, 9, 8, 1);
    SelectInGroupOnMenu(23, 0, 0, 8, 1);
  }
  else if (strcmp(CurrentAudioState, "webcam") == 0)
  {
    SelectInGroupOnMenu(23, 5, 9, 0, 1);
    SelectInGroupOnMenu(23, 0, 0, 0, 1);
  }
  else
  {
    SelectInGroupOnMenu(23, 5, 9, 9, 1);
    SelectInGroupOnMenu(23, 0, 0, 9, 1);
  }
}

void Define_Menu24()
{
  int button;

  strcpy(MenuTitle[24], "Attenuator Selection Menu (24)"); 

  // Bottom Row, Menu 24

  button = CreateButton(24, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  // 2nd Row, Menu 24

  button = CreateButton(24, 5);
  AddButtonStatus(button,"None^Fitted",&Blue);
  AddButtonStatus(button,"None^Fitted",&Green);

  button = CreateButton(24, 6);
  AddButtonStatus(button,"PE4312",&Blue);
  AddButtonStatus(button,"PE4312",&Green);

  button = CreateButton(24, 7);
  AddButtonStatus(button,"PE43713",&Blue);
  AddButtonStatus(button,"PE43713",&Green);

  button = CreateButton(24, 8);
  AddButtonStatus(button,"HMC1119",&Blue);
  AddButtonStatus(button,"HMC1119",&Green);
}

void Start_Highlights_Menu24()
{
  // Audio
  ReadAttenState();
  if (strcmp(CurrentAtten, "NONE") == 0)
  {
    SelectInGroupOnMenu(24, 5, 9, 5, 1);
  }
  else if (strcmp(CurrentAtten, "PE4312") == 0)
  {
    SelectInGroupOnMenu(24, 5, 9, 6, 1);
  }
  else if (strcmp(CurrentAtten, "PE43713") == 0)
  {
    SelectInGroupOnMenu(24, 5, 9, 7, 1);
  }
  else if (strcmp(CurrentAtten, "HMC1119") == 0)
  {
    SelectInGroupOnMenu(24, 5, 9, 8, 1);
  }
  else
  {
    SelectInGroupOnMenu(24, 5, 9, 5, 1);
  }
}

void Define_Menu25()
{
  int button;

  strcpy(MenuTitle[25], "DVB-S2 FEC Selection Menu (25)"); 

  // Bottom Row, Menu 25

  button = CreateButton(25, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  button = CreateButton(25, 0);
  AddButtonStatus(button,"FEC 3/4",&Blue);
  AddButtonStatus(button,"FEC 3/4",&Green);
  AddButtonStatus(button,"FEC 3/4",&Grey);

  button = CreateButton(25, 1);
  AddButtonStatus(button,"FEC 5/6",&Blue);
  AddButtonStatus(button,"FEC 5/6",&Green);
  AddButtonStatus(button,"FEC 5/6",&Grey);

  button = CreateButton(25, 2);
  AddButtonStatus(button,"FEC 8/9",&Blue);
  AddButtonStatus(button,"FEC 8/9",&Green);
  AddButtonStatus(button,"FEC 8/9",&Grey);

  button = CreateButton(25, 3);
  AddButtonStatus(button,"FEC 9/10",&Blue);
  AddButtonStatus(button,"FEC 9/10",&Green);
  AddButtonStatus(button,"FEC 9/10",&Grey);

  // 2nd Row, Menu 25

  button = CreateButton(25, 5);
  AddButtonStatus(button,"FEC 1/4",&Blue);
  AddButtonStatus(button,"FEC 1/4",&Green);
  AddButtonStatus(button,"FEC 1/4",&Grey);

  button = CreateButton(25, 6);
  AddButtonStatus(button,"FEC 1/3",&Blue);
  AddButtonStatus(button,"FEC 1/3",&Green);
  AddButtonStatus(button,"FEC 1/3",&Grey);

  button = CreateButton(25, 7);
  AddButtonStatus(button,"FEC 1/2",&Blue);
  AddButtonStatus(button,"FEC 1/2",&Green);
  AddButtonStatus(button,"FEC 1/2",&Grey);

  button = CreateButton(25, 8);
  AddButtonStatus(button,"FEC 3/5",&Blue);
  AddButtonStatus(button,"FEC 3/5",&Green);
  AddButtonStatus(button,"FEC 3/5",&Grey);

  button = CreateButton(25, 9);
  AddButtonStatus(button,"FEC 2/3",&Blue);
  AddButtonStatus(button,"FEC 2/3",&Green);
  AddButtonStatus(button,"FEC 2/3",&Grey);
}

void Start_Highlights_Menu25()
{
  // DVB-S2 FEC
  char Param[255];
  char Value[255];
  int fec;

  strcpy(Param,"fec");
  strcpy(Value,"");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  printf("Value=%s %s\n",Value,"Fec");
  fec=atoi(Value);
  GreyOutReset25();  // Un-grey all FECs
  switch(fec)
  {
    case 14:
      SelectInGroupOnMenu(25, 5, 9, 5, 1);
      SelectInGroupOnMenu(25, 0, 3, 5, 1);
      break;
    case 13:
      SelectInGroupOnMenu(25, 5, 9, 6, 1);
      SelectInGroupOnMenu(25, 0, 3, 6, 1);
      break;
    case 12:
      SelectInGroupOnMenu(25, 5, 9, 7, 1);
      SelectInGroupOnMenu(25, 0, 3, 7, 1);
      break;
    case 35:
      SelectInGroupOnMenu(25, 5, 9, 8, 1);
      SelectInGroupOnMenu(25, 0, 3, 8, 1);
      break;
    case 23:
      SelectInGroupOnMenu(25, 5, 9, 9, 1);
      SelectInGroupOnMenu(25, 0, 3, 9, 1);
      break;
    case 34:
      SelectInGroupOnMenu(25, 5, 9, 0, 1);
      SelectInGroupOnMenu(25, 0, 3, 0, 1);
      break;
    case 56:
      SelectInGroupOnMenu(25, 5, 9, 1, 1);
      SelectInGroupOnMenu(25, 0, 3, 1, 1);
      break;
    case 89:
      SelectInGroupOnMenu(25, 5, 9, 2, 1);
      SelectInGroupOnMenu(25, 0, 3, 2, 1);
      break;
    case 91:
      SelectInGroupOnMenu(25, 5, 9, 3, 1);
      SelectInGroupOnMenu(25, 0, 3, 3, 1);
      break;
  }
  GreyOut25();  // Grey out illegal FECs
}


void Define_Menu26()
{
  int button;
  char BandLabel[31];
  strcpy(MenuTitle[26], "Receiver LO Setting Menu (26)");

  // Bottom Row, Menu 26

  button = CreateButton(26, 0);
  strcpy(BandLabel, TabBandLabel[14]);
  AddButtonStatus(button, BandLabel, &Blue);
  AddButtonStatus(button, BandLabel, &Green);

  button = CreateButton(26, 1);
  strcpy(BandLabel, TabBandLabel[0]);
  AddButtonStatus(button, BandLabel, &Blue);
  AddButtonStatus(button, BandLabel, &Green);

  button = CreateButton(26, 2);
  strcpy(BandLabel, TabBandLabel[1]);
  AddButtonStatus(button, BandLabel, &Blue);
  AddButtonStatus(button, BandLabel, &Green);

  button = CreateButton(26, 3);
  strcpy(BandLabel, TabBandLabel[2]);
  AddButtonStatus(button, BandLabel, &Blue);
  AddButtonStatus(button, BandLabel, &Green);

  button = CreateButton(26, 4);
  AddButtonStatus(button, "Exit", &DBlue);
  AddButtonStatus(button, "Exit", &LBlue);

  // 2nd Row, Menu 26

  button = CreateButton(26, 5);
  strcpy(BandLabel, TabBandLabel[9]);
  AddButtonStatus(button, BandLabel, &Blue);
  AddButtonStatus(button, BandLabel, &Green);

  button = CreateButton(26, 6);
  strcpy(BandLabel, TabBandLabel[13]);
  AddButtonStatus(button, BandLabel, &Blue);
  AddButtonStatus(button, BandLabel, &Green);
}

void Start_Highlights_Menu26()
{
  // Set Band Select Labels
  char BandLabel[31];

  printf("Entering Start Highlights Menu26\n");

  strcpy(BandLabel, TabBandLabel[14]);
  AmendButtonStatus(ButtonNumber(26, 0), 0, BandLabel, &Blue);
  AmendButtonStatus(ButtonNumber(26, 0), 1, BandLabel, &Green);

  strcpy(BandLabel, TabBandLabel[0]);
  AmendButtonStatus(ButtonNumber(26, 1), 0, BandLabel, &Blue);
  AmendButtonStatus(ButtonNumber(26, 1), 1, BandLabel, &Green);

  strcpy(BandLabel, TabBandLabel[1]);
  AmendButtonStatus(ButtonNumber(26, 2), 0, BandLabel, &Blue);
  AmendButtonStatus(ButtonNumber(26, 2), 1, BandLabel, &Green);

  strcpy(BandLabel, TabBandLabel[2]);
  AmendButtonStatus(ButtonNumber(26, 3), 0, BandLabel, &Blue);
  AmendButtonStatus(ButtonNumber(26, 3), 1, BandLabel, &Green);

  strcpy(BandLabel, TabBandLabel[9]);
  AmendButtonStatus(ButtonNumber(26, 5), 0, BandLabel, &Blue);
  AmendButtonStatus(ButtonNumber(26, 5), 1, BandLabel, &Green);

  strcpy(BandLabel, TabBandLabel[13]);
  AmendButtonStatus(ButtonNumber(26, 6), 0, BandLabel, &Blue);
  AmendButtonStatus(ButtonNumber(26, 6), 1, BandLabel, &Green);
}

void Define_Menu27()
{
  int button;
  int i;

  strcpy(MenuTitle[27], "Frequency Preset Setting Menu (27)"); 

  // Bottom Row, Menu 27

  for (i = 0; i < 4; i = i + 1)
  {
    button = CreateButton(27, i);
    AddButtonStatus(button, FreqLabel[i + 5], &Blue);
    AddButtonStatus(button, FreqLabel[i + 5], &Green);
  }

  button = CreateButton(27, 4);
  AddButtonStatus(button, "Exit", &DBlue);
  AddButtonStatus(button, "Exit", &LBlue);

  // 2nd Row, Menu 27

  for (i = 5; i < 10; i = i + 1)
  {
    button = CreateButton(27, i);
    AddButtonStatus(button, FreqLabel[i - 5], &Blue);
    AddButtonStatus(button, FreqLabel[i - 5], &Green);
  }
}

void Start_Highlights_Menu27()
{
  // Preset Frequency Change 
  int index;
  int FreqIndex;
  int NoButton;
  char FreqLabel[31];
  div_t div_10;
  div_t div_100;
  div_t div_1000;

  if (CallingMenu == 3)      // TX Presets
  {
    for(index = 0; index < 9 ; index = index + 1)
    {
      // Define the button text
      MakeFreqText(index);
      NoButton = index + 5;   // Valid for bottom row
      if (index > 4)          // Overwrite for top row
      {
        NoButton = index - 5;
      }
      AmendButtonStatus(ButtonNumber(27, NoButton), 0, FreqBtext, &Blue);
      AmendButtonStatus(ButtonNumber(27, NoButton), 1, FreqBtext, &Green);
    }
  }
  else if (CallingMenu == 46)  // LMRX Presets
  {
    for(index = 1; index < 10 ; index = index + 1)
    {
      NoButton = index + 4;   // Valid for top row
      if (index > 5)          // Overwrite for top row
      {
        NoButton = index - 6;
      }
      if(strcmp(LMRXmode, "sat") == 0)
      {
        FreqIndex = index;
      }
      else
      {
        FreqIndex = index + 10;
      }

      div_10 = div(LMRXfreq[FreqIndex], 10);
      div_1000 = div(LMRXfreq[FreqIndex], 1000);

      if(div_10.rem != 0)  // last character not zero, so make answer of form xxx.xxx
      {
        snprintf(FreqLabel, 15, "%d.%03d", div_1000.quot, div_1000.rem);
      }
      else
      {
        div_100 = div(LMRXfreq[FreqIndex], 100);

        if(div_100.rem != 0)  // last but one character not zero, so make answer of form xxx.xx
        {
          snprintf(FreqLabel, 15, "%d.%02d", div_1000.quot, div_1000.rem / 10);
        }
        else
        {
          if(div_1000.rem != 0)  // last but two character not zero, so make answer of form xxx.x
          {
            snprintf(FreqLabel, 15, "%d.%d", div_1000.quot, div_1000.rem / 100);
          }
          else  // integer MHz, so just xxx.0
          {
            snprintf(FreqLabel, 15, "%d.0", div_1000.quot);
          }
        }
      }
      AmendButtonStatus(ButtonNumber(27, NoButton), 0, FreqLabel, &Blue);
      AmendButtonStatus(ButtonNumber(27, NoButton), 1, FreqLabel, &Green);
    }
  }
}

void Define_Menu28()
{
  int button;
  int i;

  strcpy(MenuTitle[28], "Symbol Rate/Bandwidth Preset Setting Menu (28)"); 

  // Bottom Row, Menu 28

  for (i = 0; i < 4; i = i + 1)
  {
    button = CreateButton(28, i);
    AddButtonStatus(button, SRLabel[i + 5], &Blue);
    AddButtonStatus(button, SRLabel[i + 5], &Green);
  }

  button = CreateButton(28, 4);
  AddButtonStatus(button, "Exit", &DBlue);
  AddButtonStatus(button, "Exit", &LBlue);

  // 2nd Row, Menu 16

  for (i = 5; i < 10; i = i + 1)
  {
    button = CreateButton(28, i);
    AddButtonStatus(button, SRLabel[i - 5], &Blue);
    AddButtonStatus(button, SRLabel[i - 5], &Green);
  }
}

void Start_Highlights_Menu28()
{
  // Preset SR Change 
  int index;
  int NoButton;
  char SRLabelLocal[31];

  if (CallingMenu == 3)      // TX Presets
  {
    for(index = 0; index < 9 ; index = index + 1)
    {
      NoButton = index + 5;   // Valid for bottom row
      if (index > 4)          // Overwrite for top row
      {
        NoButton = index - 5;
      }
      //index = index + 1;
      //snprintf(SRLabel, 30, "%i", SRLabel[index]);
      //snprintf(SRLabel, 30, "%i", index);
      AmendButtonStatus(ButtonNumber(28, NoButton), 0, SRLabel[index], &Blue);
      AmendButtonStatus(ButtonNumber(28, NoButton), 1, SRLabel[index], &Green);
    }
  }
  else if (CallingMenu == 46)  // LMRX Presets
  {
    for(index = 1; index < 7 ; index = index + 1)
    {
      NoButton = index + 4;   // Valid for top row
      if (index > 5)          // Overwrite for top row
      {
        NoButton = index - 6;
      }
      if(strcmp(LMRXmode, "sat") == 0)
      {
        snprintf(SRLabelLocal, 30, "%i", LMRXsr[index]);
      }
      else
      {
        snprintf(SRLabelLocal, 30, "%i", LMRXsr[index + 6]);
      }
      AmendButtonStatus(ButtonNumber(28, NoButton), 0, SRLabelLocal, &Blue);
      AmendButtonStatus(ButtonNumber(28, NoButton), 1, SRLabelLocal, &Green);
    }
    for(NoButton = 1; NoButton < 4 ; NoButton = NoButton + 1)
    {
      AmendButtonStatus(ButtonNumber(28, NoButton), 0, " ", &Blue);
      AmendButtonStatus(ButtonNumber(28, NoButton), 1, " ", &Green);
    }
  }
}

void Define_Menu29()
{
  int button;

  strcpy(MenuTitle[29], "Call, Locator and PID Setting Menu (29)"); 

  // Bottom Row, Menu 29

  button = CreateButton(29, 0);
  AddButtonStatus(button, "Video PID", &Blue);

  button = CreateButton(29, 1);
  AddButtonStatus(button, "Audio PID", &Blue);

  button = CreateButton(29, 2);
  AddButtonStatus(button, "PMT PID", &Blue);

  button = CreateButton(29, 3);
  AddButtonStatus(button, "PCR PID", &Grey);

  button = CreateButton(29, 4);
  AddButtonStatus(button, "Exit", &DBlue);
  AddButtonStatus(button, "Exit", &LBlue);

  // 2nd Row, Menu 29

  button = CreateButton(29, 5);
  AddButtonStatus(button, "Call", &Blue);

  button = CreateButton(29, 6);
  AddButtonStatus(button, "Locator", &Blue);

  button = CreateButton(29, 7);
  AddButtonStatus(button, "Set Contest^Codes", &Blue);
}

void Start_Highlights_Menu29()
{
  // Call, locator and PID
  char Buttext[31];
  char CallSign20[21];
  char Locator20[21];

  strcpyn(CallSign20, CallSign, 20);
  snprintf(Buttext, 31, "Call^%s", CallSign20);
  AmendButtonStatus(ButtonNumber(29, 5), 0, Buttext, &Blue);

  strcpyn(Locator20, Locator, 20);
  snprintf(Buttext, 31, "Locator^%s", Locator20);
  AmendButtonStatus(ButtonNumber(29, 6), 0, Buttext, &Blue);

  snprintf(Buttext, 25, "Video PID^%s", PIDvideo);
  AmendButtonStatus(ButtonNumber(29, 0), 0, Buttext, &Blue);

  snprintf(Buttext, 25, "Audio PID^%s", PIDaudio);
  AmendButtonStatus(ButtonNumber(29, 1), 0, Buttext, &Blue);

  snprintf(Buttext, 25, "PMT PID^%s", PIDpmt);
  AmendButtonStatus(ButtonNumber(29, 2), 0, Buttext, &Blue);

  snprintf(Buttext, 25, "PCR PID^%s", PIDstart);
  AmendButtonStatus(ButtonNumber(29, 3), 0, Buttext, &Grey);
}

void Define_Menu30()
{
  int button;

  strcpy(MenuTitle[30], "HDMI Display Selection Menu (30)"); 

  // Bottom Row, Menu 30

  button = CreateButton(30, 3);
  AddButtonStatus(button, "Reboot^to Apply", &Grey);
  AddButtonStatus(button, "Reboot^to Apply", &Red);

  button = CreateButton(30, 4);
  AddButtonStatus(button, "Exit", &Blue);
  AddButtonStatus(button, "Exit", &LBlue);

  // 2nd Row, Menu 30

  button = CreateButton(30, 5);
  AddButtonStatus(button, "HDMI^480p60", &Blue);
  AddButtonStatus(button, "HDMI^480p60", &Green);

  button = CreateButton(30, 6);
  AddButtonStatus(button, "HDMI^720p60", &Blue);
  AddButtonStatus(button, "HDMI^720p60", &Green);

  button = CreateButton(30, 7);
  AddButtonStatus(button, "HDMI^1080p60", &Blue);
  AddButtonStatus(button, "HDMI^1080p60", &Green);

  button = CreateButton(30, 8);
  AddButtonStatus(button, "HDMI^from EDID", &Blue);
  AddButtonStatus(button, "HDMI^from EDID", &Green);

  button = CreateButton(30, 9);
  AddButtonStatus(button, "Browser", &Blue);
  AddButtonStatus(button, "Browser", &Green);
}

void Start_Highlights_Menu30()
{
  char current_display[63] = " ";

  printf("Entering Start Highlights Menu30\n");

  // Highlight existing display type
  GetConfigParam(PATH_PCONFIG, "display", current_display);

  if (strcmp(current_display, "hdmi480") == 0)
  {
    SelectInGroupOnMenu(CurrentMenu, 5, 9, 5, 1);
  }
  if (strcmp(current_display, "hdmi720") == 0)
  {
    SelectInGroupOnMenu(CurrentMenu, 5, 9, 6, 1);
  }
  if (strcmp(current_display, "hdmi1080") == 0)
  {
    SelectInGroupOnMenu(CurrentMenu, 5, 9, 7, 1);
  }
  if (strcmp(current_display, "hdmi") == 0)
  {
    SelectInGroupOnMenu(CurrentMenu, 5, 9, 8, 1);
  }
  if (strcmp(current_display, "Browser") == 0)
  {
    SelectInGroupOnMenu(CurrentMenu, 5, 9, 9, 1);
  }
 
  if (reboot_required == true)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0);
  }
}

void Define_Menu31()
{
  int button;
  int i;
  int j;
  char Param[15];
  char DispName[10][20];

  strcpy(MenuTitle[31], "Amend Site Name and Locator (31)"); 
  
  for(i = 0; i < 10 ;i++)
  {
    sprintf(Param, "bcallsign%d", i);
    GetConfigParam(PATH_LOCATORS, Param, DispName[i]);
    DispName[i][10] = '\0';
    j = i + 5;
    if (i > 4)
    {
      j = i - 5;
    }
    button = CreateButton(31, j);
    AddButtonStatus(button, DispName[i], &Blue);
  }
}


void Start_Highlights_Menu31()
{
  int i;
  int j;
  char Param[15];
  char DispName[10][20];

  for(i = 0; i < 10 ;i++)
  {
    sprintf(Param, "bcallsign%d", i);
    GetConfigParam(PATH_LOCATORS, Param, DispName[i]);
    DispName[i][10] = '\0';
    j = i + 5;
    if (i > 4)
    {
      j = i - 5;
    }
    AmendButtonStatus(ButtonNumber(31, j), 0, DispName[i], &Blue);
  }
}


void Define_Menu32()
{
  int button;

  strcpy(MenuTitle[32], "Select and Set Reference Frequency Menu (32)"); 

  // Bottom Row, Menu 32

  //button = CreateButton(32, 0);
  //AddButtonStatus(button, "Set Ref 1", &Blue);

  //button = CreateButton(32, 1);
  //AddButtonStatus(button, "Set Ref 2", &Blue);

  //button = CreateButton(32, 2);
  //AddButtonStatus(button, "Set Ref 3", &Blue);

  //button = CreateButton(32, 3);
  //AddButtonStatus(button, "Set 5355", &Blue);

  button = CreateButton(32, 4);
  AddButtonStatus(button, "Exit", &DBlue);
  AddButtonStatus(button, "Exit", &LBlue);

  // 2nd Row, Menu 32

  //button = CreateButton(32, 5);
  //AddButtonStatus(button, "Use Ref 1", &Blue);
  //AddButtonStatus(button, "Use Ref 1", &Green);

  //button = CreateButton(32, 6);
  //AddButtonStatus(button, "Use Ref 2", &Blue);
  //AddButtonStatus(button, "Use Ref 2", &Green);

  //button = CreateButton(32, 7);
  //AddButtonStatus(button, "Use Ref 3", &Blue);
  //AddButtonStatus(button, "Use Ref 3", &Green);

  button = CreateButton(32, 9);
  AddButtonStatus(button, "RTL ppm", &Blue);
}

void Start_Highlights_Menu32()
{
  // Call, locator and PID

  char Buttext[31];

  //snprintf(Buttext, 26, "Use Ref 1^%s", ADFRef[0]);
  //AmendButtonStatus(ButtonNumber(32, 5), 0, Buttext, &Blue);
  //AmendButtonStatus(ButtonNumber(32, 5), 1, Buttext, &Green);

  //snprintf(Buttext, 26, "Use Ref 2^%s", ADFRef[1]);
  //AmendButtonStatus(ButtonNumber(32, 6), 0, Buttext, &Blue);
  //AmendButtonStatus(ButtonNumber(32, 6), 1, Buttext, &Green);

  //snprintf(Buttext, 26, "Use Ref 3^%s", ADFRef[2]);
  //AmendButtonStatus(ButtonNumber(32, 7), 0, Buttext, &Blue);
  //AmendButtonStatus(ButtonNumber(32, 7), 1, Buttext, &Green);

  //if (strcmp(ADFRef[0], CurrentADFRef) == 0)
  //{
  //  SelectInGroupOnMenu(CurrentMenu, 5, 7, 5, 1);
  //}
  //else if (strcmp(ADFRef[1], CurrentADFRef) == 0)
  //{
  //  SelectInGroupOnMenu(CurrentMenu, 5, 7, 6, 1);
  //}
  //else if (strcmp(ADFRef[2], CurrentADFRef) == 0)
  //{
  //  SelectInGroupOnMenu(CurrentMenu, 5, 7, 7, 1);
  //}
  //else
  //{
  //  SelectInGroupOnMenu(CurrentMenu, 5, 7, 7, 0);
  //}
  
  //snprintf(Buttext, 26, "Set Ref 1^%s", ADFRef[0]);
  //AmendButtonStatus(ButtonNumber(32, 0), 0, Buttext, &Blue);

  //snprintf(Buttext, 26, "Set Ref 2^%s", ADFRef[1]);
  //AmendButtonStatus(ButtonNumber(32, 1), 0, Buttext, &Blue);

  //snprintf(Buttext, 26, "Set Ref 3^%s", ADFRef[2]);
  //AmendButtonStatus(ButtonNumber(32, 2), 0, Buttext, &Blue);

  //snprintf(Buttext, 26, "Set 5355^%s", ADF5355Ref);
  //AmendButtonStatus(ButtonNumber(32, 3), 0, Buttext, &Blue);

  snprintf(Buttext, 26, "RTL ppm^%d", RTLppm);
  AmendButtonStatus(ButtonNumber(32, 9), 0, Buttext, &Blue);
}

void Define_Menu33()
{
  int button;

  strcpy(MenuTitle[33], "Check for Software Update Menu (33)"); 

  // Bottom Row, Menu 33

  //button = CreateButton(33, 3);
  //AddButtonStatus(button, "MPEG-2^License", &Blue);
  //AddButtonStatus(button, "MPEG-2^License", &Green);

  button = CreateButton(33, 4);
  AddButtonStatus(button, "Exit", &DBlue);
  AddButtonStatus(button, "Exit", &LBlue);

  // 2nd Row, Menu 33

  button = CreateButton(33, 5);
  AddButtonStatus(button, "Update", &Blue);
  AddButtonStatus(button, "Update", &Green);

  button = CreateButton(33, 6);
  AddButtonStatus(button, "Dev Update", &Blue);
  AddButtonStatus(button, "Dev Update", &Green);

  button = CreateButton(33, 7);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);
}

void Start_Highlights_Menu33()
{
  // Check for update

  if (strcmp(UpdateStatus, "NotAvailable") == 0)
  {
    AmendButtonStatus(ButtonNumber(33, 5), 0, " ", &Grey);
    AmendButtonStatus(ButtonNumber(33, 6), 0, " ", &Grey);
    AmendButtonStatus(ButtonNumber(33, 7), 0, " ", &Grey);
  }
  if (strcmp(UpdateStatus, "NormalUpdate") == 0)
  {
    AmendButtonStatus(ButtonNumber(33, 5), 0, "Update^Now", &Blue);
    AmendButtonStatus(ButtonNumber(33, 6), 0, " ", &Grey);
    AmendButtonStatus(ButtonNumber(33, 7), 0, " ", &Grey);
  }
  if (strcmp(UpdateStatus, "ForceUpdate") == 0)
  {
    AmendButtonStatus(ButtonNumber(33, 5), 0, "Force^Update", &Blue);
    AmendButtonStatus(ButtonNumber(33, 6), 0, "Dev^Update", &Blue);
    AmendButtonStatus(ButtonNumber(33, 7), 0, " ", &Grey);
  }
  if (strcmp(UpdateStatus, "DevUpdate") == 0)
  {
    AmendButtonStatus(ButtonNumber(33, 5), 0, " ", &Grey);
    AmendButtonStatus(ButtonNumber(33, 6), 0, " ", &Grey);
    AmendButtonStatus(ButtonNumber(33, 7), 0, "Confirm^Dev Update", &Red);
  }
}

void Define_Menu34()
{
  int button;

  strcpy(MenuTitle[34], "Start-up App Menu (34)"); 

  // Bottom Row, Menu 34

  button = CreateButton(34, 0);
  AddButtonStatus(button, "Boot to^Test Menu", &Blue);
  AddButtonStatus(button, "Boot to^Test Menu", &Green);

  button = CreateButton(34, 1);
  AddButtonStatus(button, "Boot to^Transmit", &Blue);
  AddButtonStatus(button, "Boot to^Transmit", &Green);

  button = CreateButton(34, 2);
  AddButtonStatus(button, "Boot to^Receive", &Blue);
  AddButtonStatus(button, "Boot to^Receive", &Green);

  button = CreateButton(34, 3);
  AddButtonStatus(button, "Boot to^Ryde RX", &Blue);
  AddButtonStatus(button, "Boot to^Ryde RX", &Green);
  AddButtonStatus(button, "Boot to^Ryde RX", &Grey);

  button = CreateButton(34, 4);
  AddButtonStatus(button, "Exit", &DBlue);
  AddButtonStatus(button, "Exit", &LBlue);

  // 2nd Row, Menu 34

  button = CreateButton(34, 5);
  AddButtonStatus(button, "Boot to^Portsdown", &Blue);
  AddButtonStatus(button, "Boot to^Portsdown", &Green);

  button = CreateButton(34, 6);
  AddButtonStatus(button, "Boot to^Langstone", &Blue);
  AddButtonStatus(button, "Boot to^Langstone", &Green);
  AddButtonStatus(button, "Boot to^Langstone", &Grey);

  button = CreateButton(34, 7);
  AddButtonStatus(button, "Boot to^Band Viewer", &Blue);
  AddButtonStatus(button, "Boot to^Band Viewer", &Green);

  button = CreateButton(34, 8);
  AddButtonStatus(button, "Boot to^Meteor Viewer", &Blue);
  AddButtonStatus(button, "Boot to^Meteor Viewer", &Green);

  //button = CreateButton(34, 9);
  //AddButtonStatus(button, "Boot to^Meteor Bcn RX", &Blue);
  //AddButtonStatus(button, "Boot to^Meteor Bcn RX", &Green);
}


void Start_Highlights_Menu34()         // Start-up App
{
  if (strcmp(StartApp, "Testmenu_boot") == 0)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 0), 1);
    SetButtonStatus(ButtonNumber(CurrentMenu, 1), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 2), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 5), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 7), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0);
  }
  else if (strcmp(StartApp, "Transmit_boot") == 0)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 1), 1);
    SetButtonStatus(ButtonNumber(CurrentMenu, 2), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 5), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 7), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0);
  }
  else if (strcmp(StartApp, "Receive_boot") == 0)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 1), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 2), 1);
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 5), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 7), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0);
  }
  else if (strcmp(StartApp, "Ryde_boot") == 0)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 1), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 2), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 1);
    SetButtonStatus(ButtonNumber(CurrentMenu, 5), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 7), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0);
  }
  else if (strcmp(StartApp, "Display_boot") == 0)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 1), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 2), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 5), 1);
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 7), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0);
  }
  else if (strcmp(StartApp, "Langstone_boot") == 0)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 1), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 2), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 5), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 1);
    SetButtonStatus(ButtonNumber(CurrentMenu, 7), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0);
  }
  else if (strcmp(StartApp, "Bandview_boot") == 0)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 1), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 2), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 5), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 7), 1);
    SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0);
  }
  else if (strcmp(StartApp, "Meteorview_boot") == 0)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 1), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 2), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 5), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 7), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 8), 1);
    SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 1), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 2), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 5), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 7), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0);
    SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0);
  }

  // Over-ride if Langstone not installed (set to Grey)
  if ((strcmp(langstone_version, "v1pluto") != 0) && (strcmp(langstone_version, "v2lime") != 0)
   && (strcmp(langstone_version, "v2pluto") != 0))
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 2);
  }

  // Over-ride if Reyde not installed (set to Grey
  if (file_exist("/home/pi/ryde/config.yaml") == 1)       // Ryde not installed
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 2);
  }
}


void Define_Menu35()    // Menu 35 Stream output Selector
{
  int button;
  int n;

  strcpy(MenuTitle[35], "Stream Output Selection Menu (35)"); 

  // Top Row, Menu 35
  button = CreateButton(35, 9);
  AddButtonStatus(button, "Amend^Preset", &Blue);
  AddButtonStatus(button, "Amend^Preset", &Red);

  // Bottom Row, Menu 35
  button = CreateButton(35, 4);
  AddButtonStatus(button, "Exit to^Main Menu", &DBlue);
  AddButtonStatus(button, "Exit to^Main Menu", &LBlue);

  for(n = 1; n < 9; n = n + 1)
  {
    if (n < 5)  // top row
    {
      button = CreateButton(35, n + 4);
      AddButtonStatus(button, " ", &Blue);
      AddButtonStatus(button, " ", &Green);
    }
    else       // Bottom Row
    {
      button = CreateButton(35, n - 5);
      AddButtonStatus(button, " ", &Blue);
      AddButtonStatus(button, " ", &Green);
    }
  }
}

void Start_Highlights_Menu35()
{
  // Stream Display Menu
  int n;
  char streamname[63];
  char key[63];

  for(n = 8; n > 0; n = n - 1)  // Go backwards to highlight first identical button
  {
    SeparateStreamKey(StreamKey[n], streamname, key);

    if (n < 5)  // top row
    {
      AmendButtonStatus(ButtonNumber(35, n + 4), 0, streamname, &Blue);
      AmendButtonStatus(ButtonNumber(35, n + 4), 1, streamname, &Green);
      if(strcmp(StreamKey[n], StreamKey[0]) == 0)
      {
        SelectInGroupOnMenu(35, 5, 8, n + 4, 1);
        SelectInGroupOnMenu(35, 0, 3, n + 4, 1);
      }
    }
    else       // Bottom Row
    {
      AmendButtonStatus(ButtonNumber(35, n - 5), 0, streamname, &Blue);
      AmendButtonStatus(ButtonNumber(35, n - 5), 1, streamname, &Green);
      if(strcmp(StreamKey[n], StreamKey[0]) == 0)
      {
        SelectInGroupOnMenu(35, 5, 8, n - 5, 1);
        SelectInGroupOnMenu(35, 0, 3, n - 5, 1);
      }
    }
  }
}

void Define_Menu36()
{
  int button;

  strcpy(MenuTitle[36], "WiFi Configuration Menu (36)"); 

  // Bottom Row, Menu 36



  button = CreateButton(36, 3);
  AddButtonStatus(button, "Disable^WiFi", &Blue);
  AddButtonStatus(button, "Disable^WiFi", &Green);
  AddButtonStatus(button, "Disable^WiFi", &Grey);

  button = CreateButton(36, 4);
  AddButtonStatus(button, "Exit", &DBlue);
  AddButtonStatus(button, "Exit", &LBlue);

  // 2nd Row, Menu 36

  button = CreateButton(36, 5);
  AddButtonStatus(button, "List WiFi^Networks", &Blue);
  AddButtonStatus(button, "List WiFi^Networks", &Green);
  AddButtonStatus(button, "List WiFi^Networks", &Grey);

  button = CreateButton(36, 6);
  AddButtonStatus(button, "Set-up WiFi^Network", &Blue);
  AddButtonStatus(button, "Set-up WiFi^Network", &Green);
  AddButtonStatus(button, "Set-up WiFi^Network", &Grey);

  button = CreateButton(36, 7);
  AddButtonStatus(button, "Check WiFi^Status", &Blue);
  AddButtonStatus(button, "Check WiFi^Status", &Green);
  AddButtonStatus(button, "Check WiFi^Status", &Grey);

  button = CreateButton(36, 8);
  AddButtonStatus(button, "Enable^WiFi", &Blue);
  AddButtonStatus(button, "Enable^WiFi", &Green);
  AddButtonStatus(button, "Enable^WiFi", &Grey);

  button = CreateButton(36, 9);
  AddButtonStatus(button, "Reset^WiFi", &Blue);
  AddButtonStatus(button, "Reset^WiFi", &Green);
  AddButtonStatus(button, "Reset^WiFi", &Grey);
}

void Start_Highlights_Menu36()
{
  if (CheckWifiEnabled() == 1)  // Wifi not enabled
  {
    SetButtonStatus(ButtonNumber(36, 5), 2);  // Grey out the "List WiFi Networks" Button
    SetButtonStatus(ButtonNumber(36, 6), 2);  // Grey out the "Set up WiFi Network" Button
  }
  else
  {
    if (GetButtonStatus(ButtonNumber(36, 5)) == 2) // "List WiFi Networks" is grey
    {
      SetButtonStatus(ButtonNumber(36, 5), 0);  // Set to Blue
    }
    if (GetButtonStatus(ButtonNumber(36, 6)) == 2) // "Set up WiFi Network" is grey
    {
      SetButtonStatus(ButtonNumber(36, 6), 0);  // Set to Blue
    }
  }
}

void Define_Menu37()
{
  int button;

  strcpy(MenuTitle[37], "Lime Configuration Menu (37)"); 

  // Bottom Row, Menu 37

  button = CreateButton(37, 0);
  AddButtonStatus(button, "Update to^Latest FW", &Blue);
  AddButtonStatus(button, "Update to^Latest FW", &Green);

  button = CreateButton(37, 1);
  AddButtonStatus(button, "Update to^FW 1.30", &Blue);
  AddButtonStatus(button, "Update to^FW 1.30", &Grey);

  button = CreateButton(37, 2);
  AddButtonStatus(button, "Update to^DVB FW", &Blue);
  AddButtonStatus(button, "Update to^DVB FW", &Grey);

  button = CreateButton(37, 3);
  AddButtonStatus(button, "LimeRFE^Disabled", &Blue);

  button = CreateButton(37, 4);
  AddButtonStatus(button, "Exit", &DBlue);
  AddButtonStatus(button, "Exit", &LBlue);

  // 2nd Row, Menu 37

  button = CreateButton(37, 5);
  AddButtonStatus(button, "Upsample^for Lime = ", &Blue);
  //AddButtonStatus(button, "LimeUtil^Info", &Blue);
  //AddButtonStatus(button, "LimeUtil^Info", &Green);

  button = CreateButton(37, 6);
  AddButtonStatus(button, "Lime^FW Info", &Blue);
  AddButtonStatus(button, "Lime^FW Info", &Green);

  button = CreateButton(37, 7);
  AddButtonStatus(button, "Lime^Report", &Blue);
  AddButtonStatus(button, "Lime^Report", &Green);

  button = CreateButton(37, 8);
  AddButtonStatus(button, "LimeRFE^Mode RX", &Grey);
  AddButtonStatus(button, "LimeRFE^Mode RX", &Blue);
  AddButtonStatus(button, "LimeRFE^Mode TX", &Red);

  button = CreateButton(37, 9);
  AddButtonStatus(button, "LimeRFE RX^Atten", &Blue);
}

void Start_Highlights_Menu37()  //  Lime Config Menu
{
  char caption[63];

  // Buttons 1 & 2 LimeSDR V1 update
  if (CheckLimeMiniV2Connect() == 0)    // Grey out for a LimeSDR V2
  {
    SetButtonStatus(ButtonNumber(37, 1), 1);
    SetButtonStatus(ButtonNumber(37, 2), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(37, 1), 0);
    SetButtonStatus(ButtonNumber(37, 2), 0);
  }

  // Button 3 LimeRFE Enable/Disable
  if (LimeRFEState == 1)  // Enabled
  {
    AmendButtonStatus(ButtonNumber(37, 3), 0, "LimeRFE^Enabled", &Blue);
  }
  else                    // Disabled
  {
    AmendButtonStatus(ButtonNumber(37, 3), 0, "LimeRFE^Disabled", &Blue);
  }

  // Button 5 LimeUpsample
  snprintf(caption, 60, "Upsample^for Lime = %d", LimeUpsample);
  AmendButtonStatus(ButtonNumber(37, 5), 0, caption, &Blue);

  // Button 8
  if (LimeRFEState == 1)  // Enabled
  {
    if (LimeRFEMode == 1)  // TX
    {
      SetButtonStatus(ButtonNumber(37, 8), 2);
    }
    else                   // RX
    {
      SetButtonStatus(ButtonNumber(37, 8), 1);
    }
  }
  else                    // Disabled
  {
    SetButtonStatus(ButtonNumber(37, 8), 0);
  }

  // Button 9 LimeRFE RX Attenuator
  if (LimeRFEState == 1)  // Enabled
  {
    snprintf(caption, 60, "LimeRFE RX^Atten = %ddB", 2 * LimeRFERXAtt);
    AmendButtonStatus(ButtonNumber(37, 9), 0, caption, &Blue);
  }
  else
  {
    AmendButtonStatus(ButtonNumber(37, 9), 0, "LimeRFE RX^Atten", &Grey);
  }
}

void Define_Menu38()
{
  int button;

  strcpy(MenuTitle[38], "Answer Yes or No (38)"); 

// 2nd Row, Menu 38

  button = CreateButton(38, 6);
  AddButtonStatus(button, "Yes", &Blue);
  AddButtonStatus(button, "Yes", &Green);

  button = CreateButton(38, 8);
  AddButtonStatus(button, "No", &Blue);
  AddButtonStatus(button, "No", &Green);
}

void Start_Highlights_Menu38()
{
  AmendButtonStatus(ButtonNumber(38, 6), 0, YesButtonCaption, &Blue);
  AmendButtonStatus(ButtonNumber(38, 6), 1, YesButtonCaption, &Green);
  AmendButtonStatus(ButtonNumber(38, 8), 0, NoButtonCaption, &Blue);
  AmendButtonStatus(ButtonNumber(38, 8), 1, NoButtonCaption, &Green);
}

void Define_Menu39()
{
  int button;

  strcpy(MenuTitle[39], "Langstone Configuration Menu (39)"); 

  // Bottom Row, Menu 39

  button = CreateButton(39, 0);
  AddButtonStatus(button, "Set Pluto IP^for Portsdown", &Blue);

  button = CreateButton(39, 1);
  AddButtonStatus(button, "Set Pluto IP^for Langstone", &Blue);

  button = CreateButton(39, 2);
  AddButtonStatus(button,"Update^Langstone",&Blue);
  AddButtonStatus(button,"Update^Langstone",&Green);
  AddButtonStatus(button,"Update^Langstone",&Grey);

  button = CreateButton(39, 4);
  AddButtonStatus(button, "Exit", &DBlue);
  AddButtonStatus(button, "Exit", &LBlue);

  button = CreateButton(39, 5);
  AddButtonStatus(button,"Install^Langstone V1",&Blue);
  AddButtonStatus(button,"Install^Langstone V1",&Green);
  AddButtonStatus(button,"Langstone V1^Installed",&Grey);

  button = CreateButton(39, 6);
  AddButtonStatus(button,"Install^Langstone V2",&Blue);
  AddButtonStatus(button,"Install^Langstone V2",&Green);
  AddButtonStatus(button,"Langstone V2^Installed",&Grey);

  button = CreateButton(39, 7);
  AddButtonStatus(button,"Langstone^use Pluto",&Blue);
  AddButtonStatus(button,"Langstone^use Lime",&Blue);
  AddButtonStatus(button,"Langstone^use Pluto",&Grey);

  button = CreateButton(39, 9);
  AddButtonStatus(button,"Start^Langstone",&Blue);
  AddButtonStatus(button,"Start^Langstone",&Green);
  AddButtonStatus(button," ",&Grey);
}

void Start_Highlights_Menu39()
{
  //Check Langstone status
  //printf("Langstone Version set as: %s\n", langstone_version);
  if (strcmp(langstone_version, "none") == 0)
  {
    SetButtonStatus(ButtonNumber(39, 2), 2);
    SetButtonStatus(ButtonNumber(39, 5), 0);
    SetButtonStatus(ButtonNumber(39, 6), 0);
    SetButtonStatus(ButtonNumber(39, 7), 2);
    SetButtonStatus(ButtonNumber(39, 9), 2);
  }
  if (strcmp(langstone_version, "v1pluto") == 0)
  {
    SetButtonStatus(ButtonNumber(39, 2), 0);
    SetButtonStatus(ButtonNumber(39, 5), 2);
    SetButtonStatus(ButtonNumber(39, 6), 0);
    SetButtonStatus(ButtonNumber(39, 7), 2);
  }
  if (strcmp(langstone_version, "v2lime") == 0)
  {
    SetButtonStatus(ButtonNumber(39, 2), 0);
    SetButtonStatus(ButtonNumber(39, 5), 0);
    SetButtonStatus(ButtonNumber(39, 6), 2);
    SetButtonStatus(ButtonNumber(39, 7), 1);
  }
  if (strcmp(langstone_version, "v2pluto") == 0)
  {
    SetButtonStatus(ButtonNumber(39, 2), 0);
    SetButtonStatus(ButtonNumber(39, 5), 0);
    SetButtonStatus(ButtonNumber(39, 6), 2);
    SetButtonStatus(ButtonNumber(39, 7), 0);
  }
}

void Define_Menu40()
{
  int button;

  strcpy(MenuTitle[40], "TS IP Configuration Menu (40)"); 

  // Bottom Row, Menu 40

  button = CreateButton(40, 0);
  AddButtonStatus(button, "Edit TS Out^IP Address", &Blue);

  button = CreateButton(40, 1);
  AddButtonStatus(button,"Edit TS Out^IP Port",&Blue);

  button = CreateButton(40, 2);
  AddButtonStatus(button,"Edit TS In^IP Port",&Blue);

  button = CreateButton(40, 3);
  AddButtonStatus(button,"Edit^TS Filename",&Blue);

  button = CreateButton(40, 4);
  AddButtonStatus(button, "Exit", &DBlue);
  AddButtonStatus(button, "Exit", &LBlue);

  button = CreateButton(40, 5);
  AddButtonStatus(button, "Edit RX Out^IP Address", &Blue);

  button = CreateButton(40, 6);
  AddButtonStatus(button,"Edit RX Out^IP Port",&Blue);
}

void Start_Highlights_Menu40()
{
  // Nothing here yet
}


void Define_Menu42()
{
  int button;

  strcpy(MenuTitle[42], "Output Device Menu (42)"); 

  // Bottom Row, Menu 42

  button = CreateButton(42, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  //button = CreateButton(42, 0);                        // Was Comp Vid
  //AddButtonStatus(button, TabModeOPtext[5], &Blue);
  //AddButtonStatus(button, TabModeOPtext[5], &Green);

  //button = CreateButton(42, 1);                        // Was DTX-1
  //AddButtonStatus(button, TabModeOPtext[6], &Blue);
  //AddButtonStatus(button, TabModeOPtext[6], &Green);

  button = CreateButton(42, 2);                          // IPTS Out
  AddButtonStatus(button, TabModeOPtext[7], &Blue);  
  AddButtonStatus(button, TabModeOPtext[7], &Green);

  button = CreateButton(42, 3);                          // Lime Mini
  AddButtonStatus(button, TabModeOPtext[8], &Blue);
  AddButtonStatus(button, TabModeOPtext[8], &Green);
  AddButtonStatus(button, TabModeOPtext[8], &Grey);

  // 2nd Row, Menu 42

  //button = CreateButton(42, 5);
  //AddButtonStatus(button, TabModeOPtext[0], &Blue);    // Was IQ
  //AddButtonStatus(button, TabModeOPtext[0], &Green);

  //button = CreateButton(42, 6);
  //AddButtonStatus(button, TabModeOPtext[1], &Blue);    // Was QPSK RF
  //AddButtonStatus(button, TabModeOPtext[1], &Green);

  button = CreateButton(42, 7);                          // Express
  AddButtonStatus(button, TabModeOPtext[2], &Blue);
  AddButtonStatus(button, TabModeOPtext[2], &Green);
  AddButtonStatus(button, TabModeOPtext[2], &Grey);

  button = CreateButton(42, 8);                          // Lime USB
  AddButtonStatus(button, TabModeOPtext[3], &Blue);
  AddButtonStatus(button, TabModeOPtext[3], &Green);
  AddButtonStatus(button, TabModeOPtext[3], &Grey);

  button = CreateButton(42, 9);                          // BATC Stream
  AddButtonStatus(button, TabModeOPtext[4], &Blue);
  AddButtonStatus(button, TabModeOPtext[4], &Green);

  // 3rd Row, Menu 42

  button = CreateButton(42, 10);                          // Jetson Lime
  AddButtonStatus(button, TabModeOPtext[9], &Blue);
  AddButtonStatus(button, TabModeOPtext[9], &Green);
  AddButtonStatus(button, TabModeOPtext[9], &Grey);

  button = CreateButton(42, 11);                          // Jetson Stream
  AddButtonStatus(button, TabModeOPtext[10], &Blue);
  AddButtonStatus(button, TabModeOPtext[10], &Green);
  AddButtonStatus(button, TabModeOPtext[10], &Grey);

  button = CreateButton(42, 12);                          // Muntjac
  AddButtonStatus(button, TabModeOPtext[11], &Blue);
  AddButtonStatus(button, TabModeOPtext[11], &Green);
  AddButtonStatus(button, TabModeOPtext[11], &Grey);

  button = CreateButton(42, 13);                          // Lime DVB
  AddButtonStatus(button, TabModeOPtext[12], &Blue);
  AddButtonStatus(button, TabModeOPtext[12], &Green);
  AddButtonStatus(button, TabModeOPtext[12], &Grey);

  button = CreateButton(42, 14);                          // Pluto
  AddButtonStatus(button, TabModeOPtext[13], &Blue);
  AddButtonStatus(button, TabModeOPtext[13], &Green);
  AddButtonStatus(button, TabModeOPtext[13], &Grey);
}

void Start_Highlights_Menu42()
{
  GreyOutReset42();

  if(strcmp(CurrentModeOP, TabModeOP[2]) == 0)  //DATVEXPRESS
  {
    SelectInGroupOnMenu(42, 5, 14, 7, 1);
    SelectInGroupOnMenu(42, 0, 3, 7, 1);
  }
  if(strcmp(CurrentModeOP, TabModeOP[3]) == 0)  // LIMEUSB
  {
    SelectInGroupOnMenu(42, 5, 14, 8, 1);
    SelectInGroupOnMenu(42, 0, 3, 8, 1);
  }
  if(strcmp(CurrentModeOP, TabModeOP[4]) == 0)  // STREAMER
  {
    SelectInGroupOnMenu(42, 5, 14, 9, 1);
    SelectInGroupOnMenu(42, 0, 3, 9, 1);
  }
  if(strcmp(CurrentModeOP, TabModeOP[7]) == 0)  // IPTS Out
  {
    SelectInGroupOnMenu(42, 5, 14, 2, 1);
    SelectInGroupOnMenu(42, 0, 3, 2, 1);
  }
  if(strcmp(CurrentModeOP, TabModeOP[8]) == 0)  // LIMEMINI
  {
    SelectInGroupOnMenu(42, 5, 14, 3, 1);
    SelectInGroupOnMenu(42, 0, 3, 3, 1);
  }
  if(strcmp(CurrentModeOP, TabModeOP[9]) == 0)  //JLIME
  {
    SelectInGroupOnMenu(42, 5, 14, 10, 1);
    SelectInGroupOnMenu(42, 0, 3, 10, 1);
  }
  if(strcmp(CurrentModeOP, TabModeOP[10]) == 0)  //JSTREAM
  {
    SelectInGroupOnMenu(42, 5, 14, 11, 1);
    SelectInGroupOnMenu(42, 0, 3, 11, 1);
  }
  if(strcmp(CurrentModeOP, TabModeOP[11]) == 0)  //MUNTJAC
  {
    SelectInGroupOnMenu(42, 5, 14, 12, 1);
    SelectInGroupOnMenu(42, 0, 3, 12, 1);
  }
  if(strcmp(CurrentModeOP, TabModeOP[12]) == 0)  //LIME DVB
  {
    SelectInGroupOnMenu(42, 5, 14, 13, 1);
    SelectInGroupOnMenu(42, 0, 3, 13, 1);
  }
  if(strcmp(CurrentModeOP, TabModeOP[13]) == 0)  //Pluto
  {
    SelectInGroupOnMenu(42, 5, 14, 14, 1);
    SelectInGroupOnMenu(42, 0, 3, 14, 1);
  }
  GreyOut42();
}

void Define_Menu43()
{
  int button;

  strcpy(MenuTitle[43], "System Configuration Menu (43)"); 

  // Bottom Row, Menu 43

  button = CreateButton(43, 4);
  AddButtonStatus(button, "Exit", &DBlue);
  AddButtonStatus(button, "Exit", &LBlue);

  button = CreateButton(43, 0);
  AddButtonStatus(button, "Restore^Factory", &Blue);
  AddButtonStatus(button, "Restore^Factory", &Green);

  button = CreateButton(43, 1);
  AddButtonStatus(button, "Restore^from USB", &Blue);
  AddButtonStatus(button, "Restore^from USB", &Green);

  button = CreateButton(43, 2);
  AddButtonStatus(button, "Restore^from /boot", &Blue);
  AddButtonStatus(button, "Restore^from /boot", &Green);

  button = CreateButton(43, 3);
  AddButtonStatus(button, "Restart^Touch", &Blue);

  // 2nd Row, Menu 43

  button = CreateButton(43, 5);
  AddButtonStatus(button, "Unmount^(Eject) USB", &Blue);
  AddButtonStatus(button, "Unmount^(Eject) USB", &Green);

  button = CreateButton(43, 6);
  AddButtonStatus(button, "Back-up^to USB", &Blue);
  AddButtonStatus(button, "Back-up^to USB", &Green);

  button = CreateButton(43, 7);
  AddButtonStatus(button, "Back-up^to /boot", &Blue);
  AddButtonStatus(button, "Back-up^to /boot", &Green);

  button = CreateButton(43, 8);
  AddButtonStatus(button, "Time^Overlay OFF", &Blue);
  AddButtonStatus(button, "Time^Overlay ON", &Blue);

  button = CreateButton(43, 9);
  AddButtonStatus(button, "SD Button^Enabled", &Blue);
  AddButtonStatus(button, "SD Button^Enabled", &Green);
  AddButtonStatus(button, "SD Button^Disabled", &Blue);
  AddButtonStatus(button, "SD Button^Disabled", &Green);

  // 3rd Row, Menu 43
  button = CreateButton(43, 10);
  AddButtonStatus(button, "Web Control^Enabled", &Blue);
  AddButtonStatus(button, "Web Control^Disabled", &Blue);

  button = CreateButton(43, 11);
  AddButtonStatus(button, "Start-up^App", &Blue);

  button = CreateButton(43, 12);
  AddButtonStatus(button, "Screen Type^7 inch", &Blue);

  button = CreateButton(43, 13);
  AddButtonStatus(button, "Invert^Pi Cam", &Blue);
  AddButtonStatus(button, "Un-invert^Pi Cam", &Blue);


  button = CreateButton(43, 14);
  AddButtonStatus(button, "Invert^Touchscreen", &Blue);
  AddButtonStatus(button, "Invert^Touchscreen", &Green);
  AddButtonStatus(button, "Invert^Touchscreen", &Grey);
}

void Start_Highlights_Menu43()
{
  if (timeOverlay == false)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 8), 1);
  }

  if (file_exist ("/home/pi/.pi-sdn") == 0)  // Hardware Shutdown Enabled
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 9), 2);
  }

  if (webcontrol == true)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 10), 0);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 10), 1);
  }

  if (strcmp(DisplayType, "Element14_7") == 0)
  {
    AmendButtonStatus(ButtonNumber(CurrentMenu, 12), 0, "Screen Type^7 inch", &Blue);
    SetButtonStatus(ButtonNumber(CurrentMenu, 14), 0);
  }
  if (strcmp(DisplayType, "dfrobot5") == 0)
  {
    AmendButtonStatus(ButtonNumber(CurrentMenu, 12), 0, "Screen Type^5 inch", &Blue);
    SetButtonStatus(ButtonNumber(CurrentMenu, 14), 2);
  }
  if (strcmp(DisplayType, "Browser") == 0)
  {
    AmendButtonStatus(ButtonNumber(CurrentMenu, 12), 0, "Browser^HDMI 480p60", &Blue);
    SetButtonStatus(ButtonNumber(CurrentMenu, 14), 2);
  }
  if (strcmp(DisplayType, "hdmi") == 0)
  {
    AmendButtonStatus(ButtonNumber(CurrentMenu, 12), 0, "Browser^and HDMI", &Blue);
    SetButtonStatus(ButtonNumber(CurrentMenu, 14), 2);
  }
  if (strcmp(DisplayType, "hdmi480") == 0)
  {
    AmendButtonStatus(ButtonNumber(CurrentMenu, 12), 0, "Browser^HDMI 480p60", &Blue);
    SetButtonStatus(ButtonNumber(CurrentMenu, 14), 2);
  }
  if (strcmp(DisplayType, "hdmi720") == 0)
  {
    AmendButtonStatus(ButtonNumber(CurrentMenu, 12), 0, "Browser^HDMI 720p60", &Blue);
    SetButtonStatus(ButtonNumber(CurrentMenu, 14), 2);
  }
  if (strcmp(DisplayType, "hdmi1080") == 0)
  {
    AmendButtonStatus(ButtonNumber(CurrentMenu, 12), 0, "Browser^HDMI 1080p60", &Blue);
    SetButtonStatus(ButtonNumber(CurrentMenu, 14), 2);
  }


  if (strcmp(CurrentPiCamOrientation, "normal") != 0)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 13), 1);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 13), 0);
  }
}

void Define_Menu44()
{
  int button;

  strcpy(MenuTitle[44], "Jetson Configuration Menu (44)"); 

  // Bottom Row, Menu 44

  button = CreateButton(44, 4);
  AddButtonStatus(button, "Exit", &DBlue);
  AddButtonStatus(button, "Exit", &LBlue);

  button = CreateButton(44, 0);
  AddButtonStatus(button, "Shutdown^Jetson", &Blue);
  AddButtonStatus(button, "Shutdown^Jetson", &Green);
  AddButtonStatus(button, "Shutdown^Jetson", &Grey);

  button = CreateButton(44, 1);
  AddButtonStatus(button, "Reboot^Jetson", &Blue);
  AddButtonStatus(button, "Reboot^Jetson", &Green);
  AddButtonStatus(button, "Reboot^Jetson", &Grey);

  button = CreateButton(44, 2);
  AddButtonStatus(button, "Set Jetson^IP address", &Blue);

  button = CreateButton(44, 3);
  AddButtonStatus(button, "LKV373^UDP IP", &Blue);

  button = CreateButton(44, 8);
  AddButtonStatus(button, "LKV373^UDP port", &Blue);

  // 2nd Row, Menu 44

  button = CreateButton(44, 5);
  AddButtonStatus(button, "Set Jetson^Username", &Blue);

  button = CreateButton(44, 6);
  AddButtonStatus(button, "Set Jetson^Password", &Blue);

  button = CreateButton(44, 7);
  AddButtonStatus(button, "Set Jetson^Root PW", &Blue);

//  button = CreateButton(44, 9);
//  AddButtonStatus(button, "SD Button^Enabled", &Blue);
//  AddButtonStatus(button, "SD Button^Enabled", &Green);
//  AddButtonStatus(button, "SD Button^Disabled", &Blue);
//  AddButtonStatus(button, "SD Button^Disabled", &Green);

  // 3rd Row, Menu 44

//  button = CreateButton(44, 10);

//  button = CreateButton(44, 11);

//  button = CreateButton(44, 13);

//  button = CreateButton(44, 14);

}

void Start_Highlights_Menu44()
{
  GreyOutReset44();
  GreyOut44();
}

void Define_Menu45()
{
  int button;

  strcpy(MenuTitle[45], "Video Source Menu (45)"); 

  // Bottom Row, Menu 45
  button = CreateButton(45, 0);                     // Contest
  AddButtonStatus(button, "Contest^Numbers", &Blue);
  AddButtonStatus(button, "Contest^Numbers", &Green);
  AddButtonStatus(button, "Contest^Numbers", &Grey);

  button = CreateButton(45, 1);                     // Webcam
  AddButtonStatus(button, "Webcam^inc C920", &Blue);
  AddButtonStatus(button, "Webcam^inc C920", &Green);
  AddButtonStatus(button, "Webcam^inc C920", &Grey);

  //button = CreateButton(45, 2);                     // Raw C920 not used
  //AddButtonStatus(button, "Raw C920^2 Mbps", &Blue);
  //AddButtonStatus(button, "Raw C920^2 Mbps", &Green);
  //AddButtonStatus(button, "Raw C920^2 Mbps", &Grey);

  button = CreateButton(45, 3);                     // HDMI
  AddButtonStatus(button, TabSource[8], &Blue);
  AddButtonStatus(button, TabSource[8], &Green);
  AddButtonStatus(button, TabSource[8], &Grey);

  button = CreateButton(45, 4);                     // Cancel
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  // 2nd Row, Menu 45

  button = CreateButton(45, 5);                     // Pi Cam
  AddButtonStatus(button, TabSource[0], &Blue);
  AddButtonStatus(button, TabSource[0], &Green);
  AddButtonStatus(button, TabSource[0], &Grey);

  button = CreateButton(45, 6);                     // Comp Vid
  AddButtonStatus(button, TabSource[1], &Blue);
  AddButtonStatus(button, TabSource[1], &Green);
  AddButtonStatus(button, TabSource[1], &Grey);

  // button = CreateButton(45, 7);                     // TCAnim not avilable on RPi 4
  // AddButtonStatus(button, TabSource[2], &Blue);
  // AddButtonStatus(button, TabSource[2], &Green);
  // AddButtonStatus(button, TabSource[2], &Grey);

  button = CreateButton(45, 8);                     // TestCard
  AddButtonStatus(button, TabSource[3], &Blue);
  AddButtonStatus(button, TabSource[3], &Green);
  AddButtonStatus(button, TabSource[3], &Grey);

  button = CreateButton(45, 9);                     // PiScreen
  AddButtonStatus(button, TabSource[4], &Blue);
  AddButtonStatus(button, TabSource[4], &Green);
  AddButtonStatus(button, TabSource[4], &Grey);
}

void Start_Highlights_Menu45()
{
  char vcoding[256];
  char vsource[256];
  ReadModeInput(vcoding, vsource);

  if(strcmp(CurrentSource, TabSource[0]) == 0)
  {
    SelectInGroupOnMenu(45, 5, 9, 5, 1);
    SelectInGroupOnMenu(45, 0, 3, 5, 1);
  }
  if(strcmp(CurrentSource, TabSource[1]) == 0)
  {
    SelectInGroupOnMenu(45, 5, 9, 6, 1);
    SelectInGroupOnMenu(45, 0, 3, 6, 1);
  }
  if(strcmp(CurrentSource, TabSource[2]) == 0)
  {
    SelectInGroupOnMenu(45, 5, 9, 7, 1);
    SelectInGroupOnMenu(45, 0, 3, 7, 1);
  }
  if(strcmp(CurrentSource, TabSource[3]) == 0)
  {
    SelectInGroupOnMenu(45, 5, 9, 8, 1);
    SelectInGroupOnMenu(45, 0, 3, 8, 1);
  }
  if(strcmp(CurrentSource, TabSource[4]) == 0)
  {
    SelectInGroupOnMenu(45, 5, 9, 9, 1);
    SelectInGroupOnMenu(45, 0, 3, 9, 1);
  }
  if(strcmp(CurrentSource, TabSource[5]) == 0)
  {
    SelectInGroupOnMenu(45, 5, 9, 0, 1);
    SelectInGroupOnMenu(45, 0, 3, 0, 1);
  }
  if(strcmp(CurrentSource, TabSource[6]) == 0)
  {
    SelectInGroupOnMenu(45, 5, 9, 1, 1);
    SelectInGroupOnMenu(45, 0, 3, 1, 1);
  }
  if(strcmp(CurrentSource, TabSource[7]) == 0)
  {
    SelectInGroupOnMenu(45, 5, 9, 2, 1);
    SelectInGroupOnMenu(45, 0, 3, 2, 1);
  }
  if(strcmp(CurrentSource, TabSource[8]) == 0)
  {
    SelectInGroupOnMenu(45, 5, 9, 3, 1);
    SelectInGroupOnMenu(45, 0, 3, 3, 1);
  }

  // Call to GreyOut inappropriate buttons
  GreyOut45();
}

void Define_Menu46()
{
  int button;

  strcpy(MenuTitle[46], "Portsdown Receiver Configuration (46)"); 

  // Bottom Row, Menu 46

  button = CreateButton(46, 0);
  AddButtonStatus(button, "UDP^IP", &Blue);
  AddButtonStatus(button, "UDP^IP", &Blue);

  button = CreateButton(46, 1);
  AddButtonStatus(button, "UDP^Port", &Blue);
  AddButtonStatus(button, "UDP^Port", &Blue);

  button = CreateButton(46, 2);
  AddButtonStatus(button, "Set Preset^RX Freqs", &Blue);
  AddButtonStatus(button, "Set Preset^RX Freqs", &Blue);

  button = CreateButton(46, 3);
  AddButtonStatus(button, "Set Preset^RX SRs", &Blue);
  AddButtonStatus(button, "Set Preset^RX SRs", &Blue);

  button = CreateButton(46, 4);
  AddButtonStatus(button, "Exit", &DBlue);
  AddButtonStatus(button, "Exit", &LBlue);

  // 2nd Row, Menu 46

  button = CreateButton(46, 5);
  AddButtonStatus(button, "Sat LNB^Offset", &Blue);
  AddButtonStatus(button, "Sat LNB^Offset", &Blue);

  button = CreateButton(46, 6);
  AddButtonStatus(button, "Autoset^LNB Offset", &Blue);
  AddButtonStatus(button, " ", &Grey);

  button = CreateButton(46, 7);
  AddButtonStatus(button, "Input^A", &Blue);
  AddButtonStatus(button, "Input^A", &Blue);

  button = CreateButton(46, 8);
  AddButtonStatus(button, "LNB Volts^OFF", &Blue);
  AddButtonStatus(button, "LNB Volts^18 Horiz", &Green);
  AddButtonStatus(button, "LNB Volts^13 Vert", &Green);

  button = CreateButton(46, 9);
  AddButtonStatus(button, "Audio out^RPi Jack", &Blue);
  AddButtonStatus(button, "Audio out^RPi Jack", &Blue);

  // 3rd Row, Menu 46

  button = CreateButton(46, 10);
  AddButtonStatus(button, "Tuner^Timeout", &Blue);

  button = CreateButton(46, 11);
  AddButtonStatus(button, "Tuner Scan^Width", &Blue);

  button = CreateButton(46, 12);
  AddButtonStatus(button, "TS Video^Channel", &Blue);
  AddButtonStatus(button, "TS Video^Channel", &Grey);

  button = CreateButton(46, 13);
  AddButtonStatus(button, "Modulation^DVB-S/S2", &Blue);
  AddButtonStatus(button, "Modulation^DVB-S/S2", &Blue);

  button = CreateButton(46, 14);
  AddButtonStatus(button, "Show Touch^Overlay", &Blue);
}

void Start_Highlights_Menu46()
{
  char LMBtext[63];

  if (strcmp(LMRXmode, "sat") == 0)
  {
    strcpy(MenuTitle[46], "Portsdown QO-100 Receiver Configuration (46)"); 
    strcpy(LMBtext, "QO-100^Input ");
    strcat(LMBtext, LMRXinput);
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0);
  }
  else
  {
    strcpy(MenuTitle[46], "Portsdown Terrestrial Receiver Configuration (46)"); 
    strcpy(LMBtext, "Terrestrial^Input ");
    strcat(LMBtext, LMRXinput);
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 1);
  }
  AmendButtonStatus(ButtonNumber(46, 7), 0, LMBtext, &Blue);

  if (strcmp(LMRXvolts, "off") == 0)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0);
  }
  if (strcmp(LMRXvolts, "h") == 0)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 8), 1);
  }
  if (strcmp(LMRXvolts, "v") == 0)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 8), 2);
  }
 

  if (strcmp(LMRXaudio, "rpi") == 0)
  {
    AmendButtonStatus(ButtonNumber(46, 9), 0, "Audio out^RPi Jack", &Blue);
  }
  else if (strcmp(LMRXaudio, "usb") == 0)
  {
    AmendButtonStatus(ButtonNumber(46, 9), 0, "Audio out^USB dongle", &Blue);
  }
  else
  {
    AmendButtonStatus(ButtonNumber(46, 9), 0, "Audio out^HDMI", &Blue);
  }

  if (strcmp(RXmod, "DVB-S") == 0)
  {
    AmendButtonStatus(ButtonNumber(46, 13), 0, "Modulation^DVB-S/S2", &Blue);
  }
  else
  {
    AmendButtonStatus(ButtonNumber(46, 13), 0, "Modulation^DVB-T/T2", &Blue);
  }
}


void Define_Menu47()
{
  int button;
  char label[63];

  strcpy(MenuTitle[47], "Portsdown Test Card Selection (47)"); 

  // Bottom Row, Menu 47

  button = CreateButton(47, 0);
  strcpy(label, "Card^");
  strcat(label, TestCardTitle[8]);
  AddButtonStatus(button, label, &Blue);
  AddButtonStatus(button, label, &Green);

  button = CreateButton(47, 1);
  strcpy(label, "Card^");
  strcat(label, TestCardTitle[9]);
  AddButtonStatus(button, label, &Blue);
  AddButtonStatus(button, label, &Green);

  button = CreateButton(47, 2);
  strcpy(label, "Card^");
  strcat(label, TestCardTitle[10]);
  AddButtonStatus(button, label, &Blue);
  AddButtonStatus(button, label, &Green);

  button = CreateButton(47, 3);
  strcpy(label, "Card^");
  strcat(label, TestCardTitle[11]);
  AddButtonStatus(button, label, &Blue);
  AddButtonStatus(button, label, &Green);

  button = CreateButton(47, 4);
  AddButtonStatus(button, "Exit", &DBlue);
  AddButtonStatus(button, "Exit", &LBlue);

  button = CreateButton(47, 5);
  strcpy(label, "Grey^Scale");
  AddButtonStatus(button, label, &Blue);
  AddButtonStatus(button, label, &Green);

  button = CreateButton(47, 6);
  strcpy(label, "Card^");
  strcat(label, TestCardTitle[5]);
  AddButtonStatus(button, label, &Blue);
  AddButtonStatus(button, label, &Green);

  button = CreateButton(47, 7);
  strcpy(label, "Card^");
  strcat(label, TestCardTitle[6]);
  AddButtonStatus(button, label, &Blue);
  AddButtonStatus(button, label, &Green);

  button = CreateButton(47, 8);
  strcpy(label, "Card^");
  strcat(label, TestCardTitle[7]);
  AddButtonStatus(button, label, &Blue);
  AddButtonStatus(button, label, &Green);

  button = CreateButton(47, 10);
  strcpy(label, "Test Card^F");
  AddButtonStatus(button, label, &Blue);
  AddButtonStatus(button, label, &Green);

  button = CreateButton(47, 11);
  strcpy(label, "Test Card^C");
  AddButtonStatus(button, label, &Blue);
  AddButtonStatus(button, label, &Green);

  button = CreateButton(47, 12);
  strcpy(label, "PM5544^ ");
  AddButtonStatus(button, label, &Blue);
  AddButtonStatus(button, label, &Green);

  button = CreateButton(47, 13);
  strcpy(label, "Colour Bars^ ");
  AddButtonStatus(button, label, &Blue);
  AddButtonStatus(button, label, &Green);

  button = CreateButton(47, 14);                        // Toggle Caption
  AddButtonStatus(button, "Caption^On", &Blue);
  AddButtonStatus(button, "Caption^Off", &Blue);
}

void Start_Highlights_Menu47()
{
  if (strcmp(CurrentCaptionState, "on") == 0)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 14), 0);
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 14), 1);
  }

  SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0);
  SetButtonStatus(ButtonNumber(CurrentMenu, 1), 0);
  SetButtonStatus(ButtonNumber(CurrentMenu, 2), 0);
  SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0);
  SetButtonStatus(ButtonNumber(CurrentMenu, 5), 0);
  SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0);
  SetButtonStatus(ButtonNumber(CurrentMenu, 7), 0);
  SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0);
  SetButtonStatus(ButtonNumber(CurrentMenu, 10), 0);
  SetButtonStatus(ButtonNumber(CurrentMenu, 11), 0);
  SetButtonStatus(ButtonNumber(CurrentMenu, 12), 0);
  SetButtonStatus(ButtonNumber(CurrentMenu, 13), 0);

  switch (CurrentTestCard)
  {
    case 0:
      SetButtonStatus(ButtonNumber(CurrentMenu, 10), 1);
      break;
    case 1:
      SetButtonStatus(ButtonNumber(CurrentMenu, 11), 1);
      break;
    case 2:
      SetButtonStatus(ButtonNumber(CurrentMenu, 12), 1);
      break;
    case 3:
      SetButtonStatus(ButtonNumber(CurrentMenu, 13), 1);
      break;
    case 4:
      SetButtonStatus(ButtonNumber(CurrentMenu, 5), 1);
      break;
    case 5:
      SetButtonStatus(ButtonNumber(CurrentMenu, 6), 1);
      break;
    case 6:
      SetButtonStatus(ButtonNumber(CurrentMenu, 7), 1);
      break;
    case 7:
      SetButtonStatus(ButtonNumber(CurrentMenu, 8), 1);
      break;
    case 8:
      SetButtonStatus(ButtonNumber(CurrentMenu, 0), 1);
      break;
    case 9:
      SetButtonStatus(ButtonNumber(CurrentMenu, 1), 1);
      break;
    case 10:
      SetButtonStatus(ButtonNumber(CurrentMenu, 2), 1);
      break;
    case 11:
      SetButtonStatus(ButtonNumber(CurrentMenu, 3), 1);
      break;
  }
}

void Define_Menu41()
{
  int button;

  strcpy(MenuTitle[41], " "); 

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

static void
terminate(int dummy)
{
  char Commnd[255];

  strcpy(ModeInput, "DESKTOP"); // Set input so webcam reset script is not called
  TransmitStop();
  LimeRFEClose();
  RTLstop();
  system("sudo killall vlc >/dev/null 2>/dev/null");
  system("sudo killall lmudp.sh >/dev/null 2>/dev/null");
  system("sudo killall longmynd >/dev/null 2>/dev/null");
  system("sudo killall /home/pi/rpidatv/bin/CombiTunerExpress >/dev/null 2>/dev/null");
  system("/home/pi/rpidatv/scripts/vlc_stream_player_stop.sh &");
  printf("Terminate\n");
  setBackColour(0, 0, 0);
  clearScreen();
  closeScreen();
  sprintf(Commnd,"sudo killall express_server >/dev/null 2>/dev/null");
  system(Commnd);
  sprintf(Commnd,"stty echo");
  system(Commnd);
  sprintf(Commnd,"reset");
  system(Commnd);
  exit(1);
}

// main initializes the system and starts Menu 1 

int main(int argc, char **argv)
{
  int NoDeviceEvent=0;
  wscreen = 800;
  hscreen = 480;
  int screenXmax, screenXmin;
  int screenYmax, screenYmin;
  int i;
  int startupmenu = 1;
  char Param[255];
  char Value[255];
  char USBVidDevice[255];
  char SetStandard[255];
  char vcoding[256];
  char vsource[256];

  strcpy(ProgramName, argv[0]);

  // Catch sigaction and call terminate
  for (i = 0; i < 16; i++)
  {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = terminate;
    sigaction(i, &sa, NULL);
  }

  // Set up wiringPi module
  if (wiringPiSetup() < 0)
  {
    return 0;
  }

  // Check startup request
  if (argc > 2 )
  {
	for ( i = 1; i < argc - 1; i += 2 )
    {
      if (strcmp(argv[i], "-b") == 0)
      {
        if (strcmp(argv[i + 1], "tx") == 0)
        {
          boot_to_tx = true;
        }
        else if (strcmp(argv[i + 1], "rx") == 0)
        {
          boot_to_rx = true;
        }
        else
        {
          startupmenu = atoi(argv[i + 1] );
        }
      }
    }
  }

  // Initialise all the spi GPIO ports to the correct state
  InitialiseGPIO();

  // Set the Analog Capture (input) Standard
  GetUSBVidDev(USBVidDevice);
  if (strlen(USBVidDevice) == 12)  // /dev/video* with a new line
  {
    strcpy(Param,"analogcamstandard");
    GetConfigParam(PATH_PCONFIG,Param,Value);
    USBVidDevice[strcspn(USBVidDevice, "\n")] = 0;  //remove the newline
    strcpy(SetStandard, "v4l2-ctl -d ");
    strcat(SetStandard, USBVidDevice);
    strcat(SetStandard, " --set-standard=");
    strcat(SetStandard, Value);
    //printf(SetStandard);
    system(SetStandard);
  }

  // Check the display type in the config file
  GetConfigParam(PATH_PCONFIG, "display", DisplayType);

  // Check for presence of touchscreen
  for(NoDeviceEvent = 0; NoDeviceEvent < 7; NoDeviceEvent++)
  {
    if (openTouchScreen(NoDeviceEvent) == 1)
    {
      if(getTouchScreenDetails(&screenXmin,&screenXmax,&screenYmin,&screenYmax)==1) break;
    }
  }

  if(NoDeviceEvent != 7)  // Touchscreen detected
  {
    touchscreen_present = true;

    // Create Touchscreen thread
    pthread_create (&thtouchscreen, NULL, &WaitTouchscreenEvent, NULL);

    // If display previously set to Browser or hdmi, correct it
    if ((strcmp(DisplayType, "Browser") == 0) || (strcmp(DisplayType, "hdmi") == 0)
     || (strcmp(DisplayType, "hdmi480") == 0) || (strcmp(DisplayType, "hdmi720") == 0)
     || (strcmp(DisplayType, "hdmi1080") == 0))
    {
      SetConfigParam(PATH_PCONFIG, "webcontrol", "enabled");
      SetConfigParam(PATH_PCONFIG, "display", "Element14_7");
      system ("/home/pi/rpidatv/scripts/set_display_config.sh");
      system ("sudo reboot now");
    }
  }
  else // No touchscreen detected, so enable webcontrol, change display type and reboot if required
  {
    touchscreen_present = false;

    if ((strcmp(DisplayType, "Browser") != 0) && (strcmp(DisplayType, "hdmi") != 0)
     && (strcmp(DisplayType, "hdmi480") != 0) && (strcmp(DisplayType, "hdmi720") != 0)
     && (strcmp(DisplayType, "hdmi1080") != 0))
    {
      SetConfigParam(PATH_PCONFIG, "webcontrol", "enabled");
      SetConfigParam(PATH_PCONFIG, "display", "hdmi720");         // Set 720p60 for maximum compatibility
      system ("/home/pi/rpidatv/scripts/set_display_config.sh");
      system ("sudo reboot now");
    }
    handle_mouse();
  }

  // Show Portsdown Logo
  system("sudo fbi -T 1 -noverbose -a \"/home/pi/rpidatv/scripts/images/BATC_Black.png\" >/dev/null 2>/dev/null");
  system("(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");

  // Calculate screen parameters
  scaleXvalue = ((float)screenXmax-screenXmin) / wscreen;
  scaleYvalue = ((float)screenYmax-screenYmin) / hscreen;

  // Define button grid
  // -25 keeps right hand side symmetrical with left hand side
  wbuttonsize=(wscreen-25)/5;
  hbuttonsize=hscreen/6;

  // Initialise direct access to the 7 inch screen
  initScreen();

  // Read in the presets from the Config files
  ReadPresets();
  ReadModeInput(vcoding, vsource);
  ReadModeOutput(vcoding);
  ReadModeEasyCap();
  ReadPiCamOrientation();
  ReadCaptionState();
  ReadTestCardState();
  ReadAudioState();
  ReadAttenState();
  ReadBand();
  ReadBandDetails();
  ReadCallLocPID();
  ReadADFRef();
  ReadRTLPresets();
  ReadRXPresets();
  ReadStreamPresets();
  ReadLMRXPresets();
  ReadLangstone();
  ReadTSConfig();
  ReadContestSites();
  ReadVLCVolume();
  ReadWebControl();  // this starts the web listener thread if required

  SetAudioLevels();

  // Initialise all the button Status Indexes to 0
  InitialiseButtons();

  // Define all the Menu buttons
  Define_Menu1();
  Define_Menu2();
  Define_Menu3();
  Define_Menu4();
  //Define_Menu5();
  Define_Menu6();
  Define_Menu7();
  Define_Menu8();
  Define_Menu9();
  Define_Menu10();
  Define_Menu11();
  Define_Menu12();
  Define_Menu13();
  Define_Menu14();
  Define_Menu15();
  Define_Menu16();
  Define_Menu17();
  Define_Menu18();
  Define_Menu19();
  Define_Menu20();
  Define_Menu21();
  Define_Menu22();
  Define_Menu23();
  Define_Menu24();
  Define_Menu25();
  Define_Menu26();
  Define_Menu27();
  Define_Menu28();
  Define_Menu29();
  Define_Menu30();
  Define_Menu31();
  Define_Menu32();
  Define_Menu33();
  Define_Menu34();
  Define_Menu35();
  Define_Menu36();
  Define_Menu37();
  Define_Menu38();
  Define_Menu39();
  Define_Menu40();
  Define_Menu41();
  Define_Menu42();
  Define_Menu43();
  Define_Menu44();
  Define_Menu45();
  Define_Menu46();
  Define_Menu47();

  // Check if DATV Express Server required and, if so, start it
  CheckExpress();

  // Check Pluto reboot status
  if (strcmp(CurrentModeOP, "PLUTO") == 0)
  {
    RebootPluto(2);
  }

  // Check for LimeNET Micro
  LimeNETMicroDet = DetectLimeNETMicro();

  // Set the Band (and filter) Switching
  // Must be done after (not before) starting DATV Express Server
  system ("sudo /home/pi/rpidatv/scripts/ctlfilter.sh");
  // and wait for it to finish using portsdown_config.txt
  usleep(100000);

  // Start the receive downconverter LO if required
  ReceiveLOStart();

  // Initialise the LimeRFE if required
  LimeRFEInit();


  // Initialise web access

  web_x = -1;
  web_y = -1;
  web_x_ptr = &web_x;
  web_y_ptr = &web_y;

  if (startupmenu == 1)                    // Main Menu
  {
    setBackColour(255, 255, 255);          // White background
    clearScreen();
    Start_Highlights_Menu1();
  }
  else if (startupmenu == 8)               // Receive Menu      
  {
    setBackColour(0, 0, 0); 
    clearScreen();
    CurrentMenu = startupmenu;
    Start_Highlights_Menu8();
  }
  else
  {
    setBackColour(0, 0, 0);          // Black background
    clearScreen();
    CurrentMenu = startupmenu;
    // Note no highlights
  }
  UpdateWindow();

  // Go and wait for the screen to be touched
  waituntil(wscreen,hscreen);

  // Not sure that the program flow ever gets here

  return 0;
}
