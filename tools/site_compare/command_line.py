#!/usr/bin/env python
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Parse a command line, retrieving a command and its arguments.

Supports the concept of command line commands, each with its own set
of arguments. Supports dependent arguments and mutually exclusive arguments.
Basically, a better optparse. I took heed of epg's WHINE() in gvn.cmdline
and dumped optparse in favor of something better.
"""

from __future__ import print_function

import os.path
import re
import string
import sys
import textwrap
import types


def IsString(var):
  """Little helper function to see if a variable is a string."""
  return type(var) in types.StringTypes


class ParseError(Exception):
  """Encapsulates errors from parsing, string arg is description."""
  pass


class Command(object):
  """Implements a single command."""

  def __init__(self, names, helptext, validator=None, impl=None):
    """Initializes Command from names and helptext, plus optional callables.

    Args:
      names:       command name, or list of synonyms
      helptext:    brief string description of the command
      validator:   callable for custom argument validation
                   Should raise ParseError if it wants
      impl:        callable to be invoked when command is called
    """
    self.names = names
    self.validator = validator
    self.helptext = helptext
    self.impl = impl
    self.args = []
    self.required_groups = []
    self.arg_dict = {}
    self.positional_args = []
    self.cmdline = None

  class Argument(object):
    """Encapsulates an argument to a command."""
    VALID_TYPES = ['string', 'readfile', 'int', 'flag', 'coords']
    TYPES_WITH_VALUES = ['string', 'readfile', 'int', 'coords']

    def __init__(self, names, helptext, type, metaname,
                 required, default, positional):
      """Command-line argument to a command.

      Args:
        names:       argument name, or list of synonyms
        helptext:    brief description of the argument
        type:        type of the argument. Valid values include:
                          string - a string
                          readfile - a file which must exist and be available
                            for reading
                          int - an integer
                          flag - an optional flag (bool)
                          coords - (x,y) where x and y are ints
        metaname:    Name to display for value in help, inferred if not
                     specified
        required:    True if argument must be specified
        default:     Default value if not specified
        positional:  Argument specified by location, not name

      Raises:
        ValueError: the argument name is invalid for some reason
      """
      if type not in Command.Argument.VALID_TYPES:
        raise ValueError("Invalid type: %r" % type)

      if required and default is not None:
        raise ValueError("required and default are mutually exclusive")

      if required and type == 'flag':
        raise ValueError("A required flag? Give me a break.")

      if metaname and type not in Command.Argument.TYPES_WITH_VALUES:
        raise ValueError("Type %r can't have a metaname" % type)

      # If no metaname is provided, infer it: use the alphabetical characters
      # of the last provided name
      if not metaname and type in Command.Argument.TYPES_WITH_VALUES:
        metaname = (
          names[-1].lstrip(string.punctuation + string.whitespace).upper())

      self.names = names
      self.helptext = helptext
      self.type = type
      self.required = required
      self.default = default
      self.positional = positional
      self.metaname = metaname

      self.mutex = []          # arguments that are mutually exclusive with
                               # this one
      self.depends = []        # arguments that must be present for this
                               # one to be valid
      self.present = False     # has this argument been specified?

    def AddDependency(self, arg):
      """Makes this argument dependent on another argument.

      Args:
        arg: name of the argument this one depends on
      """
      if arg not in self.depends:
        self.depends.append(arg)

    def AddMutualExclusion(self, arg):
      """Makes this argument invalid if another is specified.

      Args:
        arg: name of the mutually exclusive argument.
      """
      if arg not in self.mutex:
        self.mutex.append(arg)

    def GetUsageString(self):
      """Returns a brief string describing the argument's usage."""
      if not self.positional:
        string = self.names[0]
        if self.type in Command.Argument.TYPES_WITH_VALUES:
          string += "="+self.metaname
      else:
        string = self.metaname

      if not self.required:
        string = "["+string+"]"

      return string

    def GetNames(self):
      """Returns a string containing a list of the arg's names."""
      if self.positional:
        return self.metaname
      else:
        return ", ".join(self.names)

    def GetHelpString(self, width=80, indent=5, names_width=20, gutter=2):
      """Returns a help string including help for all the arguments."""
      names = [" "*indent + line +" "*(names_width-len(line)) for line in
               textwrap.wrap(self.GetNames(), names_width)]

      helpstring = textwrap.wrap(self.helptext, width-indent-names_width-gutter)

      if len(names) < len(helpstring):
        names += [" "*(indent+names_width)]*(len(helpstring)-len(names))

      if len(helpstring) < len(names):
        helpstring += [""]*(len(names)-len(helpstring))

      return "\n".join([name_line + " "*gutter + help_line for
                        name_line, help_line in zip(names, helpstring)])

    def __repr__(self):
      if self.present:
        string = '= %r' % self.value
      else:
        string = "(absent)"

      return "Argument %s '%s'%s" % (self.type, self.names[0], string)

    # end of nested class Argument

  def AddArgument(self, names, helptext, type="string", metaname=None,
                  required=False, default=None, positional=False):
    """Command-line argument to a command.

    Args:
      names:      argument name, or list of synonyms
      helptext:   brief description of the argument
      type:       type of the argument
      metaname:   Name to display for value in help, inferred if not
      required:   True if argument must be specified
      default:    Default value if not specified
      positional: Argument specified by location, not name

    Raises:
      ValueError: the argument already exists or is invalid

    Returns:
      The newly-created argument
    """
    if IsString(names): names = [names]

    names = [name.lower() for name in names]

    for name in names:
      if name in self.arg_dict:
        raise ValueError("%s is already an argument"%name)

    if (positional and required and
        [arg for arg in self.args if arg.positional] and
        not [arg for arg in self.args if arg.positional][-1].required):
      raise ValueError(
        "A required positional argument may not follow an optional one.")

    arg = Command.Argument(names, helptext, type, metaname,
                           required, default, positional)

    self.args.append(arg)

    for name in names:
      self.arg_dict[name] = arg

    return arg

  def GetArgument(self, name):
    """Return an argument from a name."""
    return self.arg_dict[name.lower()]

  def AddMutualExclusion(self, args):
    """Specifies that a list of arguments are mutually exclusive."""
    if len(args) < 2:
      raise ValueError("At least two arguments must be specified.")

    args = [arg.lower() for arg in args]

    for index in xrange(len(args)-1):
      for index2 in xrange(index+1, len(args)):
        self.arg_dict[args[index]].AddMutualExclusion(self.arg_dict[args[index2]])

  def AddDependency(self, dependent, depends_on):
    """Specifies that one argument may only be present if another is.

    Args:
      dependent:  the name of the dependent argument
      depends_on: the name of the argument on which it depends
    """
    self.arg_dict[dependent.lower()].AddDependency(
      self.arg_dict[depends_on.lower()])

  def AddMutualDependency(self, args):
    """Specifies that a list of arguments are all mutually dependent."""
    if len(args) < 2:
      raise ValueError("At least two arguments must be specified.")

    args = [arg.lower() for arg in args]

    for (arg1, arg2) in [(arg1, arg2) for arg1 in args for arg2 in args]:
      if arg1 == arg2: continue
      self.arg_dict[arg1].AddDependency(self.arg_dict[arg2])

  def AddRequiredGroup(self, args):
    """Specifies that at least one of the named arguments must be present."""
    if len(args) < 2:
      raise ValueError("At least two arguments must be in a required group.")

    args = [self.arg_dict[arg.lower()] for arg in args]

    self.required_groups.append(args)

  def ParseArguments(self):
    """Given a command line, parse and validate the arguments."""

    # reset all the arguments before we parse
    for arg in self.args:
      arg.present = False
      arg.value = None

    self.parse_errors = []

    # look for arguments remaining on the command line
    while len(self.cmdline.rargs):
      try:
        self.ParseNextArgument()
      except ParseError, e:
        self.parse_errors.append(e.args[0])

    # after all the arguments are parsed, check for problems
    for arg in self.args:
      if not arg.present and arg.required:
        self.parse_errors.append("'%s': required parameter was missing"
                                 % arg.names[0])

      if not arg.present and arg.default:
        arg.present = True
        arg.value = arg.default

      if arg.present:
        for mutex in arg.mutex:
          if mutex.present:
            self.parse_errors.append(
              "'%s', '%s': arguments are mutually exclusive" %
              (arg.argstr, mutex.argstr))

        for depend in arg.depends:
          if not depend.present:
            self.parse_errors.append("'%s': '%s' must be specified as well" %
                                     (arg.argstr, depend.names[0]))

    # check for required groups
    for group in self.required_groups:
      if not [arg for arg in group if arg.present]:
        self.parse_errors.append("%s: at least one must be present" %
                         (", ".join(["'%s'" % arg.names[-1] for arg in group])))

    # if we have any validators, invoke them
    if not self.parse_errors and self.validator:
      try:
        self.validator(self)
      except ParseError, e:
        self.parse_errors.append(e.args[0])

  # Helper methods so you can treat the command like a dict
  def __getitem__(self, key):
    arg = self.arg_dict[key.lower()]

    if arg.type == 'flag':
      return arg.present
    else:
      return arg.value

  def __iter__(self):
    return [arg for arg in self.args if arg.present].__iter__()

  def ArgumentPresent(self, key):
    """Tests if an argument exists and has been specified."""
    return key.lower() in self.arg_dict and self.arg_dict[key.lower()].present

  def __contains__(self, key):
    return self.ArgumentPresent(key)

  def ParseNextArgument(self):
    """Find the next argument in the command line and parse it."""
    arg = None
    value = None
    argstr = self.cmdline.rargs.pop(0)

    # First check: is this a literal argument?
    if argstr.lower() in self.arg_dict:
      arg = self.arg_dict[argstr.lower()]
      if arg.type in Command.Argument.TYPES_WITH_VALUES:
        if len(self.cmdline.rargs):
          value = self.cmdline.rargs.pop(0)

    # Second check: is this of the form "arg=val" or "arg:val"?
    if arg is None:
      delimiter_pos = -1

      for delimiter in [':', '=']:
        pos = argstr.find(delimiter)
        if pos >= 0:
          if delimiter_pos < 0 or pos < delimiter_pos:
            delimiter_pos = pos

      if delimiter_pos >= 0:
        testarg = argstr[:delimiter_pos]
        testval = argstr[delimiter_pos+1:]

        if testarg.lower() in self.arg_dict:
          arg = self.arg_dict[testarg.lower()]
          argstr = testarg
          value = testval

    # Third check: does this begin an argument?
    if arg is None:
      for key in self.arg_dict.iterkeys():
        if (len(key) < len(argstr) and
            self.arg_dict[key].type in Command.Argument.TYPES_WITH_VALUES and
            argstr[:len(key)].lower() == key):
          value = argstr[len(key):]
          argstr = argstr[:len(key)]
          arg = self.arg_dict[argstr]

    # Fourth check: do we have any positional arguments available?
    if arg is None:
      for positional_arg in [
          testarg for testarg in self.args if testarg.positional]:
        if not positional_arg.present:
          arg = positional_arg
          value = argstr
          argstr = positional_arg.names[0]
          break

    # Push the retrieved argument/value onto the largs stack
    if argstr: self.cmdline.largs.append(argstr)
    if value:  self.cmdline.largs.append(value)

    # If we've made it this far and haven't found an arg, give up
    if arg is None:
      raise ParseError("Unknown argument: '%s'" % argstr)

    # Convert the value, if necessary
    if arg.type in Command.Argument.TYPES_WITH_VALUES and value is None:
      raise ParseError("Argument '%s' requires a value" % argstr)

    if value is not None:
      value = self.StringToValue(value, arg.type, argstr)

    arg.argstr = argstr
    arg.value = value
    arg.present = True

    # end method ParseNextArgument

  def StringToValue(self, value, type, argstr):
    """Convert a string from the command line to a value type."""
    try:
      if type == 'string':
        pass  # leave it be

      elif type == 'int':
        try:
          value = int(value)
        except ValueError:
          raise ParseError

      elif type == 'readfile':
        if not os.path.isfile(value):
          raise ParseError("'%s': '%s' does not exist" % (argstr, value))

      elif type == 'coords':
        try:
          value = [int(val) for val in
                   re.match("\(\s*(\d+)\s*\,\s*(\d+)\s*\)\s*\Z", value).
                   groups()]
        except AttributeError:
          raise ParseError

      else:
        raise ValueError("Unknown type: '%s'" % type)

    except ParseError, e:
      # The bare exception is raised in the generic case; more specific errors
      # will arrive with arguments and should just be reraised
      if not e.args:
        e = ParseError("'%s': unable to convert '%s' to type '%s'" %
                       (argstr, value, type))
      raise e

    return value

  def SortArgs(self):
    """Returns a method that can be passed to sort() to sort arguments."""

    def ArgSorter(arg1, arg2):
      """Helper for sorting arguments in the usage string.

      Positional arguments come first, then required arguments,
      then optional arguments. Pylint demands this trivial function
      have both Args: and Returns: sections, sigh.

      Args:
        arg1: the first argument to compare
        arg2: the second argument to compare

      Returns:
        -1 if arg1 should be sorted first, +1 if it should be sorted second,
        and 0 if arg1 and arg2 have the same sort level.
      """
      return ((arg2.positional-arg1.positional)*2 +
              (arg2.required-arg1.required))
    return ArgSorter

  def GetUsageString(self, width=80, name=None):
    """Gets a string describing how the command is used."""
    if name is None: name = self.names[0]

    initial_indent = "Usage: %s %s " % (self.cmdline.prog, name)
    subsequent_indent = " " * len(initial_indent)

    sorted_args = self.args[:]
    sorted_args.sort(self.SortArgs())

    return textwrap.fill(
      " ".join([arg.GetUsageString() for arg in sorted_args]), width,
      initial_indent=initial_indent,
      subsequent_indent=subsequent_indent)

  def GetHelpString(self, width=80):
    """Returns a list of help strings for all this command's arguments."""
    sorted_args = self.args[:]
    sorted_args.sort(self.SortArgs())

    return "\n".join([arg.GetHelpString(width) for arg in sorted_args])

  # end class Command


class CommandLine(object):
  """Parse a command line, extracting a command and its arguments."""

  def __init__(self):
    self.commands = []
    self.cmd_dict = {}

    # Add the help command to the parser
    help_cmd = self.AddCommand(["help", "--help", "-?", "-h"],
                               "Displays help text for a command",
                               ValidateHelpCommand,
                               DoHelpCommand)

    help_cmd.AddArgument(
      "command", "Command to retrieve help for", positional=True)
    help_cmd.AddArgument(
      "--width", "Width of the output", type='int', default=80)

    self.Exit = sys.exit   # override this if you don't want the script to halt
                           # on error or on display of help

    self.out = sys.stdout  # override these if you want to redirect
    self.err = sys.stderr  # output or error messages

  def AddCommand(self, names, helptext, validator=None, impl=None):
    """Add a new command to the parser.

    Args:
      names:       command name, or list of synonyms
      helptext:    brief string description of the command
      validator:   method to validate a command's arguments
      impl:        callable to be invoked when command is called

    Raises:
      ValueError: raised if command already added

    Returns:
      The new command
    """
    if IsString(names): names = [names]

    for name in names:
      if name in self.cmd_dict:
        raise ValueError("%s is already a command"%name)

    cmd = Command(names, helptext, validator, impl)
    cmd.cmdline = self

    self.commands.append(cmd)
    for name in names:
      self.cmd_dict[name.lower()] = cmd

    return cmd

  def GetUsageString(self):
    """Returns simple usage instructions."""
    return "Type '%s help' for usage." % self.prog

  def ParseCommandLine(self, argv=None, prog=None, execute=True):
    """Does the work of parsing a command line.

    Args:
      argv:     list of arguments, defaults to sys.args[1:]
      prog:     name of the command, defaults to the base name of the script
      execute:  if false, just parse, don't invoke the 'impl' member

    Returns:
      The command that was executed
    """
    if argv is None: argv = sys.argv[1:]
    if prog is None: prog = os.path.basename(sys.argv[0]).split('.')[0]

    # Store off our parameters, we may need them someday
    self.argv = argv
    self.prog = prog

    # We shouldn't be invoked without arguments, that's just lame
    if not len(argv):
      self.out.writelines(self.GetUsageString())
      self.Exit()
      return None   # in case the client overrides Exit

    # Is it a valid command?
    self.command_string = argv[0].lower()
    if not self.command_string in self.cmd_dict:
      self.err.write("Unknown command: '%s'\n\n" % self.command_string)
      self.out.write(self.GetUsageString())
      self.Exit()
      return None   # in case the client overrides Exit

    self.command = self.cmd_dict[self.command_string]

    # "rargs" = remaining (unparsed) arguments
    # "largs" = already parsed, "left" of the read head
    self.rargs = argv[1:]
    self.largs = []

    # let the command object do the parsing
    self.command.ParseArguments()

    if self.command.parse_errors:
      # there were errors, output the usage string and exit
      self.err.write(self.command.GetUsageString()+"\n\n")
      self.err.write("\n".join(self.command.parse_errors))
      self.err.write("\n\n")

      self.Exit()

    elif execute and self.command.impl:
      self.command.impl(self.command)

    return self.command

  def __getitem__(self, key):
    return self.cmd_dict[key]

  def __iter__(self):
    return self.cmd_dict.__iter__()


def ValidateHelpCommand(command):
  """Checks to make sure an argument to 'help' is a valid command."""
  if 'command' in command and command['command'] not in command.cmdline:
    raise ParseError("'%s': unknown command" % command['command'])


def DoHelpCommand(command):
  """Executed when the command is 'help'."""
  out = command.cmdline.out
  width = command['--width']

  if 'command' not in command:
    out.write(command.GetUsageString())
    out.write("\n\n")

    indent = 5
    gutter = 2

    command_width = (
      max([len(cmd.names[0]) for cmd in command.cmdline.commands]) + gutter)

    for cmd in command.cmdline.commands:
      cmd_name = cmd.names[0]

      initial_indent = (" "*indent + cmd_name + " "*
                        (command_width+gutter-len(cmd_name)))
      subsequent_indent = " "*(indent+command_width+gutter)

      out.write(textwrap.fill(cmd.helptext, width,
                              initial_indent=initial_indent,
                              subsequent_indent=subsequent_indent))
      out.write("\n")

    out.write("\n")

  else:
    help_cmd = command.cmdline[command['command']]

    out.write(textwrap.fill(help_cmd.helptext, width))
    out.write("\n\n")
    out.write(help_cmd.GetUsageString(width=width))
    out.write("\n\n")
    out.write(help_cmd.GetHelpString(width=width))
    out.write("\n")

    command.cmdline.Exit()


def main():
  # If we're invoked rather than imported, run some tests
  cmdline = CommandLine()

  # Since we're testing, override Exit()
  def TestExit():
    pass
  cmdline.Exit = TestExit

  # Actually, while we're at it, let's override error output too
  cmdline.err = open(os.path.devnull, "w")

  test = cmdline.AddCommand(["test", "testa", "testb"], "test command")
  test.AddArgument(["-i", "--int", "--integer", "--optint", "--optionalint"],
                   "optional integer parameter", type='int')
  test.AddArgument("--reqint", "required integer parameter", type='int',
                   required=True)
  test.AddArgument("pos1", "required positional argument", positional=True,
                   required=True)
  test.AddArgument("pos2", "optional positional argument", positional=True)
  test.AddArgument("pos3", "another optional positional arg",
                   positional=True)

  # mutually dependent arguments
  test.AddArgument("--mutdep1", "mutually dependent parameter 1")
  test.AddArgument("--mutdep2", "mutually dependent parameter 2")
  test.AddArgument("--mutdep3", "mutually dependent parameter 3")
  test.AddMutualDependency(["--mutdep1", "--mutdep2", "--mutdep3"])

  # mutually exclusive arguments
  test.AddArgument("--mutex1", "mutually exclusive parameter 1")
  test.AddArgument("--mutex2", "mutually exclusive parameter 2")
  test.AddArgument("--mutex3", "mutually exclusive parameter 3")
  test.AddMutualExclusion(["--mutex1", "--mutex2", "--mutex3"])

  # dependent argument
  test.AddArgument("--dependent", "dependent argument")
  test.AddDependency("--dependent", "--int")

  # other argument types
  test.AddArgument("--file", "filename argument", type='readfile')
  test.AddArgument("--coords", "coordinate argument", type='coords')
  test.AddArgument("--flag", "flag argument", type='flag')

  test.AddArgument("--req1", "part of a required group", type='flag')
  test.AddArgument("--req2", "part 2 of a required group", type='flag')

  test.AddRequiredGroup(["--req1", "--req2"])

  # a few failure cases
  exception_cases = """
    test.AddArgument("failpos", "can't have req'd pos arg after opt",
       positional=True, required=True)
+++
    test.AddArgument("--int", "this argument already exists")
+++
    test.AddDependency("--int", "--doesntexist")
+++
    test.AddMutualDependency(["--doesntexist", "--mutdep2"])
+++
    test.AddMutualExclusion(["--doesntexist", "--mutex2"])
+++
    test.AddArgument("--reqflag", "required flag", required=True, type='flag')
+++
    test.AddRequiredGroup(["--req1", "--doesntexist"])
"""
  for exception_case in exception_cases.split("+++"):
    try:
      exception_case = exception_case.strip()
      exec exception_case     # yes, I'm using exec, it's just for a test.
    except ValueError:
      # this is expected
      pass
    except KeyError:
      # ...and so is this
      pass
    else:
      print("FAILURE: expected an exception for '%s'"
            " and didn't get it" % exception_case)

  # Let's do some parsing! first, the minimal success line:
  MIN = "test --reqint 123 param1 --req1 "

  # tuples of (command line, expected error count)
  test_lines = [
    ("test --int 3 foo --req1", 1),   # missing required named parameter
    ("test --reqint 3 --req1", 1),    # missing required positional parameter
    (MIN, 0),                         # success!
    ("test param1 --reqint 123 --req1", 0),  # success, order shouldn't matter
    ("test param1 --reqint 123 --req2", 0),  # success, any of required group ok
    (MIN+"param2", 0),                # another positional parameter is okay
    (MIN+"param2 param3", 0),         # and so are three
    (MIN+"param2 param3 param4", 1),  # but four are just too many
    (MIN+"--int", 1),                 # where's the value?
    (MIN+"--int 456", 0),             # this is fine
    (MIN+"--int456", 0),              # as is this
    (MIN+"--int:456", 0),             # and this
    (MIN+"--int=456", 0),             # and this
    (MIN+"--file c:\\windows\\system32\\kernel32.dll", 0),  # yup
    (MIN+"--file c:\\thisdoesntexist", 1),           # nope
    (MIN+"--mutdep1 a", 2),                          # no!
    (MIN+"--mutdep2 b", 2),                          # also no!
    (MIN+"--mutdep3 c", 2),                          # dream on!
    (MIN+"--mutdep1 a --mutdep2 b", 2),              # almost!
    (MIN+"--mutdep1 a --mutdep2 b --mutdep3 c", 0),  # yes
    (MIN+"--mutex1 a", 0),                           # yes
    (MIN+"--mutex2 b", 0),                           # yes
    (MIN+"--mutex3 c", 0),                           # fine
    (MIN+"--mutex1 a --mutex2 b", 1),                # not fine
    (MIN+"--mutex1 a --mutex2 b --mutex3 c", 3),     # even worse
    (MIN+"--dependent 1", 1),                        # no
    (MIN+"--dependent 1 --int 2", 0),                # ok
    (MIN+"--int abc", 1),                            # bad type
    (MIN+"--coords abc", 1),                         # also bad
    (MIN+"--coords (abc)", 1),                       # getting warmer
    (MIN+"--coords (abc,def)", 1),                   # missing something
    (MIN+"--coords (123)", 1),                       # ooh, so close
    (MIN+"--coords (123,def)", 1),                   # just a little farther
    (MIN+"--coords (123,456)", 0),                   # finally!
    ("test --int 123 --reqint=456 foo bar --coords(42,88) baz --req1", 0)
    ]

  badtests = 0

  for (test, expected_failures) in test_lines:
    cmdline.ParseCommandLine([x.strip() for x in test.strip().split(" ")])

    if not len(cmdline.command.parse_errors) == expected_failures:
      print("FAILED:\n  issued: '%s'\n  expected: %d\n  received: %d\n\n" %
            (test, expected_failures, len(cmdline.command.parse_errors)))
      badtests += 1

  print("%d failed out of %d tests" % (badtests, len(test_lines)))

  cmdline.ParseCommandLine(["help", "test"])


if __name__ == "__main__":
  sys.exit(main())
