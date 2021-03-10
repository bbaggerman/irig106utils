/* 

 idmptime - A utility for dumping IRIG 106 Ch 10 time packets

 Copyright (c) 2010 Irig106.org

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

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "config.h"
#include "i106_stdint.h"
#include "irig106ch10.h"

#include "i106_time.h"
#include "i106_index.h"
#include "i106_decode_time.h"
#include "i106_decode_tmats.h"


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

int           m_iVersion;    // Data file version
int           m_usMaxBuffSize;

int           m_iI106Handle;


/*
 * Function prototypes
 * -------------------
 */

void vPrintTmats(SuTmatsInfo * psuTmatsInfo, FILE * psuOutFile);
void vUsage(void);


/* ------------------------------------------------------------------------ */

int main(int argc, char ** argv)
  {

    char                    szInFile[256];     // Input file name
    char                    szOutFile[256];    // Output file name
    int                     iArgIdx;
    FILE                  * psuOutFile;        // Output file handle
    int                     iChannel;         // Channel number
    unsigned long           lTimeMsgs = 0;    // Total time messages
    int                     bVerbose;
    int                     bPrintTMATS;
    int                     bFoundIndex;
    SuPacketIndexInfo     * asuPacketIndex;
    uint32_t                uCurrIndex;
    uint32_t                uNumIndexes;
    int                     bTryIndex;
    int                     bUseIndex;

    unsigned long           ulBuffSize = 0L;
    int64_t                 llRelTime;
 
    EnI106Status            enStatus;
    SuI106Ch10Header        suI106Hdr;

    unsigned char         * pvBuff  = NULL;
    SuTmatsInfo             suTmatsInfo;

    SuTimeF1_ChanSpec     * psuChanSpecTime;
    SuTime_MsgDmyFmt      * psuTimeDmy;
    SuTime_MsgDayFmt      * psuTimeDay;

    int                     iSec;
    int                     iMin;
    int                     iHour;
    int                     iYDay;
    int                     iMDay;
    int                     iMon;
    int                     iYear;


/*
 * Process the command line arguements
 */

    if (argc < 2) 
        {
        vUsage();
        return 1;
        }

    iChannel        = -1;

    bVerbose        = bFALSE;            /* No verbosity                      */
    bTryIndex       = bFALSE;
    bUseIndex       = bFALSE;
    bPrintTMATS     = bFALSE;

    szInFile[0]  = '\0';
    strcpy(szOutFile,"");                     // Default is stdout

    for (iArgIdx=1; iArgIdx<argc; iArgIdx++) 
        {
        switch (argv[iArgIdx][0]) 
            {

            case '-' :
                switch (argv[iArgIdx][1]) 
                    {
                    case 'v' :                   /* Verbose switch */
                        bVerbose = bTRUE;
                        break;

                    case 'c' :                   /* Channel number */
                        iArgIdx++;
                        sscanf(argv[iArgIdx],"%d",&iChannel);
                        break;

                    case 'i' :                   /* Try to use index flag */
                        bTryIndex = bTRUE;
                        break;

                    case 'T' :                   /* Print TMATS flag */
                        bPrintTMATS = bTRUE;
                        break;

                    default :
                        break;
                    } /* end flag switch */
                break;

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

    fprintf(stderr, "\nIDMPTIME "MAJOR_VERSION"."MINOR_VERSION"\n");
    fprintf(stderr, "Freeware Copyright (C) 2019 Irig106.org\n\n");

	putenv("TZ=GMT0");
	tzset();

/*
 *  Open file and allocate a buffer for reading data.
 */

    enStatus = enI106Ch10Open(&m_iI106Handle, szInFile, I106_READ);

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

/*
 * Open the output file
 */

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


/*
 * Read the first header. If TMATS flag set, print TMATS and exit
 */

    // Read first header and check for data read errors
    enStatus = enI106Ch10ReadNextHeader(m_iI106Handle, &suI106Hdr);
    if (enStatus != I106_OK)
        return 1;

    // If TMATS flag set, just print TMATS and exit
    if (bPrintTMATS == bTRUE)
        {
        if (suI106Hdr.ubyDataType == I106CH10_DTYPE_TMATS)
            {
            // Make a data buffer for TMATS
            pvBuff = malloc(suI106Hdr.ulPacketLen);

            // Read the data buffer and check for read errors
            enStatus = enI106Ch10ReadData(m_iI106Handle, suI106Hdr.ulPacketLen, pvBuff);
            if (enStatus != I106_OK)
                return 1;

            // Process the TMATS info
            memset( &suTmatsInfo, 0, sizeof(suTmatsInfo) );
            enStatus = enI106_Decode_Tmats(&suI106Hdr, pvBuff, &suTmatsInfo);
            if (enStatus != I106_OK) 
                {
                fprintf(stderr, " Error processing TMATS record : Status = %d\n", enStatus);
                return 1;
                }

            vPrintTmats(&suTmatsInfo, psuOutFile);
            } // end if TMATS

        // TMATS not first message
        else
            {
            printf("Error - TMATS message not found\n");
            return 1;
            }

        return 0;
        } // end if print TMATS

/*
 * If index is present then read and process index info
 */

    bUseIndex = bFALSE;
    if (bTryIndex == bTRUE)
        {
        enStatus = enIndexPresent(m_iI106Handle, &bFoundIndex);
        if ((enStatus == I106_OK) && (bFoundIndex == bTRUE))
            {
            InitIndex(m_iI106Handle);
            enStatus = enReadIndexes(m_iI106Handle);
            if (enStatus == I106_OK)
                {
                enStatus = enGetIndexArray(m_iI106Handle, &asuPacketIndex, &uNumIndexes);
                if (enStatus == I106_OK)
                    {
                    bUseIndex = bTRUE;
                    printf("Using Time Index\n\n");
                    }
                }
            } // end if use index
        }

/*
 * Read messages until error or EOF
 */

    uCurrIndex = 0;
    while (1==1) 
        {

        if (bUseIndex == bTRUE)
            {
            // Walk index array looking for time
            while (uCurrIndex < uNumIndexes)
                {
                if  (asuPacketIndex[uCurrIndex].ubyDataType == I106CH10_DTYPE_IRIG_TIME)
                    break;
                else
                    uCurrIndex++;
                }
            if (uCurrIndex < uNumIndexes)
                {
                enI106Ch10SetPos(m_iI106Handle, asuPacketIndex[uCurrIndex].lFileOffset);
                enStatus = enI106Ch10ReadNextHeader(m_iI106Handle, &suI106Hdr);
                uCurrIndex++;
                }
            else
                {
                enStatus = I106_EOF;
                }
            } // end if use index
        else
            enStatus = enI106Ch10ReadNextHeader(m_iI106Handle, &suI106Hdr);

        // Setup a one time loop to make it easy to break out on error
        do
            {
            if (enStatus == I106_EOF)
                break;

            // Check for header read errors
            if (enStatus != I106_OK)
                break;

            // If time message then process it
            if ((suI106Hdr.ubyDataType == I106CH10_DTYPE_IRIG_TIME) &&
                ((iChannel == -1) || (iChannel == (int)suI106Hdr.uChID)))
                {

                // Make sure our buffer is big enough, size *does* matter
                if (ulBuffSize < suI106Hdr.ulPacketLen)
                    {
                    // Allocate new buffer
                    pvBuff     = realloc(pvBuff, suI106Hdr.ulPacketLen);
                    ulBuffSize = suI106Hdr.ulPacketLen;

                    // Make pointers to time structures
                    psuChanSpecTime = (SuTimeF1_ChanSpec *)pvBuff;
                    psuTimeDay      = (SuTime_MsgDayFmt *)((char *)pvBuff + sizeof(SuTimeF1_ChanSpec));
                    psuTimeDmy      = (SuTime_MsgDmyFmt *)((char *)pvBuff + sizeof(SuTimeF1_ChanSpec));
                    }
 
                // Read the data buffer
                enStatus = enI106Ch10ReadData(m_iI106Handle, ulBuffSize, pvBuff);

                // Check for data read errors
                if (enStatus != I106_OK)
                    break;

                fprintf(psuOutFile,"%3d ", suI106Hdr.uChID);

                // Print Channel ID

                // Print out the relative time value
                vTimeArray2LLInt(suI106Hdr.aubyRefTime, &llRelTime);
//              fprintf(psuOutFile,"0x%12.12llx ", llRelTime);
                fprintf(psuOutFile,"%14lld ", llRelTime);

                // Time in Day format
                if (psuChanSpecTime->uDateFmt == 0)
                    {
                    iSec   = psuTimeDay->uTSn *  10 + psuTimeDay->uSn;
                    iMin   = psuTimeDay->uTMn *  10 + psuTimeDay->uMn;
                    iHour  = psuTimeDay->uTHn *  10 + psuTimeDay->uHn;

                    iYDay  = psuTimeDay->uHDn * 100 + psuTimeDay->uTDn * 10 + psuTimeDay->uDn;

// THIS WOULD BE USEFUL IF I WANT TO FORCE MONTH AND DAY DISPLAY

                    //// Make day
                    //if (psuChanSpecTime->bLeapYear)
                    //    {
                    //    suTmTime.tm_mday  = suDoy2DmLeap[suTmTime.tm_yday].iDay;
                    //    suTmTime.tm_mon   = suDoy2DmLeap[suTmTime.tm_yday].iMonth;
                    //    suTmTime.tm_year  = 72;  // i.e. 1972, a leap year
                    //    }
                    //else
                    //    {
                    //    suTmTime.tm_mday  = suDoy2DmNormal[suTmTime.tm_yday].iDay;
                    //    suTmTime.tm_mon   = suDoy2DmNormal[suTmTime.tm_yday].iMonth;
                    //    suTmTime.tm_year  = 71;  // i.e. 1971, not a leap year
                    //    }

                    fprintf(psuOutFile,"%3.3d:%2.2d:%2.2d:%2.2d", iYDay, iHour, iMin, iSec);
                    }

                // Time in DMY format
                else
                    {
                    iSec   = psuTimeDmy->uTSn *   10 + psuTimeDmy->uSn;
                    iMin   = psuTimeDmy->uTMn *   10 + psuTimeDmy->uMn;
                    iHour  = psuTimeDmy->uTHn *   10 + psuTimeDmy->uHn;

                    iMDay  = psuTimeDmy->uTDn *   10 + psuTimeDmy->uDn;
                    iMon   = psuTimeDmy->uTOn *   10 + psuTimeDmy->uOn;
                    iYear  = psuTimeDmy->uOYn * 1000 + psuTimeDmy->uHYn * 100 + 
                             psuTimeDmy->uTYn *   10 + psuTimeDmy->uYn;
                    fprintf(psuOutFile,"%2.2d/%2.2d/%4.4d %2.2d:%2.2d:%2.2d", iMon, iMDay, iYear, iHour, iMin, iSec);
                    }

                // Print various status flags
                switch (psuChanSpecTime->uTimeSrc)
                    {
                    case I106_TIMESRC_INTERNAL     :
                        fprintf(psuOutFile," Internal/Unlocked");
                        break;
                    case I106_TIMESRC_EXTERNAL     :
                        fprintf(psuOutFile," External/Locked  ");
                        break;
                    case I106_TIMESRC_INTERNAL_RMM :
                        fprintf(psuOutFile," Internal/RMM     ");
                        break;
                    case I106_TIMESRC_NONE         :
                        fprintf(psuOutFile," None             ");
                        break;
                    default                        :
                        fprintf(psuOutFile," Source Unknown   ");
                        break;

                    } // end switch on Time Source
                    
                switch (psuChanSpecTime->uTimeFmt)
                    {
                    case I106_TIMEFMT_IRIG_B     :
                        fprintf(psuOutFile,", IRIG-B        ");
                        break;
                    case I106_TIMEFMT_IRIG_A     :
                        fprintf(psuOutFile,", IRIG-A        ");
                        break;
                    case I106_TIMEFMT_IRIG_G     :
                        fprintf(psuOutFile,", IRIG-G        ");
                        break;
                    case I106_TIMEFMT_INT_RTC    :
                        fprintf(psuOutFile,", Internal Clock");
                        break;
                    case I106_TIMEFMT_GPS_UTC    :
                        fprintf(psuOutFile,", UTC From GPS  ");
                        break;
                    case I106_TIMEFMT_GPS_NATIVE :
                        fprintf(psuOutFile,", Native GPS    ");
                        break;
                    default                      :
                        fprintf(psuOutFile,", Format Unknown");
                        break;
                    } // end switch on Time Format


                if (psuChanSpecTime->bLeapYear) fprintf(psuOutFile,", Leap Year");
                else                            fprintf(psuOutFile,", Not Leap Year");

                // Print out the data
                fprintf(psuOutFile,"\n");
                fflush(psuOutFile);

                lTimeMsgs++;
                } // end if time packet 

            } while (bFALSE); // end one time loop

        // If EOF break out of main read loop
        if (enStatus == I106_EOF)
            {
            fprintf(stderr, "End of file\n");
            break;
            }

        }   // end infinite while

/*
 * Print out some summaries
 */

    printf("\nTime Message %lu\n", lTimeMsgs);

/*
 *  Close files
 */

    enI106Ch10Close(m_iI106Handle);
    fclose(psuOutFile);

    return 0;
    }



/* ------------------------------------------------------------------------ */

void vPrintTmats(SuTmatsInfo * psuTmatsInfo, FILE * psuOutFile)
    {
    int                     iGIndex;
    int                     iRIndex;
    SuGDataSource         * psuGDataSource;
    SuRRecord             * psuRRecord;
    SuRDataSource         * psuRDataSource;

    // Print out the TMATS info
    // ------------------------

    fprintf(psuOutFile,"\n=-=-= 1553 Channel Summary =-=-=\n\n");

    // G record
    fprintf(psuOutFile,"Program Name - %s\n",psuTmatsInfo->psuFirstGRecord->szProgramName);
    fprintf(psuOutFile,"\n");
    fprintf(psuOutFile,"Channel  Data Source         \n");
    fprintf(psuOutFile,"-------  --------------------\n");

    // Data sources
    psuGDataSource = psuTmatsInfo->psuFirstGRecord->psuFirstGDataSource;
    do  {
        if (psuGDataSource == NULL) break;

        // G record data source info
        iGIndex = psuGDataSource->iIndex;

        // R record info
        psuRRecord = psuGDataSource->psuRRecord;
        do  {
            if (psuRRecord == NULL) break;
            iRIndex = psuRRecord->iIndex;

            // R record data sources
            psuRDataSource = psuRRecord->psuFirstDataSource;
            do  {
                if (psuRDataSource == NULL) 
                    break;
                if (strcasecmp(psuRDataSource->szChannelDataType,"1553IN") == 0)
                    {
//                    iRDsiIndex = psuRDataSource->iIndex;
                    fprintf(psuOutFile," %5s ",   psuRDataSource->szTrackNumber);
                    fprintf(psuOutFile,"  %-20s", psuRDataSource->szDataSourceID);
                    fprintf(psuOutFile,"\n");
                    }
                psuRDataSource = psuRDataSource->psuNext;
                } while (bTRUE);

            psuRRecord = psuRRecord->psuNext;
            } while (bTRUE);


        psuGDataSource = psuTmatsInfo->psuFirstGRecord->psuFirstGDataSource->psuNext;
        } while (bTRUE);

    return;
    }


/* ------------------------------------------------------------------------ */

void vUsage(void)
    {
    printf("\nIDMPTIME "MAJOR_VERSION"."MINOR_VERSION" "__DATE__" "__TIME__"\n");
    printf("Dump time records from a Ch 10 data file\n");
    printf("Freeware Copyright (C) 2019 Irig106.org\n\n");
    printf("Usage: idmptime <input file> <output file> [flags]\n");
    printf("   <filename> Input/output file names        \n");
    printf("   -v         Verbose                        \n");
    printf("   -c ChNum   Channel Number (default all)   \n");
    printf("   -i         Use indexes if available       \n");
    printf("   -T         Print TMATS summary and exit   \n");
    printf("                                             \n");
    printf("Time columns are:                            \n");
    printf("  Channel ID                                 \n");
    printf("  Relative Time Counter Value                \n");
    printf("  Time                                       \n");
    printf("  Time Source                                \n");
    printf("  Time Format                                \n");
    printf("  Leap Year Flag                             \n");
    }




