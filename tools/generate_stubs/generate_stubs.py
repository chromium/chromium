#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates Windows and POSIX stub files for a given set of signatures.

For libraries that need to be loaded outside of the standard executable startup
path mechanism, stub files need to be generated for the wanted functions.  In
Windows, this is done via "def" files and the delay load mechanism.  On a POSIX
system, a set of stub functions need to be generated that dispatch to functions
found via dlsym.

This script takes a set of files, where each file is a list of C-style
signatures (one signature per line).  The output is either a Windows def file,
or a header + implementation file of stubs suitable for use in a POSIX system.

This script also handles variadic functions, e.g.
void printf(const char* s, ...);

TODO(hclam): Fix the situation for variadic functions.
Stub for the above function will be generated and inside the stub function it
is translated to:
void printf(const char* s, ...) {
  printf_ptr(s, (void*)arg1);
}

Only one argument from the variadic arguments is used and it will be used as
type void*.
"""

__author__ = 'ajwong@chromium.org (Albert J. Wong)'

import optparse
import os
import re
import string
import subprocess
import sys


class Error(Exception):
  pass


class BadSignatureError(Error):
  pass


class SubprocessError(Error):

  def __init__(self, message, error_code):
    Error.__init__(self)
    self.message = message
    self.error_code = error_code

  def __str__(self):
    return 'Failed with code %s: %s' % (self.message, repr(self.error_code))


# Regular expression used to parse function signatures in the input files.
# The regex is built around identifying the "identifier" for the function name.
# We consider the identifier to be the string that follows these constraints:
#
#   1) Starts with [_a-ZA-Z] (C++ spec 2.10).
#   2) Continues with [_a-ZA-Z0-9] (C++ spec 2.10).
#   3) Precedes an opening parenthesis by 0 or more whitespace chars.
#
# From that, all preceding characters are considered the return value.
# Trailing characters should have a substring matching the form (.*).  That
# is considered the arguments.
SIGNATURE_REGEX = re.compile(r'(?P<return_type>.+?)'
                             r'(?P<name>[_a-zA-Z][_a-zA-Z0-9]+)\s*'
                             r'\((?P<params>.*?)\)')

# Used for generating C++ identifiers.
INVALID_C_IDENT_CHARS = re.compile(r'[^_a-zA-Z0-9]')

# Constants defining the supported file types options.
FILE_TYPE_WIN_X86 = 'windows_lib'
FILE_TYPE_WIN_X64 = 'windows_lib_x64'
FILE_TYPE_POSIX_STUB = 'posix_stubs'
FILE_TYPE_WIN_DEF = 'windows_def'

# Template for generating a stub function definition.  Includes a forward
# declaration marking the symbol as weak.  This template takes the following
# named parameters.
#   return_type: The return type.
#   export: The macro used to alter the stub's visibility.
#   name: The name of the function.
#   params: The parameters to the function.
#   return_prefix: 'return ' if this function is not void. '' otherwise.
#   arg_list: The arguments used to call the stub function.
STUB_FUNCTION_DEFINITION = (
    """extern %(return_type)s %(name)s(%(params)s) __attribute__((weak));
DISABLE_CFI_ICALL
%(return_type)s %(export)s %(name)s(%(params)s) {
  %(return_prefix)s%(name)s_ptr(%(arg_list)s);
}""")

# Template for generating a variadic stub function definition with return
# value.
# Includes a forward declaration marking the symbol as weak.
# This template takes the following named parameters.
#   return_type: The return type.
#   export: The macro used to alter the stub's visibility.
#   name: The name of the function.
#   params: The parameters to the function.
#   arg_list: The arguments used to call the stub function without the
#             variadic argument.
#   last_named_arg: Name of the last named argument before the variadic
#                   argument.
VARIADIC_STUB_FUNCTION_DEFINITION = (
    """extern %(return_type)s %(name)s(%(params)s) __attribute__((weak));
DISABLE_CFI_ICALL
%(return_type)s %(export)s %(name)s(%(params)s) {
  va_list args___;
  va_start(args___, %(last_named_arg)s);
  %(return_type)s ret___ = %(name)s_ptr(%(arg_list)s, va_arg(args___, void*));
  va_end(args___);
  return ret___;
}""")

# Template for generating a variadic stub function definition without
# return value.
# Includes a forward declaration marking the symbol as weak.
# This template takes the following named parameters.
#   name: The name of the function.
#   params: The parameters to the function.
#   export: The macro used to alter the stub's visibility.
#   arg_list: The arguments used to call the stub function without the
#             variadic argument.
#   last_named_arg: Name of the last named argument before the variadic
#                   argument.
VOID_VARIADIC_STUB_FUNCTION_DEFINITION = (
    """extern void %(name)s(%(params)s) __attribute__((weak));
DISABLE_CFI_ICALL
void %(export)s %(name)s(%(params)s) {
  va_list args___;
  va_start(args___, %(last_named_arg)s);
  %(name)s_ptr(%(arg_list)s, va_arg(args___, void*));
  va_end(args___);
}""")

# Template for the preamble for the stub header file with the header guards,
# standard set of includes, and namespace opener.  This template takes the
# following named parameters:
#   guard_name: The macro to use as the header guard.
#   namespace: The namespace for the stub functions.
STUB_HEADER_PREAMBLE = """// This is generated file. Do not modify directly.

#ifndef %(guard_name)s
#define %(guard_name)s

#include <map>
#include <string>
#include <vector>

namespace %(namespace)s {
"""

# Template for the end of the stub header. This closes the namespace and the
# header guards.  This template takes the following named parameters:
#   guard_name: The macro to use as the header guard.
#   namespace: The namespace for the stub functions.
STUB_HEADER_CLOSER = """}  // namespace %(namespace)s

#endif  // %(guard_name)s
"""

# The standard includes needed for the stub implementation file.  Takes one
# string substitution with the path to the associated stub header file.
IMPLEMENTATION_PREAMBLE = """// This is generated file. Do not modify directly.

#include "%(header_path)s"

#include <dlfcn.h>   // For dlsym, dlopen, RTLD_LAZY.

#include <cstdarg>

#include "%(macro_include)s"
#include "%(logging_include)s"
"""

# The start and end templates for the enum definitions used by the Umbrella
# initializer.
UMBRELLA_ENUM_START = """// Enum and typedef for umbrella initializer.
enum StubModules {
"""
UMBRELLA_ENUM_END = """  kNumStubModules
};

"""

# Start and end of the extern "C" section for the implementation contents.
IMPLEMENTATION_CONTENTS_C_START = """extern "C" {

"""
IMPLEMENTATION_CONTENTS_C_END = """
}  // extern "C"


"""

# Templates for the start and end of a namespace.  Takes one parameter, the
# namespace name.
NAMESPACE_START = """namespace %s {

"""
NAMESPACE_END = """}  // namespace %s

"""

# Comment to include before the section declaring all the function pointers
# used by the stub functions.
FUNCTION_POINTER_SECTION_COMMENT = (
    """// Static pointers that will hold the location of the real function
// implementations after the module has been loaded.
""")

# Template for the module initialization check function.  This template
# takes two parameters: the function name, and the conditional used to
# verify the module's initialization.
MODULE_INITIALIZATION_CHECK_FUNCTION = (
    """// Returns true if all stubs have been properly initialized.
bool %s() {
  return %s;
}

""")

# Template for the line that initialize the stub pointer.  This template takes
# the following named parameters:
#   name: The name of the function.
#   return_type: The return type.
#   params: The parameters to the function.
#   logging_function: Function call for error logging.
STUB_POINTER_INITIALIZER = """  const char %(name)s_name[] = "%(name)s";
  %(name)s_ptr =
    reinterpret_cast<%(return_type)s (*)(%(parameters)s)>(
      dlsym(module, %(name)s_name));
  if (!%(name)s_ptr)
    LogDlerror(%(name)s_name);
"""

LOG_DLERROR = """void LogDlerror(const char* symbol) {
  %s << "Couldn't load " << symbol << ", dlerror() says:\\n" << dlerror();
}

"""

# Template for module initializer function start and end.  This template takes
# one parameter which is the initializer function name.
MODULE_INITIALIZE_START = """// Initializes the module stubs.
void %s(void* module) {
"""
MODULE_INITIALIZE_END = """}

"""

# Template for module uninitializer function start and end.  This template
# takes one parameter which is the initializer function name.
MODULE_UNINITIALIZE_START = (
    """// Uninitialize the module stubs.  Reset pointers to nullptr.
void %s() {
""")
MODULE_UNINITIALIZE_END = """}

"""

# Open namespace and add typedef for internal data structures used by the
# umbrella initializer.
UMBRELLA_INITIALIZER_START = """namespace %s {
typedef std::map<StubModules, void*> StubHandleMap;
"""

# Function close DSOs on error and clean up dangling references.
UMBRELLA_INITIALIZER_CLEANUP_FUNCTION = (
    """static void CloseLibraries(StubHandleMap* stub_handles) {
  for (StubHandleMap::const_iterator it = stub_handles->begin();
       it != stub_handles->end();
       ++it) {
    dlclose(it->second);
  }

  stub_handles->clear();
}
""")

# Function to initialize each DSO for the given paths.
#   logging_function: Function call that will be used for error logging.
UMBRELLA_INITIALIZER_INITIALIZE_FUNCTION_START = (
    """bool InitializeStubs(const StubPathMap& path_map) {
  StubHandleMap opened_libraries;
  for (int i = 0; i < kNumStubModules; ++i) {
    StubModules cur_module = static_cast<StubModules>(i);
    // If a module is missing, we fail.
    StubPathMap::const_iterator it = path_map.find(cur_module);
    if (it == path_map.end()) {
      CloseLibraries(&opened_libraries);
      return false;
    }

    // Otherwise, attempt to dlopen the library.
    const std::vector<std::string>& paths = it->second;
    bool module_opened = false;
    for (std::vector<std::string>::const_iterator dso_path = paths.begin();
         !module_opened && dso_path != paths.end();
         ++dso_path) {
      void* handle = dlopen(dso_path->c_str(), RTLD_LAZY);
      if (handle) {
        module_opened = true;
        opened_libraries[cur_module] = handle;
      } else {
        %(logging_function)s << "dlopen(" << dso_path->c_str() << ") failed.";
        if (char *errstr = dlerror()) {
          %(logging_function)s << "dlerror() says:\\n" << errstr;
        } else {
          %(logging_function)s << "dlerror() is nullptr";
        }

      }
    }

    if (!module_opened) {
      CloseLibraries(&opened_libraries);
      return false;
    }
  }
""")

# Template to generate code to check if each module initializer correctly
# completed, and cleanup on failures.  This template takes the following
# named parameters.
#   conditional: The conditional expression for successful initialization.
#   uninitializers: The statements needed to uninitialize the modules.
UMBRELLA_INITIALIZER_CHECK_AND_CLEANUP = (
    """  // Check that each module is initialized correctly.
  // Close all previously opened libraries on failure.
  if (%(conditional)s) {
    %(uninitializers)s;
    CloseLibraries(&opened_libraries);
    return false;
  }

  return true;
}
""")

# Template for Initialize, Unininitialize, and IsInitialized functions for each
# module.  This template takes the following named parameters:
#   initialize: Name of the Initialize function.
#   uninitialize: Name of the Uninitialize function.
#   is_initialized: Name of the IsInitialized function.
MODULE_FUNCTION_PROTOTYPES = """bool %(is_initialized)s();
void %(initialize)s(void* module);
void %(uninitialize)s();

"""

# Template for umbrella initializer declaration and associated datatypes.
UMBRELLA_INITIALIZER_PROTOTYPE = (
    """typedef std::map<StubModules, std::vector<std::string>> StubPathMap;

// Umbrella initializer for all the modules in this stub file.
bool InitializeStubs(const StubPathMap& path_map);
""")


def ExtractModuleName(infile_path):
  """Infers the module name from the input file path.

  The input filename is supposed to be in the form "ModuleName.sigs".
  This function splits the filename from the extension on that basename of
  the path and returns that as the module name.

  Args:
    infile_path: String holding the path to the input file.

  Returns:
    The module name as a string.
  """
  basename = os.path.basename(infile_path)

  # This loop continously removes suffixes of the filename separated by a "."
  # character.
  while 1:
    new_basename = os.path.splitext(basename)[0]
    if basename == new_basename:
      break
    else:
      basename = new_basename
  return basename


def ParseSignatures(infile):
  """Parses function signatures in the input file.

  This function parses a file of signatures into a list of dictionaries that
  represent the function signatures in the input file.  Each dictionary has
  the following keys:
    return_type: A string with the return type.
    name: A string with the name of the function.
    params: A list of each function parameter declaration (type + name)

  The format of the input file is one C-style function signature per line, no
  trailing semicolon.  Empty lines are allowed.  An empty line is a line that
  consists purely of whitespace.  Lines that begin with a # or // are considered
  comment lines and are ignored.

  We assume that "int foo(void)" is the same as "int foo()", which is not
  true in C where "int foo()" is equivalent to "int foo(...)".  Our generated
  code is C++, and we do not handle varargs, so this is a case that can be
  ignored for now.

  Args:
    infile: File object holding a text file of function signatures.

  Returns:
    A list of dictionaries, where each dictionary represents one function
    signature.

  Raises:
    BadSignatureError: A line could not be parsed as a signature.
  """
  signatures = []
  for line in infile:
    line = line.strip()
    if line and line[0] != '#' and line[0:2] != '//':
      m = SIGNATURE_REGEX.match(line)
      if m is None:
        raise BadSignatureError('Unparsable line: %s' % line)
      signatures.append({
          'return_type':
          m.group('return_type').strip(),
          'name':
          m.group('name').strip(),
          'params': [arg.strip() for arg in m.group('params').split(',')]
      })
  return signatures


def WriteWindowsDefFile(module_name, signatures, outfile):
  """Writes a Windows def file to the given output file object.

    The def file format is basically a list of function names.  Generation is
    simple.  After outputting the LIBRARY and EXPORTS lines, print out each
    function name, one to a line, preceded by 2 spaces.

  Args:
    module_name: The name of the module we are writing a stub for.
    signatures: The list of signature hashes, as produced by ParseSignatures,
                to create stubs for.
    outfile: File handle to populate with definitions.
  """
  outfile.write('LIBRARY %s\n' % module_name)
  outfile.write('EXPORTS\n')

  for sig in signatures:
    outfile.write('  %s\n' % sig['name'])


def QuietRun(args, filter=None, write_to=sys.stdout):
  """Invoke |args| as command via subprocess.Popen, filtering lines starting
  with |filter|."""
  popen = subprocess.Popen(args, stdout=subprocess.PIPE)
  out, _ = popen.communicate()
  for line in out.decode('utf-8').splitlines():
    if not filter or not line.startswith(filter):
      write_to.write(line + '\n')
  return popen.returncode


def CreateWindowsLib(module_name, signatures, intermediate_dir, outdir_path,
                     machine):
  """Creates a Windows library file.

  Calling this function will create a lib file in the outdir_path that exports
  the signatures passed into the object.  A temporary def file will be created
  in the intermediate_dir.

  Args:
    module_name: The name of the module we are writing a stub for.
    signatures: The list of signature hashes, as produced by ParseSignatures,
                to create stubs for.
    intermediate_dir: The directory where the generated .def files should go.
    outdir_path: The directory where generated .lib files should go.
    machine: String holding the machine type, 'X86' or 'X64'.

  Raises:
    SubprocessError: If invoking the Windows "lib" tool fails, this is raised
                     with the error code.
  """
  def_file_path = os.path.join(intermediate_dir, module_name + '.def')
  lib_file_path = os.path.join(outdir_path, module_name + '.lib')
  outfile = open(def_file_path, 'w')
  try:
    WriteWindowsDefFile(module_name, signatures, outfile)
  finally:
    outfile.close()

  # Invoke the "lib" program on Windows to create stub .lib files for the
  # generated definitions.  These .lib files can then be used during
  # delay loading of the dynamic libraries.
  ret = QuietRun([
      'lib', '/nologo', '/machine:' + machine, '/def:' + def_file_path,
      '/out:' + lib_file_path
  ],
                 filter='   Creating library')
  if ret != 0:
    raise SubprocessError(
        'Failed creating %s for %s' % (lib_file_path, def_file_path), ret)


class PosixStubWriter(object):
  """Creates a file of stub functions for a library that is opened via dlopen.

  Windows provides a function in their compiler known as delay loading, which
  effectively generates a set of stub functions for a dynamic library that
  delays loading of the dynamic library/resolution of the symbols until one of
  the needed functions are accessed.

  In POSIX, RTLD_LAZY does something similar with DSOs.  This is the default
  link mode for DSOs.  However, even though the symbol is not resolved until
  first usage, the DSO must be present at load time of the main binary.

  To simulate the Windows delay load procedure, we need to create a set of
  stub functions that allow for correct linkage of the main binary, but
  dispatch to the dynamically resolved symbol when the module is initialized.

  This class takes a list of function signatures, and generates a set of stub
  functions plus initialization code for them.
  """

  def __init__(self, module_name, export_macro, signatures, logging_function):
    """Initializes PosixStubWriter for this set of signatures and module_name.

    Args:
      module_name: The name of the module we are writing a stub for.
      export_macro: A preprocessor macro used to annotate stub symbols with
                    an EXPORT marking, to control visibility.
      signatures: The list of signature hashes, as produced by ParseSignatures,
                  to create stubs for.
      logging_function: Function call that will be used for error logging.
    """
    self.signatures = signatures
    self.module_name = module_name
    self.export_macro = export_macro
    self.logging_function = logging_function

  @classmethod
  def CStyleIdentifier(cls, identifier):
    """Generates a C style identifier.

    The module_name has all invalid identifier characters removed (anything
    that's not [_a-zA-Z0-9]) and is run through string.capwords to try
    and approximate camel case.

    Args:
      identifier: The string with the module name to turn to C-style.

    Returns:
      A string that can be used as part of a C identifier.
    """
    return string.capwords(re.sub(INVALID_C_IDENT_CHARS, '', identifier))

  @classmethod
  def EnumName(cls, module_name):
    """Gets the enum name for the module.

    Takes the module name and creates a suitable enum name.  The module_name
    is munged to be a valid C identifier then prefixed with the string
    "kModule" to generate a Google style enum name.

    Args:
      module_name: The name of the module to generate an enum name for.

    Returns:
      A string with the name of the enum value representing this module.
    """
    return 'kModule%s' % PosixStubWriter.CStyleIdentifier(module_name)

  @classmethod
  def IsInitializedName(cls, module_name):
    """Gets the name of function that checks initialization of this module.

    The name is in the format IsModuleInitialized.  Where "Module" is replaced
    with the module name, munged to be a valid C identifier.

    Args:
      module_name: The name of the module to generate the function name for.

    Returns:
      A string with the name of the initialization check function.
    """
    return 'Is%sInitialized' % PosixStubWriter.CStyleIdentifier(module_name)

  @classmethod
  def InitializeModuleName(cls, module_name):
    """Gets the name of the function that initializes this module.

    The name is in the format InitializeModule.  Where "Module" is replaced
    with the module name, munged to be a valid C identifier.

    Args:
      module_name: The name of the module to generate the function name for.

    Returns:
      A string with the name of the initialization function.
    """
    return 'Initialize%s' % PosixStubWriter.CStyleIdentifier(module_name)

  @classmethod
  def UninitializeModuleName(cls, module_name):
    """Gets the name of the function that uninitializes this module.

    The name is in the format UninitializeModule.  Where "Module" is replaced
    with the module name, munged to be a valid C identifier.

    Args:
      module_name: The name of the module to generate the function name for.

    Returns:
      A string with the name of the uninitialization function.
    """
    return 'Uninitialize%s' % PosixStubWriter.CStyleIdentifier(module_name)

  @classmethod
  def StubFunctionPointer(cls, signature):
    """Generates a function pointer declaration for the given signature.

    Args:
      signature: A signature hash, as produced by ParseSignatures,
                 representing the function signature.

    Returns:
      A string with the declaration of the function pointer for the signature.
    """
    return 'static %s (*%s_ptr)(%s) = nullptr;' % (signature['return_type'],
                                                   signature['name'], ', '.join(
                                                       signature['params']))

  @classmethod
  def StubFunction(cls, signature):
    """Generates a stub function definition for the given signature.

    The function definitions are created with __attribute__((weak)) so that
    they may be overridden by a real static link or mock versions to be used
    when testing.

    Args:
      signature: A signature hash, as produced by ParseSignatures,
                 representing the function signature.

    Returns:
      A string with the stub function definition.
    """
    return_prefix = ''
    if signature['return_type'] != 'void':
      return_prefix = 'return '

    # Generate the argument list.
    arguments = [
        re.split(r'[\*& ]', arg)[-1].strip() for arg in signature['params']
    ]
    # Remove square brackets from arrays, otherwise we will end with a
    # compilation failure.
    for i in range(0, len(arguments)):
      if arguments[i].endswith('[]'):
        arguments[i] = arguments[i][0:-2]

    arg_list = ', '.join(arguments)
    if arg_list == 'void':
      arg_list = ''

    if arg_list != '' and len(arguments) > 1 and arguments[-1] == '...':
      # If the last argument is ... then this is a variadic function.
      if return_prefix != '':
        return VARIADIC_STUB_FUNCTION_DEFINITION % {
            'return_type': signature['return_type'],
            'name': signature['name'],
            'params': ', '.join(signature['params']),
            'arg_list': ', '.join(arguments[0:-1]),
            'last_named_arg': arguments[-2],
            'export': signature.get('export', '')
        }
      else:
        return VOID_VARIADIC_STUB_FUNCTION_DEFINITION % {
            'name': signature['name'],
            'params': ', '.join(signature['params']),
            'arg_list': ', '.join(arguments[0:-1]),
            'last_named_arg': arguments[-2],
            'export': signature.get('export', '')
        }
    else:
      # This is a regular function.
      return STUB_FUNCTION_DEFINITION % {
          'return_type': signature['return_type'],
          'name': signature['name'],
          'params': ', '.join(signature['params']),
          'return_prefix': return_prefix,
          'arg_list': arg_list,
          'export': signature.get('export', '')
      }

  @classmethod
  def WriteImplementationPreamble(cls, header_path, outfile, logging_include,
                                  macro_include):
    """Write the necessary includes for the implementation file.

    Args:
      header_path: The path to the header file.
      outfile: The file handle to populate.
    """
    outfile.write(
        IMPLEMENTATION_PREAMBLE % {
            'header_path': header_path,
            'logging_include': logging_include,
            'macro_include': macro_include,
        })

  @classmethod
  def WriteUmbrellaInitializer(cls, module_names, namespace, outfile,
                               logging_function):
    """Writes a single function that will open + initialize each module.

    This intializer will take in an stl map of that lists the correct
    dlopen target for each module.  The map type is
    std::map<enum StubModules, vector<std::string>> which matches one module
    to a list of paths to try in dlopen.

    This function is an all-or-nothing function.  If any module fails to load,
    all other modules are dlclosed, and the function returns.  Though it is
    not enforced, this function should only be called once.

    Args:
      module_names: A list with the names of the modules in this stub file.
      namespace: The namespace these functions should be in.
      outfile: The file handle to populate with pointer definitions.
    """
    outfile.write(UMBRELLA_INITIALIZER_START % namespace)
    outfile.write(UMBRELLA_INITIALIZER_CLEANUP_FUNCTION)

    # Create the initialization function that calls all module initializers,
    # checks if they succeeded, and backs out module loads on an error.
    outfile.write(UMBRELLA_INITIALIZER_INITIALIZE_FUNCTION_START %
                  {'logging_function': logging_function})
    outfile.write(
        '\n  // Initialize each module if we have not already failed.\n')
    for module in module_names:
      outfile.write('  %s(opened_libraries[%s]);\n' %
                    (PosixStubWriter.InitializeModuleName(module),
                     PosixStubWriter.EnumName(module)))
    outfile.write('\n')

    # Output code to check the initialization status, clean up on error.
    initializer_checks = [
        '!%s()' % PosixStubWriter.IsInitializedName(name)
        for name in module_names
    ]
    uninitializers = [
        '%s()' % PosixStubWriter.UninitializeModuleName(name)
        for name in module_names
    ]
    outfile.write(
        UMBRELLA_INITIALIZER_CHECK_AND_CLEANUP % {
            'conditional': ' ||\n      '.join(initializer_checks),
            'uninitializers': ';\n    '.join(uninitializers)
        })
    outfile.write('\n}  // namespace %s\n' % namespace)

  @classmethod
  def WriteHeaderContents(cls, module_names, namespace, header_guard, outfile):
    """Writes a header file for the stub file generated for module_names.

    The header file exposes the following:
       1) An enum, StubModules, listing with an entry for each enum.
       2) A typedef for a StubPathMap allowing for specification of paths to
          search for each module.
       3) The IsInitialized/Initialize/Uninitialize functions for each module.
       4) An umbrella initialize function for all modules.

    Args:
      module_names: A list with the names of each module in this stub file.
      namespace: The namespace these functions should be in.
      header_guard: The macro to use as our header guard.
      outfile: The output handle to populate.
    """
    outfile.write(STUB_HEADER_PREAMBLE % {
        'guard_name': header_guard,
        'namespace': namespace
    })

    # Generate the Initializer prototypes for each module.
    outfile.write('// Individual module initializer functions.\n')
    for name in module_names:
      outfile.write(
          MODULE_FUNCTION_PROTOTYPES % {
              'is_initialized': PosixStubWriter.IsInitializedName(name),
              'initialize': PosixStubWriter.InitializeModuleName(name),
              'uninitialize': PosixStubWriter.UninitializeModuleName(name)
          })

    # Generate the enum for umbrella initializer.
    outfile.write(UMBRELLA_ENUM_START)
    outfile.write('  %s = 0,\n' % PosixStubWriter.EnumName(module_names[0]))
    for name in module_names[1:]:
      outfile.write('  %s,\n' % PosixStubWriter.EnumName(name))
    outfile.write(UMBRELLA_ENUM_END)

    outfile.write(UMBRELLA_INITIALIZER_PROTOTYPE)
    outfile.write(STUB_HEADER_CLOSER % {
        'namespace': namespace,
        'guard_name': header_guard
    })

  def WriteImplementationContents(self, namespace, outfile):
    """Given a file handle, write out the stub definitions for this module.

    Args:
      namespace: The namespace these functions should be in.
      outfile: The file handle to populate.
    """
    outfile.write(IMPLEMENTATION_CONTENTS_C_START)
    self.WriteFunctionPointers(outfile)
    self.WriteStubFunctions(outfile)
    outfile.write(IMPLEMENTATION_CONTENTS_C_END)

    outfile.write(NAMESPACE_START % namespace)
    self.WriteModuleInitializeFunctions(outfile)
    outfile.write(NAMESPACE_END % namespace)

  def WriteFunctionPointers(self, outfile):
    """Write the function pointer declarations needed by the stubs.

    We need function pointers to hold the actual location of the function
    implementation returned by dlsym.  This function outputs a pointer
    definition for each signature in the module.

    Pointers will be named with the following pattern "FuntionName_ptr".

    Args:
      outfile: The file handle to populate with pointer definitions.
    """
    outfile.write(FUNCTION_POINTER_SECTION_COMMENT)

    for sig in self.signatures:
      outfile.write('%s\n' % PosixStubWriter.StubFunctionPointer(sig))
    outfile.write('\n')

  def WriteStubFunctions(self, outfile):
    """Write the function stubs to handle dispatching to real implementations.

    Functions that have a return type other than void will look as follows:

      ReturnType FunctionName(A a) {
        return FunctionName_ptr(a);
      }

    Functions with a return type of void will look as follows:

      void FunctionName(A a) {
        FunctionName_ptr(a);
      }

    Args:
      outfile: The file handle to populate.
    """
    outfile.write('// Stubs that dispatch to the real implementations.\n')
    for sig in self.signatures:
      sig['export'] = self.export_macro
      outfile.write('%s\n' % PosixStubWriter.StubFunction(sig))

  def WriteModuleInitializeFunctions(self, outfile):
    """Write functions to initialize/query initlialization of the module.

    This creates 2 functions IsModuleInitialized and InitializeModule where
    "Module" is replaced with the module name, first letter capitalized.

    The InitializeModule function takes a handle that is retrieved from dlopen
    and attempts to assign each function pointer above via dlsym.

    The IsModuleInitialized returns true if none of the required functions
    pointers are nullptr.

    Args:
      outfile: The file handle to populate.
    """
    ptr_names = ['%s_ptr' % sig['name'] for sig in self.signatures]

    # Construct the conditional expression to check the initialization of
    # all the function pointers above.  It should generate a conjunction
    # with each pointer on its own line, indented by six spaces to match
    # the indentation level of MODULE_INITIALIZATION_CHECK_FUNCTION.
    initialization_conditional = ' &&\n         '.join(ptr_names)

    outfile.write(MODULE_INITIALIZATION_CHECK_FUNCTION %
                  (PosixStubWriter.IsInitializedName(
                      self.module_name), initialization_conditional))

    # Create function that initializes the module.
    outfile.write(MODULE_INITIALIZE_START %
                  PosixStubWriter.InitializeModuleName(self.module_name))
    for sig in self.signatures:
      outfile.write(
          STUB_POINTER_INITIALIZER % {
              'name': sig['name'],
              'return_type': sig['return_type'],
              'parameters': ', '.join(sig['params']),
              'logging_function': self.logging_function
          })
    outfile.write(MODULE_INITIALIZE_END)

    # Create function that uninitializes the module (sets all pointers to
    # nullptr).
    outfile.write(MODULE_UNINITIALIZE_START %
                  PosixStubWriter.UninitializeModuleName(self.module_name))
    for sig in self.signatures:
      outfile.write('  %s_ptr = nullptr;\n' % sig['name'])
    outfile.write(MODULE_UNINITIALIZE_END)


def CreateOptionParser():
  """Creates an OptionParser for the configuration options of script.

  Returns:
    A OptionParser object.
  """
  parser = optparse.OptionParser(usage='usage: %prog [options] input')
  parser.add_option('-o',
                    '--output',
                    dest='out_dir',
                    default=None,
                    help='Output location.')
  parser.add_option(
      '-i',
      '--intermediate_dir',
      dest='intermediate_dir',
      default=None,
      help=('Location of intermediate files. Ignored for %s type' %
            FILE_TYPE_WIN_DEF))
  parser.add_option('-t',
                    '--type',
                    dest='type',
                    default=None,
                    help=('Type of file. Valid types are "%s" or "%s" or "%s" '
                          'or "%s"' % (FILE_TYPE_POSIX_STUB, FILE_TYPE_WIN_X86,
                                       FILE_TYPE_WIN_X64, FILE_TYPE_WIN_DEF)))
  parser.add_option('-s',
                    '--stubfile_name',
                    dest='stubfile_name',
                    default=None,
                    help=('Name of posix_stubs output file. Only valid with '
                          '%s type.' % FILE_TYPE_POSIX_STUB))
  parser.add_option('-p',
                    '--path_from_source',
                    dest='path_from_source',
                    default=None,
                    help=('The relative path from the project root that the '
                          'generated file should consider itself part of (eg. '
                          'third_party/ffmpeg).  This is used to generate the '
                          'header guard and namespace for our initializer '
                          'functions and does NOT affect the physical output '
                          'location of the file like -o does.  Ignored for '
                          '%s and %s types.' %
                          (FILE_TYPE_WIN_X86, FILE_TYPE_WIN_X64)))
  parser.add_option('-e',
                    '--extra_stub_header',
                    dest='extra_stub_header',
                    default=None,
                    help=('File to insert after the system includes in the '
                          'generated stub implementation file. Ignored for '
                          '%s and %s types.' %
                          (FILE_TYPE_WIN_X86, FILE_TYPE_WIN_X64)))
  parser.add_option('-m',
                    '--module_name',
                    dest='module_name',
                    default=None,
                    help=('Name of output DLL or LIB for DEF creation using '
                          '%s type.' % FILE_TYPE_WIN_DEF))
  parser.add_option('-x',
                    '--export_macro',
                    dest='export_macro',
                    default='',
                    help=('A macro to place between the return type and '
                          'function name, e.g. MODULE_EXPORT, to control the '
                          'visibility of the stub functions.'))
  parser.add_option('-l',
                    '--logging-function',
                    dest='logging_function',
                    default='VLOG(1)',
                    help=('Function call that will be used for error logging.'))
  parser.add_option('-n',
                    '--logging-include',
                    dest='logging_include',
                    default='base/logging.h',
                    help=('Header file where the logging function is defined.'))
  parser.add_option('--macro-include',
                    dest='macro_include',
                    default='base/compiler_specific.h',
                    help=('Header file where DISABLE_CFI_ICALL is defined.'))

  return parser


def ParseOptions():
  """Parses the options and terminates program if they are not sane.

  Returns:
    The pair (optparse.OptionValues, [string]), that is the output of
    a successful call to parser.parse_args().
  """
  parser = CreateOptionParser()
  options, args = parser.parse_args()

  if not args:
    parser.error('No inputs specified')

  if options.out_dir is None:
    parser.error('Output location not specified')

  if (options.type not in [
      FILE_TYPE_WIN_X86, FILE_TYPE_WIN_X64, FILE_TYPE_POSIX_STUB,
      FILE_TYPE_WIN_DEF
  ]):
    parser.error('Invalid output file type: %s' % options.type)

  if options.type == FILE_TYPE_POSIX_STUB:
    if options.stubfile_name is None:
      parser.error('Output file name needed for %s' % FILE_TYPE_POSIX_STUB)
    if options.path_from_source is None:
      parser.error('Path from source needed for %s' % FILE_TYPE_POSIX_STUB)

  if options.type == FILE_TYPE_WIN_DEF:
    if options.module_name is None:
      parser.error('Module name needed for %s' % FILE_TYPE_WIN_DEF)

  return options, args


def EnsureDirExists(dir):
  """Creates a directory. Does not use the more obvious 'if not exists: create'
  to avoid race with other invocations of the same code, which will error out
  on makedirs if another invocation has succeeded in creating the directory
  since the existence check."""
  try:
    os.makedirs(dir)
  except:
    if not os.path.isdir(dir):
      raise


def CreateOutputDirectories(options):
  """Creates the intermediate and final output directories.

  Given the parsed options, create the intermediate and final output
  directories if they do not exist.  Returns the paths to both directories
  as a pair.

  Args:
    options: An OptionParser.OptionValues object with the parsed options.

  Returns:
    The pair (out_dir, intermediate_dir), both of which are strings.
  """
  out_dir = os.path.normpath(options.out_dir)
  intermediate_dir = os.path.normpath(options.intermediate_dir)
  if intermediate_dir is None:
    intermediate_dir = out_dir

  EnsureDirExists(out_dir)
  EnsureDirExists(intermediate_dir)

  return out_dir, intermediate_dir


def CreateWindowsLibForSigFiles(sig_files, out_dir, intermediate_dir, machine,
                                export_macro):
  """For each signature file, create a Windows lib.

  Args:
    sig_files: Array of strings with the paths to each signature file.
    out_dir: String holding path to directory where the generated libs go.
    intermediate_dir: String holding path to directory generated intermediate
                      artifacts.
    machine: String holding the machine type, 'X86' or 'X64'.
    export_macro: A preprocessor macro used to annotate stub symbols with
                  an EXPORT marking, to control visibility.
  """
  for input_path in sig_files:
    infile = open(input_path, 'r')
    try:
      signatures = ParseSignatures(infile)
      module_name = ExtractModuleName(os.path.basename(input_path))
      for sig in signatures:
        sig['export'] = export_macro
      CreateWindowsLib(module_name, signatures, intermediate_dir, out_dir,
                       machine)
    finally:
      infile.close()


def CreateWindowsDefForSigFiles(sig_files, out_dir, module_name):
  """For all signature files, create a single Windows def file.

  Args:
    sig_files: Array of strings with the paths to each signature file.
    out_dir: String holding path to directory where the generated def goes.
    module_name: Name of the output DLL or LIB which will link in the def file.
  """
  signatures = []
  for input_path in sig_files:
    infile = open(input_path, 'r')
    try:
      signatures += ParseSignatures(infile)
    finally:
      infile.close()

  def_file_path = os.path.join(
      out_dir,
      os.path.splitext(os.path.basename(module_name))[0] + '.def')
  outfile = open(def_file_path, 'w')

  try:
    WriteWindowsDefFile(module_name, signatures, outfile)
  finally:
    outfile.close()


def CreatePosixStubsForSigFiles(sig_files, stub_name, out_dir, intermediate_dir,
                                path_from_source, extra_stub_header,
                                export_macro, logging_function, logging_include,
                                macro_include):
  """Create a POSIX stub library with a module for each signature file.

  Args:
    sig_files: Array of strings with the paths to each signature file.
    stub_name: String with the basename of the generated stub file.
    out_dir: String holding path to directory for the .h files.
    intermediate_dir: String holding path to directory for the .cc files.
    path_from_source: String with relative path of generated files from the
                      project root.
    extra_stub_header: String with path to file of extra lines to insert
                       into the generated header for the stub library.
    export_macro: A preprocessor macro used to annotate stub symbols with
                  an EXPORT marking, to control visibility.
    logging_function: Function call that will be used for error logging.
    logging_include: Header file where the logging function is defined.
    header_include: Header file where the logging function is defined.
  """
  header_base_name = stub_name + '.h'
  header_path = os.path.join(out_dir, header_base_name)
  impl_path = os.path.join(intermediate_dir, stub_name + '.cc')

  module_names = [ExtractModuleName(path) for path in sig_files]
  namespace = path_from_source.replace('/', '_').lower()
  header_guard = 'GEN_%s_%s_H_' % (namespace.upper(), stub_name.upper())
  header_include_path = os.path.join(path_from_source, header_base_name)

  # First create the implementation file.
  impl_file = open(impl_path, 'w')
  try:
    # Open the file, and create the preamble which consists of a file
    # header plus any necessary includes.
    PosixStubWriter.WriteImplementationPreamble(header_include_path, impl_file,
                                                logging_include, macro_include)
    if extra_stub_header is not None:
      extra_header_file = open(extra_stub_header, 'r')
      try:
        impl_file.write('\n')
        for line in extra_header_file:
          impl_file.write(line)
        impl_file.write('\n')
      finally:
        extra_header_file.close()

    impl_file.write(NAMESPACE_START % '')
    impl_file.write(LOG_DLERROR % logging_function)
    impl_file.write(NAMESPACE_END % '')

    # For each signature file, generate the stub population functions
    # for that file.  Each file represents one module.
    for input_path in sig_files:
      name = ExtractModuleName(input_path)
      infile = open(input_path, 'r')
      try:
        signatures = ParseSignatures(infile)
      finally:
        infile.close()
      writer = PosixStubWriter(name, export_macro, signatures, logging_function)
      writer.WriteImplementationContents(namespace, impl_file)

    # Lastly, output the umbrella function for the file.
    PosixStubWriter.WriteUmbrellaInitializer(module_names, namespace, impl_file,
                                             logging_function)
  finally:
    impl_file.close()

  # Then create the associated header file.
  header_file = open(header_path, 'w')
  try:
    PosixStubWriter.WriteHeaderContents(module_names, namespace, header_guard,
                                        header_file)
  finally:
    header_file.close()


def main():
  options, args = ParseOptions()
  out_dir, intermediate_dir = CreateOutputDirectories(options)

  if options.type == FILE_TYPE_WIN_X86:
    CreateWindowsLibForSigFiles(args, out_dir, intermediate_dir, 'X86',
                                options.export_macro)
  elif options.type == FILE_TYPE_WIN_X64:
    CreateWindowsLibForSigFiles(args, out_dir, intermediate_dir, 'X64',
                                options.export_macro)
  elif options.type == FILE_TYPE_POSIX_STUB:
    CreatePosixStubsForSigFiles(args, options.stubfile_name, out_dir,
                                intermediate_dir, options.path_from_source,
                                options.extra_stub_header, options.export_macro,
                                options.logging_function,
                                options.logging_include, options.macro_include)
  elif options.type == FILE_TYPE_WIN_DEF:
    CreateWindowsDefForSigFiles(args, out_dir, options.module_name)


if __name__ == '__main__':
  main()
