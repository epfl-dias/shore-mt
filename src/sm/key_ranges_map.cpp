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

#include "btree.h"
#include "sdesc.h"


foo::foo() 
    : _m(NULL),_len(0),_alloc(false)
{
}

foo::foo(const foo& v)
    : _m(v._m),_len(v._len),_alloc(false)
{
}

foo::foo(char* m, uint4_t len, bool alloc)
{
    assert(m);
    _len = len;
    if (alloc) {
        // deep copy
        _alloc = true;
        _m = (char*) malloc(_len);
        memcpy(_m, m, _len);
    }
    else {
        // shallow copy
        _alloc = false;
        _m = m;
        _len = len;
    }
}

foo::~foo()
{
    if (_alloc && _m) {
        free(_m);
        _m = NULL;
    }
}

foo& foo::operator=(const foo& v)
{
    _m = v._m;
    _len = v._len;
    _alloc = false;
    return (*this);
}


/****************************************************************** 
 *
 * Construction/Destruction
 *
 * @brief: The constuctor calls the default initialization function
 * @brief: The destructor needs to free all the malloc'ed keys
 *
 ******************************************************************/

key_ranges_map::key_ranges_map()
    : _numPartitions(0)
{
    _fookeys.clear();
}

key_ranges_map::key_ranges_map(const sinfo_s& sinfo,
                               const cvec_t& minKey, 
                               const cvec_t& maxKey, 
                               const uint numParts, 
                               const bool physical)
    : _numPartitions(0)
{
    _fookeys.clear();
    w_rc_t r = RCOK;
    if (physical) { 
        r = RC(mrb_NOT_PHYSICAL_MRBT); 
    }
    else {
        r = nophy_equal_partitions(sinfo,minKey,maxKey,numParts);
    }
    if (r.is_error()) { W_FATAL(r.err_num()); }
}

key_ranges_map::~key_ranges_map()
{
    // Delete the allocated keys in the map
    vector<foo*>::iterator iter;

    DBG(<<"Destroying the ranges map: ");

    _rwlock.acquire_write();
    for (iter = _fookeys.begin(); iter != _fookeys.end(); ++iter) {
	if (*iter) { 
            delete (*iter);
            *iter = NULL;
        }
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

w_rc_t key_ranges_map::nophy_equal_partitions(const sinfo_s& sinfo,
                                              const cvec_t& minKey, 
                                              const cvec_t& maxKey,
                                              const uint numParts)
{
    assert (_minKey);
    assert (_maxKey);
    assert(numParts);

    // Calculate the first key and the step thereafter
    int size = sizeof(int);
    int lower_bound = 0;
    int upper_bound = 0;
    minKey.copy_to(&lower_bound,size);
    maxKey.copy_to(&upper_bound,size);

    int space = upper_bound - lower_bound;
    double diff = (double)space / (double)numParts;
    assert (diff>0);     

    double d_current_key = lower_bound;
    int current = 0;
    cvec_t* scrambledKey = NULL;
    stid_t astid;
    uint4_t i=0;
    
    // Put the partitions to map

    // ---------------------
    _rwlock.acquire_write();
    _keyRangesMap.clear();
    _rwlock.release_write();
    // ---------------------

    while(i < numParts) {

	current = d_current_key;
        cvec_t acv((char*)(&current),size);
        W_DO(btree_m::_scramble_key(scrambledKey,acv,minKey.count(),sinfo.kc));

        lpid_t alpid(astid,i);
        W_DO(addPartition(*scrambledKey,alpid));

	d_current_key = d_current_key + diff;
        i++;
    }

    //printPartitions();
    return (RCOK);
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

w_rc_t key_ranges_map::addPartition(const cvec_t& key, lpid_t& newRoot)
{
    w_rc_t r = RCOK;
    
    foo kv((char*)key._base[0].ptr,key._base[0].len,false);

    _rwlock.acquire_write();        
    KRMapIt iter = _keyRangesMap.find(kv);

    if (iter==_keyRangesMap.end() ) {
        foo* newkv = new foo((char*)key._base[0].ptr,key._base[0].len,true);
        _keyRangesMap[*newkv] = newRoot;

        //cvec_t* newkey = new cvec_t(key,0,key.size());
        //_keyRangesMap[*newkey] = newRoot;
        
        _numPartitions++;
        _fookeys.push_back(newkv);
    }
    else {
        r = RC(mrb_PARTITION_EXISTS);
    }
    _rwlock.release_write();
    //printf ("%d\n",_keyRangesMap.size());

    return (r);
}

// w_rc_t key_ranges_map::addPartition(const Key& key, lpid_t& newRoot)
// {
//     char* keyS = (char*) malloc(key.size());
//     key.copy_to(keyS);
//     W_DO(_addPartition(keyS, newRoot));
//     return (RCOK);
// }


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

w_rc_t key_ranges_map::_deletePartitionByKey(const foo& kv,
					     lpid_t& root1, lpid_t& root2,
					     Key& startKey1, Key& startKey2)
{
    w_rc_t r = RCOK;

    _rwlock.acquire_write();
    
    KRMapIt iter = _keyRangesMap.lower_bound(kv);
    
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
	startKey1.put((*iter).first._m,(*iter).first._len);
    }
    else {
 	startKey1.put((*iter).first._m,(*iter).first._len);
 	root1 = iter->second;
    }
    --iter;
    startKey2.put((*iter).first._m,(*iter).first._len);
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
    foo kv((char*)key._base[0].ptr,key._base[0].len,false);
    r = _deletePartitionByKey(kv, root1, root2, startKey1, startKey2);
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
 * @fn:    getPartitionByUnscrambledKey()
 *
 * @brief: Returns the root page id, "pid", of the partition which a
 *         particular "key" belongs to. Before doing so, it scrambles
 *         the "key" according to the sinfo
 *
 * @param: cvec_t key    - Input (unscrambled key)
 * @param: lpid_t pid    - Output
 *
 ******************************************************************/

w_rc_t key_ranges_map::getPartitionByUnscrambledKey(const sinfo_s& sinfo,
                                                    const Key& key,
                                                    lpid_t& pid)
{
    cvec_t* scrambledKey = NULL;
    W_DO(btree_m::_scramble_key(scrambledKey,key,key.count(),sinfo.kc));    
    return (getPartitionByKey(*scrambledKey,pid));
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
    foo kv((char*)key._base[0].ptr,key._base[0].len,false);
    _rwlock.acquire_read();
    KRMapIt iter = _keyRangesMap.lower_bound(kv);
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

// w_rc_t key_ranges_map::getPartitionByKey(char* keyS, lpid_t& pid)
// {
//     _rwlock.acquire_read();
//     KRMapIt iter = _keyRangesMap.lower_bound(keyS);
//     if(iter == _keyRangesMap.end()) {
// 	// the key is not in the map, returns error.
//         _rwlock.release_read();
// 	return (RC(mrb_PARTITION_NOT_FOUND));
//     }
//     pid = iter->second;
//     _rwlock.release_read();
//     return (RCOK);    
// }


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
    
    if(key2 < key1) {
	// return error if the bounds are not given correctly
	return (RC(mrb_KEY_BOUNDARIES_NOT_ORDERED));
    }  

    // get start key
    foo a((char*)key1._base[0].ptr,key1._base[0].len,false);

    // get end key
    foo b((char*)key2._base[0].ptr,key2._base[0].len,false);

    _rwlock.acquire_read();

    KRMapIt iter1 = _keyRangesMap.lower_bound(a);
    KRMapIt iter2 = _keyRangesMap.lower_bound(b);

    if (iter1 == _keyRangesMap.end() || iter2 == _keyRangesMap.end()) {
	// at least one of the keys is not in the map, returns error.
        _rwlock.release_read();
	return (RC(mrb_PARTITION_NOT_FOUND));
    }

    while (iter1 != iter2) {
	pidVec.push_back(iter1->second);
	iter1--;
    }

    pidVec.push_back(iter1->second);
    
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

    startKey.set((*iter).first._m,(*iter).first._len);
    if( iter != _keyRangesMap.begin() ) {
	iter--;
        endKey.set((*iter).first._m,(*iter).first._len);
    }
    else {
	endKey.set(cvec_t::pos_inf);
    }
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
    foo kv((char*)key._base[0].ptr,key._base[0].len,false);

    _rwlock.acquire_write();
    if(_keyRangesMap.find(kv) != _keyRangesMap.end()) {
        _keyRangesMap[kv] = root;
    } 
    else {
        _rwlock.release_write();
        return (RC(mrb_PARTITION_NOT_FOUND));
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
    printf("#Partitions (%d)\n", _numPartitions);
    char* content = NULL;
    for (iter = _keyRangesMap.begin(); iter != _keyRangesMap.end(); ++iter, i++) {
	//DBG(<<"Partition " << i << "\tStart key (" << iter->first << ")\tRoot (" << iter->second << ")");        
        content = (char*)malloc(sizeof(char)*(iter->first._len)+1);
        memset(content,0,iter->first._len+1);
        memcpy(content,iter->first._m,iter->first._len);
        printf("Root (%d)\tStartKey (%s)\n", iter->second.page, content);
        free (content);
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
    assert (0); // IP: -//-

//     _rwlock.acquire_write();
    
//     // insert the new minKey
//     KRMapIt iter = _keyRangesMap.lower_bound(_minKey);
//     if(iter == _keyRangesMap.end()) {
//     	iter--;
//     }
//     _keyRangesMap[_minKey] = iter->second;

//     // delete the partitions that has lower key values than the new minKey
//     _keyRangesMap.erase(iter, _keyRangesMap.end());

//     _rwlock.release_write();
}

void key_ranges_map::setMaxKey(const Key& maxKey)
{
    // pin: not sure who is going to use this function
    assert (0); // IP: -//-

//     _rwlock.acquire_write();

//     // delete the partitions that has higher key values than the new maxKey
//     KRMapIt iter = _keyRangesMap.lower_bound(_maxKey);
//     _keyRangesMap.erase(_keyRangesMap.begin(), iter);

//     _rwlock.release_write();
}

uint key_ranges_map::getNumPartitions() const
{
    return (_numPartitions);
}

key_ranges_map::KRMap key_ranges_map::getMap() const
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

//     if (strcmp(_minKey,krm._minKey)!=0) return (false);
//     if (strcmp(_maxKey,krm._maxKey)!=0) return (false);

    assert (_keyRangesMap.size()==krm.size());

    KRMapIt myIt = _keyRangesMap.begin();
    KRMapCIt urCIt = krm._keyRangesMap.begin();

    for (; myIt != _keyRangesMap.end(); myIt++,urCIt++) {
        if ((*myIt).second!=(*urCIt).second) { return (false); }
        if ((*myIt).first!=(*urCIt).first) { return (false); }
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

