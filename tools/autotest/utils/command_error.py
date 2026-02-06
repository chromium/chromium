# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
class CommandError(Exception):
  """Exception thrown when a subcommand fails."""

  def __init__(self,
               command: list[str],
               return_code: int,
               output: str | None = None) -> None:
    Exception.__init__(self)
    self.command = command
    self.return_code = return_code
    self.output = output

  def __str__(self) -> str:
    message: str = (f'\n***\nERROR: Error while running command {self.command}'
                    f'.\nExit status: {self.return_code}\n')
    if self.output:
      message += f'Output:\n{self.output}\n'
    message += '***'
    return message


class AutotestError(Exception):
  pass
