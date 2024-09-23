// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './xf_button.js';
import './xf_circular_progress.js';
import './xf_display_panel.js';
import './xf_panel_item.js';

import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {waitUntil} from '../../common/js/test_error_reporting.js';
import {mockPluralStringProxy} from '../../common/js/unittest_util.js';

import type {PanelButton} from './xf_button.js';
import type {CircularProgress} from './xf_circular_progress.js';
import type {DisplayPanel} from './xf_display_panel.js';
import type {PanelItem} from './xf_panel_item.js';
import {PanelType} from './xf_panel_item.js';

let displayPanel: DisplayPanel;

/**
 * Adds a xf-display-panel element to the test page.
 */
export function setUpPage() {
  mockPluralStringProxy();
  const displayPanelElement = document.createElement('xf-display-panel');
  displayPanelElement.setAttribute('id', 'test-xf-display-panel');
  document.body.appendChild(displayPanelElement);
  displayPanel =
      document.querySelector<DisplayPanel>('#test-xf-display-panel')!;
}

export function tearDown() {
  displayPanel.removeAllPanelItems();
}

/**
 * Tests that adding and removing panels to <xf-display-panel> updates the
 * aria-hidden attribute.
 */
export async function testDisplayPanelAriaHidden() {
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

export async function testDisplayPanelAttachPanel(done: VoidCallback) {
  // Get the host display panel container element.
  const displayPanel =
      document.querySelector<DisplayPanel>('#test-xf-display-panel')!;

  // Test create/attach/remove sequences.
  // Create a progress panel item.
  let progressPanel = displayPanel.createPanelItem('testpanel');
  progressPanel.panelType = PanelType.PROGRESS;

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
  progressPanel.panelType = PanelType.PROGRESS;

  // Remove the panel item.
  displayPanel.removePanelItem(progressPanel);

  // Try to attach the removed panel to it's host display panel.
  displayPanel.attachPanelItem(progressPanel);

  // Verify the panel is not attached to the document.
  assertFalse(progressPanel.isConnected);

  done();
}

export async function testDisplayPanelChangingPanelTypes(done: VoidCallback) {
  // Get the host display panel container element.
  const displayPanel =
      document.querySelector<DisplayPanel>('#test-xf-display-panel')!;
  const panelItem = displayPanel.addPanelItem('testpanel');
  panelItem.panelType = PanelType.PROGRESS;

  // Setup the panel item signal (click) callback.
  let signal: string|null = null;
  panelItem.signalCallback = (name) => {
    assert(typeof name === 'string');
    signal = name;
  };

  // Verify the panel item indicator is progress.
  assertEquals(
      panelItem.indicator, 'progress',
      'Wrong panel indicator, got ' + panelItem.indicator);

  // Check cancel signal from the panel from a click.
  let cancel = panelItem.secondaryButton!;
  cancel.click();
  assertEquals(
      signal, 'cancel', 'Expected signal name "cancel". Got ' + signal);

  // Check the progress panel text container has correct aria role.
  const textHost =
      panelItem.shadowRoot!.querySelector<HTMLDivElement>('.xf-panel-text')!;
  assertEquals('alert', textHost.getAttribute('role'));

  // Change the panel item to an error panel.
  panelItem.panelType = PanelType.ERROR;

  // Verify the panel item indicator is set to error.
  assertEquals(
      panelItem.indicator, 'status',
      'Wrong panel indicator, got ' + panelItem.indicator);
  assertEquals(
      panelItem.status, 'failure',
      'Wrong panel status, got ' + panelItem.status);

  // Verify the panel item icon is the failure icon.
  const failIcon = panelItem.shadowRoot!.querySelector('iron-icon')!;
  assertEquals('files36:failure', failIcon.getAttribute('icon'));

  // Check dismiss signal from the panel from a click.
  let dismiss = panelItem.primaryButton!;
  dismiss.click();
  assertEquals(
      signal, 'dismiss', 'Expected signal name "dismiss". Got ' + signal);

  // Change the panel item to an info panel.
  panelItem.panelType = PanelType.INFO;

  // Verify the panel item indicator is set to warning.
  assertEquals(
      panelItem.indicator, 'status',
      'Wrong panel indicator, got ' + panelItem.indicator);
  assertEquals(
      panelItem.status, 'warning',
      'Wrong panel status, got ' + panelItem.status);

  // Verify the panel item icon is the warning icon.
  const warningIcon = panelItem.shadowRoot!.querySelector('iron-icon')!;
  assertEquals('files36:warning', warningIcon.getAttribute('icon'));

  // Check cancel signal from the panel from a click.
  cancel = panelItem.primaryButton!;
  cancel.click();
  assertEquals(
      signal, 'cancel', 'Expected signal name "cancel". Got ' + signal);

  // Change the panel type to a done panel.
  panelItem.panelType = PanelType.DONE;

  // Verify the panel item indicator is set to done.
  assertEquals(
      panelItem.indicator, 'status',
      'Wrong panel indicator, got ' + panelItem.indicator);
  assertEquals(
      panelItem.status, 'success',
      'Wrong panel status, got ' + panelItem.status);

  // Verify the panel item icon is the success icon.
  const successIcon = panelItem.shadowRoot!.querySelector('iron-icon')!;
  assertEquals('files36:success', successIcon.getAttribute('icon'));

  // Check the dimiss signal from the panel from a click.
  signal = 'none';
  dismiss = panelItem.primaryButton!;
  dismiss.click();
  assertEquals(
      signal, 'dismiss', 'Expected signal name "dismiss". Got ' + signal);

  // Change the type to a summary panel.
  panelItem.panelType = PanelType.SUMMARY;

  // Verify the panel item indicator is largeprogress.
  assertEquals(
      panelItem.indicator, 'largeprogress',
      'Wrong panel indicator, got ' + panelItem.indicator);

  // Check no signal emitted from the summary panel from a click.
  const expand = panelItem.primaryButton!;
  signal = 'none';
  expand.click();
  assertEquals(signal, 'none', 'Expected no signal. Got ' + signal);

  // Check the summary panel text container has no aria role.
  assertEquals('', textHost.getAttribute('role'));

  done();
}

export function testFilesDisplayPanelErrorText() {
  // Get the host display panel container element.
  const displayPanel =
      document.querySelector<DisplayPanel>('#test-xf-display-panel')!;

  // Add a panel item to the display panel container.
  const panelItem = displayPanel.addPanelItem('testpanel');

  const text =
      panelItem.shadowRoot!.querySelector<HTMLDivElement>('.xf-panel-text')!;

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
  panelItem.panelType = PanelType.ERROR;

  // Check that primary and secondary text don't change.
  assertEquals('foo', panelItem.primaryText);
  assertEquals('bar', panelItem.secondaryText);
}

export async function testFilesDisplayPanelInfo(done: VoidCallback) {
  // Get the host display panel container element.
  const displayPanel =
      document.querySelector<DisplayPanel>('#test-xf-display-panel')!;

  // Add a panel item to the display panel container.
  const panelItem = displayPanel.addPanelItem('testpanel');
  panelItem.dataset['extraButtonText'] = 'Extra button';

  const text =
      panelItem.shadowRoot!.querySelector<HTMLDivElement>('.xf-panel-text')!;

  // To work with screen readers, the text element should have aria role
  // 'alert'.
  assertEquals('alert', text.getAttribute('role'));

  // Change the primary and secondary text on the panel item.
  panelItem.primaryText = 'foo';
  panelItem.secondaryText = 'bar';

  // Check the primary and secondary text has been set on the panel.
  assertEquals('foo', panelItem.primaryText);
  assertEquals('bar', panelItem.secondaryText);

  // Change the panel item type to an info panel.
  panelItem.panelType = PanelType.INFO;

  // Check the primary and secondary text has been set on the panel.
  assertEquals('foo', panelItem.primaryText);
  assertEquals('bar', panelItem.secondaryText);

  // Setup the panel item signal (click) callback.
  let signal: string|null = null;
  panelItem.signalCallback = (name) => {
    assert(typeof name === 'string');
    signal = name;
  };

  const extraButton = panelItem.primaryButton!;
  extraButton.click();
  await waitUntil(() => !!signal);
  assertEquals(
      signal, 'extra-button',
      'Expected signal name "extra-button". Got ' + signal);
  signal = null;

  // Check cancel signal from the panel from a click.
  const cancel = panelItem.secondaryButton!;
  cancel.click();
  await waitUntil(() => !!signal);
  assertEquals(
      signal, 'cancel', 'Expected signal name "cancel". Got ' + signal);

  done();
}

export function testFilesDisplayPanelErrorMarker() {
  // Get the host display panel container element.
  const displayPanel =
      document.querySelector<DisplayPanel>('#test-xf-display-panel')!;

  // Add a summary panel item to the display panel container.
  const summaryPanel = displayPanel.addPanelItem('testpanel');
  summaryPanel.panelType = PanelType.SUMMARY;

  // Confirm the error marker is not visible by default.
  const progressIndicator =
      summaryPanel.shadowRoot!.querySelector<CircularProgress>(
          'xf-circular-progress')!;
  const errorMarker =
      progressIndicator.shadowRoot!.querySelector<SVGCircleElement>(
          '.errormark')!;
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
  const displayPanel =
      document.querySelector<DisplayPanel>('#test-xf-display-panel')!;

  // Add an error panel item to the display panel container.
  let errorPanel = displayPanel.addPanelItem('testpanel1');
  errorPanel.panelType = PanelType.ERROR;
  assertEquals('status', errorPanel.indicator);

  // Add a progress panel item to the display panel container.
  const progressPanel = displayPanel.addPanelItem('testpanel2');
  progressPanel.panelType = PanelType.PROGRESS;

  // Verify a summary panel item is created and shows the error indicator.
  const summaryContainer =
      displayPanel.shadowRoot!.querySelector<HTMLDivElement>('#summary')!;
  let summaryPanelItem =
      summaryContainer.querySelector<PanelItem>('xf-panel-item')!;
  assertEquals(PanelType.SUMMARY, summaryPanelItem.panelType);
  assertEquals('largeprogress', summaryPanelItem.indicator);
  assertEquals('visible', summaryPanelItem.errorMarkerVisibility);

  // Remove the error panel item and add a second progress panel item.
  displayPanel.removePanelItem(errorPanel);

  const extraProgressPanel = displayPanel.addPanelItem('testpanel3');
  extraProgressPanel.panelType = PanelType.PROGRESS;

  // Verify a summary panel item is created without an error indicator.
  summaryPanelItem =
      summaryContainer.querySelector<PanelItem>('xf-panel-item')!;
  assertEquals(PanelType.SUMMARY, summaryPanelItem.panelType);
  assertEquals('largeprogress', summaryPanelItem.indicator);
  assertEquals('hidden', summaryPanelItem.errorMarkerVisibility);

  // Remove the progress panels and add two info (a.k.a. warning) panel items.
  displayPanel.removeAllPanelItems();

  const infoPanel = displayPanel.addPanelItem('testpanel4');
  infoPanel.panelType = PanelType.INFO;

  const extraInfoPanel = displayPanel.addPanelItem('testpanel5');
  extraInfoPanel.panelType = PanelType.INFO;

  // Verify a summary panel item is created with a warning indicator.
  summaryPanelItem =
      summaryContainer.querySelector<PanelItem>('xf-panel-item')!;
  assertEquals(PanelType.SUMMARY, summaryPanelItem.panelType);
  assertEquals('status', summaryPanelItem.indicator);
  assertEquals('warning', summaryPanelItem.status);

  // Remove one info panel and add 2 error panel items.
  displayPanel.removePanelItem(extraInfoPanel);

  errorPanel = displayPanel.addPanelItem('testpanel6');
  errorPanel.panelType = PanelType.ERROR;

  const extraErrorPanel = displayPanel.addPanelItem('testpanel7');
  extraErrorPanel.panelType = PanelType.ERROR;

  // Verify a summary panel item is shown, with an error status indicator.
  summaryPanelItem =
      summaryContainer.querySelector<PanelItem>('xf-panel-item')!;
  assertEquals(PanelType.SUMMARY, summaryPanelItem.panelType);
  assertEquals('status', summaryPanelItem.indicator);
  assertEquals('failure', summaryPanelItem.status);

  // Remove the error panels items and add a done (a.k.a. success) panel item.
  displayPanel.removePanelItem(errorPanel);
  displayPanel.removePanelItem(extraErrorPanel);

  const donePanel = displayPanel.addPanelItem('testpanel8');
  donePanel.panelType = PanelType.DONE;

  // Verify a summary panel item is created with a warning indicator.
  summaryPanelItem =
      summaryContainer.querySelector<PanelItem>('xf-panel-item')!;
  assertEquals(PanelType.SUMMARY, summaryPanelItem.panelType);
  assertEquals('status', summaryPanelItem.indicator);
  assertEquals('warning', summaryPanelItem.status);

  // Remove the info panel items and add another done panel item.
  displayPanel.removePanelItem(infoPanel);

  const extraDonePanel = displayPanel.addPanelItem('testpanel9');
  extraDonePanel.panelType = PanelType.DONE;

  // Verify a summary panel is shown with success indicator.
  summaryPanelItem =
      summaryContainer.querySelector<PanelItem>('xf-panel-item')!;
  assertEquals(PanelType.SUMMARY, summaryPanelItem.panelType);
  assertEquals('status', summaryPanelItem.indicator);
  assertEquals('success', summaryPanelItem.status);
}

export async function testFilesDisplayPanelMixedProgress() {
  // Get the host display panel container element.
  const displayPanel =
      document.querySelector<DisplayPanel>('#test-xf-display-panel')!;

  // Add a generic progress panel item to the display panel container.
  const progressPanel = displayPanel.addPanelItem('testpanel1');
  progressPanel.panelType = PanelType.PROGRESS;
  progressPanel.progress = '1';

  // Add a format progress panel item to the display panel container.
  const formatProgressPanel = displayPanel.addPanelItem('testpanel2');
  formatProgressPanel.panelType = PanelType.FORMAT_PROGRESS;
  formatProgressPanel.progress = '2';

  // Confirm that format progress panels do not have a cancel button.
  assertEquals(null, formatProgressPanel.secondaryButton);

  // Add a drive sync progress panel item to the display panel container.

  const syncProgressPanel = displayPanel.addPanelItem('testpanel3');
  syncProgressPanel.panelType = PanelType.SYNC_PROGRESS;
  syncProgressPanel.progress = '6';

  // Confirm that sync progress panels do not have a cancel button.
  assertEquals(null, syncProgressPanel.secondaryButton);

  // Verify a summary panel item is created with the correct average.
  const summaryContainer =
      displayPanel.shadowRoot!.querySelector<HTMLDivElement>('#summary')!;
  const summaryPanelItem =
      summaryContainer.querySelector<PanelItem>('xf-panel-item')!;
  assertEquals(PanelType.SUMMARY, summaryPanelItem.panelType);
  assertEquals('largeprogress', summaryPanelItem.indicator);
  assertEquals('hidden', summaryPanelItem.errorMarkerVisibility);
  assertEquals(3, summaryPanelItem.progress);
}

export function testFilesDisplayPanelCircularProgress() {
  // Get the host display panel container element.
  const displayPanel =
      document.querySelector<DisplayPanel>('#test-xf-display-panel')!;

  // Add a progress panel item to the display panel container.
  const progressPanel = displayPanel.addPanelItem('testpanel1');
  progressPanel.panelType = PanelType.PROGRESS;

  // Verify the circular progress panel item marker stroke width.
  const circularProgress =
      progressPanel.shadowRoot!.querySelector<CircularProgress>('#indicator')!;
  const strokeWidthContainerGroup =
      circularProgress.shadowRoot!.querySelector<SVGGElement>('#circles')!;
  assertEquals('3', strokeWidthContainerGroup.getAttribute('stroke-width'));

  // Verify setting large radius on the circular marker changes stroke width.
  circularProgress.setAttribute('radius', '11');
  assertEquals('4', strokeWidthContainerGroup.getAttribute('stroke-width'));
}

// TODO(b/293228531): Reenable test when fixed for createElement().
export async function disabledTestFilesDisplayPanelSummaryPanel(
    done: VoidCallback) {
  // Get the host display panel container element.
  const displayPanel =
      document.querySelector<DisplayPanel>('#test-xf-display-panel')!;

  // Declare the expected panel item height managed from CSS.
  const singlePanelHeight = 68;

  // Add a progress panel item to the display panel container.
  let progressPanel = displayPanel.addPanelItem('testpanel1');
  progressPanel.panelType = PanelType.PROGRESS;

  // Confirm the expected height of a single panelItem.
  let bounds = progressPanel.getBoundingClientRect();
  assert(bounds.height === singlePanelHeight);

  // Add second panel item.
  progressPanel = displayPanel.addPanelItem('testpanel2');
  progressPanel.panelType = PanelType.PROGRESS;

  // Confirm multiple progress panel items are hidden by default.
  const panelContainer =
      displayPanel.shadowRoot!.querySelector<HTMLDivElement>('#panels')!;
  assertTrue(panelContainer.hasAttribute('hidden'));

  // Confirm multiple progress panels cause creation of a summary panel.
  const summaryContainer =
      displayPanel.shadowRoot!.querySelector<HTMLDivElement>('#summary')!;
  let summaryPanelItem =
      summaryContainer.querySelector<PanelItem>('xf-panel-item')!;
  assertEquals(summaryPanelItem.panelType, PanelType.SUMMARY);

  // Confirm the expected height of the summary panel.
  bounds = summaryPanelItem.getBoundingClientRect();
  assert(bounds.height === singlePanelHeight);

  // Trigger expand of the summary panel.
  const expandButton = summaryPanelItem.shadowRoot!.querySelector<PanelButton>(
      '#primary-action')!;
  expandButton.click();

  // Confirm the panel container is no longer hidden.
  assertFalse(panelContainer.hasAttribute('hidden'));

  // Remove a progress panel and ensure the summary panel is removed.
  const panelToRemove = displayPanel.findPanelItemById('testpanel1')!;
  displayPanel.removePanelItem(panelToRemove);
  summaryPanelItem =
      summaryContainer.querySelector<PanelItem>('xf-panel-item')!;
  assertEquals(summaryPanelItem, null);

  // Confirm the reference to the removed panel item is gone.
  assertEquals(displayPanel.findPanelItemById('testpanel1'), null);

  // Add another panel item and confirm the expanded state persists by
  // checking the panel container is not hidden and there is a summary panel.
  progressPanel = displayPanel.addPanelItem('testpanel1');
  progressPanel.panelType = PanelType.PROGRESS;
  assertFalse(panelContainer.hasAttribute('hidden'));
  summaryPanelItem =
      summaryContainer.querySelector<PanelItem>('xf-panel-item')!;
  assertEquals(summaryPanelItem.panelType, PanelType.SUMMARY);

  done();
}

export async function testFilesDisplayPanelTransferDetails() {
  // Get the host display panel container element.
  const displayPanel =
      document.querySelector<DisplayPanel>('#test-xf-display-panel')!;

  // Add a generic progress panel item to the display panel container.
  const progressPanel = displayPanel.addPanelItem('testpanel1');
  progressPanel.panelType = PanelType.PROGRESS;

  // Check detailed-panel is added when FilesTransferDetails enabled.
  assertEquals('detailed-panel', progressPanel.getAttribute('detailed-panel'));
}

export function testFilesDisplayPanelTransferDetailsSummary(
    done: VoidCallback) {
  // Get the host display panel container element.
  const displayPanel =
      document.querySelector<DisplayPanel>('#test-xf-display-panel')!;

  // Add two generic progress panel items to the display panel container.
  const panel1 = displayPanel.addPanelItem('testpanel1');
  panel1.panelType = PanelType.PROGRESS;

  const panel2 = displayPanel.addPanelItem('testpanel2');
  panel2.panelType = PanelType.PROGRESS;

  const panelContainer =
      displayPanel.shadowRoot!.querySelector<HTMLDivElement>('#panels')!;
  assertTrue(panelContainer.hasAttribute('hidden'));

  const summaryContainer =
      displayPanel.shadowRoot!.querySelector<HTMLDivElement>('#summary')!;
  const summaryPanelItem =
      summaryContainer.querySelector<HTMLDivElement>('#summary-panel')!;

  // Check summary panel has both detailed-panel and detailed-summary attribute.
  assertEquals(
      'detailed-panel', summaryPanelItem.getAttribute('detailed-panel'));
  assertEquals('', summaryPanelItem.getAttribute('detailed-summary'));

  // Trigger expand of the summary panel by summary label.
  const summaryLabel =
      summaryPanelItem.shadowRoot!.querySelector<HTMLDivElement>(
          '.xf-panel-text')!;
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
export async function testExtraButtonCanBeAddedAndRespondsToAction(
    done: VoidCallback) {
  // Get the host display panel container element.
  const displayPanel =
      document.querySelector<DisplayPanel>('#test-xf-display-panel')!;

  const panelItem = displayPanel.addPanelItem('testpanel');
  panelItem.dataset['extraButtonText'] = 'Extra button';
  panelItem.panelType = PanelType.DONE;

  // Setup the panel item signal (click) callback.
  let signal: string|null = null;
  panelItem.signalCallback = (name) => {
    assert(typeof name === 'string');
    signal = name;
  };

  const extraButton = panelItem.primaryButton!;
  extraButton.click();
  await waitUntil(() => !!signal);
  assertEquals(
      signal, 'extra-button',
      'Expected signal name "extra-button". Got ' + signal);
  signal = null;

  // Check dismiss signal from the panel from a click.
  const dismiss = panelItem.secondaryButton!;
  dismiss.click();
  await waitUntil(() => !!signal);
  assertEquals(
      signal, 'dismiss', 'Expected signal name "dismiss". Got ' + signal);

  done();
}
