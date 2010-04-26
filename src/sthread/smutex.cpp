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

 $Id: smutex.cpp,v 1.1.2.5 2009/10/27 16:43:32 nhall Exp $

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

#ifndef SMUTEX_SIMPLE

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
    : _holder(0), _waiters_list(), _blocking(false)
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
    membar_exit();
    w_assert1(!_holder);
}

w_rc_t smutex_t::_ensure_mine(acquire_status* status, int4_t timeout) 
{
    
#ifdef THA_RACE
    /* Tell Sun's thread analyzer that this is a logical mutex */
    THA_NOTIFY_ACQUIRE_LOCK(this);
#endif
    if(status) *status = ERROR; // just in case...

    /* I already hold the lock
     */
    sthread_t* self = sthread_t::me();
    if (self == _holder)  {
        if(status)
            *status = OWNER;
        return RCOK;
    }

    w_rc_t ret;
    sthread_t* volatile* my_holder = &_holder;
    for(int i=0; *my_holder && i < PATIENCE; i++) ;

    CRITICAL_SECTION(cs, _spin_lock);
    // FRJ: List ops!
    if (! _holder) {
        /*
         *  No holder. Grab it.
         */
        w_assert3(waiters.num_members() == 0);
        _holder = self;
        if(status)
            *status = OPEN;
    } else {
        /*
         *  Some thread holding this mutex. Block and wait.
         */
        if(status)
            *status = BLOCKED;
        
        if (timeout == WAIT_IMMEDIATE)
            ret = RC(stTIMEOUT);
        else  {
            CRITICAL_SECTION(bcs, _block_lock);
            _blocking = true;
            cs.exit(); // don't need it any more
            DBGTHRD(<<"block on mutex " << this);
            INC_STH_STATS(mutex_wait);
            ret = sthread_t::block(bcs.hand_off(), timeout, &_waiters_list, name(), this);
        }
    }
#if defined(W_DEBUG) || defined(SHORE_TRACE)
    if (! ret.is_error()) 
    self->push_resource_alloc(name(), this);
#endif

#ifdef THA_RACE
    if(!ret.is_error()) {
        THA_NOTIFY_LOCK_ACQUIRED(this);
    }
#endif
    return ret;
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
    w_assert1(! is_mine());
#ifdef THA_RACE
    /* Tell Sun's thread analyzer that this is a logical mutex */
    THA_NOTIFY_ACQUIRE_LOCK(this);
#endif
    sthread_t* self = sthread_t::me();

    CRITICAL_SECTION(cs, _spin_lock);
    w_rc_t ret;
    if (! _holder) {
        /*
         *  No holder. Grab it.
         */
        w_assert3(waiters.num_members() == 0);
        _holder = self;
    } else {
        /*
         *  Some thread holding this mutex. Block and wait.
         */
        if (self == _holder)  {
            cerr << "fatal error -- deadlock while acquiring mutex";
            if (name()) {
            cerr << " (" << name() << ")";
            }
            cerr << endl;
            W_FATAL(stINTERNAL);
        }

        if (timeout == WAIT_IMMEDIATE)
            ret = RC(stTIMEOUT);
        else  {
            /* We're about to block, but don't want to miss a wakeup signal...*/
            CRITICAL_SECTION(bcs, _block_lock);
            _blocking = true;
#if defined(W_DEBUG) || defined(SHORE_TRACE)
            cs.pause();
#else
            cs.exit(); // don't need this any more
#endif
            DBGTHRD(<<"block on mutex " << this);
            INC_STH_STATS(mutex_wait);
            ret = sthread_t::block(bcs.hand_off(), timeout, &_waiters_list, name(), this);
#if defined(W_DEBUG) || defined(SHORE_TRACE)
            cs.resume();
#endif
        }
    }
#if defined(W_DEBUG) || defined(SHORE_TRACE)
    if (! ret.is_error())  {
        self->push_resource_alloc(name(), this);
    }
#endif

#ifdef THA_RACE
    if(!ret.is_error()) {
        THA_NOTIFY_LOCK_ACQUIRED(this);
    }
#endif
    w_assert1(is_mine());
    return ret;
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
    w_assert1(is_mine());
    THA_NOTIFY_RELEASE_LOCK(this);
    CRITICAL_SECTION(cs, _spin_lock);


#if defined(W_DEBUG) || defined(SHORE_TRACE)
    _holder->pop_resource_alloc(this);
#endif

    if(_waiters_list.is_empty() && !_blocking) {
        _holder = NULL;
    }
    else 
    {
        CRITICAL_SECTION(bcs, _block_lock);
        _blocking = false;

#ifdef THA_RACE
        CRITICAL_SECTION(tcs, global_list_lock);
#endif
        _holder=_waiters_list.first();
        cs.exit();
        if (_holder) {
            W_COERCE( _holder->unblock(RCOK) );
        }
    }
    THA_NOTIFY_LOCK_RELEASED(this);
    w_assert1(! is_mine());
}
    

#endif /* !SMUTEX_SIMPLE */
