/* 

 idmp1553 - A utility for dumping IRIG 106 Ch 10 1553 data

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

*/

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
    char                    szIdxFileName[256];
    char                  * pchFileNameChar;
    int                     iArgIdx;
    FILE                  * psuOutFile;        // Output file handle
    char                  * szTime;
    int                     iWordIdx;
    int                     iMicroSec;
    int                     iChannel;         // Channel number
    int                     iRTAddr;          // RT address
    int                     iTR;              // Transmit bit
    int                     iSubAddr;         // Subaddress
    unsigned                uDecimation;      // Decimation factor
    unsigned                uDecCnt;          // Decimation count
    unsigned long           lMsgs = 0;        // Total message
    unsigned long           l1553Msgs = 0;
    int                     bVerbose;
    int                     bDecimal;         // Hex/decimal flag
    int                     bStatusResponse;
    int                     bPrintTMATS;
    int                     bInOrder;         // Dump out in order
    unsigned long           ulBuffSize = 0L;
    unsigned int            uErrorFlags;

    int                     iStatus;
    EnI106Status            enStatus;
    SuI106Ch10Header        suI106Hdr;

    unsigned char         * pvBuff  = NULL;
    SuIrig106Time           suTime;
    Su1553F1_CurrMsg        su1553Msg;
    SuTmatsInfo             suTmatsInfo;

/*
 * Process the command line arguements
 */

    if (argc < 2) 
        {
        vUsage();
        return 1;
        }

    iChannel        = -1;
    iRTAddr         = -1;
    iTR             = -1;
    iSubAddr        = -1;

    uDecimation     = 1;                 /* Decimation factor                 */
    bVerbose        = bFALSE;            /* No verbosity                      */
    bDecimal        = bFALSE;
    bStatusResponse = bFALSE;
    bPrintTMATS     = bFALSE;
    bInOrder        = bFALSE;

    szInFile[0]  = '\0';
    strcpy(szOutFile,"");                     // Default is stdout

  for (iArgIdx=1; iArgIdx<argc; iArgIdx++) {

    switch (argv[iArgIdx][0]) {

      case '-' :
        switch (argv[iArgIdx][1]) {

          case 'v' :                   /* Verbose switch */
            bVerbose = bTRUE;
            break;

          case 'c' :                   /* Channel number */
            iArgIdx++;
            sscanf(argv[iArgIdx],"%d",&iChannel);
            break;

          case 'r' :                   /* RT address */
            iArgIdx++;
            sscanf(argv[iArgIdx],"%d",&iRTAddr);
            if (iRTAddr>31) {
              printf("Invalid RT address\n");
              vUsage();
              return 1;
              }
            break;

          case 't' :                   /* TR bit */
            iArgIdx++;
            sscanf(argv[iArgIdx],"%d",&iTR);
            if ((iTR!=0)&&(iTR!=1)) {
              printf("Invalid TR flag\n");
              vUsage();
              return 1;
              }
            break;

          case 's' :                   /* Subaddress */
            iArgIdx++;
            sscanf(argv[iArgIdx],"%d",&iSubAddr);
            if (iSubAddr>31) {
              printf("Invalid subaddress\n");
              vUsage();
              return 1;
              }
            break;
          case 'd' :                   /* Decimation */
            iArgIdx++;
            sscanf(argv[iArgIdx],"%d",&uDecimation);
            break;

          case 'i' :                   /* Hex/decimal flag */
            bDecimal = bTRUE;
            break;

          case 'u' :                   /* Status response flag */
            bStatusResponse = bTRUE;
            break;

          case 'o' :                   /* Dump in time order */
            bInOrder = bTRUE;
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

  uDecCnt = uDecimation;

/*
 * Opening banner
 * --------------
 */

    fprintf(stderr, "\nIDMP1553 "MAJOR_VERSION"."MINOR_VERSION"\n");
    fprintf(stderr, "Freeware Copyright (C) 2006 Irig106.org\n\n");

	putenv("TZ=GMT0");
	tzset();

/*
 *  Open file and allocate a buffer for reading data.
 */

    if (bInOrder)
        {
        do 
            {
            // Open the data file
            enStatus = enI106Ch10Open(&m_iI106Handle, szInFile, I106_READ_IN_ORDER);
            if (enStatus != I106_OK)
                break;

            // Make the index file name
            strcpy(szIdxFileName, szInFile);
            pchFileNameChar = strrchr(szIdxFileName, '.');
            if (pchFileNameChar != NULL)
                *pchFileNameChar = '\0';
            strcat(szIdxFileName, ".iid");

            // Read or make the index
            iStatus = bReadInOrderIndex(m_iI106Handle, szIdxFileName);
            if (iStatus == bFALSE)
                {
                vMakeInOrderIndex(m_iI106Handle);
                iStatus = bWriteInOrderIndex(m_iI106Handle, szIdxFileName);
                }
            } while (bFALSE);
        }

    else
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
 * Read messages until error or EOF
 */

    lMsgs = 1;

    while (1==1) 
        {

        // Read the next header
        enStatus = enI106Ch10ReadNextHeader(m_iI106Handle, &suI106Hdr);

        // Setup a one time loop to make it easy to break out on error
        do
            {
            if (enStatus == I106_EOF)
                break;

            // Check for header read errors
            if (enStatus != I106_OK)
                break;

            // If 1553 message then process it
            if ((suI106Hdr.ubyDataType == I106CH10_DTYPE_1553_FMT_1) &&
                ((iChannel == -1) || (iChannel == (int)suI106Hdr.uChID)))
                {

                // Make sure our buffer is big enough, size *does* matter
                if (ulBuffSize < suI106Hdr.ulPacketLen)
                    {
                    pvBuff = realloc(pvBuff, suI106Hdr.ulPacketLen);
                    ulBuffSize = suI106Hdr.ulPacketLen;
                    }

                // Read the data buffer
                enStatus = enI106Ch10ReadData(m_iI106Handle, ulBuffSize, pvBuff);

                // Check for data read errors
                if (enStatus != I106_OK)
                    break;

                lMsgs++;
                if (bVerbose) 
                    fprintf(stderr, "%8.8ld Messages \r",lMsgs);

                    // Step through all 1553 messages
                enStatus = enI106_Decode_First1553F1(&suI106Hdr, pvBuff, &su1553Msg);
                while (enStatus == I106_OK)
                    {

                    // Check for matching parameters
                    if (((iRTAddr  == -1) || (iRTAddr  == su1553Msg.psuCmdWord1->suStruct.uRTAddr )) &&
                        ((iTR      == -1) || (iTR      == su1553Msg.psuCmdWord1->suStruct.bTR     )) &&
                        ((iSubAddr == -1) || (iSubAddr == su1553Msg.psuCmdWord1->suStruct.uSubAddr)))
                        {

                        // Check for decimation count down to 1
                        if (uDecCnt == 1) 
                            {
  
                            // Print out the time

                            // PROBABLY REALLY OUGHT TO CHECK FOR THAT GOOFY SECONDARY
                            // HEADER FORMAT TIME REPRESENTATION. DOES ANYONE USE THAT???
                            enI106_Rel2IrigTime(m_iI106Handle,
                                su1553Msg.psu1553Hdr->aubyIntPktTime, &suTime);
                            szTime = ctime((time_t *)&suTime.ulSecs);
							szTime[19] = '\0';
							iMicroSec = (int)(suTime.ulFrac / 10.0);
                            fprintf(psuOutFile,"%s.%6.6d", &szTime[11], iMicroSec);

                            // Print out the command word
                            fprintf(psuOutFile," Ch %d-%c %2.2d %c %2.2d %2.2d",
                              suI106Hdr.uChID,
                              su1553Msg.psu1553Hdr->iBusID ? 'B' : 'A',
                              su1553Msg.psuCmdWord1->suStruct.uRTAddr,
                              su1553Msg.psuCmdWord1->suStruct.bTR ? 'T' : 'R',
                              su1553Msg.psuCmdWord1->suStruct.uSubAddr,
                              su1553Msg.psuCmdWord1->suStruct.uWordCnt);

                            // Print out the error flags
                            uErrorFlags = 
                                su1553Msg.psu1553Hdr->bWordError          |
                                su1553Msg.psu1553Hdr->bSyncError    << 1  |
                                su1553Msg.psu1553Hdr->bWordCntError << 2  |
                                su1553Msg.psu1553Hdr->bRespTimeout  << 3  |
                                su1553Msg.psu1553Hdr->bFormatError  << 4  |
                                su1553Msg.psu1553Hdr->bMsgError     << 5  |
                                su1553Msg.psu1553Hdr->bRT2RT        << 7;
                            if (bDecimal)
                              fprintf(psuOutFile," %2d", uErrorFlags);
                            else
                              fprintf(psuOutFile," %2.2x", uErrorFlags);

                            // Print out the status response
                            if (bStatusResponse == bTRUE)
                              if (bDecimal)
                                fprintf(psuOutFile," %4d",*su1553Msg.puStatWord1);
                              else
                                fprintf(psuOutFile," %4.4x",*su1553Msg.puStatWord1);

                            // Print out the data
//                            iWordCnt = i1553WordCnt(ptCmdWord1->tStruct);
                            for (iWordIdx=0; iWordIdx<su1553Msg.uWordCnt; iWordIdx++) {
                              if (bDecimal)
                                fprintf(psuOutFile," %5.5u",su1553Msg.pauData[iWordIdx]);
                              else
                                fprintf(psuOutFile," %4.4x",su1553Msg.pauData[iWordIdx]);
                              }

                            fprintf(psuOutFile,"\n");
                            fflush(psuOutFile);

                            l1553Msgs++;
                            if (bVerbose) printf("%8.8ld 1553 Messages \r",l1553Msgs);

                            uDecCnt = uDecimation;

                            } /* end if decimation count down to 1 */

                        else 
                            {
                            uDecCnt--;
                            } /* else decrement decimation counter */

                        } // end if parameters match

                    // Get the next 1553 message
                    enStatus = enI106_Decode_Next1553F1(&su1553Msg);
                    } // end while processing 1553 messages from an IRIG packet

                } // end if logging RT to RT


            } while (bFALSE); // end one time loop

        // If EOF break out of main read loop
        if (enStatus == I106_EOF)
            {
            fprintf(stderr, "End of file\n");
            break;
            }

        }   /* End while */

/*
 * Print out some summaries
 */

    printf("\nTotal Message %lu\n", lMsgs);


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
//    int                     iRDsiIndex;
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
        iGIndex = psuGDataSource->iDataSourceNum;

        // R record info
        psuRRecord = psuGDataSource->psuRRecord;
        do  {
            if (psuRRecord == NULL) break;
            iRIndex = psuRRecord->iRecordNum;

            // R record data sources
            psuRDataSource = psuRRecord->psuFirstDataSource;
            do  {
                if (psuRDataSource == NULL) 
                    break;
                if (strcasecmp(psuRDataSource->szChannelDataType,"1553IN") == 0)
                    {
//                    iRDsiIndex = psuRDataSource->iDataSourceNum;
                    fprintf(psuOutFile," %5s ",   psuRDataSource->szTrackNumber);
                    fprintf(psuOutFile,"  %-20s", psuRDataSource->szDataSourceID);
                    fprintf(psuOutFile,"\n");
                    }
                psuRDataSource = psuRDataSource->psuNextRDataSource;
                } while (bTRUE);

            psuRRecord = psuRRecord->psuNextRRecord;
            } while (bTRUE);


        psuGDataSource = psuTmatsInfo->psuFirstGRecord->psuFirstGDataSource->psuNextGDataSource;
        } while (bTRUE);

    return;
    }


/* ------------------------------------------------------------------------ */

void vUsage(void)
    {
    printf("\nIDMP1553 "MAJOR_VERSION"."MINOR_VERSION" "__DATE__" "__TIME__"\n");
    printf("Dump 1553 records from a Ch 10 data file\n");
    printf("Freeware Copyright (C) 2006 Irig106.org\n\n");
    printf("Usage: idmp1553 <input file> <output file> [flags]\n");
    printf("   <filename> Input/output file names        \n");
    printf("   -v         Verbose                        \n");
    printf("   -c ChNum   Channel Number (default all)   \n");
    printf("   -r RT      RT Address(1-30) (default all) \n");
    printf("   -t T/R     T/R Bit (0=R 1=T) (default all)\n");
    printf("   -s SA      Subaddress (default all)       \n");
    printf("   -d Num     Dump 1 in 'Num' messages       \n");
    printf("   -i         Dump data as decimal integers  \n");
    printf("   -u         Dump status response           \n");
    printf("   -o         Dump in time order             \n");
    printf("                                             \n");
    printf("   -T         Print TMATS summary and exit   \n");
    printf("                                             \n");
    printf("The output data fields are:                  \n");
    printf("  Time Bus RT T/R SA WC Errs Data...         \n");
    printf("                                             \n");
    printf("Error Bits:                                  \n");
    printf("  0x01    Word Error                         \n");
    printf("  0x02    Sync Error                         \n");
    printf("  0x04    Word Count Error                   \n");
    printf("  0x08    Response Timeout                   \n");
    printf("  0x10    Format Error                       \n");
    printf("  0x20    Message Error                      \n");
    printf("  0x80    RT to RT                           \n");
    }




