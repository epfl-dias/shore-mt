#include "cpu_info.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>

#ifdef __x86_64__
struct cpu_info::impl_helper {
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

long cpu_info::cpuid() {
    return impl_helper::cpuid();
}

long cpu_info::socket_self() {
    unsigned a1b = impl_helper::cpuid_a1_b();
    return impl_helper::cpuid(a1b)/impl_helper::socket_size(a1b);
}
long cpu_info::socket_of(long cpuid) {
    return cpuid/impl_helper::socket_size();
}
void cpu_info::init_impl() { }

#elif defined(__SVR4)
#include <dlfcn.h>
#include <spawn.h>
extern "C" {
    typedef void* pthread_run_fn(void*);
    typedef int pthread_create_fn(pthread_t*, pthread_attr_t const*, pthread_run_fn*, void*);
}

struct cpu_info::impl_helper {
    static pthread_run_fn run_wrapper;
    static pthread_create_fn create_wrapper;
    static pthread_create_fn* old_pthread_create;
    static __thread long volatile dtrace_cpuid;
    static pid_t dtrace_pid;
    struct runargs {
	pthread_run_fn* runme;
	void* arg;
    };
};

long cpu_info::socket_of(long cid) {
    return cid/(cpu_count()/socket_count());
}
long cpu_info::socket_self() { return socket_of(cpuid()); }
long cpu_info::cpuid() { return *&cpu_info::impl_helper::dtrace_cpuid; }

pid_t cpu_info::impl_helper::dtrace_pid = 0;
long volatile cpu_info::impl_helper::dtrace_cpuid = -1;

// these serve no purpose except to give dtrace a place to hook in
// they have to live somewhere else so they don't get inlined
extern "C" void cpu_info_dtrace_global_hook(long volatile* ready) ;
extern "C" void cpu_info_dtrace_thread_hook(long volatile* cpuid) ;

// these guys hook into thread creation/destruction so we don't have
// to manually initialize all threads

extern "C"
void*
cpu_info::impl_helper::run_wrapper(void* arg)
{
    runargs* a = (runargs*) arg;
    pthread_run_fn* runme = a->runme;
    arg = a->arg;
    delete a;
    cpu_info_dtrace_thread_hook(&dtrace_cpuid);
    return runme(arg);
}
extern "C" int
pthread_create(pthread_t *tid, pthread_attr_t const* attr,
	       pthread_run_fn* runme, void* arg)
{
    cpu_info::impl_helper::runargs a = {runme, arg};
    return cpu_info
	::impl_helper
	::old_pthread_create(tid, attr,
			     &cpu_info::impl_helper::run_wrapper,
			     new cpu_info::impl_helper::runargs(a));
}

pthread_create_fn* cpu_info::impl_helper::old_pthread_create=0;

void cpu_info::init_impl() {
    if(impl_helper::dtrace_pid)
	return;
    
    // interpose on libpthread
    impl_helper::old_pthread_create = (pthread_create_fn*)
	dlsym(RTLD_NEXT, "pthread_create");
    
    /* *****************************************************************
       Fire up dtrace
       *****************************************************************
       */
#define STR(x) STR2(x)
#define STR2(x) #x
#define ALLOCA(type) (type*) alloca(sizeof(type))
#define COPYIN(ptr, type) (type*) copyin((long) ptr, sizeof(type))
#define COPYOUT(src, dest) copyout(src, (long) dest, sizeof(*src))
    
    char pidstr[10];
    char* argv[] = {
	"dtrace", "-64", "-wqCn",
	"#pragma D option bufsize=4k\n"
	"#pragma D option aggsize=4k\n"
	STR(pid$1::cpu_info_dtrace_global_hook:entry
	    /**/ {
		/* tell our caller we're here so they can stop spinning */
		this->tmp = ALLOCA(long);
		*this->tmp = 1;
		COPYOUT(this->tmp, arg0);
	    }
	    pid$1::cpu_info_dtrace_thread_hook:entry
	    /**/ {
		/* store the pointer for later preemptions */
		self->cpuid_ptr = arg0;
	    }
	    pid$1::cpu_info_dtrace_thread_hook:entry) "," STR(
	    sched::resume:on-cpu
	    /self->cpuid_ptr/ {
		this->cpuid = ALLOCA(long);
		*this->cpuid = cpu;
		COPYOUT(this->cpuid, self->cpuid_ptr);
	    }
	    proc:::exit
	    /pid == $1/ {
		exit(0);
	    }
	    ),
	pidstr,
	0
    };

#define ATTEMPT(action) do {						\
	if(int err = action) {						\
	    std::fprintf(stderr, "%s failed with error %d\n",		\
			 #action, err);					\
	    std::exit(-1);						\
	}								\
    } while(0)

    // close stdin -- we need it and dtrace doesn't
    posix_spawn_file_actions_t fa;
    ATTEMPT(posix_spawn_file_actions_init(&fa));
    ATTEMPT(posix_spawn_file_actions_addclose(&fa, STDIN_FILENO));
    ATTEMPT(posix_spawn_file_actions_addclose(&fa, STDOUT_FILENO));
    ATTEMPT(posix_spawn_file_actions_addclose(&fa, STDERR_FILENO));

    // mask all signals in the dtrace script
    posix_spawnattr_t sa;
    sigset_t sm;
    sigfillset(&sm);
    sigdelset(&sm, SIGINT); // need some way to clean up lost processes...
    ATTEMPT(posix_spawnattr_init(&sa));
    ATTEMPT(posix_spawnattr_setsigmask(&sa, &sm));
    ATTEMPT(posix_spawnattr_setflags(&sa, POSIX_SPAWN_SETSIGMASK));

    // pass it our pid
    std::sprintf(pidstr, "%d", getpid());
	
    // go!
    ATTEMPT(posix_spawn(&impl_helper::dtrace_pid, "/usr/sbin/dtrace",
			&fa, &sa, argv, 0));

    // wait for dtrace to be ready
    long volatile dtrace_ready = 0;
    std::fprintf(stderr, "Waiting for dtrace...\n");
    while( ! *&dtrace_ready) {
	cpu_info_dtrace_global_hook(&dtrace_ready);
	usleep(10*1000); // sleep 10 ms
    }
    std::printf("... dtrace ready\n");
    cpu_info_dtrace_thread_hook(&impl_helper::dtrace_cpuid);
}

#else
#error Sorry, no way to identify the cpu a thread is running on
#endif

#if defined(__linux__) || defined(__CYGWIN__)
#include <set>
#include <fcntl.h>
#include <fstream>
#define IF_MATCH(str, theSet)				\
  if ( (pos=strstr(line.c_str(), str)) ) {		\
    int id;						\
    if (1 == sscanf(pos, str "%d", &id)) {		\
      theSet.insert(id);				\
    }							\
  }
void cpu_info::helper::compute_counts(long* ccount, long* scount) {
  std::ifstream infile("/proc/cpuinfo");
  std::string line;
  std::set<int> cpus, sockets;
  while(std::getline(infile, line)) {    
    const char* pos;
    IF_MATCH("processor	: ", cpus);
    IF_MATCH("physical id	: ", sockets);
  }
  
  *ccount = cpus.size();
  *scount = sockets.size();
  if (not *scount) {
      fprintf(stderr, "No 'physical id' field seen, assuming a single socket\n");
      *scount = 1;
  }
  if(not *ccount) {
    fprintf(stderr, "Unable to read /proc/cpuinfo\n");
    exit(-1);
  }
  std::fprintf(stderr, "cpu_info sees %ld sockets and %ld cores\n",
	       *scount, *ccount);
}

#elif defined(__SVR4)
#include <kstat.h>
void cpu_info::helper::compute_counts(long* ccount, long* scount) {
    long socket_count = 0;
    long cpu_count = 0;
    kstat_ctl_t* kc = kstat_open();
    for (kstat_t* ksp = kc->kc_chain; ksp; ksp=ksp->ks_next) {
	if( ! std::strcmp(ksp->ks_module, "lgrp"))
	    socket_count++;
	if(	! std::strcmp(ksp->ks_module, "cpu")
		&& ! std::strcmp(ksp->ks_name, "sys"))
	    cpu_count++;
    }
    kstat_close(kc);
    std::fprintf(stderr, "cpu_info sees %ld sockets and %ld cores\n",
		 socket_count, cpu_count);
    *ccount = cpu_count;
    *scount = socket_count;
}

#elif defined(__APPLE__)
// See: http://developer.apple.com/library/mac/#documentation/Darwin/Reference/ManPages/man3/sysctlbyname.3.html
// $ sysctl hw.physicalcpu | awk '{print $2}'
// 4
// $ sysctl hw.ncpu | awk '{print $2}'
// 8
#include <sys/types.h>
#include <sys/sysctl.h>

// Define to use physical instead of (hyperthreaded) logical CPUs
#define USE_PHYSICAL_CPUS

void cpu_info::helper::compute_counts(long* ccount, long* scount) 
{
   // By default assume one socket and one cpu
   long socket_count = 1;
   long cpu_count = 1;

   // int sysctlbyname(const char *name, void *oldp, size_t *oldlenp, void *newp, size_t newlen);
   // To set a new value, newp is set to point to a buffer of length newlen from which the requested value is
   //    to be taken.  If a new value is not to be set, newp should be set to NULL and newlen set to 0.
#ifdef USE_PHYSICAL_CPUS
   int nphysical=0;
   *ccount = cpu_count;
   size_t nphysical_size = sizeof(nphysical);
   sysctlbyname("hw.physicalcpu", &nphysical, &nphysical_size, NULL, 0);
   cpu_count = nphysical;
#else
   int ncpus=0;
   size_t ncpus_size = sizeof(ncpus);
   sysctlbyname("hw.ncpu", &ncpus, &ncpus_size, NULL, 0);
   cpu_count = ncpus;
#endif

   *ccount = cpu_count;
   *scount = socket_count;

   std::fprintf(stderr, "cpu_info sees %ld sockets and %ld cores\n",
                socket_count, cpu_count);
}

#else
#error Sorry, no way to identify the number of sockets in this machine
#endif

#if 0
#include <cstdio>
int main() {

    for(int i=0; i < 5; i++) {
	long cpuid = cpu_info::cpuid();
	std::printf("My cpuid pointer is %p\n", &cpu_info::impl_helper::dtrace_cpuid);
	std::printf("I'm running on cpu:%ld and socket:%ld\n", cpuid, cpu_info::socket_of(cpuid));
	sleep(1);
    }
};
#endif
