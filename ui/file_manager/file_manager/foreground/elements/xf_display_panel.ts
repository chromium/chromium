// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getPluralString, str, strf} from '../../common/js/translations.js';

import type {PanelButton} from './xf_button.js';
import {getTemplate} from './xf_display_panel.html.js';
import type {PanelItem} from './xf_panel_item.js';
import {PanelType} from './xf_panel_item.js';

/**
 * A panel to display a collection of PanelItem.
 */
export class DisplayPanel extends HTMLElement {
  static get is() {
    return 'xf-display-panel' as const;
  }

  /**
   * True if the panel is collapsed to summary view.
   */
  private collapsed_: boolean = true;

  /**
   * Collection of PanelItems hosted in this DisplayPanel.
   */
  private items_: PanelItem[] = [];
  private summary_: HTMLDivElement;
  private separator_: HTMLDivElement;
  private panels_: HTMLDivElement;

  private toggleSummaryBound_ = this.toggleSummary_.bind(this);

  constructor() {
    super();
    this.createElement_();
    this.summary_ = this.shadowRoot!.querySelector<HTMLDivElement>('#summary')!;
    this.separator_ =
        this.shadowRoot!.querySelector<HTMLDivElement>('#separator')!;
    this.panels_ = this.shadowRoot!.querySelector<HTMLDivElement>('#panels')!;
  }

  /**
   * Creates an instance of DisplayPanel, attaching the template clone.
   */
  private createElement_() {
    const template = document.createElement('template');
    template.innerHTML = getTemplate() as unknown as string;
    const fragment = template.content.cloneNode(true);
    this.attachShadow({mode: 'open'}).appendChild(fragment);
  }

  /**
   * We cannot set attributes in the constructor for custom elements when using
   * `createElement()`. Set attributes in the connected callback instead.
   */
  connectedCallback() {
    this.setAriaHidden_();
  }

  /**
   * Re-enable scrollbar visibility after expand/contract animation.
   */
  private panelExpandFinished_(_: Event) {
    this.panels_.classList.remove('expanding');
    this.panels_.classList.add('expandfinished');
  }

  /**
   * Hides the active panel items at end of collapse animation.
   */
  private panelCollapseFinished_(_: Event) {
    this.panels_.hidden = true;
    this.panels_.setAttribute('aria-hidden', 'true');
    this.panels_.classList.remove('expanding');
    this.panels_.classList.add('expandfinished');
  }

  /**
   * Set attributes and style for expanded summary panel.
   */
  setSummaryExpandedState(expandButton: PanelButton) {
    expandButton.setAttribute('data-category', 'collapse');
    expandButton.setAttribute('aria-label', str('FEEDBACK_COLLAPSE_LABEL'));
    expandButton.setAttribute('aria-expanded', 'true');
    this.panels_.hidden = false;
    this.separator_.hidden = false;
  }

  /**
   * Event handler to toggle the visible state of panel items.
   */
  private toggleSummary_(_: Event) {
    const summaryPanel =
        this.summary_.querySelector<PanelItem>('xf-panel-item')!;
    const expandButton =
        summaryPanel.shadowRoot!.querySelector<PanelButton>('#primary-action')!;
    if (this.collapsed_) {
      this.collapsed_ = false;
      this.setSummaryExpandedState(expandButton);
      this.panels_.addEventListener(
          'animationend', this.panelExpandFinished_.bind(this), {once: true});
      this.panels_.setAttribute('class', 'expanded expanding');
      summaryPanel.setAttribute('data-category', 'expanded');
    } else {
      this.collapsed_ = true;
      expandButton.setAttribute('data-category', 'expand');
      expandButton.setAttribute('aria-label', str('FEEDBACK_EXPAND_LABEL'));
      expandButton.setAttribute('aria-expanded', 'false');
      this.separator_.hidden = true;
      this.panels_.addEventListener(
          'animationend', this.panelCollapseFinished_.bind(this), {once: true});
      this.panels_.setAttribute('class', 'collapsed expanding');
      summaryPanel.setAttribute('data-category', 'collapsed');
    }
  }

  /**
   * Get an array of panel items that are connected to the DOM.
   */
  private connectedPanelItems_(): PanelItem[] {
    return this.items_.filter(item => item.isConnected);
  }

  /**
   * Update the summary panel item progress indicator.
   */
  async updateProgress() {
    let total = 0;

    if (this.items_.length === 0) {
      return;
    }
    let errors = 0;
    let warnings = 0;
    let progressCount = 0;
    const connectedPanels = this.connectedPanelItems_();
    for (const panel of connectedPanels) {
      // Only sum progress for attached progress panels.
      if (panel.panelType === PanelType.PROGRESS ||
          panel.panelType === PanelType.FORMAT_PROGRESS ||
          panel.panelType === PanelType.SYNC_PROGRESS) {
        total += Number(panel.progress);
        progressCount++;
      } else if (panel.panelType === PanelType.ERROR) {
        errors++;
      } else if (panel.panelType === PanelType.INFO) {
        warnings++;
      }
    }
    if (progressCount > 0) {
      total /= progressCount;
    }
    const summaryPanel =
        this.summary_.querySelector<PanelItem>('xf-panel-item');
    if (!summaryPanel) {
      return;
    }
    // Show either a progress indicator or a status indicator (success, warning,
    // error) if no operations are ongoing.
    if (progressCount > 0) {
      // Make sure we have a progress indicator on the summary panel.
      if (summaryPanel.indicator !== 'largeprogress') {
        summaryPanel.indicator = 'largeprogress';
      }
      summaryPanel.primaryText = strf('PERCENT_COMPLETE', total.toFixed(0));
      summaryPanel.progress = String(total);
      summaryPanel.setAttribute('count', String(progressCount));
      summaryPanel.errorMarkerVisibility = (errors > 0) ? 'visible' : 'hidden';
      return;
    }

    if (summaryPanel.indicator !== 'status') {
      // Make sure we have a status indicator on the summary panel.
      summaryPanel.indicator = 'status';
    }

    if (errors > 0 && warnings > 0) {
      // Both errors and warnings: show the error indicator, along with counts
      // of both.
      summaryPanel.status = 'failure';
      const errorMessage = await this.generateErrorMessage_(errors);
      const warningMessage = await this.generateWarningMessage_(errors);
      summaryPanel.primaryText = `${errorMessage} ${warningMessage}`;
      return;
    }

    if (errors > 0) {
      // Only errors, but no warnings.
      summaryPanel.status = 'failure';
      summaryPanel.primaryText = await this.generateErrorMessage_(errors);
      return;
    }

    if (warnings > 0) {
      // Only warnings, but no errors.
      summaryPanel.status = 'warning';
      summaryPanel.primaryText = await this.generateWarningMessage_(warnings);
      return;
    }

    // No errors or warnings.
    summaryPanel.status = 'success';
    summaryPanel.primaryText = strf('PERCENT_COMPLETE', 100);
  }

  /**
   * Update the summary panel.
   * @public
   */
  updateSummaryPanel() {
    const summaryHost = this.shadowRoot!.querySelector('#summary')!;
    let summaryPanel = summaryHost.querySelector<PanelItem>('#summary-panel');

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
        button.removeEventListener('click', this.toggleSummaryBound_);
      }
      // For transfer summary details.
      const textDiv = summaryPanel.textDiv;
      if (textDiv) {
        textDiv.removeEventListener('click', this.toggleSummaryBound_);
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
      summaryPanel.panelType = PanelType.SUMMARY;
      summaryPanel.id = 'summary-panel';
      summaryPanel.setAttribute('detailed-summary', '');
      const button = summaryPanel.primaryButton!;
      if (button) {
        button.addEventListener('click', this.toggleSummaryBound_);
      }
      const textDiv = summaryPanel.textDiv;
      if (textDiv) {
        textDiv.addEventListener('click', this.toggleSummaryBound_);
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
   * @param id The identifier attached to this panel.
   */
  createPanelItem(id: string): PanelItem {
    const panel = document.createElement('xf-panel-item');
    panel.id = id;
    panel.updateProgress = this.updateProgress.bind(this);
    panel.updateSummaryPanel = this.updateSummaryPanel.bind(this);
    panel.setAttribute('indicator', 'progress');
    this.items_.push(panel);
    this.setAriaHidden_();
    this.setAttribute('detailed-panel', 'detailed-panel');
    return panel;
  }

  /**
   * Attach a panel item element inside our display panel.
   * @param panel The panel item to attach.
   */
  attachPanelItem(panel: PanelItem) {
    // Only attach the panel if it hasn't been removed.
    const index = this.items_.indexOf(panel);
    if (index === -1) {
      return;
    }

    // If it's already attached, nothing to do here.
    if (panel.isConnected) {
      return;
    }

    this.panels_.appendChild(panel);
    this.updateSummaryPanel();
    this.setAriaHidden_();
  }

  /**
   * Add a panel entry element inside our display panel.
   * @param id The identifier attached to this panel.
   */
  addPanelItem(id: string): PanelItem {
    const panel = this.createPanelItem(id);
    this.attachPanelItem(panel);
    return panel;
  }

  /**
   * Remove a panel from this display panel.
   * @param item The PanelItem to remove.
   * @public
   */
  removePanelItem(item: PanelItem) {
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
   */
  private setAriaHidden_() {
    const hasItems = this.connectedPanelItems_().length > 0;
    this.setAttribute('aria-hidden', String(!hasItems));
  }

  /**
   * Find a panel with given 'id'.
   */
  findPanelItemById(id: string): PanelItem|null {
    for (const item of this.items_) {
      if (item.getAttribute('id') === id) {
        return item;
      }
    }
    return null;
  }

  /**
   * Remove all panel items.
   */
  removeAllPanelItems(): void {
    for (const item of this.items_) {
      item.remove();
    }
    this.items_ = [];
    this.setAriaHidden_();
    this.updateSummaryPanel();
  }

  /**
   * Generates the summary panel title message based on the number of errors.
   * @param errors Number of error subpanels.
   * @return Title text.
   */
  private async generateErrorMessage_(errors: number): Promise<string> {
    if (errors <= 0) {
      console.warn(
          `generateWarningMessage_ expected errors > 0, but got ${errors}.`);
      return '';
    }
    return getPluralString('ERROR_PROGRESS_SUMMARY', errors);
  }

  /**
   * Generates the summary panel title message based on the number of warnings.
   * @param warnings Number of warning subpanels.
   * @return Title text.
   */
  private async generateWarningMessage_(warnings: number): Promise<string> {
    if (warnings <= 0) {
      console.warn(`generateWarningMessage_ expected warnings > 0, but got ${
          warnings}.`);
      return '';
    }
    return getPluralString('WARNING_PROGRESS_SUMMARY', warnings);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [DisplayPanel.is]: DisplayPanel;
  }
}

window.customElements.define(DisplayPanel.is, DisplayPanel);
