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

/*<std-header orig-src='shore' incl-file-exclusion='SM_H'>

 $Id: sm.h,v 1.303.2.15 2010/03/25 18:05:15 nhall Exp $

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

#ifndef SM_H
#define SM_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*
 *  Stuff needed by value-added servers.  NOT meant to be included by
 *  internal SM .c files, except to the extent that they need these
 *  definitions used in the API.
 */

#ifdef __GNUG__
#pragma interface
#endif

#ifndef SM_INT_4_H
#include <sm_int_4.h>
#endif

#ifndef SM_DU_STATS_H
#include <sm_du_stats.h> // declares sm_du_stats_t
#endif

#ifndef SM_STATS_H
#include <smstats.h> // declares sm_stats_info_t and sm_config_info_t
#endif

#ifndef SM_S_H
#include <sm_s.h> // declares key_type_s, rid_t, lsn_t
#endif

#ifndef LEXIFY_H
#include <lexify.h> // declares sortorder with constants
#endif

#ifndef NBOX_H
#include <nbox.h>   // key_info_t contains nbox_t
#endif /* NBOX_H */

#ifndef SORT_S_H
#include <sort_s.h> // declares key_info_t
#endif

/* DOXYGEN Documentation : */
/**\defgroup SSMINIT Starting Up, Shutting Down, Thread Context
 * \ingroup SSMAPI
 *
 * Starting the Storage Manager consists in 2 major things:
 * - Initializing the options the storage manager expects to be set.
 *   This can get rather complicated, but fortunately we offer example
 *   code.  See \ref SSMOPT and \ref SSMVAS.
 * - Constructing an instance of the class ss_m.
 *   The constructor performs recovery, and when it returns to the caller,
 *   the caller may begin using the storage manager.  
 *
 * \attention No more than one instance may exist at any time.
 * \attention Storage manager functions must be called in the context of
 * a run() method of an smthread_t. 
 *
 * Shutting down the storage manager consists of deleting the instance
 * of ss_m created above.
 *
 */

/**\defgroup SSMRECOVERY  Recovery and Logging
 * \ingroup SSMAPI
 */

/**\defgroup SSMOPT Storage Manager Options
 * \ingroup SSMAPI
 * \sa  \ref SSMVAS
 */

 /**\defgroup SSMSTG Storage Structures
 * \ingroup SSMAPI
  */
 /**\defgroup SSMVOL Devices and Volumes
 * \ingroup SSMSTG
 * A device is either an operating system file or 
 * an operating system device and 
 * is identified by a path name (absolute or relative).
 * \todo test RAW VOLUMES!!!
 * A device has a quota.  
 * A device is intended to have multiple volumes on it, but
 * in the current implementation the maximum number of volumes
 * is exactly 1.
 *
 * A volume is where data are stored.  
 * A volume is identified uniquely and persistently by a 
 * long volume ID (lvid_t).
 * Volumes can be used whenever the device they are located
 * on is mounted by the SM.  
 * Volumes have a quota.  The
 * sum of the quotas of all the volumes on a device cannot
 * exceed the device quota.
 */

/**\defgroup SSMSTORE Stores
 * \ingroup SSMSTG
 * Indexes and files are special cases of "stores".
 * Stores have properties and other metadata associated with them.
 * 
 * The property that determines the logging level of the store is
 * smlevel_3::sm_store_property_t.
 *
 * Methods that let you get and change the metatdata are:
 * - ss_m::get_store_property
 * - ss_m::set_store_property
 * - ss_m::get_store_info
 * - \ref snum_t
 */


/**\defgroup SSMFILE Files of Records
 * \ingroup SSMSTG
 * You can create, destroy, and scan files of records. You may exert some
 * control over the order in which records appear in the file (a physical
 * scan), but, in general, the storage manager decides where to put records.
 *
 * Pages in a file are slotted pages: Each page contains an array of
 * slots.
 * Records take one of three forms: small, large, and very large.
 * - Small records fit in the slots on the file pages.
 * - Large records are too big to fit on a slotted page, so they are put
 * elsewhere, and the slots point to these records.  Actually, what is
 * in a slot is a small array of page pointers to the data of the large record.
 * - A very large record is one whose slot in the file page contains
 *   a single reference to a page that is an index of data pages.
 *
 * Because records may take these forms, the API for creating records
 * contains the opportunity for you to provide a hint about the ultimate
 * size of the record so that the storage manager can create the proper
 * structure for the record immediately, rather than creating a small
 * record that is soon to be converted to a large, then a very large record
 * by subsequent appends. 
 *
 * All records contain a client-defined header.  This is for the convenience
 * of server-writers.  The header must fit on the slotted page, so it should
 * never be very large.
 *
 * The following methods manipulate files of records and the records found 
 * there.
 *
 * Modules below describe file traversal and
 * appending to files (\ref SSMSCANF), 
 * and pinning individual records in the buffer pool for extended operations 
 * (\ref SSMPIN).
 *
 * \section UNINIT Uninitialized Data
 * The functions create_rec, append_rec, and update_rec can be used to
 * write blocks of data that are all zeroes,  with minimal logging. 
 * This is useful for creating records of known size but with uninitialized data.  
 * The type zvec_t, a special case of vec_t, is for this purpose. 
 * Construct it with only a size, as follows:
 * \code
 * zvec_t zdata(100000);
 * \endcode
 * The underlying logging code recognizes that this is a vector of zeroes and
 * logs only a count, not the data themselves. 
 *
 * \section Errors
 * If an error occurs in the middle of one of these methods that is updating persistent data,
 * the record or file \e could be in an inconsistent state. 
 * The caller has the choice of aborting the transaction or rolling back to the nearest savepoint (see \ref SSMXCT).
 *
 * \sa SSMSCAN, SSMPIN, vec_t, zvec_t, IDs.
 */

/**\defgroup SSMBTREE B+-Tree Indexes
 * \ingroup SSMSTG
 * The storage manager supports B+-Tree indexes provide associative access 
 * to data by associating keys with values in 1:1 or many:1 relationships.
 * Keys may be compsed of any of the basic C-language types (integer,
 * unsigned, floating-point of several sizes) or
 * variable-length character strings (wide characters are \b not supported).
 *
 * The number of key-value pairs that an index can hold is limited by the
 * space available on the volume containing the index.
 * The minimum size of a B-Tree index is 8 pages (1 extent).
 *
 * The key-value locking method used on an index may be from
 *
 * Bulk-loading is supported.
 */


/**\defgroup SSMRTREE R*-Tree Indexes
 * \ingroup SSMSTG
 * An R-tree is a height-balanced structure designed for indexing
 * multi-dimensional spatial objects.  
 * It stores the minimial bounding box (with 2 or higher dimension) of a spatial
 * object as the key in the leaf pages.
 * This implementation is a variant of an R-Tree called an R*-Tree, which
 * improves the search performance by using a heuristic for redistributing
 * entries and dynamically reorganizing the tree during insertion.
 *
 * An R*-Tree stores key,value pairs where the key is of type nbox_t
 * and the value is of type vec_t.
 *
 * The number of key-value pairs an index can hold is limited by the space
 * available on the volume containing the index.
 * The minimum size of an R*-tree index is 8 pages.
 *
 * Bulk-loading is supported for this index.
 * 
 * \note This implementation 
 * uses coarse-grained (index-level) locking and 
 * supports only 2 dimensions and integer coordinates.
 * For information about R*-trees, see the \ref BKSS.
 */


/**\defgroup SSMXCT  Transactions
 * \ingroup SSMAPI
 *
 * All work performed on behalf of a transaction must occur while that
 * transaction is "attached" to the thread that performs the work.
 * Creating a transaction attaches it to the thread that creates the transaction. 
 * The thread may detach from the transaction and attach to another.
 * Multiple threads may attach to a single transaction and do work in certain circumstances.  
 * \todo Document what  mpl-threaded xcts may and may not do.
 *
 * All transactions are by default local. They may be created, 
 * aborted, or committed.
 *
 * A transaction may participate in a two-phase commit. The value-added server must provide the coordination, directly or indirectly, for the two-phase commit.
 * The storage manager supports preparing a local transaction for two-phase commit, and it will log the necessary data for recovering in-doubt transactions.
 * 
 */

/**\defgroup SSMLOCK Locking 
 * \ingroup SSMXCT
 */

/**\defgroup SSMSP  Partial Rollback: Savepoints
 * \ingroup SSMXCT
 * See:
 * \todo sm_save_point_t
 * save_work, rollback_work
 */
/**\defgroup SSMQK  Early Lock Release: Quarks
 * \ingroup SSMXCT
 * \todo sm_quark_t
 */
/**\defgroup SSM2PC  Distributed Transactions: Two-Phase Commit
 * \ingroup SSMXCT
 * \todo 2pc
 */

/**\defgroup SSMISC  Miscellaneous
 * \ingroup SSMAPI
 */

/**\defgroup SSMSTATS Statistics
 * \ingroup SSMAPI
 */

/**\defgroup SSMVTABLE  Virtual Tables
 * \ingroup SSMAPI
 *
 * \todo some examples of virtual tables
 */

/**\defgroup SSMDEBUG Debugging Aids (Not for general use!)
 * \ingroup SSMAPI
 *
 * \section DEBUGLEV Build-time Debugging Options
 * At configure time, you can control which debugger-related options
 * (symbols, inlining, etc) with the debug-level options. See \ref CONFIGOPT.
 * \section SSMTRACE Tracing (--enable-trace)
 * When this build option is used, additional code is included in the build to
 * enable some limited tracing.  These C Preprocessor macros apply:
 * -FUNC
 *  Outputs the function name when the function is entered.
 * -DBG 
 *  Outputs the arguments.
 * -DBGTHRD 
 *  Outputs the arguments.
 *
 *  The tracing is controlled by these environment variables:
 *  -DEBUG_FLAGS: a list of file names to trace, e.g. "smfile.cpp log.cpp"
 *  -DEBUG_FILE: name of destination for the output. If not defined, the output
 *    is sent to cerr/stderr.
 *
 *  \note This tracing is not thread-safe, as it uses streams output.
 *  \todo What's W_TRACE about? Get rid of it?
 * See \ref CONFIGOPT.
 * \section SSMENABLERC Return Code Checking (--enable-checkrc)
 * If a w_rc_t is set but not checked with method is_error(), upon destruction the
 * w_rc_t will print a message to the effect "error not checked".
 * See \ref CONFIGOPT.
 * \section SSMSMSH Storage Manager Shell 
 */

/**\defgroup SSMEG Examples
 * \ref startstop.cpp
 */

/** \file sm_vas.h
 * \details
 * This is the include file that all value-added servers should
 * include to get the Shore Storage Manager API.
 *
 */
/********************************************************************/

class page_p;
class xct_t;
class device_m;
class vec_t;
class log_m;
class lock_m;
class btree_m;
class file_m;
class pool_m;
class dir_m;
class chkpt_m;
class lid_m; 
class sm_stats_cache_t;
class option_group_t;
class option_t;
class prologue_rc_t;
class rtree_m;
class sort_stream_i;

/**\brief A point to which a transaction can roll back.
 * \ingroup SSMSP
 *\details
 * A transaction an do partial rollbacks with
 * save_work  and rollback_work, which use this class to determine
 * how far to roll back.
 * It is nothing more than a log sequence number for the work done
 * to the point when save_work is called.
 */
class sm_save_point_t : public lsn_t {
public:
    NORET            sm_save_point_t(): _tid(0,0) {};
    friend ostream& operator<<(ostream& o, const sm_save_point_t& p) {
        return o << p._tid << ':' << (const lsn_t&) p;
    }
    friend istream& operator>>(istream& i, sm_save_point_t& p) {
        char ch;
        return i >> p._tid >> ch >> (lsn_t&) p;
    }
    tid_t            tid() const { return _tid; }
private:
    friend class ss_m;
    tid_t            _tid;
};

/**\brief List of locks acquired by a transaction since
 * the quark was "opened".   
 * \details
 * When a quark is closed (by calling close()), 
 * the release_locks parameter indicates if all read
 * locks acquired during the quark should be released.
 * \note Quarks are an experimental feature for use 
 * as a building block for a more general nested-transaction facility.
 *
 * \internal See lock_x.h
 */
class sm_quark_t {
public:
    NORET            sm_quark_t() {}
    NORET            ~sm_quark_t();

    rc_t            open();
    rc_t            close(bool release=true);

    tid_t            tid()const { return _tid; }
    operator         bool()const { return (_tid != tid_t::null); }
    friend ostream& operator<<(ostream& o, const sm_quark_t& q);
    friend istream& operator>>(istream& i, sm_quark_t& q);

private:
    friend class ss_m;
    tid_t            _tid;

    // disable
    sm_quark_t(const sm_quark_t&);
    sm_quark_t& operator=(const sm_quark_t&);

};

class sm_store_info_t;
class log_entry;
class coordinator;
class tape_t;
/**\brief \b This \b is \b the \b SHORE \b Storage \b Manager \b API.
 *\details
 * Most of the API for using the storage manager is through this
 * interface class.
 */
class ss_m : public smlevel_top 
{
    friend class pin_i;
    friend class sort_stream_i;
    friend class prologue_rc_t;
    friend class log_entry;
    friend class coordinator;
    friend class tape_t;
public:

    typedef smlevel_0::ndx_t ndx_t;
    typedef smlevel_0::concurrency_t concurrency_t;
    typedef smlevel_1::xct_state_t xct_state_t;

    typedef sm_store_property_t store_property_t;

#if COMMENT
    //
    // Below is most of the interface for the SHORE Storage Manager.
    // The rest is located in pin.h, scan.h, and smthread.h
    //

    //
    // COMMON PARAMETERS
    //
    // vec_t  hdr:          data for the record header
    // vec_t  data:          data for the body of a record 
    // recsize_t len_hint:   approximately how long a record will be
    //                    when all create/appends are completed.
    //                    Used by SM to choose correct storage
    //                    structure and page allocation
    //

    //
    // TEMPORARY FILES/INDEXES
    //
    // When a file or index is created there is a tmp_flag parameter
    // that when true indicates that the file is temporary.
    // Operations on a temporary file are not logged and the
    // file will be gone the next time the volume is mounted.
    //
    // TODO: IMPLEMENTATION NOTE on Temporary Files/Indexes:
    //        Temp files cannot be trusted after transaction abort.
    //            They should be marked for removal.
    //
    // CODE STRUCTURE:
    //    Almost all ss_m functions begin by creating a prologue object
    //    whose constructor and descructor check for many common errors.
    //    In addition most ss_m::OP() functions now call an ss_m::_OP()
    //    function to do the real work.  The ss_m::OP functions should
    //    not be called by other ss_m functions, instead the corresponding
    //    ss_m::_OP function should be used.
    //

    /*
     * The Storage manager accepts a number of options described
     * below.
     * 
       Required to be set: 

    sm_bufpoolsize <#>64>
                size of buffer pool in Kbytes
                default value: <none>
    sm_logdir <directory name>
                directory for log files
                default value: <none>

       Not necessary to be set:

     sm_logging <yes/no>
                yes indicates logging and recovery should be performed
                default value: yes

     sm_backgroundflush <yes/no>
                yes indicates background buffer flushing thread is enabled
                default value: yes

     sm_logsize <KB>
                maximum size of log in Kbytes
                default value: 1600

    sm_errlog  <string>
                -        indicates log to stderr
        syslogd  indicates log to syslog daemon
        <file>   indicates log to the named Unix file
                default value: -

    sm_errlog_level  <string>
                none, emerg, fatal, alert,
        internal, error, warning, info, debug
                default value: error

    sm_lock_escalate_to_page_threshold  <int>
        positive integer   denotes the default threshold to escalate
        0                  denotes don't escalate
        default value: 5

    sm_lock_escalate_to_store_threshold  <int>
        positive integer   denotes the default threshold to escalate
        0                  denotes don't escalate
        default value: 25

    sm_lock_escalate_to_volume_threshold  <int>
        positive integer   denotes the default threshold to escalate
        0                  denotes don't escalate
        default value: 0
    
    sm_num_page_writers <int>
                positive integer   denotes the number of page writers in the bpool cleaner
        default value: 16

    *
    */
#endif /* COMMENT */

  public:
    /**\brief Add storage manager options to the given options group.
     *\ingroup SSMINIT
     *\details
     * @param[in] grp The caller's option group, to which the
     * storage manager's options will be added for processing soon.
     *
     * Before the ss_m constructor can be called, setup_options
     * \b must be called.  This will install the storage manager's options and
     * initialize any that are not required.
     * Once all required options have been set, an ss_m can be constructed.
     *
     *\note This is not thread-safe.  The application (server) must prevent
     * concurrent calls to setup_options.
     */
    static rc_t setup_options(option_group_t* grp);

    /**\brief  Initialize the storage manager.
     * \ingroup SSMINIT
     * \details
     * @param[in] callb   A callback function. This is called when/if the log becomes "too full".
     *
     * \todo TODO LOG_WARN_CALLBACK_FUNC no longer used. Clean up or
     * reinstall. See the treatment of this in init.ssm man page.
     *
     * When an ss_m object is created, the storage manager initializes.
     * If the sthreads package has not already been initialized by virtue
     * of an sthread_t running, it is initialized now.
     *
     * The log is read and recovery is performed, and control returns to
     * the caller, after which time
     * storage manager threads (instances of smthread_t) may be constructed and
     * storage manager methods may be called.  The methods are 
     * static (you may use them as follows:
     * \code
     * ss_m *UNIQ = new ss_m();
     *
     * W_DO(UNIQ->mount_dev(...))
     *     // or
     * W_DO(ss_m::mount_dev(...))
     * \endcode
     * ).
     *
     * Only one ss_m object may be extant at any time. If you try
     * to create another while the one exists, a fatal error will occur
     * (your program will choke with a message about your mistake).
     */
    ss_m(LOG_WARN_CALLBACK_FUNC callb=0);

    /**\brief  Shut down the storage manager.
     * \ingroup SSMINIT
     * \details
     * When the storage manager object is deleted, it shuts down.
     * Thereafter it is not usable until another ss_m object is 
     * constructed.
     */
    ~ss_m();

    /**\brief Determine if storage manager shuts down cleanly.
     * \ingroup SSMINIT
     * \details
     * @param[in] clean   True means shut down gracefully, false means simulate a crash.
     *
     * When the storage manager's destructor is called
     * the buffer pool is flushed to disk, unless this method is called 
     * with \e clean == \e false.
     *
     * \note If this method is used, it
     * must be called after the storage manager is 
     * constructed if it is to take effect. Each time the storage
     * manager is constructed, the state associated with this is set
     * to \e true, i.e., "shut down properly".
     *
     * \note This method is not thread-safe, only one thread should use this
     * at any time, presumably just before shutting down.
     */
    static void         set_shutdown_flag(bool clean);

private:
    void                   _construct_once(LOG_WARN_CALLBACK_FUNC x=0);
    void                   _destruct_once();


public:
    /**\brief Begin a transaction 
     *\ingroup SSMXCT
     * @param[in] timeout   Optional, controls blocking behavior.
     * \details
     *
     * Start a new transaction and "attach" it to this thread. 
     * No running transaction may be attached to this thread.
     * 
     * Storage manager methods that must block (e.g., to acquire a lock) 
     * will use the timeout given.  
     * The default timeout is the one associated with this thread.
     *
     * \todo how to set timeouts.
     * \sa timeout_in_ms
     */
    static rc_t           begin_xct(
        timeout_in_ms            timeout = WAIT_SPECIFIED_BY_THREAD);

    /**\brief Begin an instrumented transaction. 
     *\ingroup SSMXCT
     * @param[in] stats   Pointer to an allocated statistics-holding structure.
     * @param[in] timeout   Optional, controls blocking behavior.
     * \details
     * No running transaction may be already attached to this thread.
     * A new transaction is started and attached to the running thread.
     *
     * The transaction will be instrumented.
     * This structure is updated by the storage manager. \todo when?
     * If this structure is not returned to the value-added server by means 
     * of commit_xct, abort_xct, or prepare_xct, it will be deleted when
     * the transaction ends. \todo Is this correct?
     *
     * 
     * Storage manager methods that must block (e.g., to acquire a lock) 
     * will use the timeout given.  
     * The default timeout is the one associated with this thread.
     *
     * \todo when are the stats available?
     * \todo how to set timeouts.
     * \sa timeout_in_ms
     */
    static rc_t           begin_xct(
        sm_stats_info_t*         stats,  // allocated by caller
        timeout_in_ms            timeout = WAIT_SPECIFIED_BY_THREAD);

    /**\brief Begin a transaction and return the transaction id.
     *\ingroup SSMXCT
     * @param[out] tid      Transaction id of new transaction.
     * @param[in] timeout   Optional, controls blocking behavior.
     * \details
     *
     * No running transaction may be attached to this thread.
     * 
     * Storage manager methods that must block (e.g., to acquire a lock) 
     * will use the timeout given.  
     * The default timeout is the one associated with this thread.
     *
     * \todo when are the stats available?
     * \todo how to set timeouts.
     * \sa timeout_in_ms
     */
    static rc_t           begin_xct(
        tid_t&                   tid,
        timeout_in_ms            timeout = WAIT_SPECIFIED_BY_THREAD);

    /**\brief Fork the designated transaction's log stream and attach
     *to the new one.
     *
     * \ingroup SSMXCT
     *
     * \details
     *
     * Somewhat misnamed for historical reaons, the xct_t returned by
     * begin_xct actually represents one (and the only, at first) \e
     * log \e stream of the transaction. This log stream is not
     * reentrant and must be duplicated in order for other threads to
     * attach to the transaction. The storage manager considers a
     * transaction to be multithreaded at any time multiple log
     * streams exist, regardless of whether threads are currently
     * attached to them. 
     *
     * Each log stream is independent of the others, and each may have
     * its own anchor and savepoint(s); locks are still shared among
     * all of a transaction's log streams. All log streams are
     * equivalent in the sense that ecah can perform any operation the
     * original could. Any stream can fork new streams or end the
     * transaction (assuming no others exist), and the original stream
     * can be closed (assuming others exist).
     *
     * Log streams may migrate freely between threads, and
     * attaching/detaching them is a lightweight operation. However,
     * only one thread may use a stream at a time, and Bad Things Will
     * Happen (tm) if two threads use one log stream
     * simultaneously. Release builds of the SM detect simultaneous
     * attachments on a best-effort basis only, while debug builds
     * enforce these conditions strictly.
     *
     * One risk of allowing multiple log streams is the possibility
     * for them to become "tangled" with respect to savepoints. For
     * example, all streams which access the same value must roll back
     * if any does, and rolling back a stream to undo one operation
     * may roll back other, unrelated work, as a side effect. For this
     * reason, savepoint operations may not occur when multiple
     * streams are present, though multiple streams may work in parallel
     * before, during, and after an active savepoint. All work on all
     * streams performed during a savepoint can thus be rolled back to
     * reach a known, consistent state.
     *
     * \e Notes
     *
     * In order to reduce overheads the system prefers to reopen
     * previously-closed log streams rather than creating new ones. As
     * a result the cost of forking log streams is more closely tied
     * to the maximum number open at any instant rather than the total
     * number of calls to fork_log_stream.
     *
     * \e Restrictions
     *
     * At most one thread may attach to a log stream at a time, and
     * attempts to attach a second thread to a log stream fail with
     * TWOTHREAD. The converse also holds: attaching a second log
     * stream to a thread fails with INTRANS. 
     *
     * At least one log stream must exist at all times, and attempts
     * to close the last one will fail with ONETHREAD.
     *
     * Some operations fail with TWOTHREAD if more than one log stream
     * exists. Notable examples include commit_xct/abort_xct,
     * creation/deletion of stores, use of quarks, and taking/rolling
     * back savepoints. Any stream is equally suitable for performing
     * these operations (not just the one returned by xct_begin) as
     * long as all others have been closed. Further, these operations
     * do not preclude the existence of multiple streams at other
     * times (including while savepoints or quarks are active).
     *
     * Some features cannot be combined with multithreaded
     * transactions at all. The main example (at this point) is
     * 2PC. Attempts to use these features after a call to
     * fork_log_stream will fail with TWOTHREAD (even if all others
     * have since been closed). Similarly, attempts to fork the log
     * stream after calling enter_2pc fail with IN2PC.
     */
    static rc_t		 fork_log_stream(xct_t* forkme);

    /**\brief Detach from, and close, the attached log stream
     *\ingroup SSMXCT
     *
     * \details
     *
     * When multiple log streams are created by fork_log_stream, all
     * but one must be closed before the transaction can end. In
     * addition, unwanted log streams may be closed at any
     * time. Attempting to close the last log stream fails with
     * ONETHREAD.
     */
    static rc_t		close_log_stream();

    /**\brief Instruments the current thread with the stats object
     * provided.
     *
     *\details
     *
     * The storage manager tracks a wide variety of statistics which
     * are ultimately collected on a per-thread basis and eventually
     * aggregated centrally. Statistics objects are not reentrant and
     * should only be assigned to one thread at a time.
     *
     * Statistics are always cleared when attached and snapped when
     * detached from their owner, but the system never deletes
     * user-supplied objects.
     *
     * There are two ways to track statistics:
     *
     * 1. Log stream-assigned. The user explicitly attaches a
     *    statistics object to a log stream at creation time
     *    (xct_begin attaches stats to the starting stream). Closing
     *    the stream or committing the transaction detaches the stats
     *    automatically. This method may impose high overhead for
     *    short transactions.
     *
     * 2. Thread-assigned. The user explicitly attaches a statistics
     *    object to an agent thread (with no transaction
     *    attached). All transactions which subsequently attach to the
     *    thread will use the thread's object if they do not have
     *    their own. Stats are detached automatically if the thread
     *    terminates.
     *
     * Statistics cannot be detached while a transaction is attached
     * to the calling thread (failing with INTRANS). The system
     * prefers stream-assigned over thread-assigned stats if both are
     * available; if neither is available the system creates a
     * stream-assigned stats and deletes it when the stream closes.
     *
     * This function silently replaces whatever stats were previously
     * attached.
     */
    static rc_t		attach_stats(sm_stats_info_t* s);

    /*\brief detaches user stats from the current thread. 
     *
     * \details
     *
     * No effect if no stats were previously attached.
     *
     * Errors: INTRANS if a transaction is currently attached.
     */      
    static rc_t		detach_stats();

    /**\brief Make the attached transaction a thread of a distributed transaction.
     *\ingroup SSM2PC
     *
     * @param[in] gtid    Global transaction ID to associate with this transaction.  This will be logged when the transaction is prepared.
     * 
     * \note This can be called at most once for a given transaction.
     * The transaction must be attached to the calling thread.
     * No other threads may be attached to the transaction.
     */
    static rc_t           enter_2pc(const gtid_t &gtid); 
    /**\brief Assign a coordinator handle to this distributed transaction.
     *\ingroup SSM2PC
     * @param[in] h      Handle of the coordinator.  Not interpreted by
     * the storage manager.
     *
     * The storage manager associates this server handle with the transaction 
     * so that when the transaction is prepared, this information is 
     * written to the log. Upon recovery, if this transaction is still in doubt,
     * the value-added server can query the 
     * storage manager for in-doubt transactions, get their server handles,
     * and resolve the transactions.
     * See query_prepared_xct and recover_2pc.
     */
    static rc_t           set_coordinator(const server_handle_t &h); 

    /**\brief Prepare a thread of a distributed transaction.
     *\ingroup SSM2PC
     * @param[in] stats     Pointer to an allocated statistics-holding 
     *                      structure.
     * @param[out] vote     This thread's vote.
     *
     * The storage manager will prepare the attached transaction (a thread
     * of a distributed transaction) for commit.
     * If this transaction has performed no logged updates, the 
     * vote returned will be vote_readonly.
     * If this transaction can commit, the vote returned will be vote_commit.
     * If an error occurs during the prepare, the vote will be vote_abort.
     *
     * If the transaction is being instrumented, the 
     * statistics-holding structure will be returned to the caller, 
     * and the caller is responsible for its deallocation.
     */
    static rc_t           prepare_xct(
                            sm_stats_info_t*&         stats, 
                            vote_t&                   vote); 

    /**\brief Prepare a thread of a distributed transaction.
     *\ingroup SSM2PC
     * @param[out] vote     This thread's vote.
     *
     * The storage manager will prepare the attached transaction (a thread
     * of a distributed transaction) for commit.
     * If this transaction has performed no logged updates, the 
     * vote returned will be vote_readonly.
     * If this transaction can commit, the vote returned will be vote_commit.
     * If an error occurs during the prepare, the vote will be vote_abort.
     */
    static rc_t           prepare_xct(vote_t &vote); 

    /**\brief Force the transaction to vote "read-only" in a two-phase commit. 
     * \details
     * This will override the storage manager's determination of 
     * whether this thread of a distributed transaction is read-only, which is
     * based on whether the local transation thread logged anything. This
     * method may be useful if the local transaction rolled back to 
     * a savepoint.
     */
    static rc_t           force_vote_readonly(); 

    /**\brief Given a global transaction id, find the local prepared transaction
     * associated with it. 
     *\ingroup SSM2PC
     * @param[in] gtid     A global transaction ID (an opaque quantity to the storage manager).
     * @param[in] mayblock Not used.
     * @param[out] local   Return the transaction ID of the prepared SM transaction.
     * \details
     * Searches the transaction list for a prepared transaction with the given
     * global transaction id. If found, it returns a reference to the 
     * local transaction.  The transaction is attached to the running
     * thread before it is returned.
     */
    static rc_t           recover_2pc(const gtid_t & gtid,
        bool                      mayblock,
        tid_t &                   local
        );

    /**\brief  Return the number of prepared transactions.
     *\ingroup SSM2PC
     * @param[out] numtids   The number of in-doubt transactions.
     * \details
     * Used by a server at start-up, after recovery, to find out if
     * there are any in-doubt transactions.  If so, the server must
     * use the second form of query_prepared_xct to find the global
     * transaction IDs of these in-doubt transactions.
     */
    static rc_t           query_prepared_xct(int &numtids);

    /**\brief  Return the global transaction IDs of in-doubt transactions. 
     *\ingroup SSM2PC
     * @param[in] numtids   The number of global transaction ids in the list.
     * @param[in] l   The caller-provided list into which to write the 
     * global transaction-ids.
     * \details
     * Used by a server at start-up, after recovery, to find out the
     * global transaction IDs of the prepared transactions.  The storage
     * manager fills in the first numtids entries of the pre-allocated list.
     * The server may have first called the first form of query_prepared_xct
     * to find out how many such transactions there are after recovery.
     *
     * \todo Read-only transactions might not appear as in-doubt transactions?
     * \todo more details about 2pc?
     */
    static rc_t           query_prepared_xct(int numtids, gtid_t l[]);


    /**\brief Commit a transaction.
     *\ingroup SSMXCT
     * @param[in] lazy   Optional, controls flushing of log.
     * \details
     *
     * Commit the attached transaction and detach it, destroy it.
     * If \e lazy is true, the log is not synced.  This means that
     * recovery of this transaction might not be possible.
     */
    static rc_t           commit_xct(
                                     bool   lazy = false,
                                     lsn_t* plastlsn=NULL);

    /**\brief Commit an instrumented transaction and get its statistics.
     *\ingroup SSMXCT
     * @param[out] stats   Get a copy of the statistics for this transaction.
     * @param[in] lazy   Optional, controls flushing of log.
     * \details
     *
     * Commit the attached transaction and detach it, destroy it.
     * If \e lazy is true, the log is not synced.  This means that
     * recovery of this transaction might not be possible.
     */
    static rc_t            commit_xct(
        sm_stats_info_t*&         stats, 
        bool                      lazy = false,
        lsn_t* plastlsn=NULL);

    /**\brief Commit an instrumented transaction and start a new one.
     *\ingroup SSMXCT
     * @param[out] stats   Get a copy of the statistics for the first transaction.
     * @param[in] lazy   Optional, controls flushing of log.
     * \details
     *
     * Commit the attached transaction and detach it, destroy it.
     * Start a new transaction and attach it to this thread.
     * \note \e The \e new 
     * \e transaction \e inherits \e the \e locks \e of \e the \e old 
     * \e transaction.
     *
     * If \e lazy is true, the log is not synced.  This means that
     * recovery of this transaction might not be possible.
     */
    static rc_t            chain_xct(
        sm_stats_info_t*&         stats,    /* in w/new, out w/old */
        bool                      lazy = false);  

    /**\brief Commit a transaction and start a new one, inheriting locks.
     *\ingroup SSMXCT
     * @param[in] lazy   Optional, controls flushing of log.
     * \details
     *
     * Commit the attached transaction and detach it, destroy it.
     * Start a new transaction and attach it to this thread.
     * \note \e The \e new 
     * \e transaction \e inherits \e the \e locks \e of \e the \e old 
     * \e transaction.
     *
     * If \e lazy is true, the log is not synced.  This means that
     * recovery of the committed transaction might not be possible.
     * \todo anything more about chained transactions?
     */
    static rc_t            chain_xct(bool lazy = false);  

    /**\brief Abort an instrumented transaction and get its statistics.
     *\ingroup SSMXCT
     * @param[out] stats   Get a copy of the statistics for this transaction.
     * \details
     *
     * Abort the attached transaction and detach it, destroy it.
     */
    static rc_t            abort_xct(sm_stats_info_t*&  stats);
    /**\brief Abort a transaction.
     *\ingroup SSMXCT
     * \details
     *
     * Abort the attached transaction and detach it, destroy it.
     */
    static rc_t            abort_xct();

    /**\brief Populate a save point.
     *\ingroup SSMSP
     * @param[out] sp   An sm_save_point_t owned by the caller.
     *\details
     * Store in sp the needed information to be able to roll back 
     * to this point. 
     * For use with rollback_work.
     * \note Only one thread may be attached to a transaction when this
     * is called.
     */
    static rc_t            save_work(sm_save_point_t& sp);

    /**\brief Roll back to a savepoint.
     *\ingroup SSMSP
     * @param[in] sp   An sm_save_point_t owned by the caller and
     * populated by save_work.
     *\details
     * Undo everything that was 
     * done from the time save_work was called on this savepoint.
     * Locks are not freed.
     *
     * \note Only one thread may be attached to a transaction when this
     * is called.
     */
    static rc_t            rollback_work(const sm_save_point_t& sp);

    /**\brief Return the number of transactions in active state.
     *\ingroup SSMXCT
     * \details
     * While this is thread-safe, the moment a value is returned, it could
     * be out of date.
     * Useful only for debugging.
     */
    static w_base_t::uint4_t     num_active_xcts();

    /**\brief Attach the given transaction to the currently-running smthread_t.
     *\ingroup SSMXCT
     * \details
     * It is assumed that the currently running thread is an smthread_t.
     */
    static void           attach_xct(xct_t *x) { me()->attach_xct(x); }

    /**\brief Detach any attached from the currently-running smthread_t.
     *\ingroup SSMXCT
     * \details
     * Sever the connection between the running thread and the transaction.
     * This allow the running thread to attach a different 
     * transaction and to perform work in its behalf.
     */
    static void           detach_xct() { xct_t *x = me()->xct();
                                        if(x) me()->detach_xct(x); }

    /**\brief Get the transaction structure for a given a transaction id.
     *\ingroup SSMXCT
     * @param[in] tid   Transaction ID.
     *\details
     * Return a pointer to the storage manager's transaction structure.
     * Can be used with detach_xct and attach_xct.
     */
    static xct_t*          tid_to_xct(const tid_t& tid);
    /**\brief Get the transaction ID for a given a transaction structure.
     *\ingroup SSMXCT
     * @param[in] x   Pointer to transaction structure.
     *\details
     * Return the transaction ID for the given transaction.
     */
    static tid_t           xct_to_tid(const xct_t* x);

    /**\brief Print transaction information to an output stream.
     *\ingroup SSMAPIDEBUG
     * @param[in] o   Stream to which to write the information.
     * \details
     * This is for debugging only, and is not thread-safe. 
     */
    static rc_t            dump_xcts(ostream &o);

    /**\brief Get the transaction state for a given transaction (structure).
     *\ingroup SSMXCT
     * @param[in] x   Pointer to transaction structure.
     * \details
     * Returns the state of the transaction (active, prepared). It is
     * hard to get the state of an aborted or committed transaction, since
     * their structures no longer exist.
     */
    static xct_state_t     state_xct(const xct_t* x);

    /**\brief Return the amount of log this transaction would consume
     * if it rolled back.
     *\ingroup SSMXCT
     *
     * If a transaction aborts with eOUTOFLOGSPACE this function can
     * be used in conjunction with xct_reserve_log_space to
     * pre-allocate the needed amount of log space before retrying.
     */
    static smlevel_0::fileoff_t		xct_log_space_needed();

    /**\brief Require the specified amount of log space to be
     * available for this transaction before continuing.
     *\ingroup SSMXCT
     *
     * If a transaction risks running out of log space it can
     * pre-request some or all of the needed amount before starting in
     * order to improve its chances of success. Other new transactions
     * will be unable to acquire log space before this request is
     * granted (existing ones will be able to commit, unless they also
     * run out of space, because that tends to free up log space and
     * avoids wasting work).
     */
    static rc_t			xct_reserve_log_space(fileoff_t amt);
    
    /**\brief Get the locking level for the attached transaction.
     * \ingroup SSMLOCK
     */
    static concurrency_t   xct_lock_level();
    /**\brief Set the default locking level for the attached transaction.
     * \ingroup SSMLOCK
     * \details
     * @param[in] l  The level to use for the balance of this transaction.
     * Legitimate values are t_cc_record,  t_cc_page,  t_cc_file.
     *
     * \note Only one thread may be attached to the transaction when this
     * is called. If more than one thread is attached, a fatal error
     * will ensue.
     */
    static void            set_xct_lock_level(concurrency_t l);

    /**\brief Collect transaction information in a virtual table.
     * \ingroup SSMVTABLE
     * \details
     * @param[out] v  The virtual table to populate.
     * @param[in] names_too  If true, make the 
     *            first row of the table a list of the attribute names.
     *
     * All attribute values will be strings.
     * The virtual table v can be printed with its output operator
     * operator<< for ostreams.
     */
    static rc_t            xct_collect(vtable_t&v, bool names_too=true);

    /**\brief Collect buffer pool information in a virtual table.
     * \ingroup SSMVTABLE
     * \details
     * @param[out] v  The virtual table to populate.
     * @param[in] names_too  If true, make the 
     *            first row of the table a list of the attribute names.
     *
     * \attention Be wary of using this with a large buffer pool.
     *
     * All attribute values will be strings.
     * The virtual table v can be printed with its output operator
     * operator<< for ostreams.
     */
    static rc_t            bp_collect(vtable_t&v, bool names_too=true);

    /**\brief Collect lock table information in a virtual table.
     * \ingroup SSMVTABLE
     * \details
     * @param[out] v  The virtual table to populate.
     * @param[in] names_too  If true, make the 
     *            first row of the table a list of the attribute names.
     *
     * All attribute values will be strings.
     * The virtual table v can be printed with its output operator
     * operator<< for ostreams.
     */
    static rc_t            lock_collect(vtable_t&v, bool names_too=true);

    /**\brief Collect thread information in a virtual table.
     * \ingroup SSMVTABLE
     * \details
     * @param[out] v  The virtual table to populate.
     * @param[in] names_too  If true, make the 
     *            first row of the table a list of the attribute names.
     *
     * All attribute values will be strings.
     * The virtual table v can be printed with its output operator
     * operator<< for ostreams.
     */
    static rc_t            thread_collect(vtable_t&v, bool names_too=true);

    /**\brief Take a checkpoint.
     * \ingroup SSMAPIDEBUG
     * \note For debugging only!
     *
     * Force the storage manager to take a checkpoint.
     * Checkpoints are fuzzy : they can be taken while most other
     * storage manager activity is happening, even though they have
     * to be serialized with respect to each other, and with respect to
     * a few other activities.
     *
     * This is thread-safe.
     */
    static rc_t            checkpoint();

    /**\brief Force the buffer pool to flush its pages to disk.
     * \ingroup SSMAPIDEBUG
     * @param[in] invalidate   True means discard pages after flush.
     * \note For debugging only!
	 * \attention Do not call force_buffers with anything pinned.
	 * You may cause latch-latch deadlocks, as this method has
	 * to scan the entire buffer pool and EX-latch pages to prevent
	 * others from updating while it forces to disk.
	 * Since the page-order is essentially random, we cannot
	 * preclude latch-latch deadlocks with other threads.
     */
    static rc_t            force_buffers(bool invalidate = false);

    /**\brief Force the buffer pool to flush the volume header page(s)
	 * to disk.
     * \ingroup SSMAPIDEBUG
     * @param[in] vid   ID of the volume of interest
     * \note For debugging only!
	 * \attention Do not call force_vol_hdr_buffers with anything pinned.
	 * You could cause latch-latch deadlocks, as this method has
	 * to scan the entire buffer pool and EX-latch some pages.
	 * Since the page-order is essentially random, we cannot
	 * preclude latch-latch deadlocks with other threads.
	 */
    static rc_t            force_vol_hdr_buffers( const vid_t&   vid);

    /**\brief Force the buffer pool to flush to disk all pages
     * for the given store.
     * \ingroup SSMAPIDEBUG
     * @param[in] stid   Store whose pages are to be flushed.
     * @param[in] invalidate   True means discard the pages after flushing.
     * \note For debugging only!
	 * \attention Do not call force_store_buffers with anything pinned.
	 * You may cause latch-latch deadlocks, as this method has
	 * to scan the entire buffer pool and EX-latch pages to prevent
	 * others from updating while it forces to disk.
	 * Since the page-order is essentially random, we cannot
	 * preclude latch-latch deadlocks with other threads.
     */
    static rc_t            force_store_buffers(const stid_t & stid,
                                               bool invalidate);

    /**\cond skip 
     * Do not document. Very un-thread-safe.
     */
    static rc_t            dump_buffers(ostream &o);
    static rc_t            dump_locks(ostream &o);
    static rc_t            dump_locks(); // defaults to std::cout
    static rc_t            dump_exts(ostream &o, 
        vid_t                    v, 
        extnum_t                 start, 
        extnum_t                 end);

    static rc_t            dump_stores(ostream &o, 
        vid_t                    v, 
        int                      start, 
        int                      end);

    static rc_t            dump_histo(ostream &o, bool locked);

    static rc_t            snapshot_buffers(
        u_int&                 ndirty, 
        u_int&                 nclean, 
        u_int&                 nfree,
        u_int&                 nfixed);
    /**\endcond skip */

    /**\brief Get a copy of the statistics from an attached instrumented transaction.
     * \ingroup SSMXCT
     * \details
     * @param[out] stats Returns a copy of the statistics for this transaction.
     * @param[in] reset  If true, the statistics for this transaction will be zeroed.
     */
    static rc_t            gather_xct_stats(
        sm_stats_info_t&       stats, 
        bool                   reset = false);

    /**\brief Get a copy of the global statistics.
     * \ingroup SSMSTATS
     * \details
     * @param[out] stats A pre-allocated structure.
     */
    static rc_t            gather_stats(
        sm_stats_info_t&       stats
        );

    /**\brief Get a copy of configuration-dependent information.
     * \ingroup SSMMISC
     * \details
     * @param[out] info A pre-allocated structure.
     */
    static rc_t            config_info(sm_config_info_t& info);

    /**\brief
     * \todo is this used?  What about the fake_disk_whatever?
    // This method sets a milli_sec delay to occur before 
    // each disk read/write operation.  This is useful in discovering
    // thread sync bugs
    */
    static rc_t            set_disk_delay(u_int milli_sec);

    // This method tells the log manager to start generating corrupted
    // log records.  This will make it appear that a crash occurred
    // at that point in the log.  A call to this method should be
    // followed immediately by a dirty shutdown of the ssm.
    static rc_t            start_log_corruption();

    // Forces a log flush
    static rc_t 		sync_log(bool block=true);
    static rc_t 		flush_until(lsn_t& anlsn, bool block=true);

    // Allowing to access info about the important lsns (curr and durable)
    static rc_t                 get_curr_lsn(lsn_t& anlsn);
    static rc_t                 get_durable_lsn(lsn_t& anlsn);


    /*
       Device and Volume Management
       ----------------------------
       A device is either an operating system file or operating system
       device and is identified by a path name (absolute or relative).
       A device has a quota.  In theory, a device may have 
       multiple volumes on it but
       in the current implementation the maximum number of volumes
       is 1.

       A volume is where data is stored.  A volume is identified
       uniquely and persistently by a long volume ID (lvid_t).
       Volumes can be used whenever the device they are located
       on is mounted by the SM.  Volumes have a quota.  The
       sum of the quotas of all the volumes on a device cannot
       exceed the device quota.

       The basic steps to begin using a new device/volume are:
        format_dev: initialize the device
        mount_dev: allow use of the device and all its volumes
        generate_new_lvid: generate a unique ID for the volume
        create_vol: create a volume on the device
     */

    /*
     * Device management functions
     */
    /**\brief Format a device.
     * \ingroup SSMVOL
     * \details
     * @param[in] device   Operating-system file name of the "device".
     * @param[in] quota_in_KB  Quota in kilobytes.
     * @param[in] force If true, format the device even if it already exists.
     *
     * Since raw devices always "exist", \e force should be given as true 
     * for raw devices.
     *
     * A device may not be formatted if it is already mounted.
     *
     * \note This method should \b not 
     * be called in the context of a transaction.
     */
    static rc_t            format_dev(
        const char*            device,
        smksize_t              quota_in_KB,
        bool                   force);
    
    /**\brief Mount a device.
     * \ingroup SSMVOL
     * \details
     * @param[in] device   Operating-system file name of the "device".
     * @param[out] vol_cnt Number of volumes on the device.
     * @param[out] devid  A local device id assigned by the storage manager.
     * @param[in] local_vid A local handle to the (only) volume on the device,
     * to be used when a volume is mounted.  The default, vid_t::null, 
     * indicates that the storage manager can chose a value for this. 
     *
     * \note It is fine to mount a device more than once, as long as device
     * is always the same (you cannot specify a hard link or soft link to
     * an entity mounted under a different path). 
     * Device mounts are \b not reference-counted, so a single dismount_dev
     * renders the volumes on the device unusable.
     *
     * \note This method should \b not 
     * be called in the context of a transaction.
     */
    static rc_t            mount_dev(
        const char*            device,
        u_int&                 vol_cnt,
        devid_t&               devid,
        vid_t                  local_vid = vid_t::null);

    /**\brief Dismount a device.
     * \ingroup SSMVOL
     * \details
     * @param[in] device   Operating-system file name of the "device".
     *
     * \note It is fine to mount a device more than once, as long as device
     * is always the same (you cannot specify a hard link or soft link to
     * an entity mounted under a different path). 
     * Device mounts are \b not reference-counted, so a single dismount_dev
     * renders the volumes on the device unusable.
     *
     * \note This method should \b not 
     * be called in the context of a transaction.
     */

    static rc_t            dismount_dev(const char* device);

    /**\brief Dismount all mounted devices.
     * \ingroup SSMVOL
     *
     * \note This method should \b not 
     * be called in the context of a transaction.
     */
    static rc_t            dismount_all();

    // list_devices returns an array of char* pointers to the names of
    // all mounted devices.  Note that the use of a char*'s is 
    // a temporary hack until a standard string class is available.
    // the char* pointers are pointing directly into the device
    // mount table.
    // dev_cnt is the length of the list returned.
    // dev_list and devid_list must be deleted with delete [] by the
    // caller if they are not null (0).  They should be null
    // if an error is returned or if there are no devices.
    /**\brief Return a list of all mounted devices.
     * \ingroup SSMVOL
     * \details
     * @param[out] dev_list   Returned list of pointers directly into the mount table.
     * @param[out] devid_list   Returned list of associated device ids.
     * @param[out] dev_cnt   Returned number of entries in the two above lists.
     *
     * The storage manager allocates the arrays returned with new[], and the
     * caller must return these to the heap with delete[] if they are not null.
     * They will be null if an error is returned or if no devices are mounted.
     *
     * The strings to which dev_list[*] point are \b not to be deleted by
     * the caller.
     */
    static rc_t            list_devices(
        const char**&            dev_list, 
        devid_t*&                devid_list, 
        u_int&                   dev_cnt);

    /**\brief Return a list of all volume on a device.
     * \ingroup SSMVOL
     * \details
     * @param[in] device   Operating-system file name of the "device".
     * @param[out] lvid_list   Returned list of pointers directly into the mount table.
     * @param[out] lvid_cnt   Returned length of list lvid_list.
     *
     * The storage manager allocates the array lvid_list 
     * with new[], and the
     * caller must return it to the heap with delete[] if it is not null.
     * It will be null if an error is returned. 
     *
     * \note This method should \b not 
     * be called in the context of a transaction.
     */
    static rc_t            list_volumes(
        const char*            device,
        lvid_t*&               lvid_list,
        u_int&                 lvid_cnt
    );

    // get_device_quota the "quota" (in KB) of the device
    // and the amount of the quota allocated to volumes on the device.
    /**\brief Get the device quota.
     * \ingroup SSMVOL
     * \details
     * @param[in] device   Operating-system file name of the "device".
     * @param[out] quota_KB   Returned quota in kilobytes
     * @param[out] quota_used_KB   Returned portion of quota allocated to volumes
     *
     * The quota_used_KB is the portion of the quota allocated to volumes on the device.
     *
     * \note This method \b may 
     * be called in the context of a transaction.
     *
     * \note This method \b may 
     * be called in the context of a transaction.
     */
    static rc_t            get_device_quota(
        const char*             device, 
        smksize_t&              quota_KB, 
        smksize_t&              quota_used_KB);


    /*
     * Volume management functions
     */

    // fake_disk_latency
    static rc_t enable_fake_disk_latency(vid_t vid);
    static rc_t disable_fake_disk_latency(vid_t vid);
    static rc_t set_fake_disk_latency(vid_t vid, const int adelay);


    // generate a universally unique volume id
    static rc_t generate_new_lvid(lvid_t& lvid);
     
    /*
      create_vol is use to create and format a new volume.
      When a volume is stored on a raw device, formatting it
      involves the time consuming step of zero-ing every page.  This is
      necessary for correct operation of recovery.  For some users,
      this zeroing is unnecessary since only testing is being done.  In
      this case, setting skip_raw_init to true disables the zero-ing.
      Creating a volume implies the volume will be served.

      Local_vid is used to specify the local handle that should be when
      the volume is mounted.  The default value vid_t::null indicates
      that the SM can use any number it wants to use.
     */
    static rc_t            create_vol(
        const char*             device_name,
        const lvid_t&           lvid,
        smksize_t               quota_KB,
        bool                    skip_raw_init = false,
        vid_t                   local_vid = vid_t::null,
        const bool              apply_fake_io_latency = false,
        const int               fake_disk_latency = 0);

    static rc_t            destroy_vol(const lvid_t& lvid);

    // get_volume_quota gets the "quota" (in KB) of the volume
    // and the amount of the quota used in allocated extents
    static rc_t            get_volume_quota(
        const lvid_t&             lvid, 
        smksize_t&                quota_KB, 
        smksize_t&                quota_used_KB);

    // check_volume_page_types: strictly for debugging/testing
    static rc_t             check_volume_page_types(vid_t vid);
    //
    // These get_du_statistics functions are used to get volume
    // utilization stats similar to unix du and df operations.  The du
    // functions get stats on either a volume or a specific file or
    // index.
    //
    // Note that stats are
    // added the the sm_du_stats_t structure rather than overwriting it.
    //
    // When audit==true, the volume or store is SH locked and the stats
    // are audited for correctness (a fatal internal error will be
    // generated if an audit fails -- that way the system stops exactly
    // where the audit fails for easier debugging).  When audit==false,
    // only IS locks are obtained and no auditing is done.
    //

    // du on a volume by (local handle) vid
    static rc_t            get_du_statistics(
        vid_t                 vid,
        sm_du_stats_t&        du,
        bool                  audit = true); 

    static rc_t            get_du_statistics(
        const stid_t&        stid, 
        sm_du_stats_t&       du,
        bool                 audit = true);
    
    // These functions compute statistics on files and volumes
    // using only the meta data about the them.
    //
    // If cc is set to t_cc_vol or t_cc_file an SH lock will be
    // acquired before the gathering begins, others no locks will
    // be acquired.  If no locks are acquired and batch calculate
    // is not set, there is the possibility that get_file_meta_stats
    // may fail with eRETRY due to simultaneus updating (should be
    // a rare occurance).
    //
    // If batch_calculate is true then code works by making one pass
    // over the meta data, but it looks at all the meta data.  this
    // should be faster if there is not a small number of files, or
    // files use a large portion of the volume.
    //
    // If batch_calculate is false then each file is updated
    // indidually, only looking at the extent information for that
    // particular file.

    static rc_t            get_volume_meta_stats(
        vid_t                vid,
        SmVolumeMetaStats&   volume_stats,
        concurrency_t        cc = t_cc_none
    );

    static rc_t            get_file_meta_stats(
        vid_t                vid,
        w_base_t::uint4_t    num_files,
        SmFileMetaStats*     file_stats,
        bool                 batch_calculate = false,
        concurrency_t        cc = t_cc_none
    );
   
    static rc_t            vol_root_index(
        const vid_t&        v, 
        stid_t&             iid
    )    { iid.vol = v; iid.store = store_id_root_index; return RCOK; }

    /*****************************************************************
     * storage operations: smfile.cpp
     *****************************************************************/

    /**\brief Change the store property of a file or index.
     * @param[in] stid   File ID or index ID of the store to change.
     * @param[in] property   Enumeration store_property_t (alias for
     *                   smlevel_3::sm_store_property_t, q.v.)
     *
     * \details
     * The possible uses of store properties are described with 
     * smlevel_3::sm_store_property_t.
     */
    static rc_t            set_store_property(
        stid_t                stid,
        store_property_t      property
        );

    /**\brief Get the store property of a file or index.
     * @param[in] stid   File ID or index ID of the store of interest.
     * @param[in] property   Reference to enumeration store_property_t 
     *                  (alias for smlevel_3::sm_store_property_t, q.v.)
     *
     * \details
     * The possible uses of store properties are described with 
     * smlevel_3::sm_store_property_t.
     */
    static rc_t            get_store_property(
        stid_t                stid,
        store_property_t&     property);

    /**\brief Get various store information of a file or index.
     * @param[in] stid   File ID or index ID of the store of interest.
     * @param[out] info  Reference to sm_store_info_t into which to
     * write the results.
     *
     * \details
     * \todo sm.h get_store_info
     */
    static rc_t            get_store_info( 
        const stid_t&         stid, 
        sm_store_info_t&      info);

    //
    // Functions for B+tree Indexes
    //

    static rc_t            create_index(
                vid_t                 vid, 
                ndx_t                 ntype, 
                store_property_t      property,
                const char*           key_desc,
                concurrency_t         cc, 
                stid_t&               stid
    );

    // for backward compatibility:
    static rc_t            create_index(
                vid_t                 vid, 
                ndx_t                 ntype, 
                store_property_t      property,
                const char*           key_desc,
                stid_t&               stid
    );

    static rc_t            destroy_index(const stid_t& iid); 
    static rc_t            bulkld_index(
        const stid_t&             stid, 
        int                       nsrcs,
        const stid_t*             source,
        sm_du_stats_t&            stats,
        bool                      sort_duplicates = true,
        bool                      lexify_keys = true
    );
    static rc_t            bulkld_index(
        const stid_t&             stid, 
        const stid_t&             source,
        sm_du_stats_t&            stats,
        bool                      sort_duplicates = true,
        bool                      lexify_keys = true
    );
    static rc_t            bulkld_index(
        const stid_t&             stid, 
        sort_stream_i&            sorted_stream,
        sm_du_stats_t&            stats);

    static rc_t            print_index(stid_t stid);

    static rc_t            create_assoc(
        stid_t                   stid, 
        const vec_t&             key, 
        const vec_t&             el
#ifdef SM_DORA
        , const bool             bIgnoreLocks = false
#endif
    );
    static rc_t            destroy_assoc(
        stid_t                   stid, 
        const vec_t&             key,
        const vec_t&             el
#ifdef SM_DORA
        , const bool             bIgnoreLocks = false
#endif
    );
    static rc_t            destroy_all_assoc(
        stid_t                  stid, 
        const vec_t&            key,
        int&                    num_removed
    );
    static rc_t            find_assoc(
        stid_t                  stid, 
        const vec_t&            key, 
        void*                   el, 
        smsize_t&               elen, 
        bool&                   found
#ifdef SM_DORA
        , const bool             bIgnoreLocks = false
#endif
    );

    //
    // Functions for R*tree (multi-dimensional(MD), spatial) Indexes
    //
    static rc_t            create_md_index(
        vid_t                   vid, 
        ndx_t                   ntype, 
        store_property_t        property,
        stid_t&                 stid, 
        int2_t                  dim = 2
    );

    static rc_t            destroy_md_index(const stid_t& iid);

    static rc_t            bulkld_md_index(
        const stid_t&             stid, 
        int                       nsrcs,
        const stid_t*             source, 
        sm_du_stats_t&            stats,
        int2_t                    hff=75,
        int2_t                    hef=120,
        nbox_t*                   universe=NULL);

    static rc_t            bulkld_md_index(
        const stid_t&             stid, 
        const stid_t&             source, 
        sm_du_stats_t&            stats,
        int2_t                    hff=75,
        int2_t                    hef=120,
        nbox_t*                   universe=NULL);

    static rc_t            bulkld_md_index(
        const stid_t&             stid, 
        sort_stream_i&            sorted_stream,
        sm_du_stats_t&            stats,
        int2_t                    hff=75,
        int2_t                    hef=120,
        nbox_t*                   universe=NULL);

    static rc_t            print_md_index(stid_t stid);

    static rc_t            find_md_assoc(
        stid_t                    stid, 
        const nbox_t&             key, 
        void*                     el, 
        smsize_t&                 elen, 
        bool&                     found);

    static rc_t            create_md_assoc(
        stid_t                    stid, 
        const nbox_t&             key,
        const vec_t&              el);

    static rc_t            destroy_md_assoc(
        stid_t                    stid, 
        const nbox_t&             key,
        const vec_t&              el);

    static rc_t            draw_rtree(const stid_t& stid, ostream &);

    static rc_t            rtree_stats(
        const stid_t&             stid,
        rtree_stats_t&            stat,
        uint2_t                   size = 0,
        uint2_t*                  ovp = NULL,
        bool                      audit = false);

    
    /**\brief Create a file of records.
     * \ingroup SSMFILE
     * \details
     * @param[in] vid   Volume on which to create a file.
     * @param[out] fid  Returns (store) ID of the new file here.
     * @param[in] property Give the file the this property.
     * @param[in] cluster_hint Not used. 
     *
     * The cluster hint is included in the API for future use. 
     * It has no effect.
     */
    static rc_t            create_file( 
        vid_t                   vid, 
        stid_t&                 fid,
        store_property_t        property,
        shpid_t                 cluster_hint = 0
    ); 

    /**\brief Destroy a file of records.
     * \ingroup SSMFILE
     * \details
     * @param[in] fid  ID of the file to destroy.
     */
    static rc_t            destroy_file(const stid_t& fid); 

    /**\brief Create a new record.
     * \ingroup SSMFILE
     * \details
     * @param[in] fid  ID of the file in which to create a record.
     * @param[in] hdr  What to put in the record's header.
     * @param[in] len_hint  Hint about how big the record will ultimately be.
     * \todo how is this used? See also append_file_i
     * @param[in] data  What to put in the record's body. 
     * @param[out] new_rid  ID of the newly created record.
     */
    static rc_t            create_rec(
        const stid_t&            fid, 
        const vec_t&             hdr, 
        smsize_t                 len_hint, 
        const vec_t&             data, 
        rid_t&                   new_rid
#ifdef SM_DORA
        , const bool             bIgnoreLocks = false
#endif
    ); 

    /**\brief Destroy a record.
     * \ingroup SSMFILE
     * \details
     * @param[in] rid  ID of the record to destroy.
     */
    static rc_t            destroy_rec(const rid_t& rid
#ifdef SM_DORA
        , const bool             bIgnoreLocks = false
#endif
                                       );

    /**\brief Modify the body of an existing record.
     * \ingroup SSMFILE
     * \details
     * @param[in] rid  ID of the record to modify.
     * @param[in] start  First byte to change.
     * @param[in] data  What to put in the record's body.  
     *
     * This overwrites
     * the existing bytes, starting at the offset \e start through the
     * byte at \e start + \e data.size().
     * This method \b cannot \b be \b used to change the size of a record.
     * Attempting this will result in an error.
     */
    static rc_t            update_rec(
        const rid_t&             rid, 
        smsize_t                 start, 
        const vec_t&             data);

    /**\brief Modify the header of an existing record.
     * \ingroup SSMFILE
     * \details
     * @param[in] rid  ID of the record to modify.
     * @param[in] start  First byte to change.
     * @param[in] hdr  What to put in the record's header.  
     *
     * This overwrites
     * the existing bytes, starting at the offset \e start through the
     * byte at \e start + \e data.size().
     * This method \b cannot \b be \b used to change the size of a record
     * header. There are no methods for appending to or truncating a
     * record header.
     *
     * \sa pin_i::update_rec, \ref SSMPIN
     */
    static rc_t            update_rec_hdr(
        const rid_t&             rid, 
        smsize_t                 start, 
        const vec_t&             hdr);
    // see also pin_i::update_rec*()

    /**\brief Append bytes to a record body.
     * \ingroup SSMFILE
     * \details
     * @param[in] rid  ID of the record to modify.
     * @param[in] data  What to append to the record.
     *
     * \note This appends \b to a record; it does \b not append a record to a file!
     * \sa pin_i::append_rec, \ref SSMPIN
     */
    static rc_t            append_rec(
        const rid_t&             rid, 
        const vec_t&             data
                );

    /**\brief Chop bytes off the end of a record body.
     * \ingroup SSMFILE
     * \details
     * @param[in] rid  ID of the record to modify.
     * @param[in] amount  How many bytes to lop off.
     *
     * \sa pin_i::truncate_rec, \ref SSMPIN
     */
    static rc_t            truncate_rec(
        const rid_t&             rid, 
        smsize_t                 amount
    );

    /**\brief Chop bytes off the end of a record body.
     * \ingroup SSMFILE
     * \details
     * @param[in] rid  ID of the record to modify.
     * @param[in] amount  How many bytes to lop off.
     * @param[out] should_forward  Returns true if the record started out
     * large but is now small as a result of the truncation.  
     * This enables a value-added server to take action in this event,
     * should it so desire.
     *
     * \sa pin_i::truncate_rec, \ref SSMPIN
     */
    static rc_t            truncate_rec(
        const rid_t&             rid, 
        smsize_t                 amount,
        bool&                    should_forward 
    );

#ifdef OLDSORT_COMPATIBILITY
    /* old sort physical version */
    static rc_t            sort_file(
        const stid_t&             fid, 
        vid_t                     vid, 
        stid_t&                   sfid, 
        store_property_t          property,
        const key_info_t&         key_info, 
        int                       run_size,
        bool                      ascending = true,
        bool                      unique = false,
        bool                      destructive = false,
        bool                      use_new_sort = true);

    /* compatibility func for old sort physical -> new sort physical */
    static rc_t            new_sort_file(
        const stid_t&             fid, 
        vid_t                     vid, 
        stid_t&                   sfid, 
        store_property_t          property,
        const key_info_t&         key_info, 
        int                       run_size,
        bool                      ascending = true,
        bool                      unique = false,
        bool                      destructive = false
        );
#endif /* OLDSORT_COMPATIBILITY */

    /* new sort physical version : see notes below */
    static rc_t            sort_file(
        const stid_t&            fid,     // input file
        const stid_t&            sorted_fid, // output file -- 
        int                      nvids,    // array size for vids
        const vid_t*             vid,     // array of vids for temp
                        // files
                        // created by caller--
                        // can be same as input file
        sort_keys_t&            kl, // kl &
        smsize_t                min_rec_sz, // for estimating space use
        int                     run_size,   // # pages to use for a run
        int                     temp_space // # pages VM to use for scratch 
    );


    static rc_t            lvid_to_vid(
        const lvid_t&          lvid,
        vid_t&                 vid);

    static rc_t            vid_to_lvid(
        vid_t                  vid,
        lvid_t&                lvid);

    /*****************************************************************
     * Locking related functions
     *
     * NOTE: there are standard conversions from lpid_t, rid_t, and
     *       stid_t to lockid_t, so wherever a lockid_t parameter is
     *         specified a lpid_t, rid_t, or stid_t can be used.
     *
     *****************************************************************/

    /* enable/disable SLI globally for all threads created after this
       point. Does *NOT* disable SLI for existing threads.
     */
    static void			set_sli_enabled(bool enabled);
    static void			set_elr_enabled(bool enabled);

    static rc_t			set_log_features(char const* features);
    static char const* 		get_log_features();

    static rc_t            lock(
        const lockid_t&         n, 
        lock_mode_t             m,
        lock_duration_t         d = t_long,
        timeout_in_ms           timeout = WAIT_SPECIFIED_BY_XCT
    );
    
    static rc_t            unlock(const lockid_t& n);

    static rc_t            dont_escalate(
        const lockid_t&           n,
        bool                      passOnToDescendants = true
    );

    static rc_t            get_escalation_thresholds(
        w_base_t::int4_t&        toPage,
        w_base_t::int4_t&        toStore,
        w_base_t::int4_t&        toVolume);

    static rc_t            set_escalation_thresholds(
        w_base_t::int4_t       toPage,
        w_base_t::int4_t       toStore,
        w_base_t::int4_t       toVolume);

    static rc_t            query_lock(
        const lockid_t&        n, 
        lock_mode_t&           m,
        bool                   implicit = false
    );

    /*****************************************************************
     * Lock Cache related functions
     *
     * Each transaction has a cache of recently acquired locks
     * The following functions control the use of the cache.
     * Note that the functions affect the transaction currently
     * associated with the thread.
     *****************************************************************/
    // turn on(enable=true) or  off/(enable=false) the lock cache 
    // return previous state.
    static rc_t            set_lock_cache_enable(bool enable);

    // return whether lock cache is enabled
    static rc_t            lock_cache_enabled(bool& enabled);

private:

    static int _instance_cnt;
    static option_group_t* _options;
    static option_t* _hugetlbfs_path;
    static option_t* _reformat_log;
    static option_t* _prefetch;
    static option_t* _bufpoolsize;
    static option_t* _locktablesize;
    static option_t* _logdir;
    static option_t* _logsize;
    static option_t* _logbufsize;
    static option_t* _error_log;
    static option_t* _error_loglevel;
    static option_t* _lockEscalateToPageThreshold;
    static option_t* _lockEscalateToStoreThreshold;
    static option_t* _lockEscalateToVolumeThreshold;
    static option_t* _cc_alg_option;
    static option_t* _log_warn_percent;

    static option_t* _num_page_writers;


    static rc_t            _set_option_logsize(
        option_t*              opt,
        const char*            value,
        ostream*               err_stream);
    
    static rc_t            _set_option_lock_escalate_to_page(
        option_t*              opt,
        const char*            value,
        ostream*               err_stream);
    
    static rc_t            _set_option_lock_escalate_to_store(
        option_t*              opt,
        const char*            value,
        ostream*               err_stream);
    
    static rc_t            _set_option_lock_escalate_to_volume(
        option_t*              opt,
        const char*            value,
        ostream*               err_stream);
    
    static rc_t            _set_store_property(
        stid_t                stid,
        store_property_t      property);

    static rc_t            _get_store_property(
        stid_t                stid,
        store_property_t&     property);

    static rc_t         _begin_xct(
        sm_stats_info_t*      stats,  // allocated by caller
        tid_t&                tid, 
        timeout_in_ms         timeout);

    static rc_t            _commit_xct(
        sm_stats_info_t*&     stats,
        bool                  lazy,
        lsn_t* plastlsn);

    static rc_t            _prepare_xct(
        sm_stats_info_t*&     stats,
        vote_t&                v);

        // TODO : NANCY: is this for external 2pc?
    static rc_t            _set_coordinator(const server_handle_t &); 
    
    static rc_t            _enter_2pc(const gtid_t &); 
    static rc_t            _force_vote_readonly(); 
    static rc_t            _recover_2pc(const gtid_t &,// in
                                bool    mayblock,
                                tid_t    &    //out -- attached if found(?)
                            );
    static rc_t            _chain_xct(
        sm_stats_info_t*&      stats,
        bool                   lazy);

    static rc_t            _abort_xct(
        sm_stats_info_t*&      stats);

    static rc_t            _save_work(sm_save_point_t& sp);

    static rc_t            _rollback_work(const sm_save_point_t&        sp);
    static rc_t            _mount_dev(
        const char*            device,
        u_int&                 vol_cnt,
        vid_t                  local_vid);

    static rc_t            _dismount_dev(
        const char*            device,
        bool                   dismount_if_locked = true
    );
    static rc_t            _create_vol(
        const char*            device_name,
        const lvid_t&          lvid,
        smksize_t              quota_KB,
        bool                   skip_raw_init,
        const bool             apply_fake_io_latency,
        const int              fake_disk_latency);

    static rc_t            _create_index(
        vid_t                 vid, 
        ndx_t                 ntype, 
        store_property_t      property,
        const char*           key_desc,
        concurrency_t         cc,
        stid_t&               stid
    );

    static rc_t            _destroy_index(const stid_t& iid); 

    static rc_t            _get_store_info( 
        const stid_t  &       stid, 
        sm_store_info_t&      info);

    static rc_t            _bulkld_index(
        const stid_t&         stid,
        int                   nsrcs,
        const stid_t*         source,
        sm_du_stats_t&        stats,
        bool                  sort_duplicates = true,
        bool                  lexify_keys = true
    );

    static rc_t            _bulkld_index(
        const stid_t&          stid, 
        sort_stream_i&         sorted_stream,
        sm_du_stats_t&         stats
    );

    static rc_t            _print_index(const stid_t &iid);

    static rc_t            _create_assoc(
        const stid_t  &        stid, 
        const vec_t&           key, 
        const vec_t&           el
#ifdef SM_DORA
        , const bool             bIgnoreLocks = false
#endif
    );

    static rc_t            _destroy_assoc(
        const stid_t &        stid, 
        const vec_t&          key,
        const vec_t&          el
#ifdef SM_DORA
        , const bool             bIgnoreLocks = false
#endif
    );

    static rc_t            _destroy_all_assoc(
        const stid_t&        stid, 
        const vec_t&         key,
        int&                 num_removed
    );
    static rc_t            _find_assoc(
        const stid_t&        stid, 
        const vec_t&         key, 
        void*                el, 
        smsize_t&            elen, 
        bool&                found
#ifdef SM_DORA
        , const bool             bIgnoreLocks = false
#endif
    );

    // below method overloaded for rtree
    static rc_t            _create_md_index(
        vid_t                 vid, 
        ndx_t                 ntype, 
        store_property_t      property,
        stid_t&               stid, 
        int2_t                dim=2
    );

    static rc_t            _destroy_md_index(const stid_t& iid);

    static rc_t            _destroy_md_assoc(
        stid_t                stid,
        const nbox_t&         key,
        const vec_t&          el);

    static rc_t            _bulkld_md_index(
        const stid_t&         stid, 
        int                   nsrcs,
        const stid_t*         source, 
        sm_du_stats_t&        stats,
        int2_t                hff,           // for rtree only
        int2_t                hef,           // for rtree only
        nbox_t*               universe);// for rtree only

    static rc_t            _bulkld_md_index(
        const stid_t&         stid, 
        sort_stream_i&        sorted_stream,
        sm_du_stats_t&        stats,
        int2_t                hff,           // for rtree only
        int2_t                hef,           // for rtree only
        nbox_t*               universe);// for rtree only

    static rc_t            _print_md_index(stid_t stid);

    static rc_t            _create_md_assoc(
        stid_t                stid, 
        const nbox_t&         key,
        const vec_t&          el);

    static rc_t            _find_md_assoc(
        stid_t                stid, 
        const nbox_t&         key, 
        void*                 el, 
        smsize_t&             elen, 
        bool&                 found);

    //
    // The following functions deal with files of records.
    //
    static rc_t            _destroy_n_swap_file(
        const stid_t&         old_fid,
        const stid_t&         new_fid);

    static rc_t            _create_file(
        vid_t                 vid, 
        stid_t&               fid,
        store_property_t     property,
        shpid_t              cluster_hint = 0
    ); 

    static rc_t            _destroy_file(const stid_t& fid); 

    // if "forward_alloc" is true, then scanning for a new free extent, in the
    // case where file needs to grow to accomodate the new record, is done by
    // starting ahead of the currently last extent of the file, instead of the
    // beginning of the volume (i.e. extents are allocated in increasing
    // numerical order).
    static rc_t            _create_rec(
        const stid_t&            fid, 
        const vec_t&             hdr, 
        smsize_t                 len_hint, 
        const vec_t&             data, 
        rid_t&                   new_rid,
        bool                     forward_alloc = true
#ifdef SM_DORA
        , const bool             bIgnoreLocks = false
#endif
        ); 

    static rc_t            _destroy_rec(
        const rid_t&             rid
#ifdef SM_DORA
        , const bool             bIgnoreLocks = false
#endif
        );

    static rc_t            _update_rec(
        const rid_t&             rid, 
        smsize_t                 start, 
        const vec_t&             data
#ifdef SM_DORA
        , const bool             bIgnoreLocks = false
#endif
        );

    static rc_t            _update_rec_hdr(
        const rid_t&             rid, 
        smsize_t                 start, 
        const vec_t&             hdr
#ifdef SM_DORA
        , const bool             bIgnoreLocks = false
#endif
        );

    static rc_t            _append_rec(
        const rid_t&             rid, 
        const vec_t&             data
        );

    static rc_t            _truncate_rec(
            const rid_t&         rid, 
            smsize_t             amount,
            bool&                should_forward
        );

    static rc_t            _draw_rtree(const stid_t& stid, ostream &);

    static rc_t            _rtree_stats(
            const stid_t&       stid,
            rtree_stats_t&      stat,
            uint2_t             size,
            uint2_t*            ovp,
            bool                audit
        );

#ifdef OLDSORT_COMPATIBILITY
    /* old sort internal, physical */
    static rc_t            _sort_file(
        const stid_t&           fid, 
        vid_t                   vid, 
        stid_t&                 sfid, 
        store_property_t        property,
        const key_info_t&       key_info, 
        int                     run_size,
        bool                    ascending,
        bool                    unique,
        bool                    destructive
    );
#endif /* OLDSORT_COMPATIBILITY */

    /* new sort internal, physical */
    static rc_t            _sort_file(
        const stid_t&             fid,     // input file
        const stid_t&             sorted_fid, // output file -- 
                        // created by caller--
                        // can be same as input file
        int                      nvids,    // array size for vids
        const vid_t*             vid,     // array of vids for temp
        sort_keys_t&             kl,     // key location info &
        smsize_t                 min_rec_sz, // for estimating space use
        int                      run_size,   // # pages to use for a run
        int                      temp_space //# pages VM to use for scratch 
    );


#ifdef OLDSORT_COMPATIBILITY
    /* internal compatibility old sort-> new sort */
    static rc_t            _new_sort_file(
            const stid_t&         in_fid, 
            const stid_t&         out_fid, 
            const key_info_t&    ki, 
            int                  run_size,
            bool                  ascending, 
            bool                  unique, 
            bool                  keep_orig //!destructive
            ); 
#endif /* OLDSORT_COMPATIBILITY */

    static store_flag_t     _make_store_flag(store_property_t property);
    // reverse function:
    // static store_property_t    _make_store_property(w_base_t::uint4_t flag);
    // is in dir_vol_m

    // this is for df statistics  DU DF
    static rc_t            _get_du_statistics(
        vid_t                  vid, 
        sm_du_stats_t&         du,
        bool                   audit);

    static rc_t            _get_du_statistics(
        const stid_t  &        stid, 
        sm_du_stats_t&         du,
        bool                   audit);

    static rc_t            _get_volume_meta_stats(
        vid_t                  vid,
        SmVolumeMetaStats&     volume_stats,
        concurrency_t          cc);

    static rc_t            _get_file_meta_stats(
        vid_t                  vid,
        w_base_t::uint4_t      num_files,
        SmFileMetaStats*       file_stats,
        bool                   batch_calculate,
        concurrency_t          cc);
};

class sm_store_info_t {
public:
    NORET sm_store_info_t(int len) :
                store(0), stype(ss_m::t_bad_store_t), 
                ntype(ss_m::t_bad_ndx_t), cc(ss_m::t_cc_bad),
                eff(0), large_store(0), root(0),
                nkc(0), keydescrlen(len)
                {  keydescr = new char[len]; }

    NORET ~sm_store_info_t() { if (keydescr) delete[] keydescr; }

    snum_t    store;        // store id
    u_char    stype;        // store_t: t_index, t_file, ...
    u_char    ntype;        // ndx_t: t_btree, t_rtree, ...
    u_char    cc;         // concurrency_t on index: t_cc_kvl, ...
    u_char    eff;        // unused

    snum_t    large_store;    // store for large record pages of t_file
    shpid_t    root;        // root page (of index)
    w_base_t::uint4_t    nkc;        // # components in key

    int        keydescrlen;    // size of array below
    char        *keydescr;    // variable length string:
                // he who creates a sm_store_info_t
                // for use with get_store_info()
                // is responsible for allocating enough
                // space for longer key descriptors, if
                // he expects to find them.
};


ostream& operator<<(ostream& o, const vid_t& v);
istream& operator>>(istream& i, vid_t& v);
ostream& operator<<(ostream& o, const extid_t& x);
istream& operator>>(istream& o, extid_t &x);
ostream& operator<<(ostream& o, const stid_t& stid);
istream& operator>>(istream& i, stid_t& stid);
ostream& operator<<(ostream& o, const lpid_t& pid);
istream& operator>>(istream& i, lpid_t& pid);
ostream& operator<<(ostream& o, const shrid_t& r);
istream& operator>>(istream& i, shrid_t& r);
ostream& operator<<(ostream& o, const rid_t& rid);
istream& operator>>(istream& i, rid_t& rid);
ostream& operator<<(ostream& o, const sm_stats_info_t& s);
template<class ostream>
ostream& operator<<(ostream& o, const sm_config_info_t& s)
{
    o    << "  page_size " << s.page_size
     << "  max_small_rec " << s.max_small_rec
     << "  lg_rec_page_space " << s.lg_rec_page_space
     << "  buffer_pool_size " << s.buffer_pool_size
     << "  max_btree_entry_size " << s.max_btree_entry_size
     << "  exts_on_page " << s.exts_on_page
     << "  pages_per_ext " << s.pages_per_ext
     << "  multi_threaded_xct " << s.multi_threaded_xct
     << "  logging " << s.logging
      ;
    return o;
}


#ifndef VEC_T_H
#include <vec_t.h>
#endif

#ifndef SM_ESCALATION_H
#include <sm_escalation.h>
#endif

/*<std-footer incl-file-exclusion='SM_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
