// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Adds a xf-display-panel element to the test page.
 */
function setUpPage() {
  document.body.innerHTML +=
      '<xf-display-panel id="test-xf-display-panel"></xf-display-panel>';
}

function tearDown() {
  /** @type {!DisplayPanel|!Element} */
  const displayPanel = assert(document.querySelector('#test-xf-display-panel'));
  displayPanel.removeAllPanelItems();
}

async function testDisplayPanelAttachPanel(done) {
  // Get the host display panel container element.
  /** @type {!DisplayPanel|!Element} */
  const displayPanel = assert(document.querySelector('#test-xf-display-panel'));

  // Test create/attach/remove sequences.
  // Create a progress panel item.
  let progressPanel = displayPanel.createPanelItem('testpanel');
  progressPanel.panelType = progressPanel.panelTypeProgress;

  // Attach the panel to it's host display panel.
  displayPanel.attachPanelItem(progressPanel);

  // Verify the panel is attached to the document.
  assertTrue(!!progressPanel.isConnected);

  // Remove the panel item.
  displayPanel.removePanelItem(progressPanel);

  // Verify the panel item is not attached to the document.
  assertFalse(progressPanel.isConnected);

  // Create a progress panel item.
  progressPanel = displayPanel.createPanelItem('testpanel2');
  progressPanel.panelType = progressPanel.panelTypeProgress;

  // Remove the panel item.
  displayPanel.removePanelItem(progressPanel);

  // Try to attach the removed panel to it's host display panel.
  displayPanel.attachPanelItem(progressPanel);

  // Verify the panel is not attached to the document.
  assertFalse(progressPanel.isConnected);

  done();
}

async function testDisplayPanelChangingPanelTypes(done) {
  // Get the host display panel container element.
  /** @type {!DisplayPanel|!Element} */
  const displayPanel = assert(document.querySelector('#test-xf-display-panel'));
  const panelItem = displayPanel.addPanelItem('testpanel');
  panelItem.panelType = panelItem.panelTypeProgress;

  // Setup the panel item signal (click) callback.
  /** @type {?string} */
  let signal = null;
  panelItem.signalCallback = (name) => {
    assert(typeof name === 'string');
    signal = name;
  };

  // Verify the panel item indicator is progress.
  assertEquals(
      panelItem.indicator, 'progress',
      'Wrong panel indicator, got ' + panelItem.indicator);

  // Check cancel signal from the panel from a click.
  /** @type {!HTMLElement} */
  const cancel = assert(panelItem.secondaryButton);
  cancel.click();
  assertEquals(
      signal, 'cancel', 'Expected signal name "cancel". Got ' + signal);

  // Change the panel item to an error panel.
  panelItem.panelType = panelItem.panelTypeError;

  // Verify the panel item indicator is set to error.
  assertEquals(
      panelItem.indicator, 'status',
      'Wrong panel indicator, got ' + panelItem.indicator);
  assertEquals(
      panelItem.status, 'failure',
      'Wrong panel status, got ' + panelItem.status);

  // Check dismiss signal from the panel from a click.
  /** @type {!HTMLElement} */
  let dismiss = assert(panelItem.secondaryButton);
  dismiss.click();
  assertEquals(
      signal, 'dismiss', 'Expected signal name "dismiss". Got ' + signal);

  // Change the panel type to a done panel.
  panelItem.panelType = panelItem.panelTypeDone;

  // Verify the panel item indicator is set to done.
  assertEquals(
      panelItem.indicator, 'status',
      'Wrong panel indicator, got ' + panelItem.indicator);
  assertEquals(
      panelItem.status, 'success',
      'Wrong panel status, got ' + panelItem.status);

  // Check the dimiss signal from the panel from a click.
  signal = 'none';
  dismiss = assert(panelItem.primaryButton);
  dismiss.click();
  assertEquals(
      signal, 'dismiss', 'Expected signal name "dismiss". Got ' + signal);

  // Change the type to a summary panel.
  panelItem.panelType = panelItem.panelTypeSummary;

  // Verify the panel item indicator is largeprogress.
  assertEquals(
      panelItem.indicator, 'largeprogress',
      'Wrong panel indicator, got ' + panelItem.indicator);

  // Check no signal emitted from the summary panel from a click.
  const expand = assert(panelItem.primaryButton);
  signal = 'none';
  expand.click();
  assertEquals(signal, 'none', 'Expected no signal. Got ' + signal);

  done();
}

function testFilesDisplayPanelErrorText() {
  // Get the host display panel container element.
  /** @type {!DisplayPanel|!Element} */
  const displayPanel = assert(document.querySelector('#test-xf-display-panel'));

  // Add a panel item to the display panel container.
  const panelItem = displayPanel.addPanelItem('testpanel');

  // Change the primary and secondary text on the panel item.
  panelItem.primaryText = 'foo';
  panelItem.secondaryText = 'bar';

  // Check the primary and secondary text has been set on the panel.
  assertEquals('foo', panelItem.primaryText);
  assertEquals('bar', panelItem.secondaryText);

  // Change the panel item type to an error panel.
  panelItem.panelType = panelItem.panelTypeError;

  // Check the default primary text displays a generic error message.
  // Note, the i18n message gets smooshed into 'An error occurred.' in the app.
  assertEquals('$i18n{FILE_ERROR_GENERIC}', panelItem.primaryText);

  // Check the secondary text is empty.
  assertEquals('', panelItem.secondaryText);
}

// Override the formatting function for unit testing.
util.strf = (end, option) => {
  return option + end;
};

function testFilesDisplayPanelErrorMarker() {
  // Get the host display panel container element.
  /** @type {!DisplayPanel|!Element} */
  const displayPanel = assert(document.querySelector('#test-xf-display-panel'));

  // Add a summary panel item to the display panel container.
  const summaryPanel = displayPanel.addPanelItem('testpanel');
  summaryPanel.panelType = summaryPanel.panelTypeSummary;

  // Confirm the error marker is not visible by default.
  const progressIndicator =
      summaryPanel.shadowRoot.querySelector('xf-circular-progress');
  const errorMarker = progressIndicator.shadowRoot.querySelector('.errormark');
  assertEquals(
      errorMarker.getAttribute('visibility'), 'hidden',
      'Summary panel error marker should default to hidden');

  // Confirm the error marker can be made visible through property setting on
  // the progress indicator.
  progressIndicator.errorMarkerVisibility = 'visible';
  assertEquals(
      errorMarker.getAttribute('visibility'), 'visible',
      'Summary panel error marker should be visible');

  // Confirm we can change visibility of the error marker from panel item
  // property setting.
  summaryPanel.errorMarkerVisibility = 'hidden';
  assertEquals(
      errorMarker.getAttribute('visibility'), 'hidden',
      'Summary panel error marker should be hidden');

  // Confirm the panel item reflects the visibility of the error marker.
  assertEquals(
      summaryPanel.errorMarkerVisibility, 'hidden',
      'Summary panel error marker property is wrong, should be "hidden"');
}

function testFilesDisplayPanelMixedSummary() {
  // Get the host display panel container element.
  /** @type {!DisplayPanel|!Element} */
  const displayPanel = assert(document.querySelector('#test-xf-display-panel'));

  // Add an error panel item to the display panel container.
  let errorPanel = displayPanel.addPanelItem('testpanel1');
  errorPanel.panelType = errorPanel.panelTypeError;
  assertEquals('status', errorPanel.indicator);

  // Add a progress panel item to the display panel container.
  const progressPanel = displayPanel.addPanelItem('testpanel2');
  progressPanel.panelType = progressPanel.panelTypeProgress;

  // Verify a summary panel item is created and shows the error indicator.
  const summaryContainer = displayPanel.shadowRoot.querySelector('#summary');
  let summaryPanelItem = summaryContainer.querySelector('xf-panel-item');
  assertEquals(summaryPanelItem.panelTypeSummary, summaryPanelItem.panelType);
  assertEquals('largeprogress', summaryPanelItem.indicator);
  assertEquals('visible', summaryPanelItem.errorMarkerVisibility);

  // Remove the error panel item and add a second progress panel item.
  displayPanel.removePanelItem(errorPanel);
  const extraProgressPanel = displayPanel.addPanelItem('testpanel3');
  extraProgressPanel.panelType = extraProgressPanel.panelTypeProgress;

  // Verify a summary panel item is created without an error indicator.
  summaryPanelItem = summaryContainer.querySelector('xf-panel-item');
  assertEquals(summaryPanelItem.panelTypeSummary, summaryPanelItem.panelType);
  assertEquals('largeprogress', summaryPanelItem.indicator);
  assertEquals('hidden', summaryPanelItem.errorMarkerVisibility);

  // Remove the progress panel items and add 2 error panel items.
  displayPanel.removePanelItem(progressPanel);
  displayPanel.removePanelItem(extraProgressPanel);
  errorPanel = displayPanel.addPanelItem('testpanel4');
  errorPanel.panelType = errorPanel.panelTypeError;
  const extraErrorPanel = displayPanel.addPanelItem('testpanel5');
  extraErrorPanel.panelType = extraErrorPanel.panelTypeError;

  // Verify a summary panel item is shown, with an error status indicator.
  summaryPanelItem = summaryContainer.querySelector('xf-panel-item');
  assertEquals(summaryPanelItem.panelTypeSummary, summaryPanelItem.panelType);
  assertEquals('status', summaryPanelItem.indicator);
  assertEquals('failure', summaryPanelItem.status);

  // Remove the error panel items and add 2 done (a.k.a. success) panel items.
  displayPanel.removeAllPanelItems();
  const donePanel = displayPanel.addPanelItem('testpanel6');
  donePanel.panelType = donePanel.panelTypeDone;
  const extraDonePanel = displayPanel.addPanelItem('testpanel7');
  extraDonePanel.panelType = extraDonePanel.panelTypeDone;

  // Verify a summary panel is shown with success indicator.
  summaryPanelItem = summaryContainer.querySelector('xf-panel-item');
  assertEquals(summaryPanelItem.panelTypeSummary, summaryPanelItem.panelType);
  assertEquals('status', summaryPanelItem.indicator);
  assertEquals('success', summaryPanelItem.status);
}

function testFilesDisplayPanelCircularProgress() {
  // Get the host display panel container element.
  /** @type {!DisplayPanel|!Element} */
  const displayPanel = assert(document.querySelector('#test-xf-display-panel'));

  // Add a progress panel item to the display panel container.
  const progressPanel = displayPanel.addPanelItem('testpanel1');
  progressPanel.panelType = progressPanel.panelTypeProgress;

  // Verify the circular progress panel item marker stroke width.
  const circularProgress = progressPanel.shadowRoot.querySelector('#indicator');
  const strokeWidthContainerGroup =
      circularProgress.shadowRoot.querySelector('#circles');
  assertEquals('3', strokeWidthContainerGroup.getAttribute('stroke-width'));

  // Verify setting large radius on the circular marker changes stroke width.
  circularProgress.setAttribute('radius', '11');
  assertEquals('4', strokeWidthContainerGroup.getAttribute('stroke-width'));
}

async function testFilesDisplayPanelSummaryPanel(done) {
  // Get the host display panel container element.
  /** @type {!DisplayPanel|!Element} */
  const displayPanel = assert(document.querySelector('#test-xf-display-panel'));

  // Declare the expected panel item height managed from CSS.
  const singlePanelHeight = 68;

  // Add a progress panel item to the display panel container.
  let progressPanel = displayPanel.addPanelItem('testpanel1');
  progressPanel.panelType = progressPanel.panelTypeProgress;

  // Confirm the expected height of a single panelItem.
  let bounds = progressPanel.getBoundingClientRect();
  assert(bounds.height === singlePanelHeight);

  // Add second panel item.
  progressPanel = displayPanel.addPanelItem('testpanel2');
  progressPanel.panelType = progressPanel.panelTypeProgress;

  // Confirm multiple progress panel items are hidden by default.
  const panelContainer = displayPanel.shadowRoot.querySelector('#panels');
  assertTrue(panelContainer.hasAttribute('hidden'));

  // Confirm multiple progress panels cause creation of a summary panel.
  const summaryContainer = displayPanel.shadowRoot.querySelector('#summary');
  let summaryPanelItem = summaryContainer.querySelector('xf-panel-item');
  assertEquals(summaryPanelItem.panelType, summaryPanelItem.panelTypeSummary);

  // Confirm the expected height of the summary panel.
  bounds = summaryPanelItem.getBoundingClientRect();
  assert(bounds.height === singlePanelHeight);

  // Trigger expand of the summary panel.
  let expandButton =
      summaryPanelItem.shadowRoot.querySelector('#primary-action');
  expandButton.click();

  // Confirm the panel container is no longer hidden.
  assertFalse(panelContainer.hasAttribute('hidden'));

  // Remove a progress panel and ensure the summary panel is removed.
  let panelToRemove = displayPanel.findPanelItemById('testpanel1');
  displayPanel.removePanelItem(panelToRemove);
  summaryPanelItem = summaryContainer.querySelector('xf-panel-item');
  assertEquals(summaryPanelItem, null);

  // Confirm the reference to the removed panel item is gone.
  assertEquals(displayPanel.findPanelItemById('testpanel1'), null);

  // Add another panel item and confirm the expanded state persists by
  // checking the panel container is not hidden and there is a summary panel.
  progressPanel = displayPanel.addPanelItem('testpanel1');
  progressPanel.panelType = progressPanel.panelTypeProgress;
  assertFalse(panelContainer.hasAttribute('hidden'));
  summaryPanelItem = summaryContainer.querySelector('xf-panel-item');
  assertEquals(summaryPanelItem.panelType, summaryPanelItem.panelTypeSummary);

  done();
}
