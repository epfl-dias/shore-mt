/* -*- mode:C++; c-basic-offset:4 -*-
   Shore-kits -- Benchmark implementations for Shore-MT

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

/** @file:   key_ranges_map.h
 *
 *  @brief:  Definition of the page type which is used to store the key ranges partitions
 *           used by baseline MRBTrees.
 *
 *  @date:   August 2010
 *
 *  @author: Pinar Tozun (pinar)
 *  @author: Ippokratis Pandis (ipandis)
 *  @author: Ryan Johnson (ryanjohn)
 */

#ifndef RANGES_P_H
#define RANGES_P_H

#include "w_defines.h"

class ranges_p : public page_p {

public:

    MAKEPAGE(ranges_p, page_p, 1); 

    // forms the key_ranges_map from the partitions_pairs kept in this page
    rc_t fill_ranges_map(key_ranges_map& partitions);
    
    // stores the partitions' info from key_ranges_map in this page
    rc_t fill_page(key_ranges_map& partitions);    

    // stores the newly added partition info
    rc_t add_partition(cvec_t& key, lpid_t root);

    // deletes the newly deleted partition
    rc_t delete_partition(cvec_t& key, lpid_t root);

};

rc_t ranges_p::fill_ranges_map(key_ranges_map& partitions)
{
    // get the contents of the header
    char* hdr_ptr = (char*) page_p::tuple_addr(0);
    char* hdr = (char*) malloc(sizeof(int));
    memcpy(hdr_ptr, hdr, sizeof(int));
    uint4_t num_pairs = *((uint4_t*)hdr);
    free(hdr);
    //
    for(uint4_t i=1; i < num_pairs; i++) {
	// get the contents of the slot
	char* pair = (char*) page_p::tuple_addr(i);
	cvec_t pair_vec;
	pair_vec.put(pair, page_p::tuple_size(i));
	// split it to its key-root parts
	cvec_t root_vec;
	cvec_t key;
	pair_vec.split(sizeof(lpid_t), root_vec, key);
	char* root = (char*) malloc(sizeof(lpid_t));
	root_vec.copy_to(root);
	lpid_t root_id = *((lpid_t*)root);
	free(root);
	// add this pair to the partitions map
	partitions.addPartition(key, root_id);
    }

    return RCOK;
}

rc_t ranges_p::fill_page(key_ranges_map& partitions)
{
    key_ranges_map::keysIter iter;
    map<char*, lpid_t, cmp_str_greater> partitions_map = partitions.getMap();
    uint4_t i = 1;
    for(iter = partitions_map.begin(); iter != partitions_map.end(); iter++, i++) {
	cvec_t v;
	// put subroot
	char* subroot = (char*)(&(iter->second));
	v.put(subroot, sizeof(lpid_t));
	// put key
	v.put(iter->first, sizeof(iter->first));
	// add this key-subroot pair to page's data
	W_DO(page_p::splice(i, 0, v.size(), v));
    }
    // header of the page keeps how many startKey-root pairs are stored
    cvec_t hdr;
    hdr.put((char*)(&i), sizeof(uint4_t));
    W_DO(page_p::overwrite(0, 0, hdr));

    return RCOK;
}

rc_t ranges_p::add_partition(cvec_t& key, lpid_t root) 
{    
    // TODO: you can perform header related stuff as seperate private functions
    // get the contents of the header
    char* hdr_ptr = (char*) page_p::tuple_addr(0);
    char* old_hdr = (char*) malloc(sizeof(int));
    memcpy(hdr_ptr, old_hdr, sizeof(int));
    uint4_t num_pairs = *((uint4_t*)old_hdr);
    free(old_hdr);

    // update header
    num_pairs++;

    // add the partition
    cvec_t v;
    // put subroot
    char* subroot = (char*)(&root);
    v.put(subroot, sizeof(lpid_t));
    // put key
    v.put(key);
    // add this key-subroot pair to page's data
    W_DO(page_p::splice(num_pairs, 0, v.size(), v));

    cvec_t hdr;
    hdr.put((char*)(&num_pairs), sizeof(uint4_t));
    W_DO(page_p::overwrite(0, 0, hdr));

    return RCOK;
}

rc_t ranges_p::delete_partition(cvec_t& key, lpid_t root)
{
    // TODO: implement
    return RCOK;
}
#endif
