// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './xf_button.js';
import './xf_circular_progress.js';

import type {IronIconElement} from 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {str} from '../../common/js/translations.js';

import type {PanelButton} from './xf_button.js';
import type {CircularProgress} from './xf_circular_progress.js';
import {getTemplate} from './xf_panel_item.html.js';

export enum PanelType {
  DEFAULT = -1,
  PROGRESS = 0,
  SUMMARY,
  DONE,
  ERROR,
  INFO,
  FORMAT_PROGRESS,
  SYNC_PROGRESS,
}

export interface UserData {
  source?: string;
  destination?: string;
  count?: number;
}

type IronIconWithProgress = IronIconElement&{progress: string};

/**
 * A panel to display the status or progress of a file operation.
 */
export class PanelItem extends HTMLElement {
  private indicator_: CircularProgress|IronIconWithProgress|null = null;

  private panelType_ = PanelType.DEFAULT;

  /**
   * Callback that signals events happening in the panel (e.g. click).
   */
  private signal_: (clickedButton: string) => void = console.info;

  private updateSummaryPanel_: VoidCallback|null = null;
  private updateProgress_: VoidCallback|null = null;

  private onClickedBound_ = this.onClicked_.bind(this);

  /**
   * User specific data, used as a reference to persist any custom
   * data that the panel user may want to use in the signal callback.
   * e.g. holding the file name(s) used in a copy operation.
   */
  userData: UserData|null = null;

  constructor() {
    super();
    const template = document.createElement('template');
    template.innerHTML = getTemplate() as unknown as string;
    const fragment = template.content.cloneNode(true);
    this.attachShadow({mode: 'open'}).appendChild(fragment);

    this.indicator_ =
        this.shadowRoot!.querySelector<CircularProgress>('#indicator')!;
  }

  static get is() {
    return 'xf-panel-item' as const;
  }

  /**
   * Remove an element from the panel using it's id.
   */
  private removePanelElementById_(id: string): Element|null {
    const element = this.shadowRoot!.querySelector(id);
    if (element) {
      element.remove();
    }
    return element;
  }

  /**
   * Sets up the different panel types. Panels have per-type configuration
   * templates, but can be further customized using individual attributes.
   * @param type The enumerated panel type to set up.
   */
  setPanelType(type: PanelType) {
    this.setAttribute('detailed-panel', 'detailed-panel');

    if (this.panelType_ === type) {
      return;
    }

    // Remove the indicators/buttons that can change.
    this.removePanelElementById_('#indicator');
    let element = this.removePanelElementById_('#primary-action');
    if (element) {
      element.removeEventListener('click', this.onClickedBound_);
    }
    element = this.removePanelElementById_('#secondary-action');
    if (element) {
      element.removeEventListener('click', this.onClickedBound_);
    }

    // Mark the indicator as empty so it recreates on setAttribute.
    this.setAttribute('indicator', 'empty');

    const buttonSpacer = this.shadowRoot!.querySelector('#button-gap')!;

    // Default the text host to use an alert role.
    const textHost = this.shadowRoot!.querySelector('.xf-panel-text')!;
    textHost.setAttribute('role', 'alert');

    const hasExtraButton = !!this.dataset['extraButtonText'];
    // Setup the panel configuration for the panel type.
    // TOOD(crbug.com/947388) Simplify this switch breaking out common cases.
    let primaryButton: PanelButton|null = null;
    let secondaryButton: PanelButton|null = null;
    switch (type) {
      case PanelType.PROGRESS:
        this.setAttribute('indicator', 'progress');
        secondaryButton = document.createElement('xf-button');
        secondaryButton.id = 'secondary-action';
        secondaryButton.addEventListener('click', this.onClickedBound_);
        secondaryButton.dataset['category'] = 'cancel';
        secondaryButton.setAttribute('aria-label', str('CANCEL_LABEL'));
        buttonSpacer.insertAdjacentElement('afterend', secondaryButton);
        break;
      case PanelType.SUMMARY:
        this.setAttribute('indicator', 'largeprogress');
        primaryButton = document.createElement('xf-button');
        primaryButton.id = 'primary-action';
        primaryButton.dataset['category'] = 'expand';
        primaryButton.setAttribute('aria-label', str('FEEDBACK_EXPAND_LABEL'));
        // Remove the 'alert' role to stop screen readers repeatedly
        // reading each progress update.
        textHost.setAttribute('role', '');
        buttonSpacer.insertAdjacentElement('afterend', primaryButton);
        break;
      case PanelType.DONE:
        this.setAttribute('indicator', 'status');
        this.setAttribute('status', 'success');
        secondaryButton = document.createElement('xf-button');
        secondaryButton.id =
            (hasExtraButton) ? 'secondary-action' : 'primary-action';
        secondaryButton.addEventListener('click', this.onClickedBound_);
        secondaryButton.dataset['category'] = 'dismiss';
        buttonSpacer.insertAdjacentElement('afterend', secondaryButton);
        if (hasExtraButton) {
          primaryButton = document.createElement('xf-button');
          primaryButton.id = 'primary-action';
          primaryButton.dataset['category'] = 'extra-button';
          primaryButton.addEventListener('click', this.onClickedBound_);
          primaryButton.setExtraButtonText(
              this.dataset['extraButtonText'] ?? '');
          buttonSpacer.insertAdjacentElement('afterend', primaryButton);
        }
        break;
      case PanelType.ERROR:
        this.setAttribute('indicator', 'status');
        this.setAttribute('status', 'failure');
        secondaryButton = document.createElement('xf-button');
        secondaryButton.id =
            (hasExtraButton) ? 'secondary-action' : 'primary-action';
        secondaryButton.addEventListener('click', this.onClickedBound_);
        secondaryButton.dataset['category'] = 'dismiss';
        buttonSpacer.insertAdjacentElement('afterend', secondaryButton);
        if (hasExtraButton) {
          primaryButton = document.createElement('xf-button');
          primaryButton.id = 'primary-action';
          primaryButton.dataset['category'] = 'extra-button';
          primaryButton.addEventListener('click', this.onClickedBound_);
          primaryButton.setExtraButtonText(
              this.dataset['extraButtonText'] ?? '');
          buttonSpacer.insertAdjacentElement('afterend', primaryButton);
        }
        break;
      case PanelType.INFO:
        this.setAttribute('indicator', 'status');
        this.setAttribute('status', 'warning');
        secondaryButton = document.createElement('xf-button');
        secondaryButton.id =
            (hasExtraButton) ? 'secondary-action' : 'primary-action';
        secondaryButton.addEventListener('click', this.onClickedBound_);
        secondaryButton.dataset['category'] = 'cancel';
        buttonSpacer.insertAdjacentElement('afterend', secondaryButton);
        if (hasExtraButton) {
          primaryButton = document.createElement('xf-button');
          primaryButton.id = 'primary-action';
          primaryButton.dataset['category'] = 'extra-button';
          primaryButton.addEventListener('click', this.onClickedBound_);
          primaryButton.setExtraButtonText(
              this.dataset['extraButtonText'] ?? '');
          buttonSpacer.insertAdjacentElement('afterend', primaryButton);
        }
        break;
      case PanelType.FORMAT_PROGRESS:
        this.setAttribute('indicator', 'status');
        this.setAttribute('status', 'hard-drive');
        break;
      case PanelType.SYNC_PROGRESS:
        this.setAttribute('indicator', 'progress');
        break;
    }

    this.panelType_ = type;
  }

  /**
   * Registers this instance to listen to these attribute changes.
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
   */
  attributeChangedCallback(
      name: string, _: string|null, newValue: string|null) {
    let indicator: CircularProgress|IronIconWithProgress|null = null;
    let textNode: HTMLElement|null;
    // TODO(adanilo) Chop out each attribute handler into a function.
    switch (name) {
      case 'count':
        if (this.indicator_) {
          this.indicator_.setAttribute('label', newValue ?? '');
        }
        break;
      case 'errormark':
        if (this.indicator_) {
          this.indicator_.setAttribute('errormark', newValue ?? '');
        }
        break;
      case 'indicator':
        // Get rid of any existing indicator
        const oldIndicator = this.shadowRoot!.querySelector('#indicator');
        if (oldIndicator) {
          oldIndicator.remove();
        }
        switch (newValue) {
          case 'progress':
          case 'largeprogress':
            indicator = document.createElement('xf-circular-progress') as
                CircularProgress;
            if (newValue === 'largeprogress') {
              indicator.setAttribute('radius', '14');
            } else {
              indicator.setAttribute('radius', '10');
            }
            break;
          case 'status':
            indicator =
                document.createElement('iron-icon') as IronIconWithProgress;
            const status = this.getAttribute('status');
            if (status) {
              indicator.setAttribute('icon', `files36:${status}`);
            }
            break;
        }
        this.indicator_ = indicator;
        if (indicator) {
          const itemRoot =
              this.shadowRoot!.querySelector<PanelItem>('.xf-panel-item')!;
          indicator.setAttribute('id', 'indicator');
          itemRoot.prepend(indicator);
        }
        break;
      case 'panel-type':
        const panelType = Number(newValue);
        if (panelType in PanelType) {
          this.setPanelType(panelType);
        }
        if (this.updateSummaryPanel_) {
          this.updateSummaryPanel_();
        }
        break;
      case 'progress':
        if (this.indicator_) {
          this.indicator_!.progress = newValue ?? '';
          if (this.updateProgress_) {
            this.updateProgress_();
          }
        }
        break;
      case 'status':
        if (this.indicator_) {
          this.indicator_.setAttribute('icon', `files36:${newValue}`);
        }
        break;
      case 'primary-text':
        textNode = this.shadowRoot!.querySelector('.xf-panel-label-text')!;
        if (textNode) {
          textNode.textContent = newValue;
          // Set the aria labels for the activity and cancel button.
          this.setAttribute('aria-label', newValue ?? '');
        }
        break;
      case 'secondary-text':
        textNode = this.shadowRoot!.querySelector<HTMLSpanElement>(
            '.xf-panel-secondary-text');
        if (!textNode) {
          const parent = this.shadowRoot!.querySelector('.xf-panel-text');
          if (!parent) {
            return;
          }
          textNode = document.createElement('span');
          textNode.setAttribute('class', 'xf-panel-secondary-text');
          parent.appendChild(textNode);
        }
        // Remove the secondary text node if the text is empty
        if (newValue === '') {
          textNode.remove();
        } else {
          textNode.textContent = newValue;
        }
        break;
    }
  }

  /**
   * DOM connected.
   */
  connectedCallback() {
    this.addEventListener('click', this.onClickedBound_);

    // Set click event handler references.
    this.shadowRoot!.querySelector('#primary-action')
        ?.addEventListener('click', this.onClickedBound_);
    this.shadowRoot!.querySelector('#secondary-action')
        ?.addEventListener('click', this.onClickedBound_);
  }

  /**
   * DOM disconnected.
   */
  disconnectedCallback() {
    // Replace references to any signal callback.
    this.signal_ = console.info;
  }

  /**
   * Handles 'click' events from our sub-elements and sends
   * signals to the |signal_| callback if needed.
   */
  private onClicked_(event: Event) {
    event.stopImmediatePropagation();
    event.preventDefault();

    // Ignore clicks on the panel item itself.
    if (event.target === this || !event.target) {
      return;
    }

    const button = event.target! as PanelButton;
    const id = button.dataset['category'] ?? '';
    this.signal_(id);
  }

  /**
   * Sets the callback that triggers signals from events on the panel.
   */
  set signalCallback(signal: (signal: string) => void) {
    this.signal_ = signal || console.info;
  }

  /**
   * Set the visibility of the error marker.
   * @param visibility Visibility value being set.
   */
  set errorMarkerVisibility(visibility: string) {
    this.setAttribute('errormark', visibility);
  }

  /**
   *  Getter for the visibility of the error marker.
   */
  get errorMarkerVisibility() {
    // If we have an indicator on the panel, then grab the
    // visibility value from that.
    if (this.indicator_ && 'errorMarkerVisibility' in this.indicator_) {
      return this.indicator_.errorMarkerVisibility;
    }
    // If there's no indicator on the panel just return the
    // value of any attribute as a fallback.
    return this.getAttribute('errormark') ?? '';
  }

  /**
   * Setter to set the indicator type.
   * @param indicator Progress (optionally large) or status.
   */
  set indicator(indicator: string) {
    this.setAttribute('indicator', indicator);
  }

  /**
   *  Getter for the progress indicator.
   */
  get indicator() {
    return this.getAttribute('indicator') ?? '';
  }

  /**
   * Setter to set the success/failure indication.
   * @param status Status value being set.
   */
  set status(status: string) {
    this.setAttribute('status', status);
  }

  /**
   *  Getter for the success/failure indication.
   */
  get status() {
    return this.getAttribute('status') ?? '';
  }

  /**
   * Setter to set the progress property, sent to any child indicator.
   */
  set progress(progress: string) {
    this.setAttribute('progress', progress);
  }

  /**
   *  Getter for the progress indicator percentage.
   */
  get progress(): number {
    if (!this.indicator_ || !('progress' in this.indicator_)) {
      return 0;
    }
    return parseInt(this.indicator_?.progress, 10) || 0;
  }

  /**
   * Setter to set the primary text on the panel.
   * @param text Text to be shown.
   */
  set primaryText(text: string) {
    this.setAttribute('primary-text', text);
  }

  /**
   * Getter for the primary text on the panel.
   */
  get primaryText(): string {
    return this.getAttribute('primary-text') ?? '';
  }

  /**
   * Setter to set the secondary text on the panel.
   * @param text Text to be shown.
   */
  set secondaryText(text: string) {
    this.setAttribute('secondary-text', text);
  }

  /**
   * Getter for the secondary text on the panel.
   */
  get secondaryText(): string {
    return this.getAttribute('secondary-text') ?? '';
  }

  /**
   * @param shouldFade Whether the secondary text should be displayed
   *     with a faded color to avoid drawing too much attention to it.
   */
  set fadeSecondaryText(shouldFade: boolean) {
    this.toggleAttribute('fade-secondary-text', shouldFade);
  }

  /**
   * @return Whether the secondary text should be displayed with a
   *     faded color to avoid drawing too much attention to it.
   */
  get fadeSecondaryText(): boolean {
    return !!this.getAttribute('fade-secondary-text');
  }

  /**
   * Setter to set the panel type.
   * @param type Enum value for the panel type.
   */
  set panelType(type: PanelType) {
    this.setAttribute('panel-type', String(type));
  }

  /**
   * Getter for the panel type.
   */
  get panelType() {
    return this.panelType_;
  }

  /**
   * Getter for the primary action button.
   */
  get primaryButton() {
    return this.shadowRoot!.querySelector<PanelButton>('#primary-action');
  }

  /**
   * Getter for the secondary action button.
   */
  get secondaryButton() {
    return this.shadowRoot!.querySelector<PanelButton>('#secondary-action');
  }

  /**
   * Getter for the panel text div.
   */
  get textDiv() {
    return this.shadowRoot!.querySelector<HTMLDivElement>('.xf-panel-text');
  }

  /**
   * Setter to replace the default aria-label on any close button.
   * @param text Text to set for the 'aria-label'.
   */
  set closeButtonAriaLabel(text: string) {
    const action =
        this.shadowRoot!.querySelector<PanelButton>('#secondary-action');
    if (action && action.dataset['category'] === 'cancel') {
      action.setAttribute('aria-label', text);
    }
  }

  set updateProgress(callback: VoidCallback) {
    this.updateProgress_ = callback;
  }

  set updateSummaryPanel(callback: VoidCallback) {
    this.updateSummaryPanel_ = callback;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [PanelItem.is]: PanelItem;
  }
}

customElements.define(PanelItem.is, PanelItem);
