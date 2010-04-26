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

/*<std-header orig-src='shore' incl-file-exclusion='SM_BASE_H'>

 $Id: sm_base.h,v 1.146.2.16 2010/03/25 18:05:16 nhall Exp $

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

#ifndef SM_BASE_H
#define SM_BASE_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/**\file sm_base.h
 * \ingroup Macros
 */

#ifdef __GNUG__
#pragma interface
#endif

#include <climits>
#ifndef OPTION_H
#include "option.h"
#endif
#ifndef __opt_error_def_gen_h__
#include "opt_error_def_gen.h"
#endif


class ErrLog;
class sm_stats_info_t;
class xct_t;
class xct_i;

class device_m;
class io_m;
class bf_m;
class comm_m;
class log_m;
class lock_m;

class tid_t;
class option_t;

#ifndef        SM_EXTENTSIZE
#define        SM_EXTENTSIZE        8
#endif
#ifndef        SM_LOG_PARTITIONS
#define        SM_LOG_PARTITIONS        8
#endif

typedef   w_rc_t        rc_t;

/**\cond skip */
class smlevel_0 : public w_base_t {
public:
    enum { eNOERROR = 0, eFAILURE = -1 };
    enum { 
        page_sz = SM_PAGESIZE,        // page size (SM_PAGESIZE is set by makemake)
        ext_sz = SM_EXTENTSIZE,        // extent size
        max_exts = max_int4,        // max no. extents, must fit extnum_t
#if defined(_POSIX_PATH_MAX)
        max_devname = _POSIX_PATH_MAX,        // max length of unix path name
    // BEWARE: this might be larger than you want.  Array sizes depend on it.
    // The default might be small enough, e.g., 256; getconf() yields the upper
    // bound on this value.
#elif defined(MAXPATHLEN)
        max_devname = MAXPATHLEN,
#else
        max_devname = 1024,        
#endif
        max_vols = 20,                // max mounted volumes
        max_xct_thread = 20,        // max threads in a xct
        max_servers = 15,       // max servers to be connected with
        max_keycomp = 20,        // max key component (for btree)
        max_openlog = SM_LOG_PARTITIONS,        // max # log partitions
        max_dir_cache = max_vols * 10,

        /* XXX I want to propogate sthread_t::iovec_max here, but
           it doesn't work because of sm_app.h not including
           the thread package. */
        max_many_pages = 8,

        srvid_map_sz = (max_servers - 1) / 8 + 1,
        ext_map_sz_in_bytes = ((ext_sz + 7) / 8),

        dummy = 0
    };

    enum {
        max_rec_len = max_uint4
    };

    typedef sthread_base_t::fileoff_t fileoff_t;
    /*
     * Sizes-in-Kbytes for for things like volumes and devices.
     * A KB is assumes to be 1024 bytes.
     * Note: a different type was used for added type checking.
     */
    typedef sthread_t::fileoff_t smksize_t;


    typedef vote_t        xct_vote_t;

    typedef w_base_t::base_stat_t base_stat_t; 

    /*
     * rather than automatically aborting the transaction, when the
     * _log_warn_percent is exceeded, this callback is made, with a
     * pointer to the xct that did the writing, and with the
     * expectation that the result will be one of:
     * return value == RCOK --> proceed
     * return value == eUSERABORT --> victim to abort is given in the argument
     *
     * This method is in its own file, so the VAS can replace it.
     * It is a ss_m method so that the VAS can write a function that
     * is privvy to the SM private stuff.
     */
    /*
     * callback function that's called by SM on entry to
     * __almost any__ SM call that occurs on behalf of a xact.  The
     * callback is made IFF the amount of log space used exceeds the
     * threshhold determined by the option sm_log_warn; this callback
     * function chooses a victim xct and tells if the xct should be
     * aborted by returning RC(eUSERABORT).  Any other RC is returned
     * to the caller of the SM.    The arguments:
     *  xct_i*  : ptr to iterator over all xcts 
     *  xct_t *&: ref to ptr to xct : ptr to victim is returned here.
     *  base_stat_t curr: current log used by active xcts
     *  base_stat_t thresh: threshhold that was just exceeded
     *  Function must be careful not to return the same victim more
     *  than once, even though the callback may be called many 
     *  times before the victim is completely aborted.
     */
    typedef w_rc_t (*LOG_WARN_CALLBACK_FUNC) (xct_i*, xct_t *&,
        fileoff_t curr,
        fileoff_t thresh
        );

    enum switch_t {
        ON = 1,
        OFF = 0
    };

    // shorthand for basics.h CompareOp
    enum cmp_t { bad_cmp_t=badOp, eq=eqOp,
                 gt=gtOp, ge=geOp, lt=ltOp, le=leOp };

    enum store_t { t_bad_store_t, t_index, t_file,
                   t_lgrec, // t_lgrec is used for storing large record
                            // pages and is always associated with some
                            // t_file store
                   t_1page // for special store holding 1-page btrees 
                  };
    
/**\endcond skip */
    // types of indexes

    /**\brief Index types */
    enum ndx_t { 
        t_bad_ndx_t,             // illegal value
        t_btree,                 // B+tree with duplicates
        t_uni_btree,             // Unique-key btree
        t_rtree                  // R*tree
    };

    /**\enum concurrency_t 
     * \brief 
     * Lock granularities 
     * \details
     * - t_cc_bad Illegal
     * - t_cc_none No locking
     * - t_cc_record Record-level locking for files & records
     * - t_cc_page Page-level locking for files & records 
     * - t_cc_file File-level locking for files & records 
     * - t_cc_vol Volume-level locking for files and indexes 
     * - t_cc_kvl Key-value locking for B+-Tree indexes
     * - t_cc_im Aries IM locking for B+-Tree indexes : experimental
     * - t_cc_modkvl Modified key-value locking: experimental
     * - t_cc_append Used internally \todo true?
     */
    enum concurrency_t {
        t_cc_bad,                // this is an illegal value
        t_cc_none,                // no locking
        t_cc_record,                // record-level
        t_cc_page,                // page-level
        t_cc_file,                // file-level
        t_cc_vol,
        t_cc_kvl,                // key-value
        t_cc_im,                 // ARIES IM, not supported yet
        t_cc_modkvl,                 // modified ARIES KVL, for paradise use
        t_cc_append                 // append-only with scan_file_i
    };

/**\cond skip */

    /* 
     * smlevel_0::operating_mode is always set to 
     * ONE of these, but the function in_recovery() tests for
     * any of them, so we'll give them bit-mask values
     */
    enum operating_mode_t {
        t_not_started = 0, 
        t_in_analysis = 0x1,
        t_in_redo = 0x2,
        t_in_undo = 0x4,
        t_forward_processing = 0x8
    };

    static concurrency_t cc_alg;        // concurrency control algorithm
    static bool          cc_adaptive;        // is PS-AA (adaptive) algorithm used?

#include "e_error_enum_gen.h"

    static const w_error_info_t error_info[];
    static void init_errorcodes();

    static void  add_to_global_stats(const sm_stats_info_t &from);
    static void  add_from_global_stats(sm_stats_info_t &to);

    static device_m* dev;
    static io_m* io;
    static bf_m* bf;
    static lock_m* lm;

    static log_m* log;
    static tid_t* redo_tid;

    static LOG_WARN_CALLBACK_FUNC log_warn_callback;
    static fileoff_t              log_warn_trigger; 
    static int                    log_warn_exceed_percent; 

    static int    dcommit_timeout; // to convey option to coordinator,
                                   // if it is created by VAS

    static ErrLog* errlog;

    static bool        shutdown_clean;
    static bool        shutting_down;
    static bool        logging_enabled;
    static bool        do_prefetch;

    static operating_mode_t operating_mode;
    static bool in_recovery() { 
        return ((operating_mode & 
                (t_in_redo | t_in_undo | t_in_analysis)) !=0); }
    static bool in_recovery_analysis() { 
        return ((operating_mode & t_in_analysis) !=0); }
    static bool in_recovery_undo() { 
        return ((operating_mode & t_in_undo ) !=0); }
    static bool in_recovery_redo() { 
        return ((operating_mode & t_in_redo ) !=0); }

    // these variable are the default values for lock escalation counts
    static w_base_t::int4_t defaultLockEscalateToPageThreshold;
    static w_base_t::int4_t defaultLockEscalateToStoreThreshold;
    static w_base_t::int4_t defaultLockEscalateToVolumeThreshold;

    // These variables control the size of the log.
    static fileoff_t max_logsz; // max log file size

    // This variable controls checkpoint frequency.
    // Checkpoints are taken every chkpt_displacement bytes
    // written to the log.
    static fileoff_t chkpt_displacement;

    // The volume_format_version is used to test compatability
    // of software with a volume.  Whenever a change is made
    // to the SM software that makes it incompatible with
    // previouly formatted volumes, this volume number should
    // be incremented.  The value is set in sm.cpp.
    static w_base_t::uint4_t volume_format_version;

    // This is a zeroed page for use wherever initialized memory
    // is needed.
    static char zero_page[page_sz];

    // option for controlling background buffer flush thread
    static option_t* _backgroundflush;

    // Certain operations have to exclude xcts
    static queue_based_lock_t    _begin_xct_mutex;

    /*
     * Pre-defined store IDs -- see also vol.h
     * 0 -- is reserved for the extent map and the store map
     * 1 -- directory (see dir.cpp)
     * 2 -- root index (see sm.cpp)
     */
    enum {
        store_id_extentmap = 0,
        store_id_directory = 1,
        store_id_root_index = 2 
        // The store numbers for the lid indexes (if
        // the volume has logical ids) are kept in the
        // volume's root index, constants for them aren't needed.
        //
    };

    enum {
            eINTERNAL = fcINTERNAL,
            eOS = fcOS,
            eOUTOFMEMORY = fcOUTOFMEMORY,
            eNOTFOUND = fcNOTFOUND,
            eNOTIMPLEMENTED = fcNOTIMPLEMENTED
    };

    enum store_flag_t {
        // NB: this had better match sm_store_property_t (sm_int_3.h) !!!
        // or at least be convted properly every time we come through the API
        st_bad            = 0x0,
        st_regular        = 0x01, // fully logged
        st_tmp            = 0x02, // space logging only, 
                                  // file destroy on dismount/restart
        st_load_file      = 0x04, // not stored in the stnode_t, 
                            // only passed down to
                            // io_m and then converted to tmp and added to the
                            // list of load files for the xct.
                            // no longer needed
        st_insert_file     = 0x08,        // stored in stnode, but not on page.
                            // new pages are saved as tmp, old pages as regular.
        st_empty           = 0x100 // store might be empty - used ONLY
                            // as a function argument, NOT stored
                            // persistently.  Nevertheless, it's
                            // defined here to be sure that if other
                            // store flags are added, this doesn't
                            // conflict with them.
    };

    /* 
     * for use by set_store_deleting_log; 
     * type of operation to perform on the stnode 
     */
    enum store_operation_t {
            t_delete_store, 
            t_create_store, 
            t_set_deleting, 
            t_set_store_flags, 
            t_set_first_ext};

    enum store_deleting_t  {
            t_not_deleting_store, 
            t_deleting_store, 
            t_store_freeing_exts, 
            t_unknown_deleting};
};

ostream&
operator<<(ostream& o, smlevel_0::store_flag_t flag);

ostream&
operator<<(ostream& o, const smlevel_0::store_operation_t op);

ostream&
operator<<(ostream& o, const smlevel_0::store_deleting_t value);

/**\endcond  skip */

/*<std-footer incl-file-exclusion='SM_BASE_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
