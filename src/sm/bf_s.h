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

/*<std-header orig-src='shore' incl-file-exclusion='BF_S_H'>

 $Id: bf_s.h,v 1.42.2.8 2010/03/19 22:20:23 nhall Exp $

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

#ifndef BF_S_H
#define BF_S_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#include <sm_s.h>

class page_s;

class bfpid_t : public lpid_t {
public:
    NORET            bfpid_t();
    NORET            bfpid_t(const lpid_t& p);
    bfpid_t&             operator=(const lpid_t& p);
    bool                 operator==(const bfpid_t& p) const;
    static const bfpid_t    null;
};

inline NORET
bfpid_t::bfpid_t()
{
}

inline NORET
bfpid_t::bfpid_t(const lpid_t& p) : lpid_t(p)
{
}

inline bfpid_t&
bfpid_t::operator=(const lpid_t& p)  
{
    *(lpid_t*)this = p;
    return *this;
}

inline bool 
bfpid_t::operator==(const bfpid_t& p) const
{
    return vol() == p.vol() && page == p.page;
}


/*
 *  bfcb_t: buffer frame control block.
 */
class bfcb_t {
    friend class bfcb_unused_list;
private:
    int4_t volatile     _pin_cnt;        // count of pins on the page
    w_base_t::uint4_t _store_flags; // a copy of what's in the page, alas
    bfpid_t     _pid;            // page currently stored in the frame
    bfpid_t     _old_pid;        // previous page in the frame
    bool        _old_pid_valid;  // is the previous page in-transit-out?
    page_s*     _frame;          // pointer to the frame

    bool        _dirty;          // true if page is dirty
    lsn_t       _rec_lsn;        // recovery lsn

    bfcb_t*     _next_free;    // used by the (singly-linked) freelist

    int4_t      _refbit;// ref count (for strict clock algorithm)
                // for replacement policy only

    int4_t      _hotbit;// copy of refbit for use by the cleaner algorithm
                // without interfering with clock (replacement)
                // algorithm.

    int4_t       _hash_func; // which hash function was this frame placed with?
    int4_t       volatile    _hash;        // and what was the hash value?
public:
    latch_t     latch;          // latch on the frame

public:
    NORET       bfcb_t()    {};
    NORET       ~bfcb_t()   {};

    void           vtable_collect(vtable_row_t &t);
    static void    vtable_collect_names(vtable_row_t&);

    inline void    clear();

    const page_s *frame() const { return _frame; }
    page_s *frame_nonconst() { return _frame; }
    void  set_storeflags(w_base_t::uint4_t f);
    w_base_t::uint4_t  get_storeflags() const;
    w_base_t::uint4_t  get_storeflags_nocheck() const { return _store_flags; }

    const bfpid_t &pid() const  { return _pid;   }
    void  set_pid(const bfpid_t &p) { _pid = p;   }

    bool  old_pid_valid() const  { return _old_pid_valid;  }
    const bfpid_t &old_pid() const  { return _old_pid;   }
    void  clr_old_pid() { _old_pid = lpid_t::null; _old_pid_valid=false;  }
    void  set_old_pid() { _old_pid = pid();  _old_pid_valid=true; }

    bool  dirty() const  { return _dirty;  }
    void  set_dirty_bit(bool tf) { _dirty = tf;  }

    const lsn_t&  rec_lsn() const  { return _rec_lsn;  }
    void  set_rec_lsn(const lsn_t &what) { 
        // This assert is clearly not always true but I need to 
        // identify the cases.
                          w_assert0(latch.is_mine()); 
                        _rec_lsn = what;  }
    void  clr_rec_lsn() { _rec_lsn = lsn_t::null;  }


    int4_t      refbit() const { return _refbit; }
    void        set_refbit(uint4_t b) { _refbit=b; }
    void        decr_refbit() { _refbit--; }

    int4_t      hotbit() const { return _hotbit; }
    uint4_t     set_hotbit(int4_t b) { return (_hotbit = b); }
    void        decr_hotbit() { _hotbit--; }

    void        update_rec_lsn(latch_mode_t);

    void        initialize(const char *const _name,
                        page_s*           _bufpool,
                        uint4_t           hashfunc
                        );
    int4_t       hash_func() const { return _hash_func; }
    void         set_hash_func(int4_t h) { _hash_func=h; }
    int4_t       volatile    hash() const { return _hash;}
    void         set_hash(int4_t h) { _hash=h;}


public:
    inline ostream&    print_frame(ostream& o, bool in_htab);
    void         unpin_frame();
    void         pin_frame();
    bool         pin_frame_if_pinned();
#if W_DEBUG_LEVEL > 1
    void         check() const;
#else
    void         check() const {}  // no-op
#endif

    // I'm making _pin_cnt private just so I can be sure all updates
    // are through the right methods.
    void                zero_pin_cnt() { _pin_cnt=0; }
    int4_t  volatile    pin_cnt() const { return _pin_cnt; }

    // is_hot: is someone waiting for the latch?
    // NOTE: this is somewhat racy, in that it returns false negatives
    //  but if when it returns true, there is or just was a hot state
    //  on this page.    This is only used now for debugging purposes.
    bool                is_hot() const { return 
                                    latch.latch_cnt() < _pin_cnt; }
private:

    // disabled
    NORET       bfcb_t(const bfcb_t&);
    bfcb_t&     operator=(const bfcb_t&);
};

inline void
bfcb_t::clear() 
{
    _pid = lpid_t::null;
    _old_pid = lpid_t::null;
    _old_pid_valid = false;
    _dirty = false;
    _rec_lsn = lsn_t::null;
    _hotbit = 0;
    _refbit = 0;
    w_assert3(pin_cnt() == 0);
    w_assert3(latch.num_holders() <= 1);
}

/*<std-footer incl-file-exclusion='BF_S_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
