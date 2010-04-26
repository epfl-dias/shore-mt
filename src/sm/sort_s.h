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

/*<std-header orig-src='shore' incl-file-exclusion='SORT_S_H'>

 $Id: sort_s.h,v 1.30.2.8 2010/03/19 22:20:28 nhall Exp $

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

#ifndef SORT_S_H
#define SORT_S_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif

#define OLDSORT_COMPATIBILITY
#ifdef OLDSORT_COMPATIBILITY

//
// info on keys
//
struct key_info_t {
    typedef sortorder::keytype key_type_t;

    // for backward compatibility only: should use keytype henceforth
    enum dummy_t {  t_char=sortorder::kt_u1,
                    t_int=sortorder::kt_i4,
                    t_float=sortorder::kt_f4,
                    t_string=sortorder::kt_b, 
                    t_spatial=sortorder::kt_spatial};

    enum where_t { t_hdr = 0, t_body };

    key_type_t  type;       // key type
    nbox_t      universe;   // for spatial object only
    bool        derived;    // if true, the key must be the only item in rec
                            // header, and the header will not be copied to
                            // the result record (allow user to store derived
                            // key temporarily for sorting purpose).


    // following applies to file sort only
    where_t              where;      // where the key resides
    w_base_t::uint4_t    offset;     // offset fron the begin
    w_base_t::uint4_t    len;        // key length
    w_base_t::uint4_t    est_reclen; // estimated record length
    
    key_info_t() {
      type = sortorder::kt_i4;
      where = t_body;
      derived = false;
      offset = 0;
      len = sizeof(int);
      est_reclen = 0;
    }
};

//
// sort parameter
//
struct sort_parm_t {
    uint2_t run_size;        // size for each run (# of pages)
    vid_t   vol;        // volume for files
    bool   unique;        // result unique ?
    bool   ascending;        // ascending order ?
    bool   destructive;    // destroy the input file at the end ?
    bool   keep_lid;        // preserve logical oid for recs in sorted
                // file -- only for destructive sort
    smlevel_3::sm_store_property_t property; // temporary file ?

    sort_parm_t() : run_size(10), unique(false), ascending(true),
            destructive(false), keep_lid(false),
            property(smlevel_3::t_regular) {}
};

#endif /* OLDSORT_COMPATIBILITY */

/*
 * For new sort:
 */

// typedef void * key_cookie_t;
// This type was originally a void * as a way to make it
// a typeless thing; but that's most uncool now .
// When it's cast to an smsize_t, it's 'known' that the
// values are never really pointers, but are small integers.
// So here we assert that these are small ints when we make
// the type conversion.
// TODO: NANCY should come up with a better way to fix this.
struct key_cookie_t {
    void *   c;
    explicit key_cookie_t () : c(NULL) { }
    explicit key_cookie_t (int i) {
        union { int _i; void *v; } _pun = {i};
        c = _pun.v;
    }
    explicit key_cookie_t (void *v):c(v) { }

    int make_int() const { return operator int(); }
    int make_smsize_t() const { return operator smsize_t(); }

    static key_cookie_t   null; // newsort.cpp

private:

    operator int () const { 
         union { void *v; int _i; } _pun = {c};
             return _pun._i;
    }
    operator smsize_t () const { 
         union { void *v; int _i; } _pun = {c};
         smsize_t t =  _pun._i & 0xffffffff;
#ifdef ARCH_LP64
         w_assert1((_pun._i & 0xffffffff00000000) == 0);
#endif
         return t;
    }
};


class factory_t 
{
   /* users are meant to write their own factories 
    * that inherit from this
    */
public:
   factory_t();
   virtual    NORET ~factory_t();

   virtual    void* allocfunc(smsize_t)=0;
   virtual    void freefunc(const void *, smsize_t)=0;

   // none: causes no delete - used for statically allocated space
   static factory_t*    none;

   // cpp_vector - simply calls delete[] 
   static factory_t*    cpp_vector;

   void freefunc(vec_t&v) {
    for(int i=v.count()-1; i>=0; i--) {
        DBG(<<"freefuncVEC(ptr=" << (void*)v.ptr(i) << " len=" << v.len(i));
        freefunc((void *)v.ptr(i), v.len(i));
    }
   }
};

/* XXX move virtual functions to a .cpp */
inline factory_t::factory_t() {}
inline NORET factory_t::~factory_t() {}

class key_location_t 
{
public:
    bool         _in_hdr;
    smsize_t         _off;
    smsize_t         _length;
    key_location_t() : _in_hdr(false), _off(0), _length(0)  {}
    key_location_t(const key_location_t &old) : 
    _in_hdr(old._in_hdr), 
    _off(old._off), _length(old._length) {}
    bool is_in_hdr() const { return _in_hdr; }
};

class skey_t;
class file_p;
class record_t;
class object_t : public smlevel_top 
{
    friend class skey_t;
    static const object_t& none;
protected:
    void        _construct(file_p&, slotid_t);
    void        _construct(const void *hdr, smsize_t hdrlen, factory_t *,
              const void *body, smsize_t bodylen, factory_t *);

    void      _replace(const object_t&); // copy over
    NORET       object_t();
public:
    NORET     object_t(const object_t&o) 
               :
               _valid(false),
               _in_bp(false),
               _rec(0),
               _fp(0),
               _hdrfact(factory_t::none),
               _hdrlen(0),
               _hdrbuf(0),
               _bodyfact(factory_t::none),
               _bodylen(0),
               _bodybuf(0)
            { _replace(o); }

    NORET     object_t(const void *hdr, smsize_t hdrlen, factory_t& hf,
               const void *body, smsize_t bodylen, factory_t& bf) 
               :
               _valid(false),
               _in_bp(false),
               _rec(0),
               _fp(0),
               _hdrfact(&hf),
               _hdrlen(hdrlen),
               _hdrbuf(hdr),
               _bodyfact(&bf),
               _bodylen(bodylen),
               _bodybuf(body)
               { }

    NORET     ~object_t();

    bool        is_valid() const  { return _valid; }
    bool        is_in_buffer_pool() const { return is_valid() && _in_bp; }
    smsize_t  hdr_size() const { return _hdrlen; }
    smsize_t  body_size() const { return _bodylen; }

    const void *hdr(smsize_t offset) const; 
    const void *body(smsize_t offset) const;
    void        freespace();
    void        assert_nobuffers() const;
    smsize_t  contig_body_size() const; // pinned amt

    w_rc_t    copy_out(bool in_hdr, smsize_t offset, 
                smsize_t length, vec_t&dest) const;


private:      // data
    bool        _valid;
    bool        _in_bp; // in buffer pool or in memory

    // for in_buffer_pool:
    record_t*    _rec;
    file_p*        _fp;

    // for in_memory:
    factory_t*    _hdrfact;
    smsize_t    _hdrlen;
    const void*    _hdrbuf;

    factory_t*    _bodyfact;
    smsize_t    _bodylen;
    const void*    _bodybuf;

protected:
    int     _save_pin_count;
        // for callback
    void      _callback_prologue() const {
#        if W_DEBUG_LEVEL > 2
            /*
             * leaving SM
             */
            // escape const-ness of the method
            int *save_pin_count = (int *)&_save_pin_count;
            *save_pin_count = me()->pin_count();
            w_assert3(_save_pin_count == 0);
            me()->check_pin_count(0);
            me()->in_sm(false);
#        endif 
        }
    void      _callback_epilogue() const {
#        if W_DEBUG_LEVEL > 2
            /*
             * re-entering SM
             */
            me()->check_actual_pin_count(_save_pin_count);
            me()->in_sm(true);
#        endif 
        }
    void    _invalidate() {
               _valid=false;
               _in_bp=false;
               _rec = 0;
               _fp=0;
               _hdrfact=factory_t::none;
               _hdrlen=0;
               _hdrbuf=0;
               _bodyfact=factory_t::none;
               _bodylen=0;
               _bodybuf=0;
               _save_pin_count=0;
            }
};

class run_mgr;
class skey_t 
{
    // To let run_mgr construct empty skey_t
        friend class run_mgr;
public:
    NORET     skey_t(const object_t& o, smsize_t offset,
            smsize_t len, bool in_hdr) 
            : _valid(true), _in_obj(true),
            _length(len),
            _obj(&o), _offset(offset),
            _in_hdr(in_hdr), 
            _fact(factory_t::none), _buf(0)
            {
            }
    NORET     skey_t(void *buf, smsize_t offset,
            smsize_t len,  // KEY len, NOT NECESSARILY BUF len
            factory_t& f) 
            : _valid(true), _in_obj(false),
            _length(len),
            _obj(&object_t::none),
            _offset(offset),
            _in_hdr(false), 
            _fact(&f), _buf(buf)
            {
            }
    NORET   ~skey_t() {}

    smsize_t  size() const { return _length; }
    bool        is_valid() const  { return _valid; }
    bool      is_in_obj() const { return is_valid() && _in_obj; }
    bool      consistent_with_object(const object_t&o ) const { 
                     return ((_obj == &o) || !_in_obj); }
    bool      is_in_hdr() const { return is_in_obj() && _in_hdr; }

    const void *ptr(smsize_t offset) const;  // key
    void        freespace();
    void        assert_nobuffers()const;
    smsize_t  contig_length() const; // pinned amt or key length
    w_rc_t    copy_out(vec_t&dest) const;


private:
    bool        _valid; 
    bool        _in_obj; // else in mem
    smsize_t    _length;

protected:
    // for in_obj case;
    const object_t*    _obj;     
    smsize_t    _offset; // into buf or object of start of key
private:
    bool        _in_hdr;

    // for !in_obj but valid
    factory_t*    _fact;
    const void*    _buf;   // buf to be deallocated -- key
                // might not start at offset 0

protected:
    void        _invalidate();
    NORET     skey_t() : 
        _valid(false), _in_obj(false), _length(0),
        _obj(&object_t::none),
        _offset(0), _in_hdr(false),
        _fact(factory_t::none), _buf(0)
        { }
    void    _construct(const object_t *o, smsize_t off, smsize_t l, bool h) {
        _valid = true;
        _in_obj = true;
        _obj = o;
        _offset = off;
        _length = l;
        _in_hdr = h;
        _fact = factory_t::none;
        }
    void      _construct(const void *buf, smsize_t off, smsize_t l, factory_t* f) {
        _valid = true;
        _obj = 0;
        _in_obj = false;
        _offset = off;
        _length = l;
        _fact = f;
        _buf = buf;
        }
    void    _replace(const skey_t&); // copy over
    void    _replace_relative_to_obj(const object_t &, const skey_t&); // copy over
};


/*
 * Create key func: 
 *
 * This function is meant to fill in a skey_t for
 *  this key in this object. 
 *
 *  Called only when the object is first encountered in its run.
 *
 *  The buffer_space_t "preallocated" points to
 *  a pre-allocated buffer that the function may use if it so chooses,
 *  i.e., if the given buffer is of sufficient length.  If the 
 *  function does not use this space, but allocates space of its
 *  own, this function must set used_preallocated=false, and
 *  must supply a factory_t* for freeing the space.
 *  It is strongly recommended to 
 *  use the preallocated space if at all possible.
 *  If the preallocated space IS used, the result skey_t._mem_buf_info should 
 *  be a copy of the preallocated, with the length
 *  adjusted to reflect the amount of the available buffer space used.
 * 
 * If the key is to be located directly in the buffer pool, the
 * function shouldn't set _mem_buf_info, but should instead set
 * _buf_pool_info, in which case all that's needed is the
 * offset and length of the key. 
 */  
 

    typedef w_rc_t (*CSKF)(
        const rid_t&        rid,
        const object_t&     in_obj,
        key_cookie_t        cookie,  // type info
        factory_t&          internal,
        skey_t*             out
    );

/*
 * Marshal Object Function (MOF) 
 *  MOF is called 
 *  1) when object is first encountered, whether or not carry_obj()
 *  2) if carry_obj() it's also called when the object is read back
 *  from temp file.
 */

/*
 * Un-marhal Object Function (UMOF): 
 *  UMOF called when object is written to temp file
 *  and when final object is written to the result file (if is_for_file()).
 */

typedef w_rc_t (*MOF)(
    const rid_t&       rid,  // record id
    const object_t&    obj_in,
    key_cookie_t       cookie,  // type info
                // func must allocate obj_out,
    object_t*         obj_out     // SM allocates, MOF initializes, SM
                // frees its buffers
    );

typedef w_rc_t (*UMOF)(
    const rid_t&       rid,  // orig record id of object in buffer
    const object_t&    obj_in,
    key_cookie_t       cookie,  // type info
                // func must allocate obj_out,
    object_t*        obj_out // SM allocates, UMOF initializes,
            // SM will copy to disk, free mem
    );
/* 
 * Key comparison func:
 * To be used on every key comparison.  The comparison is 
 * necessarily based solely on the keys themselves; no 
 * metadata are passed in.  This will probably change.
 */

typedef int (*CF) (w_base_t::uint4_t , 
            const void *, 
            w_base_t::uint4_t , 
            const void *);

/* 
 * LEXFUNC: create Index Key function
 *
 * There are 3 main cases here:
 * 1) index key is the sort key, unchanged
 *    Internally, we don't bother to call back or copy the
 *    index key separately.
 * 2) index key is NOT the sort key; perhaps the sort key is derived,
 *    while index key is taken/created from the object, perhaps
 *    with a mem2disk applied.  
 * 3) Index key is derived from the sort key (e.g, mem2disk applied
 *    to the sort key).
 */
typedef w_rc_t (*LEXFUNC) (const void *, smsize_t , void *);
/*
 * An example of a cookie passed to CSKF funcs for cases
 * 2,3 of the index keys 
 */
struct generic_CSKF_cookie 
{
    LEXFUNC    func; // value == noLEXFUNC means don't lexify
    bool       in_hdr;
    smsize_t   offset;
    smsize_t   length;
};

class sort_keys_t 
{
    /* new sort_file() API.
     * Specifies locations of keys and other info about them,
     * as well as some characteristics of the sort, i.e., ascending,
     * etc.
     * Key 0 gets compared first, 1 next, etc.
     *
     * He who creates this data structure determines
     * what is "most significant" key, etc.
     */
public:
 
    static w_rc_t noLEXFUNC (const void *, smsize_t , void *);

    static w_rc_t noCSKF(
                const rid_t&       rid,
                const object_t&    obj,
                key_cookie_t    cookie,  // type info
                factory_t&    f,
                skey_t*        out
    );

    static w_rc_t generic_CSKF(
                const rid_t&    rid,
                const object_t&    in_obj,
                key_cookie_t    cookie,  // type info
                factory_t&    internal,
                skey_t*        out
    );

    static w_rc_t noMOF (
                const rid_t&     rid,  // record id
                const object_t&    obj,
                key_cookie_t    cookie,  // type info
                object_t*    out
    );

    static w_rc_t noUMOF (
                const rid_t&     rid,  // record id
                const object_t&    obj,
                key_cookie_t    cookie,  // type info
                object_t*    out
    );

    /*
     * Default comparision functions for in-buffer-pool
     * comparisons.  No byte-swapping is done; alignment
     * requirements must already be met before calling these.
     */
    static int string_cmp(w_base_t::uint4_t , const void* , w_base_t::uint4_t , const void*);
    static int uint8_cmp(w_base_t::uint4_t , const void* , w_base_t::uint4_t , const void* );
    static int int8_cmp(w_base_t::uint4_t , const void* , w_base_t::uint4_t , const void* );
    static int uint4_cmp(w_base_t::uint4_t , const void* , w_base_t::uint4_t , const void* );
    static int int4_cmp(w_base_t::uint4_t , const void* , w_base_t::uint4_t , const void* );
    static int uint2_cmp(w_base_t::uint4_t , const void* , w_base_t::uint4_t , const void* );
    static int int2_cmp(w_base_t::uint4_t , const void* , w_base_t::uint4_t , const void* );
    static int uint1_cmp(w_base_t::uint4_t , const void* , w_base_t::uint4_t , const void* );
    static int int1_cmp(w_base_t::uint4_t , const void* , w_base_t::uint4_t , const void* );
    static int f4_cmp(w_base_t::uint4_t , const void* , w_base_t::uint4_t , const void* );
    static int f8_cmp(w_base_t::uint4_t , const void* , w_base_t::uint4_t , const void* );

public:
    static w_rc_t f8_lex(const void *, smsize_t , void *);
    static w_rc_t f4_lex(const void *, smsize_t , void *);
    static w_rc_t u8_lex(const void *, smsize_t , void *);
    static w_rc_t i8_lex(const void *, smsize_t , void *);
    static w_rc_t u4_lex(const void *, smsize_t , void *);
    static w_rc_t i4_lex(const void *, smsize_t , void *);
    static w_rc_t u2_lex(const void *, smsize_t , void *);
    static w_rc_t i2_lex(const void *, smsize_t , void *);
    static w_rc_t u1_lex(const void *, smsize_t , void *);
    static w_rc_t i1_lex(const void *, smsize_t , void *);


private:

     /* Metadata about a key -- info about the key
     *     as it appears in all objects, not per-object
     *      key info.
     */
    class key_meta_t {
    public:
    /* 
     * callbacks:
     */ 
    CF              _cb_keycmp;  // comparison
    CSKF            _cb_keyinfo; // get location/copy/derived
    key_cookie_t    _cookie;
    w_base_t::int1_t    _mask;
    fill1        _dummy1;
    fill2        _dummy2;
    key_meta_t() : _cb_keycmp(0), _cb_keyinfo(0), 
        _cookie(0),
        _mask(t_none) {}
    key_meta_t(const key_meta_t &old) : 
        _cb_keycmp(old._cb_keycmp), 
        _cb_keyinfo(old._cb_keyinfo),
        _cookie(old._cookie),
        _mask(old._mask) {}
    };
    int        _nkeys;        // constructor 
    int        _spaces;    // _grow
    key_meta_t*     _meta;
    key_location_t* _locs;
    bool    _stable;    // set_stable, is_stable
    bool    _for_index;    // set_for_index, is_for_index
    bool    _remove_dups;    // set_unique
    bool    _remove_dup_nulls; // set_null_unique
    bool    _ascending;    // ascending, set_ascending
    bool    _deep_copy;    // set_for_index, set_for_file
    bool    _keep_orig_file;// set_for_index, set_for_file, set_keep_orig
    bool    _carry_obj;    // set_for_file, carry_obj, set_carry_obj

    CSKF    _cb_lexify;    // set_for_index, lexify
    key_cookie_t _cb_lexify_cookie;    // set_for_index

    MOF        _cb_marshal;    // set_object_marshal
    UMOF    _cb_unmarshal;    // set_object_marshal
    key_cookie_t _cb_marshal_cookie;    // set_object_marshal

    void _grow(int i);

    void _prep(int key);
    void _set_loc(int key, smsize_t off, smsize_t len) {
        key_location_t &t = _locs[key];
        t._off = off;
        t._length = len;
    }
    void _set_mask(int key,
        bool fixed, 
        bool aligned, 
        bool lexico, 
        bool in_header,
        CSKF gfunc,
        CF   cfunc,
        key_cookie_t cookie
    ) {
    key_meta_t &t = _meta[key];
    if(aligned) t._mask |= t_aligned;
    else t._mask &= ~t_aligned;
    if(fixed) t._mask |= t_fixed;
    else t._mask &= ~t_fixed;
    if(lexico) t._mask |= t_lexico;
    else t._mask &= ~t_lexico;
    if(in_header) t._mask |= sort_keys_t::t_hdr;
    else t._mask &= ~sort_keys_t::t_hdr;
    t._cb_keycmp = cfunc;
    t._cb_keyinfo = gfunc;
    w_assert3(cfunc);
    if( ! fixed) {
        w_assert3(gfunc);
    }
    t._cookie = cookie;
    }

    void _copy(const sort_keys_t &old);

public:
    /*
     * mask:
     * t_fixed: key location is fixed for all records: no need
     *    to call CSKF for each record 
     * t_aligned: key location adequately aligned (relative
     *    to the beginning of the record, which is 4-byte aligned) for
     *    in-buffer-pool comparisons with the given comparison
     *    function.  Copy to a contiguous buffer is unnecessary iff
     *    the entire key happens to be on one page (which is always
     *    the case with small records). 
     * t_lexico: key can be spread across pages and  the comparison 
     *    function can be called on successive 
     *    segments of the key -- copy to a contiguous buffer unnecessary.
     *    Implies key is adequately aligned, but does not imply t_fixed.
     *
     *  t_hdr: key is at offset in hdr rather than in body 
     */

    enum { t_none = 0x0, 
        t_fixed = 0x1, 
        t_aligned =0x2, 
        t_lexico=0x4,
        t_hdr = 0x40  // key found in hdr
        };

    NORET sort_keys_t(int nkeys);
    NORET ~sort_keys_t() {
        delete[] _meta;
        delete[] _locs;
    }
    NORET sort_keys_t(const sort_keys_t &old);

    int     nkeys() const { return _nkeys; }

    /* Call set_for_index() if you want the output file
     * to be written with objects of the form hdr==key, body==rid
     * and the input file not to be destroyed.
     * In this case, you cannot ask to re-map logical ids, or
     * to recluster the large objects (they make no sense).
     * You *MUST* provide conversion functions for the index key
     * to be converted to a lexicographic-order-able string in cases
     * 2,3 above. ONLY if the sort key is already in lexicographic-
     * order-able form, the sort key can be used directly.
     */
    // Use sort key directly
    int     set_for_index() {
        _keep_orig_file = true;
        _deep_copy = false;
        _for_index = true; 
        if(_nkeys > 1) {
            return 1; // BAD 
            // we only support single-key btrees
        }
        _cb_lexify = sort_keys_t::noCSKF;
        _cb_lexify_cookie = key_cookie_t(0);
        return 0;
        }

    // compute index key
    int     set_for_index( CSKF lfunc, key_cookie_t ck) {
                set_for_index();
                if(!lfunc) lfunc = sort_keys_t::noCSKF;
                _cb_lexify = lfunc;
                _cb_lexify_cookie = ck;
                return 0;
            }
    bool    is_for_index() const { return _for_index; }
    bool    is_for_file() const { return !_for_index; }

    bool    is_stable() const { return _stable; }
    void    set_stable(bool val) { _stable = val; }

    CSKF    lexify() const { return _cb_lexify; }
    key_cookie_t     lexify_cookie() const { return _cb_lexify_cookie; }

    // opposite of set_for_index:
    int     set_for_file(bool deepcopy, bool keeporig, bool carry_obj) {
            _for_index = false;
            (void) set_carry_obj(carry_obj);
            (void) set_deep_copy(deepcopy);
            return set_keep_orig(keeporig);
        }

    int     set_object_marshal( MOF marshal, UMOF unmarshal, key_cookie_t c) {
            _cb_marshal = marshal;
            _cb_unmarshal = unmarshal;
            _cb_marshal_cookie = c;
            return 0;
        }
    MOF     marshal_func() const { return _cb_marshal; }
    UMOF    unmarshal_func() const { return _cb_unmarshal; }
    key_cookie_t    marshal_cookie() const { return _cb_marshal_cookie; }

    bool    is_ascending() const { return _ascending; } 
    int     set_ascending( bool value = true) {  _ascending = value; return 0; }

    bool    is_unique() const { return _remove_dups; } 
    int     set_unique( bool value = true) {  _remove_dups = value; return 0; }

    bool    null_unique() const { return _remove_dup_nulls; } 
    int     set_null_unique( bool value = true) {  _remove_dup_nulls = value; 
            return 0; }

    bool    carry_obj() const {  return _carry_obj; }
    int     set_carry_obj(bool value = true) {  
        return _carry_obj = value; 
        return 0;
        }

    bool    deep_copy() const {  return _deep_copy; }
    int     set_deep_copy(bool value = true ) {  
        _deep_copy = value; 
        return 0; 
        }

    bool    keep_orig() const {  return _keep_orig_file; }
    int     set_keep_orig(bool value = true ) {  
        if(value && !_deep_copy) return 1;
        _keep_orig_file = value;
        return 0;
        }

    /* For each key, you must call either
    *  set_sortkey_fixed (in the  fixed cases), or
    *  set_sortkey_derived (in every other case).
     */
    int set_sortkey_fixed(int key, 
        smsize_t off, smsize_t len,
        bool in_header,
        bool aligned, 
        bool lexico, 
        CF   cfunc
        );
    int set_sortkey_derived(int key, 
        CSKF gfunc,
        key_cookie_t cookie,
        bool in_header,
        bool aligned, 
        bool lexico, 
        CF   cfunc
        );
    key_location_t&  get_location(int i) { return _locs[i]; }

    smsize_t offset(int i) const {
        return _locs[i]._off;
    }
    smsize_t length(int i) const {
        return _locs[i]._length;
    }
    CSKF keyinfo(int i) const {
        return _meta[i]._cb_keyinfo;
    }
    CF keycmp(int i) const {
        return _meta[i]._cb_keycmp;
    }
    key_cookie_t cookie(int i) const {
        return _meta[i]._cookie;
    }
    bool     is_lexico(int i) const {
        return (_meta[i]._mask & t_lexico)?true:false;
    }
    bool     is_fixed(int i) const {
        return (_meta[i]._mask & t_fixed)?true:false;
    }
    bool     is_aligned(int i) const {
        return (_meta[i]._mask & t_aligned)?true:false;
    }
    bool     in_hdr(int i) const {
        return (_meta[i]._mask & t_hdr)?true:false;
    }


};

inline void 
sort_keys_t::_grow(int i) 
{
    // realloc it to accommodate i more entries
    {
    key_location_t* tmp = new key_location_t[_spaces + i];
    if(!tmp) W_FATAL(fcOUTOFMEMORY);
    if(_locs) {
        memcpy(tmp, _locs, nkeys() * sizeof(key_location_t));
        delete[] _locs;
    }
    _locs = tmp;
    }
    {
    key_meta_t* tmp = new key_meta_t[_spaces + i];
    if(!tmp) W_FATAL(fcOUTOFMEMORY);
    if(_meta) {
        memcpy(tmp, _meta, nkeys() * sizeof(key_meta_t));
        delete[] _meta;
    }
    _meta = tmp;
    }
    _spaces += i;
    // don't change nkeys
}
inline NORET 
sort_keys_t::sort_keys_t(int nkeys):
    _nkeys(0),
    _spaces(0),
    _meta(0),
    _locs(0),
    _stable(false), _for_index(false), 
    _remove_dups(false), _remove_dup_nulls(false),
    _ascending(true), _deep_copy(false), _keep_orig_file(false),
    _carry_obj(false),
    _cb_lexify(sort_keys_t::noCSKF),
    _cb_lexify_cookie(0),
    _cb_marshal(sort_keys_t::noMOF),
    _cb_unmarshal(sort_keys_t::noUMOF),
    _cb_marshal_cookie(0)
{ 
    _grow(nkeys);
}

inline void 
sort_keys_t::_prep(int key) 
{
    if(key >= _spaces) {
    _grow(5);
    }
    if(key >= _nkeys) {
    // NB: hazard if not all of these filled in!
    _nkeys = key+1;
    }
}

inline void 
sort_keys_t::_copy(const sort_keys_t &old) 
{
    _prep(old.nkeys()-1);
    int i;
    for(i=0; i< old.nkeys(); i++) {
    _locs[i] = old._locs[i];
    _meta[i] = old._meta[i];
    }
    w_assert3(nkeys() == old.nkeys());
    w_assert3(_spaces >= old._spaces);
    for(i=0; i< old.nkeys(); i++) {
    w_assert3(_meta[i]._mask == old._meta[i]._mask);
    }
}

inline NORET 
sort_keys_t::sort_keys_t(const sort_keys_t &old) : 
    _nkeys(0), _spaces(0), 
    _meta(0), _locs(0) 
{
    _copy(old);
}

inline int 
sort_keys_t::set_sortkey_fixed(
    int key, 
    smsize_t off, 
    smsize_t len,
    bool in_header,
    bool aligned, 
    bool lexico,
    CF   cfunc
) 
{
    if(is_for_index() && key > 0) {
        return 1;
    }
    _prep(key);
    _set_mask(key,  true, aligned, lexico, 
                in_header, noCSKF, cfunc, key_cookie_t::null);
    _set_loc(key,  off, len);
    return 0;
}

inline int 
sort_keys_t::set_sortkey_derived(
    int key, 
    CSKF gfunc,
    key_cookie_t cookie,
    bool in_header,
    bool aligned, 
    bool lexico, 
    CF   cfunc
)
{
    if(is_for_index() && key > 0) {
    return 1;
    }
    _prep(key);
    _set_mask(key, false, aligned, lexico, 
        in_header,
        gfunc, cfunc, 
        cookie);
    return 0;
}

/*<std-footer incl-file-exclusion='SORT_S_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
