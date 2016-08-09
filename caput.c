/*
 * Stripped down version of EPICS' caput.c.
 * Version 02 2016-08-03 by Andrei Sukhanov.
 * Version 03 2016-08-09 Support arrays. mods: caput(), caput_string(), caput_numbers()
 * Version 04 2016-08-09 The number of transferred values limited to number of elements
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
int caput(const char* pvname, int isNumeric, int nvalues, const void *values)
{
	DBGprintf("caput(%s,%i,%i)\n",pvname,isNumeric,nvalues);
    int result;                 /* CA result */
    RequestT request = get;     /* User specified request type */
    //request = callback;

    //not used//int isArray = 0;            /* Deselect array mode */
    //int i;
    int count = 1;
    chtype dbrType = DBR_STRING;
    EpicsStr *sbuf = NULL;
    double *dbuf = NULL;
    //char *cbuf = NULL;
    char *ebuf = NULL;
    void *pbuf = NULL;
    //int len = 0;
    int waitStatus;

    int nPvs;                   /* Number of PVs */

    // set globals , defined in tool_lib global to default
    //charArrAsStr = 0;
    if (caPriority > CA_PRIORITY_MAX) caPriority = CA_PRIORITY_MAX;

    nPvs = 1;                   /* One PV - the rest is value(s) */
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
	//&RA/does not work here/pvs[0].nElems = nvalues;

    result = connect_pvs(pvs, nPvs); /* If the connection fails, we're done */
    if (result) {
        ca_context_destroy();
        return result;
    }
    int nElements = ca_element_count(pvs[0].ch_id);
    if(nElements<nvalues) nvalues = nElements;
    DBGprintf("PV %s[%i] connected to provide %i values\n",pvs[0].name,nElements,nvalues);
    if(isNumeric) dbrType = DBR_DOUBLE;
    else
    {
    	sbuf = calloc (count, sizeof(EpicsStr));
    	// the count is not supplied properly, we have to handle it ourselves
    	dbuf = calloc (count, sizeof(double));
    	if(!sbuf || !dbuf) {
    		fprintf(stderr, "Memory allocation failed\n");
    		return 1;
    	}
    	/*if (charArrAsStr) {
    		count = len;
    		dbrType = DBR_CHAR;
    		ebuf = calloc(strlen(cbuf)+1, sizeof(char));
    		if(!ebuf) {
    			fprintf(stderr, "Memory allocation failed\n");
    			return 1;
    		}
    		epicsStrnRawFromEscaped(ebuf, strlen(cbuf)+1, cbuf, strlen(cbuf));
    	} else*/
    	{
    		//for (i = 0; i < count; ++i) {
    		//    epicsStrnRawFromEscaped(sbuf[i], sizeof(EpicsStr), *(argv+optind+i), sizeof(EpicsStr));
    		//	*( sbuf[i]+sizeof(EpicsStr)-1 ) = '\0';
    		//}
    		epicsStrnRawFromEscaped(sbuf[0], sizeof(EpicsStr), values, sizeof(EpicsStr));
    		*( sbuf[0]+sizeof(EpicsStr)-1 ) = '\0';
    		dbrType = DBR_STRING;
    	}
        if (dbrType == DBR_STRING) pbuf = sbuf;
        else if (dbrType == DBR_CHAR) pbuf = ebuf;
        else pbuf = dbuf;
    }

    if (request == callback) {
    	DBGprintf("ca_array_put_callback(type=%i, count=%i, val=%s\n",(int)dbrType,count,(char*)pbuf);
        /* Use callback version of put */
        pvs[0].status = ECA_NORMAL;   /* All ok at the moment */
        result = ca_array_put_callback (
            dbrType, count, pvs[0].ch_id, pbuf, put_event_handler, (void *) pvs);
    } else
    {

        /* Use standard put with defined timeout */
    	if(dbrType==DBR_DOUBLE)
    	{
    		//double *dptr = (double*)values;
    		//DBGprintf("ca_array_put(DBR_DOUBLE=%i, count=%i, val[0]=%g)\n",(int)dbrType,nvalues,dptr[0]);
    		DBGprintf("ca_array_put(DBR_DOUBLE=%i, count=%i, val[0]=%g)\n",(int)dbrType,nvalues,*((double*)values));
    		result = ca_array_put (dbrType, nvalues, pvs[0].ch_id, values);
    	}
    	else
    	{
    		DBGprintf("ca_array_put(type=%i, count=%i, val=%s)\n",(int)dbrType,nvalues,(char*)pbuf);
    		result = ca_array_put (dbrType, nvalues, pvs[0].ch_id, pbuf);
    	}
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
    if(pvs[0].value) {DBGprintf("%s\n","freeing pvs.value");free(pvs[0].value); pvs[0].value=NULL;}
    DBGprintf("%s\n","freeing the rest");
    if(pvs) {free(pvs); pvs=NULL;}
    if(sbuf) {free(sbuf); sbuf=NULL;}
    if(dbuf) {free(dbuf); dbuf=NULL;}
    if(ebuf) {free(ebuf); ebuf=NULL;}
    DBGprintf("%s\n","caput out");
    return result;
}
int caput_string(const char* pvname, const char* value)
{
	return caput(pvname,0,1,value);
}
int caput_numbers(const char* pvname, int nvalues, const double *values)
{
	DBGprintf("caput_numbers(%s,%i,%g)\n",pvname,nvalues,values[0]);
	return caput(pvname,1,nvalues,values);
}
