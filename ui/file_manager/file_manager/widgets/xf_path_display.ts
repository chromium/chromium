// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addCSSPrefixSelector} from '../common/js/dom_utils.js';

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
              <div class="folder mid-folder" tabindex="-1">${e}</div>
              <div class="separator"></div>`)}
      <div class="folder last-folder" tabindex="-1">${tail}</div>
    `;
  }
}

/**
 * CSS used by the xf-path-display widget.
 */
function getCSS(): CSSResultGroup {
  const legacyStyle = css`
    :host([hidden]),
    [hidden] {
      display: none !important;
    }
    :host {
      display: flex;
      flex-direction: row;
      align-items: center;
      line-height: 20px;
      padding: 10px 0px;
      border-top: 1px solid var(--cros-separator-color);
      font-family: 'Roboto Medium';
      font-size: 13px;
      outline: none;
      user-select: none;
    }
    div.folder {
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
      transition: all 300ms;
      flex-shrink: 1;
      min-width: 0;
      padding: 4px 4px;
      border-radius: 8px;
    }
    div.mid-folder {
      color: var(--cros-text-color-secondary);
    }
    div.last-folder {
      color: var(--cros-text-color-primary);
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
      -webkit-mask-image: url(/foreground/images/files/ui/arrow_right.svg);
      -webkit-mask-position: center;
      -webkit-mask-repeat: no-repeat;
      background-color: var(--cros-text-color-secondary);
      width: 20px;
      height: 20px;
      padding: 0 2px;
      flex-grow: 0;
    }
  `;

  const refresh23Style = css`
      :host([hidden]),
      [hidden] {
        display: none !important;
      }
      :host {
        align-items: center;
        border-top: 1px solid var(--cros-sys-separator);
        display: flex;
        flex-direction: row;
        font: var(--cros-button-1-font);
        outline: none;
        padding: 8px;
        padding-inline-start: 20px;
        user-select: none;
      }
      div.folder {
        white-space: nowrap;
        overflow: hidden;
        text-overflow: ellipsis;
        transition: all 300ms;
        flex-shrink: 1;
        min-width: 0;
        padding: 4px 4px;
        border-radius: 8px;
      }
      div.folder:first-of-type {
        padding-inline-start: 0;
      }
      div.mid-folder {
        color: var(--cros-sys-on_surface_variant);
      }
      div.last-folder {
        color: var(--cros-sys-on_surface);
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
        -webkit-mask-image: url(/foreground/images/files/ui/arrow_right.svg);
        -webkit-mask-position: center;
        -webkit-mask-repeat: no-repeat;
        background-color: var(--cros-sys-on_surface_variant);
        width: 20px;
        height: 20px;
        padding: 0 2px;
        flex-grow: 0;
      }
    `;

  return [
    addCSSPrefixSelector(legacyStyle, '[theme="legacy"]'),
    addCSSPrefixSelector(refresh23Style, '[theme="refresh23"]'),
  ];
}

declare global {
  interface HTMLElementTagNameMap {
    'xf-path-display': XfPathDisplayElement;
  }
}
