# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import shutil
try:
  import sqlite3
except ImportError:
  pass
import tempfile
import unittest

from cli_tools.soundwave import pandas_sqlite
from cli_tools.soundwave import worker_pool
from core.external_modules import pandas
from telemetry import decorators


def TestWorker(args):
  con = sqlite3.connect(args.database_file)

  def Process(item):
    # Add item to the database.
    df = pandas.DataFrame({'item': [item]})
    df.to_sql('items', con, index=False, if_exists='append')

  worker_pool.Process = Process


@unittest.skipIf(pandas is None, 'pandas not available')
class TestWorkerPool(unittest.TestCase):

  @decorators.Disabled('all')  # crbug.com/939777
  def testWorkerPoolRun(self):
    tempdir = tempfile.mkdtemp()
    try:
      args = argparse.Namespace()
      args.database_file = os.path.join(tempdir, 'test.db')
      args.processes = 3
      schema = pandas_sqlite.DataFrame([('item', int)])
      items = range(20)  # We'll write these in the database.
      con = sqlite3.connect(args.database_file)
      try:
        pandas_sqlite.CreateTableIfNotExists(con, 'items', schema)
        with open(os.devnull, 'w') as devnull:
          worker_pool.Run(
              'Processing:', TestWorker, args, items, stream=devnull)
        df = pandas.read_sql('SELECT * FROM items', con)
        # Check all of our items were written.
        self.assertItemsEqual(df['item'], items)
      finally:
        con.close()
    finally:
      shutil.rmtree(tempdir)
