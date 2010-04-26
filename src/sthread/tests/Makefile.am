## Process this file with automake to produce Makefile.in
include $(top_srcdir)/Makefile.generic
RESULT_COMPARISON_FILES = 
EXTRA_DIST = testall $(RESULT_COMPARISON_FILES)
AM_CXXFLAGS    += -I$(top_srcdir)/src/sthread \
		-I$(top_srcdir)/src/fc

check_PROGRAMS     = thread1$(EXEEXT) thread2$(EXEEXT)\
		     thread3$(EXEEXT) thread4$(EXEEXT) \
		     ioperf$(EXEEXT) mmap$(EXEEXT) \
		     except$(EXEEXT) atomics$(EXEEXT) pthread_test$(EXEEXT)

TESTS = testall

LDADD = ../libsthread.a ../../fc/libfc.a $(LOCALLDADD)

pthread_test_SOURCES  = pthread_test.cpp

thread1_SOURCES      = thread1.cpp

thread2_SOURCES      = thread2.cpp

thread3_SOURCES      = thread3.cpp

thread4_SOURCES      = thread4.cpp

ioperf_SOURCES      = ioperf.cpp

except_SOURCES      = except.cpp

mmap_SOURCES      = mmap.cpp

atomics_SOURCES      = atomics.cpp

