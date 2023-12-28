// Copyright 2022 The Chromium Authors
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

  oldestIndex() {
    if (this.numFrames <= this.maxSize) {
      return 0;
    } else {
      return this.numFrames - this.maxSize;
    }
  }

  newestIndex() {
    return this.numFrames -  1;
  }
}

// Represents a single frame, and contains all associated data.
//
class DrawFrame {
  // Circular buffer supports 1 minute of frames.
  static maxBufferNumFrames = 60*60;
  static frameBuffer = new CircularBuffer(DrawFrame.maxBufferNumFrames);
  static buffer_map = new Object();
  static demo_thread = {
    thread_name: "demo thread",
    thread_id: -1,
  };
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
    this.drawCalls_ = json.drawcalls.map(c => new DrawCall(c));
    this.buffer_map = json.buff_map;
    this.resetFilter();

    this.threadMapping_ = {}

    if (!('threads' in json)) {
      json.threads = [DrawFrame.demo_thread];
    }
    json.threads.forEach(t => {
      // If new thread has not been registered yet, then register it.
      if (!(Thread.isThreadRegistered(t.thread_name))) {
        new Thread(t);
      };
      // Map thread id's to all the thread information.
      // Values are set by default when frame first comes in.
      this.threadMapping_[t.thread_id] = {threadName: t.thread_name,
                                           threadEnabled: true,
                                           overrideFilters: false,
                                           threadColor: "#000000",
                                           threadAlpha: "10"};
    });

    if (json.new_sources) {
      for (const s of json.new_sources) {
        new Source(s);
        notifyUiOfNewSource(s);
      }
    }

    for (let buff in this.buffer_map) {
      // |buffer_map| contains data URIs, which we |fetch| to get a |Blob| to
      // create an |ImageBitmap| with.
      fetch(this.buffer_map[buff])
        .then((res) => res.blob())
        .then((blob) => createImageBitmap(blob))
        .then((res) => {
          DrawFrame.buffer_map[buff] = res;
          return res;
        });
    }

    // Retain the original JSON, so that the file can be saved to local disk.
    // Ideally, the JSON would be constructed on demand, but generating
    // |new_sources| requires some work. So for now, do the easy thing.
    this.json_ = json;

    DrawFrame.frameBuffer.push(this);
  }

  submissionCount() {
    return this.drawCalls_.length + this.logs_.length;
  }

  updateCanvasSize(canvas, context, scale, orientationDeg) {
    // Swap canvas width/height for 90 or 270 deg rotations
    if (orientationDeg === 90 || orientationDeg === 270) {
      canvas.width = this.size_.height * scale;
      canvas.height = this.size_.width * scale;
    }
    // Restore original canvas width/height for 0 or 180 deg rotations
    else {
      canvas.width = this.size_.width * scale;
      canvas.height = this.size_.height * scale;
    }
    // Some text can be drawn past the canvas boundaries, so add some padding on
    // each side.
    const padding = 20;
    canvas.width += padding * 2;
    canvas.height += padding * 2;

    // Fill the actual frame bounds to an opaque color.
    context.save();
    context.fillStyle = "white";
    context.fillRect(
      padding,
      padding,
      canvas.width - padding * 2,
      canvas.height - padding * 2
    );
    context.restore();
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

  draw(canvas, context, scale, orientationDeg) {
    // Look at global state of all threads and copy those states
    // to the current frame's threadID-to-state mapping.
    for (const threadId of Object.keys(this.threadMapping_)) {
      const mappedThread = this.threadMapping_[threadId];
      mappedThread.threadEnabled =
              Thread.getThread(mappedThread.threadName).enabled_;
      mappedThread.threadColor =
              Thread.getThread(mappedThread.threadName).drawColor_;
      mappedThread.threadAlpha =
              Thread.getThread(mappedThread.threadName).fillAlpha_;
      mappedThread.overrideFilters =
              Thread.getThread(mappedThread.threadName).overrideFilters_;
    }

    // Generate a transform from frame space to canvas space.
    context.translate(canvas.width / 2, canvas.height / 2);
    if (orientationDeg === FlipEnum.HorizontalFlip.id) {
      context.scale(-1, 1);
    } else if (orientationDeg === FlipEnum.VerticalFlip.id) {
      context.scale(1, -1);
    } else {
      context.rotate(orientationDeg * Math.PI / 180);
    }
    context.scale(scale, scale);
    context.translate(-this.size_.width / 2, -this.size_.height / 2);

    for (const call of this.drawCalls_) {
      // Assumed to be a positional text call.
      if (call.text) {
        continue;
      }
      if (!this.withinFilter(call.drawIndex_)) {
        continue;
      }

      // If thread not enabled, then skip draw call from this thread.
      if (!this.threadMapping_[call.threadId_].threadEnabled) {
        continue;
      }

      call.draw(context, DrawFrame.buffer_map,
                this.threadMapping_[call.threadId_]);
    }

    // Get the current transform so that we can draw text in the right position
    // without rotating or reflecting it.
    const transformMatrix = context.getTransform();
    context.resetTransform();

    context.font = "16px 'Courier bold', monospace";

    // Draw the frame number
    {
      context.textBaseline = "bottom";
      context.fillStyle = "black";
      var newTextPos = transformMatrix.transformPoint(new DOMPoint(0, 0));
      context.fillText(this.num_, newTextPos.x, newTextPos.y);
    }


    for (const text of this.drawCalls_) {
      // Not a positional text call.
      if (!text.text) {
        continue;
      }
      // If thread not enabled, then skip text calls from this thread.
      if (!this.threadMapping_[text.threadId_].threadEnabled) {
        continue;
      }
      if (!this.withinFilter(text.drawIndex_)) {
        continue;
      }

      var color;
      // If thread is overriding, take thread color.
      if (this.threadMapping_[text.threadId_].overrideFilters) {
        color = this.threadMapping_[text.threadId_].threadColor;
      }
      // Otherwise, take filter's color.
      else {
        let filter = this.getFilter(text.sourceIndex_);
        if (!filter) continue;

        color = (filter && filter.drawColor) ?
          filter.drawColor : text.color_;
      }
      context.fillStyle = color;
      // TODO: This should also create some DrawText object or something.
      this.drawText(context,
                    text.text,
                    text.pos_.x,
                    text.pos_.y,
                    transformMatrix);
    }
  }

  // Draw text with a transformed position.
  drawText(context, text, posX, posY, transformMatrix) {
    // TODO: Set the text alignment based on the transform.
    var newTextPos = transformMatrix.transformPoint(new DOMPoint(posX, posY));

    // Make the origin of text the top-left, similar to rectangles.
    context.textBaseline = "top";

    // Fill a background rectangle behind the text with the current fill color.
    const measure = context.measureText(text);
    context.fillRect(
      newTextPos.x,
      newTextPos.y,
      measure.width,
      measure.actualBoundingBoxDescent - measure.actualBoundingBoxAscent
    );

    function perceptualBrightness(hexColor) {
      const r = parseInt(hexColor.substr(1, 2), 16) / 255;
      const g = parseInt(hexColor.substr(3, 2), 16) / 255;
      const b = parseInt(hexColor.substr(5, 2), 16) / 255;
      return Math.sqrt(
        0.299 * Math.pow(r, 2) + 0.587 * Math.pow(g, 2) + 0.114 * Math.pow(b, 2)
      );
    }

    // Attempt to make the text contrast better against the background.
    if (perceptualBrightness(context.fillStyle) > 0.65) {
      context.fillStyle = "black";
    } else {
      context.fillStyle = "white";
    }

    context.fillText(text, newTextPos.x, newTextPos.y);
  }

  appendLogs(logContainer) {
    for (const log of this.logs_) {
      if (!this.withinFilter(log.drawindex)) {
        continue;
      }

      if (!('thread_id' in log)) {
        log.thread_id = DrawFrame.demo_thread.thread_id;
      }
      // If thread not enabled, then skip draw call from this thread.
      if (!this.threadMapping_[log.thread_id].threadEnabled) {
        continue;
      }

      var color;
      let filter;
      // If thread is overriding, take thread color.
      if (this.threadMapping_[log.thread_id].overrideFilters) {
        color = this.threadMapping_[log.thread_id].threadColor;
      }
      // Otherwise, take filter's color.
      else {
        filter = this.getFilter(log.source_index);
        if (!filter) continue;

        color = (filter && filter.drawColor) ?
          filter.drawColor : log.option.color;
      }

      var container = document.createElement("span");
      var new_node = document.createTextNode(log.value);
      container.style.color = color;
      container.appendChild(new_node)
      logContainer.appendChild(container);
      logContainer.appendChild(document.createElement('br'));
    }
  }

  resetFilter() {
    this.filter(-1, -1);
  }

  filter(minIndex, maxIndex) {
    this.minIndex_ = minIndex === -1 ? 0 : minIndex;
    this.maxIndex_ = maxIndex === -1 ? this.submissionCount() : maxIndex;
  }

  minIndex() {
    return this.minIndex_;
  }

  maxIndex() {
    return this.maxIndex_;
  }

  // True iff drawIndex is in [minIndex_, maxIndex).
  withinFilter(drawIndex) {
    return drawIndex >= this.minIndex_ && drawIndex < this.maxIndex_;
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
    this.translationX = 0;
    this.translationY = 0;
  }

  updateCurrentFrame() {
    this.redrawCurrentFrame_();
    this.updateLogs_();
  }

  redrawCurrentFrame_() {
    const frame = this.getCurrentFrame();
    if (!frame) return;
    frame.updateCanvasSize(this.canvas_,
                           this.drawContext_,
                           this.viewScale,
                           this.viewOrientation);
    frame.draw(this.canvas_,
               this.drawContext_,
               this.viewScale,
               this.viewOrientation);
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

  setFrame(frameIndex, minIndex = -1, maxIndex = -1) {
    if (DrawFrame.get(frameIndex)) {
      this.currentFrameIndex_ = frameIndex;
      this.getCurrentFrame().filter(minIndex, maxIndex);
      this.updateCurrentFrame();
    }
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

  constructor(viewer, updateUi) {
    this.viewer_ = viewer;
    this.paused_ = false;
    this.nextFrameScheduled_ = false;
    this.live_ = true;
    this.updateUi_ = updateUi;

    Player.instances[0] = this;
  }

  play() {
    this.paused_ = false;
    if (this.nextFrameScheduled_) return;

    if (this.viewer_.currentFrameIndex == DrawFrame.frameBuffer.newestIndex()) {
      return;
    }

    if (this.live_) {
      this.drawNewestFrame_();
    } else {
      this.drawNextFrame_();
    }

    this.didDrawNewFrame_();

    this.nextFrameScheduled_ = true;
    requestAnimationFrame(() => {
      this.nextFrameScheduled_ = false;
      if (!this.paused_)
        this.play();
    });
  }

  live() {
    this.live_ = true;
    this.play();
  }

  pause() {
    this.paused_ = true;
    this.live_ = false;
  }

  rewind() {
    this.pause();
    this.drawPreviousFrame_();
    this.didDrawNewFrame_();
  }

  forward() {
    this.pause();
    this.drawNextFrame_();
    this.didDrawNewFrame_();
  }

  // Pauses after drawing at most |drawIndex| number of calls of the
  // |frameIndex|-th frame.
  // Draws all calls if |minIndex| and |maxIndex| are not set.
  freezeFrame(frameIndex, minIndex = -1, maxIndex = -1) {
    this.pause();
    this.viewer_.setFrame(frameIndex, minIndex, maxIndex);
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

  drawNewestFrame_() {
    let newest = DrawFrame.frameBuffer.newestIndex();
    this.viewer_.setFrame(newest);
  }

  drawNextFrame_() {
    this.viewer_.setFrame(this.viewer_.currentFrameIndex + 1);
  }

  drawPreviousFrame_() {
    this.viewer_.setFrame(this.viewer_.currentFrameIndex - 1);
  }

  didDrawNewFrame_() {
    this.updateUi_(this.viewer_.getCurrentFrame());
  }

  get currentFrameIndex() { return this.viewer_.currentFrameIndex; }

  onNewFrame() {
    let oldest = DrawFrame.frameBuffer.oldestIndex();
    if (this.currentFrameIndex < oldest) {
      this.viewer_.setFrame(oldest, -1, -1);
    }
    this.didDrawNewFrame_();

    // If the player is not paused, and a new frame is received, then make sure
    // the next frame is drawn.
    if (!this.paused_) {
      this.play();
    }
  }

  static get instance() { return Player.instances[0]; }
};
