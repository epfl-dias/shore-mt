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

private:
    
public:
    // to be kept in data array
    struct partition_pair {
	cvec_t key;       // start key of a partition
	lpid_t root;     // the subtree root that corresponds the partition
    };

    // forms the key_ranges_map from the partitions_pairs kept in this page
    void fill_ranges_map(key_ranges_map& partitions);
    
    // stores the partitions' info from key_ranges_map in this page
    void fill_page(key_ranges_map& partitions);
    
};

void ranges_p::fill_ranges_map(key_ranges_map& partitions)
{
    // TODO: implement
}

void ranges_p::fill_page(key_ranges_map& partitions)
{
    // TODO: implement
}
