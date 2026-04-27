/*  Muntjac-4 - a DVB-S2 driver for the Muntjac SDR
    Copyright (C) 2026  Brian Jordan G4EWJ

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

// Muntjac4 for Raspberry Pi

#define VERSIONX 	"muntjacsdr_dvb"
#define VERSIONX2	"1v0m"

/*
	ensure panic frames have at least 4 packets for MiniTioune 
	prevent transmit routine from winding up a cpu
	write all usb bytes at once
	remove SR100
	reset TEMPS before frequency band check
	remove TESTCARD check in chinese mode
	add LO calibration for tripling from 413.333 to 429.999
	tidy up help output
	correct -y error
	put driver and pico versions into sdt
	buffer 1.5s worth of frames before starting USB output
	correct -x and use for mixer power
	add new mixing ranges for 23cm
	correct 2m mixing range
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef signed int          int32 ;
typedef unsigned int        uint32 ;
typedef unsigned short      uint16 ;
typedef short               int16 ;
typedef unsigned char       uint8 ;
typedef signed char         int8 ;
typedef u_int64_t           uint64 ;
typedef int64_t             int64 ;


#define TEST		0

/*

Change log

2025-04-02	muntjacsdr_dvb_0v2j		remove power cap 15 (now 31)
									remove PD power correction
									correct LO calibration for power 15
2025-03-28	muntjacsdr_dvb_0v2g		First release

*/


/*
	G4EWJ October 2024

	Converts a transport stream to DVB-S2 digital symbols 
	and sends to a Muntjac via a USB serial port.
	TS to DVB-S2 conversion is handled by dvbs2neon.S

	QPSK is supported for SR 100, 125, 250, 333, 500, 1000
	8PSK is supported for SR 100, 125, 250, 333, 500
Short frame and pilots are supported (with correct bitrate reported)
	SR parameters -s 333000 and -s 333333 are supported
	333333 is the native SR - 333000 will insert a null packet every 1000 packets

	Both lowband and highband may be used simultaneously
	The bands are referred to as primary and secondary
	The secondary tx may be the same as the primary, or a carrier or sidebands  
	-f sidebands transmits alternating LSB and USB carriers for calibration

	build: gcc muntjacsdr_dvb_0v2a.c dvbs2neon0v47.S -mfpu=neon -lpthread -o muntjacsdr_dvb_0v1a
*/


// Linux return code

// if bit7    = 0 , bits 6:0 each indicate an error
// if bit7:6  = 10, bits 5:0 each indicate an error
// if bit7:6  = 11, bits 5:0 indicate a single error

#define	ERROR_NONE					0x00

// returncode bit 7 = 0

#define	ERROR_FREQUENCY				0x000001
#define	ERROR_SYMBOLRATE			0x000002
#define	ERROR_FEC					0x000004
#define	ERROR_POWER					0x000008
#define	ERROR_INPUT					0x000010
#define	ERROR_OUTPUT				0x000020
#define	ERROR_DVB					0x000040		// modulation, constellation or invalid combination with FEC

// returncode bits 7:6 = 10 + values below >> 8

#define ERROR_SECONDARYFREQ			0x000100		// secondary frequency invalid
#define ERROR_SECONDARYSR			0x000200		// SR cannot be specified for secondary
#define ERROR_SECONDARYFEC			0x000400		// FEC cannot be specified for secondary
#define ERROR_SECONDARYPOWER		0x000800		// absent or invalid

// returncode bits 7:5 = 110 + values below >> 16

#define ERROR_INPUTREAD				0x010000		 
#define ERROR_OUTPUTWRITE			0x020000
#define ERROR_LO					0x040000
#define ERROR_PARAMETER				0x080000
#define ERROR_SIGNAL				0x100000

// bits 7:5 = 111 + abs (dvbs2neon_error)

#define ERROR_DVBS2NEON				0xe0			// 0xe1-0xfe: dvbs2neon errors 1-30
#define ERROR_MUNTJAC				0xfe			// unspecified error
#define ERROR_UNDEFINED				0xff			// unspecified error
								
// dvbs2neon definitions

#define STREAM0                         0
#define CONTROL_RESET_FULL              0
#define CONTROL_RESET_STREAM            1
#define CONTROL_SET_PARAMETERS          2
#define CONTROL_SET_OUTPUT_BUFFER       3
#define CONTROL_GET_EFFICIENCY          4
#define CONTROL_GET_INFO                5
#define CONTROL_GET_LAST_BBFRAME        6
#define CONTROL_CONVERT_BBFRAME         7
#define CONTROL_GET_DATAFIELD_SIZE      8
#define CONTROL_GET_SYMBOLS_PER_FRAME   9
#define CONTROL_GET_DUMMY_FRAME         10
#define MAGIC_MARKER                    0x7388c542


// dvbs2neon errors

#define ERROR_COMMAND_INVALID							-1
#define ERROR_STREAM_INVALID							-2
#define ERROR_FRAMETYPE_INVALID							-3
#define ERROR_FEC_INVALID								-4
#define ERROR_ROLLOFF_INVALID							-5
#define ERROR_CONSTELLATION_INVALID						-6
#define ERROR_PILOTS_INVALID							-7
#define ERROR_OUTPUT_FORMAT_INVALID						-8
#define ERROR_BUFFER_ADDRESS_INVALID					-9
#define ERROR_FEC_INVALID_FOR_CONSTELLATION				-10
#define ERROR_BUFFER_POOL_INVALID						-11
#define ERROR_NO_OUTBUFFER_AVAILABLE					-12
#define ERROR_OUTPUT_FORMAT_INVALID_FOR_CONSTELLATION	-13
#define ERROR_OUTPUT_FORMAT_INVALID_FOR_ROLLOFF			-14
#define ERROR_FRAME_FEC_COMBINATION_INVALID				-15
#define ERROR_BBFRAME_NOT_AVAILABLE                     -16
#define ERROR_PACKET_SYNC_BYTE_MISSING                  -17
#define ERROR_NOT_IN_TS_MODE				            -18
#define ERROR_NOT_IN_GSE_MODE				            -19
#define ERROR_GSE_DATA_TOO_BIG				            -20
#define ERROR_UNKNOWN						            -21
#define ERROR_INVALID_MULTISTREAM						-22
#define ERROR_INVALID_DATAMODE							-23
#define ERROR_DATAFIELD_SIZE_SELECT_INVALID				-24
#define ERROR_COMBINATION_NOT_SUPPORTED					-25
#define ERROR_LOWEST						            -25


// parameter definitions compatible with dvbs2neon and DATV-Express 

#define CR_1_4              0
#define CR_1_3              1
#define CR_2_5              2
#define CR_1_2              3
#define CR_3_5              4
#define CR_2_3              5
#define CR_3_4              6
#define CR_4_5              7
#define CR_5_6              8
#define CR_8_9              9
#define CR_9_10             10
#define M_QPSK              0
#define M_8PSK              1
#define M_16APSK            2
#define M_32APSK            3
#define FRAME_NORMAL        0x00
#define FRAME_SHORT         0x10
#define PILOTS_OFF          0
#define PILOTS_ON           1
#define RO_0_35             0
#define RO_0_25             1
#define RO_0_20             2
#define RO_0_15             3
#define FEC14               0
#define FEC13               1
#define FEC25               2
#define FEC12               3
#define FEC35               4
#define FEC23               5
#define FEC34               6
#define FEC45               7
#define FEC56               8
#define FEC89               9
#define FEC910             10

#define M_DVBS				1
#define M_DVBS2				2

#define MAXFECS				13
#define FEC_CARRIER			11
#define FEC_SIDEBANDS		12
#define LIMEBAND_76GHZ		10

// record types embedded in the data sent to Muntjac
// types > 0x80 all follow the common_record format

#define     REBOOT_RECORD	       	0x81
#define     DATV_RECORD 	   	    2
#define     NARROWBAND_RECORD      	3
#define     EOF_RECORD          	0x84
#define     STATUSREQUEST_RECORD   	0x86
#define     BEGINFRAME_RECORD   	0x88
#define     ENDFRAME_RECORD     	0x89

#define		ESCAPE					0x5c			// value to show the start of embedded output data to the Muntjac
#define		SDTPID					17				// Muntjac adds its name to the provider field		
#define		TDTPID					20
#define 	TSID					1
#define		NETWORK					1

#define		TCPCHECKTIME			1000
#define		TCPLOSTTIME				5000
#define		SDTINSERTIONTIME		300
#define		SETTINGSREQUESTTIME		250

#define 	TIMENONE                0
#define 	TIMEUTC                 1
#define 	TIMELOCAL               2

#define		DEFAULTUDPINPORT		9998

	char					fecs [] [16] = 	{"1/4", "1/3", "2/5", "1/2", "3/5", "2/3", "3/4", "4/5", "5/6", "8/9", "9/10", "CARRIER", "SIDEBANDS"} ;
											
// structures for control records inserted in the output data

typedef struct 								// taken from DATV-Express
{
	uint32              frametype ;
	uint32              fec ;
	uint32              rolloff ;
	uint32              constellation ;
	uint32              pilots ;
	uint32              output_format ;     // dummy_frame in DATV-Express
	uint32				magic ;				// 0x7388c542 indicates that 16 x uint32 values follow
	uint32				datafield_size ;	// the datafield contains a whole number of packets
	uint32				extra1 ;					
	uint32				extra2 ;					
	uint32				extra3 ;					
	uint32				extra4 ;					
	uint32				extra5 ;					
	uint32				extra6 ;					
	uint32				extra7 ;					
	uint32				extra8 ;					
	uint32				extra9 ;					
	uint32				extra10 ;					
	uint32				extra11 ;					
	uint32				extra12 ;					
	uint32				extra13 ;					
	uint32				extra14 ;					
	uint32				extra15 ;					
} DVB2FrameFormat ; 


typedef struct 						// record type = 0
{
	uint8		special ;			// 0 indicates special data
	uint8		recordtype ;		// 0 indicates that this is just a symbol byte with value zero 
} record0_zerobyte ;

typedef struct						// record type = x1 (currently unused)
{
	uint8		special ;			// 0 indicates special data
	uint8		recordtype ;		// x1 indicates that only the upper nybble contains symbol information 
} record1_halfbyte ;

typedef struct
{
    uint8               special ;                       // ESCAPE indicates special data
    uint8               recordtype ;                    // 2 = DVB symbols, 3 = IQ data
    uint16              bytesfollowing ;                // to allow unknown record types to be skipped
    uint8               dataversion ;                   // for variations of this record format
    uint8               stream ;                        // future use
    uint8               txon                [2] ;       // 0xc5 = on
    int32               symbolrate ;                    // 100, 125, 250, 333, 500, 1000
    uint32              frequency           [2] ;       // hertz, for LOWBAND and highband
    uint32              bufferinginitial ;              // number of bytes to receive before starting output
    uint32              bufferlowtrigger ;              // issue BUFFER_LOW when fewer bytes are in the buffer
    int8                carriers            [2] ;       // 1 = carrier, 2 = alternating sidebands
    int8                dvbtype	;						// 1 = DVB-S, 0 = DVB-S2
    int8                frametype ;
    int8                constellation ;
    int8                fec ;
    int8                pilots ;
    int8                rolloff ;
    uint8               spare0              [4] ;
    uint8               spacer ;                        // anything that changes below here does not stop tx
    int8                pause ;
    int8                power               [2] ;       // 0-31 actual
    uint32              recordsequence ;                // should increase for every settings record
    int32               dialclicks ;
    int8                iloc                [2] ;       // local oscillator suppression
    int8                qloc                [2] ;
    int8                spare1              [4] ;
    char                magicmarker         [4] ;       // 'MJAC' to verify that this is a valid record
    uint32              crc ;                           // (currently unused)
} mjsettings_record ;


typedef struct
{
    uint8               special ;                       // ESCAPE indicates special data
    uint8               recordtype ;                    // 
    uint16              bytesfollowing ;                // to allow unknown record types to be skipped
  	int32				spare0 ;
  	int32				spare1 ;
    char                magicmarker         [4] ;       // 'MJAC' to verify that this is a valid record
    uint32              crc ;                           // (currently unused)
} common_record ;

struct tdtx
{
    uint8               sync            :8  ;

    uint8               pid1208         :5  ;
    uint8               tspriority      :1  ;
    uint8               payloadstart    :1  ;
    uint8               tserror         :1  ;

    uint8               pid0700         :8  ;

    uint8               continuity      :4  ;
    uint8               adaption        :2  ;
    uint8               scrambling      :2  ;

    uint8               pointer         :8  ;

    uint8               tableid			    ;

    uint8               filler1         :4  ;
    uint8               reserved0       :2  ;
    uint8               filler0         :1  ;
    uint8               syntax          :1  ;

    uint8               sectionlength       ;
    uint8               datetime [5]        ;

///    uint8               crc32 [4] ;

    uint8               filler2 [175]       ;
} ;


struct sdtx
{
    uint                sync            :8  ;
	
    uint                pid1208         :5  ;
    uint                tspriority      :1  ;
    uint                payloadstart    :1  ;    
    uint                tserror         :1  ;
	
    uint                pid0700         :8  ;

    uint                continuity      :4  ;
    uint                adaption        :2  ;
    uint                scrambling      :2  ;
	
    uint                pointer         :8  ;

    uint8               tableid             ;

    uint                filler1         :4  ;                
    uint                reserved0       :2  ;
    uint                filler0         :1  ;
    uint                syntax          :1  ;
	
    uint8               sectionlength       ;
    uint                filler2         :8  ;   // upper byte of TS id
    uint                tsid            :8  ;
	
    uint                currentnext     :1  ;
    uint                version         :5  ;
    uint                reserved1       :2  ;
	
    uint                section         :8  ;
    uint                lastsection     :8  ; 

    uint                filler3         :8  ;   // upper byte of network id
    uint                network         :8  ;
	
    uint                reserved4       :8  ;   // 0xff

    uint8               serviceloop         ;   // start of service infos

    uint8               filler5 [171]       ;
} ;

// global data

			struct sdtx     			sdt ;    
			struct tdtx					tdt ;	
			uint8 						savedpcrpacket [188] ;
			char						callsign 	[256] ;
			char						provider 	[256] ;
			char						callsign2 	[256] ;
			char						provider2 	[256] ;
			char						modeinput 	[256] ;
			char						title 		[4096] ;
volatile	int32						fhin ;
			int32						fh ;
volatile	int32						mjfd ;
volatile	int32						mjfd2 ;
			FILE						*mjfd3 ;
volatile	int32						terminate ;	
volatile	uint32						bytesout ;
volatile	uint32						framecount ;
volatile	uint						returncode ;
			uint16						infoport ;
			mjsettings_record			mjsettings ;		
			common_record				statusrequest ;		
			common_record				beginframe ;		
			common_record				endframe ;		
			common_record				eof ;		
			DVB2FrameFormat				neonsettings ;

			uint8						nullpacket			[188] ;
volatile	int32						nullmodify ;
			uint8						sparebuff			[32 * 1024] ;	
			pthread_t					input_thread_handle ;
			pthread_t					framing_thread_handle ;
			pthread_t					output_thread_handle ;
volatile	int32						input_thread_status ;
volatile	int32						framing_thread_status ;
volatile	int32						output_thread_status ;
volatile	uint32						packetsin ;
volatile	uint32						packetsout ;
volatile	int32						nullmodcount ;
volatile	uint32						inputstarttime ;
			uint32						lastinfodisplaytime ;
			uint32						lasttcpchecktime ;
			uint32						lasttcpactivetime ;
			int32						limeband ;
			int32						limeupsample ;
			uint32						bitrateonly ;
			int							tindex ;		
			int							zindex ;		
			int							hindex ;		

			char						info				[65536] ;
			int32						localoscsettings 	[3] [32] [2] ;			// 3 bands, 31 power levels, I and Q

			char						infoip				[256] ;
			char						inputfilename 		[256] ;
			char						outputfilename 		[256] ;
			char						muntjacpicoversion	[256] ;
			char						muntjacpicoserial	[256] ;

#define MAXRECEIVEPACKETS	2400		// buffer this many received packets
#define MAXMJINFOS			64

volatile	char						mjinforeceived		[MAXMJINFOS] [256] ;
volatile	uint32						mjinfoindexin ;
volatile	uint32						mjinfoindexout ;
volatile	int32						receivesynchronised ;
volatile	int32						receivepacketsindexin ;
volatile	int32						receivepacketsindexout ;
volatile	uint8						receivepacketsarray [MAXRECEIVEPACKETS] [188] ;		

#define MAXFRAMES			90			// buffer this many frames

			uint32						mjinbufflength ;
			char						mjinbuff [65536] ;	// for reading status from MJ
			uint8						emptyframe 		[36 * 1024] ;
			uint8						emptyframe2		[36 * 1024] ;
			uint8						nullpacketframe [36 * 1024] ;
			uint8						dummyframe 		[36 * 1024] ;

			uint8						usbtxbuff			[36 * 1024] ;
volatile	uint8						framesarray			[MAXFRAMES] [36 * 1024]  __attribute__ ((aligned (128))) ;
volatile	int32						framesindexin ;
volatile	int32						framesindexout ;

// provides a divisor from 1000

            enum                                       {SR0, SR125, SR250, SR333, SR500, SR1000} ;
			uint32						txdivisors[] = {  0,     8,     4,     3,     2,      1} ;
			uint32						txdivisor ;

#define RECEIVEBUFFSIZE					(188 * 8)

			uint8						receivebuff 		[RECEIVEBUFFSIZE] ;
			uint32						receivebufflength ;
			uint32						nullpacketsadded ;
			uint32						totalnullpacketsadded ;
			uint32						emptyframecount ;
			uint32						nullpacketframecount ;
			uint32						effectivebitsadded ;
volatile	uint32						settingsrequestcountin ;
volatile	uint32						settingsrequestcountout ;
volatile	uint32						lastsettingsrequesttime	;	
volatile	uint32						framerequestcountin ;
volatile	uint32						framerequestcountout ;
volatile	uint32						insertnullpacketcountin ;
volatile	uint32						insertnullpacketcountout ;
volatile	uint32						pdefficiency ;
volatile	uint32						realefficiency ;
volatile	uint32						packetsperframe ;
volatile	uint32						lastsdtinsertionpacket ;
volatile	uint32						lastpcrinsertionpacket ;
volatile	uint32						packetspersecond ;
volatile	uint32						sdtinsertflag ;
volatile	uint64						currentpcr ;
volatile	uint32						sdtcontinuity ;
volatile	uint32						pcrcontinuity ;
volatile	uint32						tdtcontinuity ;
volatile	uint32						lastnullinsertionpacket ;
volatile	uint32						lastpcrinsertionpacket ;
volatile	uint32						lasttdtinsertionpacket ;
volatile	uint32						pdbitrate ;
volatile	uint32						realbitrate ;
volatile	uint32						incomingtdtpacketcount ;
volatile	uint32						incomingsdtpacketcount ;
volatile	uint32						partfilledframe ;
volatile	uint32						holdoffactive ;
volatile	uint32						mjerrorcount ;
volatile	uint32						dropvideo ;
volatile	uint32						dropaudio ;
volatile	int32						transvertotherfreq ;
volatile	int32						transvertmultiplier ;
volatile	int32						packetsinframe ;
volatile	uint32						nullpacketsinsertedcount ;
volatile	uint32						nullpacketsdeletedcount ;
volatile	uint32						chinese ;
			int32						mixerpower ;

    		uint16 						udpinport ; 
    		int32						insock ; 
    		struct sockaddr_in 			insockaddr ; 
			socklen_t					insocklength ;

const 		uint32          			daysinmonth [] = {0,31,28,31,30,31,30,31,31,30,31,30,31} ;
        	uint32              		DAY0           = 0xc957 ;   // 31 December 1999
            struct tm           timex ;
            struct tm           *timexp ;
            time_t              currenttime ;
            uint32              timeinsert ;


volatile	int64		firstrealtime		= 0 ;
volatile	int64		lastrealtime		= 0 ;
volatile	int64		lastpcr 			= 0 ;
volatile 	int64		firstpcr 			= 0 ;
volatile 	int64		firstvpts 			= 0 ;
volatile 	int64		lastvpts 			= 0 ;
volatile 	int64		firstapts 			= 0 ;
volatile 	int64		lastapts 			= 0 ;
volatile 	int32		firstpacket 		= 0 ;


// function prototypes

extern  	int32   	dvbs2neon_control	(uint32, uint32, uint32, uint32) ;
extern  	int32   	dvbs2neon_packet    (uint32, uint32, uint32) ;
			uint32		monotime_ms			(void) ;
			void		sig_handler			(int32) ;
			void*		receive_routine		(void*) ;
			void*		framing_routine		(void*) ;
			void*		transmit_routine	(void*) ;
			void		netprint			(char*) ;
			void		display_help		(void) ;
			void		myexit				(void) ;
			uint8* 		modifyandstorepacket	(uint8*) ;
			uint8* 		modifyandstorepacket2	(uint8*) ;
			uint32 		reverse 			(uint32) ;
			uint32 		calculateCRC32 		(uint8*, uint32) ;
        	uint32  	juliandate          (struct tm*) ;
			void 		sdt_setup			(void) ;         	
         					
int main (int argc, char *argv[])
{
	int					argindex ;
	uint32				symbolrate ;
	int32				status ;
	int32				x ;
	int32				y ;
	uint32				tempu ;
	uint32				utemp ;
	int64				templ ;
	int32				tempiloc ;
	int32				tempqloc ;
	int32				temp ;
	char				temps				[4096] ;
	char				temps2				[512] ;
	int					ch ;
	char				*pos ;
	char				*pos2 ;
	int					errors ;
	int					mainband ;		
	int					otherband ;		
	FILE				*ip ;
	int32				p0, p1, p2, p3 ;
	uint32				now ;

    terminate = 0 ;
    signal (SIGINT,  sig_handler) ;
    signal (SIGTERM, sig_handler) ;
    signal (SIGHUP,  sig_handler) ;
    signal (SIGPIPE, sig_handler) ;

/*
	system ("cat /home/pi/videots > /home/pi/videots.ts") ;
	exit (0) ;
*/	

	tindex					= 0 ;
	zindex					= 0 ;
	hindex					= 0 ;
	returncode				= 0 ;
	symbolrate 				= 0 ;
	bitrateonly				= 0 ;
	infoport				= 0 ;
	framecount				= 0 ;
	packetsin 				= 0 ;
	packetsout				= 0 ;
	nullmodcount			= 0 ;
	bytesout 				= 0 ;
	nullmodify				= 0 ;
	input_thread_handle		= 0 ;
	output_thread_handle	= 0 ;
	framing_thread_handle	= 0 ;
	input_thread_status		= 0 ;
	framing_thread_status	= 0 ;
	output_thread_status	= 0 ;
	limeband				= -1 ;
	limeupsample			= -1 ;
	mainband				= -1 ;		
	otherband				= -1 ;
	mjfd 					= -1 ;
	mjfd2 					= -1 ;
	fhin 					= -1 ;
	fh						= -1 ;
	receivesynchronised		= 0 ;
	receivepacketsindexin	= 0 ;
	receivepacketsindexout	= 0 ;
	receivebufflength		= 0 ;	
	nullpacketsadded		= 0 ;
	totalnullpacketsadded	= 0 ;
	mjinbufflength 			= 0 ;
	emptyframecount			= 0 ;
	nullpacketframecount	= 0 ;
	effectivebitsadded		= 0 ;
	framerequestcountin		= 0 ;
	framerequestcountout	= 0 ;
	settingsrequestcountin	= 0 ;
	settingsrequestcountout	= 0 ;
	lastsettingsrequesttime	= 0 ;
	insertnullpacketcountin = 0 ;
	insertnullpacketcountout = 0 ;
	pdefficiency 			= 0 ;
	realefficiency			= 0 ;
	pdbitrate				= 0 ;
	realbitrate				= 0 ;
	packetsperframe 		= 0 ;
	lastsdtinsertionpacket 	= 0 ;
	lastpcrinsertionpacket 	= 0 ;
	packetspersecond 		= 0 ;
	sdtinsertflag			= 1 ;
	currentpcr				= 0 ;
	sdtcontinuity			= 0 ;
	pcrcontinuity			= 0 ;
	lastnullinsertionpacket	= 0 ;
	lasttdtinsertionpacket	= 0 ;
	txdivisor				= 0 ;
	incomingtdtpacketcount	= 0 ;
	incomingsdtpacketcount	= 0 ;
	partfilledframe 		= 0 ;
	mjinfoindexin			= 0 ;
	mjinfoindexout			= 0 ;
	holdoffactive			= 0 ;
	mjerrorcount			= 0 ;
	dropvideo				= 0 ;
	dropaudio				= 0 ;
	firstrealtime			= 0 ;
	lastrealtime			= 0 ;
	udpinport				= DEFAULTUDPINPORT ;
	packetsinframe			= 0 ;
	nullpacketsinsertedcount = 0 ;
	nullpacketsdeletedcount  = 0 ;
	chinese					= 0 ; 

	mixerpower 				= 26 ;

	memset ((void*) &mjinforeceived, 0, sizeof(mjinforeceived)) ;			 
	memset (&savedpcrpacket, 	0, sizeof(savedpcrpacket)) ;			 
	memset (&neonsettings, 		0, sizeof(neonsettings)) ;				
	memset (callsign, 			0, sizeof(callsign)) ;				
	memset (provider, 			0, sizeof(provider)) ;				
	memset (callsign2, 			0, sizeof(callsign2)) ;				
	memset (provider2, 			0, sizeof(provider2)) ;				
	memset (modeinput, 			0, sizeof(modeinput)) ;				
	memset (&localoscsettings, 	0xff, sizeof(localoscsettings)) ;				

	mjsettings.iloc[0]			= -1 ;
	mjsettings.qloc[0]			= -1 ;
	mjsettings.iloc[1]			= -1 ;
	mjsettings.qloc[1]			= -1 ;
	mjsettings.frametype		= -1 ;
	mjsettings.dvbtype			= -1 ;
	mjsettings.constellation	= -1 ;
	mjsettings.fec 				= -1 ;
	mjsettings.pilots			= -1 ;
	mjsettings.power[0]			= -1 ;
	mjsettings.power[1]			= -1 ;
	returncode 					= 0 ;
	errors 						= 0 ;
	transvertotherfreq			= 0 ;
	transvertmultiplier			= 0 ;

	strcpy (inputfilename, 	"") ;
	strcpy (outputfilename,	"") ;
	strcpy (infoip, 		"") ;
	strcpy (info, 			"") ;
	strcpy (title, 			"") ;
	
	memset (nullpacket, 0x00, sizeof(nullpacket)) ;
	nullpacket [0] = 0x47 ;
	nullpacket [1] = 0x1f ;
	nullpacket [2] = 0xff ;
	nullpacket [3] = 0x10 ;

	srand (time (NULL)) ;

// debug output

	strcpy (infoip, "127.0.0.1 9979") ; 

// create the title bar

	sprintf (title, "muntjacsdr_dvb-%s: reads a transport stream and sends digital symbols to a Muntjac", VERSIONX2) ;
	memset (temps2, 0, sizeof(temps2)) ;
	memset (temps2, '=', strlen(title)) ;
	sprintf (info+strlen(info), "\r\n%s\r\n%s\r\n%s\r\n", temps2, title, temps2) ;

// find the frequency parameter, as that needs to be known first to select lowband or highband
// print the command line

	sprintf (info+strlen(info), "Command line: ") ;

	for (argindex = 0 ; argindex < argc ; argindex++)
	{
		sprintf (info+strlen(info), "%s ", argv[argindex]) ;
		if (strcmp(argv[argindex], "-t") == 0)
		{
			if (tindex == 0)
			{
				tindex = argindex ;
			}
		}
		if (strcmp(argv[argindex], "-z") == 0)
		{
			if (zindex == 0)
			{
				zindex = argindex ;
			}
		}
		if (strcmp(argv[argindex], "-h") == 0)
		{
			if (hindex == 0)
			{
				hindex = argindex ;
			}
		}
	}

	sprintf (info+strlen(info), "\r\n") ;
	

 // if -h for help is specified, no need to carry on

	if (hindex)
	{
		strcpy (info, "") ;
		display_help() ;
		returncode = 0 ;
		myexit() ;
	}

// check for a frequency found
	
	if ((tindex == 0) || (zindex && (tindex > zindex)) && hindex == 0)
	{
		sprintf (info+strlen(info), "*Main frequency not set* \r\n") ;
		returncode |= ERROR_FREQUENCY ;
		myexit() ;
	}


// parse the input parameters

// process the frequency parameter first

	if (tindex)
	{
		argindex = tindex ;				// position as found above
	}
	else
	{
		argindex = 1 ;					// not found, or first parameter
	}

	while (argindex < argc)
	{
		if (strcmp (argv[argindex], "-t") == 0)
		{
			tempu = (uint32) strtod (argv [argindex + 1], 0) ;

			if (argindex == tindex)											// this is the first time the not-first-parameter frequency has been seen
			{
				tindex = -tindex ;											// next time -t is seen in sequence, it will be ignored
				argindex = -1 ;												// continue from first parameter
			}
			else if (argindex == -tindex)									// already processed, so skip it
			{
				argindex += 2 ;												// skip over the '-t xxx' that was processed first
				continue ;
			}
			
			argindex++ ;													// skip over the frequency parameter, as it was processed above

			if ((tempu >= 389.5e6 && tempu <= 510e6) || (tempu >= 770e6 && tempu <= 1030e6))
			{
				if (mainband == -1)
				{
					mainband = 0 ;
					mjsettings.frequency [0] = tempu ;
					mjsettings.txon		 [0] = 0xc5 ;						// must be set to this value to transmit				
				}
				else if (otherband == -1)
				{
					sprintf (info+strlen(info), "*Frequency set twice (%.6f)*\r\n", (double) tempu / 1000000) ;
					returncode |= ERROR_FREQUENCY ;						
					continue ;
				}
				else 
				{
					if (mjsettings.frequency[0] == 0)
					{					
						mjsettings.frequency [0] = tempu ;
						mjsettings.txon		 [0] = 0xc5 ;						// must be set to this value to transmit				
					}					
					else
					{
						sprintf (info+strlen(info), "*Same band used twice* \r\n") ;
						returncode |= ERROR_SECONDARYFREQ ;						
						continue ;
					}					
				}
			}	
			else if 
			(
				(tempu >= 3400e6 && tempu <= 3410.001e6) || 
				(tempu >= 2390e6 && tempu <= 2490.001e6) || 
				(tempu >= 1300e6 && tempu <= 1305.001e6) ||
				(tempu >= 1270e6 && tempu <= 1280.001e6) ||
				(tempu >= 1244e6 && tempu <= 1250.001e6) ||
				(tempu >= 144e6 && tempu <= 147.001e6)   ||
				(tempu >= 70.5e6 && tempu <= 71.501e6)   ||
				(tempu >= 50e6 && tempu <= 54.001e6)     ||  
				(tempu >= 28e6 && tempu <= 30.001e6)
			)
			{
				if (tempu >= 28e6 && tempu <= 30.001e6)
				{
					transvertotherfreq 	= 819e6 ;
					transvertmultiplier = -3 ;
					tempu 				= tempu - transvertmultiplier * transvertotherfreq ;
				}
				else if (tempu >= 50e6 && tempu <= 54.001e6)
				{
					transvertotherfreq 	= 812e6 ;
					transvertmultiplier = -3 ;
					tempu 				= tempu - transvertmultiplier * transvertotherfreq ;
				}
				else if (tempu >= 70.5e6 && tempu <= 71.501e6)
				{
					transvertotherfreq 	= 805e6 ;
					transvertmultiplier = -3 ;
					tempu 				= tempu - transvertmultiplier * transvertotherfreq ;
				}
				else if (tempu >= 144e6 && tempu <= 147.001e6)
				{
					transvertotherfreq 	= 780e6 ;
					transvertmultiplier = -3 ;
					tempu 				= tempu - transvertmultiplier * transvertotherfreq ;
				}
				else if (tempu >= 1244e6 && tempu <= 1250.001e6)
				{
					transvertotherfreq 	= 390e6 ;
					transvertmultiplier = -3 ;
					tempu 				= tempu - transvertmultiplier * transvertotherfreq ;
				}
				else if (tempu >= 1270e6 && tempu <= 1280.001e6)
				{
					transvertotherfreq 	= 404e6 ;
					transvertmultiplier = -3 ;
					tempu 				= tempu - transvertmultiplier * transvertotherfreq ;
				}
				else if (tempu >= 1300e6 && tempu <= 1305.001e6)
				{
					transvertotherfreq 	= 395e6 ;
					transvertmultiplier = -3 ;
					tempu 				= tempu - transvertmultiplier * transvertotherfreq ;
				}
				else if (tempu >= 3400e6 && tempu <= 3410.001e6)
				{
					transvertotherfreq 	= 1010e6 ;
					transvertmultiplier = 1 ;
					tempu 				= tempu - transvertmultiplier * transvertotherfreq ;
				}
			
				if (mainband == -1)
				{
					mainband = 1 ;
					mjsettings.frequency [1] = tempu ;
					mjsettings.txon		 [1] = 0xc5 ;						// must be set to this value to transmit				
				}
				else if (otherband == -1)
				{
					sprintf (info+strlen(info), "*Frequency set twice* \r\n") ;
					returncode |= ERROR_FREQUENCY ;						
				}
				else 
				{
					if (mjsettings.frequency[1] == 0)
					{					
						mjsettings.frequency [1] = tempu ;
						mjsettings.txon		 [1] = 0xc5 ;						// must be set to this value to transmit				
					}					
					else
					{
						sprintf (info+strlen(info), "*Same band used twice* \r\n") ;
						returncode |= ERROR_SECONDARYFREQ ;						
					}					
				}
			}	
			else
			{
				sprintf (info+strlen(info), "*Frequency invalid (%.6f)*\r\n", ((double) tempu) / 1000000) ;
				if (otherband == -1)
				{
					returncode |= ERROR_FREQUENCY ;						
				}
				else
				{
					returncode |= ERROR_SECONDARYFREQ ;						
				}
			}
		}
		else if (strcmp (argv[argindex], "-d") == 0)
		{
			bitrateonly = 1 ;
		}
		else if (strcmp (argv[argindex], "-s") == 0)
 		{
 			argindex++ ; 
			tempu = (uint32) strtod (argv [argindex], 0) ;
			symbolrate = tempu ;
			if (otherband != -1 && (tempu < 1 || tempu > 2))
			{
				sprintf (info+strlen(info), "*Symbol rate cannot be set for secondary* \r\n") ;
			}
			else 
			{
				nullmodify = 0 ;
				switch (tempu)
				{
					case  333000:	txdivisor = SR333  ; mjsettings.symbolrate = 333 ; nullmodify = 999 ; break ;
					case  125000:	txdivisor = SR125  ; break ;
					case  250000:	txdivisor = SR250  ; break ;
					case  333333:	txdivisor = SR333  ; break ;
					case  500000:	txdivisor = SR500  ; break ;
					case 1000000:	txdivisor = SR1000 ; break ; 	
					default:		txdivisor = SR0    ; break ;
				} ; 		

				if (txdivisor != SR0)
				{
					txdivisor = txdivisors [txdivisor] ;			// the actual divisor
					mjsettings.symbolrate = tempu / 1000 ;
				}
				else
				{
					sprintf (info+strlen(info), "*Symbol rate invalid* \r\n") ; 
					symbolrate = 0 ; 
					if (otherband != -1)
					{
						returncode |= ERROR_SECONDARYSR ; 
					}
					else
					{
						returncode |= ERROR_SYMBOLRATE ; 
					}
				}
			} 
		}
		else if (strcmp (argv[argindex], "-f") == 0)
		{
			argindex++ ;
			strcpy (temps, argv[argindex]) ;

			for (x = 0 ; x < strlen(temps) ; x++)
			{
				temps [x] = toupper (temps [x]) ;
			}

			for (x = MAXFECS - 1 ; x >= 0 ; x--)
			{
				if (strcmp (temps, fecs[x]) == 0)
				{
					break ;
				}
			}

			if (x < 0)			
			{
				if (otherband == -1)
				{
					sprintf (info+strlen(info), "*FEC invalid* \r\n") ;
					returncode |= ERROR_FEC ;
				}
				else
				{
					sprintf (info+strlen(info), "*FEC cannot be set for secondary* \r\n") ;
					returncode |= ERROR_SECONDARYFEC ;
				}
			}
			else
			{
				if (x == FEC_CARRIER)
				{
					tempu = 1 ;
				}
				else if (x == FEC_SIDEBANDS)
				{
					tempu = 2 ;
					if (otherband != -1)
					{
						if (mjsettings.carriers [mainband] == 0)
						{
							sprintf (info+strlen(info), "*Modulation and sidebands cannot be mixed* \r\n") ;					
							returncode |= ERROR_SECONDARYFEC ;
						} 
					}
				}
				else
				{
					tempu = 0 ;
					mjsettings.fec = x ;
				}

				if (tempu)
				{
					if (otherband != -1) 
					{
						mjsettings.carriers [otherband] = tempu ; 
					}
					else
					{
						mjsettings.carriers [mainband] = tempu ; 
					}
				}
			}	
		}
		else if (strcmp (argv[argindex], "-j") == 0)
		{
			argindex++ ;
			chinese = atoi (argv[argindex]) ;
		}
		else if (strcmp (argv[argindex], "-k") == 0)
		{
			argindex++ ;
			strncpy (callsign, argv[argindex], sizeof(callsign)) ;
		}
		else if (strcmp (argv[argindex], "-w") == 0)
		{
			argindex++ ;
			strncpy (provider, argv[argindex], sizeof(provider)) ;
		}
		else if (strcmp (argv[argindex], "-m") == 0)
		{
			argindex++ ;
			strcpy (temps, argv[argindex]) ;
			if (otherband != -1)
			{
				sprintf (info+strlen(info), "*Modulation cannot be set for secondary* \r\n") ;					
				returncode |= ERROR_DVB ;
			}
			else
			{
				for (y = 0 ; y < strlen(temps) ; y++)
				{
					temps [y] = toupper (temps [y]) ;
				}
				if (strcmp (temps, "DVBS2") == 0) 
				{
					mjsettings.dvbtype = M_DVBS2 ;
				}
				else
				{
					mjsettings.dvbtype = -2 ;
					returncode |= ERROR_DVB ;
				}
			}
		}
		else if (strcmp (argv[argindex], "-c") == 0)
		{
			argindex++ ;
			strcpy (temps, argv[argindex]) ;
			if (otherband != -1)
			{
				sprintf (info+strlen(info), "*Constellation cannot be set for secondary* \r\n") ;					
				returncode |= ERROR_DVB ;
			}
			else
			{
				for (y = 0 ; y < strlen(temps) ; y++)
				{
					temps [y] = toupper (temps [y]) ;
				}
				if (strcmp (temps, "QPSK") == 0) 
				{
					mjsettings.constellation = M_QPSK ;
				}
				else if (strcmp (temps, "8PSK") == 0) 
				{
					mjsettings.constellation = M_8PSK ;
				}
				else
				{
					mjsettings.constellation = -2 ;
					returncode |= ERROR_DVB ;
				}
			}
		}
		else if (strcmp (argv[argindex], "-p") == 0)
		{
			mjsettings.pilots = PILOTS_ON ;
		}
		else if (strcmp (argv[argindex], "-v") == 0)
		{
			mjsettings.frametype = FRAME_SHORT ;
		}
		else if (strcmp (argv[argindex], "-g") == 0)
		{
			argindex++ ;
			temp = (int) (atof (argv[argindex]) * 100) ;
			if (temp > 31 || temp < 0)
			{
				if (otherband != -1)
				{
					sprintf (info+strlen(info), "*Secondary power invalid* \r\n") ;
					returncode |= ERROR_SECONDARYPOWER ;
				}
				else
				{
					sprintf (info+strlen(info), "*Power invalid* \r\n") ;
					returncode |= ERROR_POWER ;
				}
			}

			if (mainband == -1)
			{
				mjsettings.power [0] = temp ;
				mjsettings.power [1] = temp ;					// put into both bands
			}
			else if (otherband != -1)
			{
				mjsettings.power [otherband] = temp ;					
			}
			else
			{
				mjsettings.power [mainband] = temp ;					
			}
		}
		else if (strcmp (argv[argindex], "-i") == 0)							// input file
		{
			argindex++ ;
			memset (inputfilename, 0, sizeof(inputfilename)) ;
			strncpy (inputfilename, argv[argindex], sizeof (inputfilename) - 1) ;
		}
		else if (strcmp (argv[argindex], "-o") == 0)							// output file
		{
			argindex++ ;
			memset (outputfilename, 0, sizeof(outputfilename)) ;
			strncpy (outputfilename, argv[argindex], sizeof (outputfilename) - 1) ;
		}
        else if (strcmp (argv[argindex], "-y") == 0)                           // set LO suppression
        {
            argindex++ ;

            temp   = atoi (argv[argindex]) ;
            tempiloc = temp / 100 ;
            tempqloc = temp % 100 ;

            temp   = atoi (argv[argindex]) ;
            tempiloc = temp / 100 ;
            tempqloc = temp % 100 ;
            sprintf (temps, "%04d\r\n", temp) ;

            if
            (
                   temp < 0
                || strlen(argv[argindex]) != 4
                || strncmp(argv[argindex], temps, 4)
                || (tempiloc > 63 && tempiloc != 99)
                || (tempqloc > 63 && tempqloc != 99)
            )
            {
                tempiloc = -2 ;
                tempqloc = -2 ;
                sprintf (info+strlen(info), "*LO settings error* \r\n") ;
                returncode |= ERROR_LO ;
            }

            if (mainband == -1)
            {
                mjsettings.iloc [0] = tempiloc ;
                mjsettings.qloc [0] = tempqloc ;
                mjsettings.iloc [1] = tempiloc ;
                mjsettings.qloc [1] = tempqloc ;
            }
            else if (otherband != -1)
            {
                mjsettings.iloc [otherband] = tempiloc ;
                mjsettings.qloc [otherband] = tempqloc ;
            }
            else
            {
                mjsettings.iloc [mainband] = tempiloc ;
                mjsettings.qloc [mainband] = tempqloc ;
            }
        }
		else if (strcmp (argv[argindex], "-z") == 0)							// select secondary band
		{
			if (mainband == -1)
			{
				sprintf (info+strlen(info), "*Frequency not set - parsing stopped* \r\n") ;
				returncode |= ERROR_FREQUENCY ;
				myexit() ;
			}
			else
			{
				otherband = mainband ^ 1 ;
			}
		}
		else if (strcmp (argv[argindex], "-n") == 0)							// set nullpacket add / remove modulus
		{
			argindex++ ;
			temp = atoi (argv[argindex]) ;
			nullmodify = temp ;
		} 
		else if (strcmp (argv[argindex], "-x") == 0)							
		{																
			argindex++ ;
			temp = (int) (atof (argv[argindex]) * 100) ;
			if (temp > 31 || temp < 0)
			{
				sprintf (info+strlen(info), "*Mixer power invalid* \r\n") ;
				returncode |= ERROR_POWER ;
			}
			else
			{
				mixerpower = temp ;
			}
		}
		else if (strcmp (argv[argindex], "-e") == 0)							
		{																
			argindex++ ;
			limeband = atoi (argv[argindex]) ;		
		}
		else if (strcmp (argv[argindex], "-r") == 0)							
		{																
			argindex++ ;
			limeupsample = atoi (argv[argindex]) ;
		}
		else if (strcmp (argv[argindex], "-D") == 0)							
		{																
			argindex++ ;
		}	
		else if (strcmp (argv[argindex], "-q") == 0)							
		{																
			argindex++ ;
		}	
		else if (strcmp (argv[argindex], "-u") == 0)							// 'UDP debug output (default 127.0.0.1:9979)
		{
			argindex++ ;
			memset (infoip, 0, sizeof(infoip)) ;
			strncpy (infoip, argv[argindex], sizeof (infoip) - 1) ;
			pos = strchr (infoip, ':') ;
			if (pos)
			{
				*pos = ' ' ;
			}
		}
		else
		{
			sprintf (info+strlen(info), "*Unknown parameter (%s)*\r\n", argv[argindex]) ;
			returncode |= ERROR_PARAMETER ; 
			if (hindex == 0)
			{
				myexit() ;
			}
		}
		argindex++ ;
	} ;

// check parameters

// add 2 to power for 8PSK for similar level output

	if (mjsettings.symbolrate && mjsettings.constellation == M_8PSK)
	{
		for (x = 0 ; x < 2 ; x++)
		{
			if (mjsettings.txon[x] == 0xc5 && mjsettings.carriers[x] == 0)
			{
				mjsettings.power[x] += 2 ;
				if (mjsettings.power[x] > 31)
				{
					mjsettings.power[x] = 31 ;
				}
				sprintf (info+strlen(info), "Power increased to %d for 8PSK equality\r\n", mjsettings.power[x]) ;
			}
		}
	}

// carrier

	if (mjsettings.carriers [mainband])
	{
		mjsettings.symbolrate = 0 ;
	}
	else
	{
		if (mjsettings.fec == -1)
		{
			if ((returncode & ERROR_FEC) == 0)
			{
				sprintf (info+strlen(info), "*FEC not set* \r\n") ;
				returncode |= ERROR_FEC ;
			}
		}
		if (mjsettings.symbolrate == 0)
		{
			sprintf (info+strlen(info), "*Symbol rate not set* \r\n") ;
			returncode |= ERROR_SYMBOLRATE ;
		}
	}

	if (mjsettings.symbolrate == 1000 && mjsettings.constellation == M_8PSK)
	{
		sprintf (info+strlen(info), "*8PSK at SR1000 is not supported* \r\n") ;
		returncode |= ERROR_DVB ;
	}

	if (mainband != -1)
	{
		if (mjsettings.power[mainband] == -1)
		{
			sprintf (info+strlen(info), "*Power not set* \r\n") ;
			returncode |= ERROR_POWER ;
		}
		else if (mjsettings.power[mainband] == -1)
		{
			sprintf (info+strlen(info), "*Power invalid* \r\n") ;
			returncode |= ERROR_POWER ;
		}
	}
	else if (mjsettings.frequency[0] == 1)
	{
		sprintf (info+strlen(info), "*Frequency invalid* \r\n") ;
		returncode |= ERROR_FREQUENCY ;
	}
	else if (mjsettings.frequency[0] == 0)
	{
		sprintf (info+strlen(info), "*Frequency not set* \r\n") ;
		returncode |= ERROR_FREQUENCY ;
		myexit() ;
	}

/*
	if (mjsettings.txon[0] == 0xc5)
	{
		if (mjsettings.carriers[0] != 1 && mjsettings.power[0] > 31)
		{
			mjsettings.power[0] = 31 ;
			sprintf (info+strlen(info), "*Lowband power capped at 0.31* \r\n") ;
		}
		else if (mjsettings.power[0] > 31)
		{
			mjsettings.power[0] = 31 ;
			sprintf (info+strlen(info), "*Lowband power capped at 0.31* \r\n") ;
		}
	}
*/

	if (mjsettings.txon[1] == 0xc5)
	{
		if (mjsettings.power[1] > 31)
		{
			mjsettings.power[1] = 31 ;
			sprintf (info+strlen(info), "*Highband power capped at 0.31* \r\n") ;
		}
	}

	if (otherband != -1)
	{
		if (mjsettings.power[otherband] == -1)
		{
			sprintf (info+strlen(info), "*Secondary power not set* \r\n") ;
			returncode |= ERROR_SECONDARYPOWER ;
		}

/*
		else if (mjsettings.power[otherband] == -1)
		{
			sprintf (info+strlen(info), "*Secondary power invalid* \r\n") ;
			returncode |= ERROR_SECONDARYPOWER ;
		}
*/
		
		if (mjsettings.frequency[otherband] == 0)
		{
			sprintf (info+strlen(info), "*Secondary frequency not set* \r\n") ;
			returncode |= ERROR_SECONDARYFREQ ;
		}
		else if (mjsettings.frequency[otherband] == 1)
		{
			sprintf (info+strlen(info), "*Secondary frequency invalid (%.6f)*\r\n", (double) (mjsettings.frequency[otherband]) / 1000000) ;
			returncode |= ERROR_SECONDARYFREQ ;
		}
	}

#if TEST
	if (otherband == -1)
	{
		otherband = mainband ^ 1 ;
		mjsettings.power[otherband] = 10 ;
		mjsettings.frequency[otherband] = 437000000 ;
		mjsettings.txon[otherband] = 0xc5 ;
	}
#endif

	if (returncode)
	{
		myexit() ;
	}

// check for defaults

	if (inputfilename[0] == 0)
	{
		strcpy (inputfilename, "/dev/stdin") ;								// default
		sprintf (info+strlen(info), "Default: -i %s  \r\n", inputfilename) ;
	}

	if (outputfilename[0] == 0)
	{
		strcpy (outputfilename, "/dev/ttyMJ0") ;
		sprintf (info+strlen(info), "Default: -o %s  \r\n", outputfilename) ;			// default
	}

	if (mjsettings.dvbtype == -1)								// no parameter set
	{
		sprintf (info+strlen(info), "Default: -m M_DVBS2\r\n") ;			
		mjsettings.dvbtype = M_DVBS2 ;							// default to DVB-S2
	}
	else if (mjsettings.dvbtype == -2)
	{
		sprintf (info+strlen(info), "*DVB mode invalid*\r\n") ;					
		returncode |= ERROR_DVB ;
	}

	if (mjsettings.constellation == -1)								// no parameter set
	{
		sprintf (info+strlen(info), "Default: -c M_QPSK\r\n") ;
		mjsettings.constellation = M_QPSK ;							// default to qpsk
	}
	else if (mjsettings.constellation == -2)
	{
		sprintf (info+strlen(info), "*Constellation invalid*\r\n") ;
		returncode |= ERROR_DVB ;
	}

// Setting transverter band to 76GHz will select alternating sideband mode

	if (limeband == LIMEBAND_76GHZ)
	{
		mjsettings.symbolrate = 0 ;
		mjsettings.carriers [mainband] = 2 ;
	}

	tempu = mjsettings.frequency [mainband] ;

// debug output

	strcpy (temps, "") ;
	
	if (mjsettings.frametype == -1)
	{
		mjsettings.frametype = 0 ;
		sprintf (info+strlen(info), "Default: Frame=LONG\r\n") ;
	}

	if (mjsettings.pilots == -1)
	{
		mjsettings.pilots = 0 ;
		sprintf (info+strlen(info), "Default: Pilots=OFF\r\n") ;
	}

	if 
	(
		   (mainband  != -1 && mjsettings.carriers[mainband]  == 2) 
		|| (otherband != -1 && mjsettings.carriers[otherband] == 2) 
	)
	{
		sprintf (info+strlen(info), "Default: Constellation=QPSK") ;
		mjsettings.constellation = M_QPSK ;
	}


// select mode from frequency if mode not set

/*
	if (chinese == 0)
	{
		utemp = mjsettings.frequency[mainband] % 10000 ;
		if (utemp == 1000)
		{
			chinese = 1 ;
		}
		netprint (temps) ;
	}
*/


// get settings from /home/pi/rpidatv/scripts/portsdown_config.txt if none set

///	if (chinese & 1)
	{
		fh = open ("/home/pi/rpidatv/scripts/portsdown_config.txt", O_RDONLY) ;
		if (fh > 0)
		{
			memset (sparebuff, 0, sizeof(sparebuff)) ;
			read (fh, sparebuff, sizeof(sparebuff) - 1) ;
		}
		close (fh) ;

		if (callsign[0] == 0)
		{
			strcpy (temps, "call=") ;
			pos = strstr (sparebuff, temps) ;
			if (pos)
			{
				pos += strlen(temps) ;
				pos2 = strchr (pos, '\n') ;
				if (pos2)
				{
					*pos2 = 0 ;
					strncpy (callsign, pos, sizeof(callsign) - 1) ;
				}
			}
		}
		strcpy (temps, "udpinport=") ;
		pos = strstr (sparebuff, temps) ;
		if (pos)
		{
			pos += strlen(temps) ;
			pos2 = strchr (pos, '\n') ;
			if (pos2)
			{
				*pos2 = 0 ;
				udpinport = atoi (pos) ;
			}
		}
		strcpy (temps, "modeinput=") ;
		pos = strstr (sparebuff, "modeinput=") ;
		if (pos)
		{
			pos += strlen(temps) ;
			pos2 = strchr (pos, '\n') ;
			if (pos2)
			{
				*pos2 = 0 ;
				strncpy (modeinput, pos, sizeof(modeinput) - 1) ;
			}
		}

		if (strstr (modeinput,"CARDH264") || strstr (modeinput,"CONTEST") || strstr (modeinput,"DESKTOP"))
		{
///			chinese &= ~1 ;
		}
	}

	if (chinese & 1)
	{
		if (strstr(inputfilename, ".ts"))
		{
			chinese &= ~1 ;
		}
	}

	
// copy the settings for dvbs2neon

	neonsettings.frametype 			= mjsettings.frametype ;
	neonsettings.fec 				= mjsettings.fec ;
	neonsettings.rolloff 			= mjsettings.rolloff ;
	neonsettings.constellation 		= mjsettings.constellation ;
	neonsettings.pilots 			= mjsettings.pilots ;

///	neonsettings.magic				= MAGIC_MARKER ;							// enable extra parameters
///	neonsettings.datafield_size		= 1 ;


/*
	neonsettings.magic				= 0x7388c542 ;							// enable extra parameters
	if (chinese & 1)
	{
		utemp = 1 ;
	}
	else
	{
		utemp = 0 ;
	}
	neonsettings.datafield_size		= utemp ;	
*/


// set up DATV settings record

	mjsettings.special 					= ESCAPE ;							// signify an embedded record
	mjsettings.recordtype				= DATV_RECORD ;						// record type
	mjsettings.bytesfollowing 			= sizeof (mjsettings) - 3 ;
	mjsettings.dataversion				= 1 ;									
	mjsettings.constellation 	 		= neonsettings.constellation ;
	mjsettings.rolloff		 	 		= neonsettings.rolloff ;
	mjsettings.magicmarker		[0]		= 'M' ;				
	mjsettings.magicmarker		[1]		= 'J' ;				
	mjsettings.magicmarker		[2]		= 'A' ;				
	mjsettings.magicmarker		[3]		= 'C' ; 				
	mjsettings.crc						= 0 ;									// not used yet	

// set up status request record

	statusrequest.special 				= ESCAPE ;								// signify an embedded record
	statusrequest.recordtype			= STATUSREQUEST_RECORD ;				// record type
	statusrequest.bytesfollowing 		= sizeof (statusrequest) - 3 ;	
	statusrequest.spare0				= 0 ;
	statusrequest.spare1				= 0 ;
	statusrequest.magicmarker	[0]		= 'M' ;				
	statusrequest.magicmarker	[1]		= 'J' ;				
	statusrequest.magicmarker	[2]		= 'A' ;				
	statusrequest.magicmarker	[3]		= 'C' ; 				
	statusrequest.crc					= 0 ;									// not used yet	

// set up begin frame record

	beginframe.special 					= ESCAPE ;								// signify an embedded record
	beginframe.recordtype				= BEGINFRAME_RECORD ;					// record type
	beginframe.bytesfollowing 			= sizeof (beginframe) - 3 ;	
	beginframe.spare0					= 0 ;
	beginframe.spare1					= 0 ;
	beginframe.magicmarker	[0]			= 'M' ;				
	beginframe.magicmarker	[1]			= 'J' ;				
	beginframe.magicmarker	[2]			= 'A' ;				
	beginframe.magicmarker	[3]			= 'C' ; 				
	beginframe.crc						= 0 ;									// not used yet	

// set up end frame record

	endframe.special 					= ESCAPE ;								// signify an embedded record
	endframe.recordtype					= ENDFRAME_RECORD ;						// record type
	endframe.bytesfollowing	 			= sizeof (endframe) - 3 ;	
	endframe.spare0						= 0 ;
	endframe.spare1						= 0 ;
	endframe.magicmarker	[0]			= 'M' ;				
	endframe.magicmarker	[1]			= 'J' ;				
	endframe.magicmarker	[2]			= 'A' ;				
	endframe.magicmarker	[3]			= 'C' ; 				
	endframe.crc						= 0 ;									// not used yet	

// set up EOF record

	eof.special 						= ESCAPE ;								// signify an embedded record
	eof.recordtype						= EOF_RECORD ;							// record type
	eof.bytesfollowing	 				= sizeof (eof) - 3 ;	
	eof.spare0							= 0 ;
	eof.spare1							= 0 ;
	eof.magicmarker	[0]					= 'M' ;				
	eof.magicmarker	[1]					= 'J' ;				
	eof.magicmarker	[2]					= 'A' ;				
	eof.magicmarker	[3]					= 'C' ; 				
	eof.crc								= 0 ;									// not used yet	

// set up TDT packet

    memset (&tdt, 0, sizeof(tdt)) ;
    tdt.sync            = 0x47 ;
    tdt.payloadstart    = 1 ;
    tdt.adaption        = 1 ;
    tdt.pid1208         = TDTPID >> 8 ;
    tdt.pid0700         = TDTPID & 0xff ;
    tdt.tableid         = 0x70 ;
    tdt.syntax          = 0 ;
    tdt.reserved0       = 3 ;
    tdt.sectionlength   = 5 ;
    memset (&tdt.filler2, 0xff, sizeof(tdt.filler2)) ;
	timeinsert			= TIMEUTC ;

///
	
	if (mjsettings.symbolrate)
	{

// reset dvbs2neon completely

		status = dvbs2neon_control (0, CONTROL_RESET_FULL, 0, 0) ;				// framing routine will set buffers   
		if (status < 0)
		{
			sprintf (info+strlen(info), "dvbs2neon error1 (%d)\r\n", status) ;
			returncode &= 0x00ffffff ;
			returncode |= abs (status) << 24 ;
			myexit() ;
		}


// reset the dvbs2neon stream that we will use

		status = dvbs2neon_control (STREAM0, CONTROL_RESET_STREAM, 0, 0) ;
		if (status < 0)
		{
			sprintf (info+strlen(info), "*dvbs2neon error2* (%d)\r\n", status) ;
			returncode &= 0x00ffffff ;
			returncode |= abs (status) << 24 ;
			myexit() ;
			
		}


// set the dvbs2neon parameters

		status = dvbs2neon_control (STREAM0, CONTROL_SET_PARAMETERS, (uint32) &neonsettings, 0) ;           		// set parameters
		if (status < 0)
		{
			sprintf (info+strlen(info), "*dvbs2neon error3* (%d)\r\n", status) ;
			returncode &= 0x00ffffff ;
			returncode |= abs (status) << 24 ;
			myexit() ;
		}

// get an empty frame and a dummy frame

		status = dvbs2neon_control (STREAM0, CONTROL_SET_OUTPUT_BUFFER, (uint32) &emptyframe2, 0) ;			// set buffer address
		for (x = 0 ; x < 4 ; x++)
		{
			status = dvbs2neon_packet  (STREAM0, (uint32)&nullpacket, 0) ;			
		}
		status = dvbs2neon_packet  (STREAM0, 0, 1) ;	// parameter = 1, returns a frame even if not full
		status = dvbs2neon_control (STREAM0, CONTROL_SET_OUTPUT_BUFFER, (uint32) &emptyframe, 0) ;			// set buffer address
		for (x = 0 ; x < 4 ; x++)
		{
			status = dvbs2neon_packet  (STREAM0, (uint32)&nullpacket, 0) ;			
		}
		status = dvbs2neon_packet  (STREAM0, 0, 1) ;	// parameter = 1, returns a frame even if not full
		status = dvbs2neon_control (STREAM0, CONTROL_GET_DUMMY_FRAME,   (uint32) &dummyframe, 0) ;			// set buffer address
	}

// get efficiency to calculate input bitrate

	if (symbolrate)
	{
		status = dvbs2neon_control (STREAM0, CONTROL_GET_EFFICIENCY, 0, 0) ;           
		if (status < 0)
		{
			sprintf (info+strlen(info), "*dvbs2neon error3* (%d)\r\n", status) ;
			returncode &= 0x00ffffff ;
			returncode |= abs (status) << 24 ;
			myexit() ;
		}
		else
		{
			realefficiency = status ;
			if (symbolrate == 333000)
			{
				pdefficiency = (double) realefficiency * 333000.000 / 333333.333 ;
			}
			else
			{
				pdefficiency = realefficiency ;
			}
			tempu 			= dvbs2neon_control (STREAM0, CONTROL_GET_DATAFIELD_SIZE, 0, 0) ;  
			packetsperframe = tempu / 188 ;
		}
		
		if (returncode == 0)
		{
			sprintf (info+strlen(info), "Efficiency: %d\r\n", pdefficiency) ;		// TS bit/s throughput for SR1000

			pdbitrate   = (uint64) pdefficiency   / txdivisor ;					// TS bit/s throughput for the chosen symbolrate
			realbitrate = (uint64) realefficiency / txdivisor ;		// TS bit/s throughput for the chosen symbolrate
			sprintf (info+strlen(info), "Bit rate: %u\r\n", pdbitrate) ;

			packetspersecond  = realbitrate / (188 * 8) ;
			sprintf (info+strlen(info), "Packets per second: %u\r\n", packetspersecond) ;
		}

		if (bitrateonly)
		{
			if ((returncode & (0xff000000 + ERROR_DVB)) == 0)
			{
				sprintf (info, "Net TS input bitrate should be %d", pdbitrate) ;
			}
			returncode = 0 ; 
			myexit() ;
		}
	}	

	netprint (info) ;
	strcpy (info, "") ;

// open the input file: if chinese:0, ignore command line input setting and use UDP port from PD settings

	errors = 0 ;
	if (chinese & 1)
	{
		sprintf (temps, "Opening UDP port %d for input", udpinport) ;
		netprint (temps) ;

	 	status = socket (AF_INET,SOCK_DGRAM,IPPROTO_UDP) ; 
    	if (status < 0) 
	    {	
	        sprintf (temps, "Cannot create socket for port %d ", udpinport) ;
			netprint (temps) ;
	    }
	    else
	    {
			insock = status ;
	    }

		insocklength = sizeof (insockaddr) ;
	    status = 1 ; 	
	    status = setsockopt (insock, SOL_SOCKET, SO_REUSEADDR, (char*)&status, sizeof(status)) ;
	    if (status < 0)
	    {
	        sprintf (temps, "Cannot re-use socket for port %d \r\n",udpinport) ;		
	        netprint (temps) ;
	    }	
	        
	    insockaddr.sin_family 			= AF_INET ; 
	    insockaddr.sin_port            	= htons (udpinport) ;
	   	insockaddr.sin_addr.s_addr		= INADDR_ANY ; 
	    memset 							((void*) &insockaddr.sin_zero, 0, sizeof(insockaddr.sin_zero)) ;         
	    status 							= bind (insock, (const struct sockaddr*) &insockaddr, sizeof(insockaddr)) ;

	 	status = socket (AF_INET,SOCK_DGRAM,IPPROTO_UDP) ; 
    	if (status < 0) 
	    {	
	        sprintf (temps, "Cannot create socket for port %d \r\n",udpinport) ;
	        netprint (temps) ;
	    }
	        
	    status = 1 ; 	
	    status = setsockopt (insock, SOL_SOCKET, SO_REUSEADDR, (char*)&status, sizeof(status)) ;
	    if (status < 0)
	    {
	        sprintf (temps, "Cannot re-use socket for port %d \r\n",udpinport) ;		
	        netprint (temps) ;
	    }	
	}

	fhin = open (inputfilename, O_RDONLY | O_NONBLOCK) ;
	if (fhin < 0)
	{
		sprintf (temps, "*Cannot open input file %s* \r\n", inputfilename) ;
		netprint(temps) ;
		returncode |= ERROR_INPUT ;
	}

    mjfd = open (outputfilename, O_RDWR | O_NONBLOCK) ;
	if (mjfd < 0) 
	{
        sprintf (temps, "*Cannot open output file %s for writing (%d)*", outputfilename, errno) ;
		netprint (temps) ;
		returncode |= ERROR_OUTPUT ;
	}
	else
	{
		status = flock (mjfd, LOCK_EX | LOCK_NB) ;		// lock the output file
    	if (status < 0)
	    {
    	    sprintf (temps, "*Cannot lock output file %s (%d)*", outputfilename, errno) ;
			netprint (temps) ;
			returncode |= ERROR_OUTPUT ;
    	}
    	else
    	{
		    if (strncmp (outputfilename, "/dev/tty", 8) == 0)		// set the parameters for output to a serial port
    		{
	    		sprintf (temps, "stty -F %s raw -echo > /dev/null",  outputfilename) ;
        		system (temps) ;
    		}

			mjfd2 = open (outputfilename, O_RDONLY | O_NONBLOCK) ;
			if (mjfd2 < 0)
			{
        		sprintf (temps, "*Cannot open output file %s for reading (%d)*", outputfilename, errno) ;
				netprint (temps) ;
	            returncode |= ERROR_OUTPUT ;
            }     

			mjfd3 = fopen (outputfilename, "r") ;
			if (mjfd3 == 0)
			{
        		sprintf (temps, "*Cannot open output file %s for reading (%d)*", outputfilename, errno) ;
				netprint (temps) ;
	            returncode |= ERROR_OUTPUT ;
            }

		}
    }

	if (returncode != 0)
	{
		myexit() ;
	}


// request Muntjac identification

	memset (muntjacpicoversion, 0, sizeof(muntjacpicoversion)) ;
	memset (muntjacpicoserial,  0, sizeof(muntjacpicoserial)) ;

	for (x = 5 ; x > 0 ; x--)
	{
		write (mjfd, (void*) &statusrequest, sizeof(statusrequest)) ;	// send the status request record
		usleep (100000) ;												// wait 100ms
		memset (temps, 0, sizeof(temps)) ;
		status = read (mjfd2, temps, sizeof(mjinbuff) - 1) ;
		pos = (strstr(temps, "[Muntjac4-")) ;
		if (pos)
		{
			pos += strlen ("[Muntjac4-") ;
			pos2 = strchr (pos, ':') ;
			if (pos2)
			{
				*pos2 = 0 ;
				strcpy (muntjacpicoversion, pos) ;
				pos = pos2 + 1 ;
				pos2 = strchr (pos, ' ') ;
				if (pos2)
				{
					*pos2 = 0 ;
					strcpy (muntjacpicoserial, pos) ;
					break ;
				}
			}
		}
	}
	if (x == 0)
	{
		netprint ("Muntjac4 not detected") ;
	}
	else
	{
		sprintf (temps, "Muntjac4 Pico firmware version: %s", muntjacpicoversion) ;
		netprint (temps) ;
		sprintf (temps, "Muntjac4 Pico serial number:    %s", muntjacpicoserial) ;
		netprint (temps) ;
		if (provider[0] == 0)
		{
			if (limeband >= 0)				// came from PD4
			{
				strcpy (provider, "PD4-") ;
			}
			else
			{
				strcpy (provider, "") ;
			}
			sprintf (provider+strlen(provider), "Muntjac4-%s-%s", muntjacpicoversion, VERSIONX2) ;
			sdt_setup() ;
			sprintf (provider, " ") ;
		}
	}
	
// apply local oscillator suppression settings from muntjac.mjo, if available

	memset (localoscsettings, 0xff, sizeof(localoscsettings)) ;						// preset all to -1

	sprintf (temps2, "/home/pi/rpidatv/bin/%s.mjo", muntjacpicoserial) ;
	ip = fopen (temps2, "r") ;
	if (ip == 0)
	{
		sprintf (temps2, "./%s.mjo", muntjacpicoserial) ;
		ip = fopen (temps2, "r") ;
	}

// set carrier on low band for transverting

	if (transvertotherfreq)
	{
		otherband 							= 0 ;
		mjsettings.frequency 	[otherband]	= transvertotherfreq ;
		mjsettings.power 		[otherband]	= mixerpower ;
		mjsettings.carriers		[otherband]	= 1 ;
		mjsettings.txon  		[otherband] = 0xc5 ;
	}

	if (ip == 0)
	{
		sprintf (temps, "%s.mjo LO suppression settings file not found", muntjacpicoserial) ;
		netprint (temps) ;
	}
	else
	{
		sprintf (temps, "Using LO suppression settings from %s", temps2) ;
		netprint (temps) ;
		while (!feof(ip)) 
		{
			fgets (temps, sizeof(temps), ip) ;		
			if (temps[0] != ' ' && temps[0] != '#')
			{
				sscanf (temps, "%u %u %u %u", &p0, &p1, &p2, &p3) ;
				if (p0 == 440)
				{
					x = 0 ;
				}
				else if (p0 == 2400) 
				{
					x = 1 ;
				}
				else if (p0 == 1300) 
				{
					x = 2 ;
				}
				else
				{
					x = -1 ;
				}
				if (x >= 0)
				{
					if (p1 >= 0 && p1 <= 31)											// power
					{
						if (p2 >= 0 && p2 <= 63)
						{
							localoscsettings [x] [p1] [0] = p2 ;						// I
						}
						if (p3 >= 0 && p3 <= 63)
						{
							localoscsettings [x] [p1] [1] = p3 ;						// Q
						}
					}
				}
			}
		}
		fclose (ip) ;

		for (x = 0 ; x < 2 ; x++)
		{
			strcmp (temps, "") ;
			if (mjsettings.iloc[x] == -1 && mjsettings.qloc[x] == -1)
			{
				if (mjsettings.frequency[x] >= 430e6 && mjsettings.frequency[x] <= 450e6)
				{
					strcpy (temps, "LOWBAND") ;
				}
				else if (mjsettings.frequency[x] >= 2390e6 && mjsettings.frequency[x] <= 2490e6)
				{
					strcpy (temps, "HIGHBAND") ;
				}
				else if (mjsettings.frequency[x] >= 413.333e6 && mjsettings.frequency[x] < 430e6)
				{
					strcpy (temps, "TRIPLING") ;
				}
				else
				{
					strcpy (temps, "") ;
				}

				if (temps[0])
				{
					if (strcmp (temps, "TRIPLING") == 0)
					{
						y = 2 ;
					}
					else
					{
						y = x ;
					}
					tempu = mjsettings.power [x] ;
					mjsettings.iloc [x] = localoscsettings [y] [tempu] [0] ;				
					mjsettings.qloc [x] = localoscsettings [y] [tempu] [1] ;				
					if (mjsettings.iloc[x] == -1 && mjsettings.qloc[x] == -1)				// in case settings are available only for power 0, 4, 8 . . .
					{
						tempu &= ~3 ;
						mjsettings.iloc [x] = localoscsettings [y] [tempu] [0] ;				
						mjsettings.qloc [x] = localoscsettings [y] [tempu] [1] ;				
					}
					netprint (temps) ;
				}
			}
		}
	}	


// if no local oscillator settings have been applied, change them to zero for AT86RF215 default

	if (mjsettings.iloc[0] == -1 && mjsettings.qloc[0] == -1)
	{
		mjsettings.iloc[0] = 99 ;
		mjsettings.qloc[0] = 99 ;
	}
	
	if (mjsettings.iloc[1] == -1 && mjsettings.qloc[1] == -1)
	{
		mjsettings.iloc[1] = 99 ;
		mjsettings.qloc[1] = 99 ;
	}

/*
	if (mjsettings.txon[0])
	{
		strcpy (temps, "") ;	
		sprintf (temps + strlen(temps), "Frequency=%.3f  ", (float) mjsettings.frequency[0] / 1000000) ;
		sprintf (temps + strlen(temps), "Gain=%d  ", mjsettings.power[0]) ;
		sprintf (temps + strlen(temps), "FEC=%d  ", neonsettings.fec) ;
		sprintf (temps + strlen(temps), "Symbolrate=%d  ", mjsettings.symbolrate) ;
		sprintf (temps + strlen(temps), "ILOC=%d  ", mjsettings.iloc[0]) ;
		sprintf (temps + strlen(temps), "QLOC=%d  ", mjsettings.qloc[0]) ;
		printf ("%s\r\n", temps) ;
	}
	if (mjsettings.txon[1])
	{
		strcpy (temps, "") ;	
		sprintf (temps + strlen(temps), "Frequency=%.3f  ", (float) mjsettings.frequency[1] / 1000000) ;
		sprintf (temps + strlen(temps), "Gain=%d  ", mjsettings.power[1]) ;
		sprintf (temps + strlen(temps), "FEC=%d  ", neonsettings.fec) ;
		sprintf (temps + strlen(temps), "Symbolrate=%d  ", mjsettings.symbolrate) ;
		sprintf (temps + strlen(temps), "ILOC=%d  ", mjsettings.iloc[1]) ;
		sprintf (temps + strlen(temps), "QLOC=%d  ", mjsettings.qloc[1]) ;
		printf ("%s\r\n", temps) ;
	}
*/

// outer main loop - restart in here if an error occurs ======================================================================================

	while (terminate == 0)
	{
///		if (fhin >= 0 && mjsettings.symbolrate)
		if (mjsettings.symbolrate)
		{
			status = pthread_create (&input_thread_handle, 0, receive_routine, 0) ; 
			if (status != 0) 
			{
				netprint ("*Cannot create input process thread*") ;
				myexit() ;
			}
			else
			{
				netprint ("Input process thread created") ;
			}

			status = pthread_create (&framing_thread_handle, 0, framing_routine, 0) ; 
			if (status != 0) 
			{
				netprint ("*Cannot create framing process thread*") ;
				myexit() ;
			}
			else
			{
				netprint ("Framing process thread created") ;
			}
		}
				
		status = pthread_create (&output_thread_handle, 0, transmit_routine, 0) ; 
		if (status != 0) 
		{
			netprint ("*Cannot create output process thread*") ;
			myexit() ;
		}
		else
		{
			netprint ("Output process thread created") ;
		}

		netprint ("Main loop is starting") ;

		lastinfodisplaytime = 0 ;
		lasttcpchecktime 	= 0 ;
		lasttcpactivetime	= 0 ;

		if (mjsettings.symbolrate)
		{
			if (chinese & 1)
			{			
				utemp = 2000000 / txdivisor / 8 / 10 ;						
				if (mjsettings.constellation)
				{
					utemp *= 2 ;			
				}
				mjsettings.bufferinginitial	= utemp ;					// 100ms
				mjsettings.bufferlowtrigger	= utemp / 4 ;				// 25ms
			}

			if (mjsettings.constellation)
			{
		 		mjsettings.bufferinginitial *= 2 ;
		 		mjsettings.bufferlowtrigger *= 2 ;
			}
			
			input_thread_status = 1 ;									// start the input thread
		
			while (terminate == 0 && returncode == 0 && receivesynchronised == 0)
			{
				usleep (1000) ;
			}

			if (terminate == 0 && returncode == 0)
			{
				framing_thread_status = 1 ;							// start the framing thread
			}
		}

// buffer 1.5s worth of frames before starting the USB output routine

		utemp = packetspersecond / packetsperframe ;				// frames per second
		utemp = utemp * 3 / 2 ;

		do
		{
			temp = framesindexin - framesindexout ;
			if (temp < 0)
			{
				temp += MAXFRAMES ;
			}
		} while (terminate == 0 && temp < utemp) ;
					
		if (terminate == 0 && returncode == 0)
		{
			output_thread_status = 1 ;
		}

		usleep (100000) ;
		printf ("\r\n") ;
		
// inner main loop starts here - exit on error or termination ====================================================

		memset (mjinbuff, 0, sizeof (mjinbuff)) ;
		
		while (terminate == 0 && returncode == 0)
		{
			usleep (1000) ;

#if TEST
			while (framecount < 1800) ;
			terminate = 1 ;
			continue ;
#endif

// look for info from MJ

			if (mjinfoindexin != mjinfoindexout)
			{	
				utemp = mjinfoindexout ;
				strcpy (temps2, (char*) mjinforeceived[utemp]) ;
				utemp++ ;
				if (utemp >= MAXMJINFOS)
				{
					utemp = 0 ;
				}
				mjinfoindexout = utemp ;

				temp = outputfilename [strlen(outputfilename)-1] ;
				sprintf (temps, " MJ%c: %s", temp, temps2) ;
				netprint (temps) ;
				if (strstr(temps2, "*]"))
				{
					mjerrorcount++ ;
					if (strstr(temps2, "*UNDERFLOW") == 0)
					{
						terminate = 0xff ;
						returncode = ERROR_MUNTJAC ;
					}
				}
			}

			if (terminate)
			{
				continue ;
			}

// check if settings need to be sent to MJ

			if (monotime_ms() - lastsettingsrequesttime >= SETTINGSREQUESTTIME)
			{
				lastsettingsrequesttime = monotime_ms() ;
				mjsettings.recordsequence++ ;					
				settingsrequestcountin++ ;
			}
			
			tempu = monotime_ms() ;
			if (tempu - lastinfodisplaytime >= 1000)
			{
				lastinfodisplaytime = tempu ;

				temp = framesindexin - framesindexout ;
				if (temp < 0) 
				{
					temp += MAXFRAMES ;
				}

				strcpy (temps, "");
				sprintf (temps+strlen(temps), " ====================================== \r\n") ;
				sprintf (temps+strlen(temps), " muntjacsdr_dvb-%s   \r\n", VERSIONX2) ;
				sprintf (temps+strlen(temps), " muntjac4_pico-%s    \r\n", muntjacpicoversion) ;
				if (chinese == 0)
				{
					sprintf (temps+strlen(temps), " In:  %s             \r\n", inputfilename) ;
				}
				else
				{
					sprintf (temps+strlen(temps), " In:  :%d            \r\n", udpinport) ;
				}
				sprintf (temps+strlen(temps), " Out: %s             \r\n", outputfilename) ;
				sprintf (temps+strlen(temps), " ------------------- \r\n") ;
				sprintf (temps+strlen(temps), " FR:%-8.3f PWR:%-2u ", (double) mjsettings.frequency [mainband] / 1000000, mjsettings.power[mainband]) ;
				{
					if (mjsettings.carriers[mainband] == 1)
					{
						sprintf (temps+strlen(temps), "Carrier  ") ;
					}
					else if (mjsettings.carriers[mainband] == 2)
					{
						sprintf (temps+strlen(temps), "Sidebands  ") ;
					}
					else
					{
						sprintf (temps+strlen(temps), "S:%u FEC:%s C:%u ", mjsettings.symbolrate, fecs[mjsettings.fec], mjsettings.constellation * 4 + 4) ;
					}
					sprintf (temps+strlen(temps), "\r\n") ;					
				}
				if (otherband >= 0)
				{
					sprintf (temps+strlen(temps), " FR:%-8.3f PWR:%-2u ", (double) mjsettings.frequency [otherband] / 1000000, mjsettings.power[otherband]) ;
					if (mjsettings.carriers[otherband] == 1)
					{
						sprintf (temps+strlen(temps), "Carrier  ") ;
					}
					else if (mjsettings.carriers[otherband] == 2)
					{
						sprintf (temps+strlen(temps), "Sidebands  ") ;
					}
					sprintf (temps+strlen(temps), "\r\n") ;					
				}
				if (transvertotherfreq)
				{
					sprintf 
					(
						temps+strlen(temps), 
						" FT:%-8.3f \r\n", 
						((double) mjsettings.frequency [mainband] + 
						(double) transvertmultiplier * mjsettings.frequency [otherband]) / 1000000
					) ;
				}
				if (packetsin < 10000000)
				{
					sprintf (temps2, "%7u", packetsin) ;
				}
				else
				{
					sprintf (temps2, "%6uk", packetsin / 1000) ;
				}
				sprintf (temps+strlen(temps), " -------------------------------------- \r\n") ;
				sprintf (temps+strlen(temps), " Packets  Frames   Panic  Nulls      MJ \r\n") ;
				sprintf (temps+strlen(temps), "      In     Out  Frames  SR333  Errors \r\n") ;
				sprintf (temps+strlen(temps), " -------------------------------------- \r\n") ;
				sprintf (temps+strlen(temps), " %7s  %6u %7u  %5u  %6u\r\n", temps2, framecount, framerequestcountin, nullmodcount, mjerrorcount) ;			
				sprintf (temps+strlen(temps), " ====================================== \r\n") ;
				netprint (temps) ;
			}

			if (input_thread_status == 4)
			{
				break ;		
			}
			if (output_thread_status == 4)
			{
				break ;		
			}
		}

// inner main loop end there - clean up and exit =============================================================================================

		netprint ("") ;
		sprintf (temps, "Main loop is stopping (%d %08X) ", terminate, returncode) ;
		netprint (temps) ;			

		mjsettings.txon[0] 		= 0 ;
		mjsettings.txon[1] 		= 0 ;

		if ((returncode & ERROR_OUTPUTWRITE) == 0)
		{
			if (output_thread_handle && output_thread_status) 
			{
				mjsettings.recordsequence++ ;					
				settingsrequestcountin++ ;
				output_thread_status = 2 ;				// tell output thread to terminate
				now = monotime_ms() ;
				while (monotime_ms() - now < 500)		// wait for any messages from MJ
				{
					if (mjinfoindexin != mjinfoindexout)
					{
						utemp = mjinfoindexout ;
						netprint ((char*) mjinforeceived[utemp]) ;
						utemp++ ;
						if (utemp >= MAXMJINFOS)
						{
							utemp = 0 ;
						}
						mjinfoindexout = utemp ;
					}
				}
				output_thread_status = 4 ;

//				while (output_thread_status != 4)
//				{
//					usleep (100000) ;
//				}
			}
		}
		else
		{
			terminate = 0xff ;
		}
	}
	
// end of main loop

	myexit() ;
}

void netprint (char *string)
{
	char	temps [4096] ;

	if (string[0])
	{
		sprintf (temps, "echo '%s' | netcat -uw0 %s", string, infoip) ;
		system (temps) ;			
	}
	if (strstr (outputfilename, "/dev/stdout") == 0)
	{
		printf ("%s\r\n", string) ;
	}
}

// receive packets into a circular buffer

void* receive_routine (void* dummy)
{
	int32		status ;
	int32		temp ;
	int32		temp2 ;
	int32		x ;
	int32		y ;
	uint32		pid ;
	uint32		tempu ;
	uint32		utempl ;
	uint32		threadstarttime ; 
	int 		packetsarrayfree ;
	int 		packetsinreceivebuff ;
	int 		receivepacketstoprocess ;
	uint8*		packet ;
	uint8*		ptr ;
	uint8*		ptsp ;
	uint8*		pcrp ;
	int64		pcr ;
	int64		vpts ;
	int64		apts ;
	int64		vdiff ;
	int64		adiff ;
	int64		templ ;
	double		tempf ;
	int32		bytestoread ;
	char 		temps [256] ;
	char 		temps2 [256] ;

 	int32		lastpacketsin		= 0 ;
	int32		nullpacketstoinsert	= 0 ;		

	receivepacketsindexin	= 0 ;
	receivesynchronised		= 0 ;
	inputstarttime 			= 0 ;
	receivebufflength		= 0 ;
	threadstarttime			= monotime_ms() ;

	while (input_thread_status == 0)			// wait for thread to be enabled
	{
		usleep (1000) ;
	}

	if (input_thread_status == 1)
	{
		netprint ("Input thread is starting") ;
	}
	
// clear out any previous packets lurking 

	if (mjsettings.symbolrate)
	{
		if (chinese & 1)
		{
			do
			{
				status = recvfrom 	
	            (
				    insock, sparebuff, sizeof (sparebuff), MSG_DONTWAIT,
	                (struct sockaddr*) &insockaddr, &insocklength
	            ) ;
			} while (status > 0) ;
		}
		else if (strstr(inputfilename, ".ts") == 0)
		{
			do
			{
				status = read (fhin, (void*) sparebuff, sizeof (sparebuff)) ;					
			} while (status > 0) ;
		}
	}
	
	while (input_thread_status == 1)
	{
		usleep (1000) ;

		if (mjsettings.symbolrate == 0)
		{
			continue ;
		}

		bytestoread = RECEIVEBUFFSIZE - receivebufflength ;	
		if (bytestoread)
		{
			if (chinese & 1)
			{
				status = read (fhin, (void*) (sparebuff), 4096) ;				// ignore standard input
				status = recvfrom 	
    	        (
				    insock, receivebuff + receivebufflength, bytestoread, MSG_DONTWAIT,
	                (struct sockaddr*) &insockaddr, &insocklength
    	        ) ;
			}
			else
			{
				status = read (fhin, (void*) (receivebuff + receivebufflength), bytestoread) ;
			}
			sprintf (temps, "<%d %d>", status, bytestoread) ;
///			netprint (temps) ;
		}
		else
		{
			status = 0 ;
		}
		
		if (status < 0)
		{
			if (errno != EAGAIN) 
			{
				sprintf (temps, "Input error (%d)", errno) ;
				netprint (temps) ;
				returncode |= ERROR_INPUTREAD ;
				input_thread_status = 4 ;
	       	    continue ;	
       	    }
       	    status = 0 ;
		}
		else if ((status != bytestoread) && strstr (inputfilename, ".ts") != 0)
		{
			receivebufflength 	= 0 ;
			receivesynchronised = 0 ;
            lseek (fhin, 0, SEEK_SET) ;
       	    status = 0 ;
        }
		else 
		{
			receivebufflength += status ;
		}

// ensure received packets are synchronised on start byte 0x47

		if (receivesynchronised == 0)
		{
			if (receivebufflength < 188 * 2)
			{
				continue ;
			}
				
			x = 0 ; 
			while (x < 188 && receivebuff [x] != 0x47 && receivebuff [x + 188] != 0x47) 
			{
				x++ ;
			} ;
			
			if (x == 188)							// 0x47 not found in both packets
			{
				receivebufflength -= 188 * 2 ;
				usleep (1000) ;
				continue ;
			}
			else 									// synchronised 
			{
				netprint ("Input Synced ") ;
				inputstarttime 		= monotime_ms() ;
				memmove ((void*) receivebuff, (void*) (receivebuff + x), receivebufflength - x) ;
				receivebufflength  -= x ;
				receivesynchronised = 1 ; 
				continue ;
			}
		} 	

		if (receivesynchronised == 0)
		{
			usleep (1000) ;
			continue ;
		}

// check for room in received packets array

		temp = receivepacketsindexin - receivepacketsindexout ;
		if (temp < 0)
		{
			temp += MAXRECEIVEPACKETS ;
		}
		if (temp >= MAXRECEIVEPACKETS / 2)
		{
			continue ;								// leave half of array for insertions
		}
	
		packetsinreceivebuff = receivebufflength / 188 ;
		if (packetsinreceivebuff == 0)
		{
			continue ;
		}
		receivepacketstoprocess = packetsinreceivebuff ;

//=============================================================================================================

		for (x = 0 ; x < receivepacketstoprocess ; x++)
		{
			packet = receivebuff + x * 188 ;
			pid = ((packet[1] & 0x1f) * 256) + packet [2] ; 

			if (nullpacketsinsertedcount != nullpacketsdeletedcount)
			{
				if (pid == 0x1fff)
				{		
					nullpacketsdeletedcount++ ;
					continue ;
				}
			}

			vpts = 0 ;
			apts = 0 ;
			pcr  = 0 ;
			vdiff = 0 ;
			adiff = 0 ;

			if (packet[0] != 0x47)
			{
				packet = 0 ;
				sprintf (temps, "Input unsynced <%d %d %d>", receivebufflength, receivepacketstoprocess, x) ;
				netprint (temps) ;
				receivesynchronised = 0 ;
				break ;
			} 

			if ((chinese & 1) == 0)
			{
				if (firstpcr == 0 && pid == 256)
				{
		            if (packet[5] & 0x10 && (packet[3] & 0x20)) // video, pcr and adaptation field
		            {
		                pcr = 0 ;								// get the pcr and check for duplicates
		            	pcrp = packet + 6 ;						// pointer to pcr

		                for (y = 0 ; y < 5 ; y++)
		                {
		                    pcr = pcr * 256 ;
		                    pcr += pcrp [y] ;
		                }
		                pcr  = pcr >> 7 ;									// 90kHz units
		                pcr  = pcr * 300 ;									// 27MHz units
						pcr += (pcrp [4] & 1) * 256 ;  
						pcr += pcrp [5] ;
						
						pcr = pcr - 27000000 / 2 ;  					
						firstpcr 	= pcr ;
						currentpcr 	= pcr ;
						firstpacket = packetsin ;
					}
				}
				modifyandstorepacket (packet) ;
			}
			else
			{

// check for packets that need attention

				if (pid == 8191)
				{
					packet = 0 ;										// ignore null packets
				}
				else if (pid == 257)
				{					
					if (packet [1] & 0x40)								// payload start
					{				
                    	ptsp = packet + 4  ;     				        // pointer to PES header
						if (packet[3] & 0x20)							// adaptation field
						{
                    		ptsp += packet[4] + 1 ;  		            // pointer to PES header
                    	}
                    	
						apts = 0 ;
   						if (ptsp[7] & 0x80)								// apts present
						{					
							tempu = (ptsp [9] >> 1) & 0x07 ;
							apts |= tempu ;

							apts = apts << 8 ;
							tempu = ptsp [10] ;
							apts |= tempu ;

							apts = apts << 7 ;
							tempu = (ptsp [11] >> 1) & 0x7f ;
							apts |= tempu ;

							apts = apts << 8 ;
							tempu = ptsp [12] ;
							apts |= tempu ;

							apts = apts << 7 ;
							tempu = (ptsp [13] >> 1) & 0x7f ;
							apts |= tempu ;										// already in 90kHz units
						}

						if (apts == lastapts)
						{
							apts = 0 ;
						}
						else
						{
							lastapts = apts ;
						}
					}			
				
					if (packet[3] & 0x20)							// adaptation field present
					{
						packet [5] &= ~0x10 ;						// turn off any pcr indication	
					}
				}
				else if (pid == 256)
				{
		            if (packet[5] & 0x10 && (packet[3] & 0x20)) // pcr and adaptation field
		            {
		                pcr = 0 ;								// get the pcr and check for duplicates
		            	pcrp = packet + 6 ;						// pointer to pcr

		                for (y = 0 ; y < 5 ; y++)
		                {
		                    pcr = pcr * 256 ;
		                    pcr += pcrp [y] ;
		                }
		                pcr  = pcr >> 7 ;									// 90kHz units
		                pcr  = pcr * 300 ;									// 27MHz units
						pcr += (pcrp [4] & 1) * 256 ;  
						pcr += pcrp [5] ;  					

						if (pcr == lastpcr)
						{
							packet [5] &= ~0x10 ;							// duplicate pcr so disable it
							pcr = 0 ;
						}
						else
						{
		                	lastpcr = pcr ;
						}
					}
					
					if (packet [1] & 0x40)								// payload start
					{
                    	ptsp = packet + 4  ;     				        // pointer to PES header
						if (packet[3] & 0x20)							// adaptation field
						{
                    		ptsp += packet[4] + 1 ;  		            // pointer to PES header
                    	}
                    	
   						vpts = 0 ;			
						if (ptsp[7] & 0x80)								// vpts present
						{					
							tempu = (ptsp [9] >> 1) & 0x07 ;
							vpts |= tempu ;

							vpts = vpts << 8 ;
							tempu = ptsp [10] ;
							vpts |= tempu ;

							vpts = vpts << 7 ;
							tempu = (ptsp [11] >> 1) & 0x7f ;
							vpts |= tempu ;

							vpts = vpts << 8 ;
							tempu = ptsp [12] ;
							vpts |= tempu ;

							vpts = vpts << 7 ;
							tempu = (ptsp [13] >> 1) & 0x7f ;
							vpts |= tempu ;										// already in 90kHz units
						}

						if (vpts == lastvpts)
						{
							vpts = 0 ;
						}
						else
						{
							lastvpts = vpts ;
						}
/*		
						if (vpts)
						{
							vpts -= 90000 / 4 ;			/////////////// 250ms earlier

							utempl = vpts ;
							ptsp[13] = (ptsp[13] & ~0xfe) | ((utempl <<  1) & 0xfe) ;	// 7 bits
							ptsp[12] = (ptsp[12] & ~0xff) | ((utempl >>  7) & 0xff) ;	// 8 bits		
							ptsp[11] = (ptsp[11] & ~0xfe) | ((utempl >> 14) & 0xfe) ;	// 7 bits		
							ptsp[10] = (ptsp[10] & ~0xff) | ((utempl >> 22) & 0xff) ;	// 8 bits		
							ptsp[9]  = (ptsp[9]  & ~0x0e) | ((utempl >> 29) & 0x0e) ;	// 3 bits		
						}						
*/
					}			

					if (firstvpts == 0 && vpts)
					{
						firstvpts 	= vpts ;
						firstpcr 	= (firstvpts * 300) - 27000000 * 1 / 4 ;	
						firstpacket = packetsin ;
						currentpcr  = firstpcr ;
						firstrealtime = monotime_ms() ;
						sprintf 
						(
							temps, "FPTS=%lld FPCR=%lld %lld\r\n", 
							firstvpts * 300, firstpcr, firstvpts*300 - firstpcr
						) ;
///						netprint (temps) ;
					}
					
					if (firstvpts && pcr && savedpcrpacket[0] == 0)
					{
						memmove (savedpcrpacket, packet, 188) ;
						savedpcrpacket [1] &= ~0x40 ;						// not start of pes 
						savedpcrpacket [3] &= ~0x10 ;						// no payload
						savedpcrpacket [4]  = 183 ;							// size of adaptation field
						memset (savedpcrpacket+12, 0xff, 176) ;
					}
				}

				if (vpts)
				{
					vdiff = (int64) vpts * 300 / 27000 - (int64) currentpcr / 27000 ;
					sprintf 
					(
						temps, "REAL:%lld  VPTS:%lld, CPCR:%lld, VDIFF:%lld",
						firstpcr / 27000 + monotime_ms() - firstrealtime,	
						vpts * 300 / 27000, currentpcr / 27000, vdiff
					) ;
					if (vdiff < 10000)
					{
///						netprint (temps) ;
					}
				}							

				if (apts)
				{
					adiff = (int64) apts * 300 / 27000 - (int64) currentpcr / 27000 ;
					sprintf 
					(
						temps, "APTS:%lld, CPCR:%lld, ADIFF:%lld",	
						apts * 300 / 27000, currentpcr / 27000, adiff
					) ;
					if (adiff < 100)
					{
///						netprint (temps) ;
					}
				}							

				modifyandstorepacket (packet) ;		
				packet = 0 ;

// check if null packets need to be inserted

				if (output_thread_status && vpts)
				{
					templ = (int64)vpts * 300 - (int64)currentpcr ;		// how far is pts ahead of pcr
					templ = templ / 27000 ;								// number of ms ahead
					templ = templ - 750 ;								// cap at 1000ms
					temp = templ * packetspersecond / 1000 ;									
					if (temp > 0) 
					{
						sprintf (temps, "Inserting %d packets %lld %lld %lld %d ", 
						temp, vpts * 300 / 27000, currentpcr / 27000, 
						vpts * 300 / 27000 - currentpcr / 27000, packetsin) ;
///						netprint (temps) ;
		
						for (y = 0 ; y < temp ; y++)
						{	
							modifyandstorepacket (nullpacket) ;
						}
					}
				}								
			}
		}
		
// shuffle receivebuff to remove processed packets

		memmove 
		(
			receivebuff, 
			receivebuff + receivepacketstoprocess * 188, 
			receivebufflength - receivepacketstoprocess * 188
		) ;
		receivebufflength -= receivepacketstoprocess * 188 ;
	}

	netprint ( "Input thread is stopping") ;
	input_thread_status = 4 ;
}


// take received packets and create an array of frames

void* framing_routine (void* dummy)
{
	uint		tempu ;
	int			temp ;
	int			x ;
	int			y ;
	int			status ;
///	int			packetsinframe ;
	int			symbolcount ;
	uint8*		symbolpointer ;
	uint8*		packetpointer ;
	int32		symbolstosend ;
	uint8*		firstframepointer ;
	uint8*		secondframepointer ;
	uint8*		framepointer ;
	uint32		lastrecord2time ;	
	uint32		pid ;
	uint32		framesready ;
		
	receivepacketsindexout	= 0 ;
	packetsinframe 			= 0 ;
	lastrecord2time			= 0 ;
	
	while (framing_thread_status == 0)								// wait for thread to be enabled
	{
		usleep (1000) ;
	}
	
	if (framing_thread_status == 1)
	{
		netprint ("Framing thread is starting") ;
	}
	
	while (framing_thread_status == 1)								// continue while thread enabled
	{
		if (mjsettings.symbolrate == 0)								// carrier modes, no symbol output
		{
			usleep (1000) ;
			continue ;
		}

		packetpointer = 0 ;
		partfilledframe = 0 ;

		if 
		(
			receivepacketsindexout == receivepacketsindexin &&
			framerequestcountin == framerequestcountout
		)
		{	
			usleep (1000) ;
			continue ;
		}
		
		temp = framesindexin - framesindexout ;			
		if (temp < 0)
		{
			temp += MAXFRAMES ;
		}	
		if (temp >= MAXFRAMES - 2)									// no frame buffers available
		{
			usleep (1000) ;
			continue ;
		}

		if (packetpointer == 0)
		{
			if (receivepacketsindexout != receivepacketsindexin)
			{
				packetpointer = (uint8*) receivepacketsarray [receivepacketsindexout] ;
				tempu = receivepacketsindexout ;
				tempu++ ;
				if (tempu >= MAXRECEIVEPACKETS)
				{
					tempu = 0 ;
				}
				receivepacketsindexout = tempu ;
			}
		}

		if (packetpointer == 0)
		{
			if (framerequestcountin != framerequestcountout)
			{
				framerequestcountout++ ;
				partfilledframe = 1 ;
			}
		}

		if (packetpointer == 0 && partfilledframe == 0)
		{
			continue ;
		}

		if (packetsinframe == 0)
		{
	       	status = dvbs2neon_control (STREAM0, CONTROL_SET_OUTPUT_BUFFER, (uint32) framesarray [framesindexin], 0) ;	// set buffer address
		}
///www

		if (partfilledframe)
		{
			temp = 4 - packetsinframe ;
			if (temp > 0)
			{
				nullpacketsinsertedcount += temp ;
				packetsinframe += temp ;
				for (x = 0 ; x < temp ; x++)
				{
					status = dvbs2neon_packet (STREAM0, (uint32) nullpacket, 0) ;
				}
			}
			packetpointer = 0 ;
		}
		
		status = dvbs2neon_packet (STREAM0, (uint32) packetpointer, partfilledframe) ;
		packetsout++ ;
		packetsinframe++ ;	

		if (status < 0 && status >= ERROR_LOWEST)
		{
			printf ("A dvbs2neon error %d has occurred \r\n\r\n", status) ;
			returncode &= 0x00ffffff ;
			returncode |= abs (status) << 24 ;
			myexit() ;
		}
		else if (status != 0)										// a frame has been returned
		{
			tempu = framesindexin ;
			tempu++ ;
			if (tempu >= MAXFRAMES)
			{
				tempu = 0 ;
			}
			framesindexin = tempu ;
			packetsinframe = 0 ;
		}
	}
	
	netprint ( "Framing thread is stopping") ;
	framing_thread_status++ ;
}

//ttt

void* transmit_routine (void* dummy)
{
	uint		tempu ;
	int			temp ;
	int			x ;
	int			y ;
	int			status ;
	int			symbolcount ;
	uint8*		symbolpointer ;
	int32		symbolstosend ;
	uint8*		framepointer ;
	uint8*		outputpointer ;
	uint32		bytestosend ;
	uint32		lastrecord2time ;
	int32		sendingsettings ;
	uint32		utemp ;
	char		temps [256] ;
	int			fh ;
	int8		bufferlowflag ;
	char		*pos ;			
	
	lastrecord2time	= 0 ;
	bufferlowflag 	= 0 ;
	mjinbufflength 	= 0 ;

	while (output_thread_status == 0)								// wait for thread to be enabled
	{
		usleep (1000) ;
	}

	if (output_thread_status == 1)
	{
		netprint ("Output thread is starting") ;
	}

	while (output_thread_status < 4)								// continue while thread enabled
	{
		usleep (1000) ;

		outputpointer 	= usbtxbuff ;
		sendingsettings = 0 ;
		framepointer 	= 0 ;
		
// check for time to send settings record

		if (settingsrequestcountin != settingsrequestcountout)
		{
			settingsrequestcountout++ ;

			if (output_thread_status == 2)
			{
				memcpy (outputpointer, &eof, sizeof (eof)) ;				// send an EOF record		            
	            outputpointer += sizeof (eof) ;
				output_thread_status = 3 ;
			}
			memcpy (outputpointer, &mjsettings, sizeof (mjsettings)) ;		
			outputpointer += sizeof (mjsettings) ;
		}

		if (output_thread_status == 1)
		{
			framepointer = 0 ;

			if (framepointer == 0)
			{
				if (framesindexout != framesindexin)
				{
					framepointer = (uint8*) (framesarray [framesindexout]) ;	
					utemp = framesindexout ;
					utemp++ ;
					if (utemp >= MAXFRAMES)
					{	
						utemp = 0 ;
					}
					framesindexout = utemp ;
				}
			}

			if (framepointer)
			{

				bufferlowflag = 0 ;
				framecount++ ;

				memcpy (outputpointer, &beginframe, sizeof (beginframe)) ;		
				outputpointer += sizeof (beginframe) ;
						
				symbolpointer  = framepointer ;							// initially point to the start of the frame data
				symbolpointer += ((uint16*) symbolpointer)  [0] ;		// the first short is the offset to the symbols
				symbolcount    =  ((uint16*) symbolpointer) [-1] ;		// the 2 bytes before the symbols gives the count

				if (neonsettings.constellation == M_QPSK)				// pack 4 x 2 bit symbols into each output byte
				{
					symbolstosend  = symbolcount / 4 ;					// sometimes not a multiple of 4
					symbolstosend *= 4 ;
					for (x = 0 ; x < symbolstosend ; x += 4)			// pack the symbols into bytes
					{
						tempu = 0 ;
						for (y = 0 ; y < 4 ; y++) 
						{
							tempu <<= 2 ;
							tempu |= symbolpointer [x + y] ;
						}
						if (tempu == ESCAPE || tempu == '#')
						{
							*outputpointer++ = ESCAPE ;					// stuff a zero when a zero symbol byte is output					
						}
						*outputpointer++ = tempu ;
					}
					if (symbolcount - symbolstosend == 2)				// symbolcount not a multiple of 4, so 2 symbols left over
					{
						tempu   = 0 ;
						tempu  |= symbolpointer [x++] ;					// first left over symbol
						tempu <<= 2 ;
						tempu  |= symbolpointer [x++] ;					// second left over symbol
						tempu <<= 4 ;									// move the 2 symbols to the upper nybble
						tempu  |= 0 ;									// record type 1 indication
						*outputpointer++ = ESCAPE ;						// stuff an escape byte to indicate embedded data
						*outputpointer++ = tempu ;						
					}
				}
				else if (neonsettings.constellation == M_8PSK)			// pack 4 x 2 bit symbols into each output byte
				{
					symbolstosend = symbolcount ;						// symbolcount always even for 8PSK
					for (x = 0 ; x < symbolstosend ; x += 2)			// pack the symbols for 2 frames into bytes
					{
						tempu = 0 ;
						for (y = 0 ; y < 2 ; y++) 
						{
							tempu <<= 4 ;
							tempu |= symbolpointer [x + y] ;
						}
						if (tempu == ESCAPE || tempu == '#')
						{
							*outputpointer++ = ESCAPE ;					// stuff a zero when a zero symbol byte is output					
						}
						*outputpointer++ = tempu ;
					}
				}

				memcpy (outputpointer, &endframe, sizeof (endframe)) ;		
				outputpointer += sizeof (endframe) ;
			}
		}
		
		bytestosend = outputpointer - usbtxbuff ;
		if (bytestosend && holdoffactive == 0)
		{
////			printf ("<%d>\r\n", bytestosend) ;
#if TEST
			fh = open ("/home/pi/test1.mjd", O_WRONLY | O_APPEND) ;
            status = write (fh, usbtxbuff, bytestosend) ;
			close (fh) ;			
			printf ("<%d>\r\n", status) ;
#endif
			outputpointer = usbtxbuff ;
			while (bytestosend > 0)
			{
/*
				if (bytestosend < 8192)
				{
					utemp = bytestosend ;
				}
				else
				{
					utemp = 8192 ;
	            }
*/
	            status = write (mjfd, outputpointer, bytestosend) ;
	            if (status >= 0)
	            {
	            	outputpointer += status ;
					bytestosend -= status ;
	            }
	            else if (errno == EAGAIN)
	            {
	            	usleep (1000) ;
	            }
	            else
	            {
					sprintf (temps, "Output error (%d) ", errno) ;
					netprint (temps) ;
					returncode |= ERROR_OUTPUTWRITE ;
					output_thread_status = 4 ;
					break ;
	            }
			}

			if (output_thread_status == 4)
			{
////				continue ;
			}
		}
		
// process any status info from muntjac

		status = read (mjfd2, mjinbuff + mjinbufflength, sizeof(mjinbuff) - mjinbufflength) ;
		if (status > 0)
		{
			mjinbufflength += status ;
			do
			{
				pos = strstr (mjinbuff, "\r\n") ;
				if (pos)
				{
					*pos = 0 ;
					pos += 2 ;
					utemp = mjinfoindexin ;
					strcpy ((char*)mjinforeceived[utemp], mjinbuff) ;
					utemp++ ;
					if (utemp >= MAXMJINFOS)
					{
						utemp = 0 ;
					}
					mjinfoindexin = utemp ;

					if (strstr(mjinbuff, "(BUFFER_LOW:"))
					{
						framerequestcountin++ ;							// request a panic frame 
						bufferlowflag = 1 ;
					}
					else if (strstr(mjinbuff, "HOLDOFF_ACTIVE")) 
					{
						holdoffactive = 1 ;
					}
					else if (strstr(mjinbuff, "HOLDOFF_EXPIRED")) 
					{
						holdoffactive = 0 ;
					}
				}
				else if (mjinbufflength == sizeof(mjinbuff))
				{	
					pos = mjinbuff + sizeof(mjinbuff) ;
				}
				else
				{
					pos = 0 ;
				}	
				if (pos)
				{
					memmove (mjinbuff, pos, mjinbufflength - (pos - mjinbuff)) ;
					mjinbufflength -= (pos - mjinbuff) ;
				}
///			} while (pos && (bufferlowflag == 0)) ;
			} while (pos) ;
		}
	}

	netprint ("Output thread is stopping") ;
	usleep (100000) ;
	
	output_thread_status = 5 ;
}

uint32 monotime_ms() 
{
 	long            	ms ; 						// milliseconds
    time_t          	s ;  						// Seconds
    struct timespec 	spec ;
            
    clock_gettime (CLOCK_MONOTONIC, &spec) ;             
    s  	= spec.tv_sec ;
    ms 	= spec.tv_nsec / 1000000 ; 					// Convert nanoseconds to milliseconds
    if (ms > 999) 
    {
    	s++ ;
        ms = 0 ;
    }
	return (s * 1000 + ms) ;                                            
}
                                                            
void sig_handler (int32 signo)
{
    terminate = signo ;
    returncode |= ERROR_SIGNAL ;
}

void myexit()
{
	uint32		tempu ;
	char		temps [1024] ;

	if (output_thread_handle)
	{
		output_thread_status = 4 ;
		pthread_join (output_thread_handle, 0) ;
		output_thread_handle = 0 ;
	}

	if (framing_thread_handle)
	{
		framing_thread_status = 3 ;
		pthread_join (framing_thread_handle, 0) ;
		framing_thread_handle = 0 ;
	}

	if (input_thread_handle)
	{
		input_thread_status = 3 ;
		pthread_join (input_thread_handle, 0) ;
		input_thread_handle = 0 ;
	}
   
	if (mjfd >= 0)
	{
		flock (mjfd, LOCK_UN) ;
		close (mjfd) ;
		mjfd = -1 ;
	}

	if (fhin >= 0)
	{
		close (fhin) ;
		fhin = 0 ;
	}
         
	if (returncode < 256 && returncode > 256 + ERROR_LOWEST)
	{
	}            
	else if (returncode & 0x7f)
	{
		returncode = (returncode >> 0) & 0x7f ;				// bit 7 = 0
	}
	else if (returncode & 0x3f00)
	{
		returncode = ((returncode >> 8) & 0x3f) | 0x80 ;	// bits 7:6 = 10
	}
	else if (returncode & 0xff0000)
	{
		returncode = ((returncode >> 16) & 0x1f) | 0xc0 ;	// bits 7:5 = 110
	}	
	else if (returncode >> 24)
	{
		returncode = ((returncode >> 24) & 0x1f) | 0xe0 ;	// bits 7:5 = 111
	}

	if (terminate != 0xff && returncode != 0 && returncode != 0xc0 + (ERROR_SIGNAL >> 16))
	{
		sprintf (info+strlen(info), "For help: ./%s-%s -h \r\n", VERSIONX, VERSIONX2) ;
	}

	if (bitrateonly == 0 && hindex == 0)
	{
		sprintf (info+strlen(info), "%s-%s is exiting with return code 0x%02X \r\n", VERSIONX, VERSIONX2, returncode) ;
	}

	netprint (info) ;

	usleep (100000) ;
	exit (returncode) ;
}

//www

uint8* modifyandstorepacket (uint8* packet)
{
	uint32		pid ;
	uint32		utemp ;
	uint32		tempu ;
	uint32		length ;
	uint8		*pcrp ;
	uint8		*sdtp ;
	char		temps [256] ;
	double		tempf ;
	uint64		utempl ;
	uint64		pcr ;
	int32		x ;


	if (packet == 0)
	{
		return (0) ;
	}
	else if (packet[0] == 0)
	{
		return (0) ;
	}

	pid = ((packet[1] & 0x1f) << 8) | packet[2] ;

	if ((chinese & 1) == 0)
	{
		if (pid == 256)										// video
		{
			if ((packet [3] & 0x20) && (packet [5] & 0x10))	// adaptation field and pcr 
			{
				pcr    = currentpcr ;
				utempl = pcr / 300 ;						// upper 33 bits
				utemp  = pcr - utempl * 300 ;				// lower 9 bits
				utempl = utempl << 15 ;						// move 33 bits to top of 58 bit field
				utempl = utempl | utemp ;					// move 9 bits to bottom of 48 bit field

				pcrp = packet + 6 ;
				for (x = 5 ; x >= 0 ; x--)
				{
					pcrp [x] = utempl & 0xff ;
					utempl = utempl >> 8 ;
				}		
			}
			modifyandstorepacket2 (packet) ;

		}
		else if (pid == SDTPID)							///////////////////////////////////////////////////////////
		{
			sdtcontinuity++ ;
			sdt.continuity = (sdt.continuity & 0xf0) | (sdtcontinuity & 0x0f) ; 

			modifyandstorepacket2 ((uint8*) &sdt) ;
/*
			if (provider2[0] == 0)								// incoming SDT has not already been processed
			{
		        sdtp = packet + 0x15 ; 							// point to the descriptor tag
		        if (*sdtp == 0x48) 								// service descriptor
		        {
		            sdtp += 3 ;                                 // point to the provider length
		            length = *sdtp++ ;                          // provider length
		            memset (provider2, 0, sizeof(provider2)) ;
		            strncpy (provider2, (void*)sdtp, length) ;  // copy the provider
		            sdtp += length ;                            // point to the name length
		            length = *sdtp++ ;                          // get the name length
		            memset (callsign2, 0, sizeof(callsign2)) ;
		            strncpy (callsign2, (void*)sdtp, length) ;  // copy the name
					if (strstr(provider2, "Muntjac") == 0)		// provider does not contain "Muntjac"
					{
						if (strstr(provider2, "Portsdown"))		// provider contains "Portsdown"
						{
							strcpy (provider2, "Portsdown4") ;
							sdt_setup() ;
						}
					}	
				}
			}
			if (provider2[0])									// incoming SDT has already been processed
			{
				sdt.continuity++ ;
				modifyandstorepacket2 ((uint8*) &sdt) ;
			}
			else
			{
				modifyandstorepacket2 (packet) ;
			}
*/
		}
		else
		{
			modifyandstorepacket2 (packet) ;
		}
	}
	else
	{
		if (pid == TDTPID)
		{
			incomingtdtpacketcount++ ;
			modifyandstorepacket2 (packet) ;
		}
		else if (pid == SDTPID)
		{
			incomingsdtpacketcount++ ;
			modifyandstorepacket2 (packet) ;
		}
		else if ((pid == 256) || (packetsin - lastpcrinsertionpacket >= packetspersecond / 10))
		{
			if (pid != 256)
			{
				modifyandstorepacket2 (packet) ;
				packet = savedpcrpacket ;
			}
			if (packet[3] & 0x10)							// payload present
			{
				pcrcontinuity++ ;
			}
			packet [3] = (packet [3] & 0xf0) | (pcrcontinuity & 0x0f) ; 

			if ((packet [3] & 0x20) && (packet [5] & 0x10))	// adaptation field and pcr 
			{
				utempl = currentpcr / 300 ;					// upper 33 bits
				utemp  = currentpcr - utempl * 300 ;		// lower 9 bits
				utempl = utempl << 15 ;						// move 33 bits to top of 58 bit field
				utempl = utempl | utemp ;					// move 9 bits to bottom of 48 bit field

				pcrp = packet + 6 ;
				for (x = 5 ; x >= 0 ; x--)
				{
					pcrp [x] = utempl & 0xff ;
					utempl = utempl >> 8 ;
				}
			}
			modifyandstorepacket2 (packet) ;
			lastpcrinsertionpacket = packetsin ;
		}
		else
		{
			modifyandstorepacket2 (packet) ;
		}
		
		if (incomingsdtpacketcount == 0)
		{
			if (packetsin - lastsdtinsertionpacket >= packetspersecond * 2)
			{	
				lastsdtinsertionpacket = packetsin ;
				sdt.continuity++ ;
				modifyandstorepacket2 ((uint8*) &sdt) ;				
			}
		}
		
		if (incomingtdtpacketcount == 0)
		{		
			if (packetsin - lasttdtinsertionpacket >= 5 * packetspersecond)
			{	
				lasttdtinsertionpacket = packetsin ;
				
		        timexp = &timex ;
		        currenttime = time (NULL) ;
		        if (timeinsert == TIMEUTC)
		        {
		            timexp = gmtime (&currenttime) ;
		        }
		        else if (timeinsert == TIMELOCAL)
		        {
		            timexp = localtime (&currenttime) ;
		        }

		        utemp = juliandate (timexp) ;
		        tdt.datetime [0] = (uint8) (utemp >> 8) ;
		        tdt.datetime [1] = (uint8) (utemp & 0xff) ;

		        sprintf (temps,"%u",timexp->tm_hour) ;
		        sscanf (temps,"%02X",&utemp) ;
		        tdt.datetime [2] = (uint8) utemp ;

		        sprintf (temps,"%u",timexp->tm_min) ;
		        sscanf (temps,"%02X",&utemp) ;
		        tdt.datetime [3] = (uint8) utemp ;

		        sprintf (temps,"%u",timexp->tm_sec) ;
		        sscanf (temps,"%02X",&utemp) ;
		        tdt.datetime [4] = (uint8) utemp ;

				tdtcontinuity++ ;
				tdt.continuity = (tdt.continuity & 0xf0) | (tdtcontinuity & 0x0f) ; 

				modifyandstorepacket2 ((uint8*) &tdt) ;
		    }
		}
	}	

	if (nullmodify)
	{
		if (packetsin / (nullmodify + 1) > nullmodcount)
		{
			modifyandstorepacket2 ((uint8*) &nullpacket) ;
			nullmodcount++ ;
		}
	}

	return (0) ;
}

uint8* modifyandstorepacket2 (uint8* packet)
{
	int32		temp ;
	uint32		tempu ;
	uint32		utemp ;
	uint8*		sdtp ;
	uint8*		ptsp ;
	uint8*		pcrp ;
	char		temps 		[256] ;
	char		temps2 		[256] ;
	char		provider 	[256] ;
	char		name 		[256] ;
	uint8		local		[188] ;
	int32		length ;
	uint32		pid ;
	int32		x ;
	uint64		utempl ;
	uint64		templ2 ;
	uint64		diffl ;
	static double tempf 		= 0 ;


	if (packet == 0)
	{
		return (0) ;
	}
	else if (packet[0] == 0)
	{
		return (0) ;
	}
	
	memmove ((uint8*) (receivepacketsarray [receivepacketsindexin]), packet, 188) ;
	packetsin++ ;
	
	tempu = receivepacketsindexin ;
	tempu++ ;
	if (tempu >= MAXRECEIVEPACKETS)
	{
		tempu = 0 ;
	}
	receivepacketsindexin = tempu ;

// calculate the current pcr

	utempl = packetsin - firstpacket ;			// number of packets since start

	utempl += nullpacketsinsertedcount ;
	utempl -= nullpacketsdeletedcount ;

	utempl = utempl * 1504 ;					// bits since start
	utempl = utempl * 27000000 ;				// units of 27MHz
	utempl = utempl / realbitrate ;				// divide by TS bits per second to give number of seconds
	utempl = utempl + firstpcr ;				// relative to start
	currentpcr = utempl ;						// new pcr

	return (packet) ;
}

uint32 juliandate (struct tm* timexx)
{
    uint            utemp ;
    int             x ;
    int             year ;

    year = timexx->tm_year + 1900 - 2000 ;

    utemp = DAY0 ;

    for (x = 0 ; x < year ; x++)
    {
        utemp += 365 ;
        if ((x & 3) == 0)
        {
            utemp++ ;
        }
    }

    for (x = 1 ; x < timexx->tm_mon + 1 ; x++)
    {
        utemp += daysinmonth [x] ;
        if (x == 2 && ((year & 3) == 0))
        {
            utemp++ ;
        }
    }

    utemp += timexx->tm_mday ;
    return (utemp) ;
}
			
uint32 calculateCRC32 (uint8* data, uint32 dataLength)
{
    uint32      crc ;
    uint32      poly, temp, temp2, temp4, bit31 ;
	uint32		x ;

	crc = 0xffffffff ;
	poly = 0x04c11db7 ;

   	while (dataLength-- > 0)
	{
		temp4 = poly ;
		temp2 = (crc >> 24) ^ *data ;
		temp = 0 ;

		for (x = 0 ; x < 8 ; x++)
		{
			if ((temp2 >> x) & 1)
			{
				temp ^= temp4 ;
			}

			bit31 = temp4 >> 31 ;
			temp4 <<= 1 ;
			if (bit31)
			{
				temp4 ^= poly ;
			}
		}
		crc = (crc<<8) ^ temp ;
		data++ ;
   	}
    return crc ;
}

// reverse the byte order in a word

uint32 reverse (uint32 indata)
{
	uint32		tempu ;

	tempu  = 0 ;
	tempu |= ((indata >> 24) & 0xff) <<  0 ;
	tempu |= ((indata >> 16) & 0xff) <<  8 ;
	tempu |= ((indata >>  8) & 0xff) << 16 ;
	tempu |= ((indata >>  0) & 0xff) << 24 ;
	
	return (tempu) ;
}
    

// set up SDT packet

void sdt_setup()
{
    uint8       *p, *q ;
	uint32	 	utemp ;
	char		temps [256] ;
    
    memset (&sdt,0,sizeof(sdt)) ;
 
    sdt.sync = 0x47 ;
    sdt.payloadstart = 1 ;
    sdt.adaption = 1 ;
    sdt.pid1208 = SDTPID >> 8 ;
    sdt.pid0700 = SDTPID & 0xff ;
 
    sdt.tableid = 0x42 ;                        // this TS
    sdt.syntax = 1 ;
    sdt.reserved0 = 3 ;
    sdt.currentnext = 1 ;
    sdt.reserved1 = 3 ;
    sdt.tsid = TSID ;
    sdt.network = NETWORK ;
    sdt.reserved4 = 0xff ;

    sdt.version = rand() % 32 ; 				// tableversion ;

    p = (uint8*) &sdt.serviceloop ;

    *p++ = 0 ;              // high byte of service / program id
    *p++ = (uint8) 1 ;      // program number
    *p++ = 0xfd ;           // NIT
    *p++ = 0x80 ;           // running, free, 4 bit filler
    q = p ;                 // save pointer to length
    *p++ = 0 ;              // length of following info
    *p++ = 0x48 ;           // descriptor tag 
    *p++ = 0 ;              // descriptor length
    *p++ = 1 ;              // service type = TV

	if (provider2[0] == 9999) /////////////////
	{
		sprintf (temps, "%s%s", provider2, provider) ;
	    *p++ = (uint8) strlen (temps) ;
    	memcpy (p,temps,strlen(temps)) ;
	    p += strlen(temps) ;
	}
	else
	{
	    *p++ = (uint8) strlen (provider) ;
    	memcpy (p,provider,strlen(provider)) ;
	    p += strlen(provider) ;
	}

	if (callsign2[0] == 9999) /////////
	{
	    *p++ = (uint8) strlen (callsign2) ;
    	memcpy (p,callsign2,strlen(callsign2)) ;
	    p += strlen(callsign2) ;
	}
	else
	{
	    *p++ = (uint8) strlen (callsign) ;
    	memcpy (p,callsign,strlen(callsign)) ;
	    p += strlen(callsign) ;
	}

    *q = (uint8) (p - q - 1) ;
    *(q+2) = (uint8) (p - q - 3) ;

    sdt.sectionlength = (uint8) (p - &sdt.sectionlength + 4 - 1) ;

    utemp = calculateCRC32 ((uint8*)&sdt.tableid,p - &sdt.tableid) ;
    *p++ = (uint8) ((utemp >> 24) & 0xff) ;
    *p++ = (uint8) ((utemp >> 16) & 0xff) ;
    *p++ = (uint8) ((utemp >>  8) & 0xff) ;
    *p++ = (uint8) ((utemp >>  0) & 0xff) ;

    memset (p,0xff,188-((uint)p-(uint)&sdt)) ;
}

void display_help()
{
	char	temps2 [256] ;
	
	memset (temps2, 0,   sizeof(temps2)) ;
	memset (temps2, '=', strlen(title)) ;

	strcpy (info, "") ;
	sprintf (info+strlen(info), "\r\n%s\r\n%s\r\n%s\r\n", temps2, title, temps2) ;

	sprintf (info+strlen(info),"-c  constellation      QPSK, 8PSK (default: QPSK) \r\n") ;
	sprintf (info+strlen(info),"                       (8PSK is not available for SR1000) \r\n") ;  
	sprintf (info+strlen(info),"                       (DVBS2 does not support FEC 4/5 for 8PSK) \r\n") ;
	sprintf (info+strlen(info),"-d  print bit rate     exits after printing (value is correct for frame type and pilots) \r\n") ;
	sprintf (info+strlen(info),"-f  FEC                1/4, 1/3, 2/5, 1/2, 3/5, 2/3, 3/4, 4/5, 5/6, 8/9, 9/10, carrier, sidebands \r\n") ;  
	sprintf (info+strlen(info),"                       (alternating sidebands for LO and sideband suppression test) \r\n") ;				
	sprintf (info+strlen(info),"-g  power              0.00 - 0.20 for DATV, 0.00-0.31 for carriers and sideband test \r\n") ;
	sprintf (info+strlen(info),"                       the data sheet range is 0.00-0.31, giving a nominal +14dBm maximum in 1dB steps \r\n") ;
	sprintf (info+strlen(info),"                       shoulders start to be visible above 0.10 \r\n") ;
	sprintf (info+strlen(info),"                       note that 0.3 is power level 30! \r\n") ;
	sprintf (info+strlen(info),"-h  this help file     exits after printing \r\n") ;
	sprintf (info+strlen(info),"-i  input file         (default: /dev/stdin) \r\n") ;
	sprintf (info+strlen(info),"-j  mode               decimal value - test modes including Chinese H.265 / H.264 encoder box \r\n") ;
	sprintf (info+strlen(info),"-k  callsign           replaces the callsign in the SDT \r\n") ;
	sprintf (info+strlen(info),"-m  modulation         DVBS2 (default: DVBS2 - DVBS is not available) \r\n") ;
	sprintf (info+strlen(info),"-n  null deletion      removes a null packet for multiples of the specified value, for testing only \r\n") ;
	sprintf (info+strlen(info),"-o  output file        (default: /dev/ttyMJ0) \r\n") ;
	sprintf (info+strlen(info),"-p  pilots on          no parameter \r\n") ;
	sprintf (info+strlen(info),"-s  symbol rate        125000, 250000, 333000, 333333, 500000, 1000000 \r\n") ;
	sprintf (info+strlen(info),"                       (native SR is 333333 - for 333000, a null packet is inserted every 999 packets) \r\n") ;
	sprintf (info+strlen(info),"-t  frequency          389.5e6-510e6, 770e6-1030e6, 2390e6-2490e6 \r\n") ;
	sprintf (info+strlen(info),"                       note that the data sheet ranges are 779.0-1020.0, 2400.0-2483.5 \r\n") ;
	sprintf (info+strlen(info),"                       check for signal quality and instability when operating outside those ranges \r\n") ;
	sprintf (info+strlen(info),"-u  UDP debug output   (default: 127.0.0.1:9979, nc -kluv 9979 to view) \r\n") ;
	sprintf (info+strlen(info),"-v  short frames on    no parameter \r\n") ;
	sprintf (info+strlen(info),"                       (DVBS2 does not support FEC 9/10 for short frames) \r\n") ;
	sprintf (info+strlen(info),"-w  provider           sets the provider name in the SDT \r\n") ;
	sprintf (info+strlen(info),"-x  mixer power        0.00 - 0.31 \r\n") ;
	sprintf (info+strlen(info),"-y  LO suppression     iiqq (0000 to 6363) use 9999 for AT86RF215 internal value \r\n") ;
	sprintf (info+strlen(info),"                       (if not set, looks for a .mjo file for this Muntjac) \r\n") ;
	sprintf (info+strlen(info),"                       (first in /home/pi/rpidatv/bin and then in the current directory) \r\n") ;
	sprintf (info+strlen(info),"-z  secondary band     no parameter \r\n") ;
	sprintf (info+strlen(info),"                       set -t, -g to transmit same data as main band \r\n") ;
	sprintf (info+strlen(info),"                       optionally set -y, -f carrier \r\n") ;
	sprintf (info+strlen(info),"-q -e -r -D            accepted, but not required \r\n") ;
	sprintf (info+strlen(info),"                       selecting 76GHz transverter in Portsdown will activate alternate sideband test mode \r\n") ;
	sprintf (info+strlen(info),"Return code            8 bit - see source file for details \r\n") ;

	netprint (info) ;
	strcpy (info, "") ;
}



