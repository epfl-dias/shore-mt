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

/*<std-header orig-src='shore' incl-file-exclusion='W_RC_H'>

 $Id: w_cuckoohashfuncs.h,v 1.1.2.2 2009/12/03 00:19:43 nhall Exp $

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

#ifndef W_CUCKOOHASHFUNCS_H
#define  W_CUCKOOHASHFUNCS_H

class uhash 
{
public:
    typedef w_base_t::uint8_t u64;
    u64 a;
    u64 b;
private:
    static const int bitshift= 16;
    static u64 init() 
    {
		return ((u64) sthread_t::rand() << 32) | sthread_t::rand(); 
    }
public:
    uhash() : a(init()), b(init()) { }
    ~uhash() { }
    u64 operator()(u64 x) const { return (a*x+b) >> bitshift; }
};

/* Note from FRJ re: conversation with Ken Ross:
Oh, and he said the best way to use a multiplicative hash 
is: h(x) = (a*x+b)>>16. Apparently bits 16-31 are the best, 
but bits 32-47 are still far better than 0-15 or 48-63.
*/

struct hash1 {
    uhash a;
#ifdef ARCH_LP64
    unsigned operator()(unsigned x) const { return a(x); }
#else
    w_base_t::uint8_t operator()(unsigned x) const { return w_base_t::uint8_t (a(x)); }
#endif
};

struct hash2 {
    uhash a;
    uhash b;
#ifdef ARCH_LP64
    unsigned operator()(unsigned x) const { return a(x) ^ b(x); }
#else
    w_base_t::uint8_t operator()(unsigned x) const { 
			return w_base_t::uint8_t (a(x) ^ b(x)); }
#endif
};

#endif
