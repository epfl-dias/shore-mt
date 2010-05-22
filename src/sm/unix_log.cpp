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

 $Id: unix_log.cpp,v 1.79.2.9 2010/03/19 22:20:28 nhall Exp $

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

#include "sm_int_1.h"
#include "logdef_gen.cpp"
#include "logtype_gen.h"
#include "srv_log.h"
#include "unix_log.h"

#include <w_strstream.h>

/* XXX complete sthread I/O facilities not in place yet */
#include <sys/types.h>
#include <sys/stat.h>

#ifdef __SUNPRO_CC
#include <stdio.h>
#else
#include <cstdio>        /* XXX for log recovery */
#endif

#include <os_interface.h>
#include <largefile_aware.h>


const char unix_log::master_prefix[] = "chk."; // same size as log_prefix
const char unix_log::log_prefix[] = "log.";
const char SLASH = '/';


/*********************************************************************
 *
 *  unix_log::_make_log_name(idx, buf, bufsz)
 *
 *  Make up the name of a log file in buf.
 *
 *********************************************************************/
void
unix_log::_make_log_name(uint4_t idx, char* buf, int bufsz)
{
    // this is a static function w_assert2(_partition_lock.is_mine()==true);
    w_ostrstream s(buf, (int) bufsz);
    s << srv_log::_logdir << SLASH
      << log_prefix << idx << ends;
    w_assert1(s);
}



/*********************************************************************
 * 
 *  unix_log::_make_master_name(master_lsn, min_chkpt_rec_lsn, buf, bufsz)
 *
 *  Make up the name of a master record in buf.
 *
 *********************************************************************/
void
unix_log::_make_master_name(
    const lsn_t&         master_lsn, 
    const lsn_t&        min_chkpt_rec_lsn,
    char*                 buf,
    int                        bufsz,
    bool                old_style)
{
    w_ostrstream s(buf, (int) bufsz);

    s << srv_log::_logdir << SLASH << master_prefix;
    lsn_t         array[2];
    array[0] = master_lsn;
    array[1] = min_chkpt_rec_lsn;

    create_master_chkpt_string(s, 2, array, old_style);
    s << ends;
    w_assert1(s);
}


w_rc_t
unix_log::_read_master( 
        const char *fname,
        int prefix_len,
        lsn_t &tmp,
        lsn_t& tmp1,
        lsn_t* lsnlist,
        int&   listlength,
        bool&  old_style
        )
{
    rc_t         rc;
    {
        /* make a copy */
        int        len = strlen(fname+prefix_len) + 1;
        char *buf = new char[len];
        memcpy(buf, fname+prefix_len, len);
        w_istrstream s(buf);

        rc = parse_master_chkpt_string(s, tmp, tmp1, 
                                       listlength, lsnlist, old_style);
        delete [] buf;
        if (rc.is_error()) {
            smlevel_0::errlog->clog << error_prio 
            << "bad master log file \"" << fname << "\"" << flushl;
            W_COERCE(rc);
        }
        DBG(<<"parse_master_chkpt returns tmp= " << tmp
            << " tmp1=" << tmp1
            << " old_style=" << old_style);
    }

    /*  
     * read the file for the rest of the lsn list
     */
    {
        char*         buf = new char[smlevel_0::max_devname];
        if (!buf)
            W_FATAL(fcOUTOFMEMORY);
        w_auto_delete_array_t<char> ad_fname(buf);
        w_ostrstream s(buf, int(smlevel_0::max_devname));
        s << srv_log::_logdir << SLASH << fname << ends;

        FILE* f = fopen(buf, "r");
        if(f) {
            char _chkpt_meta_buf[CHKPT_META_BUF];
            int n = fread(_chkpt_meta_buf, 1, CHKPT_META_BUF, f);
            if(n  > 0) {
                /* Be paranoid about checking for the null, since a lack
                   of it could send the istrstream driving through memory
                   trying to parse the information. */
                void *null = memchr(_chkpt_meta_buf, '\0', CHKPT_META_BUF);
                if (!null) {
                    smlevel_0::errlog->clog << error_prio 
                        << "invalid master log file format \"" 
                        << buf << "\"" << flushl;
                    W_FATAL(eINTERNAL);
                }
                    
                w_istrstream s(_chkpt_meta_buf);
                rc = parse_master_chkpt_contents(s, listlength, lsnlist);
                if (rc.is_error())  {
                    smlevel_0::errlog->clog << error_prio 
                        << "bad master log file contents \"" 
                        << buf << "\"" << flushl;
                    W_COERCE(rc);
                }
            }
            fclose(f);
        } else {
            /* backward compatibility with minor version 0: 
             * treat empty file ok
             */
            w_rc_t e = RC(eOS);
            smlevel_0::errlog->clog << error_prio
                << "ERROR: could not open existing log checkpoint file: "
                << buf << flushl;
            W_COERCE(e);
        }
    }
    return RCOK;
}

/*********************************************************************
 *
 *  unix_log::unix_log(logdir, segmentid, reformat)
 *
 *  Hidden constructor. Open and scan logdir for master lsn and last log
 *  file. Truncate last incomplete log record (if there is any)
 *  from the last log file.
 *
 *********************************************************************/
NORET
unix_log::unix_log(const char* logdir, 
    int wrbufsize,
    char *shmbase,
    bool reformat) 
: srv_log(wrbufsize, shmbase)
{
    set_size(max_logsz);
    FUNC(unix_log::unix_log);

    // FRJ: we don't actually *need* this (no trx around yet), but we
    // don't want to trip the assertions that watch for it.
    CRITICAL_SECTION(cs, _partition_lock);

    partition_number_t  last_partition = partition_num();
    bool                last_partition_exists = false;
    /* 
     * make sure there's room for the log names
     */
    fileoff_t eof= fileoff_t(0);

    w_assert1(strlen(logdir) < sizeof(srv_log::_logdir));
    strcpy(srv_log::_logdir, logdir);

    os_dirent_t *dd=0;
    os_dir_t ldir = os_opendir(srv_log::_logdir);
    if (! ldir) 
    {
        w_rc_t e = RC(eOS);
        smlevel_0::errlog->clog << error_prio 
            << "Error: could not open the log directory " << srv_log::_logdir <<flushl;
        smlevel_0::errlog->clog << error_prio 
            << "\tNote: the log directory is specified using\n" 
            "\t      the sm_logdir option." << flushl;
        W_COERCE(e);
    }
    DBGTHRD(<<"opendir " << srv_log::_logdir << " succeeded");

    /*
     *  scan directory for master lsn and last log file 
     */

    _master_lsn = null_lsn;

    uint4_t min_index = max_uint4;

    char *fname = new char [smlevel_0::max_devname];
    if (!fname)
        W_FATAL(fcOUTOFMEMORY);
    w_auto_delete_array_t<char> ad_fname(fname);

    /* Create a list of lsns for the partitions - this
     * will be used to store any hints about the last
     * lsns of the partitions (stored with checkpoint meta-info
     */ 
    lsn_t lsnlist[PARTITION_COUNT];
    int   listlength=0;
    {
        /*
         *  initialize partition table
         */
        partition_index_t i;
        for (i = 0; i < PARTITION_COUNT; i++)  {
            _part[i].init_index(i);
            _part[i]._init(this);
        }
    }

    DBGTHRD(<<"reformat= " << reformat 
            << " last_partition "  << last_partition
            << " last_partition_exists "  << last_partition_exists
            );
    if (reformat) 
    {
        smlevel_0::errlog->clog << error_prio 
            << "Reformatting logs..." << endl;

        while ((dd = os_readdir(ldir)))  
        {
            DBGTHRD(<<"master_prefix= " << master_prefix);

            unsigned int namelen = strlen(log_prefix);
            namelen = namelen > strlen(master_prefix)? namelen :
                                        strlen(master_prefix);

            const char *d = dd->d_name;
            unsigned int orig_namelen = strlen(d);
            namelen = namelen > orig_namelen ? namelen : orig_namelen;

            char *name = new char [namelen+1];
            w_auto_delete_array_t<char>  cleanup(name);

            memset(name, '\0', namelen+1);
            strncpy(name, d, orig_namelen);
            DBGTHRD(<<"name= " << name);

            bool parse_ok = (strncmp(name,master_prefix,strlen(master_prefix))==0);
            if(!parse_ok) {
                parse_ok = (strncmp(name,log_prefix,strlen(log_prefix))==0);
            }
            if(parse_ok) {
                smlevel_0::errlog->clog << error_prio 
                    << "\t" << name << "..." << endl;

                {
                    w_ostrstream s(fname, (int) smlevel_0::max_devname);
                    s << srv_log::_logdir << SLASH << name << ends;
                    w_assert1(s);
                    if( unlink(fname) < 0) {
                        w_rc_t e = RC(fcOS);
                        smlevel_0::errlog->clog << error_prio 
                            << "unlink(" << fname << "):"
                            << endl << e << endl;
                    }
                }
            }
        } 

        //  os_closedir(ldir);
        w_assert3(!last_partition_exists);
    }

    DBGTHRD(<<"about to readdir"
            << " last_partition "  << last_partition
            << " last_partition_exists "  << last_partition_exists
            );

    while ((dd = os_readdir(ldir)))  
    {
        DBGTHRD(<<"dd->d_name=" << dd->d_name);

        // XXX should abort on name too long earlier, or size buffer to fit
        const unsigned int prefix_len = strlen(master_prefix);
        w_assert3(prefix_len < smlevel_0::max_devname);

        char *buf = new char[smlevel_0::max_devname+1];
        if (!buf)
                W_FATAL(fcOUTOFMEMORY);
        w_auto_delete_array_t<char>  ad_buf(buf);

        unsigned int         namelen = prefix_len;
        const char *         dn = dd->d_name;
        unsigned int         orig_namelen = strlen(dn);

        namelen = namelen > orig_namelen ? namelen : orig_namelen;
        char *                name = new char [namelen+1];
        w_auto_delete_array_t<char>  cleanup(name);

        memset(name, '\0', namelen+1);
        strncpy(name, dn, orig_namelen);

        strncpy(buf, name, prefix_len);
        buf[prefix_len] = '\0';

        DBGTHRD(<<"name= " << name);

        bool parse_ok = ((strlen(buf)) == prefix_len);

        DBGTHRD(<<"parse_ok  = " << parse_ok
                << " buf = " << buf
                << " prefix_len = " << prefix_len
                << " strlen(buf) = " << strlen(buf));
        if (parse_ok) {
            lsn_t tmp;
            if (strcmp(buf, master_prefix) == 0)  
            {
                DBGTHRD(<<"found log file " << buf);
                /*
                 *  File name matches master prefix.
                 *  Extract master lsn & lsns of skip-records
                 */
                lsn_t tmp1;
                bool old_style=false;
                rc_t rc = _read_master(name, prefix_len, 
                        tmp, tmp1, lsnlist, listlength,
                        old_style);
                W_COERCE(rc);

                if (tmp < master_lsn())  {
                    /* 
                     *  Swap tmp <-> _master_lsn, tmp1 <-> _min_chkpt_rec_lsn
                     */
                    std::swap(_master_lsn, tmp);
                    std::swap(_min_chkpt_rec_lsn, tmp1);
                }
                /*
                 *  Remove the older master record.
                 */
                if (_master_lsn != lsn_t::null) {
                    _make_master_name(_master_lsn,
                                      _min_chkpt_rec_lsn,
                                      fname,
                                      smlevel_0::max_devname);
                    (void) unlink(fname);
                }
                /*
                 *  Save the new master record
                 */
                _master_lsn = tmp;
                _min_chkpt_rec_lsn = tmp1;
                DBG(<<" _master_lsn=" << _master_lsn
                 <<" _min_chkpt_rec_lsn=" << _min_chkpt_rec_lsn);

                DBG(<<"parse_ok = " << parse_ok);

            } else if (strcmp(buf, log_prefix) == 0)  {
                DBGTHRD(<<"found log file " << buf);
                /*
                 *  File name matches log prefix
                 */

                w_istrstream s(name + prefix_len);
                uint4_t curr;
                if (! (s >> curr))  {
                    smlevel_0::errlog->clog << error_prio 
                    << "bad log file \"" << name << "\"" << flushl;
                    W_FATAL(eINTERNAL);
                }

                DBGTHRD(<<"curr " << curr
                        << " partition_num()==" << partition_num() 
                        << " last_partition_exists " << last_partition_exists
                        );

                if (curr >= last_partition) {
                    last_partition = curr;
                    last_partition_exists = true;
                    DBGTHRD(<<"new last_partition " << curr
                        << " exits=true" );
                }
                if (curr < min_index) {
                    min_index = curr;
                }
            } else {
                DBGTHRD(<<"NO MATCH");
                DBGTHRD(<<"master_prefix= " << master_prefix);
                DBGTHRD(<<"log_prefix= " << log_prefix);
                DBGTHRD(<<"buf= " << buf);
                parse_ok = false;
            }
        } 

        /*
         *  if we couldn't parse the file name and it was not "." or ..
         *  then print an error message
         */
        if (!parse_ok && ! (strcmp(name, ".") == 0 || 
                                strcmp(name, "..") == 0)) {
            smlevel_0::errlog->clog << error_prio 
                << "unix_log: cannot parse " << name << flushl;
            W_FATAL(fcINTERNAL);
        }
    }
    os_closedir(ldir);

    DBGTHRD(<<"after closedir  " 
            << " last_partition "  << last_partition
            << " last_partition_exists "  << last_partition_exists
            );

#if W_DEBUG_LEVEL > 2
    if(reformat) {
        w_assert3(partition_num() == 1);
        w_assert3(_min_chkpt_rec_lsn.hi() == 1);
        w_assert3(_min_chkpt_rec_lsn.lo() == first_lsn(1).lo());
    } else {
       // ??
    }
    w_assert3(partition_index() == -1);
#endif 

    DBGTHRD(<<"Last partition is " << last_partition
        << " existing = " << last_partition_exists
     );

    /*
     *  Destroy all partitions less than _min_chkpt_rec_lsn
     *  Open the rest and close them.
     *  There might not be an existing last_partition,
     *  regardless of the value of "reformat"
     */
    {
        partition_number_t n;
        partition_t        *p;

        DBGTHRD(<<" min_chkpt_rec_lsn " << min_chkpt_rec_lsn() 
                << " last_partition " << last_partition);
        w_assert3(min_chkpt_rec_lsn().hi() <= last_partition);

        for (n = min_index; n < min_chkpt_rec_lsn().hi(); n++)  {
            // not an error if we can't unlink (probably doesn't exist)
            DBGTHRD(<<" destroy_file " << n << "false"); 
            unix_log::destroy_file(n, false);
        }
        for (n = _min_chkpt_rec_lsn.hi(); n <= last_partition; n++)  {
            // Find out if there's a hint about the length of the 
            // partition (from the checkpoint).  This lsn serves as a
            // starting point from which to search for the skip_log record
            // in the file.  It's a performance thing...
            lsn_t lasthint;
            for(int q=0; q<listlength; q++) {
                if(lsnlist[q].hi() == n) {
                    lasthint = lsnlist[q];
                }
            }

            // open and check each file (get its size)
            DBGTHRD(<<" open " << n << "true, false, true"); 
            p = open(n, lasthint, true, false, true);
            w_assert3(p == n_partition(n));
            p->close();
            unset_current();
            DBGTHRD(<<" done w/ open " << n );
        }
    }

    /* XXXX :  Don't have a static method on 
     * partition_t for start() 
    */
    /* end of the last valid log record / start of invalid record */
    fileoff_t pos = 0;

    { // Truncate at last complete log rec
    DBGTHRD(<<" truncate last complete log rec "); 

    /*
     *
        The goal of this code is to determine where is the last complete
        log record in the log file and truncate the file at the
        end of that record.  It detects this by scanning the file and
        either reaching eof or else detecting an incomplete record.
        If it finds an incomplete record then the end of the preceding
        record is where it will truncate the file.

        The file is scanned by attempting to fread the length of a log
        record header.        If this fread does not read enough bytes, then
        we've reached an incomplete log record.  If it does read enough,
        then the buffer should contain a valid log record header and
        it is checked to determine the complete length of the record.
        Fseek is then called to advance to the end of the record.
        If the fseek fails then it indicates an incomplete record.

     *  NB:
        This is done here rather than in peek() since in the unix-file
        case, we only check the *last* partition opened, not each
        one read.
     *
     */
    _make_log_name(last_partition, fname, smlevel_0::max_devname);
    DBGTHRD(<<" checking " << fname);

    FILE *f =  fopen(fname, "r");
    DBGTHRD(<<" opened " << fname << " fp " << f);

    fileoff_t start_pos = pos;

#ifndef SM_LOG_UNIX_NO_SKIP_SEEK
    /* If the master checkpoint is in the current partition, seek
       to its position immediately, instead of scanning from the 
       beginning of the log.   If the current partition doesn't have
       a checkpoint, must read entire paritition until the skip
       record is found. */

    const lsn_t &seek_lsn = _master_lsn;

    if (f && seek_lsn.hi() == last_partition) {
            start_pos = seek_lsn.lo();

            if (fseek(f, start_pos, SEEK_SET)) {
                smlevel_0::errlog->clog << "log read: can't seek to " << start_pos
                     << " starting log scan at origin"
                     << endl;
                start_pos = pos;
            }
            else
                pos = start_pos;
    }
#endif

    if (f)  {
        allocaN<logrec_t::hdr_sz> buf;

        DBGTHRD(<<"fread " << fname << " sz= " << logrec_t::hdr_sz);
        int n;
        while ((n = fread(buf, 1, logrec_t::hdr_sz, f)) 
                == logrec_t::hdr_sz)  
        {
          logrec_t  *l = (logrec_t*) (void*) buf;

            if( l->type() == logrec_t::t_skip) {
                break;
            }

            uint2_t len = (uint2_t) l->length();
            DBGTHRD(<<"scanned log rec type=" << int(l->type())
                    << " length=" << l->length());

            if(len < logrec_t::hdr_sz) {
                // Must be garbage and we'll have to truncate this
                // partition to size 0
                w_assert1(pos == start_pos);
            } else {
                w_assert1(len >= logrec_t::hdr_sz);

                // seek to lsn_ck at end of record
                // Subtract out sizeof(buf) because we already
                // read that (thus we have seeked past it)
                // Subtract out lsn_t to find beginning of lsn_ck.
                len -= (logrec_t::hdr_sz + sizeof(lsn_t));

                //NB: this is a RELATIVE seek
                DBGTHRD(<<"seek additional +" << len << " for lsn_ck");
                if (fseek(f, len, 1))  {
                    if (feof(f))  break;
                }

                lsn_t lsn_ck;
                n = fread(&lsn_ck, 1, sizeof(lsn_ck), f);
                DBGTHRD(<<"read lsn_ck return #bytes=" << n );
                if (n != sizeof(lsn_ck))  {
                    w_rc_t        e = RC(eOS);    
                    // reached eof
                    if (! feof(f))  {
                        smlevel_0::errlog->clog << error_prio 
                        << "ERROR: unexpected log file inconsistency." << flushl;
                        W_COERCE(e);
                    }
                    break;
                }
                DBGTHRD(<<"pos = " <<  pos
                    << " lsn_ck = " <<lsn_ck);

                // make sure log record's lsn matched its position in file
                if ( (lsn_ck.lo() != pos) ||
                    (lsn_ck.hi() != (uint4_t) last_partition ) ) {
                    // found partial log record, end of log is previous record
                    smlevel_0::errlog->clog << error_prio <<
                    "Found unexpected end of log -- probably due to a previous crash." 
                    << flushl;
                    smlevel_0::errlog->clog << error_prio <<
                    "   Recovery will continue ..." << flushl;
                    break;
                }

                // remember current position 
                pos = ftell(f) ;
            }
        }
        fclose(f);

        /* XXXX this is a race condition where someone could swap the
           files around and have your truncating something different
           than intended -- either way, so maybe keep it open longer,
           and close it after truncation?
           XXX But, can'd do that since it is only open for
           reading.  Sigh
        */
        {
            DBGTHRD(<<"explicit truncating " << fname << " to " << pos);
            int ret = os_truncate(fname, pos );
            if (ret!=0) {
                fprintf(stdout, "Error truncating file\n");
                fprintf(stdout, "Error truncating file (%s): %s\n", fname, strerror(errno));
                w_rc_t e = RC(fcOS);
                smlevel_0::errlog->clog << "truncate(" << fname << "):" << endl << e << endl;
                W_COERCE(e);
            }

            //
            // but we can't just use truncate() --
            // we have to truncate to a size that's a mpl
            // of the page size. First append a skip record
            DBGTHRD(<<"explicit opening  " << fname );
            f =  fopen(fname, "a");
            if (!f) {
                w_rc_t e = RC(fcOS);
                smlevel_0::errlog->clog << "fopen(" << fname << "):" << endl << e << endl;
                W_COERCE(e);
            }
            skip_log *s = new skip_log; // deleted below
            s->set_lsn_ck( lsn_t(uint4_t(last_partition), sm_diskaddr_t(pos)) );

            DBGTHRD(<<"writing skip_log at pos " << pos << " with lsn "
                << s->get_lsn_ck() 
                << "and size " << s->length()
                );
#ifdef W_TRACE
            {
                fileoff_t eof2 = ftell(f);
                DBGTHRD(<<"eof is now " << eof2);
            }
#endif

            if ( fwrite(s, s->length(), 1, f) != 1)  {
                w_rc_t        e = RC(eOS);    
                smlevel_0::errlog->clog << error_prio <<
                    "   fwrite: can't write skip rec to log ..." << flushl;
                W_COERCE(e);
            }
#ifdef W_TRACE
            {
                fileoff_t eof2 = ftell(f);
                DBGTHRD(<<"eof is now " << eof2);
            }
#endif
            fileoff_t o = pos;
            o += s->length();
            o = o % BLOCK_SIZE;
            DBGTHRD(<<"BLOCK_SIZE " << int(BLOCK_SIZE));
            if(o > 0) {
                o = BLOCK_SIZE - o;
                char *junk = new char[int(o)]; // delete[] at close scope
                if (!junk)
                        W_FATAL(fcOUTOFMEMORY);
#ifdef ZERO_INIT
                memset(junk,'\0', o);
#endif
                
                DBGTHRD(<<"writing junk of length " << o);
#ifdef W_TRACE
                {
                    fileoff_t eof2 = ftell(f);
                    DBGTHRD(<<"eof is now " << eof2);
                }
#endif
                int        n = fwrite(junk, int(o), 1, f);
                if ( n != 1)  {
                    w_rc_t e = RC(eOS);        
                    smlevel_0::errlog->clog << error_prio <<
                    "   fwrite: can't round out log block size ..." << flushl;
                    W_COERCE(e);
                }
#ifdef W_TRACE
                {
                    fileoff_t eof2 = ftell(f);
                    DBGTHRD(<<"eof is now " << eof2);
                }
#endif
                delete[] junk;
                o = 0;
            }
            delete s; // skip_log

            eof = ftell(f);
            w_rc_t e = RC(eOS);        /* collect the error in case it is needed */
            DBGTHRD(<<"eof is now " << eof);


            if(((eof) % BLOCK_SIZE) != 0) {
                smlevel_0::errlog->clog << error_prio <<
                    "   ftell: can't write skip rec to log ..." << flushl;
                W_COERCE(e);
            }
            W_IGNORE(e);        /* error not used */
            
            if (os_fsync(fileno(f)) < 0) {
                w_rc_t e = RC(eOS);    
                smlevel_0::errlog->clog << error_prio <<
                    "   fsync: can't sync fsync truncated log ..." << flushl;
                W_COERCE(e);
            }

#if W_DEBUG_LEVEL > 2
            {
                w_rc_t e;
                os_stat_t statbuf;
                e = MAKERC(os_fstat(fileno(f), &statbuf) == -1, eOS);
                if (e.is_error()) {
                    smlevel_0::errlog->clog << error_prio 
                            << " Cannot stat fd " << fileno(f)
                            << ":" << endl << e << endl << flushl;
                    W_COERCE(e);
                }
                DBGTHRD(<< "size of " << fname << " is " << statbuf.st_size);
            }
#endif 
            fclose(f);
        }

    } else {
        w_assert3(!last_partition_exists);
    }
    } // End truncate at last complete log rec

    /*
     *  initialize current and durable lsn for
     *  the purpose of sanity checks in open*()
     *  and elsewhere
     */
    DBGTHRD( << "partition num = " << partition_num()
        <<" current_lsn " << curr_lsn()
        <<" durable_lsn " << durable_lsn());

    lsn_t new_lsn(last_partition, pos);
    _curr_lsn = _durable_lsn = _flush_lsn = new_lsn;

    DBGTHRD( << "partition num = " << partition_num()
            <<" current_lsn " << curr_lsn()
            <<" durable_lsn " << durable_lsn());

    {
        /*
         *  create/open the "current" partition
         *  "current" could be new or existing
         *  Check its size and all the records in it
         *  by passing "true" for the last argument to open()
         */

        // Find out if there's a hint about the length of the 
        // partition (from the checkpoint).  This lsn serves as a
        // starting point from which to search for the skip_log record
        // in the file.  It's a performance thing...
        lsn_t lasthint;
        for(int q=0; q<listlength; q++) {
            if(lsnlist[q].hi() == last_partition) {
                lasthint = lsnlist[q];
            }
        }
        partition_t *p = open(last_partition, lasthint,
                last_partition_exists, true, true);

        /* XXX error info lost */
        if(!p) {
            smlevel_0::errlog->clog << error_prio 
            << "ERROR: could not open log file for partition "
            << last_partition << flushl;
            W_FATAL(eINTERNAL);
        }

        w_assert3(p->num() == last_partition);
        w_assert3(partition_num() == last_partition);
        w_assert3(partition_index() == p->index());

    }
    DBGTHRD( << "partition num = " << partition_num()
            <<" current_lsn " << curr_lsn()
            <<" durable_lsn " << durable_lsn());

    cs.exit();
}



/*********************************************************************
 * 
 *  unix_log::~unix_log()
 *
 *  Destructor. Close all open partitions.
 *
 *********************************************************************/
void 
unix_partition::close_for_append()
{
    int f = fhdl_app();
    if (f != invalid_fhdl)  {
        w_rc_t e;
        DBGTHRD(<< " CLOSE " << f);
        e = me()->close(f);
        if (e.is_error()) {
            smlevel_0::errlog->clog << "warning: error in unix log on close(app):" 
                    << endl <<  e << endl;
        }
        _fhdl_app = invalid_fhdl;
    }
}

void 
unix_partition::close_for_read()
{
    int f = fhdl_rd();
    if (f != invalid_fhdl)  {
        w_rc_t e;
        DBGTHRD(<< " CLOSE " << f);
        e = me()->close(f);
        if (e.is_error()) {
            smlevel_0::errlog->clog << "warning: error in unix partition on close(rd):" 
                << endl <<  e << endl;
        }
        _fhdl_rd = invalid_fhdl;
    }
}

unix_log::~unix_log()
{
    partition_t        *p;
    for (uint i = 0; i < max_openlog; i++) {
        p = i_partition(i);
        p->close_for_read();
        p->close_for_append();
        DBG(<< " calling clear");
        p->_clear();
    }
}


partition_t *
unix_log::i_partition(partition_index_t i) const
{
    return i<0 ? (partition_t *)0: (partition_t *) &_part[i];
}

void
unix_log::_write_master( lsn_t l, lsn_t min) 
{
    /*
     *  create new master record
     */
    char _chkpt_meta_buf[CHKPT_META_BUF];
    _make_master_name(l, min, _chkpt_meta_buf, CHKPT_META_BUF);
    DBGTHRD(<< "writing checkpoint master: " << _chkpt_meta_buf);

    FILE* f = fopen(_chkpt_meta_buf, "a");
    if (! f) {
        w_rc_t e = RC(eOS);    
        smlevel_0::errlog->clog << error_prio 
            << "ERROR: could not open a new log checkpoint file: "
            << _chkpt_meta_buf << flushl;
        W_COERCE(e);
    }

    {        /* write ending lsns into the master chkpt record */
        lsn_t         array[PARTITION_COUNT];
        int j=0;
        for(int i=0; i < PARTITION_COUNT; i++) {
            const partition_t *p = this->i_partition(i);
            if(p->num() > 0 && (p->last_skip_lsn().hi() == p->num())) {
                array[j++] = p->last_skip_lsn();
            }
        }
        if(j > 0) {
            w_ostrstream s(_chkpt_meta_buf, CHKPT_META_BUF);
            create_master_chkpt_contents(s, j, array);
        } else {
            memset(_chkpt_meta_buf, '\0', 1);
        }
        int length = strlen(_chkpt_meta_buf) + 1;
        DBG(<< " #lsns=" << j
            << " write this to master checkpoint record: " <<
                _chkpt_meta_buf);

        if(fwrite(_chkpt_meta_buf, length, 1, f) != 1) {
            w_rc_t e = RC(eOS);    
            smlevel_0::errlog->clog << error_prio 
                << "ERROR: could not write log checkpoint file contents"
                << _chkpt_meta_buf << flushl;
            W_COERCE(e);
        }
    }
    fclose(f);

    /*
     *  destroy old master record
     */
    _make_master_name(_master_lsn, 
                _min_chkpt_rec_lsn, _chkpt_meta_buf, CHKPT_META_BUF);
    (void) unlink(_chkpt_meta_buf);
}

/*********************************************************************
 * unix_partition class
 *********************************************************************/

unix_partition::unix_partition()
: _fhdl_rd(invalid_fhdl),
  _fhdl_app(invalid_fhdl)
{
}

void                        
unix_partition::set_fhdl_app(int fd)
{
   w_assert3(fhdl_app() == invalid_fhdl);
   DBGTHRD(<<"SET APP " << fd);
   _fhdl_app = fd;
}

void
unix_partition::peek(
    partition_number_t  __num, 
    const lsn_t&        end_hint,
    bool                 recovery,
    int *                fdp
)
{
    FUNC(unix_partition::peek);
    // this is a static func so we cannot assert this:
    // w_assert2(_owner->_partition_lock.is_mine()==true);
    int fd;

    // Either we have nothing opened or we are peeking at something
    // already opened.
    w_assert2(num() == 0 || num() == __num);
    w_assert3(__num != 0);

    if( num() ) {
        close_for_read();
        close_for_append();
        DBG(<< " calling clear");
        clear();
    }

    clr_state(m_exists);
    clr_state(m_flushed);

    char *fname = new char[smlevel_0::max_devname];
    if (!fname)
        W_FATAL(fcOUTOFMEMORY);
    w_auto_delete_array_t<char> ad_fname(fname);        
    unix_log::_make_log_name(__num, fname, smlevel_0::max_devname);

    smlevel_0::fileoff_t part_size = fileoff_t(0);

    DBGTHRD(<<"partition " << __num << " peek opening " << fname);

    // first create it if necessary.
    int flags = smthread_t::OPEN_RDWR | smthread_t::OPEN_SYNC
            | smthread_t::OPEN_CREATE;
    w_rc_t e;
    e = me()->open(fname, flags, 0744, fd);
    if (e.is_error()) {
        smlevel_0::errlog->clog << error_prio
            << "ERROR: cannot open log file: " << endl << e << flushl;
        W_COERCE(e);
    }
    DBGTHRD(<<"partition " << __num << " peek  opened " << fname);
    {
         w_rc_t e;
         sthread_base_t::filestat_t statbuf;
         e = me()->fstat(fd, statbuf);
         if (e.is_error()) {
                smlevel_0::errlog->clog << error_prio 
                << " Cannot stat fd " << fd << ":" 
                << endl << e  << flushl;
                W_COERCE(e);
         }
         part_size = statbuf.st_size;
         DBGTHRD(<< "partition " << __num << " peek "
             << "size of " << fname << " is " << statbuf.st_size);
    }

    // We will eventually want to write a record with the durable
    // lsn.  But if this is start-up and we've initialized
    // with a partial partition, we have to prime the
    // buf with the last block in the partition.
    //
    // If this was a pre-existing partition, we have to scan it
    // to find the *real* end of the file.

    if( part_size > 0 ) {
        w_assert3(__num == end_hint.hi() || end_hint.hi() == 0);
        _peek(__num, end_hint.lo(), part_size, recovery, fd);
    } else {
        // write a skip record so that prime() can
        // cope with it.
        // Have to do this carefully -- since using
        // the standard insert()/write code causes a
        // prime() to occur and that doesn't solve anything.

        DBGTHRD(<<" peek DESTROYING PARTITION " << __num << "  on fd " << fd);

        // First: write any-old junk
        w_rc_t e = me()->ftruncate(fd,  unix_log::BLOCK_SIZE );
        if (e.is_error())        {
             smlevel_0::errlog->clog << error_prio
                << "cannot write garbage block " << flushl;
            W_COERCE(e);
        }
        /* write the lsn of the up-coming skip record */

        // Now write the skip record and flush it to the disk:
        skip(log_base::first_lsn(__num), fd);

        // First: write any-old junk
        e = me()->fsync(fd);
        if (e.is_error()) {
             smlevel_0::errlog->clog << error_prio
                << "cannot sync after skip block " << flushl;
            W_COERCE(e);
        }

        // Size is 0
        set_size(0);
    }

    if (fdp) {
        DBGTHRD(<< "partition " << __num << " SAVED, NOT CLOSED fd " << fd);
        *fdp = fd;
    } else {
        DBGTHRD(<< " CLOSE " << fd);
        w_rc_t e = me()->close(fd);
        if (e.is_error()) {
            smlevel_0::errlog->clog << error_prio 
            << "ERROR: could not close the log file." << flushl;
            W_COERCE(e);
        }
        
    }
}

void                        
unix_partition::_flush(int fd)
{
    // We only cound the fsyncs called as
    // a result of _flush(), not from peek
    // or start-up
    INC_TSTAT(log_fsync_cnt);

    w_rc_t e = me()->fsync(fd);
    if (e.is_error()) {
         smlevel_0::errlog->clog << error_prio
            << "cannot sync after skip block " << flushl;
        W_COERCE(e);
    }
}

void
unix_partition::open_for_read(
    partition_number_t  __num,
    bool err // = true.  if true, it's an error for the partition not to exist
)
{
    FUNC(unix_partition::open_for_read);
    // protected w_assert2(_owner->_partition_lock.is_mine()==true);
    // asserted before call in srv_log.cpp

    DBGTHRD(<<"start open for part " << __num << " err=" << err);

    w_assert1(__num != 0);

    // do the equiv of opening existing file
    // if not already in the list and opened
    //
    if(fhdl_rd() == invalid_fhdl) {
        char *fname = new char[smlevel_0::max_devname];
        if (!fname)
                W_FATAL(fcOUTOFMEMORY);
        w_auto_delete_array_t<char> ad_fname(fname);

        unix_log::_make_log_name(__num, fname, smlevel_0::max_devname);

        int fd;
        w_rc_t e;
        DBGTHRD(<< "partition " << __num
                << "open_for_read OPEN " << fname);
        int flags = smthread_t::OPEN_RDONLY;

        e = me()->open(fname, flags, 0, fd);

        DBGTHRD(<< " OPEN " << fname << " returned " << fd);

        if (e.is_error()) {
            if(err) {
                smlevel_0::errlog->clog << error_prio << flushl;
                smlevel_0::errlog->clog << error_prio
                << "ERROR: cannot open log file: " << fd << flushl;
                W_COERCE(e);
            } else {
                w_assert3(! exists());
                w_assert3(_fhdl_rd == invalid_fhdl);
                // _fhdl_rd = invalid_fhdl;
                clr_state(m_open_for_read);
                DBGTHRD(<<"fhdl_app() is " << _fhdl_app);
                return;
            }
        }

        w_assert3(_fhdl_rd == invalid_fhdl);
        _fhdl_rd = fd;

        DBGTHRD(<<"size is " << size());
        // size might not be known, might be anything
        // if this is an old partition

        set_state(m_exists);
        set_state(m_open_for_read);
    }
    _num = __num;
    w_assert3(exists());
    w_assert3(is_open_for_read());
    // might not be flushed, but if
    // it isn't, surely it's flushed up to
    // the offset we're reading
    //w_assert3(flushed());

    w_assert3(_fhdl_rd != invalid_fhdl);
    DBGTHRD(<<"_fhdl_rd = " <<_fhdl_rd );
}

/*
 * close for append, or if both==true, close
 * the read-file also
 */
void
unix_partition::close(bool both) 
{
    bool err_encountered=false;
    w_rc_t e;

    // protected member: w_assert2(_owner->_partition_lock.is_mine()==true);
    // assert is done by callers
    if(is_current()) {
        // This assertion is bad -- the log flusher is probably trying 
        // to update dlsn right now!
        //        w_assert1(dlsn.hi() > num());
        //        _owner->_flush(_owner->curr_lsn());
        //w_assert3(flushed());
        _owner->unset_current();
    }
    if (both) {
        if (fhdl_rd() != invalid_fhdl) {
            DBGTHRD(<< " CLOSE " << fhdl_rd());
            e = me()->close(fhdl_rd());
            if (e.is_error()) {
                smlevel_0::errlog->clog << error_prio 
                        << "ERROR: could not close the log file."
                        << e << endl << flushl;
                err_encountered = true;
            }
        }
        _fhdl_rd = invalid_fhdl;
        clr_state(m_open_for_read);
    }

    if (is_open_for_append()) {
        DBGTHRD(<< " CLOSE " << fhdl_rd());
        e = me()->close(fhdl_app());
        if (e.is_error()) {
            smlevel_0::errlog->clog << error_prio 
            << "ERROR: could not close the log file."
            << endl << e << endl << flushl;
            err_encountered = true;
        }
        _fhdl_app = invalid_fhdl;
        clr_state(m_open_for_append);
        DBGTHRD(<<"fhdl_app() is " << _fhdl_app);
    }

    clr_state(m_flushed);
    if (err_encountered) {
        W_COERCE(e);
    }
}


void 
unix_partition::sanity_check() const
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
    }
}



/**********************************************************************
 *
 *  unix_partition::destroy()
 *
 *  Destroy a log file.
 *
 *********************************************************************/
void
unix_partition::destroy()
{
    w_assert3(num() < _owner->global_min_lsn().hi());

    if(num()>0) {
        w_assert3(exists());
        w_assert3(! is_current() );
        w_assert3(! is_open_for_read() );
        w_assert3(! is_open_for_append() );

        unix_log::destroy_file(num(), true);
        clr_state(m_exists);
        // _num = 0;
        DBG(<< " calling clear");
        clear();
    }
    w_assert3( !exists());
    sanity_check();
}

void
unix_log::destroy_file(partition_number_t n, bool pmsg)
{
    char        *fname = new char[smlevel_0::max_devname];
    if (!fname)
        W_FATAL(fcOUTOFMEMORY);
    w_auto_delete_array_t<char> ad_fname(fname);
    unix_log::_make_log_name(n, fname, smlevel_0::max_devname);
    if (unlink(fname) == -1)  {
        w_rc_t e = RC(eOS);
        smlevel_0::errlog->clog << "destroy_file " << n << " " << fname << ":" <<endl
             << e << endl;
        if(pmsg) {
            smlevel_0::errlog->clog << error_prio 
            << "warning : cannot free log file \"" 
            << fname << '\"' << flushl;
            smlevel_0::errlog->clog << error_prio 
            << "          " << e << flushl;
        }
    }
}

void
unix_partition::_clear()
{
    clr_state(m_open_for_read);
    clr_state(m_open_for_append);
    DBGTHRD(<<"partition " << num() << " clear is clobbering " 
            << _fhdl_rd << " and " << _fhdl_app);
    _fhdl_rd = invalid_fhdl;
    _fhdl_app = invalid_fhdl;
}

void
unix_partition::_init(srv_log *owner)
{
    _start = 0; // always
    _owner = owner;
    _eop = owner->limit(); // always
    DBG(<<" calling clear for partition " << _fhdl_rd);
    clear();
}


int
unix_partition::fhdl_rd() const
{
#if W_DEBUG_LEVEL > 2
    bool isopen = is_open_for_read();
    if(_fhdl_rd == invalid_fhdl) {
        w_assert3( !isopen );
    } else {
        w_assert3(isopen);
    }
#endif
    return _fhdl_rd;
}

int
unix_partition::fhdl_app() const
{
#if W_DEBUG_LEVEL > 2
    if(_fhdl_app != invalid_fhdl) {
        w_assert3(is_open_for_append());
    } else {
        w_assert3(! is_open_for_append());
    }
#endif
    return _fhdl_app;
}

