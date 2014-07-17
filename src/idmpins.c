/* 

 idmpins - A utility for dumping INS position data

 Copyright (c) 2007 Irig106.org

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
#include <math.h>
#include <assert.h>

#include "config.h"
#include "stdint.h"
#include "irig106ch10.h"

#include "i106_time.h"
#include "i106_decode_time.h"
#include "i106_decode_1553f1.h"
#include "i106_decode_tmats.h"


/*
 * Macros and definitions
 * ----------------------
 */

#define MAJOR_VERSION  "01"
#define MINOR_VERSION  "02"

#if !defined(bTRUE)
#define bTRUE   (1==1)
#define bFALSE  (1==0)
#endif

#define PREEGI_INS_SA_POSITION      27
#define PREEGI_INS_SA_ATTITUDE      25

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

typedef struct {
  unsigned         uWordCnt    : 5;    /* Data Word Count or Mode Code      */
  unsigned         uSubAddr    : 5;    /* Subaddress Specifier              */
  unsigned         bTR         : 1;    /* Transmit/Receive Flag             */
  unsigned         uRTAddr     : 5;    /* RT Address                        */
  } SuCmd1553;


// F-16/C-130/A-10 EGI

typedef struct INS_DataS {
  unsigned short   uStatus;            /*  1 */
  unsigned short   uTimeTag;           /*  2 */
           short   sVelX_MSW;          /*  3 */
  unsigned short   uVelX_LSW;          /*  4 */
           short   sVelY_MSW;          /*  5 */
  unsigned short   uVelY_LSW;          /*  6 */
           short   uVelZ_MSW;          /*  7 */
  unsigned short   uVelZ_LSW;          /*  8 */
  unsigned short   uAz;                /*  9 */
           short   sRoll;              /* 10 */
           short   sPitch;             /* 11 */
  unsigned short   uTrueHeading;       /* 12 */
  unsigned short   uMagHeading;        /* 13 */
           short         :  5;         /* 14 */
           short   sAccX : 11;         /* 14 */
           short         :  5;         /* 15 */
           short   sAccY : 11;         /* 15 */
           short         :  5;         /* 16 */
           short   sAccZ : 11;         /* 16 */
           short   sCXX_MSW;           /* 17 */
  unsigned short   uCXX_LSW;           /* 18 */
           short   sCXY_MSW;           /* 19 */
  unsigned short   uCXY_LSW;           /* 20 */
           short   sCXZ_MSW;           /* 21 */
  unsigned short   uCXZ_LSW;           /* 22 */
           short   sLon_MSW;           /* 23 */
  unsigned short   uLon_LSW;           /* 24 */
           short   sAlt;               /* 25 */
           short   sSteeringError;     /* 26 */
           short   sTiltX;             /* 27 */
           short   sTiltY;             /* 28 */
           short   sJustInCase[4];     /* 29-32 */
  } SuINS_Data;

// F-15

typedef struct INS_F15_DataS {
  unsigned short   uStatus;            // I_INS_AMX.MSG.M01.INS_STATUS
                                       //  0 - POSITION_AND_VELOCITY_VALID
                                       //  1 - ATTITUDE_VALID
                                       //  2 - BATTERY_VALID
                                       //  3 - TIME_TAG_CLOCK_VALID
                                       //  4 - BARO_INERTIAL_ALTITUDE_VALID
                                       //  5 - BIT_ACKNOWLEDGE
                                       //  6 - DEGRADED_NAV
                                       //  7 - KALMAN_FILTER_BACKGROUND_BUSY
                                       //  8 - KALMAN_FILTER_UPDATE_COMPLETE
                                       //  9 - KALMAN_FILTER_UPDATE_REJECTED
                                       // 10 - TAXI_PROHIBITED
                                       // 11 - ALIGN_HOLD
                                       // 12...14 - OPERATING_MODE
                                       // 15 - HOLDING_BRAKE_SET
  unsigned short   uVelTime;           // VELOCITY_TIME_TAG IN SEC +1.6384
  unsigned short   uAttTime;           // ATTITUDE_TIME_TAG IN SEC +1.6384
  unsigned short   uPosTime;           // PRESENT_POSITION_TIME_TAG IN SEC +1.6384
           short   sAlt;               // BARO_INERTIAL_ALTITUDE FX FT -2+16
           short   sLat_MSW;           // PRESENT_POSTION_LATITUDE FX DEG -180
  unsigned short   uLat_LSW;
           short   sLon_MSW;           // PRESENT_POSITION_LONGITUDE FX DEG -180
  unsigned short   uLon_LSW;
           short   sPitch;             // PITCH FX DEG -180
           short   sRoll;              // ROLL FX DEG -180
  unsigned short   uTrueHeading;       // TRUE_HEADING FX DEG -180
  unsigned short   uAz;                // PLATFORM_AZIMUTH FX DEG -180
  unsigned short   uWander;            // WANDER_ANGLE FX DEG -180
  unsigned short   uMagHeading;        // MAGNETIC_HEADING FX DEG -180
           short   sVelNorth;          // NORTH_VELOCITY FX FPS -2+12
           short   sVelEast;           // EAST_VELOCITY FX FPS -2+12
           short   sVelUp;             // UP_VELOCITY FX FPS -2+11
  unsigned short   uGndSpeed;          // GROUND_SPEED FX FPS -2+12
           short   sAccNorth;          // NORTH_ACCELERATION FX FPS2 -2+08
           short   sAccEast;           // EAST_ACCELERATION FX FPS2 -2+08
           short   sAccUp;             // UP_ACCELERATION FX FPS2 -2+09
           short   sJustInCase[10];    // 23-32 */
  } SuINS_F15_Data;


typedef struct {
    double        dLat;
    double        dLon;
    float         fAltitude;         /* INS altitude                      */
    float         fRoll;             /* Aircraft roll position            */
    float         fPitch;            /* Aircraft pitch position           */
    float         fHeading;          /* True heading                      */
    } SuPos;

typedef struct {
    int           bValid;
    int           bHaveAttitude;
    SuPos         suPos;
    float         fSpeed;            /* Ground speed                      */
    float         fAccel;            /* Acceleration                      */
    } SuAC_Data;

typedef struct {
    float         fAz;               /* Azimuth, 0 = North                */
    float         fEl;               /* Elevation angle, + UP, - Down     */
    float         fRange;            /* Range in nautical miles           */
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

int           m_iI106Handle;


/*
 * Function prototypes
 * -------------------
 */

void vPrintTmats(SuTmatsInfo * psuTmatsInfo, FILE * psuOutFile);
void vUsage(void);

void vCalcRel(SuPos *psuFrom, SuPos *psuTo, SuRelPos *psuRel);
void vRotate(double *pdXPos, double *pdYPos, double dRotateAngle);
void vXYZ_To_AzElDist(double dX,    double dY,    double dZ,
                      double *pdAz, double *pdEl, double *pdDist);


/* ======================================================================== */

int main (int argc, char *argv[])
  {
    FILE              * psuOutFile;         // Output file handle
    SuIrig106Time       suTime;
    char              * szTime;
    int                 iMilliSec;
    char                szInFile[80];       // Input file name
    char                szOutFile[80];      // Output file name
    int                 iIdx;
    int                 iArgIdx;
    int                 bVerbose;
    int                 bDumpAttitude;      // Dump INS aircraft attitude
    int                 bGotINS;            // Got milk?
    int                 bInRange;
    int                 bInRangePrev;
    int                 bPrintTMATS;
    unsigned            uChannel;           // Channel number
    unsigned            uRTAddr;            // RT address of INS
    unsigned            uTR;                // Transmit bit
    unsigned            uSubAddr;           // Subaddress of INS message
    unsigned            uDecimation;        // Decimation factor
    unsigned            uDecCnt;            // Decimation count
    unsigned            uINSType;           // Type of INS
    int                 iDumpRelIdx;
    unsigned long       lINSPoints = 0L;
    unsigned long       lMsgs = 0L;         // Total message

    SuAC_Data           suAC;               // Aircraft data

    float               fDumpRadius;
    SuTargPosLL       * psuFirstTarg = NULL;
    SuTargPosLL      ** ppsuCurrTarg = &psuFirstTarg;
    SuTargPosLL       * psuCurrTarg;

    EnI106Status        enStatus;
    SuI106Ch10Header    suI106Hdr;

    unsigned long       ulBuffSize = 0L;
    unsigned char     * pvBuff  = NULL;

    Su1553F1_CurrMsg    su1553Msg;
    SuTmatsInfo         suTmatsInfo;

    SuINS_Data        * psuINS01     = NULL;
    SuINS_F15_Data    * psuINS_F15   = NULL;

/* Make sure things stay on UTC */

    putenv("TZ=GMT0");
    tzset();

/*
 * Process the command line arguements
 */

    if (argc < 2) 
        {
        vUsage();
        return 1;
        }

/* Preset default values */

    suAC.bHaveAttitude = bFALSE;
    bDumpAttitude      = bFALSE;
    fDumpRadius        = 0.0;
    bInRangePrev       = bTRUE;

    uChannel    =  0;                    /* Default INS bus                   */
    uRTAddr     =  6;                    /* RT address of INS                 */
    uTR         =  1;                    /* Transmit bit                      */
    uSubAddr    = 16;                    /* Subaddress of INS message         */
    uDecimation =  1;                    /* Decimation factor                 */
    uINSType    =  1;
    bVerbose    = bFALSE;               // No verbosity
    bPrintTMATS = bFALSE;

    szInFile[0]  = '\0';
    strcpy(szOutFile,"con");            // Default is stdout

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

                    case 'a' :                   /* Aircraft attitude switch */
                        bDumpAttitude = bTRUE;
                        break;

                    case 'c' :                   /* Channel number */
                        iArgIdx++;
                        sscanf(argv[iArgIdx],"%d",&uChannel);
                        break;

                    case 'r' :                   /* RT address */
                        iArgIdx++;
                        sscanf(argv[iArgIdx],"%d",&uRTAddr);
                        if (uRTAddr>31) 
                            {
                            printf("Invalid RT address\n");
                            vUsage();
                            return 1;
                            }
                        break;

                    case 't' :                   /* TR bit */
                        iArgIdx++;
                        sscanf(argv[iArgIdx],"%d",&uTR);
                        if ((uTR!=0)&&(uTR!=1)) 
                            {
                            printf("Invalid TR flag\n");
                            vUsage();
                            return 1;
                            }
                        break;

                    case 's' :                   /* Subaddress */
                        iArgIdx++;
                        sscanf(argv[iArgIdx],"%d",&uSubAddr);
                        if (uSubAddr>31) 
                            {
                            printf("Invalid subaddress\n");
                            vUsage();
                            return 1;
                            }
                        break;

                    case 'd' :                   /* Decimation */
                        iArgIdx++;
                        sscanf(argv[iArgIdx],"%d",&uDecimation);
                        break;

                    case 'i' :                   /* INS type */
                        iArgIdx++;
                        sscanf(argv[iArgIdx],"%d",&uINSType);
                        if (uINSType>2) 
                            {
                            printf("Invalid INS type\n");
                            vUsage();
                            return 1;
                            }
                        break;

                    case 'g' :                   /* Ground target position */
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

                    case 'm' :                   /* Dump radius */
                        iArgIdx++;
                        sscanf(argv[iArgIdx],"%f",&fDumpRadius);
                        break;

                    case 'T' :                   /* Print TMATS flag */
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

    if ((strlen(szInFile)==0) || (strlen(szOutFile)==0)) 
        {
        vUsage();
        return 1;
        }

    if ((uChannel == 0) && (bPrintTMATS == bFALSE))
        {
        vUsage();
        printf("\nERROR - A channel number must be specified\n");
        return 1;
        }

/* Now initialize a bunch of stuff based on input parameters */

    uDecCnt = uDecimation;

    if (psuFirstTarg != NULL)
        bDumpAttitude = bTRUE;

    if ((fDumpRadius != 0.0) && (psuFirstTarg == NULL))
        fDumpRadius = 0.0;

    bGotINS = bFALSE;

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

    psuOutFile = fopen(szOutFile,"w");
    if (psuOutFile == NULL) 
        {
        printf("Error opening output file\n");
        return 1;
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
            ulBuffSize = suI106Hdr.ulPacketLen;

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

        fclose(psuOutFile);
        return 0;
        } // end if print TMATS

/*
 * Hold onto your shorts, here we go...
 */

    fprintf(stderr, "\nIDMPINS "MAJOR_VERSION"."MINOR_VERSION"\n");
    fprintf(stderr, "Freeware Copyright (C) 2007 Irig106.org\n\n");

    fprintf(stderr, "Input Data file '%s'\n", szInFile);
    fprintf(psuOutFile,"Input Data file '%s'\n", szInFile);

    printf("Chan %d  RT %d  TR %d  SA %d  INS Position/Attitude\n",
        uChannel, uRTAddr, uTR, uSubAddr);
    fprintf(psuOutFile,"Chan %d  RT %d  TR %d  SA %d  INS Position/Attitude\n",
        uChannel, uRTAddr, uTR, uSubAddr);

    if (psuFirstTarg != NULL) 
        {
        psuCurrTarg = psuFirstTarg;
        while (psuCurrTarg != NULL) 
            {
            fprintf(psuOutFile,"Ground Target   Lat %12.6f  Lon %12.6f  Elev %5.0f\n",
                psuCurrTarg->suPos.dLat,
                psuCurrTarg->suPos.dLon, 
                psuCurrTarg->suPos.fAltitude);
            psuCurrTarg = psuCurrTarg->psuNext;
            } // end while relative targets to calculate for
        }

    if (fDumpRadius != 0.0) 
        {
        fprintf(psuOutFile,"Dump Radius %6.1f nm\n", fDumpRadius);
        }

    fprintf(psuOutFile,"\n");

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

        if  (iIdx==1)                   fprintf(psuOutFile,"           ");
        if  (iIdx==2)                   fprintf(psuOutFile,"           ");
        if  (iIdx==3)                   fprintf(psuOutFile,"    Time   ");
        if  (iIdx==4)                   fprintf(psuOutFile,"   (UTC)   ");

        if  (iIdx==1)                   fprintf(psuOutFile,"    Raw INS    Raw INS    INS ");
        if  (iIdx==2)                   fprintf(psuOutFile,"   Longitude  Latitude   Data ");
        if  (iIdx==3)                   fprintf(psuOutFile,"   (+ East)   (+ North)  Valid");
        if  (iIdx==4)                   fprintf(psuOutFile,"   (- West)   (- South)       ");

        if ((iIdx==1)&&(bDumpAttitude)) fprintf(psuOutFile,"                                                      ");
        if ((iIdx==2)&&(bDumpAttitude)) fprintf(psuOutFile,"    INS      True     Roll     Pitch           Ground ");
        if ((iIdx==3)&&(bDumpAttitude)) fprintf(psuOutFile,"  Altitude  Heading  (+ Right) (+ Up  )        Speed  ");
        if ((iIdx==4)&&(bDumpAttitude)) fprintf(psuOutFile,"   (MSL)             (- Left ) (- Down)   G's   (kts) ");

        psuCurrTarg  = psuFirstTarg;
        iDumpRelIdx = 1;
        while (psuCurrTarg != NULL) 
            {
            if (iIdx==1)                  fprintf(psuOutFile,"  ------------ Ground Target %d ------------", iDumpRelIdx);
            if (iIdx==2)                  fprintf(psuOutFile,"  Target  Az to   Elev to   Az to   Elev to");
            if (iIdx==3)                  fprintf(psuOutFile,"   Range  Target  Target     A/C     A/C   ");
            if (iIdx==4)                  fprintf(psuOutFile,"   (NM)                                    ");
            psuCurrTarg = psuCurrTarg->psuNext;
            iDumpRelIdx++;
            }

        fprintf(psuOutFile,"\n");

        } /* end for each header line */

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

            // If 1553 message and right channel then process it
            if ((suI106Hdr.ubyDataType == I106CH10_DTYPE_1553_FMT_1) &&
                (uChannel == suI106Hdr.uChID))
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
                enStatus = enI106_Decode_First1553F1(&suI106Hdr, pvBuff, &su1553Msg);
                while (enStatus == I106_OK)
                    {
                    SuCmdWordU   * psuCmdWord;

                     // Look for INS packets
                    if ((!su1553Msg.psu1553Hdr->bRT2RT) || (uTR == 0))
                        psuCmdWord = su1553Msg.psuCmdWord1;
                    else
                        psuCmdWord = su1553Msg.psuCmdWord2;

                    if ((psuCmdWord->suStruct.uRTAddr  == uRTAddr ) &&
                        (psuCmdWord->suStruct.bTR      == uTR     ) &&
                        (psuCmdWord->suStruct.uSubAddr == uSubAddr) &&
                        (i1553WordCnt(psuCmdWord)      >= 1      ))
                    //if ((su1553Msg.psuCmdWord1->suStruct.uRTAddr  == uRTAddr ) &&
                    //    (su1553Msg.psuCmdWord1->suStruct.bTR      == uTR     ) &&
                    //    (su1553Msg.psuCmdWord1->suStruct.uSubAddr == uSubAddr) &&
                    //    (i1553WordCnt(su1553Msg.psuCmdWord1)      >= 24 ))
                        {

                        // Decode INS data
                        // ---------------

                        // F-16/C-130/A-10 EGI
                        if (uINSType == 1) 
                            {

                            // If decimation counter down to 1 then  calculate
                            if (uDecCnt == 1) 
                                {

                                psuINS01 = (SuINS_Data *)(su1553Msg.pauData);

                                // Convert INS Lat and Lon to degrees if it is to be used
                                suAC.suPos.dLat  = (180.0/M_PI)*asin((float)((long)(psuINS01->sCXZ_MSW)<<16 | (long)(psuINS01->uCXZ_LSW))/(float)0x40000000);
                                suAC.suPos.dLon  = (180.0     )*    ((float)((long)(psuINS01->sLon_MSW)<<16 | (long)(psuINS01->uLon_LSW))/(float)0x7fffffff);
                                suAC.bValid      = (psuINS01->uStatus & 0xf800) == 0;

                                if (bDumpAttitude) 
                                    {
                                    suAC.suPos.fRoll     = 180.0f * psuINS01->sRoll        / (float)0x7fff;
                                    suAC.suPos.fPitch    = 180.0f * psuINS01->sPitch       / (float)0x7fff;
                                    suAC.suPos.fHeading  = 180.0f * psuINS01->uTrueHeading / (float)0x7fff;
                                    suAC.suPos.fAltitude = psuINS01->sAlt * 4.0f;
                                    suAC.fAccel          = sqrt((float)psuINS01->sAccX*(float)psuINS01->sAccX +
                                                                (float)psuINS01->sAccY*(float)psuINS01->sAccY +
                                                                (float)psuINS01->sAccZ*(float)psuINS01->sAccZ) / 32.0f;
                                    suAC.fSpeed          = sqrt((float)psuINS01->sVelX_MSW*(float)psuINS01->sVelX_MSW +
                                                                (float)psuINS01->sVelY_MSW*(float)psuINS01->sVelY_MSW) * 3600.0f / (4.0f * 6080.0f);
                                    suAC.bHaveAttitude   = bTRUE;
                                    } // end dump attitude

                                uDecCnt = uDecimation;
                                bGotINS = bTRUE;

                                lINSPoints++;
                                if (bVerbose) printf("%8.8ld INS points     \r",lINSPoints);

                                } // end if decimation count down to 1

                            // else decrement decimation counter 
                            else 
                                uDecCnt--;

                            } // end if F-16/C-130/A-10 EGI case

                        // F-15
                        if (uINSType == 2) 
                            {

                            if (uDecCnt == 1) 
                                {

                                // Convert INS Lat and Lon to degrees if it is to be used
                                suAC.suPos.dLat   = (180.0)*((float)((long)(psuINS_F15->sLat_MSW)<<16 | (long)(psuINS_F15->uLat_LSW))/(float)0x7fffffff);
                                suAC.suPos.dLon   = (180.0)*((float)((long)(psuINS_F15->sLon_MSW)<<16 | (long)(psuINS_F15->uLon_LSW))/(float)0x7fffffff);
                                suAC.bValid      = (psuINS_F15->uStatus & 0x0003) == 3;  // 0xc000 ???

                                if (bDumpAttitude) 
                                    {
                                    suAC.suPos.fRoll     = 180.0f * psuINS_F15->sRoll        / (float)0x7fff;
                                    suAC.suPos.fPitch    = 180.0f * psuINS_F15->sPitch       / (float)0x7fff;
                                    suAC.suPos.fHeading  = 180.0f * psuINS_F15->uTrueHeading / (float)0x7fff;
//                fAltitude = psuINS_F15->sAlt * 4.0f;
// ???                fAccel    = sqrt((float)psuINS_F15->sAccNorth*(float)psuINS_F15->sAccNorth +
//                                     (float)psuINS_F15->sAccEast *(float)psuINS_F15->sAccEast  +
//                                     (float)psuINS_F15->sAccUp   *(float)psuINS_F15->sAccUp)    / 32.0f;
// ???                fSpeed    = sqrt((float)psuINS_F15->sVelNorth*(float)psuINS_F15->sVelNorth +
//                                     (float)psuINS_F15->sVelEast *(float)psuINS_F15->sVelEast) * 3600.0f / (4.0f * 6080.0f);
                                    suAC.bHaveAttitude = bTRUE;
                                    } // end dump attitude

                                uDecCnt = uDecimation;
                                bGotINS = bTRUE;

                                lINSPoints++;
                                if (bVerbose) printf("%8.8ld INS points     \r",lINSPoints);

                                } // end if decimation count down to 1

                            else 
                                uDecCnt--;
              
                            } // end if F-15 case



                        // If we got INS data then do some calculations and print it
                        // ---------------------------------------------------------

                        if (bGotINS == bTRUE) 
                            {

                            // If no ground target then we are always in range   
                            if (psuFirstTarg == NULL)
                                bInRange = bTRUE;

                            // If there is a ground target then do some calculations and see 
                            // if we are in range.
                            else 
                                {
                                if (fDumpRadius == 0.0)
                                    bInRange = bTRUE;
                                else
                                    bInRange = bFALSE;
                                psuCurrTarg = psuFirstTarg;
                                while (psuCurrTarg != NULL) 
                                    {

                                    // Calculate relative position
                                    vCalcRel(&suAC.suPos,            &(psuCurrTarg->suPos),  &(psuCurrTarg->suAC2Target));
                                    vCalcRel(&(psuCurrTarg->suPos),  &suAC.suPos,            &(psuCurrTarg->suTarget2AC));

                                    // See if in range of target
                                    if (bInRange == bFALSE)
                                        bInRange = (psuCurrTarg->suAC2Target.fRange <= fDumpRadius);

                                    // Go to the next target
                                    psuCurrTarg = psuCurrTarg->psuNext;
                                    } // end while relative targets to calculate for
                                } // end ground target calculations


                            // Print out the data
                            if (bInRange == bTRUE)
                                {

                                // If we weren't in range last time put a blank line
                                if (bInRangePrev == bFALSE)
                                    fprintf(psuOutFile,"\n");

                                enI106_Rel2IrigTime(m_iI106Handle,
                                    su1553Msg.psu1553Hdr->aubyIntPktTime, &suTime);
                                szTime = ctime((time_t *)&suTime.ulSecs);
                                szTime[19] = '\0';
                                iMilliSec = (int)(suTime.ulFrac / 10000.0);

                                fprintf(psuOutFile,"%s.%3.3d", &szTime[11], iMilliSec);
                                fprintf(psuOutFile," %11.6f %10.6f   %u ", suAC.suPos.dLon,suAC.suPos.dLat,suAC.bValid);

                                if (suAC.bHaveAttitude) 
                                    {
                                    fprintf(psuOutFile," %7.0f     %5.1f  %8.3f   %7.3f   %6.3f  %5.1f ",
                                        suAC.suPos.fAltitude,suAC.suPos.fHeading,suAC.suPos.fRoll,suAC.suPos.fPitch,suAC.fAccel,suAC.fSpeed);
                                    }

                                psuCurrTarg = psuFirstTarg;
                                while (psuCurrTarg != NULL) 
                                    {
                                    fprintf(psuOutFile,"  %6.2f  %5.1f    %5.1f    %5.1f   %5.1f  ",
                                        psuCurrTarg->suAC2Target.fRange, 
                                        psuCurrTarg->suAC2Target.fAz, psuCurrTarg->suAC2Target.fEl,
                                        psuCurrTarg->suTarget2AC.fAz, psuCurrTarg->suTarget2AC.fEl);
                                    psuCurrTarg = psuCurrTarg->psuNext;
                                    }

                                fprintf(psuOutFile,"\n");

                                bGotINS           = bFALSE;
                                suAC.bHaveAttitude = bFALSE;
                                } // end if in range

                            bInRangePrev = bInRange;

                            } // end if got INS data

                        } // end if RT, TR, and WC are OK

                    // Get the next 1553 message
                    enStatus = enI106_Decode_Next1553F1(&su1553Msg);
                    } // end while processing 1553 messages from an IRIG packet

                } // end if 1553 and right channel


            } while (bFALSE); // end one time loop

        // If EOF break out of main read loop
        if (enStatus == I106_EOF)
            {
            fprintf(stderr, "End of file\n");
            break;
            }

        }   // End while

    // Finish up */

    printf("Total INS points %8.8ld\n",lINSPoints);
    printf("Total Message %lu\n", lMsgs);

/*
 * Close data file and generally clean up
 */

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

    fprintf(psuOutFile,"\n=-=-= 1553 Channel Summary =-=-=\n\n");

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
                if (strcasecmp(psuRDataSource->szChannelDataType,"1553IN") == 0)
                    {
//                    iRDsiIndex = psuRDataSource->iDataSourceNum;
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

void vUsage(void)
    {
    printf("IDMPINS "MAJOR_VERSION"."MINOR_VERSION" "__DATE__" "__TIME__"\n");
    printf("Dump recorded INS data from F-16, C-130, and A-10 EGI aircraft\n");
    printf("vUsage: idmpins <input file> <output file> [flags]           \n");
    printf("   <filename>       Input/output file names                  \n");
    printf("   -v               Verbose                                  \n");
    printf("   -a               Dump aircraft INS attitude               \n");
    printf("   -c Bus           IRIG Channel Number                      \n");
    printf("   -r RT            INS RT Address(1-30) (default 6)         \n");
    printf("   -t T/R           INS T/R Bit (0=R 1=T) (default 1)        \n");
    printf("   -s SA            INS Message Subaddress (default 16)      \n");
    printf("   -d Num           Dump 1 in 'Num' messages (default all)   \n");
    printf("   -i Type          INS Type (default 1)                     \n");
    printf("                      1 = F-16/C-130/A-10 EGI                \n");
    printf("                      2 = F-15                               \n");
    printf("   -g Lat Lon Elev  Ground target position (ft)              \n");
    printf("   -m Dist          Only dump within this many nautical miles\n");
    printf("                      of ground target position              \n");
    printf("                                                             \n");
    printf("   -T               Print TMATS summary and exit             \n");
    return;
    }



/* ------------------------------------------------------------------------ */

/* Calculate relative bearing and depression angle to a target point in
 * space (the "psuTo" point) based on aircraft attitude and location (the
 * psuFrom point).

 Relative bearing is the angle between the A/C longitudinal axis (the
 nose of the A/C) and the project of the vector to the target point
 onto the lateral plane of the A/C (that is, the plane defined by the
 A/C longitudinal and lateral axes).

 Relative depression angle is that angle formed between the vector to the
 target point and this vector's projection onto the A/C lateral plane.

 The relative coordinate system has the psuFrom point at the origin.  The X
 axis is east, the Y axis north, and the Z axis is up.

 */

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


