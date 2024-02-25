// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './state_banner.js';

import type {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {crInjectTypeAndInit} from '../../../../common/js/cr_ui.js';
import {waitUntil} from '../../../../common/js/test_error_reporting.js';
import {getLastVisitedURL} from '../../../../common/js/util.js';
import {Command} from '../command.js';

import type {StateBanner} from './state_banner.js';

let stateBanner: StateBanner;

/**
 * Mocks out the chrome.fileManagerPrivate.openSettingsSubpage function to
 * enable interrogation of the subpage that was invoked.
 * @returns {{ restore: function(), getSubpage: (function(): string)}}
 */
function mockOpenSettingsSubpage() {
  const actualOpenSettingsSubpage =
      chrome.fileManagerPrivate.openSettingsSubpage;
  let subpage: string;
  chrome.fileManagerPrivate.openSettingsSubpage = (settingsSubpage: string) => {
    subpage = settingsSubpage;
  };
  const restore = () => {
    chrome.fileManagerPrivate.openSettingsSubpage = actualOpenSettingsSubpage;
  };
  const getSubpage = () => {
    return subpage;
  };
  return {restore, getSubpage};
}

export function setUp() {
  document.body.innerHTML = getTrustedHTML`
    <state-banner>
      <span slot="text">Banner title</span>
      <button slot="extra-button" href="http://test.com">
        Test Banner
      </button>
    </state-banner>
  `;
  stateBanner = document.body.querySelector<StateBanner>('state-banner')!;
}

/**
 * Test that the additional button can be set and the link is visited when the
 * button is clicked.
 */
export async function testAdditionalButtonCanBeClicked() {
  stateBanner.querySelector<CrButtonElement>('[slot="extra-button"]')!.click();
  assertEquals(getLastVisitedURL(), 'http://test.com');
}

/**
 * Test that the default configuration is set on the state banners to ensure
 * any overridden banners have sensible configuration.
 */
export function testStateBannerDefaults() {
  // Ensure the default allowed volume type is empty. This ensures any
  // banners that don't override this property do not show by default.
  assertEquals(stateBanner.allowedVolumes().length, 0);
}

/**
 * Test that extra buttons with a ChromeOS settings href utilise the
 * chrome.fileManagerPrivate.openSettingsSubpage appropriately. The prefix
 * chrome://os-settings/ should be stripped and the subpage passed through.
 */
export async function testChromeOsSettingsLink() {
  const mockSettingsSubpage = mockOpenSettingsSubpage();
  const subpage = 'test/settings/subpage';
  document.body.innerHTML = getTrustedHTML`
    <state-banner>
      <span slot="text">Banner title</span>
      <button slot="extra-button"
          href="chrome://os-settings/test/settings/subpage">
        Test Button
      </button>
    </state-banner>
  `;
  stateBanner = document.body.querySelector<StateBanner>('state-banner')!;
  stateBanner.querySelector<CrButtonElement>('[slot="extra-button"]')!.click();
  assertEquals(mockSettingsSubpage.getSubpage(), subpage);
  mockSettingsSubpage.restore();
}

/**
 * Test that a href with no subpage, still calls visitURL as there is no
 * internal method to make the chrome://os-settings/ page appear except for
 * link capturing.
 */
export async function testChromeOsSettingsNoSubpageLink() {
  const osSettingsLink = 'chrome://os-settings/';
  document.body.innerHTML = getTrustedHTML`
    <state-banner>
      <span slot="text">Banner title</span>
      <button slot="extra-button" href="chrome://os-settings/">
        Test Button
      </button>
    </state-banner>
  `;
  stateBanner = document.body.querySelector<StateBanner>('state-banner')!;
  stateBanner.querySelector<CrButtonElement>('[slot="extra-button"]')!.click();
  assertEquals(getLastVisitedURL(), osSettingsLink);
}

/**
 * Test that an extra-button with a command triggers an Event of the correct
 * type.
 */
export async function testCommandsCanBeUsedForExtraButtons(done: () => void) {
  document.body.innerHTML = getTrustedHTML`
    <command id="format">
      <state-banner>
        <span slot="text">Banner title</span>
        <button slot="extra-button" command="#format">
          Test Button
        </button>
      </state-banner>
  `;
  crInjectTypeAndInit(document.querySelector<Command>('command')!, Command);

  // Add a listener to wait for the #format command to be received and keep
  // track of the event it received. Given the actual command is not properly
  // setup in the unittest environment, the event bubbles up to the body and
  // we can listen for it there.
  let commandReceived = false;
  let commandEvent: Event|null = null;
  document.body.addEventListener('command', (e) => {
    commandReceived = true;
    commandEvent = e;
  });

  // Click the extra button with a command associated with it.
  stateBanner = document.body.querySelector<StateBanner>('state-banner')!;
  stateBanner.querySelector<CrButtonElement>('[slot="extra-button"]')!.click();

  // Wait until the command has been received.
  await waitUntil(() => commandReceived === true);

  // Assert the event type received is a command.
  assertEquals(commandEvent!.type, 'command');

  done();
}
