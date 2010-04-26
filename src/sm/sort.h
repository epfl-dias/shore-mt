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

/*<std-header orig-src='shore' incl-file-exclusion='SORT_H'>

 $Id: sort.h,v 1.28.2.4 2009/10/08 22:36:56 nhall Exp $

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

#ifndef SORT_H
#define SORT_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#include <lexify.h>
#include <sort_s.h>

struct sort_desc_t;
class run_scan_t;

#ifdef __GNUG__
#pragma interface
#endif

//
// chunk list for large object buffer
//
struct s_chunk  {

    char* data;
    s_chunk* next;

    NORET s_chunk() { data = 0; next = 0; };
    NORET s_chunk(w_base_t::uint4_t size, s_chunk* tail) { 
              data = new char[size];
              next = tail;
        };
    NORET ~s_chunk() { delete [] data; };
};

class chunk_mgr_t {

private:
    s_chunk* head;

    void _free_all() { 
          s_chunk* curr;
          while (head) {
        curr = head;
        head = head->next;
        delete curr;
          }
      };

public:

    NORET chunk_mgr_t() { head = 0; };
    NORET ~chunk_mgr_t() { _free_all(); };

    void  reset() { _free_all(); head = 0; };

    void* alloc(w_base_t::uint4_t size) {
          s_chunk* curr = new s_chunk(size, head);
          head = curr;
           return (void*) curr->data;
      };
};

#       define PROTOTYPE(_parms) _parms

#ifndef PFCDEFINED

#define PFCDEFINED
typedef int  (*PFC) PROTOTYPE((w_base_t::uint4_t kLen1, 
        const void* kval1, w_base_t::uint4_t kLen2, const void* kval2));

#endif

class sort_stream_i : public smlevel_top, public xct_dependent_t 
{

  friend class ss_m;

  public:

    NORET    sort_stream_i();
    NORET    sort_stream_i(const key_info_t& k,
                const sort_parm_t& s, uint est_rec_sz=0);
    NORET    ~sort_stream_i();

    static PFC  get_cmp_func(key_info_t::key_type_t type, bool up);

    void    init(const key_info_t& k, const sort_parm_t& s, uint est_rec_sz=0);
    void    finish();

    rc_t    put(const cvec_t& key, const cvec_t& elem);

    rc_t    get_next(vec_t& key, vec_t& elem, bool& eof);

    bool    is_empty() { return empty; }
    bool    is_sorted() { return sorted; }

  private:
    void    set_file_sort() { _file_sort = true; _once = false; }
    void    set_file_sort_once(sm_store_property_t prop) 
                {
                    _file_sort = true; _once = true; 
                    _property = prop; 
                }
    rc_t    file_put(const cvec_t& key, const void* rec, uint rlen,
                uint hlen, const rectag_t* tag);

    rc_t    file_get_next(vec_t& key, vec_t& elem, 
                w_base_t::uint4_t& blen, bool& eof);

    rc_t        flush_run();        // sort and flush one run

    rc_t    flush_one_rec(const record_t *rec, rid_t& rid,
                const stid_t& out_fid, file_p& last_page,
                bool to_final_file);

    rc_t    remove_duplicates();    // remove duplicates for unique sort
    rc_t    merge(bool skip_last_pass);

    void    xct_state_changed( xct_state_t old_state, xct_state_t new_state);

    key_info_t        ki;        // key info
    sort_parm_t       sp;        // sort parameters
    sort_desc_t*      sd;        // sort descriptor

    bool              sorted;        // sorted flag
    bool              eof;        // eof flag
    bool              empty;        // empty flag
    const record_t*   old_rec;    // used for sort unique
  
    bool              _file_sort;    // true if sorting a file

    int2_t*           heap;           // heap array
    int               heap_size;     // heap size
    run_scan_t*       sc;           // scan descriptor array    
    w_base_t::uint4_t num_runs;      // # of runs for each merge
    int               r;           // run index

    chunk_mgr_t       buf_space;    // in-memory storage

    // below vars used for speeding up sort if whole file fits in memory
    bool              _once;        // one pass write to result file
    sm_store_property_t _property;    // property for the result file
};

class file_p;

//
// run scans
//
class run_scan_t {
    lpid_t pid;         // current page id
    file_p* fp;         // page buffer (at most fix two pages for unique sort)
    int2_t   i;           // toggle between two pages
    int2_t   slot;        // slot for current record
    record_t* cur_rec;  // pointer to current record
    bool eof;         // end of run
    key_info_t kinfo;   // key description (location, len, type)
    int2_t   toggle_base; // default = 1, unique sort = 2
    bool   single;    // only one page
    bool   _unique;    // unique sort

public:
    PFC cmp;

    NORET run_scan_t();
    NORET ~run_scan_t();

    rc_t init(rid_t& begin, PFC c, const key_info_t& k, bool unique);
    rc_t current(const record_t*& rec);
    rc_t next(bool& eof);

    bool is_eof()   { return eof; }

    friend bool operator>(run_scan_t& s1, run_scan_t& s2);
    friend bool operator<(run_scan_t& s1, run_scan_t& s2);

    const lpid_t& page() const { return pid; }
};

/*<std-footer incl-file-exclusion='SORT_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
