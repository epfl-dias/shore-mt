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

 $Id: xct_impl.cpp,v 1.56.2.23 2010/03/25 18:05:18 nhall Exp $

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

#define SM_SOURCE
#define XCT_C
#define XCT_IMPL_C

#if W_DEBUG_LEVEL > 1
// for 
extern "C" void xstop() { }
#endif

#if W_DEBUG_LEVEL > 1
#define  X_LOG_COMMENT_ON 1
#endif

/* NB: OLD_LOG_FLUSH2  (now removed) are being
* used to phase out the old way of logging compensations.
* Once everything is working without these, we'll get rid
* of all these ifdefs; then we won't have each transaction
* keeping the last modified page pinned, and we'll have a
* much easier time eliminating latch-latch deadlocks,
* and latch/lock/mutex deadlocks
* When removing this stuff, we also need to remove:
*  the extra lsn (commented out) in logrec.h, _undo_nxt.
* We could also get rid of all code having to do with 
* undoable clr's. TODO NANCY REMOVE
* The next step is to put whatever we can from xct_t into
* the xct-thread structure, so that we can multi-thread the
* log.
*/

#ifdef __GNUG__
#pragma implementation "xct_impl.h"
#pragma implementation "xct_dependent.h"
#endif

#include <new>
#include "block_alloc.h"
#include <sm_int_4.h>
#include <sm.h>
#include "tls.h"

#include "xct_dependent.h"
#include "chkpt_serial.h"
#include "lock_x.h"
#include <sstream>

#include "crash.h"
#include "xct_impl.h"
#include "chkpt.h"

// definition of LOGTRACE is in crash.h
#define DBGX(arg) DBG(<<" th."<<me()->id << " " << "tid." << tid() << " "  arg)
#define DBGX2(arg) DBG(<<" th." << me()->id << " "  arg)


#ifdef W_TRACE
extern "C" void debugflags(const char *);
void
debugflags(const char *a) 
{
   _w_debug.setflags(a);
}
#endif /* W_TRACE */

#ifdef EXPLICIT_TEMPLATE
template class w_auto_delete_array_t<lockid_t>;
// template class w_auto_delete_array_t<lock_mode_t>; in xct.cpp
template class w_auto_delete_array_t<stid_t>;
template class w_list_t<xct_dependent_t,queue_based_lock_t>;
template class w_list_i<xct_dependent_t,queue_based_lock_t>;
#endif

/*********************************************************************
 *
 *  _1thread_name is the name of the mutex protecting the xct_t from
 *          multi-thread access
 * NB: there is a _1thread_xct_mutex for each of xct_impl and xct_t
 *
 *********************************************************************/
const char*                         xct_impl::_1thread_log_name = "1thLI";

/*********************************************************************
 *
 *  Print out tid and status
 *
 *********************************************************************/
ostream&
operator<<(ostream& o, const xct_impl& x)
{
    o << "tid="<< x._that->tid();

    o << " global_tid=";
    if (x._global_tid)  {
        o << *x._global_tid;
    }  else  {
        o << "<NONE>";
    }

    o << endl << " state=" << x.state() << " num_threads=" << x._threads_attached << endl << "   ";

    o << " defaultTimeout=";
    print_timeout(o, x.timeout_c());
    o << " first_lsn=" << x._first_lsn << " last_lsn=" << x._last_lsn << endl << "   ";

    o << " num_storesToFree=" << x._storesToFree.num_members()
      << " num_loadStores=" << x._loadStores.num_members() << endl << "   ";

    o << " in_compensated_op=" << x._in_compensated_op << " anchor=" << x._anchor;

    if(x.lock_info()) {
         o << *x.lock_info();
    }

    return o;
}

/**\var static __thread queue_based_lock_t::ext_qnode _1thread_xct_me;
 * \brief Queue node for holding mutex to serialize access to xct structure. 
 * \ingroup TLS
 */
static __thread queue_based_lock_t::ext_qnode _1thread_xct_me = EXT_QNODE_INITIALIZER;

/**\var static __thread queue_based_lock_t::ext_qnode _1thread_log_me;
 * \brief Queue node for holding mutex to serialize access to log.
 * \ingroup TLS
 */
static __thread queue_based_lock_t::ext_qnode _1thread_log_me = EXT_QNODE_INITIALIZER;

#define USE_BLOCK_ALLOC_FOR_LOGREC 1
#if USE_BLOCK_ALLOC_FOR_LOGREC 
DECLARE_TLS(block_alloc<logrec_t>, logrec_pool);
#endif

/*********************************************************************
 *
 *  xct_impl::xct_impl(that, type)
 *
 *  Begin a transaction. The transaction id is assigned automatically,
 *  and the xct record is inserted into _xlist.
 *
 *********************************************************************/
xct_impl::xct_impl(xct_t* that) 
    :   
        _that(that),
        _threads_attached(0),
        _state(xct_active),
        _forced_readonly(false),
        _vote(vote_bad), 
        _global_tid(0),
        _coord_handle(0),
        _read_only(false),
        // _first_lsn, _last_lsn, _undo_nxt, 
        _dependent_list(W_LIST_ARG(xct_dependent_t, _link), &_1thread_xct),
        _last_log(0),
        _log_buf(0),
        _log_bytes_rsvd(0),
        _log_bytes_ready(0),
	_rolling_back(false),
        _storesToFree(stid_list_elem_t::link_offset(), &_1thread_xct),
        _loadStores(stid_list_elem_t::link_offset(), &_1thread_xct),
        _in_compensated_op(0),
        _last_heard_from_coord(0),
        _xct_ended(0) // for assertions
{
#if USE_BLOCK_ALLOC_FOR_LOGREC 
    _log_buf = new (*logrec_pool) logrec_t; // deleted when xct goes away
#else
    _log_buf = new logrec_t; // deleted when xct goes away
#endif
    
#ifdef ZERO_INIT
    memset(_log_buf, '\0', sizeof(logrec_t));
#endif

    if (!_log_buf)  {
        W_FATAL(eOUTOFMEMORY);
    }

    SetDefaultEscalationThresholds();

    lock_info()->set_lock_level ( convert(cc_alg) );
    
    INC_TSTAT(begin_xct_cnt);

    DO_PTHREAD(pthread_mutex_init(&_waiters_mutex, NULL));
    DO_PTHREAD(pthread_cond_init(&_waiters_cond, NULL));
}


/*********************************************************************
 *
 *   xct_impl::xct_impl(state, last_lsn, undo_nxt)
 *
 *   Manually begin a specific transaction with a specific
 *   tid. This form mainly used by the restart recovery routines
 *
 *********************************************************************/
xct_impl::xct_impl(xct_t* that,
         state_t s, const lsn_t& last_lsn,
         const lsn_t& undo_nxt) 
    :    
        _that(that),
        _threads_attached(0),
        _state(s), 
        _forced_readonly(false),
        _vote(vote_bad), 
        _global_tid(0),
        _coord_handle(0),
        _read_only(false),
        // _first_lsn, 
        _last_lsn(last_lsn),
        _undo_nxt(undo_nxt),
        _dependent_list(W_LIST_ARG(xct_dependent_t, _link), &_1thread_xct),
        _last_log(0),
        _log_buf(0),
        _log_bytes_rsvd(0),
        _log_bytes_ready(0),
	_rolling_back(false),
        _storesToFree(stid_list_elem_t::link_offset(), &_1thread_xct),
        _loadStores(stid_list_elem_t::link_offset(), &_1thread_xct),
        _in_compensated_op(0),
        _last_heard_from_coord(0),
        _xct_ended(0) // for assertions
{

#if USE_BLOCK_ALLOC_FOR_LOGREC 
    _log_buf = new (*logrec_pool) logrec_t; // deleted when xct goes away
#else
    _log_buf = new logrec_t; // deleted when xct goes away
#endif
    if (!_log_buf)  W_FATAL(eOUTOFMEMORY);

    lock_info()->set_lock_level( convert(cc_alg) );

    SetDefaultEscalationThresholds();
    DO_PTHREAD(pthread_mutex_init(&_waiters_mutex, NULL));
    DO_PTHREAD(pthread_cond_init(&_waiters_cond, NULL));
}


/*********************************************************************
 *
 *  xct_impl::~xct_impl()
 *
 *  Clean up and free up memory used by the transaction. The 
 *  transaction has normally ended (committed or aborted) 
 *  when this routine is called.
 *
 *********************************************************************/
xct_impl::~xct_impl()
{
    if(long leftovers = _log_bytes_rsvd + _log_bytes_ready) {
	w_assert2(smlevel_0::log);
	smlevel_0::log->release_space(leftovers);
    };
	
    FUNC(xct_t::~xct_t);
    DBGX( << " ended" );

    w_assert3(_state == xct_ended);
    w_assert3(_in_compensated_op==0);

    if (shutdown_clean)  {
        w_assert1(me()->xct() == 0);
    }

    w_assert1(one_thread_attached());
    // Don't grab the mutex on destruction.
    // w_assert1(is_1thread_xct_mutex_mine());

    while (_dependent_list.pop()) ;

#if USE_BLOCK_ALLOC_FOR_LOGREC 
    logrec_pool->destroy_object(_log_buf);
#else
    delete _log_buf;
#endif
    delete _global_tid;
    delete _coord_handle;

    // clean up what's stored in the thread
    me()->no_xct(_that);

    _that = 0;

}


/*********************************************************************
 *
 *  xct_t::set_coordinator(...)
 *  xct_t::get_coordinator(...)
 *
 *  get and set the coordinator handle
 *  The handle is an opaque value that's
 *  logged in the prepare record.
 *
 *********************************************************************/
void
xct_impl::set_coordinator(const server_handle_t &h) 
{
    DBG(<<"set_coord for tid " << tid()
        << " handle is " << h);
    /*
     * Make a copy 
     */
    if(!_coord_handle) {
        _coord_handle = new server_handle_t; // deleted when xct goes way
        if(!_coord_handle) {
            W_FATAL(eOUTOFMEMORY);
        }
    }

    *_coord_handle = h;
}

const server_handle_t &
xct_impl::get_coordinator() const
{
    // caller can copy
    return *_coord_handle;
}

/*********************************************************************
 *
 *  xct_t::change_state(new_state)
 *
 *  Change the status of the transaction to new_state. All 
 *  dependents are informed of the change.
 *
 *********************************************************************/
void
xct_impl::change_state(state_t new_state)
{
    w_assert1(one_thread_attached());

    acquire_1thread_xct_mutex();
    w_assert1(is_1thread_xct_mutex_mine());

    w_assert2(_state != new_state);
    w_assert2((new_state > _state) || 
            (_state == xct_chaining && new_state == xct_active));

    state_t old_state = _state;
    _state = new_state;

    w_list_i<xct_dependent_t,queue_based_lock_t> i(_dependent_list);
    xct_dependent_t* d;
    while ((d = i.next()))  {
        d->xct_state_changed(old_state, new_state);
    }
    release_1thread_xct_mutex();
}


/*********************************************************************
 *
 *  xct_t::add_dependent(d)
 *  xct_t::remove_dependent(d)
 *
 *  Add a dependent to the dependent list of the transaction.
 *
 *********************************************************************/
rc_t
xct_impl::add_dependent(xct_dependent_t* dependent)
{
    // PROTECT
    acquire_1thread_xct_mutex();
    w_assert9(dependent->_link.member_of() == 0);
    
    w_assert1(is_1thread_xct_mutex_mine());
    _dependent_list.push(dependent);
    dependent->xct_state_changed(_state, _state);
    release_1thread_xct_mutex();
    return RCOK;
}
rc_t
xct_impl::remove_dependent(xct_dependent_t* dependent)
{
    // PROTECT (GNATS 99)
    acquire_1thread_xct_mutex();
    w_assert9(dependent->_link.member_of() != 0);
    
    w_assert1(is_1thread_xct_mutex_mine());
    dependent->_link.detach(); // is protected
    release_1thread_xct_mutex();
    return RCOK;
}

/*********************************************************************
 *
 *  xct_t::find_dependent(d)
 *
 *  Return true iff a given dependent(ptr) is in the transaction's
 *  list.   This must cleanly return false (rather than crashing) 
 *  if d is a garbage pointer, so it cannot dereference d
 *
 *  **** Used by value-added servers. ****
 *
 *********************************************************************/
bool
xct_impl::find_dependent(xct_dependent_t* ptr)
{
    // PROTECT
    xct_dependent_t        *d;
    acquire_1thread_xct_mutex();
    w_assert1(is_1thread_xct_mutex_mine());
    w_list_i<xct_dependent_t,queue_based_lock_t>    iter(_dependent_list);
    while((d=iter.next())) {
        if(d == ptr) {
            release_1thread_xct_mutex();
            return true;
        }
    }
    release_1thread_xct_mutex();
    return false;
}


/*********************************************************************
 *
 *  xct_t::prepare()
 *
 *  Enter prepare state. For 2 phase commit protocol.
 *  Set vote_abort or vote_commit if any
 *  updates were done, else return vote_readonly
 *
 *  We are called here if we are participating in external 2pc,
 *  OR we're just committing 
 *
 *  This does NOT do the commit
 *
 *********************************************************************/
rc_t 
xct_impl::prepare()
{
    // This is to be applied ONLY to the local thread.

    W_DO(check_one_thread_attached());

    if(lock_info() && lock_info()->in_quark_scope()) {
        return RC(eINQUARK);
    }

    // must convert all these stores before entering the prepared state
    // just as if we were committing.
    W_DO( ConvertAllLoadStoresToRegularStores() );

    w_assert1(_state == xct_active);

    // default unless all works ok
    _vote = vote_abort;

    _read_only = (_first_lsn == lsn_t::null);

    if(_read_only || forced_readonly()) {
        _vote = vote_readonly;
        // No need to log prepare record
#if W_DEBUG_LEVEL > 5
        // This is really a bogus assumption,
        // since a tx could have explicitly
        // forced an EX lock and then never
        // updated anything.  We'll leave it
        // in until we can run all the scripts.
        // The question is: should the lock 
        // be held until the tx is resolved,
        // even though no updates were done???
        // 
        // THIS IS A SIZZLING QUESTION.  Needs to be resolved
        if(!forced_readonly()) {
            int        total_EX, total_IX, total_SIX, num_extents;
            W_DO(lock_info()->get_lock_totals(total_EX, total_IX, total_SIX, num_extents));
            if(total_EX != 0) {
                cout 
                   << "WARNING: " << total_EX 
                   << " write locks held by a read-only transaction thread. "
                   << " ****** voting read-only ***** "
                   << endl;
             }
            // w_assert9(total_EX == 0);
        }
#endif 
        // If commit is done in the readonly case,
        // it's done by ss_m::_prepare_xct(), NOT HERE

        change_state(xct_prepared);
        // Let the stat indicate how many prepare records were
        // logged
        INC_TSTAT(s_prepared);
        return RCOK;
    }

    ///////////////////////////////////////////////////////////
    // NOT read only
    ///////////////////////////////////////////////////////////

    if(is_extern2pc() ) {
        DBG(<<"logging prepare because e2pc=" << is_extern2pc());
        W_DO(log_prepared());

    } else {
        // Not distributed -- no need to log prepare
    }

    /******************************************
    // Don't set the state until after the
    // log records are written, so that
    // if a checkpoint is going on right now,
    // it'll not log this tx in the checkpoint
    // while we are logging it.
    ******************************************/

    change_state(xct_prepared);
    INC_TSTAT(prepare_xct_cnt);

    _vote = vote_commit;
    return RCOK;
}

/*********************************************************************
 * xct_t::log_prepared(bool in_chkpt)
 *  
 * log a prepared tx 
 * (fuzzy, so to speak -- can be intermixed with other records)
 * 
 * 
 * called from xct_t::prepare() when tx requests prepare,
 * also called during checkpoint, to checkpoint already prepared
 * transactions
 * When called from checkpoint, the argument should be true, false o.w.
 *
 *********************************************************************/

rc_t
xct_impl::log_prepared(bool in_chkpt)
{
    FUNC(xct_t::log_prepared);
    w_assert1(_state == (in_chkpt?xct_prepared:xct_active));

    w_rc_t rc;

    if( !in_chkpt)  {
        // grab the mutex that serializes prepares & chkpts
        chkpt_serial_m::trx_acquire();
    }


    SSMTEST("prepare.unfinished.0");

    int total_EX, total_IX, total_SIX, num_extents;
    if( ! _coord_handle ) {
                return RC(eNOHANDLE);
    }
    rc = log_xct_prepare_st(_global_tid, *_coord_handle);
    if (rc.is_error()) { RC_AUGMENT(rc); goto done; }

    SSMTEST("prepare.unfinished.1");
    {

    rc = lock_info()->
            get_lock_totals(total_EX, total_IX, total_SIX, num_extents);
    if (rc.is_error()) { RC_AUGMENT(rc); goto done; }

    /*
     * We will not get here if this is a read-only
     * transaction -- according to _read_only, above
    */

    /*
     *  Figure out how to package the locks
     *  If they all fit in one record, do that.
     *  If there are lots of some kind of lock (most
     *  likely EX in that case), split those off and
     *  write them in a record of uniform lock mode.
     */  
    int i;

    /*
     * NB: for now, we assume that ONLY the EX locks
     * have to be acquired, and all the rest are 
     * acquired by virtue of the hierarchy.
     * ***************** except for extent locks -- they aren't
     * ***************** in the hierarchy.
     *
     * If this changes (e.g. any code uses dir_m::access(..,.., true)
     * for some non-EX lock mode, we have to figure out how
     * to locate those locks *not* acquired by virtue of an
     * EX lock acquisition, and log those too.
     *
     * We have the mechanism here to do the logging,
     * but we don't have the mechanism to separate the
     * extraneous IX locks, say, from those that DO have
     * to be logged.
     */

    i = total_EX;

    if (i < prepare_lock_t::max_locks_logged)  {
            /*
            // EX ONLY
            // we can fit them *all* in one record
            */
            lockid_t* space_l = new lockid_t[i]; // auto-del
            w_auto_delete_array_t<lockid_t> auto_del_l(space_l);

            rc = lock_info()-> get_locks(EX, i, space_l);
            if (rc.is_error()) { RC_AUGMENT(rc); goto done; }

            SSMTEST("prepare.unfinished.2");

            rc = log_xct_prepare_lk( i, EX, space_l);
            if (rc.is_error()) { RC_AUGMENT(rc); goto done; }
    }  else  {
            // can't fit them *all* in one record
            // so first we log the EX locks only,
            // the rest in one or more records

            /* EX only */
            lockid_t* space_l = new lockid_t[i]; // auto-del
            w_auto_delete_array_t<lockid_t> auto_del_l(space_l);

            rc = lock_info()-> get_locks(EX, i, space_l);
            if (rc.is_error()) { RC_AUGMENT(rc); goto done; }

            // Use as many records as needed for EX locks:
            //
            // i = number to be recorded in next log record
            // j = number left to be recorded altogether
            // k = offset into space_l array
            //
            i = prepare_lock_t::max_locks_logged;
            int j=total_EX, k=0;
            while(i < total_EX) {
                    rc = log_xct_prepare_lk(prepare_lock_t::max_locks_logged, EX, &space_l[k]);
                    if (rc.is_error()) { RC_AUGMENT(rc); goto done; }
                    i += prepare_lock_t::max_locks_logged;
                    k += prepare_lock_t::max_locks_logged;
                    j -= prepare_lock_t::max_locks_logged;
            }
            SSMTEST("prepare.unfinished.3");
            // log what's left of the EX locks (that's in j)

            rc = log_xct_prepare_lk(j, EX, &space_l[k]);
            if (rc.is_error()) { RC_AUGMENT(rc); goto done; }
    }

    {
            /* Now log the extent locks */
            i = num_extents;
            lockid_t* space_l = new lockid_t[i]; // auto-del
            lock_mode_t* space_m = new lock_mode_t[i]; // auto-del

            w_auto_delete_array_t<lockid_t> auto_del_l(space_l);
            w_auto_delete_array_t<lock_mode_t> auto_del_m(space_m);

            rc = lock_info()-> get_locks(NL, i, space_l, space_m, true);
            if (rc.is_error()) { RC_AUGMENT(rc); goto done; }

            SSMTEST("prepare.unfinished.4");
            int limit = prepare_all_lock_t::max_locks_logged;
            int k=0;
            while (i >= limit)  {
            rc = log_xct_prepare_alk(limit, &space_l[k], &space_m[k]);
            if (rc.is_error())  {
                    RC_AUGMENT(rc);
                    goto done;
            }
            i -= limit;
            k += limit;
            }
            if (i > 0)  {
            rc = log_xct_prepare_alk(i, &space_l[k], &space_m[k]);
            if (rc.is_error())  {
                    RC_AUGMENT(rc);
                    goto done;
            }
            }
    }
    }

    W_DO( PrepareLogAllStoresToFree() );

    SSMTEST("prepare.unfinished.5");

    rc = log_xct_prepare_fi(total_EX, total_IX, total_SIX, num_extents,
        this->first_lsn());
    if (rc.is_error()) { RC_AUGMENT(rc); goto done; }

done:
    // We have to force the log record to the log
    // If we're not in a chkpt, we also have to make
    // it durable
    if( !in_chkpt)  {
	_sync_logbuf();

        // free the mutex that serializes prepares & chkpts
        chkpt_serial_m::trx_release();
    }
    return rc;
}

/*********************************************************************
 *
 *  xct_t::commit(flags)
 *
 *  Commit the transaction. If flag t_lazy, log is not synced.
 *  If flag t_chain, a new transaction is instantiated inside
 *  this one, and inherits all its locks.
 *
 *********************************************************************/
rc_t
xct_impl::commit(uint4_t flags)
{
    W_DO(check_one_thread_attached());

    if(is_extern2pc()) {
        w_assert1(_state == xct_prepared);
    } else {
        w_assert1(_state == xct_active || _state == xct_prepared);
    };

    if(lock_info() && lock_info()->in_quark_scope()) {
        return RC(eINQUARK);
    }

    w_assert1(1 == atomic_add_nv(_xct_ended, 1));

    W_DO( ConvertAllLoadStoresToRegularStores() );

    SSMTEST("commit.1");
    
    change_state(flags & xct_t::t_chain ? xct_chaining : xct_committing);

    SSMTEST("commit.2");

    if (_last_lsn.valid() || !smlevel_1::log)  {
        /*
         *  If xct generated some log, write a synchronous
         *  Xct End Record.
         *  Do this if logging is turned off. If it's turned off,
         *  we won't have a _last_lsn, but we still have to do 
         *  some work here to complete the tx; in particular, we
         *  have to destroy files...
         * 
         *  Logging a commit must be serialized with logging
         *  prepares (done by chkpt).
         */
        
        // wait for the checkpoint to finish
	chkpt_serial_m::trx_acquire();
        W_DO(check_one_thread_attached());

        // don't allow a chkpt to occur between changing the state and writing
        // the log record, since otherwise it might try to change the state
        // to the current state (which causes an assertion failure).

        change_state(xct_freeing_space);
        rc_t rc = log_xct_freeing_space();
        SSMTEST("commit.3");
        chkpt_serial_m::trx_release();

        W_DO(rc);

        if (!(flags & xct_t::t_lazy) /* && !_read_only */)  {
            _sync_logbuf();
            // W_COERCE(log->flush_all());        /* sync log */
        }
#if X_LOG_COMMENT_ON
        {
            w_ostrstream s;
            s << "FreeAllStores... ";
            W_DO(log_comment(s.c_str()));
        }
#endif

        /*
         *  Do the actual destruction of all stores which were requested
         *  to be destroyed by this xct.
         */
        FreeAllStoresToFree();


        /*
         *  Free all locks. Do not free locks if chaining.
         */
        if (! (flags & xct_t::t_chain))  {
            // clear cached store references if the locks
            // are going away, because the removal of the locks
            // may allow the objects the cached entries point
            // at to become invalid
            if (_that->sdesc_cache())
                _that->sdesc_cache()->remove_all();

#if X_LOG_COMMENT_ON
            {
                w_ostrstream s;
                s << "unlock_duration, frees extents ";
                W_DO(log_comment(s.c_str()));
            }
#endif
            W_COERCE( lm->unlock_duration(t_long, true, false) );
        }

        // don't allow a chkpt to occur between changing the state and writing
        // the log record, since otherwise it might try to change the state
        // to the current state (which causes an assertion failure).

        chkpt_serial_m::trx_acquire();
        change_state(xct_ended);
        rc = log_xct_end();
        chkpt_serial_m::trx_release();

        W_DO(rc);
    }  else  {
        change_state(xct_ended);

        /*
         *  Free all locks. Do not free locks if chaining.
         *  Don't free exts as there shouldn't be any to free.
         */
        if (! (flags & xct_t::t_chain))  {
            // clear cached store references if the locks
            // are going away, because the removal of the locks
            // may allow the objects the cached entries point
            // at to become invalid
            if (_that->sdesc_cache())
                _that->sdesc_cache()->remove_all();

            W_COERCE( lm->unlock_duration(t_long, true, true) );
        }
    }

    me()->detach_xct(_that);        // no transaction for this thread
    INC_TSTAT(commit_xct_cnt);


    /*
     *  Xct is now committed
     */

    if (flags & xct_t::t_chain)  {
#if X_LOG_COMMENT_ON
        {
            w_ostrstream s;
            s << "chaining... ";
            W_DO(log_comment(s.c_str()));
        }
#endif
        /*
         *  Start a new xct in place
         *
         * FRJ: This should really be encapsulated in xct.cpp somehow
         * -- it was way out of sync with the rest of the world!
         */
	if(long leftovers = _log_bytes_rsvd + _log_bytes_ready) {
	    w_assert2(smlevel_0::log);
	    smlevel_0::log->release_space(leftovers);
	};
	_log_bytes_rsvd = _log_bytes_ready = 0;
	
        W_COERCE(_that->acquire_xlist_mutex());
        _that->_xlink.detach();
        tid() = nxt_tid().atomic_incr();
        _that->_xlist.put_in_order(_that);
        xct_t* xd = _that->_xlist.last();
        w_assert3(xd);
        _that->_oldest_tid = xd->_tid;
        _that->release_xlist_mutex();

        _first_lsn = _last_lsn = _undo_nxt = lsn_t::null;
        _xct_ended = 0;
        _last_log = 0;
        _lock_cache_enable = true;


        // should already be out of compensated operation
        w_assert3( _in_compensated_op==0 );

        me()->attach_xct(_that);
        INC_TSTAT(begin_xct_cnt);
        _state = xct_chaining; // to allow us to change state back
        // to active: there's an assert about this where we don't
        // have context to know that it's where we're chaining.
        change_state(xct_active);
    }

    return RCOK;
}


/*********************************************************************
 *
 *  xct_t::abort()
 *
 *  Abort the transaction by calling rollback().
 *
 *********************************************************************/
rc_t
xct_impl::abort()
{
    // If there are too many threads attached, tell the VAS and let it
    // ensure that only one does this.
    W_DO(check_one_thread_attached());
    w_assert1(_state == xct_active || _state == xct_prepared);

    w_assert1(1 == atomic_add_nv(_xct_ended, 1));

    w_assert1(_state == xct_active || _state == xct_prepared);

    change_state(xct_aborting);

    /*
     * clear the list of load stores as they are going to be destroyed
     */
    ClearAllLoadStores();

    W_DO( rollback(lsn_t::null) );

    if (_last_lsn.valid()) {
        /*
         *  If xct generated some log, write a Xct End Record. 
         *  We flush because if this was a prepared
         *  transaction, it really must be synchronous 
         */

        // don't allow a chkpt to occur between changing the state and writing
        // the log record, since otherwise it might try to change the state
        // to the current state (which causes an assertion failure).
#if X_LOG_COMMENT_ON
        {
            w_ostrstream s;
            s << "abort ";
            W_DO(log_comment(s.c_str()));
        }
#endif

        chkpt_serial_m::trx_acquire();
        change_state(xct_freeing_space);
        rc_t rc = log_xct_freeing_space();
        chkpt_serial_m::trx_release();

        W_DO(rc);

        _sync_logbuf();
#if X_LOG_COMMENT_ON
        {
            w_ostrstream s;
            s << "FreeAllStoresToFree ";
            W_DO(log_comment(s.c_str()));
        }
#endif

        /*
         *  Do the actual destruction of all stores which were requested
         *  to be destroyed by this xct.
         */
        FreeAllStoresToFree();

#if X_LOG_COMMENT_ON
        {
            w_ostrstream s;
            s << "unlock_duration, frees extents ";
            W_DO(log_comment(s.c_str()));
        }
#endif
        /*
         *  Free all locks 
         */
        W_COERCE( lm->unlock_duration(t_long, true, false) );

        // don't allow a chkpt to occur between changing the state and writing
        // the log record, since otherwise it might try to change the state
        // to the current state (which causes an assertion failure).

        chkpt_serial_m::trx_acquire();
        change_state(xct_ended);
        rc =  log_xct_abort();
        chkpt_serial_m::trx_release();

        W_DO(rc);
    }  else  {
        change_state(xct_ended);

        /*
         *  Free all locks. Don't free exts as there shouldn't be any to free.
         */
        W_COERCE( lm->unlock_duration(t_long, true, true) );
    }

    me()->detach_xct(_that);        // no transaction for this thread
    INC_TSTAT(abort_xct_cnt);
    return RCOK;
}


/*********************************************************************
 *
 *  xct_t::enter2pc(...)
 *
 *  Mark this tx as a thread of a global tx (participating in EXTERNAL
 *  2PC)
 *
 *********************************************************************/
rc_t 
xct_impl::enter2pc(const gtid_t &g)
{
    W_DO(check_one_thread_attached());
    w_assert1(_state == xct_active);

    if(is_extern2pc()) {
        return RC(eEXTERN2PCTHREAD);
    }
    _global_tid = new gtid_t; //deleted when xct goes away
    if(!_global_tid) {
        W_FATAL(eOUTOFMEMORY);
    }
    DBG(<<"ente2pc for tid " << tid() 
        << " global tid is " << g);
    *_global_tid = g;

    return RCOK;
}


/*********************************************************************
 *
 *  xct_t::save_point(lsn)
 *
 *  Generate and return a save point in "lsn".
 *
 *********************************************************************/
rc_t
xct_impl::save_point(lsn_t& lsn)
{
    // cannot do this with >1 thread attached
    W_DO(check_one_thread_attached());

    lsn = _last_lsn;
    return RCOK;
}


/*********************************************************************
 *
 *  xct_t::dispose()
 *
 *  Make the transaction disappear.
 *  This is only for simulating crashes.  It violates
 *  all tx semantics.
 *
 *********************************************************************/
rc_t
xct_impl::dispose()
{
    W_DO(check_one_thread_attached());
    W_COERCE( lm->unlock_duration(t_long, true, true) );
    ClearAllStoresToFree();
    ClearAllLoadStores();
    _state = xct_ended; // unclean!
    me()->detach_xct(_that);
    return RCOK;
}


/*********************************************************************
 *
 *  xct_t::_flush_logbuf(bool sync=false)
 *
 *  Write the log record buffered and update lsn pointers.
 *  If "sync" is true, force it to disk also.
 *
 *********************************************************************/
w_rc_t
xct_impl::_flush_logbuf( bool sync )
{
    // ASSUMES ALREADY PROTECTED BY MUTEX

    w_assert2(is_1thread_log_mutex_mine() || one_thread_attached());

    if (_last_log)  {

        DBGTHRD ( << " xct_t::_flush_logbuf " << _last_lsn);
        // Fill in the _prev field of the log rec if this record hasn't
        // already been compensated.
        _last_log->fill_xct_attr(tid(), _last_lsn);

        //
        // debugging prints a * if this record was written
        // during rollback
        //
        DBGX2( << " " 
                << ((char *)(state()==xct_aborting)?"RB":"FW")
                << " approx lsn:" << log->curr_lsn() 
                << " rec:" << *_last_log 
                << " size:" << _last_log->length()  
                << " prevlsn:" << _last_log->prev() 
                );

        LOGTRACE( << setiosflags(ios::right) << _last_lsn
                      << resetiosflags(ios::right) << " I: " << *_last_log 
                      << " ... " );
        {
	    logrec_t* l = _last_log;
	    _last_log = 0;
            W_DO( log->insert(*l, &_last_lsn) );
	    
	    /* LOG_RESERVATIONS

	       Now that we know the size of the log record which was
	       generated, charge the bytes to the appropriate location.

	       Normal log inserts consume /length/ available bytes and
	       reserve an additional /length/ bytes against future
	       rollbacks; undo records consume /length/ previously
	       reserved bytes and leave available bytes unchanged..
	    */
	    long bytes_used = l->length();
	    if(_rolling_back || state() != xct_active) {
		w_assert0(_log_bytes_rsvd >= bytes_used);
		_log_bytes_rsvd -= bytes_used;
	    }
	    else {
		_log_bytes_ready -= 2*bytes_used;
		_log_bytes_rsvd += bytes_used;
	    }

            // log insert effectively set_lsn to the lsn of the *next* byte of
            // the log.
            if ( ! _first_lsn.valid())  
                    _first_lsn = _last_lsn;
    
            _undo_nxt = (
                    l->is_undoable_clr() ? _last_lsn :
                    l->is_cpsn() ? l->undo_nxt() : _last_lsn);
        }
    }

    return sync? _sync_logbuf() : RCOK;
}

/*********************************************************************
 *
 *  xct_t::_sync_logbuf()
 *
 *  Force log entries up to the most recently written to disk.
 *
 *********************************************************************/
w_rc_t
xct_impl::_sync_logbuf()
{
    return log? log->flush(_last_lsn) : RCOK;
}

/*********************************************************************
 *
 *  xct_t,xct_impl::get_logbuf(ret)
 *  xct_t,xct_impl::give_logbuf(ret, page)
 *
 *  Flush the logbuf and return it in "ret" for use. Caller must call
 *  give_logbuf(ret) to free it after use.
 *  Leaves the xct's log mutex acquired
 *
 *  These are used in the log_stub.i functions
 *  and ONLY there.  THE ERROR RETURN (running out of log space)
 *  IS PREDICATED ON THAT -- in that it's expected that in the case of
 *  a normal  return (no error), give_logbuf will be called, but in
 *  the error case (out of log space), it will not, and so we must
 *  release the mutex in get_logbuf.
 *  
 *
 *********************************************************************/
rc_t 
xct_impl::get_logbuf(logrec_t*& ret, page_p const* p)
{
    // PROTECT
    ret = 0;
#if GNATS_69_FIX
    // GNATS 69: we need to return if we can't get the mutex
    // to avoid latch-mutex deadlocks between code that's latch-log-ing 
    // and code that's compensating actions (anchor-latch-log)
    //
    // We'll get this right away if we don't have multiple threads
    // attached.
    W_DO(acquire_1thread_log_mutex_conditional());
#else
    acquire_1thread_log_mutex();
#endif

    INC_TSTAT(get_logbuf);

    // Instead of flushing here, we'll flush at the end of give_logbuf()
    // and assert here that we've got nothing buffered:
    w_assert1(!_last_log);

    /* LOG_RESERVATIONS

       Make sure we have enough space, both to continue now and to
       guarantee the ability to roll back should something go wrong
       later. This means we need to reserve double space for each log
       record inserted (one for now, one for the potential undo).

       Unfortunately, we don't actually know the size of the log
       record to be inserted, so we have to be conservative and assume
       maximum size. Similarly, we don't know whether we'll eventually
       abort. We'll deal with the former by adjusting our reservation
       in _flush_logbuf, where we do know the log record's size; we deal
       with the undo reservation at commit time, releasing it all en
       masse.

       NOTE: during rollback we don't check reservations because undo
       was already paid-for when the original record was inserted.

       NOTE: we require three logrec_t worth of reservation: one each
       for forward and undo of the log record we're about to insert,
       and the third to handle asymmetric log records required to end
       the transaction, such as the commit/abort record and any
       top-level actions generated unexpectedly during rollback.
     */
    static u_int const MIN_BYTES_READY = 3*sizeof(logrec_t);
    static u_int const MIN_BYTES_RSVD =  sizeof(logrec_t);
    if(!_rolling_back && _state == xct_active
       && _log_bytes_ready < MIN_BYTES_READY) {
	if(!log->reserve_space(MIN_BYTES_READY)) {
	    /*
	       Yikes! Log full!

	       In order to reclaim space the oldest log partition must
	       have no dirty pages or active transactions associated
	       with it. If we're one of those overly old transactions
	       we have no choice but to abort.
	     */
	    INC_TSTAT(log_full);
	    bool badnews=false;
	    if(_first_lsn.valid() && _first_lsn.hi() == log->global_min_lsn().hi()) {
		INC_TSTAT(log_full_old_xct);
		badnews = true;
	    }

	    lpid_t const* pid = p? &p->pid() : 0;
	    if(!badnews && !bf->force_my_dirty_old_pages(pid)) {
		/* Dirty pages which I hold EX latches on pose a
		   significantly thornier question. On the face of it,
		   the WAL protocol suggests that we can write out any
		   such page without fear of data corruption in the
		   event of a crash.

		   However, the page_lsn is set by the WAL operation,
		   so pages not the subject of this log record are out
		   right off the bat (we have no way to know if
		   they've been changed yet).

		   If it's the current page that's the problem, the
		   only time we'd need to worry is if this and some
		   previous WAL are somehow intertwined, in the sense
		   that it's not actually safe to unlatch the page
		   before the second WAL sticks. This seems like a
		   supremely Bad Idea that (hopefully) none of our
		   code relies on...
		   
		   So, in the end, we assume we can flush page we're
		   about to WAL, if it happens to be old-dirty, but
		   any other old-dirty-EX page we hold will force us
		   to abort.

		   If we decide to play it safe, we just have to send
		   0 insetead of p to the buffer pool query.
		 */
		INC_TSTAT(log_full_old_page);
		badnews = true;
	    }

	    if(!badnews) {
		/* Now it's safe to wait for more log space to open up

                   But before we do anything, let's try to grab the
                   chkpt serial mutex so ongoing checkpoint has a
                   chance to complete before we go crazy.
                */
                chkpt_serial_m::trx_acquire();
                chkpt_serial_m::trx_release();

                static queue_based_block_lock_t emergency_log_flush_mutex;
                CRITICAL_SECTION(cs, emergency_log_flush_mutex);                
                release_1thread_log_mutex(); // the  checkpoint will acquire this
		for(int tries_left=3; tries_left > 0; tries_left--) {
		    if(tries_left == 1) {
			// the checkpoint should also do this, but just in case...
			lsn_t target = log_m::first_lsn(log->global_min_lsn().hi()+1);
			w_rc_t rc = bf->force_until_lsn(target, false);
			// did the force succeed?
			if(rc.is_error()) {
			    INC_TSTAT(log_full_giveup);
			    fprintf(stderr, "Log recovery failed\n");
#if W_DEBUG_LEVEL > 0
			    extern void dump_all_sm_stats();
			    dump_all_sm_stats();
#endif
			    if(rc.err_num() == eBPFORCEFAILED) 
				return RC(eOUTOFLOGSPACE);
			    return rc;
			}
		    }

		    // most likely it's well aware, but just in case...
		    chkpt->wakeup_and_take(); 
		    log->wait_for_space();
		    
		    // try again...
		    if(log->reserve_space(MIN_BYTES_READY)) {
			if(tries_left > 1) {
			    INC_TSTAT(log_full_wait);
			}
			else {
			    INC_TSTAT(log_full_force);
			}
			goto success;
		    }
		}

		// nothing's working... give up and abort
		stringstream tmp;
		tmp << "Log too full. me=" << me()->id
		    << " pthread=" << pthread_self()
		    << ": min_chkpt_rec_lsn=" << log->min_chkpt_rec_lsn()
		    << ", curr_lsn=" << log->curr_lsn() 
		    // also print info that shows why we didn't croak
		    // on the first check
		    << "; space left " << log->space_left()
		    << endl;
		fprintf(stderr, "%s\n", tmp.str().c_str());
		INC_TSTAT(log_full_giveup);
		badnews = true;
	    }

	    if(badnews)
		return RC(eOUTOFLOGSPACE);
	    
	success:
	    acquire_1thread_log_mutex();
	}
	_log_bytes_ready += MIN_BYTES_READY;
    }

    /* Transactions must make some log entries as they complete
       (commit or abort), so we have to have always some bytes
       reserved. This is the third of those three logrec_t in
       MIN_BYTES_READY, so we don't have to reserve more ready bytes
       just because of this.
     */
    if(_log_bytes_rsvd < MIN_BYTES_RSVD) {
	_log_bytes_ready -= MIN_BYTES_RSVD;
	_log_bytes_rsvd += MIN_BYTES_RSVD;
    }
    
    ret = _last_log = _log_buf;
    w_assert3(is_1thread_log_mutex_mine());
    return RCOK;
}


rc_t
xct_impl::give_logbuf(logrec_t* l, const page_p *page)
{
    FUNC(xct_impl::give_logbuf);
    DBG(<<"_last_log contains: "   << *l );
        
    // ALREADY PROTECTED from get_logbuf() call
    w_assert2(is_1thread_log_mutex_mine());

    w_assert1(l == _last_log);

    // WAL: hang onto last mod page so we can
    // keep it from being flushed before the log
    // is written.
    page_p last_mod_page;

    if(page != (page_p *)0) {
        // Should already be EX-latched since it's the last modified page!
        w_assert1(page->latch_mode() == LATCH_EX);

        last_mod_page = *page; // refixes
    } 
    else if (l->shpid())  
    {
        // Those records with page ids stuffed in with fill() 
        // should match those whose 2nd argument (page) is non-null,
        // so those should hit the above case.
        // Exception: alloc_file_page_log does not need the
        // page latched, so it is called "logical",
        // but does stuff the pid into the log record.
        // So for that log record, having a pid doesn't mean that
        // the page is latched or is an updated page.  We don't
        // have to worry about WAL for this page, because the
        // log records that write the page are for the format, which
        // is yet to come.
        if(l->type() != logrec_t::t_alloc_file_page) {
            w_assert2(0); // we shouldn't get here.
        }
    } else {
        // This log record doesn't use a page.
        w_assert1(l->tag() == 0);
    }

    w_assert2(is_1thread_log_mutex_mine());

    if(last_mod_page.is_fixed()) { // i.e., is  fixed
        w_assert2(l->tag()!=0);
        w_assert2(last_mod_page.latch_mode() == LATCH_EX);
    } else {
        w_assert2(l->tag()==0);
        w_assert3(last_mod_page.latch_mode() == LATCH_NL);
    }

    rc_t rc = _flush_logbuf(); // stuffs tid, _last_lsn into our record,
        // then inserts it into the log, getting _last_lsn
    if(rc.is_error())
	goto done;
    
    if(last_mod_page.is_fixed() ) {
        w_assert2(last_mod_page.latch_mode() == LATCH_EX);
        last_mod_page.set_lsns(_last_lsn);
        last_mod_page.unfix_dirty();
        w_assert1(last_mod_page.check_lsn_invariant());
    }

 done:
    release_1thread_log_mutex();
    return rc;
}


/*********************************************************************
 *
 *  xct_t::release_anchor(and_compensate)
 *
 *  stop critical sections vis-a-vis compensated operations
 *  If and_compensate==true, it makes the _last_log a clr
 *
 *********************************************************************/
void
xct_impl::release_anchor( bool and_compensate )
{
    FUNC(xct_t::release_anchor);

    w_assert3(is_1thread_log_mutex_mine());
    DBGX(    
            << " RELEASE ANCHOR " 
            << " in compensated op==" << _in_compensated_op
            << " holds xct_mutex_1==" 
            /*<< (const char *)(_1thread_xct.is_mine()? "true" : "false"*)*/
    );

    w_assert3(_in_compensated_op>0);

    if(_in_compensated_op == 1) { // will soon be 0

        // NB: this whole section could be made a bit
        // more efficient in the -UDEBUG case, but for
        // now, let's keep in all the checks

        // don't flush unless we have popped back
        // to the last compensate() of the bunch

        // Now see if this last item was supposed to be
        // compensated:
        if(and_compensate && (_anchor != lsn_t::null)) {
           VOIDSSMTEST("compensate");
           if(_last_log) {
               if ( _last_log->is_cpsn()) {
                    DBG(<<"already compensated");
                    w_assert3(_anchor == _last_log->undo_nxt());
               } else {
                   DBG(<<"SETTING anchor:" << _anchor);
                   w_assert3(_anchor <= _last_lsn);
                   _last_log->set_clr(_anchor);
               }
           } else {
               DBG(<<"no _last_log:" << _anchor);
               /* perhaps we can update the log record in the log buffer */
               if( log && !log->compensate(_last_lsn, _anchor).is_error()) {
                    INC_TSTAT(compensate_in_log);
               } else {
		   // compensations use _log_bytes_rsvd, not _log_bytes_ready
		   bool was_rolling_back = _rolling_back;
		   _rolling_back = true;
                    W_COERCE(log_compensate(_anchor));
		    _rolling_back = was_rolling_back;
                    INC_TSTAT(compensate_records);
               }
            }
        }

        _anchor = lsn_t::null;

    }
    // UN-PROTECT 
    _in_compensated_op -- ;

    DBGX(    
        << " out compensated op=" << _in_compensated_op
        << " holds xct_mutex_1==" 
        /*        << (const char *)(_1thread_xct.is_mine()? "true" : "false")*/
    );
    release_1thread_log_mutex();
}

/*********************************************************************
 *
 *  xct_t::anchor()
 *
 *  Return a log anchor (begin a top level action).
 *  If argument==true (most of the time), it stores
 *  the anchor for use with compensations.  When the
 *  argument==false, this is used (by I/O monitor) not
 *  for compensations, but only for concurrency control.
 *
 *********************************************************************/
const lsn_t& 
xct_impl::anchor(bool grabit)
{
    // PROTECT
    acquire_1thread_log_mutex();
    _in_compensated_op ++;

    INC_TSTAT(anchors);
    DBGX(    
            << " GRAB ANCHOR " 
            << " in compensated op==" << _in_compensated_op
    );


    if(_in_compensated_op == 1 && grabit) {
        // _anchor is set to null when _in_compensated_op goes to 0
        w_assert3(_anchor == lsn_t::null);
        _anchor = _last_lsn;
        DBGX(    << " anchor =" << _anchor);
    }
    DBGX(    << " anchor returns " << _last_lsn );

    return _last_lsn;
}


/*********************************************************************
 *
 *  xct_t::compensate_undo(lsn)
 *
 *  compensation during undo is handled slightly differently--
 *  the gist of it is the same, but the assertions differ, and
 *  we have to acquire the mutex first
 *********************************************************************/
void 
xct_impl::compensate_undo(const lsn_t& lsn)
{
    DBGX(    << " compensate_undo (" << lsn << ") -- state=" << state());

    acquire_1thread_log_mutex(); 
    w_assert3(_in_compensated_op);
    // w_assert9(state() == xct_aborting); it's active if in sm::rollback_work

    _compensate(lsn, _last_log?_last_log->is_undoable_clr() : false);

    release_1thread_log_mutex();
}

/*********************************************************************
 *
 *  xct_t::compensate(lsn, bool undoable)
 *
 *  Generate a compensation log record to compensate actions 
 *  started at "lsn" (commit a top level action).
 *  Generates a new log record only if it has to do so.
 *
 *********************************************************************/
void 
xct_impl::compensate(const lsn_t& lsn, bool undoable)
{
    DBGX(    << " compensate(" << lsn << ") -- state=" << state());

    // acquire_1thread_mutex(); should already be mine
    w_assert3(is_1thread_log_mutex_mine());

    _compensate(lsn, undoable);

    w_assert3(is_1thread_log_mutex_mine());
    release_anchor();
}

/*********************************************************************
 *
 *  xct_t::_compensate(lsn, bool undoable)
 *
 *  
 *  Generate a compensation log record to compensate actions 
 *  started at "lsn" (commit a top level action).
 *  Generates a new log record only if it has to do so.
 *
 *  Special case of undoable compensation records is handled by the
 *  boolean argument. (NOT USED FOR NOW)
 *
 *********************************************************************/
void 
xct_impl::_compensate(const lsn_t& lsn, bool undoable)
{
    DBGX(    << "_compensate(" << lsn << ") -- state=" << state());

    bool done = false;
    if ( _last_log ) {
        // We still have the log record here, and
        // we can compensate it.

        /*
         * lsn is got from anchor(), and anchor() returns _last_lsn.
         * _last_lsn is the lsn of the last log record
         * inserted into the log, and, since
         * this log record hasn't been inserted yet, this
         * function can't make a log record compensate to itself.
         */
        w_assert3(lsn <= _last_lsn);
        _last_log->set_clr(lsn);
        INC_TSTAT(compensate_in_xct);
        done = true;
    } else {
        /* 
        // Log record has already been inserted into the buffer.
        // Perhaps we can update the log record in the log buffer.
        // However,  it's conceivable that nothing's been written
        // since _last_lsn, and we could be trying to compensate
        // around nothing.  This indicates an error in the calling
        // code.
        */
        if( lsn >= _last_lsn) {
            INC_TSTAT(compensate_skipped);
        }
        if( log && (! undoable) && (lsn < _last_lsn)) {
            if(!log->compensate(_last_lsn, lsn).is_error()) {
                INC_TSTAT(compensate_in_log);
                done = true;
            }
        }
    }
    w_assert3(is_1thread_log_mutex_mine());

    if( !done && (lsn < _last_lsn) ) {
        /*
        // If we've actually written some log records since
        // this anchor (lsn) was grabbed, 
        //
        // force it to write a compensation-only record
        // either because there's no record on which to 
        // piggy-back the compensation, or because the record
        // that's there is an undoable/compensation and will be
        // undone (and we *really* want to compensate around it)
        */
	// compensations use _log_bytes_rsvd, not _log_bytes_ready
	bool was_rolling_back = _rolling_back;
	_rolling_back = true;
        W_COERCE(log_compensate(lsn));
	_rolling_back = was_rolling_back;
        INC_TSTAT(compensate_records);
    }
}



/*********************************************************************
 *
 *  xct_t::rollback(savept)
 *
 *  Rollback transaction up to "savept".
 *
 *********************************************************************/
rc_t
xct_impl::rollback(lsn_t save_pt)
{
    FUNC(xct_t::rollback);
    W_DO(check_one_thread_attached());


    w_rc_t            rc;
    logrec_t*         buf =0;

    // MUST PROTECT anyway, since this generates compensations
    acquire_1thread_log_mutex();

    if(_in_compensated_op > 0) {
        w_assert3(save_pt >= _anchor);
    } else {
        w_assert3(_anchor == lsn_t::null);
    }

    DBGX( << " in compensated op depth " <<  _in_compensated_op
            << " save_pt " << save_pt << " anchor " << _anchor);
    _in_compensated_op++;

    // rollback is only one type of compensated op, and it doesn't nest
    w_assert0(!_rolling_back); 
    _rolling_back = true; 

    lsn_t nxt = _undo_nxt;

    LOGTRACE( << "abort begins at " << nxt);

    if(!log) { 
        ss_m::errlog->clog  <<
        "Cannot roll back with logging turned off. " 
        << flushl; 
        return RC(eNOABORT);
    }

    { // Contain the scope of the following __copy__buf:

    logrec_t* __copy__buf = new logrec_t; // auto-del
    if(! __copy__buf) { W_FATAL(eOUTOFMEMORY); }
    w_auto_delete_t<logrec_t> auto_del(__copy__buf);
    logrec_t&         r = *__copy__buf;

    while (save_pt < nxt)  {
        rc =  log->fetch(nxt, buf, 0);
        if(rc.is_error() && rc.err_num()==eEOF) {
            DBG(<< " fetch returns EOF" );
            log->release(); // GNATS_10_FIX
            goto done;
        } else
        {
             logrec_t& temp = *buf;
             w_assert3(!temp.is_skip());
          
             /* Only copy the valid portion of 
              * the log record, then release it 
              */
             memcpy(__copy__buf, &temp, temp.length());
             log->release();
        }

        if (r.is_undo()) {
            /*
             *  Undo action of r.
             */
            LOGTRACE( << setiosflags(ios::right) << nxt
                      << resetiosflags(ios::right) << " U: " << r 
                      << " ... " );

#if W_DEBUG_LEVEL > 2
	    u_int	was_rsvd = _log_bytes_rsvd;
#endif 
            lpid_t pid = r.construct_pid();
            page_p page;

            if (! r.is_logical()) {
                store_flag_t store_flags = st_bad;
#define TMP_NOFLAG 0
                rc = page.fix(pid, page_p::t_any_p, LATCH_EX, 
                    TMP_NOFLAG, store_flags);
                if(rc.is_error()) {
                    goto done;
                }
                w_assert1(page.pid() == pid);
            }


            r.undo(page.is_fixed() ? &page : 0);

#if W_DEBUG_LEVEL > 2
            if(was_rsvd - _log_byts_rsvd  > r.length()) {
                  LOGTRACE(<< " len=" << r.length() << " B=" << bbwd );
            }
#endif 
            if(r.is_cpsn()) {
                LOGTRACE( << " compensating to " << r.undo_nxt() );
                nxt = r.undo_nxt();
            } else {
                LOGTRACE( << " undoing to " << r.prev() );
                nxt = r.prev();
            }

        } else  if (r.is_cpsn())  {
            LOGTRACE( << setiosflags(ios::right) << nxt
                      << resetiosflags(ios::right) << " U: " << r 
                      << " compensating to" << r.undo_nxt() );
            nxt = r.undo_nxt();
            // r.prev() could just as well be null

        } else {
            LOGTRACE( << setiosflags(ios::right) << nxt
              << resetiosflags(ios::right) << " U: " << r 
                      << " skipping to " << r.prev());
            nxt = r.prev();
            // w_assert9(r.undo_nxt() == lsn_t::null);
        }
    }

    // close scope so the
    // auto-release will free the log rec copy buffer, __copy__buf
    }

    _undo_nxt = nxt;

    /*
     *  The sdesc cache must be cleared, because rollback may
     *  have caused conversions from multi-page stores to single
     *  page stores.
     */
    if(_that->sdesc_cache()) {
        _that->sdesc_cache()->remove_all();
    }

done:

    DBGX( << " out compensated op");
    _in_compensated_op --;
    _rolling_back = false;
    w_assert3(_anchor == lsn_t::null ||
                _anchor == save_pt);
    release_1thread_log_mutex();

    if(save_pt != lsn_t::null) {
        INC_TSTAT(rollback_savept_cnt);
    }

    return rc;
}

/*
 * clear the list of stores to be freed upon xct completion
 * this is used by abort since rollback will recreate the
 * proper list of stores to be freed.
 *
 * don't *really* need mutex since only called when aborting => 1 thread
 * but we have the acquire here for internal documentation 
 */
void
xct_impl::ClearAllStoresToFree()
{
    w_assert2(one_thread_attached());
    // acquire_1thread_xct_mutex();
    stid_list_elem_t*        s = 0;
    while ((s = _storesToFree.pop()))  {
        delete s;
    }

    w_assert3(_storesToFree.is_empty());
    // release_1thread_xct_mutex();
}


/*
 * this function will free all the stores which need to freed
 * by this completing xct.
 *
 * don't REALLY need mutex since only called when committing/aborting
 * => 1 thread attached
 */
void
xct_impl::FreeAllStoresToFree()
{
    w_assert2(one_thread_attached());
    // acquire_1thread_xct_mutex();
    stid_list_elem_t*        s = 0;
    while ((s = _storesToFree.pop()))  {
        W_COERCE( io->free_store_after_xct(s->stid) );
        delete s;
    }
    // release_1thread_xct_mutex();
}

/*
 * this method returns the # extents allocated to stores
 * marked for deletion by this xct
 */
void
xct_impl::num_extents_marked_for_deletion(base_stat_t &num)
{
    acquire_1thread_xct_mutex();
    stid_list_elem_t*        s = 0;
    num = 0;
    SmStoreMetaStats _stats;
    base_stat_t j;

    {
    w_list_i<stid_list_elem_t,queue_based_lock_t>        i(_storesToFree);
    while ((s = i.next()))  {
        _stats.Clear();
        W_COERCE( io->get_store_meta_stats(s->stid, _stats) );
        j = _stats.numReservedPages; 
        w_assert3((j % ss_m::ext_sz) == 0);
        j /= ss_m::ext_sz;
        num += j;
    }
    }
    release_1thread_xct_mutex();
}

rc_t
xct_impl::PrepareLogAllStoresToFree()
{
    // This check is more for internal documentation than for anything
    W_DO(check_one_thread_attached());

    stid_t* stids = new stid_t[prepare_stores_to_free_t::max]; // auto-del
    w_auto_delete_array_t<stid_t> auto_del_stids(stids);

    stid_list_elem_t*              e;
    uint4_t                        num = 0;
    {
        w_list_i<stid_list_elem_t,queue_based_lock_t>        i(_storesToFree);
        while ((e = i.next()))  {
            stids[num++] = e->stid;
            if (num >= prepare_stores_to_free_t::max)  {
                W_DO( log_xct_prepare_stores(num, stids) );
                num = 0;
            }
        }
    }
    if (num > 0)  {
        W_DO( log_xct_prepare_stores(num, stids) );
    }

    return RCOK;
}


void
xct_impl::DumpStoresToFree()
{
    stid_list_elem_t*                e;
    w_list_i<stid_list_elem_t,queue_based_lock_t>        i(_storesToFree);

    acquire_1thread_xct_mutex();
    cout << "list of stores to free";
    while ((e = i.next()))  {
        cout << " <- " << e->stid;
    }
    release_1thread_xct_mutex();
    cout << endl;
}

/*
 * Moved here to work around a gcc/egcs bug that
 * caused compiler to choke.
 */
class VolidCnt {
    private:
        int unique_vols;
        int vol_map[xct_t::max_vols];
        snum_t vol_cnts[xct_t::max_vols];
    public:
        VolidCnt() : unique_vols(0) {};
        int Lookup(int vol)
            {
                for (int i = 0; i < unique_vols; i++)
                    if (vol_map[i] == vol)
                        return i;
                
                w_assert9(unique_vols < xct_t::max_vols);
                vol_map[unique_vols] = vol;
                vol_cnts[unique_vols] = 0;
                return unique_vols++;
            };
        int Increment(int vol)
            {
                return ++vol_cnts[Lookup(vol)];
            };
        int Decrement(int vol)
            {
                w_assert9(vol_cnts[Lookup(vol)]);
                return --vol_cnts[Lookup(vol)];
            };
#if W_DEBUG_LEVEL > 2
        ~VolidCnt()
            {
                for (int i = 0; i < unique_vols; i ++)
                    w_assert9(vol_cnts[i] == 0);
            };
#endif 
};

rc_t
xct_impl::ConvertAllLoadStoresToRegularStores()
{

#if X_LOG_COMMENT_ON
    {
        static int volatile uniq=0;
        static int volatile last_uniq=0;
        int    nv =  atomic_add_nv(uniq, 1);
        //Even if someone else slips in here, the
        //values should never match. 
        w_assert1(last_uniq != nv);
        *&last_uniq = nv;

        // this is to help us figure out if we
        // are issuing duplicate commits/log entries or
        // if the problem is in the log code or the
        // grabbing of the 1thread log mutex 
        w_ostrstream s;
        s << "commit " << uniq << ", ConvertAllLoadStores " 
            << " xct state " << _state
            << " xct ended " << _xct_ended
            << " this " << this
            << " thread " << me()->id;
        W_DO(log_comment(s.c_str()));
    }
#endif

    w_assert2(one_thread_attached());
    stid_list_elem_t*        s = 0;
    VolidCnt cnt;

    {
        w_list_i<stid_list_elem_t,queue_based_lock_t>        i(_loadStores);

        while ((s = i.next()))  {
            cnt.Increment(s->stid.vol);
        }
    }

    while ((s = _loadStores.pop()))  {
        bool sync_volume = (cnt.Decrement(s->stid.vol) == 0);
        store_flag_t f;
        W_DO( io->get_store_flags(s->stid, f));
        // if any combo of  st_tmp, st_insert_file, st_load_file, convert
        // but only insert and load are put into this list.
        if(f != st_regular) {
            W_DO( io->set_store_flags(s->stid, st_regular, sync_volume) );
        }
        delete s;
    }

    w_assert3(_loadStores.is_empty());

    return RCOK;
}


void
xct_impl::ClearAllLoadStores()
{
    w_assert2(one_thread_attached());
    stid_list_elem_t*        s = 0;
    while ((s = _loadStores.pop()))  {
        delete s;
    }

    w_assert3(_loadStores.is_empty());
}

smlevel_0::concurrency_t                
xct_impl::get_lock_level()  
{ 
    smlevel_0::concurrency_t l = t_cc_bad;
    acquire_1thread_xct_mutex();
    l =  convert(lock_info()->lock_level());
    release_1thread_xct_mutex();
    return l;
}

void                           
xct_impl::lock_level(concurrency_t l) 
{
    acquire_1thread_xct_mutex();
    lockid_t::name_space_t n = convert(l);
    if(n != lockid_t::t_bad) {
        lock_info()->set_lock_level(n);
    }
    release_1thread_xct_mutex();
}

void 
xct_impl::attach_thread() 
{
    acquire_1thread_xct_mutex();
#ifdef NANCY
    // NANCY TODO what about this?
    // Would like to disallow attach/detach while
    // in quarks, but it will take a modest interface
    // change to do this.
    if(lock_info() && lock_info()->in_quark_scope()) {
        return RC(eINQUARK);
    }
#endif
    int nt=atomic_add_nv(_threads_attached, 1);
    if(nt > 1) {
        INC_TSTAT(mpl_attach_cnt);
    }
    w_assert2(_threads_attached >=0);
    me()->new_xct(_that);
    release_1thread_xct_mutex();
}


void
xct_impl::detach_thread() 
{
    acquire_1thread_xct_mutex();
    atomic_add(_threads_attached, -1);
    w_assert2(_threads_attached >=0);
    me()->no_xct(_that);

    release_1thread_xct_mutex();
}

w_rc_t
xct_impl::lockblock(timeout_in_ms timeout)
{
// Used by lock manager. (lock_core_m)
// Another thread in our xct is blocking. We're going to have to
// wait on another resource, until our partner thread unblocks,
// and then try again.
    CRITICAL_SECTION(bcs, _waiters_mutex);
    w_rc_t rc;
    if(num_threads() > 1) {
        DBGTHRD(<<"blocking on condn variable");
        // Don't block if other thread has gone away
        // BUGBUG This is still racy!  multi.1 shows us that.

        // this code taken from scond_t implementation, from when
        // _waiters_cond was an scond_t:
        if(timeout == WAIT_IMMEDIATE)  return RC(sthread_t::stTIMEOUT);
        if(timeout == WAIT_FOREVER)  {
            DO_PTHREAD(pthread_cond_wait(&_waiters_cond, &_waiters_mutex));
        } else {
            struct timespec when;
            sthread_t::timeout_to_timespec(timeout, when);
            DO_PTHREAD_TIMED(pthread_cond_timedwait(&_waiters_cond, &_waiters_mutex, &when));
        }

        DBGTHRD(<<"not blocked on cond'n variable");
    }
    if(rc.is_error()) {
        return RC_AUGMENT(rc);
    } 
    return rc;
}

void
xct_impl::lockunblock()
{
// Used by lock manager. (lock_core_m)
// This thread in our xct is no longer blocking. Wake up anyone
// who was waiting on this other resource because I was 
// blocked in the lock manager.
    CRITICAL_SECTION(bcs, _waiters_mutex);
    DBGTHRD(<<"signalling waiters on cond'n variable");
    DO_PTHREAD(pthread_cond_broadcast(&_waiters_cond));
    DBGTHRD(<<"signalling cond'n variable done");
}

//
// one_thread_attached() does not acquire the 1thread mutex; it
// just checks that the vas isn't calling certain methods
// when other threads are still working on behalf of the same xct.
// It doesn't protect the vas from trying calling, say, commit and
// later attaching another thread while the commit is going on.
// --- Can't protect a vas from itself in all cases.
rc_t
xct_impl::check_one_thread_attached() const
{
    if(one_thread_attached()) return RCOK;
    return RC(eTWOTHREAD);
}

bool
xct_impl::one_thread_attached() const
{
    // wait for the checkpoint to finish
    if( _threads_attached > 1) {
        chkpt_serial_m::trx_acquire();
        if( _threads_attached > 1) {
            chkpt_serial_m::trx_release();
#if W_DEBUG_LEVEL > 2
            fprintf(stderr, 
            "Fatal VAS or SSM error: %s %d %s %d.%d \n",
            "Only one thread allowed in this operation at any time.",
            _threads_attached, 
            "threads are attached to xct",
            tid().get_hi(), tid().get_lo()
            );
            xstop();
#endif
            return false;
        }
    }
    return true;
}

bool
xct_impl::is_1thread_log_mutex_mine() const
{
  return _1thread_log.is_mine(&_1thread_log_me);
}
bool
xct_impl::is_1thread_xct_mutex_mine() const
{
  return _1thread_xct.is_mine(&_1thread_xct_me);
}

extern "C" void xctstophere(int c);
void xctstophere(int ) {
}

#if GNATS_69_FIX
w_rc_t
xct_impl::acquire_1thread_log_mutex_conditional() 
{
    if(is_1thread_log_mutex_mine())
    { 
        if(!_in_compensated_op) {
            // This is a programming error
            W_FATAL_MSG(eINTERNAL, 
                    << " duplicate acquire of 1thread log mutex");
            return RC(eINTERNAL);
        }
        // we're logging something while in a compensated operation.
        // The last release from the compensated op will free the mutex.
        return RCOK;
    }
    // GNATS 69:
    // Multi-threaded xcts cat get into a latch-mutex deadlock
    // if we unconditionally acquire here.
    // Specifically, the following scenario demonstrates the
    // problem, but in theory it could happen in file code and
    // anywhere we compensate using anchor():
    // thread 1: latch page, log update (log update calls this)
    // thread 2: grab anchor (calls this), do updates that latch pages.
    // In the single-threaded xct case the btree code is safe because
    // the different threads don't contend for this mutex.
    // This macro gives the number of retries we're willing to make
    // before returning an rc error.
    
#define ONE_THREAD_LOG_MUTEX_WAIT 1
    int  retries = (_in_compensated_op+1) * ONE_THREAD_LOG_MUTEX_WAIT;
    bool gotit  = _1thread_log.attempt(&_1thread_log_me);

    while( (!gotit) && (retries-- > 0) ) {
        usleep(1000);
        gotit = _1thread_log.attempt(&_1thread_log_me);
    }
    int tried = (ONE_THREAD_LOG_MUTEX_WAIT-retries);
    if(tried > 0) {
        ADD_TSTAT(await_1thread_log, tried);
    }
    INC_TSTAT(acquire_1thread_log);

    DBGX(    << " acquired log mutex: " << _in_compensated_op
    << " after " << (ONE_THREAD_LOG_MUTEX_WAIT-retries) << " tries ");

    if(!gotit) {
        static int count=0;
        atomic_add(count, 1);
        bool mine = is_1thread_log_mutex_mine();
        bool in_comp = _in_compensated_op;
        fprintf(stderr, 
        "NOT ACQUIRING LOG MUTEX %d after %d tries, _in_compensated_op %d mine %d\n", 
                count, tried, in_comp, int(mine)
                );
        xctstophere(count);
        return RC(eRETRY);
    }
    return RCOK;
}
#endif

void
xct_impl::acquire_1thread_log_mutex() 
{
    if(is_1thread_log_mutex_mine()) {
        DBGX( << " duplicate acquire log mutex: " << _in_compensated_op);
        return;
    }
    // the queue_based_lock_t implementation can tell if it was
    // free or held; the w_pthread_lock_t cannot,
    // and always returns false.
    bool was_contended = _1thread_log.acquire(&_1thread_log_me);
    if(was_contended)
        INC_TSTAT(await_1thread_log);
    INC_TSTAT(acquire_1thread_log);

    DBGX(    << " acquired log mutex: " << _in_compensated_op);
}

void
xct_impl::acquire_1thread_xct_mutex() // default: true
{
    if(is_1thread_xct_mutex_mine()) {
        return;
    }
    // the queue_based_lock_t implementation can tell if it was
    // free or held; the w_pthread_lock_t cannot,
    // and always returns false.
    bool was_contended = _1thread_xct.acquire(&_1thread_xct_me);
    if(was_contended) 
        INC_TSTAT(await_1thread_xct);
    DBGX(    << " acquired xct mutex");
}

void
xct_impl::release_1thread_xct_mutex()
{
    w_assert3(is_1thread_xct_mutex_mine());

    DBGX( << " release xct mutex");

    _1thread_xct.release(_1thread_xct_me);
}

void
xct_impl::release_1thread_log_mutex()
{
    w_assert2(is_1thread_log_mutex_mine());

    DBGX( << " release log mutex: " << _in_compensated_op
        << " holds xct_mutex_1==" 
          /*        << (const char *)(_1thread_xct.is_mine()? "true" : "false")*/
    );

    if(_in_compensated_op==0 ) {
        _1thread_log.release(_1thread_log_me);
    } else {
        DBGX( << " in compensated operation: can't release log mutex");
    }
}


ostream &
xct_impl::dump_locks(ostream &out) const
{
    return lock_info()->dump_locks(out);
}


smlevel_0::switch_t 
xct_impl::set_log_state(switch_t s, bool &) 
{
    xct_log_t *mine = me()->xct_log();
    switch_t old = (mine->xct_log_is_off()? OFF: ON);
    if(s==OFF) mine->set_xct_log_off();
    else mine->set_xct_log_on();
    return old;
}

void
xct_impl::restore_log_state(switch_t s, bool n ) 
{
    (void) set_log_state(s, n);
}


lockid_t::name_space_t        
xct_impl::convert(concurrency_t cc)
{
    switch(cc) {
        case t_cc_record:
                return lockid_t::t_record;

        case t_cc_page:
                return lockid_t::t_page;

        case t_cc_file:
                return lockid_t::t_store;

        case t_cc_vol:
                return lockid_t::t_vol;

        case t_cc_kvl:
        case t_cc_im:
        case t_cc_modkvl:
        case t_cc_bad: 
        case t_cc_none: 
        case t_cc_append:
                return lockid_t::t_bad;
    }
    return lockid_t::t_bad;
}

smlevel_0::concurrency_t                
xct_impl::convert(lockid_t::name_space_t n)
{
    switch(n) {
        default:
        case lockid_t::t_bad:
        case lockid_t::t_kvl:
        case lockid_t::t_extent:
            break;

        case lockid_t::t_vol:
            return t_cc_vol;

        case lockid_t::t_store:
            return t_cc_file;

        case lockid_t::t_page:
            return t_cc_page;

        case lockid_t::t_record:
            return t_cc_record;
    }
    W_FATAL(eINTERNAL); 
    return t_cc_bad;
}

NORET
xct_dependent_t::xct_dependent_t(xct_t* xd) : _xd(xd), _registered(false)
{
}

void
xct_dependent_t::register_me() {
    // it's possible that there is no active xct when this
    // function is called, so be prepared for null
    xct_t* xd = _xd;
    if (xd) {
        W_COERCE( xd->add_dependent(this) );
    }
    _registered = true;
}

NORET
xct_dependent_t::~xct_dependent_t()
{
    w_assert2(_registered);
    // it's possible that there is no active xct the constructor
    // was called, so be prepared for null
    if (_link.member_of() != NULL) {
        w_assert1(_xd);
        // GNATS 99 was: _link.detach();
        // Have to remove it under protection of the 1thread_xct_mutex
        W_COERCE(_xd->remove_dependent(this));
    }
}
