/*==========================================================================

  I106UDPRCV.C - A program to receive IRIG 106 Ch 10 UPD live data streaming
    packets and optionally write them to a Ch 10 data file.

 Copyright (c) 2015 Irig106.org

 All rights reserved.

 Redistribution and use in source and binary forms, with or without 
 modification, are permitted provided that the following conditions are 
 met:

   * Redistributions of source code must retain the above copyright 
     notice, this list of conditions and the following disclaimer.

   * Redistributions in binary form must reproduce the above copyright 
     notice, this list of conditions and the following disclaimer in the 
     documentation and/or other materials provided with the distribution.

   * Neither the name Irig106.org nor the names of its contributors may 
     be used to endorse or promote products derived from this software 
     without specific prior written permission.

 This software is provided by the copyright holders and contributors 
 "as is" and any express or implied warranties, including, but not 
 limited to, the implied warranties of merchantability and fitness for 
 a particular purpose are disclaimed. In no event shall the copyright 
 owner or contributors be liable for any direct, indirect, incidental, 
 special, exemplary, or consequential damages (including, but not 
 limited to, procurement of substitute goods or services; loss of use, 
 data, or profits; or business interruption) however caused and on any 
 theory of liability, whether in contract, strict liability, or tort 
 (including negligence or otherwise) arising in any way out of the use 
 of this software, even if advised of the possibility of such damage.

 Created by Bob Baggerman

  ==========================================================================*/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/stat.h>

// Stuff for _kbhit()
#if defined(_MSC_VER)
#include <conio.h>
#include <io.h>
//#include <Windows.h>
#endif

#if defined(__GNUC__)
#include <stropts.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#endif

// IRIG library
#include "i106_stdint.h"
#include "config.h"
#include "irig106ch10.h"
#include "i106_time.h"
#include "i106_decode_time.h"
#include "i106_decode_tmats.h"

// Macros and definitions
// ----------------------

#define MAJOR_VERSION  "01"
#define MINOR_VERSION  "01"

#if !defined(bTRUE)
#define bTRUE   (1==1)
#define bFALSE  (1==0)
#endif


// Data structures
// ---------------



// Module data
// -----------

void          * m_pvBuff     = NULL;
unsigned long   m_ulBuffSize = 0L;

char * aszPacketType[] = {
    "User Defined", "TMATS",        "Event",        "Index",        "Computer 4",   "Computer 5",   "Computer 6",   "Computer 7",
    "PCM Fmt 0",    "PCM Fmt 1",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",
    "UNDEFINED",    "Time",         "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",
    "UNDEFINED",    "1553 Fmt 1",   "16PP194",      "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",
    "UNDEFINED",    "Analog",       "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",
    "UNDEFINED",    "Discrete",     "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",
    "Message",      "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",
    "ARINC 429",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",
    "Video Fmt 0",  "Video Fmt 1",  "Video Fmt 2",  "Video Fmt 3",  "Video Fmt 4",  "UNDEFINED",    "UNDEFINED",    "UNDEFINED",
    "Image Fmt 0",  "Image Fmt 1",  "Image Fmt 2",  "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",
    "UART Fmt 0",   "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",
    "1394 Fmt 0",   "1394 Fmt 1",   "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",
    "Parallel Fmt 0","UNDEFINED",   "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",
    "Ethernet Fmt 0","Ethernet Fmt 2","UNDEFINED",  "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",
    "TSPI Fmt 0",   "TSPI Fmt 1",   "TSPI Fmt 2",   "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",
    "CAN",          "Fibre Channel Fmt 0","UNDEFINED","UNDEFINED",  "UNDEFINED",    "UNDEFINED",    "UNDEFINED",    "UNDEFINED",
	"UNDEFINED" };

// Function prototypes
// -------------------

void    vUsage(void);

#if defined(__GNUC__)
int _kbhit();
#endif

/* ======================================================================== */

int main (int argc, char *argv[])
    {
    char              * szOutFile   = NULL;
	char              * szPcapFile  = NULL;
    char              * szTmatsFile = NULL;
	int                 iI106_In;   // Input data stream handle
    int                 iI106_Out;  // Output data file handle
    unsigned int        uPort;      // UDP receive port

    EnI106Status        enStatus;

    SuI106Ch10Header    suI106Hdr;
    SuIrig106Time       suTime;
    unsigned long       ulBuffSize = 0;
    void              * pvBuff     = NULL;

    int                 iArgIdx;

    long                lPackets;
    int                 bHaveTmats    = bTRUE;
    int                 bHaveTime     = bTRUE;
    int                 bWriteFile    = bFALSE;
	int                 bReadFromPcap = bFALSE;

// Process the command line arguments
// ----------------------------------
    if (argc < 2) 
        {
        vUsage();
        return 1;
        }

    // Step through all the arguements
    for (iArgIdx=1; iArgIdx<argc; iArgIdx++) 
        {
        // Check the first character
        switch (argv[iArgIdx][0]) 
            {

            // Catch command line flags
            case '-' :
                switch (argv[iArgIdx][1]) 
                    {

                    case 'p' :                   // Port number
                        iArgIdx++;
                        if (sscanf(argv[iArgIdx],"%d",&uPort) != 1)
                            {
                            printf("Bad port number\n");
                            vUsage();
                            return 1;
                            }
                        break;

                    case 'c' :                   // TMATS file name
                        iArgIdx++;
                        szTmatsFile = argv[iArgIdx];
                        break;

                    case 'T' :                  // Wait for TMATS flag
                        bHaveTmats = bFALSE;
                        break;

                    case 't' :                  // Wait for time flag
                        bHaveTime = bFALSE;
                        break;

                    case 'P':                   // Read from pcap file
                        iArgIdx++;
                        bReadFromPcap = bTRUE;
                        szPcapFile = argv[iArgIdx];
                        break;

                    default :
                        break;
                    } // end switch on flag character
                break; // end if '-' first character

            // First character not a '-' so it must be an output file name
            default :
                szOutFile = argv[iArgIdx];
                break; // end any other first character

            } // end switch on first character
        } // end for all arguments

// Get setup to run
// ----------------

// Opening banner

    fprintf(stderr, "\nI106UDPRCV "MAJOR_VERSION"."MINOR_VERSION"\n");
    fprintf(stderr, "Freeware Copyright (C) 2019 Irig106.org\n\n");

// Open the input UDPstream and get things setup

    // Open the input data stream
    if (bReadFromPcap == bFALSE)
        {
        enStatus = enI106Ch10OpenStreamRead(&iI106_In, uPort);
        if (enStatus != I106_OK)
            {
            fprintf(stderr, "Error opening network stream : %s\n", szI106ErrorStr(enStatus));
            return 1;
            }
        fprintf(stderr, "Starting, press any key to exit...\n\n");
        } // end if read ethernet stream

    else
        {
        enStatus = enI106Ch10OpenPcapRead(&iI106_In, uPort, szPcapFile);
        if (enStatus != I106_OK)
        {
            fprintf(stderr, "Error opening pcap file : %s\n", szI106ErrorStr(enStatus));
            return 1;
        }
    } // end if read pcap

// Open the output file

    if (szOutFile != NULL)
        {
        enStatus = enI106Ch10Open(&iI106_Out, szOutFile, I106_OVERWRITE);
        if (enStatus != I106_OK)
            {
            fprintf(stderr, "Error opening output data file : %s\n", szI106ErrorStr(enStatus));
            return 1;
            }
        else
            {
            bWriteFile = bTRUE;

            // If there is a TMATS file specified then write it to the output file
            if (szTmatsFile != NULL)
                {
#if !defined(_MSC_VER)
                struct stat     suStatBuff;
#endif
                int             hTmatsFile;
                int             iFileLength;
                int             iReadLength;
                
                // Open the TMATS file
#if defined(__GNUC__)
                hTmatsFile = open(szTmatsFile, O_RDONLY);
#else
                hTmatsFile = _open(szTmatsFile, _O_RDONLY|_O_BINARY);
#endif
                if (hTmatsFile == -1)
                    fprintf(stderr, "Error opening TMATS file\n");
                else
                    {
                    SuTmats_ChanSpec * psuTmatsCsdw;
                    char             * chTmatsBuff;
                    SuTmatsInfo        suTmatsInfo;

                    // Get the file length
#if defined(_MSC_VER)       
                    iFileLength = _filelength(hTmatsFile);
#else   
                    fstat(hTmatsFile, &suStatBuff);
                    iFileLength = suStatBuff.st_size;
#endif

                    // Read the TMATS data
                    pvBuff = malloc(iFileLength+100);   // Extra space to add checksum, etc.
                    assert(pvBuff != NULL);
                    ulBuffSize = iFileLength+100;

                    psuTmatsCsdw = (SuTmats_ChanSpec *)pvBuff;
                    chTmatsBuff  = &(((char *)pvBuff)[4]);

                    // Read the file
#if defined(__GNUC__)
                    iReadLength = read(hTmatsFile, chTmatsBuff, (unsigned)iFileLength);
#else
                    iReadLength = _read(hTmatsFile, chTmatsBuff, (unsigned)iFileLength);
#endif

                    // Make the IRIG header and data buffer
                    iHeaderInit(&suI106Hdr, 0, I106CH10_DTYPE_TMATS, I106CH10_PFLAGS_CHKSUM_NONE, 0);
                    suI106Hdr.ulDataLen = iReadLength+4;
                    suI106Hdr.ubyHdrVer = 0x01;
                    suI106Hdr.aubyRefTime[0] = 0;
                    suI106Hdr.aubyRefTime[1] = 0;
                    suI106Hdr.aubyRefTime[2] = 0;
                    suI106Hdr.aubyRefTime[3] = 0;
                    suI106Hdr.aubyRefTime[4] = 0;
                    suI106Hdr.aubyRefTime[5] = 0;
                    uAddDataFillerChecksum(&suI106Hdr, pvBuff);
                    suI106Hdr.uChecksum = uCalcHeaderChecksum(&suI106Hdr);

                    // Make the TMATS CSDW
                    memset(psuTmatsCsdw, 0, sizeof(SuTmats_ChanSpec));

                    // Derive a Ch 10 version from the TMATS version field. Not perfect
                    // but better than nothing.
                    memset(&suTmatsInfo, 0, sizeof(SuTmatsInfo));
                    enI106_Decode_Tmats_Text(chTmatsBuff, iReadLength, &suTmatsInfo);
                    if (suTmatsInfo.psuFirstGRecord->szIrig106Rev != NULL)
                        {
                        int     iTokens;
                        int     iTmatsVersion;

                        iTokens = sscanf(suTmatsInfo.psuFirstGRecord->szIrig106Rev, "%d", &iTmatsVersion);
                        if (iTokens == 1)
                            {
                            switch (iTmatsVersion)
                                {
                                case  7 : psuTmatsCsdw->iCh10Ver = 0x07; break;
                                case  9 : psuTmatsCsdw->iCh10Ver = 0x08; break;
                                case 11 : psuTmatsCsdw->iCh10Ver = 0x09; break;
                                case 13 : psuTmatsCsdw->iCh10Ver = 0x0A; break;
                                case 15 : psuTmatsCsdw->iCh10Ver = 0x0B; break;
                                case 17 : psuTmatsCsdw->iCh10Ver = 0x0C; break;
                                case 19 : psuTmatsCsdw->iCh10Ver = 0x0D; break;
                                } // end switch on TMATS version
                            } // end if rev string decoded OK
                        } // end if TMATS version string found
                    enI106_Free_TmatsInfo(&suTmatsInfo);

                    // Write it to the output file
                    enStatus = enI106Ch10WriteMsg(iI106_Out, &suI106Hdr, pvBuff);

                    // Close the TMATS file
#if defined(__GNUC__)
                    close(hTmatsFile);
#else
                    _close(hTmatsFile);
#endif
                    } // end if open TMATS file OK
                } // end if TMATS file specified
            }
        } // end if output file name not null

    if (bWriteFile)
        {
        fprintf(stderr, "Writing packets to file '%s'\n", szOutFile);
        if (!bHaveTmats)
            fprintf(stderr, "Wait for first TMATS packet\n");
        if (!bHaveTime)
            fprintf(stderr, "Wait for first Time packet\n");
        fprintf(stderr, "\n");
        }

// Read data packets until EOF
// ---------------------------

    lPackets = 0L;
    enStatus = I106_OK;
    while ((!_kbhit()) && (enStatus != I106_EOF))
        {

        // Read the next header
        enStatus = enI106Ch10ReadNextHeader(iI106_In, &suI106Hdr);

        // Setup a one time loop to make it easy to break out on error
        do
            {
            if (enStatus == I106_EOF)
                break;

            // Check for header read errors
            if (enStatus != I106_OK)
                {
                fprintf(stderr, "Error enI106Ch10ReadNextHeader() : %s\n", szI106ErrorStr(enStatus));
                break;
                }

            // Get the data
            if (ulBuffSize < suI106Hdr.ulPacketLen)
                {
                pvBuff = realloc(pvBuff, suI106Hdr.ulPacketLen);
                ulBuffSize = suI106Hdr.ulPacketLen;
                }

            enStatus = enI106Ch10ReadData(iI106_In, ulBuffSize, pvBuff);
            if (enStatus != I106_OK)
                {
                fprintf(stderr, "Error enI106Ch10ReadData() : %s\n", szI106ErrorStr(enStatus));
                break;
                }

            // Print some general information
            printf("Ch %2d %-20s (0x%2.2d)", suI106Hdr.uChID, 
                aszPacketType[suI106Hdr.ubyDataType], suI106Hdr.ubyDataType);

            // Decode some selected message types
            switch (suI106Hdr.ubyDataType)
                {
                case I106CH10_DTYPE_TMATS :
                    if (bHaveTmats == bFALSE)
                        {
                        bHaveTmats = bTRUE;
                        printf("\nGot first TMATS packet\n");
                        }
//                    printf("Data Type 0x%2.2x  TMATS\n", suI106Hdr.ubyDataType);
                    break;

                case I106CH10_DTYPE_IRIG_TIME :
                    // Decode time
                    if (bHaveTime == bFALSE)
                        {
                        bHaveTime = bTRUE;
                        printf("\nGot first Time packet\n");
                        }
                    enI106_Decode_TimeF1(&suI106Hdr, pvBuff, &suTime);
                    enI106_SetRelTime(iI106_In, &suTime, suI106Hdr.aubyRefTime);

                    printf(" %s", IrigTime2String(&suTime));
                    break;

                case I106CH10_DTYPE_PCM_FMT_0 :
                case I106CH10_DTYPE_PCM_FMT_1 :
//                    printf("Data Type 0x%2.2x  PCM\n", suI106Hdr.ubyDataType);
                    break;

                case I106CH10_DTYPE_1553_FMT_1 :
//                    printf("Data Type 0x%2.2x  1553\n", suI106Hdr.ubyDataType);
                    break;

                case I106CH10_DTYPE_ANALOG :
//                    printf("Data Type 0x%2.2x  Analog\n", suI106Hdr.ubyDataType);
                    break;

                case I106CH10_DTYPE_VIDEO_FMT_0 :
                case I106CH10_DTYPE_VIDEO_FMT_1 :
                case I106CH10_DTYPE_VIDEO_FMT_2 :
//                    printf("Data Type 0x%2.2x  Video\n", suI106Hdr.ubyDataType);
                    break;

                case I106CH10_DTYPE_UART_FMT_0 :
//                    printf("Data Type 0x%2.2x  UART\n", suI106Hdr.ubyDataType);
                    break;

                case I106CH10_DTYPE_ETHERNET_FMT_0 :
//                    printf("Data Type 0x%2.2x  Ethernet\n", suI106Hdr.ubyDataType);
                    break;

                default :
//                    printf("Data Type 0x%2.2x\n", suI106Hdr.ubyDataType);
                    break;
                } // end switch on data type
            printf("\n");

                // Write packet to Ch 10 file
                if (bWriteFile && bHaveTmats && bHaveTime)
                    enStatus = enI106Ch10WriteMsg(iI106_Out, &suI106Hdr, pvBuff);

                lPackets++;

            } while (bFALSE); // end one time loop

        }   // End while waiting for keypress

// Print some stats, close the files, and get outa here
// ----------------------------------------------------

    printf("Packets Read %ld\n",lPackets);

    enI106Ch10Close(iI106_In);

    if (bWriteFile)
        enI106Ch10Close(iI106_Out);

    return 0;
    }



// ----------------------------------------------------------------------------

void vUsage(void)
    {
    printf("\nI106UDPRCV "MAJOR_VERSION"."MINOR_VERSION" "__DATE__" "__TIME__"\n");
    printf("Receive Ch 10 UDP data stream\n");
    printf("Freeware Copyright (C) 2015 Irig106.org\n\n");
    printf("Usage: i106udprcv -p port [-c filename] [-T] [-t] [-P filename] [outfile]\n");
    printf("  -p port      Receive UDP port number\n");
    printf("  -c filename  Prepend TMATS config file to output file\n");
    printf("  -T           Wait for TMATS packet before recording\n");
    printf("  -t           Wait for time packet before recording\n");
	printf("  -P filename  Read network data from pcap file\n");
	printf("  outfile      Output Ch 10 file name\n");
    return;
    }


// ----------------------------------------------------------------------------

#if defined(__GNUC__)

// Linux (POSIX) implementation of _kbhit()

int _kbhit() 
    {
    static const int    STDIN = 0;
    static int          initialized = 0;
    int                 bytesWaiting;
    struct termios      term;

    if (initialized == 0) 
        {
        // Use termios to turn off line buffering
        tcgetattr(STDIN, &term);
        term.c_lflag &= ~ICANON;
        tcsetattr(STDIN, TCSANOW, &term);
        setbuf(stdin, NULL);
        initialized = 1;
        }

    ioctl(STDIN, FIONREAD, &bytesWaiting);
    return bytesWaiting;
    }

#endif
