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

// -*- mode:c++; c-basic-offset:4 -*-
/*<std-header orig-src='shore'>

 $Id: bf_htab_old.cpp,v 1.1.2.4 2009/08/25 19:24:33 nhall Exp $

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


#ifndef BF_CORE_C
#define BF_CORE_C
#endif

#include <stdio.h>
#include <cstdlib>
#include <sstream>
#include "sm_int_0.h"

#include "bf_s.h"
#include "bf_core.h"

#include "w_cuckoohashfuncs.h"
#include "bf_htab_old.h"
// This implementation uses transit-bucket in its eviction test
#include "bf_transit_bucket.h"

#ifndef NEW_CUCKOO_HTAB

// Create a dummy class to hold an operator for 
// the bf_core_m::htab::insert method to call. This is lets us
// paramterize the hash-table insertion with a test method for
// determining eligibility for insertion.
// This is the instance used for the templates with class EvictTest
struct evict_test {
    bool operator()(bfcb_t* p, int rounds) { return bf_core_m::can_replace(p, rounds); }
};

bfcb_t* bf_core_m::htab::lookup(bfpid_t const &pid)  const
{
    bfcb_t* p = NULL;
    atomic_add(_lookups, 1);
    int count = 0;
    for(int i=0; i < HASH_COUNT && !p; i++) 
    {
	++count;
	int idx = hash(i, pid);
	bucket &b = _table[idx];
	p = b._frame;

        atomic_add(_probes, count);

	if(!p) continue; // empty bucket
	
	if(p->pin_if_pinned()) {
	    // we didn't need to lock the bucket because the frame
	    // was pinned anyway. Is it the right frame, though?
	    if(p->pid != pid) {
		// oops! got the wrong one
		p->unpin();
		p = NULL;
	    }
	}
	else {
	    // we need to acquire the bucket lock before grabbing
	    // a free latch. The nice part is we know the latch
	    // acquire will (usually) succeed immediately...
	    w_assert2(b._lock.is_mine()==false);
	    CRITICAL_SECTION(cs, b._lock);
	    w_assert2(b._lock.is_mine());
	    p = b._frame; // in case we raced...
	    
	    if(p != NULL && p->pid == pid) 
		p->pin();
	    else
		p = NULL;
	}
    }
    return p;
}

bool bf_core_m::htab::remove(bfcb_t* p) 
{
    atomic_add(_removes, 1);
    bucket &b = _table[p->hash];
    w_assert2(p->hash_func != HASH_COUNT);
    if(b._frame != p || !b._lock.is_mine() || p->pin_cnt > 0)
	return false;

    w_assert2(b._lock.is_mine());

    p->hash_func = HASH_COUNT;
    b._frame = NULL;
    return true;
}

struct attempt_critical_section 
{
    pthread_mutex_t* _lock;
    attempt_critical_section(pthread_mutex_t &lock)
	// non-zero return means failure
	: _lock(pthread_mutex_trylock(&lock)? NULL : &lock) { }
    bool valid() { return _lock != NULL; }
    ~attempt_critical_section() {
	if(_lock) pthread_mutex_unlock(_lock);
    }
};

void htabstop()
{
}

template<class EvictTest>
bfcb_t* bf_core_m::htab::_insert(bfcb_t* p, EvictTest &test, int &depth) 
{
    // TODO: NANCY These should be turned into config parameters 
    enum { MAXPASS=1000, MAXDEPTH=10 };

    // Err out if we exceed the depth of passes or the depth.
    // Depth indicates the number of moves we will make in order
    // to insert.  Passes indicates the amount of time we are
    // willing to busy-wait for another thread to finish using
    // the htab and free up its locks (this is for coping with
    // deadlock)  

    atomic_add(_inserts, 1);
    // figure out the hash codes
    int idx[HASH_COUNT];

    // p->hash_func indicates which hash func was last used
    // on a successful insert, and p->hash is the result of the
    // hash.   Note that hash_func gets initialized to HASH_COUNT on 
    // htab::insert() for the frame we are trying to insert with its pid.
    // Initialize our hashes here, but for the last successful hash,
    // you can bypass the hash function. (performance)
    // 
    for(int i=0; i < HASH_COUNT; i++) 
	idx[i] = (p->hash_func == i)? p->hash : hash(i, p->pid);

#if W_DEBUG_LEVEL >= 2
    // verify my comment above
    if(p->hash_func < HASH_COUNT) {
    w_assert2(unsigned(p->hash) == unsigned(hash(p->hash_func, p->pid)));
    }
#endif

    int pass=0;
    for(; ; pass++) 
    {
	// Err out if we exceed the number of passes,
	if(pass > HASH_COUNT) {
	    smthread_t::yield(); // no longer a no-op
	    if(pass > MAXPASS) break;
	}
	
	for(int i=0; i < HASH_COUNT; i++) {
	    // hash_func shows which hash func (i) succeeded when
	    // the entry at p was last inserted, while p->hash is its
	    // hash value.    Check the hash_func to tell which
	    // bucket-resident move got us here - that's indicated by
	    // i == hash_func and we have to skip this -- it's the
	    // insertion we are trying to remedy by moving inserting its
	    // old contents elsewhere.
	    
	    if(p->hash_func == i) {
		continue; // can't stay where it is...
	    }

	    bucket &b = _table[idx[i]];

	    atomic_add(_slots_tried, 1);

	    if(pass == 0 && b._frame != NULL) {
		continue; // prefer empty buckets the first time around
	    }

	    if(b._count > 4*_avg && (sthread_t::rand()%4) != 0) {
		continue; // avoid crowded buckets
	    }


	    // Note: # passes isn't limited so eventually (1 out of 
	    // 4 tries) we'll pass muster with the above test
	    
	    // Rather than modifying try_lock, we'll require that
	    // the caller never tries a lock that it already holds.
	    // Already holding this lock indicates a cycle in the
	    // hash table. Since we have another alternative location
	    // (if this is an n-way, n > 2), let's just skip this
	    // bucket (as if it were busy) and try another.
	    // w_assert2(b._lock.is_mine()==false);
	    if(b._lock.is_mine()) {
		// revisits counts cycle detections (not the # cycles,
		// but the #times we ran into one).
		atomic_add(_revisits, 1);
		continue;
	    }

	    if(b._lock.try_lock()) {
		// b._frame might be NULL (empty)
		// if not, cuckold will handle the eviction
		bfcb_t* v = cuckold(b._frame, test, ++depth);
		b._frame = p;
		b._count++;
		p->hash = idx[i];
		p->hash_func = i;
		w_assert2(b._lock.is_mine());
		b._lock.release();
		w_assert2(b._lock.is_mine()==false);
		w_assert2(v != b._frame);
		return v; // non-null if there was an eviction
	    }
	}
    }

    w_assert1(pass > MAXPASS) ;
    // failure! Could not insert p->pid.page
    // Now either we have to rebuild the hash table
    // or we had a deadlock or livelock.  TODO: How to detect the difference? 
    cerr << "bf_core _insert failed, depth  " << depth
    << " pass  "<<  pass << endl;
    w_assert1(pass <= MAXPASS); // to catch this for now
    return NULL;
}

template<class EvictTest>
bfcb_t* bf_core_m::htab::cuckold(bfcb_t* p, EvictTest &test, int &depth) 
{
    // was the spot empty?
    if(!p) return NULL;
    
    // can we evict it?
    atomic_add(_cuckolds, 1);
    if(1 && test(p, depth/2)) {
	transit_bucket &tb = get_transit_bucket(p->pid);
	attempt_critical_section cs(tb._mutex);
	if(cs.valid()) {
	    bucket &b = _table[p->hash];
	    w_assert1(b._lock.is_mine()); // was w_assert3
	    if(test(p, depth/2)) {
		bfcb_t* v = b._frame;
		v->hash_func = HASH_COUNT;
		tb.make_in_transit_out(v);
		return v;
	    }
	}
    }

    return _insert(p, test, depth);
}

bfcb_t* bf_core_m::htab::insert(bfcb_t* p)
{
    // update the average
    atomic_add(_insertions, 1);
    _avg = (2*_insertions + _cuckolds - 1)/_insertions;

    p->hash_func = HASH_COUNT; // just to be safe...
    
    int depth = 1;
    struct evict_test anon; // construct it
    bfcb_t* v = _insert(p, anon, depth);
    if(depth > 4) {
	atomic_add(_slow_inserts, 1);
	if(depth > 20) 
	    fprintf(stderr, "WARNING: Cuckoo hash insert took %d tries!\n", depth);
	    
    }
    return v;
}

void bf_core_m::htab::print_histo() 
{
    char temp[100];
    for(int i=0; i < _size; i++) {
	char* str = temp;
	for(int val=_table[i]._count; val > 0; val/=2)
	    *(str++) = '*';
	*(str++) = '\n';
	*str = '\0';
	if(_table[i]._frame)
	    temp[0] = '+';

	fprintf(stderr, temp);
	fflush(stderr);
    }
};

void bf_core_m::htab::print_holders() 
{
    stringstream os;
    static char const* const nums = ".,123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ#";
    for(int i=0; i < _size; i++) {
	int holder = _table[i]._lock._holder;
	if(i % 100 == 0)
	    os << "\n";
	else
	    os << " ";
	if(holder >= 64)
	    holder = 63;
	os << nums[holder];
    }
    os << ends;
    fprintf(stderr, "%s\n", os.str().c_str());
}

void bf_core_m::htab::print_occupants() 
{
    stringstream os;
    static char const* const nums = ".+";
    for(int i=0; i < _size; i++) {
	bool occupied = _table[i]._frame;
	if(i % 100 == 0)
	    os << "\n";
	else
	    os << " ";
	os << nums[occupied];
    }
    os << ends;
    fprintf(stderr, "%s\n", os.str().c_str());
}

/*********************************************************************
 *
 *  bf_core_m::_in_htab(pid)
 *
 *  Return true if e is in the hash table -- FOR DEBUGGING ONLY
 *  It should be thread-safe but it's not exactly a speedy thing to do.
 *
 *********************************************************************/

bool
bf_core_m::_in_htab(const lpid_t &pid) const
{
    static int const COUNT = htab::HASH_COUNT;
    int idx;
    bool checkedi[COUNT];
    int checked(0);
    for(int i=0; i < COUNT; i++) checkedi[i]=false;

    while(checked < COUNT) for(int i=0; i < COUNT; i++) {
	if(checkedi[i]) continue;
	idx = _htab->hash(i, pid);
	htab::bucket &b = _htab->_table[idx];
	// NOT THREAD-SAFE
        // if(b._lock.try_lock()) {
	    if(b._frame && b._frame->pid == pid) {
		w_assert1( _in_htab(b._frame) );
		b._lock.release();
		return true;
	    }
	    checkedi[i] = true;
	    checked++;
	// }
    }

    return false;
}

void                        
bf_core_m::htab::stats(bf_htab_stats_t &out) const
{
#define COPY_STAT(b,a) *&(out.bf_htab##b) = *(&(a));
	COPY_STAT(_insertions, _insertions);
	COPY_STAT(_slots_tried, _slots_tried);
	COPY_STAT(_slow_inserts, _slow_inserts);
	COPY_STAT(_lookups, _lookups);
	COPY_STAT(_probes, _probes);
	COPY_STAT(_removes, _removes);
#undef COPY_STAT
    *(&out.bf_htab_bucket_size) = sizeof(bucket);
    *(&out.bf_htab_table_size) = sizeof(bucket) * _size;
    *(&out.bf_htab_buckets) =  _size;
    *(&out.bf_htab_slot_count) =  0;
}
#endif
