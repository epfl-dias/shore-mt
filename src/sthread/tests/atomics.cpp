/*<std-header orig-src='shore'>

 $Id: atomics.cpp,v 1.1.2.6 2010/03/19 22:20:03 nhall Exp $

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

/*
 * If possible compile this _without_ optimization; so the 
 * register variables are really put in registers.
 */

#include <cassert>
#include <w_getopt.h>

#include <w.h>
#include <sthread.h>
#include <atomic_templates.h>

#include <iostream>
#include <w_strstream.h>
#include <iomanip>

pthread_barrier_t collect_barrier;
pthread_barrier_t done_barrier;

#define ITERATIONS 1
typedef enum { 
    t_inc, t_dec,                                 // one-op
    t_add, t_or, t_and, t_swap, t_set, t_clear, // two-op
    t_cas } which_t;                                 // three-op
const char *which_strings[] = {
        "t_inc", "t_dec", 
        "t_add", "t_or", "t_and", "t_swap", "t_set", "t_clear", 
        "t_cas" };


typedef void *          ptr_t ;
typedef signed char     char_t;

/* create a target for each type (t* = target of atomic_ops, r* = the result we
   collected with sequential applications of the threads' _mydiff
 */ 
static void *           tptr(0), *rptr(0);

const unsigned char CHARINIT='X'; /*integer 88*/
static w_base_t::uint1_t t8(CHARINIT),r8(CHARINIT), b8(CHARINIT);
static unsigned char    tchar(CHARINIT), rchar(CHARINIT) ; // , bchar(CHARINIT);
static unsigned char    tuchar(CHARINIT), ruchar(CHARINIT), buchar(CHARINIT);

const unsigned short SHORTINIT=0xabcd;
static w_base_t::uint2_t t16(SHORTINIT),r16(SHORTINIT), b16(SHORTINIT);
static unsigned short   tshort(SHORTINIT), rshort(SHORTINIT); // , bshort(SHORTINIT); // a little flaky
static unsigned short   tushort(SHORTINIT), rushort(SHORTINIT), bushort(SHORTINIT);

const unsigned int   T32INIT=0xabcd1023;
// const unsigned int   T32INIT=1;
static w_base_t::uint4_t t32(T32INIT),r32(T32INIT), b32(T32INIT);

#ifdef ARCH_LP64
const unsigned int   INTINIT=0xabcd1234ul;
#else
const unsigned int   INTINIT=0xabcd1023;
#endif
static unsigned int     tint(INTINIT), rint(INTINIT); //, bint(INTINIT); // a little flaky
static unsigned         tuint(INTINIT), ruint(INTINIT), buint(INTINIT);

#ifdef ARCH_LP64
const unsigned long   LONGINIT=0x1234abcd5678ef9ull;
#else
const unsigned long   LONGINIT=0x1234abcdull;
#endif
static w_base_t::uint8_t t64(0x1234abcd5678ef9ull),r64(0x1234abcd5678ef9ull), b64(0x1234abcd5678ef9ull);
static unsigned long     tlong(LONGINIT), rlong(LONGINIT); //, blong(LONGINIT); // a little flaky
static unsigned long     tulong(LONGINIT), rulong(LONGINIT), bulong(LONGINIT);

typedef enum  {  t_8, t_uchar, t_char, 
                 t_16, t_ushort, t_short,
                 t_32, t_uint, t_int, 
                 t_64, t_ulong, t_long,
               t_ptr
} target_t;

class worker_base_t : public sthread_t
{
};
typedef worker_base_t * threadptr;

template <class T>
class worker_thread_t : public worker_base_t 
{
protected:
        which_t  _w;
        int      _id;
        int      _iter;
        target_t _t;
        T        _operand; // not used by one-op threads
        T        _mydiff; // this is the net diff that this thread computed for the target
        T *      _target; // points to the thing we are using atomic ops on
public:
        void identify(const char *msg)
        {
            cerr  << msg
                << " thread id=" << _id << " which " << which_strings[int(_w)]
                << " target type " << _t
                << " iterations " << _iter
                << endl;
        }
        worker_thread_t (which_t w,
            int i, int n, T val, target_t t, T* targ) :
            _w(w), _id(i), _iter(n), _t(t), _target(targ)
        {
            _operand = val;
            _mydiff = 0;
        }
        virtual ~worker_thread_t(){}

        void run() {
            // identify("START");
            for(int i=0; i < _iter; i++) { doit(); }  
#if W_DEBUG_LEVEL > 0
                {   int e=pthread_barrier_wait(&collect_barrier); 
                w_assert1(e == 0 || e == PTHREAD_BARRIER_SERIAL_THREAD) ;
            }
            // identify("DONE - waiting on done barrier");
                {   int e=pthread_barrier_wait(&done_barrier); 
                w_assert1(e == 0 || e == PTHREAD_BARRIER_SERIAL_THREAD) ;
            }
#endif
            // identify("END RUN");
        }
        virtual void doit()=0;
        virtual void getresult(T& net)=0;
};

template <class T>
class one_op_thread_t : public worker_thread_t<T>
{
public:
    typedef void (*op_t)(volatile T *);
    typedef T (*opnv_t)(volatile T *);

private:
    op_t    _op;
    opnv_t  _opnv;

public:
    one_op_thread_t (
            which_t w, op_t op1, opnv_t op2,
            int i, int n, T val, 
            target_t t, T* targ
            ) 
        :
        worker_thread_t<T>(w, i,n,val,t, targ) ,
        _op(op1), _opnv(op2)
    {
#if 0
        cerr << "new one_op_thread for w=" << which_strings[int(w)]
        << " op1=" << op1
        << " op2=" << op2
        << " i=" <<  i
        << " n=" <<  n
        << " val=" <<  int(val)
        << " t=" <<  t
        << endl;
#endif
    }

    virtual void doit();
    virtual void getresult(T& net);

};

template <class T>
void one_op_thread_t<T>::getresult (T&net)
{ 
#if 0
    cerr << "one_op_thread getresult w=" << this->_w 
        << " net= " << net
        << " mydiff= " << int(this->_mydiff)
        << endl;
#endif
    net += this->_mydiff;

    // cerr << "NOW net= " << net << endl;
}

template <class T>
void one_op_thread_t<T>::doit()
{
#if 0
    cerr << "one_op_thread doit w=" << which_strings[int(this->_w )]
        << " applies to target, which starts at " << int(*(this->_target))
        << endl;
#endif
    (*_op)(this->_target); // do to the target what we do to our copy
    // cerr << " target is now " << int(*(this->_target)) << endl;

    // (*_op)(&(this->_mydiff));
    switch(this->_w)
    {
        case t_inc: this->_mydiff++; break;
        case t_dec: this->_mydiff--; break;
        default: w_assert1(0); break;
    }
    // cerr << " mydiff is now " << int(this->_mydiff) << endl;
};

template <class T, class T2>
class two_op_thread_t : public worker_thread_t<T>
{
public:
    typedef void (*op_t)(volatile T *, T2); 
    // TODO: what about ptr
    typedef T (*opnv_t)(volatile T *,  T2);
private:
    op_t    _op;
    opnv_t  _opnv;
public:
    static T no_op_nv(volatile T *,  T2) { return T(0); }
    two_op_thread_t (
            which_t w, op_t op1, opnv_t op2,
            int i, int n, T val, 
            target_t t, T* targ
            ) 
        :
        worker_thread_t<T>(w, i,n,val,t, targ) ,
        _op(op1), _opnv(op2)
    {
    }

    virtual void doit();
    virtual void getresult(T& net);
};
// template <class T, class T2>
// T two_op_thread_t<T,T2>::no_op_nv(volatile *T, T2) { return T(0); }

template <class T, class T2>
void two_op_thread_t<T,T2>::doit()
{
    (*_op)(this->_target, this->_operand);
    // (*_op)(&(this->_mydiff), this->_operand);
    switch(this->_w)
    {
        case t_and: this->_mydiff &= this->_operand; break;
        case t_or: this->_mydiff |=  this->_operand; break;
        case t_add: this->_mydiff += this->_operand; break;
        default: w_assert1(0); break;
    }
};

template <class T, class T2>
void two_op_thread_t<T,T2>::getresult (T&net)
{ 
    // and, or, add
    if(this->_w == t_add) { net += this->_mydiff; }
    else if (this->_w== t_or ) { net |= this->_mydiff; }
    else { w_assert1(this->_w == t_and); net &= this->_mydiff; }
}

template <class T>
class three_op_thread_t : public worker_thread_t<T>
{
public:
    typedef T (*op_t)(volatile T *, T, T); 
    // TODO: what about ptr
    typedef T (*opnv_t)(volatile T *,  T, T);
private:
    op_t    _op;
    opnv_t  _opnv;
    T   _match;
public:
    three_op_thread_t (
            which_t w, 
            op_t op1, 
            // opnv_t op2,
            int i, int n, 
            T  val,
            target_t t, 
            T* targ,
            T  m
            ) 
        :
        worker_thread_t<T>(w, i,n,val,t, targ) ,
        _op(op1), 
        // _opnv(op2), 
        _match(m)
    {
    }

    virtual void doit();
    virtual void getresult(T& net);
};

template <class T>
void three_op_thread_t<T>::doit()
{
    (*_op)(this->_target, _match, this->_operand);
    // (*_op)(&(this->_mydiff), _match, this->_operand);
    switch(this->_w)
    {
        case t_cas: {
                if(this->_mydiff==_match) 
                   this->_mydiff = this->_operand; 
                }
               break;
        default: w_assert1(0); break;
    }
};

template <class T>
void three_op_thread_t<T>::getresult (T&net)
{ 
    // This one's a bit harder to test
    w_assert1(this->_w == t_cas); 
    net = this->_mydiff;
}



int        NumDecThreads = 0;
int        NumIncThreads = 0;
int        NumAddThreads = 0;

int        NumSetThreads = 0; // TODO: not impl yet
int        NumSwapThreads = 0;
int        NumClearThreads = 0; // TODO not impl yet
// TODO: finish testing :
int        NumAndThreads = 0;
int        NumOrThreads = 0;
int        NumCasThreads = 0;
target_t TargetType(t_32); //default

bool        verbose = false;
int        parse_args(int, char **);


/**********************************************************/

// return 0 if ok, error if error
int    parse_type(char *arg)
{
    switch (arg[0])
    {
        case '8':
            TargetType = t_8;
            return strcmp(arg, "8");

        case '1':
            TargetType = t_16;
            return strcmp(arg, "16");

        case '3':
            TargetType = t_32;
            return strcmp(arg, "32");

        case '6':
            TargetType = t_64;
            return strcmp(arg, "64");

        case 'u':
            if ( !strcmp(arg, "uint") ) {
                TargetType = t_uint;
                return 0;
            }
            if ( !strcmp(arg, "uchar") ) {
                TargetType = t_uchar;
                return 0;
            }
            if ( !strcmp(arg, "ushort") ) {
                TargetType = t_ushort;
                return 0;
            }
            if ( !strcmp(arg, "ulong") ) {
                TargetType = t_ulong;
                return 0;
            }
            return 1;

        case 'i':
            TargetType = t_int;
            return strcmp(arg, "int");
            break;

        default:
            return 1;
    }
    return 0;
}

void usage(char **argv) 
{
    cerr << "usage: " << argv[0]
            << " [-i #increment-threads : default 1]"
            << endl
            << " [-d #decrement-threads : default 1]"
            << endl
            << " [-a #add-threads : default 1]"
            << endl
            << " [-O #Or-threads : default 0]"
            << endl
            << " [-A #And-threads : default 0]"
            << endl
            << " [-c #cas-threads : default 0]"
            << endl
            << " [-s #swap-threads : default 0]"
            << endl
            << " [-C #clear-threads : default 0]"
            << endl
            << " [-S #set-threads : default 0]"
            << endl
    << " [-t {8 | uchar | 16 | ushort | 32 | uint | 64 | ulong | int} : default int]"
            << endl
            << " [-v (verbose : default off)]"
            << endl;
}

int        parse_args(int argc, char **argv)
{
        int        c;
        int        errors = 0;

        /*
         * -i increment threads  <# threads>
         * -d decrement threads   <# threads>
         * -a add threads   <# threads>
         * -O or threads   <# threads>
         * -A and threads   <# threads>
         * -c cas threads   <# threads>
         * -s swap threads   <# threads>
         * -S set threads   <# threads>
         * -C clear threads   <# threads>
         *  membar*
         *
         *  -t <type> = 8, uchar, 16, ushort, 32, uint, ulong, 64
         */
        while ((c = getopt(argc, argv, "a:A:c:C:d:i:O:s:S:t:v")) != EOF) {
                switch (c) {
                case 't':
                        if(parse_type(optarg)) errors++;
                            break;
                case 'a':
                        NumAddThreads = atoi(optarg);
                        break;
                case 'A':
                        NumAndThreads = atoi(optarg);
                        break;
                case 'c':
                        NumCasThreads = atoi(optarg);
                        break;
                case 'C':
                        NumClearThreads = atoi(optarg);
                        cerr << " Clear threads are not implemented yet " << endl;
                        break;
                case 'O':
                        NumOrThreads = atoi(optarg);
                        break;
                case 's':
                        NumSwapThreads = atoi(optarg);
                        break;
                case 'S':
                        NumSetThreads = atoi(optarg);
                        cerr << " Set threads are not implemented yet " << endl;
                        break;
                case 'd':
                        NumDecThreads = atoi(optarg);
                        break;
                case 'i':
                        NumIncThreads = atoi(optarg);
                        break;
                case 'v':
                        verbose = true;
                        break;
                default:
                        errors++;
                        break;
                }
        }
        if (errors) {
            usage(argv);
        }
        return errors ? -1 : optind;
}

void
add_one_op_threads(which_t w, 
        int &i, int num, target_t tt, threadptr *workers)
{
    if(num < 1) return;

#define WORKER(w,P,X,T) \
    new one_op_thread_t<T>( w, \
            (one_op_thread_t<T>::op_t) atomic_##P##_##X,\
            (one_op_thread_t<T>::opnv_t) atomic_##P##_##X##_nv, \
            i, /* id */\
            ITERATIONS, /* # iterations */ \
            T(0), /* value to add */ \
            t_##X, /* target type, just for debugging */ \
            &t##X /* target */); 

    int error=0;
    for(int j=0; j < num; j++, i++)
    {
        switch(tt) {
        case t_8:
            switch(w) {
                case t_dec:
                    workers[i] =  WORKER(w,dec,8,uint8_t);
                    break;
                case t_inc:
                    workers[i] =  WORKER(w,inc,8,uint8_t);
                    break;
                default:
                    error++;
            }
            break;

        case t_16:
            switch(w) {
                case t_dec:
                    workers[i] =  WORKER(w,dec,16,uint16_t);
                    break;
                case t_inc:
                    workers[i] =  WORKER(w,inc,16,uint16_t);
                    break;
                default:
                    error++;
            }
            break;

        case t_32:
            switch(w) {
                case t_dec:
                    workers[i] =  WORKER(w,dec,32,uint32_t);
                    break;
                case t_inc:
                    workers[i] =  WORKER(w,inc,32,uint32_t);
                    break;
                default:
                    error++;
            }
            break;

        case t_64:
            switch(w) {
                case t_dec:
                    workers[i] =  WORKER(w,dec,64,uint64_t);
                    break;
                case t_inc:
                    workers[i] =  WORKER(w,inc,64,uint64_t);
                    break;
                default:
                    error++;
            }
            break;

        case t_long:
        case t_ulong:
            switch(w) {
                case t_dec:
                    workers[i] =  WORKER(w,dec,ulong,ulong_t);
                    break;
                case t_inc:
                    workers[i] =  WORKER(w,inc,ulong,ulong_t);
                    break;
                default:
                    error++;
            }
            break;

        case t_short:
        case t_ushort:
            switch(w) {
                case t_dec:
                    workers[i] =  WORKER(w,dec,ushort,ushort_t);
                    break;
                case t_inc:
                    workers[i] =  WORKER(w,inc,ushort,ushort_t);
                    break;
                default:
                    error++;
            }
            break;

        case t_char:
        case t_uchar:
            switch(w) {
                case t_dec:
                    workers[i] =  WORKER(w,dec,uchar,uchar_t);
                    break;
                case t_inc:
                    workers[i] =  WORKER(w,inc,uchar,uchar_t);
                    break;
                default:
                    error++;
            }
            break;

        case t_int:
        case t_uint:
            switch(w) {
                case t_dec:
                    workers[i] =  WORKER(w,dec,uint,uint_t);
                    break;
                case t_inc:
                    workers[i] =  WORKER(w,inc,uint,uint_t);
                    break;
                default:
                    error++;
            }
            break;

        default:
            error++;
            break;
        };
        if(error>0) {
            cerr 
                << "For threads of op "  << w
                << " target type " << tt
                << " isn't supported."
                << endl;
                ::exit(1);
        }
    }
#undef WORKER
}


void
add_two_op_threads(which_t w, 
        int &i, int num, target_t tt, threadptr *workers)
{
    if(num < 1) return;

    // t_add, t_or, t_and, t_swap, t_set, t_clear
    //
        // TODO: no set, clear yet ***********************************
        // TODO: no swap yet ***********************************

#define WORKER(w,P,X,T1,T2) \
    new two_op_thread_t<T1, T2>( w, \
            (two_op_thread_t<T1,T2>::op_t)atomic_##P##_##X,\
            (two_op_thread_t<T1,T2>::opnv_t)atomic_##P##_##X##_nv, \
            i, /* id */\
            ITERATIONS, /* # iterations */ \
            T2(0xabcd01), /* value to add/and/or */ \
            t_##X, /* target type, just for debugging */ \
            &t##X /* target */); 

#define WORKER_NONV(w,P,X,T1,T2) \
    new two_op_thread_t<T1, T2>( w, \
            (two_op_thread_t<T1,T2>::op_t)atomic_##P##_##X,\
            two_op_thread_t<T1,T2>::no_op_nv, \
            i, /* id */\
            ITERATIONS, /* # iterations */ \
            T2(0xabcd01), /* value to add/and/or */ \
            t_##X, /* target type, just for debugging */ \
            &t##X /* target */); 

        // TODO: no set, clear yet
        // TODO: no swap yet
        // TODO: no and/or -- have to figure out how to reconcile the
        // results; 
        //
        //
#define CASE(P,X,T1,T2) case t_##P: workers[i]=WORKER(w,P,X,T1,T2); break
#define CASEN(P,X,T1,T2) case t_##P: workers[i]=WORKER_NONV(w,P,X,T1,T2); break

    int error=0;
    for(int j=0; j < num; j++, i++)
    {
        switch(tt) {
        case t_8:
            switch(w) {
                CASE(add,8,uint8_t,int8_t);
                CASE(or,8,uint8_t,uint8_t);
                CASE(and,8,uint8_t,uint8_t);
                // CASEN(swap,8,uint8_t,uint8_t);
                default:
                    error++;
            }
            break;

        case t_16:
            switch(w) {
                CASE(add,16,uint16_t,int16_t);
                CASE(or,16,uint16_t,uint16_t);
                CASE(and,16,uint16_t,uint16_t);
                // CASEN(swap,16,uint16_t,uint16_t);
                default:
                    error++;
            }
            break;

        case t_32:
            switch(w) {
                CASE(add,32,uint32_t,int32_t);
                CASE(or,32,uint32_t,uint32_t);
                CASE(and,32,uint32_t,uint32_t);
                // CASEN(swap,32,uint32_t,uint32_t);
                default:
                    error++;
            }
            break;

        case t_64:
            switch(w) {
                CASE(add,64,uint64_t,int64_t);
                CASE(or,64,uint64_t,uint64_t);
                CASE(and,64,uint64_t,uint64_t);
                // CASEN(swap,64,uint64_t,uint64_t);
                default:
                    error++;
            }
            break;

        case t_int:
        case t_uint:
            switch(w) {
                CASE(add,int,uint_t,int);
                CASE(or,uint,uint_t,uint_t);
                CASE(and,uint,uint_t,uint_t);
                // CASEN(swap,uint,uint_t,uint_t);
                default:
                    error++;
            }
            break;

        case t_long:
        case t_ulong:
            switch(w) {
                CASE(add,long,ulong_t,long);
                CASE(or,ulong,ulong_t,ulong_t);
                CASE(and,ulong,ulong_t,ulong_t);
                // CASEN(swap,ulong,ulong_t,ulong_t);
                default:
                    error++;
            }
            break;

        case t_short:
        case t_ushort:
            switch(w) {
                CASE(add,short,ushort_t,short);
                CASE(or,ushort,ushort_t,ushort_t);
                CASE(and,ushort,ushort_t,ushort_t);
                // CASEN(swap,ushort,ushort_t,ushort_t);
                default:
                    error++;
            }
            break;

        case t_char:
        case t_uchar:
            switch(w) {
                CASE(add,char,unsigned char,char_t);
                CASE(or,uchar,uchar_t,uchar_t);
                CASE(and,uchar,uchar_t,uchar_t);
                // CASEN(swap,uchar,uchar_t,uchar_t);
                default:
                    error++;
            }
            break;

        default:
            error++;
        };
        if(error>0) {
            cerr 
                << "For threads of op "  << w
                << " target type " << tt
                << " isn't supported."
                << endl;
                ::exit(1);
        }
    }
#undef WORKER
}

void
add_three_op_threads(which_t w, 
        int &i, int num, target_t tt, threadptr *workers)
{
    if(num < 1) return;
    // t_cas

#define WORKER(w,P,X,T1) \
    new three_op_thread_t<T1>( w, \
            (three_op_thread_t<T1>::op_t)atomic_##P##_##X,\
            i, /* id */ \
            ITERATIONS, /* # iterations */ \
            T1(1), /* value swap in */ \
            t_##X, /* target type, just for debugging */ \
            &t##X, /* target */ \
            T1(45) /* value to match */ \
            ); 

    for(int j=0; j < num; j++, i++)
    {
        switch(tt) {
        case t_8:
            workers[i] =  WORKER(w,cas,8,uint8_t);
            break;

        case t_16:
            workers[i] =  WORKER(w,cas,16,uint16_t);
            break;

        case t_32:
            workers[i] =  WORKER(w,cas,32,uint32_t);
            break;

        case t_64:
            workers[i] =  WORKER(w,cas,64,uint64_t);
            break;

        case t_ptr:
            // Note: the declaration of atomic_cas_ptr seems to be out of
            // sync with the rest. the first arg should be a volatile
            // (void *) * or void*volatile*
            workers[i] =  /*WORKER(w,cas,ptr,ptr_t);*/
                    new three_op_thread_t<ptr_t>( w, 
                        three_op_thread_t<ptr_t>::op_t(atomic_cas_ptr),
                            i, /* id */ 
                            ITERATIONS, /* # iterations */ 
                            ptr_t(0x4547), /* value swap in */
                            t_ptr, /* target type, just for debugging */ 
                            &tptr, /* target */ 
                            ptr_t(0xfffff) /* value to match */
                        ); 
            break;

        case t_int:
        case t_uint:
            workers[i] =  WORKER(w,cas,uint,uint_t);
            break;

        case t_long:
        case t_ulong:
            workers[i] =  WORKER(w,cas,ulong,ulong_t);
            break;

        case t_short:
        case t_ushort:
            workers[i] =  WORKER(w,cas,ushort,ushort_t);
            break;

        case t_char:
        case t_uchar:
            workers[i] =  WORKER(w,cas,uchar,uchar_t);
            break;

        default:
            cerr 
                << "For threads of op "  << w
                << " target type " << tt
                << " isn't supported."
                << endl;
            ::exit(1);
        };
    }
#undef WORKER
}

void
collect_threads(which_t w, 
        int &i, int num, target_t tt, threadptr *workers)
{
#if 0
     cerr << "collect threads w=" << which_strings[int(w)] << " i=" << i << " num="<< num
        << " target type=" << tt << endl;
#endif

#define COLLECT_WORK(X,T) {  \
     worker_base_t *p = workers[i]; \
     w_assert1(p); \
     worker_thread_t<T> *w=dynamic_cast< worker_thread_t<T> * >(p); \
     w_assert1(w); \
     w -> getresult(r##X); }

    int error=0;
    for(int j=0; j < num; j++, i++)
    {
        switch(tt) {
        case t_ptr:
            switch(w) {
                case t_cas:
                    COLLECT_WORK(ptr,ptr_t);
                    break;
                default:
                    error++;
            }
            break;

        case t_8:
            switch(w) {
                case t_dec:
                case t_inc:
                case t_add:
                case t_or:
                case t_cas:
                case t_and:
                    COLLECT_WORK(8,uint8_t);
                    break;
                default:
                    error++;
            }
            break;

        case t_16:
            switch(w) {
                case t_dec:
                case t_inc:
                case t_add:
                case t_or:
                case t_cas:
                case t_and:
                    COLLECT_WORK(16,uint16_t);
                    break;
                default:
                    error++;
            }
            break;

        case t_32:
            switch(w) {
                case t_dec:
                case t_inc:
                case t_add:
                case t_or:
                case t_cas:
                case t_and:
                    COLLECT_WORK(32,uint32_t);
                    break;
                default:
                    error++;
            }
            break;

        case t_64:
            switch(w) {
                case t_dec:
                case t_inc:
                case t_add:
                case t_or:
                case t_cas:
                case t_and:
                    COLLECT_WORK(64,uint64_t);
                    break;
                default:
                    error++;
            }
            break;

        case t_long:
        case t_ulong:
            switch(w) {
                case t_dec:
                case t_inc:
                case t_or:
                case t_cas:
                case t_and:
                    COLLECT_WORK(ulong,ulong_t);
                    break;
                case t_add:
                    COLLECT_WORK(long,ulong_t);
                    break;
                default:
                    error++;
            }
            break;

        case t_short:
        case t_ushort:
            switch(w) {
                case t_dec:
                case t_inc:
                case t_or:
                case t_cas:
                case t_and:
                    COLLECT_WORK(ushort,ushort_t);
                    break;
                case t_add:
                    COLLECT_WORK(short,ushort_t);
                    break;
                default:
                    error++;
            }
            break;

        case t_char:
        case t_uchar:
            switch(w) {
                case t_dec:
                case t_inc:
                case t_or:
                case t_cas:
                case t_and:
                    COLLECT_WORK(uchar,uchar_t);
                    break;
                case t_add:
                    COLLECT_WORK(char,uchar_t);
                    break;
                default:
                    error++;
            }
            break;

        case t_int:
        case t_uint:
            switch(w) {
                case t_dec:
                case t_inc:
                case t_or:
                case t_cas:
                case t_and:
                    COLLECT_WORK(uint,uint_t);
                    break;
                case t_add:
                    COLLECT_WORK(int,uint_t);
                    break;
                default:
                    error++;
            }
            break;

        default:
            error++;
            break;
        };
        if(error>0) {
            cerr 
                << "For threads of op "  << w
                << " target type " << tt
                << " isn't supported."
                << endl;
                ::exit(1);
        }
    }
#undef COLLECT_WORK
}

void
compare_work(target_t tt)
{
#define COMPARE_WORK(X,T) \
   if(t##X != r##X) { cerr << " mismatched results "  << " target=" << (T)(t##X) \
             << " expected " << (T)(r##X) << " target type " << #X << endl; \
            w_assert1(0); \
     } else {  cerr << (T)(b##X)  << " --> " << (T)(r##X) << endl; }

    switch(tt) {
    case t_ptr:
        // COLLECT_WORK(ptr,ptr_t);
        // NOT YET COMPARE_WORK(ptr,ptr_t);
        w_assert1(0);
        break;

    case t_8:
        // COLLECT_WORK(8,uint8_t);
        COMPARE_WORK(8,unsigned);
        break;

    case t_16:
        // COLLECT_WORK(16,uint16_t);
        COMPARE_WORK(16,unsigned);
        break;

    case t_32:
        // COLLECT_WORK(32,uint32_t);
        COMPARE_WORK(32,unsigned);
        break;

    case t_64:
        // COLLECT_WORK(64,uint64_t);
        COMPARE_WORK(64,long long);
        break;

    case t_ulong:
        // COLLECT_WORK(ulong,ulong_t);
        COMPARE_WORK(ulong,unsigned long long);
        break;

    case t_long:
        // COLLECT_WORK(long,ulong_t);
        // NOT YET COMPARE_WORK(long,unsigned long long);
        w_assert1(0);
        break;

    case t_ushort:
        // COLLECT_WORK(ushort,ushort_t);
        COMPARE_WORK(ushort,unsigned);
        break;

    case t_short:
        // COLLECT_WORK(short,ushort_t);
        // NOT YET COMPARE_WORK(short,unsigned);
        w_assert1(0);
        break;

    case t_uchar:
        // COLLECT_WORK(uchar,uchar_t);
        COMPARE_WORK(uchar,unsigned);
        break;
    case t_char:
        // COLLECT_WORK(char,uchar_t);
        // NOT YET COMPARE_WORK(char,unsigned);
        w_assert1(0);
        break;

    case t_uint:
        // COLLECT_WORK(uint,uint_t);
        COMPARE_WORK(uint,unsigned);
        break;
    case t_int:
        // COLLECT_WORK(int,uint_t);
        // NOT YET COMPARE_WORK(int,unsigned);
        w_assert1(0);
        break;

    default:
        w_assert1(0);
        break;
    };

#undef COMPARE_WORK
}

int main(int argc, char **argv)
{
    if (parse_args(argc, argv) == -1) return 1;

    int nthreads = NumDecThreads +
                NumIncThreads +
                NumSetThreads +
                NumSwapThreads +
                NumCasThreads +
                NumClearThreads +
                NumAndThreads +
                NumOrThreads +
                NumAddThreads ;
     cout << "Total number of threads: " << nthreads << endl;  

    {
        int e;
        if((e=pthread_barrier_init(&collect_barrier, NULL, nthreads+1))!= 0)  {
            cerr << " could not init barrier: " << strerror(e) << endl;
            w_assert1(0);
        }
        if((e=pthread_barrier_init(&done_barrier, NULL, nthreads+1))!= 0)  {
            cerr << " could not init barrier: " << strerror(e) << endl;
            w_assert1(0);
        }
    }
    threadptr *workers = new threadptr[nthreads];

    int i=0;

    w_assert1(i == 0);
    add_one_op_threads(t_dec, i, NumDecThreads, TargetType, workers);
    w_assert1(i == NumDecThreads);
    for(int j=0; j < NumDecThreads; j++) { w_assert1(workers[j] != NULL); }

    add_one_op_threads(t_inc, i, NumIncThreads, TargetType, workers);
    w_assert1(i == NumDecThreads+NumIncThreads);
    for(int j=0; j < i ; j++) { w_assert1(workers[j] != NULL); }

    add_two_op_threads(t_add, i, NumAddThreads, TargetType, workers);
    w_assert1(i == NumDecThreads+NumIncThreads+NumAddThreads);
    for(int j=0; j < i ; j++) { w_assert1(workers[j] != NULL); }

    add_two_op_threads(t_and, i, NumAndThreads, TargetType, workers);
    w_assert1(i == NumDecThreads+NumIncThreads+NumAddThreads+NumAndThreads);
    for(int j=0; j < i ; j++) { w_assert1(workers[j] != NULL); }

    add_two_op_threads(t_or, i, NumOrThreads, TargetType, workers);
    w_assert1(i == NumDecThreads+NumIncThreads+NumAddThreads+NumAndThreads+NumOrThreads);

    add_three_op_threads(t_cas, i, NumCasThreads, TargetType, workers);
    w_assert1(i == NumDecThreads+NumIncThreads+NumAddThreads+NumAndThreads+NumCasThreads);

    w_assert1(i == nthreads);
    for(int j=0; j < i ; j++) { w_assert1(workers[j] != NULL); }

    for(i=0; i<nthreads; i++)
    {
        workers[i]->fork();
    }

    pthread_barrier_wait(&collect_barrier);
    // cerr << "everyone is ready to collect, nthreads=" << nthreads << endl;

    i=0;
    collect_threads(t_dec, i, NumDecThreads, TargetType, workers);
    w_assert1(i == NumDecThreads);
    collect_threads(t_inc, i, NumIncThreads, TargetType, workers);
    w_assert1(i == NumDecThreads + NumIncThreads);
    collect_threads(t_add, i, NumAddThreads, TargetType, workers);
    w_assert1(i == NumDecThreads + NumIncThreads + NumAddThreads);
    collect_threads(t_and, i, NumAndThreads, TargetType, workers);
    w_assert1(i == NumDecThreads + NumIncThreads + NumAddThreads + NumAndThreads);
    collect_threads(t_or, i, NumOrThreads, TargetType, workers);
    w_assert1(i == NumDecThreads + NumIncThreads + NumAddThreads + NumAndThreads + NumOrThreads);
    collect_threads(t_cas, i, NumCasThreads, TargetType, workers);
    w_assert1(i == NumDecThreads + NumIncThreads + NumAddThreads + NumAndThreads + NumOrThreads + NumCasThreads);
    w_assert1(i == nthreads);

    pthread_barrier_wait(&done_barrier);
    // cerr << "everyone is done" << endl;

    compare_work(TargetType);

    for(i=0; i<nthreads; i++)
    {
        workers[i]->join();
    }

    delete [] workers;
    pthread_barrier_destroy(&collect_barrier);
    pthread_barrier_destroy(&done_barrier);
    return 0;
}
