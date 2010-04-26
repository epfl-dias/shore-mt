/*<std-header orig-src='shore' incl-file-exclusion='LID_S_H'>

 $Id: lid_s.h,v 1.30.2.2 2009/10/08 20:51:25 nhall Exp $

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

#ifndef LID_S_H
#define LID_S_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif

/* 
 * The vol_lid_info_t struct contains information about a volume
 * that has a logical ID facility.  One of these structs is
 * stored in lid_m for each volume mounted.
 */
#define VOL_LID_INFO_T
class vol_lid_info_t {
public:

    lvid_t      lvid;
    vid_t	vid;

    /*
     * Serial number allocation information
     * max_id is used only by the server which manages the volume
     * The first array element refers to IDs for local references
     * (those on the same volume).
     * The second array element refers to IDs for remote references
     * (those on a different volume).
     *
     */
    w_link_t  	link;

    lvid_t hash_key() { return lvid; }
};


/*<std-footer incl-file-exclusion='LID_S_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
