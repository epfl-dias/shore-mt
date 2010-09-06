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
 *  @brief:  Definition of a map of key ranges to partitions used by
 *           baseline MRBTrees.
 *
 *  @notes:  The keys are Shore-mt cvec_t. Thread-safe.  
 *
 *  @date:   July 2010
 *
 *  @author: Pinar Tozun (pinar)
 *  @author: Ippokratis Pandis (ipandis)
 *  @author: Ryan Johnson (ryanjohn)
 */


#ifndef _KEY_RANGES_MAP_H
#define _KEY_RANGES_MAP_H

#include <iostream>
#include <cstring> 
#include <map>
#include <vector>
#include <utility>

#include <math.h>

//#include "util.h"

//#include "sm_vas.h"

#include "w_defines.h"

using namespace std;


/******************************************************************** 
 *
 * @brief: Error codes returned by MRBTrees
 *
 ********************************************************************/

enum {
    mrb_PARTITION_NOT_FOUND    = 0x830001,
    mrb_LAST_PARTITION    = 0x830002,
    mrb_KEY_BOUNDARIES_NOT_ORDERED    = 0x830003
};



// For map<char*,lpid_t> to compare char*
struct cmp_str_greater
{
    bool operator()(char const *a, char const *b)
    {
        return strcmp(a,b) > 0;
    }
};


/******************************************************************** 
 *
 * @class: key_ranges_map
 *
 * @brief: A map of key ranges to partitions. This structure is used
 *         by the multi-rooted B-tree (MRBTree). 
 *
 * @note:  The specific implementation indentifies each partition through
 *         the lpid_t of the root of the corresponding sub-tree. Hence,
 *         this implementation is for the Baseline MRBTree (non-DORA).
 *
 ********************************************************************/

class key_ranges_map
{
private:

    typedef cvec_t Key;

    // range_init_key -> root of the corresponding subtree
    map<char*, lpid_t, cmp_str_greater > _keyRangesMap;
    uint _numPartitions;
    char* _minKey;
    char* _maxKey;

protected:

    // for thread safety multiple readers/single writer lock
    occ_rwlock _rwlock;

    // Splits the partition where "key" belongs to two partitions. The start of 
    // the second partition is the "key".
    virtual w_rc_t _addPartition(char* keyS, lpid_t& oldRoot, lpid_t& newRoot);

    // Delete the partition where "key" belongs, by merging it with the 
    // previous partition
    virtual w_rc_t _deletePartitionByKey(char* keyS, lpid_t& root);

public:

    typedef map<char*, lpid_t>::iterator keysIter;
   
    //// Construction ////
    // Calls one of the initialization functions
    key_ranges_map();
    key_ranges_map(const Key& minKey, const Key& maxKey, const uint numParts);
    virtual ~key_ranges_map();


    ////  Initialization ////
    // The default initialization creates numParts partitions of equal size

    // Makes equal length partitions from scratch
    uint makeEqualPartitions(const Key& minKey, const Key& maxKey, const uint numParts, 
			     vector<lpid_t> roots);


    ////  Map management ////

    // Splits the partition where "key" belongs to two partitions. The start of 
    // the second partition is the "key".
    w_rc_t addPartition(const Key& key, lpid_t& oldRoot, lpid_t& newRoot);

    // Deletes the partition where "key" belongs by merging it with the previous 
    // partition
    w_rc_t deletePartitionByKey(const Key& key, lpid_t& root);

    // Deletes the given partition (identified by the pid), by merging it with 
    // the previous partition
    w_rc_t deletePartition(lpid_t pid);

    // Gets the partition id of the given key.
    //
    // @note: In the baseline version of the MRBTree each partition is identified
    //        by the lpid_t of the root of the corresponding sub-tree. In the 
    //        DORA version each partition can also be identified by a partition-id 
    w_rc_t getPartitionByKey(const Key& key, lpid_t& pid);
    w_rc_t operator()(const Key& key, lpid_t& pid);

    // Returns the list of partitions that cover: 
    // [key1, key2], (key1, key2], [key1, key2), or (key1, key2) ranges
    w_rc_t getPartitions(const Key& key1, bool key1Included,
                         const Key& key2, bool key2Included,                         
                         vector<lpid_t>& pidVec);

    // Returns the range boundaries of a partition in a pair
    w_rc_t getBoundaries(lpid_t pid, pair<cvec_t, cvec_t>& keyRange);
    // Returns the range boundaries of a partition in start&end key
    w_rc_t getBoundaries(lpid_t pid, cvec_t& startKey, cvec_t& endKey);


    // Returns a vector with the key boundaries for all the partitions
    w_rc_t getBoundariesVec(vector< pair<char*,char*> >& keyBoundariesVec);

    // Setters
    // TODO: decide what to do after you set these values, what seems reasonable to me
    // is change the partition structure as less as possible because later with dynamic load
    // balancing things should adjust properly, also check the corner cases for min&max kes
    void setNumPartitions(uint numPartitions);
    void setMinKey(const Key& minKey);
    void setMaxKey(const Key& maxKey);

    // Getters
    uint getNumPartitions() const;
    char* getMinKey() const;
    char* getMaxKey() const;
    map<char*, lpid_t, cmp_str_greater> getMap() const;

    // for debugging
    void printPartitions(); 


private:

    // Helper functions
    uint _distributeSpace(const char* A, const char* B, const int sz, 
                          const uint parts, char** subparts);
    
}; // EOF: key_ranges_map

#endif
