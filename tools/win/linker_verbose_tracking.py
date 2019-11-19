# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This script parses the /verbose output from the VC++ linker and uses it to
explain why a particular object file is being linked in. It parses records
like these:

      Found "public: static void * __cdecl SkTLS::Get(void * (__cdecl*)(void)...
        Referenced in chrome_crash_reporter_client_win.obj
        Referenced in skia.lib(SkError.obj)
        Loaded skia.lib(SkTLS.obj)

and then uses the information to answer questions such as "why is SkTLS.obj
being linked in. In this case it was requested by SkError.obj, and the process
is then repeated for SkError.obj. It traces the dependency tree back to a file
that was specified on the command line. Typically that file is part of a
source_set, and if that source_set is causing unnecessary code and data to be
pulled in then changing it to a static_library may reduce the binary size. See
crrev.com/2556603002 for an example of a ~900 KB savings from such a change.

In other cases the source_set to static_library fix does not work because some
of the symbols are required, while others are pulling in unwanted object files.
In these cases it can be necessary to see what symbol is causing one object file
to reference another. Removing or moving the problematic symbol can fix the
problem. See crrev.com/2559063002 for an example of such a change.

In some cases a target needs to be a source_set in component builds (so that all
of its functions will be exported) but should be a static_library in
non-component builds. The BUILD.gn pattern for that is:

  if (is_component_build) {
    link_target_type = "source_set"
  } else {
    link_target_type = "static_library"
  }
  target(link_target_type, "filters") {

One complication is that there are sometimes multiple source files with the
same name, such as mime_util.cc, all creating mime_util.obj. The script takes
whatever search criteria you pass and looks for all .obj files that were loaded
that contain that sub-string. It will print the search list that it will use
before reporting on why all of these .obj files were loaded. For instance, the
initial output if mime_util.obj is specified will be something like this:

>python linker_verbose_tracking.py verbose.txt mime_util.obj
  Searching for [u'net.lib(mime_util.obj)', u'base.lib(mime_util.obj)']

If you want to restrict the search to just one of these .obj files then you can
give a fully specified name, like this:

>python linker_verbose_tracking.py verbose.txt base.lib(mime_util.obj)

Object file name matching is case sensitive.

Typical output when run on chrome_watcher.dll verbose link output is:

>python tools\win\linker_verbose_tracking.py verbose08.txt drop_data
Database loaded - 3844 xrefs found
Searching for common_sources.lib(drop_data.obj)
common_sources.lib(drop_data.obj).obj pulled in for symbol Metadata::Metadata...
        common.lib(content_message_generator.obj)

common.lib(content_message_generator.obj).obj pulled in for symbol ...
        Command-line obj file: url_loader.mojom.obj
"""

from __future__ import print_function

import io
import pdb
import re
import sys

def ParseVerbose(input_file):
  # This matches line like this:
  #   Referenced in skia.lib(SkError.obj)
  #   Referenced in cloud_print_helpers.obj
  #   Loaded libvpx.lib(vp9_encodemb.obj)
  # groups()[0] will be 'Referenced in ' or 'Loaded ' and groups()[1] will be
  # the fully qualified object-file name (including the .lib name if present).
  obj_match = re.compile('.*(Referenced in |Loaded )(.*)')

  # Prefix used for symbols that are found and therefore loaded:
  found_prefix = '      Found'

  # This dictionary is indexed by (fully specified) object file names and the
  # payload is the list of object file names that caused the object file that
  # is the key name to be pulled in.
  cross_refs = {}
  # This dictionary has the same index as cross_refs but its payload is the
  # simple that caused the object file to be pulled in.
  cross_refed_symbols = {}

  # None or a list of .obj files that referenced a symbol.
  references = None

  # When you redirect the linker output to a file from a command prompt the
  # result will be a utf-8 (or ASCII?) output file. However if you do the same
  # thing from PowerShell you get a utf-16 file. So, we need to handle both
  # options. Only the first BOM option (\xff\xfe) has been tested, but it seems
  # appropriate to handle the other as well.
  file_encoding = 'utf-8'
  with open(input_file) as file_handle:
    header = file_handle.read(2)
    if header == '\xff\xfe' or header == '\xfe\xff':
      file_encoding = 'utf-16'
  with io.open(input_file, encoding=file_encoding) as file_handle:
    for line in file_handle:
      if line.startswith(found_prefix):
        # Create a list to hold all of the references to this symbol which
        # caused the linker to load it.
        references = []
        # Grab the symbol name
        symbol = line[len(found_prefix):].strip()
        if symbol[0] == '"':
          # Strip off leading and trailing quotes if present.
          symbol = symbol[1:-1]
        continue
      # If we are looking for references to a symbol...
      if type(references) == type([]):
        sub_line = line.strip()
        match = obj_match.match(sub_line)
        if match:
          match_type, obj_name = match.groups()
          # See if the line is part of the list of places where this symbol was
          # referenced:
          if match_type == 'Referenced in ':
            if '.lib' in obj_name:
              # This indicates a match that is xxx.lib(yyy.obj), so a
              # referencing .obj file that was itself inside of a library.
              reference = obj_name
            else:
              # This indicates a match that is just a pure .obj file name
              # I think this means that the .obj file was specified on the
              # linker command line.
              reference = ('Command-line obj file: ' +
                           obj_name)
            references.append(reference)
          else:
            assert(match_type == 'Loaded ')
            if '.lib' in obj_name and '.obj' in obj_name:
              cross_refs[obj_name] = references
              cross_refed_symbols[obj_name] = symbol
            references = None
      if line.startswith('Finished pass 1'):
        # Stop now because the remaining 90% of the verbose output is
        # not of interest. Could probably use /VERBOSE:REF to trim out
        # boring information.
        break
  return cross_refs, cross_refed_symbols


def TrackObj(cross_refs, cross_refed_symbols, obj_name):
  # Keep track of which references we've already followed.
  tracked = {}

  # Initial set of object files that we are tracking.
  targets = []
  for key in cross_refs.keys():
    # Look for any object files that were pulled in that contain the name
    # passed on the command line.
    if obj_name in key:
      targets.append(key)
  if len(targets) == 0:
    targets.append(obj_name)
  # Print what we are searching for.
  if len(targets) == 1:
    print('Searching for %s' % targets[0])
  else:
    print('Searching for %s' % targets)
  printed = False
  # Follow the chain of references up to an arbitrary maximum level, which has
  # so far never been approached.
  for i in range(100):
    new_targets = {}
    for target in targets:
      if not target in tracked:
        tracked[target] = True
        if target in cross_refs.keys():
          symbol = cross_refed_symbols[target]
          printed = True
          print('%s.obj pulled in for symbol "%s" by' % (target, symbol))
          for ref in cross_refs[target]:
            print('\t%s' % ref)
            new_targets[ref] = True
    if len(new_targets) == 0:
      break
    print()
    targets = new_targets.keys()
  if not printed:
    print('No references to %s found. Directly specified in sources or a '
          'source_set?' % obj_name)


def main():
  if len(sys.argv) < 3:
    print(r'Usage: %s <verbose_output_file> <objfile>' % sys.argv[0])
    print(r'Sample: %s chrome_dll_verbose.txt SkTLS' % sys.argv[0])
    return 0
  cross_refs, cross_refed_symbols = ParseVerbose(sys.argv[1])
  print('Database loaded - %d xrefs found' % len(cross_refs))
  if not len(cross_refs):
    print('No data found to analyze. Exiting')
    return 0
  TrackObj(cross_refs, cross_refed_symbols, sys.argv[2])

if __name__ == '__main__':
  sys.exit(main())
