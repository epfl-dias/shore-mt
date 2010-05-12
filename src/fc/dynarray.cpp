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

#include "dynarray.h"

#include <errno.h>
#include <sys/mman.h>
#include <algorithm>
#include <cstdlib>
#include <cassert>

// no system I know of *requires* larger pages than this
size_t const dynarray::PAGE_SIZE = 8192;
// most systems can't handle bigger than this, and we need a sanity check
static size_t const MM_MAX_CAPACITY = dynarray::PAGE_SIZE*1024*1024*1024;

static size_t mm_page_ceil(size_t bytes) { return (bytes+dynarray::PAGE_SIZE-1) &~ (dynarray::PAGE_SIZE-1); }
static size_t mm_bytes2pages(size_t bytes) { return (bytes+dynarray::PAGE_SIZE-1)/dynarray::PAGE_SIZE; }
static size_t mm_pages2bytes(uint32_t pages) { return pages*dynarray::PAGE_SIZE; }

int dynarray::init(size_t max_size) {
    // round up to the nearest page boundary
    max_size = mm_page_ceil(max_size);
    
    // validate inputs
    if(max_size > MM_MAX_CAPACITY)
	return EFBIG;
    if(dynarray::PAGE_SIZE > max_size)
	return EINVAL;

    /*
      The magical incantation below tells mmap to reserve address
      space within the process without actually allocating any
      memory. We are then free to re-map any subset of that
      reservation using MAP_FIXED (using MAP_FIXED without a
      reservation always fails).

      Note that MAP_FIXED is smart enough to mix and match different
      sets of permissions, so we can extend the array simply by
      remapping 0..new_size with R/W permissions, and can blow
      everything away by unmapping 0..reserved_size.

      Tested on both Linux-2.6.18/x86 and Solaris-10/Sparc.
    */
    static int const PROTS = PROT_NONE;
    static int const FLAGS = MAP_NORESERVE | MAP_ANON | MAP_PRIVATE;
    void* result = mmap(0, max_size, PROTS, FLAGS, -1, 0);
    if(result == MAP_FAILED)
	return errno;

    _base = (char*) result;
    _capacity = max_size;
    _size = 0;
    return 0;
}

int dynarray::init(dynarray const &to_copy, size_t max_size) {
    max_size = std::max(max_size, to_copy.capacity());
    if(int err=init(max_size))
	return err;

    std::memmove(_base, to_copy._base, to_copy.size());
    return 0;
}

int dynarray::fini() {
    if(int err=munmap(_base, _capacity))
	return err;
        
    _base = 0;
    _size = 0;
    _capacity = 0;
    return 0;
}

int dynarray::resize(size_t new_size) {
    // round up to the nearest page boundary
    new_size = mm_page_ceil(new_size);

    // validate
    if(_size > new_size)
	return EINVAL;

    static int const PROTS = PROT_READ | PROT_WRITE;
    static int const FLAGS = MAP_FIXED | MAP_ANON | MAP_PRIVATE;

    // remap the new range as RW. Don't mess w/ the existing region!!
    void* result = mmap(_base+_size, new_size-_size, PROTS, FLAGS, -1, 0);
    if(result == MAP_FAILED)
	return errno;

    _size = new_size;
    return 0;
}

#ifdef TEST_ME
#include <unistd.h>
#include <cstdio>

static int foocount = 0;
struct foo {
    int id;
    foo() : id(++foocount) { std::fprintf(stderr, "foo#%d\n", id); }
    ~foo() { std::fprintf(stderr, "~foo#%d\n", id); }
};

int main() {
    {
	dynarray mm;
	int err;

	err = mm.init(5l*1024*1024*1024);
	char const* base = mm;
	std::fprintf(stderr, "&mm[0] = %p\n", base);
	err = mm.resize(10000);
	err = mm.resize(100000);
	err = mm.resize(1000000);
	err = mm.fini();
    }

    {
	int err;
	dynvector<foo> dv;
	err = dv.init(100000);
	std::fprintf(stderr, "size:%ld  capacity:%ld  limit:%ld\n", dv.size(), dv.capacity(), dv.limit());
	err = dv.resize(4);
	std::fprintf(stderr, "size:%ld  capacity:%ld  limit:%ld\n", dv.size(), dv.capacity(), dv.limit());
	err = dv.resize(10);
	std::fprintf(stderr, "size:%ld  capacity:%ld  limit:%ld\n", dv.size(), dv.capacity(), dv.limit());
	foo f;
	err = dv.push_back(f);
	err = dv.push_back(f);
	err = dv.push_back(f);
	std::fprintf(stderr, "size:%ld  capacity:%ld  limit:%ld\n", dv.size(), dv.capacity(), dv.limit());
	err = dv.resize(16);
	std::fprintf(stderr, "size:%ld  capacity:%ld  limit:%ld\n", dv.size(), dv.capacity(), dv.limit());
	err = dv.fini();
    }

    return 0;
}
#endif
