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

 $Id: xct.cpp,v 1.207.2.19 2010/03/25 18:05:18 nhall Exp $

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

#ifdef __GNUG__
#pragma implementation "xct.h"
#endif

#include <new>
#include "sm_int_1.h"
#include "sdesc.h"
#include "block_alloc.h"
#include "tls.h"

#include "sm_escalation.h"
#include "lock_x.h"
#include "xct_dependent.h"
#include "xct_impl.h"

#include <w_strstream.h>

#ifdef EXPLICIT_TEMPLATE
template class w_list_t<xct_t, queue_based_lock_t>;
template class w_list_i<xct_t, queue_based_lock_t>;
template class w_keyed_list_t<xct_t, queue_based_lock_t, tid_t>;
template class w_descend_list_t<xct_t, queue_based_lock_t, tid_t>;
template class w_list_t<stid_list_elem_t, queue_based_lock_t>;
template class w_list_i<stid_list_elem_t, queue_based_lock_t>;

template class w_auto_delete_array_t<lock_mode_t>;

#endif /* __GNUG__*/

#define LOGTRACE(x)        DBG(x)
#define DBGX(arg) DBG(<<" th."<<me()->id << " " << "tid." << _tid  arg)


/*********************************************************************
 *
 *  The xct list is sorted for easy access to the oldest and
 *  youngest transaction. All instantiated xct_t objects are
 *  in the list.
 *
 *  Here are the transaction list and the mutex that protects it.
 *  Local namespace is used so that we can see that noone 
 *  uses these except through these 4 methods, below.
 *
 *********************************************************************/
namespace  localns {
    /**\var static __thread queue_based_lock_t::ext_qnode _xct_t_me_node;
     * \brief Queue node for holding mutex to prevent mutiple-thread/transaction where disallowed.
     * \ingroup TLS
     */
    static __thread queue_based_lock_t::ext_qnode _xct_t_me_node = 
                                                    EXT_QNODE_INITIALIZER;
    /**\var static __thread queue_based_lock_t::ext_qnode _xlist_mutex_node;
     * \brief Queue node for holding mutex to serialize access transaction list.
     * \ingroup TLS
     */
    static __thread queue_based_lock_t::ext_qnode _xlist_mutex_node = 
                                                    EXT_QNODE_INITIALIZER;

};

queue_based_lock_t        xct_t::_xlist_mutex;

w_descend_list_t<xct_t, queue_based_lock_t, tid_t>   
        xct_t::_xlist(W_KEYED_ARG(xct_t, _tid,_xlink), &_xlist_mutex);

void xct_t::assert_xlist_mutex_not_mine() 
{
    w_assert1(localns::_xlist_mutex_node._held == 0
           || localns::_xlist_mutex_node._held->
               is_mine(&localns::_xlist_mutex_node)==false);
}
void xct_t::assert_xlist_mutex_is_mine() 
{
     w_assert1(localns::_xlist_mutex_node._held 
        && localns::_xlist_mutex_node._held->
            is_mine(&localns::_xlist_mutex_node));
}

w_rc_t  xct_t::acquire_xlist_mutex()
{
     assert_xlist_mutex_not_mine();
     _xlist_mutex.acquire(&localns::_xlist_mutex_node);
     assert_xlist_mutex_is_mine();
     return RCOK;
}

void  xct_t::release_xlist_mutex()
{
     assert_xlist_mutex_is_mine();
     _xlist_mutex.release(localns::_xlist_mutex_node);
     assert_xlist_mutex_not_mine();
}

/*********************************************************************
 *
 *  _nxt_tid is used to generate unique transaction id
 *  _1thread_name is the name of the mutex protecting the xct_t from
 *          multi-thread access
 *
 *********************************************************************/
tid_t                                 xct_t::_nxt_tid = tid_t::null;

/*********************************************************************
 *
 *  _oldest_tid is the oldest currently-running tx (well, could be
 *  committed by now - the xct destructor updates this)
 *  This corresponds to the Shore-MT paper section 7.3, top of
 *  2nd column, page 10.
 *  TODO NANCY: make this atomic for 32-bit platforms
 *
 *********************************************************************/
tid_t                                xct_t::_oldest_tid = tid_t::null;

/*********************************************************************
 *
 *  Constructors and destructor
 *
 *********************************************************************/
// FRJ: WARNING: xct_impl::commit has some of this code as well
#define USE_BLOCK_ALLOC_FOR_XCT_IMPL  1
#if USE_BLOCK_ALLOC_FOR_XCT_IMPL 
DECLARE_TLS(block_alloc<xct_impl>, xct_impl_pool);
#endif

struct lock_info_ptr {
    xct_lock_info_t* _ptr;
    
    lock_info_ptr() : _ptr(0) { }
    
    xct_lock_info_t* take() {
        if(xct_lock_info_t* rval = _ptr) {
            _ptr = 0;
            return rval;
        }
        return new xct_lock_info_t;
    }
    void put(xct_lock_info_t* ptr) {
        if(_ptr)
            delete _ptr;
        _ptr = ptr? ptr->reset_for_reuse() : 0;
    }
    
    ~lock_info_ptr() { put(0); }
};

DECLARE_TLS(lock_info_ptr, agent_lock_info);

xct_t::xct_t(
        sm_stats_info_t* stats, 
        timeout_in_ms timeout) : 
    __stats(stats),
    __saved_lockid_t(0),
    __saved_sdesc_cache_t(0),
    __saved_xct_log_t(0),
    _tid(_nxt_tid.atomic_incr()), 
    _timeout(timeout),
    i_this(0),    
    _lock_info(agent_lock_info->take()),    
    _lock_cache_enable(true)
{
    w_assert9(tid() <= _nxt_tid);

    _lock_info->set_tid(_tid);

#if USE_BLOCK_ALLOC_FOR_XCT_IMPL 
    i_this = new (*xct_impl_pool) xct_impl(this); // deleted when xct finishes
#else
    i_this = new xct_impl(this); // deleted when xct finishes
#endif

    put_in_order();
    me()->attach_xct(this);


    if (timeout_c() == WAIT_SPECIFIED_BY_THREAD) {
        // override in this case
        set_timeout(me()->lock_timeout());
    }
    w_assert9(timeout_c() >= 0 || timeout_c() == WAIT_FOREVER);

#ifndef SDESC_CACHE_PER_THREAD
    __saved_sdesc_cache_t = new_sdesc_cache_t(); // deleted when xct finishes
#endif /* SDESC_CACHE_PER_THREAD */
}

xct_t::xct_t(const tid_t& t, state_t s, const lsn_t& last_lsn,
             const lsn_t& undo_nxt, timeout_in_ms timeout) 
    :
    __stats(0),
    __saved_lockid_t(0),
    __saved_sdesc_cache_t(0),
    __saved_xct_log_t(0),
    _tid(t), 
    _timeout(timeout),
    i_this(0),    
    _lock_info(agent_lock_info->take()),    
    _lock_cache_enable(true)
{

    // Uses user(recovery)-provided tid
    _nxt_tid.atomic_assign_max(tid());

    _lock_info->set_tid(_tid);

#if USE_BLOCK_ALLOC_FOR_XCT_IMPL 
    i_this = new (*xct_impl_pool) xct_impl(this, s , last_lsn, undo_nxt); //deleted when xct ends
#else
    i_this = new xct_impl(this, s , last_lsn, undo_nxt); //deleted when xct ends
#endif
    put_in_order();
    /// Don't attach
    // sm.tcb()->xct = 0;
    w_assert1(me()->xct() == 0);

    if (timeout_c() == WAIT_SPECIFIED_BY_THREAD) {
        // override in this case
        set_timeout(me()->lock_timeout());
    }
    w_assert9(timeout_c() >= 0 || timeout_c() == WAIT_FOREVER);

#ifndef SDESC_CACHE_PER_THREAD
    __saved_sdesc_cache_t = new_sdesc_cache_t(); // deleted when thread detaches or xct finishes
#endif /* SDESC_CACHE_PER_THREAD */
}


xct_t::~xct_t() 
{ 

    w_assert9(__stats == 0);

    W_COERCE(acquire_xlist_mutex());

    // FRJ: WARNING: xct_impl::commit has this code as well!
    _xlink.detach();
    xct_t* xd = _xlist.last();
    _oldest_tid = xd ? xd->_tid : _nxt_tid;
    release_xlist_mutex();

    if(i_this) {
#if USE_BLOCK_ALLOC_FOR_XCT_IMPL 
        xct_impl_pool->destroy_object(i_this);
#else
        delete i_this;
#endif
        i_this = 0;
    }

    if(_lock_info) {
        agent_lock_info->put(_lock_info);
    }
    if(__saved_lockid_t)  { 
        delete[] __saved_lockid_t; 
        __saved_lockid_t=0; 
    }
    if(__saved_sdesc_cache_t) {         
        delete __saved_sdesc_cache_t;
        __saved_sdesc_cache_t=0; 
    }
    if(__saved_xct_log_t) { 
        delete __saved_xct_log_t; 
        __saved_xct_log_t=0; 
    }
}


ostream&
operator<<(ostream& o, const xct_t& x)
{
    /**\brief Callback function for dumping all the smthreads 
     * attached to an xct. 
     */
    class PrintSmthreadsOfXct : public SmthreadFunc
    {
        public:
        PrintSmthreadsOfXct(ostream& out, const xct_t* x) : o(out), xct(x) {};
        void operator()(const smthread_t& smthread)  {
            if (smthread.xct() == xct)  {
                o << "--------------------" << endl << smthread;
            }
        }
        private:
        ostream&    o;
        const xct_t*    xct;

    };

    o << *x.i_this << endl;
    PrintSmthreadsOfXct f(o, &x);
    smthread_t::for_each_smthread(f);

    return o;
}

#if W_DEBUG_LEVEL > 2
/* debugger-callable */
extern "C" void dumpXct(const xct_t *x) { if(x) { cout << *x <<endl;} }

/* help for debugger-callable dumpThreadById() below */
class PrintSmthreadById : public SmthreadFunc
{
    public:
        PrintSmthreadById(ostream& out, int i ) : o(out), _i(0) {
                _i = sthread_base_t::id_t(i);
        };
        void operator()(const smthread_t& smthread);
    private:
        ostream&        o;
        sthread_base_t::id_t                 _i;
};
void PrintSmthreadById::operator()(const smthread_t& smthread)
{
    if (smthread.id == _i)  {
        o << "--------------------" << endl << smthread;
    }
}

/* debugger-callable */
extern "C" void 
dumpThreadById(int i) { 
    PrintSmthreadById f(cout, i);
    smthread_t::for_each_smthread(f);
}
#endif 

xct_t::state_t
xct_t::state() const
{
    return i_this->state();
}

/*
 * clean up existing transactions -- called from ~ss_m, so
 * this should never be subject to multiple
 * threads using the xct list.
 * NANCY TODO: DOCUMENT: that proper use of the ss_m requires that the
 * thread that creates the ssm forks all other user threads, joins them
 * before deleting the ssm.
 */
int
xct_t::cleanup(bool dispose_prepared)
{
    bool        changed_list;
    int         nprepared = 0;
    xct_t*      xd;
    W_COERCE(acquire_xlist_mutex());
    do {
        /*
         *  We cannot delete an xct while iterating. Use a loop
         *  to iterate and delete one xct for each iteration.
         */
        xct_i i(false); // do acquire the list mutex. Noone
        // else should be iterating over the xcts at this point.
        changed_list = false;
        xd = i.next();
        if (xd) {
            // Release the mutex so we can delete the xd if need be...
            release_xlist_mutex();
            switch(xd->state()) {
            case xct_active: {
                    me()->attach_xct(xd);
                    /*
                     *  We usually want to shutdown cleanly. For debugging
                     *  purposes, it is sometimes desirable to simply quit.
                     *
                     *  NB:  if a vas has multiple threads running on behalf
                     *  of a tx at this point, it's going to run into trouble.
                     */
                    if (shutdown_clean) {
                        W_COERCE( xd->abort() );
                    } else {
                        W_COERCE( xd->dispose() );
                    }
                    delete xd;
                    changed_list = true;
                } 
                break;

            case xct_freeing_space:
            case xct_ended: {
                    DBG(<< xd->tid() <<"deleting " 
                            << " w/ state=" << xd->state() );
                    // xd->change_state(xct_freeing_space);
                    // TODO: NANCY: what was this all about?
                    delete xd;
                    changed_list = true;
                }
                break;

            case xct_prepared: {
                    if(dispose_prepared) {
                        me()->attach_xct(xd);
                        W_COERCE( xd->dispose() );
                        delete xd;
                        changed_list = true;
                    } else {
                        DBG(<< xd->tid() <<"keep -- prepared ");
                        nprepared++;
                    }
                } 
                break;

            default: {
                    DBG(<< xd->tid() <<"skipping " 
                            << " w/ state=" << xd->state() );
                }
                break;
            
            } // switch on xct state
            W_COERCE(acquire_xlist_mutex());
        } // xd not null
    } while (xd && changed_list);
    release_xlist_mutex();
    return nprepared;
}




/*********************************************************************
 *
 *  xct_t::num_active_xcts()
 *
 *  Return the number of active transactions (equivalent to the
 *  size of _xlist.
 *
 *********************************************************************/
w_base_t::uint4_t
xct_t::num_active_xcts()
{
    w_base_t::uint4_t num;
    W_COERCE(acquire_xlist_mutex());
    num = _xlist.num_members();
    release_xlist_mutex();
    return  num;
}



/*********************************************************************
 *
 *  xct_t::look_up(tid)
 *
 *  Find the record for tid and return it. If not found, return 0.
 *
 *********************************************************************/
xct_t* 
xct_t::look_up(const tid_t& tid)
{
    xct_t* xd;
    xct_i iter(true);

    while ((xd = iter.next())) {
        if (xd->tid() == tid) {
            return xd;
        }
    }
    return 0;
}


/*********************************************************************
 *
 *  xct_t::oldest_tid()
 *
 *  Return the tid of the oldest active xct.
 *
 *********************************************************************/
tid_t
xct_t::oldest_tid()
{
    return _oldest_tid;
}

bool                        
xct_t::is_extern2pc() 
const
{
    return i_this->is_extern2pc();
}

void
xct_t::set_coordinator(const server_handle_t &h) 
{
    i_this->set_coordinator(h);
}

const server_handle_t &
xct_t::get_coordinator() const
{
    return i_this->get_coordinator();
}

void
xct_t::change_state(state_t new_state)
{
    i_this->change_state(new_state);
}

rc_t
xct_t::add_dependent(xct_dependent_t* dependent)
{
    return i_this->add_dependent(dependent);
}

rc_t
xct_t::remove_dependent(xct_dependent_t* dependent)
{
    return i_this->remove_dependent(dependent);
}

bool
xct_t::find_dependent(xct_dependent_t* ptr)
{
    return i_this->find_dependent(ptr);
}

rc_t 
xct_t::prepare()
{
    return i_this->prepare();
}

rc_t
xct_t::log_prepared(bool in_chkpt)
{
    return i_this->log_prepared(in_chkpt);
}

rc_t
xct_t::abort(bool save_stats_structure /* = false */)
{
    if(is_instrumented() && !save_stats_structure) {
        delete __stats;
        __stats = 0;
    }
    return i_this->abort();
}

rc_t 
xct_t::enter2pc(const gtid_t &g)
{
    return i_this->enter2pc(g);
}

/*********************************************************************
 *
 *  xct_t::recover2pc(...)
 *
 *  Locate a prepared tx with this global tid
 *
 *********************************************************************/

rc_t 
xct_t::recover2pc(const gtid_t &g,
        bool        /*mayblock*/,
        xct_t        *&xd)
{
    w_list_i<xct_t, queue_based_lock_t> i(_xlist);
    while ((xd = i.next()))  {
        if( xd->state() == xct_prepared ) {
            if(xd->gtid() &&
                *(xd->gtid()) == g) {
                // found
                // TODO  try to reach the coordinator
                return RCOK;
            }
        }
    }
    return RC(eNOSUCHPTRANS);
}

/*********************************************************************
 *
 *  xct_t::query_prepared(...)
 *
 *  given a buffer into which to write global transaction ids, fill
 *  in those for all prepared tx's
 *
 *********************************************************************/
rc_t 
xct_t::query_prepared(int list_len, gtid_t list[])
{
    w_list_i<xct_t, queue_based_lock_t> iter(_xlist);
    int i=0;
    xct_t *xd;
    while ((xd = iter.next()))  {
        if( xd->state() == xct_prepared ) {
            if(xd->gtid()) {
                if(i < list_len) {
                    list[i++]=*(xd->gtid());
                } else {
                    return RC(fcFULL);
                }
            // } else {
                // was not external 2pc
            }
        }
    }
    return RCOK;
}

/*********************************************************************
 *
 *  xct_t::query_prepared(...)
 *
 *  Tell how many prepared tx's there are.
 *
 *********************************************************************/
rc_t 
xct_t::query_prepared(int &numtids)
{
    w_list_i<xct_t, queue_based_lock_t> iter(_xlist);
    numtids=0;
    xct_t *xd;
    while ((xd = iter.next()))  {
        if( xd->state() == xct_prepared ) {
            numtids++;
        }
    }
    return RCOK;
}

rc_t
xct_t::save_point(lsn_t& lsn)
{
    return i_this->save_point(lsn);
}

rc_t
xct_t::dispose()
{
    delete __stats;
    __stats = 0;
    return i_this->dispose();
}


rc_t 
xct_t::get_logbuf(logrec_t*& ret, const page_p *page)
{
    return i_this->get_logbuf(ret, page);
}

rc_t 
xct_t::give_logbuf(logrec_t* l, const page_p *page)
{
    return i_this->give_logbuf(l, page);
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
xct_t::release_anchor( bool and_compensate )
{
    i_this->release_anchor(and_compensate);
}
const lsn_t& 
xct_t::anchor(bool grabit)
{
    return i_this->anchor(grabit);
}
void 
xct_t::compensate_undo(const lsn_t& lsn)
{
    i_this->compensate_undo(lsn);
}

void 
xct_t::compensate(const lsn_t& lsn, bool undoable)
{
    i_this->compensate(lsn, undoable);
}

/*********************************************************************
 *
 *  xct_t::rollback(savept)
 *
 *  Rollback transaction up to "savept".
 *
 *********************************************************************/
rc_t
xct_t::rollback(lsn_t save_pt)
{
    return i_this->rollback(save_pt);
}


smlevel_0::concurrency_t                
xct_t::get_lock_level()  
{ 
    return i_this->get_lock_level();
}

void                           
xct_t::lock_level(concurrency_t l) 
{
    w_assert9(i_this->one_thread_attached());
    i_this->lock_level(l);
}

int 
xct_t::num_threads() 
{
    return i_this->num_threads();
}

void 
xct_t::attach_thread() 
{
    i_this->attach_thread();
}


void 
xct_t::detach_thread() 
{
    i_this->detach_thread();
}

w_rc_t
xct_t::lockblock(timeout_in_ms timeout)
{
    return i_this->lockblock(timeout);
}

void
xct_t::lockunblock()
{
    i_this->lockunblock();
}

void
xct_t::force_nonblocking()
{
    _lock_info->set_nonblocking();
}

void
xct_t::acquire_1thread_xct_mutex() // default: true
{
    if(_1thread_xct.is_mine(&localns::_xct_t_me_node)) {
        return;
    }
    if( _1thread_xct.attempt(&localns::_xct_t_me_node) ) {
        // success
        return; 
    }
    INC_TSTAT(await_1thread_xct);
    (void) _1thread_xct.acquire(&localns::_xct_t_me_node);
}

void
xct_t::release_1thread_xct_mutex()
{
    DBGX( << " release xct mutex");

    _1thread_xct.release(localns::_xct_t_me_node);
}

rc_t
xct_t::commit(bool lazy)
{
    // w_assert9(one_thread_attached());
    // removed because a checkpoint could
    // be going on right now.... see comments
    // in log_prepared and chkpt.cpp

    return i_this->commit(t_normal | (lazy ? t_lazy : t_normal));
}


rc_t
xct_t::chain(bool lazy)
{
    w_assert9(i_this->one_thread_attached());
    return i_this->commit(t_chain | (lazy ? t_lazy : t_chain));
}


bool
xct_t::lock_cache_enabled() 
{
    bool volatile* result = &_lock_cache_enable;
    return *result;
}

bool
xct_t::set_lock_cache_enable(bool enable)
{
    bool result;
    membar_exit();
    result = atomic_swap_8((unsigned char*) &_lock_cache_enable, enable);
    membar_enter();
    return result;
}

sdesc_cache_t*          
xct_t::new_sdesc_cache_t()
{
    /* NB: this gets stored in the thread tcb(), so it's
    * a per-thread cache
    */
    sdesc_cache_t*          _sdesc_cache = new sdesc_cache_t;
    if (!_sdesc_cache) W_FATAL(eOUTOFMEMORY);
    return _sdesc_cache;
}

xct_log_t*          
xct_t::new_xct_log_t()
{
    xct_log_t*  l = new xct_log_t; 
    if (!l) W_FATAL(eOUTOFMEMORY);
    return l;
}

lockid_t*          
xct_t::new_lock_hierarchy()
{
    lockid_t*          l = new lockid_t [lockid_t::NUMLEVELS];
    if (!l) W_FATAL(eOUTOFMEMORY);
    return l;
}

sdesc_cache_t*                    
xct_t::sdesc_cache() const
{
#ifdef SDESC_CACHE_PER_THREAD
    return me()->sdesc_cache();
#else
    return __saved_sdesc_cache_t;
#endif /* SDESC_CACHE_PER_THREAD */
}

/**\brief Used by smthread upon attach_xct() to avoid excess heap activity.
 *
 * \details
 * If the xct has a stashed copy of the caches, hand them over to the
 * calling smthread. If not, allocate some off the stack.
 */
void                        
xct_t::steal(lockid_t*&l, sdesc_cache_t*&
#ifdef SDESC_CACHE_PER_THREAD
        s
#endif
        , xct_log_t*&x)
{
    /* See comments in smthread_t::new_xct() */
    acquire_1thread_xct_mutex();
    if( (l = __saved_lockid_t) != 0 ) {
        __saved_lockid_t = 0;
    } else {
        l = new_lock_hierarchy(); // deleted when thread detaches or
                                  // xct goes away
    }

#ifdef SDESC_CACHE_PER_THREAD
    if( (s = __saved_sdesc_cache_t) ) {
         __saved_sdesc_cache_t = 0;
    } else {
        s = new_sdesc_cache_t(); // deleted when thread detaches or xct finishes
    }
#endif /* SDESC_CACHE_PER_THREAD */

    if( (x = __saved_xct_log_t) ) {
        __saved_xct_log_t = 0;
    } else {
        x = new_xct_log_t(); // deleted when thread detaches or xct finishes
    }
    release_1thread_xct_mutex();
}

/**\brief Used by smthread upon detach_xct() to avoid excess heap activity.
 *
 * \details
 * If the xct has a stashed copy of the caches, free the caches
 * passed in, otherwise, hang onto them to hand over to the next
 * thread that attaches to this xct.
 */
void                        
xct_t::stash(lockid_t*&l, sdesc_cache_t*&
#ifdef SDESC_CACHE_PER_THREAD
        s
#endif
        , xct_log_t*&x)
{
    /* See comments in smthread_t::new_xct() */
    acquire_1thread_xct_mutex();
    if(__saved_lockid_t != (lockid_t *)0)  { 
        DBGX(<<"stash: delete " << l);
        if(l) {
            delete[] l; 
        }
    } else { 
        __saved_lockid_t = l; 
        DBGX(<<"stash: allocated " << l);
    }
    l = 0;

#ifdef SDESC_CACHE_PER_THREAD
    if(__saved_sdesc_cache_t) {
        DBGX(<<"stash: delete " << s);
        delete s; 
    }
    else { __saved_sdesc_cache_t = s;}
    s = 0;
#endif /* SDESC_CACHE_PER_THREAD */

    if(__saved_xct_log_t) {
        DBGX(<<"stash: delete " << x);
        delete x; 
    }
    else { __saved_xct_log_t = x; }
    x = 0;
    release_1thread_xct_mutex();
}
    


rc_t                        
xct_t::check_lock_totals(int nex, int nix, int nsix, int nextents) const
{
    int        num_EX, num_IX, num_SIX, num_extents;
    W_DO(lock_info()->get_lock_totals( num_EX, num_IX, num_SIX, num_extents));
    if( nex != num_EX || nix != num_IX || nsix != num_SIX) {
        // IX and SIX are the same for this purpose,
        // but whereas what was SH+IX==SIX when the
        // prepare record was written, will be only
        // IX when acquired implicitly as a side effect
        // of acquiring the EX locks.
        // NB: taking this out because it seems that even
        // in the absence of escalation, and if it's
        // not doing a lock_force, the numbers could be off.

        // w_assert1(nix + nsix <= num_IX + num_SIX );
        w_assert1(nex <= num_EX);

        if(nextents != num_extents) {
            smlevel_0::errlog->clog 
            << "FATAL: " <<endl
            << "nextents logged in xct_prepare_fi_log:" << nextents <<endl
            << "num_extents locked via "
            << "xct_prepare_lk_log and xct_prepare_alk_logs : " << num_extents
            << endl;
            lm->dump(smlevel_0::errlog->clog);
        }
        w_assert1(nextents == num_extents);
    }
    return RCOK;
}

rc_t                        
xct_t::obtain_locks(lock_mode_t mode, int num, const lockid_t *locks)
{
    // Turn off escalation for recovering prepared xcts --
    // so the assertions will work.
    sm_escalation_t SAVE;

#if W_DEBUG_LEVEL > 2
    int        b_EX, b_IX, b_SIX, b_extents;
    W_DO(lock_info()->get_lock_totals(b_EX, b_IX, b_SIX, b_extents));
    DBG(<< b_EX << "+" << b_IX << "+" << b_SIX << "+" << b_extents);
#endif 

    int  i;
    rc_t rc;

    for (i=0; i<num; i++) {
        DBG(<<"Obtaining lock : " << locks[i] << " in mode " << int(mode));
#if W_DEBUG_LEVEL > 2
        int        bb_EX, bb_IX, bb_SIX, bb_extents;
        W_DO(lock_info()->get_lock_totals(bb_EX, bb_IX, bb_SIX, bb_extents));
        DBG(<< bb_EX << "+" << bb_IX << "+" << bb_SIX << "+" << bb_extents);
#endif 

        rc =lm->lock(locks[i], mode, t_long, WAIT_IMMEDIATE);
        if(rc.is_error()) {
            lm->dump(smlevel_0::errlog->clog);
            smlevel_0::errlog->clog << "can't obtain lock " <<rc <<endl;
            W_FATAL(eINTERNAL);
        }
        {
            int        a_EX, a_IX, a_SIX, a_extents;
            W_DO(lock_info()->get_lock_totals(a_EX, a_IX, a_SIX, a_extents));
            DBG(<< a_EX << "+" << a_IX << "+" << a_SIX << "+" << a_extents);
            switch(mode) {
                case EX:
                    w_assert9((bb_EX + 1) == (a_EX)); 
                    break;
                case IX:
                case SIX:
                    w_assert9((bb_IX + 1) == (a_IX));
                    break;
                default:
                    break;
                    
            }
        }
    }

    return RCOK;
}

rc_t                        
xct_t::obtain_one_lock(lock_mode_t mode, const lockid_t &lock)
{
    // Turn off escalation for recovering prepared xcts --
    // so the assertions will work.
    DBG(<<"Obtaining 1 lock : " << lock << " in mode " << int(mode));

    sm_escalation_t SAVE;
    rc_t rc;
#if W_DEBUG_LEVEL > 2
    int        b_EX, b_IX, b_SIX, b_extents;
    W_DO(lock_info()->get_lock_totals(b_EX, b_IX, b_SIX, b_extents));
    DBG(<< b_EX << "+" << b_IX << "+" << b_SIX << "+" << b_extents);
#endif 
    rc = lm->lock(lock, mode, t_long, WAIT_IMMEDIATE);
    if(rc.is_error()) {
        lm->dump(smlevel_0::errlog->clog);
        smlevel_0::errlog->clog << "can't obtain lock " <<rc <<endl;
        W_FATAL(eINTERNAL);
    }
#if W_DEBUG_LEVEL > 2
    {
        int        a_EX, a_IX, a_SIX, a_extents;
        W_DO(lock_info()->get_lock_totals(a_EX, a_IX, a_SIX, a_extents));
        DBG(<< a_EX << "+" << a_IX << "+" << a_SIX << "+" << a_extents);

        // It could be a repeat, so let's do this:
        if(b_EX + b_IX + b_SIX == a_EX + a_IX + a_SIX) {
            DBG(<<"DIDN'T GET LOCK " << lock << " in mode " << int(mode));
        } else  {
            switch(mode) {
                case EX:
                    w_assert9((b_EX +  1) == (a_EX));
                    break;
                case IX:
                    w_assert9((b_IX + 1) == (a_IX));
                    break;
                case SIX:
                    w_assert9((b_SIX + 1) == (a_SIX));
                    break;
                default:
                    break;
            }
        }
    }
#endif 
    return RCOK;
}

NORET
sm_escalation_t::sm_escalation_t( int4_t p, int4_t s, int4_t v) 
{
    w_assert9(me()->xct());
    me()->xct()->GetEscalationThresholds(_p, _s, _v);
    me()->xct()->SetEscalationThresholds(p, s, v);
}
NORET
sm_escalation_t::~sm_escalation_t() 
{
    w_assert9(me()->xct());
    me()->xct()->SetEscalationThresholds(_p, _s, _v);
}


/**\brief Set the log state for this xct/thread pair to the value \e s.
 */
smlevel_0::switch_t 
xct_t::set_log_state(switch_t s) 
{
    xct_log_t *mine = me()->xct_log();

    switch_t old = (mine->xct_log_is_off()? OFF: ON);

    if(s==OFF) mine->set_xct_log_off();

    else mine->set_xct_log_on();

    return old;
}

void
xct_t::restore_log_state(switch_t s) 
{
    (void) set_log_state(s);
}


tid_t
xct_t::youngest_tid()
{
    ASSERT_FITS_IN_LONGLONG(tid_t);
    return _nxt_tid;
}

void
xct_t::update_youngest_tid(const tid_t &t)
{
    _nxt_tid.atomic_assign_max(t);
}


void
xct_t::force_readonly() 
{
    acquire_1thread_xct_mutex();
    i_this->force_readonly();
    release_1thread_xct_mutex();
}

void 
xct_t::put_in_order() {
    W_COERCE(acquire_xlist_mutex());
    _xlist.put_in_order(this);
    _oldest_tid = _xlist.last()->_tid;
    release_xlist_mutex();

#if W_DEBUG_LEVEL > 2
    W_COERCE(acquire_xlist_mutex());
    {
        // make sure that _xlist is in order
        w_list_i<xct_t, queue_based_lock_t> i(_xlist);
        tid_t t = tid_t::null;
        xct_t* xd;
        while ((xd = i.next()))  {
            w_assert1(t < xd->_tid);
        }
        w_assert1(t <= _nxt_tid);
    }
    release_xlist_mutex();
#endif 
}

void                        
xct_t::SetEscalationThresholds(int4_t toPage, int4_t toStore, int4_t toVolume)
{
    i_this->SetEscalationThresholds(toPage, toStore, toVolume);
}

void                        
xct_t::GetEscalationThresholds(int4_t &toPage, int4_t &toStore, int4_t &toVolume)
{
    i_this->GetEscalationThresholds(toPage, toStore, toVolume);
}

smlevel_0::fileoff_t
xct_t::get_log_space_used() const
{
    return i_this->_log_bytes_used
	+ i_this->_log_bytes_rsvd
	+ i_this->_log_bytes_rsvd;
}

rc_t
xct_t::wait_for_log_space(fileoff_t amt) {
    rc_t rc = RCOK;
    if(log) {
	fileoff_t still_needed = amt;
	// check whether we even need to wait...
	if(log->reserve_space(still_needed)) {
	    still_needed = 0;
	}
	else {
	    timeout_in_ms timeout = first_lsn().valid()? 100 : WAIT_FOREVER;
	    fprintf(stderr, "%s:%d: first_lsn().valid()? %d	timeout=%d\n",
		    __FILE__, __LINE__, first_lsn().valid(), timeout);
	    rc = log->wait_for_space(still_needed, timeout);
	    if(rc.is_error()) {
		//rc = RC(eOUTOFLOGSPACE);
	    }
	}
	
	// update our reservation with whateer we got
	i_this->_log_bytes_rsvd += amt - still_needed;
    }
    return rc;
}

const lsn_t&
xct_t::last_lsn() const
{
    return i_this->last_lsn();
}

void
xct_t::set_last_lsn( const lsn_t&l)
{
    i_this->set_last_lsn(l);
}

const lsn_t&
xct_t::first_lsn() const
{
    return i_this->first_lsn();
}

void
xct_t::set_first_lsn(const lsn_t &l) 
{
    i_this->set_first_lsn(l);
}

const lsn_t&
xct_t::undo_nxt() const
{
    return i_this->undo_nxt();
}

void
xct_t::set_undo_nxt(const lsn_t &l) 
{
    i_this->set_undo_nxt(l);
}

const logrec_t*
xct_t::last_log() const
{
    return i_this->last_log();
}

const gtid_t*                   
xct_t::gtid() const 
{
    return i_this->gtid();
}

vote_t
xct_t::vote() const
{
    return i_this->vote();
}

void 
xct_t::AddStoreToFree(const stid_t& stid)
{
    i_this->AddStoreToFree(stid);
}

void 
xct_t::AddLoadStore(const stid_t& stid)
{
    i_this->AddLoadStore(stid);
}

const int4_t *
xct_t::GetEscalationThresholdsArray()
{
    return i_this->GetEscalationThresholdsArray();
}


void
xct_t::dump(ostream &out) 
{
    W_COERCE(acquire_xlist_mutex());
    out << "xct_t: "
            << _xlist.num_members() << " transactions"
        << endl;
    w_list_i<xct_t, queue_based_lock_t> i(_xlist);
    xct_t* xd;
    while ((xd = i.next()))  {
        out << "********************" << endl;
        out << *xd << endl;
    }
    release_xlist_mutex();
}

void                        
xct_t::set_timeout(timeout_in_ms t) 
{ 
    _timeout = t; 
}
void                         
xct_t::num_extents_marked_for_deletion( base_stat_t &num) const
{
    i_this->num_extents_marked_for_deletion(num);
}

w_rc_t 
xct_log_warn_check_t::check(xct_t *& /*_victim */) 
{
    /* FRJ: TODO: use this with the new log reservation code. One idea
       would be to return eLOGSPACEWARN if this transaction (or some
       other?) has been forced nonblocking. Another would be to hook
       in with the LOG_RESERVATIONS stuff and warn if transactions are
       having to wait to acquire log space. Yet another way would be
       to hook in with the checkpoint thread and see if it feels
       stressed...
     */
    return RCOK;
}

rc_t
xct_t::check_one_thread_attached() const
{
    return i_this->check_one_thread_attached();
}
