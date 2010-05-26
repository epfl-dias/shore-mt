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

/*<std-header orig-src='shore' incl-file-exclusion='LOG_H'>

 $Id: log.h,v 1.79.2.13 2010/03/25 18:05:14 nhall Exp $

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

#ifndef LOG_H
#define LOG_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#undef ACQUIRE

#ifdef __GNUG__
#pragma interface
#endif


class logrec_t;
class log_buf;

/* Log manager base class.
   Handles simple stuff (set_master, etc) and leaves the
   rest to a child class
 */
class log_m : public smlevel_0 
{
public:
    enum { PARTITION_COUNT=8 };


    virtual void shutdown()=0; // do whatever needs to be done before
                               // destructor is callable
    virtual ~log_m() { }
    
    /*
      Log interface we need to implement
      - insert                         (almost done -- not log_rec aware yet)
      - compensate                (virtually done)
      - fetch                        
      - flush                         (done)
      - scavenge
      - set_master                 (done)
      - master_lsn                 (done)
      - old_master_lsn                 (done)
      - min_chkpt_rec_lsn        (done)
      - curr_lsn                 (done)
      - durable_lsn                 (done)
      - compute_space
      - flush_all                (done)
      - wait
     */
    virtual rc_t insert(logrec_t &r, lsn_t* ret)=0; // returns the lsn the data was written to
    virtual rc_t flush(lsn_t lsn)=0;
    virtual void start_flush_daemon()=0;
    virtual rc_t compensate(lsn_t orig_lsn, lsn_t undo_lsn)=0;
    virtual lsn_t curr_lsn() const;
    virtual rc_t scavenge(lsn_t min_rec_lsn, lsn_t min_xct_lsn)=0;
    virtual rc_t fetch(lsn_t &lsn, logrec_t* &rec, lsn_t* nxt=NULL)=0;
    virtual void release()=0; 
                     
    fileoff_t    reserve_space(fileoff_t howmuch);
    void   release_space(fileoff_t howmuch);
    rc_t    wait_for_space(fileoff_t &amt, timeout_in_ms timeout);
    virtual void	request_checkpoint()=0; // since log.cpp doesn't know about chkpt_m...

    void set_master(lsn_t master_lsn, lsn_t min_lsn, lsn_t min_xct_lsn);
    long space_left() const;
    lsn_t durable_lsn() const;
    lsn_t master_lsn() const;
    lsn_t old_master_lsn() const;
    lsn_t min_chkpt_rec_lsn() const;
    fileoff_t limit() const { return _partition_size; }
    fileoff_t logDataLimit() const { return _partition_data_size; }
    void start_log_corruption() { _log_corruption = true; }

    // convenience functions (composed out of other functions)
    lsn_t global_min_lsn() const { return std::min(master_lsn(), min_chkpt_rec_lsn()); }
    lsn_t global_min_lsn(lsn_t const &a) const { return std::min(global_min_lsn(), a); }
    lsn_t global_min_lsn(lsn_t const &a, lsn_t const &b) const { return std::min(global_min_lsn(a), b); }
    
    // flush won't return until target lsn before durable_lsn(), so
    // back off by one byte so we don't depend on other inserts to
    // arrive after us
    rc_t flush_all() { return flush(curr_lsn().advance(-1)); }


    // statics
    static uint4_t const         version_major;
    static uint4_t const         version_minor;

    static rc_t                 new_log_m(
                                    log_m        *&the_log,
                                    const char        *path,
                                    fileoff_t        _max_logsz,
                                    int                wrlogbufsize,
                                    char        *shmbase,
                                    bool        reformat);
    static lsn_t                first_lsn(uint4_t pnum) { return lsn_t(pnum, 0); }
    static rc_t                        check_raw_device(
                                    const char* devname,
                                    bool&        raw
                                    );
    static rc_t                 check_version(
                                    uint4_t        major,
                                    uint4_t        minor
                                    );
    static rc_t                 parse_master_chkpt_string(
                                    istream&         s,
                                    lsn_t&        master_lsn,
                                    lsn_t&        min_chkpt_rec_lsn,
                                    int&        number_of_others,
                                    lsn_t*        others,
                                    bool&        old_style
                                    );
    static rc_t                 parse_master_chkpt_contents(
                                    istream&        s,
                                    int&        listlength,
                                    lsn_t*        lsnlist
                                    );
    static void                 create_master_chkpt_string(
                                    ostream&        o,
                                    int                arraysize,
                                    const lsn_t* array,
                                    bool        old_style = false
                                    );
    static void                 create_master_chkpt_contents(
                                    ostream&        s,
                                    int                arraysize,
                                    const lsn_t*        array
                                    );

    long			max_chkpt_size() const;
    bool			verify_chkpt_reservation();
    fileoff_t			consume_chkpt_reservation(fileoff_t howmuch);
protected:
    log_m();
    

    // Writes out a master lsn record
    virtual void _write_master(lsn_t master_lsn, lsn_t min_lsn)=0;

    // partition and checkpoint management
    mutable queue_based_block_lock_t _partition_lock;
    lsn_t                   _curr_lsn;
    lsn_t                   _durable_lsn;
    lsn_t                   _master_lsn;
    lsn_t                   _old_master_lsn;
    lsn_t                   _min_chkpt_rec_lsn;
    fileoff_t volatile      _space_available; // how many unreserved bytes in the log?
    fileoff_t volatile      _space_rsvd_for_chkpt; // can we run a checkpoint right now?
    fileoff_t               _partition_size;
    fileoff_t               _partition_data_size;
    bool                    _log_corruption;
    pthread_mutex_t	    _space_lock;
    pthread_cond_t	    _space_cond;
    bool		    _waiting_for_space;
    
private:
    // no copying allowed
    log_m &operator=(log_m const &);
    log_m(log_m const &);
};



struct ringbuf_log : public log_m 
{

    struct epoch {
        lsn_t base_lsn; // lsn of _buf[0] for this epoch
        long base; // absolute position of _buf[0]
        long start; // should copy _buf[start..end]
        long end;
        epoch()
            : base_lsn(lsn_t::null), base(0), start(0), end(0)
        {
        }
        epoch(lsn_t l, long b, long s, long e)
            : base_lsn(l), base(b), start(s), end(e)
        {
        }
    };

    enum { BLOCK_SIZE=8192 };
    enum { SEGMENT_SIZE=128*BLOCK_SIZE };

                                                         // BUG_LOG_SHUTDOWN_FIX is to add this
    virtual void shutdown(); // MUST be called before destructor, it can 
                             // end the daemon thread 

    int insert(char const* data, long size);
    lsn_t flush_cycle(lsn_t old_mark);
    void write_out(long s1, long e1, long s2, long e2);
    void wait_for_space();

    virtual rc_t insert(logrec_t &r, lsn_t* l); // returns the lsn the data was written to
    virtual rc_t flush(lsn_t lsn);
    virtual rc_t compensate(lsn_t orig_lsn, lsn_t undo_lsn);
    virtual void start_flush_daemon();
    void flush_daemon();
    virtual void _flush(lsn_t base_lsn, long start1, long end1, long start2, long end2)=0;
    virtual void	request_checkpoint()=0;

    // initialize the partition size and data_size
    void set_size(fileoff_t psize);



    /*
    Divisions:

    Physical layout:

    The log consists of an unbounded number of "partitions" each
    consisting of a fixed number of "segments." A partition is the
    largest file that will be created and a segment is the size of the
    in-memory buffer. Segments are further divided into "blocks" which
    are the unit of I/O. 

    Threads insert "entries" into the log (log records). 

    One or more entries make up an "epoch" (data that will be flushed
    using a single I/O). Epochs normally end when the flush daemon
    starts to process them. When an epoch reaches the end of a
    segment, the final log entry will usually spill over into the next
    segment and the next entry will begin a new epoch at a non-zero
    offset of the new segment. However, a log entry which would spill
    over into a new partition will begin a new epoch and join it.
    */
    long volatile _waiting_for_space; // is anyone waiting for log space to open up?
    long volatile _waiting_for_flush;
    long volatile _start; // byte number of oldest unwritten byte
    long volatile _end; // byte number of insertion point
    long _segsize;
    long _blocksize;

    epoch _cur_epoch;
    epoch _old_epoch;

    char* _buf;
    lsn_t _flush_lsn;

    tatas_lock _flush_lock;
    tatas_lock _comp_lock;
    queue_based_lock_t _insert_lock;
    pthread_mutex_t _wait_flush_lock; // paired with _wait_cond, _flush_cond
    pthread_cond_t _wait_cond;  // paired with _wait_flush_lock
    pthread_cond_t _flush_cond;  // paird with _wait_flush_lock

    sthread_t* _flush_daemon;
    bool volatile _shutting_down;

    ringbuf_log(long bsize);
    virtual ~ringbuf_log();

    static long floor2(long offset, long block_size) 
        { return offset & -block_size; }

    static long ceil2(long offset, long block_size) 
        { return floor2(offset + block_size - 1, block_size); }

    static bool aligned(long offset, long block_size) 
        { return offset == floor2(offset, block_size); }

    static long floor(long offset, long block_size) 
        { return (offset/block_size)*block_size; }

    static long ceil(long offset, long block_size) 
        { return floor(offset + block_size - 1, block_size); }

};

typedef ringbuf_log log_impl;



/*
 *  NOTE:  If C++ had interfaces (as opposed to classes, which
 *  include the entire private definition of the class), this 
 *  separation of log_m from srv_log wouldn't be necessary.
 *  
 *  It would be nice not to have the extra indirection, but the
 *  if we put the implementation in log_m, in order to compile
 *  the sm, the entire world would have to know the entire implementation.
 */
class log_base : public smlevel_0 {
    friend                 class log_i;

public:
    static lsn_t        first_lsn(uint4_t pnum);

    enum { XFERSIZE =        SM_PAGESIZE };
    // const int XFERSIZE =        SM_PAGESIZE;

    enum { invalid_fhdl = -1 };

    virtual
    NORET                        ~log_base();
protected:
    NORET                        log_base(char *shmbase);
private:
    // disabled
    NORET                        log_base(const log_base&);
    // log_base&                 operator=(const log_base&);


public:
    //////////////////////////////////////////////////////////////////////
    // This is an abstract class; represents interface common to client
    // and server sides
    //////////////////////////////////////////////////////////////////////
#define VIRTUAL(x) /*virtual x = 0;*/
#define NULLARG = 0

    // FRJ: This is kind of a broken design -- we don't necessarily
    // want to expose the same API to the "client" and "server." For
    // now, though we'll just expose server-only stuff to the client
    // and expect the client to ignore it...
    // List: insert's bool is server-only
#define COMMON_INTERFACE\
    VIRTUAL(rc_t                insert(logrec_t& r, lsn_t* ret, bool can_flush=false))        \
    VIRTUAL(rc_t                compensate(const lsn_t &r, const lsn_t &u))\
    VIRTUAL(rc_t                fetch(                  \
        lsn_t&                                 lsn,                \
        logrec_t*&                     rec,                \
        lsn_t*                             nxt NULLARG ) )     \
    VIRTUAL(void                 release())                \
    VIRTUAL(rc_t                flush(const lsn_t& l))  \
    VIRTUAL(rc_t                scavenge(               \
        const lsn_t&                     min_rec_lsn,        \
        const lsn_t&                     min_xct_lsn))         \
    VIRTUAL(void                  set_master(        \
        const lsn_t&                     lsn,\
        const lsn_t&                    min_rec_lsn, \
        const lsn_t&                    min_xct_lsn))

    COMMON_INTERFACE

#undef VIRTUAL
#undef NULLARG


    //////////////////////////////////////////////////////////////////////
    // SHARED DATA:
    // All data members are shared between log processes. 
    // This was supposed to be put in shared memory and it 
    // was supposed to support a multi-process log manager.
    //////////////////////////////////////////////////////////////////////
protected:
    //w_shmem_t                        _shmem_seg;
    // TODO: remove

    struct _shared_log_info {
        bool                        _log_corruption_on;
        // XXX unseen padding
        fileoff_t _max_logsz;        // input param from cli -- partition size
        fileoff_t  _maxLogDataSize;// _max_logsz - sizeof(skiplog record)
    private:
        // MUTEX: insert
        lsn_t                        _curr_lsn;        // lsn of next record
        // MUTEX: flush
        lsn_t                        _durable_lsn;        // max lsn synced to disk

    public:
        lsn_t        curr_lsn() const { return _curr_lsn; }
        lsn_t        durable_lsn() const { return _durable_lsn; }

        
        ///////////////////////////////////////////////////////////////////////
        // set and used for restart and checkpoints
        ///////////////////////////////////////////////////////////////////////
        lsn_t                        _master_lsn;        // lsn of most recent chkpt
        lsn_t                        _old_master_lsn;// lsn of 2nd most recent chkpt
        //    min rec_lsn of dirty page list in the most recent chkpt
        lsn_t                        _min_chkpt_rec_lsn; 


        ///////////////////////////////////////////////////////////////////////
        // a number computed occasionally, used for log-space computations
        // computed by client using info in partitions
        ///////////////////////////////////////////////////////////////////////
        // OWNER: insert
        fileoff_t                _space_available;// in freeable or freed partitions

        _shared_log_info()
        : _log_corruption_on(false),
          _max_logsz(0),
          _maxLogDataSize(0),
          _space_available(0)
        {
        }
    };
    static _shared_log_info        __shared;
    struct _shared_log_info        *_shared;

    
public:
    void                        start_log_corruption() { _shared->_log_corruption_on = true; }
    lsn_t                         master_lsn()        { return _shared->_master_lsn; }
    lsn_t                        old_master_lsn(){ return _shared->_old_master_lsn; }
    lsn_t                        min_chkpt_rec_lsn() { return _shared->_min_chkpt_rec_lsn; }

    // we can't guarantee that our caller holds the proper mutex, so copy it atomically    
    lsn_t                         curr_lsn() const        { return _shared->curr_lsn(); }
    lsn_t                         durable_lsn() const        { return _shared->durable_lsn(); }
protected:
    // these are used internally by callers who *do* hold the proper mutex
    lsn_t                        get_curr_lsn() const        { return _shared->curr_lsn(); }
    lsn_t                         get_durable_lsn() const        { return _shared->durable_lsn(); }
    
public:

    /*********************************************************************
     *
     *  log_base::global_min_lsn()
     *  log_base::global_min_lsn(a)
     *  log_base::global_min_lsn(a,b)
     *
     *  Returns the lowest lsn of a log record that's still needed for
     *  any reason, namely the smallest of the arguments (if present) and
     *  the  _master_lsn and  _min_chkpt_rec_lsn.  
     *  Used to scavenge log space, among other things. 
     *
     *********************************************************************/
    const lsn_t                 global_min_lsn() const {
                                    lsn_t lsn = 
                                    MIN(_shared->_master_lsn, _shared->_min_chkpt_rec_lsn);
                                    return lsn;
                                }

    const lsn_t                 global_min_lsn(const lsn_t& a) const {
                                    lsn_t lsn = global_min_lsn(); 
                                    lsn = MIN(lsn, a);
                                    return lsn;
                                }

    const lsn_t                 global_min_lsn(const lsn_t& min_rec_lsn, 
                                                const lsn_t& min_xct_lsn) const {
                                    lsn_t lsn =  global_min_lsn();
                                    lsn = MIN(lsn, min_rec_lsn);
                                    lsn = MIN(lsn, min_xct_lsn);
                                    return lsn;
                                }
public:
    ////////////////////////////////////////////////////////////////////////
    // MISC:
    ////////////////////////////////////////////////////////////////////////
    enum { max_open_log = smlevel_0::max_openlog };
    // const        max_open_log = smlevel_0::max_openlog;

    ////////////////////////////////////////////////////////////////////////
    // check_raw_device: static and used elsewhere in sm. Should really
    // be a stand-alone function in fc/
    ////////////////////////////////////////////////////////////////////////
    static rc_t                        check_raw_device(
                                    const char* devname,
                                    bool&        raw
                                    );
    

    static uint4_t                version_major;
    static uint4_t                version_minor;

    static rc_t                        check_version(
                                    uint4_t        major,
                                    uint4_t        minor
                                    );
    static rc_t                        parse_master_chkpt_string(
                                    istream&                 s,
                                    lsn_t&                master_lsn,
                                    lsn_t&                min_chkpt_rec_lsn,
                                    int&                number_of_others,
                                    lsn_t*                others,
                                    bool&                old_style
                                    );
    static rc_t                        parse_master_chkpt_contents(
                                    istream&            s,
                                    int&                    listlength,
                                    lsn_t*                    lsnlist
                                    );
    static void                        create_master_chkpt_string(
                                    ostream&                o,
                                    int                        arraysize,
                                    const lsn_t*        array,
                                    bool                old_style = false
                                    );
    static void                        create_master_chkpt_contents(
                                    ostream&        s,
                                    int                arraysize,
                                    const lsn_t*        array
                                    );
};

inline lsn_t 
log_base::first_lsn(uint4_t pnum) { 
    return lsn_t(pnum, 0); 
}

class log_i {
public:
    NORET                        log_i(log_m& l, const lsn_t& lsn) ;
    NORET                        ~log_i();

    bool                         next(lsn_t& lsn, logrec_t*& r);
    w_rc_t&                      get_last_rc();
private:
    log_m&                       log;
    lsn_t                        cursor;
    w_rc_t                       last_rc;
};

inline NORET
log_i::log_i(log_m& l, const lsn_t& lsn) 
    : log(l), cursor(lsn)
{
    DBG(<<"creating log_i with cursor " << lsn);
}

inline
log_i::~log_i()
{
    last_rc.verify();
}

inline w_rc_t&
log_i::get_last_rc()
{
    return last_rc;
}

/*<std-footer incl-file-exclusion='LOG_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
