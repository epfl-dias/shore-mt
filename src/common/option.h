/*<std-header orig-src='shore' incl-file-exclusion='OPTION_H'>

 $Id: option.h,v 1.30.2.4 2010/03/25 18:04:28 nhall Exp $

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

#ifndef OPTION_H
#define OPTION_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#include <w_stream.h>

#ifdef __GNUG__
#pragma interface
#endif

/**\defgroup OPTIONS Options Configuration 
 *
 * The options configuration facility consists of 3 classes: option_t,
 * option_group_t, and option_file_scan_t.  
 *
 * Objects of type option_t contain information about individual options.  
 * An option has a string name and value.  
 *
 * An option_group_t manages a related group of options.  
 *
 * An option_file_scan_t is used to parse a file  containing option 
 * configuration information.
 * An option_stream_scan_t does the same for an input stream.
 *
 * \todo examples of options usage: show the code, show the file contents,
 * using print_description and print_values
*/

#ifndef __opt_error_def_gen_h__
#include "opt_error_def_gen.h"
#endif

class option_group_t;

/**\brief  A single run-time option (e.g., from a .rc file). See \ref OPTIONS.
 * \ingroup OPTIONS
 *
 *\details 
 *   Information about an option is stored in a option_t object.
 *   All options have:
 *   - a string name, 
 *   - a description of the purpose of this option,
 *   - a string of possible values to print when giving for usage messages,
 *   - a Boolean indicating if the user is required to give a value for this,
 *   - a Boolean indicating whether this option was given a non-default value
 *   - a default value, used if this is not a "required" option.
 */
class option_t : public w_base_t {
friend class option_group_t;
public:

    /// Returns true if the option name matches matchName
    bool        match(const char* matchName, bool exact=false);

    /**\brief Set the value of an option if it is not already set or if overRide is true.
     *
     * Error messages will be "printed" to err_stream if 
     * the given pointer to err_stream is non-null.
     *
     * A value of NULL indicates "please un-set the value".
    */
    w_rc_t        set_value(const char* value, bool overRide, ostream* err_stream);

    /// Get current assigned value.
    const char*        value()         { return _value;}
    /// Has this option been given a non-default value?
    bool        is_set()        { return _set; }
    /// Is this option required to be given a non-default value?
    bool        is_required()        { return _required; }
    /// Return the string name of the option.
    const char*        name()                { return _name; }
    /// Return a string of the permissible values, for printing usage info.
    const char*        possible_values(){ return _possible_values; }        
    /// Return a string of the default value.
    const char*        default_value()        { return _default_value; }        
    /// Return a string describing the meaning of the option.
    const char*        description()        { return _description; }        

    w_rc_t        copyValue(const char* value);
    // Append to the value.
    w_rc_t        concatValue(const char* value);


    // Standard call back functions for basic types
    static w_rc_t set_value_bool(option_t* opt, const char* value, ostream* err_stream);
    static w_rc_t set_value_int4(option_t* opt, const char* value, ostream* err_stream);
    static w_rc_t set_value_int8(option_t* opt, const char* value, ostream* err_stream);
    static w_rc_t set_value_charstr(option_t* opt, const char* value, ostream* err_stream);

    /* Backwards compatability */
    static w_rc_t set_value_long(option_t* opt, const char* value, ostream* err_stream);
    static w_rc_t set_value_long_long(option_t* opt, const char* value, ostream* err_stream);

    // function to convert a string to a bool (similar to strtol()).
    // first character is checked for t,T,y,Y for true
    // and f,F,n,N for false.
    // bad_str is set to true if none of these match (and 
    // false will be returned)
    static bool        str_to_bool(const char* str, bool& bad_str);

private:
    // Type for "call back" functions called when a value is set
    typedef w_rc_t (*OptionSetFunc)(option_t*, const char * value,
                        ostream* err_stream);

    // These functions are called by option_group_t::add_option().
    NORET        option_t();
    NORET        ~option_t();
                    // initialize an option_t object
    w_rc_t        init(const char* name, const char* newPoss,
                     const char* default_value, const char* description,
                     bool required, OptionSetFunc callBack,
                     ostream *err_stream);

    const char*        _name;                        // name of the option
    const char*        _possible_values;        // example possible values
    const char*        _default_value;                // default value
    const char*        _description;                // description string
    bool        _required;                // must option be set
    bool        _set;                        // option has been set
    char*        _value;                        // value for the option
    w_link_t        _link;                        // link list of options 

    /*
     *      call-back function to call when option value is set
     *
     *      Call back functions should return 0 on success or a on-zero
     *      error code on failure.  This error code should not confict
     *      with the option error codes above.
     */
    OptionSetFunc _setFunc;

};

/**\brief Group of option_t.  See \ref OPTIONS.
 *
 * \ingroup OPTIONS
 * \details
 *   Manages a set of options.  
 *
 *   An option group has  a classification hierarchy associated with it.  
 *   Each level  of the hierarchy is given a string name.  
 *   Levels are added with add_class_level().  
 *   The levels are used when looking up an option with lookup_by_class().  
 *   The level hierarchy is printed  in the form: 
 *       "level1.level2.level3."  
 *   A complete option name is specified by 
 *      "level1.level2.level3.optionName:".  
 *   A convention for level names is:
 *        programtype.programname
 *   where programtype is indicates the general type of the program
 *   and programname is the file name of the program.
 */
class option_group_t : public w_base_t {
public:
    NORET        option_group_t(int max_class_levels);
    NORET         ~option_group_t();

    // Add_class_level is used to add a level name.
    w_rc_t        add_class_level(const char* name);

    /**\brief Add an option to this group.
     *
     * \details
     * Creates an option_t and adds it to this group.
     * The set_func parameter indicates what callback function to call
     * when the option is set.  Use one of the functions
     * from option_t or write your own.
     */
    w_rc_t        add_option(const char* name, 
                       const char* possible_values,
                       const char* default_value,
                       const char* description, 
                       bool required,
                       option_t::OptionSetFunc set_func,
                       option_t*& new_opt,
                       ostream *err_stream = &cerr
                       );

    /**\brief Look up an option by name.  
     *
     * \details
     * @param[in] name   Name of option.
     * @param[in] exact  name is not an abbreviation.
     * @param[out] ret  Populate the given pointer if found.
     *
     * Abbreviations are allowed if they are sufficient to identify the option
     * and exact is false.
     */
    w_rc_t        lookup(const char* name, bool exact, option_t*&ret);


    /**\brief Look up option by class name and option name.
     * \details
     * @param[in] opt_class_name is a string of the form level1.level2.optionname.
     * A "?" can be used as a wild card for any single level name.
     * A "*" can be used as a wild card for any number of level name.
     * @param[out] ret Pass in an option_t* and it will be filled in if found.
     * @param[in] exact  opt_class_name is not an abbreviation.
     *
     * Abbreviations are allowed if they are sufficient to identify the option
     * and exact is false.
    */
    w_rc_t        lookup_by_class(const char* opt_class_name, option_t*&ret,
                            bool exact=false);

    /**\brief Set a value of an option identified by a class name and option name.
     * \details
     * Set the value of an option if it is not already set
     * or if overRide is true.
     *
     * @param[in] name is a string of the form level1.level2.optionname.
     * @param[in] exact  name is not an abbreviation.
     * @param[in] value is a string form of the value to give the option;
     *            a value of NULL indicates un-set.
     * @param[in] overRide allows the value to be overwritten.
     * @param[in] err_stream: error messages will be sent to err_stream if non-null.
     */
    w_rc_t        set_value(const char* name, bool exact,
                          const char* value, bool overRide,
                          ostream* err_stream);

    /// Print the descriptive information to the given stream.
    void        print_usage(bool longForm, ostream& err_stream);
    /// Print the descriptive information to the given stream.
    void        print_values(bool longForm, ostream& err_stream);

    /**\brief Check that all required options are set.
     *
     * \details
     * Return OPTERR_NotSet if any are not.
     * Print information about each unset option to err_stream
     */
    w_rc_t         check_required(ostream* err_stream);

    /**\brief Search the command line for options, set, remove from argv,argc.
     * \details
     * @param[in] argv The command line to search.
     * @param[in] argc Number of entries in argv[].
     * @param[in] min_len Don't process argv entries shorter than this.
     * @param[out] err_stream Send error message here if non-NULL.
     *
     * Search the command line (argv[]) for options in this group.
     * Command-line options must be preceded by "-" 
     * and are recognized only if they are min_len since
     * options may be abbreviated.  
     * Any options found are removed from argv, 
     * and argc is adjusted accordingly.
     */
    w_rc_t         parse_command_line(const char** argv, 
                    int& argc, 
                    size_t min_len, 
                    ostream* err_stream);

    /// Return a list of the options in the group.
    w_list_t<option_t,unsafe_list_dummy_lock_t>& option_list() {return _options;}

    /// Number of levels in the class names.
    int                num_class_levels(){ return _numLevels; }        
    /// The complete option class name.
    const char*        class_name()        { return _class_name; }        

private:
    w_list_t<option_t,unsafe_list_dummy_lock_t>  _options;
    char*                _class_name;
    // array of offsets into _class_name
    char**                 _levelLocation;
    int                        _maxLevels;
    int                        _numLevels;

    static bool        _error_codes_added;

        // disable copy operator
        NORET option_group_t(option_group_t const &); 
};

/**\brief Enables scanning of options group. See \ref OPTIONS.
 *
 * \ingroup OPTIONS
 * \details
 *   
 * Given an instream, scan for options in a given option_troup_t
 * on that stream.
 */
class option_stream_scan_t : public w_base_t {
        istream                &_input;
        option_group_t        *_optList;
        char                  *_line;
        const char            *_label;
        int                   _lineNum;

        enum { _maxLineLen = 2000 };

        static const char *default_label;

public:
        option_stream_scan_t(istream &is, option_group_t *option_group);
        ~option_stream_scan_t();

        // Allow a label to be associated with the stream, e.g., file name.
        void        setLabel(const char *label);

        /**\brief Scan all options, report errors to err_stream.
         *
         * @param[in] over_ride If false, new values of options will be ignored.
         *                      If true, assignments to already-set options 
         *                      will be made.
         * @param[out] err_stream Send error message here if non-NULL.
         * @param[in] exact If true, misspellings or abbreviations of
         *                      option names in the instream will result
         *                      in errors. 
         * @param[in] mismatch_ok  If true, bad option names will be ignored
         *                      and will not cause failure of the entire scan.
         */
        w_rc_t         scan(bool over_ride, ostream& err_stream, 
                             bool exact=false, bool mismatch_ok=false);

};

/**\brief Scan a text file for options. See \ref OPTIONS.
 * \ingroup OPTIONS
 *
 * \details
 * 
 * Each line of the file is either a comment or an option value setting.
 * 
 * A comment line begins with "!" or "#".
 *
 * An option value setting line has the form:
 *     level1.level2.optionname: value of option
 *
 * level1.level2.optionname is anything acceptable to
 * option_group_t::lookup_by_class().  The value of the option
 * is the string beginning with the first non-white space character
 * after the ":" and ending with last non-white space character in
 * the line.
 */ 
class option_file_scan_t : public w_base_t {
public:
    NORET        option_file_scan_t(const char* opt_file_path, option_group_t* opt_group);
    NORET        ~option_file_scan_t();

        /**\brief Scan all options, report errors to err_stream.
         *
         * @param[in] over_ride If false, new values of options will be ignored.
         * If true, assignments to already-set options will be made.
         * @param[out] err_stream Send error message here if non-NULL.
         * @param[in] exact If true, misspellings or abbreviations of
         *                      option names in the instream will result
         *                      in errors. 
         * @param[in] mismatch_ok  If true, bad option names will be ignored
         *                      and will not cause failure of the entire scan.
         */
    w_rc_t         scan(bool over_ride, ostream& err_stream, 
        bool exact=false, bool mismatch_ok=false);

protected:
    const char*           _fileName;
    option_group_t        *_optList;
};

/*<std-footer incl-file-exclusion='OPTION_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
