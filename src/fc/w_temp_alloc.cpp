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

struct sentinel_pool : w_temp_alloc::pool {
    virtual w_temp_alloc::ptr alloc(size_t bytes);
};

static sentinel_pool _sentinel;
static __thread w_temp_alloc::pool *tpool = &_sentinel;

static pthread_key_t pool_key;
static pthread_once_t pool_init_once = PTHREAD_ONCE_INIT;


w_temp_alloc::ptr w_temp_alloc::alloc(size_t sz) { return tpool->alloc(sz); }

w_temp_alloc::mark::mark()
    : end(tpool->end)
{
}

w_temp_alloc::mark::~mark() {
    if (tpool->end > tpool->hiwater)
        tpool->hiwater = tpool->end;
    
    tpool->end = end;
}

static void pool_fini(w_temp_alloc::pool *p);

extern "C" {
    // pthread_key destructor
    static void w_temp_alloc_pthread_exit(void* arg __attribute__((unused)) ) {
        assert (arg == &tpool);
        pool_fini(tpool);
    }

    // atexit callback
    static void w_temp_alloc_atexit() {
        pool_fini(tpool);
    }
            
    static void w_temp_alloc_init() {
        if (pthread_key_create(&pool_key, &w_temp_alloc_pthread_exit)) {
            fprintf(stderr,
                    "WARNING: Unable to register destructors for thread-local pool allocator\n"
                    "-> thread-local pools will not be reclaimed until process exit!\n");
        }
        atexit(&w_temp_alloc_atexit);
    }
}

w_temp_alloc::ptr sentinel_pool::alloc(size_t sz) {
    // man page says failure is always due to programmer error
    if (pthread_once(&pool_init_once, &w_temp_alloc_init))
        throw std::bad_alloc();

    tpool = new w_temp_alloc::dynarray_pool(SQL_POOL_MMAP_LIMIT);

    // register it so the thread destructor is called
    pthread_setspecific(pool_key, tpool);
    if (pthread_getspecific(pool_key) != tpool)
        fprintf(stderr,
                "WARNING: unable to register destructor for w_temp_pool of tid=%zx!\n"
                "-> thread-local memory will not be reclaimed until process exit\n",
                (uintptr_t) pthread_self());
        
    return tpool->alloc(sz);
}

void pool_fini(w_temp_alloc::pool *p) {
    assert (p == tpool);
    assert (p != &_sentinel);
    
    if (p->end > p->hiwater)
        p->hiwater = p->end;
        
    SPLOG("Thread %zd w_temp_alloc peaked at %zd bytes\n",
          (size_t)pthread_self(), p->hiwater);

    delete p;
}

w_temp_alloc::ptr w_temp_alloc::fixed_pool::alloc(size_t sz) {
    size_t nbytes = align(sz);
    if (bufsz - end < nbytes)
        throw std::bad_alloc();
    
    uint32_t offset = end;
    end += nbytes;
    ptr rval = {sz, buf + offset};
    return rval;
}

w_temp_alloc::ptr w_temp_alloc::dynarray_pool::alloc(size_t sz) {
    size_t nbytes = align(sz);
    if (data.size() - end < nbytes and data.ensure_capacity(end+nbytes))
        throw std::bad_alloc();
    
    uint32_t offset = end;
    end += nbytes;
    ptr rval = {sz, data + offset};
    return rval;
}

w_temp_alloc::pool_swap::pool_swap(w_temp_alloc::pool *p)
    : _old_pool(p)
{
    std::swap(tpool, _old_pool);
}

w_temp_alloc::pool_swap::~pool_swap() {
    std::swap(tpool, _old_pool);
}
