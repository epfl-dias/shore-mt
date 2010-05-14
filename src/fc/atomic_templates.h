/*
* TODO: COPYRIGHT 
* These typdefs could be put in a more generic place, but for now,
* are needed only for the atomic ops definitions, so we'll leave them here.
*/

#ifndef ATOMIC_TEMPLATES_H
#define ATOMIC_TEMPLATES_H

#include "atomic_ops.h"

template<class T>
void atomic_add(T volatile &val, int delta);

template<class T>
T atomic_add_nv(T volatile &val, int delta);

template<>
inline void atomic_add(unsigned int volatile &val, int delta) 
{ atomic_add_int(&val, delta); }

template<>
inline void atomic_add(int volatile &val, int delta) 
{ atomic_add_int((unsigned int*) &val, delta); }

template<>
inline void atomic_add(unsigned long volatile &val, int delta) 
{ atomic_add_long(&val, delta); }

#ifndef ARCH_LP64
// Additional template needed when building for 32 bits:
template<>
inline void atomic_add(unsigned long long volatile &val, int delta) 
{ 
    int64_t  deltalg=delta;
    atomic_add_64(&val, deltalg); 
}
#endif

template<>
inline void atomic_add(long volatile &val, int delta) 
{ atomic_add_long((unsigned long*) &val, delta); }

template<>
inline unsigned int atomic_add_nv(unsigned int volatile &val, int delta) 
{ return atomic_add_int_nv(&val, delta); }

template<>
inline int atomic_add_nv(int volatile &val, int delta) 
{ return atomic_add_int_nv((unsigned int*) &val, delta); }

template<>
inline unsigned long atomic_add_nv(unsigned long volatile &val, int delta) 
{ return atomic_add_long_nv(&val, delta); }

template<>
inline long atomic_add_nv(long volatile &val, int delta) 
{ return atomic_add_long_nv((unsigned long*) &val, delta); }

#endif
