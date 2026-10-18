#define VERSIONX "1v0p"

/*
	Creates a calibration file for adjustment of Muntjac LO suppression values
*/


/*

Change log

2026-05-05	mjcalib-1v0p		AUTO mode
2026-05-03	mjcalib-1v0k		stepped to 1v0k
2026-04-29	mjcalib-1v0j		stop transmitting before exiting
								detect I/O error
								CTRL-K to save
2026-04-28	mjcalib-1v0h		put ttyMJ0 into raw mode
2026-04-21	mjcalib-1v0g		beta testing
2025-04-02	mjcalib-1v0a		first release

*/

#define ESCAPE		0x5c
#define MAXBANDS	3
#define ESC			27


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
#include <arpa/inet.h>

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

#define FEC_CARRIER			11
#define FEC_SIDEBANDS		12
#define LIMEBAND_76GHZ		10

#define REBOOT_RECORD           0x81
#define DATV_RECORD             2
#define NARROWBAND_RECORD       3
#define EOF_RECORD              0x84
#define STATUSREQUEST_RECORD    0x86
#define BEGINFRAME_RECORD       0x88
#define ENDFRAME_RECORD         0x89


	uint32					mjfreqs     [MAXBANDS] = {437e6, 2399e6,  421e6} ;
	uint32					targetfreqs [MAXBANDS] = {437e6, 2399e6, 1263e6} ;
	uint32					filefreqs   [MAXBANDS] = {440e6, 2400e6, 1300e6} ;
	uint32					lastpower   [MAXBANDS] = {0,          0,      0} ;
												
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


typedef struct 						// record type = .0
{
	uint8		special ;			// ESCAPE indicates special data
	uint8		recordtype ;		// ESCAPE indicates that this is just a symbol byte with value zero 
} record0_zerobyte ;

typedef struct						// record type = x1 
{
	uint8		special ;			// ESCAPE indicates special data
	uint8		recordtype ;		// x1 indicates that only the upper nybble contains symbol information 
} record1_halfbyte ;

typedef struct
{
	uint8				special ;						// ESCAPE
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
    int8                dvbtype ;                       // 1 = DVB-S, 0 = DVB-S2
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
    int32               spare0 ;
    int32               spare1 ;
    char                magicmarker         [4] ;       // 'MJAC' to verify that this is a valid record
    uint32              crc ;                           // (currently unused)
} common_record ;


// global data

			char						title [1024] ;
volatile	int32						kbfd ;
volatile	int32						terminate ;	
volatile	uint32						bytesout ;
volatile	uint32						framecount ;
volatile	uint						returncode ;
			uint16						infoport ;
			mjsettings_record			mjsettings ;		
			common_record				statusrequest ;
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

			char						mjinbuff			[4096] ;
            char                        muntjacpicoversion  [256] ;
            char                        muntjacpicoserial   [256] ;
            int8						onfileoscsettings 		[MAXBANDS] [32] ;
            int8						changedoscsettings 		[MAXBANDS] [32] ;
            int8						fileoscsettings 		[MAXBANDS] [32] [2] ;
            int8						workspaceoscsettings 	[MAXBANDS] [32] [2] ;
			char						info				[4096] ;
			char						infoip				[256] ;
			char						mjsettingsfilename 	[256] ;
			char						inputfilename 		[256] ;
			char						mjfilename 			[256] ;
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
			void		print_help			(void) ;
			void		print_help0			(void) ;
			void		print_help1			(void) ;
			
				
int main (int argc, char *argv[])
{
	int					argindex ;
	uint32				symbolrate ;
	int32				status ;
	int32				x ;
	int32				y ;
	uint32				utemp ;
	int64				templ ;
	int32				tempiloc ;
	int32				tempqloc ;
	int32				temp ;
	uint8				packetsinframe ;
	char				temps				[512] ;
	int					ch ;
	int					errors ;
	int					mainband ;		
	int					otherband ;		
	FILE				*ip ;
	FILE				*op ;
	int					calband ;			// band setting in the program
	int					mjband ;			// 0 or 1 for Muntjac low / high band internally
	int32				firstpass ;
	char				*pos ;
	char				*pos2 ;
	int32				mjfdout ;
	uint				now, last ;
	char				chx ;
	int					kbfd ;
	int					change ;
    int32               p0, p1, p2, p3 ;
    int32				itemp, qtemp ;
	int32				changesaftersaving [MAXBANDS] ;
	int32				lastkey ;
	uint32				then ;

    struct sockaddr_in  mkrsockaddr ;
    float               mkrvalue ;
    float               mkrminvalue ;
    int32               mkrsequence = 0 ;
    int32               mkrsock     = 0 ;
    int32               mkrstatus ;
    char                mkrbuff         [256] ;
    int32               mkrloc          [2] ;
    int32               mkrlocmin       [2] ;
    int32               mkrlocmiddle    [2] ;
    int32               mkrlocstep ;
    int32               mkrpower ;
    int32               mkrchange ;
    int32               mkrstate ;
    int32               mkriq ;
    uint32              mkrtime ;
    uint32              mkrudptime ;
    uint32              mkrudpcount ;

    enum                MKR_STATES  {
                                        MKR_IDLE, MKR_BEGIN, MKR_NEWPOWER, MKR_MIDDLE,
                                        MKR_ABOVE, MKR_BELOW, MKR_FINISHED
                                    } ;

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
	kbfd 					= -1 ;
	firstpass				= 0 ;
	lastkey					= 0 ; 
  	
	memset (&changesaftersaving, 	 0, sizeof(changesaftersaving)) ;
	memset (&mjsettings, 			 0, sizeof(mjsettings)) ;
	memset (&workspaceoscsettings,  31, sizeof(workspaceoscsettings)) ;			// working values
	memset (&fileoscsettings,   	31, sizeof(fileoscsettings)) ;				// file values
	memset (&changedoscsettings,   	 0, sizeof(changedoscsettings)) ;				
	memset (&onfileoscsettings,   	 0, sizeof(fileoscsettings)) ;				

// set up DATV record 

	mjsettings.special 					= ESCAPE ;								// special data
	mjsettings.recordtype				= DATV_RECORD ;							// record type
	mjsettings.bytesfollowing 			= sizeof (mjsettings) - 3 ;
	mjsettings.dataversion				= 1 ;									
	mjsettings.frequency [0] 			= 0 ;
	mjsettings.frequency [1] 			= 0 ;
	mjsettings.power [0] 				= 0 ;
	mjsettings.power [1] 				= 0 ;
	mjsettings.carriers [0] 			= 2 ;									// sidebands
	mjsettings.carriers [1] 			= 2 ;									// sidebands
	mjsettings.iloc [0] 	 			= 99 ;									
	mjsettings.qloc [0]  				= 99 ;									
	mjsettings.iloc [1]  				= 99 ;									
	mjsettings.qloc [1]  				= 99 ;									
	mjsettings.symbolrate 				= 0 ;
	mjsettings.magicmarker		[0]		= 'M' ;				
	mjsettings.magicmarker		[1]		= 'J' ;				
	mjsettings.magicmarker		[2]		= 'A' ;				
	mjsettings.magicmarker		[3]		= 'C' ; 				
	mjsettings.crc						= 0 ;									// not used yet	
	mjsettings.txon [0]					= 0 ;
	mjsettings.txon [1]					= 0 ;

// set up status request record

	memset ((void*) &statusrequest, 0, sizeof(statusrequest)) ;
	statusrequest.special				= ESCAPE ;
	statusrequest.recordtype			= STATUSREQUEST_RECORD ;
	statusrequest.bytesfollowing		= sizeof (statusrequest) - 3 ;
    statusrequest.magicmarker [0]       = 'M' ;
    statusrequest.magicmarker [1]       = 'J' ;
    statusrequest.magicmarker [2]       = 'A' ;
    statusrequest.magicmarker [3]       = 'C' ;

	print_help0() ;

	ip = popen ("fuser /dev/ttyMJ0 2>null", "r") ;
	memset (temps, 0, sizeof(temps)) ;
	fread (temps, 1, sizeof(temps), ip) ;
	fclose (ip) ;

	if (atoi(temps) > 0)
	{
		printf ("The Muntjac on /dev/ttyMJ0 appears to be in use \r\n") ;
		printf ("================================================================================ \r\n") ; 
		printf ("mjcalib-%s exiting \r\n\r\n", VERSIONX) ;
		exit (1) ;
	}
	
	mjfdout = open ("/dev/ttyMJ0", O_RDWR | O_NONBLOCK) ;
	if (mjfdout < 0)
	{
		printf ("A Muntjac on /dev/ttyMJ0 was not found \r\n") ;
		printf ("================================================================================ \r\n") ; 
		printf ("mjcalib-%s exiting \r\n\r\n", VERSIONX) ;
		exit (2) ;
	}
	
	system ("stty -F /dev/ttyMJ0 raw -echo") ;

    write (mjfdout, (void*) &statusrequest, sizeof(statusrequest)) ;   	// send the status request record
    usleep (100000) ;                                               	// wait 100ms
    memset (temps, 0, sizeof(temps)) ;
    status = read (mjfdout, temps, sizeof(mjinbuff) - 1) ;
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
            }
        }
    }

    if (pos2 == 0)
    {
        printf ("Cannot retrieve Muntjac4 serial number \r\n") ;
		printf ("================================================================================ \r\n") ; 
		printf ("mjcalib-%s exiting \r\n\r\n", VERSIONX) ;
        close (mjfdout) ;
		exit (3) ;
    }

// read local oscillator suppression settings from muntjac.mjo, if available

	sprintf (mjsettingsfilename, "/home/pi/rpidatv/bin/%s.mjo", muntjacpicoserial) ;
    ip = fopen (mjsettingsfilename, "r") ;
    if (ip == 0)
    {
        sprintf (mjsettingsfilename, "./%s.mjo", muntjacpicoserial) ;
        ip = fopen (mjsettingsfilename, "r") ;
    }

	if (ip == 0)
	{
		strcpy (mjsettingsfilename, "") ;
	}
	else
	{
        while (!feof(ip))
        {
            fgets (temps, sizeof(temps), ip) ;
            if (temps[0] != ' ' && temps[0] != '#')
            {
                sscanf (temps, "%u %u %u %u", &p0, &p1, &p2, &p3) ;
                if (p0 == filefreqs[0] / 1e6)
                {
                    x = 0 ;
                }
                else if (p0 == filefreqs[1] / 1e6)
                {
                    x = 1 ;
                }
                else if (p0 == filefreqs[2] / 1e6)
                {
                    x = 2 ;
                }
                else
                {
                    x = -1 ;
                }
                if (x >= 0)
                {
                    if (p1 >= 0 && p1 <= 31)                                            // power
                    {
                        if (p2 < 0 || p2 > 63)
                        {
                        	p2 = 99 ;
                        }
                        if (p3 < 0 || p3 > 63)
                        {
                        	p3 = 99 ;
                        }                        
                        fileoscsettings [x] [p1] [0] = p2 ;                        // I
                        fileoscsettings [x] [p1] [1] = p3 ;                        // Q
						onfileoscsettings [x] [p1] = 1 ;
                    }
                }
            }
        }
        fclose (ip) ;
		memcpy (workspaceoscsettings, fileoscsettings, sizeof (workspaceoscsettings)) ;
	}

	strcpy (mjfilename, "/dev/ttyMJ0") ;

	system ("stty -F /dev/stdin raw -echo") ;
	kbfd = open ("/dev/stdin", O_RDONLY | O_NONBLOCK) ;

    mkrsequence = 0 ;
   	mkrsock     = 0 ;

    mkrstatus = socket (AF_INET,SOCK_DGRAM,IPPROTO_UDP) ;     
    if (mkrstatus >= 0)
    {
	    mkrsock                             = mkrstatus ;
	    mkrsockaddr.sin_family              = AF_INET ;
	    mkrsockaddr.sin_port                = htons (9939) ;
	    mkrsockaddr.sin_addr.s_addr         = INADDR_ANY ;
	    memset ((void*)&mkrsockaddr.sin_zero,0,sizeof(mkrsockaddr.sin_zero)) ;
	    mkrsockaddr.sin_addr.s_addr         = inet_addr ("127.0.0.1") ;
	    temp = 1 ;
    	mkrstatus = bind (mkrsock, (struct sockaddr*) &mkrsockaddr, sizeof (mkrsockaddr)) ;
		if (mkrstatus < 0)
		{
			printf ("Cannot bind UDP socket \r\n") ;
			mkrsock = -1 ;
		}
	}
	else
	{
		printf ("Cannot assign UDPsocket \r\n") ;
		mkrsock = -1 ;
	}
	
	print_help1() ;

	calband = 0 ;
	mjband  = calband >> 1 ;

	status = 0 ;
	change = 3 ;
	last = monotime_ms() ;

// main loop ***********************************************************

	mkrminvalue = 0 ;
	mkrtime		= monotime_ms() ;
	mkrudptime  = monotime_ms() ;
	mkrstate	= MKR_IDLE ; 
		                 
// Mkr -61.8 dB : SEQ=84070

	while (terminate == 0)
	{
		usleep (10000) ;

		do
		{
			temp = sizeof (mkrsockaddr) ;
	        status = recvfrom
	        (
	            mkrsock, mkrbuff, sizeof (mkrbuff), MSG_DONTWAIT,
	            (struct sockaddr*) &mkrsockaddr, &temp
	        ) ;
			if (status > 0)
			{		
				strcpy (temps, "Mkr ") ;
				pos = (strstr(mkrbuff, temps)) ;
				if (pos)
				{
					pos 	   += strlen (temps) ;
					mkrvalue 	= atof (pos) ;
					mkrudptime  = monotime_ms() ;
				}
				if (mkrstate != MKR_IDLE)
				{
					mkrudpcount++ ;
				}
			}
		} while (status > 0) ;
		
		
		if (mkrstate != MKR_IDLE)
		{
			printf ("\r\n%c[1AUDP=%d\r", 27, mkrudpcount) ;		
		}

		read (mjfdout, temps, sizeof(temps)) ;				// consume and ignore any output from MJ

		if ((int32)monotime_ms() - (int32)mkrtime >= 300)
		{
			mkrtime = monotime_ms() ;

			if (mkrstate != MKR_IDLE)
			{
				if (monotime_ms() - mkrudptime >= 2000)
				{
					printf ("%c[1A%c[K\r\n=== AUTO mode aborted - no UDP feed === \r\n\r\n", 27, 27) ;
					change   = 1 ;
					mkrstate = MKR_IDLE ;					
				}
			}

			if (mkrstate == MKR_IDLE)
			{
			}
			else if (mkrstate == MKR_BEGIN)
			{
				mkrpower 	= -4 ;							// will be incremented to zero in the next step
				mkrstate 	= MKR_NEWPOWER ;
			}
			else if (mkrstate == MKR_NEWPOWER)				// setup for this power
			{
				mkrpower 		   += 4 ;
				mkriq				= 0 ;					// I/Q selector
				mkrlocmiddle	[0]	= 32 ;
				mkrlocmiddle 	[1]	= 32 ;
				mkrloc			[0] = mkrlocmiddle [0] ;		// start in the middle			
				mkrloc			[1] = mkrlocmiddle [1] ;			
				mkrlocstep			= mkrlocmiddle [0] / 2 ;	// try above and below
				mkrminvalue			= 0 ;				
				mkrchange			= 1 ;
				changedoscsettings   [calband] [mkrpower] = 1 ;
				mkrstate			= MKR_MIDDLE ;
			}
			else if (mkrstate == MKR_MIDDLE)
			{
				if (mkrvalue < mkrminvalue)
				{
					mkrminvalue		  = mkrvalue ;		// lowest value seen
					mkrlocmin [mkriq] = mkrloc [mkriq] ;	// the loc setting for this value
				}
				mkrloc [mkriq]	= mkrlocmiddle [mkriq] + mkrlocstep ;
				mkrchange		= 1 ;
				mkrstate 		= MKR_ABOVE ;
			}
			else if (mkrstate == MKR_ABOVE)
			{
				if (mkrvalue < mkrminvalue)
				{
					mkrminvalue			= mkrvalue ;		// lowest value seen			
					mkrlocmin [mkriq]	= mkrloc [mkriq] ;	// the loc setting for this value
				}
				mkrloc [mkriq]	= mkrlocmiddle [mkriq] - mkrlocstep ;
				mkrchange		= 1 ;
				mkrstate 		= MKR_BELOW ;
			}
			else if (mkrstate == MKR_BELOW)
			{
				if (mkrvalue < mkrminvalue)
				{
					mkrminvalue			= mkrvalue ;			// lowest value seen
					mkrlocmin [mkriq]	= mkrloc [mkriq] ;		// the loc setting for this value
				}

				mkrloc		 [mkriq]	= mkrlocmin [mkriq] ;	// lowest loc value
				mkrlocmiddle [mkriq]	= mkrlocmin [mkriq] ;	// the middle value for next time
				mkrchange				= 1 ;

				if (mkriq == 0)
				{
					mkriq = 1 ;									// do Q

					mkrloc [mkriq] = mkrlocmiddle[mkriq] + mkrlocstep ;
					mkrstate 			= MKR_ABOVE ;
				}
				else if (mkrlocstep > 1)
				{	
					mkriq 				= 0 ;					// do I
					mkrlocstep 		   /= 2 ;

					mkrloc [mkriq] = mkrlocmiddle[mkriq] + mkrlocstep ;
					mkrstate 			= MKR_ABOVE ;
				}	
				else 
				{
					mkrtime += 1500 ;
					if (mkrpower <= 24)
					{
						mkrstate = MKR_NEWPOWER ;
					}
					else
					{
						mkrstate = MKR_FINISHED ;						// finished
					}
					change = 1 ;
				}
			}
			else if (mkrstate == MKR_FINISHED)
			{
				printf ("\r%c[K\r\n=== AUTO mode completed - settings have not yet been saved === \r\n\r\n", 27) ;
				lastpower [calband] = 0 ;
				change = 1 ;
				mkrstate 			= MKR_IDLE ;						
			}
		}

        if (mkrchange)
        {
			changesaftersaving [calband] ++ ;
            lastpower [calband]                             = mkrpower ;
            workspaceoscsettings [calband] [mkrpower] [0]   = mkrloc [0] ;
            workspaceoscsettings [calband] [mkrpower] [1]   = mkrloc [1] ;

            change |= 2 ;
            mkrchange = 0 ;
        }

		read (mjfdout, temps, sizeof(temps)) ;				// consume and ignore any output from MJ

		now = monotime_ms() ;
		if (now - last >= 250)
		{
			change |= 2 ;
			last = now ;
		}

		if (change)
		{
			if (calband >= MAXBANDS)
			{
				calband = 0 ;
			}
			mjband = calband & 1 ;

			mjsettings.frequency 	[mjband] 		= mjfreqs 	[calband] ;
			mjsettings.txon 		[mjband] 		= 0xc5 ;
			mjsettings.txon 		[mjband ^ 1]	= 0 ;
			mjsettings.power		[mjband]		= lastpower [calband] ;
			mjsettings.frequency 	[mjband] 		= mjfreqs   [calband] ;

			itemp = workspaceoscsettings [calband] [lastpower[calband]] [0] ;
			qtemp = workspaceoscsettings [calband] [lastpower[calband]] [1] ;
			mjsettings.iloc	[mjband] = itemp ;
			mjsettings.qloc	[mjband] = qtemp ; 

			mjsettings.recordsequence++ ;
   		    status = write (mjfdout, &mjsettings, sizeof(mjsettings)) ; 		
			if (status != sizeof(mjsettings)) 
			{
				printf ("\r\n*** I/O error %d *** \r\n", errno) ;
				terminate = 1 ;
			}
			
			if (change & 1)
			{
				utemp = lastpower[calband] ;
				if 
				(
					workspaceoscsettings [calband] [utemp] [0] == fileoscsettings [calband] [utemp] [0] &&
					workspaceoscsettings [calband] [utemp] [1] == fileoscsettings [calband] [utemp] [1] &&
					onfileoscsettings [calband] [utemp] 
				)
				{
					utemp = '*' ;
				}
				else
				{
					utemp = ' ' ;
				}

				printf 
				(
					"%c iloc=%02d qloc=%02d pwr=%02d freq=%.0f", 
					utemp,
					mjsettings.iloc[mjband], 
					mjsettings.qloc[mjband],
					mjsettings.power[mjband], 
					((double) targetfreqs [calband]) / 1000000
				) ;
				if (calband == 2)									// tripling
				{
					printf ("/%.0f", ((double) targetfreqs [calband]) / 3000000) ;
				} 

                if (mkrstate != MKR_IDLE)
                {
                	printf (" mkr=%.1f", mkrminvalue) ;
                	mkrminvalue = 0 ;
                }
                                                                                         				
				printf ("\r\n") ;

				if (change & 4)									// band change
				{
					then = monotime_ms() ;
					while (monotime_ms() - then < 250)			// delay and consume keyboard input
					{
						status = read (kbfd, &chx, 1) ;
						usleep (10000) ;		
					}
				}	
			}
			change = 0 ;
		}

		chx = 0 ;
		if (firstpass == 0)
		{
			status = read (kbfd, &chx, 1) ;
		}
		else
		{
			firstpass = 1 ;
			calband = MAXBANDS - 1 ;
			chx 	= 'b' ;
			status 	= 1 ;
		}
		chx = tolower (chx) ;
		if (status > 0)
		{
			if (chx == ESC)
			{
				lastkey = ESC ;
				continue ;
			}
			
			if (lastkey == ESC && chx == '[')			// cursor sequence - ignore next char
			{
				do 
				{
					usleep (10000) ;
					status = read (kbfd, &chx, 1) ;
				} while (status <= 0) ;
				lastkey = 0 ; 
				continue ;
			}

			lastkey = chx ;

			if (chx == 'l' || chx == 'C' - '@')			// l or CTRL-C
			{	
				temp = 0 ;
				for (x = 0 ; x < MAXBANDS ; x++)
				{
					temp += changesaftersaving [x] ;
				}
				if (temp)
				{
					mjsettings.txon [0] = 0 ;
					mjsettings.txon [1] = 0 ;
					mjsettings.recordsequence++ ;
   				    status = write (mjfdout, &mjsettings, sizeof(mjsettings)) ; 		
				
					printf ("\r\nThere are unsaved changes - do you want to close the program? Y/N . . .  \r\n") ;
					while (1)
					{
						usleep (10000) ;
						status = read (kbfd, &chx, 1) ;
						if (status > 0)
						{
							chx = tolower (chx) ;
							if (chx == 'y')
							{
								terminate++ ;
								break ;
							}
							else if (chx == 'n')
							{
								printf ("\r\n") ;
								terminate = 0 ;
								change = 3 ;
								break ;
							}
						}
					}
				}
				else
				{
					terminate++ ;
					continue ;
				}
			}
			else if (chx == '?')
			{
				change = 1 ;
				print_help() ;
			}
			else if (chx == 'b') 
			{
				change |= 5 ;
				calband++ ;
			}
			else if (chx == 'q')
			{
				change = 1 ;
				temp = workspaceoscsettings [calband] [lastpower[calband]] [0] ;
				temp++ ;
				if (temp >= 64)
				{
					temp = 0 ;
				}
				workspaceoscsettings [calband] [lastpower[calband]] [0]	= temp ;
				changedoscsettings   [calband] [lastpower[calband]]		= 1 ;
				changesaftersaving   [calband] ++ ;
			}	
			else if (chx == 'a')
			{
				change = 1 ;
				temp = workspaceoscsettings [calband] [lastpower[calband]] [0] ;
				temp-- ;
				if (temp < 0)
				{
					temp = 63 ;
				}
				workspaceoscsettings [calband] [lastpower[calband]] [0] = temp ;
				changedoscsettings   [calband] [lastpower[calband]] 	= 1 ;
				changesaftersaving   [calband] ++ ;
			}
			else if (chx == 'w')
			{
				change = 1 ;
				temp = workspaceoscsettings [calband] [lastpower[calband]] [1] ;
				temp++ ;
				if (temp >= 64)
				{
					temp = 0 ;
				}
				workspaceoscsettings [calband] [lastpower[calband]] [1] = temp ;
				changedoscsettings   [calband] [lastpower[calband]]		= 1 ;
				changesaftersaving [calband] ++ ;
			}	
			else if (chx == 's')
			{
				change = 1 ;
				temp = workspaceoscsettings [calband] [lastpower[calband]] [1] ;
				temp-- ;
				if (temp < 0)
				{
					temp = 63 ;
				}
				workspaceoscsettings [calband] [lastpower[calband]] [1] = temp ;
				changedoscsettings   [calband] [lastpower[calband]] 	= 1 ;
				changesaftersaving [calband] ++ ;
			}
			else if (chx == 'p')
			{
				if (mjsettingsfilename[0])
				{
					printf ("\r\n=== File settings restored for this power ===\r\n\r\n") ;
					change = 1 ;
					utemp  = lastpower [calband] ;
					workspaceoscsettings [calband] [utemp] [0] = fileoscsettings [calband] [utemp] [0] ;
					workspaceoscsettings [calband] [utemp] [1] = fileoscsettings [calband] [utemp] [1] ;
				}
			}
			else if (chx == 'P' - '@')					// CTRL-P
			{
				if (mjsettingsfilename[0])
				{
					printf ("\r\n=== File settings restored for all powers for the current band === \r\n\r\n") ;
					change = 1 ;
					utemp  = lastpower [calband] ;
					for (x = 0 ; x < 32 ; x++)
					{
						workspaceoscsettings [calband] [x] [0] = fileoscsettings [calband] [x] [0] ;
						workspaceoscsettings [calband] [x] [1] = fileoscsettings [calband] [x] [1] ;
					}
					changesaftersaving [calband] = 0 ; ;
				}
			}
			else if (chx == 'H' - '@')					// CTRL-H
			{
				if (mkrstate == MKR_IDLE)
				{
					printf ("\r\n=== AUTO mode started for the current band === \r\n\r\n") ;
					mkrudpcount = 0 ;
					mkrtime  	= monotime_ms() ;
					mkrudptime 	= monotime_ms() ;
					mkrstate 	= MKR_BEGIN ;
				}
				else
				{
					printf ("%c[1A%c[K\r\n=== AUTO mode aborted === \r\n\r\n", 27, 27) ;
					change   = 1 ;
					mkrstate = MKR_IDLE ;					
				}
			}
			else if (chx == 'r')
			{
				if (lastpower [calband] < 28)
				{
					change 				 = 1 ;
					lastpower [calband] += 4 ;
				}
			}	
			else if (chx == 'f')
			{
				if (lastpower [calband] >= 4)
				{
					change = 1 ;
					lastpower  [calband] -= 4 ;
				}
			}	
			else if (chx == 11)						// CTRL-K
			{
				memcpy (fileoscsettings, workspaceoscsettings, sizeof (fileoscsettings)) ;
				printf ("\r\n") ;
				if (mjsettingsfilename[0] == 0)
				{
					sprintf (mjsettingsfilename, "/home/pi/rpidatv/bin/%s.mjo", muntjacpicoserial) ;
				}
				op = fopen (mjsettingsfilename, "w") ;
				if (op == 0)
				{
					sprintf (mjsettingsfilename, "./%s.mjo", muntjacpicoserial) ;
					op = fopen (mjsettingsfilename, "w") ;
					if (op == 0)
					{
						printf ("Cannot open %s file for output \r\n", mjsettingsfilename) ;
						close (mjfdout) ;
						terminate = 1 ;
						continue ;
					}
				}
				fprintf (op, "#[%s]# \r\n", muntjacpicoserial) ;
				for (x = 0 ; x < MAXBANDS ; x++)
				{
					for (y = 0 ; y < 32 ; y += 1)
					{
						if (changedoscsettings[x][y] > 0 || onfileoscsettings[x][y])
						{
							fprintf (op, "%-4d ", filefreqs[x] / 1000000) ;
							fprintf (op, "%02d ", y) ;
							fprintf (op, "%02d ", fileoscsettings[x][y][0]) ;
							fprintf (op, "%02d ", fileoscsettings[x][y][1]) ;
							fprintf (op, "\r\n") ;
							onfileoscsettings[x][y] = 1 ;
						}
					}
					changesaftersaving [x] = 0 ;
				}
				fprintf (op, "#[END]# \r\n") ;
				fprintf (op, "\r\n") ;
				fclose (op) ;
				printf ("=== Settings saved to %s === \r\n", mjsettingsfilename) ;
				printf ("\r\n") ;
				change = 1 ;
			}
		}	
	}

	mjsettings.txon [0] = 0 ;
	mjsettings.txon [1] = 0 ;
	mjsettings.recordsequence++ ;
    status = write (mjfdout, &mjsettings, sizeof(mjsettings)) ; 		
	usleep (200000) ;

	close (kbfd) ;
	close (mjfdout) ;

	system ("stty -F /dev/stdin cooked echo") ;
	printf ("================================================================================ \r\n") ; 
	printf ("mjcalib-%s exiting \r\n\r\n", VERSIONX) ;
	exit (0) ;
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

void print_help()
{
	print_help0() ;
	print_help1() ;
}

void print_help0()
{
	printf ("\r\n") ; 
	printf ("================================================================================ \r\n") ; 
	printf ("mjcalib-%s : Muntjac LO suppression calibrator \r\n", VERSIONX) ; 
	printf ("================================================================================ \r\n") ; 
}

void print_help1()
{
    printf ("Muntjac: serial %s, firmware %s\r\n", muntjacpicoserial, muntjacpicoversion) ;
	if (mjsettingsfilename[0])
	{
		printf ("Initial settings were read from %s \r\n", mjsettingsfilename) ;
	}
	else
	{
		printf ("No .mjo settings file for this Muntjac was found \r\n") ;
	}
	printf ("--------------------------------------------------------------------------------\r\n") ;
	printf ("ILOC up/down : q/a      iloc values are from  0-63 and will wrap around         \r\n") ; 
	printf ("QLOC up/down : w/s      qloc values are from  0-63 and will wrap around         \r\n") ; 
	printf ("PWR  up/down : r/f      power values are from 0-28 in steps of 4 without wrap   \r\n") ; 
	printf ("BAND change  : b        %dMHz, %dMHz, %dMHz                                     \r\n", 
			targetfreqs[0] / 1000000, targetfreqs[1] / 1000000, targetfreqs[2] / 1000000) ;
	printf ("Previous     : p        restore file values for this power                      \r\n") ; 
	printf ("Previous all : CTRL-P   restore file values for all powers for this band        \r\n") ; 
	printf ("Keep / Save  : CTRL-K   keep and save all values into the .mjo file             \r\n") ; 
	printf ("AUTO on/off  : CTRL-H   toggle auto mode on/off                                 \r\n") ; 
	printf ("Leave / Exit : l        exit the program                                        \r\n") ; 
	printf ("Help         : ?        display this info                                       \r\n") ; 
	printf ("* on the left indicates that these are the values on file for this power        \r\n") ;
	printf ("--------------------------------------------------------------------------------\r\n") ;
	printf ("AUTO mode uses a marker value from Portsdown band viewer on UDP port 9939       \r\n") ;
	printf ("Mkr -61.8 dB     is the required format of the UDP data                         \r\n") ; 
	printf ("Set PD to auto gain, 2MHz span, frequency 0.4MHz above the test (LO) frequency  \r\n") ;
	printf ("The LO will appear 2 units left of centre, with sidebands 1.25 units either side\r\n") ;
	printf ("Adjust the coupling so that the sidebands just reach -5dB on power zero         \r\n") ;
	printf ("Set a manual marker on the LO                                                   \r\n") ;
	printf ("The power will go up to 28, so do not connect any amplifiers to the Muntjac     \r\n") ;
	printf ("45-60dB below sidebands should be achievable, except sometimes for power zero   \r\n") ;
	printf ("--------------------------------------------------------------------------------\r\n") ;
	printf ("\r\n") ;
} ;

