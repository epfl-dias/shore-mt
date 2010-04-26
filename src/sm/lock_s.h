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

/*<std-header orig-src='shore' incl-file-exclusion='LOCK_S_H'>

 $Id: lock_s.h,v 1.69.2.8 2010/03/19 22:20:24 nhall Exp $

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

#ifndef LOCK_S_H
#define LOCK_S_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif

class lock_base_t : public smlevel_1 {
public:
    // Their order is significant.

    enum status_t {
        t_no_status = 0,
        t_granted = 1,
        t_converting = 2,
        t_waiting = 4
    };

    typedef lock_mode_t lmode_t;

    typedef lock_duration_t duration_t;

    enum {
        MIN_MODE = NL, MAX_MODE = EX,
        NUM_MODES = MAX_MODE - MIN_MODE + 1
    };

    static const char* const         mode_str[NUM_MODES];
    static const char* const         duration_str[t_num_durations];
    static const bool                compat[NUM_MODES][NUM_MODES];
    static const lmode_t             supr[NUM_MODES][NUM_MODES];
};

#ifndef LOCK_S
/*
typedef lock_base_t::duration_t lock_duration_t;
// typedef lock_base_t::lmode_t lock_mode_t; lock_mode_t defined in basics.h
typedef lock_base_t::status_t status_t;

#define LOCK_NL         lock_base_t::NL
#define LOCK_IS         lock_base_t::IS
#define LOCK_IX         lock_base_t::IX
#define LOCK_SH         lock_base_t::SH
#define LOCK_SIX        lock_base_t::SIX
#define LOCK_UD         lock_base_t::UD
#define LOCK_EX         lock_base_t::EX

#define LOCK_INSTANT         lock_base_t::t_instant
#define LOCK_SHORT         lock_base_t::t_short
#define LOCK_MEDIUM        lock_base_t::t_medium
#define LOCK_LONG         lock_base_t::t_long
#define LOCK_VERY_LONG        lock_base_t::t_very_long
*/
#endif

class lockid_t {
public:
    //
    // The lock graph consists of 6 nodes: volumes, stores, pages, key values,
    // records, and extents. The first 5 of these form a tree of 4 levels.
    // The node for extents is not connected to the rest. The name_space_t
    // enumerator maps node types to integers. These numbers are used for
    // indexing into arrays containing node type specific info per entry (e.g
    // the lock caches for volumes, stores, and pages).
    //
    enum { NUMNODES = 6 };
    // The per-xct cache only caches volume, store, and page locks.
    enum { NUMLEVELS = 4 };

    enum name_space_t { // you cannot change these values with impunity
        t_bad                = 10,
        t_vol                = 0,
        t_store              = 1,  // parent is 1/2 = 0 t_vol
        t_page               = 2,  // parent is 2/2 = 1 t_store
        t_kvl                = 3,  // parent is 3/2 = 1 t_store
        t_record             = 4,  // parent is 4/2 = 2 t_page
        t_extent             = 5,
        t_user1              = 6,
        t_user2              = 7,  // parent is t_user1
        t_user3              = 8,  // parent is t_user2
        t_user4              = 9   // parent is t_user3
    };

    struct user1_t  {
        uint2_t                u1;
        user1_t() : u1(0)  {}
        user1_t(uint2_t v1) : u1(v1)  {}
    };

    struct user2_t : public user1_t  {
        uint4_t                u2;
        user2_t() : u2(0)  {}
        user2_t(uint2_t v1, uint4_t v2): user1_t(v1), u2(v2)  {}
    };

    struct user3_t : public user2_t  {
        uint4_t                u3;
        user3_t() : u3(0)  {}
        user3_t(uint2_t v1, uint4_t v2, uint4_t v3)
                : user2_t(v1, v2), u3(v3)  {}
    };

    struct user4_t : public user3_t  {
        uint4_t                u4;
        user4_t() : u4(0)  {}
        user4_t(uint2_t v1, uint4_t v2, uint4_t v3, uint4_t v4)
                : user3_t(v1, v2, v3), u4(v4)  {}
    };

private:
    union {
        // 
        w_base_t::uint8_t l[2]; 
                      // l[0,1] are just to support the hash function.

        w_base_t::uint4_t w[4]; 
                      // w[0] == s[0,1] see below
                      // w[1] == store or extent or user2
                      // w[2] == page or user3
                      // w[3] contains slot (in both s[6] and s[7]) or user4
        w_base_t::uint2_t s[8]; 
                      // s[0] high byte == lspace (lock type)
                      // s[0] low byte == boolean used for extent-not-freeable
                      // s[1] vol or user1
                      // s[2,3]==w[1] 
                      // s[4,5]== w[2] see above
                      // s[6] == slot
                      // s[7] == slot copy 
    };

public:
    bool operator<(lockid_t const &p) const;
    bool operator==(const lockid_t& p) const;
    bool operator!=(const lockid_t& p) const;
    friend ostream& operator<<(ostream& o, const lockid_t& i);
public:
    uint4_t           hash() const; // used by lock_cache
    void              zero();

private:

    //
    // lspace -- contains enum for type of lockid_t
    //
public:
    name_space_t      lspace() const;
private:
    void              set_lspace(lockid_t::name_space_t value);
    uint1_t           lspace_bits() const;

    //
    // vid - volume
    //
public:
    vid_t             vid() const;
private:
    void              set_vid(const vid_t & v);
    uint2_t           vid_bits() const;

    //
    // store - stid
    //
public:
    const snum_t&     store() const;
private:
    void              set_snum(const snum_t & s);
    void              set_store(const stid_t & s);
    uint4_t           store_bits() const;

    //
    // extent -- used only when lspace() == t_extent
    //
public:
    const extnum_t&   extent() const;
private:
    void              set_extent(const extnum_t & e);
    uint4_t           extent_bits() const;

    //
    // page id
    //
public:
    const shpid_t&    page() const;
private:
    void              set_page(const shpid_t & p);
    uint4_t           page_bits() const ;
    //
    // slot id
    //
public:
    const slotid_t&   slot() const;
private:
    void              set_slot(const slotid_t & e);
    uint2_t           slot_bits() const;
    uint4_t           slot_kvl_bits() const;

    //
    // user1
    //
public:
    uint2_t           u1() const;
private:
    void              set_u1(uint2_t i);

    //
    // user2
    //
public:
    uint4_t           u2() const;
private:
    void              set_u2(uint4_t i);

    //
    // user3
    //
public:
    uint4_t           u3() const;
private:
    void              set_u3(uint4_t i);

    //
    // user4
    //
public:
    uint4_t           u4() const;
private:
    void              set_u4(uint4_t i);

public:

    void              set_ext_has_page_alloc(bool value);
    bool              ext_has_page_alloc() const ;

    NORET             lockid_t() ;    
    NORET             lockid_t(const vid_t& vid);
    NORET             lockid_t(const extid_t& extid);    
    NORET             lockid_t(const stid_t& stid);
    NORET             lockid_t(const lpid_t& lpid);
    NORET             lockid_t(const rid_t& rid);
    NORET             lockid_t(const kvl_t& kvl);
    NORET             lockid_t(const lockid_t& i);        
    NORET             lockid_t(const user1_t& u);        
    NORET             lockid_t(const user2_t& u);        
    NORET             lockid_t(const user3_t& u);        
    NORET             lockid_t(const user4_t& u);        

    void              extract_extent(extid_t &e) const;
    void              extract_stid(stid_t &s) const;
    void              extract_lpid(lpid_t &p) const;
    void              extract_rid(rid_t &r) const;
    void              extract_kvl(kvl_t &k) const;
    void              extract_user1(user1_t &u) const;
    void              extract_user2(user2_t &u) const;
    void              extract_user3(user3_t &u) const;
    void              extract_user4(user4_t &u) const;

    bool              is_user_lock() const;

    void              truncate(name_space_t space);

    lockid_t&         operator=(const lockid_t& i);

};

ostream& operator<<(ostream& o, const lockid_t::user1_t& u);
ostream& operator<<(ostream& o, const lockid_t::user2_t& u);
ostream& operator<<(ostream& o, const lockid_t::user3_t& u);
ostream& operator<<(ostream& o, const lockid_t::user4_t& u);

istream& operator>>(istream& o, lockid_t::user1_t& u);
istream& operator>>(istream& o, lockid_t::user2_t& u);
istream& operator>>(istream& o, lockid_t::user3_t& u);
istream& operator>>(istream& o, lockid_t::user4_t& u);



#if W_DEBUG_LEVEL>0
#else
#include "lock_s_inline.h"
#endif

// This is used in probe() for lock cache
inline w_base_t::uint4_t w_hash(const lockid_t& id)
{
    return id.hash();
}

struct locker_mode_t {
    tid_t        tid;
    lock_mode_t        mode;
};

struct lock_info_t {
    lockid_t                name;
    lock_mode_t             group_mode;
    bool                    waiting;

    lock_base_t::status_t   status;
    lock_mode_t             xct_mode;
    lock_mode_t             convert_mode;
    lock_duration_t         duration;
    int                     count;
};

/*<std-footer incl-file-exclusion='LOCK_S_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
