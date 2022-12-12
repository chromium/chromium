// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {css, CSSResultGroup, customElement, html, property, repeat, XfBase} from './xf_base.js';

/**
 * An element that renders a path in a single horizontal line. The path has
 * to be set on the `path` attribute, using '/' as the separator. The element
 * tries to display the path in the full width of the container. If the path
 * is too long, each folder is proportionally clipped, and ellipsis are used
 * to indicate clipping. Hovering or focusing clipped elements make them
 * expand to their original content. Use:
 *
 *   <xf-path-display path="My files/folder/subfolder/file.txt">
 *   </xf-path-display>
 */
@customElement('xf-path-display')
export class XfPathDisplayElement extends XfBase {
  /**
   * The path to be displayed. If the path consists of multiple directories
   * they should be separated by the slash, i.e., 'foo/bar/baz'
   */
  @property({type: String, reflect: true}) path = '';

  static override get styles(): CSSResultGroup {
    return getCSS();
  }

  override render() {
    if (!this.path) {
      return html``;
    }
    const parts = this.path.split('/');
    const head = parts.slice(0, parts.length - 1);
    const tail = parts[parts.length - 1];
    return html`
      ${repeat(head, (e) => html`
              <div class="folder" tabindex="0">${e}</div>
              <div class="separator">&gt;</div>`)}
      <div class="folder" tabindex="0">${tail}</div>
    `;
  }
}

/**
 * CSS used by the xf-path-display widget.
 */
function getCSS(): CSSResultGroup {
  return css`
    :host([hidden]),
    [hidden] {
      display: none !important;
    }
    :host {
      display: flex;
      flex-direction: row;
      align-items: center;
      width: 100%;
      height: 2lh;
      border-top: 1px solid var(--cros-separator-color);
      font-family: 'Roboto Medium';
      font-size: 14px;
      outline: none;
      user-select: none;
      color: var(--cros-text-color-secondary);
    }
    div.folder {
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
      transition: all 300ms;
      flex-shrink: 1;
      min-width: 0;
    }
    div.folder:hover {
      flex-shrink: 0;
      min-width: auto;
    }
    div.folder:focus {
      flex-shrink: 0;
      min-width: auto;
    }
    div.separator {
      padding: 0px 0.5ex;
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    'xf-path-display': XfPathDisplayElement;
  }
}
