/*<std-header orig-src='shore' incl-file-exclusion='W_H'>

 $Id: w.h,v 1.19.2.5 2010/03/19 22:17:19 nhall Exp $

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


/** \mainpage SHORE Storage Manager: The Multi-Threaded Version 
 * \section Brief Description
 *
 * This is an experiment test-bed library for use by researchers who wish to
 * write multi-threaded software that manages persistent data.   
 *
 * This storage engine provides the following capabilities:
 *  - transactions with ACID properties, with ARIES-based logging and recovery,
 *  primitives for partial rollback,
 *  transaction chaining, and early lock release,
 *  - prepared-transaction support for two-phased commit,
 *  - persistent storage structures : 
 *  B+ tree indexes, R* trees (spatial indexes), and files of untyped records,
 *  - fine-grained locking for records and B+ tree indexes with deadlock detection,
 *  optional lock escalation and optional coarse-grained locking, 
 *  - in-memory buffer management with optional prefetching, 
 *  - extensible statistics-gathering, option-processing, and error-handling 
 *  facilities.
 *
 * This software runs on Pthreads, thereby providing its client software
 * (e.g., a database server) multi-threading
 * capabilities and resulting scalability from modern SMP and NUMA 
 * architectures, and has been used on Linux/x86-64 and Solaris/Niagara
 * architectures.
 *
 * \section Background
 *
 * The SHORE (Scalable Heterogeneous Object REpository) project
 * at the University of Wisconsin - Madison Department of Computer Sciences
 * produced the first release
 * of this storage manager as part of the full SHORE release in 1996.
 * The storage manager portion of the SHORE project was used by 
 * other projects at the UW and elsewhere, and was intermittently
 * maintained through 2008.
 *
 * The SHORE Storage Manager was originally developed on single-cpu Unix-based systems,
 * providing support for "value-added" cooperating peer servers, one of which was the
 * SHORE Value-Added Server (http://www.cs.wisc.edu/shore), and another of which was 
 * Paradise (http://www.cs.wisc.edu/paradise) at the University of Wisconsin;
 * TIMBER (http://www.eecs.umich.edu/db/timber) and 
 * Persicope (http://www.eecs.umich.edu/persiscope) at the University of Michigan,
 * and
 * PREDATOR (http://www.distlab.dk/predator) at Cornell.
 * (The storage manager was also used for innumerable published studies.)
 *
 * The storage manager had its own "green threads" and communications
 * layers, and until 2007, its code structure, nomenclature, and contents reflected
 * its SHORE roots.
 *
 * In 2007, the Data Intensive Applications and Systems Labaratory (DIAS)
 * at Ecole Polytechnique Federale de Lausanne
 * began work on a port of release 5.0.1 of the storage manager to Pthreads,
 * and developed more scalable synchronization primitives, identified
 * bottlenecks in the storage manager, and improved the scalability of 
 * the code.
 * This work was on a Solaris/Niagara platform and was released as Shore-MT
 * http://diaswww.epfl.ch/shore-mt).
 * It was a partial port of the storage manager and did not include documentation.
 * Projects using Shore-MT include
 * StagedDB/CMP (http://www.cs.cmu.edu/~stageddb/),
 * Lachesis (http://www.vldb.org/conf/2003/papers/S21P03.pdf), 
 * and
 * DORA (http://www.cs.cmu.edu/~ipandis/resources/CMU-CS-10-101.pdf)
 *
 * In 2009, the University of Wisconsin - Madison took the first Shore-MT
 * release and ported the remaining code to Pthreads.
 * This work as done on a Red Hat Linux/x86-64 platform.
 * This release is the result of that work, and includes this documentation,
 * bug fixes, and supporting test code.   Some of the scalability changes
 * of the DIAS
 * release has been removed in order to correct bugs, with the hope that 
 * further work will improve scalability of the completed port.
 *
 * \section Copyrights
 * Most of this library and documentation is subject one or both of the following copyrights:
 *
 *  -       \b SHORE -- \b Scalable \b Heterogeneous \b Object \b REpository
 *
 *  Copyright (c) 1994-2007 Computer Sciences Department, University of
 *                      Wisconsin -- Madison
 *                      All Rights Reserved.
 *
 *  Permission to use, copy, modify and distribute this software and its
 *  documentation is hereby granted, provided that both the copyright
 *  notice and this permission notice appear in all copies of the
 *  software, derivative works or modified versions, and any portions
 *  thereof, and that both notices appear in supporting documentation.
 *
 *  THE AUTHORS AND THE COMPUTER SCIENCES DEPARTMENT OF THE UNIVERSITY
 *  OF WISCONSIN - MADISON ALLOW FREE USE OF THIS SOFTWARE IN ITS
 *  "AS IS" CONDITION, AND THEY DISCLAIM ANY LIABILITY OF ANY KIND
 *  FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 *  This software was developed with support by the Advanced Research
 *  Project Agency, ARPA order number 018 (formerly 8230), monitored by
 *  the U.S. Army Research Laboratory under contract DAAB07-91-C-Q518.
 *  Further funding for this work was provided by DARPA through
 *  Rome Research Laboratory Contract No. F30602-97-2-0247.
 *
 * -   \b Shore-MT -- \b Multi-threaded \b port \b of \b the \b SHORE \b Storage \b Manager
 *  
 *                     Copyright (c) 2007-2009
 *        Data Intensive Applications and Systems Labaratory (DIAS)
 *                 Ecole Polytechnique Federale de Lausanne
 *  
 *                       All Rights Reserved.
 *  
 *  Permission to use, copy, modify and distribute this software and
 *  its documentation is hereby granted, provided that both the
 *  copyright notice and this permission notice appear in all copies of
 *  the software, derivative works or modified versions, and any
 *  portions thereof, and that both notices appear in supporting
 *  documentation.
 *  
 *  This code is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. THE AUTHORS
 *  DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER
 *  RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * \section START Getting Started With the Shore Storage Manager
 * A good place to start is with \ref SSMAPI.
 */

/**\addtogroup OPT
 *\section CONFIGOPT Configuration Options
 * \verbatim
SHORE-specific Features:
  --enable-lp64         default:yes     Compile to use LP 64 data model
                                        No other data model is supported yet.
                                        But we hope some day to port back to LP32.
  --enable-checkrc      default:no      Generate (expensive) code to verify return-code checking
                                        If a w_rc_t is set but not checked with
                                        method is_error(), upon destruction the
                                        w_rc_t will print a message to the effect
                                        "error not checked".
  --enable-trace        default:no      Include tracing code
                                        Run-time costly.  Good for debugging
                                        problems that are not timing-dependent.
                                        Use with DEBUG_FLAGS and DEBUG_FILE
                                        environment variables.  See \ref SSMTRACE.
  --enable-dbgsymbols   default:no      Turn on debugger symbols
                                        Use this to override what a given
                                        debugging level will normally do.
  --enable-explicit     default:no      Compile with explicit templates
                                        NOT TESTED. \todo explicit templates

  --enable-valgrind     default:no      Enable running valgrind run-time behavior
                                        Includes some code for valgrind.
  --enable-purify       default:no      Enable build of <prog>.pure
  --enable-quantify     default:no      Enable build of <prog>.quant
  --enable-purecov      default:no      Enable build of <prog>.purecov

SHORE-specific Optional Packages:
  --with-hugetlbfs        Use the hugetlbfs for the buffer pool.
                          Depending on the target architecture, this might
                          be useful.  If you use it, you will need to set
                          a path for your hugetlbfs in config/shore.def.
                          The default is :
                          #define HUGETLBFS_PATH "/mnt/huge/SSM-BUFPOOL"
  --without-mmap          Do not use mmap for the buffer pool. Trumps
                          hugetlbfs option.
  --with-debug-level1     Include level 1 debug code, optimize.
                          This includes code in w_assert1 and 
                          #if W_DEBUG_LEVEL > 0 /#endif pairs and 
                          #if W_DEBUG_LEVEL >= 1 /#endif pairs and  and
                          W_IFDEBUG1
  --with-debug-level2     Include level 2 debug code, no optimize.
                          Equivalent to debug level 1 PLUS
                          code in w_assert2 and
                          #if W_DEBUG_LEVEL > 1 /#endif pairs and
                          #if W_DEBUG_LEVEL >= 2 /#endif pairs and
                          W_IFDEBUG2
  --with-debug-level3     Include level 3 debug code, no optimize.
                          Equivalent to debug level 2 PLUS
                          includes code in w_assert3  and
                          #if W_DEBUG_LEVEL > 2 /#endif pairs and
                          #if W_DEBUG_LEVEL >= 3 /#endif pairs and
                          W_IFDEBUG3
 \endverbatim
 */

/**\addtogroup OPT
 * \section SHOREDEFOPT Build options in config/shore.def
 * In this section we describe C preprocessor macros defined (or not) in
 * config/shore.def.
 *
 * -HUGETLBFS_PATH See --with-hugetlbfs in \ref CONFIGOPT
 *
 * -USE_SSMTEST Define this if you want to include crash test hooks in your
 *
 * -DONT_TRUST_PAGE_LSN -- this is a deprecated option.
 * executable.  (For use with ssh, really).
 * A release executable should not be built with this unless you
 * are a maintainer and want to test it. It is not documented.
 *
 * -VOL_LOCK_IS_RW  Undefine this if you want to restore the non-r-w mutex for
 * the volume (vol_t) structure.
 * The volume mutex was replaced with a read-write lock because 
 * statistics showed that the vast majority of the times we grab the mutex, it
 * is for read-operations. 
 * Having changed this to a read-write lock, we now see that
 * <10% of the time does the read-lock request have to wait,
 * and roughly 40% of the time the write-lock request has to wait.  
 * As this code is still somewhat experimental, the option to turn it off
 * remains, for the time being.
 */

/**\defgroup SSMAPI SHORE Storage Manager Application Programming Interface (SSM API)
 */
/**\defgroup MISC Miscellaneous
 */
/**\defgroup Macros Significant macros 
 * \ingroup MISC
 *
 * Not all macros are included here.
 * */
/**\defgroup UNUSED Unused code 
 * \ingroup MISC
 */

#ifndef W_H
#define W_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#include <w_base.h>
#include <w_minmax.h>
#include <w_list.h>
#include <w_hash.h>

#ifdef W_INCL_BITMAP
#include <w_bitmap.h>
#endif

/*<std-footer incl-file-exclusion='W_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
