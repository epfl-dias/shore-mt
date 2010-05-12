#ifndef __TEST_ME_H
#define __TEST_ME_H

#include <cstdio>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <cstdlib>

#define EXPECT_ASSERT(x) do {				\
	std::fprintf(stderr, "Expecting an assertion...\n");	\
	pid_t pid = fork();				\
	if(pid == 0) {					\
	    x;						\
	    std::exit(0);				\
	}						\
	else {						\
	    assert(pid > 0);				\
	    int status;					\
	    int err = wait(&status);			\
	    assert(err != ECHILD);			\
	    assert(err != EINTR);			\
	    assert(err != EINVAL);			\
	    if(!WIFSIGNALED(status)) {			\
		std::fprintf(stderr, "%s:%d: `%s' did not assert as expected\n", \
			     __FILE__, __LINE__, #x);			\
		std::abort();						\
	    }						\
	}						\
    } while(0)

#endif
