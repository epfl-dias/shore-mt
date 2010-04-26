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

/*<std-header orig-src='shore' incl-file-exclusion='LOG_BUF_H'>

 $Id: log_buf.h,v 1.11.2.6 2010/03/19 22:20:24 nhall Exp $

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

#ifndef LOG_BUF_H
#define LOG_BUF_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#undef ACQUIRE

#ifdef __GNUG__
#pragma interface
#endif

class skip_log; // forward
class log_buf 
{
public:
    typedef smlevel_0::fileoff_t fileoff_t;
    enum { XFERSIZE =    8192 };

    // the absolute minimum to be able to write out skip_log entries. 
    static int mem_needed() { return 2*XFERSIZE; }
private:
    /*
      The log buffer contains four regions:
      
      0                 _flushed        _writing        _nextrec      _bufsize
      | . . . . . . . . | x x x x x x x | * * * * * * * | . . . . . . |
         ---flushed---    ---writing---   --inserted--    ---free---

      The buffer always overlays a contiguous section of exactly one
      log partition, beginning at _lsn_base. _lsn_base.hi() identifies
      which log partition the buffer overlays, and _lsn_base.lo()
      tells how far into the partition this buffer starts.

      OWNER: both
     */
    char     *_buf;


    // first byte log byte overlapped by this buffer
    // (not necessarily the beginning of a log record)
    // OWNER: both
    lsn_t     _lsn_firstbyte;

    // we've written to disk up to (but not including) this offset --
    // lies between 0 and _lsn_next
    // OWNER: flush
    fileoff_t     _flushed;

    // everything between _flushed and _writing is currently being written
    // OWNER: flush
    fileoff_t    _writing;

    // lsn of last log record buffered
    // OWNER: insert
    lsn_t     _lsn_lastrec;
    
    // offset of next log record to be buffered
    // OWNER: insert
    fileoff_t     _nextrec;

    // physical end of buffer
    // OWNER: both
    fileoff_t    _bufsize;

    /* NB: the skip_log is not defined in the class because it would
     necessitate #include-ing all the logrec_t definitions and we
     don't want to do that, sigh.
     
     FRJ: plus, we need to place the skiplog entry at the start of
     XFERSIZE bytes so we don't seg fault when writing out blocks

     OWNER: flush
    */
    skip_log    *_skip_log;

     // guard _writing separately so that compensation doesn't need the flush mutex
    queue_based_lock_t    _comp_mutex;
 
    int         skiplen() const;

    // returns the lsn associated for a given byte in the buffer
    lsn_t    get_lsn(fileoff_t offset) const {
    lsn_t rval = _lsn_firstbyte;
    return rval.advance(offset);
    }

public:

                log_buf(char *, fileoff_t sz);
                ~log_buf();

    const lsn_t    &    firstbyte() const { return _lsn_firstbyte; }
    lsn_t        flushed() const { return get_lsn(_flushed); }
    lsn_t        writing() const { return get_lsn(_writing); }
    lsn_t        nextrec() const { return get_lsn(_nextrec); }
    const lsn_t    &    lastrec() const { return _lsn_lastrec; }
    fileoff_t         bufsize() const { return _bufsize; }

    bool        fits(const logrec_t &l) const;
    void         compact();
    
    void         prime(int, fileoff_t, const lsn_t &);
    void         insert(const logrec_t &r);
    void         mark_durable();

    fileoff_t         write_to(int fd, bool force);
    bool         compensate(const lsn_t &rec, const lsn_t &undo_lsn);

    friend ostream&     operator<<(ostream &, const log_buf &);
};

/*<std-footer incl-file-exclusion='LOG_BUF_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
