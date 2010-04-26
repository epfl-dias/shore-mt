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

 $Id: log_base.cpp,v 1.34.2.8 2010/01/12 02:01:29 nhall Exp $

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

#include <sm_int_0.h>
#include "log_buf.h"

#include <w_strstream.h>

/*********************************************************************
 *
 *  log_base::check_raw_device(devname, raw)
 *
 *  Check if "devname" is a raw device. Return result in "raw".
 *
 * XXX This has problems.  Once the file is opened it should
 * never be closed.  Otherwise the file can be switched underneath
 * the system and havoc can ensue.
 *
 *********************************************************************/
rc_t
log_m::check_raw_device(const char* devname, bool& raw)
{
        w_rc_t        e;
        int        fd;

        raw = false;

        /* XXX should add a stat() to sthread for instances such as this */

        e = me()->open(devname, smthread_t::OPEN_RDONLY, 0, fd);

        if (!e.is_error()) {
                e = me()->fisraw(fd, raw);
                W_IGNORE(me()->close(fd));
        }

        return e;
}


/*********************************************************************
 *
 * log_base::version_major
 * log_base::version_minor
 *
 * increment version_minor if a new log record is appended to the
 * list of log records and the semantics, ids and formats of existing
 * log records remains unchanged.
 *
 * for all other changes to log records or their semantics
 * version_major should be incremented and version_minor set to 0.
 *
 *********************************************************************/

uint4_t const log_m::version_major = 4;
uint4_t const log_m::version_minor = 0;

/* Changes to minor version: 
 * 6/7/99 - added last_durable_skip to each partition.
 */



/*********************************************************************
 *
 * log_base::check_version
 *
 * returns LOGVERSIONTOONEW or LOGVERSIONTOOOLD if the passed in
 * version is incompatible with this sources version
 *
 *********************************************************************/

rc_t
log_m::check_version(uint4_t major, uint4_t minor)
{
        if (major == version_major && minor <= version_minor)
                return RCOK;

        int err = (major < version_major)
                        ? eLOGVERSIONTOOOLD : eLOGVERSIONTOONEW;

        smlevel_0::errlog->clog << error_prio 
            << "ERROR: log version too "
            << ((err == eLOGVERSIONTOOOLD) ? "old" : "new")
            << " sm ("
            << version_major << " . " << version_minor
            << ") log ("
            << major << " . " << minor
            << flushl;

        return RC(err);
}


/*********************************************************************
 *
 * log_base::parse_master_chkpt_string
 *
 * parse and return the master_lsn and min_chkpt_rec_lsn.
 * return an error if the version is out of sync.
 *
 *********************************************************************/
rc_t
log_m::parse_master_chkpt_string(
                istream&            s,
                lsn_t&              master_lsn,
                lsn_t&              min_chkpt_rec_lsn,
                int&                    number_of_others,
                lsn_t*                    others,
                bool&                    old_style)
{
    uint4_t major = 1;
    uint4_t minor = 0;
    char separator;

    s >> separator;

    if (separator == 'v')  {                // has version, otherwise default to 1.0
        old_style = false;
        s >> major >> separator >> minor;
        w_assert9(separator == '.');
        s >> separator;
        w_assert9(separator == '_');
    }  else  {
        old_style = true;
        s.putback(separator);
    }

    s >> master_lsn >> separator >> min_chkpt_rec_lsn;
    w_assert9(separator == '_' || separator == '.');

    if (!s)  {
        return RC(eBADMASTERCHKPTFORMAT);
    }

    number_of_others = 0;
    while(!s.eof()) {
        s >> separator;
        if(separator == '\0') break; // end of string

        if(!s.eof()) {
            w_assert9(separator == '_' || separator == '.');
            s >> others[number_of_others];
            DBG(<< number_of_others << ": extra lsn = " << 
                others[number_of_others]);
            if(!s.fail()) {
                number_of_others++;
            }
        }
    }

    return check_version(major, minor);
}

rc_t
log_m::parse_master_chkpt_contents(
                istream&            s,
                int&                    listlength,
                lsn_t*                    lsnlist
                )
{
    listlength = 0;
    char separator;
    while(!s.eof()) {
        s >> separator;
        if(!s.eof()) {
            w_assert9(separator == '_' || separator == '.');
            s >> lsnlist[listlength];
            DBG(<< listlength << ": extra lsn = " << 
                lsnlist[listlength]);
            if(!s.fail()) {
                listlength++;
            }
        }
    }
    return RCOK;
}


/*********************************************************************
 *
 * log_base::create_master_chkpt_string
 *
 * writes a string which parse_master_chkpt_string expects.
 * includes the version, and master and min chkpt rec lsns.
 *
 *********************************************************************/

void
log_m::create_master_chkpt_string(
                ostream&        s,
                int                arraysize,
                const lsn_t*        array,
                bool                old_style)
{
    w_assert1(arraysize >= 2);
    if (old_style)  {
        s << array[0] << '.' << array[1];

    }  else  {
        s << 'v' << version_major << '.' << version_minor ;
        for(int i=0; i< arraysize; i++) {
                s << '_' << array[i];
        }
    }
}

void
log_m::create_master_chkpt_contents(
                ostream&        s,
                int                arraysize,
                const lsn_t*        array
                )
{
    for(int i=0; i< arraysize; i++) {
            s << '_' << array[i];
    }
    s << ends;
}

/*********************************************************************
 *
 *  log_i::next(lsn, r)
 *
 *  Read the next record into r and return its lsn in lsn.
 *  Return false if EOF reached. true otherwise.
 *
 *********************************************************************/
bool log_i::next(lsn_t& lsn, logrec_t*& r)  
{
    bool eof = (cursor == null_lsn);
    if (! eof) {
        lsn = cursor;
        rc_t rc = log.fetch(lsn, r, &cursor);
        
        // release right away, since this is only
        // used in recovery.
        log.release();

        if (rc.is_error())  {
            last_rc = rc;
            RC_AUGMENT(last_rc);
            RC_APPEND_MSG(last_rc, << "trying to fetch lsn " << cursor);
            
            if (last_rc.err_num() == smlevel_0::eEOF)  
                eof = true;
            else  {
                smlevel_0::errlog->clog << error_prio 
                << "Fatal error : " << RC_PUSH(last_rc, smlevel_0::eINTERNAL) << flushl;
            }
        }
    }
    return ! eof;
}

void    log_base::compute_space() {}
log_base::~log_base() {}

