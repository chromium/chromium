// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {decorate} from '../../../../common/js/ui.js';
import {Command} from '../command.js';
import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {mockUtilVisitURL} from '../../../../common/js/mock_util.js';
import {waitUntil} from '../../../../common/js/test_error_reporting.js';

import {StateBanner} from './state_banner.js';

/** @type{!StateBanner} */
let stateBanner;

/**
 * Mocks out the chrome.fileManagerPrivate.openSettingsSubpage function to
 * enable interrogation of the subpage that was invoked.
 * @returns {{ restore: function(), getSubpage: (function(): string)}}
 */
function mockOpenSettingsSubpage() {
  const actualOpenSettingsSubpage =
      chrome.fileManagerPrivate.openSettingsSubpage;
  /** @type {string} */
  let subpage;
  chrome.fileManagerPrivate.openSettingsSubpage = settingsSubpage => {
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
  const html = `<state-banner>
      <span slot="text">Banner title</span>
      <button slot="extra-button" href="http://test.com">
        Test Banner
      </button>
    </state-banner>
    `;
  document.body.innerHTML = html;
  stateBanner =
      /** @type{!StateBanner} */ (document.body.querySelector('state-banner'));
}

/**
 * Test that the additional button can be set and the link is visited when the
 * button is clicked.
 */
export async function testAdditionalButtonCanBeClicked() {
  const mockVisitURL = mockUtilVisitURL();
  stateBanner.querySelector('[slot="extra-button"]').click();
  assertEquals(mockVisitURL.getURL(), 'http://test.com');
  mockVisitURL.restoreVisitURL();
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
  const html = `<state-banner>
  <span slot="text">Banner title</span>
  <button slot="extra-button" href="chrome://os-settings/${subpage}">
  Test Button
  </button>
  </state-banner>
  `;
  document.body.innerHTML = html;
  stateBanner =
      /** @type{!StateBanner} */ (document.body.querySelector('state-banner'));
  stateBanner.querySelector('[slot="extra-button"]').click();
  assertEquals(mockSettingsSubpage.getSubpage(), subpage);
  mockSettingsSubpage.restore();
}

/**
 * Test that a href with no subpage, still calls util.visitURL as there is no
 * internal method to make the chrome://os-settings/ page appear except for
 * link capturing.
 */
export async function testChromeOsSettingsNoSubpageLink() {
  const mockVisitURL = mockUtilVisitURL();
  const osSettingsLink = 'chrome://os-settings/';
  const html = `<state-banner>
      <span slot="text">Banner title</span>
      <button slot="extra-button" href="${osSettingsLink}">
        Test Button
      </button>
    </state-banner>
    `;
  document.body.innerHTML = html;
  stateBanner =
      /** @type{!StateBanner} */ (document.body.querySelector('state-banner'));
  stateBanner.querySelector('[slot="extra-button"]').click();
  assertEquals(mockVisitURL.getURL(), osSettingsLink);
  mockVisitURL.restoreVisitURL();
}

/**
 * Test that an extra-button with a command triggers an Event of the correct
 * type.
 */
export async function testCommandsCanBeUsedForExtraButtons(done) {
  const html = `<command id="format">
    <state-banner>
      <span slot="text">Banner title</span>
      <button slot="extra-button" command="#format">
        Test Button
      </button>
    </state-banner>
    `;
  document.body.innerHTML = html;
  decorate('command', Command);

  // Add a listener to wait for the #format command to be received and keep
  // track of the event it received. Given the actual command is not properly
  // setup in the unittest environment, the event bubbles up to the body and
  // we can listen for it there.
  let commandReceived = false;
  let commandEvent = null;
  document.body.addEventListener('command', (e) => {
    commandReceived = true;
    commandEvent = e;
  });

  // Click the extra button with a command associated with it.
  stateBanner =
      /** @type{!StateBanner} */ (document.body.querySelector('state-banner'));
  stateBanner.querySelector('[slot="extra-button"]').click();

  // Wait until the command has been received.
  await waitUntil(() => commandReceived == true);

  // Assert the event type received is a command.
  assertEquals(commandEvent.type, 'command');

  done();
}
