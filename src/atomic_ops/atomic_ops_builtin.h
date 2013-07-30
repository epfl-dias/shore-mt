#ifndef	_SYS_ATOMIC_H
#define	_SYS_ATOMIC_H

#include <sys/types.h>
#include <inttypes.h>
// #include <sys/inttypes.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(_KERNEL) || defined(__STDC__)
/*
 * Increment target.
 */
static inline void atomic_inc_8(volatile uint8_t *ptr) {
    __sync_add_and_fetch(ptr, 1);
}
static inline void atomic_inc_uchar(volatile uchar_t *ptr) {
    __sync_add_and_fetch(ptr, 1);
}
static inline void atomic_inc_16(volatile uint16_t *ptr) {
    __sync_add_and_fetch(ptr, 1);
}
static inline void atomic_inc_ushort(volatile ushort_t *ptr) {
    __sync_add_and_fetch(ptr, 1);
}
static inline void atomic_inc_32(volatile uint32_t *ptr) {
    __sync_add_and_fetch(ptr, 1);
}
static inline void atomic_inc_uint(volatile uint_t *ptr) {
    __sync_add_and_fetch(ptr, 1);
}
static inline void atomic_inc_ulong(volatile ulong_t *ptr) {
    __sync_add_and_fetch(ptr, 1);
}
#if defined(_KERNEL) || defined(_INT64_TYPE)
static inline void atomic_inc_64(volatile uint64_t *ptr) {
    __sync_add_and_fetch(ptr, 1);
}
#endif

/*
 * Decrement target
 */
static inline void atomic_dec_8(volatile uint8_t *ptr) {
    __sync_add_and_fetch(ptr, -1);
}
static inline void atomic_dec_uchar(volatile uchar_t *ptr) {
    __sync_add_and_fetch(ptr, -1);
}
static inline void atomic_dec_16(volatile uint16_t *ptr) {
    __sync_add_and_fetch(ptr, -1);
}
static inline void atomic_dec_ushort(volatile ushort_t *ptr) {
    __sync_add_and_fetch(ptr, -1);
}
static inline void atomic_dec_32(volatile uint32_t *ptr) {
    __sync_add_and_fetch(ptr, -1);
}
static inline void atomic_dec_uint(volatile uint_t *ptr) {
    __sync_add_and_fetch(ptr, -1);
}
static inline void atomic_dec_ulong(volatile ulong_t *ptr) {
    __sync_add_and_fetch(ptr, -1);
}
#if defined(_KERNEL) || defined(_INT64_TYPE)
static inline void atomic_dec_64(volatile uint64_t *ptr) {
    __sync_add_and_fetch(ptr, -1);
}
#endif

/*
 * Add delta to target
 */
static inline void atomic_add_8(volatile uint8_t *ptr, int8_t val) {
    __sync_add_and_fetch(ptr, val);
}
static inline void atomic_add_char(volatile uchar_t *ptr, signed char val) {
    __sync_add_and_fetch(ptr, val);
}
static inline void atomic_add_16(volatile uint16_t *ptr, int16_t val) {
    __sync_add_and_fetch(ptr, val);
}
static inline void atomic_add_short(volatile ushort_t *ptr, short val) {
    __sync_add_and_fetch(ptr, val);
}
static inline void atomic_add_32(volatile uint32_t *ptr, int32_t val) {
    __sync_add_and_fetch(ptr, val);
}
static inline void atomic_add_int(volatile uint_t *ptr, int val) {
    __sync_add_and_fetch(ptr, val);
}
static inline void atomic_add_ptr(volatile void *ptr, ssize_t val) {
    (void)__sync_add_and_fetch((char ** volatile)ptr, val);
}
static inline void atomic_add_long(volatile ulong_t *ptr, long val) {
    __sync_add_and_fetch(ptr, val);
}
#if defined(_KERNEL) || defined(_INT64_TYPE)
static inline void atomic_add_64(volatile uint64_t *ptr, int64_t val) {
    __sync_add_and_fetch(ptr, val);
}
#endif

/*
 * logical OR bits with target
 */
static inline void atomic_or_8(volatile uint8_t *ptr, uint8_t val) {
    __sync_or_and_fetch(ptr, val);
}
static inline void atomic_or_uchar(volatile uchar_t *ptr, uchar_t val) {
    __sync_or_and_fetch(ptr, val);
}
static inline void atomic_or_16(volatile uint16_t *ptr, uint16_t val) {
    __sync_or_and_fetch(ptr, val);
}
static inline void atomic_or_ushort(volatile ushort_t *ptr, ushort_t val) {
    __sync_or_and_fetch(ptr, val);
}
static inline void atomic_or_32(volatile uint32_t *ptr, uint32_t val) {
    __sync_or_and_fetch(ptr, val);
}
static inline void atomic_or_uint(volatile uint_t *ptr, uint_t val) {
    __sync_or_and_fetch(ptr, val);
}
static inline void atomic_or_ulong(volatile ulong_t *ptr, ulong_t val) {
    __sync_or_and_fetch(ptr, val);
}
#if defined(_KERNEL) || defined(_INT64_TYPE)
static inline void atomic_or_64(volatile uint64_t *ptr, uint64_t val) {
    __sync_or_and_fetch(ptr, val);
}
#endif

/*
 * logical AND bits with target
 */
static inline void atomic_and_8(volatile uint8_t *ptr, uint8_t val) {
    __sync_and_and_fetch(ptr, val);
}
static inline void atomic_and_uchar(volatile uchar_t *ptr, uchar_t val) {
    __sync_and_and_fetch(ptr, val);
}
static inline void atomic_and_16(volatile uint16_t *ptr, uint16_t val) {
    __sync_and_and_fetch(ptr, val);
}
static inline void atomic_and_ushort(volatile ushort_t *ptr, ushort_t val) {
    __sync_and_and_fetch(ptr, val);
}
static inline void atomic_and_32(volatile uint32_t *ptr, uint32_t val) {
    __sync_and_and_fetch(ptr, val);
}
static inline void atomic_and_uint(volatile uint_t *ptr, uint_t val) {
    __sync_and_and_fetch(ptr, val);
}
static inline void atomic_and_ulong(volatile ulong_t *ptr, ulong_t val) {
    __sync_and_and_fetch(ptr, val);
}
#if defined(_KERNEL) || defined(_INT64_TYPE)
static inline void atomic_and_64(volatile uint64_t *ptr, uint64_t val) {
    __sync_and_and_fetch(ptr, val);
}
#endif

/*
 * As above, but return the new value.  Note that these _nv() variants are
 * substantially more expensive on some platforms than the no-return-value
 * versions above, so don't use them unless you really need to know the
 * new value *atomically* (e.g. when decrementing a reference count and
 * checking whether it went to zero).
 */

/*
 * Increment target and return new value.
 */
static inline uint8_t atomic_inc_8_nv(volatile uint8_t *x) {
    return __sync_add_and_fetch(x, 1);
}
static inline uchar_t atomic_inc_uchar_nv(volatile uchar_t *x) {
    return __sync_add_and_fetch(x, 1);
}
static inline uint16_t atomic_inc_16_nv(volatile uint16_t *x) {
    return __sync_add_and_fetch(x, 1);
}
static inline ushort_t atomic_inc_ushort_nv(volatile ushort_t *x) {
    return __sync_add_and_fetch(x, 1);
}
static inline uint32_t atomic_inc_32_nv(volatile uint32_t *x) {
    return __sync_add_and_fetch(x, 1);
}
static inline uint_t atomic_inc_uint_nv(volatile uint_t *x) {
    return __sync_add_and_fetch(x, 1);
}
static inline ulong_t atomic_inc_ulong_nv(volatile ulong_t *x) {
    return __sync_add_and_fetch(x, 1);
}
#if defined(_KERNEL) || defined(_INT64_TYPE)
static inline uint64_t atomic_inc_64_nv(volatile uint64_t *x) {
    return __sync_add_and_fetch(x, 1);
}
#endif

/*
 * Decrement target and return new value.
 */
static inline uint8_t atomic_dec_8_nv(volatile uint8_t *x) {
    return __sync_add_and_fetch(x, -1);
}
static inline uchar_t atomic_dec_uchar_nv(volatile uchar_t *x) {
    return __sync_add_and_fetch(x, -1);
}
static inline uint16_t atomic_dec_16_nv(volatile uint16_t *x) {
    return __sync_add_and_fetch(x, -1);
}
static inline ushort_t atomic_dec_ushort_nv(volatile ushort_t *x) {
    return __sync_add_and_fetch(x, -1);
}
static inline uint32_t atomic_dec_32_nv(volatile uint32_t *x) {
    return __sync_add_and_fetch(x, -1);
}
static inline uint_t atomic_dec_uint_nv(volatile uint_t *x) {
    return __sync_add_and_fetch(x, -1);
}
static inline ulong_t atomic_dec_ulong_nv(volatile ulong_t *x) {
    return __sync_add_and_fetch(x, -1);
}
#if defined(_KERNEL) || defined(_INT64_TYPE)
static inline uint64_t atomic_dec_64_nv(volatile uint64_t *x) {
    return __sync_add_and_fetch(x, -1);
}
#endif

/*
 * Add delta to target
 */
static inline uint8_t atomic_add_8_nv(volatile uint8_t *ptr, int8_t val) {
    return __sync_add_and_fetch(ptr, val);
}
static inline uchar_t atomic_add_char_nv(volatile uchar_t *ptr, signed char val) {
    return __sync_add_and_fetch(ptr, val);
}
static inline uint16_t atomic_add_16_nv(volatile uint16_t *ptr, int16_t val) {
    return __sync_add_and_fetch(ptr, val);
}
static inline ushort_t atomic_add_short_nv(volatile ushort_t *ptr, short val) {
    return __sync_add_and_fetch(ptr, val);
}
static inline uint32_t atomic_add_32_nv(volatile uint32_t *ptr, int32_t val) {
    return __sync_add_and_fetch(ptr, val);
}
static inline uint_t atomic_add_int_nv(volatile uint_t *ptr, int val) {
    return __sync_add_and_fetch(ptr, val);
}
static inline void *atomic_add_ptr_nv(volatile void *ptr, ssize_t val) {
    return __sync_add_and_fetch((char ** volatile)ptr, val);
}
static inline ulong_t atomic_add_long_nv(volatile ulong_t *ptr, long val) {
    return __sync_add_and_fetch(ptr, val);
}
#if defined(_KERNEL) || defined(_INT64_TYPE)
static inline uint64_t atomic_add_64_nv(volatile uint64_t *ptr, int64_t val) {
    return __sync_add_and_fetch(ptr, val);
}
#endif

/*
 * logical OR bits with target and return new value.
 */
static inline uint8_t atomic_or_8_nv(volatile uint8_t *ptr, uint8_t val) {
    return __sync_or_and_fetch(ptr, val);
}
static inline uchar_t atomic_or_uchar_nv(volatile uchar_t *ptr, uchar_t val) {
    return __sync_or_and_fetch(ptr, val);
}
static inline uint16_t atomic_or_16_nv(volatile uint16_t *ptr, uint16_t val) {
    return __sync_or_and_fetch(ptr, val);
}
static inline ushort_t atomic_or_ushort_nv(volatile ushort_t *ptr, ushort_t val) {
    return __sync_or_and_fetch(ptr, val);
}
static inline uint32_t atomic_or_32_nv(volatile uint32_t *ptr, uint32_t val) {
    return __sync_or_and_fetch(ptr, val);
}
static inline uint_t atomic_or_uint_nv(volatile uint_t *ptr, uint_t val) {
    return __sync_or_and_fetch(ptr, val);
}
static inline ulong_t atomic_or_ulong_nv(volatile ulong_t *ptr, ulong_t val) {
    return __sync_or_and_fetch(ptr, val);
}
#if defined(_KERNEL) || defined(_INT64_TYPE)
static inline uint64_t atomic_or_64_nv(volatile uint64_t *ptr, uint64_t val) {
    return __sync_or_and_fetch(ptr, val);
}
#endif

/*
 * logical AND bits with target and return new value.
 */
static inline uint8_t atomic_and_8_nv(volatile uint8_t *ptr, uint8_t val) {
    return __sync_and_and_fetch(ptr, val);
}
static inline uchar_t atomic_and_uchar_nv(volatile uchar_t *ptr, uchar_t val) {
    return __sync_and_and_fetch(ptr, val);
}
static inline uint16_t atomic_and_16_nv(volatile uint16_t *ptr, uint16_t val) {
    return __sync_and_and_fetch(ptr, val);
}
static inline ushort_t atomic_and_ushort_nv(volatile ushort_t *ptr, ushort_t val) {
    return __sync_and_and_fetch(ptr, val);
}
static inline uint32_t atomic_and_32_nv(volatile uint32_t *ptr, uint32_t val) {
    return __sync_and_and_fetch(ptr, val);
}
static inline uint_t atomic_and_uint_nv(volatile uint_t *ptr, uint_t val) {
    return __sync_and_and_fetch(ptr, val);
}
static inline ulong_t atomic_and_ulong_nv(volatile ulong_t *ptr, ulong_t val) {
    return __sync_and_and_fetch(ptr, val);
}
#if defined(_KERNEL) || defined(_INT64_TYPE)
static inline uint64_t atomic_and_64_nv(volatile uint64_t *ptr, uint64_t val) {
    return __sync_and_and_fetch(ptr, val);
}
#endif

/*
 * If *arg1 == arg2, set *arg1 = arg3; return old value
 */
static inline uint8_t atomic_cas_8(volatile uint8_t *ptr, uint8_t ov, uint8_t nv) {
    return __sync_val_compare_and_swap(ptr, ov, nv);
}
static inline uchar_t atomic_cas_uchar(volatile uchar_t *ptr, uchar_t ov, uchar_t nv) {
    return __sync_val_compare_and_swap(ptr, ov, nv);
}
static inline uint16_t atomic_cas_16(volatile uint16_t *ptr, uint16_t ov, uint16_t nv) {
    return __sync_val_compare_and_swap(ptr, ov, nv);
}
static inline ushort_t atomic_cas_ushort(volatile ushort_t *ptr, ushort_t ov, ushort_t nv) {
    return __sync_val_compare_and_swap(ptr, ov, nv);
}
static inline uint32_t atomic_cas_32(volatile uint32_t *ptr, uint32_t ov, uint32_t nv) {
    return __sync_val_compare_and_swap(ptr, ov, nv);
}
static inline uint_t atomic_cas_uint(volatile uint_t *ptr, uint_t ov, uint_t nv) {
    return __sync_val_compare_and_swap(ptr, ov, nv);
}
static inline void *atomic_cas_ptr(volatile void *ptr, void * ov, void * nv) {
    return __sync_val_compare_and_swap((char ** volatile)ptr, ov, nv);
}
static inline ulong_t atomic_cas_ulong(volatile ulong_t *ptr, ulong_t ov, ulong_t nv) {
    return __sync_val_compare_and_swap(ptr, ov, nv);
}
#if defined(_KERNEL) || defined(_INT64_TYPE) || defined(__GLIBC_HAVE_LONG_LONG)
static inline uint64_t atomic_cas_64(volatile uint64_t *ptr, uint64_t ov, uint64_t nv) {
    return __sync_val_compare_and_swap(ptr, ov, nv);
}
#endif

/*
 * Swap target and return old value
 */
static inline uint8_t atomic_swap_8(volatile uint8_t *ptr, uint8_t val) {
    return __sync_lock_test_and_set(ptr, val);
}
static inline uchar_t atomic_swap_uchar(volatile uchar_t *ptr, uchar_t val) {
    return __sync_lock_test_and_set(ptr, val);
}
static inline uint16_t atomic_swap_16(volatile uint16_t *ptr, uint16_t val) {
    return __sync_lock_test_and_set(ptr, val);
}
static inline ushort_t atomic_swap_ushort(volatile ushort_t *ptr, ushort_t val) {
    return __sync_lock_test_and_set(ptr, val);
}
static inline uint32_t atomic_swap_32(volatile uint32_t *ptr, uint32_t val) {
    return __sync_lock_test_and_set(ptr, val);
}
static inline uint_t atomic_swap_uint(volatile uint_t *ptr, uint_t val) {
    return __sync_lock_test_and_set(ptr, val);
}
static inline void *atomic_swap_ptr(volatile void *ptr, void * val) {
    return __sync_lock_test_and_set((char ** volatile)ptr, val);
}
static inline ulong_t atomic_swap_ulong(volatile ulong_t *ptr, ulong_t val) {
    return __sync_lock_test_and_set(ptr, val);
}
#if defined(_KERNEL) || defined(_INT64_TYPE)
static inline uint64_t atomic_swap_64(volatile uint64_t *ptr, uint64_t val) {
    return __sync_lock_test_and_set(ptr, val);
}
#endif

/*
 * Perform an exclusive atomic bit set/clear on a target.
 * Returns 0 if bit was sucessfully set/cleared, or -1
 * if the bit was already set/cleared.
 */
static inline int atomic_set_long_excl(volatile ulong_t *ptr, uint_t bit) {
    ulong_t val = 1ul << bit;
    ulong_t ov = *ptr;
    while (1) {
        if (ov & val)
            return -1;
        
        ulong_t cur = __sync_val_compare_and_swap(ptr, ov, ov|val);
        if (cur == ov)
            return 0;
        ov = cur;
    }
}

static inline int atomic_clear_long_excl(volatile ulong_t *ptr, uint_t bit) {
    ulong_t val = 1ul << bit;
    ulong_t ov = *ptr;
    while (1) {
        if (! (ov & val))
            return -1;
        
        ulong_t cur = __sync_val_compare_and_swap(ptr, ov, ov & ~val);
        if (cur == ov)
            return 0;
        ov = cur;
    }
}

/*
 * Generic memory barrier used during lock entry, placed after the
 * memory operation that acquires the lock to guarantee that the lock
 * protects its data.  No stores from after the memory barrier will
 * reach visibility, and no loads from after the barrier will be
 * resolved, before the lock acquisition reaches global visibility.
 */
static inline void membar_enter(void) {
    __sync_synchronize();
}

/*
 * Generic memory barrier used during lock exit, placed before the
 * memory operation that releases the lock to guarantee that the lock
 * protects its data.  All loads and stores issued before the barrier
 * will be resolved before the subsequent lock update reaches visibility.
 */
static inline void membar_exit(void) {
    __sync_synchronize();
}

/*
 * Arrange that all stores issued before this point in the code reach
 * global visibility before any stores that follow; useful in producer
 * modules that update a data item, then set a flag that it is available.
 * The memory barrier guarantees that the available flag is not visible
 * earlier than the updated data, i.e. it imposes store ordering.
 */
static inline void membar_producer(void) {
    __sync_synchronize();
}

/*
 * Arrange that all loads issued before this point in the code are
 * completed before any subsequent loads; useful in consumer modules
 * that check to see if data is available and read the data.
 * The memory barrier guarantees that the data is not sampled until
 * after the available flag has been seen, i.e. it imposes load ordering.
 */
static inline void membar_consumer(void) {
    __sync_synchronize();
}
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ATOMIC_H */
