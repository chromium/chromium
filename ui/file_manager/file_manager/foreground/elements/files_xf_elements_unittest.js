// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {waitUntil} from '../../common/js/test_error_reporting.js';
import {str, util} from '../../common/js/util.js';

import {DisplayPanel} from './xf_display_panel.js';

/** @type {!DisplayPanel|!Element} */
let displayPanel;

/**
 * Adds a xf-display-panel element to the test page.
 */
export function setUp() {
  document.body.innerHTML +=
      '<xf-display-panel id="test-xf-display-panel"></xf-display-panel>';
  displayPanel = assert(document.querySelector('#test-xf-display-panel'));
}

export function tearDown() {
  displayPanel.removeAllPanelItems();
}

/**
 * Tests that adding and removing panels to <xf-display-panel> updates the
 * aria-hidden attribute.
 */
export function testDisplayPanelAriaHidden() {
  // Starts without any panel so should be hidden;
  assertEquals(displayPanel.getAttribute('aria-hidden'), 'true');

  // Create a panel, but since it isn't attached so the container should still
  // be hidden.
  const progressPanel = displayPanel.createPanelItem('testpanel');
  assertEquals(displayPanel.getAttribute('aria-hidden'), 'true');

  // Attach the Panel. It should make the container visible.
  displayPanel.attachPanelItem(progressPanel);
  assertEquals(displayPanel.getAttribute('aria-hidden'), 'false');

  // Remove the last panel and should be hidden again.
  displayPanel.removePanelItem(progressPanel);
  assertEquals(displayPanel.getAttribute('aria-hidden'), 'true');

  // Add multiple panels, then should be visible.
  displayPanel.addPanelItem('testpanel2');
  displayPanel.addPanelItem('testpanel3');
  assertEquals(displayPanel.getAttribute('aria-hidden'), 'false');

  // Clear all the panels, then should be hidden.
  displayPanel.removeAllPanelItems();
  assertEquals(displayPanel.getAttribute('aria-hidden'), 'true');
}

export async function testDisplayPanelAttachPanel(done) {
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

export async function testDisplayPanelChangingPanelTypes(done) {
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

  // Check the progress panel text container has correct aria role.
  const textHost = panelItem.shadowRoot.querySelector('.xf-panel-text');
  assertEquals('alert', textHost.getAttribute('role'));

  // Change the panel item to an error panel.
  panelItem.panelType = panelItem.panelTypeError;

  // Verify the panel item indicator is set to error.
  assertEquals(
      panelItem.indicator, 'status',
      'Wrong panel indicator, got ' + panelItem.indicator);
  assertEquals(
      panelItem.status, 'failure',
      'Wrong panel status, got ' + panelItem.status);

  // Verify the panel item icon is the failure icon.
  const failIcon = panelItem.shadowRoot.querySelector('iron-icon');
  assertEquals('files36:failure', failIcon.getAttribute('icon'));

  // Check dismiss signal from the panel from a click.
  /** @type {!HTMLElement} */
  let dismiss = assert(panelItem.primaryButton);
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

  // Verify the panel item icon is the success icon.
  const successIcon = panelItem.shadowRoot.querySelector('iron-icon');
  assertEquals('files36:success', successIcon.getAttribute('icon'));

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

  // Check the summary panel text container has no aria role.
  assertEquals('', textHost.getAttribute('role'));

  done();
}

export function testFilesDisplayPanelErrorText() {
  // Get the host display panel container element.
  /** @type {!DisplayPanel|!Element} */
  const displayPanel = assert(document.querySelector('#test-xf-display-panel'));

  // Add a panel item to the display panel container.
  const panelItem = displayPanel.addPanelItem('testpanel');

  /** @type {!HTMLElement} */
  const text = assert(panelItem.shadowRoot.querySelector('.xf-panel-text'));

  // To work with screen readers, the text element should have aria role
  // 'alert'.
  assertEquals('alert', text.getAttribute('role'));

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
  assertEquals(str('FILE_ERROR_GENERIC'), panelItem.primaryText);

  // Check the secondary text is empty.
  assertEquals('', panelItem.secondaryText);
}

// Override the formatting function for unit testing.
util.strf = (end, option) => {
  return option + end;
};

export function testFilesDisplayPanelErrorMarker() {
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

export function testFilesDisplayPanelMixedSummary() {
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

export function testFilesDisplayPanelMixedProgress() {
  // Get the host display panel container element.
  /** @type {!DisplayPanel|!Element} */
  const displayPanel = assert(document.querySelector('#test-xf-display-panel'));

  // Add a generic progress panel item to the display panel container.
  const progressPanel = displayPanel.addPanelItem('testpanel1');
  progressPanel.panelType = progressPanel.panelTypeProgress;
  progressPanel.progress = '1';

  // Add a format progress panel item to the display panel container.
  const formatProgressPanel = displayPanel.addPanelItem('testpanel2');
  formatProgressPanel.panelType = formatProgressPanel.panelTypeFormatProgress;
  formatProgressPanel.progress = '2';

  // Confirm that format progress panels do not have a cancel button.
  assertEquals(null, formatProgressPanel.secondaryButton);

  // Add a drive sync progress panel item to the display panel container.
  const syncProgressPanel = displayPanel.addPanelItem('testpanel3');
  syncProgressPanel.panelType = syncProgressPanel.panelTypeSyncProgress;
  syncProgressPanel.progress = '6';

  // Confirm that sync progress panels do not have a cancel button.
  assertEquals(null, syncProgressPanel.secondaryButton);

  // Verify a summary panel item is created with the correct average.
  const summaryContainer = displayPanel.shadowRoot.querySelector('#summary');
  const summaryPanelItem = summaryContainer.querySelector('xf-panel-item');
  assertEquals(summaryPanelItem.panelTypeSummary, summaryPanelItem.panelType);
  assertEquals('largeprogress', summaryPanelItem.indicator);
  assertEquals('hidden', summaryPanelItem.errorMarkerVisibility);
  assertEquals('3', summaryPanelItem.progress);
}

export function testFilesDisplayPanelCircularProgress() {
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

export async function testFilesDisplayPanelSummaryPanel(done) {
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
  const expandButton =
      summaryPanelItem.shadowRoot.querySelector('#primary-action');
  expandButton.click();

  // Confirm the panel container is no longer hidden.
  assertFalse(panelContainer.hasAttribute('hidden'));

  // Remove a progress panel and ensure the summary panel is removed.
  const panelToRemove = displayPanel.findPanelItemById('testpanel1');
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

export function testFilesDisplayPanelTransferDetails() {
  // Get the host display panel container element.
  /** @type {!DisplayPanel|!Element} */
  const displayPanel = assert(document.querySelector('#test-xf-display-panel'));

  // Add a generic progress panel item to the display panel container.
  const progressPanel = displayPanel.addPanelItem('testpanel1');
  progressPanel.panelType = progressPanel.panelTypeProgress;

  // Check detailed-panel is added when FilesTransferDetails enabled.
  assertEquals('detailed-panel', progressPanel.getAttribute('detailed-panel'));
}

export async function testFilesDisplayPanelTransferDetailsSummary(done) {
  // Get the host display panel container element.
  /** @type {!DisplayPanel|!Element} */
  const displayPanel = assert(document.querySelector('#test-xf-display-panel'));

  // Add two generic progress panel items to the display panel container.
  const panel1 = displayPanel.addPanelItem('testpanel1');
  panel1.panelType = panel1.panelTypeProgress;

  const panel2 = displayPanel.addPanelItem('testpanel2');
  panel2.panelType = panel2.panelTypeProgress;

  const panelContainer = displayPanel.shadowRoot.querySelector('#panels');
  assertTrue(panelContainer.hasAttribute('hidden'));

  const summaryContainer = displayPanel.shadowRoot.querySelector('#summary');
  const summaryPanelItem = summaryContainer.querySelector('#summary-panel');

  // Check summary panel has both detailed-panel and detailed-summary attribute.
  assertEquals(
      'detailed-panel', summaryPanelItem.getAttribute('detailed-panel'));
  assertEquals('', summaryPanelItem.getAttribute('detailed-summary'));

  // Trigger expand of the summary panel by summary label.
  const summaryLabel =
      summaryPanelItem.shadowRoot.querySelector('.xf-panel-text');
  summaryLabel.click();

  // Confirm the panel container is no longer hidden.
  assertFalse(panelContainer.hasAttribute('hidden'));
  assertEquals('expanded', summaryPanelItem.getAttribute('data-category'));

  done();
}

/**
 * Tests that the extra-button appears when the value is in the dataset and
 * sends the appropriate signal on click.
 */
export async function testExtraButtonCanBeAddedAndRespondsToAction(done) {
  // Get the host display panel container element.
  /** @type {!DisplayPanel|!Element} */
  const displayPanel = assert(document.querySelector('#test-xf-display-panel'));
  const panelItem = displayPanel.addPanelItem('testpanel');
  panelItem.dataset.extraButtonText = 'Extra button';
  panelItem.panelType = panelItem.panelTypeDone;

  // Setup the panel item signal (click) callback.
  /** @type {?string} */
  let signal = null;
  panelItem.signalCallback = (name) => {
    assert(typeof name === 'string');
    signal = name;
  };

  /** @type {!HTMLElement} */
  const extraButton = assert(panelItem.primaryButton);
  extraButton.click();
  await waitUntil(() => !!signal);
  assertEquals(
      signal, 'extra-button',
      'Expected signal name "extra-button". Got ' + signal);
  signal = null;

  // Check dismiss signal from the panel from a click.
  /** @type {!HTMLElement} */
  const dismiss = assert(panelItem.secondaryButton);
  dismiss.click();
  await waitUntil(() => !!signal);
  assertEquals(
      signal, 'dismiss', 'Expected signal name "dismiss". Got ' + signal);

  done();
}
