// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Represents a single frame, and contains all associated data.
//
class DrawFrame {
  static instances = [];

  static count() { return DrawFrame.instances.length; }

  static get(index) {
    const ins = DrawFrame.instances;
    if (index < 0) return ins[index];
    if (index >= ins.length) return ins[ins.length - 1];
    return ins[index];
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

    DrawFrame.instances.push(this);
  }

  submissionCount() {
    return this.drawCalls_.length + this.drawTexts_.length + this.logs_.length;
  }

  submissionFreezeIndex() {
    return this.submissionFreezeIndex_ >= 0 ? (this.submissionFreezeIndex_) :
      (this.submissionCount() - 1);
  }

  updateCanvasSize(canvas, scale) {
    canvas.width = this.size_.width * scale;
    canvas.height = this.size_.height * scale;
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

  draw(canvas, scale) {
    for (const call of this.drawCalls_) {
      if (call.drawIndex_ > this.submissionFreezeIndex()) break;

      call.draw(canvas, scale);
    }

    canvas.fillStyle = 'black';
    canvas.font = "16px Courier bold";
    canvas.fillText(this.num_, 3, 15);

    for (const text of this.drawTexts_) {
      if (text.drawindex > this.submissionFreezeIndex()) break;

      let filter = this.getFilter(text.source_index);
      if (!filter) continue;

      var color = (filter && filter.drawColor) ?
        filter.drawColor : text.option.color;
      canvas.fillStyle = color;
      // TODO: This should also create some DrawText object or something.
      canvas.fillText(text.text, text.pos[0] * scale, text.pos[1] * scale);
    }
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
    this.drawContext_ = this.canvas_.getContext('2d');

    this.currentFrameIndex_ = -1;
    this.viewScale = 1.0;
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

  redrawCurrentFrame_() {
    const frame = this.getCurrentFrame();
    if (!frame) return;
    frame.updateCanvasSize(this.canvas_, this.viewScale);
    frame.draw(this.drawContext_, this.viewScale);
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
