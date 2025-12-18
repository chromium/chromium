// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_toast/cr_toast_manager.js';
import '//resources/cr_elements/policy/cr_policy_indicator.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrToastManagerElement} from '//resources/cr_elements/cr_toast/cr_toast_manager.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {skColorToRgba} from '//resources/js/color_utils.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {FocusOutlineManager} from '//resources/js/focus_outline_manager.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {isMac} from '//resources/js/platform.js';
import {hasKeyModifiers} from '//resources/js/util.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {TileSource} from '//resources/mojo/components/ntp_tiles/tile_source.mojom-webui.js';
import {TextDirection} from '//resources/mojo/mojo/public/mojom/base/text_direction.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {MostVisitedBrowserProxy} from './browser_proxy.js';
import {getCss} from './most_visited.css.js';
import {getHtml} from './most_visited.html.js';
import type {MostVisitedInfo, MostVisitedPageCallbackRouter, MostVisitedPageHandlerRemote, MostVisitedTheme, MostVisitedTile} from './most_visited.mojom-webui.js';
import {MostVisitedWindowProxy} from './window_proxy.js';

export const MAX_TILES_DEFAULT = 8;
export const MAX_TILES_FOR_CUSTOM_LINKS = 10;
const MAX_TILES_FOR_ENTERPRISE_SHORTCUTS = 10;

function resetTilePosition(tile: HTMLElement) {
  tile.style.position = '';
  tile.style.left = '';
  tile.style.top = '';
}

function setTilePosition(tile: HTMLElement, {x, y}: {x: number, y: number}) {
  tile.style.position = 'fixed';
  tile.style.left = `${x}px`;
  tile.style.top = `${y}px`;
}

function getHitIndex(rects: DOMRect[], x: number, y: number): number {
  return rects.findIndex(
      r => x >= r.left && x <= r.right && y >= r.top && y <= r.bottom);
}

/**
 * Returns null if URL is not valid.
 */
function normalizeUrl(urlString: string): URL|null {
  try {
    const url = new URL(
        urlString.includes('://') ? urlString : `https://${urlString}/`);
    if (['http:', 'https:'].includes(url.protocol)) {
      return url;
    }
  } catch (e) {
  }
  return null;
}

const MostVisitedElementBase = I18nMixinLit(CrLitElement);

export interface MostVisitedElement {
  $: {
    actionMenu: CrActionMenuElement,
    container: HTMLElement,
    dialog: CrDialogElement,
    toastManager: CrToastManagerElement,
    addShortcut: HTMLElement,
    showMore: HTMLElement,
    showLess: HTMLElement,
  };
}

export class MostVisitedElement extends MostVisitedElementBase {
  static get is() {
    return 'cr-most-visited';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      theme: {type: Object},
      /**
       * If true, renders MV tiles in a single row up to 10 columns wide.
       * If false, renders MV tiles in up to 2 rows up to 5 columns wide.
       */
      singleRow: {type: Boolean},
      /** If true, reflows tiles that are overflowing. */
      reflowOnOverflow: {type: Boolean},

      /**
       * When the tile icon background is dark, the icon color is white for
       * contrast. This can be used to determine the color of the tile hover as
       * well.
       */
      useWhiteTileIcon_: {
        type: Boolean,
        reflect: true,
      },

      columnCount_: {type: Number, state: true},
      rowCount_: {type: Number, state: true},

      customLinksEnabled_: {
        type: Boolean,
        reflect: true,
      },

      enterpriseShortcutsEnabled_: {
        type: Boolean,
        reflect: true,
      },

      dialogTileTitle_: {type: String, state: true},
      dialogTileUrl_: {type: String, state: true},
      dialogTileUrlInvalid_: {type: Boolean, state: true},
      dialogTitle_: {type: String, state: true},
      dialogSaveDisabled_: {type: Boolean, state: true},
      dialogShortcutAlreadyExists_: {type: Boolean, state: true},
      dialogTileUrlError_: {type: String, state: true},
      dialogIsReadonly_: {type: Boolean, state: true},
      dialogSource_: {type: Number, state: true},
      info_: {type: Object, state: true},

      actionMenuRemoveDisabled_: {type: Boolean, state: true},
      actionMenuViewOrEditTitle_: {type: String, state: true},

      isDark_: {
        type: Boolean,
        reflect: true,
      },

      /**
       * Used to hide hover style and cr-icon-button of tiles while the tiles
       * are being reordered.
       */
      reordering_: {
        type: Boolean,
        reflect: true,
      },

      maxTiles_: {type: Number, state: true},
      maxVisibleTiles_: {type: Number, state: true},
      showAdd_: {type: Boolean, state: true},
      showToastButtons_: {type: Boolean, state: true},
      maxVisibleColumnCount_: {type: Number, state: true},
      tiles_: {type: Array, state: true},
      toastContent_: {type: String, state: true},
      toastSource_: {type: Number, state: true},
      autoRemovalInProgress_: {type: Boolean, state: true},

      expandableTilesEnabled: {type: Boolean, reflect: true},
      maxTilesBeforeShowMore: {type: Number, reflect: true},
      showAll_: {type: Boolean, state: true},
      showShowMore_: {type: Boolean, state: true},
      showShowLess_: {type: Boolean, state: true},

      visible_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor theme: MostVisitedTheme|null = null;
  accessor reflowOnOverflow: boolean = false;
  accessor singleRow: boolean = false;
  accessor expandableTilesEnabled: boolean = false;
  accessor maxTilesBeforeShowMore: number = 0;
  private accessor showAll_: boolean = false;
  protected accessor showShowMore_: boolean = false;
  protected accessor showShowLess_: boolean = false;
  protected accessor useWhiteTileIcon_: boolean = false;
  protected accessor columnCount_: number = 3;
  protected accessor rowCount_: number = 1;
  protected accessor customLinksEnabled_: boolean = false;
  protected accessor enterpriseShortcutsEnabled_: boolean = false;
  protected accessor dialogTileTitle_: string = '';
  protected accessor dialogTileUrl_: string = '';
  protected accessor dialogTileUrlInvalid_: boolean = false;
  protected accessor dialogTitle_: string = '';
  protected accessor dialogSaveDisabled_: boolean = true;
  private accessor dialogShortcutAlreadyExists_: boolean = false;
  protected accessor dialogTileUrlError_: string = '';
  protected accessor dialogIsReadonly_: boolean = false;
  protected accessor dialogSource_: TileSource = TileSource.CUSTOM_LINKS;
  protected accessor actionMenuRemoveDisabled_: boolean = false;
  protected accessor actionMenuViewOrEditTitle_: string = '';
  protected accessor isDark_: boolean = false;
  private accessor reordering_: boolean = false;
  private accessor maxTiles_: number = 0;
  private accessor maxVisibleTiles_: number = 0;
  protected accessor showAdd_: boolean = false;
  private accessor maxVisibleColumnCount_: number = 0;
  protected accessor tiles_: MostVisitedTile[] = [];
  protected accessor toastSource_: TileSource = TileSource.CUSTOM_LINKS;
  protected accessor autoRemovalInProgress_: boolean = false;
  protected accessor visible_: boolean = false;
  private adding_: boolean = false;
  private callbackRouter_: MostVisitedPageCallbackRouter;
  private pageHandler_: MostVisitedPageHandlerRemote;
  private windowProxy_: MostVisitedWindowProxy;
  private actionMenuTargetIndex_: number = -1;
  private dragOffset_: {x: number, y: number}|null;
  private tileRects_: DOMRect[] = [];
  private isRtl_: boolean = false;
  private mediaEventTracker_: EventTracker;
  private eventTracker_: EventTracker;
  private boundOnDocumentKeyDown_: (e: KeyboardEvent) => void = (_e) => null;
  private prefetchTimer_: null|ReturnType<typeof setTimeout> = null;
  private preconnectTimer_: null|ReturnType<typeof setTimeout> = null;
  private dragImage_: HTMLImageElement;

  private accessor info_: MostVisitedInfo|null = null;

  private get tileElements_() {
    return Array.from(
        this.shadowRoot.querySelectorAll<HTMLElement>('.tile:not([hidden])'));
  }

  constructor() {
    performance.mark('most-visited-creation-start');
    super();

    this.callbackRouter_ = MostVisitedBrowserProxy.getInstance().callbackRouter;

    this.pageHandler_ = MostVisitedBrowserProxy.getInstance().handler;

    this.windowProxy_ = MostVisitedWindowProxy.getInstance();

    // Position of the mouse with respect to the top-left corner of the tile
    // being dragged.
    this.dragOffset_ = null;

    // Create a transparent 1x1 pixel image that will replace the default drag
    // "ghost" image. The image is preloaded to ensure it's available when
    // dragging starts.
    this.dragImage_ = new Image(1, 1);
    this.dragImage_.src =
        'data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAA' +
        'ABAAEAAAICTAEAOw==';

    this.mediaEventTracker_ = new EventTracker();
    this.eventTracker_ = new EventTracker();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.isRtl_ = window.getComputedStyle(this)['direction'] === 'rtl';

    this.onSingleRowChange_();

    this.callbackRouter_.setMostVisitedInfo.addListener(
        (info: MostVisitedInfo) => {
          performance.measure('most-visited-mojo', 'most-visited-mojo-start');
          this.info_ = info;
        });

    this.pageHandler_.getMostVisitedExpandedState().then(({isExpanded}) => {
      this.showAll_ = isExpanded;
    });
    performance.mark('most-visited-mojo-start');
    this.eventTracker_.add(document, 'visibilitychange', () => {
      // This updates the most visited tiles every time the NTP tab gets
      // activated.
      if (document.visibilityState === 'visible') {
        this.pageHandler_.updateMostVisitedInfo();
      }
    });
    this.pageHandler_.updateMostVisitedInfo();
    FocusOutlineManager.forDocument(document);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.mediaEventTracker_.removeAll();
    this.eventTracker_.removeAll();
    this.ownerDocument.removeEventListener(
        'keydown', this.boundOnDocumentKeyDown_);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedProperties.has('theme')) {
      this.useWhiteTileIcon_ = this.computeUseWhiteTileIcon_();
      this.isDark_ = this.computeIsDark_();
    }

    if (changedPrivateProperties.has('info_') && this.info_ !== null) {
      this.visible_ = this.info_.visible;
      this.customLinksEnabled_ = this.info_.customLinksEnabled;
      this.enterpriseShortcutsEnabled_ = this.info_.enterpriseShortcutsEnabled;
      this.maxTiles_ = (this.customLinksEnabled_ ? MAX_TILES_FOR_CUSTOM_LINKS :
                                                   MAX_TILES_DEFAULT) +
          (this.enterpriseShortcutsEnabled_ ?
               MAX_TILES_FOR_ENTERPRISE_SHORTCUTS :
               0);
      this.tiles_ = this.info_.tiles.slice(0, this.maxTiles_);
    }

    this.showShowMore_ = this.computeShowShowMore_();
    this.showShowLess_ = this.computeShowShowLess_();
    this.showAdd_ = this.computeShowAdd_();
    this.columnCount_ = this.computeColumnCount_();
    this.rowCount_ = this.computeRowCount_();

    if (changedPrivateProperties.has('tiles_') ||
        changedPrivateProperties.has('dialogTileUrl_')) {
      this.dialogShortcutAlreadyExists_ =
          this.computeDialogShortcutAlreadyExists_();
    }

    if (changedPrivateProperties.has('dialogShortcutAlreadyExists_')) {
      this.dialogTileUrlError_ = this.computeDialogTileUrlError_();
    }

    if (changedPrivateProperties.has('dialogTitle_') ||
        changedPrivateProperties.has('dialogTileUrl_')) {
      this.dialogSaveDisabled_ = this.computeDialogSaveDisabled_();
    }
  }

  override firstUpdated() {
    this.boundOnDocumentKeyDown_ = e => this.onDocumentKeyDown_(e);
    this.ownerDocument.addEventListener(
        'keydown', this.boundOnDocumentKeyDown_);

    performance.measure('most-visited-creation', 'most-visited-creation-start');
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    this.maxVisibleTiles_ = this.computeMaxVisibleTiles_();

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedProperties.has('singleRow')) {
      this.onSingleRowChange_();
    }

    if (changedPrivateProperties.has('tiles_')) {
      if (this.tiles_.length > 0) {
        this.onTilesRendered_();
      }
    }
  }

  protected getBackgroundColorStyle_(): string {
    const skColor = this.theme ? this.theme.backgroundColor : null;
    return skColor ? skColorToRgba(skColor) : 'inherit';
  }

  // Adds "force-hover" class to the tile element positioned at `index`.
  private enableForceHover_(index: number) {
    this.tileElements_[index]!.classList.add('force-hover');
  }

  private clearForceHover_() {
    const forceHover = this.shadowRoot.querySelector('.force-hover');
    if (forceHover) {
      forceHover.classList.remove('force-hover');
    }
  }

  private computeColumnCount_(): number {
    const shortcutCount = this.tiles_ ? this.tiles_.length : 0;
    const canShowAdd = this.expandableTilesEnabled ?
        this.showAdd_ :
        this.maxTiles_ > shortcutCount;
    const canShowShowMore = this.expandableTilesEnabled && this.showShowMore_;
    const canShowShowLess = this.expandableTilesEnabled && this.showShowLess_;
    const visibleShortcutCount =
        canShowShowMore ? this.maxTilesBeforeShowMore + 1 : shortcutCount;
    const totalTileCount = visibleShortcutCount + (canShowAdd ? 1 : 0) +
        (canShowShowMore || canShowShowLess ? 1 : 0);
    const columnCount = totalTileCount <= this.maxVisibleColumnCount_ ?
        totalTileCount :
        Math.min(
            this.maxVisibleColumnCount_,
            Math.ceil(totalTileCount / (this.singleRow ? 1 : 2)));
    return columnCount || 3;
  }

  private computeRowCount_(): number {
    if (this.columnCount_ === 0) {
      return 0;
    }

    if (this.reflowOnOverflow && this.tiles_) {
      const visibleShortcutCount =
          this.expandableTilesEnabled && this.showShowMore_ ?
          this.maxTilesBeforeShowMore + 1 :
          this.tiles_.length;
      return Math.ceil(
          (visibleShortcutCount + (this.showAdd_ ? 1 : 0) +
           (this.showShowMore_ || this.showShowLess_ ? 1 : 0)) /
          this.columnCount_);
    }

    if (this.singleRow) {
      return 1;
    }

    const shortcutCount = this.tiles_ ? this.tiles_.length : 0;
    return this.columnCount_ <= shortcutCount ? 2 : 1;
  }

  private computeMaxVisibleTiles_(): number {
    if (this.expandableTilesEnabled && this.showShowMore_) {
      return this.maxTilesBeforeShowMore + 1;
    }

    if (this.reflowOnOverflow) {
      return this.maxTiles_;
    }

    return this.columnCount_ * this.rowCount_;
  }

  private computeShowAdd_(): boolean {
    if (this.showShowMore_) {
      return false;
    }
    if (!this.customLinksEnabled_) {
      return false;
    }
    // When uninitialized, the custom links may have a different source like
    // TOP_SITES or POPULAR.
    const customLinkTilesCount =
        this.tiles_.filter(tile => !this.isFromEnterpriseShortcut_(tile.source))
            .length;
    return this.tiles_.length < (this.expandableTilesEnabled && this.showAll_ ?
                                     this.maxTiles_ :
                                     this.maxVisibleTiles_) &&
        customLinkTilesCount < MAX_TILES_FOR_CUSTOM_LINKS;
  }

  private computeShowShowMore_(): boolean {
    return this.expandableTilesEnabled && !this.showAll_ && this.tiles_ &&
        this.tiles_.length > this.maxTilesBeforeShowMore;
  }

  private computeShowShowLess_(): boolean {
    return this.expandableTilesEnabled && this.showAll_ && this.tiles_ &&
        this.tiles_.length > this.maxTilesBeforeShowMore;
  }

  protected async onShowMoreClick_() {
    this.showAll_ = true;
    this.pageHandler_.setMostVisitedExpandedState(this.showAll_);
    await this.updateComplete;
    this.tileFocus_(this.maxTilesBeforeShowMore + 1);
  }

  protected async onShowLessClick_() {
    this.showAll_ = false;
    this.pageHandler_.setMostVisitedExpandedState(this.showAll_);
    await this.updateComplete;
    this.$.showMore.focus();
  }

  private computeDialogSaveDisabled_(): boolean {
    return !this.dialogTileUrl_.trim() ||
        normalizeUrl(this.dialogTileUrl_) === null ||
        this.dialogShortcutAlreadyExists_;
  }

  private computeDialogShortcutAlreadyExists_(): boolean {
    const dialogTileHref = (normalizeUrl(this.dialogTileUrl_) || {}).href;
    if (!dialogTileHref) {
      return false;
    }
    // Bypass check for enteprise shortcuts.
    if (this.dialogSource_ === TileSource.ENTERPRISE_SHORTCUTS) {
      return false;
    }
    return (this.tiles_ || []).some(({url: {url}}, index) => {
      if (index === this.actionMenuTargetIndex_) {
        return false;
      }
      const otherUrl = normalizeUrl(url);
      return otherUrl && otherUrl.href === dialogTileHref &&
          this.tiles_[index]!.source !== TileSource.ENTERPRISE_SHORTCUTS;
    });
  }

  private computeDialogTileUrlError_(): string {
    return loadTimeData.getString(
        this.dialogShortcutAlreadyExists_ ? 'shortcutAlreadyExists' :
                                            'invalidUrl');
  }

  private computeIsDark_(): boolean {
    return this.theme ? this.theme.isDark : false;
  }

  private computeUseWhiteTileIcon_(): boolean {
    return this.theme ? this.theme.useWhiteTileIcon : false;
  }

  /**
   * This method is always called when the drag and drop was finished (even when
   * the drop was canceled). If the tiles were reordered successfully, there
   * should be a tile with the "dropped" class.
   *
   * |reordering_| is not set to false when the tiles are reordered. The callers
   * will need to set it to false. This is necessary to handle a mouse drag
   * issue.
   */
  private dragEnd_() {
    if (!this.customLinksEnabled_ && !this.enterpriseShortcutsEnabled_) {
      this.reordering_ = false;
      return;
    }

    this.dragOffset_ = null;

    const dragElement =
        this.shadowRoot.querySelector<HTMLElement>('.tile.dragging');
    const droppedElement =
        this.shadowRoot.querySelector<HTMLElement>('.tile.dropped');

    if (!dragElement && !droppedElement) {
      this.reordering_ = false;
      return;
    }

    if (dragElement) {
      dragElement.classList.remove('dragging');

      this.tileElements_.forEach(el => resetTilePosition(el));
      resetTilePosition(this.$.addShortcut);
      resetTilePosition(this.$.showMore);
      resetTilePosition(this.$.showLess);
    } else if (droppedElement) {
      droppedElement.classList.remove('dropped');

      // Note that resetTilePosition has already been called on drop_.
    }
  }

  /**
   * This method is called on "drop" events (i.e. when the user drops the tile
   * on a valid region.)
   *
   * If a pointer is over a tile rect that is different from the one being
   * dragged, the dragging tile is moved to the new position. The reordering is
   * done in the DOM and by the |reorderMostVisitedTile()| call. This is done to
   * prevent flicking between the time when the tiles are moved back to their
   * original positions (by removing position absolute) and when the tiles are
   * updated via the |setMostVisitedInfo| handler.
   *
   * We remove the "dragging" class in this method, and add "dropped" to
   * indicate that the dragged tile was successfully dropped.
   */
  private drop_(x: number, y: number) {
    if (!this.customLinksEnabled_ && !this.enterpriseShortcutsEnabled_) {
      return;
    }

    const dragElement =
        this.shadowRoot.querySelector<HTMLElement>('.tile.dragging');
    if (!dragElement) {
      return;
    }

    const dragIndex = Number(dragElement.dataset['index']);
    const dropIndex = getHitIndex(this.tileRects_, x, y);
    if (dragIndex !== dropIndex && dropIndex > -1) {
      const dragTile = this.tiles_[dragIndex];
      assert(dragTile);
      const dropTile = this.tiles_[dropIndex];
      assert(dropTile);
      if (this.isFromEnterpriseShortcut_(dragTile.source) !==
          this.isFromEnterpriseShortcut_(dropTile.source)) {
        return;
      }
      const [draggingTile] = this.tiles_.splice(dragIndex, 1);
      assert(draggingTile);
      this.tiles_.splice(dropIndex, 0, draggingTile);
      this.requestUpdate();

      let newDropIndex = dropIndex;
      // When reordering custom links, the index needs to be adjusted by the
      // number of enterprise shortcuts, which are always shown first.
      if (!this.isFromEnterpriseShortcut_(draggingTile.source)) {
        newDropIndex -=
            this.tiles_.filter(t => this.isFromEnterpriseShortcut_(t.source))
                .length;
      }
      this.pageHandler_.reorderMostVisitedTile(draggingTile, newDropIndex);

      // Remove the "dragging" class here to prevent flickering.
      dragElement.classList.remove('dragging');

      // Add "dropped" class so that we can skip disabling `reordering_` in
      // `dragEnd_`.
      dragElement.classList.add('dropped');

      this.tileElements_.forEach(el => resetTilePosition(el));
      resetTilePosition(this.$.addShortcut);
      resetTilePosition(this.$.showMore);
      resetTilePosition(this.$.showLess);
    }
  }

  /**
   * The positions of the tiles are updated based on the location of the
   * pointer.
   */
  private dragOver_(x: number, y: number) {
    const dragElement =
        this.shadowRoot.querySelector<HTMLElement>('.tile.dragging');
    if (!dragElement) {
      this.reordering_ = false;
      return;
    }

    const dragIndex = Number(dragElement.dataset['index']);
    setTilePosition(dragElement, {
      x: x - this.dragOffset_!.x,
      y: y - this.dragOffset_!.y,
    });
    let dropIndex = getHitIndex(this.tileRects_, x, y);
    if (dropIndex > -1) {
      const dragTile = this.tiles_[dragIndex]!;
      const dropTile = this.tiles_[dropIndex]!;
      if (this.isFromEnterpriseShortcut_(dragTile.source) !==
          this.isFromEnterpriseShortcut_(dropTile.source)) {
        dropIndex = -1;
      }
    }
    this.tileElements_.forEach((element, i) => {
      let positionIndex;
      if (i === dragIndex) {
        return;
      } else if (dropIndex === -1) {
        positionIndex = i;
      } else if (dragIndex < dropIndex && dragIndex <= i && i <= dropIndex) {
        positionIndex = i - 1;
      } else if (dragIndex > dropIndex && dragIndex >= i && i >= dropIndex) {
        positionIndex = i + 1;
      } else {
        positionIndex = i;
      }
      setTilePosition(element, this.tileRects_[positionIndex]!);
    });
  }

  /**
   * Sets up tile reordering for both drag and touch events. This method stores
   * the following to be used in |dragOver_()| and |dragEnd_()|.
   *   |dragOffset_|: This is the mouse/touch offset with respect to the
   *       top/left corner of the tile being dragged. It is used to update the
   *       dragging tile location during the drag.
   *   |reordering_|: This is property/attribute used to hide the hover style
   *       and cr-icon-button of the tiles while they are being reordered.
   *   |tileRects_|: This is the rects of the tiles before the drag start. It is
   *       to determine which tile the pointer is over while dragging.
   */
  private dragStart_(dragElement: HTMLElement, x: number, y: number) {
    // Need to clear the tile that has a forced hover style for when the drag
    // started without moving the mouse after the last drag/drop.
    this.clearForceHover_();

    dragElement.classList.add('dragging');
    const dragElementRect = dragElement.getBoundingClientRect();
    this.dragOffset_ = {
      x: x - dragElementRect.x,
      y: y - dragElementRect.y,
    };
    const visibleElements = this.tileElements_;
    const numTiles = visibleElements.length;
    if (this.showAdd_) {
      visibleElements.push(this.$.addShortcut);
    }
    if (this.showShowMore_) {
      visibleElements.push(this.$.showMore);
    }
    if (this.showShowLess_) {
      visibleElements.push(this.$.showLess);
    }

    // Get all the rects first before setting the absolute positions.
    const allRects = visibleElements.map(t => t.getBoundingClientRect());
    this.tileRects_ = allRects.slice(0, numTiles);

    visibleElements.forEach((el, i) => {
      setTilePosition(el, allRects[i]!);
    });
    this.reordering_ = true;
  }

  protected getFaviconUrl_(url: Url): string {
    const faviconUrl = new URL('chrome://favicon2/');
    faviconUrl.searchParams.set('size', '24');
    faviconUrl.searchParams.set('scaleFactor', '1x');
    faviconUrl.searchParams.set('showFallbackMonogram', '');
    faviconUrl.searchParams.set('pageUrl', url.url);
    return faviconUrl.href;
  }

  protected getRestoreButtonText_(): string {
    return loadTimeData.getString(
        this.isFromEnterpriseShortcut_(this.toastSource_) ?
            'restoreDefaultEnterpriseShortcuts' :
            this.customLinksEnabled_ ? 'restoreDefaultLinks' :
                                       'restoreThumbnailsShort');
  }

  protected getTileTitleDirectionClass_(tile: MostVisitedTile): string {
    return tile.titleDirection === TextDirection.RIGHT_TO_LEFT ? 'title-rtl' :
                                                                 'title-ltr';
  }

  protected isHidden_(index: number): boolean {
    if (this.reflowOnOverflow && !this.showShowMore_) {
      return false;
    }

    return index >= this.maxVisibleTiles_;
  }

  protected onSingleRowChange_() {
    if (!this.isConnected) {
      return;
    }
    this.mediaEventTracker_.removeAll();
    const queryLists: MediaQueryList[] = [];
    const updateCount = () => {
      const index = queryLists.findIndex(listener => listener.matches);
      this.maxVisibleColumnCount_ =
          3 + (index > -1 ? queryLists.length - index : 0);
    };
    const maxColumnCount = this.singleRow ? 10 : 5;
    for (let i = maxColumnCount; i >= 4; i--) {
      const query = `(min-width: ${112 * (i + 1)}px)`;
      const queryList = this.windowProxy_.matchMedia(query);
      this.mediaEventTracker_.add(queryList, 'change', updateCount);
      queryLists.push(queryList);
    }
    updateCount();
  }

  protected onAdd_() {
    this.dialogIsReadonly_ = false;
    this.dialogSource_ = TileSource.CUSTOM_LINKS;
    this.dialogTitle_ = loadTimeData.getString('addLinkTitle');
    this.dialogTileTitle_ = '';
    this.dialogTileUrl_ = '';
    this.dialogTileUrlInvalid_ = false;
    this.adding_ = true;
    this.$.dialog.showModal();
  }

  protected onAddShortcutKeyDown_(e: KeyboardEvent) {
    if (hasKeyModifiers(e)) {
      return;
    }

    if (!this.tiles_ || this.tiles_.length === 0) {
      return;
    }
    const backKey = this.isRtl_ ? 'ArrowRight' : 'ArrowLeft';
    if (e.key === backKey || e.key === 'ArrowUp') {
      this.tileFocus_(this.tiles_.length - 1);
    }

    const advanceKey = this.isRtl_ ? 'ArrowLeft' : 'ArrowRight';
    if (e.key === advanceKey || e.key === 'ArrowDown') {
      if (this.showShowLess_) {
        this.$.showLess.focus();
      }
    }
  }

  protected onShowMoreKeyDown_(e: KeyboardEvent) {
    if (hasKeyModifiers(e)) {
      return;
    }

    const backKey = this.isRtl_ ? 'ArrowRight' : 'ArrowLeft';
    if (e.key === backKey || e.key === 'ArrowUp') {
      this.tileFocus_(this.maxTilesBeforeShowMore);
    }
  }

  protected onShowLessKeyDown_(e: KeyboardEvent) {
    if (hasKeyModifiers(e)) {
      return;
    }

    const backKey = this.isRtl_ ? 'ArrowRight' : 'ArrowLeft';
    if (e.key === backKey || e.key === 'ArrowUp') {
      if (this.showAdd_) {
        this.$.addShortcut.focus();
      } else {
        this.tileFocus_(this.tiles_.length - 1);
      }
    }
  }

  protected onDialogCancel_() {
    this.actionMenuTargetIndex_ = -1;
    this.$.dialog.cancel();
  }

  protected onDialogClose_() {
    this.dialogTileUrl_ = '';
    if (this.adding_) {
      this.$.addShortcut.focus();
    }
    this.adding_ = false;
  }

  protected onDialogTileUrlBlur_() {
    if (this.dialogTileUrl_.length > 0 &&
        (normalizeUrl(this.dialogTileUrl_) === null ||
         this.dialogShortcutAlreadyExists_)) {
      this.dialogTileUrlInvalid_ = true;
    }
  }

  protected onDialogTileUrlChange_(e: Event) {
    this.dialogTileUrl_ = (e.target as HTMLInputElement).value;
    this.dialogTileUrlInvalid_ = false;
  }

  protected onDialogTileNameChange_(e: Event) {
    this.dialogTileTitle_ = (e.target as HTMLInputElement).value;
  }

  protected onDocumentKeyDown_(e: KeyboardEvent) {
    if (e.altKey || e.shiftKey) {
      return;
    }

    const modifier = isMac ? e.metaKey && !e.ctrlKey : e.ctrlKey && !e.metaKey;
    if (modifier && e.key === 'z') {
      e.preventDefault();
      this.onUndoClick_();
    }
  }

  protected onDragStart_(e: DragEvent) {
    const item = this.tiles_[this.getCurrentTargetIndex_(e)]!;
    assert(item);
    if (!this.customLinksEnabled_ &&
        !this.isFromEnterpriseShortcut_(item.source)) {
      return;
    }
    // |dataTransfer| is null in tests.
    if (e.dataTransfer) {
      // Replace the ghost image that appears when dragging with a transparent
      // 1x1 pixel image.
      e.dataTransfer.setDragImage(this.dragImage_, 0, 0);
    }

    this.dragStart_(e.target as HTMLElement, e.x, e.y);

    const dragOver = (e: DragEvent) => {
      e.preventDefault();
      e.dataTransfer!.dropEffect = 'move';
      this.dragOver_(e.x, e.y);
    };

    const drop = (e: DragEvent) => {
      this.drop_(e.x, e.y);

      const dropIndex = getHitIndex(this.tileRects_, e.x, e.y);
      if (dropIndex !== -1) {
        this.enableForceHover_(dropIndex);
      }
    };

    this.ownerDocument.addEventListener('dragover', dragOver);
    this.ownerDocument.addEventListener('drop', drop);
    this.ownerDocument.addEventListener('dragend', _ => {
      this.ownerDocument.removeEventListener('dragover', dragOver);
      this.ownerDocument.removeEventListener('drop', drop);
      this.dragEnd_();

      this.addEventListener('pointermove', () => {
        this.clearForceHover_();
        // When |reordering_| is true, the normal hover style is not shown.
        // After a drop, the element that has hover is not correct. It will be
        // after the mouse moves.
        this.reordering_ = false;
      }, {once: true});
    }, {once: true});
  }

  protected onViewOrEdit_() {
    this.$.actionMenu.close();
    const tile = this.tiles_[this.actionMenuTargetIndex_]!;
    const isReadonly = !tile.allowUserEdit;
    this.dialogIsReadonly_ = isReadonly;
    this.dialogSource_ = tile.source;
    this.dialogTitle_ =
        loadTimeData.getString(isReadonly ? 'viewLinkTitle' : 'editLinkTitle');
    this.dialogTileTitle_ = tile.title;
    this.dialogTileUrl_ = tile.url.url;
    this.dialogTileUrlInvalid_ = false;
    this.$.dialog.showModal();
  }

  protected onRestoreDefaultsClick_() {
    if (!this.$.toastManager.isToastOpen || this.$.toastManager.slottedHidden) {
      return;
    }
    this.$.toastManager.hide();
    this.pageHandler_.restoreMostVisitedDefaults(this.toastSource_);
  }

  protected async onRemove_() {
    this.$.actionMenu.close();
    await this.tileRemove_(this.actionMenuTargetIndex_);
    this.actionMenuTargetIndex_ = -1;
  }

  protected async onSave_() {
    if (this.dialogIsReadonly_) {
      this.$.dialog.close();
      return;
    }
    const newUrl = {url: normalizeUrl(this.dialogTileUrl_)!.href};
    this.$.dialog.close();
    let newTitle = this.dialogTileTitle_.trim();
    if (newTitle.length === 0) {
      newTitle = this.dialogTileUrl_;
    }
    if (this.adding_) {
      const {success} =
          await this.pageHandler_.addMostVisitedTile(newUrl, newTitle);
      this.toast_(
          success ? 'linkAddedMsg' : 'linkCantCreate', success,
          TileSource.TOP_SITES);
    } else {
      const oldTile = this.tiles_[this.actionMenuTargetIndex_]!;
      if (oldTile.url.url !== newUrl.url || oldTile.title !== newTitle) {
        const {success} = await this.pageHandler_.updateMostVisitedTile(
            oldTile, newUrl, newTitle);
        this.toast_(
            success ? 'linkEditedMsg' : 'linkCantEdit', success,
            oldTile.source);
      }
      this.actionMenuTargetIndex_ = -1;
    }
  }

  private getCurrentTargetIndex_(e: Event): number {
    const target = e.currentTarget as HTMLElement;
    return Number(target.dataset['index']);
  }

  protected onTileActionButtonClick_(e: Event) {
    e.preventDefault();
    this.actionMenuTargetIndex_ = this.getCurrentTargetIndex_(e);
    const item = this.tiles_[this.getCurrentTargetIndex_(e)];
    assert(item);
    this.actionMenuRemoveDisabled_ = !item.allowUserDelete;
    this.actionMenuViewOrEditTitle_ = loadTimeData.getString(
        item.allowUserEdit ? 'editLinkTitle' : 'viewLink');
    this.$.actionMenu.showAt(e.target as HTMLElement);
  }

  protected onTileRemoveButtonClick_(e: Event) {
    e.preventDefault();
    this.tileRemove_(this.getCurrentTargetIndex_(e));
  }

  protected onTileClick_(e: MouseEvent) {
    if (e.defaultPrevented) {
      // Ignore previously handled events.
      return;
    }

    e.preventDefault();  // Prevents default browser action (navigation).

    const index = this.getCurrentTargetIndex_(e);
    const item = this.tiles_[index]!;
    this.pageHandler_.onMostVisitedTileNavigation(
        item, index, e.button || 0, e.altKey, e.ctrlKey, e.metaKey, e.shiftKey);
  }

  protected onTileKeyDown_(e: KeyboardEvent) {
    if (hasKeyModifiers(e)) {
      return;
    }

    if (e.key !== 'ArrowLeft' && e.key !== 'ArrowRight' &&
        e.key !== 'ArrowUp' && e.key !== 'ArrowDown' && e.key !== 'Delete') {
      return;
    }

    const index = this.getCurrentTargetIndex_(e);
    if (e.key === 'Delete') {
      this.tileRemove_(index);
      return;
    }

    const advanceKey = this.isRtl_ ? 'ArrowLeft' : 'ArrowRight';
    const delta = (e.key === advanceKey || e.key === 'ArrowDown') ? 1 : -1;
    const newIndex = Math.max(0, index + delta);
    if (this.showShowMore_ && newIndex === this.maxTilesBeforeShowMore + 1) {
      this.$.showMore.focus();
    } else {
      this.tileFocus_(newIndex);
    }
  }

  protected onTileHover_(e: Event) {
    if (e.defaultPrevented) {
      // Ignore previously handled events.
      return;
    }

    const item = this.tiles_[this.getCurrentTargetIndex_(e)];
    assert(item);

    // Preconnect is intended to be run on mouse hover when prerender is
    // enabled.
    if (loadTimeData.getBoolean('prerenderOnPressEnabled') &&
        loadTimeData.getInteger('preconnectStartTimeThreshold') >= 0) {
      this.preconnectTimer_ = setTimeout(() => {
        this.pageHandler_.preconnectMostVisitedTile(item);
      }, loadTimeData.getInteger('preconnectStartTimeThreshold'));
    }

    if (loadTimeData.getBoolean('prefetchTriggerEnabled') &&
        loadTimeData.getInteger('prefetchStartTimeThreshold') >= 0) {
      this.prefetchTimer_ = setTimeout(() => {
        this.pageHandler_.prefetchMostVisitedTile(item);
      }, loadTimeData.getInteger('prefetchStartTimeThreshold'));
    }
  }

  protected onTileMouseDown_(e: Event) {
    if (e.defaultPrevented) {
      // Ignore previously handled events.
      return;
    }
    if (loadTimeData.getBoolean('prerenderOnPressEnabled')) {
      const item = this.tiles_[this.getCurrentTargetIndex_(e)];
      assert(item);
      // prefetchMostVisitedTile is called explicitly to guarantee prefetch
      // ahead of prerender, and the duplicate prefetch requests will be
      // prevented at `StartPrefetch`.
      if (loadTimeData.getBoolean('prefetchTriggerEnabled')) {
        this.pageHandler_.prefetchMostVisitedTile(item);
      }
      this.pageHandler_.prerenderMostVisitedTile(item);
    }
  }

  protected onTileExit_(e: Event) {
    if (e.defaultPrevented) {
      // Ignore previously handled events.
      return;
    }

    if (this.prefetchTimer_) {
      clearTimeout(this.prefetchTimer_);
    }

    if (this.preconnectTimer_) {
      clearTimeout(this.preconnectTimer_);
    }

    if (loadTimeData.getBoolean('prerenderOnPressEnabled')) {
      this.pageHandler_.cancelPrerender();
    }
  }

  protected onUndoClick_() {
    if (!this.$.toastManager.isToastOpen || this.$.toastManager.slottedHidden) {
      return;
    }
    this.$.toastManager.hide();
    this.pageHandler_.undoMostVisitedTileAction(this.toastSource_);
  }

  protected onUndoAutoRemovalClick_() {
    if (!this.$.toastManager.isToastOpen || this.$.toastManager.slottedHidden) {
      return;
    }
    this.$.toastManager.hide();
    this.pageHandler_.undoMostVisitedAutoRemoval();
  }

  protected onTouchStart_(e: TouchEvent) {
    if (this.reordering_) {
      return;
    }
    const item = this.tiles_[this.getCurrentTargetIndex_(e)]!;
    assert(item);
    if (!this.customLinksEnabled_ &&
        !this.isFromEnterpriseShortcut_(item.source)) {
      return;
    }
    const tileElement =
        (e.composedPath() as HTMLElement[])
            .find(el => el.classList && el.classList.contains('tile'));
    if (!tileElement) {
      return;
    }
    const {clientX, clientY} = e.changedTouches[0]!;
    this.dragStart_(tileElement, clientX, clientY);
    const touchMove = (e: TouchEvent) => {
      const {clientX, clientY} = e.changedTouches[0]!;
      this.dragOver_(clientX, clientY);
    };
    const touchEnd = (e: TouchEvent) => {
      this.ownerDocument.removeEventListener('touchmove', touchMove);
      tileElement.removeEventListener('touchend', touchEnd);
      tileElement.removeEventListener('touchcancel', touchEnd);
      const {clientX, clientY} = e.changedTouches[0]!;
      this.drop_(clientX, clientY);
      this.dragEnd_();
      this.reordering_ = false;
    };
    this.ownerDocument.addEventListener('touchmove', touchMove);
    tileElement.addEventListener('touchend', touchEnd, {once: true});
    tileElement.addEventListener('touchcancel', touchEnd, {once: true});
  }

  private tileFocus_(index: number) {
    if (index < 0) {
      return;
    }
    const tileElements = this.tileElements_;
    if (index < tileElements.length) {
      (tileElements[index] as HTMLElement).querySelector('a')!.focus();
    } else if (this.showAdd_ && index === tileElements.length) {
      this.$.addShortcut.focus();
    } else if (this.showShowLess_ && index === tileElements.length) {
      this.$.showLess.focus();
    }
  }

  // TODO(crbug.com/467437715): Make private and bind to listener once browser
  // side is ready.
  autoRemovalToast() {
    this.autoRemovalInProgress_ = true;
    this.$.toastManager.show(
        loadTimeData.getString('shortcutsInactivityRemovalMsg'),
        /* hideSlotted= */ false);
  }

  private toast_(msgId: string, showButtons: boolean, source: TileSource) {
    this.autoRemovalInProgress_ = false;
    this.toastSource_ = source;
    this.$.toastManager.show(loadTimeData.getString(msgId), !showButtons);
  }

  private async tileRemove_(index: number) {
    const tile = this.tiles_[index]!;
    this.pageHandler_.deleteMostVisitedTile(tile);
    // Do not show the toast buttons when a query tile is removed unless it is
    // a custom link. Removal is not reversible for non custom link query tiles.
    this.toast_(
        'linkRemovedMsg',
        /* showButtons= */ this.customLinksEnabled_ ||
            this.enterpriseShortcutsEnabled_ || !tile.isQueryTile,
        tile.source);

    // Move focus after the next render so that tileElements_ is updated.
    await this.updateComplete;
    this.tileFocus_(index);
  }

  protected onTilesRendered_() {
    performance.measure('most-visited-rendered');
    assert(this.maxVisibleTiles_ > 0);
    this.pageHandler_.onMostVisitedTilesRendered(
        this.tiles_.slice(0, this.maxVisibleTiles_), this.windowProxy_.now());
  }

  protected getMoreActionText_(title: string) {
    // Check that 'shortcutMoreActions' is set to more than an empty string,
    // since we do not use this text for third party NTP.
    return loadTimeData.getString('shortcutMoreActions') ?
        loadTimeData.getStringF('shortcutMoreActions', title) :
        '';
  }

  protected isFromEnterpriseShortcut_(source: number) {
    return source === TileSource.ENTERPRISE_SHORTCUTS;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-most-visited': MostVisitedElement;
  }
}

customElements.define(MostVisitedElement.is, MostVisitedElement);
