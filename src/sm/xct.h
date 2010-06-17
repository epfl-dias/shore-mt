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

/*<std-header orig-src='shore' incl-file-exclusion='XCT_H'>

 $Id: xct.h,v 1.144.2.15 2010/03/25 18:05:18 nhall Exp $

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

#ifndef XCT_H
#define XCT_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif

#if W_DEBUG_LEVEL > 4
// You can rebuild with this turned on 
// if you want comment log records inserted into the log
// to help with deciphering the log when recovery bugs
// are nasty.
#define  X_LOG_COMMENT_ON 1
#define  ADD_LOG_COMMENT_SIG ,const char *debugmsg
#define  ADD_LOG_COMMENT_USE ,debugmsg
#define  LOG_COMMENT_USE(x)  ,x

#else

#define  X_LOG_COMMENT_ON 0
#define  ADD_LOG_COMMENT_SIG
#define  ADD_LOG_COMMENT_USE
#define  LOG_COMMENT_USE(x) 
#endif

class xct_dependent_t;

/**\cond skip */
/**\internal Tells whether the log is on or off for this xct at this moment.
 * \details
 * This is used internally for turning on & off the log during 
 * top-level actions.
 */
class xct_log_t : public smlevel_1 {
private:
    //per-thread-per-xct info
    bool         _xct_log_off;
public:
    NORET        xct_log_t(): _xct_log_off(false) {};
    bool         xct_log_is_off() { return _xct_log_off; }
    void         set_xct_log_off() { _xct_log_off = true; }
    void         set_xct_log_on() { _xct_log_off = false; }
};
/**\endcond skip */

class lockid_t; // forward
class sdesc_cache_t; // forward
class xct_log_t; // forward
class xct_impl; // forward
class xct_i; // forward
class restart_m; // forward
class lock_m; // forward
class lock_core_m; // forward
class lock_request_t; // forward
class xct_log_switch_t; // forward
class xct_lock_info_t; // forward
class xct_prepare_alk_log; // forward
class xct_prepare_fi_log; // forward
class xct_prepare_lk_log; // forward
class sm_quark_t; // forward
class smthread_t; // forward

class logrec_t; // forward
class page_p; // forward

/**\brief A transaction. Internal to the storage manager.
 * \ingroup LOGSPACE
 *
 * This class may be used in a limited way for the handling of 
 * out-of-log-space conditions.  See \ref LOGSPACE.
 */
class xct_t : public smlevel_1 {

/**\cond skip */
    friend class xct_i;
    friend class xct_impl; 
    friend class smthread_t;
    friend class restart_m;
    friend class lock_m;
    friend class lock_core_m;
    friend class lock_request_t;
    friend class xct_log_switch_t;
    friend class xct_prepare_alk_log;
    friend class xct_prepare_fi_log; 
    friend class xct_prepare_lk_log; 
    friend class sm_quark_t; 

protected:
    enum commit_t { t_normal = 0, t_lazy = 1, t_chain = 2 };
/**\endcond skip */
public:
/**\cond skip */
    typedef xct_state_t         state_t;

public:
    NORET                        xct_t(
        sm_stats_info_t*             stats = 0,  // allocated by caller
        timeout_in_ms                timeout = WAIT_SPECIFIED_BY_THREAD);
    NORET                       xct_t(
        const tid_t&                 tid, 
        state_t                      s, 
        const lsn_t&                 last_lsn,
        const lsn_t&                 undo_nxt,
        timeout_in_ms                timeout = WAIT_SPECIFIED_BY_THREAD);
    // NORET                    xct_t(const logrec_t& r);
    NORET                       ~xct_t();

    friend ostream&             operator<<(ostream&, const xct_t&);

    static int                  collect(vtable_t&, bool names_too);
    void                        vtable_collect(vtable_row_t &);
    static void                 vtable_collect_names(vtable_row_t &);

    NORET                       operator bool() const {
                                    return state() != xct_stale && this != 0;
                                }
 
    state_t                     state() const;
    void                        set_timeout(timeout_in_ms t) ;
    inline
    timeout_in_ms               timeout_c() const { 
                                    return  _timeout; 
                                }

    /*  
     * for 2pc: internal, external
     */
public:
    void                         force_readonly();

    vote_t                       vote() const;
    bool                         is_extern2pc() const;
    rc_t                         enter2pc(const gtid_t &g);
    const gtid_t*                gtid() const;
    const server_handle_t&       get_coordinator()const; 
    void                         set_coordinator(const server_handle_t &); 
    static rc_t                  recover2pc(const gtid_t &g,
                                 bool mayblock, xct_t *&);  
    static rc_t                  query_prepared(int &numtids);
    static rc_t                  query_prepared(int numtids, gtid_t l[]);

    rc_t                         prepare();
    rc_t                         log_prepared(bool in_chkpt=false);

    /*
     * basic tx commands:
     */
    static void                 dump(ostream &o); 
    static int                  cleanup(bool dispose_prepared=false); 
                                 // returns # prepared txs not disposed-of


    bool                        is_instrumented() {
                                   return (__stats != 0);
                                }
    void                        give_stats(sm_stats_info_t* s) {
                                    w_assert1(__stats == 0);
                                    __stats = s;
                                }
    void                        clear_stats() {
                                    memset(__stats,0, sizeof(*__stats)); 
                                }
    sm_stats_info_t*            steal_stats() {
                                    sm_stats_info_t*s = __stats; 
                                    __stats = 0;
                                    return         s;
                                }
    const sm_stats_info_t&      const_stats_ref() { return *__stats; }
    rc_t                        commit(bool lazy = false, lsn_t* plastlsn=NULL);
    rc_t                        rollback(lsn_t save_pt);
    rc_t                        save_point(lsn_t& lsn);
    rc_t                        chain(bool lazy = false);
    rc_t                        abort(bool save_stats = false);

    // used by restart.cpp, some logrecs
protected:
    sm_stats_info_t&            stats_ref() { return *__stats; }
    rc_t                        dispose();
    void                        change_state(state_t new_state);
    void                        set_first_lsn(const lsn_t &) ;
    void                        set_last_lsn(const lsn_t &) ;
    void                        set_undo_nxt(const lsn_t &) ;

public:
/**\endcond skip */
    tid_t                       tid() const { return _tid; }

    // used by checkpoint, restart:
    const lsn_t&                last_lsn() const;
    const lsn_t&                first_lsn() const;
    const lsn_t&                undo_nxt() const;
    const logrec_t*             last_log() const;
    fileoff_t			get_log_space_used() const;
    rc_t			wait_for_log_space(fileoff_t amt);
    
    // used by restart, chkpt among others
    static xct_t*               look_up(const tid_t& tid);
    static tid_t                oldest_tid();        // with min tid value
    static tid_t                youngest_tid();        // with max tid value
/**\cond skip */
    static void                 update_youngest_tid(const tid_t &);
/**\endcond skip */

    // used by sm.cpp:
    static w_base_t::uint4_t    num_active_xcts();

/**\cond skip */
    // used for compensating (top-level actions)
    const lsn_t&                anchor(bool grabit = true);
    void                        release_anchor(bool compensate
			                       ADD_LOG_COMMENT_SIG
                                   );

    // -------------------------------------------------------------
    // start_crit and stop_crit are used by the io_m to
    // ensure that at most one thread of the attached transaction
    // enters the io_m at a time. That was the original idea; now it's
    // making sure that at most one thread that's in an sm update operation
    // enters the io_m at any time (allowing concurrent read-only activity). 
    //
    // start_crit grabs the xct's 1thread_log mutex if it doesn't
    // already hold it.  False means we don't actually grab the
    // anchor, so it's not really starting a top-level action.

    void                        start_crit() { (void) anchor(false); }
    // stop_crit frees the xct's 1thread_log mutex if the depth of
    // anchor()/release() calls reaches 0.
    // False means we don't compensate, 
    // so it's not really finishing a top-level action.

    void                        stop_crit() {(void) release_anchor(false
											LOG_COMMENT_USE( "stopcrit"));}
    // -------------------------------------------------------------
    
    void                        compensate(const lsn_t&, 
                                          bool undoable
										  ADD_LOG_COMMENT_SIG
                                          );
    // for recovery:
    void                        compensate_undo(const lsn_t&);
/**\endcond skip */

    // For handling log-space warnings
    // If you've warned wrt a tx once, and the server doesn't
    // choose to abort that victim, you don't want every
    // ssm prologue to warn thereafter. This allows the
    // callback function to turn off the warnings for the (non-)victim. 
    void                         log_warn_disable() { _warn_on = true; } 
    void                         log_warn_resume() { _warn_on = false; }
    bool                         log_warn_is_on() { return _warn_on; } 

/**\cond skip */

public:
    // used in sm.cpp
    rc_t                        add_dependent(xct_dependent_t* dependent);
    rc_t                        remove_dependent(xct_dependent_t* dependent);
    bool                        find_dependent(xct_dependent_t* dependent);

    //
    //        logging functions -- used in logstub_gen.cpp only, so it's inlined here:
    //
    bool                        is_log_on() const {
                                    return (me()->xct_log()->xct_log_is_off()
                                            == false);
                                }
    rc_t                        get_logbuf(logrec_t*&, const page_p *p = 0);
    rc_t                        give_logbuf(logrec_t*, const page_p *p = 0);

    //
    //        Used by I/O layer
    //
    void                        AddStoreToFree(const stid_t& stid);
    void                        AddLoadStore(const stid_t& stid);
    //        Used by vol.cpp
    void                        set_alloced() { }

    void                        num_extents_marked_for_deletion(
                                        base_stat_t &num) const;
public:

    //        For SM interface:
    void                        GetEscalationThresholds(
                                        w_base_t::int4_t &toPage, 
                                        w_base_t::int4_t &toStore, 
                                        w_base_t::int4_t &toVolume);
    void                        SetEscalationThresholds(
                                        w_base_t::int4_t toPage,
                                        w_base_t::int4_t toStore, 
                                        w_base_t::int4_t toVolume);
    bool                        set_lock_cache_enable(bool enable);
    bool                        lock_cache_enabled();

protected:
    /////////////////////////////////////////////////////////////////
    // the following is put here because smthread 
    // doesn't know about the structures
    // and we have changed these to be a per-thread structures.
    static lockid_t*            new_lock_hierarchy();
    static sdesc_cache_t*       new_sdesc_cache_t();
    static xct_log_t*           new_xct_log_t();
    void                        steal(lockid_t*&, sdesc_cache_t*&, xct_log_t*&);
    void                        stash(lockid_t*&, sdesc_cache_t*&, xct_log_t*&);

protected:
    void                        attach_thread(); 
    void                        detach_thread(); 


    // stored per-thread, used by lock.cpp
    lockid_t*                   lock_info_hierarchy() const {
                                    return me()->lock_hierarchy();
                                }
public:
    // stored per-thread, used by dir.cpp
    sdesc_cache_t*              sdesc_cache() const;

protected:
    // for xct_log_switch_t:
    /// Set {thread,xct} pair's log-state to on/off (s) and return the old value.
    switch_t                    set_log_state(switch_t s);
    /// Restore {thread,xct} pair's log-state to on/off (s) 
    void                        restore_log_state(switch_t s);


public:
    concurrency_t                get_lock_level(); // non-const: acquires mutex 
    void                         lock_level(concurrency_t l);

    int                          num_threads();          
    rc_t                         check_one_thread_attached() const;   
    void                         attach_update_thread() {
                                    w_assert2(_updating_operations >= 0);
                                    atomic_inc(_updating_operations);
                                 }
    void                         detach_update_thread() {
                                    atomic_dec(_updating_operations);
                                    w_assert2(_updating_operations >= 0);
                                 }
    int volatile                 update_threads() const { 
                                    return _updating_operations;
    } 

protected:
    // For use by lock manager:
    w_rc_t                        lockblock(timeout_in_ms timeout);// await other thread
    void                          lockunblock(); // inform other waiters
    const w_base_t::int4_t*       GetEscalationThresholdsArray();

    rc_t                          check_lock_totals(int nex, 
                                        int nix, int nsix, int ) const;
    rc_t                          obtain_locks(lock_mode_t mode, 
                                        int nlks, const lockid_t *l); 
    rc_t                          obtain_one_lock(lock_mode_t mode, 
                                        const lockid_t &l); 

    xct_lock_info_t*              lock_info() const { return _lock_info; }

public:
    // XXX this is only for chkpt::take().  This problem needs to
    // be fixed correctly.  DO NOT USE THIS.  Really want a
    // friend that is just a friend on some methods, not the entire class.
    static w_rc_t                acquire_xlist_mutex();
    static void                  release_xlist_mutex();
    static void                  assert_xlist_mutex_not_mine();
    static void                  assert_xlist_mutex_is_mine();
    static bool                  xlist_mutex_is_mine();

    /* "poisons" the transaction so cannot block on locks (or remain
       blocked if already so), instead aborting the offending lock
       request with eDEADLOCK. We use eDEADLOCK instead of
       eLOCKTIMEOUT because all transactions must expect the former
       and must abort in response; transactions which specified
       WAIT_FOREVER won't be expecting timeouts, and the SM uses
       timeouts (WAIT_IMMEDIATE) as internal signals which do not
       usually trigger a transaction abort.

       chkpt::take uses this to ensure timely and deadlock-free
       completion/termination of transactions which would prevent a
       checkpoint from freeing up needed log space.
     */
    void			force_nonblocking();


/////////////////////////////////////////////////////////////////
// DATA
/////////////////////////////////////////////////////////////////
protected:
    // list of all transactions instances
    w_link_t                      _xlink;
    static w_descend_list_t<xct_t, queue_based_lock_t, tid_t> _xlist;
    void                         put_in_order();
private:
    static queue_based_lock_t    _xlist_mutex;


private:
    sm_stats_info_t*             __stats; // allocated by user
    lockid_t*                    __saved_lockid_t;
    sdesc_cache_t*                __saved_sdesc_cache_t;
    xct_log_t*                   __saved_xct_log_t;

    static tid_t                 _nxt_tid;// only safe for pre-emptive threads on 64-bit platforms
    static tid_t                 _oldest_tid;

    const tid_t                  _tid;
    timeout_in_ms                _timeout; // default timeout value for lock reqs
    xct_impl *                   i_this;
    bool                         _warn_on;
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

    // Count of number of threads are doing update operations.
    // Used by start_crit and stop_crit.
    volatile int                 _updating_operations; 

public:
    void                         acquire_1thread_xct_mutex(); // serialize
    void                         release_1thread_xct_mutex(); // concurrency ok

/**\endcond skip */
};

/**\cond skip */
class auto_release_anchor_t {
private:
    xct_t* _xd;
    lsn_t _anchor;
    bool _and_compensate;
    int    _line; // for debugging

    operator lsn_t const&() const { return _anchor; }
public:

    auto_release_anchor_t(bool and_compensate, int line)
        : _xd(xct()), _and_compensate(and_compensate), _line(line)
    {
	if(_xd)
	    _anchor = _xd->anchor(_and_compensate);
    }
    void compensate() {
	if(_xd)
	    _xd->compensate(_anchor, false
				LOG_COMMENT_USE("auto_release_anchor_t")
				);
	_xd = 0; // cancel pending release in destructor
    }
    ~auto_release_anchor_t(); // in xct.cpp
};
/**\endcond skip */

/*
 * Use X_DO inside compensated operations
 */
#if X_LOG_COMMENT_ON
#define X_DO1(x,anchor,line)             \
{                           \
    w_rc_t __e = (x);       \
    if (__e.is_error()) {        \
        w_assert3(xct());        \
        W_COERCE(xct()->rollback(anchor));        \
        xct()->release_anchor(true, line );    \
        return RC_AUGMENT(__e); \
    } \
}
#define to_string(x) # x
#define X_DO(x,anchor) X_DO1(x,anchor, to_string(x))

#else

#define X_DO(x,anchor)             \
{                           \
    w_rc_t __e = (x);       \
    if (__e.is_error()) {        \
        w_assert3(xct());        \
        W_COERCE(xct()->rollback(anchor));        \
        xct()->release_anchor(true);        \
        return RC_AUGMENT(__e); \
    } \
}
#endif

/**\cond skip */
class xct_log_switch_t : public smlevel_0 {
    /*
     * NB: use sparingly!!!! EVERYTHING DONE UNDER
     * CONTROL OF ONE OF THESE IS A CRITICAL SECTION
     * This is necessary to support multi-threaded xcts,
     * to prevent one thread from turning off the log
     * while another needs it on, or vice versa.
     */
    switch_t old_state;
public:
    /// Initialize old state
    NORET xct_log_switch_t(switch_t s)  : old_state(OFF)
    {
        if(smlevel_1::log) {
            INC_TSTAT(log_switches);
            if (xct()) {
                old_state = xct()->set_log_state(s);
            }
        }
    }

    NORET
    ~xct_log_switch_t()  {
        if(smlevel_1::log) {
            if (xct()) {
                xct()->restore_log_state(old_state);
            }
        }
    }
};
/**\endcond skip */

/* XXXX This is somewhat hacky becuase I am working on cleaning
   up the xct_i xct iterator to provide various levels of consistency.
   Until then, the "locking option" provides enough variance so
   code need not be duplicated or have deep call graphs. */

/**\brief Iterator over transaction list.
 *
 * This is exposed for the purpose of coping with out-of-log-space 
 * conditions. See \ref LOGSPACE.
 */
class xct_i  {
public:
    // NB: still not safe, since this does not
    // lock down the list for the entire iteration.
    
    // FRJ: Making it safe -- all non-debug users lock it down
    // manually right now anyway; the rest *should* to avoid bugs.

    /// True if this thread holds the transaction list mutex.
    bool locked_by_me() const {
        if(xct_t::xlist_mutex_is_mine()) {
            W_IFDEBUG1(if(_may_check) w_assert1(_locked);)
            return true;
        }
        return false;
    }

    /// Release transaction list mutex if this thread holds it.
    void never_mind() {
        // Be careful here: must leave in the
        // state it was when we constructed this.
        if(_locked && locked_by_me()) {
            *(const_cast<bool *>(&_locked)) = false; // grot
            xct_t::release_xlist_mutex();
        }
    }
    /// Get transaction at cursor.
    xct_t* curr() const { return unsafe_iterator.curr(); }
    /// Advance cursor.
    xct_t* next() { return unsafe_iterator.next(); }

    /**\cond skip */
    // Note that this is called to INIT the attribute "locked"
    static bool init_locked(bool lockit) 
    {
        if(lockit) {
            W_COERCE(xct_t::acquire_xlist_mutex());
        }
        return lockit;
    }
    /**\endcond skip */

    /**\brief Constructor.
    *
    * @param[in] locked_accesses Set to true if you want this
    * iterator to be safe, false if you don't care or if you already
    * hold the transaction-list mutex.
    */
    NORET xct_i(bool locked_accesses)
        : _locked(init_locked(locked_accesses)),
        _may_check(locked_accesses),
        unsafe_iterator(xct_t::_xlist)
    {
        w_assert1(_locked == locked_accesses);
        _check(_locked);
    }

    /// Desctructor. Calls never_mind() if necessary.
    NORET ~xct_i() { 
        if(locked_by_me()) {
          _check(true);
          never_mind(); 
          _check(false);
        }
    }

private:
    void _check(bool b) const  {
          if(!_may_check) return;
          if(b) xct_t::assert_xlist_mutex_is_mine(); 
          else  xct_t::assert_xlist_mutex_not_mine(); 
    }
    // FRJ: make sure init_locked runs before we actually create the iterator
    const bool            _locked;
    const bool            _may_check;
    w_list_i<xct_t,queue_based_lock_t> unsafe_iterator;

    // disabled
    xct_i(const xct_i&);
    xct_i& operator=(const xct_i&);
};
    

/**\cond skip */
// For use in sm functions that don't allow
// active xct when entered.  These are functions that
// apply to local volumes only.
class xct_auto_abort_t : public smlevel_1 {
public:
    xct_auto_abort_t(xct_t* xct) : _xct(xct) {}
    ~xct_auto_abort_t() {
        switch(_xct->state()) {
        case smlevel_1::xct_ended:
            // do nothing
            break;
        case smlevel_1::xct_active:
            W_COERCE(_xct->abort());
            break;
        default:
            W_FATAL(eINTERNAL);
        }
    }
    rc_t commit() {
        // These are only for local txs
        // W_DO(_xct->prepare());
        W_DO(_xct->commit());
        return RCOK;
    }
    rc_t abort() {W_DO(_xct->abort()); return RCOK;}

private:
    xct_t*        _xct;
};
/**\endcond skip */

/*<std-footer incl-file-exclusion='XCT_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
