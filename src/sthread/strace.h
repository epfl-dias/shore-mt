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

/*<std-header orig-src='shore' incl-file-exclusion='STRACE_H'>

 $Id: strace.h,v 1.8.2.2 2009/08/24 21:22:19 nhall Exp $

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

#ifndef STRACE_H
#define STRACE_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/


/* RESOURCE TRACING */

class _strace_t;
ostream &operator<<(ostream &s, const class _strace_t &);


class strace_t {
	enum {
		default_alloc_hint = 64
	};

	static	unsigned long id_generator();	// unique event IDs
	int	_alloc_hint;		// allocation strategy hint

	/* the first element of each chunk of allocated memory
	   is retained so they can be deleted */
	_strace_t	*_heads;

	_strace_t	*_free;
	_strace_t	*_held;

	bool	more();
	void	_release(_strace_t *);	// XXX kludgomatic

public:
	strace_t(int hint = default_alloc_hint);
	~strace_t();

	void		push(const char *name,
			     const void *id,
			     bool isLatch = false);
	void		pop(const void *id);

	_strace_t	*get();
	_strace_t	*get(const char *name,
			     const void *id,
			     bool isLatch = false);
	void		release(_strace_t *);


	ostream		&print(ostream &) const;
};

ostream &operator<<(ostream &s, const class strace_t &);

/*<std-footer incl-file-exclusion='STRACE_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
