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

#include "stdint.h"
#include "config.h"
#include "irig106ch10.h"
#include "i106_time.h"
#include "i106_decode_time.h"

/*
 * Macros and definitions
 * ----------------------
 */

#define MAJOR_VERSION  "01"
#define MINOR_VERSION  "00"

#if !defined(bTRUE)
#define bTRUE   (1==1)
#define bFALSE  (1==0)
#endif

/*
 * Data structures
 * ---------------
 */


/*
 * Module data
 * -----------
 */

void          * m_pvBuff     = NULL;
unsigned long   m_ulBuffSize = 0L;

/*
 * Function prototypes
 * -------------------
 */

// void vStats(char *szFileName);
int64_t GetCh10FileSize(char * szFilename);
void    vUsage(void);
//time_t  mkgmtime(struct tm * tm);



/* ======================================================================== */

int main (int argc, char *argv[])
    {
    char              * szOutFile;
    int                 iI106_In;   // Input data handle
    uint16_t            uPort;      // UDP receive port

    EnI106Status        enStatus;

    SuI106Ch10Header    suI106Hdr;
    SuIrig106Time       suTime;
    unsigned long       ulBuffSize = 0;
    void              * pvBuff = NULL;

//    SuI106Ch10Header    suTimeHdr;
    void              * pvTimeBuff = NULL;
    unsigned long       ulTimeBuffSize = 0;

//    struct tm         * psuTmTime;

    int                 iArgIdx;

    long                lMsgs = 0L;

    int                 bHaveTmats = bTRUE;
    int                 bHaveTime  = bTRUE;

//    int                 iStatus;


/*
 * Process the command line arguments
 */

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

/*
 * Opening banner
 * --------------
 */

    fprintf(stderr, "\nI106UDPRCV "MAJOR_VERSION"."MINOR_VERSION"\n");
    fprintf(stderr, "Freeware Copyright (C) 2015 Irig106.org\n\n");

/*
 * Open the input UDPstream and get things setup
 */

    // Open the input data file
    enStatus = enI106Ch10OpenStreamRead(&iI106_In, uPort);
    if (enStatus != I106_OK)
        {
        fprintf(stderr, "Error opening network stream : Status = %d\n", enStatus);
        return 1;
        }

/*
 * Open the output file
 */
    //enStatus = enI106Ch10Open(&iI106_Out, argv[2], I106_OVERWRITE);
    //if (enStatus != I106_OK)
    //    {
    //    fprintf(stderr, "Error opening output data file : Status = %d\n", enStatus);
    //    return 1;
    //    }

/*
 * Read data packets until EOF
 */

    lMsgs = 1;

    while (1==1) 
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

            // Decode some selected message types
            switch (suI106Hdr.ubyDataType)
                {
                case I106CH10_DTYPE_TMATS :
                    bHaveTime = bTRUE;
                    printf("Data Type 0x%2.2x  TMATS\n", suI106Hdr.ubyDataType);
                    break;

                case I106CH10_DTYPE_IRIG_TIME :
                    bHaveTime = bTRUE;
                    // Make sure our buffer is big enough, size *does* matter
                    if (ulBuffSize < suI106Hdr.ulPacketLen)
                        {
                        pvBuff = realloc(pvBuff, suI106Hdr.ulPacketLen);
                        ulBuffSize = suI106Hdr.ulPacketLen;
                        }
                    // Read the data buffer and decode time
                    enStatus = enI106Ch10ReadData(iI106_In, ulBuffSize, pvBuff);
                    enI106_Decode_TimeF1(&suI106Hdr, pvBuff, &suTime);
                    printf("Data Type 0x%2.2x  Time\n", suI106Hdr.ubyDataType);
                    break;

                default :
                    printf("Data Type 0x%2.2x\n", suI106Hdr.ubyDataType);
                    break;
                } // end switch on data type


            } while (bFALSE); // end one time loop

        // If EOF break out of main read loop
        if (enStatus == I106_EOF)
            {
            fprintf(stderr, "End of file\n");
            break;
            }

        }   /* End while */

/*
 * Print some stats, close the files, and get outa here
 */

    printf("Packets Read %ld\n",lMsgs);

    enI106Ch10Close(iI106_In);


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



