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

 $Id: w_shore_alloc.cpp,v 1.7.2.2 2009/06/23 01:01:36 nhall Exp $

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

#include <w.h>
#include "w_shore_alloc.h"

/*
 * very large thread id means global/no thread:
 */
w_heapid_t w_no_shore_thread_id = (w_heapid_t) 0;

static int bytes_allocated=0;
static int allocations=0;
static int high_water_mark=0;

w_heapid_t
w_shore_count_alloc(
	size_t amt, 
	bool is_free// =false
)
{
    if(is_free) {
	bytes_allocated -= amt;
	allocations --;
    } else {
	bytes_allocated += amt;
	if(bytes_allocated > high_water_mark)
	allocations ++;
    }

    return w_no_shore_thread_id;
}


void 	
w_shore_alloc_stats( size_t& amt, size_t& allocations, size_t& hiwat)
{
    if(amt) {
	amt = bytes_allocated;
	allocations = allocations;
	hiwat = high_water_mark;
    }
}

w_thread_id_t 
w_id2int(w_heapid_t i)
{
    union {
        w_heapid_t i;
        w_thread_id_t ret;
    }u;
    u.i = i;
    w_assert1(sizeof(u.i)==sizeof(u.ret));
    return u.ret;
    // return (w_thread_id_t) i;
}

