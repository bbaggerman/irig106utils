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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/stat.h>

// Stuff for _kbhit()
#if defined(_WIN32)
#include <conio.h>
#endif
#if defined(__GNUC__)
#include <stropts.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#endif

// IRIG library
#include "stdint.h"
#include "config.h"
#include "irig106ch10.h"
#include "i106_time.h"
#include "i106_decode_time.h"

// Macros and definitions
// ----------------------

#define MAJOR_VERSION  "01"
#define MINOR_VERSION  "00"

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

// Function prototypes
// -------------------

void    vUsage(void);

#if defined(__GNUC__)
int _kbhit();
#endif

/* ======================================================================== */

int main (int argc, char *argv[])
    {
    char              * szOutFile = NULL;
    int                 iI106_In;   // Input data stream handle
    int                 iI106_Out;  // Output data file handle
    unsigned int        uPort;      // UDP receive port

    EnI106Status        enStatus;

    SuI106Ch10Header    suI106Hdr;
    SuIrig106Time       suTime;
    unsigned long       ulBuffSize = 0;
    void              * pvBuff     = NULL;

//    void              * pvTimeBuff = NULL;
//    unsigned long       ulTimeBuffSize = 0;

    int                 iArgIdx;

    long                lPackets;
    int                 bHaveTmats = bTRUE;
    int                 bHaveTime  = bTRUE;
    int                 bWriteFile = bFALSE;


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

                    case 'T' :                   // Wait for TMATS flag
                        bHaveTmats = bFALSE;
                        break;

                    case 't' :                   // Wait for time flag
                        bHaveTime = bFALSE;
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
    fprintf(stderr, "Freeware Copyright (C) 2015 Irig106.org\n\n");

    fprintf(stderr, "Starting, press any key to exit...\n\n");

// Open the input UDPstream and get things setup

    // Open the input data stream
    enStatus = enI106Ch10OpenStreamRead(&iI106_In, uPort);
    if (enStatus != I106_OK)
        {
        fprintf(stderr, "Error opening network stream : Status = %d\n", enStatus);
        return 1;
        }

// Open the output file

    if (szOutFile != NULL)
        {
        enStatus = enI106Ch10Open(&iI106_Out, szOutFile, I106_OVERWRITE);
        if (enStatus != I106_OK)
            {
            fprintf(stderr, "Error opening output data file : Status = %d\n", enStatus);
            return 1;
            }
        else
            bWriteFile = bTRUE;
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
    while (!_kbhit())
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

            // Decode some selected message types
            switch (suI106Hdr.ubyDataType)
                {
                case I106CH10_DTYPE_TMATS :
                    if (bHaveTmats == bFALSE)
                        {
                        bHaveTmats = bTRUE;
                        printf("\nGot first TMATS packet\n");
                        }
                    printf("Data Type 0x%2.2x  TMATS\n", suI106Hdr.ubyDataType);
                    break;

                case I106CH10_DTYPE_IRIG_TIME :
                    // Decode time
                    if (bHaveTime == bFALSE)
                        {
                        bHaveTime = bTRUE;
                        printf("\nGot first Time packet\n");
                        }
                    enI106_Decode_TimeF1(&suI106Hdr, pvBuff, &suTime);
                    printf("Data Type 0x%2.2x  Time %s\n", suI106Hdr.ubyDataType, IrigTime2String(&suTime));
                    break;

                case I106CH10_DTYPE_PCM_FMT_0 :
                case I106CH10_DTYPE_PCM_FMT_1 :
                    printf("Data Type 0x%2.2x  PCM\n", suI106Hdr.ubyDataType);
                    break;

                case I106CH10_DTYPE_1553_FMT_1 :
                    printf("Data Type 0x%2.2x  1553\n", suI106Hdr.ubyDataType);
                    break;

                case I106CH10_DTYPE_ANALOG :
                    printf("Data Type 0x%2.2x  Analog\n", suI106Hdr.ubyDataType);
                    break;

                case I106CH10_DTYPE_VIDEO_FMT_0 :
                case I106CH10_DTYPE_VIDEO_FMT_1 :
                case I106CH10_DTYPE_VIDEO_FMT_2 :
                    printf("Data Type 0x%2.2x  Video\n", suI106Hdr.ubyDataType);
                    break;

                case I106CH10_DTYPE_UART_FMT_0 :
                    printf("Data Type 0x%2.2x  UART\n", suI106Hdr.ubyDataType);
                    break;

                case I106CH10_DTYPE_ETHERNET_FMT_0 :
                    printf("Data Type 0x%2.2x  Ethernet\n", suI106Hdr.ubyDataType);
                    break;

                default :
                    printf("Data Type 0x%2.2x\n", suI106Hdr.ubyDataType);
                    break;
                } // end switch on data type

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
    printf("Usage: i106udprcv <-p port> [-T] [-t] [-m multi-cast address] <outfile>\n");
    printf("  -p port   Receive UDP port number>\n");
    printf("  -T        Wait for TMATS packet before recording\n");
    printf("  -t        Wait for time packet before recording\n");
    printf("  outfile   Output Ch 10 file name\n");
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