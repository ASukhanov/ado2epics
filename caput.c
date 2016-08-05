/*
 * Stripped down version of EPICS' caput.c.
 * Version 02 2016-08-03 by Andrei Sukhanov.
 */
//''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
// the following is for debug version
//#define DBGprintf(fmt, ...) printf(fmt,__VA_ARGS__);
// the following is for release version
#define DBGprintf(fmt, ...) ;
int gVerb=0;
//,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

#include <stdio.h>
#include <string.h>
#include <epicsStdlib.h>

#include <cadef.h>
#include <epicsGetopt.h>
#include <epicsEvent.h>
#include <epicsString.h>

#include "tool_lib.h"

#define VALID_DOUBLE_DIGITS 18  /* Max usable precision for a double */

/* Different request types */
typedef enum { get, callback } RequestT;

/* Valid EPICS string */
typedef char EpicsStr[MAX_STRING_SIZE];

static epicsEventId epId;

/*+**************************************************************************
 *
 * Function:    put_event_handler
 *
 * Description: CA event_handler for request type callback
 *              Sets status flags and marks as done.
 *
 * Arg(s) In:   args  -  event handler args (see CA manual)
 *
 **************************************************************************-*/

void put_event_handler ( struct event_handler_args args )
{
    /* Retrieve pv from event handler structure */
    pv* pPv = args.usr;

    /* Store status, then give EPICS event */
    pPv->status = args.status;
    epicsEventSignal( epId );
}

pv* pvs=NULL;                /* Array of PV structures */

//'''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
int caput_string(const char* pvname, const char* value)
{
	DBGprintf("caput_string: caput(%s,%s)\n",pvname,value);
    int result;                 /* CA result */
    RequestT request = get;     /* User specified request type */
    //request = callback;

    //not used//int isArray = 0;            /* Deselect array mode */
    //int i;
    int count = 1;
    chtype dbrType = DBR_STRING;
    EpicsStr *sbuf;
    double *dbuf;
    char *cbuf = 0;
    char *ebuf = 0;
    void *pbuf;
    int len = 0;
    int waitStatus;

    int nPvs;                   /* Number of PVs */

    // set globals , defined in tool_lib global to default
    charArrAsStr = 0;
    if (caPriority > CA_PRIORITY_MAX) caPriority = CA_PRIORITY_MAX;

    //optind = 1;

    nPvs = 1;                   /* One PV - the rest is value(s) */
    //DBGprintf("caput_main: (%i,%s)\n",argc,argv[1]);
    epId = epicsEventCreate(epicsEventEmpty);  /* Create empty EPICS event (semaphore) */

                                /* Start up Channel Access */
    DBGprintf("caput_main: %s\n","<epicsEventCreate");
    result = ca_context_create(ca_enable_preemptive_callback);
    DBGprintf("caput_main: %s\n","<ca_context_create");
    if (result != ECA_NORMAL) {
        fprintf(stderr, "CA error %s occurred while trying "
                "to start channel access.\n", ca_message(result));
        return 1;
    }
                                /* Allocate PV structure array */
    //if(pvs) printf("PV %s exists, keep it\n",pvs[0].name);
    //else
    pvs = calloc (nPvs, sizeof(pv));
    if (!pvs) {
        fprintf(stderr, "Memory allocation for channel structure failed.\n");
        return 1;
    }
    DBGprintf("PV allocated at %lx\n",(unsigned long)pvs);
                                /* Connect channels */

    pvs[0].name = pvname ;   /* Copy PV name from command line */

    result = connect_pvs(pvs, nPvs); /* If the connection fails, we're done */
    if (result) {
        ca_context_destroy();
        return result;
    }
    sbuf = calloc (count, sizeof(EpicsStr));
    dbuf = calloc (count, sizeof(double));
    if(!sbuf || !dbuf) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    //optind = 2;
    //DBGprintf("count=%i, optind=%i\n",count,optind);//&RA
    if (charArrAsStr) {
    	count = len;
    	dbrType = DBR_CHAR;
    	ebuf = calloc(strlen(cbuf)+1, sizeof(char));
    	if(!ebuf) {
    		fprintf(stderr, "Memory allocation failed\n");
    		return 1;
    	}
    	epicsStrnRawFromEscaped(ebuf, strlen(cbuf)+1, cbuf, strlen(cbuf));
    } else {
    	//for (i = 0; i < count; ++i) {
    	//    epicsStrnRawFromEscaped(sbuf[i], sizeof(EpicsStr), *(argv+optind+i), sizeof(EpicsStr));
    	//	*( sbuf[i]+sizeof(EpicsStr)-1 ) = '\0';
    	//}
    	epicsStrnRawFromEscaped(sbuf[0], sizeof(EpicsStr), value, sizeof(EpicsStr));
    	    		*( sbuf[0]+sizeof(EpicsStr)-1 ) = '\0';
    	dbrType = DBR_STRING;
    }

    if (dbrType == DBR_STRING) pbuf = sbuf;
    else if (dbrType == DBR_CHAR) pbuf = ebuf;
    else pbuf = dbuf;
    DBGprintf("putting %s\n",(char*)pbuf);
    if (request == callback) {
        /* Use callback version of put */
        pvs[0].status = ECA_NORMAL;   /* All ok at the moment */
        result = ca_array_put_callback (
            dbrType, count, pvs[0].ch_id, pbuf, put_event_handler, (void *) pvs);
    } else {
        /* Use standard put with defined timeout */
        result = ca_array_put (dbrType, count, pvs[0].ch_id, pbuf);
    }
    result = ca_pend_io(caTimeout);
    if (result == ECA_TIMEOUT) {
        fprintf(stderr, "Write operation timed out: Data was not written.\n");
        return 1;
    }
    if (request == callback) {   /* Also wait for callbacks */
        waitStatus = epicsEventWaitWithTimeout( epId, caTimeout );
        if (waitStatus)
            fprintf(stderr, "Write callback operation timed out\n");

        /* retrieve status from callback */
        result = pvs[0].status;
    }

    if (result != ECA_NORMAL) {
        fprintf(stderr, "Error occurred writing data.\n");
        return 1;
    }
                                /* Shut down Channel Access */
    ca_context_destroy();
    DBGprintf("PV address at the end %lx\n",(unsigned long)pvs);
    DBGprintf("PV name %s\n",pvs[0].name);
    if(pvs[0].value) {free(pvs[0].value); pvs[0].value=NULL;}
    if(pvs) {free(pvs); pvs=NULL;}
    if(sbuf) {free(sbuf); sbuf=NULL;}
    if(dbuf) {free(dbuf); dbuf=NULL;}
    if(ebuf) {free(ebuf); ebuf=NULL;}
    return result;
}
int caput_number(const char* pvname, double value)
{
	EpicsStr str;
	snprintf(str,sizeof(str),"%g",value);
	DBGprintf("caput_number: caput(%s,%s)\n",pvname,str);
	return caput_string(pvname,str);
}
