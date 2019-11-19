// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Slide mode displays a single image and has a set of controls to navigate
 * between the images and to edit an image.
 */
class SlideMode extends cr.EventTarget {
  /**
   * @param {!HTMLElement} container Main container element.
   * @param {!HTMLElement} content Content container element.
   * @param {!HTMLElement} topToolbar Top toolbar element.
   * @param {!HTMLElement} bottomToolbar Toolbar element.
   * @param {!ImageEditorPrompt} prompt Prompt.
   * @param {!ErrorBanner} errorBanner Error banner.
   * @param {!GalleryDataModel} dataModel Data model.
   * @param {!cr.ui.ListSelectionModel} selectionModel Selection model.
   * @param {!MetadataModel} metadataModel
   * @param {!ThumbnailModel} thumbnailModel
   * @param {!{appWindow: chrome.app.window.AppWindow, readonlyDirName: string,
   *     displayStringFunction: function(), loadTimeData: Object}} context
   * Context.
   * @param {!VolumeManager} volumeManager Volume manager.
   * @param {function(function())} toggleMode Function to toggle the Gallery
   *     mode.
   * @param {function(string):string} displayStringFunction String formatting
   *     function.
   * @param {!DimmableUIController} dimmableUIController Dimmable UI controller.
   */
  constructor(
      container, content, topToolbar, bottomToolbar, prompt, errorBanner,
      dataModel, selectionModel, metadataModel, thumbnailModel, context,
      volumeManager, toggleMode, displayStringFunction, dimmableUIController) {
    super();

    /**
     * @type {!HTMLElement}
     * @private
     * @const
     */
    this.container_ = container;

    /**
     * @type {!Document}
     * @private
     * @const
     */
    this.document_ = assert(container.ownerDocument);

    /**
     * @type {!HTMLElement}
     * @const
     */
    this.content = content;

    /**
     * @type {!HTMLElement}
     * @private
     * @const
     */
    this.topToolbar_ = topToolbar;

    /**
     * @type {!HTMLElement}
     * @private
     * @const
     */
    this.bottomToolbar_ = bottomToolbar;

    /**
     * @type {!ImageEditorPrompt}
     * @private
     * @const
     */
    this.prompt_ = prompt;

    /**
     * @type {!ErrorBanner}
     * @private
     * @const
     */
    this.errorBanner_ = errorBanner;

    /**
     * @type {!GalleryDataModel}
     * @private
     * @const
     */
    this.dataModel_ = dataModel;

    /**
     * @type {!cr.ui.ListSelectionModel}
     * @private
     * @const
     */
    this.selectionModel_ = selectionModel;

    /**
     * @type {{appWindow: chrome.app.window.AppWindow, readonlyDirName: string,
     * displayStringFunction: function(), loadTimeData: Object}}
     * @private
     * @const
     */
    this.context_ = context;

    /**
     * @type {!VolumeManager}
     * @private
     * @const
     */
    this.volumeManager_ = volumeManager;

    /**
     * @type {function(function())}
     * @private
     * @const
     */
    this.toggleMode_ = toggleMode;

    /**
     * @type {function(string):string}
     * @private
     * @const
     */
    this.displayStringFunction_ = displayStringFunction;

    /**
     * @private {!DimmableUIController}
     * @const
     */
    this.dimmableUIController_ = dimmableUIController;

    /**
     * @type {function(this:SlideMode)}
     * @private
     * @const
     */
    this.onSelectionBound_ = this.onSelection_.bind(this);

    /**
     * @type {function(this:SlideMode,!Event)}
     * @private
     * @const
     */
    this.onSpliceBound_ = this.onSplice_.bind(this);

    /**
     * Unique numeric key, incremented per each load attempt used to discard
     * old attempts. This can happen especially when changing selection fast or
     * Internet connection is slow.
     *
     * @type {number}
     * @private
     */
    this.currentUniqueKey_ = 0;

    /**
     * @type {number}
     * @private
     */
    this.sequenceDirection_ = 0;

    /**
     * @type {number}
     * @private
     */
    this.sequenceLength_ = 0;

    /**
     * @type {Array<number>}
     * @private
     */
    this.savedSelection_ = null;

    /**
     * @type {GalleryItem}
     * @private
     */
    this.displayedItem_ = null;

    /**
     * @type {?number}
     * @private
     */
    this.slideHint_ = null;

    /**
     * @type {boolean}
     * @private
     */
    this.active_ = false;

    /**
     * @private {GallerySubMode}
     */
    this.subMode_ = GallerySubMode.BROWSE;

    /**
     * @type {boolean}
     * @private
     */
    this.leaveAfterSlideshow_ = false;

    /**
     * @type {boolean}
     * @private
     */
    this.fullscreenBeforeSlideshow_ = false;

    /**
     * @type {?number}
     * @private
     */
    this.slideShowTimeout_ = null;

    /**
     * @private {string|undefined}
     */
    this.loadingItemUrl_ = undefined;

    /**
     * @private {number}
     */
    this.progressBarTimer_ = 0;

    /**
     * @type {?number}
     * @private
     */
    this.spinnerTimer_ = null;

    window.addEventListener('resize', this.onResize_.bind(this));

    // ----------------------------------------------------------------
    // Initializes the UI.

    /**
     * Container for displayed image.
     * @type {!HTMLElement}
     * @private
     * @const
     */
    this.imageContainer_ = util.createChild(
        queryRequiredElement('.content', this.document_), 'image-container');

    this.document_.addEventListener('click', this.onDocumentClick_.bind(this));

    /**
     * Overwrite options and info bubble.
     * @type {!HTMLElement}
     * @private
     * @const
     */
    this.options_ = queryRequiredElement('.options', this.bottomToolbar_);

    /**
     * @type {!HTMLElement}
     * @private
     * @const
     */
    this.savedLabel_ = queryRequiredElement('.saved', this.options_);

    /**
     * @private {!Element}
     * @const
     */
    this.overwriteOriginalCheckbox_ = /** @type {!Element} */
        (queryRequiredElement('.overwrite-original', this.options_));
    this.overwriteOriginalCheckbox_.addEventListener(
        'change', this.onOverwriteOriginalCheckboxChanged_.bind(this));

    /**
     * @private {!FilesToast}
     * @const
     */
    this.filesToast_ = /** @type {!FilesToast} */
        (queryRequiredElement('files-toast'));

    /**
     * @private {!HTMLElement}
     * @const
     */
    this.bubble_ = queryRequiredElement('.bubble', this.bottomToolbar_);

    var bubbleContent = queryRequiredElement('.content', this.bubble_);
    // GALLERY_OVERWRITE_BUBBLE contains <br> tag inside message.
    bubbleContent.innerHTML = strf('GALLERY_OVERWRITE_BUBBLE');

    var bubbleClose = queryRequiredElement('.close-x', this.bubble_);
    bubbleClose.addEventListener('click', this.onCloseBubble_.bind(this));

    /**
     * Ribbon and related controls.
     * @type {!HTMLElement}
     * @private
     * @const
     */
    this.arrowBox_ = util.createChild(this.container_, 'arrow-box');

    /**
     * @type {!HTMLElement}
     * @private
     * @const
     */
    this.arrowLeft_ =
        util.createChild(this.arrowBox_, 'arrow left tool dimmable');
    this.arrowLeft_.addEventListener(
        'click', this.advanceManually.bind(this, -1));
    util.createChild(this.arrowLeft_);

    /**
     * @type {!HTMLElement}
     * @private
     * @const
     */
    this.arrowRight_ =
        util.createChild(this.arrowBox_, 'arrow right tool dimmable');
    this.arrowRight_.addEventListener(
        'click', this.advanceManually.bind(this, 1));
    util.createChild(this.arrowRight_);

    /**
     * @type {!HTMLElement}
     * @private
     * @const
     */
    this.ribbonSpacer_ =
        queryRequiredElement('.ribbon-spacer', this.bottomToolbar_);

    /**
     * @type {!Ribbon}
     * @private
     * @const
     */
    this.ribbon_ = new Ribbon(
        this.document_, window, this.dataModel_, this.selectionModel_,
        thumbnailModel);
    this.ribbonSpacer_.appendChild(this.ribbon_);

    util.createChild(this.container_, 'spinner');

    /**
     * @type {!HTMLElement}
     * @const
     */
    var slideShowButton =
        queryRequiredElement('button.slideshow', this.topToolbar_);
    slideShowButton.addEventListener(
        'click',
        this.startSlideshow.bind(this, SlideMode.SLIDESHOW_INTERVAL_FIRST));

    /**
     * @private {!PaperProgressElement}
     * @const
     */
    this.progressBar_ = /** @type {!PaperProgressElement} */
        (queryRequiredElement('#progress-bar', document));
    chrome.fileManagerPrivate.onFileTransfersUpdated.addListener(
        this.updateProgressBar_.bind(this));

    /**
     * @type {!HTMLElement}
     * @const
     */
    var slideShowToolbar =
        util.createChild(this.container_, 'tool slideshow-toolbar');

    // Play & Pause Button.
    /**
     * @type {!HTMLElement}
     * @private
     * @const
     */
    this.slideshowPlay_ = util.createChild(slideShowToolbar, 'slideshow-play');
    this.slideshowPlay_.addEventListener(
        'click', this.toggleSlideshowPause_.bind(this));
    util.createChild(slideShowToolbar, 'slideshow-end')
        .addEventListener('click', this.stopSlideshow_.bind(this));

    // Editor.
    /**
     * @type {!HTMLElement}
     * @private
     * @const
     */
    this.editButton_ = queryRequiredElement('button.edit', this.topToolbar_);
    GalleryUtil.decorateMouseFocusHandling(this.editButton_);
    this.editButton_.addEventListener('click', this.toggleEditor.bind(this));

    /**
     * @private {!FilesToggleRipple}
     * @const
     */
    this.editButtonToggleRipple_ = /** @type {!FilesToggleRipple} */
        (assert(this.editButton_.querySelector('files-toggle-ripple')));

    /**
     * @type {!HTMLElement}
     * @private
     * @const
     */
    this.printButton_ = queryRequiredElement('button.print', this.topToolbar_);
    GalleryUtil.decorateMouseFocusHandling(this.printButton_);
    this.printButton_.addEventListener('click', this.print_.bind(this));

    /**
     * @type {!HTMLElement}
     * @private
     * @const
     */
    this.editBarSpacer_ =
        queryRequiredElement('.edit-bar-spacer', this.bottomToolbar_);

    /**
     * @type {!HTMLElement}
     * @private
     * @const
     */
    this.editBarMain_ = util.createChild(this.editBarSpacer_, 'edit-main');

    /**
     * @type {!HTMLElement}
     * @private
     * @const
     */
    this.editBarMode_ =
        /** @type {!HTMLElement} */ (document.createElement('div'));
    this.editBarMode_.className = 'edit-modal';
    // Edit modal bar should be inserted before the bottom toolbar to make the
    // tab order and visual position consistent.
    this.container_.insertBefore(
        this.editBarMode_, document.querySelector('#bottom-toolbar'));

    /**
     * @type {!HTMLElement}
     * @private
     * @const
     */
    this.editBarModeWrapper_ =
        util.createChild(this.editBarMode_, 'edit-modal-wrapper dimmable');
    this.editBarModeWrapper_.hidden = true;

    /**
     * Objects supporting image display and editing.
     * @type {!Viewport}
     * @private
     * @const
     */
    this.viewport_ = new Viewport(window);
    this.viewport_.addEventListener(
        'resize', this.onViewportResize_.bind(this));

    /**
     * @type {!ImageView}
     * @private
     * @const
     */
    this.imageView_ =
        new ImageView(this.imageContainer_, this.viewport_, metadataModel);

    /**
     * @type {!ImageEditor}
     * @private
     * @const
     */
    this.editor_ = new ImageEditor(
        this.viewport_, this.imageView_, this.prompt_, {
          root: this.container_,
          image: this.imageContainer_,
          toolbar: this.editBarMain_,
          mode: this.editBarModeWrapper_
        },
        SlideMode.EDITOR_MODES, this.displayStringFunction_);
    this.editor_.addEventListener(
        'exit-clicked', this.onExitClicked_.bind(this));

    /**
     * @type {!TouchHandler}
     * @private
     * @const
     */
    this.touchHandlers_ = new TouchHandler(this.imageContainer_, this);
  }

  /**
   * Returns editor warning message if it should be shown.
   * @param {!GalleryItem} item
   * @param {string} readonlyDirName Name of read only volume. Pass empty string
   *     if volume is writable.
   * @param {!DirectoryEntry} fallbackSaveDirectory
   * @return {!Promise<?string>} Warning message. null if no warning message
   *     should be shown.
   */
  static getEditorWarningMessage(item, readonlyDirName, fallbackSaveDirectory) {
    var isReadOnlyVolume = !!readonlyDirName;
    var isWritableFormat = item.isWritableFormat();

    if (isReadOnlyVolume && !isWritableFormat) {
      return item.getCopyName(fallbackSaveDirectory).then(function(copyName) {
        return strf(
            'GALLERY_READONLY_AND_NON_WRITABLE_FORMAT_WARNING', readonlyDirName,
            copyName);
      });
    } else if (isReadOnlyVolume) {
      return Promise.resolve(/** @type {?string} */
                             (strf(
                                 'GALLERY_READONLY_WARNING', readonlyDirName)));
    } else if (!isWritableFormat) {
      var entry = item.getEntry();
      return new Promise(entry.getParent.bind(entry))
          .then(function(parentDir) {
            return item.getCopyName(parentDir);
          })
          .then(function(copyName) {
            return strf('GALLERY_NON_WRITABLE_FORMAT_WARNING', copyName);
          });
    } else {
      return Promise.resolve(/** @type {?string} */ (null));
    }
  }

  /**
   * Handles exit-clicked event.
   * @private
   */
  onExitClicked_() {
    if (this.isEditing()) {
      this.toggleEditor();
    }
  }

  /**
   * @return {string} Mode name.
   */
  getName() {
    return 'slide';
  }

  /**
   * @return {string} Mode title.
   */
  getTitle() {
    return 'GALLERY_SLIDE';
  }

  /**
   * @return {!Viewport} Viewport.
   */
  getViewport() {
    return this.viewport_;
  }

  /**
   * Load items, display the selected item.
   * @param {ImageRect} zoomFromRect Rectangle for zoom effect.
   * @param {function()} displayCallback Called when the image is displayed.
   * @param {function()} loadCallback Called when the image is displayed.
   */
  enter(zoomFromRect, displayCallback, loadCallback) {
    this.sequenceDirection_ = 0;
    this.sequenceLength_ = 0;

    // The latest |leave| call might have left the image animating. Remove it.
    this.unloadImage_();
    this.errorBanner_.clear();

    new Promise(function(fulfill) {
      // If the items are empty, just show the error message.
      if (this.getItemCount_() === 0) {
        this.displayedItem_ = null;
        this.errorBanner_.show('GALLERY_NO_IMAGES');
        fulfill();
        return;
      }

      // Remember the selection if it is empty or multiple. It will be restored
      // in |leave| if the user did not changing the selection manually.
      var currentSelection = this.selectionModel_.selectedIndexes;
      if (currentSelection.length === 1) {
        this.savedSelection_ = null;
      } else {
        this.savedSelection_ = currentSelection;
      }

      // Ensure valid single selection.
      // Note that the SlideMode object is not listening to selection change
      // yet.
      this.select(Math.max(0, this.getSelectedIndex()));

      // Show the selected item ASAP, then complete the initialization
      // (loading the ribbon thumbnails can take some time).
      var selectedItem = this.getSelectedItem();
      this.displayedItem_ = selectedItem;

      // Load the image of the item.
      this.loadItem_(
          assert(selectedItem),
          zoomFromRect ? this.imageView_.createZoomEffect(zoomFromRect) :
                         new ImageView.Effect.None(),
          displayCallback, function(loadType, delay) {
            fulfill(delay);
          });
    }.bind(this))
        .then(function(delay) {
          // Turn the mode active.
          this.active_ = true;
          ImageUtil.setAttribute(
              this.arrowBox_, 'active', this.getItemCount_() > 1);
          this.ribbon_.enable();

          // Register handlers.
          this.selectionModel_.addEventListener(
              'change', this.onSelectionBound_);
          this.dataModel_.addEventListener('splice', this.onSpliceBound_);
          this.touchHandlers_.enabled = true;

          // Wait 1000ms after the animation is done, then prefetch the next
          // image.
          this.requestPrefetch(1, delay + 1000);

          // Call load callback.
          if (loadCallback) {
            loadCallback();
          }
        }.bind(this))
        .catch(function(error) {
          console.error(error.stack, error);
        });
  }

  /**
   * Leave the mode.
   * @param {ImageRect} zoomToRect Rectangle for zoom effect.
   * @param {function()} callback Called when the image is committed and
   *   the zoom-out animation has started.
   */
  leave(zoomToRect, callback) {
    var commitDone = function() {
      this.stopEditing_();
      this.stopSlideshow_();
      ImageUtil.setAttribute(this.arrowBox_, 'active', false);
      this.selectionModel_.removeEventListener(
          'change', this.onSelectionBound_);
      this.dataModel_.removeEventListener('splice', this.onSpliceBound_);
      this.ribbon_.disable();
      this.active_ = false;
      if (this.savedSelection_) {
        this.selectionModel_.selectedIndexes = this.savedSelection_;
      }
      this.unloadImage_(zoomToRect);
      callback();
    }.bind(this);

    this.viewport_.resetView();
    if (this.getItemCount_() === 0) {
      this.errorBanner_.clear();
      commitDone();
    } else {
      this.commitItem_(commitDone);
    }

    // Disable the slide-mode only buttons when leaving.
    this.editButton_.disabled = true;
    this.printButton_.disabled = true;

    // Disable touch operation.
    this.touchHandlers_.enabled = false;
  }

  /**
   * Activate content, in a way that makes sense for the content. Currently this
   * causes video to start playing.
   */
  activateContent() {
    var content = this.imageContainer_.firstElementChild;
    if (content instanceof HTMLVideoElement) {
      content.autoplay = true;
      // Disable controls for half a second. This avoids the video starting with
      // a big "pause" button in the center (even if the mouse is not moving).
      // If the user moves their mouse after the timeout, the controls will
      // appear.
      content.controls = false;
      setTimeout(function() {
        content.controls = true;
      }, 500);
    }
  }

  /**
   * Execute an action when the editor is not busy.
   *
   * @param {function()} action Function to execute.
   */
  executeWhenReady(action) {
    this.editor_.executeWhenReady(action);
  }

  /**
   * @return {boolean} True if the mode has active tools (that should not fade).
   */
  hasActiveTool() {
    return this.isEditing();
  }

  /**
   * @return {number} Item count.
   * @private
   */
  getItemCount_() {
    return this.dataModel_.length;
  }

  /**
   * @param {number} index Index.
   * @return {GalleryItem} Item.
   */
  getItem(index) {
    var item =
        /** @type {(GalleryItem|undefined)} */ (this.dataModel_.item(index));
    return item === undefined ? null : item;
  }

  /**
   * @return {number} Selected index.
   */
  getSelectedIndex() {
    return this.selectionModel_.selectedIndex;
  }

  /**
   * @return {ImageRect} Screen rectangle of the selected image.
   */
  getSelectedImageRect() {
    if (this.getSelectedIndex() < 0) {
      return null;
    } else {
      return this.viewport_.getImageBoundsOnScreen();
    }
  }

  /**
   * @return {GalleryItem} Selected item.
   */
  getSelectedItem() {
    return this.getItem(this.getSelectedIndex());
  }

  /**
   * Toggles the full screen mode.
   * @private
   */
  toggleFullScreen_() {
    util.toggleFullScreen(
        this.context_.appWindow, !util.isFullScreen(this.context_.appWindow));
  }

  /**
   * Selection change handler.
   *
   * Commits the current image and displays the newly selected image.
   * @private
   */
  onSelection_() {
    if (this.selectionModel_.selectedIndexes.length === 0) {
      return;  // Ignore temporary empty selection.
    }

    // Forget the saved selection if the user changed the selection manually.
    if (!this.isSlideshowOn_()) {
      this.savedSelection_ = null;
    }

    if (this.getSelectedItem() === this.displayedItem_) {
      return;  // Do not reselect.
    }

    this.commitItem_(this.loadSelectedItem_.bind(this));
  }

  /**
   * Change the selection.
   *
   * @param {number} index New selected index.
   * @param {number=} opt_slideHint Slide animation direction (-1|1).
   */
  select(index, opt_slideHint) {
    this.slideHint_ = opt_slideHint || null;
    this.selectionModel_.selectedIndex = index;
    this.selectionModel_.leadIndex = index;
  }

  /**
   * Load the selected item.
   *
   * @private
   */
  loadSelectedItem_() {
    var slideHint = this.slideHint_;
    this.slideHint_ = null;

    if (this.getSelectedItem() === this.displayedItem_) {
      return;
    }  // Do not reselect.

    var index = this.getSelectedIndex();
    if (index < 0) {
      return;
    }

    var displayedIndex = this.dataModel_.indexOf(this.displayedItem_);
    var step = slideHint || (displayedIndex > 0 ? index - displayedIndex : 1);

    if (Math.abs(step) != 1) {
      // Long leap, the sequence is broken, we have no good prefetch candidate.
      this.sequenceDirection_ = 0;
      this.sequenceLength_ = 0;
    } else if (this.sequenceDirection_ === step) {
      // Keeping going in sequence.
      this.sequenceLength_++;
    } else {
      // Reversed the direction. Reset the counter.
      this.sequenceDirection_ = step;
      this.sequenceLength_ = 1;
    }

    this.displayedItem_ = this.getSelectedItem();
    var selectedItem = assertInstanceof(this.getSelectedItem(), GalleryItem);

    function shouldPrefetch(loadType, step, sequenceLength) {
      // Never prefetch when selecting out of sequence.
      if (Math.abs(step) != 1) {
        return false;
      }

      // Always prefetch if the previous load was from cache.
      if (loadType === ImageView.LoadType.CACHED_FULL) {
        return true;
      }

      // Prefetch if we have been going in the same direction for long enough.
      return sequenceLength >= 3;
    }

    this.currentUniqueKey_++;
    var selectedUniqueKey = this.currentUniqueKey_;

    // Discard, since another load has been invoked after this one.
    if (selectedUniqueKey != this.currentUniqueKey_) {
      return;
    }

    this.loadItem_(
        selectedItem,
        new ImageView.Effect.Slide(step, this.isSlideshowPlaying_()),
        function() {} /* no displayCallback */,
        function(loadType, delay) {
          // Discard, since another load has been invoked after this one.
          if (selectedUniqueKey != this.currentUniqueKey_) {
            return;
          }
          if (shouldPrefetch(loadType, step, this.sequenceLength_)) {
            this.requestPrefetch(step, delay);
          }
          if (this.isSlideshowPlaying_()) {
            this.scheduleNextSlide_();
          }
        }.bind(this));
  }

  /**
   * Unload the current image.
   *
   * @param {ImageRect=} opt_zoomToRect Rectangle for zoom effect.
   * @private
   */
  unloadImage_(opt_zoomToRect) {
    this.imageView_.unload(opt_zoomToRect);
  }

  /**
   * Data model 'splice' event handler.
   * @param {!Event} event Event.
   * @this {SlideMode}
   * @private
   */
  onSplice_(event) {
    ImageUtil.setAttribute(this.arrowBox_, 'active', this.getItemCount_() > 1);

    // Splice invalidates saved indices, drop the saved selection.
    this.savedSelection_ = null;

    if (event.removed.length != 1) {
      return;
    }

    // Delay the selection to let the ribbon splice handler work first.
    setTimeout(function() {
      if (this.dataModel_.length === 0) {
        // No items left. Unload the image, disable edit and print button, and
        // show the banner.
        this.commitItem_(function() {
          this.unloadImage_();
          this.printButton_.disabled = true;
          this.editButton_.disabled = true;
          this.errorBanner_.show('GALLERY_NO_IMAGES');
          if (this.isEditing()) {
            this.toggleEditor();
          }
        }.bind(this));
        return;
      }

      var displayedItemNotRemvoed = event.removed.every(function(item) {
        return item !== this.displayedItem_;
      }.bind(this));
      if (!displayedItemNotRemvoed) {
        // There is the next item, select it. Otherwise, select the last item.
        var nextIndex = Math.min(event.index, this.dataModel_.length - 1);
        // To force to dispatch a selection change event, unselect all before.
        this.selectionModel_.unselectAll();
        this.select(nextIndex);
        // If the removed image was edit, leave the editing mode.
        if (this.isEditing()) {
          this.toggleEditor();
        }
      }
    }.bind(this), 0);
  }

  /**
   * @param {number} direction -1 for left, 1 for right.
   * @return {number} Next index in the given direction, with wrapping.
   * @private
   */
  getNextSelectedIndex_(direction) {
    function advance(index, limit) {
      index += (direction > 0 ? 1 : -1);
      if (index < 0) {
        return limit - 1;
      }
      if (index === limit) {
        return 0;
      }
      return index;
    }

    // If the saved selection is multiple the Slideshow should cycle through
    // the saved selection.
    if (this.isSlideshowOn_() && this.savedSelection_ &&
        this.savedSelection_.length > 1) {
      var pos = advance(
          this.savedSelection_.indexOf(this.getSelectedIndex()),
          this.savedSelection_.length);
      return this.savedSelection_[pos];
    } else {
      return advance(this.getSelectedIndex(), this.getItemCount_());
    }
  }

  /**
   * Advance the selection based on the pressed key ID.
   * @param {string} keyID Key of the KeyboardEvent.
   */
  advanceWithKeyboard(keyID) {
    if (this.getItemCount_() === 0) {
      return;
    }

    var prev =
        (keyID === 'ArrowUp' || keyID === 'ArrowLeft' ||
         keyID === 'MediaTrackPrevious');
    this.advanceManually(prev ? -1 : 1);
  }

  /**
   * Advance the selection as a result of a user action (as opposed to an
   * automatic change in the slideshow mode).
   * @param {number} direction -1 for left, 1 for right.
   */
  advanceManually(direction) {
    if (this.isSlideshowPlaying_()) {
      this.pauseSlideshow_();
    }
    cr.dispatchSimpleEvent(this, 'useraction');
    this.selectNext(direction);
  }

  /**
   * Select the next item.
   * @param {number} direction -1 for left, 1 for right.
   */
  selectNext(direction) {
    this.select(this.getNextSelectedIndex_(direction), direction);
  }

  /**
   * Select the first item.
   */
  selectFirst() {
    this.select(0);
  }

  /**
   * Select the last item.
   */
  selectLast() {
    this.select(this.getItemCount_() - 1);
  }

  // Loading/unloading

  /**
   * Load and display an item.
   *
   * @param {!GalleryItem} item Item.
   * @param {!ImageView.Effect} effect Transition effect object.
   * @param {function()} displayCallback Called when the image is displayed
   *     (which can happen before the image load due to caching).
   * @param {function(number, number)} loadCallback Called when the image is
   *     fully loaded.
   * @private
   */
  loadItem_(item, effect, displayCallback, loadCallback) {
    this.dimmableUIController_.setLoading(true);
    this.showProgressBar_(item);

    var loadDone = this.itemLoaded_.bind(this, item, loadCallback);

    var displayDone = function() {
      cr.dispatchSimpleEvent(this, 'image-displayed');
      displayCallback();
    }.bind(this);

    if (item.isEditable()) {
      this.editor_.openSession(
          item, effect, this.saveCurrentImage_.bind(this, item), displayDone,
          loadDone);
    } else {
      this.imageView_.load(item, effect, displayDone, loadDone);
    }
  }

  /**
   * A callback function when the editor opens a editing session for an image.
   * @param {!GalleryItem} item Gallery item.
   * @param {function(number, number)} loadCallback Called when the image is
   *     fully loaded.
   * @param {number} loadType Load type.
   * @param {number} delay Delay.
   * @param {*=} opt_error Error.
   * @private
   */
  itemLoaded_(item, loadCallback, loadType, delay, opt_error) {
    var entry = item.getEntry();

    this.hideProgressBar_();
    this.dimmableUIController_.setLoading(false);

    if (loadType === ImageView.LoadType.ERROR) {
      // if we have a specific error, then display it
      if (opt_error) {
        this.errorBanner_.show(/** @type {string} */ (opt_error));
      } else {
        // otherwise try to infer general error
        this.errorBanner_.show('GALLERY_IMAGE_ERROR');
      }
    } else if (loadType === ImageView.LoadType.OFFLINE) {
      this.errorBanner_.show('GALLERY_IMAGE_OFFLINE');
    }

    metrics.recordUserAction(ImageUtil.getMetricName('View'));

    var toMillions = function(number) {
      return Math.round(number / (1000 * 1000));
    };

    var metadata = item.getMetadataItem();
    if (metadata) {
      metrics.recordSmallCount(
          ImageUtil.getMetricName('Size.MB'), toMillions(metadata.size));
    }

    var image = this.imageView_.getMedia();
    metrics.recordSmallCount(
        ImageUtil.getMetricName('Size.MPix'),
        toMillions(image.width * image.height));

    var extIndex = entry.name.lastIndexOf('.');
    var ext = extIndex < 0 ? '' : entry.name.substr(extIndex + 1).toLowerCase();
    if (ext === 'jpeg') {
      ext = 'jpg';
    }
    metrics.recordEnum(
        ImageUtil.getMetricName('FileType'), ext, ImageUtil.FILE_TYPES);

    // Enable or disable buttons for editing and printing.
    let canEditAndPrint = !opt_error && item.isEditable();
    this.editButton_.disabled = !canEditAndPrint;
    this.printButton_.disabled = !canEditAndPrint;

    // Saved label is hidden by default.
    this.savedLabel_.hidden = true;

    // Disable overwrite original checkbox until settings is loaded.
    this.overwriteOriginalCheckbox_.disabled = true;
    this.overwriteOriginalCheckbox_.checked = false;

    var keys = {};
    keys[SlideMode.OVERWRITE_ORIGINAL_KEY] = true;
    chrome.storage.local.get(keys, function(values) {
      // Users can overwrite original file only if loaded image is original
      // and writable.
      if (item.isOriginal() && item.isWritableFile(this.volumeManager_)) {
        this.overwriteOriginalCheckbox_.disabled = false;
        this.overwriteOriginalCheckbox_.checked =
            values[SlideMode.OVERWRITE_ORIGINAL_KEY];
      }
    }.bind(this));

    loadCallback(loadType, delay);
  }

  /**
   * Commit changes to the current item and reset all messages/indicators.
   *
   * @param {function()} callback Callback.
   * @private
   */
  commitItem_(callback) {
    this.showSpinner_(false);
    this.errorBanner_.clear();
    this.editor_.getPrompt().hide();
    this.editor_.closeSession(callback);
  }

  /**
   * Request a prefetch for the next image.
   *
   * @param {number} direction -1 or 1.
   * @param {number} delay Delay in ms. Used to prevent the CPU-heavy image
   *   loading from disrupting the animation that might be still in progress.
   */
  requestPrefetch(direction, delay) {
    if (this.getItemCount_() <= 1) {
      return;
    }

    var index = this.getNextSelectedIndex_(direction);
    this.imageView_.prefetch(assert(this.getItem(index)), delay);
  }

  // Event handlers.

  /**
   * Click handler for the entire document.
   * @param {!Event} event Mouse click event.
   * @private
   */
  onDocumentClick_(event) {
    // Events created in fakeMouseClick in test util don't pass this test.
    if (!window.IN_TEST) {
      event = assertInstanceof(event, MouseEvent);
    }

    var targetElement = assertInstanceof(event.target, HTMLElement);
    // Close the bubble if clicked outside of it and if it is visible.
    if (!this.bubble_.contains(targetElement) &&
        !this.editButton_.contains(targetElement) &&
        !this.arrowLeft_.contains(targetElement) &&
        !this.arrowRight_.contains(targetElement) && !this.bubble_.hidden) {
      this.bubble_.hidden = true;
    }
  }

  /**
   * Keydown handler.
   *
   * @param {!Event} event Event.
   * @return {boolean} True if handled.
   */
  onKeyDown(event) {
    var keyID = util.getKeyModifiers(event) + event.key;

    if (this.isSlideshowOn_()) {
      switch (keyID) {
        case 'Escape':
        case 'MediaStop':
          this.stopSlideshow_(event);
          break;

        case ' ':  // Space pauses/resumes the slideshow.
        case 'MediaPlayPause':
          this.toggleSlideshowPause_();
          break;

        case 'ArrowUp':
        case 'ArrowDown':
        case 'ArrowLeft':
        case 'ArrowRight':
        case 'MediaTrackNex':
        case 'MediaTrackPrevious':
          this.advanceWithKeyboard(keyID);
          break;
      }
      return true;  // Consume all keystrokes in the slideshow mode.
    }

    // Handles shortcut keys common for both modes (editing and not-editing).
    switch (keyID) {
      case 'Ctrl-p':  // Ctrl+'p' prints the current image.
        if (!this.printButton_.disabled) {
          this.print_();
        }
        return true;

      case 'e':  // 'e' toggles the editor.
        if (!this.editButton_.disabled) {
          this.toggleEditor(event);
        }
        return true;
    }

    // Handles shortcurt keys for editing mode.
    if (this.isEditing()) {
      if (this.editor_.onKeyDown(event)) {
        return true;
      }

      if (keyID === 'Escape') {  // Escape
        this.toggleEditor(event);
        return true;
      }

      return false;
    }

    // Handles shortcut keys for not-editing mode.
    switch (keyID) {
      case 'Escape':
        if (this.viewport_.isZoomed()) {
          this.viewport_.resetView();
          this.touchHandlers_.stopOperation();
          this.imageView_.applyViewportChange();
          return true;
        }
        break;

      case 'Home':
        this.selectFirst();
        return true;

      case 'End':
        this.selectLast();
        return true;

      case 'ArrowUp':
      case 'ArrowDown':
      case 'ArrowLeft':
      case 'ArrowRight':
        if (this.viewport_.isZoomed()) {
          var delta = SlideMode.KEY_OFFSET_MAP[keyID];
          this.viewport_.setOffset(
              ~~(this.viewport_.getOffsetX() +
                 delta[0] * this.viewport_.getZoom()),
              ~~(this.viewport_.getOffsetY() +
                 delta[1] * this.viewport_.getZoom()));
          this.touchHandlers_.stopOperation();
          this.imageView_.applyViewportChange();
        } else {
          this.advanceWithKeyboard(keyID);
        }
        return true;

      case 'MediaTrackNext':
      case 'MediaTrackPrevious':
        this.advanceWithKeyboard(keyID);
        return true;

      case 'Ctrl-=':  // Ctrl+'=' zoom in.
        this.viewport_.zoomIn();
        this.touchHandlers_.stopOperation();
        this.imageView_.applyViewportChange();
        return true;

      case 'Ctrl--':  // Ctrl+'-' zoom out.
        this.viewport_.zoomOut();
        this.touchHandlers_.stopOperation();
        this.imageView_.applyViewportChange();
        return true;

      case 'Ctrl-0':  // Ctrl+'0' zoom reset.
        this.viewport_.setZoom(1.0);
        this.touchHandlers_.stopOperation();
        this.imageView_.applyViewportChange();
        return true;
    }

    return false;
  }

  /**
   * Resize handler.
   * @private
   */
  onResize_() {
    this.touchHandlers_.stopOperation();
  }

  /**
   * Handles resize event of viewport.
   * @private
   */
  onViewportResize_() {
    // This method must be called after the resize of viewport.
    this.editor_.getBuffer().draw();
  }

  /**
   * Update thumbnails.
   */
  updateThumbnails() {
    this.ribbon_.reset();
    if (this.active_) {
      this.ribbon_.redraw();
    }
  }

  // Saving

  /**
   * Save the current image to a file.
   *
   * @param {!GalleryItem} item Item to save the image.
   * @param {function()} callback Callback.
   * @private
   */
  saveCurrentImage_(item, callback) {
    this.showSpinner_(true);

    var savedPromise = this.dataModel_.saveItem(
        this.volumeManager_, item,
        ImageUtil.ensureCanvas(this.imageView_.getEditableImage()),
        this.overwriteOriginalCheckbox_.checked);

    savedPromise
        .then(function() {
          this.showSpinner_(false);
          this.flashSavedLabel_();

          // Record UMA for the first edit.
          if (this.imageView_.getContentRevision() === 1) {
            metrics.recordUserAction(ImageUtil.getMetricName('Edit'));
          }

          // Users can change overwrite original setting only if there is no
          // undo stack and item is original and writable.
          var ableToChangeOverwriteOriginalSetting = !this.editor_.canUndo() &&
              item.isOriginal() && item.isWritableFile(this.volumeManager_);
          this.overwriteOriginalCheckbox_.disabled =
              !ableToChangeOverwriteOriginalSetting;

          callback();
        }.bind(this))
        .catch(function(error) {
          console.error(error.stack || error);

          this.showSpinner_(false);
          this.errorBanner_.show('GALLERY_SAVE_FAILED');

          callback();
        }.bind(this));
  }

  /**
   * Flash 'Saved' label briefly to indicate that the image has been saved.
   * @private
   */
  flashSavedLabel_() {
    this.savedLabel_.hidden = false;
    var setLabelHighlighted =
        ImageUtil.setAttribute.bind(null, this.savedLabel_, 'highlighted');
    setTimeout(setLabelHighlighted.bind(null, true), 0);
    setTimeout(setLabelHighlighted.bind(null, false), 300);
  }

  /**
   * Handles change event of overwrite original checkbox.
   * @private
   */
  onOverwriteOriginalCheckboxChanged_() {
    var items = {};
    items[SlideMode.OVERWRITE_ORIGINAL_KEY] =
        this.overwriteOriginalCheckbox_.checked;
    chrome.storage.local.set(items);
  }

  /**
   * Overwrite info bubble close handler.
   * @private
   */
  onCloseBubble_() {
    this.bubble_.hidden = true;
    this.setOverwriteBubbleCount_(SlideMode.OVERWRITE_BUBBLE_MAX_TIMES);
  }

  /**
   * @return {boolean} True if the slideshow is on.
   * @private
   */
  isSlideshowOn_() {
    return this.container_.hasAttribute('slideshow');
  }

  /**
   * Starts the slideshow.
   * @param {number=} opt_interval First interval in ms.
   * @param {Event=} opt_event Event.
   */
  startSlideshow(opt_interval, opt_event) {
    // Reset zoom.
    this.viewport_.resetView();
    this.imageView_.applyViewportChange();

    // Disable touch operation.
    this.touchHandlers_.enabled = false;

    // Set the attribute early to prevent the toolbar from flashing when
    // the slideshow is being started from the mosaic view.
    this.container_.setAttribute('slideshow', 'playing');

    // Hide the slideshow Play / Pause Button in the toolbar if
    // there is less than two items and show it if there is more than 1 image.
    this.slideshowPlay_.hidden = (this.getItemCount_() === 1);

    if (this.active_) {
      this.stopEditing_();
    } else {
      // We are in the Mosaic mode. Toggle the mode but remember to return.
      this.leaveAfterSlideshow_ = true;

      // Wait until the zoom animation from the mosaic mode is done.
      var startSlideshowAfterTransition = function() {
        setTimeout(function() {
          this.startSlideshow.call(
              this, SlideMode.SLIDESHOW_INTERVAL, opt_event);
        }.bind(this), ImageView.MODE_TRANSITION_DURATION);
      }.bind(this);
      this.toggleMode_(startSlideshowAfterTransition);
      return;
    }

    if (opt_event) {  // Caused by user action, notify the Gallery.
      cr.dispatchSimpleEvent(this, 'useraction');
    }

    this.fullscreenBeforeSlideshow_ =
        util.isFullScreen(this.context_.appWindow);
    if (!this.fullscreenBeforeSlideshow_) {
      this.toggleFullScreen_();
      opt_interval = (opt_interval || SlideMode.SLIDESHOW_INTERVAL) +
          SlideMode.FULLSCREEN_TOGGLE_DELAY;
    }

    // These are workarounds. Mouseout event is not dispatched when window
    // becomes fullscreen and cursor gets out of the element
    // TODO(yawano): Find better implementation.
    this.dimmableUIController_.setCursorOutOfTools();
    document.querySelector('files-tooltip').hideTooltip();

    this.resumeSlideshow_(opt_interval);

    this.setSubMode_(GallerySubMode.SLIDESHOW);
  }

  /**
   * Stops the slideshow.
   * @param {Event=} opt_event Event.
   * @private
   */
  stopSlideshow_(opt_event) {
    if (!this.isSlideshowOn_()) {
      return;
    }

    if (opt_event) {  // Caused by user action, notify the Gallery.
      cr.dispatchSimpleEvent(this, 'useraction');
    }

    this.pauseSlideshow_();
    this.container_.removeAttribute('slideshow');

    // Do not restore fullscreen if we exited fullscreen while in slideshow.
    var fullscreen = util.isFullScreen(this.context_.appWindow);
    var toggleModeDelay = 0;
    if (!this.fullscreenBeforeSlideshow_ && fullscreen) {
      this.toggleFullScreen_();
      toggleModeDelay = SlideMode.FULLSCREEN_TOGGLE_DELAY;
    }
    if (this.leaveAfterSlideshow_) {
      this.leaveAfterSlideshow_ = false;
      setTimeout(this.toggleMode_.bind(this), toggleModeDelay);
    }

    // Re-enable touch operation.
    this.touchHandlers_.enabled = true;

    this.setSubMode_(GallerySubMode.BROWSE);
  }

  /**
   * @return {boolean} True if the slideshow is playing (not paused).
   * @private
   */
  isSlideshowPlaying_() {
    return this.container_.getAttribute('slideshow') === 'playing';
  }

  /**
   * Pauses/resumes the slideshow.
   * @private
   */
  toggleSlideshowPause_() {
    cr.dispatchSimpleEvent(this, 'useraction');  // Show the tools.
    if (this.isSlideshowPlaying_()) {
      this.pauseSlideshow_();
    } else {
      this.resumeSlideshow_(SlideMode.SLIDESHOW_INTERVAL_FIRST);
    }
  }

  /**
   * @param {number=} opt_interval Slideshow interval in ms.
   * @private
   */
  scheduleNextSlide_(opt_interval) {
    console.assert(this.isSlideshowPlaying_(), 'Inconsistent slideshow state');

    if (this.slideShowTimeout_) {
      clearTimeout(this.slideShowTimeout_);
    }

    this.slideShowTimeout_ = setTimeout(function() {
      this.slideShowTimeout_ = null;
      this.selectNext(1);
    }.bind(this), opt_interval || SlideMode.SLIDESHOW_INTERVAL);
  }

  /**
   * Resumes the slideshow.
   * @param {number=} opt_interval Slideshow interval in ms.
   * @private
   */
  resumeSlideshow_(opt_interval) {
    this.container_.setAttribute('slideshow', 'playing');
    this.scheduleNextSlide_(opt_interval);
  }

  /**
   * Pauses the slideshow.
   * @private
   */
  pauseSlideshow_() {
    this.container_.setAttribute('slideshow', 'paused');
    if (this.slideShowTimeout_) {
      clearTimeout(this.slideShowTimeout_);
      this.slideShowTimeout_ = null;
    }
  }

  /**
   * @return {boolean} True if the editor is active.
   */
  isEditing() {
    return this.container_.hasAttribute('editing');
  }

  /**
   * Stops editing.
   * @private
   */
  stopEditing_() {
    if (this.isEditing()) {
      this.toggleEditor();
    }
  }

  /**
   * Sets current sub mode.
   * @param {GallerySubMode} subMode
   * @private
   */
  setSubMode_(subMode) {
    if (this.subMode_ === subMode) {
      return;
    }

    this.subMode_ = subMode;

    var event = new Event('sub-mode-change');
    event.subMode = this.subMode_;
    this.dispatchEvent(event);
  }

  /**
   * Returns current sub mode.
   * @return {GallerySubMode}
   */
  getSubMode() {
    return this.subMode_;
  }

  /**
   * Activate/deactivate editor.
   * @param {Event=} opt_event Event.
   */
  toggleEditor(opt_event) {
    if (opt_event) {  // Caused by user action, notify the Gallery.
      cr.dispatchSimpleEvent(this, 'useraction');
    }

    if (!this.active_) {
      this.toggleMode_(this.toggleEditor.bind(this));
      return;
    }

    this.stopSlideshow_();

    // Disable entering edit mode for videos.
    let startEditing = false;
    let item = this.getItem(this.getSelectedIndex());
    if (item != null) {
      startEditing = !this.isEditing() && item.isEditable();
    }

    ImageUtil.setAttribute(this.container_, 'editing', startEditing);
    this.editButtonToggleRipple_.activated = this.isEditing();

    if (this.isEditing()) {  // isEditing has just been flipped to a new value.
      // The item should not be null.
      item = assert(item);

      // Reset zoom.
      this.viewport_.resetView();

      // Scale the screen so that it doesn't overlap the toolbars.
      this.viewport_.setScreenTop(ImageEditorToolbar.HEIGHT);
      this.viewport_.setScreenBottom(ImageEditorToolbar.HEIGHT);

      this.imageView_.applyViewportChange();

      this.touchHandlers_.enabled = false;

      // Show editor warning message.
      SlideMode
          .getEditorWarningMessage(
              item, this.context_.readonlyDirName,
              assert(this.dataModel_.fallbackSaveDirectory))
          .then(function(warningMessage) {
            if (!warningMessage) {
              return;
            }

            this.filesToast_.show(warningMessage);
          }.bind(this));

      // Show overwrite original bubble if it hasn't been shown for max times.
      this.getOverwriteBubbleCount_().then(function(count) {
        if (count >= SlideMode.OVERWRITE_BUBBLE_MAX_TIMES) {
          return;
        }

        this.setOverwriteBubbleCount_(count + 1);
        this.bubble_.hidden = false;
      }.bind(this));

      this.setSubMode_(GallerySubMode.EDIT);
      this.editor_.onStartEditing();
    } else {
      this.editor_.getPrompt().hide();
      this.editor_.leaveMode(false /* not to switch mode */);

      this.viewport_.setScreenTop(0);
      this.viewport_.setScreenBottom(0);
      this.imageView_.applyViewportChange();

      this.bubble_.hidden = true;

      this.touchHandlers_.enabled = true;

      this.setSubMode_(GallerySubMode.BROWSE);
    }
  }

  /**
   * Gets count of overwrite bubble.
   * @return {!Promise<number>}
   * @private
   */
  getOverwriteBubbleCount_() {
    return new Promise(function(resolve, reject) {
      var requests = {};
      requests[SlideMode.OVERWRITE_BUBBLE_KEY] = 0;

      chrome.storage.local.get(requests, function(results) {
        if (chrome.runtime.lastError) {
          reject(chrome.runtime.lastError);
          return;
        }

        resolve(results[SlideMode.OVERWRITE_BUBBLE_KEY]);
      });
    });
  }

  /**
   * Sets count of overwrite bubble.
   * @param {number} value
   * @private
   */
  setOverwriteBubbleCount_(value) {
    var requests = {};
    requests[SlideMode.OVERWRITE_BUBBLE_KEY] = value;
    chrome.storage.local.set(requests);
  }

  /**
   * Prints the current item.
   * @private
   */
  print_() {
    this.stopEditing_();
    cr.dispatchSimpleEvent(this, 'useraction');
    window.print();
  }

  /**
   * Shows progress bar.
   * @param {!GalleryItem} item
   * @private
   */
  showProgressBar_(item) {
    this.loadingItemUrl_ = item.getEntry().toURL();

    if (this.progressBarTimer_ !== 0) {
      clearTimeout(this.progressBarTimer_);
      this.progressBarTimer_ = 0;
    }

    this.progressBar_.setAttribute('indeterminate', true);

    this.progressBarTimer_ = setTimeout(function() {
      this.progressBar_.hidden = false;
    }.bind(this), 1000);
  }

  /**
   * Hides progress bar.
   * @private
   */
  hideProgressBar_() {
    if (this.progressBarTimer_ !== 0) {
      clearTimeout(this.progressBarTimer_);
      this.progressBarTimer_ = 0;
    }

    this.loadingItemUrl_ = undefined;

    this.progressBar_.hidden = true;
  }

  /**
   * Updates progress bar.
   * @param {!chrome.fileManagerPrivate.FileTransferStatus} status
   * @private
   */
  updateProgressBar_(status) {
    if (status.fileUrl !== this.loadingItemUrl_ ||
        status.num_total_jobs !== 1) {
      // If user starts to download another image (or file), we cannot show
      // determinate progress bar anymore since total and processed are for all
      // current downloads.
      this.progressBar_.setAttribute('indeterminate', true);
      return;
    }

    // Progress begins from 5%.
    var progress = 5 + (95 * status.processed / status.total);

    this.progressBar_.removeAttribute('indeterminate');
    this.progressBar_.value = progress;
  }

  /**
   * Shows/hides the busy spinner.
   *
   * @param {boolean} on True if show, false if hide.
   * @private
   */
  showSpinner_(on) {
    if (this.spinnerTimer_) {
      clearTimeout(this.spinnerTimer_);
      this.spinnerTimer_ = null;
    }

    if (on) {
      this.spinnerTimer_ = setTimeout(function() {
        this.spinnerTimer_ = null;
        ImageUtil.setAttribute(this.container_, 'spinner', true);
      }.bind(this), 1000);
    } else {
      ImageUtil.setAttribute(this.container_, 'spinner', false);
    }
  }

  /**
   * Apply the change of viewport.
   */
  applyViewportChange() {
    this.imageView_.applyViewportChange();
  }
}

/**
 * List of available editor modes.
 * @type {!Array<ImageEditorMode>}
 * @const
 */
SlideMode.EDITOR_MODES = [
  new ImageEditorMode.InstantAutofix(), new ImageEditorMode.Crop(),
  new ImageEditorMode.Resize(), new ImageEditorMode.Exposure(),
  new ImageEditorMode.OneClick(
      'rotate_left', 'GALLERY_ROTATE_LEFT', new Command.Rotate(-1)),
  new ImageEditorMode.OneClick(
      'rotate_right', 'GALLERY_ROTATE_RIGHT', new Command.Rotate(1))
];

/**
 * Map of the key identifier and offset delta.
 * @enum {!Array<number>})
 * @const
 */
SlideMode.KEY_OFFSET_MAP = {
  'Up': [0, 20],
  'Down': [0, -20],
  'Left': [20, 0],
  'Right': [-20, 0]
};

/**
 * Local storage key for the number of times that
 * the overwrite info bubble has been displayed.
 * @const {string}
 */
SlideMode.OVERWRITE_BUBBLE_KEY = 'gallery-overwrite-bubble';

/**
 * Local storage key for overwrite original checkbox value.
 * @const {string}
 */
SlideMode.OVERWRITE_ORIGINAL_KEY = 'gallery-overwrite-original';

/**
 * Max number that the overwrite info bubble is shown.
 * @const {number}
 */
SlideMode.OVERWRITE_BUBBLE_MAX_TIMES = 5;

// Slideshow

/**
 * Slideshow interval in ms.
 */
SlideMode.SLIDESHOW_INTERVAL = 5000;

/**
 * First slideshow interval in ms. It should be shorter so that the user
 * is not guessing whether the button worked.
 */
SlideMode.SLIDESHOW_INTERVAL_FIRST = 1000;

/**
 * Empirically determined duration of the fullscreen toggle animation.
 */
SlideMode.FULLSCREEN_TOGGLE_DELAY = 500;

/**
 * Touch handlers of the slide mode.
 */
class TouchHandler {
  /**
   * @param {!Element} targetElement Event source.
   * @param {!SlideMode} slideMode Slide mode to be operated by the handler.
   */
  constructor(targetElement, slideMode) {
    /**
     * Event source.
     * @type {!Element}
     * @private
     * @const
     */
    this.targetElement_ = targetElement;

    /**
     * Target of touch operations.
     * @type {!SlideMode}
     * @private
     * @const
     */
    this.slideMode_ = slideMode;

    /**
     * Flag to enable/disable touch operation.
     * @type {boolean}
     * @private
     */
    this.enabled_ = true;

    /**
     * Whether it is in a touch operation that is started from targetElement or
     * not.
     * @type {boolean}
     * @private
     */
    this.touchStarted_ = false;

    /**
     * Whether the element is being clicked now or not.
     * @type {boolean}
     * @private
     */
    this.clickStarted_ = false;

    /**
     * The swipe action that should happen only once in an operation is already
     * done or not.
     * @type {boolean}
     * @private
     */
    this.done_ = false;

    /**
     * Event on beginning of the current gesture.
     * The variable is updated when the number of touch finger changed.
     * @type {TouchEvent}
     * @private
     */
    this.gestureStartEvent_ = null;

    /**
     * Rotation value on beginning of the current gesture.
     * @type {number}
     * @private
     */
    this.gestureStartRotation_ = 0;

    /**
     * Last touch event.
     * @type {TouchEvent}
     * @private
     */
    this.lastEvent_ = null;

    /**
     * Zoom value just after last touch event.
     * @type {number}
     * @private
     */
    this.lastZoom_ = 1.0;

    /**
     * @type {number}
     * @private
     */
    this.mouseWheelZoomOperationId_ = 0;

    targetElement.addEventListener('touchstart', this.onTouchStart_.bind(this));
    var onTouchEventBound = this.onTouchEvent_.bind(this);
    targetElement.ownerDocument.addEventListener(
        'touchmove', onTouchEventBound);
    targetElement.ownerDocument.addEventListener('touchend', onTouchEventBound);

    targetElement.addEventListener('mousedown', this.onMouseDown_.bind(this));
    targetElement.ownerDocument.addEventListener(
        'mousemove', this.onMouseMove_.bind(this));
    targetElement.ownerDocument.addEventListener(
        'mouseup', this.onMouseUp_.bind(this));
    targetElement.addEventListener('mousewheel', this.onMouseWheel_.bind(this));
  }

  /**
   * Obtains distance between fingers.
   * @param {!TouchEvent} event Touch event. It should include more than two
   *     touches.
   * @return {number} Distance between touch[0] and touch[1].
   */
  static getDistance(event) {
    var touch1 = event.touches[0];
    var touch2 = event.touches[1];
    var dx = touch1.clientX - touch2.clientX;
    var dy = touch1.clientY - touch2.clientY;
    return Math.sqrt(dx * dx + dy * dy);
  }

  /**
   * Obtains the degrees of the pinch twist angle.
   * @param {!TouchEvent} event1 Start touch event. It should include more than
   *     two touches.
   * @param {!TouchEvent} event2 Current touch event. It should include more
   *     than two touches.
   * @return {number} Degrees of the pinch twist angle.
   */
  static getTwistAngle(event1, event2) {
    var dx1 = event1.touches[1].clientX - event1.touches[0].clientX;
    var dy1 = event1.touches[1].clientY - event1.touches[0].clientY;
    var dx2 = event2.touches[1].clientX - event2.touches[0].clientX;
    var dy2 = event2.touches[1].clientY - event2.touches[0].clientY;
    var innerProduct = dx1 * dx2 + dy1 * dy2;  // |v1| * |v2| * cos(t) = x / r
    var outerProduct = dx1 * dy2 - dy1 * dx2;  // |v1| * |v2| * sin(t) = y / r
    return Math.atan2(outerProduct, innerProduct) * 180 /
        Math.PI;  // atan(y / x)
  }

  /**
   * @param {boolean} flag New value.
   */
  set enabled(flag) {
    this.enabled_ = flag;
    if (!this.enabled_) {
      this.stopOperation();
    }
  }

  /**
   * Stops the current touch operation.
   */
  stopOperation() {
    this.touchStarted_ = false;
    this.done_ = false;
    this.gestureStartEvent_ = null;
    this.lastEvent_ = null;
    this.lastZoom_ = 1.0;
  }

  /**
   * Handles touch start events.
   * @param {!Event} event Touch event.
   * @private
   */
  onTouchStart_(event) {
    event = assertInstanceof(event, TouchEvent);
    if (this.enabled_ && event.touches.length === 1) {
      this.touchStarted_ = true;
    }
  }

  /**
   * Handles touch move and touch end events.
   * @param {!Event} event Touch event.
   * @private
   */
  onTouchEvent_(event) {
    event = assertInstanceof(event, TouchEvent);
    // Check if the current touch operation started from the target element or
    // not.
    if (!this.touchStarted_) {
      return;
    }

    // Check if the current touch operation ends with the event.
    if (event.touches.length === 0) {
      this.stopOperation();
      return;
    }

    // Check if a new gesture started or not.
    var viewport = this.slideMode_.getViewport();
    if (!this.lastEvent_ ||
        this.lastEvent_.touches.length !== event.touches.length) {
      if (event.touches.length === 2 || event.touches.length === 1) {
        this.gestureStartEvent_ = event;
        this.gestureStartRotation_ = viewport.getRotation();
        this.lastEvent_ = event;
        this.lastZoom_ = viewport.getZoom();
      } else {
        this.gestureStartEvent_ = null;
        this.gestureStartRotation_ = 0;
        this.lastEvent_ = null;
        this.lastZoom_ = 1.0;
      }
      return;
    }

    // Handle the gesture movement.
    switch (event.touches.length) {
      case 1:
        if (viewport.isZoomed()) {
          // Scrolling an image by swipe.
          var dx =
              event.touches[0].screenX - this.lastEvent_.touches[0].screenX;
          var dy =
              event.touches[0].screenY - this.lastEvent_.touches[0].screenY;
          viewport.setOffset(
              viewport.getOffsetX() + dx, viewport.getOffsetY() + dy);
          this.slideMode_.applyViewportChange();
        } else {
          // Traversing images by swipe.
          if (this.done_) {
            break;
          }
          var dx = event.touches[0].clientX -
              this.gestureStartEvent_.touches[0].clientX;
          if (dx > TouchHandler.SWIPE_THRESHOLD) {
            this.slideMode_.advanceManually(-1);
            this.done_ = true;
          } else if (dx < -TouchHandler.SWIPE_THRESHOLD) {
            this.slideMode_.advanceManually(1);
            this.done_ = true;
          }
        }
        break;

      case 2:
        // Pinch zoom.
        var distance1 = TouchHandler.getDistance(this.lastEvent_);
        var distance2 = TouchHandler.getDistance(event);
        if (distance1 === 0) {
          break;
        }
        var zoom = distance2 / distance1 * this.lastZoom_;
        viewport.setZoom(zoom);

        // Pinch rotation.
        assert(this.gestureStartEvent_);
        var angle = TouchHandler.getTwistAngle(this.gestureStartEvent_, event);
        if (angle > TouchHandler.ROTATION_THRESHOLD) {
          viewport.setRotation(this.gestureStartRotation_ + 1);
        } else if (angle < -TouchHandler.ROTATION_THRESHOLD) {
          viewport.setRotation(this.gestureStartRotation_ - 1);
        } else {
          viewport.setRotation(this.gestureStartRotation_);
        }
        this.slideMode_.applyViewportChange();
        break;
    }

    // Update the last event.
    this.lastEvent_ = event;
    this.lastZoom_ = viewport.getZoom();
  }

  /**
   * Handles mouse wheel events.
   * @param {!Event} event Wheel event.
   * @private
   */
  onMouseWheel_(event) {
    var event = assertInstanceof(event, MouseEvent);
    if (!this.enabled_) {
      return;
    }

    this.stopOperation();

    var viewport = this.slideMode_.getViewport();
    var zoom = viewport.getZoom();
    if (event.wheelDeltaY > 0) {
      zoom *= TouchHandler.WHEEL_ZOOM_FACTOR;
    } else {
      zoom /= TouchHandler.WHEEL_ZOOM_FACTOR;
    }

    // Request animation frame not to set zoom more than once in a frame. This
    // is a fix for https://crbug.com/591033
    requestAnimationFrame(function(operationId) {
      if (this.mouseWheelZoomOperationId_ !== operationId) {
        return;
      }

      viewport.setZoom(zoom);
      this.slideMode_.applyViewportChange();
    }.bind(this, ++this.mouseWheelZoomOperationId_));
  }

  /**
   * Handles mouse down events.
   * @param {!Event} event Wheel event.
   * @private
   */
  onMouseDown_(event) {
    var event = assertInstanceof(event, MouseEvent);
    var viewport = this.slideMode_.getViewport();
    if (!this.enabled_ || event.button !== 0) {
      return;
    }
    this.clickStarted_ = true;
  }

  /**
   * Handles mouse move events.
   * @param {!Event} event Wheel event.
   * @private
   */
  onMouseMove_(event) {
    var event = assertInstanceof(event, MouseEvent);
    var viewport = this.slideMode_.getViewport();
    if (!this.enabled_ || !this.clickStarted_) {
      return;
    }
    this.stopOperation();
    viewport.setOffset(
        viewport.getOffsetX() +
            (/** @type {{movementX: number}} */ (event)).movementX,
        viewport.getOffsetY() +
            (/** @type {{movementY: number}} */ (event)).movementY);
    this.slideMode_.imageView_.applyViewportChange();
  }

  /**
   * Handles mouse up events.
   * @param {!Event} event Wheel event.
   * @private
   */
  onMouseUp_(event) {
    if (event.button !== 0) {
      return;
    }
    this.clickStarted_ = false;
  }
}

/**
 * If the user touched the image and moved the finger more than SWIPE_THRESHOLD
 * horizontally it's considered as a swipe gesture (change the current image).
 * @type {number}
 * @const
 */
TouchHandler.SWIPE_THRESHOLD = 100;

/**
 * Rotation threshold in degrees.
 * @type {number}
 * @const
 */
TouchHandler.ROTATION_THRESHOLD = 25;

/**
 * Zoom magnification of one scroll event.
 * @private {number}
 * @const
 */
TouchHandler.WHEEL_ZOOM_FACTOR = 1.05;
