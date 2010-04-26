## Process this file with automake to produce Makefile.in
include $(top_srcdir)/Makefile.generic
RESULT_COMPARISON_FILES = file_scan-i.out htab-n10000-R.out htab-n500.out htab-n10000-R.out
EXTRA_DIST = testall EXAMPLE_SHORECONFIG $(RESULT_COMPARISON_FILES)

## no-op rule for making the .out files
$(RESULT_COMPARISON_FILES) :

CLEANFILES = ./log/* ./volumes/*

AM_CXXFLAGS          +=  \
		 -I$(top_srcdir)/src/fc \
		 -I$(top_srcdir)/src/sthread \
		 -I$(top_srcdir)/src/common  \
		 -I$(top_srcdir)/src/sm 

check_PROGRAMS     = file_scan$(EXEEXT) \
		    startstop$(EXEEXT) \
		    create_rec$(EXEEXT) \
		    file_scan_many$(EXEEXT) \
		    htab$(EXEEXT) 

TESTS = testall

startstop_SOURCES      = startstop.cpp 
file_scan_SOURCES      = file_scan.cpp options.cpp 
file_scan_many_SOURCES      = file_scan_many.cpp options.cpp 
create_rec_SOURCES      = create_rec.cpp options.cpp 

htab_SOURCES      = htab.cpp

LDADD      = \
	$(top_builddir)/src/sm/libsm.a  \
	$(top_builddir)/src/common/libcommon.a  \
	$(top_builddir)/src/sthread/libsthread.a  \
	$(top_builddir)/src/fc/libfc.a  \
	$(LOCALLDADD)

file_scan.pure:  
	$(PURIFY_BIN)/purify $(PURIFY_FLAGS)  -collector=$(PURIFY_COLLECTOR)  $(CXX) -o $@ $(file_scan_SOURCES:.cpp=.o) $(LDADD) $(LIBS)


