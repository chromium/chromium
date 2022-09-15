// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {getTemplate} from './xf_dlp_restriction_details_dialog.html.js';

/**
 * Dialog to show Data Leak Prevention (DLP) restriction details about a file.
 */
export class XfDlpRestrictionDetailsDialog extends HTMLElement {
  // TODO(crbug.com/1351739): Add translation strings instead of hardcoded
  // values.
  private dialog: CrDialogElement;
  private messageContainer: HTMLDivElement;
  private details: chrome.fileManagerPrivate.DlpRestrictionDetails[] = [];

  constructor() {
    super();

    // Create element content.
    const template = document.createElement('template');
    template.innerHTML = getTemplate() as unknown as string;
    const fragment = template.content.cloneNode(true);
    this.attachShadow({mode: 'open'}).appendChild(fragment);

    this.dialog = this.shadowRoot!.querySelector('#dialog')! as CrDialogElement;
    this.messageContainer =
        this.shadowRoot?.querySelector('#message')! as HTMLDivElement;
  }

  /**
   * Shows the dialog.
   * @param details DLP restriction details. Should not be empty and should
   *     contain at most one element per level (only the first one will be
   * used).
   */
  showDlpRestrictionDetailsDialog(
      details: chrome.fileManagerPrivate.DlpRestrictionDetails[]) {
    if (details.length === 0) {
      console.error(`No DLP restriction details to display.`);
      return;
    }
    this.details = details;
    // Add the general text.
    const message = 'This file is confidential and subject to restrictions ' +
        'by administrator policy. The following file access and ' +
        'transfer actions are enforced.';
    this.messageContainer.innerHTML = message;
    // Add a section for each restriction level.
    this.showDetailsForLevel(chrome.fileManagerPrivate.DlpLevel.BLOCK);
    this.showDetailsForLevel(chrome.fileManagerPrivate.DlpLevel.WARN);
    this.showDetailsForLevel(chrome.fileManagerPrivate.DlpLevel.REPORT);

    this.dialog.showModal();
  }

  /**
   * Renders restriction details for the given level.
   * @param level DLP level. Must be one of BLOCK, WARN, REPORT.
   */
  private showDetailsForLevel(level: chrome.fileManagerPrivate.DlpLevel) {
    if (level === chrome.fileManagerPrivate.DlpLevel.ALLOW) {
      console.warn('Should not be called for ALLOW.');
      return;
    }

    const details = this.details.find((d) => d.level === level);
    const detailsContainer: HTMLSpanElement =
        this.shadowRoot?.querySelector(`#${level}-details`)! as HTMLSpanElement;
    if (!details ||
        (details.urls.length === 0 && details.components.length === 0)) {
      detailsContainer.toggleAttribute('hidden', true);
      return;
    }
    detailsContainer.toggleAttribute('hidden', false);
    this.showUrlsForLevel(details.urls, level);
    this.showComponentsForLevel(details.components, level);
  }

  /**
   * If `urls` is not empty, creates a formatted string with the restricted
   * destination URLs and shows it in the label designated for `level`. If
   * `urls` is empty, hides the corresponding element in the dialog. Expects
   * that `level` is one of BLOCK, WARN, REPORT.
   *
   * If the restricted list includes a wildcard, then instead of a list "all
   * urls" is used. In that case, if there's a level with higher priority than
   * the one being rendered, "all urls except..." is used.
   *
   * @param urls List of restricted URLs.
   * @param level DLP level. Must be one of BLOCK, WARN, REPORT.
   */
  private showUrlsForLevel(
      urls: string[], level: chrome.fileManagerPrivate.DlpLevel) {
    if (level === chrome.fileManagerPrivate.DlpLevel.ALLOW) {
      console.warn('Should not be called for ALLOW.');
      return;
    }

    const urlsListItem =
        this.shadowRoot?.querySelector(`#${level}-li-urls`)! as HTMLLIElement;
    if (urls.length === 0) {
      urlsListItem.toggleAttribute('hidden', true);
      return;
    }
    urlsListItem.toggleAttribute('hidden', false);

    const urlsLabel =
        this.shadowRoot?.querySelector(`#${level}-urls`)! as HTMLLabelElement;

    const wildcardIdx = urls.indexOf('*');
    if (wildcardIdx !== -1) {
      const excludedUrls = [];
      // Append "except" all higher levels.
      const higherLevels = this.getHigherLevels(level);
      for (const level of higherLevels) {
        const exceptDetails = this.details.find((d) => d.level === level);
        if (exceptDetails && exceptDetails.urls.length > 0) {
          excludedUrls.push(exceptDetails.urls);
        }
      }
      if (excludedUrls.length === 0) {
        urlsLabel.textContent = 'File access by all urls';
      } else {
        urlsLabel.textContent =
            'File access by all urls except ' + excludedUrls.join(', ');
      }
    } else {
      urlsLabel.textContent = 'File access by ' + urls.join(', ');
    }
  }

  /**
   * @param level Level to be checked.
   * @returns List of levels, if any, with higher priority.
   */
  private getHigherLevels(level: chrome.fileManagerPrivate.DlpLevel):
      chrome.fileManagerPrivate.DlpLevel[] {
    switch (level) {
      case chrome.fileManagerPrivate.DlpLevel.BLOCK:
        return [chrome.fileManagerPrivate.DlpLevel.ALLOW];
      case chrome.fileManagerPrivate.DlpLevel.WARN:
        return [
          chrome.fileManagerPrivate.DlpLevel.ALLOW,
          chrome.fileManagerPrivate.DlpLevel.BLOCK,
        ];
      case chrome.fileManagerPrivate.DlpLevel.REPORT:
        return [
          chrome.fileManagerPrivate.DlpLevel.ALLOW,
          chrome.fileManagerPrivate.DlpLevel.BLOCK,
          chrome.fileManagerPrivate.DlpLevel.WARN,
        ];
      case chrome.fileManagerPrivate.DlpLevel.ALLOW:
      default:
        return [];
    }
  }

  /**
   * If `components` is not empty, creates a formatted string with the
   * restricted components and shows it in the label designated for `level`. If
   * `components` is empty, hides the corresponding element in the dialog.
   * Expects that `level` is one of BLOCK, WARN, REPORT.
   *
   * @param components List of restricted components.
   * @param level DLP level. Must be one of BLOCK, WARN, REPORT.
   */
  private showComponentsForLevel(
      components: string[], level: chrome.fileManagerPrivate.DlpLevel) {
    if (level === chrome.fileManagerPrivate.DlpLevel.ALLOW) {
      console.warn('Should not be called for ALLOW.');
      return;
    }

    const componentsListItem: HTMLLIElement =
        this.shadowRoot?.querySelector(`#${level}-li-components`)! as
        HTMLLIElement;
    if (components.length === 0) {
      componentsListItem.toggleAttribute('hidden', true);
      return;
    }
    componentsListItem.toggleAttribute('hidden', false);

    const componentsLabel: HTMLLabelElement =
        this.shadowRoot?.querySelector(`#${level}-components`)! as
        HTMLLabelElement;
    componentsLabel.textContent = 'File transfer to ' + components.join(', ');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'xf-dlp-restriction-details-dialog': XfDlpRestrictionDetailsDialog;
  }
}

window.customElements.define(
    'xf-dlp-restriction-details-dialog', XfDlpRestrictionDetailsDialog);
