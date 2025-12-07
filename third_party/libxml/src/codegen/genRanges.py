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
import rangetab

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

# minTableSize gives the minimum number of ranges which must be present
# before a 256-byte lookup table is produced.  If there are less than this
# number, a macro with inline comparisons is generated
minTableSize = 6

# dictionary of functions, key=name, element contains char-map and range-list
Functs = {}

state = 0

try:
    defines = open("codegen/ranges.def", "r")
except:
    print("Missing codegen/ranges.def, aborting ...")
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

try:
    output = open("codegen/ranges.inc", "w")
except:
    print("Failed to open codegen/ranges.inc")
    sys.exit(1)

#
# Now output the generated data.
#

fkeys = sorted(Functs.keys())

for f in fkeys:

# First we convert the specified single-byte values into a group of ranges.
    if max(Functs[f][0]) > 0:   # only check if at least one entry
        rangeTable = makeRange(Functs[f][0])
        numRanges = len(rangeTable)
        if numRanges >= minTableSize:   # table is worthwhile
            # write the constant data to the code file
            output.write("const unsigned char %s_tab[256] = {\n" % f)
            pline = "   "
            for n in range(255):
                pline += " 0x%02x," % Functs[f][0][n]
                if len(pline) > 72:
                    output.write(pline + "\n")
                    pline = "   "
            output.write(pline + " 0x%02x };\n\n" % Functs[f][0][255])

#
# Next we do the unicode ranges
#

for f in fkeys:
    if len(Functs[f][1]) > 0:   # only generate if unicode ranges present
        rangeTable = Functs[f][1]
        rangeTable.sort()       # ascending tuple sequence
        group = rangetab.gen_range_tables(output, f, '_srng', '_lrng',
                                          rangeTable)

        output.write("const xmlChRangeGroup %sGroup =\n\t%s;\n\n" %
                     (f, group))

output.close()

