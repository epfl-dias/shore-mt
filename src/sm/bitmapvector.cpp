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

/*<std-header orig-src='shore'>

 $Id: bitmapvector.cpp,v 1.6.2.3 2009/10/30 23:52:17 nhall Exp $

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

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#define SM_SOURCE
#define BITMAPVECTOR_C

#include <sm_int_1.h>
#include <bitmapvector.h>

/***************************************************************************
 *                                                                         *
 * BitMapVector class                                                      *
 *                                                                         *
 ***************************************************************************/

BitMapVector::BitMapVector(uint4_t numberOfBits)
:   size(0),
    vector(0)
{
    size = (numberOfBits + BitsPerWord - 1) / BitsPerWord;
    vector = new uint4_t[size];
    ClearAll();

    w_assert3(numberOfBits <= size * BitsPerWord);
    w_assert3(numberOfBits > (size - 1) * BitsPerWord);
}


BitMapVector::BitMapVector(const BitMapVector& v)
: size(0),
  vector(0)
{
	*this = v;
}


void BitMapVector::Resize(uint4_t newSize)
{
    w_assert3(newSize > size);

    uint4_t*	newVector = new uint4_t[newSize];
    if (!newVector)
    	W_FATAL(fcOUTOFMEMORY);

    uint4_t i=0;
    for (i = 0; i < size; i++)
	newVector[i] = vector[i];
    
    for (i = size; i < newSize; i++)
	newVector[i] = 0;
    
    delete [] vector;
    vector = newVector;
    size = newSize;
}


BitMapVector& BitMapVector::operator=(const BitMapVector& v)
{
    if (this != &v)  {
	if (v.size > size)
	    Resize(v.size);

	uint4_t i=0;
	for (i = 0; i < v.size; i++)
	    vector[i] = v.vector[i];
	
	for (i = v.size; i < size; i++)
	    vector[i] = 0;
    }
    return *this;
}


bool BitMapVector::operator==(const BitMapVector& v)
{
	for (uint4_t i = 0; i < size; i++)
		if (vector[i] != v.vector[i])
			return false;
	
	return true;
}


BitMapVector::~BitMapVector()
{
    delete [] vector;
    vector = 0;
    size = 0;
}


void BitMapVector::ClearAll()
{
    for (uint4_t i = 0; i < size; i++)
	vector[i] = 0;
}


int4_t BitMapVector::FirstSetOnOrAfter(uint4_t index) const
{
    uint4_t	wordIndex = index / BitsPerWord;
    uint4_t	mask = (1 << (index % BitsPerWord));

    while (wordIndex < size)  {
	if (vector[wordIndex] & mask)
	    return index;
	
	index++;
	mask <<= 1;
	if (mask == 0)  {
	    wordIndex++;
	    mask = 1;
	}
    }

    return -1;
}


int4_t BitMapVector::FirstClearOnOrAfter(uint4_t index) const
{
    uint4_t	wordIndex = index / BitsPerWord;
    uint4_t	mask = (1 << (index % BitsPerWord));

    while (wordIndex < size)  {
	if (!(vector[wordIndex] & mask))
	    return index;
	
	index++;
	mask <<= 1;
	if (mask == 0)  {
	    wordIndex++;
	    mask = 1;
	}
    }

    return index;
}


void BitMapVector::SetBit(uint4_t index)
{
    if (index / BitsPerWord >= size)  {
	Resize((index + BitsPerWord) / BitsPerWord);
	w_assert3(index / BitsPerWord < size);
    }

    vector[index / BitsPerWord] |= (1 << (index % BitsPerWord));
}


void BitMapVector::ClearBit(uint4_t index)
{
    if (index / BitsPerWord < size)  {
	vector[index / BitsPerWord] &= ~(1 << (index % BitsPerWord));
    }
}


bool BitMapVector::IsBitSet(uint4_t index) const
{
    if (index / BitsPerWord >= size)
	return false;
    else
	return ((vector[index / BitsPerWord] & (1 << (index % BitsPerWord))) != 0);
}


void BitMapVector::OrInBitVector(const BitMapVector& v, bool& changed)
{
    if (size < v.size)
	Resize(v.size);
    
    for (uint4_t i = 0; i < v.size; i++)  {
	changed |= ((~vector[i] & v.vector[i]) != 0);
	vector[i] |= v.vector[i];
    }
}

