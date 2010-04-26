/*<std-header orig-src='shore' incl-file-exclusion='UNIX_STATS_H'>

 $Id: unix_stats.h,v 1.23.2.3 2009/09/14 00:08:44 nhall Exp $

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

#ifndef UNIX_STATS_H
#define UNIX_STATS_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#include "w_workaround.h"

/*
 * stats.h
 *
 * Class definition for the stats class.
 * Member functions are defined in stats.c
 */
#ifndef  _stats
#define  _stats

#ifdef __GNUG__
#pragma interface
#endif

#include <iosfwd>
#include <w_rusage.h>


/*
 * unix_stats objects are meant to be used as follows:
 *
 *        s.start();
 *        section of code for which stats are being gathered
 *        s.Stop();
 *        examine gathered unix_stats
 */

#if defined(SOLARIS2)  && defined(USE_SLASH_PROC)
#undef __STATS_H__
#undef  _stats
#include <solaris_stats.h>
#else /* not SOLARIS2 or not defined USE_SLASH_PROC */
#ifdef __GNUC__
#pragma interface
#endif


#include <w_gettimeofday.h>

/*
 * Stats for Unix processes.
 */
class unix_stats {
protected:
    struct timeval  time1;		/* gettimeofday() buffer */
    struct timeval  time2;		/* gettimeofday() buffer */
    struct rusage   rusage1;	/* getrusage() buffer */
    struct rusage   rusage2;	/* getrusage() buffer */
    int	iterations;
#ifdef Linux
#define WHO enum __rusage_who
#else
#define WHO int
#endif
    WHO who;

public:
    unix_stats();
    unix_stats(int who); // if other than self

    float compute_time() const;

    void   start();			/* start gathering stats  */
	// you can start and then stop multiple times-- don't need to continue
    void   stop(int iter=1);			/* stop gathering stats  */
    int    clocktime() const;		/* elapsed real time in micro-seconds */
    int    usertime() const;		/* elapsed user time in micro-seconds */
    int    systime() const;	/* elapsed system time in micro-seconds */
    /* variants */
    int    s_clocktime() const;		/* diff of seconds only */
    int    s_usertime() const;		
    int    s_systime() const;	
    int    us_clocktime() const;	/* diff of microseconds only */
    int    us_usertime() const;		
    int    us_systime() const;	

    int    page_reclaims() const;	/* page reclaims */
    int    page_faults() const;	/* page faults */
    int    swaps() const;			/* swaps */
    int    inblock() const;		/* page-ins */
    int    oublock() const;		/* page-outs */
    int    rss() const;			/* resident-set size */
    int    vcsw() const;			/* voluntary context swtch */
    int    invcsw() const;			/* involuntary context swtch */
    int    msgsent() const;			/* socket messages sent */
    int    msgrecv() const;			/* socket messages recvd */
    int    signals() const;			/* signals dispatched */

    ostream &print(ostream &) const;
};
extern ostream& operator<<(ostream&, const unix_stats &s);
#endif



float compute_time(const struct timeval *start_time, const struct timeval *end_time);

#endif /*_stats*/

/*<std-footer incl-file-exclusion='UNIX_STATS_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
