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

/*<std-header orig-src='shore' incl-file-exclusion='SMTHREAD_H'>

 $Id: smthread.h,v 1.97.2.17 2010/03/25 18:05:17 nhall Exp $

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

#ifndef SMTHREAD_H
#define SMTHREAD_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/**\file smthread.h
 * \ingroup Macros
 */

#ifndef W_H
#include <w.h>
#endif
#ifndef SM_BASE_H
#include <sm_base.h>
#endif
#ifndef STHREAD_H
#include <sthread.h>
#endif
#include <w_bitvector.h>

/**\enum special_timeout_in_ms_t
 * \brief Special values for timeout_in_ms.
 * \details
 * - WAIT_FOREVER : no timeout
 * - WAIT_IMMEDIATE : do not block
 * - WAIT_SPECIFIED_BY_XCT : use the per-transaction custom timeout value
 * - WAIT_SPECIFIED_BY_THREAD : use the per-smthread custom timeout value
 */
enum special_timeout_in_ms_t {
    WAIT_FOREVER = sthread_t::WAIT_FOREVER,
    WAIT_IMMEDIATE = sthread_t::WAIT_IMMEDIATE,
    WAIT_SPECIFIED_BY_XCT = sthread_t::WAIT_SPECIFIED_BY_XCT,
    WAIT_SPECIFIED_BY_THREAD = sthread_t::WAIT_SPECIFIED_BY_THREAD
};

typedef sthread_t::timeout_in_ms timeout_in_ms;

class xct_t;
class xct_log_t;
class sdesc_cache_t;
class lockid_t;

#ifdef __GNUG__
#pragma interface
#endif

class smthread_t;

/**\brief Callback class use with smthread::for_each_smthread()
 * \details
 * Derive your per-smthread processing function (callback) from this.
 */
class SmthreadFunc {
public:
    virtual ~SmthreadFunc();
    
    virtual void operator()(const smthread_t& smthread) = 0;
};


/**\cond skip */
enum { FINGER_BITS=3 };
typedef w_bitvector_t<256>    sm_thread_map_t;
/**\endcond skip */

/**\brief Fingerprint for this smthread.
 * \details
 * Each smthread_t has a unique fingerprint. This is used by the
 * deadlock detector.  The fingerprint is a bitmap; each thread's
 * bitmap is unique, the deadlock detector ORs fingerprints together
 * to make a "digest" of the waits-for-map.
 * Rather than have fingerprints associated with transactions, we
 * associate them with threads.
 *
 * This class provides synchronization (protection) for updating the map.
 */
class  atomic_thread_map_t : public sm_thread_map_t {
private:
    mutable srwlock_t   _map_lock;
public:
    bool has_reader() const {
        return _map_lock.has_reader();
    }
    bool has_writer() const {
        return _map_lock.has_writer();
    }
    void lock_for_read() const {
        _map_lock.acquire_read();
    }
    void lock_for_write() {
        _map_lock.acquire_write();
    }
    void unlock_reader() const{
        w_assert2(_map_lock.has_reader());
        _map_lock.release_read();
    }
    void unlock_writer() {
        w_assert2(_map_lock.has_writer());
        _map_lock.release_write();
    }
    atomic_thread_map_t () {
        w_assert1(_map_lock.has_reader() == false);
        w_assert1(_map_lock.has_writer() == false);
    }
    ~atomic_thread_map_t () { 
        w_assert1(_map_lock.has_reader() == false);
        w_assert1(_map_lock.has_writer() == false);
    }

    atomic_thread_map_t &operator=(const atomic_thread_map_t &other) {
        // Copy only the bitmap portion; do not touch the
        // _map_lock
#if W_DEBUG_LEVEL > 0
        bool X=_map_lock.has_reader();
        bool Y=_map_lock.has_writer();
#endif
        copy(other);
#if W_DEBUG_LEVEL > 0
        w_assert1(_map_lock.has_reader() == X); 
        w_assert1(_map_lock.has_writer() == Y); 
#endif
        return *this;
    }
}; 


/**\cond skip */
typedef void st_proc_t(void*);

class sm_stats_info_t; // forward
/**\endcond skip */

/**\brief Storage Manager client thread.
 * \ingroup SSMINIT
 * \details
 * \attention
 * All threads that use storage manager functions must be of this type
 * or of a derived type.
 *
 * This is because the storage manager keeps its own thread-local state.
 */
class smthread_t : public sthread_t {
    friend class smthread_init_t;
    struct tcb_t {
        xct_t*   xct;
        int      pin_count;      // number of rsrc_m pins
        int      prev_pin_count; // previous # of rsrc_m pins
        timeout_in_ms lock_timeout;    // timeout to use for lock acquisitions
        bool    _in_sm;      // thread is in sm ss_m:: function
#ifdef ARCH_LP64
        /* XXX Really want kc_buf aligned to the alignment of the most
           restrictive type. It would be except sizeof above bool == 8,
           and timeout_in_ms is 4 bytes. */
        fill1            _fill1;        
        fill2            _fill2;        
#endif

        sdesc_cache_t     *_sdesc_cache;
        lockid_t          *_lock_hierarchy;
        xct_log_t*        _xct_log;
        sm_stats_info_t*  _TL_stats; // thread-local stats

        void    create_TL_stats();
        void    clear_TL_stats();
        void    destroy_TL_stats();
        inline sm_stats_info_t& TL_stats() { return *_TL_stats;}
        inline const sm_stats_info_t& TL_stats_const() const { 
                                                 return *_TL_stats; }

        tcb_t() : 
            xct(0), 
            pin_count(0), 
            prev_pin_count(0),
            lock_timeout(WAIT_FOREVER), // default for a thread
            _in_sm(false), 
            _sdesc_cache(0), 
            _lock_hierarchy(0), 
            _xct_log(0), 
            _TL_stats(0)
        { 
            create_TL_stats();
        }
        ~tcb_t() { destroy_TL_stats(); }
    };

    tcb_t              _tcb;
    st_proc_t* const   _proc;
    void* const        _arg;

    bool               _try_initialize_fingerprint(); // true: failure false: ok
    void               _initialize_fingerprint();
    void               _uninitialize_fingerprint();
    short              _fingerprint[FINGER_BITS]; // dreadlocks
    atomic_thread_map_t  _fingerprint_map; // map containing only fingerprint

public:
    const atomic_thread_map_t&  get_fingerprint_map() const
                            {   return _fingerprint_map; } 

public:

    /**\brief Normal constructor for a storage manager client.
     * \details
     * @param[in] f Stored in thread for client's convenience, may
     * be used in run() method.
     * @param[in] arg Stored in thread for client's convenience, may
     * be used in run() method.
     * @param[in] priority Required, but not used in storage manager.
     * @param[in] name Optional thread name, used for debugging.
     * @param[in] lockto Timeout for lock waiting.  See timeout_in_ms.
     * @param[in] stack_size Best to use default.
     */
    NORET            smthread_t(
        st_proc_t*             f, 
        void*                  arg,
        priority_t             priority = t_regular,
        const char*            name = 0, 
        timeout_in_ms          lockto = WAIT_FOREVER,
        unsigned               stack_size = default_stack);

    /**\brief Normal constructor for a storage manager client.
     * \details
     * @param[in] priority Required, but not used in storage manager.
     * @param[in] name Optional thread name, used for debugging.
     * @param[in] lockto Timeout for lock waiting.  See timeout_in_ms.
     * @param[in] stack_size Best to use default.
     */
    NORET            smthread_t(
        priority_t             priority = t_regular,
        const char*            name = 0,
        timeout_in_ms          lockto = WAIT_FOREVER,
        unsigned               stack_size = default_stack
        );

    // This is helpful for debugging and besides, it returns a w_rc_t
    // so there is an opportunity to check for things like
    // no xcts attached, etc. and deliver this info to the client.
    
    /**\brief  Returns when this thread ends.
     * @param[in] timeout Not used.
     * \details
     * Errors:
     * -ePINACTIVE: if the thread ended while holding a pinned record.
     * -eINTRANS: if the thread ended while attached to a transaction.
     */
    w_rc_t               join(timeout_in_ms timeout = WAIT_FOREVER);

    NORET                ~smthread_t();

    /**\cond skip */
    /* public for debugging */
    static void          init_fingerprint_map();
    /**\endcond skip */

    /**\brief Called before run() is called. */
    virtual void         before_run();

    /**\brief Main work routine. */
    virtual void         run() = 0;

    /**\brief Call when run finishes, before join() returns */
    virtual void         after_run();
    
    virtual smthread_t*          dynamic_cast_to_smthread();
    virtual const smthread_t*    dynamic_cast_to_const_smthread() const;

    /**\brief RTTI
     * \details
     * Run-time type info: Derived threads are expected to
     * add thread types and override thread_type()
     */
    enum SmThreadTypes     {smThreadType = 1, smLastThreadType};
    /**\brief RTTI
     * \details
     * Run-time type info: Derived threads are expected to
     * add thread types and override thread_type()
     */
    virtual int            thread_type() { return smThreadType; }

    /**\brief Iterator over all smthreads. Thread-safe and so use carefully.
     * \details
     * @param[in] f Callback function.
     * For each smthread, this calls the callback function f.
     * Because this grabs a lock on the list of all shore threads, 
     * whether or not they are smthreads, this prevents new threads
     * from starting and old ones from finishing, so don't use with
     * long-running functions.
     */
    static void            for_each_smthread(SmthreadFunc& f);
    
    /**\cond skip
     **\brief Attach this thread to the given transaction.
     * \ingroup SSMXCT
     * @param[in] x Transaction to attach to the thread
     * \details
     * Attach this thread to the transaction \e x or, equivalently,
     * attach \e x to this thread.
     * \note "this" thread need not be the running thread.
     *
     * Only one transaction may be attached to a thread at any time.
     * More than one thread may attach to a transaction concurrently.
     */
    void             attach_xct(xct_t* x);
    /**\brief Detach this thread from the given transaction.
     * \ingroup SSMXCT
     * @param[in] x Transaction to detach from the thread.
     * \details
     * Detach this thread from the transaction \e x or, equivalently,
     * detach \e x from this thread.
     * \note "this" thread need not be the running thread.
     *
     * If the transaction is not attached, returns error.
     * \endcond skip
     */
    void             detach_xct(xct_t* x);

    /// get lock_timeout value
    inline
    timeout_in_ms        lock_timeout() { 
                    return tcb().lock_timeout; 
                }
    /// set lock_timeout value
    inline 
    void             lock_timeout(timeout_in_ms i) { 
                    tcb().lock_timeout = i;
                }

    /// return xct this thread is running
    inline
    xct_t*             xct() { return tcb().xct; }

    /// return xct this thread is running
    inline
    xct_t*             xct() const { return tcb().xct; }

    /**\brief Return currently-running smthread. 
     * \details
     * \note Assumes all threads are smthreads
     */
    static smthread_t*         me() { return (smthread_t*) sthread_t::me(); }

    /// Return thread-local statistics collected for this thread.
    inline sm_stats_info_t& TL_stats() { 
                                       return tcb().TL_stats(); }

    /// Add thread-local stats into the given structure.
    void add_from_TL_stats(sm_stats_info_t &w) const;

    // NOTE: These macros don't have to be atomic since these thread stats
    // are stored in the smthread and collected when the smthread's tcb is
    // destroyed.
    
/**\def GET_TSTAT(x) me()->TL_stats().sm.x
 *\brief Get per-thread statistic named x
*/
#define GET_TSTAT(x) me()->TL_stats().sm.x

/**\def INC_TSTAT(x) me()->TL_stats().sm.x++
 *\brief Increment per-thread statistic named x by y
 */
#define INC_TSTAT(x) me()->TL_stats().sm.x++


/**\def INC_TSTAT_BFHT(x) me()->TL_stats().bfht.bf_htab_x++
 *\brief Increment per-thread statistic named x by y
 */
#define INC_TSTAT_BFHT(x) me()->TL_stats().bfht.bf_htab #x++

/**\def ADD_TSTAT(x,y) me()->TL_stats().sm.x += (y)
 *\brief Increment statistic named x by y
 */
#define ADD_TSTAT(x,y) me()->TL_stats().sm.x += (y)

/**\def SET_TSTAT(x,y) me()->TL_stats().sm.x = (y)
 *\brief Set per-thread statistic named x to y
 */
#define SET_TSTAT(x,y) me()->TL_stats().sm.x = (y)


    /**\cond skip */
    /*
     *  These functions are used to verify than nothing is
     *  left pinned accidentally.  Call mark_pin_count before an
     *  operation and check_pin_count after it with the expected
     *  number of pins that should not have been realeased.
     */
    void             mark_pin_count();
    void             check_pin_count(int change);
    void             check_actual_pin_count(int actual) ;
    void             incr_pin_count(int amount) ;
    int              pin_count() ;
   
    /*
     *  These functions are used to verify that a thread
     *  is only in one ss_m::, scan::, or pin:: function at a time.
     */
    inline
    void             in_sm(bool in)    { tcb()._in_sm = in; }
    inline 
    bool             is_in_sm() const { return tcb()._in_sm; }

    void             new_xct(xct_t *);
    void             no_xct(xct_t *);

    inline
    xct_log_t*       xct_log() { return tcb()._xct_log; }
    inline
    lockid_t *       lock_hierarchy() { return tcb()._lock_hierarchy; }

    inline
    sdesc_cache_t *  sdesc_cache() { return tcb()._sdesc_cache; }

    virtual void     _dump(ostream &) const; // to be over-ridden
    static int       collect(vtable_t&, bool names_too);
    virtual void     vtable_collect(vtable_row_t& t);
    static  void     vtable_collect_names(vtable_row_t& t);
    /**\endcond skip */

    /* TODO NANCY CLEAN UP THESE COMMENTS */
    /* thread-level block() and unblock aren't public or protected
       accessible.  Control sm thread-level blocking with ordinary
       synchronization tools at the sm level */
    w_rc_t::errcode_t smthread_block(timeout_in_ms WAIT_FOREVER,
                      const char * const caller = 0,
                      const void * id = 0);
    w_rc_t            smthread_unblock(w_rc_t::errcode_t e);

    /* block/unblock is used as an adhoc sync. method instead of
       using "guaranteed" synchronization.  Some places in the code
       which block/unblock may already have a mutex that locks
       the synchronization area.  This interface allows those locations
       to block(area_mutex) and get rid of the overhead associated
       with locking another mutex for the sm-level block */
private:
    w_rc_t::errcode_t _smthread_block( timeout_in_ms WAIT_FOREVER,
                              const char * const why =0);
    w_rc_t           _smthread_unblock(w_rc_t::errcode_t e);
public:
    void             prepare_to_block();

    /* functions to get/set whether this thread should generate log warnings */
    /* TODO NANCY: DOCUMENT */
    bool            generate_log_warnings() const { 
                            return _generate_log_warnings; }
    void            set_generate_log_warnings(bool b) { 
                            _generate_log_warnings=b; }
private:

    /* sm-specif block / unblock implementation */
    volatile bool   _unblocked;
    bool            _waiting;

    bool            _generate_log_warnings;

    inline
    tcb_t           &tcb() { return _tcb; }

    inline
    const tcb_t     &tcb() const { return _tcb; }
};

/**\cond skip */
class smthread_init_t {
public:
    NORET            smthread_init_t();
    NORET            ~smthread_init_t();
private:
    static int       count;
};
/**\endcond  skip */



/**\cond skip */

inline smthread_t* 
me() 
{ 
    return smthread_t::me(); 
}


inline xct_t* 
xct() 
{ 
    return me()->xct(); 
}


inline void 
smthread_t::mark_pin_count()
{    
    tcb().prev_pin_count = tcb().pin_count;
}

inline void 
smthread_t::check_pin_count(int W_IFDEBUG4(change)) 
{
#if W_DEBUG_LEVEL > 3
    int diff = tcb().pin_count - tcb().prev_pin_count;
    if (change >= 0) {
        w_assert4(diff <= change);
    } else {
        w_assert4(diff >= change);
    }
#endif 
}

inline void 
smthread_t::check_actual_pin_count(int W_IFDEBUG3(actual)) 
{
    w_assert3(tcb().pin_count == actual);
}


inline void 
smthread_t::incr_pin_count(int amount) 
{
    tcb().pin_count += amount; 
}

inline int 
smthread_t::pin_count() 
{
    return tcb().pin_count;
}

void
DumpBlockedThreads(ostream& o);

/*
 * redefine DBGTHRD to use our threads
 */
#ifdef DBGTHRD
#undef DBGTHRD
#endif
#define DBGTHRD(arg) DBG(<< " th." << smthread_t::me()->id << " " arg)
#ifdef W_TRACE
/* 
 * redefine FUNC to print the thread id
 */
#undef FUNC
#define FUNC(fn)\
  do { char const* fname = __func__; \
    DBGTHRD(<< fname);} while(0)
#endif /* W_TRACE */

/**\endcond skip */


/*<std-footer incl-file-exclusion='SMTHREAD_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
