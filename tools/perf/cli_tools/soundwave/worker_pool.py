# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Use a pool of workers to concurrently process a sequence of items.

Example usage:

    from soundwave import worker_pool

    def MyWorker(args):
      # This is called once for each worker to initialize it.

      def Process(item):
        # This will be called once for each item processed by this worker.

      # Hook up the Process function so the worker_pool module can find it.
      worker_pool.Process = Process

    args.processes = 10  # Set number of processes to be used by the pool.
    worker_pool.Run('This might take a while: ', MyWorker, args, items)
"""
import logging
import multiprocessing
import sys

from core.external_modules import pandas


# Worker implementations override this value
Process = NotImplemented  # pylint: disable=invalid-name


def ProgressIndicator(label, iterable, stream=None):
  if stream is None:
    stream = sys.stdout
  stream.write(label)
  stream.flush()
  for _ in iterable:
    stream.write('.')
    stream.flush()
  stream.write('\n')
  stream.flush()


def Run(label, worker, args, items, stream=None):
  """Use a pool of workers to concurrently process a sequence of items.

  Args:
    label: A string displayed by the progress indicator when the job starts.
    worker: A function with the worker implementation. See example above.
    args: An argparse.Namespace() object used to initialize the workers. The
        value of args.processes is the number of processes used by the pool.
    items: An iterable with items to process by the pool of workers.
    stream: A file-like object for the progress indicator output, defaults to
        sys.stdout.

  Returns:
    Total time in seconds spent by the pool to process all items.
  """
  pool = multiprocessing.Pool(
      processes=args.processes, initializer=worker, initargs=(args,))
  time_started = pandas.Timestamp.utcnow()
  try:
    ProgressIndicator(label, pool.imap_unordered(_Worker, items), stream=stream)
    time_finished = pandas.Timestamp.utcnow()
  finally:
    # Ensure resources (e.g. db connections from workers) are freed up.
    pool.terminate()
    pool.join()
  return (time_finished - time_started).total_seconds()


def _Worker(item):
  try:
    Process(item)  # pylint: disable=not-callable
  except KeyboardInterrupt:
    pass
  except:
    logging.exception('Worker failed with exception')
    raise
