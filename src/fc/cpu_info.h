#ifndef __CPU_INFO_H
#define __CPU_INFO_H

struct cpu_info {
    static long cpuid();
    static long socket_of(long cpuid);
    
    static long socket_self();
    static long socket_count() { return get_helper()->socket_count; }
    static long cpu_count() { return get_helper()->cpu_count; }

    struct helper {
	static void compute_counts(long *ccount, long* scount);
	helper() {
	    compute_counts(&cpu_count, &socket_count);
	}
	long socket_count;
	long cpu_count;
    };	
    static helper* get_helper() {
	static helper h;
	return &h;
    }
    struct impl_helper;
};

#ifdef __x86_64__
struct cpu_info::impl_helper {
#if 0
    enum { INTEL, AMD, OTHER };
    static int get_vendor() {
	union {
	    char vendor[12];
	    unsigned data[3];
	};
	asm("cpuid" : "=b"(data[0]), "=d"(data[1]), "=c"(data[2]) : "a"(0));
	if( ! std::memcmp(vendor, "GenuineIntel", sizeof(vendor)))
	    return INTEL;
	if( ! std::memcmp(vendor, "AuthenticAMD", sizeof(vendor)))
	    return AMD;
	return OTHER;
    }
#endif
    static unsigned cpuid(unsigned a1_b=cpuid_a1_b()) {
	return a1_b >> 24;
    }
    static unsigned socket_size(unsigned a1_b=cpuid_a1_b()) {
	return (a1_b >> 16) & 0xff;
    }
    static unsigned cpuid_a1_b() {
	unsigned output;
	asm("cpuid" : "=b"(output) : "a"(1));
	return output;
    }
};

inline
long cpu_info::cpuid() {
    return impl_helper::cpuid();
}

inline
long cpu_info::socket_self() {
    unsigned a1b = impl_helper::cpuid_a1_b();
    return impl_helper::cpuid(a1b)/impl_helper::socket_size(a1b);
}
inline
long cpu_info::socket_of(long cpuid) {
    return cpuid/impl_helper::socket_size();
}
#elif defined(__SVR4)
#include "polylock.h"
inline
long cpu_info::cpu_self() {
    return poly_lock::cpu_self();
}
#else
#error Sorry, no way to identify the cpu a thread is running on
#endif

#include <cstdio>
#ifdef __linux__
#include <cstring>
#include <set>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
inline
void cpu_info::helper::compute_counts(long* ccount, long* scount) {
    int fd = open("/proc/cpuinfo", O_RDONLY);
    int bufmax = 100;
    char buf[bufmax+1];
    int bufsize=0;
    std::set<int> cpus, sockets;
    int count;
    while( (count=read(fd, buf+bufsize, bufmax-bufsize)) > 0) {
	bufsize += count;
	buf[bufsize] = 0;
	
#define IF_MATCH(str, theSet)				\
	if ( (pos=strstr(buf, str)) ) {			\
	    int id;					\
	    if (1 == sscanf(pos, str "%d", &id)) {	\
		theSet.insert(id);			\
	    }						\
	    pos += sizeof(str);				\
	}
	
	char* pos;
	IF_MATCH("processor	: ", cpus)
	else IF_MATCH("physical id	: ", sockets)
	else {
	    pos = buf + bufsize/2;
	}
#undef IF_MATCH
	bufsize = buf+bufsize - pos;
	memmove(buf, pos, bufsize);
    }
    if(cpus.empty() || sockets.empty()) {
	fprintf(stderr, "Unable to read /proc/cpuinfo\n");
	exit(-1);
    }
    std::fprintf(stderr, "cpu_info sees %ld sockets and %ld cores\n",
		 sockets.size(), cpus.size());
    *ccount = cpus.size();
    *scount = sockets.size();
}
#elif defined(__SVR4)
#include <kstat.h>
#include <cstring>
inline
void cpu_info::helper::compute_counts(long* ccount, long* scount) {
    long socket_count = 0;
    long cpu_count = 0;
    kstat_ctl_t* kc = kstat_open();
    for (kstat_t* ksp = kc->kc_chain; ksp; ksp=ksp->ks_next) {
	if( ! std::strcmp(ksp->ks_module, "lgrp"))
	    _socket_count++;
	if(	! std::strcmp(ksp->ks_module, "cpu")
		&& ! std::strcmp(ksp->ks_name, "sys"))
	    _cpu_count++;
    }
    kstat_close(kc);
    std::fprintf(stderr, "cpu_info sees %ld sockets and %ld cores\n",
		 socket_count, cpu_count);
    *ccount = cpu_count;
    *scount = socket_count;
}
#else
#error Sorry, no way to identify the number of sockets in this machine
#endif

static struct cpu_info_init {
    cpu_info_init() {
	cpu_info::get_helper();
    }
} __cpu_info_init;

#endif
