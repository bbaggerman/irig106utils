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

 $RCSfile: idmptmat.c,v $
 $Date: 2006-10-11 02:45:57 $
 $Revision: 1.1 $

 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "stdint.h"

#include "irig106ch10.h"
//#include "i106_time.h"
//#include "i106_decode_time.h"
//#include "i106_decode_1553f1.h"
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


// Channel ID info
/*
typedef struct 
    {
    unsigned char       uChanType;
    unsigned char       uChanIdx;
    } SuChannelDecode;
*/

/*
 * Module data
 * -----------
 */

//    SuChannelDecode         m_asuChanInfo[0x10000];

/*
 * Function prototypes
 * -------------------
 */

void     vUsage(void);


/* ------------------------------------------------------------------------ */

int main(int argc, char ** argv)
    {

    char                    szInFile[80];     // Input file name
    char                    szOutFile[80];    // Output file name
    int                     iArgIdx;
    FILE                  * ptOutFile;        // Output file handle
    int                     bRawOutput;
    unsigned long           ulBuffSize = 0L;
    unsigned long           ulReadSize;

    int                     iI106Ch10Handle;
    EnI106Status            enStatus;
    SuI106Ch10Header        suI106Hdr;

    unsigned char         * pvBuff = NULL;

    int                     iGIndex;
    int                     iRIndex;
    int                     iRDsiIndex;
    SuTmatsInfo             suTmatsInfo;
    SuGDataSource         * psuGDataSource;
    SuRRecord             * psuRRecord;
    SuRDataSource         * psuRDataSource;

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

    bRawOutput   = bFALSE;               // No verbosity
    szInFile[0]  = '\0';
    strcpy(szOutFile,"con");             // Default is stdout

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

    if ((strlen(szInFile)==0) || (strlen(szOutFile)==0)) 
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

    enStatus = enI106Ch10Open(&iI106Ch10Handle, szInFile, I106_READ);
    if (enStatus != I106_OK)
        {
        printf("Error opening data file : Status = %d\n", enStatus);
        return 1;
        }


/*
 * Open the output file
 */

    ptOutFile = fopen(szOutFile,"w");
    if (ptOutFile == NULL) 
        {
        printf("Error opening output file\n");
        return 1;
        }


//  printf("Computing histogram...\n");


    // Read the next header
    enStatus = enI106Ch10ReadNextHeader(iI106Ch10Handle, &suI106Hdr);

    if (enStatus != I106_OK)
        {
        printf(" Error reading header : Status = %d\n", enStatus);
        return 1;
        }

    // Make sure our buffer is big enough, size *does* matter
    if (ulBuffSize < suI106Hdr.ulDataLen+8)
        {
        pvBuff = realloc(pvBuff, suI106Hdr.ulDataLen+8);
        ulBuffSize = suI106Hdr.ulDataLen+8;
        }

    // Read the data buffer
    ulReadSize = ulBuffSize;
    enStatus = enI106Ch10ReadData(iI106Ch10Handle, &ulBuffSize, pvBuff);
    if (enStatus != I106_OK)
        {
        printf(" Error reading data : Status = %d\n", enStatus);
        return 1;
        }

    if (suI106Hdr.ubyDataType != I106CH10_DTYPE_TMATS)
        {
        printf(" Error reading data : first message not TMATS");
        return 1;
        }

    // Process the TMATS info
    enI106_Decode_Tmats(&suI106Hdr, pvBuff, ulBuffSize, &suTmatsInfo);
    if (enStatus != I106_OK) 
        {
        printf(" Error processing TMATS record : Status = %d\n", enStatus);
        return 1;
        }

    // Print out the TMATS info
    // ------------------------

    // G record
    printf("IDMPTMAT "MAJOR_VERSION"."MINOR_VERSION"\n");
    printf("TMATS from file %s\n\n", szInFile);
    printf("(G) Program Name - %s\n",suTmatsInfo.psuFirstGRecord->szProgramName);
    printf("(G) IRIG 106 Rev - %s\n",suTmatsInfo.psuFirstGRecord->szIrig106Rev);

    // Data sources
    psuGDataSource = suTmatsInfo.psuFirstGRecord->psuFirstGDataSource;
    do  {
        if (psuGDataSource == NULL) break;

        // G record data source info
        iGIndex = psuGDataSource->iDataSourceNum;
        printf("  (G\\DSI-%i) Data Source ID   - %s\n",
            psuGDataSource->iDataSourceNum,
            suTmatsInfo.psuFirstGRecord->psuFirstGDataSource->szDataSourceID);
        printf("  (G\\DST-%i) Data Source Type - %s\n",
            psuGDataSource->iDataSourceNum,
            suTmatsInfo.psuFirstGRecord->psuFirstGDataSource->szDataSourceType);

        // R record info
        psuRRecord = psuGDataSource->psuRRecord;
        do  {
            if (psuRRecord == NULL) break;
            iRIndex = psuRRecord->iRecordNum;
            printf("    (R-%i\\ID) Data Source ID - %s\n",
                iRIndex, psuRRecord->szDataSourceID);

            // R record data sources
            psuRDataSource = psuRRecord->psuFirstDataSource;
            do  {
                if (psuRDataSource == NULL) break;
                iRDsiIndex = psuRDataSource->iDataSourceNum;
                printf("      (R-%i\\DSI-%i) Data Source ID - %s\n", iRIndex, iRDsiIndex, psuRDataSource->szDataSourceID);
                printf("      (R-%i\\DST-%i) Channel Type   - %s\n", iRIndex, iRDsiIndex, psuRDataSource->szChannelDataType);
                printf("      (R-%i\\TK1-%i) Track Number   - %i\n", iRIndex, iRDsiIndex, psuRDataSource->iTrackNumber);
                psuRDataSource = psuRDataSource->psuNextRDataSource;
                } while (bTRUE);

            psuRRecord = psuRRecord->psuNextRRecord;
            } while (bTRUE);


        psuGDataSource = suTmatsInfo.psuFirstGRecord->psuFirstGDataSource->psuNextGDataSource;
        } while (bTRUE);

/*
    printf(" - %s",suTmatsInfo);
    printf(" - %s",suTmatsInfo);
    printf(" - %s",suTmatsInfo);
    printf(" - %s",suTmatsInfo);
*/

    // Done so clean up
    free(pvBuff);
    pvBuff = NULL;

    fclose(ptOutFile);

    return 0;
    }


/* ------------------------------------------------------------------------ */

void vUsage(void)
  {
  printf("IDMPTMAT "MAJOR_VERSION"."MINOR_VERSION" "__DATE__" "__TIME__"\n");
  printf("Usage: idmptmat <infile> <outfile> <flags>\n");
  printf("  -r      Output raw TMATS\n");
  return;
  }



