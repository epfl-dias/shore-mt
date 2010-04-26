#ifndef TPMCS_LOCK_H
#define TPMCS_LOCK_H
/* -*- mode:C++; c-basic-offset:4 -*-
     Shore-MT -- Multi-threaded port of the SHORE storage manager
   
                       Copyright (c) 2007-2009
      Data Intensive Applications and Systems Labaratory (DIAS)
               Ecole Polytechnique Federale de Lausanne
   
                         All Rights Reserved.
   
   Permission to use, copy, modify and distribute this software and
   its documentation is hereby granted, provided that both the
   copyright notice and this permission notice appear in all copies of
   the software, derivative works or modified versions, and any
   portions thereof, and that both notices appear in supporting
   documentation.
   
   This code is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. THE AUTHORS
   DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER
   RESULTING FROM THE USE OF THIS SOFTWARE.
*/

// -*- mode:c++; c-basic-offset:4 -*-

/**\cond skip */
/**\todo TPMCS locks don't build TODO. Perhaps remove it altogether */
/** \brief A time-published MCS lock. 
 * This is a simplified version of the lock
* cited below, and may be used as a drop-in replacement for a normal
*   MCS lock.
*   
*   B He, W Scherer III, M Scott, "Preemption Adaptivity in
*   Time-Published Queue-Based Spinlocks." In Proc. HiPC'05.
*   Abstract at: http://www.springerlink.com/content/t578363585747371/
*   
*   WARNING: This code is debugged but does not quite match the current
*   MCS lock; it should be a usable replacement for mcs_lock if updated
*   with ext_qnode operations, but (as always) Caveat Emptor.
*/
struct tpmcs_lock 
{
    struct ext_qnode; // forward
    enum status { DEAD=0, WAITING, GRANTED };
    typedef ext_qnode volatile* qnode_ptr;
    struct ext_qnode {
        qnode_ptr _next;
        hrtime_t _touched;
        status _status;
        //    ext_qnode() : _next(NULL), _waiting(false) { }
    };
    qnode_ptr volatile _tail;
    hrtime_t volatile _cs_start;
#ifndef ARCH_LP64
    hrtime_t volatile _acquires;
#endif
    hrtime_t _max_cs;
    tpmcs_lock(hrtime_t max_cs=0);

    void spin_on_waiting(qnode_ptr me);
    bool attempt(qnode_ptr me);
    void acquire(qnode_ptr me);
    qnode_ptr spin_on_next(qnode_ptr me);
    void release(qnode_ptr me);
    bool is_mine(qnode_ptr me) { return _tail == me; }
};

/**\endcond skip */

#endif
