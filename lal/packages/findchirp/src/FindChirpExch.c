/*----------------------------------------------------------------------- 
 * 
 * File Name: FindChirpExch.c
 *
 * Author: Allen, B. and Creighton, J. D. E.
 * 
 * Revision: $Id$
 * 
 *-----------------------------------------------------------------------
 */

#include "LALStdlib.h"
#include "DataBuffer.h"
#include "FindChirpExch.h"

NRCSID (FINDCHIRPEXCHC, "$Id$");

void
InitializeExchange (
    Status      *status,
    ExchParams **exchParamsOut,
    ExchParams  *exchParamsInp,
    INT4         myProcNum
    )
{
  MPIMessage hello; /* initialization message */

  INITSTATUS (status, "InitializeExchange", FINDCHIRPEXCHC);
  ATTATCHSTATUSPTR (status);

  ASSERT (exchParamsOut, status, FINDCHIRPEXCH_ENULL, FINDCHIRPEXCH_MSGENULL);
  ASSERT (!*exchParamsOut, status, FINDCHIRPEXCH_ENNUL, FINDCHIRPEXCH_MSGENNUL);

  /* now allocate memory for output exchange parameters */
  *exchParamsOut = (ExchParams *) LALMalloc (sizeof(ExchParams));
  ASSERT (*exchParamsOut, status, FINDCHIRPEXCH_ENULL, FINDCHIRPEXCH_MSGENULL);

  if (exchParamsInp) /* I am initializing the exchange */
  {
    INT4 dest = exchParamsInp->partnerProcNum;

    /* initialize communications */

    hello.msg    = exchParamsInp->exchObjectType;
    hello.source = myProcNum;

    /*
     * just use send field as a means to communicate number of objects:
     * set to negative to indicate that initializer (I) want to receive
     * those objects
     *
     * add one to the number of objects in case it is zero
     */

    ASSERT (exchParamsInp->numObjects >= 0, status,
            FINDCHIRPEXCH_ENOBJ, FINDCHIRPEXCH_MSGENOBJ);
    if (exchParamsInp->send)
    {
      hello.send = exchParamsInp->numObjects + 1;
    }
    else
    {
      hello.send = -(exchParamsInp->numObjects + 1);
    }

    /* send off the communications */
    MPISendMsg (status->statusPtr, &hello, dest);
    CHECKSTATUSPTR (status);

    /* copy the input structure to the output structure */
    (*exchParamsOut)->exchObjectType = exchParamsInp->exchObjectType;
    (*exchParamsOut)->send           = exchParamsInp->send;
    (*exchParamsOut)->numObjects     = exchParamsInp->numObjects;
    (*exchParamsOut)->partnerProcNum = exchParamsInp->partnerProcNum;
  }
  else /* I am waiting for someone else to initialize the exchange */
  {
    /* wait for incoming message */
    MPIRecvMsg (status->statusPtr, &hello);
    CHECKSTATUSPTR (status);

    /* the message contains all the information needed */

    (*exchParamsOut)->exchObjectType = hello.msg;
    (*exchParamsOut)->send           = (hello.send < 0);
    (*exchParamsOut)->numObjects     = abs(hello.send) - 1;
    (*exchParamsOut)->partnerProcNum = hello.source;
  }

  DETATCHSTATUSPTR (status);
  RETURN (status);
}

void
FinalizeExchange (
    Status      *status,
    ExchParams **exchParams
    )
{
  INT2       magic = 0xA505; /* A SOS */
  INT2Vector goodbye;

  INITSTATUS (status, "FinalizeExchange", FINDCHIRPEXCHC);
  ATTATCHSTATUSPTR (status);

  ASSERT (exchParams, status, FINDCHIRPEXCH_ENULL, FINDCHIRPEXCH_MSGENULL);
  ASSERT (*exchParams, status, FINDCHIRPEXCH_ENULL, FINDCHIRPEXCH_MSGENULL);

  /* by convension the sending partner initializes the final handshake */

  if ((*exchParams)->send)
  {
    INT4 dest = (*exchParams)->partnerProcNum;

    goodbye.length = 1;
    goodbye.data   = &magic;

    MPISendINT2Vector (status->statusPtr, &goodbye, dest);
    CHECKSTATUSPTR (status);
  }
  else
  {
    INT4 source  = (*exchParams)->partnerProcNum;
    INT2 myMagic = 0;

    goodbye.length = 1;
    goodbye.data   = &myMagic;

    MPIRecvINT2Vector (status->statusPtr, &goodbye, source);
    CHECKSTATUSPTR (status);

    ASSERT (goodbye.data[0] == magic, status, 
            FINDCHIRPEXCH_EHAND, FINDCHIRPEXCH_MSGEHAND);
  }

  /* empty memory */
  LALFree (*exchParams);
  *exchParams = NULL;

  DETATCHSTATUSPTR (status);
  RETURN (status);
}

void
ExchangeDataSegment (
    Status      *status,
    DataSegment *segment,
    ExchParams  *exchParams
    )
{
  CHARVector box; /* a box to hold some bytes of data */

  INITSTATUS (status, "ExchangeDataSegment", FINDCHIRPEXCHC);
  ATTATCHSTATUSPTR (status);

  /* only do a minimal check to see if arguments are somewhat reasonable */
  ASSERT (exchParams, status, FINDCHIRPEXCH_ENULL, FINDCHIRPEXCH_MSGENULL);
  ASSERT (segment, status, FINDCHIRPEXCH_ENULL, FINDCHIRPEXCH_MSGENULL);
  ASSERT (segment->data, status, FINDCHIRPEXCH_ENULL, FINDCHIRPEXCH_MSGENULL);
  ASSERT (segment->spec, status, FINDCHIRPEXCH_ENULL, FINDCHIRPEXCH_MSGENULL);
  ASSERT (segment->resp, status, FINDCHIRPEXCH_ENULL, FINDCHIRPEXCH_MSGENULL);

  if (exchParams->send)  /* I am sending */
  {
    INT4 dest = exchParams->partnerProcNum;

    /* stuff the data segment into a box */
    box.length = sizeof (DataSegment);
    box.data   = (CHAR *) segment;

    /* send box (this sends too much, but it is simple) */
    MPISendCHARVector (status->statusPtr, &box, dest);
    CHECKSTATUSPTR (status);

    /* send pointer fields of data segment */

    /* data */
    MPISendINT2TimeSeries (status->statusPtr, segment->data, dest);
    CHECKSTATUSPTR (status);

    /* spec */
    MPISendREAL4FrequencySeries (status->statusPtr, segment->spec, dest);
    CHECKSTATUSPTR (status);

    /* resp */
    MPISendCOMPLEX8FrequencySeries (status->statusPtr, segment->resp, dest);
    CHECKSTATUSPTR (status);
  }
  else /* I am receiving */
  {
    DataSegment tmpSegment;
    INT4        source = exchParams->partnerProcNum;

    box.length = sizeof (DataSegment);
    box.data   = (CHAR *) &tmpSegment;

    /* receive box */
    MPIRecvCHARVector (status->statusPtr, &box, source);
    CHECKSTATUSPTR (status);

    /* set relevant fields */
    segment->endOfData = tmpSegment.endOfData;
    segment->newLock   = tmpSegment.newLock;
    segment->newCal    = tmpSegment.newCal;
    segment->number    = tmpSegment.number;

    /* receive remaining fields */

    /* data */
    MPIRecvINT2TimeSeries (status->statusPtr, segment->data, source);
    CHECKSTATUSPTR (status);

    /* spec */
    MPIRecvREAL4FrequencySeries (status->statusPtr, segment->spec, source);
    CHECKSTATUSPTR (status);

    /* resp */
    MPIRecvCOMPLEX8FrequencySeries (status->statusPtr, segment->resp, source);
    CHECKSTATUSPTR (status);
  }

  DETATCHSTATUSPTR (status);
  RETURN (status);
}


void
ExchangeInspiralBankIn (
    Status         *status,
    InspiralBankIn *bankIn,
    ExchParams     *exchParams
    )
{
  CHARVector box; /* a box to hold some bytes of data */

  INITSTATUS (status, "ExchangeInspiralBankIn", FINDCHIRPEXCHC);
  ATTATCHSTATUSPTR (status);

  /* only do a minimal check to see if arguments are somewhat reasonable */
  ASSERT (exchParams, status, FINDCHIRPEXCH_ENULL, FINDCHIRPEXCH_MSGENULL);
  ASSERT (bankIn, status, FINDCHIRPEXCH_ENULL, FINDCHIRPEXCH_MSGENULL);

  /* stuff the template bank input into a box */
  box.length = sizeof (InspiralBankIn);
  box.data   = (CHAR *) bankIn;

  if (exchParams->send)  /* I am sending */
  {
    INT4 dest = exchParams->partnerProcNum;

    /* send box */
    MPISendCHARVector (status->statusPtr, &box, dest);
    CHECKSTATUSPTR (status);
  }
  else /* I am receiving */
  {
    INT4 source = exchParams->partnerProcNum;

    /* receive box */
    MPIRecvCHARVector (status->statusPtr, &box, source);
    CHECKSTATUSPTR (status);
  }

  DETATCHSTATUSPTR (status);
  RETURN (status);
}


void
ExchangeInspiralTemplate (
    Status           *status,
    InspiralTemplate *tmplt,
    ExchParams       *exchParams
    )
{
  CHARVector box; /* a box to hold some bytes of data */

  INITSTATUS (status, "ExchangeInspiralTemplate", FINDCHIRPEXCHC);
  ATTATCHSTATUSPTR (status);

  /* only do a minimal check to see if arguments are somewhat reasonable */
  ASSERT (exchParams, status, FINDCHIRPEXCH_ENULL, FINDCHIRPEXCH_MSGENULL);
  ASSERT (tmplt, status, FINDCHIRPEXCH_ENULL, FINDCHIRPEXCH_MSGENULL);

  /* stuff the template into a box */
  box.length = sizeof (InspiralTemplate);
  box.data   = (CHAR *) tmplt;

  if (exchParams->send)  /* I am sending */
  {
    INT4 dest = exchParams->partnerProcNum;

    /* send box */
    MPISendCHARVector (status->statusPtr, &box, dest);
    CHECKSTATUSPTR (status);
  }
  else /* I am receiving */
  {
    INT4 source = exchParams->partnerProcNum;

    /* receive box */
    MPIRecvCHARVector (status->statusPtr, &box, source);
    CHECKSTATUSPTR (status);
  }

  DETATCHSTATUSPTR (status);
  RETURN (status);
}


void
ExchangeInspiralEvent (
    Status        *status,
    InspiralEvent *event,
    ExchParams    *exchParams
    )
{
  CHARVector box; /* a box to hold some bytes of data */

  INITSTATUS (status, "ExchangeInspiralEvent", FINDCHIRPEXCHC);
  ATTATCHSTATUSPTR (status);

  /* only do a minimal check to see if arguments are somewhat reasonable */
  ASSERT (exchParams, status, FINDCHIRPEXCH_ENULL, FINDCHIRPEXCH_MSGENULL);
  ASSERT (event, status, FINDCHIRPEXCH_ENULL, FINDCHIRPEXCH_MSGENULL);

  /* stuff the event into a box */
  box.length = sizeof (InspiralEvent);
  box.data   = (CHAR *) event;

  if (exchParams->send)  /* I am sending */
  {
    INT4 dest = exchParams->partnerProcNum;

    /* send box */
    MPISendCHARVector (status->statusPtr, &box, dest);
    CHECKSTATUSPTR (status);
  }
  else /* I am receiving */
  {
    INT4 source = exchParams->partnerProcNum;

    /* receive box */
    MPIRecvCHARVector (status->statusPtr, &box, source);
    CHECKSTATUSPTR (status);
  }

  DETATCHSTATUSPTR (status);
  RETURN (status);
}

