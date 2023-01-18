# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''The 'grit build' tool.
'''


import collections
import codecs
import filecmp
import getopt
import gzip
import os
import shutil
import sys

from grit import grd_reader
from grit import shortcuts
from grit import util
from grit.format import minifier
from grit.node import brotli_util
from grit.node import include
from grit.node import message
from grit.node import structure
from grit.tool import interface


# It would be cleaner to have each module register itself, but that would
# require importing all of them on every run of GRIT.
'''Map from <output> node types to modules under grit.format.'''
_format_modules = {
  'android': 'android_xml',
  'c_format': 'c_format',
  'chrome_messages_json': 'chrome_messages_json',
  'chrome_messages_json_gzip': 'chrome_messages_json',
  'data_package': 'data_pack',
  'policy_templates': 'policy_templates_json',
  'rc_all': 'rc',
  'rc_header': 'rc_header',
  'rc_nontranslateable': 'rc',
  'rc_translateable': 'rc',
  'resource_file_map_source': 'resource_map',
  'resource_map_header': 'resource_map',
  'resource_map_source': 'resource_map',
}

def GetFormatter(type):
  modulename = 'grit.format.' + _format_modules[type]
  __import__(modulename)
  module = sys.modules[modulename]
  try:
    return module.Format
  except AttributeError:
    return module.GetFormatter(type)


class RcBuilder(interface.Tool):
  '''A tool that builds RC files and resource header files for compilation.

Usage:  grit build [-o OUTPUTDIR] [-D NAME[=VAL]]*

All output options for this tool are specified in the input file (see
'grit help' for details on how to specify the input file - it is a global
option).

Options:

  -a FILE           Assert that the given file is an output. There can be
                    multiple "-a" flags listed for multiple outputs. If a "-a"
                    or "--assert-file-list" argument is present, then the list
                    of asserted files must match the output files or the tool
                    will fail. The use-case is for the build system to maintain
                    separate lists of output files and to catch errors if the
                    build system's list and the grit list are out-of-sync.

  --assert-file-list  Provide a file listing multiple asserted output files.
                    There is one file name per line. This acts like specifying
                    each file with "-a" on the command line, but without the
                    possibility of running into OS line-length limits for very
                    long lists.

  -o OUTPUTDIR      Specify what directory output paths are relative to.
                    Defaults to the current directory.

  -p FILE           Specify a file containing a pre-determined mapping from
                    resource names to resource ids which will be used to assign
                    resource ids to those resources. Resources not found in this
                    file will be assigned ids normally. The motivation is to run
                    your app's startup and have it dump the resources it loads,
                    and then pass these via this flag. This will pack startup
                    resources together, thus reducing paging while all other
                    resources are unperturbed. The file should have the format:
                      RESOURCE_ONE_NAME 123
                      RESOURCE_TWO_NAME 124

  -D NAME[=VAL]     Specify a C-preprocessor-like define NAME with optional
                    value VAL (defaults to 1) which will be used to control
                    conditional inclusion of resources.

  -E NAME=VALUE     Set environment variable NAME to VALUE (within grit).

  -f FIRSTIDSFILE   Path to a python file that specifies the first id of
                    value to use for resources.  A non-empty value here will
                    override the value specified in the <grit> node's
                    first_ids_file.

  -w ALLOWLISTFILE  Path to a file containing the string names of the
                    resources to include.  Anything not listed is dropped.

  -t PLATFORM       Specifies the platform the build is targeting; defaults
                    to the value of sys.platform. The value provided via this
                    flag should match what sys.platform would report for your
                    target platform; see grit.node.base.EvaluateCondition.

  --allowlist-support
                    Generate code to support extracting a resource allowlist
                    from executables.

  --write-only-new flag
                    If flag is non-0, write output files to a temporary file
                    first, and copy it to the real output only if the new file
                    is different from the old file.  This allows some build
                    systems to realize that dependent build steps might be
                    unnecessary, at the cost of comparing the output data at
                    grit time.

  --depend-on-stamp
                    If specified along with --depfile and --depdir, the depfile
                    generated will depend on a stampfile instead of the first
                    output in the input .grd file.

  --js-minifier     A command to run the Javascript minifier. If not set then
                    Javascript won't be minified. The command should read the
                    original Javascript from standard input, and output the
                    minified Javascript to standard output. A non-zero exit
                    status will be taken as indicating failure.

  --css-minifier    A command to run the CSS minifier. If not set then CSS won't
                    be minified. The command should read the original CSS from
                    standard input, and output the minified CSS to standard
                    output. A non-zero exit status will be taken as indicating
                    failure.

  --brotli          The full path to the brotli executable generated by
                    third_party/brotli/BUILD.gn, required if any entries use
                    compress="brotli".

Conditional inclusion of resources only affects the output of files which
control which resources get linked into a binary, e.g. it affects .rc files
meant for compilation but it does not affect resource header files (that define
IDs).  This helps ensure that values of IDs stay the same, that all messages
are exported to translation interchange files (e.g. XMB files), etc.
'''

  def ShortDescription(self):
    return 'A tool that builds RC files for compilation.'

  def Run(self, opts, args):
    brotli_util.SetBrotliCommand(None)
    os.environ['cwd'] = os.getcwd()
    self.output_directory = '.'
    first_ids_file = None
    predetermined_ids_file = None
    allowlist_filenames = []
    assert_output_files = []
    target_platform = None
    depfile = None
    depdir = None
    allowlist_support = False
    write_only_new = False
    depend_on_stamp = False
    js_minifier = None
    css_minifier = None
    replace_ellipsis = True
    (own_opts, args) = getopt.getopt(
        args, 'a:p:o:D:E:f:w:t:',
        ('depdir=', 'depfile=', 'assert-file-list=', 'help',
         'output-all-resource-defines', 'no-output-all-resource-defines',
         'no-replace-ellipsis', 'depend-on-stamp', 'js-minifier=',
         'css-minifier=', 'write-only-new=', 'allowlist-support', 'brotli='))
    for (key, val) in own_opts:
      if key == '-a':
        assert_output_files.append(val)
      elif key == '--assert-file-list':
        with open(val) as f:
          assert_output_files += f.read().splitlines()
      elif key == '-o':
        self.output_directory = val
      elif key == '-D':
        name, val = util.ParseDefine(val)
        self.defines[name] = val
      elif key == '-E':
        (env_name, env_value) = val.split('=', 1)
        os.environ[env_name] = env_value
      elif key == '-f':
        # TODO(joi@chromium.org): Remove this override once change
        # lands in WebKit.grd to specify the first_ids_file in the
        # .grd itself.
        first_ids_file = val
      elif key == '-w':
        allowlist_filenames.append(val)
      elif key == '--no-replace-ellipsis':
        replace_ellipsis = False
      elif key == '-p':
        predetermined_ids_file = val
      elif key == '-t':
        target_platform = val
      elif key == '--depdir':
        depdir = val
      elif key == '--depfile':
        depfile = val
      elif key == '--write-only-new':
        write_only_new = val != '0'
      elif key == '--depend-on-stamp':
        depend_on_stamp = True
      elif key == '--js-minifier':
        js_minifier = val
      elif key == '--css-minifier':
        css_minifier = val
      elif key == '--allowlist-support':
        allowlist_support = True
      elif key == '--brotli':
        brotli_util.SetBrotliCommand([os.path.abspath(val)])
      elif key == '--help':
        self.ShowUsage()
        sys.exit(0)

    if len(args):
      print('This tool takes no tool-specific arguments.')
      return 2
    self.SetOptions(opts)
    self.VerboseOut('Output directory: %s (absolute path: %s)\n' %
                    (self.output_directory,
                     os.path.abspath(self.output_directory)))

    if allowlist_filenames:
      self.allowlist_names = set()
      for allowlist_filename in allowlist_filenames:
        self.VerboseOut('Using allowlist: %s\n' % allowlist_filename)
        allowlist_contents = util.ReadFile(allowlist_filename, 'utf-8')
        self.allowlist_names.update(allowlist_contents.strip().split('\n'))

    if js_minifier:
      minifier.SetJsMinifier(js_minifier)

    if css_minifier:
      minifier.SetCssMinifier(css_minifier)

    self.write_only_new = write_only_new

    self.res = grd_reader.Parse(opts.input,
                                debug=opts.extra_verbose,
                                first_ids_file=first_ids_file,
                                predetermined_ids_file=predetermined_ids_file,
                                defines=self.defines,
                                target_platform=target_platform)

    # Set an output context so that conditionals can use defines during the
    # gathering stage; we use a dummy language here since we are not outputting
    # a specific language.
    self.res.SetOutputLanguage('en')
    self.res.SetAllowlistSupportEnabled(allowlist_support)
    self.res.RunGatherers()

    # Replace ... with the single-character version. http://crbug.com/621772
    if replace_ellipsis:
      for node in self.res:
        if isinstance(node, message.MessageNode):
          node.SetReplaceEllipsis(True)

    self.Process()

    if assert_output_files:
      if not self.CheckAssertedOutputFiles(assert_output_files):
        return 2

    if depfile and depdir:
      self.GenerateDepfile(depfile, depdir, first_ids_file, depend_on_stamp)

    return 0

  def __init__(self, defines=None):
    # Default file-creation function is codecs.open().  Only done to allow
    # overriding by unit test.
    self.fo_create = codecs.open

    # key/value pairs of C-preprocessor like defines that are used for
    # conditional output of resources
    self.defines = defines or {}

    # self.res is a fully-populated resource tree if Run()
    # has been called, otherwise None.
    self.res = None

    # The set of names that are allowlisted to actually be included in the
    # output.
    self.allowlist_names = None

    # Whether to compare outputs to their old contents before writing.
    self.write_only_new = False

  @staticmethod
  def AddAllowlistTags(start_node, allowlist_names):
    # Walk the tree of nodes added attributes for the nodes that shouldn't
    # be written into the target files (skip markers).
    for node in start_node:
      # Same trick data_pack.py uses to see what nodes actually result in
      # real items.
      if (isinstance(node, include.IncludeNode) or
          isinstance(node, message.MessageNode) or
          isinstance(node, structure.StructureNode)):
        text_ids = node.GetTextualIds()
        # Mark the item to be skipped if it wasn't in the allowlist.
        if text_ids and text_ids[0] not in allowlist_names:
          node.SetAllowlistMarkedAsSkip(True)

  @staticmethod
  def ProcessNode(node, output_node, outfile):
    '''Processes a node in-order, calling its formatter before and after
    recursing to its children.

    Args:
      node: grit.node.base.Node subclass
      output_node: grit.node.io.OutputNode
      outfile: open filehandle
    '''
    base_dir = util.dirname(output_node.GetOutputFilename())

    formatter = GetFormatter(output_node.GetType())
    formatted = formatter(node, output_node.GetLanguage(), output_dir=base_dir)
    # NB: Formatters may be generators or return lists.  The writelines API
    # accepts iterables as a shortcut to calling write directly.  That means
    # you can pass strings (iteration yields characters), but not bytes (as
    # iteration yields integers).  Python 2 worked due to its quirks with
    # bytes/string implementation, but Python 3 fails.  It's also a bit more
    # inefficient to call write once per character/byte.  Handle all of this
    # ourselves by calling write directly on strings/bytes before falling back
    # to writelines.
    if isinstance(formatted, ((str,), bytes)):
      outfile.write(formatted)
    else:
      outfile.writelines(formatted)
    if output_node.GetType() == 'data_package':
      with open(output_node.GetOutputFilename() + '.info', 'w') as infofile:
        if node.info:
          # We terminate with a newline so that when these files are
          # concatenated later we consistently terminate with a newline so
          # consumers can account for terminating newlines.
          infofile.writelines(['\n'.join(node.info), '\n'])

  @staticmethod
  def _EncodingForOutputType(output_type):
    # Microsoft's RC compiler can only deal with single-byte or double-byte
    # files (no UTF-8), so we make all RC files UTF-16 to support all
    # character sets.
    if output_type in ('rc_header', 'resource_file_map_source',
                       'resource_map_header', 'resource_map_source'):
      return 'cp1252'
    if output_type in ('android', 'c_format',  'plist', 'plist_strings', 'doc',
                       'json', 'android_policy', 'chrome_messages_json',
                       'chrome_messages_json_gzip', 'policy_templates'):
      return 'utf_8'
    # TODO(gfeher) modify here to set utf-8 encoding for admx/adml
    return 'utf_16'

  def Process(self):
    for output in self.res.GetOutputFiles():
      output.output_filename = os.path.abspath(os.path.join(
        self.output_directory, output.GetOutputFilename()))

    # If there are allowlisted names, tag the tree once up front, this way
    # while looping through the actual output, it is just an attribute check.
    if self.allowlist_names:
      self.AddAllowlistTags(self.res, self.allowlist_names)

    for output in self.res.GetOutputFiles():
      self.VerboseOut('Creating %s...' % output.GetOutputFilename())

      # Set the context, for conditional inclusion of resources
      self.res.SetOutputLanguage(output.GetLanguage())
      self.res.SetOutputContext(output.GetContext())
      self.res.SetFallbackToDefaultLayout(output.GetFallbackToDefaultLayout())
      self.res.SetDefines(self.defines)

      # Assign IDs only once to ensure that all outputs use the same IDs.
      if self.res.GetIdMap() is None:
        self.res.InitializeIds()

      # Make the output directory if it doesn't exist.
      self.MakeDirectoriesTo(output.GetOutputFilename())

      # Write the results to a temporary file and only overwrite the original
      # if the file changed.  This avoids unnecessary rebuilds.
      out_filename = output.GetOutputFilename()
      tmp_filename = out_filename + '.tmp'
      tmpfile = self.fo_create(tmp_filename, 'wb')

      output_type = output.GetType()
      if output_type != 'data_package':
        encoding = self._EncodingForOutputType(output_type)
        tmpfile = util.WrapOutputStream(tmpfile, encoding)

      # Iterate in-order through entire resource tree, calling formatters on
      # the entry into a node and on exit out of it.
      with tmpfile:
        self.ProcessNode(self.res, output, tmpfile)

      if output_type == 'chrome_messages_json_gzip':
        gz_filename = tmp_filename + '.gz'
        with open(tmp_filename, 'rb') as tmpfile, open(gz_filename, 'wb') as f:
          with gzip.GzipFile(filename='', mode='wb', fileobj=f, mtime=0) as fgz:
            shutil.copyfileobj(tmpfile, fgz)
        os.remove(tmp_filename)
        tmp_filename = gz_filename

      # Now copy from the temp file back to the real output, but on Windows,
      # only if the real output doesn't exist or the contents of the file
      # changed.  This prevents identical headers from being written and .cc
      # files from recompiling (which is painful on Windows).
      if not os.path.exists(out_filename):
        os.rename(tmp_filename, out_filename)
      else:
        # CHROMIUM SPECIFIC CHANGE.
        # This clashes with gyp + vstudio, which expect the output timestamp
        # to change on a rebuild, even if nothing has changed, so only do
        # it when opted in.
        if not self.write_only_new:
          write_file = True
        else:
          files_match = filecmp.cmp(out_filename, tmp_filename)
          write_file = not files_match
        if write_file:
          shutil.copy2(tmp_filename, out_filename)
        os.remove(tmp_filename)

      self.VerboseOut(' done.\n')

    # Print warnings if there are any duplicate shortcuts.
    warnings = shortcuts.GenerateDuplicateShortcutsWarnings(
        self.res.UberClique(), self.res.GetTcProject())
    if warnings:
      print('\n'.join(warnings))

    # Print out any fallback warnings, and missing translation errors, and
    # exit with an error code if there are missing translations in a non-pseudo
    # and non-official build.
    warnings = self.res.UberClique().MissingTranslationsReport()
    if warnings:
      self.VerboseOut(warnings)
    if self.res.UberClique().HasMissingTranslations():
      print(self.res.UberClique().missing_translations_)
      sys.exit(-1)


  def CheckAssertedOutputFiles(self, assert_output_files):
    '''Checks that the asserted output files are specified in the given list.

    Returns true if the asserted files are present. If they are not, returns
    False and prints the failure.
    '''
    # Compare the absolute path names, sorted.
    asserted = sorted([os.path.abspath(i) for i in assert_output_files])
    actual = sorted([
        os.path.abspath(os.path.join(self.output_directory,
                                     i.GetOutputFilename()))
        for i in self.res.GetOutputFiles()])

    if asserted != actual:
      missing = list(set(asserted) - set(actual))
      extra = list(set(actual) - set(asserted))
      duplicates = [
          path for path, count in collections.Counter(actual).items()
          if count > 1
      ]
      error = '''Asserted file list does not match.

Missing output files:
%s
Extra output files:
%s
Duplicate actual output files:
%s
'''
      print(error %
            ('\n'.join(missing), '\n'.join(extra), '\n'.join(duplicates)))
      return False
    return True


  def GenerateDepfile(self, depfile, depdir, first_ids_file, depend_on_stamp):
    '''Generate a depfile that contains the implicit dependencies of the input
    grd. The depfile will be in the same format as a makefile, and will contain
    references to files relative to |depdir|. It will be put in |depfile|.

    For example, supposing we have three files in a directory src/

    src/
      blah.grd    <- depends on input{1,2}.xtb
      input1.xtb
      input2.xtb

    and we run

      grit -i blah.grd -o ../out/gen \
           --depdir ../out \
           --depfile ../out/gen/blah.rd.d

    from the directory src/ we will generate a depfile ../out/gen/blah.grd.d
    that has the contents

      gen/blah.h: ../src/input1.xtb ../src/input2.xtb

    Where "gen/blah.h" is the first output (Ninja expects the .d file to list
    the first output in cases where there is more than one). If the flag
    --depend-on-stamp is specified, "gen/blah.rd.d.stamp" will be used that is
    'touched' whenever a new depfile is generated.

    Note that all paths in the depfile are relative to ../out, the depdir.
    '''
    depfile = os.path.abspath(depfile)
    depdir = os.path.abspath(depdir)
    infiles = self.res.GetInputFiles()

    # We want to trigger a rebuild if the first ids change.
    if first_ids_file is not None:
      infiles.append(first_ids_file)

    if (depend_on_stamp):
      output_file = depfile + ".stamp"
      # Touch the stamp file before generating the depfile.
      with open(output_file, 'a'):
        os.utime(output_file, None)
    else:
      # Get the first output file relative to the depdir.
      outputs = self.res.GetOutputFiles()
      output_file = os.path.join(self.output_directory,
                                 outputs[0].GetOutputFilename())

    output_file = os.path.relpath(output_file, depdir)
    # The path prefix to prepend to dependencies in the depfile.
    prefix = os.path.relpath(os.getcwd(), depdir)
    deps_text = ' '.join([os.path.join(prefix, i) for i in infiles])

    depfile_contents = output_file + ': ' + deps_text
    self.MakeDirectoriesTo(depfile)
    outfile = self.fo_create(depfile, 'w', encoding='utf-8')
    outfile.write(depfile_contents)

  @staticmethod
  def MakeDirectoriesTo(file):
    '''Creates directories necessary to contain |file|.'''
    dir = os.path.split(file)[0]
    if not os.path.exists(dir):
      os.makedirs(dir)
