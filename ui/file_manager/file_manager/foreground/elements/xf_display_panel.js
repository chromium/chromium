// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {str, strf} from '../../common/js/translations.js';

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

    /** @private @type {?Element} */
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.summary_ = this.shadowRoot.querySelector('#summary');

    /** @private @type {?Element} */
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.separator_ = this.shadowRoot.querySelector('#separator');

    /** @private @type {?Element} */
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.panels_ = this.shadowRoot.querySelector('#panels');

    // @ts-ignore: error TS7014: Function type, which lacks return-type
    // annotation, implicitly has an 'any' return type.
    /** @private @type {!function(!Event):void} */
    // @ts-ignore: error TS2339: Property 'listener_' does not exist on type
    // 'DisplayPanel'.
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
   * We cannot set attributes in the constructor for custom elements when using
   * `createElement()`. Set attributes in the connected callback instead.
   * @private
   */
  // @ts-ignore: error TS6133: 'connectedCallback' is declared but its value is
  // never read.
  connectedCallback() {
    this.setAriaHidden_();
  }

  /**
   * Get the custom element template string.
   * @private
   * @return {string}
   */
  // @ts-ignore: error TS6133: 'html_' is declared but its value is never read.
  static html_() {
    return `<!--_html_template_start_-->
    <!--_html_template_end_-->`;
  }

  /**
   * Re-enable scrollbar visibility after expand/contract animation.
   * @param {!Event} event
   */
  // @ts-ignore: error TS6133: 'event' is declared but its value is never read.
  panelExpandFinished(event) {
    this.classList.remove('expanding');
    this.classList.add('expandfinished');
    // @ts-ignore: error TS2339: Property 'listener_' does not exist on type
    // 'DisplayPanel'.
    this.removeEventListener('animationend', this.listener_);
  }

  /**
   * Hides the active panel items at end of collapse animation.
   * @param {!Event} event
   */
  // @ts-ignore: error TS6133: 'event' is declared but its value is never read.
  panelCollapseFinished(event) {
    this.hidden = true;
    this.setAttribute('aria-hidden', 'true');
    this.classList.remove('expanding');
    this.classList.add('expandfinished');
    // @ts-ignore: error TS2339: Property 'listener_' does not exist on type
    // 'DisplayPanel'.
    this.removeEventListener('animationend', this.listener_);
  }

  /**
   * Set attributes and style for expanded summary panel.
   * @private
   */
  // @ts-ignore: error TS7006: Parameter 'expandButton' implicitly has an 'any'
  // type.
  setSummaryExpandedState(expandButton) {
    expandButton.setAttribute('data-category', 'collapse');
    expandButton.setAttribute('aria-label', str('FEEDBACK_COLLAPSE_LABEL'));
    expandButton.setAttribute('aria-expanded', 'true');
    // @ts-ignore: error TS2339: Property 'hidden' does not exist on type
    // 'Element'.
    this.panels_.hidden = false;
    // @ts-ignore: error TS2339: Property 'hidden' does not exist on type
    // 'Element'.
    this.separator_.hidden = false;
  }

  /**
   * Event handler to toggle the visible state of panel items.
   * @private
   */
  // @ts-ignore: error TS7006: Parameter 'event' implicitly has an 'any' type.
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
    let warnings = 0;
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
      } else if (panel.panelType === panel.panelTypeInfo) {
        warnings++;
      }
    }
    if (progressCount > 0) {
      total /= progressCount;
    }
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    const summaryPanel = this.summary_.querySelector('xf-panel-item');
    if (!summaryPanel) {
      return;
    }
    // Show either a progress indicator or a status indicator (success, warning,
    // error) if no operations are ongoing.
    if (progressCount > 0) {
      // Make sure we have a progress indicator on the summary panel.
      // @ts-ignore: error TS2339: Property 'indicator' does not exist on type
      // 'Element'.
      if (summaryPanel.indicator != 'largeprogress') {
        // @ts-ignore: error TS2339: Property 'indicator' does not exist on type
        // 'Element'.
        summaryPanel.indicator = 'largeprogress';
      }
      // @ts-ignore: error TS2339: Property 'primaryText' does not exist on type
      // 'Element'.
      summaryPanel.primaryText = strf('PERCENT_COMPLETE', total.toFixed(0));
      // @ts-ignore: error TS2339: Property 'progress' does not exist on type
      // 'Element'.
      summaryPanel.progress = total;
      // @ts-ignore: error TS2345: Argument of type 'number' is not assignable
      // to parameter of type 'string'.
      summaryPanel.setAttribute('count', progressCount);
      // @ts-ignore: error TS2339: Property 'errorMarkerVisibility' does not
      // exist on type 'Element'.
      summaryPanel.errorMarkerVisibility = (errors > 0) ? 'visible' : 'hidden';
      return;
    }

    // @ts-ignore: error TS2339: Property 'indicator' does not exist on type
    // 'Element'.
    if (summaryPanel.indicator != 'status') {
      // Make sure we have a status indicator on the summary panel.
      // @ts-ignore: error TS2339: Property 'indicator' does not exist on type
      // 'Element'.
      summaryPanel.indicator = 'status';
    }

    if (errors > 0 && warnings > 0) {
      // Both errors and warnings: show the error indicator, along with counts
      // of both.
      // @ts-ignore: error TS2339: Property 'status' does not exist on type
      // 'Element'.
      summaryPanel.status = 'failure';
      // @ts-ignore: error TS2339: Property 'primaryText' does not exist on type
      // 'Element'.
      summaryPanel.primaryText = strf('ERROR_PROGRESS_SUMMARY_PLURAL', errors) +
          ' ' + this.generateWarningMessage_(warnings);
      return;
    }

    if (errors > 0) {
      // Only errors, but no warnings.
      // @ts-ignore: error TS2339: Property 'status' does not exist on type
      // 'Element'.
      summaryPanel.status = 'failure';
      // @ts-ignore: error TS2339: Property 'primaryText' does not exist on type
      // 'Element'.
      summaryPanel.primaryText = strf('ERROR_PROGRESS_SUMMARY_PLURAL', errors);
      if (warnings > 0) {
        // @ts-ignore: error TS2339: Property 'primaryText' does not exist on
        // type 'Element'.
        summaryPanel.primaryText +=
            ' ' + this.generateWarningMessage_(warnings);
      }
      return;
    }

    if (warnings > 0) {
      // Only warnings, but no errors.
      // @ts-ignore: error TS2339: Property 'status' does not exist on type
      // 'Element'.
      summaryPanel.status = 'warning';
      // @ts-ignore: error TS2339: Property 'primaryText' does not exist on type
      // 'Element'.
      summaryPanel.primaryText = this.generateWarningMessage_(warnings);
      return;
    }

    // No errors or warnings.
    // @ts-ignore: error TS2339: Property 'status' does not exist on type
    // 'Element'.
    summaryPanel.status = 'success';
    // @ts-ignore: error TS2339: Property 'primaryText' does not exist on type
    // 'Element'.
    summaryPanel.primaryText = strf('PERCENT_COMPLETE', 100);
  }

  /**
   * Update the summary panel.
   * @public
   */
  updateSummaryPanel() {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    const summaryHost = this.shadowRoot.querySelector('#summary');
    // @ts-ignore: error TS18047: 'summaryHost' is possibly 'null'.
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
      // @ts-ignore: error TS2339: Property 'primaryButton' does not exist on
      // type 'Element'.
      const button = summaryPanel.primaryButton;
      if (button) {
        button.removeEventListener('click', this.toggleSummary);
      }
      // For transfer summary details.
      // @ts-ignore: error TS2339: Property 'textDiv' does not exist on type
      // 'Element'.
      const textDiv = summaryPanel.textDiv;
      if (textDiv) {
        textDiv.removeEventListener('click', this.toggleSummary);
      }
      summaryPanel.remove();
      // @ts-ignore: error TS2339: Property 'hidden' does not exist on type
      // 'Element'.
      this.panels_.hidden = false;
      // @ts-ignore: error TS2339: Property 'hidden' does not exist on type
      // 'Element'.
      this.separator_.hidden = true;
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.panels_.classList.remove('collapsed');
      return;
    }
    // Show summary panel if there are more than 1 panel items.
    if (count > 1 && !summaryPanel) {
      summaryPanel = document.createElement('xf-panel-item');
      // @ts-ignore: error TS2345: Argument of type 'number' is not assignable
      // to parameter of type 'string'.
      summaryPanel.setAttribute('panel-type', 1);
      summaryPanel.id = 'summary-panel';
      summaryPanel.setAttribute('detailed-summary', '');
      // @ts-ignore: error TS2339: Property 'primaryButton' does not exist on
      // type 'Element'.
      const button = summaryPanel.primaryButton;
      if (button) {
        button.parent = this;
        button.addEventListener('click', this.toggleSummary);
      }
      // @ts-ignore: error TS2339: Property 'textDiv' does not exist on type
      // 'Element'.
      const textDiv = summaryPanel.textDiv;
      if (textDiv) {
        textDiv.parent = this;
        textDiv.addEventListener('click', this.toggleSummary);
      }
      // @ts-ignore: error TS18047: 'summaryHost' is possibly 'null'.
      summaryHost.appendChild(summaryPanel);
      // Setup the panels based on expand/collapse state of the summary panel.
      if (this.collapsed_) {
        // @ts-ignore: error TS2339: Property 'hidden' does not exist on type
        // 'Element'.
        this.panels_.hidden = true;
        summaryPanel.setAttribute('data-category', 'collapsed');
      } else {
        this.setSummaryExpandedState(button);
        // @ts-ignore: error TS2531: Object is possibly 'null'.
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
    // @ts-ignore: error TS2551: Property 'parent' does not exist on type
    // 'HTMLElement'. Did you mean 'part'?
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
    // @ts-ignore: error TS18047: 'displayPanel' is possibly 'null'.
    const index = displayPanel.items_.indexOf(panel);
    if (index === -1) {
      return;
    }

    // If it's already attached, nothing to do here.
    if (panel.isConnected) {
      return;
    }

    // @ts-ignore: error TS18047: 'displayPanel.panels_' is possibly 'null'.
    displayPanel.panels_.appendChild(panel);
    // @ts-ignore: error TS18047: 'displayPanel' is possibly 'null'.
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
    // @ts-ignore: error TS2345: Argument of type 'boolean' is not assignable to
    // parameter of type 'string'.
    this.setAttribute('aria-hidden', !hasItems);
  }

  /**
   * Find a panel with given 'id'.
   * @public
   */
  // @ts-ignore: error TS7006: Parameter 'id' implicitly has an 'any' type.
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

  /**
   * Generates the summary panel title message based on the number of warnings.
   * @param {number} warnings Number of warning subpanels.
   * @returns {string} Title text.
   * @private
   */
  generateWarningMessage_(warnings) {
    if (warnings <= 0) {
      console.warn(`generateWarningMessage_ expected warnings > 0, but got ${
          warnings}.`);
      return '';
    }
    return warnings === 1 ? str('WARNING_PROGRESS_SUMMARY_SINGLE') :
                            strf('WARNING_PROGRESS_SUMMARY_PLURAL', warnings);
  }
}

window.customElements.define('xf-display-panel', DisplayPanel);

//# sourceURL=//ui/file_manager/file_manager/foreground/elements/xf_display_panel.js
