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

#ifndef __DYNARRAY_H
#define __DYNARRAY_H

#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <algorithm>

/* A memory-mapped array which exploits the capabilities provided by
   mmap in order to grow dynamically without moving existing data or
   wasting memory.

   Ideal for situations where you don't know the final size of the
   array, the potential maximum is known but very large, and a
   threaded environment makes it unsafe to resize by reallocating.

   NOTE: the array only supports growing, under the assumption that
   any array which can shrink safely at all can shrink safely to size
   zero (with the data copied to a new, smaller dynarray)

   This approach makes the most sense in a 64-bit environment where
   address space is cheap.

   Note that most systems cannot reserve more than 2-8TB of
   contiguous address space (32-128TB total), most likely because most
   machines don't have that much swap space anyway.

 */
struct dynarray {
    static size_t const PAGE_SIZE;
    
    /* Attempts to initialize the array with a capacity of /max_size/ bytes
       of address space and /size()/ zero.

       @return 0 on success, appropriate errno on failure
     */
    int init(size_t max_size);

    /* Attempts to make a deep copy of /to_copy/, setting my capacity
       to the larger of /to_copy.capacity()/ and /max_size/

       @return 0 on success, appropriate errno on failure
     */
    int init(dynarray const &to_copy, size_t max_size=0);

    /* Destroys the existing mapping, if any, and returns the object
       to its uninitialized state
     */
    int fini();

    /* The reserved size of this mapping. The limit is set at
       initialization and cannot change later.
     */
    size_t capacity() const { return _capacity; }

    /* Ensures that at least /new_size/ bytes (<= /capacity/) are
       mapped and available for use.

       @return 0, or an appropriate errno on failure
     */
    int resize(size_t new_size);

    /* The currently available size. Assuming sufficient memory is
       available the array can grow to /capacity()/ bytes -- using
       calls to /resize()/.
     */
    size_t size() const { return _size; }
    
    operator char*() { return _base; }
    operator char const*() const { return _base; }

    dynarray() : _base(0), _size(0), _capacity(0) { }
    
private:
    // only safe if we're willing to throw exceptions (use init() and memcpy() instead)
    dynarray(dynarray const &other);
    dynarray &operator=(dynarray const &other);
    
    char* _base;
    size_t _size;
    size_t _capacity;
};

/* A compile-time predicate.

   Compilation will fail for B=false because only B=true has a definition
*/
template<bool B>
struct dy_fail_unless;

template<>
struct dy_fail_unless<true> {
    static bool valid() { return true; }
};




/* Think std::vector except backed by a dynarray.

 */
template<typename T, size_t Align=__alignof__(T)>
struct dynvector : dy_fail_unless<((Align &-Align) == Align)> {
    enum { ALIGN = (Align < __alignof__(T))? __alignof__(T) : Align };
    
    /* Initialize an empty dynvector with /limit() == max_count/

       @return 0 on success or an appropriate errno
     */
    int init(size_t max_count) {
	/* if we for some reason need coarser-than-page alignment
	   we'll allocate an extra align-block worth of space and then
	   use black magic to find the lowest alignment boundary not
	   lower than the start of our underlying dynarray.
	 */
	size_t align_extra = (ALIGN > dynarray::PAGE_SIZE)? ALIGN : 0;
	int err = _arr.init(count2bytes(max_count)+align_extra);
	union { char* c; long n; } u = {_arr};
	u.n += ALIGN-1;
	u.n &= -long(ALIGN);
	_align_offset = u.c - _arr;
	return err;
    }

    /* Destroy all contained objects and deallocate memory, returning
       the object to its uninitialized state.

       @return 0 on success or an appropriate errno
     */
    int fini() {
	for(size_t i=0; i < _size; i++)
	    (*this)[i].~T();

	_size = 0;
	return _arr.fini();
    }

    /* The largest number of elements the underlying dynarray instance
       can accommodate
     */
    size_t limit() const {
	return bytes2count(_arr.capacity());
    }

    /* The current capacity of this dynvector (= elements worth of
       allocated memory)
     */
    size_t capacity() const {
	return bytes2count(_arr.size());
    }

    /* The current logical size of this dynvector (= elements pushed
       so far)
     */
    size_t size() const {
	return _size;
    }

    /* Ensure space for the requested number of elements.

       Spare capacity is managed automatically, but this method can be
       useful if the caller knows the structure will grow rapidly or
       needs to ensure early on that all the needed capacity is
       available (e.g. Linux... overcommit.. OoM killer).

       @return 0 on success or an appropriate errno
    */
    int reserve(size_t new_capacity) {
	return _arr.resize(count2bytes(new_capacity));
    }

    /* Default-construct objects at-end (if needed) to make /size() == new_size/

       @return 0 on success or an appropriate errno
     */
    int resize(size_t new_size) {
	if(int err=_maybe_grow(new_size))
	    return err;

	for(size_t i=size(); i < new_size; i++)
	    new (_at(i)) T;
	
	_size = new_size;
	return 0;
    }

    /* Add /obj/ at-end, incrementing /size()/ by one

       @return 0 on success or an appropriate errno
     */
    int push_back(T const &obj) {
	size_t new_size = _size+1;
	if(int err=_maybe_grow(new_size))
	    return err;

	new (_at(_size)) T(obj);
	_size = new_size;
	return 0;
    }

    T &back() { return this->operator[](size()-1); }
    T const &back() const { return this->operator[](size()-1); }

    /* Returns the ith element of the array; it is the caller's
       responsibility to ensure the index is in bounds.
     */
    T &operator[](size_t i) { return *(T*) _at(i); }
    T const &operator[](size_t i) const { return *(T const*) _at(i); }

    dynvector() : _size(0), _align_offset(0) { }
    
private:
    static size_t count2bytes(size_t count) { return count*sizeof(T); }
    static size_t bytes2count(size_t bytes) { return bytes/sizeof(T); }

    int _maybe_grow(size_t new_count) {
	static size_t const MIN_SIZE = 64;
	size_t new_bytes  = std::max(MIN_SIZE, count2bytes(new_count));
	int err = 0;
	if(_arr.size() < new_bytes) {
	    size_t next_size = std::max(new_bytes, 2*_arr.size());
	    err = _arr.resize(next_size);
	
	    // did we reach a limit?
	    if(err == EFBIG) 
		err = _arr.resize(new_bytes);
	}
	return err;
    }

    void* _at(size_t idx) const {
	return (void*) &_arr[_align_offset + count2bytes(idx)];
    }
    
    dynarray _arr;
    size_t _size; // element count, not bytes!!
    size_t _align_offset;
};


#endif
