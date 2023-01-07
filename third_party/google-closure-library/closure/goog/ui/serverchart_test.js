/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.ServerChartTest');
goog.setTestOnly();

const ServerChart = goog.require('goog.ui.ServerChart');
const Uri = goog.require('goog.Uri');
const events = goog.require('goog.events');
const testSuite = goog.require('goog.testing.testSuite');

function tryToCreateBarChart(bar) {
  bar.addDataSet([8, 23, 7], '008000');
  bar.addDataSet([31, 11, 7], 'ffcc33');
  bar.addDataSet([2, 43, 70, 3, 43, 74], '3072f3');
  bar.setLeftLabels(['', '20K', '', '60K', '', '100K']);
  bar.setXLabels(['O', 'N', 'D']);
  bar.setMaxValue(100);
  const uri = bar.getUri();
  assertEquals('br', uri.getParameterValue(ServerChart.UriParam.TYPE));
  assertEquals('180x104', uri.getParameterValue(ServerChart.UriParam.SIZE));
  assertEquals(
      'e:D6NtDQ,S7F4DQ,AAaxsZApaxvA',
      uri.getParameterValue(ServerChart.UriParam.DATA));
  assertEquals(
      '008000,ffcc33,3072f3',
      uri.getParameterValue(ServerChart.UriParam.DATA_COLORS));
  assertEquals(
      '100K||60K||20K|',
      uri.getParameterValue(ServerChart.UriParam.LEFT_Y_LABELS));
  assertEquals('O|N|D', uri.getParameterValue(ServerChart.UriParam.X_LABELS));
}

testSuite({
  testSchemeIndependentBarChartRequest() {
    const bar = new ServerChart(ServerChart.ChartType.BAR, 180, 104, null);
    tryToCreateBarChart(bar);
    const uri = bar.getUri();
    const schemeIndependentUri =
        new Uri(ServerChart.CHART_SERVER_SCHEME_INDEPENDENT_URI);
    assertEquals('', uri.getScheme());
    assertEquals(schemeIndependentUri.getDomain(), uri.getDomain());
  },

  testHttpBarChartRequest() {
    const bar = new ServerChart(
        ServerChart.ChartType.BAR, 180, 104, null,
        ServerChart.CHART_SERVER_HTTP_URI);
    tryToCreateBarChart(bar);
    const uri = bar.getUri();
    const httpUri = new Uri(ServerChart.CHART_SERVER_HTTP_URI);
    assertEquals('http', uri.getScheme());
    assertEquals(httpUri.getDomain(), uri.getDomain());
  },

  testHttpsBarChartRequest() {
    const bar = new ServerChart(
        ServerChart.ChartType.BAR, 180, 104, null,
        ServerChart.CHART_SERVER_HTTPS_URI);
    tryToCreateBarChart(bar);
    const uri = bar.getUri();
    const httpsUri = new Uri(ServerChart.CHART_SERVER_HTTPS_URI);
    assertEquals('https', uri.getScheme());
    assertEquals(httpsUri.getDomain(), uri.getDomain());
  },

  testMinValue() {
    const pie = new ServerChart(ServerChart.ChartType.PIE3D, 180, 104);
    pie.addDataSet([1, 2, 3], '000000');
    assertEquals(pie.getMinValue(), 0);

    const line = new ServerChart(ServerChart.ChartType.LINE, 180, 104);
    line.addDataSet([1, 2, 3], '000000');
    assertEquals(line.getMinValue(), 1);
  },

  testMargins() {
    const pie = new ServerChart(ServerChart.ChartType.PIE3D, 180, 104);
    pie.setMargins(1, 2, 3, 4);
    assertEquals(
        '1,2,3,4',
        pie.getUri().getParameterValue(ServerChart.UriParam.MARGINS));
  },

  testSetParameterValue() {
    const scatter = new ServerChart(ServerChart.ChartType.SCATTER, 180, 104);
    const key = ServerChart.UriParam.DATA_COLORS;
    const value = '000000,FF0000|00FF00|0000FF';
    scatter.setParameterValue(key, value);

    assertEquals(
        'unexpected parameter value', value,
        scatter.getUri().getParameterValue(key));

    scatter.removeParameter(key);

    assertUndefined(
        'parameter not removed', scatter.getUri().getParameterValue(key));
  },

  testTypes() {
    let chart;

    chart = new ServerChart(ServerChart.ChartType.CONCENTRIC_PIE, 180, 104);

    assertTrue(chart.isPieChart());
    assertFalse(chart.isBarChart());
    assertFalse(chart.isMap());
    assertFalse(chart.isLineChart());

    chart =
        new ServerChart(ServerChart.ChartType.HORIZONTAL_GROUPED_BAR, 180, 104);

    assertFalse(chart.isPieChart());
    assertTrue(chart.isBarChart());
    assertTrue(chart.isHorizontalBarChart());
    assertTrue(chart.isGroupedBarChart());
    assertFalse(chart.isVerticalBarChart());
    assertFalse(chart.isStackedBarChart());

    chart = new ServerChart(ServerChart.ChartType.BAR, 180, 104);
    assertTrue(chart.isBarChart());
    assertTrue(chart.isStackedBarChart());
    assertFalse(chart.isGroupedBarChart());

    chart = new ServerChart(ServerChart.ChartType.XYLINE, 180, 104);
    assertTrue('I thought lxy was a line chart', chart.isLineChart());
    assertFalse('lxy is definitely not a pie chart', chart.isPieChart());
  },

  testBarChartRequest() {
    const bar = new ServerChart(ServerChart.ChartType.BAR, 180, 104);
    tryToCreateBarChart(bar);
    const httpUri = new Uri(ServerChart.CHART_SERVER_URI);
    const uri = bar.getUri();
    assertEquals(httpUri.getDomain(), uri.getDomain());
  },

  testClearDataSets() {
    let chart = new ServerChart(ServerChart.ChartType.BAR, 180, 104);
    tryToCreateBarChart(chart);
    const uriBefore = chart.getUri();
    chart.clearDataSets();
    chart = new ServerChart(ServerChart.ChartType.BAR, 180, 104);
    tryToCreateBarChart(chart);
    const uriAfter = chart.getUri();
    assertEquals(uriBefore.getScheme(), uriAfter.getScheme());
    assertEquals(uriBefore.getDomain(), uriAfter.getDomain());
    assertEquals(uriBefore.getPath(), uriAfter.getPath());
  },

  testMultipleDatasetsTextEncoding() {
    const chart = new ServerChart(ServerChart.ChartType.BAR, 180, 104);
    chart.setEncodingType(ServerChart.EncodingType.TEXT);
    chart.addDataSet([0, 25, 100], '008000');
    chart.addDataSet([12, 2, 7.1], '112233');
    chart.addDataSet([82, 16, 2], '3072f3');
    const uri = chart.getUri();
    assertEquals(
        't:0,25,100|12,2,7.1|82,16,2',
        uri.getParameterValue(ServerChart.UriParam.DATA));
  },

  testVennDiagramRequest() {
    const venn = new ServerChart(ServerChart.ChartType.VENN, 300, 200);
    venn.setTitle('Google Employees');
    const weights = [
      80,  // Size of circle A
      60,  // Size of circle B
      40,  // Size of circle C
      20,  // Overlap of A and B
      20,  // Overlap of A and C
      20,  // Overlap of B and C
      5,
    ];  // Overlap of A, B and C
    const labels = [
      'C Hackers',   // Label for A
      'LISP Gurus',  // Label for B
      'Java Jockeys',
    ];  // Label for C
    venn.setVennSeries(weights, labels);
    const uri = venn.getUri();
    const httpUri = new Uri(ServerChart.CHART_SERVER_URI);
    assertEquals(httpUri.getDomain(), uri.getDomain());
    assertEquals('v', uri.getParameterValue(ServerChart.UriParam.TYPE));
    assertEquals('300x200', uri.getParameterValue(ServerChart.UriParam.SIZE));
    assertEquals(
        'e:..u7d3MzMzMzAA', uri.getParameterValue(ServerChart.UriParam.DATA));
    assertEquals(
        'Google Employees', uri.getParameterValue(ServerChart.UriParam.TITLE));
    assertEquals(
        labels.join('|'),
        uri.getParameterValue(ServerChart.UriParam.LEGEND_TEXTS));
  },

  testSparklineChartRequest() {
    const chart = new ServerChart(ServerChart.ChartType.SPARKLINE, 300, 200);
    chart.addDataSet([8, 23, 7], '008000');
    chart.addDataSet([31, 11, 7], 'ffcc33');
    chart.addDataSet([2, 43, 70, 3, 43, 74], '3072f3');
    chart.setLeftLabels(['', '20K', '', '60K', '', '100K']);
    chart.setXLabels(['O', 'N', 'D']);
    chart.setMaxValue(100);
    const uri = chart.getUri();
    assertEquals('ls', uri.getParameterValue(ServerChart.UriParam.TYPE));
    assertEquals('300x200', uri.getParameterValue(ServerChart.UriParam.SIZE));
    assertEquals(
        'e:D6NtDQ,S7F4DQ,AAaxsZApaxvA',
        uri.getParameterValue(ServerChart.UriParam.DATA));
    assertEquals(
        '008000,ffcc33,3072f3',
        uri.getParameterValue(ServerChart.UriParam.DATA_COLORS));
    assertEquals(
        '100K||60K||20K|',
        uri.getParameterValue(ServerChart.UriParam.LEFT_Y_LABELS));
    assertEquals('O|N|D', uri.getParameterValue(ServerChart.UriParam.X_LABELS));
  },

  testLegendPositionRequest() {
    const chart = new ServerChart(ServerChart.ChartType.SPARKLINE, 300, 200);
    chart.addDataSet([0, 100], '008000', 'foo');
    chart.setLegendPosition(ServerChart.LegendPosition.TOP);
    assertEquals('t', chart.getLegendPosition());
    const uri = chart.getUri();
    assertEquals(
        't', uri.getParameterValue(ServerChart.UriParam.LEGEND_POSITION));
  },

  testSetGridParameter() {
    const gridArg = '20,20,4,4';
    const chart = new ServerChart(ServerChart.ChartType.SPARKLINE, 300, 200);
    chart.addDataSet([0, 100], '008000', 'foo');
    chart.setGridParameter(gridArg);
    assertEquals(gridArg, chart.getGridParameter());
    const uri = chart.getUri();
    assertEquals(gridArg, uri.getParameterValue(ServerChart.UriParam.GRID));
  },

  testSetMarkerParameter() {
    const markerArg = 's,FF0000,0,-1,5';
    const chart = new ServerChart(ServerChart.ChartType.SPARKLINE, 300, 200);
    chart.addDataSet([0, 100], '008000', 'foo');
    chart.setMarkerParameter(markerArg);
    assertEquals(markerArg, chart.getMarkerParameter());
    const uri = chart.getUri();
    assertEquals(
        markerArg, uri.getParameterValue(ServerChart.UriParam.MARKERS));
  },

  testNullDataPointRequest() {
    let chart = new ServerChart(ServerChart.ChartType.SPARKLINE, 300, 200);
    chart.addDataSet([40, null, 10], '008000');
    assertEquals(10, chart.getMinValue());
    assertEquals(40, chart.getMaxValue());
    let uri = chart.getUri();
    assertEquals('e:..__AA', uri.getParameterValue(ServerChart.UriParam.DATA));

    chart = new ServerChart(ServerChart.ChartType.SPARKLINE, 300, 200);
    chart.addDataSet([-5, null, -1], '008000');
    assertEquals(-5, chart.getMinValue());
    assertEquals(-1, chart.getMaxValue());
    uri = chart.getUri();
    assertEquals('e:AA__..', uri.getParameterValue(ServerChart.UriParam.DATA));
  },

  testSetBarSpaceWidths() {
    const noSpaceBetweenBarsSpecified = '20';
    const noSpaceBetweenBarsChart =
        new ServerChart(ServerChart.ChartType.VERTICAL_STACKED_BAR);
    noSpaceBetweenBarsChart.setBarSpaceWidths(20);
    let uri = noSpaceBetweenBarsChart.getUri();
    assertEquals(
        noSpaceBetweenBarsSpecified,
        uri.getParameterValue(ServerChart.UriParam.BAR_HEIGHT));

    const spaceBetweenBarsSpecified = '20,5';
    const spaceBetweenBarsChart =
        new ServerChart(ServerChart.ChartType.HORIZONTAL_STACKED_BAR);
    spaceBetweenBarsChart.setBarSpaceWidths(20, 5);
    uri = spaceBetweenBarsChart.getUri();
    assertEquals(
        spaceBetweenBarsSpecified,
        uri.getParameterValue(ServerChart.UriParam.BAR_HEIGHT));

    const spaceBetweenGroupsSpecified = '20,5,6';
    const spaceBetweenGroupsChart =
        new ServerChart(ServerChart.ChartType.HORIZONTAL_STACKED_BAR);
    spaceBetweenGroupsChart.setBarSpaceWidths(20, 5, 6);
    uri = spaceBetweenGroupsChart.getUri();
    assertEquals(
        spaceBetweenGroupsSpecified,
        uri.getParameterValue(ServerChart.UriParam.BAR_HEIGHT));

    const groupsButNotBarsSpecified = '20,6';
    const groupsButNotBarsChart =
        new ServerChart(ServerChart.ChartType.HORIZONTAL_STACKED_BAR);
    groupsButNotBarsChart.setBarSpaceWidths(20, undefined, 6);
    uri = groupsButNotBarsChart.getUri();
    assertEquals(
        groupsButNotBarsSpecified,
        uri.getParameterValue(ServerChart.UriParam.BAR_HEIGHT));
  },

  testSetAutomaticBarWidth() {
    const noSpaceBetweenBarsSpecified = 'a';
    const noSpaceBetweenBarsChart =
        new ServerChart(ServerChart.ChartType.VERTICAL_STACKED_BAR);
    noSpaceBetweenBarsChart.setAutomaticBarWidth();
    let uri = noSpaceBetweenBarsChart.getUri();
    assertEquals(
        noSpaceBetweenBarsSpecified,
        uri.getParameterValue(ServerChart.UriParam.BAR_HEIGHT));

    const spaceBetweenBarsSpecified = 'a,5';
    const spaceBetweenBarsChart =
        new ServerChart(ServerChart.ChartType.HORIZONTAL_STACKED_BAR);
    spaceBetweenBarsChart.setAutomaticBarWidth(5);
    uri = spaceBetweenBarsChart.getUri();
    assertEquals(
        spaceBetweenBarsSpecified,
        uri.getParameterValue(ServerChart.UriParam.BAR_HEIGHT));

    const spaceBetweenGroupsSpecified = 'a,5,6';
    const spaceBetweenGroupsChart =
        new ServerChart(ServerChart.ChartType.HORIZONTAL_STACKED_BAR);
    spaceBetweenGroupsChart.setAutomaticBarWidth(5, 6);
    uri = spaceBetweenGroupsChart.getUri();
    assertEquals(
        spaceBetweenGroupsSpecified,
        uri.getParameterValue(ServerChart.UriParam.BAR_HEIGHT));

    const groupsButNotBarsSpecified = 'a,6';
    const groupsButNotBarsChart =
        new ServerChart(ServerChart.ChartType.HORIZONTAL_STACKED_BAR);
    groupsButNotBarsChart.setAutomaticBarWidth(undefined, 6);
    uri = groupsButNotBarsChart.getUri();
    assertEquals(
        groupsButNotBarsSpecified,
        uri.getParameterValue(ServerChart.UriParam.BAR_HEIGHT));
  },

  testSetDataScaling() {
    const dataScalingArg = '0,160';
    const dataArg = 't:0,50,100,130';
    const chart =
        new ServerChart(ServerChart.ChartType.VERTICAL_STACKED_BAR, 300, 200);
    chart.addDataSet([0, 50, 100, 130], '008000');
    chart.setDataScaling(0, 160);
    const uri = chart.getUri();
    assertEquals(
        dataScalingArg,
        uri.getParameterValue(ServerChart.UriParam.DATA_SCALING));
    assertEquals(dataArg, uri.getParameterValue(ServerChart.UriParam.DATA));
  },

  testSetMultiAxisLabelStyle() {
    const noFontSizeChart =
        new ServerChart(ServerChart.ChartType.HORIZONTAL_STACKED_BAR, 300, 200);
    noFontSizeChart.addDataSet([0, 50, 100, 130], '008000');
    let axisNumber =
        noFontSizeChart.addMultiAxis(ServerChart.MultiAxisType.LEFT_Y_AXIS);
    const noFontSizeArgs = `${axisNumber},009000`;
    noFontSizeChart.setMultiAxisLabelStyle(axisNumber, '009000');
    const noFontSizeUri = noFontSizeChart.getUri();
    assertEquals(
        noFontSizeArgs,
        noFontSizeUri.getParameterValue(ServerChart.UriParam.MULTI_AXIS_STYLE));

    const noAlignChart =
        new ServerChart(ServerChart.ChartType.HORIZONTAL_STACKED_BAR, 300, 200);
    noAlignChart.addDataSet([0, 50, 100, 130], '008000');
    const xAxisNumber =
        noAlignChart.addMultiAxis(ServerChart.MultiAxisType.X_AXIS);
    const yAxisNumber =
        noAlignChart.addMultiAxis(ServerChart.MultiAxisType.LEFT_Y_AXIS);
    const noAlignArgs = `${xAxisNumber},009000,12|${yAxisNumber},007000,14`;
    noAlignChart.setMultiAxisLabelStyle(xAxisNumber, '009000', 12);
    noAlignChart.setMultiAxisLabelStyle(yAxisNumber, '007000', 14);
    const noAlignUri = noAlignChart.getUri();
    assertEquals(
        noAlignArgs,
        noAlignUri.getParameterValue(ServerChart.UriParam.MULTI_AXIS_STYLE));

    const noLineTicksChart =
        new ServerChart(ServerChart.ChartType.HORIZONTAL_STACKED_BAR, 300, 200);
    noLineTicksChart.addDataSet([0, 50, 100, 130], '008000');
    axisNumber =
        noLineTicksChart.addMultiAxis(ServerChart.MultiAxisType.LEFT_Y_AXIS);
    const noLineTicksArgs = `${axisNumber},009000,12,0`;
    noLineTicksChart.setMultiAxisLabelStyle(
        axisNumber, '009000', 12, ServerChart.MultiAxisAlignment.ALIGN_CENTER);
    const noLineTicksUri = noLineTicksChart.getUri();
    assertEquals(
        noLineTicksArgs,
        noLineTicksUri.getParameterValue(
            ServerChart.UriParam.MULTI_AXIS_STYLE));

    const allParamsChart =
        new ServerChart(ServerChart.ChartType.HORIZONTAL_STACKED_BAR, 300, 200);
    allParamsChart.addDataSet([0, 50, 100, 130], '008000');
    axisNumber =
        allParamsChart.addMultiAxis(ServerChart.MultiAxisType.LEFT_Y_AXIS);
    const allParamsArgs = `${axisNumber},009000,12,0,lt`;
    allParamsChart.setMultiAxisLabelStyle(
        axisNumber, '009000', 12, ServerChart.MultiAxisAlignment.ALIGN_CENTER,
        ServerChart.AxisDisplayType.LINE_AND_TICKS);
    const allParamsUri = allParamsChart.getUri();
    assertEquals(
        allParamsArgs,
        allParamsUri.getParameterValue(ServerChart.UriParam.MULTI_AXIS_STYLE));
  },

  testSetBackgroundFill() {
    const chart =
        new ServerChart(ServerChart.ChartType.HORIZONTAL_STACKED_BAR, 300, 200);
    assertEquals(0, chart.getBackgroundFill().length);
    chart.setBackgroundFill([{color: '00ff00'}]);
    assertObjectEquals(
        {area: 'bg', effect: 's', color: '00ff00'},
        chart.getBackgroundFill()[0]);
    chart.setBackgroundFill([{color: '00ff00'}, {area: 'c', color: '00ff00'}]);
    assertObjectEquals(
        {area: 'bg', effect: 's', color: '00ff00'},
        chart.getBackgroundFill()[0]);
    assertObjectEquals(
        {area: 'c', effect: 's', color: '00ff00'},
        chart.getBackgroundFill()[1]);

    chart.setParameterValue(
        ServerChart.UriParam.BACKGROUND_FILL,
        'bg,s,00ff00|c,lg,45,ff00ff|bg,s,ff00ff');
    assertEquals(0, chart.getBackgroundFill().length);
  },

  testSetMultiAxisRange() {
    const chart =
        new ServerChart(ServerChart.ChartType.HORIZONTAL_STACKED_BAR, 300, 200);
    const x = chart.addMultiAxis(ServerChart.MultiAxisType.X_AXIS);
    const top = chart.addMultiAxis(ServerChart.MultiAxisType.TOP_AXIS);
    chart.setMultiAxisRange(x, -500, 500, 100);
    chart.setMultiAxisRange(top, 0, 10);
    const range = chart.getMultiAxisRange();

    assertArrayEquals(range[x], [-500, 500, 100]);
    assertArrayEquals(range[top], [0, 10]);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testGetConvertedValue() {
    const chart = new ServerChart(ServerChart.ChartType.VERTICAL_STACKED_BAR);

    assertThrows('No exception thrown when minValue > maxValue', () => {
      /** @suppress {visibility} suppression added to enable type checking */
      const result =
          chart.getConvertedValue_(90, 24, 3, ServerChart.EncodingType.SIMPLE);
    });

    assertEquals(
        '_',
        chart.getConvertedValue_(
            90, 100, 101, ServerChart.EncodingType.SIMPLE));

    assertEquals(
        '_',
        chart.getConvertedValue_(null, 0, 5, ServerChart.EncodingType.SIMPLE));
    assertEquals(
        '__',
        chart.getConvertedValue_(
            null, 0, 150, ServerChart.EncodingType.EXTENDED));
    assertEquals(
        '24',
        chart.getConvertedValue_(24, 1, 200, ServerChart.EncodingType.TEXT));
    assertEquals(
        'H',
        chart.getConvertedValue_(24, 1, 200, ServerChart.EncodingType.SIMPLE));
    assertEquals(
        'HZ',
        chart.getConvertedValue_(
            24, 1, 200, ServerChart.EncodingType.EXTENDED));

    // Out-of-range values should give a missing data point, not an empty
    // string.
    assertEquals(
        '__',
        chart.getConvertedValue_(0, 1, 200, ServerChart.EncodingType.EXTENDED));
    assertEquals(
        '__',
        chart.getConvertedValue_(
            201, 1, 200, ServerChart.EncodingType.EXTENDED));
    assertEquals(
        '_',
        chart.getConvertedValue_(0, 1, 200, ServerChart.EncodingType.SIMPLE));
    assertEquals(
        '_',
        chart.getConvertedValue_(201, 1, 200, ServerChart.EncodingType.SIMPLE));
    assertEquals(
        '_',
        chart.getConvertedValue_(0, 1, 200, ServerChart.EncodingType.TEXT));
    assertEquals(
        '_',
        chart.getConvertedValue_(201, 1, 200, ServerChart.EncodingType.TEXT));
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testGetChartServerValues() {
    const chart = new ServerChart(ServerChart.ChartType.HORIZONTAL_STACKED_BAR);
    const values = [0, 1, 2, 56, 90, 120];
    const minValue = 0;
    const maxValue = 140;
    const expectedSimple = 'AABYn0';
    assertEquals(
        expectedSimple,
        chart.getChartServerValues_(values, minValue, maxValue));
    const expectedText = '0,1,2,56,90,120';
    assertEquals(
        expectedSimple,
        chart.getChartServerValues_(values, minValue, maxValue));
  },

  testUriLengthLimit() {
    const chart = new ServerChart(ServerChart.ChartType.SPARKLINE, 300, 200);
    let longUri = null;
    events.listen(chart, ServerChart.Event.URI_TOO_LONG, (e) => {
      longUri = e.uri;
    });
    assertEquals(ServerChart.EncodingType.AUTOMATIC, chart.getEncodingType());
    chart.addDataSet(
        [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9], '008000');
    assertEquals(
        'e:AAHHOOVVccjjqqxx44..AAHHOOVVccjjqqxx44..',
        chart.getUri().getParameterValue(ServerChart.UriParam.DATA));
    chart.setUriLengthLimit(100);
    assertEquals(
        's:AHOUbipv29AHOUbipv29',
        chart.getUri().getParameterValue(ServerChart.UriParam.DATA));
    chart.setUriLengthLimit(80);
    assertEquals(null, longUri);
    chart.getUri();
    assertNotEquals(null, longUri);
  },

  testVisibleDataSets() {
    let uri;

    const bar = new ServerChart(ServerChart.ChartType.BAR, 180, 104);
    bar.addDataSet([8, 23, 7], '008000');
    bar.addDataSet([31, 11, 7], 'ffcc33');
    bar.addDataSet([2, 43, 70, 3, 43, 74], '3072f3');
    bar.setMaxValue(100);

    bar.setNumVisibleDataSets(0);
    uri = bar.getUri();
    assertEquals(
        'e0:D6NtDQ,S7F4DQ,AAaxsZApaxvA',
        uri.getParameterValue(ServerChart.UriParam.DATA));

    bar.setNumVisibleDataSets(1);
    uri = bar.getUri();
    assertEquals(
        'e1:D6NtDQ,S7F4DQ,AAaxsZApaxvA',
        uri.getParameterValue(ServerChart.UriParam.DATA));

    bar.setNumVisibleDataSets(2);
    uri = bar.getUri();
    assertEquals(
        'e2:D6NtDQ,S7F4DQ,AAaxsZApaxvA',
        uri.getParameterValue(ServerChart.UriParam.DATA));

    bar.setNumVisibleDataSets(null);
    uri = bar.getUri();
    assertEquals(
        'e:D6NtDQ,S7F4DQ,AAaxsZApaxvA',
        uri.getParameterValue(ServerChart.UriParam.DATA));
  },

  testTitle() {
    const chart = new ServerChart(ServerChart.ChartType.BAR, 180, 104);
    assertEquals('Default title size', 13.5, chart.getTitleSize());
    assertEquals('Default title color', '333333', chart.getTitleColor());
    chart.setTitle('Test title');
    chart.setTitleSize(7);
    chart.setTitleColor('ff0000');
    const uri = chart.getUri();
    assertEquals(
        'Changing chart title failed', 'Test title',
        uri.getParameterValue(ServerChart.UriParam.TITLE));
    assertEquals(
        'Changing title size and color failed', 'ff0000,7',
        uri.getParameterValue(ServerChart.UriParam.TITLE_FORMAT));
    assertEquals('New title size', 7, chart.getTitleSize());
    assertEquals('New title color', 'ff0000', chart.getTitleColor());
  },
});
