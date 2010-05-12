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

#ifndef BLOCK_ALLOC_H
#define BLOCK_ALLOC_H

#include "dynarray.h"
#include "mem_block.h"

// for placement new support, which users need
#include <new>
#include <w.h>
#include <stdlib.h>
#include <deque>

template<class T>
class block_alloc; // forward
template<class T>
inline void* operator new(size_t nbytes, block_alloc<T> &alloc); // forward
template<class T>
void operator delete(void* ptr, block_alloc<T> &alloc); // forward

/** \brief A factory for speedier allocation from the heap.
 *
 * This allocator is intended for use in a multithreaded environment
 * where many short-lived objects are created and released.
 *
 * Allocations are not thread safe, but deallocations are. This allows
 * each thread to allocate objects cheaply, without worrying about who
 * will eventually deallocate them (they must still be deallocated, of
 * course).
 * To use: give each thread its own allocator: that provides the thread-safety.
 *
 * The factory is backed by a global dynarray which manages
 * block-level allocation; each block provides N chips to hand out to
 * clients. The allocator maintains a cache of blocks just large
 * enough that it can expect to recycle the oldest cached block as
 * each active block is consumed; the cache can both grow and shrink
 * to match demand.
 *
 * PROS:
 * - most allocations are extremely cheap -- no malloc(), no atomic ops
 * - deallocations are also cheap -- one atomic op
 * - completely immune to the ABA problem 
 * - memory can be redistributed among threads between bursts

 * CONS:
 *
 * - each thread must have its own allocator, which means managing
 *   thread-local storage (if compilers ever support non-POD __thread
 *   objects, this problem would go away).
 *
 * - though threads attempt to keep their caches reasonably-sized,
 *   they will only do so at allocation or thread destruction time,
 *   leading to potential hoarding
 *
 * - memory leaks (or unexpectedly long-lived objects) are extremly
 *   expensive because they keep a whole block from being freed
 *   instead of just one object. However, the remaining chips of each
 *   block are at least available for reuse. 
 */

template<class T, class Owner=T>
class block_pool 
{
    typedef memory_block::meta_block_size<sizeof(T)> BlockSize;
    enum { BLOCK_SIZE = BlockSize::BYTES };
    enum { UNIT_COUNT = BlockSize::COUNT };
    enum { UNIT_SIZE = sizeof(T) };

    // use functions because dbx can't see enums...
#define TEMPLATE_ARGS unit_size(), unit_count(), block_size()
    static size_t block_size() { return BLOCK_SIZE; }
    static size_t unit_count() { return UNIT_COUNT; }
    static size_t unit_size() { return UNIT_SIZE; }

    struct block : memory_block::block {
	char	_filler[BLOCK_SIZE-sizeof(memory_block::block)];
	block() : memory_block::block(TEMPLATE_ARGS) { }
    };
    
    struct pool : memory_block::block_pool {
	dynvector<block, BLOCK_SIZE> 	_blocks;
	std::deque<block*> 		_free_list;
	pthread_mutex_t			_lock;

	pool() {
	    pthread_mutex_init(&_lock, 0);
	    int err = _blocks.init(1024*1024);
	    if(err) throw std::bad_alloc();
	}
	~pool() {
	    _blocks.fini();
	}
	
	virtual block* _acquire_block() {
	    block* rval;
	    pthread_mutex_lock(&_lock);
	    if(_free_list.empty()) {
		int err = _blocks.resize(_blocks.size()+1);
		if(err) throw std::bad_alloc();
		rval = &_blocks.back();
	    }
	    else {
		rval = _free_list.front();
		_free_list.pop_front();
	    }
	    pthread_mutex_unlock(&_lock);
	    
	    return rval;
	}

	virtual void _release_block(memory_block::block* b) {
	    pthread_mutex_lock(&_lock);
	    _free_list.push_back((block*) b);
	    pthread_mutex_unlock(&_lock);
	}

	virtual bool validate_pointer(void* ptr) {
	    // no need for the mutex... dynarray only grows
	    union { void* v; size_t n; } u={ptr}, v={&_blocks[0]};
	    u.n -= v.n;
	    return u.n < _blocks.size()*sizeof(block);
	}
    };
    static pool* get_pool() {
	static pool p;
	return &p;
    }
    
    memory_block::block_list _blist;

public:
  
    block_pool()
	: _blist(get_pool(), TEMPLATE_ARGS)
    {
    }
    
    void* acquire() { return _blist.acquire(TEMPLATE_ARGS); }
    
    /* Verify that we own the object then find its block and report it
       as released. If \e destruct is \e true then call the object's
       desctructor also.
     */
    static
    void release(void* ptr) {
	assert(get_pool()->validate_pointer(ptr));
	block::release(ptr, TEMPLATE_ARGS);
    }
};

template<class T>
struct block_alloc {
    void destroy_object(T* ptr) {
	ptr->~T();
	_pool.release(ptr);
    }
    
private:
    block_pool<T, block_alloc<T> > _pool;
    
    // let operator new/delete access alloc()
    friend void* operator new<>(size_t, block_alloc<T> &);
    friend void  operator delete<>(void*, block_alloc<T> &);
};

template<class T>
inline
void* operator new(size_t nbytes, block_alloc<T> &alloc) 
{
  w_assert1(nbytes == sizeof(T));
  return alloc._pool.acquire();
}

/* No, this isn't a way to do "placement delete" (if only the language
   allowed that symmetry)... this operator is only called -- by the
   compiler -- if T's constructor throws
 */
template<class T>
inline
void operator delete(void* ptr, block_alloc<T> &alloc) 
{
    alloc._pool.release(ptr);
    w_assert2(0); // let a debug version catch this.
}

#undef TEMPLATE_ARGS

#endif
