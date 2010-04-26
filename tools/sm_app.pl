#!/s/std/bin/perl -w

# <std-header style='perl' orig-src='shore'>
#
#  $Id: sm_app.pl,v 1.8 1999/06/07 19:09:15 kupsch Exp $
#
# SHORE -- Scalable Heterogeneous Object REpository
#
# Copyright (c) 1994-99 Computer Sciences Department, University of
#                       Wisconsin -- Madison
# All Rights Reserved.
#
# Permission to use, copy, modify and distribute this software and its
# documentation is hereby granted, provided that both the copyright
# notice and this permission notice appear in all copies of the
# software, derivative works or modified versions, and any portions
# thereof, and that both notices appear in supporting documentation.
#
# THE AUTHORS AND THE COMPUTER SCIENCES DEPARTMENT OF THE UNIVERSITY
# OF WISCONSIN - MADISON ALLOW FREE USE OF THIS SOFTWARE IN ITS
# "AS IS" CONDITION, AND THEY DISCLAIM ANY LIABILITY OF ANY KIND
# FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
#
# This software was developed with support by the Advanced Research
# Project Agency, ARPA order number 018 (formerly 8230), monitored by
# the U.S. Army Research Laboratory under contract DAAB07-91-C-Q518.
# Further funding for this work was provided by DARPA through
# Rome Research Laboratory Contract No. F30602-97-2-0247.
#
#   -- do not edit anything above this line --   </std-header>


# Usage: perl <this file>  <file-list>
#
# creates sm_app.h
#

$outfile = "sm_app.h";
open(TO, ">$outfile") || die "Cannot open $outfile.\n";

print TO "#ifndef SM_APP_H\n";
print TO "#define SM_APP_H\n";
print TO "/* THIS IS A GENERATED FILE -- DO NOT EDIT */\n";
print TO "#include <stddef.h>\n";

foreach $FILE (@ARGV) {
	# those with relative path names are searched
	# those without are included
	if ($FILE !~ /^\./) { 
		print TO "#include \"$FILE\"\n";
	}
}

foreach $FILE (@ARGV) {
	# those with relative path names are searched
	# those without are included
	if ($FILE =~ /^\./) {
		$copy = 0;
		open(FROM, "<$FILE") || die "Cannot open $FILE.\n";
		while (<FROM>) {
		    /BEGIN VISIBLE TO APP/ && do { $copy = 1;};
		    /END VISIBLE TO APP/ && do { $copy = 0; };
		    if($copy==1) {
			print TO $_;
		    }
		}
		close FROM;
	}
}

print TO "#endif /*SM_APP_H*/\n";
close TO;
