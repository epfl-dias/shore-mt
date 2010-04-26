/*<std-header orig-src='shore'>

 $Id: sort_funcs4.cpp,v 1.17.2.11 2010/03/19 22:20:31 nhall Exp $

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

/*
 * A set of applications functions -- to be moved into the
 * sm library -- to handle new sorting API
 */


#include "shell.h"
#include "sort_funcs.h"

#include <new>

#ifdef EXPLICIT_TEMPLATE
template class w_auto_delete_array_t<metadata>;
template class w_auto_delete_array_t<double>;
template class w_auto_delete_array_t<CF>;
template class w_auto_delete_array_t<CSKF>;
template class w_auto_delete_t<sort_keys_t>;
#endif

w_rc_t 
copyMOF (
    const rid_t&         ,  // record id
    const object_t&        obj_in,
    key_cookie_t        ,  // type info
                            // func must allocate obj_out,
    object_t*        obj_out         // SM will free mem, delete
)
{ 
    /* Just for the heck of it, let's malloc some space and make
     * a copy of the object in memory 
     */
    smsize_t         blength = obj_in.body_size();
    smsize_t         hlength = obj_in.hdr_size();

    factory_t&        f = *factory_t::cpp_vector;
    void*        h=0;
    void*        b=0;
    if(hlength) {
        h = f.allocfunc(hlength);
        vec_t hdr(h, hlength);
        W_DO(obj_in.copy_out(true , 0, hlength, hdr));
    }
    if(blength) {
        b = f.allocfunc(blength);
        vec_t body(b, blength);
        W_DO(obj_in.copy_out(false, 0, blength, body));
    }
    new (obj_out) object_t(h, hlength, f, b, blength, f);
    return RCOK;
}

w_rc_t 
copyUMOF (
        const rid_t&         ,  // record id
        const object_t&        obj_in,
        key_cookie_t        ,  // type info
                                // func must allocate obj_out,
        object_t*        obj_out         // SM will free mem, delete
)
{ 
    new (obj_out) object_t(obj_in);
    return RCOK;
}

/*
 Scan file and verify that it's sorted --
 Expects metadata in hdr.  This is for the multikey sort test.
*/
int
check_multikey_file( stid_t  fid, sort_keys_t&kl, bool do_compare)
{
#define STRINGMAX ss_m::page_sz
    pin_i*          pin;
    bool            eof;
    rc_t            rc;
    scan_file_i*    scan;
    int             comparisons=0;
    int             nrecords=0;
    int             nulls=0;

    int nkeys = kl.nkeys();
    int k;

    scan = new scan_file_i(fid, ss_m::t_cc_file);

    char *strbuf = new char[STRINGMAX];
    w_auto_delete_array_t<char> autodel(strbuf);

    int   *oldlengths = new int[nkeys];
    typedef const char * charp;
    charp  *oldkeys = new charp[nkeys];
    w_auto_delete_array_t<int> autodel2(oldlengths);
    w_auto_delete_array_t<const char *> autodel3(oldkeys);
    charp *keys = new charp[nkeys];
    int  *lengths = new int[nkeys];
    w_auto_delete_array_t<int> autodel4(lengths);
    w_auto_delete_array_t<const char *> autodel5(keys);

    bool first = true;

    while ( !(rc=scan->next(pin, 0, eof)).is_error() && !eof) {
         nrecords++;
         struct metadata *meta;
#ifdef W_DEBUG
         smsize_t metadata_size = sizeof(metadata) * nkeys;
         w_assert3(pin->hdr_size() == metadata_size);
#endif
         meta = (metadata *)pin->hdr();

        DBG(<<" scan record " << pin->rid());
        nbox_t box(2); // uses only 2-d box

        /* 
         * Locate key(s) in new object
         */
        int myoffset= 0;
        for(k=0; k<nkeys; k++) {
            myoffset += meta[k].offset;
            keys[k] = pin->body() + meta[k].offset;
            if(pin->is_small()) {
                DBG(<<"key " << k << " is " << keys[k]
                        << " length " << meta[k].length);
            } else {
                //TODO: can't handle this
                cerr <<"key " << k << " is " << keys[k]
                        << " length " << meta[k].length << endl;
                W_FATAL_MSG(fcNOTIMPLEMENTED, 
                    << "can't check large objects yet "<<endl
                        );
                delete scan;
                return 1;
            }
            lengths[k] = meta[k].length;
            myoffset += lengths[k];
        }

        if(do_compare && !first) {
            int j=0;
            /* compare with last one read */
            /* compare most significant key first */
            /* if uniq cannot be == */
            for(k=0; k<nkeys; k++) {
                DBG(<<" keycmp oldlen=" << 
                    oldlengths[k] << " newlen= " << lengths[k]);
                if(oldlengths[k] == 0 || lengths[k] == 0) {
                     j = oldlengths[k] - lengths[k];
                     // If one or the other is null, comparison
                     // is determined by length only 
                    comparisons++;
                } else {
                    if (meta[k].t == test_spatial) {
                        nbox_t box1(2), box2(2);
                        box2.bytes2box(keys[k],  box2.klen());
                        DBG(<<"new key is " << box2);
                        int hvalue2 = box2.hvalue(universe);
                        DBG(<< " hvalue is " << hvalue2
                                << " with universe " << universe
                        );

                        int hvalue1;
                        w_assert1(oldlengths[k] == sizeof(hvalue1));
                        memcpy(&hvalue1, oldkeys[k], oldlengths[k]);

                        j= (*kl.keycmp(k))(
                            sizeof(hvalue1), &hvalue1,
                            sizeof(hvalue2), &hvalue2
                            );
                    } else {
                        j= (*kl.keycmp(k))(
                            oldlengths[k], oldkeys[k],
                            lengths[k], keys[k]
                            );
                    }
                    comparisons++;
                    DBG(<<" keycmp  key " << k  << " old record with " << pin->rid() 
                            << " yields " << j);
                        // j < 0 iff old < new
                }
                if(j==0) {
                    DBG(<<"compare next key");
                    continue; // to next key
                } else {
                    DBG(<<"Skip rest of keys: key " 
                        << k 
                        << " comparison is " << j
                        );
                    break; // stop here
                }
            }
            if(kl.is_unique()) {
                w_assert1(j != 0);
                if(j==0) {
                    delete scan;
                    return 2;
                }
            }
            if(kl.is_ascending()) {
                w_assert1(j <= 0);
                if(j>0)  {
                    delete scan;
                    return 3;
                }
            } else {
                w_assert1(j >= 0);
                if(j<0) {
                    delete scan;
                    return 4;
                }
            }
        }

        /* copy out key */
        memset(strbuf, 'Z', STRINGMAX);
        {
            char *dest = strbuf;
            for(k=0; k<nkeys; k++) {
                w_assert1(dest - strbuf < STRINGMAX);
                oldkeys[k] = dest;
                oldlengths[k] = lengths[k];
                if (meta[k].t == test_spatial) {
                    nbox_t box(2);
                    box.bytes2box(keys[k], box.klen());
                    int hvalue = box.hvalue(universe);
                    DBG(<<"saving hvalue " << hvalue << " for box " << box);
                    w_assert1(lengths[k] == sizeof(hvalue));
                    memcpy(dest, &hvalue, lengths[k]);
                } else {
                    memcpy(dest, keys[k], lengths[k]);
                }
                dest += lengths[k];
                DBG(<<"Copied key : length[k]="  << oldlengths[k]);
                if(lengths[k] == 0) nulls++;
            }
            if(!do_compare) {
                // TODO: print key, oid instead
            }
        }
        first = false;

    } //scan

cout << "check_multikey_file did  " 
        << comparisons << " key comparisons for " 
        << nrecords << " records (" 
        << nulls << " null records)" << endl << endl;
    delete scan;
    return 0;
}

/*
 * t_multikey_sort_file...
 * This does NOT have a logical-id counterpart
 */
int
t_multikey_sort_file(Tcl_Interp* ip, int ac, TCL_AV char* av[])
{
    static int count_bv_objects =0;

    FUNC(t_multikey_sort_file);
    const int vid_arg =1;
    const int nrecs_arg =2;
    const int runsize_arg =3;
    const int up_arg =4;
    const int uniq_arg =5;
    const int output_arg =6;
    const int keep_arg = 7;
    const int deep_arg = 8;
    const int carry_arg = 9;
    const int nkeys_arg =10;
    const int type_arg =11;
    const int atr_arg =12;
    int   file_length = 0;


    rand48&  generator(get_generator());
    generator.seed(99); // pick any seed
        // we're doing this to be sure that the #duplicates 
        // generated is identical for all tests of same type and nrecs

    const char *usage_string = 
//1   2    3       4       5           6          7           8           9               10    11, 12    [13,14] ...
"vid nrecs runsize up|down uniq|dupok  file|index keep|nokeep deep|shallow carry|nocarry  nkeys type FVDNA [type FVDNA]*";

    /* handle up to 5 keys */
    if (check(ip, usage_string, ac, atr_arg+1, 
                atr_arg+3, atr_arg+5, atr_arg+7, atr_arg+9)) {
        cerr<< "ac = " << ac <<endl;
        cerr<< "want " << atr_arg+1 << "..." << atr_arg+8 <<endl;
        return TCL_ERROR;
    }


    /* 
     * vid_arg 
     */
    stid_t fid;
    int volumeid = atoi(av[vid_arg]);

    /* 
     * nrecs, runsize
     */
    int nrecs = atoi(av[nrecs_arg]);
    int runsize = atoi(av[runsize_arg]);

    /* 
     * up/down
     */
    bool ascending = true;
    if(strcmp(av[up_arg],"down")==0) {
        ascending = false;
    } else if(strcmp(av[up_arg],"up")==0) {
        ascending = true;
    } else {
        cerr << av[up_arg] << ":" << endl;
        cerr << "bad argument " << up_arg << " [up|down]" <<endl;
        return TCL_ERROR;
    }

    /* 
     * uniq/dupok
     */
    bool uniq = false;
    if(strcmp(av[uniq_arg],"uniq")==0) {
        uniq = true;
    } else if(strcmp(av[uniq_arg],"dupok")==0) {
        uniq = false;
    } else {
        cerr << av[uniq_arg] << ":" << endl;
        cerr << "bad argument " << uniq_arg << " [uniq|dupok]" <<endl;
        return TCL_ERROR;
    }

    /* 
     * file/index
     */
    bool index_output = false;
    if(strcmp(av[output_arg],"index")==0) {
        index_output = true;
    } else if(strcmp(av[output_arg],"file")==0) {
        index_output = false;
    } else {
        cerr << av[output_arg] << ":" << endl;
        cerr << "bad argument " << output_arg << " [index|file]" <<endl;
        return TCL_ERROR;
    }

    /*
     * keep|nokeep
     */
    bool keep_orig = false; 
    if(strcmp(av[keep_arg],"keep")==0) {
        keep_orig = true;
    } else if(strcmp(av[keep_arg],"nokeep")==0) {
        keep_orig  = false;
    } else {
        cerr << av[keep_arg] << ":" << endl;
        cerr << "bad argument " << keep_arg << " [keep|nokeep]" <<endl;
        return TCL_ERROR;
    }
    /*
     * deep|shallow
     */
    bool deep_copy = false; 
    if(strcmp(av[deep_arg],"deep")==0) {
        deep_copy = true;
    } else if(strcmp(av[deep_arg],"shallow")==0) {
        deep_copy  = false;
    } else {
        cerr << av[deep_arg] << ":" << endl;
        cerr << "bad argument " << deep_arg << " [deep|shallow]" <<endl;
        return TCL_ERROR;
    }

    /*
     * carry obj or re-visit orig page
     */
    bool carry_obj = false; 
    if(strcmp(av[carry_arg],"carry")==0) {
        carry_obj = true;
    } else if(strcmp(av[carry_arg],"nocarry")==0) {
        carry_obj  = false;
    } else {
        cerr << av[carry_arg] << ":" << endl;
        cerr << "bad argument " << carry_arg << " [carry|nocarry]" <<endl;
        return TCL_ERROR;
    }

    /*
     * nkeys
     */
    int nkeys = atoi(av[nkeys_arg]);

    
    const int max_keys_handled = 5;
    bool make_fixed[max_keys_handled];
    bool make_aligned[max_keys_handled];
    bool make_nullable[max_keys_handled];
    bool make_derived[max_keys_handled];
    typed_btree_test t[max_keys_handled];
    CF *keycmp = new CF[nkeys];
    w_auto_delete_array_t<CF> auto_del5(keycmp);

    CSKF *derive_func = new CSKF[max_keys_handled];
    w_auto_delete_array_t<CSKF> auto_del6(derive_func);


    universe.nullify();

    int k;
    for(k = max_keys_handled-1; k>=0; k--) {
        make_fixed[k] = 
        (make_aligned[k] = 
        (make_nullable[k] =
        (make_derived[k] = false)));

        t[k] = test_nosuch;
        derive_func[k] = sort_keys_t::noCSKF;
    }
    for(k = nkeys-1; k>=0; k--) {
        /*
         * key type (type_arg + (k*2))
         */
        int arg = type_arg + (k*2);
        if(ac < arg+1) {
            cerr << "Not enough arguments for key " << k <<endl;
            return TCL_ERROR;
        }
            t[k] = cvt2type(av[arg]);

        /*
         * key location attributes
         * (atr_arg + (k*2))
         */
        arg = atr_arg + (k*2);
        if(ac < arg+1) {
            cerr << "Not enough arguments for key " << k <<endl;
            return TCL_ERROR;
        }
        TCL_AV char *cp = av[arg];
        while(*cp) {
            switch(*cp++) {
            case 'F':
                DBG(<<"make FIXED key " << k);
                make_fixed[k] = true;
                // TODO: all prior keys must be fixed too - our
                // test mechanism doesn't handle variable before
                // fixed
                break;
            case 'V':
                DBG(<<"make VARIABLE-loc key " << k);
                make_fixed[k] = false;
                break;
            case 'N':
                DBG(<<"make NULL key " << k);
                make_nullable[k] =true;
                // NB: this disables fixed: see below
                break;
            case 'D':
                DBG(<<"make DERIVED key " << k);
                make_derived[k] =true;
                break;
            case 'A':
                DBG(<<"make ALIGNED key " << k);
                make_aligned[k] = true;
                break;
            default:
                cerr <<"Unrecognized key location attribute for key " 
                        << k << ":" << *cp << endl;
                return TCL_ERROR;
            }
            if(t[k] == test_spatial) {
                make_derived[k] =true;
            }
            // if nullable, it's no longer fixed!
            if(make_nullable[k]) make_fixed[k]=false;
        }
    }
    {
        bool found=false;
        for(k = nkeys-1; k>=0; k--) {
            if(make_fixed[k]) { 
                found = true; 
                DBG(<<"FIXED " << k );
            } else { 
                if(found) {
                    cerr << "**********************************************"
                        <<endl;
                    cerr <<"Test does not support variable (V or not F) key "
                    << " before a fixed key " << endl;
                    return TCL_ERROR;
                }
                DBG(<<"NOT FIXED " << k );
            }
        }
    }

    sort_keys_t kl(nkeys);
    generic_CSKF_cookie g;
    {
        CSKF cskfunc;
        for(k = 0; k <  nkeys; k++) {
            keycmp[k] = getcmpfunc(t[k], cskfunc, key_cookie_t(&g));
        }
        if(index_output) {
            if(nkeys > 1) {
                cerr << "Multi-key indexes are not supported" <<endl;
                return TCL_ERROR;
            }

            // no key cookie
            if(kl.set_for_index(cskfunc, key_cookie_t(&g))) {
                w_reset_strstream(tclout);
                tclout << smsh_err_name(ss_m::eBADARGUMENT) << ends;
                Tcl_AppendResult(ip, tclout.c_str(), 0);
                w_reset_strstream(tclout);
                return TCL_ERROR;
            }
            keep_orig = true;
        }
    }
    /*
     * Create the input file and
     * go about creating the records
     * From this point on, encountering an error means
     * we have to delete this file (and/or the result file).
     *
     * Use an unlogged (load) file just to keep the logging to a minimum.
     */

    DO( sm->create_file(vid_t(volumeid), fid, ss_m::t_load_file
                ));
    deleter        d1(fid); // auto-delete

    int nulls=0, smallkeys=0;
    smsize_t total_length=0;
    smsize_t metadata_size = sizeof(metadata) * nkeys;
    metadata *meta = 0;
    meta = new metadata[nkeys];
    w_auto_delete_array_t<metadata> auto_del7(meta);
    {

        int j=0;
        for(k = 0; k <  nkeys; k++) {
            meta[j++].t = t[k];
        }
        const smsize_t         max_total_length = 3 * ss_m::page_sz;


        /* Set up metadata & create the objects all at once.
         * If we're doing fixed, we could do this in
         * two separate loops; instread, we check an assertion
         * in the fixed case
         */
        char*                object_base   = new char[max_total_length];
        char*                object   = object_base;
        w_auto_delete_array_t<char> auto_del8(object_base);

        memset(object_base, '\0', max_total_length);

        for(j=0; j < nrecs; j++) {
            smsize_t    offset = 0;
            object = object_base;

            for(k = 0; k <  nkeys; k++) {
                smsize_t    length = 0;
                smsize_t    dlength = 0; // derived (key) length
                smsize_t buff =  0;
                if(make_aligned[k]) {
                    /* make sure each key is 8-byte aligned */
                    /* XXX use align tools */
                    if(offset & 0x7) {
                        buff = offset + 8;
                        buff &= ~0x7;
                        buff -= offset;
                    }
                } else if(!make_fixed[k]) {
                    if(count_bv_objects > 5) {
                       // Force compare_in_pieces to compare
                       // things that aren't in page-sync
                       buff = count_bv_objects * 5;
                    } else {
                        buff = smsize_t(generator.rand() & 0x8);
                    }
                } // else buff = 0;
                DBG(<<"buff size " << buff);
                offset += buff;
                memset(object, 'z', buff);
                object += buff;

                /* If this isn't the first time around and we're doing
                 * fixed-location keys, make sure the
                 * data we've computed is the same as it was the last
                 * time around 
                 */
                if(j > 0 && make_fixed[k]) {
                    w_assert1(meta[k].offset == offset);
                    DBG(<<"offset=" << offset);
                } else {
                    meta[k].offset = offset;
                    DBG(<<"offset=" << offset);
                }
                meta[k].fixed = make_fixed[k];
                meta[k].aligned = make_aligned[k];
                meta[k].nullable = make_nullable[k];

                typed_btree_test t = meta[k].t;
                // cout << "Key " << k << " at offset " << offset ;

                {
                    // cout << " type: " ;
                    switch(t) {
                    case test_spatial: {
                        nbox_t box2(2);
                        length = box2.klen();
                        make_random_box(object, length);

                        // Derive key and sort as integer
                        dlength = sizeof(int);

                        meta[k].lexico = false;
                        meta[k].aligned = true;
                        meta[k].derived = true;
                        derive_func[k] = mhilbert;
                        w_assert3(make_derived[k]);
                        }
                        break;

                    case test_bv:
                    case test_blarge:

                        // cout << "BV" ;
                        // info.type = key_info_t::t_string;
                        // pseudo-randomly choose these

                        if(make_fixed[k]) {
                            length = smsize_t(::getpid()) & 0x7f;
                    // cerr << "test_bv using length " << length <<endl;
                        } else {
                            length = 
                            smsize_t(generator.rand() 
                                    & 0x7f);  // keep it reasonably small
                        }
                        // minimum length for not-nullable case
                        if(length == 0 && !make_nullable[k]) length = 1;

                        /*
                         * Every 6,7,8,9,10th object:
                         * Make it big and make it cross page boundaries
                         * (ONLY if not made for indexes)
                         * Any race here is benign because it doesn't have
                         * to be exact and I think the smsh scripts that
                         * do this don't do multi-threaded sort_files,
                         * although we should test this (not that a sort
                         * file is multithreaded, but that it's
                         * reentrant) 
                         */
                        atomic_add(count_bv_objects, 1) ;
                        {
                            int c = count_bv_objects;
                            c %= 10;
                            count_bv_objects = c;
                        }

                        if( !kl.is_for_index() && !make_fixed[k] && 
                                (count_bv_objects > 5)) {
                            // make it huge, make it differ from
                            // other such objects only in last "length"
                            // bytes, to force compare_in_pieces.
                            smsize_t last_length = length;
                            smsize_t add_length = 2 * ss_m::page_sz + 1;
                            w_assert1(offset + add_length + last_length 
                                < max_total_length);
                            memset(object, 'E', add_length);
                            make_random_alpha(object+add_length, last_length);
                            length = add_length + last_length;
                        } else {
                            // random alpha string:
                            make_random_alpha(object, length);
                        }
                        meta[k].lexico = true;
                        meta[k].aligned = true;
                        dlength = length;
                        break;

                    case test_b23:
                        // cout << "B 23";
                        length = 23;
                        make_random_alpha(object, length);
                        meta[k].lexico = true;
                        meta[k].aligned = true;
                        dlength = length;
                        break;

                    case test_b1:
                        smallkeys++;
                        // cout << "B 1";
                        length = 1;
                        make_random_alpha(object, length);
                        meta[k].lexico = true;
                        meta[k].aligned = true;
                        dlength = length;
                        break;

                    case test_u1:
                    case test_i1: {
                        smallkeys++;
                        // cout << "I/U 1";
                        w_base_t::uint1_t i=
                        w_base_t::uint1_t(generator.rand() & 0xff);
                        length = 1;
                        memcpy(object, &i, length);
                        }
                        dlength = length;
                        break;

                    case test_u2:
                    case test_i2: {
                        // cout << "I/U 2";
                        w_base_t::uint2_t i=
                        w_base_t::uint2_t(generator.rand() & 0xffff);
                        length = 2;
                        memcpy(object, &i, length);
                        }
                        dlength = length;
                        break;

                    case test_u4:
                    case test_i4: {
                        // cout << "I/U 4";
                            w_base_t::uint4_t i= generator.rand();
                            length = 4;
                            memcpy(object, &i, length);
                        }
                        dlength = length;
                        break;

                    case test_u8:
                    case test_i8: {
                        w_base_t::uint8_t i=
                                generator.rand();
                        length = 8;
                        memcpy(object, &i, length);
                        }
                        dlength = length;
                        break;

                    case test_f4: {
                        // cout << "F 4";
                        float i = float(generator.drand());
                        length = 4;
                        memcpy(object, &i, length);
                        dlength = length;
                        }
                        break;

                    case test_f8:{
                        // cout << "F 8" ;
                        double i = double(generator.drand());
                        length = 8;
                        memcpy(object, &i, length);
                        dlength = length;
                        }
                        break;

                    default:
                        cerr << "switch" <<endl;
                        w_assert3(0);
                    } /* switch */
                } 


                if(j > 0 && make_fixed[k] && !make_nullable[k]) {
                    w_assert1(meta[k].length == dlength);
                } else {
                    DBG(<<"set length to " << dlength);
                    meta[k].length = dlength;
                }
                if(make_nullable[k] && (count_bv_objects <= 4)
                        &&  (j != nrecs-1) // not the last one
                        ) {
                    // For some random # objects (but NOT the last one, 
                    // null the key
                    // Need to add length into offsets of object,
                    // but set the metadata's length to 0 for this
                    // object only
                    // NB: can't do this to the last object because
                    // we use meta[k].length to set up the info structure
                    // for the sort, and we don't want to have clobbered
                    // the length info.
                    unsigned int xxx = int(generator.rand());
                    if(xxx & 0x00600200) {
                        DBG(<<"forcing NULL");
                        nulls++;
                        meta[k].length = 0;
                        length = 0;
                        dlength = 0;
                    }
                }
                // cout << " length " << length <<endl;
                offset += length;
                object += length; // get ready for next key
                if(offset > max_total_length) {
                    cerr << "exceeded max length" <<endl;
                    w_assert3(0);
                }
                total_length = offset;
            }


            // Put object in file
            vec_t hdr(meta, metadata_size);
            vec_t data(object_base, total_length);

            rid_t rid;
            DBG( << "create_rec: sizeof(meta)=" << metadata_size);
            DBG( << "       total length=" << total_length );
    if (linked.verbose_flag) {
        cout << "create rec: total_length =" << total_length <<endl;
        file_length += total_length;
    }

            if(1) { // DEBUG : print keys
            for(int kk=0; kk<nkeys; kk++) {
            DBG( << "       offset for object is " << meta[kk].offset);
                char *o = object_base + meta[kk].offset;
                if(meta[kk].length == 0) {
                    DBG(<<"NULL");
                } else switch(meta[kk].t) {
                case test_spatial: {
                        nbox_t b(2);
                        w_assert3(b.klen() < int(sizeof(b)));
                        b.bytes2box((const char *)o, int(b.klen()));
                        DBG(<<" BOX(" << b <<" ) hvalue=" << b.hvalue(universe) );
                    }
                    break;

                case test_bv:
                case test_blarge:
                case test_b23:
                case test_b1: {
                    char *p = o;
                    if(strlen(p) > 1000) {
                        while (*p == 'E') p++; 
                    }
                    if(p-o > 0) {
                        DBG(<<" E(" <<(int)(p-o) <<" times)" << p);
                    } else {
                        DBG( << "string:"<< o);
                    }
                    } 
                    break;
                case test_i1: {
                    w_base_t::int1_t i;
                    memcpy(&i, o, 1);
                    DBG(<<int(i));
                    }; break;
                case test_u1: {
                    w_base_t::uint1_t i;
                    memcpy(&i, o, 1);
                    DBG(<<unsigned(i));
                    }; break;
                case test_i2: {
                    w_base_t::int2_t i;
                    memcpy(&i, o, 2);
                    DBG(<<i);
                    }; break;
                case test_u2: {
                    w_base_t::uint2_t i;
                    memcpy(&i, o, 2);
                    DBG(<<i);
                    }; break;
                case test_i4: {
                    w_base_t::int4_t i;
                    memcpy(&i, o, 4);
                    DBG(<<i);
                    }; break;
                case test_u4: {
                    w_base_t::uint4_t i;
                    memcpy(&i, o, 4);
                    DBG(<<i);
                    }; break;
                case test_i8: {
                    w_base_t::int8_t i;
                    memcpy(&i, o, 8);
                    DBG(<<i);
                    }; break;
                case test_u8: {
                    w_base_t::uint8_t i;
                    memcpy(&i, o, 8);
                    DBG(<<i);
                    }; break;
                case test_f4: {
                    w_base_t::f4_t i;
                    memcpy(&i, o, 4);
                    DBG(<<i);
                    }; break;
                case test_f8: {
                    w_base_t::f8_t i;
                    memcpy(&i, o, 8);
                    DBG(<<i);
                    }; break;
                default:
                    w_assert1(0);
                    break;
                }
            } }
            DO( sm->create_rec(fid, hdr, 10, data, rid) );
#ifdef W_TRACE
            {
                struct metadata *m = (metadata *)hdr.ptr(0);
                for(k = 0; k <  nkeys; k++) {
                    DBG( <<  rid 
                            << " WROTE METADATA: " 
                            << " offset=" << m[k].offset
                            << " length=" << m[k].length
                            );
                }
            }
#endif /* W_TRACE */
            memset(object_base, '\0', total_length);
        }
        DBG( << "Done creating " << nrecs << " records " );
    }


    for(k = 0; k <  nkeys; k++) {
        if(make_fixed[k]) {
            kl.set_sortkey_fixed(k, 
                meta[k].offset, meta[k].length, 
                false, // in body
                meta[k].aligned, 
                meta[k].lexico, 
                keycmp[k]
                );
        } else if(derive_func[k] == sort_keys_t::noCSKF) {
          // else use default info: no offset/length, false for both
          // fixed and aligned
          kl.set_sortkey_derived(k, 
                get_key_info,
                key_cookie_t(k),
                false, // in body
                meta[k].aligned, 
                meta[k].lexico, 
                keycmp[k]
                );
        } else {
          /*
           * Sort on derived key. 
           */
          // else use default info: no offset/length, false for both
          // fixed and aligned
          kl.set_sortkey_derived(k, 
                derive_func[k],
                key_cookie_t(k),
                false, // in body
                meta[k].aligned, 
                meta[k].lexico, 
                keycmp[k]
                );
        }
    }
    /*
     * NB: this test only sorts the file - it does
     * not produce output for indexes.
     */

    if(!index_output) {
        if(kl.set_for_file(deep_copy, keep_orig, carry_obj)) {
            cerr << "Problem with set_for_file" <<endl;
            return TCL_ERROR;
        }
    }
    kl.set_stable(true);
    // if(kl.set_object_marshal(copyMOF, copyUMOF, 0)) 
    if(kl.set_object_marshal(copyMOF, sort_keys_t::noUMOF, key_cookie_t::null)) 
    {
        cerr << "Problem with set_object_marshal" <<endl;
        return TCL_ERROR;
    }


    w_rc_t rc;
    if (linked.verbose_flag) {
        /*
         * print the input file, fid
         */
        cout << "****************   INPUT FILE   *******************" <<endl;
        if(1) {
            scan_file_i *scan = new scan_file_i(fid);
            rc = dump_scan(*scan, cout, true); // hex form
            if(rc.is_error()) cout << "returns rc=" << rc <<endl;
            delete scan;
        }
        /* See how much disk space the file uses */
        if(1) {
            // sm_du_stats_t stats;
            // stats.clear();
            // rc =sm->get_du_statistics(fid, stats, false);
            // stats.print(cout, "input file stats:");
            cout << "total file data =" << file_length <<endl;
        }
        cout << "**************** END INPUT FILE *******************" <<endl;

    }

    /************************ sort the file ********************/

    stid_t     ofid;
    {   /* create output file for results */
        int volumeid = atoi(av[vid_arg]);
        DO( sm->create_file(vid_t(volumeid), ofid, ss_m::t_load_file
                    ) );
    }
    deleter        d2(ofid); // auto-delete

    const int   min_rec_sz = sizeof(metadata);
    vid_t vid(volumeid);

    {
        sm_stats_info_t* stats = new sm_stats_info_t;
        w_auto_delete_t<sm_stats_info_t>         autodel(stats);

        sm_stats_info_t& internal = *stats; 
        DO( sm->gather_xct_stats(internal, true));
    }
    if(kl.set_ascending(ascending)) {
        cout << "Cannot set ascending to " << ascending <<endl;
        return TCL_ERROR;
    }
    if(kl.set_unique(uniq)) {
        cout << "Cannot set uniq to " << uniq <<endl;
        return TCL_ERROR;
    }
    if(kl.set_null_unique(uniq)) {
        cout << "Cannot set nulls_uniq to " << uniq <<endl;
        return TCL_ERROR;
    }
    if(kl.set_deep_copy(deep_copy)) {
        cout << "Cannot set deep_copy to " << deep_copy <<endl;
        return TCL_ERROR;
    }
    if(kl.set_keep_orig(keep_orig)) {
        cout << "Cannot set keep_orig to " << keep_orig <<endl;
        return TCL_ERROR;
    }

    // This is the new sort phys version 
    rc = sm->sort_file( 
            fid, 
            ofid, 
            1, 
            &vid,
            kl                 , // sort_keys_t&                   
            min_rec_sz        ,
            runsize                , // run sz
            runsize*ss_m::page_sz // # pages for scratch
        );


    if(rc.is_error()) {
        if(rc.err_num() == ss_m::eBADARGUMENT) {
                // Print it to get line # from the stack trace
            cerr << "Bad argument to multi-key sort_file: " << rc <<endl;
        } else {
            cerr << "Unexpected error from sort_file: " << rc <<endl;
        }
        w_reset_strstream(tclout);
        tclout << smsh_err_name(rc.err_num()) << ends;
        Tcl_AppendResult(ip, tclout.c_str(), 0);
        w_reset_strstream(tclout);
        return TCL_ERROR;
    }
    /* If sort_file succeeded and !keep_orig, the input file
     * was destroyed.  If an error occurred, it will have
     * prevented the input file from being destroyed, so the
     * return above is ok.
     */
    if(!keep_orig) d1.disarm();

    int fck = check_multikey_file(ofid, kl, true);

fprintf(stderr, "check_multikey_file returns %d\n", fck);

    switch (fck) {
    case -1:
        // error
        Tcl_AppendResult(ip, "Cannot check file", 0);
        break;
    case 1:
        // error: can't handle large objects yet
        break;

    default:
        cerr << "failed reason = " << fck <<endl << flushl;

        Tcl_AppendResult(ip, "File check failed ", 0);
        break;
    case 0:
        if (linked.verbose_flag) {
            cout << "Check file succeeded" <<endl;
        }
        break;
    }
    if (linked.verbose_flag || fck > 1) {
        /*
         * print the input file, ofid
         */
        cout << "****************   OUTPUT FILE   *******************" <<endl;
        scan_file_i *scan = new scan_file_i(ofid);
        rc = dump_scan(*scan, cout, true);
        if(rc.is_error()) cout << "returns rc=" << rc <<endl;
        delete scan;
        cout << "**************** END OUTPUT FILE *******************" <<endl;
    }
    {
        sm_stats_info_t* stats = new sm_stats_info_t;
        w_auto_delete_t<sm_stats_info_t>         autodel(stats);

        sm_stats_info_t& internal = *stats; 
        DO( sm->gather_xct_stats(internal, false));
        /*
         * A feeble sanity check based on the stats 
         */
        if(internal.sm.sort_duplicates > 0) {
            /* 
             * had better have uniq option turned on, and 
             * probably should have 2 or more nulls, or u1/i1 type
             */
            if(!uniq) {
                cerr << "duplicates removed (" 
                        << internal.sm.sort_duplicates 
                        << "); uniq arg not given" <<endl;
                w_assert1(0);
            }
            if(nulls <=1 && smallkeys ==0 ) {
                cerr << "duplicates removed (" 
                        << internal.sm.sort_duplicates 
                        << ") nulls= " << nulls
                        << " NEEDS hand-checking" <<endl;
                // w_assert1(0);
            }
        }
        if(int(internal.sm.sort_recs_created) != nrecs) {
            if(!uniq) {
                cerr << "recs_created = " 
                        << internal.sm.sort_recs_created 
                        << "; expected " << nrecs << endl;
                // NOTE: This can only work if we #defined INSTRUMENT_SORT
                // in sm/newsort.cpp
                w_assert1(0);
            }
        }
    }

    /* input and output files will be destroyed by d1 and d2 */

    if(fck>1) {
        return TCL_ERROR;
    } else {
        Tcl_AppendResult(ip, tclout.c_str(), 0);
        w_reset_strstream(tclout);
        return TCL_OK;
    }
}
