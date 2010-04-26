#if DEAD
/* -*- mode:C++; c-basic-offset:4 -*-
     Shore-MT -- Multi-threaded port of the SHORE storage manager
   
                       Copyright (c) 2007-2009
      Data Intensive Applications and Systems Labaratory (DIAS)
               Ecole Polytechnique Federale de Lausanne
   
                         All Rights Reserved.
   
   Permission to use, copy, modify and distribute this software and
   its documentation is hereby granted, provided that both the
   copyright notice and this permission notice appear in all copies of
   the software, derivative works or modified versions, and any
   portions thereof, and that both notices appear in supporting
   documentation.
   
   This code is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. THE AUTHORS
   DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER
   RESULTING FROM THE USE OF THIS SOFTWARE.
*/


/*<std-header orig-src='shore'>

 $Id: alloc_sthread.cpp,v 1.6.2.4 2009/10/30 15:01:59 nhall Exp $

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


#include <cstdlib>
#include <w.h>
#include <sthread.h>
#include <w_debug.h>

/* 
 * Stuff to do per-thread heap management
 *
 * This might get moved into the sm layer by using
 * dynamic casting to smthreads, but for now, while
 * we figure out what really gives us useful info,
 * it remains here.
 */

void 			
sthread_t::per_thread_alloc_stats(
    size_t& amt, 
    size_t& allocations,
    size_t& hiwat
) const 
{
    if(amt) {
	amt = _bytes_allocated;
	allocations = _allocations;
	hiwat = _high_water_mark;
    }
}

w_heapid_t
sthread_t::per_thread_alloc(
    size_t amt, bool is_free
) 
{
    if(amt) {
	if(is_free) {
	    _bytes_allocated -= amt;
	    _allocations--;
	} else {
	    _bytes_allocated += amt;
	    if( _bytes_allocated > _high_water_mark) 
		_high_water_mark = _bytes_allocated;
	    _allocations++;
	}
    }
    union {
	w_heapid_t ret;
        sthread_t *ptr;
    } u;
    u.ptr = this;
    w_assert1(sizeof(u.ret)==sizeof(u.ptr));
    return u.ret;
    // return (w_heapid_t) this;
}

/* XXX race condition */
w_heapid_t w_no_shore_thread_id = (w_heapid_t) 0;
static int bytes_allocated=0;
static int allocations=0;
static int high_water_mark=0;
w_heapid_t
w_shore_thread_alloc(
    size_t amt, 
    bool is_free// =false
)
{
    static int count = 0;
    count++;
    sthread_t *t = sthread_t::me();
    if(t != (sthread_t *)0) {
	return t->per_thread_alloc(amt, is_free);
    } else if(amt) {
	if(is_free) {
	    bytes_allocated -= amt;
	    allocations--;
	} else {
	    bytes_allocated += amt;
	    if( bytes_allocated > high_water_mark) 
		high_water_mark = bytes_allocated;
	    allocations++;
	}
    }
    return w_no_shore_thread_id;
}

void 	
w_shore_alloc_stats( size_t& amt, size_t& allocs, size_t& hiwat)
{
    amt = bytes_allocated;
    allocs = allocations;
    hiwat = high_water_mark;
}

sthread_t::id_t
w_id2int(w_heapid_t i)
{
    sthread_t *t = (sthread_t *)i;
    if(t) return (sthread_t::id_t) t->id+1;
    else return  (sthread_t::id_t) 0;
}

#endif /*DEAD*/
