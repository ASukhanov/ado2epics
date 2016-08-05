/*
 * Stripped down version of EPICS' tool_lib.h.
 * Version 02 2016-08-03 by Andrei Sukhanov.
 */

#ifndef INCLtool_libh
#define INCLtool_libh

#include <epicsTime.h>

#define DEFAULT_CA_PRIORITY 0  /* Default CA priority */

/* Structure representing one PV (= channel) */
typedef struct 
{
    const char* name;
    chid  ch_id;
    long  dbfType;
    long  dbrType;
    unsigned long nElems;       // True length of data in value
    unsigned long reqElems;     // Requested length of data
    int status;
    void* value;
    epicsTimeStamp tsPreviousC;
    epicsTimeStamp tsPreviousS;
    char firstStampPrinted;
    char onceConnected;
} pv;

extern int charArrAsStr;    /* used for -S option - treat char array as (long) string */
extern capri caPriority;    /* CA priority */
extern double caTimeout;    /* Wait time default (see -w option) */
extern int  connect_pvs (pv *pvs, int nPvs );

/*
 * no additions below this endif
 */
#endif /* ifndef INCLtool_libh */
