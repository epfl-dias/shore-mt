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

/*<std-header orig-src='shore' incl-file-exclusion='RAW_LOG_H'>

 $Id: raw_log.h,v 1.19.2.6 2010/03/19 22:20:25 nhall Exp $

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

#ifndef RAW_LOG_H
#define RAW_LOG_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif

class raw_log; // forward

#define DEV_BLOCK_SIZE     512

class raw_partition : public partition_t {
    friend class raw_log;

public:

    // these are abstract in the parent class
    void            _init(srv_log *o);
    void            _clear();
    int             fhdl_rd() const;
    int             fhdl_app() const;
    void            open_for_read(partition_number_t n, bool err=true);
    void            open_for_append(partition_number_t n);
    void            close_for_append();
    void            close_for_read();
    w_rc_t          read(logrec_t *&r, lsn_t &ll, int fd = invalid_fhdl);
    void            close(bool both);
    void            destroy();
    void            sanity_check() const;

    void            peek(partition_number_t n, 
                    const lsn_t& end_hint, bool, int *fd=0);
    void             open_for_append();
        void            set_fhdl_app(int fd);

    void            _flush(int /*fd*/){}
};


class raw_log : public srv_log {
    friend class raw_partition;

    NORET            raw_log(const char* logdir,
                    int wrbufsize,
                    char *shmbase,
                    bool reformat);

    mutable queue_based_block_lock_t _partition_mutex;
    void                acquire_partition_mutex() const;
public:
    // only used in raw_log.cpp so it's probably deprecated
    // NANCY TODO : ask Ryan about this
    struct partition_lock_t {
    private:
        raw_log* _log;
        void             dont_release() { _log = NULL; }
        void             release() ;
    public:
                        partition_lock_t(raw_log* log) : _log(log) { 
                            _log->acquire_partition_mutex(); 
                        }
                        ~partition_lock_t() { if(_log) release(); }
    };

public:
    static    rc_t        new_raw_log(raw_log *&the_log,
                        const char *logdir,
                        int    wrbufsize,
                        char    *shmbase,
                        bool    reformat);
    NORET            ~raw_log();

    virtual void            _write_master(lsn_t l, lsn_t min);
    partition_t *        i_partition(partition_index_t i) const;


    void             _read_master(
                lsn_t& l,
                lsn_t& min,
                lsn_t* lsnlist,
                int&   listlength
                ); 
    void             _make_master_name(
                const lsn_t&     master_lsn, 
                const lsn_t&    min_chkpt_rec_lsn,
                char*         buf,
                int        bufsz);
private:
    raw_partition        _part[PARTITION_COUNT];
    static int            _fhdl_rd;
    static int            _fhdl_app;

    unsigned            _dev_bsize;

protected:
    int                fhdl_rd() const { return _fhdl_rd; }
    int                fhdl_app() const { return _fhdl_app; }
};

/*<std-footer incl-file-exclusion='RAW_LOG_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
