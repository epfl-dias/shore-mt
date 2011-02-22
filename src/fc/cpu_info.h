#ifndef __CPU_SELF_H
#define __CPU_SELF_H

struct cpu_info {
    static long cpu_self();
    static long socket_self() {
	return socket_of(cpu_self());
    }

    static long socket_of(long cpuid) {
	return cpuid/socket_count();
    }
    
    //static long cpu_count();
    static long socket_count() {
	static long count = __compute_socket_count();
	return count;
    }

    static long __compute_socket_count();
};

#ifdef __x86_64__
long cpu_info::cpu_self() {
    unsigned output;
    asm("cpuid" : "=b"(output) : "a"(1));
    return output >> 24;
}
#elif defined(__SVR4)
#include "polylock.h"
long cpu_info::cpu_self() {
    return poly_lock::cpu_self();
}
#else
#error Sorry, no way to identify the cpu a thread is running on
#endif

#ifdef __linux__
#include <numa.h>
long cpu_info::__compute_socket_count() {
    return 1+numa_max_node();
}
#elif defined(__SVR4)
#include <kstat.h>
#include <cstring>
long cpu_info::__compute_socket_count() {
    long count = 0;
    kstat_ctl_t* kc = kstat_open();
    for (kstat_t* ksp = kc->kc_chain; ksp; ksp=ksp->ks_next) {
	if(std::strcmp(ksp->ks_module, "lgrp") == 0)
	    count++;
    }
    kstat_close(kc);
    return count;
}
#else
#error Sorry, no way to identify the number of sockets in this machine
#endif

#endif
