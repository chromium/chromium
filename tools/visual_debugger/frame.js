// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Enum class to identify horizontal or vertical flips
//
class FlipEnum {
  static HorizontalFlip = new FlipEnum(1);
  static VerticalFlip = new FlipEnum(2);

  constructor(id) {
    this.id = id;
  }
}
// Circular buffer to store the past X amount of frames.
//
class CircularBuffer {
  constructor(size) {
    this.instances = Array(size);
    this.maxSize = size;
    this.numFrames = 0;
  }

  get(index) {
    if (index < 0 || index < this.numFrames - this.maxSize ||
        index >= this.numFrames) {
      return undefined;
    }
    return this.instances[index % this.maxSize];
  }

  push(frame) {
    // Push frames into buffer
    this.instances[this.numFrames % this.maxSize] = frame;
    this.numFrames++;
  }
}

// Represents a single frame, and contains all associated data.
//
class DrawFrame {
  static maxBufferNumFrames = 10000;
  static frameBuffer = new CircularBuffer(DrawFrame.maxBufferNumFrames);

  static count() { return DrawFrame.frameBuffer.instances.length; }

  static get(index) {
    return DrawFrame.frameBuffer.get(index);
  }

  constructor(json) {
    this.num_ = parseInt(json.frame);
    this.size_ = {
      width: parseInt(json.windowx),
      height: parseInt(json.windowy),
    };
    this.logs_ = json.logs;
    this.drawTexts_ = json.text;
    this.drawCalls_ = json.drawcalls.map(c => new DrawCall(c));
    this.submissionFreezeIndex_ = -1;
    if (json.new_sources) {
      for (const s of json.new_sources) {
        new Source(s);
      }
    }

    // Retain the original JSON, so that the file can be saved to local disk.
    // Ideally, the JSON would be constructed on demand, but generating
    // |new_sources| requires some work. So for now, do the easy thing.
    this.json_ = json;

    DrawFrame.frameBuffer.push(this);

    // Update scrubber as new frames come in
    const scrubberFrame = document.querySelector('#scrubberframe');

    scrubberFrame.max = DrawFrame.frameBuffer.numFrames - 1;

    // Handle scrubber when # of frames reached cap of circular buffer.
    if (DrawFrame.frameBuffer.numFrames > DrawFrame.frameBuffer.maxSize) {
      const oldestFrameId = DrawFrame.frameBuffer.numFrames
                            - DrawFrame.frameBuffer.maxSize;

      scrubberFrame.min = oldestFrameId;
      // Once the scrubber reaches the left very (oldest frame),
      // update scrubber value to match scrubber min value and
      // update drawing on canvas to match correspondingly.
      if (scrubberFrame.value <= scrubberFrame.min) {
        scrubberFrame.value = oldestFrameId;
        Player.instance.forward();
      }
    }
    // Handle scrubber when # of frames haven't yet reached buffer cap.
    else {
      scrubberFrame.min = 0;
    }
  }

  submissionCount() {
    return this.drawCalls_.length + this.drawTexts_.length + this.logs_.length;
  }

  submissionFreezeIndex() {
    return this.submissionFreezeIndex_ >= 0 ? (this.submissionFreezeIndex_) :
      (this.submissionCount() - 1);
  }

  updateCanvasOrientation(canvas, orientationDeg) {
    // Swap canvas width/height for 90 or 270 deg rotations
    if (orientationDeg === 90 || orientationDeg === 270) {
      canvas.width = this.size_.height;
      canvas.height = this.size_.width;
    }
    // Restore original canvas width/height for 0 or 180 deg rotations
    else {
      canvas.width = this.size_.width;
      canvas.height = this.size_.height;
    }
  }

  updateCanvasSize(canvas, scale) {
    canvas.width *= scale;
    canvas.height *= scale;
  }

  getFilter(source_index) {
    const filters = Filter.enabledInstances();
    let filter = undefined;
    // TODO: multiple filters can match the same draw call. For now, let's just
    // pick the earliest filter that matches, and let it decide what to do.
    for (const f of filters) {
      if (f.matches(Source.instances[source_index])) {
        filter = f;
        break;
      }
    }

    // No filters match this draw. So skip.
    if (!filter) return undefined;
    if (!filter.shouldDraw) return undefined;

    return filter;
  }

  draw(canvas, context, scale, orientationDeg, transformMatrix) {
    for (const call of this.drawCalls_) {
      if (call.drawIndex_ > this.submissionFreezeIndex()) break;

      call.draw(canvas, context, scale, orientationDeg, transformMatrix);
    }

    context.fillStyle = 'black';
    context.font = "16px Courier bold";

    const frameNumberPosX = 3;
    const frameNumberPosY = 15;
    const newFrameNumPos = this.rotateFlipText(frameNumberPosX, frameNumberPosY,
                                        orientationDeg, scale, transformMatrix);

    context.fillText(this.num_, newFrameNumPos[0], newFrameNumPos[1]);

    for (const text of this.drawTexts_) {
      const textPosX = text.pos[0];
      const textPosY = text.pos[1];

      if (text.drawindex > this.submissionFreezeIndex()) break;

      let filter = this.getFilter(text.source_index);
      if (!filter) continue;

      const newTextPos = this.rotateFlipText(textPosX, textPosY,
                                              orientationDeg, scale,
                                              transformMatrix);

      var color = (filter && filter.drawColor) ?
        filter.drawColor : text.option.color;
      context.fillStyle = color;
      // TODO: This should also create some DrawText object or something.
      context.fillText(text.text, newTextPos[0], newTextPos[1]);
    }
  }

  // Rotates and flips texts
  rotateFlipText(textPosX, textPosY, orientationDeg, scale, transformMatrix) {
    var translationX = 0;
    var translationY = 0;

    // Determine amount of translation depending on orientation.
    // We want to put the texts back in frame.
    switch(orientationDeg) {
      default:
        break;
      case 90:
        translationX = canvas.width/scale;
        break;
      case 180:
        translationX = canvas.width/scale;
        translationY = canvas.height/scale;
        break;
      case 270:
        translationY = canvas.height/scale;
        break;
      case FlipEnum.HorizontalFlip.id:
        translationX = canvas.width/scale;
        break;
      case FlipEnum.VerticalFlip.id:
        translationY = canvas.height/scale;
        break;
    }

    var newTextPosX;
    var newTextPosY;
    // Use rotation/mirroring matrix to get rotated/flipped coords
    switch (orientationDeg) {
      default:
        newTextPosX = textPosX * transformMatrix[0][0] +
                      textPosY * transformMatrix[0][1] + translationX;
        newTextPosY = textPosX * transformMatrix[1][0] +
                      textPosY * transformMatrix[1][1] + translationY;
        break;
      case FlipEnum.HorizontalFlip.id:
        newTextPosX = -textPosX + translationX;
        newTextPosY = textPosY;
        break;
      case FlipEnum.VerticalFlip.id:
        newTextPosX = textPosX;
        newTextPosY = -textPosY + translationY;
        break;
    }
    return [newTextPosX * scale, newTextPosY * scale];
  }

  appendLogs(logContainer) {
    for (const log of this.logs_) {
      if (log.drawindex > this.submissionFreezeIndex()) break;

      let filter = this.getFilter(log.source_index);
      if (!filter) continue;

      var color = (filter && filter.drawColor) ?
        filter.drawColor : log.option.color;
      var container = document.createElement("span");
      var new_node = document.createTextNode(log.value);
      container.style.color = color;
      container.appendChild(new_node)
      logContainer.appendChild(container);
      logContainer.appendChild(document.createElement('br'));
    }
  }

  unfreeze() {
    this.submissionFreezeIndex_ = -1;
  }

  freeze(index) {
    this.submissionFreezeIndex_ = index;
  }

  toJSON() {
    return this.json_;
  }
}


// Controller for the viewer.
//
class Viewer {
  constructor(canvas, log) {
    this.canvas_ = canvas;
    this.logContainer_ = log;
    this.drawContext_ = this.canvas_.getContext("2d");

    this.currentFrameIndex_ = -1;
    this.viewScale = 1.0;
    this.viewOrientation = 0;
    this.transformMatrix = [[1,0],[0,1]]; // Identity matrix
    this.translationX = 0;
    this.translationY = 0;
  }

  updateCurrentFrame() {
    this.redrawCurrentFrame_();
    this.updateLogs_();
  }

  drawNextFrame() {
    // When we switch to a different frame, we need to unfreeze the current
    // frame (to make sure the frame draws completely the next time it is drawn
    // in the player).
    this.unfreeze();
    if (DrawFrame.get(this.currentFrameIndex_ + 1)) {
      ++this.currentFrameIndex_;
      this.updateCurrentFrame();
      return true;
    }
  }

  drawPreviousFrame() {
    // When we switch to a different frame, we need to unfreeze the current
    // frame (to make sure the frame draws completely the next time it is drawn
    // in the player).
    this.unfreeze();
    if (DrawFrame.get(this.currentFrameIndex_ - 1)) {
      --this.currentFrameIndex_;
      this.updateCurrentFrame();
    }
  }

  updateTransformMatrix(orientationDeg) {
    const orientationRad = orientationDeg * (Math.PI/180.0);
    // Clockwise rotation Matrix
    this.transformMatrix =
                      [[Math.cos(orientationRad), -Math.sin(orientationRad)],
                      [Math.sin(orientationRad), Math.cos(orientationRad)]];
  }

  redrawCurrentFrame_() {
    const frame = this.getCurrentFrame();
    if (!frame) return;
    frame.updateCanvasOrientation(this.canvas_, this.viewOrientation);
    frame.updateCanvasSize(this.canvas_, this.viewScale);
    this.updateTransformMatrix(this.viewOrientation);
    // this.drawContext_.translate(this.translationX, this.translationY);
    frame.draw(this.canvas_, this.drawContext_,
                this.viewScale, this.viewOrientation,
                this.transformMatrix);
  }

  updateLogs_() {
    this.logContainer_.textContent = '';
    const frame = this.getCurrentFrame();
    if (!frame) return;
    frame.appendLogs(this.logContainer_);
  }

  getCurrentFrame() {
    return DrawFrame.get(this.currentFrameIndex_);
  }

  get currentFrameIndex() { return this.currentFrameIndex_; }

  setViewerScale(scaleAsInt) {
    this.viewScale = scaleAsInt / 100.0;
  }

  setViewerOrientation(orientationAsInt) {
    this.viewOrientation = orientationAsInt;
  }

  freezeFrame(frameIndex, drawIndex) {
    if (DrawFrame.get(frameIndex)) {
      this.currentFrameIndex_ = frameIndex;
      this.getCurrentFrame().freeze(drawIndex);
      this.updateCurrentFrame();
    }
  }

  unfreeze() {
    const frame = this.getCurrentFrame();
    if (frame) frame.unfreeze();
  }

  zoomToMouse(currentMouseX, currentMouseY, delta) {
    var factor = 1.1;
    if (delta > 0) {
      factor = 0.9;
    }
    // this.translationX = currentMouseX;
    // this.translationY = currentMouseY;
    // this.updateCurrentFrame();
    this.viewScale *= factor;
    this.updateCurrentFrame();
    // this.translationX = -currentMouseX;
    // this.translationY = -currentMouseY;
    // this.updateCurrentFrame();
  }
};

// Controls the player.
//
class Player {
  static instances = [];

  constructor(viewer, draw_cb) {
    this.viewer_ = viewer;
    this.paused_ = false;
    this.nextFrameScheduled_ = false;

    this.drawCb_ = draw_cb;

    Player.instances[0] = this;
  }

  play() {
    this.paused_ = false;
    if (this.nextFrameScheduled_) return;

    const drawn = this.viewer_.drawNextFrame();
    this.didDrawNewFrame_();
    if (!drawn) return;

    this.nextFrameScheduled_ = true;
    requestAnimationFrame(() => {
      this.nextFrameScheduled_ = false;
      if (!this.paused_)
        this.play();
    });
  }

  pause() {
    this.paused_ = true;
  }

  rewind() {
    this.pause();
    this.viewer_.drawPreviousFrame();
    this.didDrawNewFrame_();
  }

  forward() {
    this.pause();
    this.viewer_.drawNextFrame();
    this.didDrawNewFrame_();
  }

  // Pauses after drawing at most |drawIndex| number of calls of the
  // |frameIndex|-th frame.
  // Draws all calls if |drawIndex| is not set.
  freezeFrame(frameIndex, drawIndex = -1) {
    this.pause();
    this.viewer_.freezeFrame(parseInt(frameIndex), parseInt(drawIndex));
    this.didDrawNewFrame_();
  }

  setViewerScale(scaleAsString) {
    this.viewer_.setViewerScale(parseInt(scaleAsString));
    this.refresh();
  }

  setViewerOrientation(orientationAsString) {
    // Set orientationAsInt as selected orientation degree
    // Horizontal Flip enum or Vertical Flip enum
    const orientationAsInt = parseInt(orientationAsString) >= 0 ?
      parseInt(orientationAsString) :
      (orientationAsString === "Horizontal Flip" ?
          FlipEnum.HorizontalFlip.id : FlipEnum.VerticalFlip.id);

    this.viewer_.setViewerOrientation(orientationAsInt);
    this.refresh();
  }

  refresh() {
    this.viewer_.updateCurrentFrame();
  }

  didDrawNewFrame_() {
    this.drawCb_(this.viewer_.getCurrentFrame());
  }

  get currentFrameIndex() { return this.viewer_.currentFrameIndex; }

  onNewFrame() {
    // If the player is not paused, and a new frame is received, then make sure
    // the next frame is drawn.
    if (!this.paused_) this.play();
  }

  static get instance() { return Player.instances[0]; }
};
