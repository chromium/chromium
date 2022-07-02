// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Represents a source for a draw call, or a log, etc.
//
class Source {
  static instances = [];

  constructor(json) {
    this.file_ = json.file;
    this.func_ = json.func;
    this.line_ = json.line;
    this.anno_ = json.anno;
    const index = parseInt(json.index);
    Source.instances[index] = this;
  }

  get file() { return this.file_; }
  get func() { return this.func_; }
  get anno() { return this.anno_; }
};

// Represents a draw call.
// This is currently only used for drawing rect. But this (or something like
// this) could potentially be also used for drawing logs etc.
//
class DrawCall {
  constructor(json) {
    // e.g. {"drawindex":"44","option":{"alpha":"1.000000","color":"#ffffff"}
    // ,"pos":"0.000000,763.000000","size":"255x5","source_index":"0"}
    this.sourceIndex_ = parseInt(json.source_index);
    this.drawIndex_ = parseInt(json.drawindex);
    this.size_ = {
      width: json.size[0],
      height: json.size[1],
    };

    this.pos_ = {
      x: json.pos[0],
      y: json.pos[1],
    };
    if (json.option) {
      this.color_ = json.option.color;
      this.alpha_ = DrawCall.alphaIntToHex(json.option.alpha)
    }
  }

  // Used in conversion of Json.
  static alphaIntToHex(value) {
    value = Math.trunc(value);
    value = Math.max(0, Math.min(value, 255));
    return value.toString(16).padStart(2, '0');
  }

  // Used internally to convert from UI filter to hex.
  static alphaFloatToHex(value) {
    value = Math.trunc(value * 255);
    value = Math.max(0, Math.min(value, 255));
    return value.toString(16).padStart(2, '0');
  }

  draw(canvas, context, scale, orientationDeg, transformMatrix) {
    let filter = undefined;
    const filters = Filter.enabledInstances();
    // TODO: multiple filters can match the same draw call. For now, let's just
    // pick the earliest filter that matches, and let it decide what to do.
    for (const f of filters) {
      if (f.matches(Source.instances[this.sourceIndex_])) {
        filter = f;
        break;
      }
    }

    // No filters match this draw. So skip.
    if (!filter) return;
    if (!filter.shouldDraw) return;

    var color = (filter && filter.drawColor) ? filter.drawColor : this.color_
    var alpha = (filter && filter.fillAlpha) ?
    DrawCall.alphaFloatToHex(parseFloat(filter.fillAlpha) / 100) : this.alpha_;

    const newCallPosAndDimension = this.rotateCall(canvas, orientationDeg,
                                                      scale, transformMatrix);

    if (color && alpha) {
      context.fillStyle = color + alpha;
      context.fillRect(newCallPosAndDimension[0],
                        newCallPosAndDimension[1],
                        newCallPosAndDimension[2],
                        newCallPosAndDimension[3]);
    }

    context.strokeStyle = color;
    context.strokeRect(newCallPosAndDimension[0],
                        newCallPosAndDimension[1],
                        newCallPosAndDimension[2],
                        newCallPosAndDimension[3]);
  }

  // Rotates and flips quads from draw calls
  rotateCall(canvas, orientationDeg, scale, transformMatrix) {
    // Swap width and height of quads if 90 or 270 deg rotation occurred
    const callWidth = (orientationDeg === 90 || orientationDeg === 270) ?
                        this.size_.height : this.size_.width;
    const callHeight = (orientationDeg === 90 || orientationDeg === 270) ?
                        this.size_.width : this.size_.height;

    var translationX = 0;
    var translationY = 0;
    // Determine amount of translation depending on orientation.
    // We want to put the quads back in frame and relocate xy-pos
    // to top left corner of quads.
    switch(orientationDeg) {
      default:
        break;
      case 90:
        // divide canvas width/height by scale
        // because we want values before scaling
        translationX = canvas.width/scale - callWidth;
        break;
      case 180:
        translationX = canvas.width/scale - callWidth;
        translationY = canvas.height/scale - callHeight;
        break;
      case 270:
        translationY = canvas.height/scale - callHeight;
        break;
      case FlipEnum.HorizontalFlip.id:
        translationX = canvas.width/scale - callWidth;
        break;
      case FlipEnum.VerticalFlip.id:
        translationY = canvas.height/scale - callHeight;
        break;
    }

    var newPosX;
    var newPosY;
    // Use rotation/mirroring matrix to get rotated/flipped coords
    switch (orientationDeg) {
      default:
        newPosX = this.pos_.x * transformMatrix[0][0] +
                  this.pos_.y * transformMatrix[0][1] + translationX;
        newPosY = this.pos_.x * transformMatrix[1][0] +
                  this.pos_.y * transformMatrix[1][1] + translationY;
        break;
      case FlipEnum.HorizontalFlip.id:
        newPosX = -this.pos_.x + translationX;
        newPosY = this.pos_.y;
        break;
      case FlipEnum.VerticalFlip.id:
        newPosX = this.pos_.x;
        newPosY = -this.pos_.y + translationY;
        break;
    }
    return [newPosX * scale, newPosY * scale,
            callWidth * scale, callHeight * scale];
  }
};


// Represents a filter for draw calls. A filter specifies a selector (e.g.
// filename, and/or function name), and the action to take (e.g. skip draw, or
// color to use for draw, etc.) if the filter matches.
class Filter {
  static instances = [];

  constructor(selector, action, index) {
    this.selector_ = {
      filename: selector.filename,
      func: selector.func,
      anno: selector.anno,
    };

    console.log(selector);
    console.log(action);
    // XXX: If there are multiple selectors that apply to the same draw, then
    // I guess the newest filter will take effect.
    this.action_ = {
      skipDraw: action.skipDraw,
      color: action.color,
      alpha: action.alpha,
    };

    // Enabled by default.
    this.enabled_ = true;

    if (index === undefined) {
      Filter.instances.push(this);
      this.index_ = Filter.instances.length - 1;
    }
    else {
      Filter.instances[index] = this;
      this.index_ = index;
    }
  }

  get enabled() { return this.enabled_; }
  set enabled(e) { this.enabled_ = e; }

  get file() { return this.selector_.filename || ""; }
  get func() { return this.selector_.func || ""; }
  get anno() { return this.selector_.anno || "" };

  get shouldDraw() { return !this.action_.skipDraw; }
   // undefined if using caller color
  get drawColor() { return this.action_.color; }
  // undefined if using caller alpha
  get fillAlpha() { return this.action_.alpha; }

  get index() { return this.index_; }

  get streamFilter() {
    return {
      selector: {
        file: this.selector_.filename,
        func: this.selector_.func,
        anno: this.selector_.anno
      },
      active: !this.action_.skipDraw,
      enabled: this.enabled_
    }
  }

  matches(source) {
    if (!(source instanceof Source)) return false;
    if (!this.enabled) return false;

    if (this.selector_.filename) {
      const m = source.file.search(this.selector_.filename);
      if (m == -1) return false;
    }

    if (this.selector_.func) {
      const m = source.func.search(this.selector_.func);
      if (m == -1) return false;
    }

    if (this.selector_.anno) {
      const m = source.anno.search(this.selector_.anno);
      if (m == -1) return false;
    }

    return true;
  }

  createUIString() {
    let str = '';
    if (this.selector_.filename) {
      const parts = this.selector_.filename.split('/');
      str += ` <i class="material-icons-outlined md-18">
      text_snippet</i>${parts[parts.length - 1]}`;
    }
    if (this.selector_.func) {
      str += ` <i class="material-icons-outlined md-18">
      code</i>${this.selector_.func}`;
    }
    if (this.selector_.anno) {
      str += ` <i class="material-icons-outlined md-18">
      message</i>${this.selector_.anno}`;
    }
    return str;
  }

  static enabledInstances() {
    return Filter.instances.filter(f => f.enabled);
  }

  static getFilter(index) {
    return Filter.instances[index];
  }

  static swapFilters(indexA, indexB) {
    var filterA = Filter.instances[indexA];
    var filterB = Filter.instances[indexB];
    filterA.index_ = indexB;
    filterB.index_ = indexA;
    Filter.instances[indexB] = filterA;
    Filter.instances[indexA] = filterB;
  }

  static deleteFilter(index) {
    Filter.instances.splice(index, 1);
    for (var i = index; i < Filter.instances.length; i++) {
      Filter.instances[i].index_ -= 1;
    }
  }

  static sendStreamFilters() {
    const message = {};
    message['method'] = 'VisualDebugger.filterStream';
    message['params'] = {
      filter: { filters: Filter.instances.map((f) => f.streamFilter) }
    };
    Connection.sendMessage(message);
  }
};

