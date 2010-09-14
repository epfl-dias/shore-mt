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
{
}

key_ranges_map::key_ranges_map(const Key& minKey, const Key& maxKey, const uint numParts)
{
    // IP: call a function that makes sure that no such store exists
    //     create a store
    // pin: this is only called due to a request to an existing store so we don't
    //      have to check for this
    
    // Calls the default initialization. 
    // pin: this shouldn't be how we do this, the map should be initialized by a
    //      default key value and the request to makeEqualPartitions should be made
    //      explicitly and for only initialization
    //      the checks that would control these constraints should be in SM-API call
    //      make_equal_partitions
    //      so i actually want to completely remove this constructor. =)
    // _numPartitions = makeEqualPartitions(minKey,maxKey,numParts);

    fprintf (stdout, "%d partitions created", _numPartitions);
}


key_ranges_map::~key_ranges_map()
{
    // Delete the allocated keys in the map
    keysIter iter;
    uint i=0;

    _rwlock.acquire_write();

    DBG(<<"Destroying the ranges map: ");
    for (iter = _keyRangesMap.begin(); iter != _keyRangesMap.end(); ++iter, ++i) {
        DBG(<<"Partition " << i << "\tStart key (" << iter->first << ")\tRoot (" << lpid_t << ")");
        free (iter->first);
    }

    // Delete the boundary keys
    if(_minKey != NULL)
	free (_minKey);
    if(_maxKey != NULL)
	free (_maxKey);

    _rwlock.release_write();    
}


/****************************************************************** 
 *
 * @fn:     makeEqualPartitions()
 *
 * @brief:  Makes equal length partitions from scratch
 *
 * @return: The number of partitions that were actually created
 *
 ******************************************************************/

uint key_ranges_map::makeEqualPartitions(const Key& minKey, const Key& maxKey, 
                                         const uint numParts, vector<lpid_t> roots)
{
    assert (minKey<=maxKey);

    // IP: make sure that the store does not have any entries (isClean())
    // pin: the check for this should be in SM-API call make_equal_partitions
    
    _rwlock.acquire_write();

    // Set min/max keys
    uint minsz = minKey.size();
    uint maxsz = maxKey.size();    
    _minKey = (char*) malloc(minsz);
    _maxKey = (char*) malloc(maxsz);    
    minKey.copy_to(_minKey, sizeof(minsz));
    maxKey.copy_to(_maxKey, sizeof(maxsz));


    int dummy_min = *((int*)_minKey);
    int dummy_max = *((int*)_maxKey);
	
    uint keysz = min(minsz,maxsz); // Use that many chars for the keys entries

    uint partsCreated = 0; // In case it cannot divide to numParts partitions

    uint base=0;
    while (_minKey[base] == _maxKey[base]) {
         // discard the common prefixes
         base++; 
    }

    // To not to have integer overflow while computing the space between two strings
    uint sz = (keysz - base < 4) ? (keysz - base) : 4;
    char** subParts = (char**)malloc(numParts*sizeof(char*));
    char* A = (char*) malloc(base+sz+1);
    char* B = (char*) malloc(base+sz+1);
    partsCreated = _distributeSpace(strncpy(A, _minKey, base+sz), strncpy(B, _maxKey, base+sz),
				    sz, numParts, subParts);

    free(A);
    free(B);

    // put the partitions to map
    _keyRangesMap.clear();
    for(uint i = 0; i < partsCreated; i++) { 
        _keyRangesMap[subParts[i]] = roots[i];
    }

    _rwlock.release_write();

    assert (partsCreated != 0); // Should have created at least one partition
    return (partsCreated);
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
    assert (strcmp(A,B)); // It should A<B

    uint numASCII = 95;
    uint startASCII = 31;
    int totalSize = strlen(A);

    // compute the space between two strings
    uint space = 0;
    for(int i=totalSize-1, j=0; i>totalSize-sz-1; i--,j++)
	space = space + ((int) B[i] - (int) A[i]) * (int) pow(numASCII, j);

    // find the approximate range each partitions should keep
    uint range = space / partitions;
   
    // compute the partitions
    char* currentKey = (char*) malloc(totalSize+1);
    char* nextKey = (char*) malloc(totalSize+1);
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
    char* keyS = (char*) malloc(key.size());
    key.copy_to(keyS);
    return (_addPartition(keyS, newRoot));
}


/****************************************************************** 
 *
 * @fn:    deletePartition{,ByKey}()
 *
 * @brief: Deletes a partition, by merging it with the partition which
 *         is before that, based either on a partition identified or
 *         a key.
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

    keysIter iter = _keyRangesMap.lower_bound(keyS);

    if(iter == _keyRangesMap.end()) {
	// partition not found, return an error
	return (RC(mrb_PARTITION_NOT_FOUND));
    }
    root2 = iter->second;
    ++iter;
    if(iter == _keyRangesMap.end()) {
	--iter;
	if(iter == _keyRangesMap.begin()) {
	    // partition is the last partition, cannot be deleted
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
    char* keyS = (char*) malloc(key.size());
    key.copy_to(keyS);
    r = _deletePartitionByKey(keyS, root1, root2, startKey1, startKey2);
    free (keyS);
    return (r);
}

w_rc_t key_ranges_map::deletePartition(lpid_t& root1, lpid_t& root2,
				       Key& startKey1, Key& startKey2)
{
    w_rc_t r = RCOK;
    bool bFound = false;

    keysIter iter;
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
    } else {
	return (RC(mrb_PARTITION_NOT_FOUND));
    }

    return (r);
}


/****************************************************************** 
 *
 * @fn:    getPartitionByKey()
 *
 * @brief: Returns the partition which a particular key belongs to
 *
 ******************************************************************/

w_rc_t key_ranges_map::getPartitionByKey(const Key& key, lpid_t& pid)
{
    char* keyS = (char*) malloc(key.size());
    key.copy_to(keyS);

    _rwlock.acquire_read();
    keysIter iter = _keyRangesMap.lower_bound(keyS);
    free (keyS);
    if(iter == _keyRangesMap.end()) {
	// the key is not in the map, returns error.
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
 * @brief: Returns the list of partitions that cover one of the key ranges:
 *         [key1, key2], (key1, key2], [key1, key2), or (key1, key2) 
 *
 ******************************************************************/

w_rc_t key_ranges_map::getPartitions(const Key& key1, bool key1Included,
                                     const Key& key2, bool key2Included,
                                     vector<lpid_t>& pidVec) 
{
    w_rc_t r = RCOK;
    
    if(key2 < key1) {
	// return error if the bounds are not given correctly
	return (RC(mrb_KEY_BOUNDARIES_NOT_ORDERED));
    }  
    char* key1S = (char*) malloc(key1.size());
    key1.copy_to(key1S);
    char* key2S = (char*) malloc(key2.size());
    key2.copy_to(key2S);

    _rwlock.acquire_read();
    keysIter iter1 = _keyRangesMap.lower_bound(key1S);
    keysIter iter2 = _keyRangesMap.lower_bound(key2S);
    if (iter1 == _keyRangesMap.end() || iter2 == _keyRangesMap.end()) {
	// at least one of the keys is not in the map, returns error.
	return (RC(mrb_PARTITION_NOT_FOUND));
    }
    while (iter1 != iter2) {
	pidVec.push_back(iter1->second);
	iter1++;
    }
    // check for !key2Included
    if(key2Included || strcmp(iter1->first,key2S)!=0)
	pidVec.push_back(iter1->second);
    _rwlock.release_read();

    return (r);
}

/****************************************************************** 
 *
 * @fn:    getAllPartitions()
 *
 * @brief: Returns the list of all root ids in partitions 
 *
 ******************************************************************/

w_rc_t key_ranges_map::getAllPartitions(vector<lpid_t>& pidVec) 
{
    w_rc_t r = RCOK;
    
    _rwlock.acquire_read();
    for(keysIter iter = _keyRangesMap.begin(); iter != _keyRangesMap.end(); iter++) {
	pidVec.push_back(iter->second);
    }
    _rwlock.release_read();

    return (r);
}

/****************************************************************** 
 *
 * @fn:    getBoundaries()
 *
 * @brief: Returns the range boundaries of a partition in a pair
 *
 ******************************************************************/

w_rc_t key_ranges_map::getBoundaries(lpid_t pid, pair<cvec_t, cvec_t>& keyRange) 
{
    keysIter iter;
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
 * @brief: Returns the range boundaries of a partition in start&end key
 *
 ******************************************************************/

w_rc_t key_ranges_map::getBoundaries(lpid_t pid, cvec_t& startKey, cvec_t& endKey) 
{
    keysIter iter;
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
    iter++;
    if(iter == _keyRangesMap.end() && _maxKey != NULL) { 
        // check whether it is the last range
	endKey.set(_maxKey, sizeof(_maxKey));
    }
    else {
        endKey.set(iter->first, sizeof(iter->first));
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
    keysIter iter;
    pair<char*, char*> keyPair;
    keyPair.second = NULL;

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
 * @brief: Updates the root of the partition starting with key
 *
 ******************************************************************/

w_rc_t key_ranges_map::updateRoot(const Key& key, const lpid_t& root)
{
    char* keyS = (char*) malloc(key.size());
    key.copy_to(keyS);

    _rwlock.acquire_read();
    if(_keyRangesMap.find(keyS) != _keyRangesMap.end()) {
	_keyRangesMap[keyS] = root;
    } else {
	return (RC(mrb_PARTITION_NOT_FOUND));
    }
    _rwlock.release_read();
    return (RCOK);
}


/****************************************************************** 
 *
 * Helper functions
 *
 ******************************************************************/

void key_ranges_map::printPartitions()
{
    keysIter iter;
    uint i = 0;
    _rwlock.acquire_read();
    DBG(<<"Printing ranges map: ");
    for (iter = _keyRangesMap.begin(); iter != _keyRangesMap.end(); ++iter, i++) {
	DBG(<<"Partition " << i << "\tStart key (" << iter->first << ")\tRoot (" << iter->second << ")");
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
    keysIter iter = _keyRangesMap.lower_bound(_minKey);
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
    keysIter iter = _keyRangesMap.lower_bound(_maxKey);
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

