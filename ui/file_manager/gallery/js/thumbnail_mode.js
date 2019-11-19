// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Thumbnail Mode.
 */
class ThumbnailMode extends cr.EventTarget {
  /**
   * @param {!HTMLElement} container A container.
   * @param {!ErrorBanner} errorBanner Error banner.
   * @param {!GalleryDataModel} dataModel Gallery data model.
   * @param {!cr.ui.ListSelectionModel} selectionModel List selection model.
   * @param {function()} changeToSlideModeCallback A callback to be called to
   *     change to slide mode due to activating a thumbnail.
   */
  constructor(
      container, errorBanner, dataModel, selectionModel,
      changeToSlideModeCallback) {
    super();

    /**
     * @private {!ErrorBanner}
     * @const
     */
    this.errorBanner_ = errorBanner;

    /**
     * @private {!GalleryDataModel}
     * @const
     */
    this.dataModel_ = dataModel;

    /**
     * @private {function()}
     * @const
     */
    this.changeToSlideModeCallback_ = changeToSlideModeCallback;

    this.dataModel_.addEventListener('splice', this.onSplice_.bind(this));

    this.thumbnailView_ =
        new ThumbnailView(container, dataModel, selectionModel);
    this.thumbnailView_.addEventListener(
        'thumbnail-double-click', this.onThumbnailDoubleClick_.bind(this));
  }

  /**
   * Returns name of this mode.
   * @return {string} Mode name.
   */
  getName() {
    return 'thumbnail';
  }

  /**
   * Returns title of this mode.
   * @return {string} Mode title.
   */
  getTitle() {
    return 'GALLERY_THUMBNAIL';
  }

  /**
   * Returns current sub mode.
   * @return {GallerySubMode}
   */
  getSubMode() {
    return GallerySubMode.BROWSE;
  }

  /**
   * Executes an action. An action is executed immediately since this mode does
   * not have busy state.
   */
  executeWhenReady(action) {
    action();
  }

  /**
   * @return {boolean} Always true. Toolbar is always visible.
   */
  hasActiveTool() {
    return true;
  }

  /**
   * Handles key down event.
   * @param {!Event} event An event.
   * @return {boolean} True when an event is handled.
   */
  onKeyDown(event) {
    switch (event.key) {
      case 'Enter':
        if (event.target.matches('li.thumbnail')) {
          this.changeToSlideModeCallback_();
          return true;
        }
        break;
    }

    return false;
  }

  /**
   * Handles splice event of data model.
   */
  onSplice_() {
    if (this.dataModel_.length === 0) {
      this.errorBanner_.show('GALLERY_NO_IMAGES');
    } else {
      this.errorBanner_.clear();
    }
  }

  /**
   * Handles thumbnail double click event of Thumbnail View.
   * @param {!Event} event An event.
   * @private
   */
  onThumbnailDoubleClick_(event) {
    this.changeToSlideModeCallback_();
  }

  /**
   * Shows thumbnail view.
   */
  show() {
    this.thumbnailView_.show();
  }

  /**
   * Hides thumbnail view.
   */
  hide() {
    this.thumbnailView_.hide();
  }

  /**
   * Performs thumbnail mode enter animation.
   * @param {number} index Selected thumbnail index.
   * @param {!ImageRect} rect A rect from which the transformation starts.
   */
  performEnterAnimation(index, rect) {
    this.thumbnailView_.performEnterAnimation(index, rect);
  }

  /**
   * Focus to thumbnail mode.
   */
  focus() {
    this.thumbnailView_.focus();
  }

  /**
   * Returns thumbnail rect of the index.
   * @param {number} index An index of thumbnail.
   * @return {!ClientRect} A rect of thumbnail.
   */
  getThumbnailRect(index) {
    return this.thumbnailView_.getThumbnailRect(index);
  }
}

/**
 * Thumbnail view.
 * TODO(yawano): Optimization. Remove DOMs outside of viewport, reuse them.
 * TODO(yawano): Extract ThumbnailView as a polymer element.
 */
class ThumbnailView extends cr.EventTarget {
  /**
   * @param {!HTMLElement} container A container.
   * @param {!GalleryDataModel} dataModel Gallery data model.
   * @param {!cr.ui.ListSelectionModel} selectionModel List selection model.
   */
  constructor(container, dataModel, selectionModel) {
    super();

    /**
     * @private {!HTMLElement}
     */
    this.container_ = container;

    /**
     * @private {!GalleryDataModel}
     */
    this.dataModel_ = dataModel;

    /**
     * @private {!cr.ui.ListSelectionModel}
     */
    this.selectionModel_ = selectionModel;

    /**
     * @private {!Object}
     */
    this.thumbnails_ = {};

    /**
     * @private {boolean}
     */
    this.scrolling_ = false;

    /**
     * @private {number}
     */
    this.initialScreenY_ = 0;

    /**
     * @private {number}
     */
    this.initialScrollTop_ = 0;

    /**
     * @private {number}
     */
    this.scrollbarTimeoutId_ = 0;

    /**
     * @private {!HTMLElement}
     */
    this.list_ = assertInstanceof(document.createElement('ul'), HTMLElement);
    this.container_.appendChild(this.list_);

    /**
     * @private {!HTMLElement}
     */
    this.scrollbar_ =
        assertInstanceof(document.createElement('div'), HTMLElement);
    this.scrollbar_.classList.add('scrollbar');

    /**
     * @private {!HTMLElement}
     */
    this.scrollbarThumb_ =
        assertInstanceof(document.createElement('div'), HTMLElement);
    this.scrollbarThumb_.classList.add('thumb');
    this.scrollbar_.appendChild(this.scrollbarThumb_);
    this.container_.appendChild(this.scrollbar_);

    /**
     * @private {!HTMLElement}
     */
    this.animationThumbnail_ =
        assertInstanceof(document.createElement('div'), HTMLElement);
    this.animationThumbnail_.classList.add('animation-thumbnail');
    this.container_.appendChild(this.animationThumbnail_);

    this.container_.addEventListener('scroll', this.onScroll_.bind(this));
    this.container_.addEventListener('click', this.onClick_.bind(this));
    this.container_.addEventListener('dblclick', this.onDblClick_.bind(this));

    // Set tabIndex to -1 as the container can capture keydown events.
    this.container_.tabIndex = -1;
    this.container_.addEventListener('keydown', this.onKeydown_.bind(this));

    this.scrollbarThumb_.addEventListener(
        'mousedown', this.onScrollbarThumbMouseDown_.bind(this));
    window.addEventListener('mousemove', this.onWindowMouseMove_.bind(this));
    window.addEventListener('mouseup', this.onWindowMouseUp_.bind(this));

    this.dataModel_.addEventListener('splice', this.onSplice_.bind(this));
    this.dataModel_.addEventListener('content', this.onContent_.bind(this));
    this.selectionModel_.addEventListener(
        'change', this.onSelectionChange_.bind(this));
  }

  /**
   * Shows thumbnail view.
   */
  show() {
    this.container_.hidden = false;
  }

  /**
   * Hides thumbnail view.
   */
  hide() {
    this.container_.hidden = true;
  }

  /**
   * Handles scroll bar thumb mouse down event.
   * @param {!Event} event An event.
   * @private
   */
  onScrollbarThumbMouseDown_(event) {
    this.scrolling_ = true;
    this.initialScreenY_ = event.screenY;
    this.initialScrollTop_ = this.container_.scrollTop;
  }

  /**
   * Handles mouse move event of window.
   * @param {!Event} event An event.
   * @private
   */
  onWindowMouseMove_(event) {
    if (this.scrolling_) {
      var diff = event.screenY - this.initialScreenY_;
      var scrollTop = this.initialScrollTop_ +
          (diff * this.container_.scrollHeight / this.scrollbar_.clientHeight);
      this.container_.scrollTop = scrollTop;
    }

    this.resetTimerOfScrollbar_();
  }

  /**
   * Handles mouse up event of window.
   * @param {!Event} event An event.
   * @private
   */
  onWindowMouseUp_(event) {
    this.scrolling_ = false;
    this.resetTimerOfScrollbar_();
  }

  /**
   * Handles scroll of viewport.
   * @param {!Event} event An event.
   * @private
   */
  onScroll_(event) {
    this.updateScrollBar_();
  }

  /**
   * Moves selection to specified direction.
   * @param {string} direction Direction. Should be 'ArrowLeft', 'ArrowRight',
   *     'ArrowUp', or 'ArrowDown'.
   * @param {boolean} selectRange True to perform range selection.
   * @private
   */
  moveSelection_(direction, selectRange) {
    var step;
    if ((direction === 'ArrowLeft' && !isRTL()) ||
        (direction === 'ArrowRight' && isRTL()) || (direction === 'ArrowUp')) {
      step = -1;
    } else if (
        (direction === 'ArrowRight' && !isRTL()) ||
        (direction === 'ArrowLeft' && isRTL()) || (direction === 'ArrowDown')) {
      step = 1;
    } else {
      assertNotReached();
    }

    var vertical = direction === 'ArrowUp' || direction === 'ArrowDown';
    var baseIndex = this.selectionModel_.leadIndex !== -1 ?
        this.selectionModel_.leadIndex :
        this.selectionModel_.selectedIndex;
    var baseRect = this.getThumbnailRect(baseIndex);
    var baseCenter = baseRect.left + baseRect.width / 2;
    var minHorizontalGap = Number.MAX_VALUE;
    var index = null;

    for (var i = baseIndex + step; 0 <= i && i < this.dataModel_.length;
         i += step) {
      // Skip error thumbnail.
      var thumbnail = this.getThumbnailAt_(i);
      if (thumbnail.isError()) {
        continue;
      }

      // Look for the horizontally nearest item if it is vertical move.
      // Otherwise it just use the current i.
      if (vertical) {
        var rect = this.getThumbnailRect(i);
        var verticalGap = Math.abs(baseRect.top - rect.top);
        if (verticalGap === 0) {
          continue;
        } else if (verticalGap >= ThumbnailView.ROW_HEIGHT * 2) {
          break;
        }
        // If centerGap - rect.width / 2 < 0, the image is located just
        // above the center point of base image since baseCenter is in the range
        // (rect.left, rect.right). In this case we use 0 as distance. Otherwise
        // centerGap - rect.width / 2 equals to the distance between baseCenter
        // and either of rect.left or rect.right that is closer to centerGap.
        var centerGap = Math.abs(baseCenter - (rect.left + rect.width / 2));
        var horizontalGap = Math.max(centerGap - rect.width / 2, 0);
        if (horizontalGap < minHorizontalGap) {
          minHorizontalGap = horizontalGap;
          index = i;
        }
      } else {
        index = i;
        break;
      }
    }

    if (index !== null) {
      // Move selection.
      if (selectRange && this.selectionModel_.anchorIndex !== -1) {
        // Since anchorIndex will be set to 0 by unselectAll, copy the value.
        var anchorIndex = this.selectionModel_.anchorIndex;
        this.selectionModel_.unselectAll();
        this.selectionModel_.selectRange(anchorIndex, index);
        this.selectionModel_.anchorIndex = anchorIndex;
      } else {
        this.selectionModel_.selectedIndex = index;
        this.selectionModel_.anchorIndex = index;
      }

      this.selectionModel_.leadIndex = index;
      this.scrollTo_(index);
    }
  }

  /**
   * Scrolls viewport to show the thumbnail of the index.
   * @param {number} index Index of a thumbnail which becomes to appear in the
   *     viewport.
   * @private
   *
   * TODO(yawano): Add scroll animation.
   */
  scrollTo_(index) {
    var thumbnailRect = this.getThumbnailRect(index);

    if (thumbnailRect.top - ThumbnailView.MARGIN < ImageEditorToolbar.HEIGHT) {
      this.container_.scrollTop -=
          ImageEditorToolbar.HEIGHT - thumbnailRect.top + ThumbnailView.MARGIN;
    } else if (
        thumbnailRect.bottom + ThumbnailView.MARGIN >
        this.container_.clientHeight) {
      this.container_.scrollTop += thumbnailRect.bottom + ThumbnailView.MARGIN -
          this.container_.clientHeight;
    }
  }

  /**
   * Updates scroll bar.
   * @private
   */
  updateScrollBar_() {
    var scrollTop = this.container_.scrollTop;
    var scrollHeight = this.container_.scrollHeight;
    var clientHeight = this.container_.clientHeight;

    // If viewport is not long enough to scroll, do not show scrollbar.
    if (scrollHeight <= clientHeight) {
      this.scrollbar_.hidden = true;
      return;
    }

    this.scrollbar_.hidden = false;

    var thumbHeight =
        ~~(this.scrollbar_.clientHeight * clientHeight / scrollHeight);
    var thumbTop = ~~(scrollTop * this.scrollbar_.clientHeight / scrollHeight);

    this.scrollbarThumb_.style.height = thumbHeight + 'px';
    this.scrollbarThumb_.style.marginTop = thumbTop + 'px';

    this.resetTimerOfScrollbar_();
  }

  /**
   * Resets timer to fade out scrollbar. If scrollbar is already faded-out, this
   * method makes it visible and set timeout. If user is scrolling, this method
   * just clears existing timer.
   * @private
   */
  resetTimerOfScrollbar_() {
    this.scrollbar_.classList.toggle('transparent', false);

    if (this.scrollbarTimeoutId_) {
      clearTimeout(this.scrollbarTimeoutId_);
      this.scrollbarTimeoutId_ = 0;
    }

    // If user is scrolling, do not set timeout.
    if (this.scrolling_) {
      return;
    }

    this.scrollbarTimeoutId_ = setTimeout(function() {
      this.scrollbarTimeoutId_ = 0;
      this.scrollbar_.classList.toggle('transparent', true);
    }.bind(this), ThumbnailView.SCROLLBAR_TIMEOUT);
  }

  /**
   * Handles splice event of data model.
   * @param {!Event} event An event.
   * @private
   */
  onSplice_(event) {
    if (event.removed) {
      for (var i = 0; i < event.removed.length; i++) {
        this.remove_(event.removed[i]);
      }
    }

    if (event.added && event.added.length > 0) {
      // Get a thumbnail before which new thumbnail is inserted.
      var insertBefore = null;
      var galleryItem = this.dataModel_.item(event.index + event.added.length);
      if (galleryItem) {
        insertBefore = this.thumbnails_[galleryItem.getEntry().toURL()];
      }

      for (var i = 0; i < event.added.length; i++) {
        this.insert_(event.added[i], insertBefore);
      }
    }
  }

  /**
   * Handles content event of data model.
   * @param {!Event} event An event.
   * @private
   */
  onContent_(event) {
    var galleryItem = event.item;
    var oldEntry = event.oldEntry;
    var thumbnail = this.thumbnails_[oldEntry.toURL()];
    if (thumbnail) {
      // Update map.
      delete this.thumbnails_[oldEntry.toURL()];
      this.thumbnails_[galleryItem.getEntry().toURL()] = thumbnail;

      thumbnail.update();
    }
  }

  /**
   * Handles selection change event.
   * @param {!Event} event An event.
   * @private
   */
  onSelectionChange_(event) {
    var changes = event.changes;
    var lastSelectedThumbnail = null;

    for (var i = 0; i < changes.length; i++) {
      var change = changes[i];

      var galleryItem = this.dataModel_.item(change.index);
      if (!galleryItem) {
        continue;
      }

      var thumbnail = this.thumbnails_[galleryItem.getEntry().toURL()];
      if (!thumbnail) {
        continue;
      }

      thumbnail.setSelected(change.selected);

      // We should not focus to error thumbnail.
      if (change.selected && !thumbnail.isError()) {
        lastSelectedThumbnail = thumbnail;
      }
    }

    // If new item is selected, focus to it. If multiple thumbnails are
    // selected, focus to the last one.
    if (lastSelectedThumbnail) {
      lastSelectedThumbnail.getContainer().focus();
    }
  }

  /**
   * Handles click event.
   * @param {!Event} event An event.
   * @private
   */
  onClick_(event) {
    var target = event.target;
    if (target.matches('.selection.frame')) {
      var selectionMode = ThumbnailView.SelectionMode.SINGLE;
      if (event.ctrlKey) {
        selectionMode = ThumbnailView.SelectionMode.MULTIPLE;
      }
      if (event.shiftKey) {
        selectionMode = ThumbnailView.SelectionMode.RANGE;
      }

      this.selectByThumbnail_(target.parentNode.getThumbnail(), selectionMode);
      return;
    }

    // If empty space is clicked, unselect current selection.
    this.selectionModel_.unselectAll();
  }

  /**
   * Handles double click event.
   * @param {!Event} event An event.
   * @private
   */
  onDblClick_(event) {
    var target = event.target;
    if (target.matches('.selection.frame')) {
      this.selectByThumbnail_(
          target.parentNode.getThumbnail(), ThumbnailView.SelectionMode.SINGLE);
      var thumbnailDoubleClickEvent = new Event('thumbnail-double-click');
      this.dispatchEvent(thumbnailDoubleClickEvent);
    }
  }

  /**
   * Handles keydown event.
   * @param {!Event} event
   * @private
   */
  onKeydown_(event) {
    var keyString = util.getKeyModifiers(event) + event.key;

    switch (keyString) {
      case 'ArrowRight':
      case 'ArrowLeft':
      case 'ArrowUp':
      case 'ArrowDown':
      case 'Shift-ArrowRight':
      case 'Shift-ArrowLeft':
      case 'Shift-ArrowUp':
      case 'Shift-ArrowDown':
        this.moveSelection_(event.key, event.shiftKey);
        event.stopPropagation();
        break;
      case 'Ctrl-a':  // Crtl+A
        this.selectionModel_.selectAll();
        event.stopPropagation();
        break;
    }
  }

  /**
   * Selects a thumbnail.
   * @param {!ThumbnailView.Thumbnail} thumbnail Thumbnail to be selected.
   * @param {ThumbnailView.SelectionMode} selectionMode
   * @private
   */
  selectByThumbnail_(thumbnail, selectionMode) {
    var index = this.dataModel_.indexOf(thumbnail.getGalleryItem());

    if (selectionMode === ThumbnailView.SelectionMode.SINGLE) {
      this.selectionModel_.unselectAll();
      this.selectionModel_.setIndexSelected(index, true);
      this.selectionModel_.anchorIndex = index;
    } else if (selectionMode === ThumbnailView.SelectionMode.MULTIPLE) {
      this.selectionModel_.setIndexSelected(
          index, this.selectionModel_.selectedIndexes.indexOf(index) === -1);
    } else if (selectionMode === ThumbnailView.SelectionMode.RANGE) {
      var leadIndex = this.selectionModel_.leadIndex;
      if (leadIndex < 0) {
        leadIndex = 0;
      }
      this.selectionModel_.unselectAll();
      this.selectionModel_.selectRange(leadIndex, index);
    } else {
      assertNotReached();
    }

    this.selectionModel_.leadIndex = index;
  }

  /**
   * Inserts an item.
   * @param {!GalleryItem} galleryItem A gallery item.
   * @param {!ThumbnailView.Thumbnail} insertBefore A thumbnail before which new
   *     thumbnail is inserted. Set null for adding at the end of the list.
   * @private
   */
  insert_(galleryItem, insertBefore) {
    var thumbnail = new ThumbnailView.Thumbnail(galleryItem);
    this.thumbnails_[galleryItem.getEntry().toURL()] = thumbnail;
    if (insertBefore) {
      this.list_.insertBefore(
          thumbnail.getContainer(), insertBefore.getContainer());
    } else {
      this.list_.appendChild(thumbnail.getContainer());
    }

    // Set selection state.
    var index = this.dataModel_.indexOf(galleryItem);
    thumbnail.setSelected(this.selectionModel_.getIndexSelected(index));

    this.updateScrollBar_();
  }

  /**
   * Removes an item.
   * @param {!GalleryItem} galleryItem A gallery item.
   * @private
   */
  remove_(galleryItem) {
    var thumbnail = this.thumbnails_[galleryItem.getEntry().toURL()];
    this.list_.removeChild(thumbnail.getContainer());
    delete this.thumbnails_[galleryItem.getEntry().toURL()];
  }

  /**
   * Returns thumbnail instance at specified index.
   * @param {number} index Index of the thumbnail.
   * @return {!ThumbnailView.Thumbnail} Thumbnail at the index.
   * @private
   */
  getThumbnailAt_(index) {
    var galleryItem = this.dataModel_.item(index);
    return this.thumbnails_[galleryItem.getEntry().toURL()];
  }

  /**
   * Returns a rect of the specified thumbnail.
   * @param {number} index An index of the thumbnail.
   * @return {!ClientRect} Rect of the thumbnail.
   */
  getThumbnailRect(index) {
    var thumbnail = this.getThumbnailAt_(index);
    return thumbnail.getContainer().getBoundingClientRect();
  }

  /**
   * Performs enter animation.
   * @param {number} index Index of the thumbnail which is animated.
   * @param {!ImageRect} rect A rect from which the transformation starts.
   *
   * TODO(yawano): Consider to move this logic to thumbnail mode.
   */
  performEnterAnimation(index, rect) {
    this.scrollTo_(index);
    this.updateScrollBar_();

    var thumbnailRect = this.getThumbnailRect(index);
    var thumbnail = this.getThumbnailAt_(index);

    // If thumbnail is not loaded yet or failed to load, do not perform
    // animation.
    if (!thumbnail.getBackgroundImage() || thumbnail.isError()) {
      return;
    }

    // Hide animating thumbnail.
    thumbnail.setTransparent(true);

    this.animationThumbnail_.style.backgroundImage =
        thumbnail.getBackgroundImage();
    this.animationThumbnail_.classList.add('animating');
    this.animationThumbnail_.width = thumbnail.getWidth();
    this.animationThumbnail_.height = ThumbnailView.ROW_HEIGHT;

    var animationPlayer = this.animationThumbnail_.animate(
        [
          {
            height: rect.height + 'px',
            left: rect.left + 'px',
            top: rect.top + 'px',
            width: rect.width + 'px',
            offset: 0,
            easing: 'linear'
          },
          {
            height: thumbnailRect.height + 'px',
            left: thumbnailRect.left + 'px',
            top: thumbnailRect.top + 'px',
            width: thumbnailRect.width + 'px',
            offset: 1
          }
        ],
        250);

    animationPlayer.addEventListener('finish', function() {
      this.animationThumbnail_.classList.remove('animating');
      thumbnail.setTransparent(false);
    }.bind(this));
  }

  /**
   * Focus to thumbnail view. If an item is selected, focus to it.
   */
  focus() {
    if (this.selectionModel_.selectedIndexes.length === 0) {
      this.container_.focus();
      return;
    }

    var index = this.selectionModel_.leadIndex !== -1 ?
        this.selectionModel_.leadIndex :
        this.selectionModel_.selectedIndex;
    var thumbnail = this.getThumbnailAt_(index);
    thumbnail.getContainer().focus();
  }
}

/**
 * Row height.
 * @const {number}
 *
 * TODO(yawano): Change so that Gallery adjust row height depending on image
 *     collection and window size to cover viewport as much as possible.
 */
ThumbnailView.ROW_HEIGHT = 160;  // px

/**
 * Margins between thumbnails. This should be synced with CSS.
 * @const {number}
 */
ThumbnailView.MARGIN = 4;  // px

/**
 * Timeout to fade out scrollbar.
 * @const {number}
 */
ThumbnailView.SCROLLBAR_TIMEOUT = 1500;  // ms

/**
 * Selection mode.
 * @enum {string}
 */
ThumbnailView.SelectionMode = {
  SINGLE: 'single',
  MULTIPLE: 'multiple',
  RANGE: 'range'
};

/**
 * Thumbnail.
 */
ThumbnailView.Thumbnail = class {
  /**
   * @param {!GalleryItem} galleryItem A gallery item.
   */
  constructor(galleryItem) {
    /**
     * @private {!GalleryItem}
     */
    this.galleryItem_ = galleryItem;

    /**
     * @private {boolean}
     */
    this.selected_ = false;

    /**
     * @private {ThumbnailLoader}
     */
    this.thumbnailLoader_ = null;

    /**
     * @private {number}
     */
    this.thumbnailLoadRequestId_ = 0;

    /**
     * @private {number}
     */
    this.width_ = 0;

    /**
     * @private {*}
     */
    this.error_ = null;

    /**
     * @private {!HTMLElement}
     */
    this.container_ =
        assertInstanceof(document.createElement('li'), HTMLElement);
    this.container_.tabIndex = 1;
    this.container_.classList.add('thumbnail');

    /**
     * @private {!HTMLElement}
     */
    this.imageFrame_ =
        assertInstanceof(document.createElement('div'), HTMLElement);
    this.imageFrame_.classList.add('image', 'frame');

    if (FileType.isVideo(galleryItem.getEntry())) {
      this.imageFrame_.classList.add('video');
    }

    this.container_.appendChild(this.imageFrame_);

    /**
     * @private {!HTMLElement}
     */
    this.selectionFrame_ =
        assertInstanceof(document.createElement('div'), HTMLElement);
    this.selectionFrame_.classList.add('selection', 'frame');
    this.container_.appendChild(this.selectionFrame_);

    this.container_.style.height = ThumbnailView.ROW_HEIGHT + 'px';
    this.container_.getThumbnail = function(thumbnail) {
      return thumbnail;
    }.bind(null, this);

    this.update();
  }

  /**
   * Returns a gallery item.
   * @return {!GalleryItem} A gallery item.
   */
  getGalleryItem() {
    return this.galleryItem_;
  }

  /**
   * Change selection state of this thumbnail.
   * @param {boolean} selected True to make this thumbnail selected.
   */
  setSelected(selected) {
    this.selected_ = selected;
    this.container_.classList.toggle('selected', selected);
  }

  /**
   * Returns a container.
   * @return {!HTMLElement} A container.
   */
  getContainer() {
    return this.container_;
  }

  /**
   * Sets this thumbnail as transparent.
   * @param {boolean} transparent True to make this thumbnail transparent.
   */
  setTransparent(transparent) {
    this.container_.classList.toggle('transparent', transparent);
  }

  /**
   * Returns width of this thumbnail.
   * @return {number} Width of this thumbnail.
   */
  getWidth() {
    return this.width_;
  }

  /**
   * Returns whether this has failed to load thumbnail or not.
   * @return {boolean} True if thumbnail load has failed.
   */
  isError() {
    return !!this.error_;
  }

  /**
   * Sets error.
   * @param {*} error Error object. Set null to clear error.
   * @private
   */
  setError_(error) {
    this.error_ = error;
    this.container_.classList.toggle('error', !!this.error_);
  }

  /**
   * Sets width of this thumbnail.
   * @param {number} width Width.
   * @private
   */
  setWidth_(width) {
    if (this.width_ === width) {
      return;
    }

    this.width_ = width;
    this.container_.style.width = this.width_ + 'px';
  }

  /**
   * Returns background image style of this thumbnail.
   * @return {string} Background image.
   */
  getBackgroundImage() {
    return this.imageFrame_.style.backgroundImage;
  }

  /**
   * Updates thumbnail.
   */
  update() {
    // Update title.
    this.container_.setAttribute('title', this.galleryItem_.getFileName());

    // Calculate and set width.
    var metadata = this.galleryItem_.getMetadataItem();
    if (!metadata) {
      this.setWidth_(ThumbnailView.ROW_HEIGHT);
      return;
    }

    var rotated = metadata.imageRotation % 2 === 1;
    var imageWidth = rotated ? metadata.imageHeight : metadata.imageWidth;
    var imageHeight = rotated ? metadata.imageWidth : metadata.imageHeight;
    this.setWidth_(~~(imageWidth * ThumbnailView.ROW_HEIGHT / imageHeight));

    // Set thumbnail.
    var thumbnailMetadata = this.galleryItem_.getThumbnailMetadataItem();
    if (!thumbnailMetadata) {
      return;
    }

    this.loadAndSetThumbnail_(
            thumbnailMetadata, false /* do not force to generate thumbnail */)
        .then(function(result) {
          if (!result || result.height >= ThumbnailView.ROW_HEIGHT ||
              result.loadTarget === ThumbnailLoader.LoadTarget.FILE_ENTRY ||
              metadata.imageHeight <= ThumbnailView.ROW_HEIGHT ||
              (thumbnailMetadata.external &&
               !thumbnailMetadata.external.present)) {
            return;
          }

          // If thumbnail height is lower than ThumbnailView.ROW_HEIGHT,
          // generate thumbnail from image content.
          this.loadAndSetThumbnail_(
              thumbnailMetadata, true /* force to generate thumbnail */);
        }.bind(this));
  }

  /**
   * Loads thumbnail and sets it.
   * @param {!ThumbnailMetadataItem} thumbnailMetadata
   * @param {boolean} forceToGenerate True to force generating thumbnail from
   *     image content.
   * @return {!Promise<?{height:number,
   *     loadTarget:?ThumbnailLoader.LoadTarget}>} null is returned for error
   *     case.
   * @private
   */
  loadAndSetThumbnail_(thumbnailMetadata, forceToGenerate) {
    this.thumbnailLoadRequestId_++;

    var loadTargets = forceToGenerate ?
        [ThumbnailLoader.LoadTarget.FILE_ENTRY] :
        undefined /* default value */;

    this.thumbnailLoader_ = new ThumbnailLoader(
        this.galleryItem_.getEntry(), undefined /* opt_loaderType */,
        thumbnailMetadata, undefined /* opt_mediaType */, loadTargets);
    return this.thumbnailLoader_.loadAsDataUrl(ThumbnailLoader.FillMode.FIT)
        .then(function(requestId, result) {
          // Discard the result of old request.
          if (requestId !== this.thumbnailLoadRequestId_) {
            return null;
          }

          // Update width by using the width of actual data.
          this.setWidth_(
              ~~(result.width * ThumbnailView.ROW_HEIGHT / result.height));

          this.imageFrame_.style.backgroundImage = 'url(' + result.data + ')';
          this.setError_(null);

          return {
            height: result.height,
            loadTarget: this.thumbnailLoader_.getLoadTarget()
          };
        }.bind(this, this.thumbnailLoadRequestId_))
        .catch(function(requestId, error) {
          if (requestId !== this.thumbnailLoadRequestId_) {
            return null;
          }

          this.setError_(error);
          return null;
        }.bind(this, this.thumbnailLoadRequestId_));
  }
};
