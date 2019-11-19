// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Adds a FilesMessage element to the page, initially hidden.
 */
function setUpPage() {
  document.body.innerHTML +=
      '<files-message id="test-files-message" hidden></files-message>';
}

/**
 * Sets the <files-message> element content, and returns the element.
 * @return {!FilesMessage|!Element}
 */
function setFilesMessageContent() {
  // Get the FilesMessage element.
  /** @type {!FilesMessage|!Element} */
  let message = assert(document.querySelector('#test-files-message'));

  // Use the FilesMessage.setContent() method to assign all its settable
  // properties in one go.
  message.setContent({
    icon: {icon: 'cr:expand-more', label: 'More'},
    message: 'Some very informative text message',
    dismiss: 'Dismiss',
    action: 'View offline files',
    close: {label: 'Close'},
  });

  // <files-message> [hidden] controls visual display.
  assertEquals('none', window.getComputedStyle(message).display);
  assertNotEqual(null, message.getAttribute('hidden'));
  assertTrue(message.hidden);

  // <files-message> extends HTMLElement and so the setContent() method
  // can set its HTMLElement properties too, e.g., hidden here.
  message.setContent({
    hidden: false,
  });

  // <files-message> element should be visually displayed.
  assertNotEqual('none', window.getComputedStyle(message).display);
  assertEquals(null, message.getAttribute('hidden'));
  assertFalse(message.hidden);

  return message;
}

/**
 * Tests the <files-message> element: check display, clicking, aria roles,
 * setting its content via the element's property setters, and hiding its
 * sub-elements.
 */
async function testFilesMessage(done) {
  // Setup and return the <files-message> element.
  /** @type {!FilesMessage|!Element} */
  const message = setFilesMessageContent();

  // Get its sub-elements.
  /** @type {!HTMLElement} */
  const icon = assert(message.shadowRoot.querySelector('#icon'));
  /** @type {!HTMLElement} */
  const text = assert(message.shadowRoot.querySelector('#text'));
  /** @type {!HTMLElement} */
  const dismiss = assert(message.shadowRoot.querySelector('#dismiss'));
  /** @type {!HTMLElement} */
  const action = assert(message.shadowRoot.querySelector('#action'));
  /** @type {!HTMLElement} */
  const close = assert(message.shadowRoot.querySelector('#close'));

  // The sub-elements should be displayed.
  assertNotEqual('none', window.getComputedStyle(icon).display);
  assertNotEqual('none', window.getComputedStyle(text).display);
  assertNotEqual('none', window.getComputedStyle(dismiss).display);
  assertNotEqual('none', window.getComputedStyle(action).display);
  assertNotEqual('none', window.getComputedStyle(close).display);

  // To work with screen readers, the icon & text element containing
  // parent element should have aria role 'alert'.
  const iconParent = icon.parentElement;
  assertEquals('alert', iconParent.getAttribute('role'));
  const textParent = text.parentElement;
  assertEquals('alert', textParent.getAttribute('role'));

  // The sub-elements should have aria-label content.
  assertEquals('More', icon.getAttribute('aria-label'));
  assertEquals('Dismiss', dismiss.getAttribute('aria-label'));
  assertEquals('View offline files', action.getAttribute('aria-label'));
  assertEquals('Close', close.getAttribute('aria-label'));

  // The text element has no aria role: it's just plain text.
  assertEquals(null, text.getAttribute('role'));
  const initialText = 'Some very informative text message';
  assertEquals(initialText, text.getAttribute('aria-label'));
  assertEquals(null, text.getAttribute('tabindex'));

  // Setup the FilesMessage visual signals (click) callback.
  /** @type {?string} */
  let signal = null;
  message.setSignalCallback((name) => {
    assert(typeof name === 'string');
    signal = name;
  });

  // Clicking the main element does not emit a visual signal.
  signal = 'main-element-not-clickable';
  assertNotEqual(null, message.onclick);
  message.click();
  assertEquals('main-element-not-clickable', signal);

  // Clicking sub-elements emits a visual signal, if needed.
  icon.click();
  assertEquals('cr:expand-more', signal);
  assertEquals(null, text.onclick);
  signal = 'text-element-not-clickable';
  text.click();
  assertEquals('text-element-not-clickable', signal);
  dismiss.click();
  assertEquals('dismiss', signal);
  action.click();
  assertEquals('action', signal);
  close.click();
  assertEquals('cr:close', signal);

  // The icon button icon type and label can be changed.
  message.icon = {icon: 'cr:open', label: 'aria icon label'};
  assertEquals('aria icon label', icon.getAttribute('aria-label'));
  assertEquals('cr:open', icon.getAttribute('iron-icon'));
  assertEquals('0', icon.getAttribute('tabindex'));

  // Clicking the icon signals its iron-icon name.
  signal = null;
  icon.click();
  assertEquals('cr:open', signal);

  // The icon may be changed to be an informative icon.
  message.info = {icon: 'cr:info', label: 'aria info label'};
  assertEquals('aria info label', icon.getAttribute('aria-label'));
  assertEquals('cr:info', icon.getAttribute('iron-icon'));

  // The role of an informative icon is image, not button.
  assertEquals('img', icon.getAttribute('role'));
  assertEquals(null, icon.getAttribute('tabindex'));

  // Clicking an informative icon does not emit a signal.
  signal = 'informative-icon-not-clickable';
  assertEquals(null, icon.onclick);
  icon.click();
  assertEquals('informative-icon-not-clickable', signal);

  // Setting icon button to null, hides the button.
  message.icon = null;
  assertEquals('hidden', window.getComputedStyle(icon).visibility);
  assertEquals(null, icon.getAttribute('iron-icon'));
  signal = 'hidden-icon-not-clickable';
  assertEquals(null, icon.onclick);
  icon.click();
  assertEquals('hidden-icon-not-clickable', signal);

  // The close button aria-label can be changed.
  message.close = {label: 'aria close label'};
  assertEquals('aria close label', close.getAttribute('aria-label'));
  assertEquals('cr:close', close.getAttribute('iron-icon'));
  assertNotEqual(null, close.onclick);
  signal = null;
  close.click();
  assertEquals('cr:close', signal);

  // Setting close button to null, hides the button.
  message.close = null;
  assertEquals('hidden', window.getComputedStyle(close).visibility);
  assertEquals(null, close.getAttribute('iron-icon'));
  signal = 'hidden-close-icon-not-clickable';
  assertEquals(null, close.onclick);
  close.click();
  assertEquals('hidden-close-icon-not-clickable', signal);

  // Setting message text, dismiss, and action, changes their text.
  message.message = 'Change text';
  assertEquals('Change text', text.getAttribute('aria-label'));
  message.dismiss = 'Change dismiss';
  assertEquals('Change dismiss', dismiss.getAttribute('aria-label'));
  message.action = 'Change action';
  assertEquals('Change action', action.getAttribute('aria-label'));

  // Setting dimiss button to null, hides the button.
  message.dismiss = null;
  assertEquals('none', window.getComputedStyle(dismiss).display);
  assertEquals(null, dismiss.getAttribute('aria-label'));
  signal = 'hidden-dismiss-button-not-clickable';
  assertEquals(null, dismiss.onclick);
  dismiss.click();
  assertEquals('hidden-dismiss-button-not-clickable', signal);

  // Setting action button to null, hides the button.
  message.action = null;
  assertEquals('none', window.getComputedStyle(action).display);
  assertEquals(null, action.getAttribute('aria-label'));
  signal = 'hidden-action-button-not-clickable';
  assertEquals(null, action.onclick);
  action.click();
  assertEquals('hidden-action-button-not-clickable', signal);

  // Setting null text should display empty text.
  message.message = null;
  assertEquals('', text.getAttribute('aria-label'));
  assertNotEqual('none', window.getComputedStyle(text).display);

  done();
}
