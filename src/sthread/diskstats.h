/*<std-header orig-src='shore' incl-file-exclusion='DISKSTATS_H'>

 $Id: diskstats.h,v 1.14.2.2 2009/06/23 01:01:47 nhall Exp $

SHORE -- Scalable Heterogeneous Object REpository

Copyright (c) 1994-99 Computer Sciences Department, University of
                      Wisconsin -- Madison
All Rights Reserved.

Permission to use, copy, modify and distribute this software and its
documentation is hereby granted, provided that both the copyright
notice and this permission notice appear in all copies of the
software, derivative works or modified versions, and any portions
thereof, and that both notices appear in supporting documentation.

THE AUTHORS AND THE COMPUTER SCIENCES DEPARTMENT OF THE UNIVERSITY
OF WISCONSIN - MADISON ALLOW FREE USE OF THIS SOFTWARE IN ITS
"AS IS" CONDITION, AND THEY DISCLAIM ANY LIABILITY OF ANY KIND
FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.

This software was developed with support by the Advanced Research
Project Agency, ARPA order number 018 (formerly 8230), monitored by
the U.S. Army Research Laboratory under contract DAAB07-91-C-Q518.
Further funding for this work was provided by DARPA through
Rome Research Laboratory Contract No. F30602-97-2-0247.

*/

#ifndef DISKSTATS_H
#define DISKSTATS_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#include <unix_stats.h>
#include <cstring>


struct S {
	int writes;
	int bwritten;
	int reads;
	int skipreads;
	int bread;
	int fsyncs;
	int ftruncs;
	int discards;

	S(): writes(0), bwritten(0),
		reads(0), bread(0),
		fsyncs(0), discards(0) {}
};

class U : public unix_stats {
public:
	// j is the iteratons
	void copy(int j, const U &u) {
		*this = u;
		if(j < 1) {
			iterations = 0;
		} else {
			iterations = j;
		}
	}
};

struct S s;
class  U u;

/*<std-footer incl-file-exclusion='DISKSTATS_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
