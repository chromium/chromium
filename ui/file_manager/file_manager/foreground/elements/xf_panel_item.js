// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A panel to display the status or progress of a file operation.
 * @extends HTMLElement
 */
class PanelItem extends HTMLElement {
  constructor() {
    super();
    const host = document.createElement('template');
    host.innerHTML = this.constructor.template_;
    this.attachShadow({mode: 'open'}).appendChild(host.content.cloneNode(true));

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
     * @type {string|Object}
     */
    this.userData = null;
  }

  /**
   * Static getter for the custom element template.
   * @private
   * @return {string}
   */
  static get template_() {
    return `<style>
              .xf-panel-item {
                  align-items: center;
                  background-color: rgba(0,0,0,0);
                  border-radius: 4px;
                  display: flex;
                  flex-direction: row;
                  max-width: 400px;
              }

              .xf-button {
                  height: 36px;
                  width: 36px;
              }

              .xf-panel-text {
                  font: 13px Roboto;
                  line-height: 20px;
                  max-width: 216px;
                  overflow: hidden;
                  text-overflow: ellipsis;
                  white-space: nowrap;
              }

              .xf-panel-label-text {
                  outline: none;
              }

              :host([panel-type='3']) .xf-panel-text {
                  width: 216px;
              }

              :host([panel-type='3']) .xf-panel-label-text {
                  display: -webkit-box;
                  -webkit-line-clamp: 2;
                  -webkit-box-orient: vertical;
                  overflow: hidden;
                  white-space: normal;
              }

              :host([panel-type='3']) .xf-linebreaker {
                  display: none;
              }

              .xf-panel-label-text {
                  color: var(--google-grey-900);
                  max-width: 216px;
                  text-overflow: ellipsis;
                  overflow: hidden;
                  white-space: nowrap;
              }

              .xf-panel-secondary-text {
                  color: var(--google-grey-700);
              }

              :host(:not([detailed-panel])) .xf-padder-4 {
                  width: 4px;
              }

              :host(:not([detailed-panel])) .xf-padder-16 {
                  width: 16px;
              }

              :host(:not([detailed-panel])) .xf-grow-padder {
                  flex-grow: 16;
                  width: 24px;
              }

              xf-circular-progress {
                  padding: 16px;
              }

              :host(:not([detailed-summary])) iron-icon {
                  height: 36px;
                  padding: 16px;
                  width: 36px;
              }

              // TODO(crbug.com/947388) Use '--goog' prefixed CSS varables.
              .xf-success {
                  color: rgb(52, 168, 83);
              }

              .xf-failure {
                  color: rgb(234, 67, 53);
              }

              :host([panel-type='0']) .xf-panel-item {
                  height: var(--progress-height);
                  padding-top: var(--progress-padding-top);
                  padding-bottom: var(--progress-padding-bottom);
              }

              :host(:not([panel-type='0'])) .xf-panel-item {
                  height: 68px;
              }

              :host([detailed-panel]) .xf-panel-item {
                  height: 68px;
                  width: 400px;
              }

              :host([detailed-panel]:not([detailed-summary])) .xf-panel-text {
                  margin-inline-end: 24px;
                  margin-inline-start: 24px;
              }

              :host([detailed-panel][panel-type='2']) .xf-panel-secondary-text {
                  color: var(--google-green-600);
              }

              :host([detailed-panel]:not([detailed-summary])) xf-button {
                  margin-inline-end: 12px;
                  margin-inline-start: auto;
              }

              :host([detailed-panel]:not([detailed-summary])) #indicator {
                  display: none;
              }

              :host([detailed-summary][data-category='collapsed'])
              .xf-panel-item {
                  width: 236px;
              }

              :host([detailed-summary]) .xf-panel-text {
                  align-items: center;
                  display: flex;
                  height: 48px;
                  max-width: unset;
                  width: 100%;
              }

              :host([detailed-summary]) #indicator {
                  margin-inline-start: 22px;
                  padding: 0;
              }

              :host([detailed-summary][data-category='collapsed']) #indicator {
                  margin-inline-end: 20px;
                  min-width: 28px;
              }

              :host([detailed-summary][data-category='expanded']) #indicator {
                  margin-inline-end: 18px;
                  min-width: 32px;
              }

              :host([detailed-summary]) #primary-action {
                  align-items: center;
                  display: flex;
                  height: 48px;
                  justify-content: center;
                  margin-inline-end: 10px;
                  margin-inline-start: auto;
                  width: 48px;
              }

              :host([detailed-panel]) .xf-padder-4 {
                  display: none;
              }

              :host([detailed-panel]) .xf-padder-16 {
                  display: none;
              }

              :host([detailed-panel]) .xf-grow-padder {
                  display: none;
              }
            </style>
            <div class='xf-panel-item'>
                <xf-circular-progress id='indicator'>
                </xf-circular-progress>
                <div class='xf-panel-text' role='alert'>
                    <span class='xf-panel-label-text' tabindex='0'>
                    </span>
                    <br class='xf-linebreaker'/>
                </div>
                <div class='xf-grow-padder'></div>
                <xf-button id='secondary-action' tabindex='-1'>
                </xf-button>
                <div id='button-gap' class='xf-padder-4'></div>
                <xf-button id='primary-action' tabindex='-1'>
                </xf-button>
                <div class='xf-padder-16'></div>
            </div>`;
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
    if (util.isTransferDetailsEnabled()) {
      this.setAttribute('detailed-panel', 'detailed-panel');
    }

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
        secondaryButton.setAttribute('aria-label', '$i18n{CANCEL_LABEL}');
        buttonSpacer.insertAdjacentElement('afterend', secondaryButton);
        break;
      case this.panelTypeSummary:
        this.setAttribute('indicator', 'largeprogress');
        primaryButton = document.createElement('xf-button');
        primaryButton.id = 'primary-action';
        primaryButton.dataset.category = 'expand';
        primaryButton.setAttribute(
            'aria-label', '$i18n{FEEDBACK_EXPAND_LABEL}');
        // Remove the 'alert' role to stop screen readers repeatedly
        // reading each progress update.
        textHost.setAttribute('role', '');
        buttonSpacer.insertAdjacentElement('afterend', primaryButton);
        break;
      case this.panelTypeDone:
        this.setAttribute('indicator', 'status');
        this.setAttribute('status', 'success');
        primaryButton = document.createElement('xf-button');
        primaryButton.id = 'primary-action';
        primaryButton.onclick = assert(this.onclick);
        primaryButton.dataset.category = 'dismiss';
        buttonSpacer.insertAdjacentElement('afterend', primaryButton);
        break;
      case this.panelTypeError:
        this.setAttribute('indicator', 'status');
        this.setAttribute('status', 'failure');
        this.primaryText = '$i18n{FILE_ERROR_GENERIC}';
        this.secondaryText = '';
        secondaryButton = document.createElement('xf-button');
        secondaryButton.id = 'secondary-action';
        secondaryButton.onclick = assert(this.onclick);
        secondaryButton.dataset.category = 'dismiss';
        buttonSpacer.insertAdjacentElement('afterend', secondaryButton);
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
