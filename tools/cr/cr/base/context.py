# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Application context management for the cr tool.

Contains all the support code to enable the shared context used by the cr tool.
This includes the configuration variables and command line handling.
"""

from __future__ import print_function

import argparse
import os
import cr

class _DumpVisitor(cr.visitor.ExportVisitor):
  """A visitor that prints all variables in a config hierarchy."""

  def __init__(self, with_source):
    super(_DumpVisitor, self).__init__({})
    self.to_dump = {}
    self.with_source = with_source

  def StartNode(self):
    if self.with_source:
      self._DumpNow()
    super(_DumpVisitor, self).StartNode()

  def EndNode(self):
    if self.with_source or not self.stack:
      self._DumpNow()
    super(_DumpVisitor, self).EndNode()
    if not self.stack:
      self._DumpNow()

  def Visit(self, key, value):
    super(_DumpVisitor, self).Visit(key, value)
    if key in self.store:
      str_value = str(self.store[key])
      if str_value != str(os.environ.get(key, None)):
        self.to_dump[key] = str_value

  def _DumpNow(self):
    if self.to_dump:
      if self.with_source:
        print('From', self.Where())
      for key in sorted(self.to_dump.keys()):
        print('  ', key, '=', self.to_dump[key])
      self.to_dump = {}


class _ShowHelp(argparse.Action):
  """An argparse action to print the help text.

  This is like the built in help text printing action, except it knows to do
  nothing when we are just doing the early speculative parse of the args.
  """

  def __call__(self, parser, namespace, values, option_string=None):
    if cr.context.speculative:
      return
    command = cr.Command.GetActivePlugin()
    if command:
      command.parser.print_help()
    else:
      parser.print_help()
    exit(1)


class _ArgumentParser(argparse.ArgumentParser):
  """An extension of an ArgumentParser to enable speculative parsing.

  It supports doing an early parse that never produces errors or output, to do
  early collection of arguments that may affect what other arguments are
  allowed.
  """

  def error(self, message):
    if cr.context.speculative:
      return
    super(_ArgumentParser, self).error(message)

  def parse_args(self):
    if cr.context.speculative:
      result = self.parse_known_args()
      if result:
        return result[0]
      return None
    return super(_ArgumentParser, self).parse_args()

  def parse_known_args(self, args=None, namespace=None):
    result = super(_ArgumentParser, self).parse_known_args(args, namespace)
    if result is None:
      return namespace, None
    return result


# The context stack
_stack = []


class _ContextData:
  pass


class Context(cr.config.Config):
  """The base context holder for the cr system.

  This holds the common context shared throughout cr.
  Mostly this is stored in the Config structure of variables.
  """

  def __init__(self, name='Context'):
    super(Context, self).__init__(name)
    self._data = _ContextData()

  def CreateData(self, description='', epilog=''):
    self._data.args = None
    self._data.arguments = cr.config.Config('ARGS')
    self._data.derived = cr.config.Config('DERIVED')
    self.AddChildren(*cr.config.GLOBALS)
    self.AddChildren(
        cr.config.Config('ENVIRONMENT', literal=True, export=True).Set(
            {k: self.ParseValue(v) for k, v in os.environ.items()}),
        self._data.arguments,
        self._data.derived,
    )
    # Build the command line argument parser
    self._data.parser = _ArgumentParser(add_help=False, description=description,
                                        epilog=epilog)
    self._data.subparsers = self.parser.add_subparsers()
    # Add the global arguments
    self.AddCommonArguments(self._data.parser)
    self._data.gclient = {}

  @property
  def data(self):
    return self._data

  def __enter__(self):
    """ To support using 'with cr.base.context.Create():'"""
    _stack.append(self)
    cr.context = self
    return self

  def __exit__(self, *_):
    _stack.pop()
    if _stack:
      cr.context = _stack[-1]
    return False

  def AddSubParser(self, source):
    parser = source.AddArguments(self._data.subparsers)

  @classmethod
  def AddCommonArguments(cls, parser):
    """Adds the command line arguments common to all commands in cr."""
    parser.add_argument(
        '-h', '--help',
        action=_ShowHelp, nargs=0,
        help='show the help message and exit.'
    )
    parser.add_argument(
        '--dry-run', dest='CR_DRY_RUN',
        action='store_true', default=None,
        help="""
          Don't execute commands, just print them. Implies verbose.
          Overrides CR_DRY_RUN
          """
    )
    parser.add_argument('-v',
                        '--verbose',
                        dest='CR_VERBOSE',
                        action='count',
                        default=0,
                        help="""
          Print information about commands being performed.
          Repeating multiple times increases the verbosity level.
          Overrides CR_VERBOSE
          """)

  @property
  def args(self):
    return self._data.args

  @property
  def arguments(self):
    return self._data.arguments

  @property
  def speculative(self):
    return self._data.speculative

  @property
  def derived(self):
    return self._data.derived

  @property
  def parser(self):
    return self._data.parser

  @property
  def remains(self):
    remains = getattr(self._data.args, '_remains', None)
    if remains and remains[0] == '--':
      remains = remains[1:]
    return remains

  @property
  def verbose(self):
    if self.autocompleting:
      return 0
    return self.Find('CR_VERBOSE') or (self.dry_run and 1 or 0)

  @property
  def dry_run(self):
    if self.autocompleting:
      return True
    return self.Find('CR_DRY_RUN')

  @property
  def autocompleting(self):
    return 'COMP_WORD' in os.environ

  @property
  def gclient(self):
    if not self._data.gclient:
      self._data.gclient = cr.base.client.ReadGClient()
    return self._data.gclient

  def ParseArgs(self, speculative=False):
    cr.plugin.DynamicChoices.only_active = not speculative
    self._data.speculative = speculative
    self._data.args = self._data.parser.parse_args()
    self._data.arguments.Wipe()
    if self._data.args:
      self._data.arguments.Set(
          {k: v for k, v in vars(self._data.args).items() if v is not None})

  def DumpValues(self, with_source):
    _DumpVisitor(with_source).VisitNode(self)


def Create(description='', epilog=''):
  context = Context()
  context.CreateData(description=description, epilog=epilog)
  return context
