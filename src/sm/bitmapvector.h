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

/*<std-header orig-src='shore' incl-file-exclusion='BITMAPVECTOR_H'>

 $Id: bitmapvector.h,v 1.4.2.3 2009/10/30 23:52:17 nhall Exp $

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

#ifndef BITMAPVECTOR_H
#define BITMAPVECTOR_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif

class BitMapVector  {
    public:
	NORET			    BitMapVector(uint4_t numberOfBits = 32);
	NORET			    BitMapVector(const BitMapVector& v);
	BitMapVector&		    operator=(const BitMapVector& v);
	bool			    operator==(const BitMapVector& v);
	bool			    operator!=(const BitMapVector& v)
		{
		    return !(*this == v);
		};
	NORET			    ~BitMapVector();

	void			    ClearAll();
	int4_t			    FirstSetOnOrAfter(uint4_t index) const;
	int4_t			    FirstClearOnOrAfter(uint4_t index = 0) const;
	void			    SetBit(uint4_t bitNumber);
	void			    ClearBit(uint4_t bitNumber);
	bool			    IsBitSet(uint4_t bitNumber) const;
	void			    OrInBitVector(const BitMapVector& v, bool& changed);

    private:
	uint4_t			    size;
	uint4_t*			    vector;
	enum			    { BitsPerWord = sizeof(uint4_t) * CHAR_BIT };

	void			    Resize(uint4_t numberOfBits);
};

/*<std-footer incl-file-exclusion='BITMAPVECTOR_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
