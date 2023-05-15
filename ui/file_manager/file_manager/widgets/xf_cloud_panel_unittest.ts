// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertNotEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {waitUntil} from '../common/js/test_error_reporting.js';

import {CloudPanelType, XfCloudPanel} from './xf_cloud_panel.js';

/**
 * Creates new <xf-cloud-panel> for each test.
 */
export function setUp() {
  document.body.innerHTML = '<xf-cloud-panel></xf-cloud-panel>';
}

/**
 * Returns the <xf-cloud-panel> element.
 */
function getCloudPanelElement(): XfCloudPanel {
  const element = document.querySelector('xf-cloud-panel');
  assertNotEquals(null, element, 'xf-cloud-panel is null');
  assertEquals('XF-CLOUD-PANEL', element!.tagName);
  return element! as XfCloudPanel;
}

/**
 * Asynchronously waits for the attribute value on `element` to equal `want`.
 */
async function waitForAttributeValue(
    element: HTMLElement, attributeName: string, want: string): Promise<void> {
  let value = null;
  await waitUntil(() => {
    value = element.getAttribute(attributeName);
    return value !== null;
  });
  assertEquals(value, want);
}

/**
 * Wait for a computed style to equal `want`.
 */
async function waitForStyles(
    element: HTMLElement, tag: string, want: string): Promise<void> {
  return waitUntil(() => {
    const styleMap = element.computedStyleMap();
    if (!styleMap.has(tag) || !styleMap.get(tag)) {
      return false;
    }
    return styleMap.get(tag)!.toString() === want;
  });
}

/**
 * Helper to get an element parented at `root` asynchronously.
 */
async function getElement<T extends HTMLElement>(
    root: Document|DocumentFragment, selector: string): Promise<T> {
  let element: T|null = null;
  await waitUntil(() => {
    element = root.querySelector(selector);
    return element !== null;
  });
  assertNotEquals(element, null);
  return element!;
}

/**
 * Get the progress bar element in the cloud panel shadowroot.
 */
async function getProgressBar(): Promise<HTMLProgressElement> {
  const element = getCloudPanelElement();
  let progressElement: HTMLProgressElement|null = null;
  await waitUntil(() => {
    progressElement =
        element.shadowRoot!.querySelector<HTMLProgressElement>('progress');
    return progressElement !== null;
  });
  return progressElement! as HTMLProgressElement;
}

/**
 * Tests that the initial `<xf-cloud-panel>` element only has the Google Drive
 * settings link and the progress state is not visible.
 */
export async function testInitialElementOnlyHasLink(done: () => void) {
  const element = getCloudPanelElement();

  // Expect neither `items` nor `progress` to be set on `<xf-cloud-panel>`.
  assertEquals(element.getAttribute('items'), null);
  assertEquals(element.getAttribute('percentage'), null);

  // Ensure the the container of progress state should have display none (as a
  // result of the absence of `items` and `progress` attributes above).
  const progressStateElement =
      await getElement<HTMLDivElement>(element.shadowRoot!, '#progress-state');
  await waitForStyles(progressStateElement, 'display', 'none');

  done();
}

/**
 * Tests that when updating the progress values, it updates the underlying
 * progress bar element.
 */
export async function testProgressStateUpdatesProgressBar(done: () => void) {
  const element = getCloudPanelElement();
  const progressStateElement =
      await getElement<HTMLDivElement>(element.shadowRoot!, '#progress-state');

  // The initial progress state should not show until the type is updated.
  assertEquals(element.getAttribute('items'), null);
  assertEquals(element.getAttribute('percentage'), null);
  await waitForStyles(progressStateElement, 'display', 'none');
  // Update the items and progress
  element.setAttribute('items', '3');
  element.setAttribute('percentage', '12');

  // Wait for the progress bar to update and the #progress-state div to show.
  const progress = await getProgressBar();
  await waitForStyles(progressStateElement, 'display', 'block');
  await waitForAttributeValue(progress, 'value', '12');
  done();
}

/**
 * Tests that when clicking the "Google Drive settings" button an event is
 * emitted.
 */
export async function testWhenGoogleDriveSettingsIsClickedEventIsEmitted(
    done: () => void) {
  const element = getCloudPanelElement();

  // Set up an event listener for the button to be clicked.
  let clicks = 0;
  element.addEventListener(
      XfCloudPanel.events.DRIVE_SETTINGS_CLICKED, () => ++clicks);

  // Click the "Google Drive settings" button.
  const settingsButton =
      await getElement<HTMLButtonElement>(element.shadowRoot!, 'button.action');
  settingsButton!.click();

  // Wait until the number of clicks has incremented.
  await waitUntil(() => clicks === 1);

  done();
}

/**
 * Tests that when percentage is 100% it should show the "All files synced"
 * state and not the progress state.
 */
export async function testWhenPercentage100OnlyDoneStateShows(
    done: () => void) {
  const element = getCloudPanelElement();
  const progressStateElement =
      await getElement<HTMLDivElement>(element.shadowRoot!, '#progress-state');
  const progressFinishedElement = await getElement<HTMLDivElement>(
      element.shadowRoot!, '#progress-finished');

  // When no attributes have been set, no div should be visible.
  await waitForStyles(progressStateElement, 'display', 'none');
  await waitForStyles(progressFinishedElement, 'display', 'none');

  // Update the items to 3 and total percentage to 50%.
  element.setAttribute('items', '3');
  element.setAttribute('percentage', '50');

  // Ensure the progressStateElement is showing but the finished element is not.
  await waitForStyles(progressStateElement, 'display', 'block');
  await waitForStyles(progressFinishedElement, 'display', 'none');

  // Update the total percentage to 100%.
  element.setAttribute('percentage', '100');

  // Ensure the progressState is not showing but the finished element is
  // showing.
  await waitForStyles(progressStateElement, 'display', 'none');
  await waitForStyles(progressFinishedElement, 'display', 'flex');
  done();
}

/**
 * Tests that when the offline type attribute is supplied, the other states
 * should all be hidden.
 */
export async function testWhenOfflineTypeAttributeInUseOtherStatesHidden(
    done: () => void) {
  const element = getCloudPanelElement();
  const progressStateElement =
      await getElement<HTMLDivElement>(element.shadowRoot!, '#progress-state');
  const progressFinishedElement = await getElement<HTMLDivElement>(
      element.shadowRoot!, '#progress-finished');
  const progressOfflineElement = await getElement<HTMLDivElement>(
      element.shadowRoot!, '#progress-offline');

  // When no attributes have been set, no div should be visible.
  await waitForStyles(progressStateElement, 'display', 'none');
  await waitForStyles(progressFinishedElement, 'display', 'none');
  await waitForStyles(progressOfflineElement, 'display', 'none');

  // Update the items to 3 and total percentage to 50%.
  element.setAttribute('items', '3');
  element.setAttribute('percentage', '50');

  // Ensure only the in progress element is visible.
  await waitForStyles(progressStateElement, 'display', 'block');
  await waitForStyles(progressFinishedElement, 'display', 'none');
  await waitForStyles(progressOfflineElement, 'display', 'none');

  // Update the type to be offline.
  element.setAttribute('type', 'offline');

  // Ensure the only visible div is the offline one.
  await waitForStyles(progressStateElement, 'display', 'none');
  await waitForStyles(progressFinishedElement, 'display', 'none');
  await waitForStyles(progressOfflineElement, 'display', 'flex');
  done();
}

/**
 * Tests that when the not-enough-space type attribute is supplied, the other
 * states should all be hidden.
 */
export async function testWhenNotEnoughSpaceTypeAttributeInUseOtherStatesHidden(
    done: () => void) {
  const element = getCloudPanelElement();
  const progressStateElement =
      await getElement<HTMLDivElement>(element.shadowRoot!, '#progress-state');
  const progressFinishedElement = await getElement<HTMLDivElement>(
      element.shadowRoot!, '#progress-finished');
  const progressOfflineElement = await getElement<HTMLDivElement>(
      element.shadowRoot!, '#progress-offline');
  const progressNotEnoughSpaceElement = await getElement<HTMLDivElement>(
      element.shadowRoot!, '#progress-not-enough-space');

  // When no attributes have been set, no div should be visible.
  await waitForStyles(progressStateElement, 'display', 'none');
  await waitForStyles(progressFinishedElement, 'display', 'none');
  await waitForStyles(progressOfflineElement, 'display', 'none');
  await waitForStyles(progressNotEnoughSpaceElement, 'display', 'none');

  // Update the items to 3 and total percentage to 50%.
  element.setAttribute('items', '3');
  element.setAttribute('percentage', '50');

  // Ensure only the in progress element is visible.
  await waitForStyles(progressStateElement, 'display', 'block');
  await waitForStyles(progressFinishedElement, 'display', 'none');
  await waitForStyles(progressOfflineElement, 'display', 'none');
  await waitForStyles(progressNotEnoughSpaceElement, 'display', 'none');

  // Update the type to be offline.
  element.setAttribute('type', 'offline');

  // Ensure the only visible div is the offline one.
  await waitForStyles(progressStateElement, 'display', 'none');
  await waitForStyles(progressFinishedElement, 'display', 'none');
  await waitForStyles(progressOfflineElement, 'display', 'flex');
  await waitForStyles(progressNotEnoughSpaceElement, 'display', 'none');

  // Update the type to be not-enough-space.
  element.setAttribute('type', 'not-enough-space');

  // Ensure the only visible div is the not-enough-space one.
  await waitForStyles(progressStateElement, 'display', 'none');
  await waitForStyles(progressFinishedElement, 'display', 'none');
  await waitForStyles(progressOfflineElement, 'display', 'none');
  await waitForStyles(progressNotEnoughSpaceElement, 'display', 'flex');

  done();
}

/**
 * Tests that only accepted cloud panel types are valid values for the `type`
 * attribute.
 */
export async function testOnlyAcceptedTypesUpdateTypeProperty(
    done: () => void) {
  const element = getCloudPanelElement();

  // The `type` attribute should initially be undefined.
  assertEquals(element.type, undefined);

  // Setting it to a valid value should update the underlying type.
  element.setAttribute('type', 'not-enough-space');
  await waitForAttributeValue(element, 'type', CloudPanelType.NOT_ENOUGH_SPACE);

  // Setting it to some random value will update the HTML elements type
  // attribute but the actual elements `type` property will get set to null as
  // it is not an acceptable value.
  element.setAttribute('type', 'non-existant-type');
  await waitForAttributeValue(element, 'type', 'non-existant-type');
  assertEquals(element.type, null);

  done();
}
