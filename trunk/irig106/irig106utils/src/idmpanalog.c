/* 

 idmpanalog - A utility for dumping AnalogF1 data

 Copyright (c) 2014 Irig106.org

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
 Brought to life by Spencer Hatch in Andøya, Norge, NOV 2014

 2014/11 "In works"
  
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <inttypes.h>

#include "config.h"
#include "stdint.h"
#include "irig106ch10.h"

#include "i106_time.h"
#include "i106_decode_time.h"
#include "i106_decode_tmats.h"
#include "i106_decode_analogf1.h"


#ifdef __cplusplus
namespace Irig106 {
extern "C" {
#endif

/*
 * Macros and definitions
 * ----------------------
 */

#define MAJOR_VERSION  "01"
#define MINOR_VERSION  "00"

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
    uint16_t                  uChID;
    BOOL                      bEnabled;         // Flag for channel enabled
    SuRDataSource           * psuRDataSrc;      // Pointer to the corresponding TMATS RRecord
    void                    * psuAttributes;    // Pointer to the corresponding Attributes (if present)
    BOOL                      bFirst;
} SuChanInfo;

/*
 * Function prototypes
 * -------------------
 */

void vPrintTmats(SuTmatsInfo * psuTmatsInfo, FILE * psuOutFile);
EnI106Status AssembleAttributesFromTMATS(FILE *psuOutFile, SuTmatsInfo * psuTmatsInfo, SuChanInfo * apsuChanInfo[], int MaxSuChanInfo);
EnI106Status PrintChanAttributes_ANALOGF1(SuChanInfo * psuChanInfo, FILE * psuOutFile);
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

    fprintf(stderr, "\nIDMPANALOG "MAJOR_VERSION"."MINOR_VERSION"\n");
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

    enStatus = enI106_Decode_Tmats(&suI106Hdr, pvBuff, &suTmatsInfo);
    if (enStatus != I106_OK) 
    {
        fprintf(stderr, " Error processing TMATS record : Status = %d\n", enStatus);
        return 1;
    }

    enStatus = AssembleAttributesFromTMATS(psuOutFile, &suTmatsInfo, apsuChanInfo, MAX_SUCHANINFO);
    if (enStatus != I106_OK) 
        {
        fprintf(stderr, " Error assembling Analog attributes from TMATS record : Status = %d\n", enStatus);
        return 1;
        }

    //Show Analog Channel Attributes, if desired
    if (bPrintTMATS == bTRUE)
    {
	int iChanIdx = 0;

	do
	{
	    enStatus = PrintChanAttributes_ANALOGF1(apsuChanInfo[iChanIdx], psuOutFile);
	    iChanIdx++;
        } while ( iChanIdx < 256 ); //
	
	vPrintTmats(&suTmatsInfo, psuOutFile);

        return(0);

    }

    

/*
 * Read messages until error or EOF
 */
    int32_t nMessages = 0;
    int32_t nAnalogMessages = 0;
    while (1==1) 
    {
        nMessages++;
        printf("nMessages is %i\n", nMessages);
      
        // Read the next header
        enStatus = enI106Ch10ReadNextHeader(m_iI106Handle, &suI106Hdr);

        // Setup a one-time loop to make it easy to break out on error
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
                // Make sure our buffer is big enough
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

            // If ANALOGF1 message then process it
            if ((suI106Hdr.ubyDataType == I106CH10_DTYPE_ANALOG) &&
                ((uChannel == -1) || (uChannel == (int)suI106Hdr.uChID)))
            {
	        nAnalogMessages++;
		printf("nAnalogMessages is %" PRIi32 "\n",nAnalogMessages);
		printf("Header datalen is %" PRIu32 "\n",suI106Hdr.ulDataLen);

                SuAnalogF1_CurrMsg suAnalogF1Msg;

                // Make sure our buffer is big enough
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
                suAnalogF1Msg.psuAttributes = (SuAnalogF1_Attributes *)apsuChanInfo[suI106Hdr.uChID]->psuAttributes;
 
                assert(suAnalogF1Msg.psuAttributes != NULL);

                // Step through all ANALOGF1 messages
		
		if( apsuChanInfo[suI106Hdr.uChID]->bFirst )
		{
		  enStatus = enI106_Setup_AnalogF1(&suI106Hdr, pvBuff, &suAnalogF1Msg);

		  enStatus = enI106_Decode_FirstAnalogF1(&suI106Hdr, pvBuff, &suAnalogF1Msg);
		  apsuChanInfo[suI106Hdr.uChID]->bFirst = bFALSE;
		}
		else
		{
		  enStatus = enI106_Decode_FirstAnalogF1(&suI106Hdr, pvBuff, &suAnalogF1Msg);
		}
                while (enStatus == I106_OK)
                {
                     // Print the channel
                     fprintf(psuOutFile, "ANAIN-%d: ", suI106Hdr.uChID);

//                   szTime = IrigTime2StringF(&suTime, -1);
                     szTime = IrigTime2String(&suTime);
                     fprintf(psuOutFile,"%s ", szTime);

                     // Get the next ANALOGF1 message
                     enStatus = enI106_Decode_NextAnalogF1(&suAnalogF1Msg);

		} // end while processing ANALOGF1 messages from an IRIG packet

	    } // end if ANALOGF1
	    
	} while (bFALSE); // end one-time loop

        // If EOF, break out of main read loop
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

    fprintf(psuOutFile,"\n=-=-= ANALOGF1 Channel Summary =-=-=\n\n");

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
                if (strcasecmp(psuRDataSource->szChannelDataType,"ANAIN") == 0)
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
                // Analog special
                if (strcasecmp(apsuChanInfo[iTrackNumber]->psuRDataSrc->szChannelDataType,"ANAIN") == 0)
                    FreeOutputBuffers_AnalogF1((SuAnalogF1_Attributes *) apsuChanInfo[iTrackNumber]->psuAttributes);
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
		apsuChanInfo[iTrackNumber]->bFirst = bTRUE;

                if (strcasecmp(psuRDataSrc->szChannelDataType,"ANAIN") == 0)
                    {
                    // Create the correspondent attributes structure
                    if((apsuChanInfo[iTrackNumber]->psuAttributes = calloc(1, sizeof(SuAnalogF1_Attributes))) == NULL)
                        {
                        _snprintf(&szText[TextLen], SizeOfText - TextLen, "%s: %s\n", szModuleText, szI106ErrorStr(I106_BUFFER_TOO_SMALL));
                        fprintf(psuOutFile, szText);
                        FreeChanInfoTable(apsuChanInfo, MaxSuChanInfo);
                        return(I106_BUFFER_TOO_SMALL);
                        }
                    // Fill the attributes, don't check the return status I106_INVALID_PARAMETER
                    enStatus = Set_Attributes_AnalogF1(psuRDataSrc, (SuAnalogF1_Attributes *)apsuChanInfo[iTrackNumber]->psuAttributes);

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

  EnI106Status I106_CALL_DECL PrintChanAttributes_ANALOGF1(SuChanInfo * psuChanInfo, FILE * psuOutFile)
{
    if (psuChanInfo == NULL)
    {
        return I106_INVALID_PARAMETER; 
    }
    if ( (psuChanInfo->psuRDataSrc == NULL)  || (psuChanInfo->psuAttributes == NULL) )
    {
        return I106_INVALID_PARAMETER; 
    }

    PrintAttributesfromTMATS_AnalogF1(psuChanInfo->psuRDataSrc, psuChanInfo->psuAttributes, psuOutFile);

    return I106_OK;
}
  
void vUsage(void)
{
    printf("\nIDMPANALOG "MAJOR_VERSION"."MINOR_VERSION" "__DATE__" "__TIME__"\n");
    printf("Dump ANALOGF1 records from a Ch 10 data file\n");
    printf("Freeware Copyright (C) 2010 Irig106.org\n\n");
    printf("Usage: idmpanalog <input file> <output file> [flags]\n");
    printf("   <filename> Input/output file names        \n");
    printf("   -v         Verbose                        \n");
    printf("   -c ChNum   Channel Number (default all)   \n");
    printf("   -T         Print TMATS summary and exit   \n");
    printf("                                             \n");
    printf("The output data fields are:                  \n");
    printf("Time  ChanID  Data Data ...                  \n");
}


#ifdef __cplusplus
} //namespace
} // extern c
#endif



