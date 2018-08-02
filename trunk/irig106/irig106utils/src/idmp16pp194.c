/* 

 idmp16pp194 - A utility for dumping IRIG 106 Ch 10 16PP194 data

 Copyright (c) 2018 Irig106.org

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
#include "i106_decode_16pp194.h"
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
    int                     iArgIdx;
    FILE                  * psuOutFile;        // Output file handle
    char                  * szTime;
    int                     iWordIdx;
    int                     iMicroSec;
    int                     iChannel;         // Channel number
//    int                     iRTAddr;          // RT address
//    int                     iTR;              // Transmit bit
//    int                     iSubAddr;         // Subaddress
//    unsigned                uDecimation;      // Decimation factor
//    unsigned                uDecCnt;          // Decimation count
    unsigned long           lMsgs = 0;        // Total message
    unsigned long           l16PP194Msgs = 0;
    int                     bVerbose;
//    int                     bDecimal;         // Hex/decimal flag
//    int                     bStatusResponse;
    int                     bPrintTMATS;
//    int                     bInOrder;         // Dump out in order
    unsigned long           ulBuffSize = 0L;

    EnI106Status            enStatus;
    SuI106Ch10Header        suI106Hdr;

    unsigned char         * pvBuff  = NULL;
    SuIrig106Time           suTime;
    Su16PP194_CurrMsg       su16PP194Msg;
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
    bVerbose        = bFALSE;            // No verbosity
    bPrintTMATS     = bFALSE;

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

      } // end command line arg switch
    } // end for all arguments

    if (strlen(szInFile)==0) 
        {
        vUsage();
        return 1;
        }

/*
 * Opening banner
 * --------------
 */

    fprintf(stderr, "\nIDMP16PP194 "MAJOR_VERSION"."MINOR_VERSION"\n");
    fprintf(stderr, "Freeware Copyright (C) 2018 Irig106.org\n\n");

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

            // If 16PP194 message then process it
            if ((suI106Hdr.ubyDataType == I106CH10_DTYPE_16PP194) &&
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
                enStatus = enI106_Decode_First16PP194(&suI106Hdr, pvBuff, &su16PP194Msg);
                while (enStatus == I106_OK)
                    {

#if 0
                    // Check for matching parameters
                    if (((iRTAddr  == -1) || (iRTAddr  == su16PP194Msg.psuCmdWord1->suStruct.uRTAddr )) &&
                        ((iTR      == -1) || (iTR      == su16PP194Msg.psuCmdWord1->suStruct.bTR     )) &&
                        ((iSubAddr == -1) || (iSubAddr == su16PP194Msg.psuCmdWord1->suStruct.uSubAddr)))
                        {

#endif

                        // Print out the time
                        // PROBABLY REALLY OUGHT TO CHECK FOR THAT GOOFY SECONDARY
                        // HEADER FORMAT TIME REPRESENTATION. DOES ANYONE USE THAT???
                        enI106_Rel2IrigTime(m_iI106Handle, su16PP194Msg.psu16PP194Msg->aubyIntPktTime, &suTime);
                        szTime = ctime((time_t *)&suTime.ulSecs);
						szTime[19] = '\0';
						iMicroSec = (int)(suTime.ulFrac / 10.0);
                        fprintf(psuOutFile,"%s.%6.6d", &szTime[11], iMicroSec);

                        // Print out the Channel ID
                        fprintf(psuOutFile," Ch %-3d", suI106Hdr.uChID);

                        // Print out the data words
                        for (iWordIdx=0; iWordIdx<12; iWordIdx++)
                            fprintf(psuOutFile," %4.4x", su16PP194Msg.psu16PP194Msg->suData[iWordIdx]);

                        fprintf(psuOutFile,"\n");
                        fflush(psuOutFile);

                        l16PP194Msgs++;
                        if (bVerbose) printf("%8.8ld 16PP194 Messages \r",l16PP194Msgs);

#if 0
                        } // end if parameters match
#endif
                    // Get the next 16PP194 message
                    enStatus = enI106_Decode_Next16PP194(&su16PP194Msg);
                    } // end while processing 1553 messages from an IRIG packet

                } // end if 16PP194 data packet type


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

    fprintf(psuOutFile,"\n=-=-= 1553 / 16PP194 Channel Summary =-=-=\n\n");

    // G record
    fprintf(psuOutFile,"Program Name - %s\n",psuTmatsInfo->psuFirstGRecord->szProgramName);
    fprintf(psuOutFile,"\n");
    fprintf(psuOutFile,"Channel  Bus Type  Data Source         \n");
    fprintf(psuOutFile,"-------  --------  --------------------\n");

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
                    switch (atoi(psuRDataSource->su1553.szDataTypeFormat))
                        {
                        case 1  : fprintf(psuOutFile,"  1553    "); break;
                        case 2  : fprintf(psuOutFile,"  16PP194 "); break;
                        default : fprintf(psuOutFile,"          "); break;
                        } // end switch on bus type format
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




