/*<std-header orig-src='shore' incl-file-exclusion='STHREAD_H'>

 $Id: scond.h,v 1.1.2.3 2009/09/15 20:02:03 nhall Exp $

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

/*  -- do not edit anything above this line --   </std-header>*/

/*
 *   NewThreads is Copyright 1992, 1993, 1994, 1995, 1996, 1997 by:
 *
 *    Josef Burger    <bolo@cs.wisc.edu>
 *    Dylan McNamee    <dylan@cse.ogi.edu>
 *      Ed Felten       <felten@cs.princeton.edu>
 *
 *   All Rights Reserved.
 *
 *   NewThreads may be freely used as long as credit is given
 *   to the above authors and the above copyright is maintained.
 */

/*
 * The base thread functionality of Shore Threads is derived
 * from the NewThreads implementation wrapped up as c++ objects.
 */

#ifndef SCOND_H
#define SCOND_H

// TODO: NANCY: when I turn this on, TM1 deadlocks - every
// thread awaits a condition variable
#undef USE_PTHREAD_COND_T

/*
 *  Condition Variable
 *  Note from Ryan:  they did not change all scond_t to pthread_cond_t
 *  because scond_t guarantees fifo ordering and some uses require that.
 *  On the other hand, scond_t is higher-overhead than pthread_cond_t
 *  so the latter was preferred when it could be used.
 *  NANCY TODO: verify all current uses of scond_t and see if they
 *  really require fifo ordering.    Then clean up this comment.
 *  The other potential benefit of the s* primitives
 *  is the resource tracing, but we will  probably get rid of that.
 *  If we get rid of scond_t, perhaps we can get rid of smutex_t also,
 *  since it's needed for scond_t uses, but other than that and resource
 *  tracing, doesn't have much to recommend it over pthread mutexes
 */
class scond_t : public sthread_named_base_t  
{
public:
    NORET            scond_t(const char* name = 0);
    NORET            ~scond_t();

    w_rc_t            wait(
		smutex_t*             m, 
		timeout_in_ms         timeout = WAIT_FOREVER) 
 	      { return wait(*m, timeout); }

    w_rc_t            wait(
		smutex_t&             m, 
		timeout_in_ms         timeout = WAIT_FOREVER);

    void             signal();
    static void      signal(sthread_priority_list_t* list);
    void             broadcast();
    static void      broadcast(sthread_priority_list_t* list);
    bool             is_hot() const;

#ifdef USE_PTHREAD_COND_T
private:
	pthread_cond_t              _cond;
	// moved the next 2 items to sevsem_t
#else
protected:
    sthread_priority_list_t    _waiters;
    pthread_mutex_t             _lock;
#endif

private:
    // disabled
    NORET            scond_t(const scond_t&);
    scond_t&         operator=(const scond_t&);
};


inline bool
scond_t::is_hot() const 
{
#ifdef USE_PTHREAD_COND_T
    return  false;
#else
    return ! _waiters.is_empty();
#endif
}

#endif
