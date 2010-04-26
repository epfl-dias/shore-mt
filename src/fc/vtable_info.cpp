/*<std-header orig-src='shore'>

 $Id: vtable_info.cpp,v 1.14.2.4 2009/11/23 22:32:48 nhall Exp $

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

#include <w.h>
#include <vtable_info.h>

#include <w_strstream.h>


void 
vtable_info_t::set_uint(int a, unsigned int v) {
	// check for reasonable attribute #
	w_assert1(a < N);
	// check attribute not already set
	w_assert9(strlen(_get_const(a)) == 0); 
	w_ostrstream o(_get(a), vtable_info_t::vtable_value_size);
	o << v << ends;
}
void 
vtable_info_t::set_base(int a, w_base_t::base_float_t v) {
	// check for reasonable attribute #
	w_assert9(a < N);
	// check attribute not already set
	w_assert9(strlen(_get_const(a)) == 0); 
	w_ostrstream o(_get(a), vtable_info_t::vtable_value_size);
	o << v << ends;
}
void 
vtable_info_t::set_base(int a,  w_base_t::base_stat_t v) {
	// check for reasonable attribute #
	w_assert9(a < N);
	// check attribute not already set
	w_assert9(strlen(_get_const(a)) == 0); 
	w_ostrstream o(_get(a), vtable_info_t::vtable_value_size);
	o << v << ends;
}
void 
vtable_info_t::set_int(int a, int v) {
	// check for reasonable attribute #
	w_assert9(a < N);
	// check attribute not already set
	w_assert9(strlen(_get_const(a)) == 0); 
	w_ostrstream o(_get(a), vtable_info_t::vtable_value_size);
	o << v << ends;
}
void 
vtable_info_t::set_string(int a, const char *v) {
	// check for reasonable attribute #
	w_assert1(a < N);
	w_assert1((int)strlen(v) < vtable_value_size);
	// check attribute not already set
	w_assert9(strlen(_get_const(a)) == 0); 
	strcpy(_get(a), v);
	w_assert9(_get_const(a)[strlen(v)] == '\0'); 
}

ostream& 
vtable_info_t::operator<<(ostream &o) {
    for(int i=0; i<N; i++) {
	    if(strlen(_get_const(i)) > 0) {
	    o <<  i << ": " << _get_const(i) <<endl;
	    }
    }
    o <<  endl;
    return o;
}


int 
vtable_info_array_t::init(int e, int s) {

    _entries_filled = 0;
    _entries = e;
    _entrysize = s;
    _entrysizeflat = vtable_info_t::size(s);

    _array_alias = new char [_entries * _entrysizeflat];

    if(_array_alias) {
	// initialize the array
	memset(_array_alias, '\0', _entries * _entrysizeflat);

	for(int i=0; i<_entries; i++) {
	    _get(i)->init(_entrysize);
#ifdef W_DEBUG

	    vtable_info_t *g = _get(i);
	    w_assert9(g->n() == _entrysize);
	    for(int j=0; j<_entrysize; j++) {
		w_assert9(strlen(g->operator[](0)) == 0);
	    }
#endif /* W_DEBUG */
	}
	return 0;
    } else {
	return -1;
    }
}

ostream& 
vtable_info_array_t::operator<<(ostream &o) const {
    for(int i=0; i<_entries_filled; i++) {
	_get(i)->operator<<(o) ;
    }
    o <<  endl;
    return o;
}

vtable_info_t* 
vtable_info_array_t::_get(int i) const {
    w_assert9(i >= 0);
    w_assert9(i <= _entries);
    w_assert9(_entries_filled <= _entries);
    vtable_info_t* v =
    (vtable_info_t *)&_array_alias[_entrysizeflat * i];
    return v;
}

int			
vtable_info_array_t::realloc()
{
    char*		_array_alias_tmp = _array_alias;

#ifdef W_DEBUG
cerr << "vtable_info_array_t::realloc() from " 
	<< _entries
	<< " to " 
	<< _entries * 2
	<< endl;
#endif
    _entries *= 2;
    _array_alias = new char [_entries * _entrysizeflat];
    if(_array_alias == 0) return -1;

    memcpy(_array_alias, _array_alias_tmp, _entries_filled * _entrysizeflat);
    memset((_array_alias + (_entries_filled * _entrysizeflat)),
		    '\0', (_entries - _entries_filled) * _entrysizeflat);

    /* same checks as in init() */
    for(int i=_entries_filled; i < _entries; i++) {
	_get(i)->init(_entrysize);
#ifdef W_DEBUG

	vtable_info_t *g = _get(i);
	w_assert9(g->n() == _entrysize);
	for(int j=0; j<_entrysize; j++) {
	    w_assert9(strlen(g->operator[](0)) == 0);
	}
#endif /* W_DEBUG */
    }
	
    delete[] _array_alias_tmp;
    return 0;
}

int global_vtable_last (0);
