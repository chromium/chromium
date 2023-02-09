// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './xf_button.js';
import './xf_circular_progress.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {str, util} from '../../common/js/util.js';

import {DisplayPanel} from './xf_display_panel.js';

/** @type {!HTMLTemplateElement} */
const htmlTemplate = html`{__html_template__}`;

/**
 * A panel to display the status or progress of a file operation.
 * @extends HTMLElement
 */
export class PanelItem extends HTMLElement {
  constructor() {
    super();
    const fragment = htmlTemplate.content.cloneNode(true);
    this.attachShadow({mode: 'open'}).appendChild(fragment);

    /** @private {Element} */
    this.indicator_ = this.shadowRoot.querySelector('#indicator');

    /**
     * TODO(crbug.com/947388) make this a closure enum.
     * @const
     */
    this.panelTypeDefault = -1;
    this.panelTypeProgress = 0;
    this.panelTypeSummary = 1;
    this.panelTypeDone = 2;
    this.panelTypeError = 3;
    this.panelTypeInfo = 4;
    this.panelTypeFormatProgress = 5;
    this.panelTypeSyncProgress = 6;

    /** @private {number} */
    this.panelType_ = this.panelTypeDefault;

    /** @private @type {?function(Event)} */
    this.onclick = this.onClicked_.bind(this);

    /** @public {?DisplayPanel} */
    this.parent = null;

    /**
     * Callback that signals events happening in the panel (e.g. click).
     * @private @type {!function(*)}
     */
    this.signal_ = console.log;

    /**
     * User specific data, used as a reference to persist any custom
     * data that the panel user may want to use in the signal callback.
     * e.g. holding the file name(s) used in a copy operation.
     * @type {?Object}
     */
    this.userData = null;
  }

  /**
   * Remove an element from the panel using it's id.
   * @return {?Element}
   * @private
   */
  removePanelElementById_(id) {
    const element = this.shadowRoot.querySelector(id);
    if (element) {
      element.remove();
    }
    return element;
  }

  /**
   * Sets up the different panel types. Panels have per-type configuration
   * templates, but can be further customized using individual attributes.
   * @param {number} type The enumerated panel type to set up.
   * @private
   */
  setPanelType(type) {
    this.setAttribute('detailed-panel', 'detailed-panel');

    if (this.panelType_ === type) {
      return;
    }

    // Remove the indicators/buttons that can change.
    this.removePanelElementById_('#indicator');
    let element = this.removePanelElementById_('#primary-action');
    if (element) {
      element.onclick = null;
    }
    element = this.removePanelElementById_('#secondary-action');
    if (element) {
      element.onclick = null;
    }

    // Mark the indicator as empty so it recreates on setAttribute.
    this.setAttribute('indicator', 'empty');

    const buttonSpacer = this.shadowRoot.querySelector('#button-gap');

    // Default the text host to use an alert role.
    const textHost = assert(this.shadowRoot.querySelector('.xf-panel-text'));
    textHost.setAttribute('role', 'alert');

    const hasExtraButton = !!this.dataset['extraButtonText'];
    // Setup the panel configuration for the panel type.
    // TOOD(crbug.com/947388) Simplify this switch breaking out common cases.
    /** @type {?Element} */
    let primaryButton = null;
    /** @type {?Element} */
    let secondaryButton = null;
    switch (type) {
      case this.panelTypeProgress:
        this.setAttribute('indicator', 'progress');
        secondaryButton = document.createElement('xf-button');
        secondaryButton.id = 'secondary-action';
        secondaryButton.onclick = assert(this.onclick);
        secondaryButton.dataset.category = 'cancel';
        secondaryButton.setAttribute('aria-label', str('CANCEL_LABEL'));
        buttonSpacer.insertAdjacentElement('afterend', secondaryButton);
        break;
      case this.panelTypeSummary:
        this.setAttribute('indicator', 'largeprogress');
        primaryButton = document.createElement('xf-button');
        primaryButton.id = 'primary-action';
        primaryButton.dataset.category = 'expand';
        primaryButton.setAttribute('aria-label', str('FEEDBACK_EXPAND_LABEL'));
        // Remove the 'alert' role to stop screen readers repeatedly
        // reading each progress update.
        textHost.setAttribute('role', '');
        buttonSpacer.insertAdjacentElement('afterend', primaryButton);
        break;
      case this.panelTypeDone:
        this.setAttribute('indicator', 'status');
        this.setAttribute('status', 'success');
        secondaryButton = document.createElement('xf-button');
        secondaryButton.id =
            (hasExtraButton) ? 'secondary-action' : 'primary-action';
        secondaryButton.onclick = assert(this.onclick);
        secondaryButton.dataset.category = 'dismiss';
        buttonSpacer.insertAdjacentElement('afterend', secondaryButton);
        if (hasExtraButton) {
          primaryButton = document.createElement('xf-button');
          primaryButton.id = 'primary-action';
          primaryButton.dataset['category'] = 'extra-button';
          primaryButton.onclick = assert(this.onclick);
          primaryButton.setExtraButtonText(this.dataset['extraButtonText']);
          buttonSpacer.insertAdjacentElement('afterend', primaryButton);
        }
        break;
      case this.panelTypeError:
        this.setAttribute('indicator', 'status');
        this.setAttribute('status', 'failure');
        this.primaryText = str('FILE_ERROR_GENERIC');
        this.secondaryText = '';
        secondaryButton = document.createElement('xf-button');
        secondaryButton.id =
            (hasExtraButton) ? 'secondary-action' : 'primary-action';
        secondaryButton.onclick = assert(this.onclick);
        secondaryButton.dataset.category = 'dismiss';
        buttonSpacer.insertAdjacentElement('afterend', secondaryButton);
        if (hasExtraButton) {
          primaryButton = document.createElement('xf-button');
          primaryButton.id = 'primary-action';
          primaryButton.dataset.category = 'extra-button';
          primaryButton.onclick = assert(this.onclick);
          primaryButton.setExtraButtonText(this.dataset['extraButtonText']);
          buttonSpacer.insertAdjacentElement('afterend', primaryButton);
        }
        break;
      case this.panelTypeInfo:
        break;
      case this.panelTypeFormatProgress:
        this.setAttribute('indicator', 'status');
        this.setAttribute('status', 'hard-drive');
        break;
      case this.panelTypeSyncProgress:
        this.setAttribute('indicator', 'progress');
        break;
    }

    this.panelType_ = type;
  }

  /**
   * Registers this instance to listen to these attribute changes.
   * @private
   */
  static get observedAttributes() {
    return [
      'count',
      'errormark',
      'indicator',
      'panel-type',
      'primary-text',
      'progress',
      'secondary-text',
      'status',
    ];
  }

  /**
   * Callback triggered by the browser when our attribute values change.
   * @param {string} name Attribute that's changed.
   * @param {?string} oldValue Old value of the attribute.
   * @param {?string} newValue New value of the attribute.
   * @private
   */
  attributeChangedCallback(name, oldValue, newValue) {
    /** @type {Element} */
    let indicator = null;
    /** @type {Element} */
    let textNode;
    // TODO(adanilo) Chop out each attribute handler into a function.
    switch (name) {
      case 'count':
        if (this.indicator_) {
          this.indicator_.setAttribute('label', newValue || '');
        }
        break;
      case 'errormark':
        if (this.indicator_) {
          this.indicator_.setAttribute('errormark', newValue || '');
        }
        break;
      case 'indicator':
        // Get rid of any existing indicator
        const oldIndicator = this.shadowRoot.querySelector('#indicator');
        if (oldIndicator) {
          oldIndicator.remove();
        }
        switch (newValue) {
          case 'progress':
          case 'largeprogress':
            indicator = document.createElement('xf-circular-progress');
            if (newValue === 'largeprogress') {
              indicator.setAttribute('radius', '14');
            } else {
              indicator.setAttribute('radius', '10');
            }
            break;
          case 'status':
            indicator = document.createElement('iron-icon');
            const status = this.getAttribute('status');
            if (status) {
              indicator.setAttribute('icon', `files36:${status}`);
            }
            break;
        }
        this.indicator_ = indicator;
        if (indicator) {
          const itemRoot = this.shadowRoot.querySelector('.xf-panel-item');
          indicator.setAttribute('id', 'indicator');
          itemRoot.prepend(indicator);
        }
        break;
      case 'panel-type':
        this.setPanelType(Number(newValue));
        if (this.parent && this.parent.updateSummaryPanel) {
          this.parent.updateSummaryPanel();
        }
        break;
      case 'progress':
        if (this.indicator_) {
          this.indicator_.progress = Number(newValue);
          if (this.parent && this.parent.updateProgress) {
            this.parent.updateProgress();
          }
        }
        break;
      case 'status':
        if (this.indicator_) {
          this.indicator_.setAttribute('icon', `files36:${newValue}`);
        }
        break;
      case 'primary-text':
        textNode = this.shadowRoot.querySelector('.xf-panel-label-text');
        if (textNode) {
          textNode.textContent = newValue;
          // Set the aria labels for the activity and cancel button.
          this.setAttribute('aria-label', /** @type {string} */ (newValue));
        }
        break;
      case 'secondary-text':
        textNode = this.shadowRoot.querySelector('.xf-panel-secondary-text');
        if (!textNode) {
          const parent = this.shadowRoot.querySelector('.xf-panel-text');
          if (!parent) {
            return;
          }
          textNode = document.createElement('span');
          textNode.setAttribute('class', 'xf-panel-secondary-text');
          parent.appendChild(textNode);
        }
        // Remove the secondary text node if the text is empty
        if (newValue == '') {
          textNode.remove();
        } else {
          textNode.textContent = newValue;
        }
        break;
    }
  }

  /**
   * DOM connected.
   * @private
   */
  connectedCallback() {
    this.onclick = this.onClicked_.bind(this);

    // Set click event handler references.
    let button = this.shadowRoot.querySelector('#primary-action');
    if (button) {
      button.onclick = this.onclick;
    }
    button = this.shadowRoot.querySelector('#secondary-action');
    if (button) {
      button.onclick = this.onclick;
    }
  }

  /**
   * DOM disconnected.
   * @private
   */
  disconnectedCallback() {
    // Replace references to any signal callback.
    this.signal_ = console.log;

    // Clear click event handler references.
    let button = this.shadowRoot.querySelector('#primary-action');
    if (button) {
      button.onclick = null;
    }
    button = this.shadowRoot.querySelector('#secondary-action');
    if (button) {
      button.onclick = null;
    }
    this.onclick = null;
  }

  /**
   * Handles 'click' events from our sub-elements and sends
   * signals to the |signal_| callback if needed.
   * @param {?Event} event
   * @private
   */
  onClicked_(event) {
    event.stopImmediatePropagation();
    event.preventDefault();

    // Ignore clicks on the panel item itself.
    if (event.target === this) {
      return;
    }

    const id = assert(event.target.dataset.category);
    this.signal_(id);
  }

  /**
   * Sets the callback that triggers signals from events on the panel.
   * @param {?function(*)} signal
   */
  set signalCallback(signal) {
    this.signal_ = signal || console.log;
  }

  /**
   * Set the visibility of the error marker.
   * @param {string} visibility Visibility value being set.
   */
  set errorMarkerVisibility(visibility) {
    this.setAttribute('errormark', visibility);
  }

  /**
   *  Getter for the visibility of the error marker.
   */
  get errorMarkerVisibility() {
    // If we have an indicator on the panel, then grab the
    // visibility value from that.
    if (this.indicator_) {
      return this.indicator_.errorMarkerVisibility;
    }
    // If there's no indicator on the panel just return the
    // value of any attribute as a fallback.
    return this.getAttribute('errormark');
  }

  /**
   * Setter to set the indicator type.
   * @param {string} indicator Progress (optionally large) or status.
   */
  set indicator(indicator) {
    this.setAttribute('indicator', indicator);
  }

  /**
   *  Getter for the progress indicator.
   */
  get indicator() {
    return this.getAttribute('indicator');
  }

  /**
   * Setter to set the success/failure indication.
   * @param {string} status Status value being set.
   */
  set status(status) {
    this.setAttribute('status', status);
  }

  /**
   *  Getter for the success/failure indication.
   */
  get status() {
    return this.getAttribute('status');
  }

  /**
   * Setter to set the progress property, sent to any child indicator.
   * @param {string} progress Progress value being set.
   * @public
   */
  set progress(progress) {
    this.setAttribute('progress', progress);
  }

  /**
   *  Getter for the progress indicator percentage.
   */
  get progress() {
    return this.indicator_.progress || 0;
  }

  /**
   * Setter to set the primary text on the panel.
   * @param {string} text Text to be shown.
   */
  set primaryText(text) {
    this.setAttribute('primary-text', text);
  }

  /**
   * Getter for the primary text on the panel.
   * @return {string}
   */
  get primaryText() {
    return this.getAttribute('primary-text');
  }

  /**
   * Setter to set the secondary text on the panel.
   * @param {string} text Text to be shown.
   */
  set secondaryText(text) {
    this.setAttribute('secondary-text', text);
  }

  /**
   * Getter for the secondary text on the panel.
   * @return {string}
   */
  get secondaryText() {
    return this.getAttribute('secondary-text');
  }

  /**
   * @param {boolean} shouldFade Whether the secondary text should be displayed
   *     with a faded color to avoid drawing too much attention to it.
   */
  set fadeSecondaryText(shouldFade) {
    this.toggleAttribute('fade-secondary-text', shouldFade);
  }

  /**
   * @return {boolean} Whether the secondary text should be displayed with a
   *     faded color to avoid drawing too much attention to it.
   */
  get fadeSecondaryText() {
    return !!this.getAttribute('fade-secondary-text');
  }

  /**
   * Setter to set the panel type.
   * @param {number} type Enum value for the panel type.
   */
  set panelType(type) {
    this.setAttribute('panel-type', type);
  }

  /**
   * Getter for the panel type.
   * TODO(crbug.com/947388) Add closure annotations to getters.
   */
  get panelType() {
    return this.panelType_;
  }

  /**
   * Getter for the primary action button.
   */
  get primaryButton() {
    return this.shadowRoot.querySelector('#primary-action');
  }

  /**
   * Getter for the secondary action button.
   */
  get secondaryButton() {
    return this.shadowRoot.querySelector('#secondary-action');
  }

  /**
   * Getter for the panel text div.
   */
  get textDiv() {
    return this.shadowRoot.querySelector('.xf-panel-text');
  }

  /**
   * Setter to replace the default aria-label on any close button.
   * @param {string} text Text to set for the 'aria-label'.
   */
  set closeButtonAriaLabel(text) {
    const action = this.shadowRoot.querySelector('#secondary-action');
    if (action && action.dataset.category === 'cancel') {
      action.setAttribute('aria-label', text);
    }
  }
}

window.customElements.define('xf-panel-item', PanelItem);

//# sourceURL=//ui/file_manager/file_manager/foreground/elements/xf_panel_item.js
