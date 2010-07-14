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

  ==========================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "stdint.h"

#include "irig106ch10.h"
#include "i106_time.h"
#include "i106_decode_time.h"

/*
 * Macros and definitions
 * ----------------------
 */

#define MAJOR_VERSION  "01"
#define MINOR_VERSION  "02"

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
time_t  mkgmtime(struct tm * tm);



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

    SuI106Ch10Header    suTimeHdr;
    void              * pvTimeBuff = NULL;
    unsigned long       ulTimeBuffSize = 0;

    struct tm         * psuTmTime;

    int                 iArgIdx;

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
    int                 iStartHour, iStartMin, iStartSec;
    int                 iStopHour,  iStopMin,  iStopSec;

    int                 bUseStartPercent;
    int                 bUseStopPercent;
    float               fStartPercent;
    float               fStopPercent;
    int64_t             llFileSize;
    int64_t             llStartOffset;
    int64_t             llStopOffset;
    int64_t             llCurrOffset;

    int                 bFoundTmats = bFALSE;
    int                 bHaveTime  = bFALSE;
    int                 bNeedTime   = bTRUE;

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
                // Try to decode a time
                iStatus = sscanf(argv[iArgIdx],"+%d:%d:%d",
                    &iStartHour,&iStartMin,&iStartSec);
                if (iStatus == 3)
                    {
                    bUseStartTime    = bTRUE;
                    bUseStartPercent = bFALSE;
                    break;
                    }

                // Try to decode a percentage
                iStatus = sscanf(argv[iArgIdx],"+%f%%", &fStartPercent);
                if (iStatus == 1)
                    {
                    bUseStartTime    = bFALSE;
                    bUseStartPercent = bTRUE;
                    break;
                    }

                // Neither worked so it must be an error
                vUsage();
                return 1;
                break;

            case '-' :
                // Try to decode a time
                iStatus = sscanf(argv[iArgIdx],"-%d:%d:%d",
                    &iStopHour,&iStopMin,&iStopSec);
                if (iStatus == 3) 
                    {
                    bUseStopTime    = bTRUE;
                    bUseStopPercent = bFALSE;
                    break;
                    }

                // Try to decode a percentage
                iStatus = sscanf(argv[iArgIdx],"-%f%%", &fStopPercent);
                if (iStatus == 1)
                    {
                    bUseStopTime    = bFALSE;
                    bUseStopPercent = bTRUE;
                    break;
                    }

                vUsage();
                return 1;
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
    switch (enStatus)
        {
        case I106_OPEN_WARNING :
            fprintf(stderr, "Warning opening data file : Status = %d\n", enStatus);
            break;
        case I106_OK :
            break;
        default :
            fprintf(stderr, "Error opening data file : Status = %d\n", enStatus);
            return 1;
            break;
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
    enI106Ch10GetPos(iI106_In, &llCurrOffset);
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

    // Figure out strart/stop offsets
    if (bUseStartPercent == bTRUE)
        {
        llFileSize    = GetCh10FileSize(argv[1]);
        llStartOffset = (int64_t)(llFileSize * (fStartPercent / 100.0));
        }

    if (bUseStopPercent == bTRUE)
        {
        llFileSize    = GetCh10FileSize(argv[1]);
        llStopOffset  = (int64_t)(llFileSize * (fStopPercent / 100.0));
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

        // Check for special conditions and state changes
        // ----------------------------------------------

        // If we trim data off the beginning of the file, a few special things
        // need to happen.  The TMATS record needs to be copied to the output
        // file regardless.  A time packet needs to be written before any
        // data packets.  This logic keeps running copy of the latest time
        // packet.  When it's time for data to be written, first write the most
        // recent time packet.

        // If before time or offset limit, handle any special processing
        if ((bUseStartTime    == bTRUE) && (llPacketTime < llStartTime  )  ||
            (bUseStartPercent == bTRUE) && (llCurrOffset < llStartOffset))
            {

            if ((suI106Hdr.ubyDataType == I106CH10_DTYPE_TMATS) &&
                (bFoundTmats           == bFALSE              ))
                {

                // Make sure our buffer is big enough, size *does* matter
                if (ulBuffSize < suI106Hdr.ulPacketLen)
                    {
                    pvBuff = realloc(pvBuff, suI106Hdr.ulPacketLen);
                    ulBuffSize = suI106Hdr.ulPacketLen;
                    }

                // Read the data
                enStatus = enI106Ch10ReadData(iI106_In, ulBuffSize, pvBuff);
                if (enStatus != I106_OK)
                    {
                    fprintf(stderr, " Error reading header : Status = %d\n", enStatus);
                    continue;
                    }

                // If it's TMATS, rewrite some of the fields
                // SAVE THIS MESS FOR ANOTHER DAY

                // Write it to the output file
                enStatus = enI106Ch10WriteMsg(iI106_Out, &suI106Hdr, pvBuff);
                } // end it TMATS packet

            // If it's a time packet keep a copy of the latest
            if (suI106Hdr.ubyDataType == I106CH10_DTYPE_IRIG_TIME)
                {
                // Make sure our time buffer is big enough
                if (ulTimeBuffSize < suI106Hdr.ulPacketLen)
                    {
                    pvTimeBuff = realloc(pvTimeBuff, suI106Hdr.ulPacketLen);
                    ulTimeBuffSize = suI106Hdr.ulPacketLen;
                    }

                // Save the header
                memcpy(&suTimeHdr, &suI106Hdr, sizeof(SuI106Ch10Header));

                // Read and save the data
                enStatus = enI106Ch10ReadData(iI106_In, ulTimeBuffSize, pvTimeBuff);
                bHaveTime = bTRUE;

                // Update the relative to clock time mapping and update the time limit values
                enI106_Decode_TimeF1(&suTimeHdr, pvTimeBuff, &suTime);
                enI106_SetRelTime(iI106_In, &suTime, suI106Hdr.aubyRefTime);
                if (bUseStartTime == bTRUE)
                    {
                    enI106_Irig2RelTime(iI106_In, &suStartTime, abyStartTime);
                    llStartTime = 0L;
                    memcpy((char *)&(llStartTime), (char *)&abyStartTime, 6);
                    }
                if (bUseStopTime == bTRUE)
                    {
                    enI106_Irig2RelTime(iI106_In, &suStopTime, abyStopTime);
                    llStopTime = 0L;
                    memcpy((char *)&(llStopTime), (char *)&abyStopTime, 6);
                    // Handle midnight rollover
                    if ((bUseStartTime == bTRUE) && (llStopTime < llStartTime))
                        llStopTime += (int64_t)(60 * 60 * 24) * (int64_t)10000000;
                    }
                } // end if IRIG time packet

// MIGHT NEED TO DO SOME STUFF WITH INDEX PACKETS HERE
            } // if before time limit


        // If after time or offset limit, handle any special processing
        else if ((bUseStopTime    == bTRUE) && (llPacketTime > llStopTime  )  ||
                 (bUseStopPercent == bTRUE) && (llCurrOffset > llStopOffset))
            break;


        // Any other state is just a copy
        else
            {

            // If we have time and need time, then first write time
            if ((bHaveTime == bTRUE) && (bNeedTime == bTRUE))
                {
                enStatus = enI106Ch10WriteMsg(iI106_Out, &suTimeHdr, pvTimeBuff);
                bNeedTime = bFALSE;
                }

            // Make sure our buffer is big enough, size *does* matter
            if (ulBuffSize < suI106Hdr.ulPacketLen)
                {
                pvBuff = realloc(pvBuff, suI106Hdr.ulPacketLen);
                ulBuffSize = suI106Hdr.ulPacketLen;
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
            } // end else just copy


        lWriteMsgs++;

        // Read the next message header
        enI106Ch10GetPos(iI106_In, &llCurrOffset);
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



/* ------------------------------------------------------------------------ */

int64_t GetCh10FileSize(char * szFilename)
    {

#if defined(_MSC_VER)
    struct _stati64    suFileInfo; 
    _stati64(szFilename, &suFileInfo);
    return suFileInfo.st_size;
#else
#endif

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
        if (m_ulBuffSize < suI106Hdr.ulPacketLen)
            {
            m_pvBuff = realloc(m_pvBuff, suI106Hdr.ulPacketLen);
            m_ulBuffSize = suI106Hdr.ulPacketLen;
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
    printf("Trim a Ch 10 data file based on time or file offset\n");
    printf("Freeware Copyright (C) 2006 Irig106.org\n\n");
    printf("Usage: i106trim <infile> <outfile> [+hh:mm:ss] [-hh:mm:ss]\n");
    printf("  +hh:mm:ss - Start copy time\n");
    printf("  -hh:mm:ss - Stop copy time\n");
    printf("  +<num>%%   - Start copy at position <num> percent into the file\n");
    printf("  -<num>%%   - Stop copy at position <num> percent into the file\n");
    printf("Or:    fftrim <infile> to get stats\n");
    return;
    }



/* ------------------------------------------------------------------------ */

