## Process this file with automake to produce Makefile.in
SUBDIRS = tests
include $(top_srcdir)/Makefile.generic
EXTRA_DIST = fc_error.dat

test_me: $(FC_ERROR_GENFILES_H) test_dynarray test_mem_block
	./test_dynarray
	./test_mem_block

TEST_CXXFLAGS = $(PEDANTIC)  $(TUNE_FLAGS) -DTEST_ME
TEST_CXXFLAGS += -g $(GENERIC_YESDEBUG_FLAGS) $(W_WARNINGS) $(PURE_CXXFLAGS)
TEST_CXXFLAGS += $(ALWAYS_FLAGS) $(TEMPLATEFLAGS) 

bin_PROGRAMS = test_dynarray test_mem_block
test_dynarray_SOURCES = dynarray.cpp
test_dynarray_CXXFLAGS = $(TEST_CXXFLAGS)

test_mem_block_SOURCES = mem_block.cpp
test_mem_block_CXXFLAGS = $(TEST_CXXFLAGS)

lib_LIBRARIES     = libfc.a

FC_ERROR_GENFILES_H  = fc_errmsg_gen.h \
             fc_einfo_gen.h \
             fc_error_def_gen.h \
             fc_error_enum_gen.h \
             fc_einfo_bakw_gen.h

GENFILES = $(FC_ERROR_GENFILES_H)

include_HEADERS = \
    atomic_class_pool.h \
    atomic_container.h \
    atomic_templates.h \
    block_alloc.h \
    errlog.h errlog_s.h \
    gethrtime.h \
    largefile_aware.h \
    nbox.h \
    os_types.h \
    dynarray.h \
    mem_block.h \
    rand48.h \
    stime.h \
    tls.h \
    vtable.h \
    valgrind_help.h \
    w.h w_autodel.h w_base.h w_bitmap.h w_bitvector.h \
    w_compat_strstream.h \
    w_hashing.h \
    w_debug.h w_defines.h \
    w_error.h \
    w_findprime.h \
    w_form.h \
    w_getopt.h w_gettimeofday.h \
    w_hash.h w_heap.h w_list.h w_minmax.h \
    w_rc.h w_rusage.h w_shore_alloc.h \
    w_stream.h w_strstream.h \
	w_stat.h \
    w_workaround.h \
    $(GENFILES)

libfc_a_SOURCES      = \
        rand48.cpp \
	dynarray.cpp \
	mem_block.cpp \
	block_alloc.cpp \
        gethrtime.cpp \
        w_compat_strstream.cpp \
        w_form.cpp w_form_base.cpp \
        common_tmpl.cpp w_debug.cpp errlog.cpp w_getopt.cpp nbox.cpp \
        stime.cpp vtable.cpp w_base.cpp w_bitmap.cpp w_error.cpp \
        valgrind_help.cpp \
        w_listm.cpp w_rc.cpp \
        w_input.cpp tls.cpp w_findprime.cpp

MOSTLYCLEANFILES = $(GENFILES)

debug.$(OBJEXT): $(FC_ERROR_GENFILES_H)
$(FC_ERROR_GENFILES_H): fc_error.dat
	$(PERL) $(top_srcdir)/tools/errors.pl -d -e $?

