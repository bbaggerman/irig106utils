/*==========================================================================

  i106stat - Generate histogram-like statistics on a Irig 106 data file

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

 $RCSfile: i106stat.c,v $
 $Date: 2006-11-30 02:42:59 $
 $Revision: 1.7 $

 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "config.h"
#include "stdint.h"
#include "irig106ch10.h"

#include "i106_time.h"
#include "i106_decode_time.h"
#include "i106_decode_1553f1.h"
#include "i106_decode_tmats.h"


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


#define MAX_1553_BUSES             8
#define TOTAL_1553_STAT_BUFF_IDX   (0x1000L * MAX_1553_BUSES)

#define CHANTYPE_UNKNOWN        0
#define CHANTYPE_PCM            1
#define CHANTYPE_ANALOG         2
#define CHANTYPE_DISCRETE       3
#define CHANTYPE_IRIG_TIME      4
#define CHANTYPE_VIDEO          5
#define CHANTYPE_UART           6
#define CHANTYPE_1553           7
#define CHANTYPE_ARINC_429      8
#define CHANTYPE_MESSAGE_DATA   9
#define CHANTYPE_IMAGE DATA    10


/*
 * Data structures
 * ---------------
 */

/* These hold the number of messages of each type for the histogram. */

// Message counts
typedef struct
  {
    unsigned long * pul1553Msgs;
    unsigned long * pul1553Errs;
    unsigned long   ulErrTimeout;
    unsigned long   ulUserDefined;
    unsigned long   ulIrigTime;
    unsigned long   ulAnalog;
    unsigned long   ulTMATS;
    unsigned long   ulEvents;
    unsigned long   ulIndex;
    unsigned long   ulPCM;
    unsigned long   ulMonitor;
    unsigned long   ulMPEG2;
    unsigned long   ulUART;
    unsigned long   ulOther;
    unsigned long   ulTotal;

    long            lFileStartTime;
    long            lStartTime;
    long            lStopTime;
    unsigned char   abyFileStartTime[6];
    unsigned char   abyStartTime[6];
    unsigned char   abyStopTime[6];
    int             bLogRT2RT;
    int             bRT2RTFound;
    unsigned long   ulReadErrors;
    } SuCounts;

// Channel ID info
typedef struct 
    {
    unsigned char       uChanType;
    unsigned char       uChanIdx;
    } SuChannelDecode;


/*
 * Module data
 * -----------
 */

int                     m_iI106Handle;
SuChannelDecode         m_asuChanInfo[0x10000];

/*
 * Function prototypes
 * -------------------
 */

void     vResetCounts(SuCounts * psuCnt);
void     vPrintCounts(SuCounts * psuCnt, FILE * ptOutFile);
void     vPrintTmats(SuTmatsInfo * psuTmatsInfo, FILE * ptOutFile);
void     vProcessTmats(SuTmatsInfo * psuTmatsInfo);
unsigned uGet1553ChanID(unsigned uChanIdx);
void     vUsage(void);


/* ------------------------------------------------------------------------ */

int main(int argc, char ** argv)
    {

    char                    szInFile[80];     // Input file name
    char                    szOutFile[80];    // Output file name
    int                     iArgIdx;
    FILE                  * ptOutFile;        // Output file handle
    int                     bVerbose;
    int                     bFoundFileStartTime = bFALSE;
    int                     bFoundDataStartTime = bFALSE;
    unsigned short          usPackedIdx;
    unsigned long           ulBuffSize = 0L;
    unsigned long           ulReadSize;

    unsigned char           uChanIdx;

//    SuIrig106Time           suIrigTime;
    EnI106Status            enStatus;
    SuI106Ch10Header        suI106Hdr;
    Su1553F1_CurrMsg        su1553Msg;
    SuTmatsInfo             suTmatsInfo;

    unsigned char         * pvBuff = NULL;

    SuCounts                suCnt;


/* Make sure things stay on UTC */

    putenv("TZ=GMT0");
    tzset();

/*
 *  Allocate memory for 1553 histogram.
 */

    suCnt.pul1553Errs = NULL;
    suCnt.pul1553Msgs = NULL;
    vResetCounts(&suCnt);

/*
 * Process the command line arguements
 */

    if (argc < 2) {
    vUsage();
    return 1;
    }

    bVerbose          = bFALSE;               // No verbosity
    suCnt.bLogRT2RT   = bFALSE;               // Don't keep track of RT to RT
    suCnt.bRT2RTFound = bFALSE;
    szInFile[0]  = '\0';
    strcpy(szOutFile,"");                     // Default is stdout

    for (iArgIdx=1; iArgIdx<argc; iArgIdx++) 
        {

        switch (argv[iArgIdx][0]) 
            {

            // Handle command line flags
            case '-' :
                switch (argv[iArgIdx][1]) 
                    {

                    case 'r' :                   // Log RT to RT
                        suCnt.bLogRT2RT = bTRUE;
                        break;

                    case 'v' :                   // Verbose switch
                        bVerbose = bTRUE;
                        break;

                    default :
                        break;
                    } /* end flag switch */
                break;

            // Anything else must be a file name
            default :
                if (szInFile[0] == '\0') strcpy(szInFile, argv[iArgIdx]);
                else                     strcpy(szOutFile,argv[iArgIdx]);
                break;

            } /* end command line arg switch */
        } /* end for all arguments */

    if (strlen(szInFile)==0) 
        {
        vUsage();
        return 1;
        }

/*
 * Open file and get everything init'ed
 * ------------------------------------
 */


/*
 *  Open file and allocate a buffer for reading data.
 */

    enStatus = enI106Ch10Open(&m_iI106Handle, szInFile, I106_READ);
    if (enStatus != I106_OK)
        {
        fprintf(stderr, "Error opening data file : Status = %d\n", enStatus);
        return 1;
        }

    enStatus = enI106_SyncTime(m_iI106Handle, bFALSE, 0);
    if (enStatus != I106_OK)
        {
        fprintf(stderr, "Error establishing time sync : Status = %d\n", enStatus);
        return 1;
        }

/*
 * Open the output file
 */

    // If output file specified then open it    
    if (strlen(szOutFile) != 0)
        {
        ptOutFile = fopen(szOutFile,"w");
        if (ptOutFile == NULL) 
            {
            fprintf(stderr, "Error opening output file\n");
            return 1;
            }
        }

    // No output file name so use stdout
    else
        {
        ptOutFile = stdout;
        }


  printf("Computing histogram...\n");


/*
 * Loop until there are no more message whilst keeping track of all the
 * various message counts.
 * --------------------------------------------------------------------
 */

    while (1==1) 
        {

        // Read the next header
        enStatus = enI106Ch10ReadNextHeader(m_iI106Handle, &suI106Hdr);

        // Setup a one time loop to make it easy to break out on error
        do
            {
            if (enStatus == I106_EOF)
                {
                fprintf(stderr, "End of file\n");
                break;
                }

            // Check for header read errors
            if (enStatus != I106_OK)
                {
                suCnt.ulReadErrors++;
                break;
                }

            // Make sure our buffer is big enough, size *does* matter
            if (ulBuffSize < suI106Hdr.ulDataLen+8)
                {
                pvBuff = realloc(pvBuff, suI106Hdr.ulDataLen+8);
                ulBuffSize = suI106Hdr.ulDataLen+8;
                }

            // Read the data buffer
            ulReadSize = ulBuffSize;
            enStatus = enI106Ch10ReadData(m_iI106Handle, ulBuffSize, pvBuff);

            // Check for data read errors
            if (enStatus != I106_OK)
                {
                suCnt.ulReadErrors++;
                break;
                }

            suCnt.ulTotal++;
            if (bVerbose) 
                fprintf(stderr, "%8.8ld Messages \r",suCnt.ulTotal);

            // Save data start and stop times
            if ((suI106Hdr.ubyDataType != I106CH10_DTYPE_TMATS) &&
                (suI106Hdr.ubyDataType != I106CH10_DTYPE_IRIG_TIME))
                {
                if (bFoundDataStartTime == bFALSE) 
                    {
                    memcpy((char *)suCnt.abyStartTime, (char *)suI106Hdr.aubyRefTime, 6);
                    bFoundDataStartTime = bTRUE;
                    }
                else
                    {
                    memcpy((char *)suCnt.abyStopTime, (char *)suI106Hdr.aubyRefTime, 6);
                    }
                } // end if data message

            // Log the various data types
            switch (suI106Hdr.ubyDataType)
                {

                case I106CH10_DTYPE_USER_DEFINED :      // 0x00
                    suCnt.ulUserDefined++;
                    break;

                case I106CH10_DTYPE_TMATS :             // 0x01
                    suCnt.ulTMATS++;

                    // Only decode the first TMATS record
                    if (suCnt.ulTMATS != 0)
                        {
                        // Save file start time
                        memcpy((char *)&suCnt.abyFileStartTime, (char *)suI106Hdr.aubyRefTime, 6);

                        // Process TMATS info for later use
                        enI106_Decode_Tmats(&suI106Hdr, pvBuff, &suTmatsInfo);
                        if (enStatus != I106_OK) 
                            break;
                        vProcessTmats(&suTmatsInfo);
                        }
                    break;

                case I106CH10_DTYPE_RECORDING_EVENT :   // 0x02
                    suCnt.ulEvents++;
                    break;

                case I106CH10_DTYPE_RECORDING_INDEX :   // 0x03
                    suCnt.ulIndex++;
                    break;

                case I106CH10_DTYPE_PCM :               // 0x09
                    suCnt.ulPCM++;
                    break;

                case I106CH10_DTYPE_IRIG_TIME :         // 0x11
                    suCnt.ulIrigTime++;
                    break;

                case I106CH10_DTYPE_1553_FMT_1 :        // 0x19

                    uChanIdx = m_asuChanInfo[suI106Hdr.ubyDataType].uChanIdx;
    //              if (m_asuChanInfo[suI106Hdr.ubyDataType].uChanType != CHANTYPE_1553)

                    // Step through all 1553 messages
                    enStatus = enI106_Decode_First1553F1(&suI106Hdr, pvBuff, &su1553Msg);
                    while (enStatus == I106_OK)
                        {

                        // Update message count
                        usPackedIdx  = 0;
                        usPackedIdx |=  uChanIdx                          << 11;
                        usPackedIdx |= (*(su1553Msg.puCmdWord1) & 0xF800) >> 5;
                        usPackedIdx |= (*(su1553Msg.puCmdWord1) & 0x0400) >> 5;
                        usPackedIdx |= (*(su1553Msg.puCmdWord1) & 0x03E0) >> 5;
                        suCnt.pul1553Msgs[usPackedIdx]++;
                        suCnt.ulMonitor++;

                        // Update the error counts
                        if (su1553Msg.psu1553Hdr->bMsgError != 0) 
                            suCnt.pul1553Errs[usPackedIdx]++;

                        if (su1553Msg.psu1553Hdr->bRespTimeout != 0)
                            suCnt.ulErrTimeout++;

                        // Get the next 1553 message
                        enStatus = enI106_Decode_Next1553F1(&su1553Msg);
                        }

                        // If logging RT to RT then do it for second command word
                        if (su1553Msg.psu1553Hdr->bRT2RT == 1)
                            suCnt.bRT2RTFound = bTRUE;

                        if (suCnt.bLogRT2RT==bTRUE) 
                            {
                            usPackedIdx  = 0;
                            usPackedIdx |=  uChanIdx                          << 11;
                            usPackedIdx |= (*(su1553Msg.puCmdWord2) & 0xF800) >> 5;
                            usPackedIdx |= (*(su1553Msg.puCmdWord2) & 0x0400) >> 5;
                            usPackedIdx |= (*(su1553Msg.puCmdWord2) & 0x03E0) >> 5;
                            suCnt.pul1553Msgs[usPackedIdx]++;
                        } // end if logging RT to RT

                    break;

                case I106CH10_DTYPE_ANALOG :            // 0x21
                    suCnt.ulAnalog++;
                    break;

                case I106CH10_DTYPE_MPEG2 :             // 0x40
                    suCnt.ulMPEG2++;
                    break;

                case I106CH10_DTYPE_UART :              // 0x50
                    suCnt.ulUART++;
                    break;

                default:
                    suCnt.ulOther++;
                    break;

                } // end switch on message type

            } while (bFALSE); // end one time loop

        // If EOF break out of main read loop
        if (enStatus == I106_EOF)
            {
            fprintf(stderr, "End of file\n");
            break;
            }

        }   /* End while */


/*
 * Now print out the results of histogram.
 * ---------------------------------------
 */

    vPrintTmats(&suTmatsInfo, ptOutFile);
    vPrintCounts(&suCnt, ptOutFile);

/*
 *  Free dynamic memory.
 */

    free(pvBuff);
    pvBuff = NULL;

    fclose(ptOutFile);

    return 0;
    }



/* ------------------------------------------------------------------------ */

void vResetCounts(SuCounts * psuCnt)
{

  // Free 1553 counts memory
  if (psuCnt->pul1553Msgs != NULL)
    free(psuCnt->pul1553Msgs);
  if (psuCnt->pul1553Errs != NULL)
    free(psuCnt->pul1553Errs);

  // Clear out the counts
  memset(psuCnt, 0x00, sizeof(SuCounts));

  psuCnt->pul1553Msgs = calloc(TOTAL_1553_STAT_BUFF_IDX, sizeof (unsigned long));
  memset(psuCnt->pul1553Msgs, 0x00, 0x3FFFL*sizeof (unsigned long));

  psuCnt->pul1553Errs = calloc(TOTAL_1553_STAT_BUFF_IDX, sizeof (unsigned long));
  memset(psuCnt->pul1553Errs, 0x00, 0x3FFFL*sizeof (unsigned long));
  
  assert(psuCnt->pul1553Msgs != NULL);
  assert(psuCnt->pul1553Errs != NULL);

  return;
}



/* ------------------------------------------------------------------------ */

void vPrintCounts(SuCounts * psuCnt, FILE * ptOutFile)
    {
    long            lMsgIdx;
    unsigned        uChanID;
    struct tm     * psuTmTime;
    char            szTime[50];
    char          * szTimeFmt = "%m/%d/%Y %H:%M:%S";
    SuIrig106Time   suIrigTime;

//    uint64_t        llFileStartTime;
//    uint64_t        llStartTime;
//    uint64_t        llStopTime;

    fprintf(ptOutFile,"\n=-=-= Message Totals by Type =-=-=\n\n");

/*
  if (psuCnt->ulHeader != 0)
    fprintf(ptOutFile,"Headers:          %10lu\n", psuCnt->ulHeader);

  if (psuCnt->ulEvent != 0)
    fprintf(ptOutFile,"Events:           %10lu\n", psuCnt->ulEvent);
*/

    if (psuCnt->ulMonitor != 0) 
        {
        assert(psuCnt->pul1553Msgs != NULL);
        assert(psuCnt->pul1553Errs != NULL);
        fprintf(ptOutFile,"1553 :            %10lu\n", psuCnt->ulMonitor);

        for (lMsgIdx=0; lMsgIdx<TOTAL_1553_STAT_BUFF_IDX; lMsgIdx++) 
            {
            if (psuCnt->pul1553Msgs[lMsgIdx] != 0) 
                {
                uChanID = uGet1553ChanID((lMsgIdx >> 11) & 0x0007);
                fprintf(ptOutFile,"    ChanID %d  RT %2d  %c  SA %2d  Msgs %9lu  Errs %9lu\n",
                    uChanID,
                    (lMsgIdx >>  6) & 0x001f,
                    (lMsgIdx >>  5) & 0x0001 ? 'T' : 'R',
                    (lMsgIdx      ) & 0x001f,
                    psuCnt->pul1553Msgs[lMsgIdx],
                    psuCnt->pul1553Errs[lMsgIdx]);
                } // end if count not zero
            } // end for each RT

//      fprintf(ptOutFile,"  Manchester Errors :   %10lu\n", psuCnt->ulErrManchester);
//      fprintf(ptOutFile,"  Parity Errors     :   %10lu\n", psuCnt->ulErrParity);
//      fprintf(ptOutFile,"  Overrun Errors    :   %10lu\n", psuCnt->ulErrOverrun);
        fprintf(ptOutFile,"    Timeout Errors    :   %10lu\n", psuCnt->ulErrTimeout);

        if (psuCnt->bRT2RTFound == bTRUE) 
            {
            fprintf(ptOutFile,"\n  Warning - RT to RT transfers found in the data\n");
            if (psuCnt->bLogRT2RT == bTRUE)
                fprintf(ptOutFile,"  Message total is NOT the sum of individual RT totals\n");
            else 
                fprintf(ptOutFile,"  Some transmit RTs may not be shown\n");
            } // end if RT to RT

        } // end if 1553 messages

    if (psuCnt->ulUserDefined != 0)
        fprintf(ptOutFile,"User Defined      %10lu\n",   psuCnt->ulUserDefined);

    if (psuCnt->ulEvents != 0)
        fprintf(ptOutFile,"Events            %10lu\n",   psuCnt->ulEvents);

    if (psuCnt->ulIndex != 0)
        fprintf(ptOutFile,"Index             %10lu\n",   psuCnt->ulIndex);

    if (psuCnt->ulPCM != 0)
        fprintf(ptOutFile,"PCM               %10lu\n",   psuCnt->ulPCM);

    if (psuCnt->ulIrigTime != 0)
        fprintf(ptOutFile,"IRIG Time         %10lu\n",   psuCnt->ulIrigTime);

    if (psuCnt->ulAnalog != 0)
        fprintf(ptOutFile,"Analog            %10lu\n",   psuCnt->ulAnalog);

    if (psuCnt->ulMPEG2 != 0)
        fprintf(ptOutFile,"MPEG Video        %10lu\n",   psuCnt->ulMPEG2);

    if (psuCnt->ulUART != 0)
        fprintf(ptOutFile,"UART              %10lu\n",   psuCnt->ulUART);

    if (psuCnt->ulTMATS != 0)
        fprintf(ptOutFile,"TMATS             %10lu\n",   psuCnt->ulTMATS);

    if (psuCnt->ulOther != 0)
        fprintf(ptOutFile,"Other messages    %10lu\n",   psuCnt->ulOther);

    fprintf(ptOutFile,"\nTOTAL RECORDS:    %10lu\n\n", psuCnt->ulTotal);

    fprintf(ptOutFile,"=-=-= File Time Summary =-=-=\n\n");

    enI106_Rel2IrigTime(m_iI106Handle, psuCnt->abyFileStartTime, &suIrigTime);
    psuTmTime = gmtime((time_t *)&(suIrigTime.ulSecs));
    strftime(szTime, 50, szTimeFmt, psuTmTime);
    fprintf(ptOutFile,"File Start %s\n",  szTime);

    enI106_Rel2IrigTime(m_iI106Handle, psuCnt->abyStartTime, &suIrigTime);
    psuTmTime = gmtime((time_t *)&(suIrigTime.ulSecs));
    strftime(szTime, 50, szTimeFmt, psuTmTime);
    fprintf(ptOutFile,"Data Start %s\n",  szTime);

    enI106_Rel2IrigTime(m_iI106Handle, psuCnt->abyStopTime, &suIrigTime);
    psuTmTime = gmtime((time_t *)&(suIrigTime.ulSecs));
    strftime(szTime, 50, szTimeFmt, psuTmTime);
    fprintf(ptOutFile,"Data Stop  %s\n\n",  szTime);

    return;
    }




/* ------------------------------------------------------------------------ */

void vPrintTmats(SuTmatsInfo * psuTmatsInfo, FILE * ptOutFile)
    {
    int                     iGIndex;
    int                     iRIndex;
    int                     iRDsiIndex;
    SuGDataSource         * psuGDataSource;
    SuRRecord             * psuRRecord;
    SuRDataSource         * psuRDataSource;

    // Print out the TMATS info
    // ------------------------

    fprintf(ptOutFile,"\n=-=-= Channel Summary =-=-=\n\n");

    // G record
    fprintf(ptOutFile,"Program Name - %s\n",psuTmatsInfo->psuFirstGRecord->szProgramName);
    fprintf(ptOutFile,"IRIG 106 Rev - %s\n",psuTmatsInfo->psuFirstGRecord->szIrig106Rev);
    fprintf(ptOutFile,"Channel  Type          Data Source         \n");
    fprintf(ptOutFile,"-------  ------------  --------------------\n");

    // Data sources
    psuGDataSource = psuTmatsInfo->psuFirstGRecord->psuFirstGDataSource;
    do  {
        if (psuGDataSource == NULL) break;

        // G record data source info
        iGIndex = psuGDataSource->iDataSourceNum;

        // R record info
        psuRRecord = psuGDataSource->psuRRecord;
        do  {
            if (psuRRecord == NULL) break;
            iRIndex = psuRRecord->iRecordNum;

            // R record data sources
            psuRDataSource = psuRRecord->psuFirstDataSource;
            do  {
                if (psuRDataSource == NULL) break;
                iRDsiIndex = psuRDataSource->iDataSourceNum;
                fprintf(ptOutFile," %5i ",   psuRDataSource->iTrackNumber);
                fprintf(ptOutFile,"  %-12s", psuRDataSource->szChannelDataType);
                fprintf(ptOutFile,"  %-20s", psuRDataSource->szDataSourceID);
                fprintf(ptOutFile,"\n");
                psuRDataSource = psuRDataSource->psuNextRDataSource;
                } while (bTRUE);

            psuRRecord = psuRRecord->psuNextRRecord;
            } while (bTRUE);


        psuGDataSource = psuTmatsInfo->psuFirstGRecord->psuFirstGDataSource->psuNextGDataSource;
        } while (bTRUE);

    return;
    }



/* ------------------------------------------------------------------------ */

void vProcessTmats(SuTmatsInfo * psuTmatsInfo)
    {
    unsigned            uArrayIdx;
    unsigned char       u1553ChanIdx;
    SuRRecord         * psuRRecord;
    SuRDataSource     * psuRDataSrc;

    // Initialize the message mapping array
    for (uArrayIdx=0; uArrayIdx<0x10000; uArrayIdx++)
        {
        m_asuChanInfo[uArrayIdx].uChanType = CHANTYPE_UNKNOWN;
        m_asuChanInfo[uArrayIdx].uChanIdx  = 0;
        }

    // Map IRIG106 messages and channels
    u1553ChanIdx = 0;
    psuRRecord   = psuTmatsInfo->psuFirstRRecord;
    while (psuRRecord != NULL)
        {
        // Get the first data source for this R record
        psuRDataSrc = psuRRecord->psuFirstDataSource;
        while (psuRDataSrc != NULL)
            {
            // See if 1553 channel data type
            if (strcasecmp(psuRDataSrc->szChannelDataType,"1553IN") == 0)
                {
                m_asuChanInfo[psuRDataSrc->iTrackNumber].uChanType = CHANTYPE_1553;
                m_asuChanInfo[psuRDataSrc->iTrackNumber].uChanIdx  = u1553ChanIdx++;
                } // end if 1553 channel type

            // Get the next R record data source
            psuRDataSrc = psuRDataSrc->psuNextRDataSource;
            } // end while walking R data source linked list

        // Get the next R record
        psuRRecord = psuRRecord->psuNextRRecord;

        } // end while walking R record linked list

    return;
    }



/* ------------------------------------------------------------------------ */

unsigned uGet1553ChanID(unsigned uChanIdx)
    {
    unsigned    uChanID;

    for (uChanID=0; uChanID <0x10000; uChanID++)
        {
        if ((m_asuChanInfo[uChanID].uChanType == CHANTYPE_1553) &&
            (m_asuChanInfo[uChanID].uChanIdx  == uChanIdx))
            break;
        } // end for all channel id's

    return uChanID;
    }



/* ------------------------------------------------------------------------ */

void vUsage(void)
  {
  printf("I106STAT "MAJOR_VERSION"."MINOR_VERSION" "__DATE__" "__TIME__"\n");
  printf("Usage: i106stat <input file> <output file> [flags]\n");
  printf("   <filename> Input/output file names\n");
  printf("   -r         Log both sides of RT to RT transfers\n");
  printf("   -v         Verbose\n");
  }


