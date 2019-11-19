// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * ImageEditor is the top level object that holds together and connects
 * everything needed for image editing.
 */
class ImageEditor extends cr.EventTarget {
  /**
   * @param {!Viewport} viewport The viewport.
   * @param {!ImageView} imageView The ImageView containing the images to edit.
   * @param {!ImageEditorPrompt} prompt Prompt instance.
   * @param {!{image: !HTMLElement, root: !HTMLElement, toolbar: !HTMLElement,
   * mode: !HTMLElement}} DOMContainers Various DOM containers required for the
   *     editor.
   * @param {!Array<!ImageEditorMode>} modes Available editor modes.
   * @param {function(string, ...string)} displayStringFunction String
   *     formatting function.
   *
   * TODO(yawano): Remove displayStringFunction from arguments.
   */
  constructor(
      viewport, imageView, prompt, DOMContainers, modes,
      displayStringFunction) {
    super();

    this.rootContainer_ = DOMContainers.root;
    this.container_ = DOMContainers.image;
    this.modes_ = modes;
    this.displayStringFunction_ = displayStringFunction;

    /**
     * @private {ImageEditorMode}
     */
    this.currentMode_ = null;

    /**
     * @private {HTMLElement}
     */
    this.currentTool_ = null;

    /**
     * @private {boolean}
     */
    this.settingUpNextMode_ = false;

    ImageUtil.removeChildren(this.container_);

    this.viewport_ = viewport;

    this.imageView_ = imageView;

    this.buffer_ = new ImageBuffer();
    this.buffer_.addOverlay(this.imageView_);

    this.panControl_ = new ImageEditor.MouseControl(
        this.rootContainer_, this.container_, this.getBuffer());
    this.panControl_.setDoubleTapCallback(this.onDoubleTap_.bind(this));

    this.mainToolbar_ =
        new ImageEditorToolbar(DOMContainers.toolbar, displayStringFunction);

    this.modeToolbar_ = new ImageEditorToolbar(
        DOMContainers.mode, displayStringFunction,
        this.onOptionsChange.bind(this), true /* done button */);
    this.modeToolbar_.addEventListener(
        'done-clicked', this.onDoneClicked_.bind(this));
    this.modeToolbar_.addEventListener(
        'cancel-clicked', this.onCancelClicked_.bind(this));

    this.prompt_ = prompt;

    this.commandQueue_ = null;

    // -----------------------------------------------------------------
    // Populate the toolbar.

    /**
     * @type {!Array<string>}
     * @private
     */
    this.actionNames_ = [];

    this.mainToolbar_.clear();

    // Create action buttons.
    for (var i = 0; i != this.modes_.length; i++) {
      var mode = this.modes_[i];
      var button = this.createToolButton_(
          mode.name, mode.title, this.enterMode.bind(this, mode), mode.instant);
      mode.bind(
          button, this.getBuffer(), this.getViewport(), this.getImageView());
      this.registerAction_(mode.name);
    }

    /**
     * @type {!HTMLElement}
     * @private
     */
    this.undoButton_ = this.createToolButton_(
        'undo', 'GALLERY_UNDO', this.undo.bind(this), true /* instant */);
    this.registerAction_('undo');

    /**
     * @type {!HTMLElement}
     * @private
     */
    this.redoButton_ = this.createToolButton_(
        'redo', 'GALLERY_REDO', this.redo.bind(this), true /* instant */);
    this.registerAction_('redo');

    /**
     * @private {!HTMLElement}
     * @const
     */
    this.exitButton_ = /** @type {!HTMLElement} */
        (queryRequiredElement('.edit-mode-toolbar cr-button.exit'));
    this.exitButton_.addEventListener('click', this.onExitClicked_.bind(this));

    /**
     * @private {!FilesToast}
     */
    this.filesToast_ = /** @type {!FilesToast}*/
        (queryRequiredElement('files-toast'));
  }

  /**
   * Handles click event of exit button.
   * @private
   */
  onExitClicked_() {
    var event = new Event('exit-clicked');
    this.dispatchEvent(event);
  }

  /**
   * Creates a toolbar button.
   * @param {string} name Button name.
   * @param {string} title Button title.
   * @param {function(Event)} handler onClick handler.
   * @param {boolean} isInstant True if this tool (mode) is instant.
   * @return {!HTMLElement} A created button.
   * @private
   */
  createToolButton_(name, title, handler, isInstant) {
    var button = this.mainToolbar_.addButton(
        title,
        isInstant ? ImageEditorToolbar.ButtonType.ICON :
                    ImageEditorToolbar.ButtonType.ICON_TOGGLEABLE,
        handler, name /* opt_className */);
    return button;
  }

  /**
   * @return {boolean} True if no user commands are to be accepted.
   */
  isLocked() {
    return !this.commandQueue_ || this.commandQueue_.isBusy();
  }

  /**
   * @return {boolean} True if the command queue is busy.
   */
  isBusy() {
    return this.commandQueue_ && this.commandQueue_.isBusy();
  }

  /**
   * Reflect the locked state of the editor in the UI.
   * @param {boolean} on True if locked.
   */
  lockUI(on) {
    ImageUtil.setAttribute(this.rootContainer_, 'locked', on);
  }

  /**
   * Report the tool use to the metrics subsystem.
   * @param {string} name Action name.
   */
  recordToolUse(name) {
    metrics.recordEnum(
        ImageUtil.getMetricName('Tool'), name, this.actionNames_);
  }

  /**
   * Content update handler.
   * @private
   */
  calculateModeApplicativity_() {
    for (var i = 0; i != this.modes_.length; i++) {
      var mode = this.modes_[i];
      ImageUtil.setAttribute(
          assert(mode.button_), 'disabled', !mode.isApplicable());
    }
  }

  /**
   * Open the editing session for a new image.
   *
   * @param {!GalleryItem} item Gallery item.
   * @param {!ImageView.Effect} effect Transition effect object.
   * @param {function(function())} saveFunction Image save function.
   * @param {function()} displayCallback Display callback.
   * @param {function(!ImageView.LoadType, number, *=)} loadCallback Load
   *     callback.
   */
  openSession(item, effect, saveFunction, displayCallback, loadCallback) {
    if (this.commandQueue_) {
      throw new Error('Session not closed');
    }

    this.lockUI(true);

    var self = this;
    this.imageView_.load(
        item, effect, displayCallback, function(loadType, delay, error) {
          self.lockUI(false);

          // Always handle an item as original for new session.
          item.setAsOriginal();

          self.commandQueue_ = new CommandQueue(
              assert(self.container_.ownerDocument),
              assert(self.imageView_.getEditableImage()), saveFunction);
          self.commandQueue_.attachUI(
              self.getImageView(), self.getPrompt(), self.filesToast_,
              self.updateUndoRedo.bind(self), self.lockUI.bind(self));
          self.updateUndoRedo();
          loadCallback(loadType, delay, error);
        });
  }

  /**
   * Close the current image editing session.
   * @param {function()} callback Callback.
   */
  closeSession(callback) {
    this.getPrompt().hide();
    if (this.imageView_.isLoading()) {
      if (this.commandQueue_) {
        console.warn('Inconsistent image editor state');
        this.commandQueue_ = null;
      }
      this.imageView_.cancelLoad();
      this.lockUI(false);
      callback();
      return;
    }
    if (!this.commandQueue_) {
      // Session is already closed.
      callback();
      return;
    }

    this.executeWhenReady(callback);
    this.commandQueue_.close();
    this.commandQueue_ = null;
  }

  /**
   * Commit the current operation and execute the action.
   *
   * @param {function()} callback Callback.
   */
  executeWhenReady(callback) {
    if (this.commandQueue_) {
      this.leaveMode(false /* not to switch mode */);
      this.commandQueue_.executeWhenReady(callback);
    } else {
      if (!this.imageView_.isLoading()) {
        console.warn('Inconsistent image editor state');
      }
      callback();
    }
  }

  /**
   * @return {boolean} True if undo queue is not empty.
   */
  canUndo() {
    return !!this.commandQueue_ && this.commandQueue_.canUndo();
  }

  /**
   * Undo the recently executed command.
   */
  undo() {
    if (this.isLocked()) {
      return;
    }
    this.recordToolUse('undo');

    // First undo click should dismiss the uncommitted modifications.
    if (this.currentMode_ && this.currentMode_.isUpdated()) {
      this.modeToolbar_.reset();
      this.currentMode_.reset();
      return;
    }

    this.getPrompt().hide();
    this.leaveModeInternal_(false, false /* not to switch mode */);
    this.commandQueue_.undo();
    this.updateUndoRedo();
    this.calculateModeApplicativity_();
  }

  /**
   * Redo the recently un-done command.
   */
  redo() {
    if (this.isLocked()) {
      return;
    }
    this.recordToolUse('redo');
    this.getPrompt().hide();
    this.leaveModeInternal_(false, false /* not to switch mode */);
    this.commandQueue_.redo();
    this.updateUndoRedo();
    this.calculateModeApplicativity_();
  }

  /**
   * Update Undo/Redo buttons state.
   */
  updateUndoRedo() {
    var canUndo = this.commandQueue_ && this.commandQueue_.canUndo();
    var canRedo = this.commandQueue_ && this.commandQueue_.canRedo();
    ImageUtil.setAttribute(this.undoButton_, 'disabled', !canUndo);
    ImageUtil.setAttribute(this.redoButton_, 'disabled', !canRedo);
  }

  /**
   * @return {HTMLCanvasElement|HTMLImageElement} The current image.
   */
  getImage() {
    return this.getImageView().getEditableImage();
  }

  /**
   * @return {!ImageBuffer} ImageBuffer instance.
   */
  getBuffer() {
    return this.buffer_;
  }

  /**
   * @return {!ImageView} ImageView instance.
   */
  getImageView() {
    return this.imageView_;
  }

  /**
   * @return {!Viewport} Viewport instance.
   */
  getViewport() {
    return this.viewport_;
  }

  /**
   * @return {!ImageEditorPrompt} Prompt instance.
   */
  getPrompt() {
    return this.prompt_;
  }

  /**
   * Handle the toolbar controls update.
   * @param {Object} options A map of options.
   */
  onOptionsChange(options) {
    ImageUtil.trace.resetTimer('update');
    if (this.currentMode_) {
      this.currentMode_.update(options);
    }
    ImageUtil.trace.reportTimer('update');
  }

  /**
   * Register the action name. Required for metrics reporting.
   * @param {string} name Button name.
   * @private
   */
  registerAction_(name) {
    this.actionNames_.push(name);
  }

  /**
   * @return {ImageEditorMode} The current mode.
   */
  getMode() {
    return this.currentMode_;
  }

  /**
   * The user clicked on the mode button.
   *
   * @param {!ImageEditorMode} mode The new mode.
   */
  enterMode(mode) {
    if (this.isLocked()) {
      return;
    }

    if (this.currentMode_ === mode) {
      // Currently active editor tool clicked, commit if modified.
      this.leaveModeInternal_(
          this.currentMode_.updated_, false /* not to switch mode */);
      return;
    }

    // Guard not to call setUpMode_ more than once.
    if (this.settingUpNextMode_) {
      return;
    }
    this.settingUpNextMode_ = true;

    this.recordToolUse(mode.name);

    this.leaveMode(true /* to switch mode */);

    // The above call could have caused a commit which might have initiated
    // an asynchronous command execution. Wait for it to complete, then proceed
    // with the mode set up.
    this.commandQueue_.executeWhenReady(function() {
      this.setUpMode_(mode);
      this.settingUpNextMode_ = false;
    }.bind(this));
  }

  /**
   * Set up the new editing mode.
   *
   * @param {!ImageEditorMode} mode The mode.
   * @private
   */
  setUpMode_(mode) {
    this.currentTool_ = mode.button_;
    this.currentMode_ = mode;
    this.rootContainer_.setAttribute('editor-mode', mode.name);

    // Activate toggle ripple if button is toggleable.
    var filesToggleRipple =
        this.currentTool_.querySelector('files-toggle-ripple');
    if (filesToggleRipple) {
      // Current mode must NOT be instant for toggleable button.
      assert(!this.currentMode_.instant);
      filesToggleRipple.activated = true;
    }

    // Scale the screen so that it doesn't overlap the toolbars. We should scale
    // the screen before setup of current mode is called to make the current
    // mode able to set up with new screen size.
    if (!this.currentMode_.instant) {
      this.getViewport().setScreenTop(
          ImageEditorToolbar.HEIGHT + mode.paddingTop);
      this.getViewport().setScreenBottom(
          ImageEditorToolbar.HEIGHT * 2 + mode.paddingBottom);
      this.getImageView().applyViewportChange();
    }

    this.currentMode_.setUp();

    this.calculateModeApplicativity_();
    if (this.currentMode_.instant) {  // Instant tool.
      this.leaveModeInternal_(true, false /* not to switch mode */);
      return;
    }

    this.exitButton_.hidden = true;

    this.modeToolbar_.clear();
    this.currentMode_.createTools(this.modeToolbar_);
    this.modeToolbar_.show(true);
  }

  /**
   * Handles click event of Done button.
   * @param {!Event} event An event.
   * @private
   */
  onDoneClicked_(event) {
    this.leaveModeInternal_(true /* commit */, false /* not to switch mode */);
  }

  /**
   * Handles click event of Cancel button.
   * @param {!Event} event An event.
   * @private
   */
  onCancelClicked_(event) {
    this.leaveModeInternal_(
        false /* not commit */, false /* not to switch mode */);
  }

  /**
   * The user clicked on 'OK' or 'Cancel' or on a different mode button.
   * @param {boolean} commit True if commit is required.
   * @param {boolean} leaveToSwitchMode True if it leaves to change mode.
   * @private
   */
  leaveModeInternal_(commit, leaveToSwitchMode) {
    if (!this.currentMode_) {
      return;
    }

    // If the current mode is 'Resize', and commit is required,
    // leaving mode should be stopped when an input value is not valid.
    if (commit && this.currentMode_.name === 'resize') {
      var resizeMode = /** @type {!ImageEditorMode.Resize} */
          (this.currentMode_);
      if (!resizeMode.isInputValid()) {
        resizeMode.showAlertDialog();
        return;
      }
    }

    this.modeToolbar_.show(false);
    this.rootContainer_.removeAttribute('editor-mode');

    // If it leaves to switch mode, do not restore screen size since the next
    // mode might change screen size. We should avoid to show intermediate
    // animation which tries to restore screen size.
    if (!leaveToSwitchMode) {
      this.getViewport().setScreenTop(ImageEditorToolbar.HEIGHT);
      this.getViewport().setScreenBottom(ImageEditorToolbar.HEIGHT);
      this.getImageView().applyViewportChange();
    }

    this.currentMode_.cleanUpUI();

    if (commit) {
      var self = this;
      var command = this.currentMode_.getCommand();
      if (command) {  // Could be null if the user did not do anything.
        this.commandQueue_.execute(command);
        this.updateUndoRedo();
      }
    }

    var filesToggleRipple =
        this.currentTool_.querySelector('files-toggle-ripple');
    if (filesToggleRipple) {
      filesToggleRipple.activated = false;
    }

    this.exitButton_.hidden = false;

    this.currentMode_.cleanUpCaches();
    this.currentMode_ = null;
    this.currentTool_ = null;
  }

  /**
   * Leave the mode, commit only if required by the current mode.
   * @param {boolean} leaveToSwitchMode True if it leaves to switch mode.
   */
  leaveMode(leaveToSwitchMode) {
    this.leaveModeInternal_(
        !!this.currentMode_ && this.currentMode_.updated_ &&
            this.currentMode_.implicitCommit,
        leaveToSwitchMode);
  }

  /**
   * Enter the editor mode with the given name.
   *
   * @param {string} name Mode name.
   * @private
   */
  enterModeByName_(name) {
    for (var i = 0; i !== this.modes_.length; i++) {
      var mode = this.modes_[i];
      if (mode.name === name) {
        if (!mode.button_.hasAttribute('disabled')) {
          this.enterMode(mode);
        }
        return;
      }
    }
    console.error('Mode "' + name + '" not found.');
  }

  /**
   * Key down handler.
   * @param {!Event} event The keydown event.
   * @return {boolean} True if handled.
   */
  onKeyDown(event) {
    if (this.currentMode_ && this.currentMode_.isConsumingKeyEvents()) {
      return false;
    }

    switch (util.getKeyModifiers(event) + event.key) {
      case 'Escape':
      case 'Enter':
        if (this.getMode()) {
          this.leaveModeInternal_(
              event.key === 'Enter', false /* not to switch mode */);
          return true;
        }
        break;

      case 'Ctrl-z':  // Ctrl+Z
        if (this.commandQueue_.canUndo()) {
          this.undo();
          return true;
        }
        break;

      case 'Ctrl-y':  // Ctrl+Y
        if (this.commandQueue_.canRedo()) {
          this.redo();
          return true;
        }
        break;

      case 'a':
        this.enterModeByName_('autofix');
        return true;

      case 'b':
        this.enterModeByName_('exposure');
        return true;

      case 'c':
        this.enterModeByName_('crop');
        return true;

      case 'l':
        this.enterModeByName_('rotate_left');
        return true;

      case 'r':
        this.enterModeByName_('rotate_right');
        return true;
    }
    return false;
  }

  /**
   * Double tap handler.
   * @param {number} x X coordinate of the event.
   * @param {number} y Y coordinate of the event.
   * @private
   */
  onDoubleTap_(x, y) {
    if (this.getMode()) {
      var action = this.buffer_.getDoubleTapAction(x, y);
      if (action === ImageBuffer.DoubleTapAction.COMMIT) {
        this.leaveModeInternal_(true, false /* not to switch mode */);
      } else if (action === ImageBuffer.DoubleTapAction.CANCEL) {
        this.leaveModeInternal_(false, false /* not to switch mode */);
      }
    }
  }

  /**
   * Called when the user starts editing image.
   */
  onStartEditing() {
    this.calculateModeApplicativity_();
  }
}

/**
 * A helper object for panning the ImageBuffer.
 */
ImageEditor.MouseControl = class {
  /**
   * @param {!HTMLElement} rootContainer The top-level container.
   * @param {!HTMLElement} container The container for mouse events.
   * @param {!ImageBuffer} buffer Image buffer.
   */
  constructor(rootContainer, container, buffer) {
    this.rootContainer_ = rootContainer;
    this.container_ = container;
    this.buffer_ = buffer;

    var handlers = {
      'touchstart': this.onTouchStart,
      'touchend': this.onTouchEnd,
      'touchcancel': this.onTouchCancel,
      'touchmove': this.onTouchMove,
      'mousedown': this.onMouseDown,
      'mouseup': this.onMouseUp
    };

    for (var eventName in handlers) {
      container.addEventListener(
          eventName, handlers[eventName].bind(this), false);
    }

    // Mouse move handler has to be attached to the window to receive events
    // from outside of the window. See: http://crbug.com/155705
    window.addEventListener('mousemove', this.onMouseMove.bind(this), false);

    /**
     * @type {?ImageBuffer.DragHandler}
     * @private
     */
    this.dragHandler_ = null;

    /**
     * @type {boolean}
     * @private
     */
    this.dragHappened_ = false;

    /**
     * @type {?{x: number, y: number, time:number}}
     * @private
     */
    this.touchStartInfo_ = null;

    /**
     * @type {?{x: number, y: number, time:number}}
     * @private
     */
    this.previousTouchStartInfo_ = null;
  }

  /**
   * Returns an event's position.
   *
   * @param {!(MouseEvent|Touch)} e Pointer position.
   * @return {!Object} A pair of x,y in page coordinates.
   * @private
   */
  static getPosition_(e) {
    return {x: e.pageX, y: e.pageY};
  }

  /**
   * Returns touch position or null if there is more than one touch position.
   *
   * @param {!TouchEvent} e Event.
   * @return {Object?} A pair of x,y in page coordinates.
   * @private
   */
  getTouchPosition_(e) {
    if (e.targetTouches.length == 1) {
      return ImageEditor.MouseControl.getPosition_(e.targetTouches[0]);
    } else {
      return null;
    }
  }

  /**
   * Touch start handler.
   * @param {!TouchEvent} e Event.
   */
  onTouchStart(e) {
    var position = this.getTouchPosition_(e);
    if (position) {
      this.touchStartInfo_ = {x: position.x, y: position.y, time: Date.now()};
      this.dragHandler_ =
          this.buffer_.getDragHandler(position.x, position.y, true /* touch */);
      this.dragHappened_ = false;
    }
  }

  /**
   * Touch end handler.
   * @param {!TouchEvent} e Event.
   */
  onTouchEnd(e) {
    if (!this.dragHappened_ && this.touchStartInfo_ &&
        Date.now() - this.touchStartInfo_.time <=
            ImageEditor.MouseControl.MAX_TAP_DURATION_) {
      this.buffer_.onClick(this.touchStartInfo_.x, this.touchStartInfo_.y);
      if (this.previousTouchStartInfo_ &&
          Date.now() - this.previousTouchStartInfo_.time <
              ImageEditor.MouseControl.MAX_DOUBLE_TAP_DURATION_) {
        var prevTouchCircle = new Circle(
            this.previousTouchStartInfo_.x, this.previousTouchStartInfo_.y,
            ImageEditor.MouseControl.MAX_DISTANCE_FOR_DOUBLE_TAP_);
        if (prevTouchCircle.inside(
                this.touchStartInfo_.x, this.touchStartInfo_.y)) {
          this.doubleTapCallback_(
              this.touchStartInfo_.x, this.touchStartInfo_.y);
        }
      }
      this.previousTouchStartInfo_ = this.touchStartInfo_;
    } else {
      this.previousTouchStartInfo_ = null;
    }
    this.onTouchCancel();
  }

  /**
   * Default double tap handler.
   * @param {number} x X coordinate of the event.
   * @param {number} y Y coordinate of the event.
   * @private
   */
  doubleTapCallback_(x, y) {}

  /**
   * Sets callback to be called when double tap detected.
   * @param {function(number, number)} callback New double tap callback.
   */
  setDoubleTapCallback(callback) {
    this.doubleTapCallback_ = callback;
  }

  /**
   * Touch cancel handler.
   */
  onTouchCancel() {
    this.dragHandler_ = null;
    this.dragHappened_ = false;
    this.touchStartInfo_ = null;
    this.lockMouse_(false);
  }

  /**
   * Touch move handler.
   * @param {!TouchEvent} e Event.
   */
  onTouchMove(e) {
    var position = this.getTouchPosition_(e);
    if (!position) {
      return;
    }

    if (this.touchStartInfo_ && !this.dragHappened_) {
      var tapCircle = new Circle(
          this.touchStartInfo_.x, this.touchStartInfo_.y,
          ImageEditor.MouseControl.MAX_MOVEMENT_FOR_TAP_);
      this.dragHappened_ = !tapCircle.inside(position.x, position.y);
    }
    if (this.dragHandler_ && this.dragHappened_) {
      this.dragHandler_(position.x, position.y, e.shiftKey);
      this.lockMouse_(true);
    }
  }

  /**
   * Mouse down handler.
   * @param {!MouseEvent} e Event.
   */
  onMouseDown(e) {
    var position = ImageEditor.MouseControl.getPosition_(e);

    this.dragHandler_ =
        this.buffer_.getDragHandler(position.x, position.y, false /* mouse */);
    this.dragHappened_ = false;
    this.updateCursor_(position);
  }

  /**
   * Mouse up handler.
   * @param {!MouseEvent} e Event.
   */
  onMouseUp(e) {
    var position = ImageEditor.MouseControl.getPosition_(e);

    if (!this.dragHappened_) {
      this.buffer_.onClick(position.x, position.y);
    }
    this.dragHandler_ = null;
    this.dragHappened_ = false;
    this.lockMouse_(false);
  }

  /**
   * Mouse move handler.
   * @param {!Event} e Event.
   */
  onMouseMove(e) {
    e = assertInstanceof(e, MouseEvent);
    var position = ImageEditor.MouseControl.getPosition_(e);

    if (this.dragHandler_ && !e.which) {
      // mouseup must have happened while the mouse was outside our window.
      this.dragHandler_ = null;
      this.lockMouse_(false);
    }

    this.updateCursor_(position);
    if (this.dragHandler_) {
      this.dragHandler_(position.x, position.y, e.shiftKey);
      this.dragHappened_ = true;
      this.lockMouse_(true);
    }
  }

  /**
   * Update the UI to reflect mouse drag state.
   * @param {boolean} on True if dragging.
   * @private
   */
  lockMouse_(on) {
    ImageUtil.setAttribute(this.rootContainer_, 'mousedrag', on);
  }

  /**
   * Update the cursor.
   *
   * @param {!Object} position An object holding x and y properties.
   * @private
   */
  updateCursor_(position) {
    var oldCursor = this.container_.getAttribute('cursor');
    var newCursor = this.buffer_.getCursorStyle(
        position.x, position.y, !!this.dragHandler_);
    if (newCursor != oldCursor) {  // Avoid flicker.
      this.container_.setAttribute('cursor', newCursor);
    }
  }
};

/**
 * Maximum movement for touch to be detected as a tap (in pixels).
 * @private
 * @const
 */
ImageEditor.MouseControl.MAX_MOVEMENT_FOR_TAP_ = 8;

/**
 * Maximum time for touch to be detected as a tap (in milliseconds).
 * @private
 * @const
 */
ImageEditor.MouseControl.MAX_TAP_DURATION_ = 500;

/**
 * Maximum distance from the first tap to the second tap to be considered
 * as a double tap.
 * @private
 * @const
 */
ImageEditor.MouseControl.MAX_DISTANCE_FOR_DOUBLE_TAP_ = 32;

/**
 * Maximum time for touch to be detected as a double tap (in milliseconds).
 * @private
 * @const
 */
ImageEditor.MouseControl.MAX_DOUBLE_TAP_DURATION_ = 1000;
