/*==========================================================================

  i106stat - Generate histogram-like statistics on a Irig 106 data file

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

 $RCSfile: i106stat.c,v $
 $Date: 2006-01-10 19:56:57 $
 $Revision: 1.1 $

 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "stdint.h"

#include "irig106ch10.h"
#include "i106_decode_time.h"
#include "i106_decode_1553f1.h"
//#include "i106_decode_tmats.h"


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


#define MAX_1553_BUSES             16
#define TOTAL_1553_STAT_BUFF_IDX   (0x1000L * MAX_1553_BUSES)

/*
 * Data structures
 * ---------------
 */

/* These hold the number of messages of each type for the histogram. */

typedef struct
  {
    unsigned long * pul1553Msgs;
    unsigned long * pul1553Errs;
    unsigned long   ulIrigTime;
    unsigned long   ulTMATS;
    unsigned long   ulMonitor;
    unsigned long   ulMPEG2;
    unsigned long   ulOther;
    unsigned long   ulTotal;

    long            lFileStartTime;
    long            lStartTime;
    long            lStopTime;
    int             bLogRT2RT;
    int             bRT2RTFound;
    } SuCounts;


/*
 * Module data
 * -----------
 */


/*
 * Function prototypes
 * -------------------
 */

void vResetCounts(SuCounts * psuCnt);
void vPrintCounts(SuCounts * psuCnt, FILE * ptOutFile);
void vUsage(void);


/* ------------------------------------------------------------------------ */

int main(int argc, char ** argv)
    {

    char                    szInFile[80];     // Input file name
    char                    szOutFile[80];    // Output file name
    int                     iArgIdx;
    FILE                  * ptOutFile;        // Output file handle
    int                     bVerbose;
    unsigned long           ulBuffSize = 0L;
    unsigned long           ulReadSize;

    int                     iI106Ch10Handle;
    EnI106Status            enStatus;
    SuI106Ch10Header        suI106Hdr;
    Su1553F1_CurrMsg        su1553Msg;
    SuIrigTimeF1            suIrigTime;
//    SuTmatsInfo             suTmatsInfo;

    unsigned char         * pvBuff = NULL;

    SuCounts                suCnt;


/* Make sure things stay on UTC */

    putenv("TZ=GMT0");
    tzset();

/*
 *  Allocate memory for 1553 histogram.
 */

    suCnt.pul1553Errs = NULL;
    suCnt.pul1553Msgs = NULL;
    vResetCounts(&suCnt);

/*
 * Process the command line arguements
 */

    if (argc < 2) {
    vUsage();
    return 1;
    }

    bVerbose          = bFALSE;               // No verbosity
    suCnt.bLogRT2RT   = bFALSE;               // Don't keep track of RT to RT
    suCnt.bRT2RTFound = bFALSE;
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

                    case 'r' :                   // Log RT to RT
                        suCnt.bLogRT2RT = bTRUE;
                        break;

                    case 'v' :                   // Verbose switch
                        bVerbose = bTRUE;
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


  printf("Computing histogram...\n");


/*
 * Loop until there are no more message whilst keeping track of all the
 * various message counts.
 * --------------------------------------------------------------------
 */

    while (1==1) 
        {

        // Read the next header
        enStatus = enI106Ch10ReadNextHeader(iI106Ch10Handle, &suI106Hdr);

        if (enStatus == I106_EOF)
            {
            printf("End of file\n");
            break;
            }

        if (enStatus != I106_OK)
            {
            printf(" Error reading header : Status = %d\n", enStatus);
            break;
            }

        // Make sure our buffer is big enough, size *does* matter
        if (ulBuffSize < suI106Hdr.ulDataLen)
            {
            pvBuff = realloc(pvBuff, suI106Hdr.ulDataLen);
            ulBuffSize = suI106Hdr.ulDataLen;
            }

        // Read the next data buffer
        ulReadSize = ulBuffSize;
        enStatus = enI106Ch10ReadNextData(iI106Ch10Handle, &ulBuffSize, pvBuff);
        if (enStatus != I106_OK)
            {
            printf(" Error reading header : Status = %d\n", enStatus);
            break;
            }

        suCnt.ulTotal++;
        if (bVerbose) printf("%8.8ld Messages \r",suCnt.ulTotal);

        // Keep track of file and data start and stop times
        if (suI106Hdr.ubyDataType != I106CH10_DTYPE_TMATS)
            {

            // Convert relative time to clock time
            enI106_Rel2IrigTime(suI106Hdr.aubyRefTime, &suIrigTime);

            // Only calculate file start time from time message
            if (suI106Hdr.ubyDataType == I106CH10_DTYPE_IRIG_TIME)
                {
                if (suCnt.lFileStartTime == 0L)
                    suCnt.lFileStartTime = suIrigTime.ulSecs;
                } // end if IRIG time message

            // Anything else is a data packet
            else
                {
                if (suCnt.lStartTime == 0L) suCnt.lStartTime = suIrigTime.ulSecs;
                else                        suCnt.lStopTime  = suIrigTime.ulSecs;
                }
            } // end if not TMATS

        // Log the various data types
        switch (suI106Hdr.ubyDataType)
            {

            case I106CH10_DTYPE_TMATS :         // 0x01

            case I106CH10_DTYPE_IRIG_TIME :     // 0x11

            case I106CH10_DTYPE_1553_FMT_1 :    // 0x19
                // Step through all 1553 messages
                enStatus = enI106_Decode_First1553F1(&suI106Hdr, pvBuff, &su1553Msg);
                while (enStatus == I106_OK)
                    {
                    usPackedMsg = 0;
                    usPackedMsg |=  ptMonMsg->suStatus.wBusNum    << 11;
                    usPackedMsg |= (ptMonMsg->wCmdWord1 & 0xF800) >> 5;
                    usPackedMsg |= (ptMonMsg->wCmdWord1 & 0x0400) >> 5;
                    usPackedMsg |= (ptMonMsg->wCmdWord1 & 0x03E0) >> 5;
                    suCnt.pul1553Msgs[usPackedMsg]++;
                    suCnt.ulMonitor++;
                    enStatus = enI106_Decode_Next1553F1(&su1553Msg);
                    }
/*
          // Use an intermediate pointer to make things easier to read.
          ptMonMsg = (SuFfdSummitMonMsg *) pvBuff;

          // Pack parameters to make an array index
          if (ptMonMsg->suStatus.wBusNum >= MAX_1553_BUSES) {
            printf("1553 bus number '%d' > Max number of buses supported '%d'",
              ptMonMsg->suStatus.wBusNum, MAX_1553_BUSES);
            break;
            }

          usPackedMsg = 0;
          usPackedMsg |=  ptMonMsg->suStatus.wBusNum    << 11;
          usPackedMsg |= (ptMonMsg->wCmdWord1 & 0xF800) >> 5;
          usPackedMsg |= (ptMonMsg->wCmdWord1 & 0x0400) >> 5;
          usPackedMsg |= (ptMonMsg->wCmdWord1 & 0x03E0) >> 5;
          suCnt.pul1553Msgs[usPackedMsg]++;

          // Update the error counts
          if ((ptMonMsg->suStatus.wErrs   != 0) ||
              (ptMonMsg->suStatus.bMsgErr != 0))
            suCnt.pul1553Errs[usPackedMsg]++;
          if ((ptMonMsg->suStatus.wErrs & 0x01) != 0)
            suCnt.ulErrManchester++;
          if ((ptMonMsg->suStatus.wErrs & 0x02) != 0)
            suCnt.ulErrParity++;
          if ((ptMonMsg->suStatus.wErrs & 0x04) != 0)
            suCnt.ulErrOverrun++;
          if ((ptMonMsg->suStatus.wErrs & 0x08) != 0)
            suCnt.ulErrTimeout++;

          // If logging RT to RT then do it for second command word
          if (ptMonMsg->suStatus.bRTRT==1)
            suCnt.bRT2RTFound = bTRUE;
            if (suCnt.bLogRT2RT==bTRUE) {
              usPackedMsg = 0;
              usPackedMsg |=  ptMonMsg->suStatus.wBusNum    << 11;
              usPackedMsg |= (ptMonMsg->wCmdWord2 & 0xF800) >> 5;
              usPackedMsg |= (ptMonMsg->wCmdWord2 & 0x0400) >> 5;
              usPackedMsg |= (ptMonMsg->wCmdWord2 & 0x03E0) >> 5;
              suCnt.pul1553Msgs[usPackedMsg]++;
              } // end if logging RT to RT

          // Update the error counts
          if ((ptMonMsg->suStatus.wErrs   != 0) ||
              (ptMonMsg->suStatus.bMsgErr != 0))
            suCnt.pul1553Errs[usPackedMsg]++;
          if ((ptMonMsg->suStatus.wErrs & 0x01) != 0)
            suCnt.ulErrManchester++;
          if ((ptMonMsg->suStatus.wErrs & 0x02) != 0)
            suCnt.ulErrParity++;
          if ((ptMonMsg->suStatus.wErrs & 0x04) != 0)
            suCnt.ulErrOverrun++;
          if ((ptMonMsg->suStatus.wErrs & 0x08) != 0)
            suCnt.ulErrTimeout++;

          break;
*/

        default:
          ++suCnt.ulOther;
        } // end switch on message type

    }   /* End while */



/*
 * Now print out the results of histogram.
 * ---------------------------------------
 */

  vPrintCounts(&suCnt, ptOutFile);

/*
 *  Free dynamic memory.
 */

  free(pvBuff);
  pvBuff = NULL;

  fclose(ptOutFile);

  return 0;
  }



/* ------------------------------------------------------------------------ */

void vResetCounts(SuCounts * psuCnt)
{

  // Free 1553 counts memory
  if (psuCnt->pul1553Msgs != NULL)
    free(psuCnt->pul1553Msgs);
  if (psuCnt->pul1553Errs != NULL)
    free(psuCnt->pul1553Errs);

  // Clear out the counts
  memset(psuCnt, 0x00, sizeof(SuCounts));
  psuCnt->pul1553Msgs = calloc(TOTAL_1553_STAT_BUFF_IDX, sizeof (unsigned long));
//  memset(psuCnt->pul1553Msgs, 0x00, 0x3FFFL*sizeof (unsigned long));
  psuCnt->pul1553Errs = calloc(TOTAL_1553_STAT_BUFF_IDX, sizeof (unsigned long));
//  memset(psuCnt->pul1553Errs, 0x00, 0x3FFFL*sizeof (unsigned long));
  
  assert(psuCnt->pul1553Msgs!=NULL);
  assert(psuCnt->pul1553Errs!=NULL);

  return;
}



/* ------------------------------------------------------------------------ */

void vPrintCounts(SuCounts * psuCnt, FILE * ptOutFile)
{
  long   lMsgIdx;

  fprintf(ptOutFile,"\n=-=-= Message Totals by Type-=-=-=\n\n", psuCnt->ulTotal);

  fprintf(ptOutFile,"File Start %s",  ctime(&(psuCnt->lFileStartTime)));
  fprintf(ptOutFile,"Data Start %s",  ctime(&(psuCnt->lStartTime)));
  fprintf(ptOutFile,"Data Stop  %s\n",ctime(&(psuCnt->lStopTime)));

/*
  if (psuCnt->ulHeader != 0)
    fprintf(ptOutFile,"Headers:          %10lu\n", psuCnt->ulHeader);

  if (psuCnt->ulEvent != 0)
    fprintf(ptOutFile,"Events:           %10lu\n", psuCnt->ulEvent);

  if (psuCnt->ulMonitor != 0) {
    assert(psuCnt->pul1553Msgs!=NULL);
    assert(psuCnt->pul1553Errs!=NULL);
    fprintf(ptOutFile,"SuMMIT Monitor:   %10lu\n", psuCnt->ulMonitor);
    for (lMsgIdx=0; lMsgIdx<TOTAL_1553_STAT_BUFF_IDX; lMsgIdx++) {
      if (psuCnt->pul1553Msgs[lMsgIdx] != 0) {
        fprintf(ptOutFile,"\tBus %d  RT %2d  %c  SA %2d  Msgs %9lu  Errs %9lu\n",
          (lMsgIdx >> 11) & 0x0007,
          (lMsgIdx >>  6) & 0x001f,
          (lMsgIdx >>  5) & 0x0001 ? 'T' : 'R',
          (lMsgIdx      ) & 0x001f,
          psuCnt->pul1553Msgs[lMsgIdx],
          psuCnt->pul1553Errs[lMsgIdx]);
        } // end if count not zero
      } // end for each RT

    fprintf(ptOutFile,"  Manchester Errors :   %10lu\n", psuCnt->ulErrManchester);
    fprintf(ptOutFile,"  Parity Errors     :   %10lu\n", psuCnt->ulErrParity);
    fprintf(ptOutFile,"  Overrun Errors    :   %10lu\n", psuCnt->ulErrOverrun);
    fprintf(ptOutFile,"  Timeout Errors    :   %10lu\n", psuCnt->ulErrTimeout);

    if (psuCnt->bRT2RTFound == bTRUE) {
      fprintf(ptOutFile,"\n  Warning - RT to RT transfers found in the data\n");
      if (psuCnt->bLogRT2RT == bTRUE) {
        fprintf(ptOutFile,"  Message total is NOT the sum of individual RT totals\n");
        }
      else {
        fprintf(ptOutFile,"  Some transmit RTs may not be shown\n");
        }
      }

    } // end if 1553 messages

  if (psuCnt->ulGPS_3A != 0)
    fprintf(ptOutFile,"GPS_3A:           %10lu\n",   psuCnt->ulGPS_3A);

  if (psuCnt->ulGTRI_Serial != 0)
    fprintf(ptOutFile,"GTRI Serial:      %10lu\n",   psuCnt->ulGTRI_Serial);

  if (psuCnt->ulDiscretes != 0)
    fprintf(ptOutFile,"Discretes:        %10lu\n",   psuCnt->ulDiscretes);

  if (psuCnt->ulSysLog != 0)
    fprintf(ptOutFile,"SysLog:           %10lu\n",   psuCnt->ulSysLog);

  if (psuCnt->ulUser != 0)
    fprintf(ptOutFile,"User:             %10lu\n",   psuCnt->ulUser);

  if (psuCnt->ulUnidentified != 0)
    fprintf(ptOutFile,"Unidentified:     %10lu\n",   psuCnt->ulUnidentified);

  fprintf(ptOutFile,"\nTOTAL RECORDS:    %10lu\n\n", psuCnt->ulTotal);
*/

  return;
}



/* ------------------------------------------------------------------------ */

void vUsage(void)
  {
  printf("I106STAT "MAJOR_VERSION"."MINOR_VERSION" "__DATE__" "__TIME__"\n");
  printf("Usage: i106stat <input file> <output file> [flags]\n");
  printf("   <filename> Input/output file names\n");
  printf("   -r         Log both sides of RT to RT transfers\n");
  printf("   -v         Verbose\n");
  }



