// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {ArrayDataModel} from '../../../common/js/array_data_model.js';

import {Grid} from './grid.js';

import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';
// clang-format on

/**
 * @suppress {visibility} Allow test to reach to private properties.
 */
export function testGetColumnCount() {
  const g = Grid.prototype;
  // @ts-ignore: error TS2339: Property 'measured_' does not exist on type
  // 'Grid'.
  g.measured_ = {
    height: 8,
    marginTop: 0,
    marginBottom: 0,
    width: 10,
    marginLeft: 0,
    marginRight: 0,
  };
  // @ts-ignore: error TS2339: Property 'getColumnCount_' does not exist on type
  // 'Grid'.
  let columns = g.getColumnCount_();
  // @ts-ignore: error TS2339: Property 'measured_' does not exist on type
  // 'Grid'.
  g.measured_.width = 0;
  // @ts-ignore: error TS2339: Property 'getColumnCount_' does not exist on type
  // 'Grid'.
  columns = g.getColumnCount_();
  // Item width equals 0.
  assertEquals(0, columns);

  // @ts-ignore: error TS2339: Property 'measured_' does not exist on type
  // 'Grid'.
  g.measured_.width = 10;
  // @ts-ignore: error TS2339: Property 'getColumnCount_' does not exist on type
  // 'Grid'.
  columns = g.getColumnCount_();
  // No item in the list.
  assertEquals(0, columns);

  // @ts-ignore: error TS2551: Property 'dataModel_' does not exist on type
  // 'Grid'. Did you mean 'dataModel'?
  g.dataModel_ = new ArrayDataModel([0, 1, 2]);
  // @ts-ignore: error TS2339: Property 'horizontalPadding_' does not exist on
  // type 'Grid'.
  g.horizontalPadding_ = 0;
  // @ts-ignore: error TS2339: Property 'clientWidthWithoutScrollbar_' does not
  // exist on type 'Grid'.
  g.clientWidthWithoutScrollbar_ = 8;
  // @ts-ignore: error TS2339: Property 'getColumnCount_' does not exist on type
  // 'Grid'.
  columns = g.getColumnCount_();
  // Client width is smaller than item width.
  assertEquals(0, columns);

  // @ts-ignore: error TS2339: Property 'clientWidthWithoutScrollbar_' does not
  // exist on type 'Grid'.
  g.clientWidthWithoutScrollbar_ = 20;
  // Client height can fit two rows.
  // @ts-ignore: error TS2551: Property 'clientHeight_' does not exist on type
  // 'Grid'. Did you mean 'clientHeight'?
  g.clientHeight_ = 16;
  // @ts-ignore: error TS2339: Property 'getColumnCount_' does not exist on type
  // 'Grid'.
  columns = g.getColumnCount_();
  assertEquals(2, columns);

  // Client height can not fit two rows. A scroll bar is needed.
  // @ts-ignore: error TS2551: Property 'clientHeight_' does not exist on type
  // 'Grid'. Did you mean 'clientHeight'?
  g.clientHeight_ = 15;
  // @ts-ignore: error TS2339: Property 'clientWidthWithScrollbar_' does not
  // exist on type 'Grid'.
  g.clientWidthWithScrollbar_ = 18;
  // @ts-ignore: error TS2339: Property 'getColumnCount_' does not exist on type
  // 'Grid'.
  columns = g.getColumnCount_();
  // Can not fit two columns due to the scroll bar.
  assertEquals(1, columns);

  // @ts-ignore: error TS2551: Property 'clientHeight_' does not exist on type
  // 'Grid'. Did you mean 'clientHeight'?
  g.clientHeight_ = 16;
  // @ts-ignore: error TS2339: Property 'measured_' does not exist on type
  // 'Grid'.
  g.measured_.marginTop = 1;
  // @ts-ignore: error TS2339: Property 'getColumnCount_' does not exist on type
  // 'Grid'.
  columns = g.getColumnCount_();
  // Can fit two columns due to uncollapse margin.
  assertEquals(2, columns);

  // @ts-ignore: error TS2339: Property 'measured_' does not exist on type
  // 'Grid'.
  g.measured_.marginBottom = 1;
  // @ts-ignore: error TS2339: Property 'getColumnCount_' does not exist on type
  // 'Grid'.
  columns = g.getColumnCount_();
  // Can not fit two columns due to margin.
  assertEquals(1, columns);

  // @ts-ignore: error TS2339: Property 'measured_' does not exist on type
  // 'Grid'.
  g.measured_.marginTop = 0;
  // @ts-ignore: error TS2339: Property 'measured_' does not exist on type
  // 'Grid'.
  g.measured_.marginBottom = 0;
  // @ts-ignore: error TS2339: Property 'measured_' does not exist on type
  // 'Grid'.
  g.measured_.marginLeft = 1;
  // @ts-ignore: error TS2339: Property 'getColumnCount_' does not exist on type
  // 'Grid'.
  columns = g.getColumnCount_();
  // Can fit two columns due to uncollapse margin.
  assertEquals(2, columns);

  // @ts-ignore: error TS2339: Property 'measured_' does not exist on type
  // 'Grid'.
  g.measured_.marginRight = 1;
  // @ts-ignore: error TS2339: Property 'getColumnCount_' does not exist on type
  // 'Grid'.
  columns = g.getColumnCount_();
  // Can not fit two columns due to margin on left and right side.
  assertEquals(1, columns);

  // @ts-ignore: error TS2339: Property 'measured_' does not exist on type
  // 'Grid'.
  g.measured_.marginRight = 0;
  // @ts-ignore: error TS2339: Property 'horizontalPadding_' does not exist on
  // type 'Grid'.
  g.horizontalPadding_ = 2;
  // @ts-ignore: error TS2339: Property 'clientWidthWithoutScrollbar_' does not
  // exist on type 'Grid'.
  g.clientWidthWithoutScrollbar_ = 22;
  // @ts-ignore: error TS2339: Property 'getColumnCount_' does not exist on type
  // 'Grid'.
  columns = g.getColumnCount_();
  // Can fit two columns as (22-2=)20px width is avaiable for grid items.
  assertEquals(2, columns);

  // @ts-ignore: error TS2339: Property 'horizontalPadding_' does not exist on
  // type 'Grid'.
  g.horizontalPadding_ = 3;
  // @ts-ignore: error TS2339: Property 'getColumnCount_' does not exist on type
  // 'Grid'.
  columns = g.getColumnCount_();
  // Can not fit two columns due to bigger horizontal padding.
  assertEquals(1, columns);
}
