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

 $Id: scond.cpp,v 1.1.2.2 2009/10/19 15:12:36 nhall Exp $

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

#include <w_statistics.h>
#include <sthread_stats.h>

/*********************************************************************
 *
 *  scond_t::scond_t(name)
 *
 *  Construct a conditional variable. "Name" is used for debugging.
 *
 *********************************************************************/
scond_t::scond_t(const char* nm)
#ifndef USE_PTHREAD_COND_T
  : _waiters()
#endif
{
#ifdef USE_PTHREAD_COND_T
    int e = pthread_cond_init(&_cond, NULL);
	w_assert1(e==0);
#else
    pthread_mutex_init(&_lock, NULL);
#endif
    if (nm) rename(nm?"c:":0, nm);
}


/*
 *  scond_t::~scond_t()
 *
 *  Destroy a condition variable.
 */
scond_t::~scond_t()
{
#ifdef USE_PTHREAD_COND_T
	pthread_cond_destroy(&_cond);
#else
	pthread_mutex_destroy(&_lock);
#endif
}


/*
 *  scond_t::wait(mutex, timeout)
 *
 *  Wait for a condition. Current thread release mutex and wait
 *  up to timeout milliseconds for the condition to fire. When
 *  the thread wakes up, it re-acquires the mutex before returning.
 *
 */
w_rc_t
scond_t::wait(smutex_t& m, timeout_in_ms timeout)
{
    if (timeout == WAIT_IMMEDIATE)  {
		return RC(stTIMEOUT);
    }
#ifdef USE_PTHREAD_COND_T
	int e = pthread_cond_wait(&_cond, &(m._block_lock));
	w_assert1(e==0);
	return RCOK;
#else
    w_rc_t rc;
	{
		CRITICAL_SECTION(cs, _lock);
		m.release();
		rc = sthread_t::block(cs.hand_off(), timeout, &_waiters, name(), this);

		INC_STH_STATS(scond_wait);
		
	}
    
    // Reacquire m after the cs exits to avoid a->b vs b->a deadlock
    W_DO(m.acquire());
	return rc;
#endif
}

/*
 *  scond_t::signal()
 *
 *  Wake up one waiter of the condition variable.
 */
void scond_t::signal()
{
#ifdef USE_PTHREAD_COND_T
	 int e = pthread_cond_signal(&_cond);
	 w_assert1(e==0);
#else
    CRITICAL_SECTION(cs, _lock);
    signal(&_waiters);
#endif
}

void scond_t::signal(sthread_priority_list_t* 
#ifndef USE_PTHREAD_COND_T
			waiters
#endif
			) 
{
#ifdef USE_PTHREAD_COND_T
	w_assert1(0); // we don't support this
#else
#ifdef THA_RACE
    CRITICAL_SECTION(tcs, global_list_lock);
#endif
    sthread_t* t;
    t = waiters->first();
    if (t)  {
		W_COERCE( t->unblock(RCOK) );
    }
#endif
}


/*
 *  scond_t::broadcast()
 *
 *  Wake up all waiters of the condition variable.
 */
void scond_t::broadcast()
{
#ifdef USE_PTHREAD_COND_T
	 int e = pthread_cond_broadcast(&_cond);
	 w_assert1(e==0);
#else
    CRITICAL_SECTION(cs, _lock);
    broadcast(&_waiters);
#endif
}

void scond_t::broadcast(sthread_priority_list_t* 
#ifndef USE_PTHREAD_COND_T
			waiters
#endif
			) 
{
#ifdef USE_PTHREAD_COND_T
#else
#ifdef THA_RACE
    CRITICAL_SECTION(tcs, global_list_lock);
#endif
    sthread_t* t;
    while ((t = waiters->first()) != 0)  {
		W_COERCE( t->unblock(RCOK) );
    }
#endif
}


