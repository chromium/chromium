// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Action links are elements that are used to perform an in-page navigation or
// action (e.g. showing a dialog).
//
// They look like normal anchor (<a>) tags as their text color is blue. However,
// they're subtly different as they're not initially underlined (giving users a
// clue that underlined links navigate while action links don't).
//
// Action links look very similar to normal links when hovered (hand cursor,
// underlined). This gives the user an idea that clicking this link will do
// something similar to navigation but in the same page.
//
// They can be created in JavaScript like this (note second arg):
//
//   var link = document.createElement('a', {is: 'action-link'});
//
// or with a constructor like this:
//
//   var link = new ActionLink();
//
// They can be used easily from HTML as well, like so:
//
//   <a is="action-link">Click me!</a>
//
// NOTE: <action-link> and document.createElement('action-link') don't work.

class ActionLink extends HTMLAnchorElement {
  connectedCallback() {
    // Action links can start disabled (e.g. <a is="action-link" disabled>).
    this.tabIndex = this.disabled ? -1 : 0;

    if (!this.hasAttribute('role')) {
      this.setAttribute('role', 'link');
    }

    this.addEventListener('keydown', function(e) {
      if (!this.disabled && e.key === 'Enter' && !this.href) {
        // Schedule a click asynchronously because other 'keydown' handlers
        // may still run later (e.g. document.addEventListener('keydown')).
        // Specifically options dialogs break when this timeout isn't here.
        // NOTE: this affects the "trusted" state of the ensuing click. I
        // haven't found anything that breaks because of this (yet).
        window.setTimeout(this.click.bind(this), 0);
      }
    });

    function preventDefault(e) {
      e.preventDefault();
    }

    function removePreventDefault() {
      document.removeEventListener('selectstart', preventDefault);
      document.removeEventListener('mouseup', removePreventDefault);
    }

    this.addEventListener('mousedown', function() {
      // This handlers strives to match the behavior of <a href="...">.

      // While the mouse is down, prevent text selection from dragging.
      document.addEventListener('selectstart', preventDefault);
      document.addEventListener('mouseup', removePreventDefault);

      // If focus started via mouse press, don't show an outline.
      if (document.activeElement !== this) {
        this.classList.add('no-outline');
      }
    });

    this.addEventListener('blur', function() {
      this.classList.remove('no-outline');
    });
  }

  /** @param {boolean} disabled */
  set disabled(disabled) {
    if (disabled) {
      HTMLAnchorElement.prototype.setAttribute.call(this, 'disabled', '');
    } else {
      HTMLAnchorElement.prototype.removeAttribute.call(this, 'disabled');
    }
    this.tabIndex = disabled ? -1 : 0;
  }

  get disabled() {
    return this.hasAttribute('disabled');
  }

  /** @override */
  setAttribute(attr, val) {
    if (attr.toLowerCase() === 'disabled') {
      this.disabled = true;
    } else {
      HTMLAnchorElement.prototype.setAttribute.apply(this, arguments);
    }
  }

  /** @override */
  removeAttribute(attr) {
    if (attr.toLowerCase() === 'disabled') {
      this.disabled = false;
    } else {
      HTMLAnchorElement.prototype.removeAttribute.apply(this, arguments);
    }
  }
}
customElements.define('action-link', ActionLink, {extends: 'a'});
