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

/*<std-header orig-src='shore'>

 $Id: w_statistics.cpp,v 1.30.2.5 2009/10/30 23:50:10 nhall Exp $

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

#ifdef __GNUG__
#pragma implementation
#endif
#include <w.h>

#include "w_statistics.h"
#include "fc_error_enum_gen.h"
#include <cassert>

#include <w_stream.h> 

w_stat_t		w_statistics_t::error_stat(-1);
int			    w_statistics_t::error_int=-1;
unsigned int    w_statistics_t::error_uint=~0;
long			w_statistics_t::error_long=-1L;
unsigned long   w_statistics_t::error_ulong=~0L;
float 			w_statistics_t::error_float=-1.0;

bool
operator==(const w_stat_t &one, const w_stat_t &other)
{
	if(&one==&other) return true;

	return (one._u.i == other._u.i);
}
bool
operator!=(const w_stat_t &one, const w_stat_t &other)
{
	return !(one==other);
}

void 
w_statistics_t::append(w_stat_module_t *v)
{
	v->next = 0;
	if(_first==0) {
		_first = v;
	}
	if(_last) {
		_last->next = v;
	}
	_last = v;
}

w_rc_t		
w_statistics_t::add_module_static(
	const char *desc, int base, int count,
	const char **strings, const char *types, const w_stat_t *values)
{
	w_rc_t	e;
	if(empty() || !writable()) {
		e = _newmodule(desc,false,
			base,count,
			strings,false,
			types,false,
			values,0,false);
	} else {
		// cannot add a static entry if dynamic ones are there
		e = RC(fcMIXED);
	}
	return e;
}


w_rc_t		
w_statistics_t::_newmodule(
	const char *desc, 
	bool d_malloced,

	int base, int count,
	const char **strings, 
	bool s_malloced,

	const char *types, 
	bool t_malloced,

	const w_stat_t *values,
	w_stat_t *w_values,
	bool v_malloced // = false
)
{
	w_stat_module_t *v;

	if(empty()) {
		values_are_dynamic = v_malloced;
	}else {
		w_assert9(values_are_dynamic==v_malloced);
	}
	w_assert9(!(values==0 && w_values==0));


	if( !(v = find(base))) {
		// didn't find it-- that's what we expected
		// if this is the first time << was used,
		// or if we're making a copy.

		v = new w_stat_module_t(
			desc,d_malloced,
			strings,s_malloced,
			types, t_malloced,
			base,count,v_malloced);
		if(!v) {
			return RC(fcOUTOFMEMORY);
		}

		append(v);

		// compute longest string:
		if(v->strings) {
			int j,mx=0,ln=0;
			for(j = 0; j<count; j++) {
				ln = strlen(strings[j]);
				if(mx<ln) mx = ln;
			}
			v->longest=mx;
		}
	} else {

		// Module already there -- this happens when
		// we are using << operator from a class
		// to collect a *new* copy of the stats.
		// (We'd like to allow users to do this twice
		// in a row without having to call free() 
		// or some such thing between uses of <<.

		w_assert9(v->base == base);
		if(v->count != count) {
			cerr <<"Adding module " << desc <<
			 ", which conflicts with module " << v->d
			 <<", which is already registered." << endl;
			 return RC(fcFOUND);
		}
		w_assert9(v->count == count);
		w_assert9(v->strings == strings || strings==0);
		w_assert9(v->types == types || types==0);
		w_assert9(v->d == desc || desc==0);

		// get rid of the old copy if necessary
		if(v->m_values) {
			w_assert9(v->values == v->w_values);
			w_assert9(v->w_values != 0);
			delete[] v->w_values;
			v->w_values=0;
			v->values = 0;
		}
	}

	// fill in or overwrite the values
	if(v_malloced) {
		v->values = v->w_values = w_values;
	} else {
		v->values = values;
	}

	//
	// re-compute signature for whole thing
	//
	hash();

	return RCOK;
}

void
w_statistics_t::hash()
{
	w_statistics_iw 		iter(*this);
	w_stat_module_t 		*m;
	int 					i;

	_signature = 0;
	for (i=0, m = iter.curr(); m != 0; m = iter.next(),i++) {
		_signature ^= (m->base | m->count);
	}
}

w_statistics_t::w_statistics_t():
	_signature(0),
	_first(0), _last(0),
	values_are_dynamic(false),
	_print_flags(print_nonzero_only)
{ }

w_statistics_t::~w_statistics_t()
{
	make_empty();
}

void
w_statistics_t::make_empty()
{
	w_stat_module_t *save;
	if(_first) {
		for (w_stat_module_t *v = _first; v != 0; ) {
			save = v->next;
			delete v;
			v = save;
		}
		_first = 0;
		_last = 0;
	}
}


void	
w_statistics_t::printf() const
{
	cout << *this;
}


char *
w_stat_module_t::strcpy(const char *s) const
{
	int i = strlen(s);
	char *z = (char *)malloc(i+1);
	if(z) {
		memcpy(z,s,i);
		z[i]='\0';
	}
	return z;
}

const char **
w_stat_module_t::getstrings(bool copyall) const
{
	char ** _s = 0;
	if(copyall) {
		_s = (char **)malloc(count * sizeof(char *));
		if(_s) { 
			// copy every stinking string
			int 	j;
			for(int i=0; i<count; i++) {
				j = strlen(strings[i]);
				_s[i] =  strcpy(strings[i]);
				if(!_s[i]) {
					goto bad;
				}
			}
		}
		return (const char **)_s;
	} else {
		return strings;
	}
bad:
	for(int i=0; i<count; i++) {
		if(_s[i]) {
			free(_s[i]);
		} else {
			break;
		}
	}
	free(_s);
	return 0;
}

w_rc_t
w_stat_module_t::copy_descriptors_from(
	const w_stat_module_t &from
)
{
	char			*_d=0;
	char			*_t=0;
	const char		**_s = 0;

	_d = strcpy(from.d);
	if(!_d) {
		goto bad;
	}
	_t = strcpy(from.types);
	if(!_t) {
		goto bad;
	}
	_s = from.getstrings(true);
	if(!_s) {
		goto bad;
	}

	make_empty(false);
	d = _d;
	m_d = 1;
	types = _t;
	m_types = 1;
	strings = _s;
	m_strings = 1;

	return RCOK;


bad:
	if(_s)  {
		for(int i=0; i<count; i++) {
			if(_s[i]) {
				free((char *)_s[i]);
			} else {
				break;
			}
		}
		free(_s);
	}
	if(_d) free(_d);
	if(_t) free(_t);
	return RC(fcOUTOFMEMORY);
}

w_stat_module_t	*
w_statistics_t::find(int base) const	
{
	w_stat_module_t	*v=0;

	for(v = _first; v!=0 ;v=v->next){
		if(v->base == base) {
			return v;
		}
	}
	assert(v==0);
	return v;
}

w_stat_module_t	*
w_statistics_iw::find(int base) const	
{
	w_stat_module_t	*v=0;

	for(v = _su.s->first_w(); v!=0 ;v=v->next){
		if(v->base == base) {
			return v;
		}
	}
	assert(v==0);
	return v;
}


int	
w_statistics_t::int_val(int base, int i)
{
	const w_stat_module_t *m = find(base);
	if(m)  {
		const w_stat_t &v = find_val(m,i);
		if(v != error_stat) 
			return int(v);
	}
	return error_int;
}

unsigned int
w_statistics_t::uint_val(int base, int i)
{
	const w_stat_module_t *m = find(base);
	if(m)  {
		const w_stat_t &v = find_val(m,i);
		if(v != error_stat) 
			return unsigned(int(v));
	}
	return error_uint;
}

long
w_statistics_t::long_val(int base, int i)
{
	const w_stat_module_t *m = find(base);
	if(m)  {
		const w_stat_t &v = find_val(m,i);
		if(v != error_stat) 
			return long(int(v));
	}
	return error_long;
}

unsigned long
w_statistics_t::ulong_val(int base, int i)
{
	const w_stat_module_t *m = find(base);
	if(m)  {
		const w_stat_t &v = find_val(m,i);
		if(v != error_stat) 
			return u_long(int(v));
	}
	return error_ulong;
}

float
w_statistics_t::float_val(int base, int i)
{
	const w_stat_module_t *m = find(base);
	if(m)  {
		const w_stat_t &v = find_val(m,i);
		if(v != error_stat) return (float)v;
	}
	return error_float;
}


char	
w_statistics_t::typechar(int base, int i)
{
	w_stat_module_t *v = find(base);
	if(v) {
		return v->types[i];
	}
	return '\0';
}
const	char	*
w_statistics_t::typestring(int base)
{
	w_stat_module_t *v = find(base);
	if(v) return v->types;
	return "";
}
const	char	*
w_statistics_t::module(int base)
{
	w_stat_module_t *v = find(base);
	if(v) return v->d;
	return "unknown";
}
const	char	*
w_statistics_t::module(int base, int /*i_not_used*/)
{
	return module(base);
}

const	char	*
w_statistics_t::string(int base, int i)
{
	w_stat_module_t *v = find(base);
	if(v) return find_str(v, i);
	return "unknown";
}

const w_stat_t	&
w_statistics_t::find_val(const w_stat_module_t *m, int i) const
{
	if(i<0 || i >= m->count) {
		return error_stat;
	}
	return m->values[i];
}

const char		*
w_statistics_t::find_str(const w_stat_module_t *m, int i) const	
{
	if(i<0 || i >= m->count) {
		return "bad index";
	}
	return m->strings[i];
}

/* XXX The 'long' and 'unsigned long' casts for 'v' and 'l' exist
   because 'v' and 'l' are only 'int' width on 64 bit machines.  That
   is a historical problem that can be fixed once stats support sized
   types in addition to named types. */

ostream	&
operator<<(ostream &out, const w_statistics_t &s) 
{
	w_statistics_i 		iter(s);
	const w_stat_module_t 	*m;
	int					i, field, longest;

	// compute longest strings in all modules
	for (longest=0,m = iter.curr(); m != 0; m = iter.next()) {
		longest = m->longest>longest ? m->longest : longest;
	}

	field = longest+2;
	if(field < 10) field=10;

	iter.reset();

	for (m = iter.curr(); m != 0; m = iter.next()) {
		if(m->d) {
			out << endl << m->d << ": " << endl;
		} else {
			out << "unnamed module: ";
		}
		for(i=0; i< m->count; i++) {
			switch(m->types[i]) {

			case 'i':
				if((s._print_flags & w_statistics_t::print_nonzero_only)==0 || 
					(m->values[i]._u.i != 0)) {
					W_FORM2(out,("%*.*s: ", field, field, m->strings[i]));
#if defined(LARGEFILE_AWARE) || defined(ARCH_LP64)
					W_FORM2(out,("%10ld",m->values[i]._u.i));
#else
					W_FORM2(out,("%10d",m->values[i]._u.i));
#endif
					out << endl;
				}
				break;

			case 'd':
			case 'f':
				if((s._print_flags & w_statistics_t::print_nonzero_only)==0 || 
					(m->values[i]._u.f != 0.0)) {
					W_FORM2(out,("%*.*s: ", field, field, m->strings[i]));
					W_FORM2(out,("%#10.6g",m->values[i]._u.f));
					out << endl;
				}
				break;
			default:
				out << "Fatal error at file:" 
				<< __FILE__ << " line: " << __LINE__ << endl;
				assert(0);
			}
		}
	}
	out << flush;

	return out;
}


bool	
w_stat_module_t::operator==(const w_stat_module_t&r) const
{
	return( 
		this->base == r.base
		&&
		this->count == r.count
		&&
		(strcmp(this->types, r.types)==0)
		&&
		(strcmp(this->d, r.d)==0) );
}

w_statistics_t	&
operator+=(w_statistics_t &l, const w_statistics_t &r)
{
	w_rc_t e;
	if(l.writable()) {
		e = l.add(r);
	} else {
		e = RC(fcREADONLY);
	}
	if(e.is_error()) {
		// cerr << e ;
		cerr << "Cannot apply += operator to read-only w_statistics_t." << endl;
	}
	return l;
}

w_statistics_t	&
operator-=(w_statistics_t &l, const w_statistics_t &r)
{
	w_rc_t e;
	if(l.writable()) {
		e = l.subtract(r);
	} else {
		e = RC(fcREADONLY);
	}
	if(e.is_error()) {
		cerr << e << ":" << endl;
		cerr << "Cannot apply -= operator to read-only w_statistics_t." << endl;
	}
	return l;
}


w_rc_t			
w_statistics_t::copy_descriptors_from(const w_statistics_t &from)
{
	w_statistics_iw		l(*this);
	w_statistics_i		r(from);
	const w_stat_module_t 	*rm;
	w_stat_module_t 	*lm;
	w_rc_t				e;

	for (lm = l.curr(), rm=r.curr(); 
		lm != 0 && rm !=0; 
		lm = l.next(), rm = r.next()
	) {
		// is this module the same in both structures?
		// (not only do we expect to *find* the module
		// in the right-hand structure, we expect the
		// two structs to be exactly alike
		if( !(*lm == *rm))  {
			return RC(fcNOTFOUND);
		}
		e = lm->copy_descriptors_from(*rm);
		if(e.is_error()) break;
	}
	return e;
}

w_rc_t			
w_statistics_t::add(const w_statistics_t &right)
{
	w_statistics_i		l(*this);
	w_statistics_i		r(right);
	const w_stat_module_t 	*lm,*rm;
	w_rc_t				e;

	for (lm = l.curr(), rm=r.curr(); 
		lm != 0 && rm !=0; 
		lm = l.next(), rm = r.next()
	) {
		// is this module the same in both structures?
		// (not only do we expect to *find* the module
		// in the right-hand structure, we expect the
		// two structs to be exactly alike
		if( !(*lm == *rm))  {
			return RC(fcNOTFOUND);
		}
		e = lm->op(w_stat_module_t::add,rm);
		if(e.is_error()) return e;
	}
	return RCOK;
}


w_rc_t			
w_statistics_t::subtract(const w_statistics_t &right)
{
	w_statistics_i		l(*this);
	w_statistics_i		r(right);
	const w_stat_module_t 	*lm,*rm;
	w_rc_t				e;

	for (lm = l.curr(), rm=r.curr(); 
		lm != 0 && rm !=0; 
		lm = l.next(), rm = r.next()
	) {
		// is this module the same in both structures?
		// (not only do we expect to *find* the module
		// in the right-hand structure, we expect the
		// two structs to be exactly alike
		if( !(*lm == *rm))  {
			return RC(fcNOTFOUND);
		}
		e = lm->op(w_stat_module_t::subtract,rm);
		if(e.is_error()) return e;
	}
	return RCOK;
}

w_rc_t			
w_statistics_t::zero() const
{
	w_statistics_i		iter(*this);
	const w_stat_module_t 	*m;
	w_rc_t				e;

	for (m = iter.curr(); m != 0; m = iter.next()) {
		e = m->op(w_stat_module_t::zero);
		if(e.is_error()) return e;
	}
	return RCOK;
}

w_rc_t
w_stat_module_t::op(enum operators which, const w_stat_module_t *r) const
{
	int i;

	if(!m_values) {
		return RC(fcREADONLY);
	} else {
		w_assert9(values==w_values);
	}

	if(which != zero) {
		w_assert9(r!=0);
		w_assert9(count == r->count);
	}

	for(i=0; i< count; i++) {
		if(which != zero) {
			w_assert9(types[i] == r->types[i]);
		}
		switch(types[i]) {
		case 'i':
			if(which==add) {
				w_values[i]._u.i += r->values[i]._u.i;
			} else if (which==subtract) {
				w_values[i]._u.i -= r->values[i]._u.i;
			} else if (which==zero) {
				w_values[i]._u.i = 0;
			} else if (which==assign) {
				w_values[i]._u.i = r->values[i]._u.i;
			} else {
				W_FATAL_MSG(fcINTERNAL, << "base case");
			}
			break;
		case 'f':
		case 'd':
			if(which==add) {
				w_values[i]._u.f += r->values[i]._u.f;
			} else if (which==subtract) {
				w_values[i]._u.f -= r->values[i]._u.f;
			} else if (which==zero) {
				w_values[i]._u.f = 0.0;
			} else if (which==assign) {
				w_values[i]._u.f = r->values[i]._u.f;
			} else {
				W_FATAL_MSG(fcINTERNAL, << "base case");
			}
			break;
		default:
			W_FATAL_MSG(fcINTERNAL, << "base case");
		}
	}
	return RCOK;
}

