/*
 * Stripped down version of EPICS' tool_lib.c.
 * Version 02 2016-08-03 by Andrei Sukhanov.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cadef.h>

#include "tool_lib.h"

int charArrAsStr = 0;    /* used for -S option - treat char array as (long) string */
double caTimeout = 1.0;  /* wait time default (see -w option) */
capri caPriority = DEFAULT_CA_PRIORITY;  /* CA Priority */

/*+**************************************************************************
 *
 * Function:	create_pvs
 *
 * Description:	Creates an arbitrary number of PVs
 *
 * Arg(s) In:	pvs   -  Pointer to an array of pv structures
 *              nPvs  -  Number of elements in the pvs array
 *              pCB   -  Connection state change callback
 *
 * Arg(s) Out:	none
 *
 * Return(s):	Error code:
 *                  0  -  All PVs created
 *                  1  -  Some PV(s) not created
 *
 **************************************************************************-*/
 
int create_pvs (pv* pvs, int nPvs, caCh *pCB)
{
    int n;
    int result;
    int returncode = 0;
                                 /* Issue channel connections */
    for (n = 0; n < nPvs; n++) {
        result = ca_create_channel (pvs[n].name,
                                    pCB,
                                    &pvs[n],
                                    caPriority,
                                    &pvs[n].ch_id);
        if (result != ECA_NORMAL) {
            fprintf(stderr, "CA error %s occurred while trying "
                    "to create channel '%s'.\n", ca_message(result), pvs[n].name);
            pvs[n].status = result;
            returncode = 1;
        }
    }

    return returncode;
}
/*+**************************************************************************
 *
 * Function:	connect_pvs
 *
 * Description:	Connects an arbitrary number of PVs
 *
 * Arg(s) In:	pvs   -  Pointer to an array of pv structures
 *              nPvs  -  Number of elements in the pvs array
 *
 * Arg(s) Out:	none
 *
 * Return(s):	Error code:
 *                  0  -  All PVs connected
 *                  1  -  Some PV(s) not connected
 *
 **************************************************************************-*/
 
int connect_pvs (pv* pvs, int nPvs)
{
    int returncode = create_pvs ( pvs, nPvs, 0);
    if ( returncode == 0 ) {
                            /* Wait for channels to connect */
        int result = ca_pend_io (caTimeout);
        if (result == ECA_TIMEOUT)
        {
            if (nPvs > 1)
            {
                fprintf(stderr, "Channel connect timed out: some PV(s) not found.\n");
            } else {
                fprintf(stderr, "Channel connect timed out: '%s' not found.\n", 
                        pvs[0].name);
            }
            returncode = 1;
        }
    }
    return returncode;
}
