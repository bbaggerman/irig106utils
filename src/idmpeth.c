/* 

 idmpeth - A utility for dumping IRIG 106 Ch 10 Ethernet data

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
#include "stdint.h"
#include "irig106ch10.h"

#include "i106_time.h"
#include "i106_decode_time.h"
#include "i106_decode_ethernet.h"
#include "i106_decode_tmats.h"


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

int           m_iVersion;    // Data file version
int           m_usMaxBuffSize;

int           m_iI106Handle;
FILE        * m_psuOutFile;        // Output file handle


/*
 * Function prototypes
 * -------------------
 */

void PrintEthernetFrame(SuEthernetF0_Header * psuEthHdr, SuEthernetF0_CurrMsg * psuEthMsg);
void vPrintTmats(SuTmatsInfo * psuTmatsInfo);
void vUsage(void);


/* ------------------------------------------------------------------------ */

int main(int argc, char ** argv)
  {

    char                    szInFile[256];     // Input file name
    char                    szOutFile[256];    // Output file name
//    char                    szIdxFileName[256];
//    char                  * pchFileNameChar;
    int                     iArgIdx;
    char                  * szTime;
//    int                     iWordIdx;
//    int                     iMilliSec;
    int                     iChannel;         // Channel number
//    int                     iRTAddr;          // RT address
//    int                     iTR;              // Transmit bit
//    int                     iSubAddr;         // Subaddress
//    unsigned                uDecimation;      // Decimation factor
//    unsigned                uDecCnt;          // Decimation count
    unsigned long           lMsgs = 0;        // Total message
    unsigned long           lEthMsgs = 0;
    int                     bVerbose;
    int                     bDecimal;         // Hex/decimal flag
//    int                     bStatusResponse;
    int                     bPrintTMATS;
//    int                     bInOrder;         // Dump out in order
    unsigned long           ulBuffSize = 0L;
//    unsigned int            uErrorFlags;

//    int                     iStatus;
    EnI106Status            enStatus;
    SuI106Ch10Header        suI106Hdr;

    unsigned char         * pvBuff  = NULL;
    SuIrig106Time           suTime;
//    Su1553F1_CurrMsg        su1553Msg;
    SuEthernetF0_CurrMsg    suEthMsg;
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

    bVerbose        = bFALSE;            /* No verbosity                      */
    bDecimal        = bFALSE;
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

                    case 'i' :                   /* Hex/decimal flag */
                        bDecimal = bTRUE;
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

    fprintf(stderr, "\nIDMPETH "MAJOR_VERSION"."MINOR_VERSION"\n");
    fprintf(stderr, "Freeware Copyright (C) 2010 Irig106.org\n\n");

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
        m_psuOutFile = fopen(szOutFile,"w");
        if (m_psuOutFile == NULL) 
            {
            fprintf(stderr, "Error opening output file\n");
            return 1;
            }
        }

    // No output file name so use stdout
    else
        {
        m_psuOutFile = stdout;
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

            vPrintTmats(&suTmatsInfo);
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

            // If IRIG time message then process it
            if (suI106Hdr.ubyDataType == I106CH10_DTYPE_IRIG_TIME)
                {
                // Make sure our buffer is big enough, size *does* matter
                if (ulBuffSize < suI106Hdr.ulPacketLen)
                    {
                    pvBuff = realloc(pvBuff, suI106Hdr.ulPacketLen);
                    ulBuffSize = suI106Hdr.ulPacketLen;
                    }

                // Read the data buffer and decode time
                enStatus = enI106Ch10ReadData(m_iI106Handle, ulBuffSize, pvBuff);
                enI106_Decode_TimeF1(&suI106Hdr, pvBuff, &suTime);
                enI106_SetRelTime(m_iI106Handle, &suTime, suI106Hdr.aubyRefTime);
                }

            // If ethernet message then process it
            if ((suI106Hdr.ubyDataType == I106CH10_DTYPE_ETHERNET_FMT_0) &&
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

                // Step through all ethernet messages
                enStatus = enI106_Decode_FirstEthernetF0(&suI106Hdr, pvBuff, &suEthMsg);
                while (enStatus == I106_OK)
                    {
#if 0
                    enI106_Rel2IrigTime(m_iI106Handle,
                        suEthMsg.psuEthernetF0Hdr->aubyIntPktTime, &suTime);
                    szTime = ctime((time_t *)&suTime.ulSecs);
					szTime[19] = '\0';
					iMilliSec = (int)(suTime.ulFrac / 10000.0);
                    fprintf(m_psuOutFile,"%s.%3.3d", &szTime[11], iMilliSec);
#else
                    // Print out the time
                    enI106_Rel2IrigTime(m_iI106Handle,
                        suEthMsg.psuEthernetF0Hdr->aubyIntPktTime, &suTime);
                    szTime = IrigTime2String(&suTime);
                    fprintf(m_psuOutFile,"%s", szTime);
#endif

                    if ((suEthMsg.psuChanSpec->uFormat       == I106_ENET_FMT_PHYSICAL   ) &&
                        (suEthMsg.psuEthernetF0Hdr->uContent == I106_ENET_CONTENT_FULLMAC))
                        PrintEthernetFrame(suEthMsg.psuEthernetF0Hdr, &suEthMsg);
                    else
                        fprintf(m_psuOutFile, "Unknown ethernet frame type\n");

                    fflush(m_psuOutFile);

                    lEthMsgs++;
                    if (bVerbose) printf("%8.8ld 1553 Messages \r",lEthMsgs);

                    // Get the next 1553 message
                    enStatus = enI106_Decode_NextEthernetF0(&suEthMsg);
                    } // end while processing ethernet messages from an IRIG packet

                } // end if ethernet type
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
    fclose(m_psuOutFile);

    return 0;
    }



/* ------------------------------------------------------------------------ */

void PrintEthernetFrame(SuEthernetF0_Header * psuEthHdr, SuEthernetF0_CurrMsg * psuEthMsg)
    {
    SuEthernetF0_Physical_FullMAC   * psuEthData = (SuEthernetF0_Physical_FullMAC *)psuEthMsg->pauData;
    int     iDataLen;
    int     iDataIdx;
    
    // Byte swap the type / length field
    psuEthData->uTypeLen = (0xff00 & (psuEthData->uTypeLen << 8)) |
                           (0x00ff & (psuEthData->uTypeLen >> 8));

    // Display ethernet frame type
    if (psuEthData->uTypeLen > 0x600)
        fprintf(m_psuOutFile, " EthernetII");
    else
        fprintf(m_psuOutFile, " 802.3     ");

    // Destination ethernet address
    fprintf(m_psuOutFile, " %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x",
        psuEthData->abyDestAddr[0], psuEthData->abyDestAddr[1], psuEthData->abyDestAddr[2],
        psuEthData->abyDestAddr[3], psuEthData->abyDestAddr[4], psuEthData->abyDestAddr[5]);

    // Source ethernet address
    fprintf(m_psuOutFile, " %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x",
        psuEthData->abySrcAddr[0], psuEthData->abySrcAddr[1], psuEthData->abySrcAddr[2],
        psuEthData->abySrcAddr[3], psuEthData->abySrcAddr[4], psuEthData->abySrcAddr[5]);

    // Ethernet type / 802.3 length
    switch (psuEthData->uTypeLen)
        {
        case 0x0800 : // IP
            fprintf(m_psuOutFile, " IP    ");
            break;
        case 0x0806 : // IP
            fprintf(m_psuOutFile, " ARP   ");
            break;
        default :
            if (psuEthData->uTypeLen >= 0x0600)
                fprintf(m_psuOutFile, " 0x%4x", psuEthData->uTypeLen);
            else
                fprintf(m_psuOutFile, " 802.3 ");
            break;
        } // end switch on type / length
        
    // Data
    iDataLen = psuEthHdr->uDataLen - 14;
    for (iDataIdx=0; iDataIdx<iDataLen; iDataIdx++)
        {
        fprintf(m_psuOutFile, " %2.2x", psuEthData->abyData[iDataIdx]);
        }

    fprintf(m_psuOutFile, "\n");

    return;
    }




/* ------------------------------------------------------------------------ */

void vPrintTmats(SuTmatsInfo * psuTmatsInfo)
    {
    int                     iGIndex;
    int                     iRIndex;
    int                     iRDsiIndex;
    SuGDataSource         * psuGDataSource;
    SuRRecord             * psuRRecord;
    SuRDataSource         * psuRDataSource;

    // Print out the TMATS info
    // ------------------------

    fprintf(m_psuOutFile,"\n=-=-= 1553 Channel Summary =-=-=\n\n");

    // G record
    fprintf(m_psuOutFile,"Program Name - %s\n",psuTmatsInfo->psuFirstGRecord->szProgramName);
    fprintf(m_psuOutFile,"\n");
    fprintf(m_psuOutFile,"Channel  Data Source         \n");
    fprintf(m_psuOutFile,"-------  --------------------\n");

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
                    iRDsiIndex = psuRDataSource->iDataSourceNum;
                    fprintf(m_psuOutFile," %5i ",   psuRDataSource->iTrackNumber);
                    fprintf(m_psuOutFile,"  %-20s", psuRDataSource->szDataSourceID);
                    fprintf(m_psuOutFile,"\n");
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
    printf("\nIDMPETH "MAJOR_VERSION"."MINOR_VERSION" "__DATE__" "__TIME__"\n");
    printf("Dump Ethernet records from a Ch 10 data file\n");
    printf("Freeware Copyright (C) 2010 Irig106.org\n\n");
    printf("Usage: idmpeth <input file> <output file> [flags]\n");
    printf("   <filename> Input/output file names        \n");
    printf("   -v         Verbose                        \n");
    printf("   -c ChNum   Channel Number (default all)   \n");
    printf("   -i         Dump data as decimal integers  \n");
    printf("                                             \n");
    printf("   -T         Print TMATS summary and exit   \n");
    printf("                                             \n");
    printf("The output data fields are:                  \n");
//    printf("  Time Bus RT T/R SA WC Errs Data...         \n");
    printf("                                             \n");
    }




