/* 

 idmpindex - A utility for dumping IRIG 106 Ch 10 index records

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
#include "i106_decode_index.h"
#include "i106_decode_tmats.h"
#include "i106_index.h"

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

//extern SuCh10Index          m_asuCh10Index[MAX_HANDLES];

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
    unsigned long           lMsgs = 0;        // Total message
    unsigned long           lRootMsgs = 0;
    unsigned long           lNodeMsgs = 0;
    int                     bVerbose;
	int						bPrintTime;
	int						bPrintEvents;
	int                     bDecimal;         // Hex/decimal flag
    int                     bPrintTMATS;

    EnI106Status            enStatus;
    SuI106Ch10Header        suI106Hdr;

    unsigned long           ulRootBuffSize = 0L;
    unsigned char         * pvRootBuff  = NULL;
    unsigned long           ulNodeBuffSize = 0L;
    unsigned char         * pvNodeBuff  = NULL;

	SuIndex_CurrMsg         suCurrRootIndexMsg;
	SuIndex_CurrMsg         suCurrNodeIndexMsg;

    SuTmatsInfo             suTmatsInfo;

    int64_t                 llCurrRootOffset;
    int64_t                 llNextRootOffset;

    int                     iRootIndexPackets = 0;
    int                     iNodeIndexPackets = 0;
    int                     iNodeIndexes      = 0;

    int                     bFoundIndex;
/*
 * Process the command line arguements
 */

    if (argc < 2) 
        {
        vUsage();
        return 1;
        }

	bPrintTime		= bFALSE;
	bPrintEvents	= bFALSE;
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

                    case 't' :                   /* Print time packets */
                        bPrintTime = bTRUE;
                        break;

                    case 'e' :                   /* Print event packets */
                        bPrintEvents = bTRUE;
                        break;

                    case 'v' :                   /* Verbose switch */
                        bVerbose = bTRUE;
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

    fprintf(stderr, "\nIDMPINDEX "MAJOR_VERSION"."MINOR_VERSION"\n");
    fprintf(stderr, "Freeware Copyright (C) 2013 Irig106.org\n\n");

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
            if (bVerbose) printf("Open file OK\n");
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
    else
        if (bVerbose) printf("Sync to file time OK\n");


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
        char  * pvBuff;

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
            free(pvBuff);
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
 * Use basic i106_decode_index.* routines to read and check indexes
 * ----------------------------------------------------------------
 */

    // Go to last message, it should be a root index
    enStatus = enI106Ch10LastMsg(m_iI106Handle);
    if (enStatus != I106_OK)
        {
        printf ("ERROR %d - Last packet not found\n", enStatus);
        return 1;
        }

    if (bVerbose) printf("Moved to last packet\n");

    // Read what should be a root index packet
    enStatus = enI106Ch10GetPos(m_iI106Handle, &llCurrRootOffset);
    enStatus = enI106Ch10ReadNextHeader(m_iI106Handle, &suI106Hdr);

    if (enStatus != I106_OK)
        {
        printf ("ERROR %d - Can't read last packet header\n", enStatus);
        return 1;
        }

    if (suI106Hdr.ubyDataType != I106CH10_DTYPE_RECORDING_INDEX)
        {
        printf ("ERROR - Last root index packet not found\n");
        return 1;
        }

    if (bVerbose) printf("Root index packet found in last packet\n");

    // Check index linkage
    // -------------------

    // Loop until done reading root index packets
    while (1==1) 
        {

        // Read the root index packet
        // Make sure our buffer is big enough, size *does* matter
        if (ulRootBuffSize < suI106Hdr.ulPacketLen)
            {
            pvRootBuff = realloc(pvRootBuff, suI106Hdr.ulPacketLen);
            ulRootBuffSize = suI106Hdr.ulPacketLen;
            }

        // Read the data buffer
        enStatus = enI106Ch10ReadData(m_iI106Handle, ulRootBuffSize, pvRootBuff);

        // Check for data read errors
        if (enStatus != I106_OK)
            {
            printf ("ERROR %d - Can't read root index packet %d data\n", enStatus, iRootIndexPackets);
            break;
            }
        else
            if (bVerbose) printf("Read root index packet %d\n", iRootIndexPackets);

        iRootIndexPackets++;

        // Decode the first root index message
        enStatus = enI106_Decode_FirstIndex(&suI106Hdr, pvRootBuff, &suCurrRootIndexMsg);

        // Loop on all root index messages
        while (1==1)
            {
            // Root message, go to node packet and decode
            if      (enStatus == I106_INDEX_ROOT)
                {
                enStatus = enI106Ch10SetPos(m_iI106Handle, *(suCurrRootIndexMsg.plFileOffset));
                if (enStatus != I106_OK)
                    {
                    printf ("ERROR %d - Can't set file position to next index position from root packet\n", enStatus);
                    break;
                    }

                enStatus = enI106Ch10ReadNextHeader(m_iI106Handle, &suI106Hdr);

                if (enStatus != I106_OK)
                    {
                    printf ("ERROR %d - Can't read node packet header\n", enStatus);
                    return 1;
                    }

                if (suI106Hdr.ubyDataType != I106CH10_DTYPE_RECORDING_INDEX)
                    {
                    printf ("ERROR - Node index packet not found\n");
                    return 1;
                    }

                // Read the index packet
                // Make sure our buffer is big enough, size *does* matter
                if (ulRootBuffSize < suI106Hdr.ulPacketLen)
                    {
                    pvNodeBuff = realloc(pvNodeBuff, suI106Hdr.ulPacketLen);
                    ulNodeBuffSize = suI106Hdr.ulPacketLen;
                    }

                // Read the data buffer
                enStatus = enI106Ch10ReadData(m_iI106Handle, ulNodeBuffSize, pvNodeBuff);

                // Check for data read errors
                if (enStatus != I106_OK)
                    {
                    printf ("ERROR %d - Can't read node index packet data\n", enStatus);
                    break;
                    }

//                if (bVerbose) printf("Read node index packet %d from root index packet %d\n", iNodeIndexPackets, iRootIndexPackets);

                iNodeIndexPackets++;

                // Decode the first node index message
                enStatus = enI106_Decode_FirstIndex(&suI106Hdr, pvNodeBuff, &suCurrNodeIndexMsg);

                // Loop on all node index messages
                while (1==1)
                    {
                    // Node message, go to node packet and decode
                    if     (enStatus == I106_INDEX_NODE)
                        {
                        iNodeIndexes++;

                        // Go to indexed packet and make sure it matches node data
                        enStatus = enI106Ch10SetPos(m_iI106Handle, *(suCurrNodeIndexMsg.plFileOffset));
                        if (enStatus != I106_OK)
                            {
                            printf ("ERROR %d - Can't set file position from node index\n", enStatus);
                            break;
                            }

                        enStatus = enI106Ch10ReadNextHeader(m_iI106Handle, &suI106Hdr);

                        if (enStatus != I106_OK)
                            {
                            printf ("ERROR %d - Can't read indexed packet header\n", enStatus);
                            return 1;
                            }

						// Print out time and event packets
						switch ((suCurrNodeIndexMsg.psuNodeData)->uDataType)
						{
						case I106CH10_DTYPE_IRIG_TIME :
							if ((bVerbose == bTRUE) || (bPrintTime == bTRUE))
								printf("Ch S%2u  TIME      Offset %-14llu\n", 
									(suCurrNodeIndexMsg.psuNodeData)->uChannelID, 
									*(suCurrNodeIndexMsg.plFileOffset));
							break;

						case I106CH10_DTYPE_RECORDING_EVENT :
							if ((bVerbose == bTRUE) || (bPrintEvents == bTRUE))
								printf("Ch S%2u  EVENT     Offset %-14llu\n", 
									(suCurrNodeIndexMsg.psuNodeData)->uChannelID, 
									*(suCurrNodeIndexMsg.plFileOffset));
							break;

						default :
							if (bVerbose == bTRUE)
								printf("Ch S%2u  Type 0x%2.2x Offset %-14llu\n", 
									(suCurrNodeIndexMsg.psuNodeData)->uChannelID, 
									(suCurrNodeIndexMsg.psuNodeData)->uDataType,
									*(suCurrNodeIndexMsg.plFileOffset));
							break;
						} // end switch on data type

                        if (suI106Hdr.ubyDataType != (suCurrNodeIndexMsg.psuNodeData)->uDataType)
                            {
                            printf ("ERROR - Indexed packet data type doesn't match node index\n");
                            }

                        if (suI106Hdr.uChID != (suCurrNodeIndexMsg.psuNodeData)->uChannelID)
                            {
                            printf ("ERROR - Indexed packet Channel ID doesn't match node index\n");
                            }

                        } // end if node index message

                    else if ((enStatus == I106_INDEX_ROOT) || (enStatus == I106_INDEX_ROOT_LINK))
                        {
                        }

                    // Done reading messages from the index buffer
                    else if (enStatus == I106_NO_MORE_DATA)
                        {
                        break;
                        }

                    // Any other return status is an error of some sort
                    else
                        {
                        printf ("ERROR %d - Unexpected error in node First / Next\n", enStatus);
                        break;
                        }

                    // Get the next node index message
                    enStatus = enI106_Decode_NextIndex(&suCurrNodeIndexMsg);


                    } // end while walking node index packet


                } // end if root index message

            // Last root message links to the next root packet
            else if (enStatus == I106_INDEX_ROOT_LINK)
                {
                llNextRootOffset = *(suCurrRootIndexMsg.plFileOffset);
                }

            // If it comes back as a node message then there was a problem
            else if (enStatus == I106_INDEX_NODE)
                {
                printf ("ERROR - Root packet expected, node packet found\n");
                break;
                }

            // Done reading messages from the index buffer
            else if (enStatus == I106_NO_MORE_DATA)
                {
                break;
                }

            // Any other return status is an error of some sort
            else
                {
                printf ("ERROR %d - Unexpected error in root First / Next\n", enStatus);
                break;
                }

            // Get the next root index message
            enStatus = enI106_Decode_NextIndex(&suCurrRootIndexMsg);

            } // end while walking root index packet
        
        // If the next root offset is equal to our current offset then we are done
        if (llCurrRootOffset == llNextRootOffset)
            break;

        // Go to the next root packet
        enStatus = enI106Ch10SetPos(m_iI106Handle, llNextRootOffset);
        if (enStatus != I106_OK)
            {
            printf ("ERROR %d - Can't set file position to next root packet\n", enStatus);
            break;
            }

        llCurrRootOffset = llNextRootOffset;
        enStatus = enI106Ch10ReadNextHeader(m_iI106Handle, &suI106Hdr);
        if (enStatus != I106_OK)
            {
            printf ("ERROR %d - Can't read root packet header\n", enStatus);
            break;
            }

        if (suI106Hdr.ubyDataType != I106CH10_DTYPE_RECORDING_INDEX)
            {
            printf ("ERROR - Next root index packet not found\n");
            break;
            }

        } // End while looping on root index packets

/*
 * Print out some summaries
 */

    printf("Root Index Packts = %d\n", iRootIndexPackets);
    printf("Node Index Packts = %d\n", iNodeIndexPackets);
    printf("Node Indexes      = %d\n", iNodeIndexes);


/*
 * Use i106_index.* routines to read and check indexes
 * ---------------------------------------------------
 */

    enStatus = enIndexPresent(m_iI106Handle, &bFoundIndex);
    if (enStatus != I106_OK)
        {
        printf ("ERROR %d - Can't determine if index is present\n", enStatus);
        bFoundIndex = bFALSE;
        }

    if (bFoundIndex == bFALSE)
        {
        printf ("ERROR - Index is not present\n", enStatus);
        }

    enStatus = enReadIndexes(m_iI106Handle);
    if (enStatus != I106_OK)
        {
        printf ("ERROR %d - Can't read indexes\n", enStatus);
        }

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
    int                     iRDsiIndex;
    SuGDataSource         * psuGDataSource;
    SuRRecord             * psuRRecord;
    SuRDataSource         * psuRDataSource;

    // Print out the TMATS info
    // ------------------------

    fprintf(psuOutFile,"\n=-=-= TMATS Channel Summary =-=-=\n\n");

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
//                if (strcasecmp(psuRDataSource->szChannelDataType,"429IN") == 0)
//                    {
                    iRDsiIndex = psuRDataSource->iDataSourceNum;
                    fprintf(psuOutFile," %5i ",   psuRDataSource->iTrackNumber);
                    fprintf(psuOutFile,"  %-20s", psuRDataSource->szDataSourceID);
                    fprintf(psuOutFile,"\n");
//                    }
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
    printf("\nIDMPINDEX "MAJOR_VERSION"."MINOR_VERSION" "__DATE__" "__TIME__"\n");
    printf("Dump index data from a Ch 10 data file\n");
    printf("Freeware Copyright (C) 2013 Irig106.org\n\n");
    printf("Usage: idmpindex <input file> <output file> [flags]\n");
    printf("   <filename> Input/output file names        \n");
    printf("   -v               Verbose                                  \n");
    printf("   -t               Print indexed time packets               \n");
    printf("   -e               Print indexed event packets              \n");
    printf("   -T               Print TMATS summary and exit             \n");
    }



