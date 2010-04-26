/*<std-header orig-src='shore' incl-file-exclusion='VTABLE_INFO_H'>

 $Id: vtable_info.h,v 1.12.2.5 2009/11/23 22:32:48 nhall Exp $

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

#ifndef VTABLE_INFO_H
#define VTABLE_INFO_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*
 * Definition of generic structure for converting lower layer info 
 * to a virtual table.  
 */

#include <cstring>

/* The only reason vtable_info_t is a template
 * is that there is no way to allocate an array of 
 * these things except with the default constructor.
 * If there were, this wouldn't be a template, and
 * we'd allocate an array as in 
 *  	v(n)[j].
 */
extern int global_vtable_last;
class vtable_info_t {
public:
    enum { vtable_value_size = 64 };

private:
    typedef char vtable_value_t [vtable_value_size];

public:
    NORET vtable_info_t() { }
    NORET ~vtable_info_t() { }

    void init(int n) {
	    N = n; 
#ifdef W_DEBUG
	    // had better have been initialized by caller
	    for(int a=0; a<n; a++) {
		    w_assert9(strlen(_get_const(a)) == 0); 
	    }
#endif
    }
    static int  size(int n) {
	    w_assert9( sizeof(vtable_value_t[3]) ==
		    3 * sizeof(vtable_value_t[1] ));

	    return sizeof(vtable_info_t) +
		    (n-1) * (sizeof(vtable_value_t[1]))
		    ;
    }
    void set_uint(int a, unsigned int v);
    void set_int(int a, int v);
    void set_base(int a, w_base_t::base_stat_t v);
    void set_base(int a, w_base_t::base_float_t v);
    void set_string(int a, const char *v);

    char *operator[](int a) const {
	    return _get(a);
    }
    ostream& operator<<(ostream &o);
    int n() const { return N; }

private:
    vtable_info_t(const vtable_info_t&); // disabled 
    vtable_info_t& operator=(const vtable_info_t&); // disabled 

    const char *_get_const(int a) const {
		return _get(a);
    }
    char *_get(int a) const {
	    w_assert1(a < N);
	    w_assert1(a >= 0);
	    char * v = (char *)&_list[0] 
		    + (sizeof(vtable_value_t) * a);
	    return v;
    }

    int			N;
    vtable_value_t 	_list[1];
};

class vtable_info_array_t {
public:
    NORET vtable_info_array_t() {}

    int init(int e, int s);

    NORET ~vtable_info_array_t() {
	delete[] _array_alias;
    }

    vtable_info_t& operator[] (int i) const {
	// called by updater so can't use this assertion
	// w_assert1(i < _entries_filled);
	return *_get(i);
    }

    ostream& operator<<(ostream &o) const;

    int			quant() const { return _entries_filled; }
    int			size() const { return _entries; }
    void		filled() { 
				_entries_filled++; 
				w_assert9(_entries_filled <= _entries);
			}
    void		back_out(int n) {  _entries_filled -= n; }
    int			realloc();
private:
    vtable_info_array_t(const vtable_info_array_t&); // disabled
    vtable_info_array_t& operator=(const vtable_info_array_t&); // disabled

    vtable_info_t* _get(int i) const;

    int			_entries;
    int			_entries_filled;
    int			_entrysize; //#strings
    int			_entrysizeflat;//#string * sizeof string
    char*		_array_alias;

};

template <class T>
class vtable_func 
{
public: 
    NORET vtable_func(vtable_info_array_t &v):
	_curr(0), _array(v) { }

    NORET ~vtable_func() { }

    void operator()(const T& t) // gather func
	{
		// escape the const-ness if possible
		T &t2 = (T &)t;
		t2.vtable_collect( _array[_curr] );
		// bump its counter
		_array.filled();
		w_assert9(_curr < _array.size());
		_curr++;
	}
    typedef void vtable_collect_func(vtable_info_t& t);
    void call(vtable_collect_func f) // arbitrary gather func
	{
		f(_array[_curr]);
		// bump its counter
		_array.filled();
		w_assert9(_curr < _array.size());
		_curr++;
	}
    void		back_out(int n) {  
	_curr -= n; 
	_array.back_out(n);
    }
    int			realloc() {
	return _array.realloc();
    }

protected:
    int			 _curr;
    vtable_info_array_t& _array;
};

/*<std-footer incl-file-exclusion='VTABLE_INFO_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
