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

/*<std-header orig-src='shore' incl-file-exclusion='HASH_LRU_H'>

 $Id: hash_lru.h,v 1.37.2.3 2009/09/01 21:41:15 nhall Exp $

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

#ifndef HASH_LRU_H
#define HASH_LRU_H

#include "w_defines.h"
#include "w_hash.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifndef W_BASE_H
#include <w_base.h>
#endif
#ifndef STHREAD_H
#include <sthread.h>
#endif
#ifndef LATCH_H
#include <latch.h>
#endif


/*********************************************************************
Template Class: hash_lru_t

hash_lru_t uses hash_t to implement a fixed size and mutex protected
hash table with chaining on collisions.  The maximum number of
entries in the table is set at construction.  When a entry needs to
be added and the table is full, on old entry is removed based on
an LRU policy (this is similar rsrc_m).  All operations on the
table acquire a single mutex before proceeding.

Possible Uses:
    This class is useful where you need multi-threaded (and short)
    access to a fixed size pool of elements.  We use it to
    manage caches of small objects.

Requirements:
    Class T can be of any type
    an == operator must be defined to compare K
    a uint4_t hash(const K&) function must be defined

Implementation issues:
    A table entry, hash_lru_entry_t, contains the key, K, and the
    data, T, associated with the key along with the link_t needed
    by hash_t.  In addition, a reference bit is maintained to
    support the lru clock.

*********************************************************************/

/*
 *  hash_lru_entry_t
 *	control block for a hash_entry contain an element of TYPE
 */
template <class TYPE, class KEY>
struct hash_lru_entry_t {
public:
    w_link_t		link;		// link for use in the hash table
    KEY			key;		// key of the resource
    TYPE 		entry;		// entry put in the table
    bool		ref;		// boolean: ref bit
    
#ifdef GNUG_BUG_7
    ~hash_lru_entry_t() {};
#endif
};

// iterator over the hash table
template <class TYPE, class KEY> class hash_lru_i;


// Hash table (with a fixed number of elements) where 
// replacement is LRU.  The hash table is protected by
// a mutex.
template <class TYPE, class KEY>
class hash_lru_t : public w_base_t 
{
    friend class hash_lru_i<TYPE, KEY>;
public:
    hash_lru_t(int n, const char *descriptor=0);
    ~hash_lru_t();

    // grab entry and keep mutex on table if found
    TYPE* grab(const KEY& k, bool& found, bool& is_new);

    // find entry and keep mutex on table if found
    TYPE* find(const KEY& k, int ref_bit = 1);
    // copy out entry (return true if found) and release mutex on table
    bool find(const KEY& k, TYPE& entry_, int ref_bit = 1);

    // to remove an element, you must have used grab or find
    // and not yet released the mutex.  You must then release it.
    void remove(const TYPE*& t);

    // acquire the table mutex
    void acquire_mutex(sthread_t::timeout_in_ms timeout = 
                                         sthread_t::WAIT_FOREVER) 
	      { _mutex._acquire(timeout); }

    // release mutex obtained by grab, find, remove or acquire_mutex
    void release_mutex() {_mutex.release();}
    bool is_mine() const {return _mutex.is_mine();}
   
    void dump();
    int size() {return _total;}  // size of the hash table
    unsigned	ref_cnt;
    unsigned	hit_cnt;
private:
    mutable smutex_t _mutex; /* initialized with descriptor by constructor */

    void _remove(const TYPE*& t);
    hash_lru_entry_t<TYPE, KEY>* _replacement();
    bool _in_htab(hash_lru_entry_t<TYPE, KEY>* e);

    hash_lru_entry_t<TYPE, KEY>* _entry;

    w_hash_t< hash_lru_entry_t<TYPE, KEY>, KEY> _htab;

    w_list_t< hash_lru_entry_t<TYPE, KEY> > _unused;

    int _clock;
    const int _total;

    // disabled
    hash_lru_t(const hash_lru_t&);
    hash_lru_t &operator=(const hash_lru_t&);
};

template <class TYPE, class KEY>
class hash_lru_i {
public:
    hash_lru_i(hash_lru_t<TYPE, KEY>& h, int start = 0)
    : _idx(start), _curr(0), _h(h)
	{W_COERCE( _h._mutex.acquire());};
    ~hash_lru_i();
    
    TYPE* next();
    TYPE* curr() 	{ return _curr ? &_curr->entry : 0; }
    KEY* curr_key() 	{ return _curr ? &_curr->key : 0; }
    void discard_curr();
private:
    int 			_idx;
    hash_lru_entry_t<TYPE, KEY>* _curr;
    hash_lru_t<TYPE, KEY>& 	_h;

    // disabled
    hash_lru_i(const hash_lru_i&);
    hash_lru_i& operator=(const hash_lru_i&);
};

#if defined(__GNUC__) 
#if defined(IMPLEMENTATION_HASH_LRU_H) || !defined(EXTERNAL_TEMPLATES)
#include "hash_lru.cpp"
#endif
#endif

/*<std-footer incl-file-exclusion='HASH_LRU_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
