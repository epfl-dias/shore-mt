/*<std-header orig-src='shore' incl-file-exclusion='ZVEC_T_H'>

 $Id: zvec_t.h,v 1.10.2.2 2009/10/30 23:51:12 nhall Exp $

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

#ifndef ZVEC_T_H
#define ZVEC_T_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

class zvec_t : public vec_t {
public:
    zvec_t() : vec_t(zero_location,0)        {};
    zvec_t(size_t l) : vec_t(zero_location, l)        {};
    zvec_t &put(size_t l) { reset().put(zero_location,l); return *this; }
private:
    // disabled
    zvec_t(const zvec_t&) :vec_t(zero_location, 0)  {}
    zvec_t &operator=(zvec_t);
    // disabled other constructors from vec_t
    zvec_t(const cvec_t& v1, const cvec_t& v2);/* {} */
    zvec_t(const void* p, size_t l); // {}
    zvec_t(const vec_t& v, size_t offset, size_t limit); // {}
};

/*<std-footer incl-file-exclusion='ZVEC_T_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
