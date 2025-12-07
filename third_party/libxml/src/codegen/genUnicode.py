#!/usr/bin/env python3
#
# Original script modified in November 2003 to take advantage of
# the character-validation range routines, and updated to the
# current Unicode information (Version 4.0.1)
#
# NOTE: there is an 'alias' facility for blocks which are not present in
#	the current release, but are needed for ABI compatibility.  This
#	must be accomplished MANUALLY!  Please see the comments below under
#     'blockAliases'
#
import sys
import string
import rangetab

#
# blockAliases is a small hack - it is used for mapping block names which
# were were used in the 3.1 release, but are missing or changed in the current
# release.  The format is "OldBlockName:NewBlockName1[,NewBlockName2[,...]]"
blockAliases = []
blockAliases.append("CombiningMarksforSymbols:CombiningDiacriticalMarksforSymbols")
blockAliases.append("Greek:GreekandCoptic")
blockAliases.append("PrivateUse:PrivateUseArea,SupplementaryPrivateUseArea-A," + 
	"SupplementaryPrivateUseArea-B")

# minTableSize gives the minimum number of ranges which must be present
# before a range table is produced.  If there are less than this
# number, inline comparisons are generated
minTableSize = 8

blockfile = "Blocks-4.0.1.txt"
catfile = "UnicodeData-4.0.1.txt"


#
# Now process the "blocks" file, reducing it to a dictionary
# indexed by blockname, containing a tuple with the applicable
# block range
#
BlockNames = {}
try:
    blocks = open(blockfile, "r")
except:
    print("Missing %s, aborting ..." % blockfile)
    sys.exit(1)

for line in blocks.readlines():
    if line[0] == '#':
        continue
    line = line.strip()
    if line == '':
        continue
    try:
        fields = line.split(';')
        range = fields[0].strip()
        (start, end) = range.split("..")
        name = fields[1].strip()
        name = name.replace(' ', '')
    except:
        print("Failed to process line: %s" % (line))
        continue
    start = int(start, 16)
    end = int(end, 16)
    try:
        BlockNames[name].append((start, end))
    except:
        BlockNames[name] = [(start, end)]
blocks.close()
print("Parsed %d blocks descriptions" % (len(BlockNames.keys())))

for block in blockAliases:
    alias = block.split(':')
    alist = alias[1].split(',')
    for comp in alist:
        if comp in BlockNames:
            if alias[0] not in BlockNames:
                BlockNames[alias[0]] = []
            for r in BlockNames[comp]:
                BlockNames[alias[0]].append(r)
        else:
            print("Alias %s: %s not in Blocks" % (alias[0], comp))
            continue

#
# Next process the Categories file. This is more complex, since
# the file is in code sequence, and we need to invert it.  We use
# a dictionary with index category-name, with each entry containing
# all the ranges (codepoints) of that category.  Note that category
# names comprise two parts - the general category, and the "subclass"
# within that category.  Therefore, both "general category" (which is
# the first character of the 2-character category-name) and the full
# (2-character) name are entered into this dictionary.
#
try:
    data = open(catfile, "r")
except:
    print("Missing %s, aborting ..." % catfile)
    sys.exit(1)

nbchar = 0;
Categories = {}
for line in data.readlines():
    if line[0] == '#':
        continue
    line = line.strip()
    if line == '':
        continue
    try:
        fields = line.split(';')
        point = fields[0].strip()
        value = 0
        while point != '':
            value = value * 16
            if point[0] >= '0' and point[0] <= '9':
                value = value + ord(point[0]) - ord('0')
            elif point[0] >= 'A' and point[0] <= 'F':
                value = value + 10 + ord(point[0]) - ord('A')
            elif point[0] >= 'a' and point[0] <= 'f':
                value = value + 10 + ord(point[0]) - ord('a')
            point = point[1:]
        name = fields[2]
    except:
        print("Failed to process line: %s" % (line))
        continue
    
    nbchar = nbchar + 1
    # update entry for "full name"
    try:
        Categories[name].append(value)
    except:
        try:
            Categories[name] = [value]
        except:
            print("Failed to process line: %s" % (line))
    # update "general category" name
    try:
        Categories[name[0]].append(value)
    except:
        try:
            Categories[name[0]] = [value]
        except:
            print("Failed to process line: %s" % (line))

data.close()
print("Parsed %d char generating %d categories" % (nbchar, len(Categories.keys())))

#
# The data is now all read.  Time to process it into a more useful form.
#
# reduce the number list into ranges
for cat in Categories.keys():
    list = Categories[cat]
    start = -1
    prev = -1
    end = -1
    ranges = []
    for val in list:
        if start == -1:
            start = val
            prev = val
            continue
        elif val == prev + 1:
            prev = val
            continue
        elif prev == start:
            ranges.append((prev, prev))
            start = val
            prev = val
            continue
        else:
            ranges.append((start, prev))
            start = val
            prev = val
            continue
    if prev == start:
        ranges.append((prev, prev))
    else:
        ranges.append((start, prev))
    Categories[cat] = ranges

#
# Assure all data is in alphabetic order, since we will be doing binary
# searches on the tables.
#
bkeys = sorted(BlockNames.keys())

ckeys = sorted(Categories.keys())

#
# Generate the resulting files
#
try:
    output = open("codegen/unicode.inc", "w")
except:
    print("Failed to open codegen/unicode.inc")
    sys.exit(1)

#
# For any categories with more than minTableSize ranges we generate
# a range table suitable for xmlCharInRange
#
for name in ckeys:
    if len(Categories[name]) <= minTableSize or name == 'Cs':
        continue
    ranges = Categories[name]
    group = rangetab.gen_range_tables(output, 'xml' + name, 'S', 'L', ranges)
    output.write("static const xmlChRangeGroup xml%sG = %s;\n\n" %
                 (name, group))

for name in ckeys:
    if name == 'Cs':
        continue
    ranges = Categories[name]
    output.write("static int\nxmlUCSIsCat%s(int code) {\n" % name)
    if len(Categories[name]) > minTableSize:
        output.write("    return(xmlCharInRange((unsigned int)code, &xml%sG)"
            % name)
    else:
        start = 1
        for range in ranges:
            (begin, end) = range;
            if start:
                output.write("    return(");
                start = 0
            else:
                output.write(" ||\n           ");
            if (begin == end):
                output.write("(code == %s)" % (hex(begin)))
            else:
                output.write("((code >= %s) && (code <= %s))" % (
                         hex(begin), hex(end)))
    output.write(");\n}\n\n")

#
# Range tables for blocks
#

blockGroups = ''
for block in bkeys:
    name = block.replace('-', '')
    ranges = BlockNames[block]
    group = rangetab.gen_range_tables(output, 'xml' + name, 'S', 'L', ranges)
    output.write("\n")
    if blockGroups != '':
        blockGroups += ",\n"
    blockGroups += '  {"%s",\n   %s}' % (block, group)

output.write("static const xmlUnicodeRange xmlUnicodeBlocks[] = {\n")
output.write(blockGroups)
output.write("\n};\n\n")

output.close()
