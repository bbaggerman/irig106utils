/*==========================================================================

  idmptmat - Read and dump a TMATS record from an IRIG 106 Ch 10 data file

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

#include "stdint.h"

#include "irig106ch10.h"
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
 * Module data
 * -----------
 */


/*
 * Function prototypes
 * -------------------
 */

void    vDumpRaw(SuI106Ch10Header * psuI106Hdr, void * pvBuff, FILE * psuOutFile);
void    vDumpTree(SuI106Ch10Header * psuI106Hdr, void * pvBuff, FILE * psuOutFile);
void    vDumpChannel(SuI106Ch10Header * psuI106Hdr, void * pvBuff, FILE * psuOutFile);
void    vDumpSig(SuI106Ch10Header * psuI106Hdr, void * pvBuff, FILE * psuOutFile);
void    vUsage(void);


/* ------------------------------------------------------------------------ */

int main(int argc, char ** argv)
    {

    char                    szInFile[1000];     // Input file name
    char                    szOutFile[1000];    // Output file name
    int                     iArgIdx;
    FILE                  * psuOutFile;        // Output file handle
    int                     bRawOutput;
    int                     bTreeOutput;
    int                     bChannelOutput;
    int                     bSigOutput;
    unsigned long           ulBuffSize = 0L;

    int                     iI106Ch10Handle;
    EnI106Status            enStatus;
    SuI106Ch10Header        suI106Hdr;

    unsigned char         * pvBuff = NULL;


/* Make sure things stay on UTC */

    putenv("TZ=GMT0");
    tzset();

/*
 * Process the command line arguements
 */

    if (argc < 2) {
    vUsage();
    return 1;
    }

    bRawOutput     = bFALSE;    // No verbosity
    bTreeOutput    = bFALSE;
    bChannelOutput = bFALSE;
    bSigOutput     = bFALSE;
    szInFile[0]    = '\0';
    strcpy(szOutFile,"");       // Default is stdout

    for (iArgIdx=1; iArgIdx<argc; iArgIdx++) 
        {

        switch (argv[iArgIdx][0]) 
            {

            // Handle command line flags
            case '-' :
                switch (argv[iArgIdx][1]) 
                    {

                    case 'r' :                   // Raw output
                        bRawOutput = bTRUE;
                        break;

                    case 't' :                   // Tree output
                        bTreeOutput = bTRUE;
                        break;

                    case 'c' :                   // Channel summary
                        bChannelOutput = bTRUE;
                        break;

                    case 's' :                   // Signature
                        bSigOutput = bTRUE;
                        break;

                    default :
                        break;
                    } // end flag switch
                break;

            // Anything else must be a file name
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

    // Make sure at least on output is turned on
    if ((bRawOutput     == bFALSE) &&
        (bTreeOutput    == bFALSE) &&
        (bChannelOutput == bFALSE) &&
        (bSigOutput     == bFALSE))
        bChannelOutput = bTRUE;

/*
 * Opening banner
 * --------------
 */

    fprintf(stderr, "\nIDMPTMAT "MAJOR_VERSION"."MINOR_VERSION"\n");
    fprintf(stderr, "Freeware Copyright (C) 2006 Irig106.org\n\n");

/*
 * Open file and get everything init'ed
 * ------------------------------------
 */

    // Open file and allocate a buffer for reading data.
    enStatus = enI106Ch10Open(&iI106Ch10Handle, szInFile, I106_READ);
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
 * Read the TMATS record
 */

    // Read the next header
    enStatus = enI106Ch10ReadNextHeader(iI106Ch10Handle, &suI106Hdr);

    if (enStatus != I106_OK)
        {
        fprintf(stderr, " Error reading header : Status = %d\n", enStatus);
        return 1;
        }

    // Make sure our buffer is big enough, size *does* matter
    if (ulBuffSize < uGetDataLen(&suI106Hdr))
        {
        pvBuff = realloc(pvBuff, uGetDataLen(&suI106Hdr));
        ulBuffSize = uGetDataLen(&suI106Hdr);
        }

    // Read the data buffer
    enStatus = enI106Ch10ReadData(iI106Ch10Handle, ulBuffSize, pvBuff);
    if (enStatus != I106_OK)
        {
        fprintf(stderr, " Error reading data : Status = %d\n", enStatus);
        return 1;
        }

    if (suI106Hdr.ubyDataType != I106CH10_DTYPE_TMATS)
        {
        fprintf(stderr, " Error reading data : first message not TMATS");
        return 1;
        }

    // Generate output
    fprintf(psuOutFile, "IDMPTMAT "MAJOR_VERSION"."MINOR_VERSION"\n");
    fprintf(psuOutFile, "TMATS from file %s\n\n", szInFile);

    if (bRawOutput == bTRUE)
        vDumpRaw(&suI106Hdr, pvBuff, psuOutFile);

    if (bTreeOutput == bTRUE)
        vDumpTree(&suI106Hdr, pvBuff, psuOutFile);

    if (bChannelOutput == bTRUE)
        vDumpChannel(&suI106Hdr, pvBuff, psuOutFile);

    if (bSigOutput == bTRUE)
        vDumpSig(&suI106Hdr, pvBuff, psuOutFile);

    // Done so clean up
    free(pvBuff);
    pvBuff = NULL;

    fclose(psuOutFile);

    return 0;
    }


/* ------------------------------------------------------------------------ */

// Output the raw, unformated TMATS record

void  vDumpRaw(SuI106Ch10Header * psuI106Hdr, void * pvBuff, FILE * psuOutFile)
    {
    unsigned long    lChrIdx;
    char           * achBuff = pvBuff;

    // Now dump TMATS after the CSDW
    for (lChrIdx = 4; lChrIdx<psuI106Hdr->ulDataLen; lChrIdx++)
        fputc(achBuff[lChrIdx], psuOutFile);

    return;
    }



/* ------------------------------------------------------------------------ */

void vDumpTree(SuI106Ch10Header * psuI106Hdr, void * pvBuff, FILE * psuOutFile)
    {
    EnI106Status            enStatus;
    int                     iGIndex;
    int                     iRIndex;
    int                     iRDsiIndex;
    SuTmatsInfo             suTmatsInfo;
    SuGDataSource         * psuGDataSource;
    SuRRecord             * psuRRecord;
    SuRDataSource         * psuRDataSource;

    // Process the TMATS info
    memset( &suTmatsInfo, 0, sizeof(suTmatsInfo) );
    enStatus = enI106_Decode_Tmats(psuI106Hdr, pvBuff, &suTmatsInfo);
    if (enStatus != I106_OK) 
        {
        fprintf(stderr, " Error processing TMATS record : Status = %d\n", enStatus);
        return;
        }

    // Print out the TMATS info
    // ------------------------

    // G record
    fprintf(psuOutFile, "(G) Program Name - %s\n",suTmatsInfo.psuFirstGRecord->szProgramName);
    fprintf(psuOutFile, "(G) IRIG 106 Rev - %s\n",suTmatsInfo.psuFirstGRecord->szIrig106Rev);

    // Data sources
    psuGDataSource = suTmatsInfo.psuFirstGRecord->psuFirstGDataSource;
    do  {
        if (psuGDataSource == NULL) break;

        // G record data source info
        iGIndex = psuGDataSource->iDataSourceNum;
        fprintf(psuOutFile, "  (G\\DSI-%i) Data Source ID   - %s\n",
            psuGDataSource->iDataSourceNum,
            suTmatsInfo.psuFirstGRecord->psuFirstGDataSource->szDataSourceID);
        fprintf(psuOutFile, "  (G\\DST-%i) Data Source Type - %s\n",
            psuGDataSource->iDataSourceNum,
            suTmatsInfo.psuFirstGRecord->psuFirstGDataSource->szDataSourceType);

        // R record info
        psuRRecord = psuGDataSource->psuRRecord;
        do  {
            if (psuRRecord == NULL) break;
            iRIndex = psuRRecord->iRecordNum;
            fprintf(psuOutFile, "    (R-%i\\ID) Data Source ID - %s\n",
                iRIndex, psuRRecord->szDataSourceID);

            // R record data sources
            psuRDataSource = psuRRecord->psuFirstDataSource;
            do  {
                if (psuRDataSource == NULL) break;
                iRDsiIndex = psuRDataSource->iDataSourceNum;
                fprintf(psuOutFile, "      (R-%i\\DSI-%i) Data Source ID - %s\n", iRIndex, iRDsiIndex, psuRDataSource->szDataSourceID);
                fprintf(psuOutFile, "      (R-%i\\DST-%i) Channel Type   - %s\n", iRIndex, iRDsiIndex, psuRDataSource->szChannelDataType);
                fprintf(psuOutFile, "      (R-%i\\TK1-%i) Track Number   - %s\n", iRIndex, iRDsiIndex, psuRDataSource->szTrackNumber);
                psuRDataSource = psuRDataSource->psuNextRDataSource;
                } while (bTRUE);

            psuRRecord = psuRRecord->psuNextRRecord;
            } while (bTRUE);


        psuGDataSource = suTmatsInfo.psuFirstGRecord->psuFirstGDataSource->psuNextGDataSource;
        } while (bTRUE);


    return;
    }



/* ------------------------------------------------------------------------ */

void vDumpChannel(SuI106Ch10Header * psuI106Hdr, void * pvBuff, FILE * psuOutFile)
    {
    EnI106Status            enStatus;
    int                     iGIndex;
    int                     iRIndex;
//    int                     iRDsiIndex;
    SuTmatsInfo             suTmatsInfo;
    SuGDataSource         * psuGDataSource;
    SuRRecord             * psuRRecord;
    SuRDataSource         * psuRDataSource;

    // Process the TMATS info
    memset( &suTmatsInfo, 0, sizeof(suTmatsInfo) );
    enStatus = enI106_Decode_Tmats(psuI106Hdr, pvBuff, &suTmatsInfo);
    if (enStatus != I106_OK) 
        {
        fprintf(stderr, " Error processing TMATS record : Status = %d\n", enStatus);
        return;
        }

    // Print out the TMATS info
    // ------------------------

    // G record
    fprintf(psuOutFile, "Program Name - %s\n",suTmatsInfo.psuFirstGRecord->szProgramName);
    fprintf(psuOutFile, "IRIG 106 Rev - %s\n",suTmatsInfo.psuFirstGRecord->szIrig106Rev);
    fprintf(psuOutFile, "Channel  Type          Enabled   Data Source         \n");
    fprintf(psuOutFile, "-------  ------------  --------  --------------------\n");

    // Data sources
    psuGDataSource = suTmatsInfo.psuFirstGRecord->psuFirstGDataSource;
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
                fprintf(psuOutFile, " %5s ",   psuRDataSource->szTrackNumber);
                fprintf(psuOutFile, "  %-12s", psuRDataSource->szChannelDataType);
                fprintf(psuOutFile, "  %-8s",  psuRDataSource->bEnabled ? "Enabled" : "Disabled");
                fprintf(psuOutFile, "  %-20s", psuRDataSource->szDataSourceID);
                fprintf(psuOutFile, "\n");
                psuRDataSource = psuRDataSource->psuNextRDataSource;
                } while (bTRUE);

            psuRRecord = psuRRecord->psuNextRRecord;
            } while (bTRUE);


        psuGDataSource = suTmatsInfo.psuFirstGRecord->psuFirstGDataSource->psuNextGDataSource;
        } while (bTRUE);

    return;
    }



/* ------------------------------------------------------------------------ */

void    vDumpSig(SuI106Ch10Header * psuI106Hdr, void * pvBuff, FILE * psuOutFile)
    {
    EnI106Status            enStatus;
    SuTmatsInfo             suTmatsInfo;
    uint16_t                iOpCode;
    uint32_t                iSignature;

    // Process the TMATS info
    memset( &suTmatsInfo, 0, sizeof(suTmatsInfo) );
    enStatus = enI106_Decode_Tmats(psuI106Hdr, pvBuff, &suTmatsInfo);
    if (enStatus != I106_OK) 
        {
        fprintf(stderr, " Error processing TMATS record : Status = %d\n", enStatus);
        return;
        }

    enStatus = enI106_Tmats_Signature(&((char *)pvBuff)[4], psuI106Hdr->ulDataLen-4, TMATS_SIGVER_DEFAULT, TMATS_SIGFLAG_NONE, &iOpCode, &iSignature);
    if (enStatus != I106_OK) 
        {
        fprintf(stderr, " Error processing TMATS signature : Status = %d\n", enStatus);
        return;
        }


    // Print out the TMATS info
    // ------------------------

    // G record
    fprintf(psuOutFile, "(G) Program Name - %s\n",suTmatsInfo.psuFirstGRecord->szProgramName);
    fprintf(psuOutFile, "%2.2X-%8.8X\n", iOpCode, iSignature);

    return;
    }



/* ------------------------------------------------------------------------ */

void vUsage(void)
    {
    printf("\nIDMPTMAT - IDMPTMAT "MAJOR_VERSION"."MINOR_VERSION" "__DATE__" "__TIME__"\n");
    printf("Read and output TMATS record from a Ch 10 data file\n");
    printf("Freeware Copyright (C) 2011 Irig106.org\n\n");
    printf("Usage: idmptmat <infile> <outfile> <flags>\n");
    printf("  -c      Output channel summary format (default)\n");
    printf("  -t      Output tree view format\n");
    printf("  -r      Output raw TMATS\n");
    printf("  -s      Output TMATS signature\n");
    return;
    }



