// Copyright 2022 The Chromium Authors
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
// This is currently only used for drawing rects and positional text.

class DrawCall {
  constructor(json) {
    // e.g. {"drawindex":"44", option":{"alpha":"1.000000","color":"#ffffff"}
    // ,"pos":"0.000000,763.000000","size":"255x5","source_index":"0",
    // "thread_id":"123456"}
    this.sourceIndex_ = parseInt(json.source_index);
    this.drawIndex_ = parseInt(json.drawindex);
    this.threadId_ =
        parseInt(json.thread_id) || DrawFrame.demo_thread.thread_id;
    this.text = json.text;
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
    this.buffer_id = json.buff_id || -1;
    if (json.uv_size && json.uv_pos) {
      this.uv_size = {
        width: json.uv_size[0],
        height: json.uv_size[1],
      };
      this.uv_pos = {
        x: json.uv_pos[0],
        y: json.uv_pos[1],
      };
    }
    else {
      this.uv_size = {
        width: 1.0,
        height: 1.0,
      };
      this.uv_pos = {
        x: 0.0,
        y: 0.0,
      };
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

  draw(context, buffer_map, threadConfig) {
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

    var color;
    var alpha;
    // If thread drawing is overriding filters.
    if (threadConfig.overrideFilters) {
      color = threadConfig.threadColor;
      alpha = threadConfig.threadAlpha;
    }
    // Otherwise, follow filter draw options.
    else {
      // No filters match this draw. So skip.
      if (!filter) return;
      if (!filter.shouldDraw) return;

      color = (filter && filter.drawColor) ? filter.drawColor : this.color_
      alpha = (filter && filter.fillAlpha) ?
      DrawCall.alphaFloatToHex(parseFloat(filter.fillAlpha) / 100) :
                                                              this.alpha_;
    }

    if (color && alpha) {
      context.fillStyle = color + alpha;
      context.fillRect(this.pos_.x,
                       this.pos_.y,
                       this.size_.width,
                       this.size_.height);
    }

    context.strokeStyle = color;
    context.strokeRect(this.pos_.x,
                       this.pos_.y,
                       this.size_.width,
                       this.size_.height);
    var buff_id = this.buffer_id.toString();
    if(buffer_map[buff_id]) {
      var buff_width = buffer_map[buff_id].width;
      var buff_height = buffer_map[buff_id].height;
      context.drawImage(buffer_map[buff_id],
                        this.uv_pos.x * buff_width,
                        this.uv_pos.y * buff_height,
                        this.uv_size.width * buff_width,
                        this.uv_size.height * buff_height,
                        this.pos_.x,
                        this.pos_.y,
                        this.size_.width,
                        this.size_.height);
    }
  }
};


// Represents a filter for draw calls. A filter specifies a selector (e.g.
// filename, and/or function name), and the action to take (e.g. skip draw, or
// color to use for draw, etc.) if the filter matches.
class Filter {
  static instances = [];

  constructor(enabled, selector, action, index) {
    this.selector_ = {
      filename: selector.filename,
      func: selector.func,
      anno: selector.anno,
    };

    // XXX: If there are multiple selectors that apply to the same draw, then
    // I guess the newest filter will take effect.
    this.action_ = {
      skipDraw: action.skipDraw,
      color: action.color,
      alpha: action.alpha,
    };

    this.enabled_ = enabled;

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
