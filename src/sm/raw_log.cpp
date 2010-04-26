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

 $Id: raw_log.cpp,v 1.56.2.7 2010/03/25 18:05:14 nhall Exp $

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

#include <cstdlib>
#include "sm_int_1.h"
#include "logdef_gen.cpp"
#include "logtype_gen.h"
#include "srv_log.h"
#include "raw_log.h"
#include "log_buf.h"

#include <w_strstream.h>

int                raw_log::_fhdl_rd = invalid_fhdl;
int                raw_log::_fhdl_app = invalid_fhdl;


__thread queue_based_lock_t::ext_qnode raw_log_me_node;
void                
raw_log::acquire_partition_mutex() const {
    _partition_mutex.acquire(&raw_log_me_node);
}

void             
raw_log::partition_lock_t::release() { 
    _log->_partition_mutex.release(raw_log_me_node);
    dont_release(); 
}
/*********************************************************************
 * 
 *  raw_log::_make_master_name(master_lsn, min_chkpt_rec_lsn, buf, bufsz)
 *
 *  Make up the name of a master record in buf.
 *
 *********************************************************************/
void
raw_log::_make_master_name(
    const lsn_t&         master_lsn, 
    const lsn_t&        min_chkpt_rec_lsn,
    char*                 buf,
    int                        _bufsz)
{
    FUNC(raw_log::_make_master_name);
    w_ostrstream s(buf, (int) _bufsz);
    lsn_t         array[PARTITION_COUNT];
    array[0] = master_lsn;
    array[1] = min_chkpt_rec_lsn;
    create_master_chkpt_string(s, 2, array);

    /*
     * Append a delimiter and a separator
     */
    s << "X"; // delimiter

    int j=0;
    for(int i=0; i < PARTITION_COUNT; i++) {
        const partition_t *p = this->i_partition(i);
        if(p->num() > 0 && (p->last_skip_lsn().hi() == p->num())) {
            array[j++] = p->last_skip_lsn();
        }
    }
    create_master_chkpt_contents(s, j, array);
    w_assert1(s);
}


/*********************************************************************
 *
 *  raw_log::raw_log(logdir, int, int, char*, reformat)
 *
 *  Hidden constructor. Open and scan logdir for master lsn and last log
 *  file. Truncate last incomplete log record (if there is any)
 *  from the last log file.
 *
 *********************************************************************/


NORET
raw_log::raw_log(
        const char* /*logdir*/, 
        int         wrbufsize,
        char *shmbase,
        bool /* reformat*/
) 
: srv_log(wrbufsize, shmbase),
  _dev_bsize(DEV_BSIZE)        /* unix-ish default, could be 0 */
{
    FUNC(raw_log::raw_log);

    W_FATAL_MSG(eINTERNAL, << "Raw Log is not supported");
}



/*********************************************************************
 * 
 *  raw_log::~raw_log()
 *
 *  Destructor. Close all open partitions.
 *
 *********************************************************************/
raw_log::~raw_log()
{
    FUNC(raw_log::~raw_log);

    w_rc_t e;
    if (_fhdl_rd != invalid_fhdl)  {
                e = me()->close(_fhdl_rd);
                if (e.is_error()) {
                        cerr << "warning: raw_log on close(rd):" << endl << e << endl;
                }
                _fhdl_rd = invalid_fhdl;
    }
    if (_fhdl_app != invalid_fhdl)  {
                e = me()->close(_fhdl_app);
                if (e.is_error()) {
                        cerr << "warning: raw log on close(app):" << endl << e << endl;
                }
                _fhdl_app = invalid_fhdl;
    }
}


partition_t *
raw_log::i_partition(partition_index_t i) const
{
    return i<0 ? (partition_t *)0: (partition_t *) &_part[i];
}

// MUTEX: partition
void
raw_log::_read_master(
    lsn_t& l,
    lsn_t& min,
    lsn_t* lsnlist,
    int&   listlength
) 
{
    FUNC(raw_log::_read_master);
    int fd = _fhdl_rd;
    w_assert3(fd);

    /* XXX unfortunately this means that logs only work with 512 byte
       devices. But all that's really required is that checkpt block
       size be a mpl of the the device block size and be large enough 
       to hold the master.
    */
    w_assert1(CHKPT_META_BUF == _dev_bsize);

    DBG(<<"read_master seeking to " << 0);
    
    w_rc_t e;
    e = me()->pread(fd, readbuf(), CHKPT_META_BUF, 0);
    if (e.is_error()) {
        smlevel_0::errlog->clog << error_prio 
            << "ERROR: could not read checkpoint: "
            << flushl;
        W_COERCE(e);
    } 
    /*
     * Locate the separator 
     */
    char *REST=0, *c = readbuf();
    for(int i=0; i < CHKPT_META_BUF; i++) {
        if( *(c+i) == 'X') {
            // replace with a null char
            *(c+i) = '\0';
            i++;
            REST = (c + i);
            DBG(<<"Located X separator - prior buf is " << c
                << " REST = " << REST);
            break;
        }
    }

    /* XXX Possible loss of bits in cast */
    w_assert1(readbufsize() < fileoff_t(w_base_t::int4_max));
    { 
        w_istrstream s(readbuf(), int(readbufsize()));
        lsn_t tmp; lsn_t tmp1;

        bool  old_style;
        listlength=0;
        rc_t rc;
        rc = parse_master_chkpt_string(s, tmp, tmp1, 
                                       listlength, lsnlist,
                                       old_style);
        if (rc.is_error())  {
            smlevel_0::errlog->clog << error_prio 
                << "Bad checkpoint master record name -- " 
                << " you must reformat the log."
                << flushl;
            W_COERCE(rc);
        }
        l = tmp;
        min = tmp1;
    }

    if( *REST != '\0' ) {
        rc_t rc;
        w_istrstream s(REST, int(readbufsize()) - (REST - readbuf()));
        rc = parse_master_chkpt_contents(s, listlength, lsnlist);
        if (rc.is_error()) {
            smlevel_0::errlog->clog << error_prio 
                    << "Bad checkpoint master contents -- " 
                    << " you must reformat the log."
                    << flushl;
            W_COERCE(rc);
        }
    }


}

void
raw_log::_write_master(
    lsn_t l,
    lsn_t min
) 
{
    FUNC(raw_log::_write_master);
    /*
     *  create new master record
     */
    char _chkpt_meta_buf[CHKPT_META_BUF];
    _make_master_name(l, min, _chkpt_meta_buf, CHKPT_META_BUF);

    {        /* write ending lsns into the master chkpt record */

        /* cast away const-ness of _chkpt_meta_buf */
        char *_ptr = (char *)_chkpt_meta_buf + strlen(_chkpt_meta_buf);

        lsn_t         array[PARTITION_COUNT];
        int j=0;
        for(int i=0; i < PARTITION_COUNT; i++) {
            const partition_t *p = this->i_partition(i);
            if(p->num() > 0 && (p->last_skip_lsn().hi() == p->num())) {
                array[j++] = p->last_skip_lsn();
            }
        }
        if(j > 0) {
            w_ostrstream s(_ptr, CHKPT_META_BUF);
            create_master_chkpt_contents(s, j, array);
        } else {
            memset(_ptr, '\0', 1);
        }
        DBG(<< " #lsns=" << j
            << "write this to master checkpoint record: " <<
                _ptr);

    }

    DBG(<< "writing checkpoint master: " << _chkpt_meta_buf);


    int fd = fhdl_app();

    w_assert3(fd);

    DBG(<<"write_master seeking to " << 0);
    w_rc_t e = me()->pwrite(fd, _chkpt_meta_buf, CHKPT_META_BUF, 0);
    if (e.is_error()) {
        smlevel_0::errlog->clog << error_prio
            << "ERROR: could not write checkpoint: "
            << _chkpt_meta_buf << flushl;
        W_COERCE(e);
    } 
}

/*********************************************************************
 * raw_partition class
 *********************************************************************/

void                        
raw_partition::set_fhdl_app(int /*fd*/)
{
   // This is a null operation in the raw case because
   // the file descriptors are opened ONCE at startup.

   w_assert3(fhdl_app() != invalid_fhdl);
}

void
raw_partition::open_for_read(
    partition_number_t __num,
    bool err // = true -- if true, consider it an error
        // if the partition doesn't exist
) 
{
    FUNC(raw_partition::open_for_read);
    w_assert3(fhdl_rd() != invalid_fhdl);
    if( err && !exists()) {
        smlevel_0::errlog->clog << error_prio 
            << "raw_log: partition " << __num 
            << " does not exist"
             << flushl;
        /* XXX bogus error info */
        W_FATAL(smlevel_0::eINTERNAL);
    }
    set_state(m_open_for_read);
    return ;
}
void raw_partition::close_for_read() {}
void raw_partition::close_for_append() {}

void
raw_partition::close(bool /* both */)
{
    FUNC(raw_partition::close);

    // We have to flush the writebuf
    // if it's "ours", which is to say,
    // if this is the current partition
    if(is_current()) {
        W_FATAL(fcINTERNAL); // this can't happen any more. Caller?
        /*
        w_assert3(
            writebuf().firstbyte().hi() == num()
                ||
            writebuf().firstbyte() == lsn_t::null);
        */
        //        flush(fhdl_app());
        _owner->unset_current();
    }

    // and invalidate it
    w_assert3(! is_current());

    clr_state(m_open_for_read);
    clr_state(m_open_for_append);
}


void 
raw_partition::sanity_check() const
{

    if(num() == 0) {
       // initial state
       w_assert3(size() == nosize);
       w_assert3(!is_open_for_read());
       w_assert3(!is_open_for_append());
       w_assert3(!exists());
       // don't even ask about flushed
    } else {
       w_assert3(exists());
       (void) is_open_for_read();
       (void) is_open_for_append();
    }
    if(is_current()) {
       w_assert3(is_open_for_append());
#if dont_do_this
       // FRJ: flushed status is too volatile to assert on
        // TODO-- rewrite this
        if(flushed() && _owner->writebuf()->firstbyte().hi()==num()) {
            w_assert3(_owner->writebuf()->flushed().lo() == size());
        }
#endif
    }
}

void
raw_partition::_clear()
{
}

void
raw_partition::_init(srv_log *owner)
{
    clear();
    _owner = owner;
}

int
raw_partition::fhdl_rd() const
{
#if W_DEBUG_LEVEL > 2
    bool isopen = is_open_for_read();
    if(raw_log::_fhdl_rd == invalid_fhdl) {
        w_assert3( !isopen );
    }
#endif 
    return raw_log::_fhdl_rd;
}

int
raw_partition::fhdl_app() const
{
    return raw_log::_fhdl_app;
}

/**********************************************************************
 *
 *  raw_partition::destroy()
 *
 *  Destroy a partition
 *
 *********************************************************************/
void
raw_partition::destroy()
{
    FUNC(raw_partition::destroy);
    // Write a skip record into the partition
    // Let the lsn of the skip record be determined
    // by   : num() if this partition already exists
    // ow by: _index if this is a matter of starting over,
    // (we don't have a num() for the partition).


    if(num()>0) {
        w_assert3(num() < _owner->global_min_lsn().hi());
        w_assert3(  exists());
        w_assert3(! is_current() );
        w_assert3(! is_open_for_append() );
    } else {
        _num = _index;
    }

    skip(lsn_t::null, fhdl_app());

    clear();
    clr_state(m_exists);
    clr_state(m_flushed);
    clr_state(m_open_for_read);
    clr_state(m_open_for_append);


    sanity_check();
}




/**********************************************************************
 *
 *  raw_partition::peek(num, recovery, fd) -- for raw only
 */
void
raw_partition::peek(
    partition_number_t         num_wanted,
    const lsn_t&        end_hint,
    bool                 recovery,
    int *                fdp
)
{
    FUNC(raw_partition::peek);
    int fd = fhdl_app();
    w_assert3(fd);
    if(fdp) *fdp  = fd;

    w_assert3(num_wanted == end_hint.hi() || end_hint.hi() == 0);

    _peek(num_wanted, end_hint.lo(), (_eop - start()), recovery, fd);
}

