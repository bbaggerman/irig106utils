/*==========================================================================

  I106TRIM.C - A program to trim Ch 10 data files based on start and/or
  stop times.

 Copyright (c) 2006 Irig106.org

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

 $RCSfile: i106trim.c,v $
 $Date: 2006-12-15 02:06:56 $
 $Revision: 1.6 $

  ==========================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <memory.h>

#include "stdint.h"

#include "irig106ch10.h"
#include "i106_time.h"
#include "i106_decode_time.h"

/*
 * Macros and definitions
 * ----------------------
 */

#define MAJOR_VERSION  "01"
#define MINOR_VERSION  "01"

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
void vUsage(void);
time_t mkgmtime(struct tm * tm);



/* ======================================================================== */

int main (int argc, char *argv[])
    {
    int                 iI106_In;    // Input data file handle
    int                 iI106_Out;   // Output data file handle

    EnI106Status        enStatus;

    SuI106Ch10Header    suI106Hdr;
    SuIrig106Time       suTime;
    unsigned long       ulBuffSize = 0;
    void              * pvBuff = NULL;

    struct tm         * psuTmTime;

    int                 iArgIdx;
    int                 iStartHour, iStartMin, iStartSec;
    int                 iStopHour,  iStopMin,  iStopSec;

    long                lWriteMsgs = 0L;

    uint8_t             abyStartTime[6];
    uint8_t             abyStopTime[6];
    SuIrig106Time       suStartTime;
    SuIrig106Time       suStopTime;
    int64_t             llStartTime      = -1L;
    int64_t             llStopTime       = -1L;
    int64_t             llPacketTime;

    int                 bUseStartTime;
    int                 bUseStopTime;
    int                 bFoundTmats = bFALSE;
    int                 bFoundTime  = bFALSE;
    int                 bCopyPacket;

    int                 iStatus;


/*
 * Process the command line arguments
 */

    if (argc < 2) 
        {
        vUsage();
        return 1;
        }

    bUseStartTime = bFALSE;
    bUseStopTime  = bFALSE;

    for (iArgIdx=3; iArgIdx<argc; iArgIdx++) 
        {

        switch (argv[iArgIdx][0]) 
            {

            case '+' :
                iStatus = sscanf(argv[iArgIdx],"+%d:%d:%d",
                    &iStartHour,&iStartMin,&iStartSec);
                if (iStatus != 3) 
                    {
                    vUsage();
                    return 1;
                    }
                bUseStartTime = bTRUE;
                break;

            case '-' :
                iStatus = sscanf(argv[iArgIdx],"-%d:%d:%d",
                    &iStopHour,&iStopMin,&iStopSec);
                if (iStatus != 3) 
                    {
                    vUsage();
                    return 1;
                    }
                bUseStopTime = bTRUE;
                break;

            } /* end command line arg switch */
        } /* end for all arguments */

/*
 * Opening banner
 * --------------
 */

    fprintf(stderr, "\nI106TRIM "MAJOR_VERSION"."MINOR_VERSION"\n");
    fprintf(stderr, "Freeware Copyright (C) 2006 Irig106.org\n\n");

/*
 * Open the input file and get things setup
 */

    // Open the input data file
    enStatus = enI106Ch10Open(&iI106_In, argv[1], I106_READ);
    if (enStatus != I106_OK)
        {
        fprintf(stderr, "Error opening input data file : Status = %d\n", enStatus);
        return 1;
        }

    // Get clock time synchronized
    enStatus = enI106_SyncTime(iI106_In, bFALSE, 0);
    if (enStatus != I106_OK)
        {
        fprintf(stderr, "Error establishing time sync : Status = %d\n", enStatus);
        return 1;
        }

/*
 * Open the output file
 */
    enStatus = enI106Ch10Open(&iI106_Out, argv[2], I106_OVERWRITE);
    if (enStatus != I106_OK)
        {
        fprintf(stderr, "Error opening output data file : Status = %d\n", enStatus);
        return 1;
        }

/*
 * Print out some info and get started
 */

    printf("Input Data File  '%s'\n",argv[1]);
    printf("Output Data File '%s'\n",argv[2]);

/*
 * Read the first message header and setup some stuff
 */
    
    // Read the first message header
    enStatus = enI106Ch10ReadNextHeader(iI106_In, &suI106Hdr);

    // Check for read errors and end of file
    if (enStatus == I106_EOF) 
        {
        fprintf(stderr, "EOF reading header\n");
        return 1;
        }

    if (enStatus != I106_OK)
        {
        fprintf(stderr, "Error reading header : Status = %d\n", enStatus);
        return 1;
        }

    // Figure out start/stop counts
    enStatus = enI106_Rel2IrigTime(iI106_In, suI106Hdr.aubyRefTime, &suTime);
    psuTmTime = gmtime((time_t *)&suTime.ulSecs);

    if (bUseStartTime == bTRUE)
        {
        psuTmTime->tm_hour = iStartHour;
        psuTmTime->tm_min  = iStartMin;
        psuTmTime->tm_sec  = iStartSec;
        suStartTime.ulFrac = 0L;
        suStartTime.ulSecs = mkgmtime(psuTmTime);
        enI106_Irig2RelTime(iI106_In, &suStartTime, abyStartTime);
        llStartTime = 0L;
        memcpy((char *)&(llStartTime), (char *)&abyStartTime, 6);
        }

    if (bUseStopTime == bTRUE)
        {
        psuTmTime->tm_hour = iStopHour;
        psuTmTime->tm_min  = iStopMin;
        psuTmTime->tm_sec  = iStopSec;
        suStopTime.ulFrac = 0L;
        suStopTime.ulSecs = mkgmtime(psuTmTime);
        enI106_Irig2RelTime(iI106_In, &suStopTime, abyStopTime);
        llStopTime = 0L;
        memcpy((char *)&(llStopTime), (char *)&abyStopTime, 6);
        // Handle midnight rollover
        if ((bUseStartTime == bTRUE) && (llStopTime < llStartTime))
            llStopTime += (int64_t)(60 * 60 * 24) * (int64_t)10000000;
        }

    // Try jumping to the start time
    if (bUseStartTime == bTRUE) 
        {
// SUPPORT FOR THIS IS NOT QUITE READY YET
        }

/*
 * Read data packets until EOF
 */

    lWriteMsgs = 0L;
    while (bTRUE) 
        {

        vTimeArray2LLInt(suI106Hdr.aubyRefTime, &llPacketTime);
        bCopyPacket = bFALSE;

        // If before time limit, handle any special processing
        if ((bUseStartTime == bTRUE) && (llPacketTime < llStartTime))
            {

            if ((suI106Hdr.ubyDataType == I106CH10_DTYPE_TMATS) &&
                (bFoundTmats           == bFALSE))
                {
                // If it's TMATS, rewrite some of the fields
// SAVE THIS MESS FOR ANOTHER DAY
                bCopyPacket = bTRUE;
                bFoundTmats = bTRUE;
                }

            if ((suI106Hdr.ubyDataType == I106CH10_DTYPE_IRIG_TIME) &&
                (bFoundTime            == bFALSE))
                {
                bCopyPacket = bTRUE;
                bFoundTime  = bTRUE;
                }

// MIGHT NEED TO DO SOME STUFF WITH INDEX PACKETS HERE
            } // if before time limit

        // If after time limit, handle any special processing
        else if ((bUseStopTime  == bTRUE) && (llPacketTime > llStopTime ))
            break;

        // Any other state is just a copy
        else
            bCopyPacket = bTRUE;

        // Read and copy to packet to the output file
        if (bCopyPacket == bTRUE)
            {

            // Make sure our buffer is big enough, size *does* matter
            if (ulBuffSize < suI106Hdr.ulDataLen+8)
                {
                pvBuff = realloc(pvBuff, suI106Hdr.ulDataLen+8);
                ulBuffSize = suI106Hdr.ulDataLen+8;
                }

            // Read the data
            enStatus = enI106Ch10ReadData(iI106_In, ulBuffSize, pvBuff);
            if (enStatus != I106_OK)
                {
                fprintf(stderr, " Error reading header : Status = %d\n", enStatus);
                continue;
                }

            // If it's an index record fix it up
// NEED TO IMPLEMENT INDEX RECORDS SOME DAY

            // Write it to the output file
            enStatus = enI106Ch10WriteMsg(iI106_Out, &suI106Hdr, pvBuff);
            }
        lWriteMsgs++;

        // Read the next message header
        enStatus = enI106Ch10ReadNextHeader(iI106_In, &suI106Hdr);

        // Check for read errors and end of file
        if (enStatus == I106_EOF) 
            break;

        if (enStatus != I106_OK)
            {
            fprintf(stderr, " Error reading header : Status = %d\n", enStatus);
            continue;
            }

        } // end while read/write

/*
 * Print some stats, close the files, and get outa here
 */

    printf("Packets Written %ld\n",lWriteMsgs);

    enI106Ch10Close(iI106_In);
    enI106Ch10Close(iI106_Out);


  return 0;
  }



#if 0
/* ------------------------------------------------------------------------ */

void vStats(char *szFileName)
  {
  SuFfdHandle      tFFD;
  unsigned short   usMsgType;
  SuFfdrTime       tTime;
  unsigned short   usBuffSize;
  void *           pvBuff = NULL;
  int              iStatus;

  // Open the data file
  usBuffSize = 0;
  iStatus = FfdOpen(&tFFD, szFileName, EnBioMode_READ, usBuffSize);
  if (iStatus != EnFfdStatus_OK) {
    printf ("\nERROR %d opening input file\n",iStatus);
    return;
    }

  // Make the input data buffer
  usBuffSize = tFFD.wMaxBuffSize;
  pvBuff     = malloc (usBuffSize);

  // Read the first data packet to get the start time
  usBuffSize = tFFD.wMaxBuffSize;
  iStatus = FfdReadNextMessage(&tFFD, &usMsgType, &tTime, &usBuffSize, pvBuff);
  printf("Start  %s",  ctime(&tTime.lTimeMS));

  // Read the last data packet to get the end time
  BioLastBuff(&tFFD.suBioHandle);
  usBuffSize = tFFD.wMaxBuffSize;
  iStatus = FfdReadNextMessage(&tFFD, &usMsgType, &tTime, &usBuffSize, pvBuff);
  printf("End    %s",  ctime(&tTime.lTimeMS));

  free(pvBuff);

  return;
  }



/* ------------------------------------------------------------------------ */

// Find the first IRIG time message

int iFindFirstTime(int iI106Handle)

        
    while (1==1) 
        {

        // Read the next header
        enStatus = enI106Ch10ReadNextHeader(iI106Ch10Handle, &suI106Hdr);

        if (enStatus != I106_OK)
            {
            return enStatus;
            }

        // Make sure our buffer is big enough, size *does* matter
        if (m_ulBuffSize < suI106Hdr.ulDataLen)
            {
            m_pvBuff = realloc(m_pvBuff, suI106Hdr.ulDataLen);
            m_ulBuffSize = suI106Hdr.ulDataLen;
            }

        // Read the next data buffer
        ulReadSize = ulBuffSize;
        enStatus = enI106Ch10ReadNextData(iI106Ch10Handle, &ulBuffSize, pvBuff);
        if (enStatus != I106_OK)
            {
            printf(" Error reading header : Status = %d\n", enStatus);
            break;
            }

        } // end while searching for time packet

    return 1;
    } // end iFindFirstTime()



#endif

/* ------------------------------------------------------------------------ */

void vUsage(void)
    {
    printf("\nI106TRIM "MAJOR_VERSION"."MINOR_VERSION" "__DATE__" "__TIME__"\n");
    printf("Trim a Ch 10 data file based on time\n");
    printf("Freeware Copyright (C) 2006 Irig106.org\n\n");
    printf("Usage: i106trim <infile> <outfile> [+hh:mm:ss] [-hh:mm:ss]\n");
    printf("  +hh:mm:ss - Start copy time\n");
    printf("  -hh:mm:ss - Stop copy time\n");
    printf("Or:    fftrim <infile> to get stats\n");
    return;
    }



/* ------------------------------------------------------------------------ */

