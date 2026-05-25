/*
 *	Name:		MUS File Player
 *	Version:	1.75
 *	Author:		Vladimir Arnost (QA-Software)
 *	Last revision:	Mar-8-1996
 *	Compiler:	Borland C++ 3.1
 *
 */

/*
 * Revision History:
 *
 *	Aug-8-1994	V1.00	V.Arnost
 *		Written from scratch
 *	Aug-9-1994	V1.10	V.Arnost
 *		Some minor changes to improve sound quality. Tried to add
 *		stereo sound capabilities, but failed to -- my SB Pro refuses
 *		to switch to stereo mode.
 *	Aug-13-1994	V1.20	V.Arnost
 *		Stereo sound fixed. Now works also with Sound Blaster Pro II
 *		(chip OPL3 -- gives 18 "stereo" (ahem) channels).
 *		Changed code to handle properly notes without volume.
 *		(Uses previous volume on given channel.)
 *		Added cyclic channel usage to avoid annoying clicking noise.
 *	Aug-17-1994	V1.30	V.Arnost
 *		Completely rewritten time synchronization. Now the player runs
 *		on IRQ 8 (RTC Clock - 1024 Hz).
 *	Aug-28-1994	V1.40	V.Arnost
 *		Added Adlib and SB Pro II detection.
 *		Fixed bug that caused high part of 32-bit registers (EAX,EBX...)
 *		to be corrupted.
 *	Oct-30-1994	V1.50	V.Arnost
 *		Tidied up the source code
 *		Added C key - invoke COMMAND.COM
 *		Added RTC timer interrupt flag check (0000:04A0)
 *		Added BLASTER environment variable parsing
 *		FIRST PUBLIC RELEASE
 *	Apr-16-1995	V1.60	V.Arnost
 *		Split into modules
 *	May-1-1995	V1.61	V.Arnost
 *		Added /T option to support different ways of handling of
 *		the timers.
 *	Jul-12-1995	V1.62	V.Arnost
 *		Added short help (activated by key '?')
 *	Jul-22-1995	V1.63	V.Arnost
 *		Added volume control
 *	Aug-8-1995	V1.64	V.Arnost
 *		Added dosshell() function
 *		Default timer mode changed to TIMER_CNT140 in 32-bit models
 *	Aug-10-1995	V1.65	V.Arnost
 *		Tidied up, added comments
 *	Aug-12-1995	V1.70	V.Arnost
 *		Added AWE32 code
 *	Aug-14-1995	V1.71	V.Arnost
 *		Added MPU-401 code
 *	Aug-24-1995	V1.72	V.Arnost
 *		Added directory list searching to load a bank file from
 *		other than active directory
 *	Sep-8-1995	V1.73	V.Arnost
 *		Added instance parameter in MLinit()
 *	Oct-20-1995	V1.74	V.Arnost
 *		Added RTC timers other than 1024 Hz, default changed to 128Hz
 *	Mar-8-1996	V1.75	V.Arnost
 *		Added MIDI running status to help save MIDI-port bandwidth
 */
 
 #include "musplay.h"

#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <bios.h>
#include <conio.h>
#include <process.h>
#include <ctype.h>
#include "muslib/muslib.h"

#define VERSION	   "1.75"
#define COPYRIGHT  "MUS File Player  Version "VERSION"  (c) 1994-96 QA-Software"

//#define DEBUG			// turn on debugging messages


/* Shrink stack and heap to usable minimum */
extern unsigned _stklen = 1024;
#ifdef NEARDATAPTR
extern unsigned _heaplen = 10240;
#endif


/* Global variables */
const int	maxDriver = DRV_SBMIDI;
uint	driver = DRV_DUMMY;		/* sound driver # */
uint	port;				/* base port */
uint	handle;				/* MUS handle */
char   *bankname;			/* General MIDI bank filename */

uint	SBPort     = SBPROPORT;
uint	AWE32Port  = AWE32PORT;
uint	MPU401Port = MPU401PORT;

const char *searchPath[8] = {"", NULL};

/* Command-line parameters */
char   *musname = "TEST.mus";
char   *userbankname = NULL;
uint	help = 0;
//char   *execCmd = NULL;
#ifdef __386__
uint	timer = TIMER_CNT140;
#else
uint	timer = TIMER_RTC128;
#endif /*  __386__ */

uint	loopCount = -1;
uint	singlevoice = 0;
//uint	stereo = 1;
uint	volume = 256;

struct musicBlock *mus = NULL;


static void addSearchPath(const char *filename)
{
    if (filename)
    {
	const char **list;

	for(list = searchPath; *list; list++);
	*list = filename;
    }
}

static char *getProgramPath(char *filename)
{
    char *path = strdup(filename);
    filename = path + strlen(path);
    while (--filename >= path)
	if (*filename == '\\' || *filename == '/' || *filename == ':')
	{
	    filename[1] = '\0';
	    return path;
	}
    return NULL;
}

static int addSFBANKPath(void)
{
    char *sound = getenv("SOUND");
    if (sound)
    {
	char buf[128];
	strcpy(buf, sound);
	strcat(buf, "\\SFBANK\\");
	addSearchPath(strdup(buf));
	return 0;
    }
    return -1;
}

static int openFile(const char *filename)
{
    const char **list;
    for(list = searchPath; *list; list++)
    {
	char buf[128];
	int fd;

	strcpy(buf, *list);
	strcat(buf, filename);
	if ( (fd = open(buf, O_RDONLY|O_BINARY)) != -1 )
	    return fd;
    }
    return -1;
}

static int detectHardware(void)
{
    uint mask = MLparseBlaster("A:E:P", &SBPort, &AWE32Port, &MPU401Port);

    if (!(mask & 2) && mask & 1)
	AWE32Port = SBPort + 0x400;

    if (driver == DRV_DUMMY)
    {
	//printf("Detecting hardware ");
	if (MLdetectHardware(DRV_MPU401, MPU401Port, -1, -1))
	    driver = DRV_MPU401;

	fputc('.', stdout);
	if (MLdetectHardware(DRV_OPL2, ADLIBPORT, -1, -1))
	    driver = DRV_OPL2;

	fputc('.', stdout);
	if (MLdetectHardware(DRV_OPL3, SBPort, -1, -1))
	    driver = DRV_OPL3;

	fputc('.', stdout);
	if (MLdetectHardware(DRV_AWE32, AWE32Port, -1, -1))
	    driver = DRV_AWE32;

	/* SB MIDI is not auto-detected by default, because many */
	/* cards have a MIDI port, but only few people use it */
	//printf("\r");
    }

    /* set driver settings */
    switch (driver) {
	case DRV_OPL2:
	    port = ADLIBPORT;
	    bankname = "GENMIDI.OP2";
	    break;
	case DRV_OPL3:
	    port = SBPort;
	    bankname = "GENMIDI.OP2";
	    break;
	case DRV_AWE32:
	    port = AWE32Port;
	    bankname = "GENMIDI.AWE";
	    addSFBANKPath();
	    break;
	case DRV_MPU401:
	    port = MPU401Port;
	    bankname = NULL;
	    break;
	case DRV_SBMIDI:
	    port = SBPort;
	    bankname = NULL;
	    break;
    }

    /* report found device */
    switch (driver) {
	case DRV_DUMMY:
	    puts("No sound hardware detected. Exiting.");
	    break;
	case DRV_OPL2:
	    puts("Adlib (OPL2) detected (9 mono voices)");
	    break;
	case DRV_OPL3:
	    puts("Sound Blaster Pro II (OPL3) detected (18 stereo voices)");
	    break;
	case DRV_AWE32:
	    puts("Sound Blaster AWE32 (EMU 8000) detected (30 stereo voices)");
	    break;
	case DRV_MPU401:
	    puts("MPU-401 MIDI Interface detected");
	    break;
	case DRV_SBMIDI:
	    puts("Sound Blaster MIDI Interface detected");
	    break;
    }
    return driver;
}

void stopMusPlayTest()
{
	MLstop(handle);
	MLfreeHandle(handle);
    MLshutdownTimer();
}

bool startMusPlayTest()
{
    int fd;
    int retcode;

	addSearchPath(".\\");

    /* initialize MUSLIB */
    MLinit(0);

    /* register music drivers */
    MLaddDriver(&OPL2driver);
    MLaddDriver(&OPL3driver);
    MLaddDriver(&AWE32driver);
    MLaddDriver(&MPU401driver);
    MLaddDriver(&SBMIDIdriver);

    /* detect music hardware */
    if (!detectHardware())
	return false;

    /* initialize timer and sound hardware */
    if (MLinitTimer(timer))
    {
	//printf("FATAL ERROR: Cannot initialize timer. Aborting.\n");
	return false;
    }
//    MLinitDriver(driver);
    if (MLinitHardware(driver, port, -1, -1))
    {
	MLshutdownTimer();
	//printf("FATAL ERROR: Cannot initialize hardware. Aborting.\n");
	return false;
    }

    /* load MUS file */
    /* the file must be open in binary mode */
    if ( (fd = open(musname, O_RDONLY|O_BINARY)) == -1)
    {
	//printf("Can't open file %s\n", musname);
	return false;
    } else
	//printf("Reading file %s ... ", musname);

    handle = MLallocHandle(driver);
    if (MLloadMUS(handle, fd, 0xFFFF))
    {
	close(fd);
	//printf("Can't load file %s\n", musname);
	return false;
    }
    close(fd);
    {
	//static char *mem[] = {"Error", "Low", "EMS", "XMS"};
	//puts(mem[MLgetBlock(handle)->score.bufferType]);
    }

#ifdef DEBUG
    //printf("DEBUG: coreleft = %lu\n", (long)coreleft());
#endif

    /* load instrument bank */
    if (userbankname)
	bankname = userbankname;
    if (bankname)
    {
	if ( (fd = openFile(bankname)) == -1)
	{
	    //printf("Can't open file %s\n", bankname);
	    return false;
	} else
	    //printf("Reading file %s ...\n", bankname);
	if (MLloadBank(driver, fd, 0))
	{
	    close(fd);
	    //printf("Can't load bank %s\n", bankname);
	    return false;
	}
	close(fd);
    }
#ifdef DEBUG
    //printf("DEBUG: coreleft = %lu\n", (long)coreleft());
#endif

    MLdriverParam(driver, DP_SINGLE_VOICE, singlevoice, NULL);

    MLsetLoopCount(handle, loopCount);
    MLsetVolume(handle, volume);

    return true;
}

uint32 getMusTicks()
{
	if (mus) {
		return (uint32)((mus->time * 1000.0f) / 140.0f);
	}
	return 0;
}

void runMusPlayTest()
{
    mus = MLgetBlock(handle);

    MLplay(handle);
}
