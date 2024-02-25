// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {ArrayDataModel} from '../../../common/js/array_data_model.js';

import {Grid} from './grid.js';

export function testGetColumnCount() {
  const g = Grid.prototype;
  g['measured_'] = {
    height: 8,
    marginTop: 0,
    marginBottom: 0,
    width: 10,
    marginLeft: 0,
    marginRight: 0,
  };
  let columns = g['getColumnCount_']();
  g['measured_'].width = 0;
  columns = g['getColumnCount_']();
  // Item width equals 0.
  assertEquals(0, columns);

  g['measured_'].width = 10;
  columns = g['getColumnCount_']();
  // No item in the list.
  assertEquals(0, columns);

  g['dataModel_'] = new ArrayDataModel([0, 1, 2]);
  g['horizontalPadding_'] = 0;
  g['clientWidthWithoutScrollbar_'] = 8;
  columns = g['getColumnCount_']();
  // Client width is smaller than item width.
  assertEquals(0, columns);

  g['clientWidthWithoutScrollbar_'] = 20;
  // Client height can fit two rows.
  g['clientHeight_'] = 16;
  columns = g['getColumnCount_']();
  assertEquals(2, columns);

  // Client height can not fit two rows. A scroll bar is needed.
  g['clientHeight_'] = 15;
  g['clientWidthWithScrollbar_'] = 18;
  columns = g['getColumnCount_']();
  // Can not fit two columns due to the scroll bar.
  assertEquals(1, columns);

  g['clientHeight_'] = 16;
  g['measured_'].marginTop = 1;
  columns = g['getColumnCount_']();
  // Can fit two columns due to uncollapse margin.
  assertEquals(2, columns);

  g['measured_'].marginBottom = 1;
  columns = g['getColumnCount_']();
  // Can not fit two columns due to margin.
  assertEquals(1, columns);

  g['measured_'].marginTop = 0;
  g['measured_'].marginBottom = 0;
  g['measured_'].marginLeft = 1;
  columns = g['getColumnCount_']();
  // Can fit two columns due to uncollapse margin.
  assertEquals(2, columns);

  g['measured_'].marginRight = 1;
  columns = g['getColumnCount_']();
  // Can not fit two columns due to margin on left and right side.
  assertEquals(1, columns);

  g['measured_'].marginRight = 0;
  g['horizontalPadding_'] = 2;
  g['clientWidthWithoutScrollbar_'] = 22;
  columns = g['getColumnCount_']();
  // Can fit two columns as (22-2=)20px width is available for grid items.
  assertEquals(2, columns);

  g['horizontalPadding_'] = 3;
  columns = g['getColumnCount_']();
  // Can not fit two columns due to bigger horizontal padding.
  assertEquals(1, columns);
}
