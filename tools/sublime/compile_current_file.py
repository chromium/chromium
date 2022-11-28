#!/usr/bin/env python
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import fnmatch
import logging
import os
import os.path
import queue as Queue
import sublime
import sublime_plugin
import subprocess
import sys
import tempfile
import threading
import time

# Path to the version of ninja checked in into Chrome.
rel_path_to_ninja = os.path.join('third_party', 'ninja', 'ninja')


class PrintOutputCommand(sublime_plugin.TextCommand):
  def run(self, edit, **args):
    self.view.set_read_only(False)
    self.view.insert(edit, self.view.size(), args['text'])
    self.view.show(self.view.size())
    self.view.set_read_only(True)


class CompileCurrentFile(sublime_plugin.TextCommand):
  # static thread so that we don't try to run more than once at a time.
  thread = None
  lock = threading.Lock()

  def __init__(self, args):
    super(CompileCurrentFile, self).__init__(args)
    self.thread_id = threading.current_thread().ident
    self.text_to_draw = ""
    self.interrupted = False

  def description(self):
    return ("Compiles the file in the current view using Ninja, so all that "
            "this file and it's project depends on will be built first\n"
            "Note that this command is a toggle so invoking it while it runs "
            "will interrupt it.")

  def draw_panel_text(self):
    """Draw in the output.exec panel the text accumulated in self.text_to_draw.

    This must be called from the main UI thread (e.g., using set_timeout).
    """
    assert self.thread_id == threading.current_thread().ident
    logging.debug("draw_panel_text called.")
    self.lock.acquire()
    text_to_draw = self.text_to_draw
    self.text_to_draw = ""
    self.lock.release()

    if len(text_to_draw):
      self.output_panel.run_command('print_output', {'text': text_to_draw})
      self.view.window().run_command("show_panel", {"panel": "output.exec"})
      logging.debug("Added text:\n%s.", text_to_draw)

  def update_panel_text(self, text_to_draw):
    self.lock.acquire()
    self.text_to_draw += text_to_draw
    self.lock.release()
    sublime.set_timeout(self.draw_panel_text, 0)

  def execute_command(self, command, cwd):
    """Execute the provided command and send ouput to panel.

    Because the implementation of subprocess can deadlock on windows, we use
    a Queue that we write to from another thread to avoid blocking on IO.

    Args:
      command: A list containing the command to execute and it's arguments.
    Returns:
      The exit code of the process running the command or,
       1 if we got interrupted.
      -1 if we couldn't start the process
      -2 if we couldn't poll the running process
    """
    logging.debug("Running command: %s", command)

    def EnqueueOutput(out, queue):
      """Read all the output from the given handle and insert it into the queue.

      Args:
        queue: The Queue object to write to.
      """
      while True:
        # This readline will block until there is either new input or the handle
        # is closed. Readline will only return None once the handle is close, so
        # even if the output is being produced slowly, this function won't exit
        # early.
        # The potential dealock here is acceptable because this isn't run on the
        # main thread.
        data = out.readline()
        if not data:
          break
        queue.put(data, block=True)
      out.close()

    try:
      os.chdir(cwd)
      proc = subprocess.Popen(command, stdout=subprocess.PIPE, shell=True,
                              stderr=subprocess.STDOUT, stdin=subprocess.PIPE)
    except OSError as e:
      logging.exception('Execution of %s raised exception: %s.', (command, e))
      return -1

    # Use a Queue to pass the text from the reading thread to this one.
    stdout_queue = Queue.Queue()
    stdout_thread = threading.Thread(target=EnqueueOutput,
                                     args=(proc.stdout, stdout_queue))
    stdout_thread.daemon = True  # Ensure this exits if the parent dies
    stdout_thread.start()

    # We use the self.interrupted flag to stop this thread.
    while not self.interrupted:
      try:
        exit_code = proc.poll()
      except OSError as e:
        logging.exception('Polling execution of %s raised exception: %s.',
                          command, e)
        return -2

      # Try to read output content from the queue
      current_content = ""
      for _ in range(2048):
        try:
          current_content += stdout_queue.get_nowait().decode('utf-8')
        except Queue.Empty:
          break
      self.update_panel_text(current_content)
      current_content = ""
      if exit_code is not None:
        while stdout_thread.isAlive() or not stdout_queue.empty():
          try:
            current_content += stdout_queue.get(
                               block=True, timeout=1).decode('utf-8')
          except Queue.Empty:
            # Queue could still potentially contain more input later.
            pass
        time_length = datetime.datetime.now() - self.start_time
        self.update_panel_text("%s\nDone!\n(%s seconds)" %
                               (current_content, time_length.seconds))
        return exit_code
      # We sleep a little to give the child process a chance to move forward
      # before we poll it again.
      time.sleep(0.1)

    # If we get here, it's because we were interrupted, kill the process.
    proc.terminate()
    return 1

  def run(self, edit, target_build):
    """The method called by Sublime Text to execute our command.

    Note that this command is a toggle, so if the thread is are already running,
    calling run will interrupt it.

    Args:
      edit: Sumblime Text specific edit brace.
      target_build: Release/Debug/Other... Used for the subfolder of out.
    """
    # There can only be one... If we are running, interrupt and return.
    if self.thread and self.thread.is_alive():
      self.interrupted = True
      self.thread.join(5.0)
      self.update_panel_text("\n\nInterrupted current command:\n%s\n" % command)
      self.thread = None
      return

    # It's nice to display how long it took to build.
    self.start_time = datetime.datetime.now()
    # Output our results in the same panel as a regular build.
    self.output_panel = self.view.window().get_output_panel("exec")
    self.output_panel.set_read_only(True)
    self.view.window().run_command("show_panel", {"panel": "output.exec"})
    # TODO(mad): Not sure if the project folder is always the first one... ???
    project_folder = self.view.window().folders()[0]
    self.update_panel_text("Compiling current file %s\n" %
                           self.view.file_name())
    # The file must be somewhere under the project folder...
    if (project_folder.lower() !=
        self.view.file_name()[:len(project_folder)].lower()):
      self.update_panel_text(
          "ERROR: File %s is not in current project folder %s\n" %
              (self.view.file_name(), project_folder))
    else:
      output_dir = os.path.join(project_folder, 'out', target_build)
      source_relative_path = os.path.relpath(self.view.file_name(),
                                             output_dir)
      # On Windows the caret character needs to be escaped as it's an escape
      # character.
      carets = '^'
      if sys.platform.startswith('win'):
        carets = '^^'
      command = [
          os.path.join(project_folder, rel_path_to_ninja), "-C",
          os.path.join(project_folder, 'out', target_build),
          source_relative_path + carets]
      self.update_panel_text(' '.join(command) + '\n')
      self.interrupted = False
      self.thread = threading.Thread(target=self.execute_command,
                                     kwargs={"command":command,
                                             "cwd": output_dir})
      self.thread.start()

    time_length = datetime.datetime.now() - self.start_time
    logging.debug("Took %s seconds on UI thread to startup",
                  time_length.seconds)
    self.view.window().focus_view(self.view)
