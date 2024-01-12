// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {css, customElement, html, XfBase} from './xf_base.js';

@customElement('xf-splitter')
export class XfSplitter extends XfBase {
  static get events() {
    return {
      SPLITTER_DRAGMOVE: 'splitter_dragmove',
      SPLITTER_RESIZE: 'splitter_resize',
    } as const;
  }

  static override get styles() {
    return getCSS();
  }

  private handlers_: Map<string, (e: any) => void>|null = null;
  private startPosition_: number = 0;
  private beforeStartWidth_: number = -1;
  private afterStartWidth_: number = -1;
  private isTouch_: boolean = false;
  private beforeResizingElement_: HTMLElement|null = null;
  private afterResizingElement_: HTMLElement|null = null;
  private isRTLlayout_: boolean = false;

  static get splitterBarSize() {
    return 24;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.handlers_ = new Map();
  }

  override disconnectedCallback() {
    this.finishDrag_();
    this.handlers_ = null;
    super.disconnectedCallback();
  }

  override render() {
    return html`<slot name="splitter-before"></slot>
                <div id="splitter"
                  @mousedown=${this.onMousedown_}
                  @touchstart=${this.onTouchstart_}>
                  <div id="tracker"></div>
                </div>
                <slot name="splitter-after"></slot>`;
  }

  private setupDrag_(startPosition: number, isTouchStart: boolean) {
    assert(!!this.handlers_);
    this.startPosition_ = startPosition;
    this.isTouch_ = isTouchStart;
    const finishDragBound = this.finishDrag_.bind(this);
    if (this.isTouch_) {
      this.handlers_.set('touchmove', this.onTouchMove_.bind(this));
      this.handlers_.set('touchend', finishDragBound);
      this.handlers_.set('touchcancel', finishDragBound);
      // Another touch start (we somehow missed touchend or touchcancel).
      this.handlers_.set('touchstart', finishDragBound);
    } else {
      this.handlers_.set('mousemove', this.onMouseMove_.bind(this));
      this.handlers_.set('mouseup', finishDragBound);
    }
    const doc = this.ownerDocument;
    for (const [eventType, handler] of this.handlers_) {
      doc.addEventListener(eventType, handler, true);
    }
    this.beforeResizingElement_ = this.firstElementChild as HTMLElement | null;
    assert(!!this.beforeResizingElement_);
    this.beforeStartWidth_ =
        parseFloat(
            doc.defaultView!.getComputedStyle(this.beforeResizingElement_)
                .width) +
        this.beforeResizingElement_.offsetWidth -
        this.beforeResizingElement_.clientWidth;
    this.afterResizingElement_ = this.lastElementChild as HTMLElement | null;
    assert(!!this.afterResizingElement_);
    this.afterStartWidth_ =
        parseFloat(doc.defaultView!.getComputedStyle(this.afterResizingElement_)
                       .width) +
        this.afterResizingElement_.offsetWidth -
        this.afterResizingElement_.clientWidth;
    this.classList.add('splitter-active');
    this.isRTLlayout_ =
        window.getComputedStyle(this).getPropertyValue('direction') === 'rtl';
  }

  private finishDrag_() {
    assert(!!this.handlers_);
    const doc = this.ownerDocument;
    for (const [eventType, handler] of this.handlers_) {
      doc.removeEventListener(eventType, handler, true);
    }
    this.handlers_.clear();
    this.classList.remove('splitter-active');
    assert(!!this.beforeResizingElement_);
    let computedWidth = parseFloat(
        doc.defaultView!.getComputedStyle(this.beforeResizingElement_).width);
    // Send a resize event if either side changed size.
    if (this.beforeStartWidth_ !== computedWidth) {
      this.dispatchEvent(new CustomEvent(XfSplitter.events.SPLITTER_RESIZE));
    } else {
      assert(!!this.afterResizingElement_);
      computedWidth = parseFloat(
          doc.defaultView!.getComputedStyle(this.afterResizingElement_).width);
      if (this.afterStartWidth_ !== computedWidth) {
        this.dispatchEvent(new CustomEvent(XfSplitter.events.SPLITTER_RESIZE));
      }
    }
  }

  private doMove_(newPosition: number) {
    const delta = this.isRTLlayout_ ? this.startPosition_ - newPosition :
                                      newPosition - this.startPosition_;
    let newWidth = this.beforeStartWidth_ + delta;
    assert(!!this.beforeResizingElement_);
    this.beforeResizingElement_.style.width = newWidth + 'px';
    newWidth = this.afterStartWidth_ - delta;
    assert(!!this.afterResizingElement_);
    this.afterResizingElement_.style.width = newWidth + 'px';
    this.dispatchEvent(new CustomEvent(XfSplitter.events.SPLITTER_DRAGMOVE));
  }

  /** Handles mouse down on the splitter. */
  private onMousedown_(event: MouseEvent) {
    // Activate only for first button (0).
    if (event.button) {
      return;
    }
    this.setupDrag_(event.clientX, false);
    // Inhibit selection.
    event.preventDefault();
  }

  private onMouseMove_(event: MouseEvent) {
    this.doMove_(event.clientX);
  }

  /** Handles touchstart on the splitter. */
  private onTouchstart_(event: TouchEvent) {
    if (event.touches.length === 1) {
      this.setupDrag_(event.touches[0]!.clientX, true);
      if (event.cancelable) {
        event.preventDefault();
      }
    }
  }

  private onTouchMove_(event: TouchEvent) {
    if (event.touches.length === 1) {
      this.doMove_(event.touches[0]!.clientX);
    }
  }
}

function getCSS() {
  return css`
    :host {
      --xf-splitter-cursor: col-resize;
      --xf-splitter-hover-color: var(--cros-sys-hover_on_subtle);
      --xf-splitter-tracker-offset: 0px;
      display: flex;
      flex: none;
      margin: 0;
      position: relative;
      width: 100%;
    }

    #splitter:hover #tracker {
      background-color: var(--xf-splitter-hover-color);
    }

    #splitter {
      cursor: var(--xf-splitter-cursor);
      display: flex;
      flex-direction: column;
      justify-content: center;
      min-width: ${XfSplitter.splitterBarSize}px;
      width: ${XfSplitter.splitterBarSize}px;
    }

    #tracker {
      border: none;
      border-radius: 8px;
      height: 64px;
      left: var(--xf-splitter-tracker-offset);
      min-width: 16px;
      padding: 0;
      position: relative;
      width: 16px;
      z-index: var(--xf-splitter-z-index);
    }

    :host-context(html[dir=rtl]) #tracker {
      right: var(--xf-splitter-tracker-offset);
    }

    #tracker:hover {
      background-color: var(--xf-splitter-hover-color);
      cursor: var(--xf-splitter-cursor, col-resize);
    }
  `;
}

export type SplitterDragmoveEvent = CustomEvent;
export type SplitterResizeEvent = CustomEvent;

declare global {
  interface HTMLElementEventMap {
    [XfSplitter.events.SPLITTER_DRAGMOVE]: SplitterDragmoveEvent;
    [XfSplitter.events.SPLITTER_RESIZE]: SplitterResizeEvent;
  }

  interface HTMLElementTagNameMap {
    'xf-splitter': XfSplitter;
  }
}
