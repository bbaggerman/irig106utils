/* 

 idmpgps - A utility for dumping GPS position information from IRIG 106 Ch 10 
   data files

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

*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "config.h"
#include "stdint.h"
#include "irig106ch10.h"

#include "i106_time.h"
#include "i106_decode_time.h"
#include "i106_decode_uart.h"
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

#define CR      (13)
#define LF      (10)


/*
 * Data structures
 * ---------------
 */

// Hold GPGGA data
typedef struct
    {
    int         bValid;
    float       fLatitude;      // Latitude
    float       fLongitude;     // Longitude
    float       fFixQuality;    // Fix quality
    int         iNumSats;       // Number of satellites
    float       fHDOP;          // HDOP
    float       fAltitude;      // Altitude MSL (meters)
    float       fHAE;           // Height above WGS84 ellipsoid (meters)
    } SuNmeaGPGGA;

// Hold GPRMC data
typedef struct
    {
    int         bValid;
    } SuNmeaGPRMC;

// Hold all data for a particular time
typedef struct
    {
    int             iSeconds;       // Seconds since midnight of these messages
    SuNmeaGPGGA     suNmeaGPGGA;
    SuNmeaGPRMC     suNmeaGPRCM;
    } SuNmeaInfo;

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

void vPrintTmats(SuTmatsInfo * psuTmatsInfo, FILE * ptOutFile);

void ClearNmeaInfo(SuNmeaInfo * psuNmeaInfo);
int  iDecodeNmeaTime(const char * szNmea);
int  bDecodeNmea(const char * szNmeaMsg, SuNmeaInfo * psuNmeaInfo);
void DisplayData(SuNmeaInfo * psuNmeaInfo);
char * NmeaStrTok(char * szNmea, char * szTokens);

void vUsage(void);


/* ------------------------------------------------------------------------ */

int main(int argc, char ** argv)
  {

    char                    szInFile[256];      // Input file name
    char                    szOutFile[256];     // Output file name
    int                     iArgIdx;
    FILE                  * ptOutFile;          // Output file handle

    int                     iChannel;           // Channel number
    unsigned long           lMsgs = 0L;         // Total message
//    long                    lUartMsgs = 0L;
    long                    lGpsPoints = 0L;
    int                     bVerbose;
    int                     bString;
    int                     iWordIdx;

    int                     bPrintTMATS;
    unsigned long           ulBuffSize = 0L;

    EnI106Status            enStatus;
    SuI106Ch10Header        suI106Hdr;

    unsigned char         * pvBuff  = NULL;
    SuUartF0_CurrMsg        suUartMsg;
    SuTmatsInfo             suTmatsInfo;

    char                  * pchNmeaBuff;
    int                     iNmeaBuffLen = 1000;
    int                     iNmeaBuffIdx = 0;
    SuNmeaInfo              suNmeaInfo;
    int                     bDecodeStatus;

    int                     iSeconds;
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
    bString         = bFALSE;
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
                    case 'v' :                   // Verbose switch
                        bVerbose = bTRUE;
                        break;

                    case 'c' :                   // Channel number
                        iArgIdx++;
                        sscanf(argv[iArgIdx],"%d",&iChannel);
                        break;

                    case 's' :                   // Verbose switch
                        bString = bTRUE;
                        break;

                    case 'T' :                   // Print TMATS flag
                        bPrintTMATS = bTRUE;
                        break;

                    default :
                        break;
                    } // end flag switch
                break;

            default :
                if (szInFile[0] == '\0') strcpy(szInFile, argv[iArgIdx]);
                else                     strcpy(szOutFile,argv[iArgIdx]);
                break;

            } // end command line arg switch
        } // end for all arguments

    // An input file name is required
    if (strlen(szInFile)==0) 
        {
        vUsage();
        return 1;
        }

    // Channel number is required
    if (iChannel == -1) 
        {
        vUsage();
        return 1;
        }

/*
 * Opening banner
 * --------------
 */

    fprintf(stderr, "\nIDMPGPS "MAJOR_VERSION"."MINOR_VERSION"\n");
    fprintf(stderr, "Freeware Copyright (C) 2010 Irig106.org\n\n");

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
        ptOutFile = fopen(szOutFile,"w");
        if (ptOutFile == NULL) 
            {
            fprintf(stderr, "Error opening output file\n");
            return 1;
            }
        }

    // No output file name so use stdout
    else
        {
        ptOutFile = stdout;
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

            vPrintTmats(&suTmatsInfo, ptOutFile);
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

    ClearNmeaInfo(&suNmeaInfo);

    // Get some memory for the NMEA string buffer
    pchNmeaBuff = (char *)malloc(iNmeaBuffLen);

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

            // If UART message on the right channel then process it
            if ((suI106Hdr.ubyDataType == I106CH10_DTYPE_UART_FMT_0) &&
                (iChannel              == (int)suI106Hdr.uChID))
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

                // Step through all UART messages
                enStatus = enI106_Decode_FirstUartF0(&suI106Hdr, pvBuff, &suUartMsg);
                while (enStatus == I106_OK)
                    {

//                    enI106_RelInt2IrigTime(m_iI106Handle, suUartMsg.suTimeRef.uRelTime, &suUartMsg.suTimeRef.suIrigTime);
//                    fprintf(ptOutFile,"%s ", IrigTime2String(&suUartMsg.suTimeRef.suIrigTime));

                    // Copy UART data to holding buffer
                    for (iWordIdx=0; iWordIdx<suUartMsg.psuUartHdr->uDataLength; iWordIdx++) 
                        {
                        // If end of line then check buffer for message
                        if ((suUartMsg.pauData[iWordIdx] == CR) ||
                            (suUartMsg.pauData[iWordIdx] == LF))
                            {
                            // Null terminate the string
                            pchNmeaBuff[iNmeaBuffIdx] = '\0';

                            // If the time changed then output the data
                            iSeconds = iDecodeNmeaTime(pchNmeaBuff);
                            if ((iSeconds != -1) &&
                                (iSeconds != suNmeaInfo.iSeconds))
                                {
                                DisplayData(&suNmeaInfo);
                                ClearNmeaInfo(&suNmeaInfo);
                                }

                            // Decode it if there is anything there
                            if (iNmeaBuffIdx != 0)
                                bDecodeStatus = bDecodeNmea(pchNmeaBuff, &suNmeaInfo);

                            // Get setup for a new message
                            pchNmeaBuff[0] = '\0';
                            iNmeaBuffIdx = 0;
                            } // end if CR or LF was found

                        // If beginning of new message then check buffer for message
                        else if (suUartMsg.pauData[iWordIdx] == '$')
                            {
                            // Null terminate the string
                            pchNmeaBuff[iNmeaBuffIdx] = '\0';

                            // If the time changed then output the data
                            iSeconds = iDecodeNmeaTime(pchNmeaBuff);
                            if ((iSeconds != -1) &&
                                (iSeconds != suNmeaInfo.iSeconds))
                                {
                                DisplayData(&suNmeaInfo);
                                ClearNmeaInfo(&suNmeaInfo);
                                }

                            // Decode it if there is anything there
                            if (iNmeaBuffIdx != 0)
                                bDecodeStatus = bDecodeNmea(pchNmeaBuff, &suNmeaInfo);

                            // Store the beginning of the new message
                            pchNmeaBuff[0] = suUartMsg.pauData[iWordIdx];
                            iNmeaBuffIdx = 1;

                            } // end if new message found

                        // Else just copy the character to the buffer, but only if we've
                        // already found the beginning of a message
                        else
                            {
                            if (pchNmeaBuff[0] == '$')
                                {
                                pchNmeaBuff[iNmeaBuffIdx] = suUartMsg.pauData[iWordIdx];
                                iNmeaBuffIdx++;
                                }
                            } // else just copy character to buffer

                        } // end for all char in UART message

                    // Some broken recorder implementation strip the CR/LF so need to check
                    // at the end of the UART message
                    if (iNmeaBuffIdx != 0)
                        {
                        // Null terminate the string
                        pchNmeaBuff[iNmeaBuffIdx] = '\0';

                        // If the time changed then output the data
                        iSeconds = iDecodeNmeaTime(pchNmeaBuff);
                        if ((iSeconds != -1) &&
                            (iSeconds != suNmeaInfo.iSeconds))
                            {
                            DisplayData(&suNmeaInfo);
                            ClearNmeaInfo(&suNmeaInfo);
                            }

                        // Try to decode
                        bDecodeStatus = bDecodeNmea(pchNmeaBuff, &suNmeaInfo);

                        // If something decoded then reset NMEA buffer
                        if (bDecodeStatus == bTRUE)
                            {
                            pchNmeaBuff[0] = '\0';
                            iNmeaBuffIdx = 0;
                            }
                        } // end if buffer not empty

                    lGpsPoints++;
                    if (bVerbose) printf("%8.8ld GPS Points \r",lGpsPoints);

                    // Get the next UART message
                    enStatus = enI106_Decode_NextUartF0(&suUartMsg);
                    } // end while processing UART messages from an IRIG packet

                } // end if UART type and channel of interest

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
    fclose(ptOutFile);

    return 0;
    }



/* ------------------------------------------------------------------------ */

void vPrintTmats(SuTmatsInfo * psuTmatsInfo, FILE * ptOutFile)
    {
    int                     iGIndex;
    int                     iRIndex;
    SuGDataSource         * psuGDataSource;
    SuRRecord             * psuRRecord;
    SuRDataSource         * psuRDataSource;

    // Print out the TMATS info
    // ------------------------

    fprintf(ptOutFile,"\n=-=-= UART Channel Summary =-=-=\n\n");

    // G record
    fprintf(ptOutFile,"Program Name - %s\n",psuTmatsInfo->psuFirstGRecord->szProgramName);
    fprintf(ptOutFile,"\n");
    fprintf(ptOutFile,"Channel  Data Source         \n");
    fprintf(ptOutFile,"-------  --------------------\n");

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
                if (strcasecmp(psuRDataSource->szChannelDataType,"UARTIN") == 0)
                    {
                    fprintf(ptOutFile," %5s ",   psuRDataSource->szTrackNumber);
                    fprintf(ptOutFile,"  %-20s", psuRDataSource->szDataSourceID);
                    fprintf(ptOutFile,"\n");
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
    printf("\nIDMPGPS "MAJOR_VERSION"."MINOR_VERSION" "__DATE__" "__TIME__"\n");
    printf("Dump GPS position from a Ch 10 data file\n");
    printf("Freeware Copyright (C) 2010 Irig106.org\n\n");
    printf("Usage: idmpgps <input file> <output file> [flags]\n");
    printf("   <filename> Input/output file names        \n");
    printf("   -v         Verbose                        \n");
    printf("   -c ChNum   Channel Number (default all)   \n");
    printf("   -T         Print TMATS summary and exit   \n");
    printf("                                             \n");
    printf("The output data fields are:                  \n");
    printf("                                             \n");
    }




/* ------------------------------------------------------------------------ */

// Clear out all NMEA info and get ready for a new set of messages
void ClearNmeaInfo(SuNmeaInfo * psuNmeaInfo)
    {
    memset(psuNmeaInfo, 0, sizeof(SuNmeaInfo));
    psuNmeaInfo->iSeconds = -1;

    return;
    }



/* ------------------------------------------------------------------------ */

// Take a peak at the NMEA sentence and return the number of seconds since midnight
int  iDecodeNmeaTime(const char * szNmea)
    {
    char    szLocalNmeaBuff[1000];
    char  * szNmeaType;
    char  * szNmeaTime;
    int     iTokens;
    int     iSeconds;
    int     iHour;
    int     iMin;
    int     iSec;

    // Make a local copy of the string because strtok() inserts nulls
    strcpy(szLocalNmeaBuff, szNmea);

    // Get the first field (NMEA Sentence Type)
    szNmeaType = NmeaStrTok(szLocalNmeaBuff, ",");

    // If we got no sentence type then return error
    if (szNmeaType == NULL)
        return -1;

    // Figure out what kind of sentence and then find the time
    if ((strcmp(szNmeaType, "$GPGGA") == 0) ||
        (strcmp(szNmeaType, "$GPRMC") == 0))
        {
        szNmeaTime = NmeaStrTok(NULL,",");
        if (szNmeaTime == NULL)
            return -1;
        else
            {
            iTokens = sscanf(szNmeaTime, "%2d%2d%2d", &iHour, &iMin, &iSec);
            if (iTokens != 3)
                return -1;
            iSeconds = iHour*3600 + iMin*60 + iSec;
            } // end if time string found
        } // end if GPGGA or GPRMC

    return iSeconds;
    }



/* ------------------------------------------------------------------------ */

// Figure out what kind of message and decode the fields
int  bDecodeNmea(const char * szNmeaMsg, SuNmeaInfo * psuNmeaInfo)
    {
    char    szLocalNmeaBuff[1000];
    char  * szNmeaType;
    char  * szTemp;
    //int     iTokens;
    //int     iSeconds;
    //int     iHour;
    //int     iMin;
    //int     iSec;

    // Make a local copy of the string because strtok() inserts nulls
    strcpy(szLocalNmeaBuff, szNmeaMsg);

    // Get the first field (NMEA Sentence Type)
    szNmeaType = NmeaStrTok(szLocalNmeaBuff, ",");

    // If we got no sentence type then return error
    if (szNmeaType == NULL)
        return -1;

    // Check the checksum checkchars checksum sum
    // SOMEDAY

/*
$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,1.0,0000*47
   |   |      |          |           | |  |   |       |      |   |    |
   |   |      |          |           | |  |   |       |      |   |    47 Checksum
   |   |      |          |           | |  |   |       |      |   | 
   |   |      |          |           | |  |   |       |      |   0000 Differential reference station
   |   |      |          |           | |  |   |       |      |
   |   |      |          |           | |  |   |       |      1.0, Age of differential GPS data
   |   |      |          |           | |  |   |       |
   |   |      |          |           | |  |   |       46.9,M Height of geoid (m) above WGS84 ellipsoid
   |   |      |          |           | |  |   |
   |   |      |          |           | |  |   545.4,M Altitude (m) above mean sea level
   |   |      |          |           | |  |
   |   |      |          |           | |  0.9 Horizontal dilution of position (HDOP)
   |   |      |          |           | |
   |   |      |          |           | 08 Number of satellites being tracked
   |   |      |          |           |
   |   |      |          |           1 Fix quality: 0 = invalid
   |   |      |          |                          1 = GPS fix (SPS)
   |   |      |          |                          2 = DGPS fix
   |   |      |          |                          3 = PPS fix
   |   |      |          |                          4 = Real Time Kinematic
   |   |      |          |                          5 = Float RTK
   |   |      |          |                          6 = estimated (dead reckoning) (2.3 feature)
   |   |      |          |                          7 = Manual input mode
   |   |      |          |                          8 = Simulation mode
   |   |      |          |
   |   |      |          01131.000,E Longitude 11 deg 31.000' E
   |   |      |
   |   |      4807.038,N Latitude 48 deg 07.038' N  
   |   |
   |   123519 Fix taken at 12:35:19 UTC
   |
   GGA Global Positioning System Fix Data
*/
    if (strcmp(szNmeaType, "$GPGGA") == 0)
        {
        // Time
        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }

        // Latitude
        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }

        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }

        // Longitude
        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }

        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }

        // Fix quality
        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }

        // Number of satellites
        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }

        // HDOP
        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }

        // Altitude MSL (meters)
        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }
        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }

        // Height above WGS84 ellipsoid (meters)
        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }
        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }

        // Age of differential GPS data
        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }

        // Differential reference station
        szTemp = NmeaStrTok(NULL,"*");
        if (szTemp[0] != '\0') 
            {
            }

        // Checksum
        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }

        } // end if GPGGA

/*
$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W,A*6A
   |   |      | |          |           |     |     |      |       | |
   |   |      | |          |           |     |     |      |       | *6A Checksum data
   |   |      | |          |           |     |     |      |       |
   |   |      | |          |           |     |     |      |       A Mode Indicator
   |   |      | |          |           |     |     |      |
   |   |      | |          |           |     |     |      003.1,W Magnetic Variation
   |   |      | |          |           |     |     |
   |   |      | |          |           |     |     230394 Date - 23rd of March 1994
   |   |      | |          |           |     |
   |   |      | |          |           |     084.4 Track angle in degrees
   |   |      | |          |           |     
   |   |      | |          |           022.4 Speed over the ground in knots
   |   |      | |          |
   |   |      | |          01131.000,E Longitude 11 deg 31.000' E
   |   |      | |
   |   |      | 4807.038,N Latitude 48 deg 07.038' N
   |   |      |
   |   |      A Status A=active or V=Void
   |   |
   |   123519 Fix taken at 12:35:19 UTC
   |
   RMC Recommended Minimum sentence C
*/
    else if (strcmp(szNmeaType, "$GPRMC") == 0)
        {
//char      szTemp1[100];
//char      szTemp2[100];
//char      szTemp3[100];
//char      szTemp4[100];
//char      szTemp5[100];
//char      szTemp6[100];
//char      szTemp7[100];
//char      szTemp8[100];
//char      szTemp9[100];
//char      szTemp10[100];
//char      szTemp11[100];

//sscanf(szNmeaMsg, "%[^,]s%[^,]s%[^,]s%[^,]s%[^,]s%[^,]s%[^,]s%[^,]s%[^,]s%[^,]s%[^,%s",
//sscanf(szNmeaMsg, "%[^,]s",
//sscanf(szNmeaMsg, "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s",
//sscanf(szNmeaMsg, "%[^,]s,%[^,]s,%[^,]s,%[^,]s",
//    szTemp1, szTemp2,  szTemp3,  szTemp4,  szTemp5,  szTemp6,  szTemp7,  szTemp8,  szTemp9,  szTemp10,  szTemp11);

        // Time
        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }

        // Status
        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }

        // Latitude
        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }
        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }

        // Longitude
        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }
        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }

        // Speed
        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }

        // Track
        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }

        // Date
        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }

        // Magnetic Variation
        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }
        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }

        // Mode Indicator
        szTemp = NmeaStrTok(NULL,"*");
        if (szTemp[0] != '\0') 
            {
            }

        // Checksum
        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }

        // Test
        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }
        szTemp = NmeaStrTok(NULL,",");
        if (szTemp[0] != '\0') 
            {
            }

        } // end if GPRMC

    return bTRUE;
    }




/* ------------------------------------------------------------------------ */

void DisplayData(SuNmeaInfo * psuNmeaInfo)
    {

    return;
    }


/* ------------------------------------------------------------------------ */

// Tokenize a NMEA string similar to how strtok() works.  The problem with
// strtok() is that it doesn't handle the case where there is no string
// between tokens.  NMEA just *loves* to do this.
char * NmeaStrTok(char * szNmea, char * szDelimiters)
    {
    static size_t   iFirstCharIdx = 0;
    static size_t   iLastCharIdx  = 0;
    static size_t   iNmeaLen      = 0;
    static char *   szNmeaLocal   = "\0";

    // Non-null szNmea means this is an initial call
    if (szNmea != NULL)
        {
        szNmeaLocal   = szNmea;
        iFirstCharIdx = 0;
        iLastCharIdx  = 0;
        iNmeaLen      = strlen(szNmea);
        } // end if first call with input string

    // Null means subsequent call
    else
        {
        // If not at the end of the string, move just beyond the previous token
        if (iLastCharIdx < iNmeaLen)
            iFirstCharIdx = iLastCharIdx + 1;
        else
            iFirstCharIdx = iLastCharIdx;
        }

    // Walk the string until a delimiter is found
    iLastCharIdx  = iFirstCharIdx;
    while (strchr(szDelimiters, (int)szNmeaLocal[iLastCharIdx]) == NULL)
        {
        iLastCharIdx++;
        }

    szNmeaLocal[iLastCharIdx] = '\0';

    return &szNmeaLocal[iFirstCharIdx];
    }