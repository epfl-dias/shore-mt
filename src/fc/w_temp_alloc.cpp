/* -*- mode:C++; c-basic-offset:4 -*-
     Shore-MT -- Multi-threaded port of the SHORE storage manager
   
                          Copyright (c) 2013

                    Department of Computer Science
                        University of Toronto
                                   
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
#include "w_temp_alloc.h"

#include <pthread.h>
#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>
#include <memory>
#include <inttypes.h>

#if !HAVE_DECL_MAP_ANONYMOUS
#if HAVE_DECL_MAP_ANON
#define MAP_ANONYMOUS MAP_ANON
#else
#error I need a way to create anonymous private mappings with mmap
#endif
#endif

#ifndef SQL_POOL_MMAP_LIMIT
#define SQL_POOL_MMAP_LIMIT (10*1024*1024)
#endif

#define SQL_POOL_DEBUG

#ifdef SQL_POOL_DEBUG
#define SPLOG(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#else
#define SPLOG(fmt, ...)
#endif

struct w_temp_pool {
    char *buf;
    size_t bufsz;
    size_t end;
    size_t hiwater;

    void init();
    void fini();
    w_temp_alloc::ptr alloc(size_t sz);
    void grow_buf();
};

static __thread w_temp_pool tpool;
static pthread_key_t pool_key;
static pthread_once_t pool_init_once = PTHREAD_ONCE_INIT;
    
static size_t system_pagesz;
static size_t pool_buf_bound;


w_temp_alloc::ptr w_temp_alloc::alloc(size_t sz) { return tpool.alloc(sz); }

w_temp_alloc::mark::mark()
    : end(tpool.end)
{
}

w_temp_alloc::mark::~mark() {
    if (tpool.end > tpool.hiwater)
        tpool.hiwater = tpool.end;
    
    tpool.end = end;
}

extern "C" {
    // pthread_key destructor
    static void w_temp_alloc_pthread_exit(void* arg __attribute__((unused)) ) {
        assert (arg == &tpool);
        tpool.fini();
    }

    // atexit callback
    static void w_temp_alloc_atexit() {
        tpool.fini();
    }
            
    static void w_temp_alloc_init() {
        long rval = sysconf(_SC_PAGESIZE);
        if (rval < 0) {
            system_pagesz = 4096;
            fprintf(stderr,
                    "WARNING: Unable to determine the page size on this system.\n"
                    "-> Defaulting to %zd\n", system_pagesz);
        }
        system_pagesz = rval;
        pool_buf_bound = alignon(SQL_POOL_MMAP_LIMIT, system_pagesz);
        
        if (pthread_key_create(&pool_key, &w_temp_alloc_pthread_exit)) {
            fprintf(stderr,
                    "WARNING: Unable to register destructors for thread-local pool allocator\n"
                    "-> thread-local pools will not be reclaimed until process exit!\n");
        }
        atexit(&w_temp_alloc_atexit);
    }
}

void w_temp_pool::init() {
    // man page says failure is always due to programmer error
    if (pthread_once(&pool_init_once, &w_temp_alloc_init))
        throw std::bad_alloc();

    // register it so the thread destructor is called
    pthread_setspecific(pool_key, this);
    if (pthread_getspecific(pool_key) != this)
        fprintf(stderr,
                "WARNING: unable to register destructor for w_temp_pool of tid=%zx!\n"
                "-> thread-local memory will not be reclaimed until process exit\n",
                (uintptr_t) pthread_self());
        
    int flags = MAP_PRIVATE|MAP_ANONYMOUS;
    buf = (char*) mmap(0, pool_buf_bound, PROT_NONE, flags, -1, 0);
    if (buf == MAP_FAILED)
        throw std::bad_alloc();

}

void w_temp_pool::fini() {
    if (not buf)
        return; // never used in this thread
        
    if (end > hiwater)
        hiwater = end;
        
    SPLOG("Thread %zd w_temp_alloc peaked at %zd bytes\n",
          (size_t)pthread_self(), hiwater);
        
    munmap(buf, pool_buf_bound);
    bufsz = end = hiwater = 0;
    buf = 0;
}

void w_temp_pool::grow_buf() {
    size_t newsz = 3*bufsz/2;
    if (not buf) {
        init();
        newsz = 1024;
    }
    if (bufsz == pool_buf_bound)
        throw std::bad_alloc();
    
    newsz = alignon(newsz, system_pagesz);
    if (newsz > pool_buf_bound)
        newsz = pool_buf_bound;

    if (mprotect(buf+bufsz, newsz-bufsz, PROT_READ|PROT_WRITE))
        throw std::bad_alloc();
    
    bufsz = newsz;
}

w_temp_alloc::ptr w_temp_pool::alloc(size_t sz) {
    size_t nbytes = align(sz);
    if (bufsz - end < nbytes)
        grow_buf();
            
    uint32_t rval = end;
    end += nbytes;
    w_temp_alloc::ptr result = { sz, buf + rval };
    return result;
}
