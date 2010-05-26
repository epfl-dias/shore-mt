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

/*<std-header orig-src='shore'>

 $Id: log.cpp,v 1.127.2.19 2010/03/19 22:20:24 nhall Exp $

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
#define LOG_C
#ifdef __GNUG__
#   pragma implementation
#endif

#include <sm_int_1.h>
#include <srv_log.h>
#include "logdef_gen.cpp"
#include "crash.h"
#include <algorithm> // for std::swap
#include <stdio.h> // snprintf

// like page_p::tag_name, but doesn't W_FATAL on garbage
static char const* page_tag_to_str(int page_tag) {
    switch (page_tag) {
    case page_p::t_extlink_p: 
        return "t_extlink_p";
    case page_p::t_stnode_p:
        return "t_stnode_p";
    case page_p::t_keyed_p:
        return "t_keyed_p";
    case page_p::t_btree_p:
        return "t_btree_p";
    case page_p::t_rtree_p:
        return "t_rtree_p";
    case page_p::t_file_p:
        return "t_file_p";
    default:
        return "<garbage>";
    }
}

// make dbx print log records at all, and in a readable way
char const* 
db_pretty_print(logrec_t const* rec, int /*i=0*/, char const* /*s=0*/) 
{
    static char tmp_data[1024];
    
    // what categories?
    typedef char const* str;
    str undo=(rec->is_undo()? "undo " : ""),
        redo=(rec->is_redo()? "redo " : ""),
        cpsn=(rec->is_cpsn()? "cpsn " : ""),
        logical=(rec->is_logical()? "logical " : "");

    char scratch[100];
    str extra = scratch;
    switch(rec->type()) {

    case logrec_t::t_page_mark:
    case logrec_t::t_page_reclaim:
        snprintf(scratch, sizeof(scratch), "    { idx=%d, len=%d }\n",
                 *(short*) rec->data(), *(short*) (rec->data()+sizeof(short)));
        break;
    default:
        extra = "";
        break;
    }
    
    snprintf(tmp_data, sizeof(tmp_data),
             "{\n"
             "    _len = %d\n"
             "    _type = %s\n"
             "    _cat = %s%s%s%s\n"
             "    _tid = %d.%d\n"
             "    _pid = %d.%d.%d\n"
             "    _page_tag = %s\n"
             "    _prev = %d.%lld\n"
             "    _ck_lsn = %d.%lld\n"
             "%s"
             "}",
             rec->length(),
             rec->type_str(),
             logical, undo, redo, cpsn,
             rec->tid().get_hi(), rec->tid().get_lo(),
             rec->construct_pid().vol().vol, 
                     rec->construct_pid().store(), 
                     rec->construct_pid().page,
             page_tag_to_str(rec->tag()),
             (rec->prev().hi()), (long long int)(rec->prev().lo()),
             (rec->get_lsn_ck().hi()), (long long int)(rec->get_lsn_ck().lo()),
             extra
             );
    return tmp_data;
}


class flush_daemon_thread_t : public smthread_t {
    ringbuf_log* _log;
public:
    flush_daemon_thread_t(ringbuf_log* log) : 
         smthread_t(t_regular, "flush_daemon", WAIT_NOT_USED), _log(log) { }

    virtual void run() { _log->flush_daemon(); }
};

// Make sure to allocate enough extra space that log wraps can always fit
ringbuf_log::ringbuf_log(long bsize)
    : _waiting_for_space(false), _waiting_for_flush(false),
      _start(0), _end(0),
      _segsize(ceil(bsize, SEGMENT_SIZE)), 
      _blocksize(BLOCK_SIZE),
      _buf(new char[_segsize]),
      _shutting_down(false)
{
    DO_PTHREAD(pthread_mutex_init(&_wait_flush_lock, NULL));
    DO_PTHREAD(pthread_cond_init(&_wait_cond, NULL));
    DO_PTHREAD(pthread_cond_init(&_flush_cond, NULL));
    /* Create thread (1) to flush the log */
    _flush_daemon = new flush_daemon_thread_t(this);
}

void ringbuf_log::start_flush_daemon() 
{
    _flush_daemon->fork();
}

void
ringbuf_log::shutdown() // GNATS_11 fix is to add this method
{ 
    // gnats 52:  RACE: We set _shutting_down and signal between the time
    // the daemon checks _shutting_down (false) and waits.
    //
    // So we need to notice if the daemon got the message.
    // It tells us it did by resetting the flag after noticing
    // that the flag is true.
    // There should be no interference with these two settings
    // of the flag since they happen strictly in that sequence.
    //
    _shutting_down = true;
    while (*&_shutting_down) {
        CRITICAL_SECTION(cs, _wait_flush_lock);
        // Use signal since the only thread that should be waiting 
        // on the _flush_cond is the log flush daemon.
        DO_PTHREAD(pthread_cond_broadcast(&_flush_cond));
    }
    _flush_daemon->join();
    delete _flush_daemon;
    _flush_daemon=NULL;
}

ringbuf_log::~ringbuf_log() 
{
    w_assert1(_durable_lsn == _curr_lsn);
    delete [] _buf;
    DO_PTHREAD(pthread_mutex_destroy(&_wait_flush_lock));
    DO_PTHREAD(pthread_cond_destroy(&_wait_cond));
    DO_PTHREAD(pthread_cond_destroy(&_flush_cond));
}
#if W_DEBUG_LEVEL > 1
// A debugging structure into which we write some debugging info
// so that when we croak we can sort of see what was going on for GNATS 58
struct tracer {
    int _line; // __LINE__
    int _end; 
    int _start; 
    int _ce_end; 
    int _ce_start; 
public:
    void set(int l, int e, int s ,int cee , int ces ) 
    {
        _line=l;
        _end=e;
        _start = s;
        _ce_end = cee;
        _ce_start = ces;
    }
};
__thread   int lct(0);
__thread tracer TR[0x100]; // 256
#define CLEART { ct=0; }
#define TRACE(l,i,u,c,p) TR[(lct++)&0xff].set(l,i,u,c,p)
extern "C" void logdumpT()
{
    for(int i=0; i < lct; i++)
    {
        cout  << " line=" << TR[i]._line 
            << " end=" << TR[i]._end 
            << " start=" << TR[i]._start 
            << " current_epoch.end=" << TR[i]._ce_end 
            << " current_epoch.start=" << TR[i]._ce_start 
            << endl;
    }
}
#else
#define CLEART 
#define TRACE(l,i,u,c,p)
#endif

rc_t ringbuf_log::insert(logrec_t &rec, lsn_t* rlsn) 
{
   INC_TSTAT(log_inserts);
    
   long size = rec.length();
   char const* data = (char const*) &rec;
   w_assert1((unsigned long)(size) <= sizeof(logrec_t));


  /* Copy our data into the buffer and update/create epochs. Note
     that, while we may race the flush daemon to update the epoch
     record, it will not touch the buffer until after we succeed so
     there is no race with memcpy(). If we do lose an epoch update
     race, it is only because the flush changed old_epoch.begin to
     equal old_epoch.end. This mutex ensures we don't race with any
     other inserts.
   */
  CRITICAL_SECTION(ics, _insert_lock);
  
  /* make sure there's actually space available, accounting for the
     fact that flushes always work with full blocks.
   */
  while(*&_waiting_for_space || 
          *&_end - *&_start + size > _segsize - 2*_blocksize) 
  {
      ics.pause();
      {
          CRITICAL_SECTION(cs, _wait_flush_lock);
          while(*&_end - *&_start + size > _segsize - 2*_blocksize) {
              _waiting_for_space = true;
              // Use signal since the only thread that should be waiting 
              // on the _flush_cond is the log flush daemon.
              DO_PTHREAD(pthread_cond_signal(&_flush_cond));
              DO_PTHREAD(pthread_cond_wait(&_wait_cond, &_wait_flush_lock));
          }
      }
      ics.resume();
  }

  /* _curr_lsn, _cur_epoch.end, and _end are all strongly related;
     _cur_epoch.end and _end are convenience variables to avoid doing
     expensive operations like modulus and division on the _curr_lsn:

     _cur_epoch.end is the buffer position for the current lsn
     
     _end is the non-wrapping version of _cur_epoch.end: at log init
     time it is set to a value in the range [0, _segsize) and is
     incremented by every log write for the lifetime of the database.

     _start and _durable_lsn have the same relationship as _end and
     _curr_lsn
   */
    w_assert2(_cur_epoch.end >= 0 && _cur_epoch.end <= _segsize);
    w_assert2(_cur_epoch.end % _segsize == _curr_lsn.lo() % _segsize);
    w_assert2(_cur_epoch.end % _segsize == _end % _segsize);
    w_assert2(_end >= _start);
    w_assert2(_end - _start <= _segsize - 2*_blocksize);


    long end = _cur_epoch.end;
    long new_end = end + size;
    long spillsize = new_end - _segsize;
    lsn_t curr_lsn = _curr_lsn;
    lsn_t next_lsn = _cur_epoch.base_lsn + new_end;
    TRACE(__LINE__,_end, _start, _cur_epoch.end, _cur_epoch.start);
    if(spillsize <= 0) {
        // normal insert
        rec.set_lsn_ck(curr_lsn);
        memcpy(_buf+end, data, size);

        // update epoch. no need for a lock if we just increment its end
        _cur_epoch.end = new_end;
        TRACE(__LINE__,_end, _start, _cur_epoch.end, _cur_epoch.start);
    }
    else if(next_lsn.lo() <= _partition_data_size) {
        // wrap within a partition. next_lsn is still valid but not new_end
        long partsize = size - spillsize;
        rec.set_lsn_ck(curr_lsn);
        memcpy(_buf+end, data, partsize);
        memcpy(_buf, data+partsize, spillsize);
        
        TRACE(__LINE__,_end, _start, _cur_epoch.end, _cur_epoch.start);
        // update epochs
        CRITICAL_SECTION(cs, _flush_lock);
        w_assert3(_old_epoch.start == _old_epoch.end);
        _old_epoch = epoch(_cur_epoch.base_lsn, _cur_epoch.base, 
                    _cur_epoch.start, _segsize);
        _cur_epoch.base_lsn += _segsize;
        _cur_epoch.base += _segsize;
        _cur_epoch.start = 0;
        _cur_epoch.end = new_end = spillsize;
        TRACE(__LINE__,_end, _start, _cur_epoch.end, _cur_epoch.start);
    }
    else {
        /* new partition! need to update next_lsn/new_end to reflect this

	   LOG_RESERVATIONS

	   Also, consume whatever (now-unusable) space was left in the
	   old partition so it doesn't seem to be available.
	 */
	long leftovers = _partition_data_size - curr_lsn.lo();
	w_assert2(leftovers >= 0);
	if(leftovers && !reserve_space(leftovers))
	    return RC(eOUTOFLOGSPACE);
	
        curr_lsn = first_lsn(next_lsn.hi()+1);
        next_lsn = curr_lsn + size;
        long new_base = _cur_epoch.base + _segsize;
        rec.set_lsn_ck(curr_lsn);
        memcpy(_buf, data, size);

        TRACE(__LINE__,_end, _start, _cur_epoch.end, _cur_epoch.start);
        // update epochs
        CRITICAL_SECTION(cs, _flush_lock);
        w_assert3(_old_epoch.start == _old_epoch.end);
        _old_epoch = _cur_epoch;
        _cur_epoch = epoch(curr_lsn, new_base, 0, new_end=size);
        TRACE(__LINE__,_end, _start, _cur_epoch.end, _cur_epoch.start);
    }

    // all done. let the world know
    if(rlsn) *rlsn = curr_lsn;
    _curr_lsn = next_lsn;
    _end = _cur_epoch.base + new_end;
    
    TRACE(__LINE__,_end, _start, _cur_epoch.end, _cur_epoch.start);
    w_assert2(_cur_epoch.end >= 0 && _cur_epoch.end <= _segsize);
    w_assert2(_cur_epoch.end % _segsize == _curr_lsn.lo() % _segsize);
    w_assert2(_cur_epoch.end % _segsize == _end % _segsize);
    w_assert2(_end >= _start);
    w_assert2(_end - _start <= _segsize - 2*_blocksize);

#ifdef USING_VALGRIND
    // I did once have this at the beginning but then we
    // croak because we haven't called set_lsn_ck yet
    if(RUNNING_ON_VALGRIND)
    {
        check_definedness(&rec, rec.length());
        check_valgrind_errors(__LINE__, __FILE__);
    }
#endif

    ADD_TSTAT(log_bytes_generated,size);
    return RCOK;
}

rc_t ringbuf_log::flush(lsn_t lsn) 
{
    ASSERT_FITS_IN_POINTER(lsn_t);
    // else our reads to _durable_lsn would be unsafe

    // don't let them flush past end of log -- they might wait forever...

    lsn = std::min(lsn, (*&_curr_lsn)+ -1);
    
    // already durable?
    if(lsn >= *&_durable_lsn) {
        CRITICAL_SECTION(cs, _wait_flush_lock);
        while(lsn >= *&_durable_lsn) {
            *&_waiting_for_flush = true;
            // Use signal since the only thread that should be waiting 
            // on the _flush_cond is the log flush daemon.
            DO_PTHREAD(pthread_cond_signal(&_flush_cond));
            DO_PTHREAD(pthread_cond_wait(&_wait_cond, &_wait_flush_lock));
        }
    } else {
        INC_TSTAT(log_dup_sync_cnt);
    }
    return RCOK;
}

void ringbuf_log::flush_daemon() 
{
    /* Algorithm: attempt to flush the buffer XFER_SIZE bytes at a
       time. If we empty out the buffer, block until either enough
       bytes get written or a thread specifically requests a flush.
     */
    lsn_t last_flush_lsn;
    bool success = false;
    while(1) {

        // wait for a kick. Kicks come at regular intervals from
        // inserts, but also at arbitrary times when threads request a
        // flush.
        {
            CRITICAL_SECTION(cs, _wait_flush_lock);
            if(success && (*&_waiting_for_space || *&_waiting_for_flush)) {
                _waiting_for_flush = _waiting_for_space = false;
                DO_PTHREAD(pthread_cond_broadcast(&_wait_cond)); // wake up anyone waiting on log flush
            }

            if(*&_shutting_down) {
                _shutting_down = false;
                break;
            }
        
            // sleep. We don't care if we get a spurious wakeup
            if(!success && !*&_waiting_for_space && !*&_waiting_for_flush) {
                // Use signal since the only thread that should be waiting 
                // on the _flush_cond is the log flush daemon.
                DO_PTHREAD(pthread_cond_wait(&_flush_cond, &_wait_flush_lock));
                INC_TSTAT(log_sync_cnt);
            }
        }

        lsn_t lsn = flush_cycle(last_flush_lsn);
        success = (lsn != last_flush_lsn);
        last_flush_lsn = lsn;
    }

    // make sure the buffer is completely empty before leaving...
    for(lsn_t lsn; 
        (lsn=flush_cycle(last_flush_lsn)) != last_flush_lsn; 
        last_flush_lsn=lsn) ;
}

lsn_t ringbuf_log::flush_cycle(lsn_t old_mark) 
{
    // anything new?
    if(_curr_lsn == old_mark)
        return old_mark;
    
    // flush
    lsn_t base_lsn_before, base_lsn_after;
    long base, start1, end1, start2, end2;
    {
        CRITICAL_SECTION(cs, _flush_lock);
        base_lsn_before = _old_epoch.base_lsn;
        base_lsn_after = _cur_epoch.base_lsn;
        base = _cur_epoch.base;
        if(_old_epoch.start == _old_epoch.end) {
            // no wrap -- flush only the new
            start2 = _cur_epoch.start;
            end2 = _cur_epoch.end;            
            _cur_epoch.start = end2;

            start1 = start2; // fake start1 so the start_lsn calc below works
            end1 = start2;
            
            base_lsn_before = base_lsn_after;
        }
        else if(base_lsn_before.file() == base_lsn_after.file()) {
            // wrapped within partition -- flush both
            start2 = _cur_epoch.start;
            end2 = _cur_epoch.end;            
            _cur_epoch.start = end2;

            start1 = _old_epoch.start;
            end1 = _old_epoch.end;
            _old_epoch.start = end1;
            
            w_assert1(base_lsn_before + _segsize == base_lsn_after);
        }
        else {
            // new partition -- flushing only the old
            start2 = 0;
            end2 = 0; // don't fake end2 because end_lsn needs to see '0' 

            start1 = _old_epoch.start;
            end1 = _old_epoch.end;
            _old_epoch.start = end1;

            w_assert1(base_lsn_before.file()+1 == base_lsn_after.file());
        }
    }

    lsn_t start_lsn = base_lsn_before + start1;
    lsn_t end_lsn   = base_lsn_after + end2;
    long  new_start = base + end2;
    {
        CRITICAL_SECTION(cs, _comp_lock);
        _flush_lsn = end_lsn;
    }

    w_assert1(end_lsn == first_lsn(start_lsn.hi()+1)
              || end_lsn.lo() - start_lsn.lo() == (end1-start1) + (end2-start2));
    _flush(start_lsn, start1, end1, start2, end2);
    _durable_lsn = end_lsn;
    _start = new_start;

    return end_lsn;
}



rc_t ringbuf_log::compensate(lsn_t orig_lsn, lsn_t undo_lsn) 
{
    // somewhere in the calling code, we didn't actually log anything.
    // so this would be a compensate to myself.  i.e. a no-op
    if(orig_lsn == undo_lsn)
        return RCOK;

    // FRJ: this assertion wasn't there originally, but I don't see
    // how the situation could possibly be correct
    w_assert1(orig_lsn <= _curr_lsn);
    
    // no need to grab a mutex if it's too late
    if(orig_lsn < _flush_lsn)
      return RC(eBADCOMPENSATION);
    
    CRITICAL_SECTION(cs, _comp_lock);
    // check again; did we just miss it?
    lsn_t flsn = _flush_lsn;
    if(orig_lsn < flsn)
      return RC(eBADCOMPENSATION);
    
    /* where does it live? the buffer is always aligned with a
       buffer-sized chunk of the partition, so all we need to do is
       take a modulus of the lsn to get its buffer position. It may be
       wrapped but we know it's valid because we're less than a buffer
       size from the current _flush_lsn -- nothing newer can be
       overwritten until we release the mutex.
     */
    long pos = orig_lsn.lo() % _segsize;
    if(pos >= _segsize - logrec_t::hdr_sz) 
        return RC(eBADCOMPENSATION); // split record. forget it.

    // aligned?
    w_assert1((pos & 0x7) == 0);
    
    // grab the record and make sure it's valid
    logrec_t* s = (logrec_t*) &_buf[pos];

    // valid length?
    w_assert1((s->length() & 0x7) == 0);
    
    // split after the log header? don't mess with it
    if(pos + long(s->length()) > _segsize)
        return RC(eBADCOMPENSATION);
    
    lsn_t lsn_ck = s->get_lsn_ck();
    if(lsn_ck != orig_lsn) {
        // this is a pretty rare occurrence, and I haven't been able
        // to figure out whether it's actually a bug
        fprintf(stderr, "\nlsn_ck = %d.%lld, orig_lsn = %d.%lld\n",
                lsn_ck.hi(), 
                (long long int)(lsn_ck.lo()), 
                orig_lsn.hi(), 
                (long long int)(orig_lsn.lo()));
        return RC(eBADCOMPENSATION);
    }
    w_assert1(s->prev() == lsn_t::null || s->prev() >= undo_lsn);

    // Not quite sure what this is, but apparently you can't compensate it...
    if(s->is_undoable_clr())
        return RC(eBADCOMPENSATION);

    // success!
    DBGTHRD(<<"COMPENSATING LOG RECORD " << undo_lsn << " : " << *s);
    s->set_clr(undo_lsn);
    return RCOK;
}

log_m::log_m()
    : _min_chkpt_rec_lsn(first_lsn(1)), _space_available(0),
      _space_rsvd_for_chkpt(0),
      _partition_size(0), _partition_data_size(0), _log_corruption(false),
      _waiting_for_space(false)
{
    pthread_mutex_init(&_space_lock, 0);
    pthread_cond_init(&_space_cond, 0);
}

/*********************************************************************
 * 
 *  srv_log::set_master(master_lsn, min_rec_lsn, min_xct_lsn)
 *
 *********************************************************************/
void log_m::set_master(lsn_t mlsn, lsn_t min_rec_lsn, lsn_t min_xct_lsn) 
{
    CRITICAL_SECTION(cs, _partition_lock);
    lsn_t min_lsn = std::min(min_rec_lsn, min_xct_lsn);
    _write_master(mlsn, min_lsn);
    _old_master_lsn = _master_lsn;
    _master_lsn = mlsn;
    _min_chkpt_rec_lsn = min_lsn;
}

lsn_t log_m::curr_lsn() const 
{
  // no lock needed -- atomic read of a monotonically increasing value
  return _curr_lsn;
}

lsn_t log_m::durable_lsn() const {
    ASSERT_FITS_IN_POINTER(lsn_t);
    // else need to join the insert queue

    return _durable_lsn;
}
lsn_t log_m::master_lsn() const {
    ASSERT_FITS_IN_POINTER(lsn_t);
    // else need to grab the partition mutex

    return _master_lsn;
}
lsn_t log_m::old_master_lsn() const {
    ASSERT_FITS_IN_POINTER(lsn_t);
    // else need to grab the partition mutex

    return _old_master_lsn;
}

lsn_t log_m::min_chkpt_rec_lsn() const {
    ASSERT_FITS_IN_POINTER(lsn_t);
    // else need to grab the partition mutex

    return _min_chkpt_rec_lsn;
}


/* Compute size of the biggest checkpoint we ever risk having to take...
 */
long log_m::max_chkpt_size() const {
    /* BUG: the number of transactions which might need to be
       checkpointed is potentially unbounded. However, it's rather
       unlikely we'll ever see more than 10k at any one time...
     */
    static long const GUESS_MAX_XCT_COUNT = 10000;
    static long const FUDGE = sizeof(logrec_t);
    long bf_tab_size = bf->npages()*sizeof(chkpt_bf_tab_t::brec_t);
    long xct_tab_size = GUESS_MAX_XCT_COUNT*sizeof(chkpt_xct_tab_t::xrec_t);
    return FUDGE + bf_tab_size + xct_tab_size;
}


void ringbuf_log::set_size(fileoff_t size) 
{
    /* The log consists of PARTITION_COUNT files, each with space for
       some integer number of log buffers plus one extra block for
       writing skip records.
     */
    fileoff_t usable_psize = size/PARTITION_COUNT - BLOCK_SIZE;
    // _partition_data_size = floor(usable_psize, (_segsize-BLOCK_SIZE));
    _partition_data_size = floor(usable_psize, (_segsize));
    if(_partition_data_size == 0) {
        fprintf(stderr, 
"log size is too small: size %ld usable_psize %ld, _segsize %ld, blocksize 0x%x\n",
                size, usable_psize, _segsize, BLOCK_SIZE);
        W_FATAL(eOUTOFLOGSPACE);
    }
    _partition_size = _partition_data_size + BLOCK_SIZE;
    /*
    fprintf(stderr, 
"size %ld usable_psize %ld _segsize %ld _part_data_size %ld _part_size %ld\n",
            size,
            usable_psize,
            _segsize,
            _partition_data_size,
            _partition_size
           );
   */
    release_space(PARTITION_COUNT*_partition_data_size);
    if(!verify_chkpt_reservation() || _space_rsvd_for_chkpt > _partition_data_size) {
	fprintf(stderr,
		"log partitions too small compared to buffer pool:\n"
		"	%lld bytes per partition available\n"
		"	%lld bytes needed for checkpointing dirty pages\n",
		(long long)_partition_data_size, (long long)_space_rsvd_for_chkpt);
	W_FATAL(eOUTOFLOGSPACE);
    }
}



rc_t        log_m::new_log_m(log_m        *&the_log,
                         const char *path,
                         fileoff_t _max_logsz,
                         int wrbufsize,
                         char  *shmbase,
                         bool  reformat)
{
    rc_t rc = srv_log::new_log_m(the_log, path, _max_logsz, wrbufsize, shmbase, reformat);
    if(!rc.is_error())
        the_log->start_flush_daemon();
    return rc.reset();
}

smlevel_0::fileoff_t log_m::space_left() const { return *&_space_available; }

typedef smlevel_0::fileoff_t fileoff_t;
static fileoff_t take_space(fileoff_t volatile* ptr, int amt) {
    fileoff_t ov = *ptr;
    while(1) {
	if(ov < amt)
	    return 0;
	fileoff_t nv = ov - amt;
	fileoff_t cv = atomic_cas_64((uint64_t*)ptr, ov, nv);
	if(ov == cv)
	    return amt;
	
	ov = cv;
    }
}

fileoff_t log_m::reserve_space(fileoff_t amt) {
    return (amt > 0)? take_space(&_space_available, amt) : 0;
}

fileoff_t log_m::consume_chkpt_reservation(fileoff_t amt) {
    return (amt > 0)? take_space(&_space_rsvd_for_chkpt, amt) : 0;
}

// make sure we have enough log reservation (conservative)
bool log_m::verify_chkpt_reservation() {
    fileoff_t space_needed = max_chkpt_size();
    while(*&_space_rsvd_for_chkpt < 2*space_needed) {
	if(reserve_space(space_needed)) {
	    // abuse take_space...
	    take_space(&_space_rsvd_for_chkpt, -space_needed);
	}
	else if(*&_space_rsvd_for_chkpt < space_needed) {
	    /* oops...

	       can't even guarantee the minimum of one checkpoint
	       needed to reclaim log space and solve the problem
	     */
	    W_FATAL(eOUTOFLOGSPACE);
	}
	else {
	    // must reclaim a log partition
	    return false;
	}
    }
    return true;
}

void log_m::release_space(fileoff_t amt) {
    w_assert0(amt >= 0);
    /* NOTE: The use of _waiting_for_space is purposefully racy
       because we don't want to pay the cost of a mutex for every
       space release (which should happen every transaction
       commit...). Instead waiters use a timeout in case they fall
       through the cracks.

       We broadcast whenever a "significant" amount of space seems to
       be free.
     */
    fileoff_t now_available = atomic_add_long_nv((uint64_t*) &_space_available, amt);
    w_assert0(now_available <= _partition_data_size*PARTITION_COUNT);
    //    fprintf(stderr, ">*<*><*>< Log space: %ld\n", now_available);
    if(_waiting_for_space && now_available > 16*sizeof(logrec_t)) {
	pthread_cond_broadcast(&_space_cond);
	_waiting_for_space = false;
    }
}

void log_m::wait_for_space() {
    // wait for a signal or 100ms, whichever is longer...
    pthread_mutex_lock(&_space_lock);
    struct timespec when;
    sthread_t::timeout_to_timespec(100, when);
    _waiting_for_space = true;
    pthread_cond_timedwait(&_space_cond, &_space_lock, &when);
    pthread_mutex_unlock(&_space_lock);
}
