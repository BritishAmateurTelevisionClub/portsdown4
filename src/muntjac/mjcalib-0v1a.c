#define VERSIONX "0v1a"

/*

Change log

2025-04-02	mjcalib-0v1a		first release

*/

#define LOWERFREQ 437e6
#define UPPERFREQ 2400e6

/*
	Enables manual adjustment of Muntjac LO suppression values
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
			uint32						localoscsettings 	[2] [8] [2] ;

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
	int					band ;

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
  
	memset (&mjsettings, 0, sizeof(mjsettings)) ;

	band = 0 ;

	mjsettings.special 					= 0 ;									// special data
	mjsettings.recordtype				= 2 ;									// record type
	mjsettings.bytesfollowing 			= sizeof (mjsettings) - 3 ;
	mjsettings.dataversion				= 1 ;									
	mjsettings.frequency [0] 			= LOWERFREQ ;
	mjsettings.frequency [1] 			= UPPERFREQ ;
	mjsettings.power [0] 				= 0 ;
	mjsettings.power [1] 				= 0 ;
	mjsettings.carriers [0] 			= 2 ;									// sidebands
	mjsettings.carriers [1] 			= 2 ;									// sidebands
	mjsettings.iloc [0] 	 			= 32 ;									// sidebands
	mjsettings.qloc [0]  				= 32 ;									// sidebands
	mjsettings.iloc [1]  				= 32 ;									// sidebands
	mjsettings.qloc [1]  				= 32 ;									// sidebands
	mjsettings.symbolrate 				= 0 ;
	mjsettings.magicmarker		[0]		= 'M' ;				
	mjsettings.magicmarker		[1]		= 'J' ;				
	mjsettings.magicmarker		[2]		= 'A' ;				
	mjsettings.magicmarker		[3]		= 'C' ; 				
	mjsettings.crc						= 0 ;									// not used yet	
	mjsettings.txon [band]				= 0xc5 ;

	uint		now, last ;
	char		chx ;
	int			fhin ;
	int			change ;

	strcpy (outputfilename, "/dev/ttyMJ0") ;

	system ("stty -F /dev/ttyMJ0 raw -echo") ;
  	fhout = open (outputfilename, O_WRONLY) ;
    if (fhout < 0)
    {
        printf ("Cannot open output file %s\r\n", outputfilename) ;
		return (1) ;
	}

	system ("stty -F /dev/stdin raw -echo") ;
	fhin = open ("/dev/stdin", O_RDONLY | O_NONBLOCK) ;

	printf ("\r\n") ;
	printf ("mjcalib-%s : Muntjac LO suppression calibrator \r\n", VERSIONX) ;
	printf ("================================================\r\n") ;
	printf ("up/down keys: pwr q/a, iloc w/s, qloc e/d \r\n") ;
	printf ("b to change band, SPACE to quit \r\n") ;
	printf ("\r\n") ;

	change = 1 ;
	last = monotime_ms() ;
	while (terminate == 0)
	{
		usleep (1000) ;
		if (change)
		{
			printf 
			(
				"freq=%.3f pwr=%02d iloc=%02d qloc=%02d \r\n", 
				((float) mjsettings.frequency[band]) / 1000000, mjsettings.power[band], 
				mjsettings.iloc[band], mjsettings.qloc[band]
			) ;
		}
		now = monotime_ms() ;
		if (now - last >= 250)
		{
			change = 1 ;
			last = now ;
		}
		if (change)
		{
			mjsettings.recordsequence++ ;
   		    status = write (fhout, &mjsettings, sizeof(mjsettings)) ; 		
   		    if (status < 0)
   		    {
				printf ("Output error \r\n") ;
				terminate = 1 ;
   		    }
			change = 0 ;
		}
		status = read (fhin, &chx, 1) ;
		chx = tolower (chx) ;
		if (status > 0)
		{
			if (chx == ' ' || chx == 3)
			{	
				break ;
			}
			else if (chx == 'b')
			{
				mjsettings.txon [band] = 0 ;
				band ^= 1 ;
				mjsettings.txon [band] = 0xc5 ;
				change = 1 ;
			}
			else if (chx == 'w')
			{
				change = 1 ;
				mjsettings.iloc [band]++ ;
				if (mjsettings.iloc [band] > 63)
				{
					mjsettings.iloc [band] = 0 ;
				}
			}	
			else if (chx == 's')
			{
				change = 1 ;
				mjsettings.iloc [band]-- ;
				if (mjsettings.iloc [band] < 0)
				{
					mjsettings.iloc [band] = 63 ;
				}
			}
			else if (chx == 'e')
			{
				change = 1 ;
				mjsettings.qloc [band]++ ;
				if (mjsettings.qloc [band] > 63)
				{
					mjsettings.qloc [band] = 0 ;
				}
			}	
			else if (chx == 'd')
			{
				change = 1 ;
				mjsettings.qloc [band]-- ;
				if (mjsettings.qloc [band] < 0)
				{
					mjsettings.qloc [band] = 63 ;
				}
			}
			else if (chx == 'q')
			{
				change = 1 ;
				mjsettings.power [band]++ ;
				if (mjsettings.power [band] > 31)
				{
					mjsettings.power [band] = 31 ;
				}
			}	
			else if (chx == 'a')
			{
				change = 1 ;
				mjsettings.power [band]-- ;
				if (mjsettings.power [band] < 0)
				{
					mjsettings.power [band] = 0 ;
				}
			}
		}	
	}

	close (fhin) ;
	close (fhout) ;
	system ("stty -F /dev/stdin cooked echo") ;
	printf ("\r\n") ;
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


