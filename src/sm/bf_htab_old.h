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
/*<std-header orig-src='shore' incl-file-exclusion='BF_CORE_H'>

 $Id: bf_htab_old.h,v 1.1.2.2 2009/08/01 00:46:40 nhall Exp $

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

#ifndef BF_HTAB_OLD_H
#define BF_HTAB_OLD_H

#ifdef __GNUG__
#pragma interface
#endif

struct bf_core_m::htab 
{
#ifdef HTAB_UNIT_TEST_C
    // NOTE: these static funcs are NOT thread-safe; they are
    // for unit-testing only.  
    friend bfcb_t* htab_lookup(bf_core_m *, bfpid_t const &pid, Tstats &) ;
    friend bfcb_t* htab_insert(bf_core_m *, bfpid_t const &pid, Tstats &);
    friend  bool    htab_remove(bf_core_m *, bfpid_t const &pid, Tstats &) ;
    friend  void htab_dumplocks(bf_core_m*);
    friend  void htab_count(bf_core_m *core, int &frames, int &slots);
#endif

    struct bucket {
	bfcb_t* volatile	_frame;
	tatas_lock		_lock;
	int			_count; // was residents
	bucket() : _frame(NULL), _count(0) { }

	bfcb_t *get_frame(bfpid_t /*pid*/) { return _frame; }
    };

    enum {H1, H2, H3, HASH_COUNT};
    bucket*	_table;
    hash2	_hash[HASH_COUNT];
    int		_size;
    int		_avg; // estimate

    mutable int _insertions;
    mutable int _inserts;
    mutable int _slots_tried;
    mutable int _cuckolds;
    mutable int _slow_inserts;
    
    mutable int _lookups;
    mutable int _probes;
    mutable int _removes; 
    mutable int _revisits;

    htab(int size)
	: _insertions(0), _inserts(0), _slots_tried(0),
	_cuckolds(0), _slow_inserts(0), _lookups(0), _probes(0), 
	_removes(0), _revisits(0)
    {
	_table = new bucket[size];
	_size = size;
    }
    bfcb_t* lookup(bfpid_t const &pid) const;
    bool remove(bfcb_t* p);
    bfcb_t* insert(bfcb_t* p);

    // see class evict_test in bf_htab_old.cpp
    template<class EvictTest>
	    bfcb_t* _insert(bfcb_t* p, EvictTest &test, int &depth);
    template<class EvictTest>
	    bfcb_t* cuckold(bfcb_t* p, EvictTest &test, int &depth);
    
    unsigned hash(int h, bfpid_t const &pid) const {
	w_assert3(h >= H1 && h < HASH_COUNT);
	unsigned x = pid.vol() ^ pid.page;
	w_assert3(h < HASH_COUNT);
	return _hash[h](x) % _size;
    }
    
    ~htab() {
	//	print_histo();
	fprintf(stderr, "BPool stats:\n");
	fprintf(stderr, 
		"\t%d insertions averaging %.2lf tries each (%d took > 4 tries)\n",
		_insertions, 1.0 + _cuckolds/(double) _insertions, _slow_inserts);
	fprintf(stderr, "\t%d lookups averaging %.2lf probes each (max %d)\n",
		_lookups, _probes/(double) _lookups, HASH_COUNT);
	fprintf(stderr, "\tbucket size %d bytes\n", int(sizeof(bucket)));
	fprintf(stderr, "\ttable size %d bytes\n", int(sizeof(bucket)) * _size);
	delete [] _table;
    }
    void print_histo();
    void print_holders();
    void print_occupants();
};

#endif
