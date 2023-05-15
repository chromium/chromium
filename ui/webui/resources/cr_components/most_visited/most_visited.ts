// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {skColorToRgba} from 'chrome://resources/js/color_utils.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {hasKeyModifiers} from 'chrome://resources/js/util_ts.js';
import {TextDirection} from 'chrome://resources/mojo/mojo/public/mojom/base/text_direction.mojom-webui.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {DomRepeat, DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MostVisitedBrowserProxy} from './browser_proxy.js';
import {getTemplate} from './most_visited.html.js';
import {MostVisitedInfo, MostVisitedPageCallbackRouter, MostVisitedPageHandlerRemote, MostVisitedTheme, MostVisitedTile} from './most_visited.mojom-webui.js';
import {MostVisitedWindowProxy} from './window_proxy.js';

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

const MostVisitedElementBase = I18nMixin(PolymerElement);

export interface MostVisitedElement {
  $: {
    actionMenu: CrActionMenuElement,
    container: HTMLElement,
    dialog: CrDialogElement,
    toast: CrToastElement,
    addShortcut: HTMLElement,
    tiles: DomRepeat,
  };
}

export class MostVisitedElement extends MostVisitedElementBase {
  static get is() {
    return 'cr-most-visited';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      theme: Object,

      /**
       * If true, renders MV tiles in a single row up to 10 columns wide.
       * If false, renders MV tiles in up to 2 rows up to 5 columns wide.
       */
      singleRow: {
        type: Boolean,
        value: false,
        observer: 'onSingleRowChange_',
      },

      /** If true, reflows tiles that are overflowing. */
      reflowOnOverflow: {
        type: Boolean,
        value: false,
      },

      /**
       * When the tile icon background is dark, the icon color is white for
       * contrast. This can be used to determine the color of the tile hover as
       * well.
       */
      useWhiteTileIcon_: {
        type: Boolean,
        reflectToAttribute: true,
        computed: `computeUseWhiteTileIcon_(theme)`,
      },

      /**
       * If true wraps the tile titles in white pills.
       */
      useTitlePill_: {
        type: Boolean,
        reflectToAttribute: true,
        computed: `computeUseTitlePill_(theme)`,
      },

      columnCount_: {
        type: Number,
        computed:
            `computeColumnCount_(singleRow, tiles_, maxVisibleColumnCount_, maxTiles_)`,
      },

      rowCount_: {
        type: Number,
        computed: 'computeRowCount_(singleRow, columnCount_, tiles_)',
      },

      customLinksEnabled_: {
        type: Boolean,
        reflectToAttribute: true,
      },

      dialogTileTitle_: String,

      dialogTileUrl_: {
        type: String,
        observer: 'onDialogTileUrlChange_',
      },

      dialogTileUrlInvalid_: {
        type: Boolean,
        value: false,
      },

      dialogTitle_: String,

      dialogSaveDisabled_: {
        type: Boolean,
        computed: `computeDialogSaveDisabled_(dialogTitle_, dialogTileUrl_,
            dialogShortcutAlreadyExists_)`,
      },

      dialogShortcutAlreadyExists_: {
        type: Boolean,
        computed: 'computeDialogShortcutAlreadyExists_(tiles_, dialogTileUrl_)',
      },

      dialogTileUrlError_: {
        type: String,
        computed: `computeDialogTileUrlError_(dialogTileUrl_,
            dialogShortcutAlreadyExists_)`,
      },

      isDark_: {
        type: Boolean,
        reflectToAttribute: true,
        computed: `computeIsDark_(theme)`,
      },

      /**
       * Used to hide hover style and cr-icon-button of tiles while the tiles
       * are being reordered.
       */
      reordering_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      maxTiles_: {
        type: Number,
        computed: 'computeMaxTiles_(customLinksEnabled_)',
      },

      maxVisibleTiles_: {
        type: Number,
        computed: 'computeMaxVisibleTiles_(columnCount_, rowCount_)',
      },

      showAdd_: {
        type: Boolean,
        value: false,
        computed:
            'computeShowAdd_(tiles_, maxVisibleTiles_, customLinksEnabled_)',
      },

      showToastButtons_: Boolean,

      maxVisibleColumnCount_: Number,

      tiles_: Array,

      toastContent_: String,

      visible_: {
        type: Boolean,
        reflectToAttribute: true,
      },
    };
  }

  public theme: MostVisitedTheme|null;
  public reflowOnOverflow: boolean;
  public singleRow: boolean;
  private useWhiteTileIcon_: boolean;
  private useTitlePill_: boolean;
  private columnCount_: number;
  private rowCount_: number;
  private customLinksEnabled_: boolean;
  private dialogTileTitle_: string;
  private dialogTileUrl_: string;
  private dialogTileUrlInvalid_: boolean;
  private dialogTitle_: string;
  private dialogSaveDisabled_: boolean;
  private dialogShortcutAlreadyExists_: boolean;
  private dialogTileUrlError_: string;
  private isDark_: boolean;
  private reordering_: boolean;
  private maxTiles_: number;
  private maxVisibleTiles_: number;
  private showAdd_: boolean;
  private showToastButtons_: boolean;
  private maxVisibleColumnCount_: number;
  private tiles_: MostVisitedTile[];
  private toastContent_: string;
  private visible_: boolean;
  private adding_: boolean = false;
  private callbackRouter_: MostVisitedPageCallbackRouter;
  private pageHandler_: MostVisitedPageHandlerRemote;
  private windowProxy_: MostVisitedWindowProxy;
  private setMostVisitedInfoListenerId_: number|null = null;
  private actionMenuTargetIndex_: number = -1;
  private dragOffset_: {x: number, y: number}|null;
  private tileRects_: DOMRect[] = [];
  private isRtl_: boolean;
  private mediaEventTracker_: EventTracker;
  private eventTracker_: EventTracker;
  private boundOnDocumentKeyDown_: (e: KeyboardEvent) => void;

  private get tileElements_() {
    return Array.from(
        this.shadowRoot!.querySelectorAll<HTMLElement>('.tile:not([hidden])'));
  }

  // Suppress TypeScript's error TS2376 to intentionally allow calling
  // performance.mark() before calling super().
  // @ts-ignore
  constructor() {
    performance.mark('most-visited-creation-start');
    super();

    this.callbackRouter_ = MostVisitedBrowserProxy.getInstance().callbackRouter;

    this.pageHandler_ = MostVisitedBrowserProxy.getInstance().handler;

    this.windowProxy_ = MostVisitedWindowProxy.getInstance();

    /**
     * This is the position of the mouse with respect to the top-left corner
     * of the tile being dragged.
     */
    this.dragOffset_ = null;

    this.mediaEventTracker_ = new EventTracker();
    this.eventTracker_ = new EventTracker();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.isRtl_ = window.getComputedStyle(this)['direction'] === 'rtl';

    this.onSingleRowChange_();

    this.setMostVisitedInfoListenerId_ =
        this.callbackRouter_.setMostVisitedInfo.addListener(
            (info: MostVisitedInfo) => {
              performance.measure(
                  'most-visited-mojo', 'most-visited-mojo-start');
              this.visible_ = info.visible;
              this.customLinksEnabled_ = info.customLinksEnabled;
              assert(this.maxTiles_);
              this.tiles_ = info.tiles.slice(0, this.maxTiles_);
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

  override ready() {
    super.ready();

    this.boundOnDocumentKeyDown_ = e => this.onDocumentKeyDown_(e);
    this.ownerDocument.addEventListener(
        'keydown', this.boundOnDocumentKeyDown_);

    performance.measure('most-visited-creation', 'most-visited-creation-start');
  }

  private rgbaOrInherit_(skColor: SkColor|null): string {
    return skColor ? skColorToRgba(skColor) : 'inherit';
  }

  private clearForceHover_() {
    const forceHover = this.shadowRoot!.querySelector('.force-hover');
    if (forceHover) {
      forceHover.classList.remove('force-hover');
    }
  }

  private computeColumnCount_(): number {
    const shortcutCount = this.tiles_ ? this.tiles_.length : 0;
    const canShowAdd = this.maxTiles_ > shortcutCount;
    const tileCount =
        Math.min(this.maxTiles_, shortcutCount + (canShowAdd ? 1 : 0));
    const columnCount = tileCount <= this.maxVisibleColumnCount_ ?
        tileCount :
        Math.min(
            this.maxVisibleColumnCount_,
            Math.ceil(tileCount / (this.singleRow ? 1 : 2)));
    return columnCount || 3;
  }

  private computeRowCount_(): number {
    if (this.columnCount_ === 0) {
      return 0;
    }

    if (this.reflowOnOverflow && this.tiles_) {
      return Math.ceil(
          (this.tiles_.length + (this.showAdd_ ? 1 : 0)) / this.columnCount_);
    }

    if (this.singleRow) {
      return 1;
    }

    const shortcutCount = this.tiles_ ? this.tiles_.length : 0;
    return this.columnCount_ <= shortcutCount ? 2 : 1;
  }

  private computeMaxTiles_(): number {
    return this.customLinksEnabled_ ? 10 : 8;
  }

  private computeMaxVisibleTiles_(): number {
    if (this.reflowOnOverflow) {
      return this.computeMaxTiles_();
    }

    return this.columnCount_ * this.rowCount_;
  }

  private computeShowAdd_(): boolean {
    return this.customLinksEnabled_ && this.tiles_ &&
        this.tiles_.length < this.maxVisibleTiles_;
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
    return (this.tiles_ || []).some(({url: {url}}, index) => {
      if (index === this.actionMenuTargetIndex_) {
        return false;
      }
      const otherUrl = normalizeUrl(url);
      return otherUrl && otherUrl.href === dialogTileHref;
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

  private computeUseTitlePill_(): boolean {
    return this.theme ? this.theme.useTitlePill : false;
  }

  /**
   * If a pointer is over a tile rect that is different from the one being
   * dragged, the dragging tile is moved to the new position. The reordering
   * is done in the DOM and the by the |reorderMostVisitedTile()| call. This is
   * done to prevent flicking between the time when the tiles are moved back to
   * their original positions (by removing position absolute) and when the
   * tiles are updated via a |setMostVisitedTiles()| call.
   *
   * |reordering_| is not set to false when the tiles are reordered. The callers
   * will need to set it to false. This is necessary to handle a mouse drag
   * issue.
   */
  private dragEnd_(x: number, y: number) {
    if (!this.customLinksEnabled_) {
      this.reordering_ = false;
      return;
    }
    this.dragOffset_ = null;
    const dragElement =
        this.shadowRoot!.querySelector<HTMLElement>('.tile.dragging');
    if (!dragElement) {
      this.reordering_ = false;
      return;
    }
    const dragIndex = (this.$.tiles.modelForElement(dragElement) as unknown as {
                        index: number,
                      }).index;
    dragElement.classList.remove('dragging');
    this.tileElements_.forEach(el => resetTilePosition(el));
    resetTilePosition(this.$.addShortcut);
    const dropIndex = getHitIndex(this.tileRects_, x, y);
    if (dragIndex !== dropIndex && dropIndex > -1) {
      const [draggingTile] = this.tiles_.splice(dragIndex, 1);
      this.tiles_.splice(dropIndex, 0, draggingTile);
      this.notifySplices('tiles_', [
        {
          index: dragIndex,
          removed: [draggingTile],
          addedCount: 0,
          object: this.tiles_,
          type: 'splice',
        },
        {
          index: dropIndex,
          removed: [],
          addedCount: 1,
          object: this.tiles_,
          type: 'splice',
        },
      ]);
      this.pageHandler_.reorderMostVisitedTile(draggingTile.url, dropIndex);
    }
  }

  /**
   * The positions of the tiles are updated based on the location of the
   * pointer.
   */
  private dragOver_(x: number, y: number) {
    const dragElement =
        this.shadowRoot!.querySelector<HTMLElement>('.tile.dragging');
    if (!dragElement) {
      this.reordering_ = false;
      return;
    }
    const dragIndex = (this.$.tiles.modelForElement(dragElement) as unknown as {
                        index: number,
                      }).index;
    setTilePosition(dragElement, {
      x: x - this.dragOffset_!.x,
      y: y - this.dragOffset_!.y,
    });
    const dropIndex = getHitIndex(this.tileRects_, x, y);
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
      setTilePosition(element, this.tileRects_[positionIndex]);
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
    const tileElements = this.tileElements_;
    // Get all the rects first before setting the absolute positions.
    this.tileRects_ = tileElements.map(t => t.getBoundingClientRect());
    if (this.showAdd_) {
      const element = this.$.addShortcut;
      setTilePosition(element, element.getBoundingClientRect());
    }
    tileElements.forEach((tile, i) => {
      setTilePosition(tile, this.tileRects_[i]);
    });
    this.reordering_ = true;
  }

  private getFaviconUrl_(url: Url): string {
    const faviconUrl = new URL('chrome://favicon2/');
    faviconUrl.searchParams.set('size', '24');
    faviconUrl.searchParams.set('scaleFactor', '1x');
    faviconUrl.searchParams.set('showFallbackMonogram', '');
    faviconUrl.searchParams.set('pageUrl', url.url);
    return faviconUrl.href;
  }

  private getRestoreButtonText_(): string {
    return loadTimeData.getString(
        this.customLinksEnabled_ ? 'restoreDefaultLinks' :
                                   'restoreThumbnailsShort');
  }

  private getTileTitleDirectionClass_(tile: MostVisitedTile): string {
    return tile.titleDirection === TextDirection.RIGHT_TO_LEFT ? 'title-rtl' :
                                                                 'title-ltr';
  }

  private isHidden_(index: number): boolean {
    if (this.reflowOnOverflow) {
      return false;
    }

    return index >= this.maxVisibleTiles_;
  }

  private onSingleRowChange_() {
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

  private onAdd_() {
    this.dialogTitle_ = loadTimeData.getString('addLinkTitle');
    this.dialogTileTitle_ = '';
    this.dialogTileUrl_ = '';
    this.dialogTileUrlInvalid_ = false;
    this.adding_ = true;
    this.$.dialog.showModal();
  }

  private onAddShortcutKeyDown_(e: KeyboardEvent) {
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
  }

  private onDialogCancel_() {
    this.actionMenuTargetIndex_ = -1;
    this.$.dialog.cancel();
  }

  private onDialogClose_() {
    this.dialogTileUrl_ = '';
    if (this.adding_) {
      this.$.addShortcut.focus();
    }
    this.adding_ = false;
  }

  private onDialogTileUrlBlur_() {
    if (this.dialogTileUrl_.length > 0 &&
        (normalizeUrl(this.dialogTileUrl_) === null ||
         this.dialogShortcutAlreadyExists_)) {
      this.dialogTileUrlInvalid_ = true;
    }
  }

  private onDialogTileUrlChange_() {
    this.dialogTileUrlInvalid_ = false;
  }

  private onDocumentKeyDown_(e: KeyboardEvent) {
    if (e.altKey || e.shiftKey) {
      return;
    }

    const modifier = isMac ? e.metaKey && !e.ctrlKey : e.ctrlKey && !e.metaKey;
    if (modifier && e.key === 'z') {
      e.preventDefault();
      this.onUndoClick_();
    }
  }

  private onDragStart_(e: DragEvent) {
    if (!this.customLinksEnabled_) {
      return;
    }
    // |dataTransfer| is null in tests.
    if (e.dataTransfer) {
      // Remove the ghost image that appears when dragging.
      e.dataTransfer.setDragImage(new Image(), 0, 0);
    }

    this.dragStart_(e.target as HTMLElement, e.x, e.y);
    const dragOver = (e: DragEvent) => {
      e.preventDefault();
      e.dataTransfer!.dropEffect = 'move';
      this.dragOver_(e.x, e.y);
    };
    this.ownerDocument.addEventListener('dragover', dragOver);
    this.ownerDocument.addEventListener('dragend', e => {
      this.ownerDocument.removeEventListener('dragover', dragOver);
      this.dragEnd_(e.x, e.y);
      const dropIndex = getHitIndex(this.tileRects_, e.x, e.y);
      if (dropIndex !== -1) {
        this.tileElements_[dropIndex].classList.add('force-hover');
      }
      this.addEventListener('pointermove', () => {
        this.clearForceHover_();
        // When |reordering_| is true, the normal hover style is not shown.
        // After a drop, the element that has hover is not correct. It will be
        // after the mouse moves.
        this.reordering_ = false;
      }, {once: true});
    }, {once: true});
  }

  private onEdit_() {
    this.$.actionMenu.close();
    this.dialogTitle_ = loadTimeData.getString('editLinkTitle');
    const tile = this.tiles_[this.actionMenuTargetIndex_];
    this.dialogTileTitle_ = tile.title;
    this.dialogTileUrl_ = tile.url.url;
    this.dialogTileUrlInvalid_ = false;
    this.$.dialog.showModal();
  }

  private onRestoreDefaultsClick_() {
    if (!this.$.toast.open || !this.showToastButtons_) {
      return;
    }
    this.$.toast.hide();
    this.pageHandler_.restoreMostVisitedDefaults();
  }

  private async onRemove_() {
    this.$.actionMenu.close();
    await this.tileRemove_(this.actionMenuTargetIndex_);
    this.actionMenuTargetIndex_ = -1;
  }

  private async onSave_() {
    const newUrl = {url: normalizeUrl(this.dialogTileUrl_)!.href};
    this.$.dialog.close();
    let newTitle = this.dialogTileTitle_.trim();
    if (newTitle.length === 0) {
      newTitle = this.dialogTileUrl_;
    }
    if (this.adding_) {
      const {success} =
          await this.pageHandler_.addMostVisitedTile(newUrl, newTitle);
      this.toast_(success ? 'linkAddedMsg' : 'linkCantCreate', success);
    } else {
      const {url, title} = this.tiles_[this.actionMenuTargetIndex_];
      if (url.url !== newUrl.url || title !== newTitle) {
        const {success} = await this.pageHandler_.updateMostVisitedTile(
            url, newUrl, newTitle);
        this.toast_(success ? 'linkEditedMsg' : 'linkCantEdit', success);
      }
      this.actionMenuTargetIndex_ = -1;
    }
  }

  private onTileActionButtonClick_(e: DomRepeatEvent<MostVisitedTile>) {
    e.preventDefault();
    this.actionMenuTargetIndex_ = e.model.index;
    this.$.actionMenu.showAt(e.target as HTMLElement);
  }

  private onTileRemoveButtonClick_(e: DomRepeatEvent<MostVisitedTile>) {
    e.preventDefault();
    this.tileRemove_(e.model.index);
  }

  private onTileClick_(e: DomRepeatEvent<MostVisitedTile, MouseEvent>) {
    if (e.defaultPrevented) {
      // Ignore previousely handled events.
      return;
    }

    if (loadTimeData.getBoolean('handleMostVisitedNavigationExplicitly')) {
      e.preventDefault();  // Prevents default browser action (navigation).
    }

    this.pageHandler_.onMostVisitedTileNavigation(
        e.model.item, e.model.index, e.button || 0, e.altKey, e.ctrlKey,
        e.metaKey, e.shiftKey);
  }

  private onTileKeyDown_(e: DomRepeatEvent<MostVisitedTile, KeyboardEvent>) {
    if (hasKeyModifiers(e)) {
      return;
    }

    if (e.key !== 'ArrowLeft' && e.key !== 'ArrowRight' &&
        e.key !== 'ArrowUp' && e.key !== 'ArrowDown' && e.key !== 'Delete') {
      return;
    }

    const index = e.model.index;
    if (e.key === 'Delete') {
      this.tileRemove_(index);
      return;
    }

    const advanceKey = this.isRtl_ ? 'ArrowLeft' : 'ArrowRight';
    const delta = (e.key === advanceKey || e.key === 'ArrowDown') ? 1 : -1;
    this.tileFocus_(Math.max(0, index + delta));
  }

  private onUndoClick_() {
    if (!this.$.toast.open || !this.showToastButtons_) {
      return;
    }
    this.$.toast.hide();
    this.pageHandler_.undoMostVisitedTileAction();
  }

  private onTouchStart_(e: TouchEvent) {
    if (this.reordering_ || !this.customLinksEnabled_) {
      return;
    }
    const tileElement =
        (e.composedPath() as HTMLElement[])
            .find(el => el.classList && el.classList.contains('tile'));
    if (!tileElement) {
      return;
    }
    const {clientX, clientY} = e.changedTouches[0];
    this.dragStart_(tileElement, clientX, clientY);
    const touchMove = (e: TouchEvent) => {
      const {clientX, clientY} = e.changedTouches[0];
      this.dragOver_(clientX, clientY);
    };
    const touchEnd = (e: TouchEvent) => {
      this.ownerDocument.removeEventListener('touchmove', touchMove);
      tileElement.removeEventListener('touchend', touchEnd);
      tileElement.removeEventListener('touchcancel', touchEnd);
      const {clientX, clientY} = e.changedTouches[0];
      this.dragEnd_(clientX, clientY);
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
      tileElements[index].focus();
    } else if (this.showAdd_ && index === tileElements.length) {
      this.$.addShortcut.focus();
    }
  }

  private toast_(msgId: string, showButtons: boolean) {
    this.toastContent_ = loadTimeData.getString(msgId);
    this.showToastButtons_ = showButtons;
    this.$.toast.show();
  }

  private tileRemove_(index: number) {
    const {url, isQueryTile} = this.tiles_[index];
    this.pageHandler_.deleteMostVisitedTile(url);
    // Do not show the toast buttons when a query tile is removed unless it is a
    // custom link. Removal is not reversible for non custom link query tiles.
    this.toast_(
        'linkRemovedMsg',
        /* showButtons= */ this.customLinksEnabled_ || !isQueryTile);
    this.tileFocus_(index);
  }

  private onTilesRendered_() {
    performance.measure('most-visited-rendered');
    assert(this.maxVisibleTiles_);
    this.pageHandler_.onMostVisitedTilesRendered(
        this.tiles_.slice(0, this.maxVisibleTiles_), this.windowProxy_.now());
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-most-visited': MostVisitedElement;
  }
}

customElements.define(MostVisitedElement.is, MostVisitedElement);
