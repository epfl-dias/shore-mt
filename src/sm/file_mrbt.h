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

/*<std-header orig-src='shore' incl-file-exclusion='FILE_H'>

 $Id: file.h,v 1.102 2010/06/08 22:28:55 nhall Exp $

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

#ifndef FILE_H
#define FILE_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif

class sdesc_t; // forward

#ifndef FILE_S_H
#include "file_s.h"
#endif

struct file_stats_t;
struct file_pg_stats_t;
struct lgdata_pg_stats_t;
struct lgindex_pg_stats_t;
class  pin_i;

class file_m; // forward

/*
 * Page type for a file of records.
 */
class file_mrbt_p : public file_p {
    friend class file__m;
    friend class pin_i;

public:
    // free space on file_p is page_p less file_p header slot
    enum { data_sz = page_p::data_sz - align(sizeof(file_p_hdr_mrbt_t)) - 
                                                     sizeof(slot_t),
           min_rec_size = sizeof(rectag_t) + sizeof(slot_t)
           };

    MAKEPAGE(file_mrbt_p, page_p, 1);          // Macro to install basic functions from page_p.

#define DUMMY_CLUSTER_ID 0

private:
    rc_t                 set_owner(const lpid_t& new_owner);
    rc_t                 get_owner(lpid_t &owner) const;

    // -- mrbt
    rc_t                 shift(slotid_t snum, file_mrbt_p* rsib);
    //
};

/*
 *  This is the header specific to a file page when mrbt design is used without regular heap files.
 *  It is stored in  the first slot on the page.
 */
struct file_p_hdr_mrbt_t {
    clust_id_t    cluster;
	// See DUMMY_CLUSTER_ID in file.cpp, page.h 
	// It is the default value of the cluster id here.
    lpid_t owner; // id of the btree leaf page that points to this heap page
};

inline rc_t file_mrbt_p::set_owner(const lpid_t& owner)
{
    cvec_t owner_vec;
    owner_vec.put(&owner, sizeof(lpid_t));
    W_DO(page_p::overwrite(0, sizeof(file_p_hdr_t), owner_vec));    
}

inline rc_t file_mrbt_p::get_owner(lpid_t& owner) const
{
    owner = *((lpid_t*)page_p::tuple_addr(0));
}
/*<std-footer incl-file-exclusion='FILE_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
