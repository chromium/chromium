// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertNotEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {waitUntil} from '../common/js/test_error_reporting.js';
import {waitForElementUpdate} from '../common/js/unittest_util.js';

import {XfCloudPanel} from './xf_cloud_panel.js';

// Creates new <xf-cloud-panel> for each test.
export function setUp() {
  document.body.innerHTML = getTrustedHTML`
    <xf-cloud-panel></xf-cloud-panel>
  `;
}

// Returns the <xf-cloud-panel> element.
async function getCloudPanel(): Promise<XfCloudPanel> {
  const panel = document.querySelector<XfCloudPanel>('xf-cloud-panel')!;
  assertNotEquals(null, panel);
  assertEquals('XF-CLOUD-PANEL', panel.tagName);
  await waitForElementUpdate(panel);
  return panel;
}

// Checks that a computed style equals `want`.
function checkStyle(element: HTMLElement, tag: string, want: string) {
  const style = element.computedStyleMap().get(tag)!;
  assertNotEquals(null, style);
  assertEquals(want, style.toString());
}

// The different types of selectors that appear.
enum PanelSelector {
  PREPARING = '#preparing-state',
  PROGRESSING = '#progress-state',
  FINISHED = '#progress-finished',
  OFFLINE = '#progress-offline',
  NOT_ENOUGH_SPACE = '#progress-not-enough-space',
  METERED = '#progress-metered-network',
}

// Checks that a panel type is visible and that all the other types are hidden.
function checkVisiblePanel(panel: XfCloudPanel, selector: PanelSelector) {
  const progressStateElement =
      panel.shadowRoot!.querySelector<HTMLDivElement>('#progress-state')!;
  const progressFinishedElement =
      panel.shadowRoot!.querySelector<HTMLDivElement>('#progress-finished')!;
  const progressOfflineElement =
      panel.shadowRoot!.querySelector<HTMLDivElement>('#progress-offline')!;
  const progressNotEnoughSpaceElement =
      panel.shadowRoot!.querySelector<HTMLDivElement>(
          '#progress-not-enough-space')!;
  const progressPreparingElement =
      panel.shadowRoot!.querySelector<HTMLDivElement>('#progress-preparing')!;
  const progressMeteredNetworkElement =
      panel.shadowRoot!.querySelector<HTMLDivElement>(
          '#progress-metered-network')!;

  // Some stages use flexbox to center or vertically align their items, others
  // use a normal block display.
  const displayValue = (type: PanelSelector, success: string = 'block') => {
    return (type === selector) ? success : 'none';
  };

  checkStyle(
      progressStateElement, 'display', displayValue(PanelSelector.PROGRESSING));
  checkStyle(
      progressFinishedElement, 'display',
      displayValue(PanelSelector.FINISHED, 'flex'));
  checkStyle(
      progressOfflineElement, 'display',
      displayValue(PanelSelector.OFFLINE, 'flex'));
  checkStyle(
      progressNotEnoughSpaceElement, 'display',
      displayValue(PanelSelector.NOT_ENOUGH_SPACE, 'flex'));
  checkStyle(
      progressPreparingElement, 'display',
      displayValue(PanelSelector.PREPARING, 'flex'));
  checkStyle(
      progressMeteredNetworkElement, 'display',
      displayValue(PanelSelector.METERED, 'flex'));
}

// Tests that the initial `<xf-cloud-panel>` element defaults to the preparing
// state until both items and percentage are set.
export async function testInitialElementIsInPreparingState() {
  const panel = await getCloudPanel();

  // Expect neither `items` nor `progress` to be set on `<xf-cloud-panel>`.
  assertEquals(panel.getAttribute('items'), null);
  assertEquals(panel.getAttribute('percentage'), null);

  // When no items or percentage is set on the element, it should show in a
  // preparing state.
  checkVisiblePanel(panel, PanelSelector.PREPARING);
}

// Tests that when updating the progress values, it updates the underlying
// progress bar element.
export async function testProgressStateUpdatesProgressBar() {
  const panel = await getCloudPanel();

  // The initial progress state should default to preparing.
  assertEquals(panel.getAttribute('items'), null);
  assertEquals(panel.getAttribute('percentage'), null);
  checkVisiblePanel(panel, PanelSelector.PREPARING);

  // Update the items and progress
  panel.setAttribute('items', '3');
  panel.setAttribute('percentage', '12');

  // Wait for the progress bar to update and the #progress-state div to show.
  await waitForElementUpdate(panel);
  checkVisiblePanel(panel, PanelSelector.PROGRESSING);
  const progress =
      panel.shadowRoot!.querySelector<HTMLProgressElement>('progress')!;
  assertEquals('12', progress.getAttribute('value'));
}

// Tests that when clicking the "Google Drive settings" button an event is
// emitted.
export async function testWhenGoogleDriveSettingsIsClickedEventIsEmitted() {
  const panel = await getCloudPanel();

  // Set up an event listener for the button to be clicked.
  let clicks = 0;
  panel.addEventListener(
      XfCloudPanel.events.DRIVE_SETTINGS_CLICKED, () => ++clicks);

  // Click the "Google Drive settings" button.
  const settingsButton =
      panel.shadowRoot!.querySelector<HTMLButtonElement>('button.action')!;
  settingsButton.click();

  // Wait until the number of clicks has incremented.
  await waitUntil(() => clicks === 1);
}

// Tests that when percentage is 100% it should show the "All files synced"
// state and not the progress state.
export async function testWhenPercentage100OnlyDoneStateShows() {
  const panel = await getCloudPanel();

  // When no attributes have been set, should default to preparing.
  checkVisiblePanel(panel, PanelSelector.PREPARING);

  // Update the items to 3 and total percentage to 50%.
  panel.setAttribute('items', '3');
  panel.setAttribute('percentage', '50');

  // Ensure the progressStateElement is showing but the finished element is not.
  checkVisiblePanel(panel, PanelSelector.PROGRESSING);

  // Update the total percentage to 100%.
  panel.setAttribute('percentage', '100');

  // Ensure the progressState is not showing but the finished element is
  // showing.
  checkVisiblePanel(panel, PanelSelector.FINISHED);
}

// Tests that when the offline type attribute is supplied, the other states
// should all be hidden.
export async function testWhenOfflineTypeAttributeInUseOtherStatesHidden() {
  const panel = await getCloudPanel();

  // When no attributes have been set, should default to preparing.
  checkVisiblePanel(panel, PanelSelector.PREPARING);

  // Update the items to 3 and total percentage to 50%.
  panel.setAttribute('items', '3');
  panel.setAttribute('percentage', '50');

  // Ensure only the in progress element is visible.
  checkVisiblePanel(panel, PanelSelector.PROGRESSING);

  // Update the type to be offline.
  panel.setAttribute('type', 'offline');

  // Ensure the only visible div is the offline one.
  checkVisiblePanel(panel, PanelSelector.OFFLINE);
}

// Tests that when the not_enough_space type attribute is supplied, the other
// states should all be hidden.
export async function
testWhenNotEnoughSpaceTypeAttributeInUseOtherStatesHidden() {
  const panel = await getCloudPanel();

  // When no attributes have been set, should default to preparing.
  checkVisiblePanel(panel, PanelSelector.PREPARING);

  // Update the items to 3 and total percentage to 50%.
  panel.setAttribute('items', '3');
  panel.setAttribute('percentage', '50');

  // Ensure only the in progress element is visible.
  checkVisiblePanel(panel, PanelSelector.PROGRESSING);

  // Update the type to be not_enough_space.
  panel.setAttribute('type', 'not_enough_space');

  // Ensure the only visible div is the not_enough_space one.
  checkVisiblePanel(panel, PanelSelector.NOT_ENOUGH_SPACE);
}

// Tests that only accepted cloud panel types are valid values for the `type`
// attribute.
export async function testOnlyAcceptedTypesUpdateTypeProperty() {
  const panel = await getCloudPanel();

  // The `type` attribute should initially be undefined.
  assertEquals(panel.type, undefined);

  // Setting it to a valid value should update the underlying type.
  panel.setAttribute('type', 'not_enough_space');
  checkVisiblePanel(panel, PanelSelector.NOT_ENOUGH_SPACE);

  // Setting it to some random value will update the HTML elements type
  // attribute but the actual elements `type` property will get set to null as
  // it is not an acceptable value.
  panel.setAttribute('type', 'non-existant-type');
  assertEquals('non-existant-type', panel.getAttribute('type'));
  assertEquals(panel.type, null);
}

// Tests that when percentage is 0, the progress is shown instead of preparing.
export async function testVariousCombinationsOfAttributes() {
  const panel = await getCloudPanel();

  // Setting the items to 1 but no percentage should show the preparing state.
  panel.setAttribute('items', '1');
  checkVisiblePanel(panel, PanelSelector.PREPARING);

  // Only setting the percentage attribute should stay in preparing.
  panel.removeAttribute('items');
  panel.setAttribute('percentage', '0');
  assertEquals('0', panel.getAttribute('percentage'));
  checkVisiblePanel(panel, PanelSelector.PREPARING);

  // When percentage is 0 and items is 1, the preparing should disappear and the
  // progress should show.
  panel.setAttribute('items', '1');
  panel.setAttribute('percentage', '0');
  checkVisiblePanel(panel, PanelSelector.PROGRESSING);

  // When no items are set but the percentage is 100, the panel should be
  // finished.
  panel.setAttribute('items', '0');
  panel.setAttribute('percentage', '100');
  checkVisiblePanel(panel, PanelSelector.FINISHED);

  // The type attribute should take precedence over progressing.
  panel.setAttribute('type', 'offline');
  checkVisiblePanel(panel, PanelSelector.OFFLINE);
}

// Tests that metered network properly updates the state.
export async function testMeteredNetworkState() {
  const panel = await getCloudPanel();

  // Setting it to a valid value should update the underlying type.
  panel.setAttribute('type', 'metered_network');
  checkVisiblePanel(panel, PanelSelector.METERED);
}
