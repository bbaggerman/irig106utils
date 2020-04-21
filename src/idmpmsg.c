/* 

 idmpmsg - A utility for dumping IRIG 106 Ch 10 Message Format 0 data

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
#include "i106_stdint.h"
#include "irig106ch10.h"

#include "i106_time.h"
#include "i106_decode_time.h"
#include "i106_decode_message.h"
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
    char                    buffer[80];
    struct tm             * timeInfo;
    int                     iWordIdx;
    int                     iMicroSec;
    int                     iChannel;         // Channel number
    int                     iSubChannel;
    unsigned                uDecimation;      // Decimation factor
    unsigned                uDecCnt;          // Decimation count
    unsigned long           lPack = 0;        // Total Packets
    unsigned long           lF0Msgs = 0;      // Total F0 messages
    int                     bSubChannel;
    int                     bVerbose;
    int                     bDecimal;         // Hex/decimal flag
    int                     bPrintTMATS;
    int                     bInOrder;         // Dump out in order
    int                     bCSV;
    unsigned long           ulBuffSize = 0L;
    unsigned int            uErrorFlags;

    int                     iStatus;
    EnI106Status            enStatus;
    SuI106Ch10Header        suI106Hdr;

    unsigned char         * pvBuff  = NULL;
    SuIrig106Time           suTime;
    SuMessageF0_CurrMsg    suF0Msg;
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
    iSubChannel     = -1;

    uDecimation     = 1;                 /* Decimation factor                 */
    bVerbose        = bFALSE;            /* No verbosity                      */
    bDecimal        = bFALSE;
    bSubChannel     = bFALSE;
    bPrintTMATS     = bFALSE;
    bInOrder        = bFALSE;
    bDecimal        = bFALSE;
    bCSV            = bFALSE;

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

          case 's' :                   /* SubChannel */
            iArgIdx++;
            sscanf(argv[iArgIdx],"%d",&iSubChannel);
            if (iSubChannel>31) {
              printf("Invalid subchannel\n");
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

          case 'o' :                   /* Dump in time order */
            bInOrder = bTRUE;
            break;

          case 'T' :                   /* Print TMATS flag */
            bPrintTMATS = bTRUE;
            break;

          case 'S':                   /* Output CSV format with semicolon */
              bCSV = bTRUE;
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

    fprintf(stderr, "\nIDMPMSG "MAJOR_VERSION"."MINOR_VERSION"\n");
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
 * Read packets and messages until error or EOF
 */

    lPack = 0;

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

            
            if ((suI106Hdr.ubyDataType == I106CH10_DTYPE_MESSAGE) &&
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

                lPack++;
                if (bVerbose)
                    fprintf(stderr, "%8.8ld Messages packets\r", lPack);

                // Step through all F0 messages
                enStatus = enI106_Read_FirstMSGF0(&suI106Hdr, pvBuff, &suF0Msg);
                while (enStatus == I106_OK)
                {

                    if (((iSubChannel == -1) || (iSubChannel == suF0Msg.psuMSGF0Hdr->uSubChannel)))
                    {
                        // Check for decimation count down to 1
                        if (uDecCnt == 1)
                        {
                            enI106_Rel2IrigTime(m_iI106Handle,
                            suF0Msg.psuMSGF0Hdr->aubyIntPktTime, &suTime);
                            szTime = ctime((time_t*)&suTime.ulSecs);
                            szTime[19] = '\0';
                            iMicroSec = (int)(suTime.ulFrac / 10.0);

                            if (bCSV) {
                                //Print time_t in raw format
                                fprintf(psuOutFile, "%ld.%6.6d", suTime.ulSecs, iMicroSec);
                                //Print time in readable format : JJJ:HH:MM:SS (JJJ Day of the year)
                                timeInfo = localtime(&suTime.ulSecs);
                                strftime(buffer, 80, "%j:%H:%M:%S", timeInfo);
                                fprintf(psuOutFile, ";%s", buffer);
                            }
                            else {
                                fprintf(psuOutFile, "%s.%6.6d", &szTime[11], iMicroSec);
                            }

                            if (bCSV) {
                                fprintf(psuOutFile, ";%1.1d", suF0Msg.psuChanSpec->uType);
                                fprintf(psuOutFile, ";%d", suF0Msg.psuMSGF0Hdr->uSubChannel);
                                fprintf(psuOutFile, ";%1.1d", suF0Msg.psuMSGF0Hdr->bDataError);
                                fprintf(psuOutFile, ";%1.1d", suF0Msg.psuMSGF0Hdr->bFmtError);
                                for (iWordIdx = 0; iWordIdx < suF0Msg.psuMSGF0Hdr->uMsgLength; iWordIdx++) {
                                    if (bDecimal)
                                        fprintf(psuOutFile, ";%3.3u", suF0Msg.pauData[iWordIdx]);
                                    else
                                        fprintf(psuOutFile, ";%2.2X", suF0Msg.pauData[iWordIdx]);
                                }
                            }
                            else {
                                fprintf(psuOutFile, " %1.1d", suF0Msg.psuChanSpec->uType);
                                fprintf(psuOutFile, " %d", suF0Msg.psuMSGF0Hdr->uSubChannel);
                                fprintf(psuOutFile, " %1.1d", suF0Msg.psuMSGF0Hdr->bDataError);
                                fprintf(psuOutFile, " %1.1d", suF0Msg.psuMSGF0Hdr->bFmtError);
                                for (iWordIdx = 0; iWordIdx < suF0Msg.psuMSGF0Hdr->uMsgLength; iWordIdx++) {
                                    if (bDecimal)
                                        fprintf(psuOutFile, " %3.3u", suF0Msg.pauData[iWordIdx]);
                                    else
                                        fprintf(psuOutFile, " %2.2X", suF0Msg.pauData[iWordIdx]);
                                }
                            }

                            fprintf(psuOutFile, "\n");
                            fflush(psuOutFile);

                            lF0Msgs++;
                            if (bVerbose) printf("%8.8ld F0 Messages \r", lF0Msgs);

                            uDecCnt = uDecimation;
                        } //* end if decimation count down to 1 */
                        else
                        {
                            uDecCnt--;
                        } /* else decrement decimation counter */
                        

                    } // End if SubChannel match
                    
                    enStatus = enI106_Read_NextMSGF0(&suF0Msg);
                } // end while processing F0 messages from an IRIG packet

                } // End of message type filtering
            } while (bFALSE); // end one time loop

        // If EOF break out of main read loop
        if (enStatus == I106_EOF)
            {
            fprintf(stderr, "\nEnd of file\n");
            break;
            }

        }   /* End while */

/*
 * Print out some summaries
 */

    printf("\nTotal : %lu F0 messages in %lu good IRIG106 packets\n", lF0Msgs, lPack);


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

    fprintf(psuOutFile,"\n=-=-= Message F0 Channel Summary =-=-=\n\n");

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
                if (strcasecmp(psuRDataSource->szChannelDataType,"MSGIN") == 0)
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
    printf("\nIDMPMSG "MAJOR_VERSION"."MINOR_VERSION" "__DATE__" "__TIME__"\n");
    printf("Dump Messages records from a Ch 10 data file\n");
    printf("Freeware Copyright (C) 2006 Irig106.org\n\n");
    printf("Usage: idmpmsg <input file> <output file> [flags]\n");
    printf("   <filename> Input/output file names                              \n");
    printf("   -v         Verbose                                              \n");
    printf("   -c ChNum   Channel Number (default all)                         \n");
    printf("   -s SC      SubChannel (default all)                             \n");
    printf("   -d Num     Dump 1 in 'Num' messages                             \n");
    printf("   -i         Dump data as decimal integers                        \n");
    printf("   -o         Dump in time order                                   \n");
    printf("   -S         Dump in CSV (fixed 32 DW column num.)                \n");
    printf("                                                                   \n");
    printf("   -T         Print TMATS summary and exit                         \n");
    printf("                                                                   \n");
    printf("The output data fields are:                                        \n");
    printf("  Raw_Time Time Type SubChannel DataError FormatError Data         \n");
    printf("                                                                   \n");
    }




