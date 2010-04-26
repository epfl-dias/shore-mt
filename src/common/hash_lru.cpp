/*<std-header orig-src='shore'>

 $Id: hash_lru.cpp,v 1.29.2.5 2009/12/08 22:05:43 nhall Exp $

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

#ifndef HASH_LRU_C
#define HASH_LRU_C

#include <cstdlib>
#include <w_debug.h>

template <class TYPE, class KEY>
bool 
hash_lru_t<TYPE, KEY>::_in_htab(hash_lru_entry_t<TYPE, KEY>* e)
{
    return e->link.member_of() != &_unused;
}

/* XXX this is disgusting.   There was inline code doing
        int(&(_entry->XXX))
   Where _entry == 0 and XXX == member name, to determine the offsetof
   XXX in hash_lru_entry_t.  offsetof() should really be used for that.
   However there is a big problem betweent the CPP and the c++ compiler
   parsing that whole mess.  I introduced this macro and the comment so
   document why this is being used so it doesn't appear to be magick.
 */
#define hash_lru_offsetof(name,member)        ((size_t)(&(name->member)))

// XXX mirror the "normal" list/keyed/hash things here so it looks normal
#define        W_LIST_LRU_ARG(class,key)        hash_lru_offsetof(class,key)
#define        W_HASH_LRU_ARG(class,key,link)        \
        hash_lru_offsetof(class,key), hash_lru_offsetof(class,link)

template <class TYPE, class KEY>
hash_lru_t<TYPE, KEY>::hash_lru_t(int n, const char *desc) 
: ref_cnt(0), 
  hit_cnt(0),
  _mutex(desc),
  _entry(0), 
  _htab(n * 2, W_HASH_LRU_ARG(_entry, key, link)),
  _unused(W_LIST_LRU_ARG(_entry, link)),
  _clock(-1), 
  _total(n)
{
        _entry = new hash_lru_entry_t<TYPE, KEY> [_total];
        w_assert1(_entry);
    
        for (int i = 0; i < _total; i++)
                _unused.append(&_entry[i]);
}

#ifdef hash_lru_offsetof
#undef hash_lru_offsetof
#endif

template <class TYPE, class KEY>
hash_lru_t<TYPE, KEY>::~hash_lru_t()
{
    W_COERCE( _mutex.acquire() );
    for (int i = 0; i < _total; i++) {
        w_assert9(! _in_htab(_entry + i) );
    }
    while (_unused.pop()) ;
    delete [] _entry;
    _mutex.release();
}

template <class TYPE, class KEY>
TYPE* hash_lru_t<TYPE, KEY>::grab(
    const KEY& k,
    bool& found,
    bool& is_new)
{
    is_new = false;
    
    W_COERCE( _mutex.acquire() );                // enter monitor
    ref_cnt++;
    hash_lru_entry_t<TYPE, KEY>* p = _htab.lookup(k);
    
    if ( (found = (p != 0)) ) {
        hit_cnt++;
    } else {
        is_new = ((p = _unused.pop()) != 0);
        if (! is_new)  p = _replacement();
        w_assert9(p);
        p->key = k, _htab.push(p);
    }
    
    p->ref = true;

    return &p->entry;
    //_mutex.release(); // caller must release the mutex
}

template <class TYPE, class KEY>
TYPE* hash_lru_t<TYPE, KEY>::find(
    const KEY& k,
    int ref_bit)
{
    W_COERCE( _mutex.acquire() );
    ref_cnt++;
    hash_lru_entry_t<TYPE, KEY>* p = _htab.lookup(k);
    if (! p) {
        _mutex.release();
        return 0;
    }

    hit_cnt++;
    if ((ref_bit>0) && !(p->ref))  
        p->ref = true;

    return &p->entry;
}

template <class TYPE, class KEY>
bool 
hash_lru_t<TYPE, KEY>::find(
    const KEY& k,
    TYPE& entry_,
    int ref_bit)
{
    W_COERCE( _mutex.acquire() );
    ref_cnt++;
    hash_lru_entry_t<TYPE, KEY>* p = _htab.lookup(k);
    if (! p) {
        _mutex.release();
        return false;
    }

    hit_cnt++;
    if (ref_bit > p->ref)  p->ref = ref_bit;

    entry_ = p->entry;
    _mutex.release();
    return true;
}

template <class TYPE, class KEY>
void hash_lru_t<TYPE, KEY>::remove(const TYPE*& t)
{
    w_assert9(_mutex.is_mine());
    // caclulate offset (off) of TYPE entry in *this
#ifdef HP_CC_BUG_3
    hash_lru_entry_t<TYPE, KEY>& tmp = *(hash_lru_entry_t<TYPE, KEY>*)0;
    int off = ((char*)&tmp.entry) - ((char*)&tmp);
#else
    typedef hash_lru_entry_t<TYPE, KEY> HashType;
    int off = w_offsetof(HashType, entry);
#endif
    hash_lru_entry_t<TYPE, KEY>* p = (hash_lru_entry_t<TYPE, KEY>*) (((char*)t)-off);
    w_assert9(t == &p->entry);
    _htab.remove(p);
    t = NULL;
    _unused.push(p);
}

template <class TYPE, class KEY>
hash_lru_entry_t<TYPE, KEY>* 
hash_lru_t<TYPE, KEY>::_replacement()
{
    hash_lru_entry_t<TYPE, KEY>* p;
    int start = (_clock+1 == _total) ? 0 : _clock+1, rounds = 0;
    int i;
    for (i = start; ; i++)  {
        if (i == _total) i = 0;
        if (i == start && ++rounds == 3)  {
            cerr << "hash_lru_t: cannot find free resource" << endl;
            dump();
            W_FATAL(fcFULL);
        }
        p = _entry + i;
        w_assert9( _in_htab(p) );
        if (!p->ref) {
            break;
        }
        p->ref = false;
    }
    w_assert9( _in_htab(p) );
    //hash_lru_entry_t<TYPE, KEY>* tmp = p;  // use tmp since remove sets arg to 0
    _htab.remove(p);        // remove p from hash table
    _clock = i;
    return p;
}

template <class TYPE, class KEY>
void 
hash_lru_t<TYPE,KEY>::dump()
{
    FUNC(hash_lru_t<>::dump);
    for (int i = 0; i < _total; i++)  {
        hash_lru_entry_t<TYPE, KEY>* p = _entry + i;
        if (_in_htab(p))  {
            DBG( << i << '\t'
                 << p->key << '\t');
        }
    }
}

template <class TYPE, class KEY>
hash_lru_i<TYPE, KEY>::~hash_lru_i()
{
    _h._mutex.release();
}

template <class TYPE, class KEY>
TYPE* 
hash_lru_i<TYPE, KEY>::next()
{
    _curr = 0; 
    while( _idx < _h._total ) {
        hash_lru_entry_t<TYPE, KEY>* p = &_h._entry[_idx++];
        if (_h._in_htab(p)) {
            _curr = p;
            break;
        }
    }
    return _curr ? &_curr->entry : 0;
}

template <class TYPE, class KEY>
void 
hash_lru_i<TYPE, KEY>::discard_curr()
{
    w_assert9(_curr);
    const TYPE* tmp = &_curr->entry;
    _h.remove(tmp);
    _curr = 0;
}

#endif /* HASH_LRU_C */

