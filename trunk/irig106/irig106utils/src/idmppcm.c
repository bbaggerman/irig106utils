/* 

 idmppcm - A utility for dumping PcmF1 data

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
 'Stolen' by Hans-Gerhard Flohr and modified for PcmF1 dumping
 2014/04/07 Initial Version 1.0
 2014/04/23 Version 1.1 
 Changes:   Inversing meaning of swap data bytes / words
            Correcting llintpkttime calculation if a new packet was received

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
#include "i106_decode_tmats.h"
#include "i106_decode_pcmf1.h"


#ifdef __cplusplus
namespace Irig106 {
extern "C" {
#endif

/*
 * Macros and definitions
 * ----------------------
 */

#define MAJOR_VERSION  "01"
#define MINOR_VERSION  "01"

#if defined(__GNUC__)
#define _MAX_PATH    4096
#define _snprintf    snprintf
#endif

#if !defined(bTRUE)
#define bTRUE   (1==1)
#define bFALSE  (1==0)
#endif

#define BOOL int

/*
 * Data structures
 * ---------------
 */


/*
 * Module data
 * -----------
 */

int           m_iI106Handle;

// Per channel statistics
typedef struct              _SuChanInfo         // Channel info
{
    uint16_t                uChID;
    BOOL                    bEnabled;           // Flag for channel enabled
    SuRDataSource           * psuRDataSrc;      // Pointer to the corresponding TMATS RRecord
    void                    * psuAttributes;    // Pointer to the corresponding Attributes (if present)
} SuChanInfo;

/*
 * Function prototypes
 * -------------------
 */

void vPrintTmats(SuTmatsInfo * psuTmatsInfo, FILE * psuOutFile);
EnI106Status AssembleAttributesFromTMATS(FILE *psuOutFile, SuTmatsInfo * psuTmatsInfo, SuChanInfo * apsuChanInfo[], int MaxSuChanInfo);
int PostProcessFrame_PcmF1(SuPcmF1_CurrMsg * psuCurrMsg);
void vUsage(void);


/* ------------------------------------------------------------------------ */

int main(int argc, char ** argv)
    {

    char                    szInFile[256];     // Input file name
    char                    szOutFile[256];    // Output file name
    int                     iArgIdx;
    FILE                  * psuOutFile;        // Output file handle
    char                  * szTime;
    unsigned int            uChannel;          // Channel number
    int                     bVerbose;
    int                     bPrintTMATS;
    int                     bDontSwapRawData;
    unsigned long           ulBuffSize = 0L;

    EnI106Status            enStatus;
    SuI106Ch10Header        suI106Hdr;

    unsigned char         * pvBuff  = NULL;
    SuIrig106Time           suTime;
    SuTmatsInfo             suTmatsInfo;

    // Channel Info array
    #define MAX_SUCHANINFO  0x10000             // 64kb, ... a rather great pointer table for the channel infos 
                                                // but the channel is a 16 bit word...
    SuChanInfo            * apsuChanInfo[MAX_SUCHANINFO]; // Channel info, needed for the attributes

/*
 * Process the command line arguements
 */

    if (argc < 2) 
        {
        vUsage();
        return 1;
        }

    uChannel         = -1;
    bVerbose         = bFALSE;            /* No verbosity                      */
    bPrintTMATS      = bFALSE;
    bDontSwapRawData = bFALSE;            /* don't swap the raw input data           */

    szInFile[0]  = '\0';
    strcpy(szOutFile,"");                // Default is stdout

    memset(&suTmatsInfo, 0, sizeof(suTmatsInfo) );
    memset(apsuChanInfo, 0, sizeof(apsuChanInfo));

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
                        if(iArgIdx >= argc)
                            {
                            vUsage();
                            return 1;
                            }
                        sscanf(argv[iArgIdx],"%u",&uChannel);
                        break;

                    case 's' :                   /* Swap bytes */
                        bDontSwapRawData = 1;
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

    fprintf(stderr, "\nIDMPCM "MAJOR_VERSION"."MINOR_VERSION"\n");
    fprintf(stderr, "Freeware Copyright (C) 2014 Irig106.org\n\n");

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
        
       fprintf(psuOutFile, "Input file: %s\n", szInFile);
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

    if (suI106Hdr.ubyDataType == I106CH10_DTYPE_TMATS)
        {
        // Make a data buffer for TMATS
        pvBuff = (unsigned char *)malloc(suI106Hdr.ulPacketLen);

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

        } // end if TMATS

    // TMATS not first message
    else
        {
        printf("Error - TMATS message not found\n");
        return 1;
        }

    if (bPrintTMATS == bTRUE)
        { 
        vPrintTmats(&suTmatsInfo, psuOutFile);
        return(0);
        }

    enStatus = enI106_Decode_Tmats(&suI106Hdr, pvBuff, &suTmatsInfo);
    if (enStatus != I106_OK) 
        {
        fprintf(stderr, " Error processing TMATS record : Status = %d\n", enStatus);
        return 1;
        }

    enStatus = AssembleAttributesFromTMATS(psuOutFile, &suTmatsInfo, apsuChanInfo, MAX_SUCHANINFO);
    if (enStatus != I106_OK) 
        {
        fprintf(stderr, " Error assembling Pcm attributes from TMATS record : Status = %d\n", enStatus);
        return 1;
        }


/*
 * Read messages until error or EOF
 */

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
                    pvBuff = (unsigned char *)realloc(pvBuff, suI106Hdr.ulPacketLen);
                    ulBuffSize = suI106Hdr.ulPacketLen;
                    }

                // Read the data buffer and decode time
                enStatus = enI106Ch10ReadData(m_iI106Handle, ulBuffSize, pvBuff);
                enI106_Decode_TimeF1(&suI106Hdr, pvBuff, &suTime);
                enI106_SetRelTime(m_iI106Handle, &suTime, suI106Hdr.aubyRefTime);
                }

            // If PCMF1 message then process it
            if ((suI106Hdr.ubyDataType == I106CH10_DTYPE_PCM_FMT_1) &&
                ((uChannel == -1) || (uChannel == (int)suI106Hdr.uChID)))
                {
                SuPcmF1_CurrMsg suPcmF1Msg;

                // Make sure our buffer is big enough, size *does* matter
                if (ulBuffSize < suI106Hdr.ulPacketLen)
                    {
                    pvBuff = (unsigned char *)realloc(pvBuff, suI106Hdr.ulPacketLen);
                    ulBuffSize = suI106Hdr.ulPacketLen;
                    }

                // Read the data buffer
                enStatus = enI106Ch10ReadData(m_iI106Handle, ulBuffSize, pvBuff);

                // Check for data read errors
                if (enStatus != I106_OK)
                    break;

                assert(apsuChanInfo[suI106Hdr.uChID] != NULL);

                // Get the attributes
                suPcmF1Msg.psuAttributes = (SuPcmF1_Attributes *)apsuChanInfo[suI106Hdr.uChID]->psuAttributes;
 
                assert(suPcmF1Msg.psuAttributes != NULL);

                if (bDontSwapRawData)
                    {
                    // Add / modify attributes not covered by the TMATS
                    // Special for Example_1.c10 we need the byte swap
                    suPcmF1Msg.psuAttributes->bDontSwapRawData = 1;
                    //#ifdef TEST_EXT_DEFINITIONS
                        // Another method to set the attributes
                        enStatus = Set_Attributes_Ext_PcmF1(suPcmF1Msg.psuAttributes->psuRDataSrc, suPcmF1Msg.psuAttributes,
                        -1, -1, -1, -1,
                        -1, -1,
                        -1, - 1, -1, -1,
                        -1, -1, -1, 
                        // External additional data
                        -1, bDontSwapRawData);
                    //#endif

                    // End special for Example_1.c10
                    }

                // Step through all PCMF1 messages
                enStatus = enI106_Decode_FirstPcmF1(&suI106Hdr, pvBuff, &suPcmF1Msg);
                while (enStatus == I106_OK)
                    {
                    int       PrintDigits;
                    int       Remainder;
                    uint32_t  Count;

                    /*nParityErrors =*/ PostProcessFrame_PcmF1(&suPcmF1Msg); // Applies the word mask, checks for errors
                    PrintDigits = suPcmF1Msg.psuAttributes->ulCommonWordLen / 4; // 4 bits: a half byte
                    Remainder   = suPcmF1Msg.psuAttributes->ulCommonWordLen % 4;
                    if(Remainder)
                        PrintDigits += 2;

                     // Print the channel
                     fprintf(psuOutFile, "PCMIN-%d: ", suI106Hdr.uChID);

                     // Print out the time
                     enI106_RelInt2IrigTime(m_iI106Handle, suPcmF1Msg.llIntPktTime, &suTime);
//                   szTime = IrigTime2StringF(&suTime, -1);
                     szTime = IrigTime2String(&suTime);
                     fprintf(psuOutFile,"%s ", szTime);

                     // Print out the data
                     for(Count = 0; Count < suPcmF1Msg.psuAttributes->ulWordsInMinorFrame - 1; Count++)
                         {
                        static char cParityError;
                        cParityError = suPcmF1Msg.psuAttributes->pauOutBufErr[Count] ? '?' : ' ';
                        // Note the I64 in the format
                        fprintf(psuOutFile, "%0*I64X%c", PrintDigits, 
                             suPcmF1Msg.psuAttributes->paullOutBuf[Count], cParityError);
                         }
                     fprintf(psuOutFile,"\n");

                     // Get the next PCMF1 message
                    enStatus = enI106_Decode_NextPcmF1(&suPcmF1Msg);

                    } // end while processing PCMF1 messages from an IRIG packet

                } // end if PCMF1

            } while (bFALSE); // end one time loop

        // If EOF break out of main read loop
        if (enStatus == I106_EOF)
            {
            fprintf(stderr, "End of file\n");
            break;
            }

        } // End while reading packet headers forever

/*
 * Print out some summaries
 */

/*
 *  Close files
 */

    enI106Ch10Close(m_iI106Handle);
    fclose(psuOutFile);

    return 0;
    }



/* ------------------------------------------------------------------------ */
/* Note: Most of the code below is from Irig106.org / Bob Baggerman */
void vPrintTmats(SuTmatsInfo * psuTmatsInfo, FILE * psuOutFile)
    {
    int                     iGIndex;
    int                     iRIndex;
    int                     iRDsiIndex;
    SuGDataSource         * psuGDataSource;
    SuRRecord             * psuRRecord;
    SuRDataSource         * psuRDataSource;

    // Print out the TMATS info
    // ------------------------

    fprintf(psuOutFile,"\n=-=-= PCMF1 Channel Summary =-=-=\n\n");

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
                if (strcasecmp(psuRDataSource->szChannelDataType,"PCMIN") == 0)
                    {
                    iRDsiIndex = psuRDataSource->iDataSourceNum;
                    fprintf(psuOutFile," %5i ",   psuRDataSource->iTrackNumber);
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

void FreeChanInfoTable(SuChanInfo * apsuChanInfo[], int MaxSuChanInfo)
    {
    int iTrackNumber;

    if(apsuChanInfo == NULL)
        return;

    for(iTrackNumber = 0; iTrackNumber < MaxSuChanInfo; iTrackNumber++)
        {
        if(apsuChanInfo[iTrackNumber] != NULL)
            {
            if(apsuChanInfo[iTrackNumber]->psuAttributes != NULL)
                {
                // Pcm special
                if (strcasecmp(apsuChanInfo[iTrackNumber]->psuRDataSrc->szChannelDataType,"PCMIN") == 0)
                    FreeOutputBuffers_PcmF1((SuPcmF1_Attributes *) apsuChanInfo[iTrackNumber]->psuAttributes);
                free(apsuChanInfo[iTrackNumber]->psuAttributes);
                apsuChanInfo[iTrackNumber]->psuAttributes = NULL;
                } 

            free(apsuChanInfo[iTrackNumber]);
            apsuChanInfo[iTrackNumber] = NULL;
            } // end if channel info not null
        } // end for all track numbers
    } // End FreeChanInfoTable


/* ------------------------------------------------------------------------ */

EnI106Status AssembleAttributesFromTMATS(FILE *psuOutFile, SuTmatsInfo * psuTmatsInfo, SuChanInfo * apsuChanInfo[], int MaxSuChanInfo)
    {
    static char                   * szModuleText = "Assemble Attributes From TMATS";
    char                          szText[_MAX_PATH + _MAX_PATH];
    int                           SizeOfText = sizeof(szText);
    int                           TextLen = 0;

    SuRRecord                     * psuRRecord;
    SuRDataSource                 * psuRDataSrc;
    int                           iTrackNumber; 
    EnI106Status                  enStatus;

    memset(szText, 0, SizeOfText--); // init and set the size to one less

    if((psuTmatsInfo->psuFirstGRecord == NULL) || (psuTmatsInfo->psuFirstRRecord == NULL))
        {
        _snprintf(&szText[TextLen], SizeOfText - TextLen, "%s: %s\n", szModuleText, szI106ErrorStr(I106_INVALID_DATA));
        fprintf(psuOutFile, szText);
        return(I106_INVALID_DATA);
        }
        
    // Find channels mentioned in TMATS record
    psuRRecord = psuTmatsInfo->psuFirstRRecord;
    while (psuRRecord != NULL)
        {

        // Get the first data source for this R record
        psuRDataSrc = psuRRecord->psuFirstDataSource;
        while (psuRDataSrc != NULL)
            {
            if(psuRDataSrc->szTrackNumber == NULL)
                continue;

            iTrackNumber = psuRDataSrc->iTrackNumber;

            if(iTrackNumber >= MaxSuChanInfo)
                return(I106_BUFFER_TOO_SMALL);

            // Make sure a message count structure exists
            if (apsuChanInfo[iTrackNumber] == NULL)
                {
                if((apsuChanInfo[iTrackNumber] = (SuChanInfo *)calloc(1, sizeof(SuChanInfo))) == NULL)
                    {
                    _snprintf(&szText[TextLen], SizeOfText - TextLen, "%s: %s\n", szModuleText, szI106ErrorStr(I106_BUFFER_TOO_SMALL));
                    fprintf(psuOutFile, szText);
                    FreeChanInfoTable(apsuChanInfo, MaxSuChanInfo);
                    return(I106_BUFFER_TOO_SMALL);
                    }

                // Now save channel type and name
                apsuChanInfo[iTrackNumber]->uChID = iTrackNumber;
                apsuChanInfo[iTrackNumber]->bEnabled = psuRDataSrc->bEnabled;
                apsuChanInfo[iTrackNumber]->psuRDataSrc = psuRDataSrc;

                if (strcasecmp(psuRDataSrc->szChannelDataType,"PCMIN") == 0)
                    {
                    // Create the correspondent attributes structure
                    if((apsuChanInfo[iTrackNumber]->psuAttributes = calloc(1, sizeof(SuPcmF1_Attributes))) == NULL)
                        {
                        _snprintf(&szText[TextLen], SizeOfText - TextLen, "%s: %s\n", szModuleText, szI106ErrorStr(I106_BUFFER_TOO_SMALL));
                        fprintf(psuOutFile, szText);
                        FreeChanInfoTable(apsuChanInfo, MaxSuChanInfo);
                        return(I106_BUFFER_TOO_SMALL);
                        }
                    // Fill the attributes, don't check the return status I106_INVALID_PARAMETER
                    enStatus = Set_Attributes_PcmF1(psuRDataSrc, (SuPcmF1_Attributes *)apsuChanInfo[iTrackNumber]->psuAttributes);
                    }
                }

            // Get the next R record data source
            psuRDataSrc = psuRDataSrc->psuNextRDataSource;
            } // end while walking R data source linked list

        // Get the next R record
        psuRRecord = psuRRecord->psuNextRRecord;

        } // end while walking R record linked list

    return(I106_OK);
    }

/* ------------------------------------------------------------------------ */

/////////////////////////////////////////////////////////////////////////////
// Post Process a Minor Frame
// Checks for parity errors, moves to data to the output buffer, applies the word mask
// Returns the number of parity errors in the minor frame
int PostProcessFrame_PcmF1(SuPcmF1_CurrMsg * psuCurrMsg)
    {
    SuPcmF1_Attributes  * psuAttributes;
    uint32_t Count;

    uint64_t ullDataWord;
    int iParityErrors = 0;

    psuAttributes = psuCurrMsg->psuAttributes;
    if(psuAttributes == NULL)
        return(iParityErrors);

    for(Count = 0; Count < psuAttributes->ulWordsInMinorFrame; Count++)
        {
        ullDataWord = psuAttributes->paullOutBuf[Count];
        if(CheckParity_PcmF1(ullDataWord, psuAttributes->ulCommonWordLen, psuAttributes->ulParityType, psuAttributes->ulParityTransferOrder))
            {
            psuAttributes->pauOutBufErr[Count] = 1; // Parity error
            iParityErrors++;
            }
        else
            psuAttributes->pauOutBufErr[Count] = 0; // No parity error

        // Save the masked word
        ullDataWord &= psuAttributes->ullCommonWordMask;
        psuAttributes->paullOutBuf[Count] = ullDataWord;
        }

    return(iParityErrors);
    }


/* ------------------------------------------------------------------------ */

void vUsage(void)
    {
    printf("\nIDMPPCM "MAJOR_VERSION"."MINOR_VERSION" "__DATE__" "__TIME__"\n");
    printf("Dump PCMF1 records from a Ch 10 data file\n");
    printf("Freeware Copyright (C) 2010 Irig106.org\n\n");
    printf("Usage: idmppcm <input file> <output file> [flags]\n");
    printf("   <filename> Input/output file names        \n");
    printf("   -v         Verbose (unused)               \n");
    printf("   -c ChNum   Channel Number (default all)   \n");
    printf("   -s         Don't swap raw data            \n");
    printf("   -T         Print TMATS summary and exit   \n");
    printf("                                             \n");
    printf("The output data fields are:                  \n");
    printf("Time  ChanID  Data Data ...                  \n");
    }


#ifdef __cplusplus
} //namespace
} // extern c
#endif



