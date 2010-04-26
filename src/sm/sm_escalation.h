/*<std-header orig-src='shore' incl-file-exclusion='SM_ESCALATION_H'>

 $Id: sm_escalation.h,v 1.10.2.4 2010/01/28 04:54:15 nhall Exp $

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

#ifndef SM_ESCALATION_H
#define SM_ESCALATION_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif

class sm_escalation_t {
public:
    NORET        sm_escalation_t(
                    w_base_t::int4_t p = dontEscalate,
                    w_base_t::int4_t s = dontEscalate, 
                    w_base_t::int4_t v = dontEscalate);
    NORET        ~sm_escalation_t(); 
private:
    w_base_t::int4_t _p;
    w_base_t::int4_t _s;
    w_base_t::int4_t _v; // save old values
    // disable
    sm_escalation_t(const sm_escalation_t&);
    sm_escalation_t& operator=(const sm_escalation_t&);
};

/*<std-footer incl-file-exclusion='SM_ESCALATION_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
