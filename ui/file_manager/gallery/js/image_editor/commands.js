// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* eslint-disable no-var */

/**
 * Command queue is the only way to modify images.
 * Supports undo/redo.
 * Command execution is asynchronous (callback-based).
 *
 * @param {!Document} document Document to create canvases in.
 * @param {!HTMLCanvasElement|!HTMLImageElement} image The canvas with the
 *    original image.
 * @param {function(function())} saveFunction Function to save the image.
 * @constructor
 * @struct
 */
function CommandQueue(document, image, saveFunction) {
  this.document_ = document;
  this.undo_ = [];
  this.redo_ = [];
  this.subscribers_ = [];

  /**
   * @type {HTMLCanvasElement|HTMLImageElement}
   * @private
   */
  this.currentImage_ = image;

  /**
   * @type {HTMLCanvasElement|HTMLImageElement}
   * @private
   */
  this.baselineImage_ = image;

  /**
   * @type {HTMLCanvasElement|HTMLImageElement}
   * @private
   */
   this.previousImage_ = null;

  this.saveFunction_ = saveFunction;
  this.busy_ = false;
  this.UIContext_ = {};
}

/**
 * Attach the UI elements to the command queue.
 * Once the UI is attached the results of image manipulations are displayed.
 *
 * @param {!ImageView} imageView The ImageView object to display the results.
 * @param {!ImageEditorPrompt} prompt Prompt to use with this CommandQueue.
 * @param {function()} updateUndoRedo Function to update undo and redo buttons
 *     state.
 * @param {function(boolean)} lock Function to enable/disable buttons etc.
 */
CommandQueue.prototype.attachUI = function(
    imageView, prompt, updateUndoRedo, lock) {
  this.UIContext_ = {
    imageView: imageView,
    prompt: prompt,
    updateUndoRedo: updateUndoRedo,
    lock: lock
  };
};

/**
 * Execute the action when the queue is not busy.
 * @param {function()} callback Callback.
 */
CommandQueue.prototype.executeWhenReady = function(callback) {
  if (this.isBusy()) {
    this.subscribers_.push(callback);
  } else {
    setTimeout(callback, 0);
  }
};

/**
 * @return {boolean} True if the command queue is busy.
 */
CommandQueue.prototype.isBusy = function() {
  return this.busy_;
};

/**
 * Set the queue state to busy. Lock the UI.
 * @private
 */
CommandQueue.prototype.setBusy_ = function() {
  if (this.busy_) {
    throw new Error('CommandQueue already busy');
  }

  this.busy_ = true;

  if (this.UIContext_.lock) {
    this.UIContext_.lock(true);
  }

  ImageUtil.trace.resetTimer('command-busy');
};

/**
 * Set the queue state to not busy. Unlock the UI and execute pending actions.
 * @private
 */
CommandQueue.prototype.clearBusy_ = function() {
  if (!this.busy_) {
    throw new Error('Inconsistent CommandQueue already not busy');
  }

  this.busy_ = false;

  // Execute the actions requested while the queue was busy.
  while (this.subscribers_.length) {
    this.subscribers_.shift()();
  }

  if (this.UIContext_.lock) {
    this.UIContext_.lock(false);
  }

  ImageUtil.trace.reportTimer('command-busy');
};

/**
 * Commit the image change: save and unlock the UI.
 * @param {boolean} showUndoAction True to show undo action in the toast.
 * @param {number=} opt_delay Delay in ms (to avoid disrupting the animation).
 * @private
 */
CommandQueue.prototype.commit_ = function(showUndoAction, opt_delay) {
  setTimeout(this.saveFunction_.bind(null, this.clearBusy_.bind(this)),
      opt_delay || 0);
};

/**
 * Internal function to execute the command in a given context.
 *
 * @param {!Command} command The command to execute.
 * @param {!Object} uiContext The UI context.
 * @param {function(number=)} callback Completion callback.
 * @private
 */
CommandQueue.prototype.doExecute_ = function(command, uiContext, callback) {
  if (!this.currentImage_) {
    throw new Error('Cannot operate on null image');
  }

  command.execute(
      this.document_,
      this.currentImage_,
      /**
       * @type {function(HTMLCanvasElement, number=)}
       */
      (function(result, opt_delay) {
        this.previousImage_ = this.currentImage_;
        this.currentImage_ = result;
        callback(opt_delay);
      }.bind(this)),
      uiContext);
};

/**
 * Executes the command.
 *
 * @param {!Command} command Command to execute.
 * @param {boolean=} opt_keep_redo True if redo stack should not be cleared.
 */
CommandQueue.prototype.execute = function(command, opt_keep_redo) {
  this.setBusy_();

  if (!opt_keep_redo) {
    this.redo_ = [];
  }

  this.undo_.push(command);

  this.doExecute_(command, this.UIContext_,
      this.commit_.bind(this, true /* Show undo action */));
};

/**
 * @return {boolean} True if Undo is applicable.
 */
CommandQueue.prototype.canUndo = function() {
  return this.undo_.length != 0;
};

/**
 * Undo the most recent command.
 */
CommandQueue.prototype.undo = function() {
  if (!this.canUndo()) {
    throw new Error('Cannot undo');
  }

  this.setBusy_();

  var command = this.undo_.pop();
  this.redo_.push(command);

  var self = this;

  function complete() {
    var delay = command.revertView(
        self.currentImage_, self.UIContext_.imageView);
    self.commit_(false /* Do not show undo action */, delay);
  }

  if (this.previousImage_) {
    // First undo after an execute call.
    this.currentImage_ = this.previousImage_;
    this.previousImage_ = null;

    complete();
    // TODO(kaznacheev) Consider recalculating previousImage_ right here
    // by replaying the commands in the background.
  } else {
    this.currentImage_ = this.baselineImage_;

    var replay = function(index) {
      if (index < self.undo_.length) {
        self.doExecute_(self.undo_[index], {}, replay.bind(null, index + 1));
      } else {
        complete();
      }
    };

    replay(0);
  }
};

/**
 * @return {boolean} True if Redo is applicable.
 */
CommandQueue.prototype.canRedo = function() {
  return this.redo_.length != 0;
};

/**
 * Repeat the command that was recently un-done.
 */
CommandQueue.prototype.redo = function() {
  if (!this.canRedo()) {
    throw new Error('Cannot redo');
  }

  this.execute(this.redo_.pop(), true);
};

/**
 * Closes internal buffers. Call to ensure, that internal buffers are freed
 * as soon as possible.
 */
CommandQueue.prototype.close = function() {
  // Free memory used by the undo buffer.
  this.currentImage_ = null;
  this.previousImage_ = null;
  this.baselineImage_ = null;
};

/**
 * Command object encapsulates an operation on an image and a way to visualize
 * its result.
 *
 * @param {string} name Command name.
 * @constructor
 * @struct
 */
function Command(name) {
  this.name_ = name;
}

/**
 * @return {string} String representation of the command.
 */
Command.prototype.toString = function() {
  return 'Command ' + this.name_;
};

/**
 * Execute the command and visualize its results.
 *
 * The two actions are combined into one method because sometimes it is nice
 * to be able to show partial results for slower operations.
 *
 * @param {!Document} document Document on which to execute command.
 * @param {!HTMLCanvasElement|!HTMLImageElement} srcImage Image to execute on.
 *    Do NOT modify this object.
 * @param {function(HTMLCanvasElement, number=)} callback Callback to call on
 *   completion.
 * @param {!Object} uiContext Context to work in.
 */
Command.prototype.execute = function(document, srcImage, callback, uiContext) {
  console.error('Command.prototype.execute not implemented');
};

/**
 * Visualize reversion of the operation.
 *
 * @param {!HTMLCanvasElement|!HTMLCanvasElement} image previous image.
 * @param {!ImageView} imageView ImageView to revert.
 * @return {number} Animation duration in ms.
 */
Command.prototype.revertView = function(image, imageView) {
  imageView.replace(image);
  return 0;
};

/**
 * Creates canvas to render on.
 *
 * @param {!Document} document Document to create canvas in.
 * @param {!HTMLCanvasElement|!HTMLImageElement} srcImage to copy optional
 *    dimensions from.
 * @param {number=} opt_width new canvas width.
 * @param {number=} opt_height new canvas height.
 * @return {!HTMLCanvasElement} Newly created canvas.
 * @private
 */
Command.prototype.createCanvas_ = function(
    document, srcImage, opt_width, opt_height) {
  var result = assertInstanceof(document.createElement('canvas'),
      HTMLCanvasElement);
  result.width = opt_width || srcImage.width;
  result.height = opt_height || srcImage.height;
  return result;
};


/**
 * Rotate command
 * @param {number} rotate90 Rotation angle in 90 degree increments (signed).
 * @constructor
 * @extends {Command}
 * @struct
 */
Command.Rotate = function(rotate90) {
  Command.call(this, 'rotate(' + rotate90 * 90 + 'deg)');
  this.rotate90_ = rotate90;
};

Command.Rotate.prototype = { __proto__: Command.prototype };

/** @override */
Command.Rotate.prototype.execute = function(
    document, srcImage, callback, uiContext) {
  var result = this.createCanvas_(
      document,
      srcImage,
      (this.rotate90_ & 1) ? srcImage.height : srcImage.width,
      (this.rotate90_ & 1) ? srcImage.width : srcImage.height);
  ImageUtil.drawImageTransformed(
      result, srcImage, 1, 1, this.rotate90_ * Math.PI / 2);
  var delay;
  if (uiContext.imageView) {
    delay = uiContext.imageView.replaceAndAnimate(result, null, this.rotate90_);
  }
  setTimeout(callback, 0, result, delay);
};

/** @override */
Command.Rotate.prototype.revertView = function(image, imageView) {
  return imageView.replaceAndAnimate(image, null, -this.rotate90_);
};


/**
 * Crop command.
 *
 * @param {!ImageRect} imageRect Crop rectangle in image coordinates.
 * @constructor
 * @extends {Command}
 * @struct
 */
Command.Crop = function(imageRect) {
  Command.call(this, 'crop' + imageRect.toString());
  this.imageRect_ = imageRect;
};

Command.Crop.prototype = { __proto__: Command.prototype };

/** @override */
Command.Crop.prototype.execute = function(
    document, srcCanvas, callback, uiContext) {
  var result = this.createCanvas_(
      document, srcCanvas, this.imageRect_.width, this.imageRect_.height);
  var ctx = assertInstanceof(result.getContext('2d'), CanvasRenderingContext2D);
  ImageRect.drawImage(ctx, srcCanvas, null, this.imageRect_);
  var delay;
  if (uiContext.imageView) {
    delay = uiContext.imageView.replaceAndAnimate(result, this.imageRect_, 0);
  }
  setTimeout(callback, 0, result, delay);
};

/** @override */
Command.Crop.prototype.revertView = function(image, imageView) {
  return imageView.animateAndReplace(image, this.imageRect_);
};


/**
 * Filter command.
 *
 * @param {string} name Command name.
 * @param {function(!ImageData,!ImageData,number,number)} filter Filter
 *     function.
 * @param {?string} message Message to display when done.
 * @constructor
 * @extends {Command}
 * @struct
 */
Command.Filter = function(name, filter, message) {
  Command.call(this, name);
  this.filter_ = filter;
  this.message_ = message;
};

Command.Filter.prototype = { __proto__: Command.prototype };

/** @override */
Command.Filter.prototype.execute = function(
    document, srcImage, callback, uiContext) {
  var result = this.createCanvas_(document, srcImage);
  var self = this;
  var previousRow = 0;

  function onProgressVisible(updatedRow, rowCount) {
    if (updatedRow == rowCount) {
      uiContext.imageView.replace(result);
      if (self.message_) {
        uiContext.prompt.show(self.message_, 2000);
      }
      callback(result);
    } else {
      var viewport = uiContext.imageView.viewport_;

      var imageStrip = ImageRect.createFromBounds(viewport.getImageBounds());
      imageStrip.top = previousRow;
      imageStrip.height = updatedRow - previousRow;

      var screenStrip = ImageRect.createFromBounds(
          viewport.getImageBoundsOnScreen());
      screenStrip.top = Math.round(viewport.imageToScreenY(previousRow));
      screenStrip.height =
          Math.round(viewport.imageToScreenY(updatedRow)) - screenStrip.top;

      uiContext.imageView.paintDeviceRect(result, imageStrip);
      previousRow = updatedRow;
    }
  }

  function onProgressInvisible(updatedRow, rowCount) {
    if (updatedRow == rowCount) {
      callback(result);
    }
  }

  filter.applyByStrips(result, srcImage, this.filter_,
      uiContext.imageView ? onProgressVisible : onProgressInvisible);
};

/**
 * Resize Command
 * @param {number} inputWidth width user input
 * @param {number} inputHeight height user input
 * @constructor
 * @extends {Command}
 * @struct
 */
Command.Resize = function(inputWidth, inputHeight) {
  Command.call(this, 'resize(x:' + inputWidth + ',y:' + inputHeight + ')');
  this.newWidth_ = inputWidth;
  this.newHeight_ = inputHeight;
};

Command.Resize.prototype = {__proto__: Command.prototype};

/** @override */
Command.Resize.prototype.execute = function(
    document, srcImage, callback, uiContext) {
  var result = this.createCanvas_(
    document, srcImage, this.newWidth_, this.newHeight_);

  var scaleX = this.newWidth_ / srcImage.width;
  var scaleY = this.newHeight_ / srcImage.height;
  ImageUtil.drawImageTransformed(result, srcImage, scaleX, scaleY, 0);

  if (uiContext.imageView) {
    uiContext.imageView.replace(result);
  }
  callback(result);
};
