// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './page_favicon.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {URLVisit} from './history_cluster_types.mojom-webui.js';
import {Annotation} from './history_cluster_types.mojom-webui.js';
import {getCss} from './url_visit.css.js';
import {getHtml} from './url_visit.html.js';
import {insertHighlightedTextWithMatchesIntoElement} from './utils.js';

/**
 * @fileoverview This file provides a custom element displaying a visit to a
 * page within a cluster. A visit features the page favicon, title, a timestamp,
 * as well as an action menu.
 */

/**
 * Maps supported annotations to localized string identifiers.
 */
const annotationToStringId: Map<number, string> = new Map([
  [Annotation.kBookmarked, 'bookmarked'],
]);

declare global {
  interface HTMLElementTagNameMap {
    'url-visit': UrlVisitElement;
  }
}

const ClusterMenuElementBase = I18nMixinLit(CrLitElement);

export interface UrlVisitElement {
  $: {
    actionMenuButton: HTMLElement,
    title: HTMLElement,
    url: HTMLElement,
  };
}

export class UrlVisitElement extends ClusterMenuElementBase {
  static get is() {
    return 'url-visit';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * The current query for which related clusters are requested and shown.
       */
      query: {type: String},

      /**
       * The visit to display.
       */
      visit: {type: Object},

      /**
       * Whether this visit is within a persisted cluster.
       */
      fromPersistence: {type: Boolean},

      /**
       * Usually this is true, but this can be false if deleting history is
       * prohibited by Enterprise policy.
       */
      allowDeletingHistory_: {type: Boolean},

      /**
       * Whether the cluster is in the side panel.
       */
      inSidePanel_: {
        type: Boolean,
        reflect: true,
      },

      renderActionMenu_: {type: Boolean},
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  query: string = '';
  visit?: URLVisit;
  fromPersistence: boolean = false;
  protected annotations_: string[] = [];
  protected allowDeletingHistory_: boolean =
      loadTimeData.getBoolean('allowDeletingHistory');
  private inSidePanel_: boolean = loadTimeData.getBoolean('inSidePanel');
  protected renderActionMenu_: boolean = false;

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('visit')) {
      assert(this.visit);
      insertHighlightedTextWithMatchesIntoElement(
          this.$.title, this.visit.pageTitle, this.visit.titleMatchPositions);
      insertHighlightedTextWithMatchesIntoElement(
          this.$.url, this.visit.urlForDisplay,
          this.visit.urlForDisplayMatchPositions);
    }
  }

  //============================================================================
  // Event handlers
  //============================================================================

  private onAuxClick_() {
    // Notify the parent <history-cluster> element of this event.
    this.fire('visit-clicked', this.visit);
  }

  protected onClick_(event: MouseEvent) {
    // Ignore previously handled events.
    if (event.defaultPrevented) {
      return;
    }

    event.preventDefault();  // Prevent default browser action (navigation).

    // To record metrics.
    this.onAuxClick_();

    this.openUrl_(event);
  }

  protected onContextMenu_(event: MouseEvent) {
    // Because WebUI has a Blink-provided context menu that's suitable, and
    // Side Panel always UIs always have a custom context menu.
    if (!loadTimeData.getBoolean('inSidePanel') || !this.visit) {
      return;
    }

    BrowserProxyImpl.getInstance().handler.showContextMenuForURL(
        this.visit.normalizedUrl, {x: event.clientX, y: event.clientY});
  }

  protected onKeydown_(e: KeyboardEvent) {
    // To be consistent with <history-list>, only handle Enter, and not Space.
    if (e.key !== 'Enter') {
      return;
    }

    // To record metrics.
    this.onAuxClick_();

    this.openUrl_(e);
  }

  protected async onActionMenuButtonClick_(event: Event) {
    event.preventDefault();  // Prevent default browser action (navigation).

    if (!this.renderActionMenu_) {
      this.renderActionMenu_ = true;
      await this.updateComplete;
    }
    const menu = this.shadowRoot!.querySelector('cr-action-menu');
    assert(menu);
    menu.showAt(this.$.actionMenuButton);
  }

  protected onHideSelfButtonClick_(event: Event) {
    this.emitMenuButtonClick_(event, 'hide-visit');
  }

  protected onRemoveSelfButtonClick_(event: Event) {
    this.emitMenuButtonClick_(event, 'remove-visit');
  }

  private emitMenuButtonClick_(event: Event, emitEventName: string) {
    event.preventDefault();  // Prevent default browser action (navigation).

    this.fire(emitEventName, this.visit);

    // This can also be triggered from the hide visit icon, in which case the
    // menu may not be rendered.
    if (this.renderActionMenu_) {
      const menu = this.shadowRoot!.querySelector('cr-action-menu');
      assert(menu);
      menu.close();
    }
  }

  //============================================================================
  // Helper methods
  //============================================================================

  protected computeAnnotations_(): string[] {
    // Disabling annotations until more appropriate design for annotations in
    // the side panel is complete.
    if (this.inSidePanel_ || !this.visit) {
      return [];
    }
    return this.visit.annotations
        .map((annotation: number) => annotationToStringId.get(annotation))
        .filter(
            (id: string|undefined):
                id is string => {
                  return !!id;
                })
        .map((id: string) => loadTimeData.getString(id));
  }

  protected computeDebugInfo_(): string {
    if (!loadTimeData.getBoolean('isHistoryClustersDebug') || !this.visit) {
      return '';
    }

    return JSON.stringify(this.visit.debugInfo);
  }

  private openUrl_(event: MouseEvent|KeyboardEvent) {
    assert(this.visit);
    BrowserProxyImpl.getInstance().handler.openHistoryCluster(
        this.visit.normalizedUrl, {
          middleButton: (event as MouseEvent).button === 1,
          altKey: event.altKey,
          ctrlKey: event.ctrlKey,
          metaKey: event.metaKey,
          shiftKey: event.shiftKey,
        });
  }
}

customElements.define(UrlVisitElement.is, UrlVisitElement);
