// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './page_favicon.js';
import './history_clusters_shared_style.css.js';
import '../../cr_elements/cr_action_menu/cr_action_menu.js';
import '../../cr_elements/cr_icon_button/cr_icon_button.js';
import '../../cr_elements/cr_lazy_render/cr_lazy_render.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrActionMenuElement} from '../../cr_elements/cr_action_menu/cr_action_menu.js';
import {CrLazyRenderElement} from '../../cr_elements/cr_lazy_render/cr_lazy_render.js';
import {loadTimeData} from '../../js/load_time_data.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {Annotation, URLVisit} from './history_clusters.mojom-webui.js';
import {getTemplate} from './url_visit.html.js';
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
  [Annotation.kTabGrouped, 'savedInTabGroup'],
]);

declare global {
  interface HTMLElementTagNameMap {
    'url-visit': VisitRowElement;
  }
}

const ClusterMenuElementBase = I18nMixin(PolymerElement);

interface VisitRowElement {
  $: {
    actionMenu: CrLazyRenderElement<CrActionMenuElement>,
    actionMenuButton: HTMLElement,
    title: HTMLElement,
    url: HTMLElement,
  };
}

class VisitRowElement extends ClusterMenuElementBase {
  static get is() {
    return 'url-visit';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The current query for which related clusters are requested and shown.
       */
      query: String,

      /**
       * The visit to display.
       */
      visit: Object,

      /**
       * Annotations to show for the visit (e.g., whether page was bookmarked).
       */
      annotations_: {
        type: Object,
        computed: 'computeAnnotations_(visit)',
      },

      /**
       * Usually this is true, but this can be false if deleting history is
       * prohibited by Enterprise policy.
       */
      allowDeletingHistory_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('allowDeletingHistory'),
      },

      /**
       * Debug info for the visit.
       */
      debugInfo_: {
        type: String,
        computed: 'computeDebugInfo_(visit)',
      },

      /**
       * Whether the cluster is in the side panel.
       */
      inSidePanel_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('inSidePanel'),
        reflectToAttribute: true,
      },

      /**
       * Page title for the visit. This property is actually unused. The side
       * effect of the compute function is used to insert the HTML elements for
       * highlighting into this.$.title element.
       */
      unusedTitle_: {
        type: String,
        computed: 'computeTitle_(visit)',
      },

      /**
       * This property is actually unused. The side effect of the compute
       * function is used to insert HTML elements for the highlighted
       * `this.visit.urlForDisplay` URL into the `this.$.url` element.
       */
      unusedUrlForDisplay_: {
        type: String,
        computed: 'computeUrlForDisplay_(visit)',
      },
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  query: string;
  visit: URLVisit;
  private annotations_: string[];
  private allowDeletingHistory_: boolean;
  private debugInfo_: string;
  private inSidePanel_: boolean;
  private unusedTitle_: string;
  private unusedVisibleUrl_: string;

  //============================================================================
  // Event handlers
  //============================================================================

  private onAuxClick_() {
    // Notify the parent <history-cluster> element of this event.
    this.dispatchEvent(new CustomEvent('visit-clicked', {
      bubbles: true,
      composed: true,
      detail: this.visit,
    }));
  }

  private onClick_(event: MouseEvent) {
    // Ignore previously handled events.
    if (event.defaultPrevented) {
      return;
    }

    event.preventDefault();  // Prevent default browser action (navigation).

    // To record metrics.
    this.onAuxClick_();

    this.openUrl_(event);
  }

  private onContextMenu_(event: MouseEvent) {
    // Because WebUI has a Blink-provided context menu that's suitable, and
    // Side Panel always UIs always have a custom context menu.
    if (!loadTimeData.getBoolean('inSidePanel')) {
      return;
    }

    BrowserProxyImpl.getInstance().handler.showContextMenuForURL(
        this.visit.normalizedUrl, {x: event.clientX, y: event.clientY});
  }

  private onKeydown_(e: KeyboardEvent) {
    // To be consistent with <history-list>, only handle Enter, and not Space.
    if (e.key !== 'Enter') {
      return;
    }

    // To record metrics.
    this.onAuxClick_();

    this.openUrl_(e);
  }

  private onActionMenuButtonClick_(event: Event) {
    this.$.actionMenu.get().showAt(this.$.actionMenuButton);
    event.preventDefault();  // Prevent default browser action (navigation).
  }

  private onRemoveSelfButtonClick_(event: Event) {
    event.preventDefault();  // Prevent default browser action (navigation).

    this.dispatchEvent(new CustomEvent('remove-visit', {
      bubbles: true,
      composed: true,
      detail: this.visit,
    }));

    this.$.actionMenu.get().close();
  }

  //============================================================================
  // Helper methods
  //============================================================================

  private computeAnnotations_(): string[] {
    // Disabling annotations until more appropriate design for annotations in
    // the side panel is complete.
    if (this.inSidePanel_) {
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

  private computeDebugInfo_(): string {
    if (!loadTimeData.getBoolean('isHistoryClustersDebug')) {
      return '';
    }

    return JSON.stringify(this.visit.debugInfo);
  }

  private computeTitle_(_visit: URLVisit): string {
    insertHighlightedTextWithMatchesIntoElement(
        this.$.title, this.visit.pageTitle, this.visit.titleMatchPositions);
    return this.visit.pageTitle;
  }

  private computeUrlForDisplay_(_visit: URLVisit): string {
    insertHighlightedTextWithMatchesIntoElement(
        this.$.url, this.visit.urlForDisplay,
        this.visit.urlForDisplayMatchPositions);
    return this.visit.urlForDisplay;
  }

  private openUrl_(event: MouseEvent|KeyboardEvent) {
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

customElements.define(VisitRowElement.is, VisitRowElement);
