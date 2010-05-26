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

 $Id: srv_log.cpp,v 1.57.2.15 2010/03/25 18:05:17 nhall Exp $

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

#define debug_log false

#define SM_SOURCE
#define LOG_C
#ifdef __GNUG__
#   pragma implementation
#endif

#include "sm_int_1.h"
#include "logtype_gen.h"
#include "srv_log.h"
#include "log_buf.h"
#include "unix_log.h"
#include "raw_log.h"

// chkpt.h needed to kick checkpoint thread
#include "chkpt.h"

#include <sstream>

// needed for skip_log
#include "logdef_gen.cpp"


char    srv_log::_logdir[max_devname];
bool    srv_log::_initialized = false;

/*********************************************************************
 *
 *  srv_log *srv_log::new_log_m(logdir,segid)
 *
 *  CONSTRUCTOR.  Returns one or the other log types.
 *
 *********************************************************************/

// BUG_MCS_LOG_1_FIX : was mcs_log::new_log_m 
// Fix is to remove mcs_log and make that new_log_m become the srv_log's method 
w_rc_t
srv_log::new_log_m(
    log_m        *&srv_log_p,
    const char* logdir,
    fileoff_t /*_max_logsz*/,
    int wrbufsize,
    char *shmbase,
    bool reformat
)
{
    bool        is_raw=false;
    rc_t        rc;

    /* 
     * see if this is a raw device or a Unix directory 
     * and then create the appropriate kind of log manager
     */

    rc = check_raw_device(logdir, is_raw);

    if(rc.is_error()) {
        // not mt-safe, but this is not going to happen in concurrency scenario
        smlevel_0::errlog->clog << error_prio  
                << "Error: cannot open the log file(s) " << logdir
                << ":" << endl << rc << flushl;
        return rc;
    }

    /* Force the log to be treated as raw. */
    char        *s = getenv("SM_LOG_RAW");
    if (s && atoi(s) > 0)
            is_raw = true;

    /* The log created here is deleted by the ss_m. */
    log_m *l = 0;
    if (!is_raw) {
        DBGTHRD(<<" log is unix file" );
        if (smlevel_0::max_logsz == 0)  {
            // not mt-safe, but this is not going to happen in 
            // concurrency scenario
            smlevel_0::errlog->clog << error_prio
                << "Error: log size must be non-zero for non-raw log devices"
                << flushl;
            /* XXX should genertae invalid log size of something instead? */
            return RC(eOUTOFLOGSPACE);
        }

        l = new unix_log(logdir, wrbufsize, shmbase, reformat);
    }
    else {
      rc = RC(fcNOTIMPLEMENTED);
    }
    if (rc.is_error())
        return rc;    

    srv_log_p = l;
    return RCOK;
}

NORET
srv_log::srv_log(
        int wrbufsize,
        char * /*shmbase*/
) : 
    ringbuf_log(wrbufsize),
    _curr_index(-1),
    _curr_num(1),
    _wrbufsize(wrbufsize),
    _readbuf(new char[BLOCK_SIZE*4]),
    _skip_log(new skip_log)
{
    FUNC(srv_log::srv_log);
    // now are attached to shm segment

    if (wrbufsize < 64 * 1024) {
        // not mt-safe, but this is not going to happen in concurrency scenario
        errlog->clog << error_prio 
        << "Log buf size (sm_logbufsize) too small: "
        << wrbufsize << ", require at least " << 64 * 1024 
        << endl; 
        errlog->clog << error_prio << endl;
        fprintf(stderr,
            "Log buf size (sm_logbufsize) too small: %d, need %d\n",
            wrbufsize, 64*1024);
        W_FATAL(OPT_BadValue);
    }

    w_assert1(is_aligned(_readbuf));
    DBGTHRD(<< "_readbuf is at " << W_ADDR(_readbuf));
    pthread_mutex_init(&_scavenge_lock, 0);
    pthread_cond_init(&_scavenge_cond, 0);
}

NORET
srv_log::~srv_log()
{
    delete [] _readbuf;
    delete _skip_log;
}

/**\var static __thread queue_based_block_lock_t::ext_qnode log_me_node;
 * \brief Queue node for holding partition lock.
 * \ingroup TLS
 */
__thread queue_based_block_lock_t::ext_qnode log_me_node;

void
srv_log::acquire() 
{
    _partition_lock.acquire(&log_me_node);
}
void
srv_log::release() 
{
    _partition_lock.release(log_me_node);
}


partition_index_t
srv_log::get_index(uint4_t n) const
{
    const partition_t        *p;
    for(int i=0; i<max_openlog; i++) {
        p = i_partition(i);
        if(p->num()==n) return i;
    }
    return -1;
}

partition_t *
srv_log::n_partition(partition_number_t n) const
{
    partition_index_t i = get_index(n);
    return (i<0)? (partition_t *)0 : i_partition(i);
}


partition_t *
srv_log::curr_partition() const
{
    w_assert3(partition_index() >= 0);
    return i_partition(partition_index());
}

/*********************************************************************
 *
 *  srv_log::scavenge(min_rec_lsn, min_xct_lsn)
 *
 *  Scavenge (free, reclaim) unused log files. All log files with index less
 *  than the minimum of min_rec_lsn, min_xct_lsn, _master_lsn, and 
 *  _min_chkpt_rec_lsn can be scavenged. 
 *
 *********************************************************************/
rc_t
srv_log::scavenge(lsn_t min_rec_lsn, lsn_t min_xct_lsn)
{
    FUNC(srv_log::scavenge);
    CRITICAL_SECTION(cs, _partition_lock);
    pthread_mutex_lock(&_scavenge_lock);

#if W_DEBUG_LEVEL > 2
    sanity_check();
#endif 
    partition_t        *p;

    lsn_t lsn = global_min_lsn(min_rec_lsn,min_xct_lsn);
    partition_number_t min_num;
    {
        /* 
         *  find min_num -- the lowest of all the partitions
         */
        min_num = partition_num();
        for (uint i = 0; i < smlevel_0::max_openlog; i++)  {
            p = i_partition(i);
            if( p->num() > 0 &&  p->num() < min_num )
                min_num = p->num();
        }
    }

    DBGTHRD( << "scavenge until lsn " << lsn << ", min_num is " 
         << min_num << endl );

    /*
     *  recycle all partitions  whose num is less than
     *  lsn.hi().
     */
    int count=0;
    for ( ; min_num < lsn.hi(); ++min_num)  {
        p = n_partition(min_num); 
        w_assert3(p);
        if (durable_lsn() < p->first_lsn() )  {
            W_FATAL(fcINTERNAL); // why would this ever happen?
            //            set_durable(first_lsn(p->num() + 1));
        }
        w_assert3(durable_lsn() >= p->first_lsn());
        DBGTHRD( << "scavenging log " << p->num() << endl );
        count++;
        p->close(true);
        p->destroy();
    }
    
    if(count > 0) {
	/* LOG_RESERVATIONS

	   reinstate the log space from the reclaimed partitions. We
	   can put back the entire partition size because every log
	   insert which finishes off a partition will consume whatever
	   unused space was left at the end.

	   Skim off the top of the released space whatever it takes to
	   top up the log checkpoint reservation.
	 */
	fileoff_t reclaimed = count*_partition_data_size;
	fileoff_t max_chkpt = max_chkpt_size();
	while(!verify_chkpt_reservation() && reclaimed > 0) {
	    long skimmed = std::min(max_chkpt, reclaimed);
	    atomic_add_long((uint64_t*)&_space_rsvd_for_chkpt, skimmed);
	    reclaimed -= skimmed;
	}
	release_space(reclaimed);
	pthread_cond_signal(&_scavenge_cond);
    }
    pthread_mutex_unlock(&_scavenge_lock);

    return RCOK;
}


/*********************************************************************
 *
 *  srv_log::flush(const lsn_t& lsn)
 *
 *  Flush all log records with lsn less than the parameter lsn
 *
 *********************************************************************/
void
srv_log::_flush(lsn_t lsn, long start1, long end1, long start2, long end2)
{

    // time to open a new partition? (used to be in srv_log::insert)
    partition_t* p = curr_partition();
    if(lsn.file() != p->num()) {
        partition_number_t n = p->num();
        w_assert3(lsn.file() == n+1);
        w_assert3(n != 0);

        {
	    /* FRJ: before starting into the CS below we have to be
	       sure an empty partition waits for us (otherwise we
	       deadlock because partition scavenging is protected by
	       the _partition_lock as well).
	     */
	    pthread_mutex_lock(&_scavenge_lock);
	retry:
	    bf->activate_background_flushing();
	    smlevel_1::chkpt->wakeup_and_take();
	    u_int oldest = log->global_min_lsn().hi();
	    if(oldest + PARTITION_COUNT == lsn.file()) {
		fprintf(stderr, "Can't open partition %d until partition %d is reclaimed\n",
			lsn.file(), oldest);
		pthread_cond_wait(&_scavenge_cond, &_scavenge_lock);
		goto retry;
	    }
	    pthread_mutex_unlock(&_scavenge_lock);
		

            // grab the lock -- we're about to mess with partitions
            CRITICAL_SECTION(cs, _partition_lock);
            p->close();  
            unset_current();
            DBG(<<" about to open " << n+1);
            //end_hint, existing,forappend,recovery
            p = open(n+1, lsn_t::null, false, true, false);
        }
        
        // it's a new partition -- size is now 0
        w_assert3(curr_partition()->size()== 0);
        w_assert3(partition_num() != 0);
    }

    /*
      stolen from partition_t::flush
    */
    p->flush(p->fhdl_app(), lsn, _buf, start1, end1, start2, end2);
    long size = (end2 - start2) + (end1 - start1);
    p->set_size(lsn.lo()+size);

#if W_DEBUG_LEVEL > 2
    sanity_check();
#endif 
}

void
srv_log::prime(int fd, fileoff_t start, lsn_t next) 
{
    w_assert1(_durable_lsn == _curr_lsn); // better be startup/recovery!
    long boffset = prime(_buf, fd, start, next);
    _durable_lsn = _flush_lsn = _curr_lsn = next;

    /* FRJ: the new code assumes that the buffer is always aligned
       with some buffer-sized multiple of the partition, so we need to
       return how far into the current segment we are.
     */
    long offset = next.lo() % _segsize;
    long base = next.lo() - offset;
    lsn_t start_lsn(next.hi(), base);
    _cur_epoch = epoch(start_lsn, base, offset, offset);
    _end = _start = next.lo();

    // move the primed data where it belongs (watch out, it might overlap)
    memmove(_buf+offset-boffset, _buf, boffset);
}

// prime buf with the partial block ending at 'next'; return the size
// of that partial block (possibly 0)
// based on log_buf::prime()
long                 
srv_log::prime(char* buf, int fd, fileoff_t start, lsn_t next)
{
    FUNC(log_buf::prime);
    
    // we are about to write a record for a certain lsn,
    // which maps to the given offset.
    //  But if this is start-up and we've initialized
    // with a partial partition, we have to prime the
    // buf with the last block in the partition

    fileoff_t b = floor(next.lo(), BLOCK_SIZE);

    //
    // this should happen only when we've just got
    // the size at start-up and it's not a full block
    // or
    // we've been scavenging and closing partitions
    // in the midst of normal operations
    // 
    lsn_t first = lsn_t(uint4_t(next.hi()), sm_diskaddr_t(b));
    if(first != next) {
        w_assert3(first.lo() < next.lo());
        fileoff_t offset = start + first.lo();

        DBG(<<" reading " << int(BLOCK_SIZE) << " on fd " << fd );
        int n = 0;
        w_rc_t e = me()->pread(fd, buf, BLOCK_SIZE, offset);
        if (e.is_error()) {
            // Not mt-safe, but it's a fatal error anyway
            W_FATAL_MSG(e.err_num(), 
                        << "cannot read log: lsn " << first 
                        << "pread(): " << e 
                        << "pread() returns " << n << endl);
        }
    }
    return next.lo() - first.lo();
}

void 
srv_log::sanity_check() const
{
    if(!_initialized) return;

#if W_DEBUG_LEVEL > 1
    partition_index_t   i;
    const partition_t*  p;
    bool                found_current=false;
    bool                found_min_lsn=false;

    // we should not be calling this when
    // we're in any intermediate state, i.e.,
    // while there's no current index
    
    if( _curr_index >= 0 ) {
        w_assert1(_curr_num > 0);
    } else {
        // initial state: _curr_num == 1
        w_assert1(_curr_num == 1);
    }
    w_assert1(durable_lsn() <= curr_lsn());
    w_assert1(durable_lsn() >= first_lsn(1));

    for(i=0; i<max_openlog; i++) {
        p = i_partition(i);
        p->sanity_check();

        w_assert1(i ==  p->index());

        // at most one open for append at any time
        if(p->num()>0) {
            w_assert1(p->exists());
            w_assert1(i ==  get_index(p->num()));
            w_assert1(p ==  n_partition(p->num()));

            if(p->is_current()) {
                w_assert1(!found_current);
                found_current = true;

                w_assert1(p ==  curr_partition());
                w_assert1(p->num() ==  partition_num());
                w_assert1(i ==  partition_index());

                w_assert1(p->is_open_for_append());
            } else if(p->is_open_for_append()) {
                // FRJ: not always true with concurrent inserts
                //w_assert1(p->flushed());
            }

            // look for global_min_lsn 
            if(global_min_lsn().hi() == p->num()) {
                //w_assert1(!found_min_lsn);
                // don't die in case global_min_lsn() is null lsn
                found_min_lsn = true;
            }
        } else {
            w_assert1(!p->is_current());
            w_assert1(!p->exists());
        }
    }
    w_assert1(found_min_lsn || (global_min_lsn()== lsn_t::null));
#endif 
}

/*********************************************************************
 *
 *  srv_log::fetch(lsn, rec, nxt)
 * 
 *  used in rollback and log_i
 *
 *  Fetch a record at lsn, and return it in rec. Optionally, return
 *  the lsn of the next record in nxt.  The ll parameter also returns
 *  the lsn of the log record actually fetched.  This is necessary
 *  since it is possible while scanning to specify an lsn
 *  that points to the end of a log file and therefore is actually
 *  the first log record in the next file.
 *
 *********************************************************************/
rc_t
srv_log::fetch(lsn_t& ll, logrec_t*& rp, lsn_t* nxt)
{
    FUNC(srv_log::fetch);

    DBGTHRD(<<"fetching lsn " << ll 
        << " , _curr_lsn = " << curr_lsn()
        << " , _durable_lsn = " << durable_lsn());

    if( ll.hi() >= 2147483647) { // TODO NANCY do something cleaner with this 
        DBGTHRD(<<" ERROR  -- ???" );
    }

#if W_DEBUG_LEVEL > 2
    sanity_check();
#endif 

    // it's not sufficient to flush to ll, since
    // ll is at the *beginning* of what we want
    // to read...
    W_DO(flush(ll+sizeof(logrec_t)));

    // protect against double-acquire
    acquire(); // caller must release the _partition_lock mutex

    /*
     *  Find and open the partition
     */

    partition_t        *p = 0;
    uint4_t        last_hi=0;
    while (!p) {
        if(last_hi == ll.hi()) {
            // can happen on the 2nd or subsequent round
            // but not first
            DBGTHRD(<<"no such partition " << ll  );
            return RC(eEOF);
        }
        if (ll >= curr_lsn())  {
            /*
             *  This would constitute a
             *  read beyond the end of the log
             */
            DBGTHRD(<<"fetch at lsn " << ll  << " returns eof -- _curr_lsn=" 
                    << curr_lsn());
            return RC(eEOF);
        }
        last_hi = ll.hi();

        /*
        //     open(partition,end_hint, existing,forappend,recovery
        */
        DBG(<<" about to open " << ll.hi());
        if ((p = open(ll.hi(), lsn_t::null, true, false, false))) {

            // opened one... is it the right one?
            DBGTHRD(<<"opened... p->size()=" << p->size());

            if ( ll.lo() >= p->size() ||
                (p->size() == partition_t::nosize && ll.lo() >= limit()))  {
                DBGTHRD(<<"seeking to " << ll.lo() << ";  beyond p->size() ... OR ...");
                DBGTHRD(<<"limit()=" << limit() << " & p->size()==" 
                        << int(partition_t::nosize));

                ll = first_lsn(ll.hi() + 1);
                DBGTHRD(<<"getting next partition: " << ll);
                p = 0; continue;
            }
        }
    }

    W_COERCE(p->read(rp, ll));
    {
        logrec_t        &r = *rp;

        if (r.type() == logrec_t::t_skip && r.get_lsn_ck() == ll) {

            DBGTHRD(<<"seeked to skip" << ll );
            DBGTHRD(<<"getting next partition.");
            ll = first_lsn(ll.hi() + 1);
            // FRJ: BUG? Why are we so certain this partition is even
            // open, let alone open for read?
            p = n_partition(ll.hi());
            if(!p)
                p = open(ll.hi(), lsn_t::null, false, false, false);

            // re-read
            
            W_COERCE(p->read(rp, ll));
        } 
    }
    logrec_t        &r = *rp;

    if (r.lsn_ck().hi() != ll.hi()) {
        W_FATAL_MSG(fcINTERNAL,
            << "Fatal error: log record " << ll 
            << " is corrupt in lsn_ck().hi() " 
            << r.get_lsn_ck()
            << endl);
    } else if (r.lsn_ck().lo() != ll.lo()) {
        W_FATAL_MSG(fcINTERNAL,
            << "Fatal error: log record " << ll 
            << "is corrupt in lsn_ck().lo()" 
            << r.get_lsn_ck()
            << endl);
    }

    if (nxt) {
        lsn_t tmp = ll;
        *nxt = tmp.advance(r.length());
    }

#ifdef UNDEF
    int saved = r._checksum;
    r._checksum = 0;
    for (int c = 0, i = 0; i < r.length(); c += ((char*)&r)[i++]);
    w_assert1(c == saved);
#endif

    DBGTHRD(<<"fetch at lsn " << ll  << " returns " << r);
#if W_DEBUG_LEVEL > 2
    sanity_check();
#endif 

    // caller must release the _partition_lock mutex
    return RCOK;
}


/*
 * open_for_append(num, end_hint)
 * "open" a file  for the given num for append, and
 * make it the current file.
 */
// MUTEX: flush, insert, partition
void
partition_t::open_for_append(partition_number_t __num, 
        const lsn_t& end_hint) 
{
    FUNC(partition::open_for_append);

    // shouldn't be calling this if we're already open
    w_assert3(!is_open_for_append());
    // We'd like to use this assertion, but in the
    // raw case, it's wrong: fhdl_app() is NOT synonymous
    // with is_open_for_append() and the same goes for ...rd()
    // w_assert3(fhdl_app() == 0);

    int         fd;

    DBG(<<"open_for_append num()=" << num()
            << "__num=" << __num
            << "_num=" << _num
            << " about to peek");

    /*
    if(num() == __num) {
        close_for_read();
        close_for_append();
        _num = 0; // so the peeks below
        // will work -- it'll get reset
        // again anyway.
   }
   */
    /* might not yet know its size - discover it now  */
    peek(__num, end_hint, true, &fd); // have to know its size
    w_assert3(fd);
    if(size() == nosize) {
        // we're opening a new partition
        set_size(0);
    }
        
    _num = __num;
    // size() was set in peek()
    w_assert1(size() != partition_t::nosize);
    //    _owner->set_durable( lsn_t(num(), sm_diskaddr_t(size())) );

    set_fhdl_app(fd);
    set_state(m_flushed);
    set_state(m_exists);
    set_state(m_open_for_append);

    _owner->set_current( index(), num() );
    return ;
}

/*********************************************************************
 * 
 *  srv_log::close_min(n)
 *
 *  Close the partition with the smallest index(num) or an unused
 *  partition, and 
 *  return a ptr to the partition
 *
 *  The argument n is the partition number for which we are going
 *  to use the free partition.
 *
 *********************************************************************/
// MUTEX: partition
partition_t        *
srv_log::close_min(partition_number_t n)
{
    // kick the cleaner thread(s)
    bf->activate_background_flushing();
    
    FUNC(srv_log::close_min);
    
    /*
     *  If a free partition exists, return it.
     */

    /*
     * first try the slot that is n % max_openlog
     * That one should be free.
     */
    int tries=0;
 again:
    partition_index_t    i =  (int)((n-1) % max_openlog);
    partition_number_t   min = min_chkpt_rec_lsn().hi();
    partition_t         *victim;

    victim = i_partition(i);
    if((victim->num() == 0)  ||
        (victim->num() < min)) {
        // found one -- doesn't matter if it's the "lowest"
        // but it should be
    } else {
        victim = 0;
    }

    if (victim)  {
        w_assert3( victim->index() == (partition_index_t)((n-1) % max_openlog));
    }
    /*
     *  victim is the chosen victim partition.
     */
    if(!victim) {
        /*
         * uh-oh, no space left. Kick the page cleaners, wait a bit, and 
         * try again. Do this no more than 8 times.
         *
         */
        {
            w_ostrstream msg;
            msg << error_prio 
            << "Thread " << me()->id << " "
            << "Out of log space  (" 
            << space_left()
            << "); No empty partitions."
            << endl;
            fprintf(stderr, "%s\n", msg.c_str());
        }
        
        if(tries++ > 8) W_FATAL(smlevel_0::eOUTOFLOGSPACE);
        bf->activate_background_flushing();
        me()->sleep(1000);
        goto again;
    }
    w_assert1(victim);
    // num could be 0

    /*
     *  Close it.
     */
    if(victim->exists()) {
        /*
         * Cannot close it if we need it for recovery.
         */
        if(victim->num() >= min_chkpt_rec_lsn().hi()) {
            w_ostrstream msg;
            msg << " Cannot close min partition -- still in use!" << endl;
            // not mt-safe
            smlevel_0::errlog->clog << error_prio  << msg.c_str() << flushl;
        }
        w_assert1(victim->num() < min_chkpt_rec_lsn().hi());

        victim->close(true);
        victim->destroy();

    } else {
        w_assert3(! victim->is_open_for_append());
        w_assert3(! victim->is_open_for_read());
    }
    w_assert1(! victim->is_current() );
    
    victim->clear();

    return victim;
}

bool                        
partition_t::exists() const
{
   bool res = (_mask & m_exists) != 0;
#if W_DEBUG_LEVEL > 2
   if(res) {
       w_assert3(num() != 0);
    }
#endif 
   return res;
}

bool                        
partition_t::is_open_for_read() const
{
   bool res = (_mask & m_open_for_read) != 0;
   return res;
}

bool                        
partition_t::is_open_for_append() const
{
   bool res = (_mask & m_open_for_append) != 0;
   return res;
}

bool
partition_t::is_current()  const
{
    //  rd could be open
    if(index() == _owner->partition_index()) {
        w_assert3(num()>0);
        w_assert3(_owner->partition_num() == num());
        w_assert3(exists());
        w_assert3(_owner->curr_partition() == this);
        w_assert3(_owner->partition_index() == index());
        w_assert3(this->is_open_for_append());

        return true;
    }
#if W_DEBUG_LEVEL > 2
    if(num() == 0) {
        w_assert3(!this->exists());
    }
#endif 
    return false;
}

// Initialize on first access:
// block to be cleared upon first use.
class block_of_zeroes {
private:
    char _block[srv_log::BLOCK_SIZE];
public:
    NORET block_of_zeroes() {
        memset(&_block[0], 0, srv_log::BLOCK_SIZE);
    }
    char *block() { return _block; }
};

char *block_of_zeros() {

    static block_of_zeroes z;
    return z.block();
}

/*
 * partition::flush(int fd, bool force)
 * flush to disk whatever's been
 * buffered, 
 * and retain the minimum necessary
 * portion of the last block (DEVSZ) bytes)
 */
void 
partition_t::flush(int fd, lsn_t lsn, 
        char* buf, 
        long start1, 
        long end1, 
        long start2, 
        long end2)
{
    long size = (end2 - start2) + (end1 - start1);
    long write_size = size;

    { // sync log
    DBGTHRD( << "Sync-ing log" );

    // This change per e-mail from Ippokratis, 16 Jun 09:
    // long file_offset = _owner->floor(lsn.lo(), srv_log::BLOCK_SIZE);
    // works because BLOCK_SIZE is always a power of 2
    long file_offset = ringbuf_log::floor2(lsn.lo(), srv_log::BLOCK_SIZE);

    long delta = lsn.lo() - file_offset;
  
    // adjust down to the nearest full block
    w_assert1(start1 >= delta); // really offset - delta >= 0, but works for unsigned...
    write_size += delta; // account for the extra (clean) bytes
    start1 -= delta;

    /* FRJ: This seek is safe (in theory) because only one thread
       can flush at a time and all other accesses to the file use
       pread/pwrite (which doesn't change the file pointer).
     */
    fileoff_t where = start() + file_offset;
    w_rc_t e = me()->lseek(fd, where, sthread_t::SEEK_AT_SET);
    if (e.is_error()) {
        W_FATAL_MSG(e.err_num(), << "ERROR: could not seek to "
                                << file_offset
                                << " + " << start()
                                << " to write log record"
                                << endl);
    }
    } // end sync log
    
    /*
       stolen from log_buf::write_to
    */
    { // Copy a skip record to the end of the buffer.
        skip_log* s = _owner->_skip_log;
        s->set_lsn_ck(lsn+size);

        // Hopefully the OS is smart enough to coalesce the writes
        // before sending them to disk. If not, and it's a problem
        // (e.g. for direct I/O), the alternative is to assemble the last
        // block by copying data out of the buffer so we can append the
        // skiplog without messing up concurrent inserts. However, that
        // could mean copying up to BLOCK_SIZE bytes.
        long total = write_size + s->length();

        // This change per e-mail from Ippokratis, 16 Jun 09:
        // long grand_total = _owner->ceil(total, srv_log::BLOCK_SIZE);
        // works because BLOCK_SIZE is always a power of 2
        long grand_total = ringbuf_log::ceil2(total, srv_log::BLOCK_SIZE);

        typedef sdisk_base_t::iovec_t iovec_t;

        iovec_t iov[] = {
            iovec_t(buf+start1,                end1-start1),
            iovec_t(buf+start2,                end2-start2), 
            iovec_t(s,                        s->length()),
            iovec_t(block_of_zeros(),         grand_total-total), 
        };
    
        w_rc_t e = me()->writev(fd, iov, sizeof(iov)/sizeof(iovec_t));
        if (e.is_error()) {
            smlevel_0::errlog->clog << error_prio 
                                    << "ERROR: could not flush log buf:"
                                    << " fd=" << fd
                                << " xfersize=" 
                                << srv_log::BLOCK_SIZE
                                << " vec parts: " 
                                << " " << iov[0].iov_len
                                << " " << iov[1].iov_len
                                << " " << iov[2].iov_len
                                << " " << iov[3].iov_len
                                    << ":" << endl
                                    << e
                                    << flushl;
            cerr 
                                    << "ERROR: could not flush log buf:"
                                    << " fd=" << fd
                                    << " xfersize=" << srv_log::BLOCK_SIZE
                                << srv_log::BLOCK_SIZE
                                << " vec parts: " 
                                << " " << iov[0].iov_len
                                << " " << iov[1].iov_len
                                << " " << iov[2].iov_len
                                << " " << iov[3].iov_len
                                    << ":" << endl
                                    << e
                                    << flushl;
            W_COERCE(e);
        }
    } // end copy skip record

    this->_flush(fd);
}

/*
 *  partition_t::_peek(num, peek_loc, whole_size,
        recovery, fd) -- used by both -- contains
 *   the guts
 *
 *  Peek at a partition num() -- see what num it represents and
 *  if it's got anything other than a skip record in it.
 *
 *  If recovery==true,
 *  determine its size, if it already exists (has something
 *  other than a skip record in it). In this case its num
 *  had better match num().
 *
 *  If it's just a skip record, consider it not to exist, and
 *  set _num to 0, leave it "closed"
 *
 *********************************************************************/
void
partition_t::_peek(
    partition_number_t num_wanted,
    fileoff_t        peek_loc,
    fileoff_t        whole_size,
    bool recovery,
    int fd
)
{
    FUNC(partition_t::_peek);
    w_assert3(num() == 0 || num() == num_wanted);
    clear();

    clr_state(m_exists);
    clr_state(m_flushed);
    clr_state(m_open_for_read);

    w_assert3(fd);

    logrec_t        *l = 0;

    // seek to start of partition or to the location given
    // in peek_loc -- that's a location we suspect might
    // be the end of-the-log skip record.
    //
    // the lsn passed to read(rec,lsn) is not
    // inspected for its hi() value
    //
    bool  peeked_high = false;
    if(    (peek_loc != partition_t::nosize)
        && (peek_loc <= this->_eop) 
        && (peek_loc < whole_size) ) {
        peeked_high = true;
    } else {
        peek_loc = 0;
        peeked_high = false;
    }
again:
    lsn_t pos = lsn_t(uint4_t(num()), sm_diskaddr_t(peek_loc));

    lsn_t lsn_ck = pos ;
    w_rc_t rc;

    while(pos.lo() < this->_eop) {
        DBGTHRD("pos.lo() = " << pos.lo()
                << " and eop=" << this->_eop);
        if(recovery) {
            // increase the starting point as much as possible.
            // to decrease the time for recovery
            if(pos.hi() == _owner->master_lsn().hi() &&
               pos.lo() < _owner->master_lsn().lo())  {
                  if(!debug_log) {
                      pos = _owner->master_lsn();
                  }
            }
        }
        DBGTHRD( <<"reading pos=" << pos <<" eop=" << this->_eop);

        rc = read(l, pos, fd);
        DBGTHRD(<<"POS " << pos << ": tx." << *l);

        if(rc.err_num() == smlevel_0::eEOF) {
            // eof or record -- wipe it out
            DBGTHRD(<<"EOF--Skipping!");
            skip(pos, fd);
            break;
        }
                
        DBGTHRD(<<"peek index " << _index 
            << " l->length " << l->length() 
            << " l->type " << int(l->type()));

        w_assert1(l->length() >= logrec_t::hdr_sz);
        {
            // check lsn
            lsn_ck = l->get_lsn_ck();
            int err = 0;

            DBGTHRD( <<"lsnck=" << lsn_ck << " pos=" << pos
                <<" l.length=" << l->length() );


            if( ( l->length() < logrec_t::hdr_sz )
                ||
                ( l->length() > sizeof(logrec_t) )
                ||
                ( lsn_ck.lo() !=  pos.lo() )
                ||
                (num_wanted  && (lsn_ck.hi() != num_wanted) )
                ) {
                err++;
            }

            if( num_wanted  && (lsn_ck.hi() != num_wanted) ) {
                // Wrong partition - break out/return
                DBGTHRD(<<"NOSTASH because num_wanted="
                        << num_wanted
                        << " lsn_ck="
                        << lsn_ck
                    );
                return;
            }

            DBGTHRD( <<"type()=" << int(l->type())
                << " index()=" << this->index() 
                << " lsn_ck=" << lsn_ck
                << " err=" << err );

            /*
            // if it's a skip record, and it's the first record
            // in the partition, its lsn might be null.
            //
            // A skip record that's NOT the first in the partiton
            // will have a correct lsn.
            */

            if( l->type() == logrec_t::t_skip   && 
                pos == first_lsn()) {
                // it's a skip record and it's the first rec in partition
                if( lsn_ck != lsn_t::null )  {
                    DBGTHRD( <<" first rec is skip and has lsn " << lsn_ck );
                    err = 1; 
                }
            } else {
                // ! skip record or ! first in the partition
                if ( (lsn_ck.hi()-1) % max_open_log != (uint4_t)this->index()) {
                    DBGTHRD( <<"unexpected end of log");
                    err = 2;
                }
            }
            if(err > 0) {
                // bogus log record, 
                // consider end of log to be previous record

                if(err > 1) {
                    smlevel_0::errlog->clog << error_prio <<
                    "Found unexpected end of log --"
                    << " probably due to a previous crash." 
                    << flushl;
                }

                if(peeked_high) {
                    // set pos to 0 and start this loop all over
                    DBGTHRD( <<"Peek high failed at loc " << pos);
                    peek_loc = 0;
                    peeked_high = false;
                    goto again;
                }

                /*
                // Incomplete record -- wipe it out
                */
#if W_DEBUG_LEVEL > 2
                if(pos.hi() != 0) {
                   w_assert3(pos.hi() == num_wanted);
                }
#endif 

                // assign to lsn_ck so that the when
                // we drop out the loop, below, pos is set
                // correctly.
                lsn_ck = lsn_t(num_wanted, pos.lo());
                skip(lsn_ck, fd);
                break;
            }
        }
        // DBGTHRD(<<" changing pos from " << pos << " to " << lsn_ck );
        pos = lsn_ck;

        DBGTHRD(<< " recovery=" << recovery
            << " master=" << _owner->master_lsn()
        );
        if( l->type() == logrec_t::t_skip 
            || !recovery) {
            /*
             * IF 
             *  we hit a skip record 
             * or 
             *  if we're not in recovery (i.e.,
             *  we aren't trying to find the last skip log record
             *  or check each record's legitimacy)
             * THEN 
             *  we've seen enough
             */
            DBGTHRD(<<" BREAK EARLY ");
            break;
        }
        pos.advance(l->length());
    }

    // pos == 0 if the first record
    // was a skip or if we don't care about the recovery checks.

    DBGTHRD(<<"pos= " << pos << "l->type()=" << int(l->type()));

#if W_DEBUG_LEVEL > 2
    if(pos.lo() > first_lsn().lo()) {
        w_assert3(l!=0);
    }
#endif 

    if( pos.lo() > first_lsn().lo() || l->type() != logrec_t::t_skip ) {
        // we care and the first record was not a skip record
        _num = pos.hi();

        // let the size *not* reflect the skip record
        // and let us *not* set it to 0 (had we not read
        // past the first record, which is the case when
        // we're peeking at a partition that's earlier than
        // that containing the master checkpoint
        // 
        if(pos.lo()> first_lsn().lo()) set_size(pos.lo());

        // OR first rec was a skip so we know
        // size already
        // Still have to figure out if file exists

        set_state(m_exists);

        DBGTHRD(<<"STASHED num()=" << num()
                << " size()=" << size()
            );
    } else { 
        w_assert3(num() == 0);
        w_assert3(size() == nosize || size() == 0);
        // size can be 0 if the partition is exactly
        // a skip record
        DBGTHRD(<<"SIZE NOT STASHED ");
    }
}


// MUTEX: partition
void
partition_t::skip(const lsn_t &ll, int fd)
{
    FUNC(partition_t::skip);

    // Current partition should flush(), not skip()
    w_assert1(_num == 0 || _num != _owner->partition_num());
    
    DBGTHRD(<<"skip at " << ll);

    char* _skipbuf = new char[srv_log::BLOCK_SIZE*2];
    // FRJ: We always need to prime() partition ops (peek, open, etc)
    // always use a different buffer than log inserts.
    long offset = _owner->prime(_skipbuf, fd, start(), ll);
    
    // Make sure that flush writes a skip record
    this->flush(fd, ll, _skipbuf, offset, offset, offset, offset);
    delete [] _skipbuf;
    DBGTHRD(<<"wrote and flushed skip record at " << ll);

    set_last_skip_lsn(ll);
}

/*
 * partition_t::read(logrec_t *&rp, lsn_t &ll, int fd)
 * 
 * expect ll to be correct for this partition.
 * if we're reading this for the first time,
 * for the sake of peek(), we expect ll to be
 * lsn_t(0,0), since we have no idea what
 * its lsn is supposed to be, but in fact, we're
 * trying to find that out.
 *
 * If a non-zero fd is given, the read is to be done
 * on that fd. Otherwise it is assumed that the
 * read will be done on the fhdl_rd().
 */
// MUTEX: partition
w_rc_t
partition_t::read(logrec_t *&rp, lsn_t &ll, int fd)
{
    FUNC(partition::read);

    INC_TSTAT(log_fetches);

    if(fd == invalid_fhdl) fd = fhdl_rd();

#if W_DEBUG_LEVEL > 2
    w_assert3(fd);
    if(exists()) {
        if(fd) w_assert3(is_open_for_read());
        w_assert3(num() == ll.hi());
    }
#endif 

    fileoff_t pos = ll.lo();
    fileoff_t lower = pos / XFERSIZE;

    lower *= XFERSIZE;
    fileoff_t off = pos - lower;

    DBGTHRD(<<"seek to lsn " << ll
        << " index=" << _index << " fd=" << fd
        << " pos=" << pos
        << " lower=" << lower  << " + " << start()
        << " fd=" << fd
    );

    /* 
     * read & inspect header size and see
     * and see if there's more to read
     */
    int b = 0;
    fileoff_t leftover = logrec_t::hdr_sz;
    bool first_time = true;

    rp = (logrec_t *)(readbuf() + off);

    DBGTHRD(<< "off= " << ((int)off)
        << "readbuf()@ " << W_ADDR(readbuf())
        << " rp@ " << W_ADDR(rp)
    );

    while (leftover > 0) {

        DBGTHRD(<<"leftover=" << int(leftover) << " b=" << b);

        w_rc_t e = me()->pread(fd, (void *)(readbuf() + b), XFERSIZE, start() + lower + b);
        DBGTHRD(<<"after me()->read() size= " << int(XFERSIZE));


        if (e.is_error()) {
                /* accept the short I/O error for now */
                smlevel_0::errlog->clog << error_prio 
                        << "read(" << int(XFERSIZE) << ")" << flushl;
                W_COERCE(e);
        }
        b += XFERSIZE;
        w_assert3(b <= srv_log::READ_BUF_SIZE);

        // 
        // This could be written more simply from
        // a logical standpoint, but using this
        // first_time makes it a wee bit more readable
        //
        if (first_time) {
            if( rp->length() > sizeof(logrec_t) || 
            rp->length() < logrec_t::hdr_sz ) {
                w_assert1(ll.hi() == 0); // in peek()
                return RC(smlevel_0::eEOF);
            }
            first_time = false;
            leftover = rp->length() - (b - off);
            DBGTHRD(<<" leftover now=" << leftover);
        } else {
            leftover -= XFERSIZE;
            w_assert3(leftover == (int)rp->length() - (b - off));
            DBGTHRD(<<" leftover now=" << leftover);
        }
    }
    DBGTHRD( << "readbuf()@ " << W_ADDR(readbuf())
        << " first 4 chars are: "
        << (int)(*((char *)readbuf()))
        << (int)(*((char *)readbuf()+1))
        << (int)(*((char *)readbuf()+2))
        << (int)(*((char *)readbuf()+3))
    );
    return RCOK;
}

/*********************************************************************
 * 
 *  srv_log::open(num, end_hint, existing, forappend, during_recovery)
 *
 *  This partition structure is free and usable.
 *  Open it as partition num. 
 *
 *  if existing==true, the partition "num" had better already exist,
 *  else it had better not already exist.
 * 
 *  if forappend==true, making this the new current partition.
 *    and open it for appending was well as for reading
 *
 *   if during_recovery==true, make sure the entire partition is 
 *   checked and its size is recorded accurately.
 *   end_hint is used iff during_recovery is true.
 *
 *********************************************************************/

// MUTEX: partition
partition_t        *
srv_log::open(partition_number_t  __num, 
        const lsn_t&  end_hint,
        bool existing, 
        bool forappend, 
        bool during_recovery
)
{
    w_assert3(__num > 0);

#if W_DEBUG_LEVEL > 2
    // sanity checks for arguments:
    {
        // bool case1 = (existing  && forappend && during_recovery);
        bool case2 = (existing  && forappend && !during_recovery);
        // bool case3 = (existing  && !forappend && during_recovery);
        // bool case4 = (existing  && !forappend && !during_recovery);
        // bool case5 = (!existing  && forappend && during_recovery);
        // bool case6 = (!existing  && forappend && !during_recovery);
        bool case7 = (!existing  && !forappend && during_recovery);
        bool case8 = (!existing  && !forappend && !during_recovery);

        w_assert3( ! case2);
        w_assert3( ! case7);
        w_assert3( ! case8);
    }

#endif 

    // see if one's already opened with the given __num
    partition_t *p = n_partition(__num);

#if W_DEBUG_LEVEL > 2
    if(forappend) {
        w_assert3(partition_index() == -1);
        // there should now be *no open partition*
        partition_t *c;
        int i;
        for (i = 0; i < max_openlog; i++)  {
            c = i_partition(i);
            w_assert3(! c->is_current());
        }
    }
#endif 

    if(!p) {
        /*
         * find an empty partition to use
         */
        DBG(<<"find a new partition structure  to use " );
        p = close_min(__num);
        w_assert1(p);
        p->peek(__num, end_hint, during_recovery);
    }



    if(existing && !forappend) {
        DBG(<<"about to open for read");
        p->open_for_read(__num);
        w_assert3(p->is_open_for_read());
        w_assert3(p->num() == __num);
        w_assert3(p->exists());
    }


    if(forappend) {
        /*
         *  This becomes the current partition.
         */
        p->open_for_append(__num, end_hint);
        if(during_recovery) {
          // FRJ: moved this code out of open_for_append() so we can
          // prime() a brand new log properly during startup

          // We will eventually want to write a record with the durable
          // lsn.  But if this is start-up and we've initialized
          // with a partial partition, we have to prime the
          // buf with the last block in the partition.
          w_assert1(durable_lsn() == curr_lsn());
          prime(p->fhdl_app(), p->start(), durable_lsn());
        }
        w_assert3(p->exists());
        w_assert3(p->is_open_for_append());
    }
    return p;
}

void
srv_log::unset_current()
{
    _curr_index = -1;
    _curr_num = 0;
}

void
srv_log::set_current(
        partition_index_t i, 
        partition_number_t num
)
{
    w_assert3(_curr_index == -1);
    w_assert3(_curr_num  == 0 || _curr_num == 1);
    _curr_index = i;
    _curr_num = num;
}

