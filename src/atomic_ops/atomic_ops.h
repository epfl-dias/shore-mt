#include "shore-config.h"

#if defined(HAVE_ATOMIC_H) && !defined(HAVE_MEMBAR_ENTER)
#error  atomic_ops does not include membar_ops
#endif
#if defined(HAVE_MEMBAR_ENTER) && !defined(HAVE_ATOMIC_H) 
#error  membar_ops defined but atomic_ops missing
#endif

#ifdef HAVE_ATOMIC_H
#include <atomic.h>
#else

#if defined(__GLIBC_HAVE_LONG_LONG) && __GLIBC_HAVE_LONG_LONG!=0

#ifndef _INT64_TYPE
#define _INT64_TYPE
#define _INT64_TYPE_DEFINED
#endif

#endif

#include "atomic_ops_impl.h"

// Clean up after defining these
#ifdef _INT64_TYPE_DEFINED
#undef _INT64_TYPE
#undef _INT64_TYPE_DEFINED
#endif

#endif
