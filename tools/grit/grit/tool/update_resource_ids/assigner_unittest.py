#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import os
import sys
import traceback
import unittest
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../../..'))

from grit.tool.update_resource_ids import assigner, common, parser

# |spec| format: A comma-separated list of (old) start IDs. Modifiers:
# * Prefix with n '*' to assign the item's META "join" field to n + 1.
# * Suffix with "+[usage]" to assign |usage| for the item (else default=10)


def _RenderTestResourceId(spec):
  """Renders barebone resource_ids data based on |spec|."""
  data = '{"SRCDIR": ".",'
  for i, tok in enumerate(spec.split(',')):
    num_star = len(tok) - len(tok.lstrip('*'))
    tok = tok[num_star:]
    meta = '"META":{"join": %d},' % (num_star + 1) if num_star else ''
    start_id = tok.split('+')[0]  # Strip '+usage'.
    data += '"foo%d.grd": {%s"includes": [%s]},' % (i, meta, start_id)
  data += '}'
  return data


def _CreateTestItemList(spec):
  """Creates list of ItemInfo based on |spec|."""
  data = _RenderTestResourceId(spec)
  item_list = common.BuildItemList(
      parser.ResourceIdParser(data, parser.Tokenize(data)).Parse())
  # Assign usages from "id+usage", default to 10.
  for i, tok in enumerate(spec.split(',')):
    item_list[i].tags[0].usage = int((tok.split('+') + ['10'])[1])
  return item_list


def _RunCoarseIdAssigner(spec):
  item_list = _CreateTestItemList(spec)
  coarse_id_assigner = assigner.DagCoarseIdAssigner(item_list, 1)
  new_id_list = []  # List of new IDs, to check ID assignment.
  new_spec_list = []  # List of new tokens to construct new |spec|.
  for item, start_id in coarse_id_assigner.GenStartIds():  # Topo-sorted..
    new_id_list.append(str(start_id))
    meta = item.meta
    num_join = meta['join'].val if meta and 'join' in meta else 0
    t = '*' * max(0, num_join - 1)
    t += str(start_id)
    t += '' if item.tags[0].usage == 10 else '+' + str(item.tags[0].usage)
    new_spec_list.append((item.lo, t))
    coarse_id_assigner.FeedWeight(item, item.tags[0].usage)
  new_spec = ','.join(s for _, s in sorted(new_spec_list))
  return ','.join(new_id_list), new_spec


class AssignerUnittest(unittest.TestCase):

  def testDagAssigner(self):
    test_cases = [
        # Trivial.
        ('0', '0'),
        ('137', '137'),
        ('5,15', '5,6'),
        ('11,18', '11+7,12'),
        ('5,5', '5,5'),
        # Series only.
        ('0,10,20,30,40', '0,1,2,3,4'),
        ('5,15,25,35,45,55', '5,6,7,8,9,10'),
        ('5,15,25,35,45,55', '5,7,100,101,256,1001'),
        ('0,10,20,45,85', '0,1,2+25,3+40,4'),
        # Branching with and without join.
        ('0,0,10,20,20,30,40', '0,0,1,2,2,3,4'),
        ('0,0,10,20,20,30,40', '0,0,*1,2,2,*3,4'),
        ('0,0,2,12,12,16,26', '0+4,0+2,1,2+8,2+4,3,4'),
        ('0,0,4,14,14,22,32', '0+4,0+2,*1,2+8,2+4,*3,4'),
        # Wide branching with and without join.
        ('0,10,10,10,10,10,10,20,30', '0,1,1,1,1,1,1,2,3'),
        ('0,10,10,10,10,10,10,20,30', '0,1,1,1,1,1,1,*****2,3'),
        ('0,2,2,2,2,2,2,7,17', '0+2,1+4,1+19,1,1+4,1+2,1+5,2,3'),
        ('0,2,2,2,2,2,2,21,31', '0+2,1+4,1+19,1,1+4,1+2,1+5,*****2,3'),
        # Expanding different branch, without join.
        ('0,10,10,10,60,70,80', '0,1+15,1+15,1+50,2,3,4'),
        ('0,10,10,10,25,35,45', '0,1+15,1+50,1+15,2,3,4'),
        ('0,10,10,10,25,35,45', '0,1+50,1+15,1+15,2,3,4'),
        # ... with join.
        ('0,10,10,10,60,70,80', '0,1+15,1+15,1+50,**2,3,4'),
        ('0,10,10,10,60,70,80', '0,1+15,1+50,1+15,**2,3,4'),
        ('0,10,10,10,60,70,80', '0,1+50,1+15,1+15,**2,3,4'),
        # ... with alternative join.
        ('0,10,10,10,60,70,80', '0,1+15,1+15,1+50,2,**3,4'),
        ('0,10,10,10,25,60,70', '0,1+15,1+50,1+15,2,**3,4'),
        ('0,10,10,10,25,60,70', '0,1+50,1+15,1+15,2,**3,4'),
        # Examples from assigner.py.
        ('0,10,10,20,0,10,20,30,0,10',
         '0,1,1,*2,0,4,5,*6,0,7'),  # SA|AB|SDEC|SF
        ('0,10,0,10,20,30', '0,1,0,2,*3,4'),  # SA|SB*CD
        ('0,10,0,10,20,30', '0,1,0,2,3,*4'),  # SA|SBC*D
        ('0,7,0,5,11,21', '0+7,1+4,0+5,2+3,*3,4'),  # SA|SB*CD
        ('0,7,0,5,8,18', '0+7,1+4,0+5,2+3,3,*4'),  # SA|SBC*D
        ('0,0,0,0,10,20', '0,0,0,0,*1,**2'),  # S|S|S|S*A**B
        ('0,0,0,0,10,20', '0,0,0,0,**1,*2'),  # S|S|S|S**A*B
        ('0,0,0,0,6,16', '0+8,0+7,0+6,0+5,*1,**2'),  # S|S|S|S*A**B
        ('0,0,0,0,7,17', '0+8,0+7,0+6,0+5,**1,*2'),  # S|S|S|S**A*B
        # Long branches without join.
        ('0,10,0,0,10,20,0,10,20,30', '0,1,0,0,1,2,0,1,2,3'),
        ('0,30,0,0,20,30,0,10,13,28', '0+30,1,0+50,0+20,1,2+17,0,1+3,2+15,3'),
        # Long branches with join.
        ('0,10,0,0,10,20,0,10,20,30', '0,1,0,0,1,2,0,1,2,***3'),
        ('0,30,0,0,20,30,0,10,13,50',
         '0+30,1,0+50,0+20,1,2+17,0,1+3,2+15,***3'),
        # 2-level hierarchy.
        ('0,10,10,20,0,10,10,20,30', '0,1,1,*2,0,1,1,*2,*3'),
        ('0,2,2,10,0,3,3,6,34', '0+2,1+5,1+8,*2+24,0+3,1+2,1+3,*2+27,*3'),
        ('0,2,2,10,0,3,3,6,34', '0+2,1+5,1+8,*2+24,0+3,1+2,1+3,*2+28,*3'),
        ('0,2,2,10,0,3,3,6,35', '0+2,1+5,1+8,*2+24,0+3,1+2,1+3,*2+29,*3'),
        # Binary hierarchy.
        ('0,0,10,0,0,10,20,0,0,10,0,0,10,20,30',
         '0,0,*1,0,0,*1,*2,0,0,*1,0,0,*1,*2,*3'),
        ('0,0,2,0,0,5,11,0,0,8,0,0,5,14,18',
         '0+1,0+2,*1+3,0+4,0+5,*1+6,*2+7,0+8,0+7,*1+6,0+5,0+4,*1+3,*2+2,*3+1'),
        # Joining from different heads.
        ('0,10,20,30,40,30,20,10,0,50', '0,1,2,3,4,3,2,1,0,****5'),
        # Miscellaneous.
        ('0,1,0,11', '0+1,1,0,*1'),
    ]
    for exp, spec in test_cases:
      try:
        actual, new_spec = _RunCoarseIdAssigner(spec)
        self.assertEqual(exp, actual)
        # Test that assignment is idempotent.
        actual2, new_spec2 = _RunCoarseIdAssigner(new_spec)
        self.assertEqual(actual, actual2)
        self.assertEqual(new_spec, new_spec2)
      except Exception as e:
        print(common.Color.RED(traceback.format_exc().rstrip()))
        print('Failed spec: %s' % common.Color.CYAN(spec))
        print('   Expected: %s' % common.Color.YELLOW(exp))
        print('     Actual: %s' % common.Color.YELLOW(actual))
        if new_spec != new_spec2:
          print('Not idempotent')
        if isinstance(e, AssertionError):
          raise e


if __name__ == '__main__':
  unittest.main()
