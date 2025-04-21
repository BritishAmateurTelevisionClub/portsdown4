#define VERSIONX "muntjacsdr_dvb-0v2j"

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
	Short frame and pilots are supported (with correct bitrate)
	SR parameters -s 333000 and -s 333333 are supported
	333333 is the native SR - 333000 will insert a null packet every 1000

	Both lowband and highband may be used simultaneously
	The bands are referred to as primary and secondary
	The secondary tx may be the same as the primary, or a carrier or sidebands  
	-f sidebands transmits alternating LSB and USB carriers for calibration

	build: gcc muntjacsdr_dvb_0v2a.c dvbs2neon0v47.S -mfpu=neon -lpthread -o muntjacsdr_dvb_0v1a
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

// types

typedef signed int          int32 ;
typedef unsigned int        uint32 ;
typedef unsigned short      uint16 ;
typedef short      			int16 ;
typedef unsigned char       uint8 ;
typedef signed char       	int8 ;
typedef u_int64_t           uint64 ;
typedef int64_t           	int64 ;


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

#define MAXPACKETS			8192

#define MAXFECS				13
#define FEC_CARRIER			11
#define FEC_SIDEBANDS		12
#define LIMEBAND_76GHZ		10


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
    uint8               special ;                       // 0 indicates special data
    uint8               recordtype ;                    // 2 = DVB symbols, 3 = SSB
    uint16              bytesfollowing ;                // to allow unknown record types to be skipped
    uint8               dataversion ;                   // for variations of this record format
    uint8               stream ;                        // future use
    uint8               txon                [2] ;       // 0xc5 = on
    int32               symbolrate ;                    // 100, 125, 250, 333, 500, 1000
    uint32              frequency           [2] ;       // hertz, for lowband and highband
    int8                carriers            [2] ;       // 1 = carrier, 2 = alternating sidebands
    int8                dvbtype ;                       // 1 = DVB-S, 0 = DVB-S2
    int8                frametype ;
    int8                constellation ;
    int8                fec ;
    int8                pilots ;
    int8                rolloff ;
    uint32              buffering ;                     // number of bytes to receive before starting output
    uint32              spacer ;                        // anything that changes below here does not cause a r$
    int8                iloc                [2] ;       // local oscillator suppression
    int8                qloc                [2] ;
    int8                power               [2] ;       // 0-31
    int16               dialclicks ;                    // frequency offset in units of 33.060709635416Hz
    int8                pause ;
    int8                spare               [5] ;
    uint16              recordsequence ;                // should increase for every settings record
    char                magicmarker         [4] ;       // 'MJAC' to verify that this is a valid record
    uint32              crc ;                           // (currently unused)
} mjsettings_record ;

// global data

			char						title [1024] ;
volatile	int32						fhin ;
volatile	int32						fhout ;
volatile	int32						terminate ;	
volatile	uint32						bytesout ;
volatile	uint32						framecount ;
volatile	uint						returncode ;
			uint16						infoport ;
			mjsettings_record			mjsettings ;		
			DVB2FrameFormat				neonsettings ;
																					// buffer144k is working ram for dvbs2neon	
			uint8						nullpacket			[188] ;
volatile	int32						nullmodify ;
			uint8						outputbuff			[32 * 1024] ;			// pack the symbols and insert the settings here
			uint8						tspacketsbuff		[MAXPACKETS] [188] ;
volatile	int32						tspacketsindexin ;							// for packets read from the input file
volatile	int32						tspacketsindexout ;							// for packets used by the output routine
			pthread_t					input_thread ;
			pthread_t					output_thread ;
volatile	int32						input_thread_status ;
volatile	int32						output_thread_status ;
volatile	uint32						packetsin ;
volatile	uint32						packetsout ;
volatile	int32						nullmodcount ;
volatile	uint32						inputstarttime ;
			uint32						lastinfodisplaytime ;
			int32						limeband ;
			int32						limeupsample ;
			uint32						bitrateonly ;
			int							tindex ;		
			int							zindex ;		
			int							hindex ;		

			char						info				[4096] ;
			int32						localoscsettings 	[2] [32] [2] ;			// 2 bands, 31 power levels, I and Q

			char						infoip				[256] ;
			char						inputfilename 		[256] ;
			char						outputfilename 		[256] ;
			uint8 						buffer144k			[144 * 1024]  __attribute__ ((aligned (512))) ;		


// function prototypes

extern  	int32   	dvbs2neon_control	(uint32, uint32, uint32, uint32) ;
extern  	int32   	dvbs2neon_packet    (uint32, uint32, uint32) ;
			uint32		monotime_ms			(void) ;
			void		sig_handler			(int32) ;
			void*		input_routine		(void*) ;
			void*		output_routine		(void*) ;
			void		netprint			(char*) ;
			void		display_help		(void) ;
			void		myexit				(void) ;
		
int main (int argc, char *argv[])
{
	int					argindex ;
	uint32				symbolrate ;
	int32				status ;
	int32				x ;
	int32				y ;
	uint32				tempu ;
	int64				templ ;
	int32				tempiloc ;
	int32				tempqloc ;
	int32				temp ;
	uint8				packetsinframe ;
	char				temps				[512] ;
	int					ch ;
	char				*pos ;
	int					errors ;
	int					mainband ;		
	int					otherband ;		
	FILE				*ip ;
	int32				p0, p1, p2, p3 ;

    terminate = 0 ;
    signal (SIGINT,  sig_handler) ;
    signal (SIGTERM, sig_handler) ;
    signal (SIGHUP,  sig_handler) ;
    signal (SIGPIPE, sig_handler) ;

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
	tspacketsindexin		= 0 ;
	tspacketsindexout		= 0 ;
	input_thread			= 0 ;
	output_thread			= 0 ;
	input_thread_status		= 0 ;
	output_thread_status	= 0 ;
	limeband				= -1 ;
	limeupsample			= -1 ;
	mainband				= -1 ;		
	otherband				= -1 ;
	fhout 					= -1 ;
	fhin 					= -1 ;
  
	memset (&mjsettings, 		0, sizeof(mjsettings)) ;			 
	memset (&neonsettings, 		0, sizeof(neonsettings)) ;				
	memset (&localoscsettings, 	0xff, sizeof(localoscsettings)) ;				

	mjsettings.iloc[0]			= -1 ;
	mjsettings.qloc[0]			= -1 ;
	mjsettings.iloc[1]			= -1 ;
	mjsettings.qloc[1]			= -1 ;
	mjsettings.dvbtype			= -1 ;
	mjsettings.constellation	= -1 ;
	mjsettings.fec 				= -1 ;
	mjsettings.power[0]			= -1 ;
	mjsettings.power[1]			= -1 ;
	returncode 					= 0 ;
	errors 						= 0 ;

	strcpy (inputfilename, 	"") ;
	strcpy (outputfilename,	"") ;
	strcpy (infoip, 		"") ;
	strcpy (info, 			"") ;
	strcpy (title, 			"") ;
	
	memset (nullpacket, 0xff, sizeof(nullpacket)) ;
	nullpacket [0] = 0x47 ;
	nullpacket [1] = 0x1f ;
	nullpacket [2] = 0xff ;
	nullpacket [3] = 0x00 ;

	sprintf (title+strlen(title), "\r\n") ;
	sprintf (temps, "%s: reads a transport stream and sends digital symbols to a Muntjac ", VERSIONX) ;
	sprintf (title+strlen(title), "%s\r\n", temps) ;
	memset (temps, '=', strlen(temps)) ;
	sprintf (title+strlen(title), "%s\r\n", temps) ;

// find the frequency parameter, as that needs to be known first to select lowband or highband
// put the input parameters into a string for display

	sprintf (title+strlen(title), "Command line: ") ;
	for (argindex = 0 ; argindex < argc ; argindex++)
	{
		sprintf (title+strlen(title), "%s ", argv[argindex]) ;
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
	sprintf (title+strlen(title), "\r\n") ;
	memset (temps, '-', strlen(temps)) ;
	sprintf (title+strlen(title), "%s\r\n", temps) ;

	if (tindex && zindex && (tindex > zindex) && hindex == 0)
	{
		sprintf (info+strlen(info), "*Frequency not set* \r\n") ;
		returncode |= ERROR_FREQUENCY ;
		myexit() ;
	}


// debug output

	strcpy (infoip, "127.0.0.1 9979") ; /////////////////////////////


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


 // if -h for help is specified, no need to process further

	if (hindex)
	{
		strcpy (title, "") ;
		strcpy (info, "") ;
		display_help() ;
		returncode = 0 ;
		myexit() ;
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

			if ((tempu >= 389.5e6 && tempu <= 510e6) || (tempu >= 779e6 && tempu <= 1020e6))
			{
				if (mainband == -1)
				{
					mainband = 0 ;
					mjsettings.frequency [0] = tempu ;
					mjsettings.txon		 [0] = 0xc5 ;						// must be set to this value to transmit				
				}
				else if (otherband == -1)
				{
					sprintf (info+strlen(info), "*Frequency set twice* \r\n") ;
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
						sprintf (info+strlen(info), "*Band already used* \r\n") ;
						returncode |= ERROR_SECONDARYFREQ ;						
						continue ;
					}					
				}
			}	
			else if (tempu >= 2390e6 && tempu <= 2490e6)
			{
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
						sprintf (info+strlen(info), "*Band already used* \r\n") ;
						returncode |= ERROR_SECONDARYFREQ ;						
					}					
				}
			}	
/*			
			else if (tempu >= 3400e6 && tempu <= 3410e6 && mainband == -1 && otherband == -1)
			{
///				otherband = 1 ;
				mjsettings.frequency [1] = 2390e6 ;
				mjsettings.txon		 [1] = 0xc5 ;								// must be set to this value to transmit				
				mjsettings.power	 [1] = 28 ;	
				mjsettings.carriers	 [1] = 1 ;	
				mainband = 0 ;
				mjsettings.frequency [0] = tempu - mjsettings.frequency [1] ;
				mjsettings.txon		 [0] = 0xc5 ;								// must be set to this value to transmit								
			}
*/
			else
			{
				sprintf (info+strlen(info), "*Frequency invalid (%.6f)* \r\n", ((double) tempu) / 1000000) ;
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
				switch (tempu)
				{
					case  333000:	mjsettings.symbolrate = 333 ; nullmodify = 1000 ; break ;
					case  100000:
					case  125000:
					case  250000:
					case  333333:
					case  500000:
					case 1000000: 	mjsettings.symbolrate = tempu / 1000 ; break ;			
					default: 		sprintf (info+strlen(info), "*Symbol rate invalid* \r\n") ; 
									symbolrate = 0 ; 
									if (otherband != -1)
									{
										returncode |= ERROR_SECONDARYSR ; 
									}
									else
									{
										returncode |= ERROR_SYMBOLRATE ; 
									}
									break ;
				} ;
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

			sprintf (temps, "%04d", temp) ;
			if 
			(
				   temp < 0 
				|| strlen(argv[argindex]) != 4 
				|| strcmp(argv[argindex], temps)
				|| (tempiloc > 63 && tempiloc != 99)
				|| (tempqloc > 63 && tempqloc != 99)
			)
			{
				tempiloc = -2 ;
				tempqloc = -2 ;
				sprintf (info, "*LO settings error* \r\n") ;
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
				sprintf (info, "*Frequency not set - parsing stopped* \r\n") ;
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
		else if (strcmp (argv[argindex], "-w") == 0)							// 'UDP debug output (default 127.0.0.1:9979)
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
			sprintf (info+strlen(info), "*Unknown parameter: %s* \r\n", argv[argindex]) ;
			returncode |= ERROR_PARAMETER ; 
			if (hindex == 0)
			{
				myexit() ;
			}
		}
		argindex++ ;
	} ;

// check parameters

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
		sprintf (info, "*Frequency not set - parsing stopped* \r\n") ;
		returncode |= ERROR_FREQUENCY ;
		myexit() ;
	}

/*
	if (mjsettings.txon[0] == 0xc5)
	{
		if (mjsettings.carriers[0] != 1 && mjsettings.power[0] > 31)
		{
			mjsettings.power[0] = 31 ;
			sprintf (info, "*Lowband power capped at 0.31* \r\n") ;
		}
		else if (mjsettings.power[0] > 31)
		{
			mjsettings.power[0] = 31 ;
			sprintf (info, "*Lowband power capped at 0.31* \r\n") ;
		}
	}
*/

	if (mjsettings.txon[1] == 0xc5)
	{
		if (mjsettings.power[1] > 31)
		{
			mjsettings.power[1] = 31 ;
			sprintf (info, "*Highband power capped at 0.31* \r\n") ;
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
			sprintf (info+strlen(info), "*Secondary frequency invalid* \r\n") ;
			returncode |= ERROR_SECONDARYFREQ ;
		}
	}

// check for defaults

	strcpy (temps, "") ;

	if (inputfilename[0] == 0)
	{
		strcpy (inputfilename, "/dev/stdin") ;								// default
		sprintf (temps+strlen(temps), "-i %s  ", inputfilename) ;
	}

	if (outputfilename[0] == 0)
	{
		strcpy (outputfilename, "/dev/ttyMJ0") ;
		sprintf (temps+strlen(temps), "-o %s  ", outputfilename) ;			// default
	}

	if (mjsettings.dvbtype == -1)								// no parameter set
	{
		sprintf (temps+strlen(temps), "-m M_DVBS2  ") ;			
		mjsettings.dvbtype = M_DVBS2 ;							// default to DVB-S2
	}
	else if (mjsettings.dvbtype == -2)
	{
		sprintf (info+strlen(info), "*Modulation invalid* \r\n") ;					
		returncode |= ERROR_DVB ;
	}

	if (mjsettings.constellation == -1)								// no parameter set
	{
		sprintf (temps+strlen(temps), "-c M_QPSK  ") ;
		mjsettings.constellation = M_QPSK ;							// default to qpsk
	}
	else if (mjsettings.constellation == -2)
	{
		sprintf (info+strlen(info), "*Constellation invalid* \r\n") ;
		returncode |= ERROR_DVB ;
	}

/*
// compensate for PD power modification

	if (limeupsample == 1)
	{
		mjsettings.power [mainband] += 6 ;							
		sprintf (info+strlen(info), "Power corrected to 0.%02d \r\n", mjsettings.power[mainband]) ;
	}
*/

	if (strlen (temps))
	{
		sprintf (info+strlen(info), "Default:    %s \r\n", temps) ;
	}
	
// Setting LimeMini transverter band to 76GHz will select alternating sideband mode

	if (limeband == LIMEBAND_76GHZ)
	{
		mjsettings.symbolrate = 0 ;
		mjsettings.carriers [mainband] = 2 ;
	}

	tempu = mjsettings.frequency [mainband] ;

/*
	if (tempu >= 3400e6 && tempu <= 3410e6)
	{	
		mjsettings.carriers		[0] = 	mjsettings.carriers	[mainband] ;  	 
		mjsettings.power  	 	[0] = 	mjsettings.power 	[mainband] ;
		mjsettings.power  	 	[1] =	28 ;
		mjsettings.carriers  	[1] = 	1 ;
		mjsettings.frequency 	[1] = 	2390e6 ;
		mjsettings.frequency 	[0] = 	tempu - mjsettings.frequency [1] ;
		mjsettings.txon		 	[0] = 	0xc5 ;
		mjsettings.txon		 	[1] = 	0xc5 ;
	}
*/

// debug output

	strcpy (temps, "") ;
	
	if (mjsettings.frametype == FRAME_NORMAL)
	{
		sprintf (temps+strlen(temps), "Frame=LONG  ") ;
	}

	if (mjsettings.pilots == PILOTS_OFF)
	{
		sprintf (temps+strlen(temps), "Pilots=OFF  ") ;
	}

	if (strlen (temps))
	{
		sprintf (info+strlen(info), "Default:    %s \r\n", temps) ;
	}

	if 
	(
		   (mainband  != -1 && mjsettings.carriers[mainband]  == 2) 
		|| (otherband != -1 && mjsettings.carriers[otherband] == 2) 
	)
	{
		mjsettings.constellation = M_QPSK ;
	}
	
// copy the settings for dvbs2neon

	neonsettings.frametype 		= mjsettings.frametype ;
	neonsettings.fec 			= mjsettings.fec ;
	neonsettings.rolloff 		= mjsettings.rolloff ;
	neonsettings.constellation 	= mjsettings.constellation ;
	neonsettings.pilots 		= mjsettings.pilots ;

// set up record 2 for tx settings

	mjsettings.special 					= 0 ;									// special data
	mjsettings.recordtype				= 2 ;									// record type
	mjsettings.bytesfollowing 			= sizeof (mjsettings) - 3 ;
	mjsettings.dataversion				= 1 ;									
	mjsettings.constellation 	 		= neonsettings.constellation ;
	mjsettings.rolloff		 	 		= neonsettings.rolloff ;
	mjsettings.magicmarker		[0]		= 'M' ;				
	mjsettings.magicmarker		[1]		= 'J' ;				
	mjsettings.magicmarker		[2]		= 'A' ;				
	mjsettings.magicmarker		[3]		= 'C' ; 				
	mjsettings.crc						= 0 ;									// not used yet	

	if (mjsettings.symbolrate)
	{

// reset dvbs2neon completely and pass buffer pool

		status = dvbs2neon_control (0, CONTROL_RESET_FULL, (uint32) &buffer144k, sizeof (buffer144k)) ;   
		if (status < 0)
		{
			sprintf (info+strlen(info), "dvbs2neon1 error (%d)\r\n", status) ;
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

		if (returncode == 0)
		{
			tempu = (uint64) status * symbolrate / 1000000 ;
			sprintf (info+strlen(info), "Efficiency: %d \r\n", status) ;
			sprintf (info+strlen(info), "Bit rate:   %u \r\n", tempu) ;
		}

		if (bitrateonly)
		{
			if ((returncode & (0xff000000 + ERROR_DVB)) == 0)
			{
				strcpy (title, "") ;
				sprintf (info, "Net TS bitrate input should be %d \r\n", tempu) ;
			}
			strcpy (outputfilename, "") ;
			returncode = 0 ; 
			myexit() ;
		}
	}	

	sprintf (info+strlen(info), "\r\n") ;


// open the input file

	fhin = open (inputfilename, O_RDONLY | O_NONBLOCK) ;
	if (fhin < 0)
	{
		sprintf (info+strlen(info), "*Cannot open input file %s* \r\n", inputfilename) ;
		returncode |= ERROR_INPUT ;
	}


// open and lock the output file or serial port

	if (strstr(outputfilename, "tty"))
	{
    	fhout = open (outputfilename, O_WRONLY | O_NONBLOCK) ;
	}
	else
	{
    	fhout = open (outputfilename, O_WRONLY | O_NONBLOCK) ;
	}
    if (fhout < 0)
    {
        sprintf (info+strlen(info), "*Cannot open output file %s*\r\n", outputfilename) ;
		returncode |= ERROR_OUTPUT ;
	}
	else
	{


// lock the output file
	
		status = flock (fhout, LOCK_EX | LOCK_NB) ;
    	if (status < 0)
	    {
    	    sprintf (info+strlen(info), "*Cannot lock output file %s*\r\n", outputfilename) ;
			returncode |= ERROR_OUTPUT ;
    	}
    	else
    	{


// set the parameters for output to a serial port

		    if (strncmp (outputfilename, "/dev/tty", 8) == 0)
    		{
	    		sprintf (temps, "stty -F %s raw -echo > /dev/null",  outputfilename) ;
        		system (temps) ;
    		}
		}
    }

	if (returncode != 0)
	{
		myexit() ;
	}


// apply local oscillator suppression settings from muntjac.mjo, if available

	memset (localoscsettings, 0xff, sizeof(localoscsettings)) ;						// preset all to -1

	ip = fopen ("/home/pi/rpidatv/bin/muntjac.mjo", "r") ;
	if (ip == 0)
	{
		ip = fopen ("muntjac.mjo", "r") ;
	}

	if (ip)
	{
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
			if (mjsettings.iloc[x] == -1 && mjsettings.qloc[x] == -1)
			{
				strcpy (temps, "") ;
				if (mjsettings.frequency[x] >= 430e6 && mjsettings.frequency[x] <= 450e6)
				{
					strcpy (temps, "LOWBAND") ;
				}
				else if (mjsettings.frequency[x] >= 2390e6 && mjsettings.frequency[x] <= 2490e6)
				{
					strcpy (temps, "HIGHBAND") ;
				}
				else
				{
					strcpy (temps, "") ;
				}
				if (temps[0])
				{
					tempu = mjsettings.power [x] ;
					mjsettings.iloc [x] = localoscsettings [x] [tempu] [0] ;				
					mjsettings.qloc [x] = localoscsettings [x] [tempu] [1] ;				
					if (mjsettings.iloc[x] == -1 && mjsettings.qloc[x] == -1)				// in case settings are available only for power 0, 4, 8 . . .
					{
						tempu &= ~3 ;
						mjsettings.iloc [x] = localoscsettings [x] [tempu] [0] ;				
						mjsettings.qloc [x] = localoscsettings [x] [tempu] [1] ;				
					}
					if (mjsettings.iloc[x] != -1 || mjsettings.qloc[x] != -1)				// in case settings are available only for power 0, 4, 8 . . .
					{
						sprintf (info+strlen(info), "Using %s LO suppression settings from muntjac.mjo \r\n", temps) ;	
					}
				}
			}
		}
	}	

///		sprintf (info+strlen(info), "Using LO suppression settings from muntjac.mjo \r\n") ;	

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

	usleep (100000) ; /////////////
	
	if (fhin >= 0 && mjsettings.symbolrate)
	{
		status = pthread_create (&input_thread, 0, input_routine, 0) ; 
		if (status != 0) 
		{
			sprintf (info+strlen(info), "*Cannot create input process thread* \r\n") ;
			myexit() ;
		}
		else
		{
			sprintf (info+strlen(info), "Input process thread created \r\n") ;
		}
	}
	
	status = pthread_create (&output_thread, 0, output_routine, 0) ; 
	if (status != 0) 
	{
		sprintf (info+strlen(info), "*Cannot create output process thread* \r\n") ;
		myexit() ;
	}
	else
	{
		sprintf (info+strlen(info), "Output process thread created \r\n") ;
	}

	sprintf (info+strlen(info), "Main loop is starting \r\n") ;
	
	if (strcmp(outputfilename, "/dev/stdout") != 0)
	{
		printf (title) ;
		printf (info) ;
	}

	netprint (title) ;
	netprint (info) ;
	strcpy   (info, "") ;
	strcpy   (title, "") ;
	
// main loop

	input_thread_status = 1 ;									// start the input thread
	lastinfodisplaytime = 0 ;

	while (terminate == 0)
	{
		usleep (100000) ;

		tempu = monotime_ms() ;
		if (lastinfodisplaytime && (tempu - lastinfodisplaytime >= 1000))
		{
			lastinfodisplaytime = tempu ;

			strcpy (temps, "");
			sprintf (temps+strlen(temps), " ================================================= \r\n") ;
			sprintf (temps+strlen(temps), " PD:%s \r\n", VERSIONX) ;
			sprintf (temps+strlen(temps), " ---------------------- \r\n") ;
			sprintf (temps+strlen(temps), " FR:%.3f  SR:%u  FEC:%s  C:%u  PWR:%d\r\n", (float)mjsettings.frequency[mainband] / 1000000, mjsettings.symbolrate, fecs[mjsettings.fec], mjsettings.constellation * 4 + 4, mjsettings.power[mainband]) ;
			sprintf (temps+strlen(temps), " ------------------------------------------------- \r\n") ;
			sprintf (temps+strlen(temps), " PacketsIn  PacketsOut  Frames  KbytesOut    Nulls \r\n") ;
			sprintf (temps+strlen(temps), " ------------------------------------------------- \r\n") ;
			sprintf (temps+strlen(temps), " %9u  %10u  %6u  %9u  %+7d \r\n", packetsin, packetsout, framecount, bytesout / 1024, nullmodcount) ;			
			sprintf (temps+strlen(temps), " ================================================= \r\n") ;
			if (strcmp(outputfilename, "/dev/stdout") != 0)
			{
				printf ("\r\n%s",temps) ;
			}
			netprint (temps) ;
		}

		if (output_thread_status == 0)
		{
			if ((mjsettings.symbolrate == 0) || (inputstarttime && monotime_ms() - inputstarttime >= 600))
			{
				output_thread_status = 1 ;													// let the output thread run
				usleep (1000000) ;
				lastinfodisplaytime = monotime_ms() ;
			}
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

// end of main loop

	sprintf (info+strlen(info), "\r") ;
	sprintf (info+strlen(info), "Main loop is stopping \r\n") ;
	output_thread_status = 2 ;


// give the output thread 750ms to send shutdown settings to Muntjac

	mjsettings.txon[0] = 0 ;
	mjsettings.txon[1] = 0 ;

	tempu = mjsettings.recordsequence ;
	x = 75 ;
	do
	{
		usleep (10000) ;
		x-- ;
	} while (tempu == mjsettings.recordsequence && x > 0) ;

	if (output_thread) 
	{
		while (output_thread_status != 4) ;
///		output_thread_status = 3 ;
///		pthread_join (output_thread, 0) ;
///		output_thread = 0 ;
	}

	if (input_thread && input_thread_status) 
	{
		input_thread_status = 3 ;
		pthread_join (input_thread, 0) ;
		input_thread = 0 ;
	}

	myexit() ;
}

void netprint (char *string)
{
	char	temps [1024] ;
	
	if (infoip[0])
	{
		sprintf (temps, "echo '%s' | netcat -uw0 %s", string, infoip) ;
		system (temps) ;			
	}
}

void* input_routine (void* dummy)
{
	int32		status ;
	int32		temp ;
	int32		x ;
	uint32		inputpacketbufflength ;
	uint8		inputpacketbuff		[188 * 2] ;

	while (input_thread_status != 1)			// wait for thread to be enabled
	{
		usleep (5000) ;
	}

///	sprintf (info+strlen(info), "Input thread is starting \r\n") ;
	
	inputstarttime 			= 0 ;
	inputpacketbufflength 	= 0 ;

	while (input_thread_status == 1)
	{
		temp = tspacketsindexin - tspacketsindexout ;
		if (temp < 0)
		{
			temp += MAXPACKETS ;
		}
		if (temp >= MAXPACKETS * 15 / 16)
		{
			usleep (1000) ;
			continue ;
		}

		status = read (fhin, inputpacketbuff + inputpacketbufflength, 188 * 2 - inputpacketbufflength) ;		// get a packet
		if (status < 0)														// restart from beginning of file
		{
			if (errno == EAGAIN)
			{
				usleep (1000) ;
				continue ;													// no data available
			}
			else
			{
///				printf ("An input error has occurred (%d) \r\n", errno) ;
				returncode = ERROR_INPUTREAD ;
				input_thread_status = 4 ;
				continue ;
			}
		}
		else if (status == 0 && strstr (inputfilename, ".ts"))				// restart from beginning of file	
		{
			lseek (fhin, 0, SEEK_SET) ;
			inputpacketbufflength = 0 ;
			memset (inputpacketbuff, 0, sizeof(inputpacketbuff)) ;
			continue ;
		}
		else
		{
			inputpacketbufflength += status ;
		}

		if (inputpacketbufflength < 188 * 2)
		{
			continue ;
		}

		x = 0 ;
		while (x < 188 && inputpacketbuff[x] != 0x47 && inputpacketbuff [x + 188] != 0x47)
		{
			x++ ;
		} ;

		if (x > 0)
		{
			memmove (inputpacketbuff, inputpacketbuff + x, sizeof (inputpacketbuff) - x) ;
			inputpacketbufflength -= x ;
			memset (inputpacketbuff + inputpacketbufflength, 0, sizeof(inputpacketbuff) - inputpacketbufflength) ;
		}

		memcpy (tspacketsbuff[tspacketsindexin], inputpacketbuff, 188) ;
		tspacketsindexin++ ;
		if (tspacketsindexin >= MAXPACKETS)
		{
			tspacketsindexin = 0 ;
		}

		memcpy (inputpacketbuff, inputpacketbuff + 188, 188) ;
		memset (inputpacketbuff + 188, 0, 188) ;
		inputpacketbufflength -= 188 ;

		packetsin++ ;
		if (packetsin == 1)
		{
			inputstarttime = monotime_ms() ;
		}
	}

///	sprintf (info+strlen(info), "\rInput thread is stopping \r\n") ;
}

/*
	on return from dvbs2neon_packet:

		status (signed) indicates:	
		0							more packets are needed to fill a frame
		< 0 && >= ERROR_LOWEST		an error has occured
		else						(uint32) status is a pointer to a buffer of symbols
									one symbol per byte, IQ in bits 1-0 for QPSK

		the symbols for 2 frames are combined before output, 
		to avoid filling only half a byte when packing symbols
*/


///xxx


void* output_routine (void* dummy)
{
	uint		tempu ;
	int			temp ;
	int			x ;
	int			y ;
	int			status ;
	int			packetsinframe ;
	int			symbolcount ;
	uint8*		symbolpointer ;
	int32		symbolstosend ;
	uint8*		firstframepointer ;
	uint8*		secondframepointer ;
	uint8*		framepointer ;
	uint8*		outputpointer ;
	uint32		bytestosend ;
	uint8*		packetpointer ;
	uint32		lastrecord2time ;
	
	while (output_thread_status == 0)								// wait for thread to be enabled
	{
		usleep (5000) ;
	}

///	sprintf (info+strlen(info), "Output thread is starting \r\n") ;

	packetpointer	 	= 0 ;
	tspacketsindexout 	= 0 ;
	packetsinframe 		= 0 ;
	lastrecord2time		= 0 ;
	outputpointer		= outputbuff ;
	
	while (output_thread_status < 3)											// continue while thread enabled
	{

// check for time to send settings record

		tempu = monotime_ms() ;
		if (tempu - lastrecord2time >= 250 || output_thread_status == 2)
		{
			lastrecord2time	= tempu ;
			if (output_thread_status == 2)
			{
				outputpointer = outputbuff ;
				output_thread_status = 3 ;							// exit next time around the loop
			}
/*
			memset 	    		   (outputpointer, 0xff, 256) ;	// to neutralise any stuffing
			outputpointer 		+= 256 ;
			mjsettings.txon [0]	 = 0 ;
			mjsettings.txon [1]  = 0 ;
*/
			mjsettings.recordsequence++ ;					
			memcpy (outputpointer, &mjsettings, sizeof (mjsettings)) ;		
			outputpointer += sizeof (mjsettings) ;
///			printf ("SEQ=%d \r\n", mjsettings.recordsequence) ;
		}

// write to the output file - non blocking, so it will go around the loop a few times

		bytestosend = outputpointer - outputbuff ;
		if (bytestosend)
		{
	       	outputpointer = outputbuff ;
    	   	do
	    	{
				if (bytestosend > 1024)
				{
					tempu = 1024 ;
				}
				else
				{
					tempu = bytestosend ;
				}
					
	       	    status = write (fhout, outputpointer, tempu) ; 
    	       	if (status >= 0)
	    	    {
	    	       	outputpointer 	+= status ;
					bytesout 		+= status ;
        	   	    bytestosend 	-= status ;
		        }
    	       	else if (errno != EAGAIN)
        	   	{
///					printf ("An output error has occurred (%d %d) \r\n", status, errno) ;
					returncode = ERROR_OUTPUTWRITE ;
					output_thread_status = 4 ;
	   	            break ;
    	   	    }
        	    else
	           	{
    	           	usleep (5000) ;
        	    }
        	    
   	    	} while (bytestosend > 0) ;
		}


		outputpointer = outputbuff ;
		packetpointer = 0 ;

		if (mjsettings.symbolrate && nullmodify)
		{
			if (nullmodify > 0)
			{
				if (packetsout && (packetsout % (nullmodify + 1)) == 0)
				{
					packetpointer = nullpacket ;
					nullmodcount++ ;
				}
			}
		}

		if (packetpointer == 0 && tspacketsindexout != tspacketsindexin)
		{
			packetpointer = tspacketsbuff [tspacketsindexout] ;		// get a packet from the input store
			tspacketsindexout++ ;
			if (tspacketsindexout >= MAXPACKETS)
			{
				tspacketsindexout = 0 ;								// wrap around
			}	
		}
		
		if (mjsettings.symbolrate == 0 || output_thread_status == 2)
		{
			packetpointer = 0 ;										// carrier modes do not need packets, even if available
		}

		if (packetpointer)
		{
			packetsout++ ;
	       	status = dvbs2neon_packet (STREAM0, (uint32) packetpointer, 0) ;	// give dvbs2neon a packet to process
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
				framecount++ ;
				symbolpointer = (uint8*) status ;
				symbolcount   = ((uint16*) symbolpointer) [-1] ;		// the 2 bytes before the pointer target

				outputpointer = outputbuff ; 							// reset the output buffer pointer

				if (neonsettings.constellation == M_QPSK)				// pack 4 x 2 bit symbols into each output byte
				{
					symbolstosend  = symbolcount / 4 ;
					symbolstosend *= 4 ;
					for (x = 0 ; x < symbolstosend ; x += 4)			// pack the symbols into bytes
					{
						tempu = 0 ;
						for (y = 0 ; y < 4 ; y++) 
						{
							tempu <<= 2 ;
							tempu |= symbolpointer [x + y] ;
						}
						if (tempu == 0 || tempu == '#')
						{
							*outputpointer++ = 0 ;						// stuff a zero when a zero symbol byte is output					
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
						tempu  |= 1 ;									// record type 1 indication
						*outputpointer++ = 0 ;							// stuff a zero to indicate a record 
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
						if (tempu == 0 || tempu == '#')
						{
							*outputpointer++ = 0 ;						// stuff a zero when a zero symbol byte is output					
						}
						*outputpointer++ = tempu ;
					}
				} 
				else
				{
					symbolstosend = 0 ;
				}
			
				if (symbolstosend == 0)
				{
///					continue ;
				}
			}
		}
			
		packetsinframe = 0 ;
	}

	usleep (100000) ;
	output_thread_status = 4 ;
	
///	sprintf (info+strlen(info), "\rOutput thread is stopping \r\n") ;
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
    terminate = 1 ;
    returncode |= ERROR_SIGNAL ;
}

void myexit()
{
	uint32		tempu ;

	if (output_thread)
	{
		output_thread_status = 3 ;
		pthread_join (output_thread, 0) ;
		output_thread = 0 ;
	}

	if (input_thread)
	{
		input_thread_status = 3 ;
		pthread_join (input_thread, 0) ;
		input_thread = 0 ;
	}
		
	if (fhout >= 0)
	{
		flock (fhout, LOCK_UN) ;
		close (fhout) ;
		fhout = 0 ;
	}

	if (fhin >= 0)
	{
		close (fhin) ;
		fhin = 0 ;
	}
               
	if (returncode & 0x7f)
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

	if (returncode != 0 && returncode != 0xc0 + (ERROR_SIGNAL >> 16))
	{
		sprintf (info+strlen(info), "For help: ./muntjacsdr_dvb_0v1a -h \r\n") ;
	}

	if (bitrateonly == 0 && hindex == 0)
	{
		sprintf (info+strlen(info), "%s is exiting with return code 0x%02X \r\n", VERSIONX, returncode) ;
		sprintf (info+strlen(info), "\r\n") ;
	}
	
	netprint (title) ;
	netprint (info) ;

    if (strcmp(outputfilename, "/dev/stdout") != 0)
    {
        printf (title) ;
        printf (info) ;
    }
	strcpy (info, "") ;

	usleep (100000) ;
	exit (returncode) ;
}
    
void display_help()
{
	char	temps [256] ;
	
	printf ("\r\n") ;

	sprintf (temps, "%s: reads a transport stream and sends digital symbols to a Muntjac", VERSIONX) ;
	printf ("%s\r\n", temps) ;
	memset (temps, '=', strlen(temps)) ;
	printf ("%s\r\n", temps) ;

	printf ("-h  this help file     exits after printing \r\n") ;
	printf ("-d  print bit rate     exits after printing (value is correct for frame type and pilots) \r\n") ;
	printf ("-i  input file         (default: /dev/stdin) \r\n") ;
	printf ("-o  output file        (default: /dev/ttyMJ0) \r\n") ;
	printf ("-t  frequency          389.5e6-510.0e6, 779.0e6-1020.0e6, 2390.0e6-2490.0e6 \r\n") ;
///	printf ("                       3400.0e6-3410.0e6 (sets low band 1010-1020MHz and high band carrier 2390MHz) \r\n") ;
	printf ("-x  mixer              specify mixer type (future use \r\n") ;
	printf ("-s  symbol rate        100000, 125000, 250000, 333000, 333333, 500000, 1000000 \r\n") ;
	printf ("                       (native SR is 333333 - for 333000, a null packet is inserted every 1000 packets) \r\n") ;
	printf ("-f  FEC                1/4, 1/3, 2/5, 1/2, 3/5, 2/3, 3/4, 4/5, 5/6, 8/9, 9/10, carrier, sidebands \r\n") ;  
	printf ("                       (alternating sidebands for LO and sideband suppression test) \r\n") ;				
	printf ("-g  power              0.00 - 0.31 (max 0.15 for non-lowband carrier) \r\n") ;
	printf ("-c  constellation      QPSK, 8PSK (default: QPSK) \r\n") ;
	printf ("                       (8PSK is not available for SR1000) \r\n") ;  
	printf ("                       (DVBS2 does not support FEC 4/5 for 8PSK) \r\n") ;
	printf ("-m  modulation         DVBS2 (default: DVBS2 - DVBS is not available) \r\n") ;
	printf ("-v  short frames on    no parameter \r\n") ;
	printf ("                       (DVBS2 does not support FEC 9/10 for short frames) \r\n") ;
	printf ("-p  pilots on          no parameter \r\n") ;
	printf ("-y  LO suppression     iiqq (0000 to 6363) use 9999 for AT86RF215 internal value \r\n") ;
	printf ("                       (if not set, looks for file /home/pi/rpidatv/bin/muntjac.mjo) \r\n") ;
	printf ("-z  secondary band     no parameter \r\n") ;
	printf ("                       set -t, -g to transmit same data as main band \r\n") ;
	printf ("                       optionally set -y, -f carrier, -f sidebands \r\n") ;
	printf ("-w  UDP debug output   (default: 127.0.0.1:9979, nc -kluv 0 9979 to view) \r\n") ;
	printf ("-n  null deletion      removes a null packet for multiples of the specified value, for testing only \r\n") ;
	printf ("-q -e -r -D            accepted, but not required \r\n") ;
	printf ("Return code            8 bit - see source file for details \r\n") ;
	printf ("\r\n") ;
}










