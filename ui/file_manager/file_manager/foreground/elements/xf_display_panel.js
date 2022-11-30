// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {str, util} from '../../common/js/util.js';

import {PanelItem} from './xf_panel_item.js';

/** @type {!HTMLTemplateElement} */
const htmlTemplate = html`{__html_template__}`;

/**
 * A panel to display a collection of PanelItem.
 * @extends HTMLElement
 */
export class DisplayPanel extends HTMLElement {
  constructor() {
    super();
    this.createElement_();

    /** @private {?Element} */
    this.summary_ = this.shadowRoot.querySelector('#summary');

    /** @private {?Element} */
    this.separator_ = this.shadowRoot.querySelector('#separator');

    /** @private {?Element} */
    this.panels_ = this.shadowRoot.querySelector('#panels');

    /** @private {!function(!Event)} */
    this.listener_;

    /**
     * True if the panel is collapsed to summary view.
     * @type {boolean}
     * @private
     */
    this.collapsed_ = true;

    /**
     * Collection of PanelItems hosted in this DisplayPanel.
     * @type {!Array<PanelItem>}
     * @private
     */
    this.items_ = [];

    this.setAriaHidden_();
  }

  /**
   * Creates an instance of DisplayPanel, attaching the template clone.
   * @private
   */
  createElement_() {
    const fragment = htmlTemplate.content.cloneNode(true);
    this.attachShadow({mode: 'open'}).appendChild(fragment);
  }

  /**
   * Get the custom element template string.
   * @private
   * @return {string}
   */
  static html_() {
    return `<!--_html_template_start_-->
    <!--_html_template_end_-->`;
  }

  /**
   * Re-enable scrollbar visibility after expand/contract animation.
   * @param {!Event} event
   */
  panelExpandFinished(event) {
    this.classList.remove('expanding');
    this.classList.add('expandfinished');
    this.removeEventListener('animationend', this.listener_);
  }

  /**
   * Hides the active panel items at end of collapse animation.
   * @param {!Event} event
   */
  panelCollapseFinished(event) {
    this.hidden = true;
    this.setAttribute('aria-hidden', 'true');
    this.classList.remove('expanding');
    this.classList.add('expandfinished');
    this.removeEventListener('animationend', this.listener_);
  }

  /**
   * Set attributes and style for expanded summary panel.
   * @private
   */
  setSummaryExpandedState(expandButton) {
    expandButton.setAttribute('data-category', 'collapse');
    expandButton.setAttribute('aria-label', str('FEEDBACK_COLLAPSE_LABEL'));
    expandButton.setAttribute('aria-expanded', 'true');
    this.panels_.hidden = false;
    this.separator_.hidden = false;
  }

  /**
   * Event handler to toggle the visible state of panel items.
   * @private
   */
  toggleSummary(event) {
    const panel = event.currentTarget.parent;
    const summaryPanel = panel.summary_.querySelector('xf-panel-item');
    const expandButton =
        summaryPanel.shadowRoot.querySelector('#primary-action');
    if (panel.collapsed_) {
      panel.collapsed_ = false;
      panel.setSummaryExpandedState(expandButton);
      panel.panels_.listener_ = panel.panelExpandFinished;
      panel.panels_.addEventListener('animationend', panel.panelExpandFinished);
      panel.panels_.setAttribute('class', 'expanded expanding');
      summaryPanel.setAttribute('data-category', 'expanded');
    } else {
      panel.collapsed_ = true;
      expandButton.setAttribute('data-category', 'expand');
      expandButton.setAttribute('aria-label', str('FEEDBACK_EXPAND_LABEL'));
      expandButton.setAttribute('aria-expanded', 'false');
      panel.separator_.hidden = true;
      panel.panels_.listener_ = panel.panelCollapseFinished;
      panel.panels_.addEventListener(
          'animationend', panel.panelCollapseFinished);
      panel.panels_.setAttribute('class', 'collapsed expanding');
      summaryPanel.setAttribute('data-category', 'collapsed');
    }
  }

  /**
   * Get an array of panel items that are connected to the DOM.
   * @return {!Array<PanelItem>}
   * @private
   */
  connectedPanelItems_() {
    return this.items_.filter(item => item.isConnected);
  }

  /**
   * Update the summary panel item progress indicator.
   * @public
   */
  updateProgress() {
    let total = 0;

    if (this.items_.length == 0) {
      return;
    }
    let errors = 0;
    let progressCount = 0;
    const connectedPanels = this.connectedPanelItems_();
    for (const panel of connectedPanels) {
      // Only sum progress for attached progress panels.
      if (panel.panelType === panel.panelTypeProgress ||
          panel.panelType === panel.panelTypeFormatProgress ||
          panel.panelType === panel.panelTypeSyncProgress) {
        total += Number(panel.progress);
        progressCount++;
      } else if (panel.panelType === panel.panelTypeError) {
        errors++;
      }
    }
    if (progressCount > 0) {
      total /= progressCount;
    }
    const summaryPanel = this.summary_.querySelector('xf-panel-item');
    if (summaryPanel) {
      // Show either a progress indicator or error count if no operations going.
      if (progressCount > 0) {
        // Make sure we have a progress indicator on the summary panel.
        if (summaryPanel.indicator != 'largeprogress') {
          summaryPanel.indicator = 'largeprogress';
        }
        summaryPanel.primaryText =
            util.strf('PERCENT_COMPLETE', total.toFixed(0));
        summaryPanel.progress = total;
        summaryPanel.setAttribute('count', progressCount);
        summaryPanel.errorMarkerVisibility =
            (errors > 0) ? 'visible' : 'hidden';
      } else if (errors == 0) {
        if (summaryPanel.indicator != 'status') {
          summaryPanel.indicator = 'status';
          summaryPanel.status = 'success';
          summaryPanel.primaryText = util.strf('PERCENT_COMPLETE', 100);
        }
      } else {
        // Make sure we have a failure indicator on the summary panel.
        if (summaryPanel.indicator != 'status') {
          summaryPanel.indicator = 'status';
          summaryPanel.status = 'failure';
        }
        summaryPanel.primaryText =
            util.strf('ERROR_PROGRESS_SUMMARY_PLURAL', errors);
      }
    }
  }

  /**
   * Update the summary panel.
   * @public
   */
  updateSummaryPanel() {
    const summaryHost = this.shadowRoot.querySelector('#summary');
    let summaryPanel = summaryHost.querySelector('#summary-panel');

    // Make the display panel available by tab if there are panels to
    // show and there's an aria-label for use by a screen reader.
    if (this.hasAttribute('aria-label')) {
      this.tabIndex = this.items_.length ? 0 : -1;
    }
    // Work out how many panel items are being shown.
    const count = this.connectedPanelItems_().length;
    // If there's only one panel item active, no need for summary.
    if (count <= 1 && summaryPanel) {
      const button = summaryPanel.primaryButton;
      if (button) {
        button.removeEventListener('click', this.toggleSummary);
      }
      // For transfer summary details.
      const textDiv = summaryPanel.textDiv;
      if (textDiv) {
        textDiv.removeEventListener('click', this.toggleSummary);
      }
      summaryPanel.remove();
      this.panels_.hidden = false;
      this.separator_.hidden = true;
      this.panels_.classList.remove('collapsed');
      return;
    }
    // Show summary panel if there are more than 1 panel items.
    if (count > 1 && !summaryPanel) {
      summaryPanel = document.createElement('xf-panel-item');
      summaryPanel.setAttribute('panel-type', 1);
      summaryPanel.id = 'summary-panel';
      summaryPanel.setAttribute('detailed-summary', '');
      const button = summaryPanel.primaryButton;
      if (button) {
        button.parent = this;
        button.addEventListener('click', this.toggleSummary);
      }
      const textDiv = summaryPanel.textDiv;
      if (textDiv) {
        textDiv.parent = this;
        textDiv.addEventListener('click', this.toggleSummary);
      }
      summaryHost.appendChild(summaryPanel);
      // Setup the panels based on expand/collapse state of the summary panel.
      if (this.collapsed_) {
        this.panels_.hidden = true;
        summaryPanel.setAttribute('data-category', 'collapsed');
      } else {
        this.setSummaryExpandedState(button);
        this.panels_.classList.add('expandfinished');
        summaryPanel.setAttribute('data-category', 'expanded');
      }
    }
    if (summaryPanel) {
      this.updateProgress();
    }
  }

  /**
   * Create a panel item suitable for attaching to our display panel.
   * @param {string} id The identifier attached to this panel.
   * @return {PanelItem}
   * @public
   */
  createPanelItem(id) {
    const panel = document.createElement('xf-panel-item');
    panel.id = id;
    // Set the containing parent so the child panel can
    // trigger updates in the parent (e.g. progress summary %).
    panel.parent = this;
    panel.setAttribute('indicator', 'progress');
    this.items_.push(/** @type {!PanelItem} */ (panel));
    this.setAriaHidden_();
    this.setAttribute('detailed-panel', 'detailed-panel');
    return /** @type {!PanelItem} */ (panel);
  }

  /**
   * Attach a panel item element inside our display panel.
   * @param {PanelItem} panel The panel item to attach.
   * @public
   */
  attachPanelItem(panel) {
    const displayPanel = panel.parent;

    // Only attach the panel if it hasn't been removed.
    const index = displayPanel.items_.indexOf(panel);
    if (index === -1) {
      return;
    }

    // If it's already attached, nothing to do here.
    if (panel.isConnected) {
      return;
    }

    displayPanel.panels_.appendChild(panel);
    displayPanel.updateSummaryPanel();
    this.setAriaHidden_();
  }

  /**
   * Add a panel entry element inside our display panel.
   * @param {string} id The identifier attached to this panel.
   * @return {PanelItem}
   * @public
   */
  addPanelItem(id) {
    const panel = this.createPanelItem(id);
    this.attachPanelItem(panel);
    return /** @type {!PanelItem} */ (panel);
  }

  /**
   * Remove a panel from this display panel.
   * @param {PanelItem} item The PanelItem to remove.
   * @public
   */
  removePanelItem(item) {
    const index = this.items_.indexOf(item);
    if (index === -1) {
      return;
    }
    item.remove();
    this.items_.splice(index, 1);
    this.setAriaHidden_();
    this.updateSummaryPanel();
  }

  /**
   * Set aria-hidden to false if there is no panel.
   * @private
   */
  setAriaHidden_() {
    const hasItems = this.connectedPanelItems_().length > 0;
    this.setAttribute('aria-hidden', !hasItems);
  }

  /**
   * Find a panel with given 'id'.
   * @public
   */
  findPanelItemById(id) {
    for (const item of this.items_) {
      if (item.getAttribute('id') === id) {
        return item;
      }
    }
    return null;
  }

  /**
   * Remove all panel items.
   * @public
   */
  removeAllPanelItems() {
    for (const item of this.items_) {
      item.remove();
    }
    this.items_ = [];
    this.setAriaHidden_();
    this.updateSummaryPanel();
  }
}

window.customElements.define('xf-display-panel', DisplayPanel);

//# sourceURL=//ui/file_manager/file_manager/foreground/elements/xf_display_panel.js
