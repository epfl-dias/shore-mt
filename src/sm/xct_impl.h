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

/*<std-header orig-src='shore' incl-file-exclusion='XCT_IMPL_H'>

 $Id: xct_impl.h,v 1.25.2.14 2010/03/25 18:05:18 nhall Exp $

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

#ifndef XCT_IMPL_H
#define XCT_IMPL_H

#ifdef XCT_H
#error Must not include both xct.h and xct_impl.h
#endif

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif

class stid_list_elem_t  {
    public:
    stid_t        stid;
    w_link_t    _link;

    stid_list_elem_t(const stid_t& theStid)
        : stid(theStid)
        {};
    ~stid_list_elem_t()
    {
        if (_link.member_of() != NULL)
            _link.detach();
    }
    static w_base_t::uint4_t    link_offset()
    {
        return W_LIST_ARG(stid_list_elem_t, _link);
    }
};


#if 0
class xct_impl : public smlevel_1 
{
#else
#define xct_impl xct_t
    /* start with what was declared in xct.h and add in the
       implementation-jprivate member functions...
     */
#define XCT_IMPL_H_1
#include "xct.h"
#undef XCT_IMPL_H_1
#undef XCT_H
#endif
private:
    // TODO: GRoT! highly redundant and confusing, but less code-ripping required for the initial code change
    #define _that this

    friend class block_alloc<xct_t>;
private:
    void            acquire_1thread_log_mutex();
#if GNATS_69_FIX
    rc_t            acquire_1thread_log_mutex_conditional();
#endif
    void            release_1thread_log_mutex();
    bool            is_1thread_log_mutex_mine() const;
    void            assert_1thread_log_mutex_free()const;
    bool            is_1thread_xct_mutex_mine() const;
    void            assert_1thread_xct_mutex_free()const;

public:

    rc_t            _abort();
    rc_t            _commit(w_base_t::uint4_t flags,lsn_t* plastlsn=NULL);

protected:
    // for xct_log_switch_t:
    switch_t             set_log_state(switch_t s, bool &nested);
    void             restore_log_state(switch_t s, bool nested);

private:
    bool            one_thread_attached() const;   // assertion
    // helper function for compensate() and compensate_undo()
    void             _compensate(const lsn_t&, bool undoable = false);

 private: // disabled for now
    // void            invalidate_logbuf();

    w_base_t::int4_t  escalationThresholds[lockid_t::NUMLEVELS-1];
 public:
    void            SetDefaultEscalationThresholds();

    void            ClearAllStoresToFree();
    void            FreeAllStoresToFree();

    rc_t            PrepareLogAllStoresToFree();
    void            DumpStoresToFree();
    rc_t            ConvertAllLoadStoresToRegularStores();
    void            ClearAllLoadStores();

 public:
    ostream &       dump_locks(ostream &) const;

    /////////////////////////////////////////////////////////////////
private:
    /////////////////////////////////////////////////////////////////
    // non-const because it acquires mutex:
    // removed, now that the lock mgrs use the const,inline-d form
    // timeout_in_ms        timeout(); 

    static void    xct_stats(
            u_long&             begins,
            u_long&             commits,
            u_long&             aborts,
            bool                 reset);

    bool            forced_readonly() const;

    w_rc_t             _flush_logbuf(bool sync=false);
    w_rc_t	       _sync_logbuf(bool block=true);

    void 		_teardown(bool is_chaining);

public:
    /* A nearly-POD struct whose only job is to enable a N:1
       relationship between the log streams of a transaction (xct_t)
       and its core functionality such as locking and 2PC (xct_core).

       Any transaction state which should not eventually be replicated
       per-thread goes here. Usually such state is protected by the
       1-thread-xct-mutex.

       Static data members can stay in xct_t, since they're not even
       duplicated per-xct, let alone per-thread.
     */
    struct xct_core
    {
	xct_core(tid_t const &t, state_t s, timeout_in_ms timeout);
	~xct_core();

	//-- from xct.h ----------------------------------------------------
	tid_t                  _tid;
	timeout_in_ms                _timeout; // default timeout value for lock reqs
	xct_lock_info_t*             _lock_info;

	/* 
	 * _lock_cache_enable is protected by its own mutex, because
	 * it is used from the lock manager, and the lock mgr is used
	 * by the volume mgr, which necessarily holds the xct's 1thread_log
	 * mutex.  Thus, in order to avoid mutex-mutex deadlocks,
	 * we have a mutex to cover _lock_cache_enable that is used
	 * for NOTHING but reading and writing this datum.
	 */
	bool                          _lock_cache_enable;
	
	// the 1thread_xct mutex is used to ensure that only one thread
	// is using the xct structure on behalf of a transaction 
	// TBD whether this should be a spin- or block- lock:
	queue_based_lock_t              _1thread_xct;

	//-- from xct_impl.h -----------------------------------------------
	
	// to be manipulated only by smthread funcs
	volatile int       _threads_attached; 

	// used in lockblock, lockunblock, by lock_core 
	pthread_cond_t            _waiters_cond;  // paired with _waiters_mutex
	mutable pthread_mutex_t   _waiters_mutex;  // paired with _waiters_cond

	state_t             _state;
	bool                _forced_readonly;
	vote_t              _vote;
	gtid_t *            _global_tid; // null if not participating
	server_handle_t*    _coord_handle; // ignored for now
	bool                _read_only;

	/*
	 * List of stores which this xct will free after completion
	 * Protected by _1thread_xct.
	 */
	w_list_t<stid_list_elem_t,queue_based_lock_t>    _storesToFree;

	/*
	 * List of load stores:  converted to regular on xct commit,
	 *                act as a temp files during xct
	 */
	w_list_t<stid_list_elem_t,queue_based_lock_t>    _loadStores;

	// for two-phase commit:
	time_t            _last_heard_from_coord;

	volatile int      _xct_ended; // used for self-checking (assertions) only
    };
    
private: // all data members private
    // the 1thread_xct mutex is used to ensure that only one thread
    // is using the xct structure on behalf of a transaction 
    // It protects a number of things, including the xct_dependents list
    static const char* _1thread_xct_name; // name of xct mutex

#warning GRoT (whole point of xct streams)
    // the 1thread_log mutex is used to ensure that only one thread
    // is logging on behalf of this xct 
    mutable queue_based_lock_t   _1thread_log;
    static const char* _1thread_log_name; // name of log mutex

    lsn_t               _first_lsn;
    lsn_t               _last_lsn;
    lsn_t               _undo_nxt;

    // list of dependents: protected by _1thread_xct
    // FRJ: this will become per-stream and not need the mutex any more
    w_list_t<xct_dependent_t,queue_based_lock_t>    _dependent_list;

    /*
     *  lock request stuff
     */
    static 
    lockid_t::name_space_t    convert(concurrency_t cc);
    static
    concurrency_t             convert(lockid_t::name_space_t n);

    /*
     *  log_m related
     */
    logrec_t*          _last_log;    // last log generated by xct
    logrec_t*          _log_buf;

    /* track log space needed to avoid wedging the transaction in the
       event of an abort due to full log
     */ 
    fileoff_t		_log_bytes_rsvd; // reserved for rollback
    fileoff_t		_log_bytes_ready; // available for insert/reservations
    fileoff_t		_log_bytes_used; // total used by the xct
    bool		_rolling_back;


     volatile int            _in_compensated_op; 
        // in the midst of a compensated operation
        // use an int because they can be nested.
     lsn_t            _anchor;
        // the anchor for the outermost compensated op

    xct_core*		_core;

#if 0
};
#else
#define XCT_IMPL_H_2
#include "xct.h"
#undef XCT_IMPL_H_2
#endif

#ifdef XCT_IMPL_C
#undef inline
#define inline
#endif

inline
xct_impl::state_t
xct_impl::state() const
{
    return _core->_state;
}


inline
bool
operator>(const xct_t& x1, const xct_t& x2)
{
    return (x1.tid() > x2.tid());
}

inline void
xct_impl::SetEscalationThresholds(w_base_t::int4_t toPage, 
                w_base_t::int4_t toStore, 
                w_base_t::int4_t toVolume)
{
    if (toPage != dontModifyThreshold)
                escalationThresholds[2] = toPage;
    
    if (toStore != dontModifyThreshold)
                escalationThresholds[1] = toStore;
    
    if (toVolume != dontModifyThreshold)
                escalationThresholds[0] = toVolume;
}

inline void
xct_impl::SetDefaultEscalationThresholds()
{
    SetEscalationThresholds(smlevel_0::defaultLockEscalateToPageThreshold,
            smlevel_0::defaultLockEscalateToStoreThreshold,
            smlevel_0::defaultLockEscalateToVolumeThreshold);
}

inline void
xct_impl::GetEscalationThresholds(w_base_t::int4_t &toPage, 
                w_base_t::int4_t &toStore, 
                w_base_t::int4_t &toVolume)
{
    toPage = escalationThresholds[2];
    toStore = escalationThresholds[1];
    toVolume = escalationThresholds[0];
}

inline const w_base_t::int4_t *
xct_impl::GetEscalationThresholdsArray()
{
    return escalationThresholds;
}

inline void xct_impl::AddStoreToFree(const stid_t& stid)
{
    acquire_1thread_xct_mutex();
    _core->_storesToFree.push(new stid_list_elem_t(stid));
    release_1thread_xct_mutex();
}

inline void xct_impl::AddLoadStore(const stid_t& stid)
{
    acquire_1thread_xct_mutex();
    _core->_loadStores.push(new stid_list_elem_t(stid));
    release_1thread_xct_mutex();
}

inline
vote_t
xct_impl::vote() const
{
    return _core->_vote;
}

inline
const lsn_t&
xct_impl::last_lsn() const
{
    return _last_lsn;
}

inline
void
xct_impl::set_last_lsn( const lsn_t&l)
{
    _last_lsn = l;
}

inline
const lsn_t&
xct_impl::first_lsn() const
{
    return _first_lsn;
}

inline
void
xct_impl::set_first_lsn(const lsn_t &l) 
{
    _first_lsn = l;
}

inline
const lsn_t&
xct_impl::undo_nxt() const
{
    return _undo_nxt;
}

inline
void
xct_impl::set_undo_nxt(const lsn_t &l) 
{
    _undo_nxt = l;
}

inline
const logrec_t*
xct_impl::last_log() const
{
    return _last_log;
}

inline
bool
xct_impl::forced_readonly() const
{
    return _core->_forced_readonly;
}

/*********************************************************************
 *
 *  bool xct_impl::is_extern2pc()
 *
 *  return true iff this tx is participating
 *  in an external 2-phase commit protocol, 
 *  which is effected by calling enter2pc() on this
 *
 *********************************************************************/
inline bool            
xct_impl::is_extern2pc() 
const
{
    // true if is a thread of global tx
    return _core->_global_tid != 0;
}


inline
const gtid_t*           
xct_impl::gtid() const 
{
    return _core->_global_tid;
}

#ifdef XCT_IMPL_C
#undef inline
#define inline inline
#endif

/*<std-footer incl-file-exclusion='XCT_IMPL_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
