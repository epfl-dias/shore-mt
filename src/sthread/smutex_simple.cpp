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

// -*- mode:c++; c-basic-offset:4 -*-
/*<std-header orig-src='shore'>

 $Id: smutex_simple.cpp,v 1.1.2.1 2009/08/25 14:45:43 nhall Exp $

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
#include <w_debug.h>
#include <sthread.h>

#ifdef SMUTEX_SIMPLE
#include <w_statistics.h>
#include <sthread_stats.h>
enum {FAST_TO_CANCEL_ONE_SLOW=10, PATIENCE=10000,
      PTHREAD_THRESHOLD=20*FAST_TO_CANCEL_ONE_SLOW,
      LONG_PATH_NS=20000
};

/*********************************************************************
 *
 *  smutex_t::smutex_t(name)
 *
 *  Construct a mutex. Name is used for debugging.
 *
 *********************************************************************/
smutex_t::smutex_t(const char* nm)
    : _holder(0)
{
    pthread_mutex_init(&_block_lock, NULL);
    if (nm) rename(nm?"m:":0, nm);
}

/*********************************************************************
 *
 *  smutex_t::~smutex_t()
 *
 *  Destroy the mutex. There should not be any _holder.
 *
 *********************************************************************/
smutex_t::~smutex_t()
{
    w_assert1(!_holder);
}

// Return in status:  OWNER if I'm already the owner when this is called;
// BLOCKED if I had to block to get the mutex
// OPEN if I had did not have to block to get the mutex
w_rc_t smutex_t::_ensure_mine(acquire_status* status, int4_t timeout) 
{
    sthread_t* self = sthread_t::me();
    if(status) *status = ERROR; // just in case...

    if(_holder == self) {
        if(status) *status = OWNER;
    } else {
        int err = pthread_mutex_trylock(&_block_lock);
        if(err) {
            w_assert3(err == EBUSY);
            if(timeout == WAIT_IMMEDIATE) 
                return RC(stTIMEOUT);

            err = pthread_mutex_lock(&_block_lock);
            w_assert3(!err);
            if(status) *status = BLOCKED;
        }
        else {
            if(status) *status = OPEN;
        }
    }
    _holder = self;
    return RCOK;
}

/*********************************************************************
 *
 *  smutex_t::acquire(timeout)
 *
 *  Acquire the mutex. Block and wait for up to timeout milliseconds
 *  if some other thread is holding the mutex.
 *
 *********************************************************************/
w_rc_t
smutex_t::_acquire(int4_t timeout)
{
    sthread_t* self = sthread_t::me();
    w_assert1(_holder != self);
    
    int err = pthread_mutex_trylock(&_block_lock);
    if(err) {
		w_assert1(err == EBUSY);
		if(timeout == WAIT_IMMEDIATE)
			return RC(stTIMEOUT);
    
		err = pthread_mutex_lock(&_block_lock);
		w_assert3(!err);
    }
    _holder = self;
    return RCOK;
}


/*********************************************************************
 *
 *  smutex_t::release()
 *
 *  Release the mutex. If there are waiters, then wake up one.
 *
 *********************************************************************/
void
smutex_t::release()
{
#if W_DEBUG_LEVEL > 2
    sthread_t* self = sthread_t::me();
    w_assert3(_holder == self);
#endif
    _holder = NULL;
    int err = pthread_mutex_unlock(&_block_lock);
    w_assert1(!err);
    return;
}
    
#endif
