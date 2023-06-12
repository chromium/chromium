# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import shlex
import subprocess
import sys
from six.moves import input  # pylint: disable=redefined-builtin


COLOR_ANSI_CODE_MAP = {
  'black': 90,
  'red': 91,
  'green': 92,
  'yellow': 93,
  'blue': 94,
  'magenta': 95,
  'cyan': 96,
  'white': 97,
}


def Colored(message, color):
  """Wraps the message into ASCII color escape codes.

  Args:
    message: Message to be wrapped.
    color: See COLOR_ANSI_CODE_MAP.keys() for available choices.
  """
  # This only works on Linux and OS X. Windows users must install ANSICON or
  # use VT100 emulation on Windows 10.
  assert color in COLOR_ANSI_CODE_MAP, 'Unsupported color'
  return '\033[%dm%s\033[0m' % (COLOR_ANSI_CODE_MAP[color], message)


def Info(message, **kwargs):
  print(message.format(**kwargs))


def Comment(message, **kwargs):
  """Prints an import message to the user."""
  print(Colored(message.format(**kwargs), 'yellow'))


def Fatal(message, **kwargs):
  """Displays an error to the user and terminates the program."""
  Error(message, **kwargs)
  sys.exit(1)


def Error(message, **kwargs):
  """Displays an error to the user."""
  print(Colored(message.format(**kwargs), 'red'))


def Step(name):
  """Display a decorated message to the user.

  This is useful to separate major stages of the script. For simple messages,
  please use comment function above.
  """
  boundary = max(80, len(name))
  print(Colored('=' * boundary, 'green'))
  print(Colored(name, 'green'))
  print(Colored('=' * boundary, 'green'))


def Ask(question, answers=None, default=None):
  """Asks the user to answer a question with multiple choices.

  Users are able to press Return to access the default answer (if specified) and
  to type part of the full answer, e.g. "y", "ye" or "yes" are all valid answers
  for "yes". The func will ask user again in case an invalid answer is provided.

  Raises ValueError if default is specified, but not listed an a valid answer.

  Args:
    question: Question to be asked.
    answers: List or dictinary describing user choices. In case of a dictionary,
        the keys are the options display to the user and values are the return
        values for this method. In case of a list, returned values are same as
        options displayed to the user. When presenting to the user, the order of
        answers is preserved if list is used, otherwise answers are sorted
        alphabetically. Defaults to {'yes': True, 'no': False}.
    default: Default option chosen on empty answer. Defaults to 'yes' if default
        value is used for answers parameter or to lack of default answer
        otherwise.

  Returns:
    Chosen option from answers. Full option name is returned even if user only
    enters part of it or chooses the default.
  """
  if answers is None:
    answers = {'yes': True, 'no': False}
    default = 'yes'
  if isinstance(answers, list):
    ordered_answers = answers
    answers = {v: v for v in answers}
  else:
    ordered_answers = sorted(answers)

  # Generate a set of prefixes for all answers such that the user can type just
  # the minimum number of characters required, e.g. 'y' or 'ye' can be used for
  # the 'yes' answer. Shared prefixes are ignored, e.g. 'n' and 'ne' will not be
  # accepted if 'negate' and 'next' are both valid answers, whereas 'nex' and
  # 'neg' would be accepted.
  inputs = {}
  common_prefixes = set()
  for ans, retval in answers.items():
    for i in range(len(ans)):
      inp = ans[:i+1]
      if inp in inputs:
        common_prefixes.add(inp)
        del inputs[inp]
      if inp not in common_prefixes:
        inputs[inp] = retval

  if default is None:
    prompt = ' [%s] ' % '/'.join(ordered_answers)
  elif default in answers:
    ans_with_def = (a if a != default else a.upper() for a in ordered_answers)
    prompt = ' [%s] ' % '/'.join(ans_with_def)
  else:
    raise ValueError('invalid default answer: "%s"' % default)

  while True:
    print(Colored(question + prompt, 'cyan'), end=' ')
    choice = input().strip()
    if default is not None and choice == '':
      return inputs[default]
    if choice in inputs:
      return inputs[choice]
    if choice.lower() in inputs:
      return inputs[choice.lower()]
    choices = sorted(['"%s"' % a for a in sorted(answers.keys())])
    Error('Please respond with %s or %s.' %
          (', '.join(choices[:-1]), choices[-1]))


def Prompt(question, accept_empty=False):
  while True:
    print(Colored(question, color='cyan'))
    answer = input().strip()
    if answer or accept_empty:
      return answer
    Error('Please enter non-empty answer')


def CheckLog(command, log_path, env=None):
  """Executes a command and writes its stdout to a specified log file.

  On non-zero return value, also prints the content of the file to the screen
  and raises subprocess.CalledProcessError.

  Args:
    command: Command to be run as a list of arguments.
    log_path: Path to a file to which the output will be written.
    env: Environment to run the command in.
  """
  with open(log_path, 'w') as f:
    try:
      cmd_str = (' '.join(
          shlex.quote(c)
          for c in command) if isinstance(command, list) else command)
      print(Colored(cmd_str, 'blue'))
      print(Colored('Logging stdout & stderr to %s' % log_path, 'blue'))
      subprocess.check_call(
          command, stdout=f, stderr=subprocess.STDOUT, shell=False, env=env)
    except subprocess.CalledProcessError:
      Error('=' * 80)
      Error('Received non-zero return code. Log content:')
      Error('=' * 80)
      subprocess.call(['cat', log_path])
      Error('=' * 80)
      raise


def Run(command, ok_fail=False, **kwargs):
  """Prints and runs the command. Allows to ignore non-zero exit code."""
  if not isinstance(command, list):
    raise ValueError('command must be a list')
  print(Colored(' '.join(shlex.quote(c) for c in command), 'blue'))
  try:
    return subprocess.check_call(command, **kwargs)
  except subprocess.CalledProcessError as cpe:
    if not ok_fail:
      raise
    return cpe.returncode
