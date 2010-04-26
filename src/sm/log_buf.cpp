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

 $Id: log_buf.cpp,v 1.33.2.5 2010/03/19 22:20:24 nhall Exp $

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

#include "sm_int_1.h"
#include "log.h"
#ifdef __GNUG__
#pragma implementation "log_buf.h"
#endif
#include "log_buf.h"
#include "logdef_gen.cpp"
#include "crash.h"

#include <new>


/*********************************************************************
 * class log_buf
 *
 * FRJ:
 * The original log_buf implementation assumed a single mutex
 * (externally provided by log_m) protected the entire log
 * buffer. This introduced a massive serialization bottleneck because
 * every log write/flush (performed while holding the mutex) delayed
 * buffered log inserts.
 *
 * The new implementation assumes two mutexen:
 *
 * "flush mutex" protects state related to the logical front of the
 * buffer: _lsn_firstbyte, _lsn_flushed, _buf_offset, _buf_writing
 *
 * "insert mutex" protects state related to the logical end of the
 * buffer: _lsn_nextrec, _lsn_lastrec, _buf_end.
 *
 * Assumption:
 * - The log is always primed for append (ie everyone who calls
 * partition_t::open also calls log_buf::prime), so log inserts need
 * not worry about priming.
 * - The flush mutex is always acquired before the insert mutex
 * - Every caller of write_to wants a skip_log appended to the log stream
 *
 *********************************************************************/

// MUTEX: insert, comp
bool
log_buf::compensate(const lsn_t &rec, const lsn_t &undo_lsn) 
{
    if (rec == undo_lsn)  {
    // somewhere in the calling code, we didn't actually
    // log anything.  so this would be a compensate to
    // myself.  i.e. a no-op
    // FRJ: the caller should have checked this before grabbing the mutex!
    W_FATAL(fcINTERNAL);
    }

    CRITICAL_SECTION(cs, _comp_mutex);
    //
    // We check lastrec() (last logrecord written into this buffer)
    // instead of nextrec() because
    // we don't want a compensation rec to point to itself.
    //
    // FRJ: even though nextrec() and lastrec() could change at any
    // time, they *are* coherent and grow monotonically; the worst
    // that can happen is we return false when we needn't have (and
    // for the life of me I can't figure how rec could be larger than
    // either of those two!)
    //
    if((writing() < rec && nextrec() > rec)
    && 
    (lastrec() > undo_lsn)
    ) {
    w_assert3(rec.hi() == firstbyte().hi());

    fileoff_t offset = rec.lo() - firstbyte().lo();

    logrec_t *s = (logrec_t *)(_buf + offset);
    w_assert1(s->prev() == lsn_t::null ||
        s->prev() >= undo_lsn);

    if( ! s->is_undoable_clr() ) {
        DBGTHRD(<<"REWRITING LOG RECORD " << rec
        << " : " << *s);
        s->set_clr(undo_lsn);
        return true;
    }
    }
    return false;
}

inline
int
log_buf::skiplen() const { return _skip_log->length(); }

// MUTEX: insert
bool
log_buf::fits(const logrec_t &r) const
{
    w_assert3(_nextrec <= bufsize());
    // If r.length() > bufsize(), we've configured with
    // a logbuf too small for the page size
    w_assert1((int)r.length() <= bufsize());
    // unsafe without holding flush mutex
    //    w_assert3(nextrec().lo() - firstbyte().lo() == len());

    return _nextrec + r.length() <= bufsize();
}

// caller holds insert mutex
void
log_buf::insert(const logrec_t &r)
{
    DBGTHRD(<<" BEFORE insert" << *this);
    // had better have been primed
    w_assert3(firstbyte() != lsn_t::null);
    w_assert3(r.type() != logrec_t::t_skip);

    memcpy(_buf + _nextrec, &r, r.length());
    _lsn_lastrec = nextrec();
    _nextrec += r.length();

    // unsafe because we don't hold the flush mutex
    //    w_assert3(nextrec().lo() - firstbyte().lo() == len());

    DBGTHRD(<<" AFTER insert" << *this);
}


//  assumes fd is already positioned at the right place
//  returns the number of bytes written
//  MUTEX: flush
log_buf::fileoff_t 
log_buf::write_to(int fd, bool force)
{
    DBGTHRD(<<" BEFORE write_to" << *this);

    int cc=0;
    int b, xfersize;

    {
    // copy _nextrec into _writing so we have a stable value without
    // freezing out inserts.
    // BUGBUG: This will race on 32-bit platforms with large file support 
    w_assert3(_writing == _flushed);
    CRITICAL_SECTION(cs, _comp_mutex);
    _writing = _nextrec;
    }
    
    /* FRJ: merged log_buf::insertskip into this function because we
       no longer insert a skip record into the buffer -- we write it
       out directly so that others can continue inserting into the
       buffer. 
     */
    
    // Copy a skip record to the end of the buffer.    
    _skip_log->set_lsn_ck(writing());
    // s->fill_xct_attr(tid_t::null, prevrec());
    
    //
    // How much can we write out and dispense with altogether?
    // Take into account the skip record for the purpose of
    // writing, but not for the purpose of advancing the
    // metadata in writebuf.
    //
    // FRJ: We pass two buffers to writev: one contains the slice of
    // the log buffer between _flushed and _writing and the other
    // contains the skip record, plus enough empty space to bring the
    // total up to a multiple of XFERSIZE.
    //
    // Hopefully the OS is smart enough to coalesce the two writes
    // before sending them to disk. If not, and it's a problem
    // (e.g. for direct I/O), the best option is probably to change
    // the log format so we can truly insert a skip record and write
    // it out, then cancel it after the write completes (either by
    // compensating or by clobbering it with a no-op of the same
    // size).
    //
    // The alternative is to assemble the last block by copying data
    // out of the log_buf so we can append the skiplog without messing
    // up concurrent inserts. However, that could be as many as
    // XFERSIZE-skiplen() bytes. The first way avoids large copy at
    // the cost of 16 or 32 wasted bytes per flush.
    //
    fileoff_t datalen = _writing - _flushed;
    if(datalen == 0 && !force) {
    // As of the instant we checked, the buffer was durable. Tell our caller.
    return 0;
    }
    fileoff_t writelen = datalen + skiplen();

    for( b = 0, xfersize = 0; writelen > xfersize; b++) xfersize += XFERSIZE;

    // b = number of whole blocks to write
    // xfersize is the smallest multiple of XFERSIZE
    // that's greater than len()

    w_assert3(xfersize == XFERSIZE * b);

    DBGTHRD(<<"we'll write " << b << " blocks, or " << xfersize
    << " bytes" );

#if W_DEBUG_LEVEL > 2
    DBGTHRD(<<" MIDDLE write_to" << *this);
#endif 

    typedef sdisk_base_t::iovec_t iovec_t;
    iovec_t iov[2] = {
    iovec_t(_buf+_flushed,    datalen), 
    iovec_t(_skip_log,    xfersize - datalen)
    };
    
    w_rc_t e = me()->writev(fd, iov, 2);
    if (e.is_error()) {
    smlevel_0::errlog->clog << error_prio 
      << "ERROR: could not flush log buf:"
      << " fd=" << fd
      << " b=" << b
      << " xfersize=" << xfersize
      << " cc= " << cc
      << ":" << endl
      << e
      << flushl;
    W_COERCE(e);
    }

#ifdef W_DEBUG_AND_NOT_BROKEN
    // FRJ: This is broken because we don't store the skip record in
    // _buf any more, and we don't depend on the caller to remember to
    // insert the skip record -- we write it explicitly.
    {
    //  nextrec() should always be the start of the last
    //  durable skip record.
    w_assert3(firstbyte().hi() == nextrec().hi());
    lsn_t off = nextrec();
    // nextrec() - firstbyte() == offset
    off.advance(-firstbyte().lo());
    int f = off.lo();
    logrec_t *l = (logrec_t *)(buf() + f);
    w_assert3(l->type() == s->type());
    lsn_t ck =  l->get_lsn_ck();
    w_assert3(ck == nextrec());
    w_assert3((int)l->length() == skiploglen );
    }
#endif

    w_assert3(_flushed+xfersize >= _writing);
    // FRJ: make our caller update this so it can sync the file first
    //    _flushed = _writing;

    DBGTHRD(<<" AFTER write_to" << *this);
    return xfersize;
}

// MUTEX: flush
void log_buf::mark_durable() {
    w_assert3(_flushed <= _writing);
    _flushed = _writing;
}
    
// moves all unwritten data to the front of the buffer (making
// _written < XFERSIZE) presumably because fits() returned
// false.
// MUTEX: insert, flush
void log_buf::compact() {
    DBGTHRD(<<" BEFORE compact" << *this);
    /* FRJ: This code is *vastly* simplified over what used to occur
       at the end of write_to because the skip_log never gets written
       to the buffer. Even if we changed things to write a skip_log
       entry, we wouldn't have to go back to what existed previously
       (I don't think).

       We do still have to make sure we respect block boundaries,
       though.
     */
    w_assert3(_flushed == _writing);
    w_assert3(_flushed < _bufsize);

    // are we already at front?
    if(_flushed < XFERSIZE)
    return;

    fileoff_t flushed_offset = _flushed % XFERSIZE;
    fileoff_t flushed_block = _flushed - flushed_offset;
    fileoff_t region_size = _nextrec - flushed_block;
    fileoff_t region_offset = flushed_block;

    // memcpy is faster than memmove, but breaks if the regions overlap
    if(region_size > region_offset) 
    memmove(_buf, _buf+region_offset, region_size);
    else
    memcpy(_buf, _buf+region_offset, region_size);

    // update the metadata
    // slide the buffer forward and decrement the offsets to compensate
    // NOTE: compaction does not affect written/durable status
    _lsn_firstbyte.advance(region_offset);
    _flushed -= region_offset;
    _writing  = _flushed;
    _nextrec -= region_offset;

    DBGTHRD(<<" AFTER compact" << *this);
}

// carve off the first XFERSIZE bytes of b for the skip_log
log_buf::log_buf(char *b, fileoff_t sz)
    :
    _buf(b+XFERSIZE),
    _lsn_firstbyte(lsn_t::null),
    _flushed(0),
    _writing(0),
    _lsn_lastrec(lsn_t::null),
    _nextrec(0),
    _bufsize(sz-XFERSIZE),
    _skip_log(new(b) skip_log)
{
    // We need a minimum of two blocks: one to hold a (partial) block
    // of data and one to hold the skip_log. If we also want to insert
    // log entries, it would make sense to have a lot more blocks in
    // the buffer, but isn't technically required
    w_assert2(sz >= mem_needed());
    // on 64-bit we are always aligned because _skip_log_data is
    // after the _skip_log pointer. On other platforms we may not
    // be aligned, but skip_log has code to deal with that safely.
}

log_buf::~log_buf()
{
    // don't delete, since addr is provided/built-in
    _buf = 0;
    _skip_log = 0;
}

// MUTEX: flush and insert
void         
log_buf::prime(int fd, fileoff_t offset, const lsn_t &next)
{
    FUNC(log_buf::prime);
    // we are about to write a record for a certain lsn,
    // which maps to the given offset.
    //  But if this is start-up and we've initialized
    // with a partial partition, we have to prime the
    // buf with the last block in the partition

    fileoff_t b = (next.lo() / XFERSIZE) * XFERSIZE;

    DBGTHRD(<<" BEFORE prime" << *this);

    DBG(<<"firstbyte()=" << firstbyte() << " next = " << next);
#if W_DEBUG_LEVEL > 2
    if( firstbyte().hi() == next.hi() ) {
    // same partition
    // it never makes sense to prime or to have primed
    // primed the buffer for anything less than the
    // durable lsn, since this buffer is used only for
    // appending records
    w_assert3(b <= nextrec().lo());
    w_assert3(next == nextrec());

    if(b >= firstbyte().lo()) {
        // it's in the range covered by the buf

        w_assert3(b <= nextrec().lo());
    }
    }
#endif 

    if( firstbyte().hi() != next.hi() ||
        (
    // implied: firstbyte().hi() == next.hi() &&
       b < firstbyte().lo() ) 
    ) {

#if W_DEBUG_LEVEL > 2
    w_assert3(flushed().lo() == nextrec().lo());
    // i.e., it's durable and it's
    // ok to lose the data here
#endif 

    //
    // this should happen only when we've just got
    // the size at start-up and it's not a full block
    // or
    // we've been scavenging and closing partitions
    // in the midst of normal operations
    // 


    //
    lsn_t first = lsn_t(uint4_t(next.hi()), sm_diskaddr_t(b));
    if(first != next) {
        w_assert3(first.lo() < next.lo());
        offset += first.lo();

        DBG(<<" reading " << int(XFERSIZE) << " from " << offset << "on fd " << fd );
        int n = 0;
        w_rc_t e = me()->pread(fd, _buf, XFERSIZE, offset);
        if (e.is_error()) {
        smlevel_0::errlog->clog << error_prio 
                    << "cannot read log: lsn " << first << flushl;
        smlevel_0::errlog->clog << error_prio 
                    << "read(): " << e << flushl;
        smlevel_0::errlog->clog << error_prio 
                    << "read() returns " << n << flushl;
        W_COERCE(e);
        }
    }
    _lsn_lastrec = lsn_t::null;
    _lsn_firstbyte = first;
    _flushed = _writing = _nextrec = next.lo() - first.lo();
    }
    w_assert3(firstbyte().hi() == next.hi());
    w_assert3(firstbyte().lo() <= next.lo());
    w_assert3(flushed().lo() <= nextrec().lo());
    w_assert3(nextrec()== next);
    DBGTHRD(<<" AFTER prime" << *this);
}

ostream&     
operator<<(ostream &o, const log_buf &l) 
{
    o <<" firstbyte()=" << l.firstbyte()
      <<" flushed()=" << l.flushed()
      <<" writing()=" << l.writing()
      <<" nextrec()=" << l.nextrec()
      <<" lastrec()=" << l.lastrec();

#ifdef SERIOUS_DEBUG
    if(l.firstbyte().hi() > 0 && l.firstbyte().lo() == srv_log::zero) {
    // go forward
    _w_debug.clog << "FORWARD:" <<  flushl;
    int       i=0;
    char      *b;
    lsn_t       pos =  l.firstbyte();
    logrec_t  *r;

    // limit to 10
    while(pos.lo() < l.len() && i++ < 10) {
        b = l.buf + pos.lo();
        r = (logrec_t *)b;
        _w_debug.clog << "pos: " << pos << " -- contains " << *r << flushl;

        b += r->length();
        pos.advance(r->length()); 
    }
    } else if(l.lastrec() != lsn_t::null) {
        _w_debug.clog << "BACKWARD: " << flushl;

    char      *b = l.buf + l.len() - (int)sizeof(lsn_t);
    lsn_t       pos =  *(lsn_t *)b;
    lsn_t       lsn_ck;

    w_assert3(pos == l.lastrec() || l.lastrec() == lsn_t::null);
    w_assert3(pos.hi() == l.firstbyte().hi());

    logrec_t  *r;
    while(pos.lo() >= l.firstbyte().lo()) {
        w_assert3(pos.hi() == l.firstbyte().hi());
        b = l.buf + (pos.lo() - l.firstbyte().lo());
        r = (logrec_t *)b;

        // lsn_ck = *((lsn_t *)((char *)r + (r->length() - sizeof(lsn_t))));
        memcpy(&lsn_ck, ((char *)r + (r->length() - sizeof(lsn_t))),
                sizeof(lsn_ck));

        w_assert3(lsn_ck == pos || r->type() == logrec_t::t_skip);

        _w_debug.clog << "pos: " << pos << " -- contains " << *r << flushl;

        b -=  sizeof(lsn_t);
        pos = *((lsn_t *) b);
    }
    }
#endif /*SERIOUS_DEBUG*/

    return o;
}

