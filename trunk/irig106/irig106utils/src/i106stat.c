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

 $RCSfile: i106stat.c $
 $Date: 2008/07/02 12:59:36EDT $
 $Revision: 1.3 $

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

#define MAJOR_VERSION  "B1"
#define MINOR_VERSION  "02"

#if !defined(bTRUE)
#define bTRUE   (1==1)
#define bFALSE  (1==0)
#endif

/*
 * Data structures
 * ---------------
 */


/* These hold the number of messages of each type for the histogram. */

// 1553 channel counts
typedef struct
    {
    unsigned long       ulTotalIrigMsgs;
    unsigned long       ulTotalBusMsgs;
    unsigned long       aulMsgs[0x4000];
    unsigned long       aulErrs[0x4000];
    unsigned long       ulErr1553Timeout;
    int                 bRT2RTFound;
    } SuChanInfo1553;

// Per channel statistics
typedef struct
  {
    unsigned int        iChanID;
    int                 iPrevSeqNum;
    unsigned long       ulSeqNumError;
    unsigned char       szChanType[32];
    unsigned char       szChanName[32];
    SuChanInfo1553    * psu1553Info;
    unsigned long       ulUserDefined;
    unsigned long       ulIrigTime;
    unsigned long       ulAnalog;
    unsigned long       ulTMATS;
    unsigned long       ulEvents;
    unsigned long       ulIndex;
    unsigned long       ulPCM;
    unsigned long       ulMPEG2;
    unsigned long       ulUART;
    unsigned long       ulOther;
    } SuChanInfo;


/*
 * Module data
 * -----------
 */

int                 m_bLogRT2RT;
int                 m_bVerbose;

/*
 * Function prototypes
 * -------------------
 */

void     vPrintCounts(SuChanInfo * psuChanInfo, FILE * ptOutFile);
void     vPrintTmats(SuTmatsInfo * psuTmatsInfo, FILE * ptOutFile);
void     vProcessTmats(SuTmatsInfo * psuTmatsInfo, SuChanInfo * apsuChanInfo[]);
void     vUsage(void);


/* ------------------------------------------------------------------------ */

int main(int argc, char ** argv)
    {

    // Array of pointers to the SuChanInfo structure
    static SuChanInfo     * apsuChanInfo[0x10000];

    unsigned char           abyFileStartTime[6];
    unsigned char           abyStartTime[6];
    unsigned char           abyStopTime[6];
    int                     bFoundFileStartTime = bFALSE;
    int                     bFoundDataStartTime = bFALSE;
    unsigned long           ulReadErrors;
    unsigned long           ulTotal;

    FILE                  * ptOutFile;        // Output file handle
    int                     hI106In;
    char                    szInFile[80];     // Input file name
    char                    szOutFile[80];    // Output file name
    int                     iArgIdx;
    unsigned short          usPackedIdx;
    unsigned long           ulBuffSize = 0L;
    unsigned long           ulReadSize;

    unsigned int            uChanIdx;

    EnI106Status            enStatus;
    SuI106Ch10Header        suI106Hdr;
    Su1553F1_CurrMsg        su1553Msg;
    SuTmatsInfo             suTmatsInfo;
    SuIrig106Time           suIrigTime;
    struct tm             * psuTmTime;
    char                    szTime[50];
    char                  * szDateTimeFmt = "%m/%d/%Y %H:%M:%S";
    char                  * szDayTimeFmt  = "%j:%H:%M:%S";
    char                  * szTimeFmt;

    unsigned char         * pvBuff = NULL;

    memset( &suTmatsInfo, 0, sizeof(suTmatsInfo) );

// Make sure things stay on UTC

    putenv("TZ=GMT0");
    tzset();

/*
 * Initialize the channel info array pointers to all NULL
 */

    memset(apsuChanInfo, 0, sizeof(apsuChanInfo));
    ulTotal      = 0L;
    ulReadErrors = 0L;

/*
 * Process the command line arguements
 */

    if (argc < 2) 
        {
        vUsage();
        return 1;
        }

    m_bVerbose    = bFALSE;               // No verbosity
    m_bLogRT2RT   = bFALSE;               // Don't keep track of RT to RT
    szInFile[0] = '\0';
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
                        m_bLogRT2RT = bTRUE;
                        break;

                    case 'v' :                   // Verbose switch
                        m_bVerbose = bTRUE;
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
 * Opening banner
 * --------------
 */

    fprintf(stderr, "\nI106STAT "MAJOR_VERSION"."MINOR_VERSION"\n");
    fprintf(stderr, "Freeware Copyright (C) 2006 Irig106.org\n\n");

/*
 * Opens file and get everything init'ed
 * ------------------------------------
 */


    // Open file and allocate a buffer for reading data.
    enStatus = enI106Ch10Open(&hI106In, szInFile, I106_READ);
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

    enStatus = enI106_SyncTime(hI106In, bFALSE, 0);
    if (enStatus != I106_OK)
        {
        fprintf(stderr, "Error establishing time sync : Status = %d\n", enStatus);
        return 1;
        }

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


    fprintf(stderr, "Computing histogram...\n");


/*
 * Loop until there are no more message whilst keeping track of all the
 * various message counts.
 * --------------------------------------------------------------------
 */

    while (1==1) 
        {

        // Read the next header
        enStatus = enI106Ch10ReadNextHeader(hI106In, &suI106Hdr);

        // Setup a one time loop to make it easy to break out on error
        do
            {
            if (enStatus == I106_EOF)
                {
                break;
                }

            // Check for header read errors
            if (enStatus != I106_OK)
                {
                ulReadErrors++;
                break;
                }

            // Make sure our buffer is big enough, size *does* matter
            if (ulBuffSize < uGetDataLen(&suI106Hdr))
                {
                pvBuff = realloc(pvBuff, uGetDataLen(&suI106Hdr));
                ulBuffSize = uGetDataLen(&suI106Hdr);
                }

            // Read the data buffer
            ulReadSize = ulBuffSize;
            enStatus = enI106Ch10ReadData(hI106In, ulBuffSize, pvBuff);

            // Check for data read errors
            if (enStatus != I106_OK)
                {
                ulReadErrors++;
                break;
                }

            // If this is a new channel, malloc some memory for counts and
            // set the pointer in the channel info array to it.
            if (apsuChanInfo[suI106Hdr.uChID] == NULL)
                {
                apsuChanInfo[suI106Hdr.uChID] = (SuChanInfo *)malloc(sizeof(SuChanInfo));
                memset(apsuChanInfo[suI106Hdr.uChID], 0, sizeof(SuChanInfo));
                apsuChanInfo[suI106Hdr.uChID]->iChanID = suI106Hdr.uChID;
                // Now save channel type and name
                if (suI106Hdr.uChID == 0)
                    {
                    strcpy(apsuChanInfo[suI106Hdr.uChID]->szChanType, "RESERVED");
                    strcpy(apsuChanInfo[suI106Hdr.uChID]->szChanName, "SYSTEM");
                    }
                else
                    {
                    strcpy(apsuChanInfo[suI106Hdr.uChID]->szChanType, "UNKNOWN");
                    strcpy(apsuChanInfo[suI106Hdr.uChID]->szChanName, "UNKNOWN");
                    }
                }

            ulTotal++;
            if (m_bVerbose) 
                fprintf(stderr, "%8.8ld Messages \r",ulTotal);

            // Save data start and stop times
            if ((suI106Hdr.ubyDataType != I106CH10_DTYPE_TMATS) &&
                (suI106Hdr.ubyDataType != I106CH10_DTYPE_IRIG_TIME))
                {
                if (bFoundDataStartTime == bFALSE) 
                    {
                    memcpy((char *)abyStartTime, (char *)suI106Hdr.aubyRefTime, 6);
                    bFoundDataStartTime = bTRUE;
                    }
                else
                    {
                    memcpy((char *)abyStopTime, (char *)suI106Hdr.aubyRefTime, 6);
                    }
                } // end if data message

            // Log the various data types
            switch (suI106Hdr.ubyDataType)
                {

                case I106CH10_DTYPE_USER_DEFINED :      // 0x00
                    apsuChanInfo[suI106Hdr.uChID]->ulUserDefined++;
                    break;

                case I106CH10_DTYPE_TMATS :             // 0x01
                    apsuChanInfo[suI106Hdr.uChID]->ulTMATS++;

                    // Only decode the first TMATS record
                    if (apsuChanInfo[suI106Hdr.uChID]->ulTMATS != 0)
                        {
                        // Save file start time
                        memcpy((char *)&abyFileStartTime, (char *)suI106Hdr.aubyRefTime, 6);

                        // Process TMATS info for later use
                        enI106_Decode_Tmats(&suI106Hdr, pvBuff, &suTmatsInfo);
                        if (enStatus != I106_OK) 
                            break;
                        vProcessTmats(&suTmatsInfo, apsuChanInfo);
                        }
                    break;

                case I106CH10_DTYPE_RECORDING_EVENT :   // 0x02
                    apsuChanInfo[suI106Hdr.uChID]->ulEvents++;
                    break;

                case I106CH10_DTYPE_RECORDING_INDEX :   // 0x03
                    apsuChanInfo[suI106Hdr.uChID]->ulIndex++;
                    break;

                case I106CH10_DTYPE_PCM :               // 0x09
                    apsuChanInfo[suI106Hdr.uChID]->ulPCM++;
                    break;

                case I106CH10_DTYPE_IRIG_TIME :         // 0x11
                    apsuChanInfo[suI106Hdr.uChID]->ulIrigTime++;
                    break;

                case I106CH10_DTYPE_1553_FMT_1 :        // 0x19

                    // If first 1553 message for this channel, setup the 1553 counts
                    if (apsuChanInfo[suI106Hdr.uChID]->psu1553Info == NULL)
                        {
                        apsuChanInfo[suI106Hdr.uChID]->psu1553Info = 
                            malloc(sizeof(SuChanInfo1553));
                        memset(apsuChanInfo[suI106Hdr.uChID]->psu1553Info, 0x00, sizeof(SuChanInfo1553));
                        }

                    apsuChanInfo[suI106Hdr.uChID]->psu1553Info->ulTotalIrigMsgs++;

                    // Step through all 1553 messages
                    enStatus = enI106_Decode_First1553F1(&suI106Hdr, pvBuff, &su1553Msg);
                    while (enStatus == I106_OK)
                        {

                        // Update message count
                        apsuChanInfo[suI106Hdr.uChID]->psu1553Info->ulTotalBusMsgs++;
                        usPackedIdx = (su1553Msg.psuCmdWord1->uValue >> 5) & 0x3FFF;
                        apsuChanInfo[suI106Hdr.uChID]->psu1553Info->aulMsgs[usPackedIdx]++;

                        // Update the error counts
                        if (su1553Msg.psu1553Hdr->bMsgError != 0) 
                            apsuChanInfo[suI106Hdr.uChID]->psu1553Info->aulErrs[usPackedIdx]++;

                        if (su1553Msg.psu1553Hdr->bRespTimeout != 0)
                            apsuChanInfo[suI106Hdr.uChID]->psu1553Info->ulErr1553Timeout++;

                        // Get the next 1553 message
                        enStatus = enI106_Decode_Next1553F1(&su1553Msg);
                        }

                        // If logging RT to RT then do it for second command word
                        if (su1553Msg.psu1553Hdr->bRT2RT == 1)
                            apsuChanInfo[suI106Hdr.uChID]->psu1553Info->bRT2RTFound = bTRUE;

                        if (m_bLogRT2RT==bTRUE) 
                            {
                            usPackedIdx = (su1553Msg.psuCmdWord2->uValue >> 5) & 0x3FFF;
                            apsuChanInfo[suI106Hdr.uChID]->psu1553Info->aulMsgs[usPackedIdx]++;
                        } // end if logging RT to RT

                    break;

                case I106CH10_DTYPE_ANALOG :            // 0x21
                    apsuChanInfo[suI106Hdr.uChID]->ulAnalog++;
                    break;

                case I106CH10_DTYPE_VIDEO_FMT_0 :       // 0x40
                    apsuChanInfo[suI106Hdr.uChID]->ulMPEG2++;
                    break;

                case I106CH10_DTYPE_UART_FMT_0 :        // 0x50
                    apsuChanInfo[suI106Hdr.uChID]->ulUART++;
                    break;

                default:
                    apsuChanInfo[suI106Hdr.uChID]->ulOther++;
                    break;

                } // end switch on message type

            } while (bFALSE); // end one time loop

        // If EOF break out of main read loop
        if (enStatus == I106_EOF)
            {
            break;
            }

        }   /* End while */


/*
 * Now print out the results of histogram.
 * ---------------------------------------
 */

//    vPrintTmats(&suTmatsInfo, ptOutFile);

    fprintf(ptOutFile,"\n=-=-= Message Totals by Channel and Type =-=-=\n\n");
    for (uChanIdx=0; uChanIdx<0x1000; uChanIdx++)
        {
        if (apsuChanInfo[uChanIdx] != NULL)
            {
            vPrintCounts(apsuChanInfo[uChanIdx], ptOutFile);
            }
        }

    
    fprintf(ptOutFile,"=-=-= File Time Summary =-=-=\n\n");

    enI106_Rel2IrigTime(hI106In, abyFileStartTime, &suIrigTime);
    if (suIrigTime.enFmt == I106_DATEFMT_DMY)
        szTimeFmt = szDateTimeFmt;
    else
        szTimeFmt = szDayTimeFmt;
    psuTmTime = gmtime((time_t *)&(suIrigTime.ulSecs));
    strftime(szTime, 50, szTimeFmt, psuTmTime);
    fprintf(ptOutFile,"File Start %s\n",  szTime);

    enI106_Rel2IrigTime(hI106In, abyStartTime, &suIrigTime);
    psuTmTime = gmtime((time_t *)&(suIrigTime.ulSecs));
    strftime(szTime, 50, szTimeFmt, psuTmTime);
    fprintf(ptOutFile,"Data Start %s\n",  szTime);

    enI106_Rel2IrigTime(hI106In, abyStopTime, &suIrigTime);
    psuTmTime = gmtime((time_t *)&(suIrigTime.ulSecs));
    strftime(szTime, 50, szTimeFmt, psuTmTime);
    fprintf(ptOutFile,"Data Stop  %s\n\n",  szTime);

    fprintf(ptOutFile,"\nTOTAL RECORDS:    %10lu\n\n", ulTotal);

/*
 *  Free dynamic memory.
 */

    free(pvBuff);
    pvBuff = NULL;

    fclose(ptOutFile);

    return 0;
    }



/* ------------------------------------------------------------------------ */

void vPrintCounts(SuChanInfo * psuChanInfo, FILE * ptOutFile)
    {
    long            lMsgIdx;

    // Make Channel ID line lead-in string
    fprintf(ptOutFile,"ChanID %3d : %s : %s\n", 
        psuChanInfo->iChanID, psuChanInfo->szChanType, psuChanInfo->szChanName);

    if (psuChanInfo->ulTMATS != 0)
        fprintf(ptOutFile,"    TMATS             %10lu\n",   psuChanInfo->ulTMATS);

    if (psuChanInfo->ulEvents != 0)
        fprintf(ptOutFile,"    Events            %10lu\n",   psuChanInfo->ulEvents);

    if (psuChanInfo->ulIndex != 0)
        fprintf(ptOutFile,"    Index             %10lu\n",   psuChanInfo->ulIndex);

    if (psuChanInfo->ulIrigTime != 0)
        fprintf(ptOutFile,"    IRIG Time         %10lu\n",   psuChanInfo->ulIrigTime);

    if ((psuChanInfo->psu1553Info != NULL)  &&
        (psuChanInfo->psu1553Info->ulTotalBusMsgs != 0))
        {

        // Loop through all RT, TR, and SA combinations
        for (lMsgIdx=0; lMsgIdx<0x4000; lMsgIdx++) 
            {
            if (psuChanInfo->psu1553Info->aulMsgs[lMsgIdx] != 0) 
                {
                fprintf(ptOutFile,"    RT %2d  %c  SA %2d  Msgs %9lu  Errs %9lu\n",
                    (lMsgIdx >>  6) & 0x001f,
                    (lMsgIdx >>  5) & 0x0001 ? 'T' : 'R',
                    (lMsgIdx      ) & 0x001f,
                    psuChanInfo->psu1553Info->aulMsgs[lMsgIdx],
                    psuChanInfo->psu1553Info->aulErrs[lMsgIdx]);
                } // end if count not zero
            } // end for each combination

//      fprintf(ptOutFile,"  Manchester Errors :   %10lu\n", psuChanInfo->ulErrManchester);
//      fprintf(ptOutFile,"  Parity Errors     :   %10lu\n", psuChanInfo->ulErrParity);
//      fprintf(ptOutFile,"  Overrun Errors    :   %10lu\n", psuChanInfo->ulErrOverrun);
//      fprintf(ptOutFile,"  Timeout Errors    :   %10lu\n", psuChanInfo->ulErrTimeout);

        if (psuChanInfo->psu1553Info->bRT2RTFound == bTRUE) 
            {
            fprintf(ptOutFile,"\n  Warning - RT to RT transfers found in the data\n");
            if (m_bLogRT2RT == bTRUE)
                fprintf(ptOutFile,"    Message total is NOT the sum of individual RT totals\n");
            else 
                fprintf(ptOutFile,"    Some transmit RTs may not be shown\n");
            } // end if RT to RT

        fprintf(ptOutFile,"    Totals - %ld Message in %ld IRIG Records\n",
            psuChanInfo->psu1553Info->ulTotalBusMsgs,
            psuChanInfo->psu1553Info->ulTotalIrigMsgs);
        } // end if 1553 messages

    if (psuChanInfo->ulPCM != 0)
        fprintf(ptOutFile,"    PCM               %10lu\n",   psuChanInfo->ulPCM);

    if (psuChanInfo->ulAnalog != 0)
        fprintf(ptOutFile,"    Analog            %10lu\n",   psuChanInfo->ulAnalog);

    if (psuChanInfo->ulMPEG2 != 0)
        fprintf(ptOutFile,"    MPEG Video        %10lu\n",   psuChanInfo->ulMPEG2);

    if (psuChanInfo->ulUART != 0)
        fprintf(ptOutFile,"    UART              %10lu\n",   psuChanInfo->ulUART);

    if (psuChanInfo->ulUserDefined != 0)
        fprintf(ptOutFile,"    User Defined      %10lu\n",   psuChanInfo->ulUserDefined);

    if (psuChanInfo->ulOther != 0)
        fprintf(ptOutFile,"    Other messages    %10lu\n",   psuChanInfo->ulOther);

    fprintf(ptOutFile,"\n",   psuChanInfo->ulOther);
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

void vProcessTmats(SuTmatsInfo * psuTmatsInfo, SuChanInfo * apsuChanInfo[])
    {
//    unsigned            uArrayIdx;
//    unsigned char       u1553ChanIdx;
    SuRRecord         * psuRRecord;
    SuRDataSource     * psuRDataSrc;

    // Find channels mentioned in TMATS record
    psuRRecord   = psuTmatsInfo->psuFirstRRecord;
    while (psuRRecord != NULL)
        {
        // Get the first data source for this R record
        psuRDataSrc = psuRRecord->psuFirstDataSource;
        while (psuRDataSrc != NULL)
            {
            // Make sure a message count structure exists
            if (apsuChanInfo[psuRDataSrc->iTrackNumber] == NULL)
                {
// SOMEDAY PROBABLY WANT TO HAVE DIFFERENT COUNT STRUCTURES FOR EACH CHANNEL TYPE
                apsuChanInfo[psuRDataSrc->iTrackNumber] = malloc(sizeof(SuChanInfo));
                memset(apsuChanInfo[psuRDataSrc->iTrackNumber], 0, sizeof(SuChanInfo));
                apsuChanInfo[psuRDataSrc->iTrackNumber]->iChanID = psuRDataSrc->iTrackNumber;
                }

            // Now save channel type and name
            strcpy(apsuChanInfo[psuRDataSrc->iTrackNumber]->szChanType, psuRDataSrc->szChannelDataType);
            strcpy(apsuChanInfo[psuRDataSrc->iTrackNumber]->szChanName, psuRDataSrc->szDataSourceID);

            // Get the next R record data source
            psuRDataSrc = psuRDataSrc->psuNextRDataSource;
            } // end while walking R data source linked list

        // Get the next R record
        psuRRecord = psuRRecord->psuNextRRecord;

        } // end while walking R record linked list

    return;
    }



/* ------------------------------------------------------------------------ */

void vUsage(void)
    {
    printf("\nI106STAT "MAJOR_VERSION"."MINOR_VERSION" "__DATE__" "__TIME__"\n");
    printf("Print totals by channel and message type from a Ch 10 data file\n");
    printf("Freeware Copyright (C) 2006 Irig106.org\n\n");
    printf("Usage: i106stat <input file> <output file> [flags]\n");
    printf("   <filename> Input/output file names\n");
    printf("   -r         Log both sides of RT to RT transfers\n");
    printf("   -v         Verbose\n");
    }


