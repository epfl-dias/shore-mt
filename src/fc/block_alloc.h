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

#include <atomic_templates.h>

// for placement new support, which users need
#include <new>
#include <w.h>
#include <stdlib.h>

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
 * The factory goes to the global heap (::malloc(), ::free()) to allocate
 * blocks for itself; each block provides N chips to hand out to 
 * clients. When a block is consumed, another block is taken from the
 * global heap.  
 *
 * Chips can be deallocated by any thread.
 * The entire block is returned to the heap (::free()) when all its
 * chips have been "released" by some client, and the block itself
 * has also been destroyed by the thread allocator that created it.
 *
 * PROS:
 * - most allocations are extremely cheap -- no malloc(), no atomic ops
 * - deallocations are also cheap -- one atomic op
 * - completely immune to the ABA problem 
 * - releases memory back to the OS after bursts

 * CONS:   
 * - each thread must have its own allocator, which means managing
 *   thread-local storage (if compilers ever support non-POD __thread
 *   objects, this problem would go away) and introduces a trade-off
 *   between amortizing malloc and wasting space.     
 * - memory leaks are extremly expensive because they keep a whole
 *   block from being freed instead of just one object.
 * - long-held chips are expensive (even if not leaked) for the same
 *   reason.
 */


template<class T>
class block_alloc 
{
private:
  enum { ALLOCATOR=1, LIVE_OBJECT=2 };

  /// a "chunk" or block of chips taken from the global heap
  struct block 
  {
    // only deal with integer multiples of sizeof(block_alloc**) bytes
    enum { ROUNDED_TSIZE=sizeof(T) + 2*sizeof(block**) - sizeof(T)%sizeof(block**) };
    enum { UNIT_SIZE=sizeof(block**) + ROUNDED_TSIZE};

    char* pos;
    unsigned int locks; // ALLOCATOR + n*LIVE_OBJECT
    unsigned int count; // debug/assert only
    char data[0];
    
    block(unsigned n, unsigned asize)
      : pos(data+asize), locks(ALLOCATOR + n*LIVE_OBJECT), count(n)
    {
    }
    
    /// Carve out a chip from this block.
    void* alloc(int &n) 
    {
      // nothing left
      if(pos == data) {
        n = count;
        (void) report_release(ALLOCATOR);
        return 0;
      }

      // carve out a chunk for ourselves, initialize it, and return it
      pos -= UNIT_SIZE;
      union { void* v; block** p; } u = {pos};
      *u.p = this;
      return u.p + 1;
    }
    
    /** Report to this block that we gave back a chip.
     * This just updates a count, and if all have been given back,
     * it gives this block back to the global heap.
     *
     * We have to be careful here... the block should be freed once all
     * n objects have been handed out and released, *and* after whoever
     * allocated the block_alloc knows there are none left. Whoever
     * turns this shared counter to zero is responsible to release its
     * memory.
     * \attention NOTE the implicit assumption that the thread that allocated is still
     * around until all items have been freed.
     */
    bool report_release(int who) 
    {
      membar_exit();
      if(!atomic_add_int_nv(&locks, -who)) {
        ::free(this);
        return true;
      }
      return false;
    }

  }; // block

  /// The current block from which we are carving out chips.
  /// We never put chips back.
  block* _block;

public:
  
  block_alloc(int count=256)
    : _block(create_block(count))
  {
  }
  
  /** \brief Pseudo-free any unallocated units. 
    * \details
    * Pseudo-free any unallocated units so that it looks like
    * they were allocated and freed, faking this into freeing the
    * entire block with ::free.
    * If there are any outstanding allocated units, we can't free this
    * _block, and that means something's been allocated but not freed
    * by our client (mem lk) OR it's still in use in something like
    * the lock manager or xcts.  
    * The problem is that those data structures can be allocated by one
    * thread and deallocated by any thread.  
    * As long as they are deallocated with destroy_object, 
    * AND the originating thread has not disappeared,
    * it's ok,
    * they'll end up on the right block. 
    * But there is the possibility
    *  \bug This is not entirely correct (GNATS 42). The problem is
    * that the thread that allocated the allocator must still be around.
    * of destroying the block_alloc before some of the items have been
    * reported released, because the thread can go away before the objects
    * are released.
    * 
    * What we need to do is delegate the freeing to the main thread
    * somehow, so that everything gets freed when the last
    * smthread goes away (preferably when the sm shuts down).
    */
  ~block_alloc() {
    int remaining = (_block->pos - _block->data)/block::UNIT_SIZE;
    if(remaining) {
      bool freed = _block->report_release(ALLOCATOR + remaining*LIVE_OBJECT);
      w_assert1(freed);
    }
  }
  
  /// Retrieve the owning block and verify that it owns this object.
  // Then report the chip released if argument \e destruct is \e true.
  static void destroy_object(T* ptr, bool destruct=true) 
  {
    union { T* t; block** p; char* c; } u = {ptr};

    block* owner = u.p[-1];

    w_assert1(owner->data < u.c && u.c < owner->data + block::UNIT_SIZE*owner->count);

    // destroy the object
    if(destruct)
        u.t->~T();

    // let the owner know we've released an object
    (void) owner->report_release(LIVE_OBJECT);
  }
  
private:
  // let operator new access alloc()
  friend void* operator new<>(size_t, block_alloc<T> &);
  friend void  operator delete<>(void*, block_alloc<T> &);
  
  /// Create a new block with given number of chips.
  static block* create_block(unsigned count) 
  {
    int asize = count * block::UNIT_SIZE;
    void *buf = ::malloc(sizeof(block)+asize); // 8-byte aligned by default
    if(!buf) throw std::bad_alloc();
    return  new(buf) block(count, asize);
  }

  /// Allocate a chip.
  void* alloc() 
  {
    int count=0;
    void* rval = _block->alloc(count); // returns count
    if(!rval) {
      _block = create_block(count);
      rval = _block->alloc(count);
    }

    union { void* v; block** p; } u = {rval};
    w_assert1( u.p[-1] ==  _block );

    return rval;
  }
  
};

template<class T>
inline void* operator new(size_t nbytes, block_alloc<T> &alloc) 
{
  w_assert1(nbytes == sizeof(T));
  return alloc.alloc();
}

template<class T>
inline void operator delete(void* ptr, block_alloc<T> &alloc) 
{
  /* According to FRJ this template exists to handle the case
   * when the constructor throws and we have to delete; at that
   * point, the same alloc._block exists (it would have to be a
   * pretty bad race for it not to exist
   * Nevertheless, the asserts handle the most general case.
   */
  w_assert1(alloc._block->locks > block_alloc<T>::ALLOCATOR);

  /* even if it's empty we can safely access _block directly because
     our caller doesn't know yet and won't have released it.
   */
  // w_assert1(alloc._block->locks & block_alloc<T>::ALLOCATOR);
  // (void) alloc._block->report_release(block_alloc<T>::LIVE_OBJECT);

  // by calling destroy_object, we should get the correct owning block,
  // just in case this gets called somewhere besides in the case mentioned
  // above
  destroy_object( dynamic_cast<T*>(ptr), false/* don't destruct*/);

  w_assert2(0); // let a debug version catch this.
}

#endif
