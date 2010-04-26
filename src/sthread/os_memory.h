#ifdef HAVE_MEMORY_H
#include <memory.h>
#else
#ifdef HAVE_STRING_H
#include <string.h>
#else
#error Need definitions for memset and companions.
#endif
#endif
