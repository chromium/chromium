#!/usr/bin/env python3
#
# Portions of this script have been (shamelessly) stolen from the
# prior work of Daniel Veillard (genUnicode.py)
#
# I, however, take full credit for any bugs, errors or difficulties :-)
#
# William Brack
# October 2003
#
# 18 October 2003
# Modified to maintain binary compatibility with previous library versions
# by adding a suffix 'Q' ('quick') to the macro generated for the original,
# function, and adding generation of a function (with the original name) which
# instantiates the macro.
#

import sys
import time

#
# A routine to take a list of yes/no (1, 0) values and turn it
# into a list of ranges.  This will later be used to determine whether
# to generate single-byte lookup tables, or inline comparisons
#
def makeRange(lst):
    ret = []
    pos = 0
    while pos < len(lst):
        try:            # index generates exception if not present
            s = lst[pos:].index(1)      # look for start of next range
        except:
            break                       # if no more, finished
        pos += s                        # pointer to start of possible range
        try:
            e = lst[pos:].index(0)      # look for end of range
            e += pos
        except:                         # if no end, set to end of list
            e = len(lst)
        ret.append((pos, e-1))          # append range tuple to list
        pos = e + 1                     # ready to check for next range
    return ret

sources = "chvalid.def"                 # input filename

# minTableSize gives the minimum number of ranges which must be present
# before a 256-byte lookup table is produced.  If there are less than this
# number, a macro with inline comparisons is generated
minTableSize = 6

# dictionary of functions, key=name, element contains char-map and range-list
Functs = {}

state = 0

try:
    defines = open("chvalid.def", "r")
except:
    print("Missing chvalid.def, aborting ...")
    sys.exit(1)

#
# The lines in the .def file have three types:-
#   name:   Defines a new function block
#   ur:     Defines individual or ranges of unicode values
#   end:    Indicates the end of the function block
#
# These lines are processed below.
#
for line in defines.readlines():
    # ignore blank lines, or lines beginning with '#'
    if line[0] == '#':
        continue
    line = line.strip()
    if line == '':
        continue
    # split line into space-separated fields, then split on type
    try:
        fields = line.split(' ')
        #
        # name line:
        #   validate any previous function block already ended
        #   validate this function not already defined
        #   initialize an entry in the function dicitonary
        #       including a mask table with no values yet defined
        #
        if fields[0] == 'name':
            name = fields[1]
            if state != 0:
                print("'name' %s found before previous name" \
                      "completed" % (fields[1]))
                continue
            state = 1
            if name in Functs:
                print("name '%s' already present - may give" \
                      " wrong results" % (name))
            else:
                # dict entry with two list elements (chdata, rangedata)
                Functs[name] = [ [], [] ]
                for v in range(256):
                    Functs[name][0].append(0)
        #
        # end line:
        #   validate there was a preceding function name line
        #   set state to show no current function active
        #
        elif fields[0] == 'end':
            if state == 0:
                print("'end' found outside of function block")
                continue
            state = 0

        #
        # ur line:
        #   validate function has been defined
        #   process remaining fields on the line, which may be either
        #       individual unicode values or ranges of values
        #
        elif fields[0] == 'ur':
            if state != 1:
                raise Exception("'ur' found outside of 'name' block")
            for el in fields[1:]:
                pos = el.find('..')
                # pos <=0 means not a range, so must be individual value
                if pos <= 0:
                    # cheap handling of hex or decimal values
                    if el[0:2] == '0x':
                        value = int(el[2:],16)
                    elif el[0] == "'":
                        value = ord(el[1])
                    else:
                        value = int(el)
                    if ((value < 0) | (value > 0x1fffff)):
                        raise Exception('Illegal value (%s) in ch for'\
                                ' name %s' % (el,name))
                    # for ur we have only ranges (makes things simpler),
                    # so convert val to range
                    currange = (value, value)
                # pos > 0 means this is a range, so isolate/validate
                # the interval
                else:
                    # split the range into it's first-val, last-val
                    (first, last) = el.split("..")
                    # convert values from text into binary
                    if first[0:2] == '0x':
                        start = int(first[2:],16)
                    elif first[0] == "'":
                        start = ord(first[1])
                    else:
                        start = int(first)
                    if last[0:2] == '0x':
                        end = int(last[2:],16)
                    elif last[0] == "'":
                        end = ord(last[1])
                    else:
                        end = int(last)
                    if (start < 0) | (end > 0x1fffff) | (start > end):
                        raise Exception("Invalid range '%s'" % el)
                    currange = (start, end)
                # common path - 'currange' has the range, now take care of it
                # We split on single-byte values vs. multibyte
                if currange[1] < 0x100: # single-byte
                    for ch in range(currange[0],currange[1]+1):
                        # validate that value not previously defined
                        if Functs[name][0][ch]:
                            msg = "Duplicate ch value '%s' for name '%s'" % (el, name)
                            raise Exception(msg)
                        Functs[name][0][ch] = 1
                else:                   # multi-byte
                    if currange in Functs[name][1]:
                        raise Exception("range already defined in" \
                                " function")
                    else:
                        Functs[name][1].append(currange)

    except:
        print("Failed to process line: %s" % (line))
        raise
#
# At this point, the entire definition file has been processed.  Now we
# enter the output phase, where we generate the two files chvalid.c and'
# chvalid.h
#
# To do this, we first output the 'static' data (heading, fixed
# definitions, etc.), then output the 'dynamic' data (the results
# of the above processing), and finally output closing 'static' data
# (e.g. the subroutine to process the ranges)
#

#
# Generate the headings:
#
try:
    header = open("include/libxml/chvalid.h", "w")
except:
    print("Failed to open include/libxml/chvalid.h")
    sys.exit(1)

try:
    output = open("chvalid.c", "w")
except:
    print("Failed to open chvalid.c")
    sys.exit(1)

date = time.asctime(time.localtime(time.time()))

header.write(
"""/*
 * Summary: Unicode character range checking
 * Description: this module exports interfaces for the character
 *               range validation APIs
 *
 * This file is automatically generated from the cvs source
 * definition files using the genChRanges.py Python script
 *
 * Generation date: %s
 * Sources: %s
 * Author: William Brack <wbrack@mmm.com.hk>
 */

#ifndef __XML_CHVALID_H__
#define __XML_CHVALID_H__

#include <libxml/xmlversion.h>
#include <libxml/xmlstring.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Define our typedefs and structures
 *
 */
typedef struct _xmlChSRange xmlChSRange;
typedef xmlChSRange *xmlChSRangePtr;
struct _xmlChSRange {
    unsigned short\tlow;
    unsigned short\thigh;
};

typedef struct _xmlChLRange xmlChLRange;
typedef xmlChLRange *xmlChLRangePtr;
struct _xmlChLRange {
    unsigned int\tlow;
    unsigned int\thigh;
};

typedef struct _xmlChRangeGroup xmlChRangeGroup;
typedef xmlChRangeGroup *xmlChRangeGroupPtr;
struct _xmlChRangeGroup {
    int\t\t\tnbShortRange;
    int\t\t\tnbLongRange;
    const xmlChSRange\t*shortRange;\t/* points to an array of ranges */
    const xmlChLRange\t*longRange;
};

/**
 * Range checking routine
 */
XMLPUBFUN int
\t\txmlCharInRange(unsigned int val, const xmlChRangeGroup *group);

""" % (date, sources));
output.write(
"""/*
 * chvalid.c:\tthis module implements the character range
 *\t\tvalidation APIs
 *
 * This file is automatically generated from the cvs source
 * definition files using the genChRanges.py Python script
 *
 * Generation date: %s
 * Sources: %s
 * William Brack <wbrack@mmm.com.hk>
 */

#define IN_LIBXML
#include "libxml.h"
#include <libxml/chvalid.h>

#include <stddef.h>

/*
 * The initial tables ({func_name}_tab) are used to validate whether a
 * single-byte character is within the specified group.  Each table
 * contains 256 bytes, with each byte representing one of the 256
 * possible characters.  If the table byte is set, the character is
 * allowed.
 *
 */
""" % (date, sources));

#
# Now output the generated data.
# We try to produce the best execution times.  Tests have shown that validation
# with direct table lookup is, when there are a "small" number of valid items,
# still not as fast as a sequence of inline compares.  So, if the single-byte
# portion of a range has a "small" number of ranges, we output a macro for inline
# compares, otherwise we output a 256-byte table and a macro to use it.
#

fkeys = sorted(Functs.keys())

for f in fkeys:

# First we convert the specified single-byte values into a group of ranges.
# If the total number of such ranges is less than minTableSize, we generate
# an inline macro for direct comparisons; if greater, we generate a lookup
# table.
    if max(Functs[f][0]) > 0:   # only check if at least one entry
        rangeTable = makeRange(Functs[f][0])
        numRanges = len(rangeTable)
        if numRanges >= minTableSize:   # table is worthwhile
            header.write("XMLPUBVAR const unsigned char %s_tab[256];\n" % f)
            header.write("""
/**
 * %s_ch:
 * @c: char to validate
 *
 * Automatically generated by genChRanges.py
 */
""" % f)
            header.write("#define %s_ch(c)\t(%s_tab[(c)])\n" % (f, f))

            # write the constant data to the code file
            output.write("const unsigned char %s_tab[256] = {\n" % f)
            pline = "   "
            for n in range(255):
                pline += " 0x%02x," % Functs[f][0][n]
                if len(pline) > 72:
                    output.write(pline + "\n")
                    pline = "   "
            output.write(pline + " 0x%02x };\n\n" % Functs[f][0][255])

        else:           # inline check is used
            # first another little optimisation - if space is present,
            # put it at the front of the list so it is checked first
            try:
                ix = rangeTable.remove((0x20, 0x20))
                rangeTable.insert(0, (0x20, 0x20))
            except:
                pass
            firstFlag = 1

            header.write("""
/**
 * %s_ch:
 * @c: char to validate
 *
 * Automatically generated by genChRanges.py
 */
""" % f)
            # okay, I'm tired of the messy lineup - let's automate it!
            pline = "#define %s_ch(c)" % f
            # 'ntab' is number of tabs needed to position to col. 33 from name end
            ntab = 4 - (len(pline)) // 8
            if ntab < 0:
                ntab = 0
            just = ""
            for i in range(ntab):
                just += "\t"
            pline = pline + just + "("
            for rg in rangeTable:
                if not firstFlag:
                    pline += " || \\\n\t\t\t\t "
                else:
                    firstFlag = 0
                if rg[0] == rg[1]:              # single value - check equal
                    pline += "((c) == 0x%x)" % rg[0]
                else:                           # value range
                # since we are doing char, also change range ending in 0xff
                    if rg[1] != 0xff:
                        pline += "((0x%x <= (c)) &&" % rg[0]
                        pline += " ((c) <= 0x%x))" % rg[1]
                    else:
                        pline += " (0x%x <= (c))" % rg[0]
            pline += ")\n"
            header.write(pline)

    header.write("""
/**
 * %sQ:
 * @c: char to validate
 *
 * Automatically generated by genChRanges.py
 */
""" % f)
    pline = "#define %sQ(c)" % f
    ntab = 4 - (len(pline)) // 8
    if ntab < 0:
        ntab = 0
    just = ""
    for i in range(ntab):
        just += "\t"
    header.write(pline + just + "(((c) < 0x100) ? \\\n\t\t\t\t ")
    if max(Functs[f][0]) > 0:
        header.write("%s_ch((c)) :" % f)
    else:
        header.write("0 :")

    # if no ranges defined, value invalid if >= 0x100
    numRanges = len(Functs[f][1])
    if numRanges == 0:
        header.write(" 0)\n\n")
    else:
        if numRanges >= minTableSize:
            header.write(" \\\n\t\t\t\t xmlCharInRange((c), &%sGroup))\n\n"  % f)
        else:           # if < minTableSize, generate inline code
            firstFlag = 1
            for rg in Functs[f][1]:
                if not firstFlag:
                    pline += " || \\\n\t\t\t\t "
                else:
                    firstFlag = 0
                    pline = "\\\n\t\t\t\t("
                if rg[0] == rg[1]:              # single value - check equal
                    pline += "((c) == 0x%x)" % rg[0]
                else:                           # value range
                    pline += "((0x%x <= (c)) &&" % rg[0]
                    pline += " ((c) <= 0x%x))" % rg[1]
            pline += "))\n\n"
            header.write(pline)


    if len(Functs[f][1]) > 0:
        header.write("XMLPUBVAR const xmlChRangeGroup %sGroup;\n" % f)


#
# Next we do the unicode ranges
#

for f in fkeys:
    if len(Functs[f][1]) > 0:   # only generate if unicode ranges present
        rangeTable = Functs[f][1]
        rangeTable.sort()       # ascending tuple sequence
        numShort = 0
        numLong  = 0
        for rg in rangeTable:
            if rg[1] < 0x10000: # if short value
                if numShort == 0:       # first occurrence
                    pline = "static const xmlChSRange %s_srng[] = {" % f
                else:
                    pline += ","
                numShort += 1
                if len(pline) > 60:
                    output.write(pline + "\n")
                    pline = "    "
                else:
                    pline += " "
                pline += "{0x%x, 0x%x}" % (rg[0], rg[1])
            else:               # if long value
                if numLong == 0:        # first occurrence
                    if numShort > 0:    # if there were shorts, finish them off
                        output.write(pline + "};\n")
                    pline = "static const xmlChLRange %s_lrng[] = { " % f
                else:
                    pline += ", "
                numLong += 1
                if len(pline) > 60:
                    output.write(pline + "\n")
                    pline = "    "
                pline += "{0x%x, 0x%x}" % (rg[0], rg[1])
        output.write(pline + "};\n")    # finish off last group

        pline = "const xmlChRangeGroup %sGroup =\n\t{%d, %d, " % (f, numShort, numLong)
        if numShort > 0:
            pline += "%s_srng" % f
        else:
            pline += "(xmlChSRangePtr)0"
        if numLong > 0:
            pline += ", %s_lrng" % f
        else:
            pline += ", (xmlChLRangePtr)0"

        output.write(pline + "};\n\n")

output.write(
"""
/**
 * xmlCharInRange:
 * @val: character to be validated
 * @rptr: pointer to range to be used to validate
 *
 * Does a binary search of the range table to determine if char
 * is valid
 *
 * Returns: true if character valid, false otherwise
 */
int
xmlCharInRange (unsigned int val, const xmlChRangeGroup *rptr) {
    int low, high, mid;
    const xmlChSRange *sptr;
    const xmlChLRange *lptr;

    if (rptr == NULL) return(0);
    if (val < 0x10000) {\t/* is val in 'short' or 'long'  array? */
\tif (rptr->nbShortRange == 0)
\t    return 0;
\tlow = 0;
\thigh = rptr->nbShortRange - 1;
\tsptr = rptr->shortRange;
\twhile (low <= high) {
\t    mid = (low + high) / 2;
\t    if ((unsigned short) val < sptr[mid].low) {
\t\thigh = mid - 1;
\t    } else {
\t\tif ((unsigned short) val > sptr[mid].high) {
\t\t    low = mid + 1;
\t\t} else {
\t\t    return 1;
\t\t}
\t    }
\t}
    } else {
\tif (rptr->nbLongRange == 0) {
\t    return 0;
\t}
\tlow = 0;
\thigh = rptr->nbLongRange - 1;
\tlptr = rptr->longRange;
\twhile (low <= high) {
\t    mid = (low + high) / 2;
\t    if (val < lptr[mid].low) {
\t\thigh = mid - 1;
\t    } else {
\t\tif (val > lptr[mid].high) {
\t\t    low = mid + 1;
\t\t} else {
\t\t    return 1;
\t\t}
\t    }
\t}
    }
    return 0;
}

""");

#
# finally, generate the ABI compatibility functions
#
for f in fkeys:
    output.write("""
/**
 * %s:
 * @ch:  character to validate
 *
 * This function is DEPRECATED.
""" % f);
    if max(Functs[f][0]) > 0:
        output.write(" * Use %s_ch or %sQ instead" % (f, f))
    else:
        output.write(" * Use %sQ instead" % f)
    output.write("""
 *
 * Returns true if argument valid, false otherwise
 */
""")
    output.write("int\n%s(unsigned int ch) {\n    return(%sQ(ch));\n}\n\n" % (f,f))
    header.write("XMLPUBFUN int\n\t\t%s(unsigned int ch);\n" % f);
#
# Run complete - write trailers and close the output files
#

header.write("""
#ifdef __cplusplus
}
#endif
#endif /* __XML_CHVALID_H__ */
""")

header.close()

output.close()

