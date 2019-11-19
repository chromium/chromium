# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

try:
  import sqlite3
except ImportError:
  pass
import unittest

from cli_tools.soundwave import pandas_sqlite
from core.external_modules import pandas


@unittest.skipIf(pandas is None, 'pandas not available')
class TestPandasSQLite(unittest.TestCase):
  def testCreateTableIfNotExists_newTable(self):
    df = pandas_sqlite.DataFrame(
        [('bug_id', int), ('summary', str), ('status', str)], index='bug_id')
    con = sqlite3.connect(':memory:')
    try:
      self.assertFalse(pandas.io.sql.has_table('bugs', con))
      pandas_sqlite.CreateTableIfNotExists(con, 'bugs', df)
      self.assertTrue(pandas.io.sql.has_table('bugs', con))
    finally:
      con.close()

  def testCreateTableIfNotExists_alreadyExists(self):
    df = pandas_sqlite.DataFrame(
        [('bug_id', int), ('summary', str), ('status', str)], index='bug_id')
    con = sqlite3.connect(':memory:')
    try:
      self.assertFalse(pandas.io.sql.has_table('bugs', con))
      pandas_sqlite.CreateTableIfNotExists(con, 'bugs', df)
      self.assertTrue(pandas.io.sql.has_table('bugs', con))
      # It's fine to call a second time.
      pandas_sqlite.CreateTableIfNotExists(con, 'bugs', df)
      self.assertTrue(pandas.io.sql.has_table('bugs', con))
    finally:
      con.close()

  def testInsertOrReplaceRecords_tableNotExistsRaises(self):
    column_types = (('bug_id', int), ('summary', str), ('status', str))
    rows = [(123, 'Some bug', 'Started'), (456, 'Another bug', 'Assigned')]
    df = pandas_sqlite.DataFrame(column_types, index='bug_id', rows=rows)
    con = sqlite3.connect(':memory:')
    try:
      with self.assertRaises(AssertionError):
        pandas_sqlite.InsertOrReplaceRecords(con, 'bugs', df)
    finally:
      con.close()

  def testInsertOrReplaceRecords_existingRecords(self):
    column_types = (('bug_id', int), ('summary', str), ('status', str))
    rows1 = [(123, 'Some bug', 'Started'), (456, 'Another bug', 'Assigned')]
    df1 = pandas_sqlite.DataFrame(column_types, index='bug_id', rows=rows1)
    rows2 = [(123, 'Some bug', 'Fixed'), (789, 'A new bug', 'Untriaged')]
    df2 = pandas_sqlite.DataFrame(column_types, index='bug_id', rows=rows2)
    con = sqlite3.connect(':memory:')
    try:
      pandas_sqlite.CreateTableIfNotExists(con, 'bugs', df1)

      # Write first data frame to database.
      pandas_sqlite.InsertOrReplaceRecords(con, 'bugs', df1)
      df = pandas.read_sql('SELECT * FROM bugs', con, index_col='bug_id')
      self.assertEqual(len(df), 2)
      self.assertEqual(df.loc[123]['status'], 'Started')

      # Write second data frame to database.
      pandas_sqlite.InsertOrReplaceRecords(con, 'bugs', df2)
      df = pandas.read_sql('SELECT * FROM bugs', con, index_col='bug_id')
      self.assertEqual(len(df), 3)  # Only one extra record added.
      self.assertEqual(df.loc[123]['status'], 'Fixed')  # Bug is now fixed.
      self.assertItemsEqual(df.index, (123, 456, 789))
    finally:
      con.close()
