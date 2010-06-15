/*<std-header orig-src='shore' incl-file-exclusion='STHREAD_H'>

 $Id: smutex.h,v 1.1.2.2 2009/10/08 20:49:21 nhall Exp $

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

#ifndef SMUTEX_H
#define SMUTEX_H


#undef SMUTEX_SIMPLE

class scond_t;
/*
 *  Mutual Exclusion
 */
class smutex_t : public sthread_named_base_t {
	friend class scond_t; // to get access to its mutex
public:
    enum acquire_status {OPEN, OWNER, BLOCKED, ERROR};
    NORET               smutex_t(const char* name = 0);
    NORET               ~smutex_t();

    w_rc_t              acquire() { return _acquire(WAIT_FOREVER); }
    w_rc_t              attempt() { return _acquire(WAIT_IMMEDIATE); }
    w_rc_t              ensure_mine(acquire_status* status)
                          { return _ensure_mine(status, WAIT_FOREVER); }
    void                release();
    bool                is_mine() const;
    bool                is_locked();
    const char *        holder_name() const // use in debugger: not thread-safe
                          { return _holder ? _holder->name() : "NONE"; }; 

private:
    w_rc_t            _acquire(int4_t timeout);
    w_rc_t            _ensure_mine(acquire_status* status, int4_t timeout);
private:
    friend class latch_t;
    sthread_t*         _holder;    // owner of the mutex

#ifndef SMUTEX_SIMPLE
    sthread_priority_list_t  _waiters_list; // used by non-simple impl,
    mcs_lock             _spin_lock;
    int                  _blocking;
#endif
    pthread_mutex_t      _block_lock;

    // disabled
    NORET                smutex_t(const smutex_t&);
    smutex_t&            operator=(const smutex_t&);
};


inline bool
smutex_t::is_mine() const
{
#ifdef W_DEBUG
    /* Benign race -- the cs() is just here to keep race detection
     * tools happy. Writes to this value are protected by a mutex. If
     * I hold the lock this always returns true; if I don't hold the
     * lock it will never return true. I don't care *who* owns (or
     * doesn't own) the lock if it's not me.
     */
    CRITICAL_SECTION(cs, _spin_lock);
#endif
    return _holder == sthread_t::me();
}

inline bool
smutex_t::is_locked()
{
#ifdef W_DEBUG
    /* Benign race -- the cs() is just here to keep race detection
     * tools happy. Writes to this value are protected by a mutex. If
     * I hold the lock this always returns true; if I don't hold the
     * lock it will never return true. I don't care *who* owns (or
     * doesn't own) the lock if it's not me.
     */
    CRITICAL_SECTION(cs, _spin_lock);
#endif
    return _holder != 0;
}



#endif
