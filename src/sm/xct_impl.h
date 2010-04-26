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

class xct_impl : public smlevel_1 
{
    friend class xct_t;
    friend class xct_log_switch_t;
    friend class restart_m;

    typedef xct_t::commit_t commit_t;
private:
    xct_t*    _that;
    tid_t&    nxt_tid() { return _that->_nxt_tid; }

public:
    typedef xct_t::state_t     state_t;

    NORET            xct_impl(
        xct_t*                that);
    NORET            xct_impl(
        xct_t*                that,
        state_t               s, 
        const lsn_t&          last_lsn,
        const lsn_t&          undo_nxt
    );
    // NORET         xct_impl(const logrec_t& r);
    NORET            ~xct_impl();


private:
    void            acquire_1thread_log_mutex();
#if GNATS_69_FIX
    rc_t            acquire_1thread_log_mutex_conditional();
#endif
    void            release_1thread_log_mutex();
    bool            is_1thread_log_mutex_mine() const;
    void            assert_1thread_log_mutex_free()const;
    void            acquire_1thread_xct_mutex();
    void            release_1thread_xct_mutex();
    bool            is_1thread_xct_mutex_mine() const;
    void            assert_1thread_xct_mutex_free()const;

private:
    vote_t          vote() const;

public:
    state_t         state() const;


    bool            is_extern2pc() const;
    rc_t            enter2pc(const gtid_t &g);
    const gtid_t*           gtid() const;
    const server_handle_t&     get_coordinator()const; 
    void             set_coordinator(const server_handle_t &); 

    rc_t            prepare();
    rc_t            log_prepared(bool in_chkpt=false);
    rc_t            commit(w_base_t::uint4_t flags);
    rc_t            rollback(lsn_t save_pt);
    rc_t            save_point(lsn_t& lsn);
    rc_t            abort();
    rc_t            dispose();

    rc_t            add_dependent(xct_dependent_t* dependent);
    rc_t            remove_dependent(xct_dependent_t* dependent);
    bool            find_dependent(xct_dependent_t* dependent);

protected:
    // for xct_log_switch_t:
    switch_t             set_log_state(switch_t s, bool &nested);
    void             restore_log_state(switch_t s, bool nested);

private:
    rc_t            check_one_thread_attached() const;   // returns rc_t
    bool            one_thread_attached() const;   // assertion
    // helper function for compensate() and compensate_undo()
    void             _compensate(const lsn_t&, bool undoable = false);

protected:
    void             detach_thread() ;
    void             attach_thread() ;

public:
    const lsn_t&     anchor(bool grabit = true);
    void             release_anchor(bool compensate=true);
    void             compensate(const lsn_t&, bool undoable = false);
    void             compensate_undo(const lsn_t&);

    NORET            operator bool() const;

    /*
     *    logging functions -- used in logstub_gen.cpp
     */
    rc_t            get_logbuf(logrec_t*&);
    void            give_logbuf(logrec_t*, const page_p *p = 0);
 private: // disabled for now
    // void            invalidate_logbuf();

    w_base_t::int4_t  escalationThresholds[lockid_t::NUMLEVELS-1];
 public:
    void            SetEscalationThresholds(w_base_t::int4_t toPage,
                       w_base_t::int4_t toStore,                             w_base_t::int4_t toVolume);
    void            SetDefaultEscalationThresholds();
    void            GetEscalationThresholds(w_base_t::int4_t &toPage, 
                        w_base_t::int4_t &toStore, 
                    w_base_t::int4_t &toVolume);
    const w_base_t::int4_t*    GetEscalationThresholdsArray();

    void            AddStoreToFree(const stid_t& stid);
    void            AddLoadStore(const stid_t& stid);
    
    void            ClearAllStoresToFree();
    void            FreeAllStoresToFree();

    rc_t            PrepareLogAllStoresToFree();
    void            DumpStoresToFree();
    rc_t            ConvertAllLoadStoresToRegularStores();
    void            ClearAllLoadStores();

    void             num_extents_marked_for_deletion(
                    base_stat_t &num);

 public:
    void            flush_logbuf();
    ostream &       dump_locks(ostream &) const;

    /////////////////////////////////////////////////////////////////

 public:
    concurrency_t   get_lock_level(); // non-const because it acquires mutex 
    void            lock_level(concurrency_t l);
private:
    /////////////////////////////////////////////////////////////////
    // non-const because it acquires mutex:
    // removed, now that the lock mgrs use the const,inline-d form
    // timeout_in_ms        timeout(); 

    // does not acquire the mutex :
    inline
    timeout_in_ms  timeout_c() const { return  _that->_timeout; }

    static void    xct_stats(
            u_long&             begins,
            u_long&             commits,
            u_long&             aborts,
            bool                 reset);

    friend ostream& operator<<(ostream&, const xct_impl&);

    tid_t           tid() const { return _that->tid(); }    

    void            force_readonly();
    bool            forced_readonly() const;

    void            change_state(state_t new_state);
    void            set_first_lsn(const lsn_t &) ;
    void            set_last_lsn(const lsn_t &) ;
    void            set_undo_nxt(const lsn_t &) ;

public:

    // used by checkpoint, restart:
    const lsn_t&        last_lsn() const;
    const lsn_t&        first_lsn() const;
    const lsn_t&        undo_nxt() const;
    const logrec_t*     last_log() const;

public:

    // NB: TO BE USED ONLY BY LOCK MANAGER 
    w_rc_t             lockblock(timeout_in_ms timeout);// await other 
                                           // thread's wake-up in lm
    void               lockunblock();      // inform other waiters
    int                num_threads() const { return _threads_attached; }

private:
    void               _flush_logbuf(bool sync=false);

private: // all data members private
                // to be manipulated only by smthread funcs
    volatile int       _threads_attached; 
                    
    timeout_in_ms     _timeout; // default timeout value for lock reqs
                      // duplicated in xct_t : TODO remove from xct_impl

    // the 1thread_xct mutex is used to ensure that only one thread
    // is using the xct structure on behalf of a transaction 
    // It protects a number of things, including the xct_dependents list
    mutable queue_based_lock_t   _1thread_xct;
    static const char* _1thread_xct_name; // name of xct mutex

    // used in lockblock, lockunblock, by lock_core 
    pthread_cond_t            _waiters_cond;  // paired with _waiters_mutex
    mutable pthread_mutex_t   _waiters_mutex;  // paired with _waiters_cond

    // the 1thread_log mutex is used to ensure that only one thread
    // is logging on behalf of this xct 
    mutable queue_based_lock_t   _1thread_log;
    static const char* _1thread_log_name; // name of log mutex

    state_t             _state;
    bool                _forced_readonly;
    vote_t              _vote;
    gtid_t *            _global_tid; // null if not participating
    server_handle_t*    _coord_handle; // ignored for now
    bool                _read_only;
    lsn_t               _first_lsn;
    lsn_t               _last_lsn;
    lsn_t               _undo_nxt;
    bool                _lock_cache_enable;

    // list of dependents: protected by _1thread_xct
    w_list_t<xct_dependent_t,queue_based_lock_t>    _dependent_list;

    /*
     *  lock request stuff
     */
    lockid_t::name_space_t    convert(concurrency_t cc);
    concurrency_t             convert(lockid_t::name_space_t n);

    xct_lock_info_t*          lock_info() const { return _that->_lock_info; }

    /*
     *  log_m related
     */
    logrec_t*          _last_log;    // last log generated by xct
    logrec_t*          _log_buf;
    /*
     * For figuring the amount of log bytes needed to roll back
     */
    u_int              _log_bytes_fwd; // bytes written during forward 
                            // progress
    u_int              _log_bytes_bwd; // bytes written during rollback


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


     volatile int            _in_compensated_op; 
        // in the midst of a compensated operation
        // use an int because they can be nested.
     lsn_t            _anchor;
        // the anchor for the outermost compensated op

     // for two-phase commit:
     time_t            _last_heard_from_coord;

     volatile int      _xct_ended; // used for self-checking (assertions) only

private:

    // disabled
    NORET            xct_impl(const xct_impl&);
    xct_impl&             operator=(const xct_impl&);
};


inline
xct_impl::state_t
xct_impl::state() const
{
    return _state;
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
    _storesToFree.push(new stid_list_elem_t(stid));
    release_1thread_xct_mutex();
}

inline void xct_impl::AddLoadStore(const stid_t& stid)
{
    acquire_1thread_xct_mutex();
    _loadStores.push(new stid_list_elem_t(stid));
    release_1thread_xct_mutex();
}

inline
vote_t
xct_impl::vote() const
{
    return _vote;
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
xct_impl::operator bool() const
{
    return _state != xct_stale && this != 0;
}

inline
void
xct_impl::force_readonly() 
{
    _forced_readonly = true;
}

inline
bool
xct_impl::forced_readonly() const
{
    return _forced_readonly;
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
    return _global_tid != 0;
}


inline
const gtid_t*           
xct_impl::gtid() const 
{
    return _global_tid;
}

/*<std-footer incl-file-exclusion='XCT_IMPL_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
