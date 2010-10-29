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

/** @file:   key_ranges_map.cpp
 *
 *  @brief:  Implementation of a map of key ranges to partitions used by
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

//#include "w_defines.h"

#ifdef __GNUG__
#           pragma implementation "key_ranges_map.h"
#endif


#include "key_ranges_map.h"


/****************************************************************** 
 *
 * Construction/Destruction
 *
 * @brief: The constuctor calls the default initialization function
 * @brief: The destructor needs to free all the malloc'ed keys
 *
 ******************************************************************/

key_ranges_map::key_ranges_map()
    : _minKey(NULL),_maxKey(NULL),_numPartitions(0)
{
}

key_ranges_map::key_ranges_map(char* minKey, 
                               char* maxKey, 
                               const uint numParts, 
                               const bool physical)
    : _minKey(NULL),_maxKey(NULL),_numPartitions(0)
{
    if (physical) {
        w_assert1(0); // Not implemented
    }
    else {
        nophy_equal_partitions(minKey,maxKey,numParts);
    }
}

key_ranges_map::key_ranges_map(const key_ranges_map& rhs)
    : _keyRangesMap(rhs._keyRangesMap),_numPartitions(rhs._numPartitions)
{
    _minKey = strdup(rhs._minKey);
    _maxKey = strdup(rhs._maxKey);
}

key_ranges_map::~key_ranges_map()
{
    // Delete the allocated keys in the map
    KRMapIt iter;
    uint i=0;

    _rwlock.acquire_write();

    DBG(<<"Destroying the ranges map: ");
    for (iter = _keyRangesMap.begin(); iter != _keyRangesMap.end(); ++iter, ++i) {
	if (iter->first) { 
            free (iter->first); 
        }
    }

    // Delete the boundary keys
    if(_minKey) {
	free (_minKey);
	_minKey = NULL;
    }
    if(_maxKey) {
	free (_maxKey);
	_maxKey = NULL;
    }

    _rwlock.release_write();    
}


/****************************************************************** 
 *
 * @fn:     nophy_equal_partitions()
 *
 * @brief:  Makes equal length partitions from scratch without any
 *          physical changes
 *
 * @return: The number of partitions that were actually created
 *
 ******************************************************************/

uint key_ranges_map::nophy_equal_partitions(char* minKey, 
                                            char* maxKey,
                                            const uint numParts)
{
#warning PT: nophy_equal_partitions works only for ints now! has to be re-written

    assert (_minKey);
    assert (_maxKey);
    assert(numParts);

    _minKey = (char*)malloc(sizeof(char)*sizeof(int));
    memcpy(_minKey,minKey,sizeof(int));
    _maxKey = (char*)malloc(sizeof(char)*sizeof(int));
    memcpy(_maxKey,maxKey,sizeof(int));
    
    // Calculate the array for lower_bounds 
    int size = sizeof(int);
    int lower_bound = *(int*)minKey;
    int upper_bound = *(int*)maxKey;
 
    double space = upper_bound - lower_bound;
    double diff = (double)space / (double)numParts;

    uint partsCreated = 1; // In case it cannot divide to numParts partitions
    double current_key = lower_bound;

    char** subParts = (char**)malloc(numParts*sizeof(char*));
    subParts[0] = (char*) malloc(size);
    memset(subParts[0],0,size);
    memcpy(subParts[0],(char*) (&lower_bound),size);

    while(partsCreated < numParts) {
	current_key = current_key + diff;
	int startKey = current_key;
	subParts[partsCreated] = (char*) malloc(size);
        memset(subParts[partsCreated],0,size);
	memcpy(subParts[partsCreated],(char*) (&startKey),size);
	partsCreated++;
    }

    stid_t astid;
    

    // ---------------------
    _rwlock.acquire_write();
    
    // put the partitions to map
    _keyRangesMap.clear();
    for(uint4_t i = 0; i < partsCreated; i++) { 
        lpid_t alpid(astid,i);
        _keyRangesMap[subParts[i]] = alpid;
    }

    // Should have created at least one partition
    _numPartitions = partsCreated;

    _rwlock.release_write();
    // ---------------------

    printPartitions();

    free (subParts);
    return (_numPartitions);
}



/****************************************************************** 
 *
 * @fn:      _distributeSpace()
 *
 * @brief:   Helper function, which tries to evenly distribute the space between
 *           two strings to a certain number of subspaces
 *
 * @param:   const char* A    - The beginning of the space
 * @param:   const char* B    - The end of the space
 * @param:   const int sz    - The common size of the two strings
 * @param:   const uint parts - The number of partitions which should be created
 * @param:   char** subParts  - An array of strings with the boundaries of the
 *                              distribution
 * 
 * @returns: The number of partitions actually created
 *
 ******************************************************************/

uint key_ranges_map::_distributeSpace(const char* A, 
                                      const char* B, 
                                      const int sz, 
                                      const uint partitions, 
                                      char** subParts)
{
    
    assert (strcmp(A,B)<0); // It should A<B

    double numASCII = 95;
    uint startASCII = 31;
    int totalSize = strlen(A);

    // compute the space between two strings
    uint space = 0;
    for(int i=totalSize-1, j=0; i>totalSize-sz-1; i--,j++)
	space = space + ((int) B[i] - (int) A[i]) * (int) pow(numASCII, j);

    // find the approximate range each partitions should keep
    uint range = space / partitions;
       
    // compute the partitions
    char* currentKey = (char*) malloc(sz);
    char* nextKey = (char*) malloc(sz);
    strcpy(nextKey, A);
    
    uint currentDest;
    int currentCharPos;
    uint div;
    uint rmd;
    uint pcreated = 0;
    
    // first partition
    subParts[pcreated] = (char*) malloc(totalSize+1);
    strcpy(subParts[pcreated], nextKey);

    // other partitions
    for(pcreated = 1; pcreated < partitions; pcreated++) {
	strcpy(currentKey, nextKey);
	currentCharPos=totalSize-1;
	div = range;

	do {
	    currentDest = (int) currentKey[currentCharPos] - startASCII + div;
	    div = currentDest / numASCII;
	    rmd = currentDest - div*numASCII;
	    nextKey[currentCharPos] = (char) rmd + startASCII;
	    currentCharPos--;
	} while(div > numASCII);
   
	if(div != 0) {
	    nextKey[currentCharPos] = (char) ((int) currentKey[currentCharPos] + div);
	    currentCharPos--;
	}
      
	for(; currentCharPos >= totalSize-sz-1; currentCharPos--) {
	    nextKey[currentCharPos] = currentKey[currentCharPos];
	}

	subParts[pcreated] = (char*) malloc(totalSize+1);
	strcpy(subParts[pcreated], nextKey);
    }

    free(currentKey);
    free(nextKey);
    
    return pcreated;
}



/****************************************************************** 
 *
 * @fn:    addPartition()
 *
 * @brief: Splits the partition where "key" belongs to two partitions. 
 *         The start of the second partition is the "key".
 *
 * @param:   cvec_t key     - The starting key of the new partition (Input)
 * @param:   lpid_t newRoot - The root of the sub-tree which maps to the new partition (Input)
 *
 *
 ******************************************************************/

w_rc_t key_ranges_map::_addPartition(char* keyS, lpid_t& newRoot)
{
    w_rc_t r = RCOK;

    _rwlock.acquire_write();

    if ( (_minKey == NULL || strcmp(_minKey, keyS) <= 0) &&
	 (_maxKey == NULL || strcmp(keyS, _maxKey) <= 0) ) {
	_keyRangesMap[keyS] = newRoot;
        _numPartitions++;
    }
    else {
        r = RC(mrb_OUT_OF_BOUNDS);
    }

    _rwlock.release_write();

    return (r);
}

w_rc_t key_ranges_map::addPartition(const Key& key, lpid_t& newRoot)
{
    w_rc_t r = RCOK;
    //if(key.count() == 1) {
    //	r = _addPartition((char*)key._pair[0].ptr, newRoot);
    //_keyCounts.insert((char*)key._pair[0].ptr);
    //} else {
    char* keyS = (char*) malloc(key.size());
    key.copy_to(keyS);
    r = _addPartition(keyS, newRoot);
    //}
    return (r);
}


/****************************************************************** 
 *
 * @fn:    deletePartition{,ByKey}()
 *
 * @brief: Deletes a partition, by merging it with the partition which
 *         is before that, based either on a partition identified or
 *         a key.
 *
 * @param: cvec_t key    - The key which is in the partition to be deleted
 *                         (Input for deletePartitionByKey) 
 * @param: lpid_t root1  - The root of the merged partition which has the lower key values (Output)
 * @param: lpid_t root2  - The root of the merged partition which has the lower key values (Output)
 *                         (Also input for deletePartition)
 * @param: cvec_t startKey1 - The start key of the partition that maps to root1 (Output)
 * @param: cvec_t startKey2 - The start key of the partition that maps to root2 (Output)
 *
 * @note:  Here the startKey1 < startKey2 but in the map they startKey2
 *         comes before startKey1.
 ******************************************************************/

w_rc_t key_ranges_map::_deletePartitionByKey(char* keyS,
					     lpid_t& root1, lpid_t& root2,
					     Key& startKey1, Key& startKey2)
{
    w_rc_t r = RCOK;

    _rwlock.acquire_write();

    KRMapIt iter = _keyRangesMap.lower_bound(keyS);

    if(iter == _keyRangesMap.end()) {
	// partition not found, return an error
        _rwlock.release_write();
	return (RC(mrb_PARTITION_NOT_FOUND));
    }
    root2 = iter->second;
    ++iter;
    if(iter == _keyRangesMap.end()) {
	--iter;
	if(iter == _keyRangesMap.begin()) {
	    // partition is the last partition, cannot be deleted
            _rwlock.release_write();
	    return (RC(mrb_LAST_PARTITION));
	}
	root1 = root2;
	startKey1.put(iter->first, sizeof(iter->first));
    }
    else {
	startKey1.put(iter->first, sizeof(iter->first));
	root1 = iter->second;
    }
    --iter;
    startKey2.put(iter->first, sizeof(iter->first));
    root2 = iter->second;
    _keyRangesMap.erase(iter);
    _numPartitions--;

    _rwlock.release_write();
    return (r);
}

w_rc_t key_ranges_map::deletePartitionByKey(const Key& key,
					    lpid_t& root1, lpid_t& root2,
					    Key& startKey1, Key& startKey2)
{
    w_rc_t r = RCOK;
    if(key.count() == 1) {
	r = _deletePartitionByKey((char*)key._pair[0].ptr, root1, root2, startKey1, startKey2);
	//if(!r.is_error()) {
	//    _keyCounts.erase(_keyCounts.find((char*)key._pair[0].ptr));
	//}
    } else {
	char* keyS = (char*) malloc(key.size());
	key.copy_to(keyS);
	r = _deletePartitionByKey(keyS, root1, root2, startKey1, startKey2);
	free (keyS);
    }
    return (r);
}

w_rc_t key_ranges_map::deletePartition(lpid_t& root1, lpid_t& root2,
				       Key& startKey1, Key& startKey2)
{
    w_rc_t r = RCOK;
    bool bFound = false;

    KRMapIt iter;
    _rwlock.acquire_read();
    for (iter = _keyRangesMap.begin(); iter != _keyRangesMap.end(); ++iter) {
        if (iter->second == root2) {
            bFound = true;
	    break;
        }
    }
    _rwlock.release_read();

    if (bFound) {
        r = _deletePartitionByKey(iter->first, root1, root2, startKey1, startKey2);
    } 
    else {
	return (RC(mrb_PARTITION_NOT_FOUND));
    }

    return (r);
}


/****************************************************************** 
 *
 * @fn:    getPartitionByKey()
 *
 * @brief: Returns the root page id, "pid", of the partition which a
 *         particular "key" belongs to
 *
 * @param: cvec_t key    - Input
 * @param: lpid_t pid    - Output
 *
 ******************************************************************/

w_rc_t key_ranges_map::getPartitionByKey(const Key& key, lpid_t& pid)
{
    _rwlock.acquire_read();
    KRMapIt iter;
    if(key.count() == 1) {
	iter = _keyRangesMap.lower_bound((char*)key._pair[0].ptr);
    } else {
	char* keyS = (char*) malloc(key.size());
	key.copy_to(keyS);
	iter = _keyRangesMap.lower_bound(keyS);
	free (keyS);
    }
    if(iter == _keyRangesMap.end()) {
	// the key is not in the map, returns error.
        _rwlock.release_read();
	return (RC(mrb_PARTITION_NOT_FOUND));
    }
    pid = iter->second;
    _rwlock.release_read();
    return (RCOK);    
}


/****************************************************************** 
 *
 * @fn:    getPartitionByKey()
 *
 * @brief: Returns the root page id, "pid", of the partition which a
 *         particular "keyS" belongs to
 *
 * @param: char* keyS    - Input
 * @param: lpid_t pid    - Output
 *
 ******************************************************************/

w_rc_t key_ranges_map::getPartitionByKey(char* keyS, lpid_t& pid)
{
    _rwlock.acquire_read();
    KRMapIt iter = _keyRangesMap.lower_bound(keyS);
    if(iter == _keyRangesMap.end()) {
	// the key is not in the map, returns error.
        _rwlock.release_read();
	return (RC(mrb_PARTITION_NOT_FOUND));
    }
    pid = iter->second;
    _rwlock.release_read();
    return (RCOK);    
}


/****************************************************************** 
 *
 * @fn:    getPartitions()
 *
 * @param: cvec_t key1    - The start key for the partitions list (Input)
 * @param: bool key1Included - Indicates whether key1 should be included or not (Input)
 * @param: cvec_t key2    - The end key for the partitions list (Input)
 * @param: bool key2Included - Indicates whether key2 should be included or not (Input)
 * @param: vector<lpid_t> pidVec  - The list of partitions' root ids (Output)
 *
 * @brief: Returns the list of partitions that cover one of the key ranges:
 *         [key1, key2], (key1, key2], [key1, key2), or (key1, key2) 
 *
 ******************************************************************/

w_rc_t key_ranges_map::getPartitions(const Key& key1, bool key1Included,
                                     const Key& key2, bool key2Included,
                                     vector<lpid_t>& pidVec) 
{
    w_rc_t r = RCOK;
    
    if(key2 <= key1) {
	// return error if the bounds are not given correctly
	return (RC(mrb_KEY_BOUNDARIES_NOT_ORDERED));
    }  

    KRMapIt iter1;
    KRMapIt iter2;
    char* key2S = NULL;
    
    _rwlock.acquire_read();

    // get start key
    if(key1.count() == 1) {
	iter1 = _keyRangesMap.lower_bound((char*)key1._pair[0].ptr);
    } else {
	char* key1S = (char*) malloc(key1.size());
	key1.copy_to(key1S);
	iter1 = _keyRangesMap.lower_bound(key1S);
    }

    // get end key
    if(key2.count() == 1) {
	iter2 = _keyRangesMap.lower_bound((char*)key2._pair[0].ptr);
    } else {
	key2S = (char*) malloc(key2.size());
	key1.copy_to(key2S);
	iter2 = _keyRangesMap.lower_bound(key2S);
    }

    if (iter1 == _keyRangesMap.end() || iter2 == _keyRangesMap.end()) {
	// at least one of the keys is not in the map, returns error.
        _rwlock.release_read();
	return (RC(mrb_PARTITION_NOT_FOUND));
    }
    while (iter1 != iter2) {
	pidVec.push_back(iter1->second);
	iter1--;
    }
    // check for !key2Included
    if(key2Included ||
       (key2.count() == 1 && umemcmp(iter1->first,key2._pair[0].ptr,key2.size())!=0) ||
       umemcmp(iter1->first,key2S,key2.size())!=0) {
	pidVec.push_back(iter1->second);
    }

    _rwlock.release_read();
    return (r);
}

/****************************************************************** 
 *
 * @fn:    getAllPartitions()
 *
 * @brief: Returns the list of the root ids of all partitions
 *
 * @param: vector<lpid_t> pidVec    - Output 
 *
 ******************************************************************/

w_rc_t key_ranges_map::getAllPartitions(vector<lpid_t>& pidVec) 
{
    _rwlock.acquire_read();
    for(KRMapIt iter = _keyRangesMap.begin(); iter != _keyRangesMap.end(); iter++) {
	pidVec.push_back(iter->second);
    }
    _rwlock.release_read();
    return (RCOK);
}

/****************************************************************** 
 *
 * @fn:    getAllStartKeys()
 *
 * @brief: Returns the list of the start keys of all partitions
 *
 * @param: vector<cvec_t*> keysVec    - Output 
 *
 ******************************************************************/

w_rc_t key_ranges_map::getAllStartKeys(vector<cvec_t*>& keysVec) 
{
    _rwlock.acquire_read();
    for(KRMapIt iter = _keyRangesMap.begin(); iter != _keyRangesMap.end(); iter++) {
	cvec_t key(iter->first, sizeof(iter->first));
	keysVec.push_back(&key);
    }
    _rwlock.release_read();
    return (RCOK);
}

/****************************************************************** 
 *
 * @fn:    getBoundaries()
 *
 * @param: lpid_t pid    - The root of the partition whose boundaries is returned (Input)
 * @param: pair<cvec_t, cvec_t> - The boundaries (Output)
 *
 * @brief: Returns the range boundaries of a partition in a pair
 *
 ******************************************************************/

w_rc_t key_ranges_map::getBoundaries(lpid_t pid, pair<cvec_t, cvec_t>& keyRange) 
{
    KRMapIt iter;
    bool bFound = false;

    _rwlock.acquire_read();
    for (iter = _keyRangesMap.begin(); iter != _keyRangesMap.end(); ++iter) {
        if (iter->second == pid) {
            bFound = true;
	    break;
        }
    }
    _rwlock.release_read();
    
    if(!bFound) {
	// the pid is not in the map, returns error.
	return (RC(mrb_PARTITION_NOT_FOUND));
    }

    keyRange.first.put(iter->first, sizeof(iter->first));
    iter++;
    if(iter == _keyRangesMap.end() && _maxKey != NULL) { 
        // check whether it is the last range
	keyRange.second.put(_maxKey, sizeof(_maxKey));
    }
    else {
        keyRange.second.put(iter->first, sizeof(iter->first));
    }

    return (RCOK);
}

/****************************************************************** 
 *
 * @fn:    getBoundaries()
 *
 * @param: lpid_t pid    - The root of the partition whose boundaries is returned (Input)
 * @param: cvec_t startKey - The start key of the partition's key-range (Output)
 * @param: cvec_t endKey  - The end key of the partition's key-range (Output)
 * @param: bool last      - Indicates whether the partition is the last one,
 *                          the one with the highest key values (Output)
 *
 * @brief: Returns the range boundaries of a partition in start&end key
 *
 ******************************************************************/

w_rc_t key_ranges_map::getBoundaries(lpid_t pid, cvec_t& startKey, cvec_t& endKey, bool& last) 
{
    KRMapIt iter;
    bool bFound = false;
    
    _rwlock.acquire_read();
    for (iter = _keyRangesMap.begin(); iter != _keyRangesMap.end(); ++iter) {
        if (iter->second == pid) {
            bFound = true;
	    break;
        }
    }
    _rwlock.release_read();
    
    if(!bFound) {
	// the pid is not in the map, returns error.
	return (RC(mrb_PARTITION_NOT_FOUND));
    }

    startKey.set(iter->first, sizeof(iter->first));
    if( iter != _keyRangesMap.begin() ) {
	iter--;
        endKey.set(iter->first, sizeof(iter->first));
    }
    else if(_maxKey == NULL) {
	last = true;
    }
    else {
	last = true;
	endKey.set(_maxKey, sizeof(_maxKey));
    }
    return (RCOK);
}

/****************************************************************** 
 *
 * @fn:    getBoundariesVec()
 *
 * @brief: Returns a vector with the key boundaries for all the partitions
 *
 ******************************************************************/

w_rc_t key_ranges_map::getBoundariesVec(vector< pair<char*,char*> >& keyBoundariesVec)
{
    KRMapIt iter;
    pair<char*, char*> keyPair;
    keyPair.second = NULL;
    keyBoundariesVec.clear();
    keyBoundariesVec.reserve(_keyRangesMap.size());

    _rwlock.acquire_read();
    for (iter = _keyRangesMap.begin(); iter != _keyRangesMap.end(); ++iter) {
        keyPair.first = iter->first;
        if (!keyBoundariesVec.empty()) {
            // Visit the last entry and update the upper boundary
            keyBoundariesVec.back().second = iter->first;
        }
        // Add entry to the vector
        keyBoundariesVec.push_back(keyPair);
    }
    _rwlock.release_read();
    return (RCOK);
}

/****************************************************************** 
 *
 * @fn:    updateRoot()
 *
 * @param: cvec_t key    - The root of the partition that keeps this key is updated (Input)
 * @param: lpid_t root   - New root value for the updated partition (Input)
 *
 * @brief: Updates the root of the partition starting with key
 *
 ******************************************************************/

w_rc_t key_ranges_map::updateRoot(const Key& key, const lpid_t& root)
{
    _rwlock.acquire_write();
    if(key.count() == 1) {
	if(_keyRangesMap.find((char*)key._pair[0].ptr) != _keyRangesMap.end()) {
	    _keyRangesMap[(char*)key._pair[0].ptr] = root;
	} else {
            _rwlock.release_write();
	    return (RC(mrb_PARTITION_NOT_FOUND));
	}
    } else {
	char* keyS = (char*) malloc(key.size());
	key.copy_to(keyS);
	if(_keyRangesMap.find(keyS) != _keyRangesMap.end()) {
	    _keyRangesMap[keyS] = root;
	} else {
            _rwlock.release_read();
	    return (RC(mrb_PARTITION_NOT_FOUND));
	}
    }
    _rwlock.release_write();
    return (RCOK);
}


/****************************************************************** 
 *
 * Helper functions
 *
 ******************************************************************/

void key_ranges_map::printPartitions()
{
    KRMapIt iter;
    uint i = 0;
    _rwlock.acquire_read();
    //DBG(<<"Printing ranges map: ");
    for (iter = _keyRangesMap.begin(); iter != _keyRangesMap.end(); ++iter, i++) {
	//DBG(<<"Partition " << i << "\tStart key (" << iter->first << ")\tRoot (" << iter->second << ")");
        printf("Root (%d)\tStartKey (%d)(%s)\n", iter->second.page, *(int*)iter->first, iter->first);
    }
    _rwlock.release_read();
}

void key_ranges_map::setNumPartitions(uint numPartitions)
{
    // pin: we do not actually need this function
    //      how to adjust the partitions is ambiguous
    _rwlock.acquire_write();
    _numPartitions = numPartitions;
    _rwlock.release_write();
}

void key_ranges_map::setMinKey(const Key& minKey)
{  
    // pin: not sure who is going to use this function
    
    _rwlock.acquire_write();

    // update the minKey
    if(_minKey == NULL) {
	_minKey = (char*) malloc(minKey.size()); 
    }
    minKey.copy_to(_minKey);
    
    // insert the new minKey
    KRMapIt iter = _keyRangesMap.lower_bound(_minKey);
    if(iter == _keyRangesMap.end()) {
    	iter--;
    }
    _keyRangesMap[_minKey] = iter->second;

    // delete the partitions that has lower key values than the new minKey
    _keyRangesMap.erase(iter, _keyRangesMap.end());

    _rwlock.release_write();
}

void key_ranges_map::setMaxKey(const Key& maxKey)
{
    // pin: not sure who is going to use this function

    _rwlock.acquire_write();

    // update the maxKey
    if(_maxKey == NULL) {
	_maxKey = (char*) malloc(maxKey.size()); 
    }
    maxKey.copy_to(_maxKey);

    // delete the partitions that has higher key values than the new maxKey
    KRMapIt iter = _keyRangesMap.lower_bound(_maxKey);
    _keyRangesMap.erase(_keyRangesMap.begin(), iter);

    _rwlock.release_write();
}

uint key_ranges_map::getNumPartitions() const
{
    return (_numPartitions);
}

char* key_ranges_map::getMinKey() const
{
    assert(_minKey);
    return (_minKey);
}

char* key_ranges_map::getMaxKey() const
{
    assert(_maxKey);
    return (_maxKey);
}

map<char*, lpid_t, cmp_greater> key_ranges_map::getMap() const
{
    return (_keyRangesMap);
}


/****************************************************************** 
 *
 * @fn:    is_same()
 *
 * @brief: Returns true if the two maps are the same.
 *         - They have the same number of partitions
 *         - Each partition has the same starting point
 *
 ******************************************************************/

bool key_ranges_map::is_same(const key_ranges_map& krm)
{
    if (_numPartitions!=krm._numPartitions) { return (false); }
    else { return (true); } // IP: HACK

//     if (strcmp(_minKey,krm._minKey)!=0) return (false);
//     if (strcmp(_maxKey,krm._maxKey)!=0) return (false);

    assert (_keyRangesMap.size()==krm.size());

    KRMapIt myIt = _keyRangesMap.begin();
    KRMapCIt urCIt = krm._keyRangesMap.begin();

    for (; myIt != _keyRangesMap.end(); myIt++,urCIt++) {
        if ((*myIt).second!=(*urCIt).second) { return (false); }
        if (strcmp((*myIt).first,(*urCIt).first)!=0) { return (false); }
    }
    return (true);
}


#if 0
int main(void)
{
    /*
      cout << "key_ranges_map(10, 100, 10)" << endl;
      key_ranges_map* KeyMap = new key_ranges_map(10, 100, 10);
      (*KeyMap).printPartitions();
      cout << endl;
  
      cout << "addPartition(60)" << endl;
      (*KeyMap).addPartition(60);
      (*KeyMap).printPartitions();
      cout << endl;
  
      cout << "deletePartitionWithKey(20)" << endl;
      (*KeyMap).deletePartitionWithKey(20);
      (*KeyMap).printPartitions();
      cout << endl;
  
      cout << "deletePartition(9)" << endl;
      (*KeyMap).deletePartition(9);
      (*KeyMap).printPartitions();
      cout << endl;
  
      cout << "getPartitionWithKey(70)" << endl;
      cout << (*KeyMap).getPartitionWithKey(70) << endl;
      cout << endl;
  
      cout << (*KeyMap)(70) << endl;
      cout << endl;
  
      cout << "setMinKey(6)" << endl;
      (*KeyMap).setMinKey(6);
      (*KeyMap).printPartitions();
      cout << endl;
  
      cout << "setMaxKey(110)" << endl;
      (*KeyMap).setMaxKey(110);
      (*KeyMap).printPartitions();
      cout << endl;
  
      cout << "[36, 64]" << endl;
      vector<int> v = (*KeyMap).getPartitions(36,true,64,true);
      for(int i=0; i < v.size(); i++)
      cout << v[i] << " " << endl;
      cout << endl;
  
      cout << "(36, 64]" << endl;
      v = (*KeyMap).getPartitions(36,false,64,true);
      for(int i=0; i < v.size(); i++)
      cout << v[i] << " " << endl;
      cout << endl;
  
      cout << "[36, 64)" << endl;
      v = (*KeyMap).getPartitions(36,true,64,false);
      for(int i=0; i < v.size(); i++)
      cout << v[i] << " " << endl;
      cout << endl;
  
      cout << "(36, 64)" << endl;
      v = (*KeyMap).getPartitions(36,false,64,false);
      for(int i=0; i < v.size(); i++)
      cout << v[i] << " " << endl;
      cout << endl;
  
      cout << "get range of partition 3" << endl;
      pair<int,int> p = (*KeyMap).getBoundaries(3);
      cout << "[" << p.first << ", " << p.second << ")" << endl;
      cout << endl;
    */

    return 0;
}
#endif

