/*<std-header orig-src='shore'>

 $Id: create_rec.cpp,v 1.1.2.4 2010/03/19 22:20:38 nhall Exp $

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
 * This program is a test of file scan and lid performance
 */

#include <w_stream.h>
#include <sys/types.h>
#include <cassert>

// This include brings in all header files needed for writing a VAs 
#include "sm_vas.h"

#include "w_getopt.h"
int cmdline_num_rec(-1); // trumps config options if set
int num_rec(0); // set by config options

ss_m* ssm = 0;

// shorten error code type name
typedef w_rc_t rc_t;

// this is implemented in options.cpp
w_rc_t init_config_options(option_group_t& options,
                        const char* prog_type,
                        int& argc, char** argv);

sm_config_info_t config_info;

struct file_info_t {
    static const char* key;
    stid_t         fid;
    rid_t       first_rid;
    int         num_rec;
    int         rec_size;
};
const char* file_info_t::key = "SCANFILE";

ostream &
operator << (ostream &o, const file_info_t &info)
{
    o << "key " << info.key
    << " fid " << info.fid
    << " first_rid " << info.first_rid
    << " num_rec " << info.num_rec
    << " rec_size " << info.rec_size ;
    return o;
}


typedef        smlevel_0::smksize_t        smksize_t;

/*
 * looks up file info in the root index
*/
w_rc_t
find_file_info(vid_t vid, stid_t root_iid, file_info_t &info)
{

    W_DO(ssm->begin_xct());

    bool        found;

    W_DO(ss_m::vol_root_index(vid, root_iid));

    smsize_t    info_len = sizeof(info);
#ifndef CAN_CREATE_ANONYMOUS_VEC_T
    const vec_t        key_vec_tmp(file_info_t::key, strlen(file_info_t::key));
    W_DO(ss_m::find_assoc(root_iid,
                          key_vec_tmp,
                          &info, info_len, found));
#else
    W_DO(ss_m::find_assoc(root_iid,
                      vec_t(file_info_t::key, strlen(file_info_t::key)),
                      &info, info_len, found));
#endif
    if (!found) {
        cerr << "No file information found" <<endl;
        return RC(fcASSERT);
    } else {
       cerr << __LINE__ << " found assoc "
                << file_info_t::key << " --> " << info << endl;
    }

    W_DO(ssm->commit_xct());

    return RCOK;
 
}

/*
 * This function either formats a new device and creates a
 * volume on it, or mounts an already existing device and
 * returns the ID of the volume on it.
 *
 * It's borrowed from elsewhere; it can handle mounting
 * an already existing device, even though in this main program
 * we don't ever do that.
 */

rc_t
do_work(const char* device_name, bool init_device,
                        smksize_t quota, lvid_t& lvid, int num_rec,
                        smsize_t rec_size, 
                        stid_t& fid,
                        rid_t& first_rid
                        )
{
    devid_t        devid;
    u_int        vol_cnt;
    rc_t         rc;

    stid_t root_iid;  // root index ID
    file_info_t info;

	vid_t vid(1);
    if (init_device) 
	{
        cout << "Formatting device: " << device_name 
             << " with a " << quota << "KB quota ..." << endl;
        W_DO(ssm->format_dev(device_name, quota, true));

        cout << "Mounting device: " << device_name  << endl;
        // mount the new device
        W_DO(ssm->mount_dev(device_name, vol_cnt, devid));

        cout << "Mounted device: " << device_name  
             << " volume count " << vol_cnt
             << " device " << devid
             << endl;

        // generate a volume ID for the new volume we are about to
        // create on the device
        cout << "Generating new lvid: " << endl;
        W_DO(ssm->generate_new_lvid(lvid));
        cout << "Generated lvid " << lvid <<  endl;

        // create the new volume 
        cout << "Creating a new volume on the device" << endl;
        cout << "    with a " << quota << "KB quota ..." << endl;
        cout << "    with local handle(phys volid) " << vid << endl;
        W_DO(ssm->create_vol(device_name, lvid, quota, false, vid));
        cout << "Created " <<  vid << endl;

        rec_size = config_info.max_small_rec; // minus a header

        // create and fill file to scan
        cout << "Creating a file with " << num_rec 
            << " records of size " << rec_size << endl;
        W_DO(ssm->begin_xct());

        W_DO(ssm->create_file(vid, info.fid, smlevel_3::t_regular));
        rid_t rid;

        rec_size -= align(sizeof(int));

/// each record will have its ordinal number in the header
/// and zeros for data 

        char* dummy = new char[rec_size];
        memset(dummy, '\0', rec_size);
        vec_t data(dummy, rec_size);

        {
            int i = rec_size;
            const vec_t hdr(&i, sizeof(i));;
            W_COERCE(ssm->create_rec(info.fid, hdr,
                                    rec_size, data, rid));
            if (i == 0) {
                info.first_rid = rid;
            }        
        }
        delete [] dummy;
        info.num_rec = num_rec;
        info.rec_size = rec_size;

        // record file info in the root index : this stores some
        // attributes of the file in general
        W_DO(ss_m::vol_root_index(vid, root_iid));

#ifndef CAN_CREATE_ANONYMOUS_VEC_T
        const vec_t key_vec_tmp(file_info_t::key, strlen(file_info_t::key));
        const vec_t info_vec_tmp(&info, sizeof(info));
        W_DO(ss_m::create_assoc(root_iid,
                                key_vec_tmp,
                                info_vec_tmp));
        cerr << __LINE__ << " Creating assoc "
                << file_info_t::key << " --> " << info << endl;
#else
        W_DO(ss_m::create_assoc(root_iid,
                vec_t(file_info_t::key, strlen(file_info_t::key)),
                vec_t(&info, sizeof(info))));
        cerr << __LINE__ << " Creating assoc "
                << file_info_t::key << " --> " << info << endl;
#endif
        W_DO(ssm->commit_xct());

    } else {
        cout << "Using already existing device: " << device_name << endl;
        // mount already existing device
        rc = ssm->mount_dev(device_name, vol_cnt, devid);
        if (rc.is_error()) {
            cerr << "Error: could not mount device: " << device_name << endl;
            cerr << "   Did you forget to run the server with -i the first time?" << endl;
            return rc;
        }
        
        // find ID of the volume on the device
        lvid_t* lvid_list;
        u_int   lvid_cnt;
        W_DO(ssm->list_volumes(device_name, lvid_list, lvid_cnt));
        if (lvid_cnt == 0) {
            cerr << "Error, device has no volumes" << endl;
            exit(1);
        }
        lvid = lvid_list[0];
        delete [] lvid_list;
    }


    file_info_t info2;
    W_COERCE(find_file_info(vid, root_iid, info2 ));

    if(init_device)
    {
        if(info.first_rid != info2.first_rid) {
            cerr << "first_rid : " << info.first_rid
            << " stored info has " << info2.first_rid << endl; 
            return RC(fcASSERT);
        }
        if(info.fid != info2.fid) {
            cerr << "fid : " << info.fid
            << " stored info has " << info2.fid << endl; 
            return RC(fcASSERT);
        }
    }
    first_rid = info2.first_rid;
    fid = info2.fid;

    return RCOK;
}


void
usage(option_group_t& options)
{
    cerr << "Usage: server [-h] [-i] -s p|l|s -l r|f [options]" << endl;
    cerr << "       -i initialize device/volume and create file with nrec records" << endl;
    cerr << "       -s scantype   use p(hysical) l(logical) or s(can_i)"  << endl;
    cerr << "       -l lock granularity r(record) or f(ile)" << endl;
    cerr << "Valid options are: " << endl;
    options.print_usage(true, cerr);
}

/* create an smthread based class for all sm-related work */
class smthread_user_t : public smthread_t {
        int        argc;
        char        **argv;
public:
        int        retval;

        smthread_user_t(int ac, char **av) 
                : smthread_t(t_regular, "smthread_user_t"),
                argc(ac), argv(av), retval(0) { }
        ~smthread_user_t()  {}
        void run();
};

/**\defgroup EGOPTIONS Example of setting up options.
 * This method creates configuration options, starts up
 * the storage manager,
 */
void smthread_user_t::run()
{
    option_t* opt_device_name = 0;
    option_t* opt_device_quota = 0;
    option_t* opt_num_rec = 0;
    option_t* opt_rec_size = 0;

    cout << "Processing configuration options ..." << endl;

	// Create an option group for my three options.
    const int option_level_cnt = 3; 
    option_group_t options(option_level_cnt);

    W_COERCE(options.add_option("device_name", "device/file name",
                         NULL, "device containg volume holding file to scan",
                         true, option_t::set_value_charstr,
                         opt_device_name));

    W_COERCE(options.add_option("device_quota", "# > 1000",
                         "2000", "quota for device",
                         false, option_t::set_value_long,
                         opt_device_quota));

	// Default number of records to create is 1.
    W_COERCE(options.add_option("num_rec", "# > 0",
                         "1", "number of records in file",
                         true, option_t::set_value_long,
                         opt_num_rec));

	// Default size of record is 100.
    W_COERCE(options.add_option("rec_size", "# > 0",
                         "100", "size for records",
                         true, option_t::set_value_long,
                         opt_rec_size));

    // Have the SSM add its options to my group.
    W_COERCE(ss_m::setup_options(&options));

    w_rc_t rc = init_config_options(options, "server", argc, argv);
    if (rc.is_error()) {
        usage(options);
        retval = 1;
        return;
    }


    // Process the command line: looking for the "-h" flag
    int option;
    while ((option = getopt(argc, argv, "h")) != -1) {
        switch (option) {
        case 'h' :
            usage(options);
            break;
        default:
            usage(options);
            retval = 1;
            return;
            break;
        }
    }

	// Now start a storage manager.
    cout << "Starting SSM and performing recovery ..." << endl;
    ssm = new ss_m();
    if (!ssm) {
        cerr << "Error: Out of memory for ss_m" << endl;
        retval = 1;
        return;
    }

    lvid_t lvid;  
    rid_t start_rid;
    stid_t fid;
    smksize_t quota = strtol(opt_device_quota->value(), 0, 0);
    num_rec = strtol(opt_num_rec->value(), 0, 0);
    smsize_t rec_size = strtol(opt_rec_size->value(), 0, 0);

    W_COERCE(ss_m::config_info(config_info));

	// Subroutine to set up the device and volume and
	// create the num_rec records of rec_size.
    rc = do_work(opt_device_name->value(), 
            true, quota, lvid, 
            num_rec, rec_size, fid, start_rid);

    if (rc.is_error()) {
        cerr << "could not set up device/volume due to: " << endl;
        cerr << rc << endl;
        delete ssm;
        rc = RCOK;   // force deletion of w_error_t info hanging off rc
                     // otherwise a leak for w_error_t will be reported
        retval = 1;
        if(rc.is_error()) 
            W_COERCE(rc); // avoid error not checked.
        return;
    }


	// Clean up and shut down
    cout << "\nShutting down SSM ..." << endl;
    delete ssm;

    cout << "Finished!" << endl;

    return;
}

// This was copied from file_scan so it has lots of extra junk
int
main(int argc, char* argv[])
{
        smthread_user_t *smtu = new smthread_user_t(argc, argv);
        if (!smtu)
                W_FATAL(fcOUTOFMEMORY);

        w_rc_t e = smtu->fork();
        if(e.is_error()) {
            cerr << "error forking thread: " << e <<endl;
            return 1;
        }
        e = smtu->join();
        if(e.is_error()) {
            cerr << "error forking thread: " << e <<endl;
            return 1;
        }

        int        rv = smtu->retval;
        delete smtu;

        return rv;
}

