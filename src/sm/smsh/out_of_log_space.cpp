
/*

* Callback function that's called by SM on entry to
* __almost any__ SM call that occurs on behalf of a xact.  The
* callback is made IFF the amount of log space used exceeds the
* threshhold determined by the option sm_log_warn; this callback
* function chooses a victim xct and tells if the xct should be
* aborted by returning RC(eUSERABORT).  Any other RC is returned
* to the caller of the SM.    The arguments:
*  xct_i*  : ptr to iterator over all xcts 
*  xct_t *&: ref to ptr to xct : ptr to victim is returned here.
*  base_stat_t curr: current log used by active xcts
*  base_stat_t thresh: threshhold that was just exceeded
*  Function must be careful not to return the same victim more
*  than once, even though the callback may be called many 
*  times before the victim is completely aborted.
*/


/*
* This needs internal SM definitions, so fake it like
* you're an internal xct code module
*/
#define SM_LEVEL 1
#define SM_SOURCE
#define XCT_C
#include "sm_int_1.h"
#include "e_error_def_gen.h"

#include <sm_vas.h>

static pthread_mutex_t oven = PTHREAD_MUTEX_INITIALIZER;
static tid_t    pan;
int    ncalls = 0; // just for the heck of it.

extern class ss_m* sm;

w_rc_t out_of_log_space (xct_i* , xct_t *& xd,
    smlevel_0::fileoff_t curr,
    smlevel_0::fileoff_t thresh
)
{
	{
		fprintf(stderr, "Called out_of_log_space with curr %ld thresh %ld\n",
				curr, thresh);
		w_ostrstream o;
		static sm_stats_info_t curr;

		W_DO( sm->gather_stats(curr));

		o << curr << ends;
		fprintf(stderr, "stats: %s\n" , o.c_str()); 
	}
     w_rc_t    rc;
     xd = me()->xct();
     if(xd) {
         CRITICAL_SECTION(cs, oven);
         ncalls++;
         if(pan != tid_t::null) {
             xct_t* xx = xct_t::look_up(pan);
             if(!xx) {
            // no longer aborting
             cout << "No longer aborting tx " << pan 
                << " (ncalls = " << ncalls << ")" <<endl;
             pan = tid_t::null;
             ncalls = 0;
             }
         }
         if(pan == tid_t::null) {
             cout << " Curr=" << curr << " Thresh=" << thresh <<endl;
             cout << " Returning E_USERABORT for xct " << xd->tid() << endl;
             pan = xd->tid();
             rc = RC(E_USERABORT);
         } else {
             // don't do anything  - give aborting xct a chance
             // to run
             me()->yield();
             xd = 0;
         }
     }
     return rc;
}
