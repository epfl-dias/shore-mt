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

/*<std-header orig-src='shore' incl-file-exclusion='PAGE_S_H'>

 $Id: page_s.h,v 1.30.2.10 2010/03/19 22:20:24 nhall Exp $

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

#ifndef PAGE_S_H
#define PAGE_S_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif

/* BEGIN VISIBLE TO APP */

/*
 * Basic page structure for all pages.
 */
class xct_t;

class page_s {
public:
    typedef int2_t slot_offset_t; // -1 if vacant
    typedef uint2_t slot_length_t;
    struct slot_t {
        slot_offset_t offset;        // -1 if vacant
        slot_length_t length;
    };
    
    class space_t {
        public:
        space_t()     {};
        ~space_t()    {};
        
        // called on page format, rflag is true for rsvd_mode() pages
        void init_space_t(int nfree, bool rflag)  { 
            _tid = tid_t(0, 0);
            _nfree = nfree;
            _nrsvd = _xct_rsvd = 0;

            _rflag = rflag;
        }
        
        int nfree() const    { return _nfree; }
        // rflag: true for rsvd_mode() pages, that is pages
        // that have space reservation, namely file pages.
        // Space reservation is all about maintaining slot ids.
        bool rflag() const    { return _rflag!=0; }
        
#ifndef BUG_SPACE_FIX
#define BUG_SPACE_FIX
#endif

#ifdef BUG_SPACE_FIX
        int             usable_for_new_slots() const {
                            return nfree() - nrsvd();
                        }
#endif
        int             usable(xct_t* xd); // might free space
                        // slot_bytes means bytes for new slots
        rc_t            acquire(int amt, int slot_bytes, xct_t* xd,
                                        bool do_it=true);
        void            release(int amt, xct_t* xd);
        void            undo_acquire(int amt, xct_t* xd);
        void            undo_release(int amt, xct_t* xd);
        const tid_t&    tid() const { return _tid; }
        int2_t          nrsvd() const { return _nrsvd; }
        int2_t          xct_rsvd() const { return _xct_rsvd; }


        private:
       
        void _check_reserve();
        
        tid_t    _tid;        // youngest xct contributing to _nrsvd
        /* NB: this use of int2_t prevents us from having 65K pages */
#if SM_PAGESIZE >= 65536
#error Page sizes this big are not supported
#endif
        int2_t    _nfree;        // free space counter
        int2_t    _nrsvd;        // reserved space counter
        int2_t    _xct_rsvd;    // amt of space contributed by _tid to _nrsvd
        int2_t    _rflag; 
    }; // space_t

    enum {
        hdr_sz = (0
              + 2 * sizeof(lsn_t) 
              + sizeof(lpid_t)
              + 2 * sizeof(shpid_t) 
              + sizeof(space_t)
              + 4 * sizeof(int2_t)
              + 2 * sizeof(w_base_t::int4_t)
              + 2 * sizeof(slot_t)
#if ALIGNON == 0x8
              + sizeof(fill4)
#endif
              + 0),
        data_sz = (smlevel_0::page_sz - hdr_sz),
        max_slot = data_sz / sizeof(slot_t) + 2
    };

 
    /* NOTE: you can verify these offsets with smsh: enter "sm sizeof" */
    /* offset 0 */
    lsn_t    lsn1;

    /* offset 8 */
    lpid_t    pid;            // id of the page

    /* offset 20 */
    shpid_t    next;            // next page

    /* offset 24 */
    shpid_t    prev;            // previous page

    /* offset 28 */
    uint2_t    tag;            // page_p::tag_t

    /* offset 30 */
    uint2_t    end;            // offset to end of data area

    /* offset 32 */
    space_t     space;            // space management

    /* offset 48 */
    int2_t    nslots;            // number of slots

    /* offset 50 */
    int2_t    nvacant;        // number of vacant slots

    /* offset 52 */
    fill4    _fill0;            // align to 8-byte boundary, alas

    /* offset 56 */
    w_base_t::uint4_t    _private_store_flags;        // page_p::store_flag_t
    w_base_t::uint4_t    get_page_storeflags() const { return _private_store_flags;}
    w_base_t::uint4_t    set_page_storeflags(uint4_t f) { 
                         return (_private_store_flags=f);}

    /* offset 60 */
    w_base_t::uint4_t    page_flags;        // page_p::page_flag_t

#if (ALIGNON == 0x8) && !defined(ARCH_LP64)
    // TODO: NANCY - for 32-bit platform
    // FRJ: this seems to be broken now that lsn_t is always 8 bytes. disabling
    
    /* Yes, the conditions above are  correct.  The default is 8 byte
       alignment.  If compatability mode is wanted, it reverts to 4 ...
       unless 8 byte alignment is specified, in which case we need 
       the filler. */
    /* XXX really want alignment to the aligned object size for
       a particular architecture.   Fortunately this works for all
       common platforms and the above types.  A better solution would
       be to specify a desired alignment, or make 'data' a union which
       contains the type that has the most restrictive alignment
       constraints. */
    fill4    _fill0;            // align to 8-byte boundary
#endif

    /* offset 64 */
    char     data[data_sz];        // must be aligned

    /* offset 8176 */
    slot_t    reserved_slot[1];    // 2nd slot (declared to align
                    // end of _data)
                    //
    /* offset 8180 */
    slot_t    slot[1];        // 1st slot

    /* offset 8184 */
    lsn_t    lsn2;


    void ntoh(vid_t vid);

    // XXX this class is really intended to be a "STRUCT" as a bunch of
    // things layed out in memory.  Unfortunately it includes
    // some _classes_ which makes that a bit difficult.   Why?
    // ... well that means that it may need to be constructed or
    // destructed.   Probably the best way to handle this is to make
    // a static method  which creates page_s instances from a memory
    // buffer ... and destroy them the same way, thence fixing the
    // constructor/destructor problem.   This is a work-around for now.
    page_s() { }
    ~page_s() { }
};

/* END VISIBLE TO APP */

/*<std-footer incl-file-exclusion='PAGE_S_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
