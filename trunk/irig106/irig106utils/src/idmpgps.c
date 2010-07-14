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
#include <math.h>
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

#if !defined(__GNUC__)
#define M_PI        3.14159265358979323846
#define M_PI_2      1.57079632679489661923
#define M_PI_4      0.785398163397448309616
#endif

#define DEG_TO_RAD(angle)     ((angle)*M_PI/180.0)
#define RAD_TO_DEG(angle)     ((angle)*180.0/M_PI)
#define FT_TO_METERS(ft)      ((ft)*0.3048)
#define METERS_TO_FT(meters)  ((meters)/0.3048)
#define SQUARED(value)        ((value)*(value))
#define CUBED(value)          ((value)*(value)*(value))


/*
 * Data structures
 * ---------------
 */

// Hold GPGGA data
typedef struct
    {
    int         bValid;
    double      fLatitude;      // Latitude
    double      fLongitude;     // Longitude
    int         iFixQuality;    // Fix quality
    int         iNumSats;       // Number of satellites
    float       fHDOP;          // HDOP
    float       fAltitude;      // Altitude MSL (meters)
    float       fHAE;           // Height above WGS84 ellipsoid (meters)
    } SuNmeaGPGGA;

// Hold GPRMC data
typedef struct
    {
    int         bValid;
    double      fLatitude;      // Latitude
    double      fLongitude;     // Longitude
    float       fSpeed;         // Speed over the ground, knots
    float       fTrack;         // Course over the ground, degrees true
    int         iYear;
    int         iMonth;
    int         iDay;
    float       fMagVar;        // Magnetic variance, + = W, - = E
    } SuNmeaGPRMC;

// Hold all data for a particular time
typedef struct
    {
    int             iSeconds;       // Seconds since midnight of these messages
    SuNmeaGPGGA     suNmeaGPGGA;
    SuNmeaGPRMC     suNmeaGPRMC;
    } SuNmeaInfo;


// This is a lot of stuff I lifted from dumpins while riding through the Utah desert
// in the back seat.  BTW, southern Utah is *way* cool.  I highly recommend spending
// a few days in Moab, UT to enjoy Canyonlands and Dead Horse Point.  The Grand Canyon
// is grand but I like Canyonlands better.  OK, back to the code comments. These 
// structures support calculating distance and angles to and from a target location 
// on the ground.

typedef struct {
    double        dLat;
    double        dLon;
    float         fAltitude;         // GPS altitude MSL feet
    float         fRoll;             // Aircraft roll position
    float         fPitch;            // Aircraft pitch position
    float         fHeading;          // True heading
    } SuPos;

typedef struct {
    //int           bValidPos;
    //int           bValidAlt;
    //int           bValidHeading;
    SuPos         suPos;
    float         fSpeed;            // Ground speed
    } SuAC_Data;

typedef struct {
    float         fAz;               // Azimuth, 0 = North
    float         fEl;               // Elevation angle, + UP, - Down
    float         fRange;            // Range in nautical miles
    int           bValidAz;
    int           bValidEl;
    int           bValidRange;
    } SuRelPos;

typedef struct TargPosLL_S {
    SuPos                suPos;
    SuRelPos             suAC2Target;
    SuRelPos             suTarget2AC;
    struct TargPosLL_S * psuNext;
    } SuTargPosLL;

/*
 * Module data
 * -----------
 */

int             m_iVersion;    // Data file version
int             m_usMaxBuffSize;

int             m_iI106Handle;

int             m_bDumpGGA;
int             m_bDumpRMC;

/*
 * Function prototypes
 * -------------------
 */

void vPrintTmats(SuTmatsInfo * psuTmatsInfo, FILE * psuOutFile);

void ClearNmeaInfo(SuNmeaInfo * psuNmeaInfo);
void DisplayTitles(SuTargPosLL * psuFirstTarg, FILE * psuOutFile);
void DisplayData(SuNmeaInfo * psuNmeaInfo, SuTargPosLL * psuFirstTarg, FILE * psuOutFile);

// NMEA string routines
int  iDecodeNmeaTime(const char * szNmea);
int  bDecodeNmea(const char * szNmeaMsg, SuNmeaInfo * psuNmeaInfo);
char * NmeaStrTok(char * szNmea, char * szTokens);

void CalculexGpsFix(char * achNmeaBuff, int iBuffLen);

// Target relative calculations
void CalcTargetData(SuNmeaInfo * psuNmeaInfo, SuTargPosLL * psuFirstTarg, float fDumpRadius, int * pbInRange);
void vCalcRel(SuPos *psuFrom, SuPos *psuTo, SuRelPos *psuRel);
void vRotate(double *pdXPos, double *pdYPos, double dRotateAngle);
void vXYZ_To_AzElDist(double dX,    double dY,    double dZ,
                      double *pdAz, double *pdEl, double *pdDist);

void vUsage(void);


/* ------------------------------------------------------------------------ */

int main(int argc, char ** argv)
  {

    char                    szInFile[256];      // Input file name
    char                    szOutFile[256];     // Output file name
    int                     iArgIdx;
    FILE                  * psuOutFile;         // Output file handle

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

    enum EnReadState { WaitForStart, CopyMsgChar, CopyChecksum } enReadState = WaitForStart;

    char                  * pchNmeaBuff;
    int                     iNmeaBuffLen = 1000;
    int                     iNmeaBuffIdx = 0;
    int                     iChecksumIdx = 0;
    SuNmeaInfo              suNmeaInfo;
    int                     bDecodeStatus;

    int                     iSeconds;

    // Ground target info
    int                 iTargIdx;
    float               fDumpRadius = 0;
    SuTargPosLL       * psuFirstTarg = NULL;

    SuTargPosLL      ** ppsuCurrTarg = &psuFirstTarg;
    SuTargPosLL       * psuCurrTarg;
    int                 bInRange = bTRUE;

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

    m_bDumpGGA       = bFALSE;
    m_bDumpRMC       = bFALSE;

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

                    case 'G' :                   // Dump GGA data
                        m_bDumpGGA = bTRUE;
                        break;

                    case 'C' :                   // Dump RMC data
                        m_bDumpRMC = bTRUE;
                        break;

                    case 'g' :                   // Ground target position
                        *ppsuCurrTarg = malloc(sizeof(SuTargPosLL));
                        (*ppsuCurrTarg)->psuNext = NULL;
                        iArgIdx++;
                        sscanf(argv[iArgIdx],"%lf",&((*ppsuCurrTarg)->suPos.dLat));
                        iArgIdx++;
                        sscanf(argv[iArgIdx],"%lf",&((*ppsuCurrTarg)->suPos.dLon));
                        iArgIdx++;
                        sscanf(argv[iArgIdx],"%f", &((*ppsuCurrTarg)->suPos.fAltitude));
                        (*ppsuCurrTarg)->suPos.fRoll    = 0.0f;
                        (*ppsuCurrTarg)->suPos.fPitch   = 0.0f;
                        (*ppsuCurrTarg)->suPos.fHeading = 0.0f;
                        ppsuCurrTarg = &((*ppsuCurrTarg)->psuNext);
                        break;

                    case 'm' :                   // Dump radius
                        iArgIdx++;
                        sscanf(argv[iArgIdx],"%f",&fDumpRadius);
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

    // If not message data was specified then dump it all
    if ((m_bDumpGGA == bFALSE) &&
        (m_bDumpRMC == bFALSE))
        {
        m_bDumpGGA = bTRUE;
        m_bDumpRMC = bTRUE;
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

    //enStatus = enI106_SyncTime(m_iI106Handle, bFALSE, 0);
    //if (enStatus != I106_OK)
    //    {
    //    fprintf(stderr, "Error establishing time sync : Status = %d\n", enStatus);
    //    return 1;
    //    }


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
 * Everything OK so print some info
 */

    fprintf(stderr, "Input Data file '%s'\n", szInFile);
    if (psuOutFile != stderr)
        fprintf(psuOutFile,"Input Data file '%s'\n", szInFile);

    if (psuFirstTarg != NULL) 
        {
        psuCurrTarg = psuFirstTarg;
        iTargIdx = 1;
        while (psuCurrTarg != NULL) 
            {
            fprintf(psuOutFile,"Ground Target %d - Lat %12.6f  Lon %12.6f  Elev %5.0f\n",
                iTargIdx,
                psuCurrTarg->suPos.dLat,
                psuCurrTarg->suPos.dLon, 
                psuCurrTarg->suPos.fAltitude);
            psuCurrTarg = psuCurrTarg->psuNext;
            iTargIdx++;
            } // end while relative targets to calculate for
        }

    if (fDumpRadius != 0.0) 
        {
        fprintf(psuOutFile,"Dump Radius %6.1f nm\n", fDumpRadius);
        }

    fprintf(psuOutFile,"\n\n");

    // Print column titles. Harder than it sounds.
    DisplayTitles(psuFirstTarg, psuOutFile);

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
//                    fprintf(psuOutFile,"%s ", IrigTime2String(&suUartMsg.suTimeRef.suIrigTime));

                    // Copy UART data to holding buffer
                    for (iWordIdx=0; iWordIdx<suUartMsg.psuUartHdr->uDataLength; iWordIdx++) 
                        {
                        switch (enReadState)
                            {
                            // Waiting for the first "$" character of a new message
                            case WaitForStart :
                            default :
                                // If beginning of new message then check buffer for message
                                if (suUartMsg.pauData[iWordIdx] == '$')
                                    {
                                    pchNmeaBuff[0] = suUartMsg.pauData[iWordIdx];
                                    iNmeaBuffIdx = 1;
                                    enReadState = CopyMsgChar;
                                    }
                                break;

                            // Copy character to holding buffer, waiting for terminating "*" character
                            case CopyMsgChar :
                                pchNmeaBuff[iNmeaBuffIdx] = suUartMsg.pauData[iWordIdx];
                                iNmeaBuffIdx++;
                                if (suUartMsg.pauData[iWordIdx] == '*')
                                    {
                                    enReadState  = CopyChecksum;
                                    iChecksumIdx = 0;
                                    }
                                break;

                            // Found terminating character, now copying checksum characters
                            case CopyChecksum :
                                pchNmeaBuff[iNmeaBuffIdx] = suUartMsg.pauData[iWordIdx];
                                iNmeaBuffIdx++;
                                iChecksumIdx++;

                                // If we've got the checksum then decode the message
                                if (iChecksumIdx > 1)
                                    {
                                    // Null terminate the string
                                    pchNmeaBuff[iNmeaBuffIdx] = '\0';

                                    // Calculex fix
                                    CalculexGpsFix(pchNmeaBuff, iNmeaBuffIdx);

                                    // If the time changed then output the data
                                    iSeconds = iDecodeNmeaTime(pchNmeaBuff);
                                    if ((iSeconds != -1) &&
                                        (suNmeaInfo.iSeconds != -1) &&
                                        (iSeconds != suNmeaInfo.iSeconds))
                                        {
                                        // If we've got targets then do some calculations
                                        bInRange = bTRUE;
                                        if (psuFirstTarg != NULL)
                                            CalcTargetData(&suNmeaInfo, psuFirstTarg, fDumpRadius, &bInRange);
                                        if (bInRange)
                                            DisplayData(&suNmeaInfo, psuFirstTarg, psuOutFile);
                                        ClearNmeaInfo(&suNmeaInfo);
                                        }

                                    // Decode the NMEA message
                                    bDecodeStatus = bDecodeNmea(pchNmeaBuff, &suNmeaInfo);

                                    // Get setup for a new NMEA message
                                    pchNmeaBuff[0] = '\0';
                                    iNmeaBuffIdx = 0;
                                    enReadState = WaitForStart;
                                } // end if copy checksum done
                                break;

                            } // end switch on read state
                        } // end for all char in UART message

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
    fclose(psuOutFile);

    return 0;
    }



/* ------------------------------------------------------------------------ */

void vPrintTmats(SuTmatsInfo * psuTmatsInfo, FILE * psuOutFile)
    {
    int                     iGIndex;
    int                     iRIndex;
    SuGDataSource         * psuGDataSource;
    SuRRecord             * psuRRecord;
    SuRDataSource         * psuRDataSource;

    // Print out the TMATS info
    // ------------------------

    fprintf(psuOutFile,"\n=-=-= UART Channel Summary =-=-=\n\n");

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
                if (strcasecmp(psuRDataSource->szChannelDataType,"UARTIN") == 0)
                    {
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
    if (szNmeaType[0] == '\0')
        return -1;

    // Figure out what kind of sentence and then find the time
    if ((strcmp(szNmeaType, "$GPGGA") == 0) ||
        (strcmp(szNmeaType, "$GPRMC") == 0))
        {
        szNmeaTime = NmeaStrTok(NULL,",");
        if (szNmeaTime[0] == '\0')
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
//    char  * szTemp;

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
        char    * szTime;
        char    * szLat;
        char    * szLatNS;
        char    * szLon;
        char    * szLonEW;
        char    * szQuality;
        char    * szSats;
        char    * szHDOP;
        char    * szAlt;
        char    * szAltM;
        char    * szHAE;
        char    * szHAEM;
        char    * szDiffAge;
        char    * szStation;
        char    * szChecksum;

        // Time
        szTime = NmeaStrTok(NULL,",");
        if (szTime[0] != '\0') 
            {
            int     iTokens;
            int     iHour;
            int     iMin;
            int     iSec;
            iTokens = sscanf(szTime, "%2d%2d%2d", &iHour, &iMin, &iSec);
            if (iTokens != 3)
                return -1;
            psuNmeaInfo->iSeconds = iHour*3600 + iMin*60 + iSec;
            }

        // Latitude
        szLat = NmeaStrTok(NULL,",");
        if (szLat[0] != '\0') 
            {
            int     iDegrees;
            double  fSeconds;
            int     iTokens;
            iTokens = sscanf(szLat, "%2d%lf", &iDegrees, &fSeconds);
            if (iTokens == 2)
                psuNmeaInfo->suNmeaGPGGA.fLatitude = iDegrees + fSeconds/60.0;
            }

        szLatNS = NmeaStrTok(NULL,",");
        if (szLatNS[0] != '\0') 
            {
            if (szLatNS[0] == 'S')
                psuNmeaInfo->suNmeaGPGGA.fLatitude = -psuNmeaInfo->suNmeaGPGGA.fLatitude;
            }

        // Longitude
        szLon = NmeaStrTok(NULL,",");
        if (szLon[0] != '\0') 
            {
            int     iDegrees;
            double  fSeconds;
            int     iTokens;
            iTokens = sscanf(szLon, "%3d%lf", &iDegrees, &fSeconds);
            if (iTokens == 2)
                psuNmeaInfo->suNmeaGPGGA.fLongitude = iDegrees + fSeconds/60.0;
            }

        szLonEW = NmeaStrTok(NULL,",");
        if (szLonEW[0] != '\0') 
            {
            if (szLonEW[0] == 'W')
                psuNmeaInfo->suNmeaGPGGA.fLongitude = -psuNmeaInfo->suNmeaGPGGA.fLongitude;
            }

        // Fix quality
        szQuality = NmeaStrTok(NULL,",");
        if (szQuality[0] != '\0') 
            {
            psuNmeaInfo->suNmeaGPGGA.iFixQuality = atoi(szQuality);
            }
        else
            psuNmeaInfo->suNmeaGPGGA.iFixQuality = 0;;

        // Number of satellites
        szSats = NmeaStrTok(NULL,",");
        if (szSats[0] != '\0')
            {
            psuNmeaInfo->suNmeaGPGGA.iNumSats = atoi(szSats);
            }

        // HDOP
        szHDOP = NmeaStrTok(NULL,",");
        if (szHDOP[0] != '\0') 
            {
            psuNmeaInfo->suNmeaGPGGA.fHDOP = (float)atof(szHDOP);
            }

        // Altitude MSL (meters)
        szAlt = NmeaStrTok(NULL,",");
        if (szAlt[0] != '\0') 
            {
            psuNmeaInfo->suNmeaGPGGA.fAltitude = (float)atof(szAlt);
            }

        szAltM = NmeaStrTok(NULL,",");
        if (szAltM[0] != '\0') 
            {
            }

        // Height above WGS84 ellipsoid (meters)
        szHAE = NmeaStrTok(NULL,",");
        if (szHAE[0] != '\0') 
            {
            psuNmeaInfo->suNmeaGPGGA.fHAE = (float)atof(szHAE);
            }

        szHAEM = NmeaStrTok(NULL,",");
        if (szHAEM[0] != '\0') 
            {
            }

        // Age of differential GPS data
        szDiffAge = NmeaStrTok(NULL,",");
        if (szDiffAge[0] != '\0') 
            {
            }

        // Differential reference station
        szStation = NmeaStrTok(NULL,"*");
        if (szStation[0] != '\0') 
            {
            }

        // Checksum
        szChecksum = NmeaStrTok(NULL,",");
        if (szChecksum[0] != '\0') 
            {
            }

        // Set validity flag
        if ((szLat[0] != '\0') && (szLon[0] != '\0') && (psuNmeaInfo->suNmeaGPGGA.iFixQuality != 0))
            psuNmeaInfo->suNmeaGPGGA.bValid = bTRUE;
        else
            psuNmeaInfo->suNmeaGPGGA.bValid = bFALSE;
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
        char    * szTime;
        char    * szStatus;
        char    * szLat;
        char    * szLatNS;
        char    * szLon;
        char    * szLonEW;
        char    * szSpeed;
        char    * szTrack;
        char    * szDate;
        char    * szMagVar;
        char    * szMagVarEW;
        char    * szMode;
        char    * szChecksum;

        // Time
        szTime = NmeaStrTok(NULL,",");
        if (szTime[0] != '\0') 
            {
            int     iTokens;
            int     iHour;
            int     iMin;
            int     iSec;
            iTokens = sscanf(szTime, "%2d%2d%2d", &iHour, &iMin, &iSec);
            if (iTokens != 3)
                return -1;
            psuNmeaInfo->iSeconds = iHour*3600 + iMin*60 + iSec;
            }

        // Status
        szStatus = NmeaStrTok(NULL,",");
        if (szStatus[0] != '\0') 
            {
            if (szStatus[0] == 'A')
                psuNmeaInfo->suNmeaGPRMC.bValid = bTRUE;
            else
                psuNmeaInfo->suNmeaGPRMC.bValid = bFALSE;
            }
        else
            psuNmeaInfo->suNmeaGPRMC.bValid = bFALSE;

        // Latitude
        szLat = NmeaStrTok(NULL,",");
        if (szLat[0] != '\0') 
            {
            int     iDegrees;
            float   fSeconds;
            int     iTokens;
            iTokens = sscanf(szLat, "%2d%f", &iDegrees, &fSeconds);
            if (iTokens == 2)
                psuNmeaInfo->suNmeaGPRMC.fLatitude = iDegrees + fSeconds/60;
            }

        szLatNS = NmeaStrTok(NULL,",");
        if (szLatNS[0] != '\0') 
            {
            if (szLatNS[0] == 'S')
                psuNmeaInfo->suNmeaGPRMC.fLatitude = -psuNmeaInfo->suNmeaGPRMC.fLatitude;
            }

        // Longitude
        szLon = NmeaStrTok(NULL,",");
        if (szLon[0] != '\0') 
            {
            int     iDegrees;
            float   fSeconds;
            int     iTokens;
            iTokens = sscanf(szLon, "%3d%f", &iDegrees, &fSeconds);
            if (iTokens == 2)
                psuNmeaInfo->suNmeaGPRMC.fLongitude = iDegrees + fSeconds/60;
            }

        szLonEW = NmeaStrTok(NULL,",");
        if (szLonEW[0] != '\0') 
            {
            if (szLonEW[0] == 'W')
                psuNmeaInfo->suNmeaGPRMC.fLongitude = -psuNmeaInfo->suNmeaGPRMC.fLongitude;
            }

        // Speed
        szSpeed = NmeaStrTok(NULL,",");
        if (szSpeed[0] != '\0') 
            {
            psuNmeaInfo->suNmeaGPRMC.fSpeed = (float)atof(szSpeed);
            }

        // Track
        szTrack = NmeaStrTok(NULL,",");
        if (szTrack[0] != '\0') 
            {
            psuNmeaInfo->suNmeaGPRMC.fTrack = (float)atof(szTrack);
            }

        // Date
        szDate = NmeaStrTok(NULL,",");
        if (szDate[0] != '\0') 
            {
            int     iTokens;
            iTokens = sscanf(szDate, "%2d%2d%2d", 
                &psuNmeaInfo->suNmeaGPRMC.iDay, 
                &psuNmeaInfo->suNmeaGPRMC.iMonth, 
                &psuNmeaInfo->suNmeaGPRMC.iYear);
            psuNmeaInfo->suNmeaGPRMC.iYear += 2000;
            }

        // Magnetic Variation
        szMagVar = NmeaStrTok(NULL,",");
        if (szMagVar[0] != '\0') 
            {
            psuNmeaInfo->suNmeaGPRMC.fMagVar = (float)atof(szMagVar);
            }

        szMagVarEW = NmeaStrTok(NULL,",");
        if (szMagVarEW[0] != '\0') 
            {
            if (szMagVarEW[0] == 'E')
                psuNmeaInfo->suNmeaGPRMC.fMagVar = -psuNmeaInfo->suNmeaGPRMC.fMagVar;
            }

        // Mode Indicator
        szMode = NmeaStrTok(NULL,"*");
        if (szMode[0] != '\0') 
            {
            }

        // Checksum
        szChecksum = NmeaStrTok(NULL,",");
        if (szChecksum[0] != '\0') 
            {
            }

        } // end if GPRMC

    return bTRUE;
    }



/* ------------------------------------------------------------------------ */

// Print out data column headers
void DisplayTitles(SuTargPosLL * psuFirstTarg, FILE * psuOutFile)
    {
    int             iIdx;
    char            szTargetTitle[100];
    int             iDumpRelIdx;
    SuTargPosLL   * psuCurrTarg;

    /* This queer little piece of code is strictly for the convenience of the
     * programmer.  You see, there are multiple lines of header information to
     * write out to the output file but depending upon the command line flags
     * not all columns of data may be needed.  This weird structure allows me
     * to keep data column headers together to make it easier to keep things
     * lined up and looking pretty.  I just wanted to let you know that, no,
     * I haven't lost my ever lovin' mind.
     */

    for (iIdx=1; iIdx<=4; iIdx++) 
        {

        if (m_bDumpRMC)
            {
            if (iIdx==1)    fprintf(psuOutFile,"           ");
            if (iIdx==2)    fprintf(psuOutFile,"           ");
            if (iIdx==3)    fprintf(psuOutFile,"   Date    ");
            if (iIdx==4)    fprintf(psuOutFile,"           ");
            }

        if (m_bDumpRMC || m_bDumpGGA)
            {
            if (iIdx==1)    fprintf(psuOutFile,"         ");
            if (iIdx==2)    fprintf(psuOutFile,"         ");
            if (iIdx==3)    fprintf(psuOutFile,"  Time   ");
            if (iIdx==4)    fprintf(psuOutFile,"  (UTC)  ");
            }

        if (m_bDumpRMC || m_bDumpGGA)
            {
            if (iIdx==1)    fprintf(psuOutFile,"     GPS        GPS    ");
            if (iIdx==2)    fprintf(psuOutFile,"  Longitude  Latitude  ");
            if (iIdx==3)    fprintf(psuOutFile,"  (+ East)   (+ North) ");
            if (iIdx==4)    fprintf(psuOutFile,"  (- West)   (- South) ");
            }

        if (m_bDumpGGA)
        {
            if (iIdx==1)    fprintf(psuOutFile,"                ");
            if (iIdx==2)    fprintf(psuOutFile,"    Altitude    ");
            if (iIdx==3)    fprintf(psuOutFile,"   MSL     HAE  ");
            if (iIdx==4)    fprintf(psuOutFile," (feet)  (feet) ");
        }

        if (m_bDumpRMC)
            {
            if (iIdx==1)    fprintf(psuOutFile,"                  ");
            if (iIdx==2)    fprintf(psuOutFile,"   True    Ground ");
            if (iIdx==3)    fprintf(psuOutFile,"  Course   Speed  ");
            if (iIdx==4)    fprintf(psuOutFile,"           (kts)  ");
            }

        szTargetTitle[0] = '\0';
        psuCurrTarg  = psuFirstTarg;
        iDumpRelIdx = 1;
        while (psuCurrTarg != NULL) 
            {
            if (m_bDumpGGA || m_bDumpRMC)
                {
                if (iIdx==1)    strcat(szTargetTitle, " --------------");
                if (iIdx==2)    fprintf(psuOutFile,   " Range   Az to ");
                if (iIdx==3)    fprintf(psuOutFile,   " to A/C   A/C  ");
                if (iIdx==4)    fprintf(psuOutFile,   "  (nm)   (true)");
                }

            if (m_bDumpGGA)
                {
                if (iIdx==1)    strcat(szTargetTitle, "---------");
                if (iIdx==2)    fprintf(psuOutFile,   " Elev to ");
                if (iIdx==3)    fprintf(psuOutFile,   "   A/C   ");
                if (iIdx==4)    fprintf(psuOutFile,   "         ");
                }

            if (m_bDumpRMC)
                {
                if (iIdx==1)    strcat(szTargetTitle, "--------");
                if (iIdx==2)    fprintf(psuOutFile,   " Bearing");
                if (iIdx==3)    fprintf(psuOutFile,   " to Targ");
                if (iIdx==4)    fprintf(psuOutFile,   "  (true)");
                }

            if (iIdx==1)
                {
                int     iTitleStringOffset;
                char    szTitle[100];
                sprintf(szTitle, " Gnd Target %d ", iDumpRelIdx);
                iTitleStringOffset = (int)(strlen(szTargetTitle) - strlen(szTitle)) / 2;
                memcpy(&szTargetTitle[iTitleStringOffset], szTitle, strlen(szTitle));
                fprintf(psuOutFile, "%s", szTargetTitle);
                }

            // Get the next target
            psuCurrTarg = psuCurrTarg->psuNext;
            iDumpRelIdx++;
            }

        fprintf(psuOutFile,"\n");

        } // end for each header line

    return;
    }



/* ------------------------------------------------------------------------ */

void DisplayData(SuNmeaInfo * psuNmeaInfo, SuTargPosLL * psuFirstTarg, FILE * psuOutFile)
    {
    SuTargPosLL   * psuCurrTarg;

    // Date, only valid if GPRMC valid
    if (m_bDumpRMC)
        {
        if (psuNmeaInfo->suNmeaGPRMC.bValid == bTRUE)
            fprintf(psuOutFile,"%2.2d/%2.2d/%4.4d ", 
                psuNmeaInfo->suNmeaGPRMC.iMonth, psuNmeaInfo->suNmeaGPRMC.iDay, psuNmeaInfo->suNmeaGPRMC.iYear);
        else
            fprintf(psuOutFile,"--/--/---- ");
        }

    // Time, always valid if message is valid
    if (m_bDumpRMC || m_bDumpGGA)
        {
        int iHour, iMin, iSec;
        iHour = (int) (psuNmeaInfo->iSeconds/3600.0);
        iMin  = (int)((psuNmeaInfo->iSeconds - iHour*3600)/60.0);
        iSec  =        psuNmeaInfo->iSeconds - iHour*3600 - iMin*60;
        fprintf(psuOutFile,"%2.2d:%2.2d:%2.2d ", iHour, iMin, iSec);
        }

    // Position
    if (m_bDumpRMC || m_bDumpGGA)
        {
        if      (psuNmeaInfo->suNmeaGPGGA.bValid == bTRUE)
            fprintf(psuOutFile," %10.5f  %9.5f ", psuNmeaInfo->suNmeaGPGGA.fLongitude, psuNmeaInfo->suNmeaGPGGA.fLatitude);
        else if (psuNmeaInfo->suNmeaGPRMC.bValid == bTRUE)
            fprintf(psuOutFile," %10.5f  %9.5f ", psuNmeaInfo->suNmeaGPRMC.fLongitude, psuNmeaInfo->suNmeaGPRMC.fLatitude);
        else
            fprintf(psuOutFile," ----.-----  ---.----- ");
        }

    // Altitude
    if (m_bDumpGGA)
        {
        if (psuNmeaInfo->suNmeaGPGGA.bValid == bTRUE)
            fprintf(psuOutFile," %6.0f  %6.0f ", 
                METERS_TO_FT(psuNmeaInfo->suNmeaGPGGA.fAltitude), 
                METERS_TO_FT(psuNmeaInfo->suNmeaGPGGA.fHAE));
        else
            fprintf(psuOutFile," ------  ------ ");
        }

    // Course over the ground and speed
    if (m_bDumpRMC)
        {
        if (psuNmeaInfo->suNmeaGPRMC.bValid == bTRUE)
            fprintf(psuOutFile,"  %5.1f    %6.1f ", 
                psuNmeaInfo->suNmeaGPRMC.fTrack, psuNmeaInfo->suNmeaGPRMC.fSpeed);
        else
            fprintf(psuOutFile,"  -----    ------ ");
        }

    psuCurrTarg  = psuFirstTarg;
    while (psuCurrTarg != NULL) 
        {
        char    szRange[10];
        char    szAz2AC[10];
        char    szEl2AC[10];
        char    szBearing2Targ[10];

        // First make invalid data versions of target relative data
        strcpy(szRange,       "----.-");
        strcpy(szAz2AC,        "---.-");
        strcpy(szEl2AC,        "---.-");
        strcpy(szBearing2Targ, "---.-");

        // Now make string versions of the target relative data for valid data
        if (psuCurrTarg->suTarget2AC.bValidRange)
            sprintf(szRange, "%6.1f", psuCurrTarg->suTarget2AC.fRange);
        if (psuCurrTarg->suTarget2AC.bValidAz)
            sprintf(szAz2AC, "%5.1f", psuCurrTarg->suTarget2AC.fAz);
        if (psuCurrTarg->suTarget2AC.bValidEl)
            sprintf(szEl2AC, "%5.1f", psuCurrTarg->suTarget2AC.fEl);
        if (psuCurrTarg->suAC2Target.bValidAz)
            sprintf(szBearing2Targ, "%5.1f", psuCurrTarg->suAC2Target.fAz);

        if (m_bDumpGGA || m_bDumpRMC)
            fprintf(psuOutFile," %6s  %5s ", szRange, szAz2AC);

        if (m_bDumpGGA)
            fprintf(psuOutFile,"  %5s ", szEl2AC);

        if (m_bDumpRMC)
            fprintf(psuOutFile,"   %5s ", szBearing2Targ);

        // Get the next target
        psuCurrTarg = psuCurrTarg->psuNext;
        }

    fprintf(psuOutFile,"\n");

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
    while ((strchr(szDelimiters, (int)szNmeaLocal[iLastCharIdx]) == NULL) &&
           (szNmeaLocal[iLastCharIdx]                            != '\0'))
        {
        iLastCharIdx++;
        }

    szNmeaLocal[iLastCharIdx] = '\0';

    return &szNmeaLocal[iFirstCharIdx];
    }



/* ------------------------------------------------------------------------ */

// The Calculex MONSSTR record incorrectly records the GPRMC GPS message. It
// looks like they tokenize the string (probably to extract date and time), 
// and in the proces replace the field seperator character "," with NULL 
// characters.  The fix is to go back and replace any NULLs with commas again.
// Word is that this is fixed in a future software release.
void CalculexGpsFix(char * achNmeaBuff, int iBuffLen)
    {
    int iStrIdx;
    for (iStrIdx=0; iStrIdx<iBuffLen; iStrIdx++)
        {
        if (achNmeaBuff[iStrIdx] == '\0')
            achNmeaBuff[iStrIdx] = ',';
        }
    return;
    }


/* ------------------------------------------------------------------------ */

/* 
 Calculate relative bearing and depression angle to a target point in
 space (the "psuTo" point) based on aircraft attitude and location (the
 psuFrom point).

 Relative bearing is the angle between the A/C longitudinal axis (the
 nose of the A/C) and the project of the vector to the target point
 onto the lateral plane of the A/C (that is, the plane defined by the
 A/C longitudinal and lateral axes).

 Relative depression angle is that angle formed between the vector to the
 target point and this vector's projection onto the A/C lateral plane.

 The relative coordinate system has the psuFrom point at the origin.  The X
 axis is east, the Y axis north, and the Z axis is up.

 */

// Calculate the relative position from the target.  Since this code was lifted
// from the idumpins program, this routine sort of acts as a shim between what is
// limited GPS data (position + ground track masquerading as heading, maybe) and
// full 6DOF provided by a true INS.
void CalcTargetData(SuNmeaInfo * psuNmeaInfo, SuTargPosLL * psuFirstTarg, float fDumpRadius, int * pbInRange)
    {
    int             bValidPos;
    int             bValidAlt;
    int             bValidHeading;

    SuAC_Data       suGPS;
    SuTargPosLL   * psuCurrTarg;

    // If dump radius not used then we are always in range
    if (fDumpRadius == 0.0)
        *pbInRange = bTRUE;
    else
        *pbInRange = bFALSE;

    // Put GPS position into the AC position structure
    if      (psuNmeaInfo->suNmeaGPGGA.bValid == bTRUE)
        {
        suGPS.suPos.dLon = psuNmeaInfo->suNmeaGPGGA.fLongitude;
        suGPS.suPos.dLat = psuNmeaInfo->suNmeaGPGGA.fLatitude;
        bValidPos = bTRUE;
        }
    else if (psuNmeaInfo->suNmeaGPRMC.bValid == bTRUE)
        {
        suGPS.suPos.dLon = psuNmeaInfo->suNmeaGPRMC.fLongitude;
        suGPS.suPos.dLat = psuNmeaInfo->suNmeaGPRMC.fLatitude;
        bValidPos = bTRUE;
        }
    else
        {
        bValidPos = bFALSE;
        }

    // Altitude
    if (psuNmeaInfo->suNmeaGPGGA.bValid == bTRUE)
        {
        suGPS.suPos.fAltitude = (float)METERS_TO_FT(psuNmeaInfo->suNmeaGPGGA.fAltitude);
        bValidAlt = bTRUE;
        }
    else
        {
        suGPS.suPos.fAltitude = 0.0;
        bValidAlt = bFALSE;
        }

    // Track over the ground is approximately heading
    if (psuNmeaInfo->suNmeaGPRMC.bValid == bTRUE)
        {
        suGPS.suPos.fHeading = psuNmeaInfo->suNmeaGPRMC.fTrack;
        bValidHeading = bTRUE;
        }
    else
        {
        suGPS.suPos.fHeading = 0.0;
        bValidHeading = bFALSE;
        }

    // Roll and pitch assume straight and level
    suGPS.suPos.fPitch = 0.0;
    suGPS.suPos.fRoll  = 0.0;

    // Step through all the targets
    psuCurrTarg = psuFirstTarg;
    while (psuCurrTarg != NULL) 
        {
        // Set valid flags based on info we have
        psuCurrTarg->suTarget2AC.bValidAz    = bValidPos;
        psuCurrTarg->suTarget2AC.bValidEl    = bValidAlt;
        psuCurrTarg->suTarget2AC.bValidRange = bValidPos;
        psuCurrTarg->suAC2Target.bValidAz    = bValidHeading;
        psuCurrTarg->suAC2Target.bValidEl    = bFALSE;
        psuCurrTarg->suAC2Target.bValidRange = bValidPos;

        // Calculate relative position
        vCalcRel(&suGPS.suPos,           &(psuCurrTarg->suPos),  &(psuCurrTarg->suAC2Target));
        vCalcRel(&(psuCurrTarg->suPos),  &suGPS.suPos,           &(psuCurrTarg->suTarget2AC));

        // See if in range of target
        if (*pbInRange == bFALSE)
            *pbInRange = (psuCurrTarg->suAC2Target.fRange <= fDumpRadius);

        // Go to the next target
        psuCurrTarg = psuCurrTarg->psuNext;
        } // end while relative targets to calculate for

    return;
    } // end CalcTargetData();



/* ------------------------------------------------------------------------ */

void vCalcRel(SuPos *psuFrom, SuPos *psuTo, SuRelPos *psuRel)
    {
    double  dRelX, dRelY, dRelZ;
    double  dAz,   dEl,   dRange;

    // Calculate the vector from the psuFrom to the psuTo point.  The psuFrom
    // point is always at the origin.  Note that this is an aproximation
    // that doesn't take into account the curvature of the earth.
    dRelX = (psuTo->dLon      - psuFrom->dLon) * 60.0 * cos(DEG_TO_RAD(psuTo->dLat));
    dRelY = (psuTo->dLat      - psuFrom->dLat) * 60.0;
    dRelZ = (psuTo->fAltitude - psuFrom->fAltitude) / 6080.0;

    // Rotate about the heading, pitch, and roll axes. Order is important!
    vRotate(&dRelX, &dRelY, -psuFrom->fHeading);
    vRotate(&dRelY, &dRelZ,  psuFrom->fPitch);
    vRotate(&dRelX, &dRelZ, -psuFrom->fRoll);

    // Convert to polar coordinates
    vXYZ_To_AzElDist(dRelX, dRelY, dRelZ, &dAz, &dEl, &dRange);
    psuRel->fAz    = (float)fmod(dAz+360.0, 360.0);
    psuRel->fEl    = (float)dEl;
    psuRel->fRange = (float)dRange;

    return;
    }



/* ------------------------------------------------------------------------ */

/* Take a position in XY coordinates and rotate it by the given angle.
   The rotation angle is in degrees, and positive rotation is clockwise. */

void vRotate(double *pdXPos, double *pdYPos, double dRotateAngle)
    {
    double  dStartAngle;
    double  dDistance;

    // Convert to polar coordinates
    dStartAngle = atan2(*pdYPos,*pdXPos);
    dDistance   = sqrt((*pdXPos)*(*pdXPos)+(*pdYPos)*(*pdYPos));

    // Subtract rotation angle and convert back to rectangular coordinates
    *pdXPos = dDistance * cos(dStartAngle - DEG_TO_RAD(dRotateAngle));
    *pdYPos = dDistance * sin(dStartAngle - DEG_TO_RAD(dRotateAngle));

    return;
    }



/* ------------------------------------------------------------------------ */

/* Convert rectangular to polar coordinate system */

void vXYZ_To_AzElDist(double dX,    double dY,    double dZ,
                      double *pdAz, double *pdEl, double *pdDist)
    {

    *pdDist = sqrt(dX*dX + dY*dY + dZ*dZ);
    *pdAz   = 90.0 - RAD_TO_DEG(atan2(dY, dX));
    *pdEl   = RAD_TO_DEG(asin(dZ/(*pdDist)));

    return;
    }



/* ------------------------------------------------------------------------ */

void vUsage(void)
    {
    printf("\nIDMPGPS "MAJOR_VERSION"."MINOR_VERSION" "__DATE__" "__TIME__"\n");
    printf("Dump GPS position from a Ch 10 data file\n");
    printf("Freeware Copyright (C) 2010 Irig106.org\n\n");
    printf("Usage: idmpgps <input file> <output file> [flags]\n");
    printf("   <filename> Input/output file names                        \n");
    printf("   -v               Verbose                                  \n");
    printf("   -c ChNum         Channel Number (required)                \n");
    printf("   -G               Print NMEA GGA data                      \n");
    printf("   -C               Print NMEA RMC data                      \n");
    printf("   -g Lat Lon Elev  Ground target position (ft)              \n");
    printf("   -m Dist          Only dump within this many nautical miles\n");
    printf("                      of ground target position. Can be used \n");
    printf("                      multiple times for multiple ground     \n");
    printf("                      targets.                               \n");
    printf("   -T               Print TMATS summary and exit             \n");
    }




