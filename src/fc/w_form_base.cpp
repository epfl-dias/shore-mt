/*<std-header orig-src='shore'>

 $Id: w_form_base.cpp,v 1.1.2.3 2010/03/19 22:17:19 nhall Exp $

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

/*
 *   This software is Copyright 1989, 1991, 1992, 1993, 1994, 1998,
 *                              2003, 2004 by:
 *
 *    Josef Burger    <bolo@cs.wisc.edu>
 *
 *   All Rights Reserved.
 *
 *   This software may be freely used as long as credit is given
 *   to the author and this copyright is maintained.
 */

#include "w_form.h"

// XXX an alias would be better, but this works
#define    w_form    form

// really want this from the include, but compatability problems
// with anything wanting to use form() are an issue.
extern const char *w_form(const char *, ...);


#define FMT(name, type, format)    \
    const char *name(type t, int length)    \
    {    \
        return w_form(format, length, t);    \
    }

FMT(dec, int, "%*d")
FMT(dec, unsigned, "%*u")
FMT(dec, long, "%*ld")
FMT(dec, unsigned long, "%*lu")

FMT(hex, int, "%*x")
FMT(hex, unsigned, "%*x")
FMT(hex, long, "%*lx")
FMT(hex, unsigned long, "%*lx")

FMT(oct, int, "%*o")
FMT(oct, unsigned, "%*o")
FMT(oct, long, "%*lo")
FMT(oct, unsigned long, "%*lo")


