## Process this file with automake to produce Makefile.in

include $(top_srcdir)/Makefile.generic

if HAVE_GCC_ATOMIC_BUILTINS
OSUBDIRS =
OSOURCES =
OHEADERS = atomic_ops_builtin.h
else
OSUBDIRS = asm_linkage atomic_ops
OSOURCES = atomic_ops.S
OHEADERS = atomic_ops_impl.h
endif

EXTRA_DIST =  makelinks.sh
SUBDIRS =  $(OSUBDIRS) 
DIST_SUBDIRS =  $(SUBDIRS)

lib_LIBRARIES     = libatomic_ops.a

include_HEADERS = atomic_ops.h $(OHEADERS)

libatomic_ops_a_SOURCES      = $(OSOURCES)

atomic_ops.o:  links

links: 
	chmod +x ./makelinks.sh
	./makelinks.sh
