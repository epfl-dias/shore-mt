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
#ifndef __W_TEMP_ALLOC_H
#define __W_TEMP_ALLOC_H

#include "w_defines.h"

#include <algorithm>
#include <cstring>

/* A thread-local region allocator, designed for objects with a
   lifetime similar to that of a variable on the stack, but which are
   inconvenient to actually allocate on the stack for some
   reason. Examples include objects whose size is unknown at compile
   time, or whose allocation is performed by a helper function.

   The allocator is *extremely* fast due to its extreme simplicity: a
   "bump" allocator that remembers only how much memory it has
   dispensed so far. Creating a w_temp_alloc::mark object records the
   allocator's current state, which will be restored when the mark
   goes out of scope, freeing all memory that was allocated by that
   thread during its lifetime. As with calling malloc() or placement
   new, the user must call destructors manually if such is required.

   As with stack-allocated storage, it is unsafe to assume that memory
   obtained from w_temp_alloc can be passed to other threads or stored
   after the current function returns, because a w_temp_alloc::mark
   could be about to go out of scope in the caller. However, a
   function *can* safely return memory obtained from w_temp_alloc to
   the caller, because the caller knows when (or whether) any of its
   marks will invalidate the memory.

   There is also an STL-compatible allocator based on this allocator,
   for efficient allocation of temporary containers, strings, etc.
*/
struct w_temp_alloc {
    struct mark {
        size_t end;
        mark();
        ~mark();
    };
    
    /* A pointer to storage that may have been allocated by
       w_temp_alloc and should not be passed to ::free or operator
       delete. Unlike w_temp_alloc::buf, it is POD (no constructor or
       destructor) and does *not* allocate storage when initialized or
       copied.
    */
    struct ptr {
        size_t _bufsz;
        char *_buf;

        struct _decay {
            char *buf;
            template <typename T>
            operator T*() const { return (T*) buf; }
        };

        struct _decay_const {
            char const *buf;
            template <typename T>
            operator T const*() const { return (T const*) buf; }
        };

        bool is_valid() const { return _buf; }
        size_t size() const { return _bufsz; }
    
        _decay decay() {
	    _decay result = { _buf };
	    return result;
	}
        _decay_const decay() const {
	    _decay_const result = { _buf };
	    return result;
	}
    
        // change the (perceived) size of this buf
        void resize(size_t newsz) {
            if (newsz > _bufsz) 
                *this = alloc(newsz);
            else
                _bufsz = newsz;
        }

        void clear() {
            _bufsz = 0;
            _buf = 0;
        }
    };

    // Allocate a chunk of memory
    static ptr alloc(size_t sz);

    /* Convenience method: an instance of w_temp_alloc decays to a
       pointer of any type, as well as w_temp_alloc::ptr. This allows
       the following idiom, for example:

       w_temp_alloc::ptr p1 = w_temp_alloc(32);
       void *p2 = w_temp_alloc(48);
       char *p3 = w_temp_alloc(100);
       
     */
    explicit w_temp_alloc(size_t sz)
        : _ptr(alloc(sz))
    {
    }

    operator ptr() { return _ptr; }

    template <typename T>
    operator T*() { return _ptr.decay(); }

    /* An object that represents the value of a chunk of memory. When
       copied, it allocates new storage and copies the contents over
       (unlike w_temp_alloc::ptr, which merly copies the pointer).
     */
    struct buf : ptr {
        explicit buf(size_t sz=0)
            : ptr(alloc(sz))
        {
        }

        /* wrap a pointer to an arbitrary chunk of memory (not
           necessarily allocated by w_temp_alloc)
        */
        buf(size_t sz, char *ptr) {
            _bufsz = sz;
            _buf = ptr;
        }

        // copy a chunk of memory
        buf(buf const &v)
            : ptr(alloc(v._bufsz))
        {
            memcpy(_buf, v._buf, v._bufsz);
        }

        // copy an existing buffer into this one
        buf &operator=(buf v);

        ~buf() { clear(); }
    };

    // an STL-compatible allocator
    template <typename T>
    struct allocator {
        typedef T value_type;

        typedef T *pointer;
        typedef T const *const_pointer;
        
        typedef T &reference;
        typedef T const &const_reference;

        typedef size_t size_type;
        typedef ptrdiff_t difference_type;

        allocator() { }
        allocator(allocator const &) { }
        template <typename U>
        allocator(allocator<U> const &) { }

        T* allocate(size_t n, void* =0) {
            return (T*) alloc(n*sizeof(value_type)).decay();
        }
        
        void deallocate(T*, size_t) { /* nop */ }
        
        size_type max_size() const { return size_t(-8); }
        
        template <class U> struct rebind { typedef allocator<U> other; };

#ifdef USE_CXX11
        template <typename... Args>
        void construct(pointer p, Args... args) {
            new (p) value_type(std::forward<Args>(args)...);
        }
#endif

        void construct(pointer p, const_reference v) {
            new (p) value_type(v);
        }

        void destroy(pointer p) {
            p->~value_type();
        }
    };

    ptr _ptr;
};

// efficient swap for w_temp_alloc::buf (no copying involved) 
inline
void swap(w_temp_alloc::buf &a, w_temp_alloc::buf &b) {
    using std::swap;
    swap(a._bufsz, b._bufsz);
    swap(a._buf, b._buf);
}

inline
w_temp_alloc::buf &w_temp_alloc::buf::operator=(w_temp_alloc::buf v) {
    using std::swap;
    swap(*this, v);
    return *this;
}


// stupid, but required by the STL...
template <typename A, typename B>
bool operator==(w_temp_alloc::allocator<A> const &, w_temp_alloc::allocator<B> const &) {
    return true;
}

template <typename A, typename B>
bool operator!=(w_temp_alloc::allocator<A> const &, w_temp_alloc::allocator<B> const &) {
    return false;
}

#endif
