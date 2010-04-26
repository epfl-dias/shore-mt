#ifndef BF_HTAB_STATS_T_STAT_GEN_CPP
#define BF_HTAB_STATS_T_STAT_GEN_CPP

/* DO NOT EDIT --- GENERATED from bf_htab_stats.dat by stats.pl
		   on Mon Dec 14 19:52:59 2009

<std-header orig-src='shore' genfile='true'>

SHORE -- Scalable Heterogeneous Object REpository

Copyright (c) 1994-99 Computer Sciences Department, University of
                      Wisconsin -- Madison
All Rights Reserved.

Permission to use, copy, modify and distribute this software and its
documentation is hereby granted, provided that both the copyright
notice and this permission notice appear in all copies of the
software, derivative works or modified versions, and any portions
thereof, and that both notices appear in supporting documentation.

THE AUTHORS AND THE COMPUTER SCIENCES DEPARTMENT OF THE UNIVERSITY
OF WISCONSIN - MADISON ALLOW FREE USE OF THIS SOFTWARE IN ITS
"AS IS" CONDITION, AND THEY DISCLAIM ANY LIABILITY OF ANY KIND
FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.

This software was developed with support by the Advanced Research
Project Agency, ARPA order number 018 (formerly 8230), monitored by
the U.S. Army Research Laboratory under contract DAAB07-91-C-Q518.
Further funding for this work was provided by DARPA through
Rome Research Laboratory Contract No. F30602-97-2-0247.

*/

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/


w_statistics_t &
operator<<(w_statistics_t &s,const bf_htab_stats_t &t)
{
    w_rc_t e;
    e = s.add_module_static("Buffer manager hash table ",0xa0000,13,t.stat_names,t.stat_types,(&t._dummy_before_stats)+1);
    if(e.is_error()) {
        cerr <<  e << endl;
    }
    return s;
}
const
		char	*bf_htab_stats_t::stat_types =
"iidiiiidiiiii";

#endif /* BF_HTAB_STATS_T_STAT_GEN_CPP */
