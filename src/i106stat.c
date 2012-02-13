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
#include "i106_decode_arinc429.h"
#include "i106_decode_tmats.h"


/*
 * Macros and definitions
 * ----------------------
 */

#define MAJOR_VERSION  "01"
#define MINOR_VERSION  "03"

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
    unsigned long       ulTotalIrigPackets;
    unsigned long       ulTotalBusMsgs;
    unsigned long       ulTotalIrigPacketErrors;
    unsigned long       aulMsgs[0x4000];
    unsigned long       aulErrs[0x4000];
    unsigned long       ulErr1553Timeout;
    int                 bRT2RTFound;
    } SuChanInfo1553;

// ARINC 429 counts
typedef struct
    { 
    unsigned long   aulMsgs[0x100][0x100];
    } SuARINC429;


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
    unsigned long       ulEthernet;
    SuARINC429        * paARINC429;
    unsigned long       ulOther;
    } SuChanInfo;


/*
 * Module data
 * -----------
 */

int                 m_bLogRT2RT;
int                 m_bVerbose;
unsigned char       m_aArincLabelMap[0x100];


/*
 * Function prototypes
 * -------------------
 */

void     vPrintCounts(SuChanInfo * psuChanInfo, FILE * psuOutFile);
void     vPrintTmats(SuTmatsInfo * psuTmatsInfo, FILE * psuOutFile);
void     vProcessTmats(SuTmatsInfo * psuTmatsInfo, SuChanInfo * apsuChanInfo[]);
void     vMakeArincLabelMap(unsigned char m_aArincLabelMap[]);
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
    unsigned long           ulBadPackets;
    unsigned long           ulTotal;

    FILE                  * psuOutFile;        // Output file handle
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
    SuArinc429F0_CurrMsg    suArincMsg;
    SuTmatsInfo             suTmatsInfo;
    SuIrig106Time           suIrigTime;
    struct tm             * psuTmTime;
    char                    szTime[50];
    char                  * szDateTimeFmt = "%m/%d/%Y %H:%M:%S";
    char                  * szDayTimeFmt  = "%j:%H:%M:%S";
    char                  * szTimeFmt;

    unsigned char         * pvBuff = NULL;


// Make sure things stay on UTC

    putenv("TZ=GMT0");
    tzset();

/*
 * Initialize the channel info array pointers to all NULL
 */

    memset(apsuChanInfo, 0, sizeof(apsuChanInfo));
    ulTotal      = 0L;
    ulReadErrors = 0L;
    ulBadPackets = 0L;

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
        psuOutFile = fopen(szOutFile,"w");
        if (psuOutFile == NULL) 
            {
            fprintf(stderr, "Error opening output file\n");
            return 1;
            }
        }

    // No output file name so use stdout
    else
        {
        psuOutFile = stdout;
        }

    // Make the ARINC label map just in case
    vMakeArincLabelMap(m_aArincLabelMap);

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
                        memset( &suTmatsInfo, 0, sizeof(suTmatsInfo) );
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

                    apsuChanInfo[suI106Hdr.uChID]->psu1553Info->ulTotalIrigPackets++;

                    // Step through all 1553 messages
                    enStatus = enI106_Decode_First1553F1(&suI106Hdr, pvBuff, &su1553Msg);
                    if (enStatus == I106_OK)
                        {
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

                            // If logging RT to RT then do it for second command word
                            if (su1553Msg.psu1553Hdr->bRT2RT == 1)
                                {
                                apsuChanInfo[suI106Hdr.uChID]->psu1553Info->bRT2RTFound = bTRUE;

                                if (m_bLogRT2RT==bTRUE) 
                                    {
                                    usPackedIdx = (su1553Msg.psuCmdWord2->uValue >> 5) & 0x3FFF;
                                    apsuChanInfo[suI106Hdr.uChID]->psu1553Info->aulMsgs[usPackedIdx]++;
                                    } // end if logging RT to RT
                                } // end if RT to RT

                            // Get the next 1553 message
                            enStatus = enI106_Decode_Next1553F1(&su1553Msg);
                            } // end while I106_OK
                        } // end if Decode First 1553 OK

                    // Decode not good so mark it as a packet error
                    else
                        {
                        apsuChanInfo[suI106Hdr.uChID]->psu1553Info->ulTotalIrigPacketErrors++;
                        }

                    break;

                case I106CH10_DTYPE_ANALOG :            // 0x21
                    apsuChanInfo[suI106Hdr.uChID]->ulAnalog++;
                    break;

                case I106CH10_DTYPE_ARINC_429_FMT_0 :   // 0x38
                    // If first ARINC 429 message for this channel, setup the counts
                    if (apsuChanInfo[suI106Hdr.uChID]->paARINC429 == NULL)
                        {
                        apsuChanInfo[suI106Hdr.uChID]->paARINC429 = 
                            malloc(sizeof(SuARINC429));
                        memset(apsuChanInfo[suI106Hdr.uChID]->paARINC429, 0x00, sizeof(SuARINC429));
                        }

                    // Step through all ARINC 429 messages
                    enStatus = enI106_Decode_FirstArinc429F0(&suI106Hdr, pvBuff, &suArincMsg);
                    if (enStatus == I106_OK)
                        {
                        while (enStatus == I106_OK)
                            {
                            unsigned char   uBus;
                            unsigned char   uLabel;

                            // Update message count
                            uBus   = (unsigned char)suArincMsg.psu429Hdr->uBusNum;
                            uLabel = (unsigned char)m_aArincLabelMap[suArincMsg.psu429Data->uLabel];
                            apsuChanInfo[suI106Hdr.uChID]->paARINC429->aulMsgs[uBus][uLabel]++;

                            // Get the next ARINC 429 message
                            enStatus = enI106_Decode_NextArinc429F0(&suArincMsg);
                            } // end while I106_OK
                        } // end if Decode First ARINC 429 OK

                    break;

                case I106CH10_DTYPE_VIDEO_FMT_0 :       // 0x40
                    apsuChanInfo[suI106Hdr.uChID]->ulMPEG2++;
                    break;

                case I106CH10_DTYPE_UART_FMT_0 :        // 0x50
                    apsuChanInfo[suI106Hdr.uChID]->ulUART++;
                    break;


                case I106CH10_DTYPE_ETHERNET_FMT_0 :    // 0x68
                    apsuChanInfo[suI106Hdr.uChID]->ulEthernet++;
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

//    vPrintTmats(&suTmatsInfo, psuOutFile);

    fprintf(psuOutFile,"\n=-=-= Message Totals by Channel and Type =-=-=\n\n");
    for (uChanIdx=0; uChanIdx<0x1000; uChanIdx++)
        {
        if (apsuChanInfo[uChanIdx] != NULL)
            {
            vPrintCounts(apsuChanInfo[uChanIdx], psuOutFile);
            }
        }

    
    fprintf(psuOutFile,"=-=-= File Time Summary =-=-=\n\n");

    enI106_Rel2IrigTime(hI106In, abyFileStartTime, &suIrigTime);
    if (suIrigTime.enFmt == I106_DATEFMT_DMY)
        szTimeFmt = szDateTimeFmt;
    else
        szTimeFmt = szDayTimeFmt;
    psuTmTime = gmtime((time_t *)&(suIrigTime.ulSecs));
    strftime(szTime, 50, szTimeFmt, psuTmTime);
    fprintf(psuOutFile,"File Start %s\n",  szTime);

    enI106_Rel2IrigTime(hI106In, abyStartTime, &suIrigTime);
    psuTmTime = gmtime((time_t *)&(suIrigTime.ulSecs));
    strftime(szTime, 50, szTimeFmt, psuTmTime);
    fprintf(psuOutFile,"Data Start %s\n",  szTime);

    enI106_Rel2IrigTime(hI106In, abyStopTime, &suIrigTime);
    psuTmTime = gmtime((time_t *)&(suIrigTime.ulSecs));
    strftime(szTime, 50, szTimeFmt, psuTmTime);
    fprintf(psuOutFile,"Data Stop  %s\n\n",  szTime);

    fprintf(psuOutFile,"\nTOTAL RECORDS:    %10lu\n\n", ulTotal);

/*
 *  Free dynamic memory.
 */

    free(pvBuff);
    pvBuff = NULL;

    fclose(psuOutFile);

    return 0;
    }



/* ------------------------------------------------------------------------ */

void vPrintCounts(SuChanInfo * psuChanInfo, FILE * psuOutFile)
    {
    long            lMsgIdx;

    // Make Channel ID line lead-in string
    fprintf(psuOutFile,"ChanID %3d : %s : %s\n", 
        psuChanInfo->iChanID, psuChanInfo->szChanType, psuChanInfo->szChanName);

    if (psuChanInfo->ulTMATS != 0)
        fprintf(psuOutFile,"    TMATS             %10lu\n",   psuChanInfo->ulTMATS);

    if (psuChanInfo->ulEvents != 0)
        fprintf(psuOutFile,"    Events            %10lu\n",   psuChanInfo->ulEvents);

    if (psuChanInfo->ulIndex != 0)
        fprintf(psuOutFile,"    Index             %10lu\n",   psuChanInfo->ulIndex);

    if (psuChanInfo->ulIrigTime != 0)
        fprintf(psuOutFile,"    IRIG Time         %10lu\n",   psuChanInfo->ulIrigTime);

    if ((psuChanInfo->psu1553Info != NULL)  &&
        (psuChanInfo->psu1553Info->ulTotalBusMsgs != 0))
        {

        // Loop through all RT, TR, and SA combinations
        for (lMsgIdx=0; lMsgIdx<0x4000; lMsgIdx++) 
            {
            if (psuChanInfo->psu1553Info->aulMsgs[lMsgIdx] != 0) 
                {
                fprintf(psuOutFile,"    RT %2d  %c  SA %2d  Msgs %9lu  Errs %9lu\n",
                    (lMsgIdx >>  6) & 0x001f,
                    (lMsgIdx >>  5) & 0x0001 ? 'T' : 'R',
                    (lMsgIdx      ) & 0x001f,
                    psuChanInfo->psu1553Info->aulMsgs[lMsgIdx],
                    psuChanInfo->psu1553Info->aulErrs[lMsgIdx]);
                } // end if count not zero
            } // end for each combination

//      fprintf(psuOutFile,"  Manchester Errors :   %10lu\n", psuChanInfo->ulErrManchester);
//      fprintf(psuOutFile,"  Parity Errors     :   %10lu\n", psuChanInfo->ulErrParity);
//      fprintf(psuOutFile,"  Overrun Errors    :   %10lu\n", psuChanInfo->ulErrOverrun);
//      fprintf(psuOutFile,"  Timeout Errors    :   %10lu\n", psuChanInfo->ulErrTimeout);

        if (psuChanInfo->psu1553Info->bRT2RTFound == bTRUE) 
            {
            fprintf(psuOutFile,"\n  Warning - RT to RT transfers found in the data\n");
            if (m_bLogRT2RT == bTRUE)
                fprintf(psuOutFile,"    Message total is NOT the sum of individual RT totals\n");
            else 
                fprintf(psuOutFile,"    Some transmit RTs may not be shown\n");
            } // end if RT to RT

        fprintf(psuOutFile,"    Totals - %ld Message in %ld good IRIG packets, %ld bad packets\n",
            psuChanInfo->psu1553Info->ulTotalBusMsgs,
            psuChanInfo->psu1553Info->ulTotalIrigPackets,
            psuChanInfo->psu1553Info->ulTotalIrigPacketErrors);
        } // end if 1553 messages

    if (psuChanInfo->ulPCM != 0)
        fprintf(psuOutFile,"    PCM               %10lu\n",   psuChanInfo->ulPCM);

    if (psuChanInfo->ulAnalog != 0)
        fprintf(psuOutFile,"    Analog            %10lu\n",   psuChanInfo->ulAnalog);

    if (psuChanInfo->paARINC429 != NULL)
        {
        unsigned int    uBus;
        unsigned int    uLabel;
        for (uBus=0; uBus<0x100; uBus++)
            for (uLabel=0; uLabel<0x100; uLabel++)
                {
                if (psuChanInfo->paARINC429->aulMsgs[uBus][uLabel] != 0)
                    fprintf(psuOutFile,"    ARINC 429  Subchan %3u  Label %3o    Msgs %10lu\n", uBus, uLabel, 
                        psuChanInfo->paARINC429->aulMsgs[uBus][uLabel]);
                }
        }


    if (psuChanInfo->ulMPEG2 != 0)
        fprintf(psuOutFile,"    MPEG Video        %10lu\n",   psuChanInfo->ulMPEG2);

    if (psuChanInfo->ulUART != 0)
        fprintf(psuOutFile,"    UART              %10lu\n",   psuChanInfo->ulUART);

    if (psuChanInfo->ulUserDefined != 0)
        fprintf(psuOutFile,"    User Defined      %10lu\n",   psuChanInfo->ulUserDefined);

    if (psuChanInfo->ulEthernet != 0)
        fprintf(psuOutFile,"    Ethernet          %10lu\n",   psuChanInfo->ulEthernet);

    if (psuChanInfo->ulOther != 0)
        fprintf(psuOutFile,"    Other messages    %10lu\n",   psuChanInfo->ulOther);

    fprintf(psuOutFile,"\n",   psuChanInfo->ulOther);
    return;
    }




/* ------------------------------------------------------------------------ */

void vPrintTmats(SuTmatsInfo * psuTmatsInfo, FILE * psuOutFile)
    {
    int                     iGIndex;
    int                     iRIndex;
//    int                     iRDsiIndex;
    SuGDataSource         * psuGDataSource;
    SuRRecord             * psuRRecord;
    SuRDataSource         * psuRDataSource;

    // Print out the TMATS info
    // ------------------------

    fprintf(psuOutFile,"\n=-=-= Channel Summary =-=-=\n\n");

    // G record
    fprintf(psuOutFile,"Program Name - %s\n",psuTmatsInfo->psuFirstGRecord->szProgramName);
    fprintf(psuOutFile,"IRIG 106 Rev - %s\n",psuTmatsInfo->psuFirstGRecord->szIrig106Rev);
    fprintf(psuOutFile,"Channel  Type          Data Source         \n");
    fprintf(psuOutFile,"-------  ------------  --------------------\n");

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
//                iRDsiIndex = psuRDataSource->iDataSourceNum;
                fprintf(psuOutFile," %5s ",   psuRDataSource->szTrackNumber);
                fprintf(psuOutFile,"  %-12s", psuRDataSource->szChannelDataType);
                fprintf(psuOutFile,"  %-20s", psuRDataSource->szDataSourceID);
                fprintf(psuOutFile,"\n");
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
    int                 iTrackNumber;

    // Find channels mentioned in TMATS record
    psuRRecord   = psuTmatsInfo->psuFirstRRecord;
    while (psuRRecord != NULL)
        {
        // Get the first data source for this R record
        psuRDataSrc = psuRRecord->psuFirstDataSource;
        while (psuRDataSrc != NULL)
            {
            iTrackNumber = atoi(psuRDataSrc->szTrackNumber);

            // Make sure a message count structure exists
            if (apsuChanInfo[iTrackNumber] == NULL)
                {
// SOMEDAY PROBABLY WANT TO HAVE DIFFERENT COUNT STRUCTURES FOR EACH CHANNEL TYPE
                apsuChanInfo[iTrackNumber] = malloc(sizeof(SuChanInfo));
                memset(apsuChanInfo[iTrackNumber], 0, sizeof(SuChanInfo));
                apsuChanInfo[iTrackNumber]->iChanID = iTrackNumber;
                }

            // Now save channel type and name
            strcpy(apsuChanInfo[iTrackNumber]->szChanType, psuRDataSrc->szChannelDataType);
            strcpy(apsuChanInfo[iTrackNumber]->szChanName, psuRDataSrc->szDataSourceID);

            // Get the next R record data source
            psuRDataSrc = psuRDataSrc->psuNextRDataSource;
            } // end while walking R data source linked list

        // Get the next R record
        psuRRecord = psuRRecord->psuNextRRecord;

        } // end while walking R record linked list

    return;
    }



/* ------------------------------------------------------------------------ */

// The ARINC 429 label field is bit reversed. This array maps the ARINC
// label field to its non-reversed bretheren.

void vMakeArincLabelMap(unsigned char m_aArincLabelMap[])
    {
    unsigned int    uLabelIdx;
    unsigned char   uRLabel;
    unsigned char   uLabel;
    int             iBitIdx;

    for (uLabelIdx=0; uLabelIdx<0x100; uLabelIdx++)
        {
        uLabel = (unsigned char)uLabelIdx;
        uRLabel = 0;
        for (iBitIdx=0; iBitIdx<8; iBitIdx++)
            {
            uRLabel <<= 1;
            uRLabel  |= uLabel & 0x01;
            uLabel  >>= 1;
            } // end for each bit in the label
        m_aArincLabelMap[uLabelIdx] = uRLabel;
        } // end for each label

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


