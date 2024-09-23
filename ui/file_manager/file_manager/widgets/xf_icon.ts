// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {iconSetToCSSBackgroundImageValue} from '../common/js/util.js';
import {ICON_TYPES} from '../foreground/js/constants.js';

import {css, customElement, html, property, type PropertyValues, styleMap, svg, XfBase} from './xf_base.js';

@customElement('xf-icon')
export class XfIcon extends XfBase {
  /**
   * The icon size, can be "extra-small", "small" or "large" (from
   * `XfIcon.size`).
   */
  @property({type: String, reflect: true}) size: string = XfIcon.sizes.SMALL;

  /**
   * The icon type, different type will render different SVG file
   * (from `ICON_TYPES`).
   */
  @property({type: String, reflect: true}) type = '';

  /**
   * Some icon data are directly passed from outside in base64 format. If
   * `iconSet` is provided, `type` will be ignored.
   */
  @property({attribute: false})
  iconSet: chrome.fileManagerPrivate.IconSet|null = null;

  static get sizes() {
    return {
      EXTRA_SMALL: 'extra_small',
      SMALL: 'small',
      MEDIUM: 'medium',
      LARGE: 'large',
    } as const;
  }

  static get multiColor() {
    return {
      [ICON_TYPES.CANT_PIN]:
          svg`<use xlink:href="foreground/images/files/ui/cant_pin.svg#cant_pin"></use>`,
      [ICON_TYPES.CLOUD_DONE]:
          svg`<use xlink:href="foreground/images/files/ui/cloud_done.svg#cloud_done"></use>`,
      [ICON_TYPES.CLOUD_ERROR]:
          svg`<use xlink:href="foreground/images/files/ui/cloud_error.svg#cloud_error"></use>`,
      [ICON_TYPES.CLOUD_OFFLINE]:
          svg`<use xlink:href="foreground/images/files/ui/cloud_offline.svg#cloud_offline"></use>`,
      [ICON_TYPES.CLOUD_PAUSED]:
          svg`<use xlink:href="foreground/images/files/ui/cloud_paused.svg#cloud_paused"></use>`,
      [ICON_TYPES.CLOUD_SYNC]:
          svg`<use xlink:href="foreground/images/files/ui/cloud_sync.svg#cloud_sync"></use>`,
      [ICON_TYPES.ERROR]:
          svg`<use xlink:href="foreground/images/files/ui/error.svg#error"></use>`,
      [ICON_TYPES.OFFLINE]:
          svg`<use xlink:href="foreground/images/files/ui/offline.svg#offline"></use>`,
    };
  }

  static override get styles() {
    return getCSS();
  }

  override render() {
    if (this.type === ICON_TYPES.BLANK) {
      return html``;
    }

    if (Object.keys(XfIcon.multiColor).includes(this.type)) {
      return html`
        <span class="multi-color keep-color">
          <svg>
            ${XfIcon.multiColor[this.type]}
          </svg>
        </span>`;
    }

    if (this.iconSet) {
      const backgroundImageStyle = {
        'background-image': iconSetToCSSBackgroundImageValue(this.iconSet),
      };
      return html`<span class="keep-color" style=${
          styleMap(backgroundImageStyle)}></span>`;
    }

    return html`
      <span></span>
    `;
  }

  override updated(changedProperties: PropertyValues<this>) {
    if (changedProperties.has('type')) {
      this.validateTypeProperty_(this.type);
    }
  }

  private validateTypeProperty_(type: string) {
    if (this.iconSet) {
      // Ignore checking "type" if iconSet is provided.
      return;
    }
    if (!type) {
      console.warn('Empty type will result in an square being rendered.');
      return;
    }
    const validTypes = Object.values(ICON_TYPES);
    if (!validTypes.find((t) => t === type)) {
      console.warn(
          `Type ${type} is not a valid icon type, please check ICON_TYPES.`);
    }
  }
}

function getCSS() {
  return css`
    :host {
      --xf-icon-color: var(--cros-sys-on_surface);
      --xf-icon-base-color: var(--cros-sys-app_base);
      --xf-icon-positive-color: var(--cros-sys-positive);
      --xf-icon-error-color: var(--cros-sys-error);
      --xf-icon-progress-color: var(--cros-sys-progress);
      --xf-secondary-color: var(--cros-sys-secondary);
      display: inline-block;
    }

    span {
      display: block;
    }

    span:not(.keep-color) {
      -webkit-mask-position: center;
      -webkit-mask-repeat: no-repeat;
      background-color: var(--xf-icon-color);
    }

    span.keep-color {
      background-position: center center;
      background-repeat: no-repeat;
    }

    :host-context([disabled]) span.keep-color {
      opacity: 0.38;
    }

    span.multi-color {
      display: flex;
      align-items: stretch;
      justify-content: stretch;
    }

    :host([size="extra_small"]) span {
      height: 16px;
      width: 16px;
    }

    :host([size="extra_small"]) span.keep-color {
      background-size: 16px 16px;
    }

    :host([size="extra_small"]) span:not(.keep-color) {
      -webkit-mask-size: 16px;
    }

    :host([size="small"]) span {
      height: 20px;
      width: 20px;
    }

    :host([size="small"]) span.keep-color {
      background-size: 20px 20px;
    }

    :host([size="small"]) span:not(.keep-color) {
      -webkit-mask-size: 20px;
    }

    :host([size="medium"]) span {
      height: 32px;
      width: 32px;
    }

    :host([size="medium"]) span.keep-color {
      background-size: 32px 32px;
    }

    :host([size="medium"]) span:not(.keep-color) {
      -webkit-mask-size: 32px;
    }

    :host([size="large"]) span {
      height: 48px;
      width: 48px;
    }

    :host([size="large"]) span.keep-color {
      background-size: 48px 48px;
    }

    :host([size="large"]) span:not(.keep-color) {
      -webkit-mask-size: 48px;
    }

    :host([type="android_files"]) span {
      -webkit-mask-image: url(../foreground/images/volumes/android.svg);
    }

    :host([type="archive"]) span {
      -webkit-mask-image: url(../foreground/images/filetype/filetype_archive.svg);
    }

    :host([type="audio"]) span {
      -webkit-mask-image: url(../foreground/images/filetype/filetype_audio.svg);
    }

    :host([type="bruschetta"]) span {
      -webkit-mask-image: url(../foreground/images/volumes/bruschetta.svg);
    }

    :host([type="crostini"]) span {
      -webkit-mask-image: url(../foreground/images/volumes/linux_files.svg);
    }

    :host([type="camera-folder"]) span {
      -webkit-mask-image: url(../foreground/images/volumes/camera.svg);
    }

    :host([type="computer"]) span {
      -webkit-mask-image: url(../foreground/images/volumes/computer.svg);
    }

    :host([type="computers_grand_root"]) span {
      -webkit-mask-image: url(../foreground/images/volumes/devices.svg);
    }

    :host([type="downloads"]) span {
      -webkit-mask-image: url(../foreground/images/volumes/downloads.svg);
    }

    :host([type="drive"]) span {
      -webkit-mask-image: url(../foreground/images/volumes/drive.svg);
    }

    :host([type="drive_offline"]) span {
      -webkit-mask-image: url(../foreground/images/volumes/offline.svg);
    }

    :host([type="drive_shared_with_me"]) span {
      -webkit-mask-image: url(../foreground/images/volumes/shared.svg);
    }

    :host([type="drive_logo"]) span {
      -webkit-mask-image: url(../foreground/images/files/ui/drive_logo.svg);
    }

    :host([type="drive_bulk_pinning"]) span {
      -webkit-mask-image: url(../foreground/images/files/ui/drive_bulk_pinning.svg);
    }

    :host([type="excel"]) span {
      -webkit-mask-image: url(../foreground/images/filetype/filetype_excel.svg);
    }

    :host([type="external_media"]) span,
    :host([type="removable"]) span,
    :host([type="usb"]) span {
      -webkit-mask-image: url(../foreground/images/volumes/usb.svg);
    }

    :host([type="drive_recent"]) span, :host([type="recent"]) span {
      -webkit-mask-image: url(../foreground/images/volumes/recent.svg);
    }

    :host([type="folder"]) span {
      -webkit-mask-image: url(../foreground/images/filetype/filetype_folder.svg);
    }

    :host([type="generic"]) span, :host([type="glink"]) span {
      -webkit-mask-image: url(../foreground/images/filetype/filetype_generic.svg);
    }

    :host([type="gdoc"]) span {
      -webkit-mask-image: url(../foreground/images/filetype/filetype_gdoc.svg);
    }

    :host([type="gdraw"]) span {
      -webkit-mask-image: url(../foreground/images/filetype/filetype_gdraw.svg);
    }

    :host([type="gform"]) span {
      -webkit-mask-image: url(../foreground/images/filetype/filetype_gform.svg);
    }

    :host([type="gmap"]) span {
      -webkit-mask-image: url(../foreground/images/filetype/filetype_gmap.svg);
    }

    :host([type="gsheet"]) span {
      -webkit-mask-image: url(../foreground/images/filetype/filetype_gsheet.svg);
    }

    :host([type="gsite"]) span {
      -webkit-mask-image: url(../foreground/images/filetype/filetype_gsite.svg);
    }

    :host([type="gmaillayout"]) span {
      -webkit-mask-image: url(../foreground/images/filetype/filetype_gmaillayout.svg);
    }

    :host([type="gslides"]) span {
      -webkit-mask-image: url(../foreground/images/filetype/filetype_gslides.svg);
    }

    :host([type="gtable"]) span {
      -webkit-mask-image: url(../foreground/images/filetype/filetype_gtable.svg);
    }

    :host([type="image"]) span, :host([type="raw"]) span {
      -webkit-mask-image: url(../foreground/images/filetype/filetype_image.svg);
    }

    :host([type="mtp"]) span {
      -webkit-mask-image: url(../foreground/images/volumes/phone.svg);
    }

    :host([type="my_files"]) span {
      -webkit-mask-image: url(../foreground/images/volumes/my_files.svg);
    }

    :host([type="optical"]) span {
      -webkit-mask-image: url(../foreground/images/volumes/cd.svg);
    }

    :host([type="pdf"]) span {
      -webkit-mask-image: url(../foreground/images/filetype/filetype_pdf.svg);
    }

    :host([type="plugin_vm"]) span {
      -webkit-mask-image: url(../foreground/images/volumes/plugin_vm_ng.svg);
    }

    :host([type="ppt"]) span {
      -webkit-mask-image: url(../foreground/images/filetype/filetype_ppt.svg);
    }

    :host([type="script"]) span {
      -webkit-mask-image: url(../foreground/images/filetype/filetype_script.svg);
    }

    :host([type="sd"]) span {
      -webkit-mask-image: url(../foreground/images/volumes/sd.svg);
    }

    :host([type="service_drive"]) span {
      -webkit-mask-image: url(../foreground/images/volumes/service_drive.svg);
    }

    :host([type="shared_drive"]) span {
      -webkit-mask-image: url(../foreground/images/filetype/filetype_team_drive.svg);
    }

    :host([type="shared_drives_grand_root"]) span {
      -webkit-mask-image: url(../foreground/images/volumes/team_drive.svg);
    }

    :host([type="shared_folder"]) span {
      -webkit-mask-image: url(../foreground/images/filetype/filetype_folder_shared.svg);
    }

    :host([type="shortcut"]) span {
      -webkit-mask-image: url(../foreground/images/volumes/shortcut.svg);
    }

    :host([type="sites"]) span {
      -webkit-mask-image: url(../foreground/images/filetype/filetype_sites.svg);
    }

    :host([type="smb"]) span {
      -webkit-mask-image: url(../foreground/images/volumes/smb.svg);
    }

    :host([type="team_drive"]) span, :host([type="unknown_removable"]) span {
      -webkit-mask-image: url(../foreground/images/volumes/hard_drive.svg);
    }

    :host([type="thumbnail_generic"]) span {
      -webkit-mask-image: url(../foreground/images/files/ui/filetype_placeholder_generic.svg);
    }

    :host([type="tini"]) span {
      -webkit-mask-image: url(../foreground/images/filetype/filetype_tini.svg);
    }

    :host([type="trash"]) span {
      -webkit-mask-image: url(../foreground/images/files/ui/delete_ng.svg);
    }

    :host([type="video"]) span {
      -webkit-mask-image: url(../foreground/images/filetype/filetype_video.svg);
    }

    :host([type="word"]) span {
      -webkit-mask-image: url(../foreground/images/filetype/filetype_word.svg);
    }

    :host([type="check"]) span {
      -webkit-mask-image: url(../foreground/images/files/ui/check.svg);
    }

    :host([type="bulk_pinning_battery_saver"]) span {
      -webkit-mask-image: url(../foreground/images/files/ui/bulk_pinning_battery_saver.svg);
    }

    :host([type="bulk_pinning_done"]) span {
      -webkit-mask-image: url(../foreground/images/files/ui/bulk_pinning_done.svg);
    }

    :host([type="bulk_pinning_offline"]) span {
      -webkit-mask-image: url(../foreground/images/files/ui/bulk_pinning_offline.svg);
    }

    :host([type="cloud"]) span {
      -webkit-mask-image: url(../foreground/images/files/ui/cloud.svg);
    }

    :host([type="error_banner"]) span {
      -webkit-mask-image: url(../foreground/images/files/ui/error_banner_icon.svg);
    }

    :host([type='star']) span {
      -webkit-mask-image: url(../foreground/images/files/ui/star.svg);
    }

    :host([type='gdoc']) span,
    :host([type='script']) span,
    :host([type='tini']) span {
      background-color: var(--cros-sys-progress);
    }

    :host([type='audio']) span,
    :host([type='gdraw']) span,
    :host([type='image']) span,
    :host([type='gmap']) span,
    :host([type='pdf']) span,
    :host([type='video']) span,
    :host([type='gmaillayout']) span {
      background-color: var(--cros-sys-error);
    }

    :host([type='gsheet']) span,
    :host([type='gtable']) span {
      background-color: var(--cros-sys-positive);
    }

    :host([type='gslides']) span {
      background-color: var(--cros-sys-warning);
    }

    :host([type='gform']) span {
      background-color: var(--cros-sys-file_form);
    }

    :host([type='gsite']) span,
    :host([type='sites']) span {
      background-color: var(--cros-sys-file_site);
    }

    :host([type='excel']) span {
      background-color: var(--cros-sys-file_ms_excel);
    }

    :host([type='ppt']) span {
      background-color: var(--cros-sys-file_ms_ppt);
    }

    :host([type='word']) span {
      background-color: var(--cros-sys-file_ms_word);
    }

    /**
     * These icons are never shown on their own but are shown as suffix icons,
     * hence why they are smaller with offset margins. At the moment these are
     * only supported with "small" size prefix icons.
     */
    :host([type='cloud_done']) span,
    :host([type='cloud_error']) span,
    :host([type='cloud_offline']) span,
    :host([type='cloud_paused']) span,
    :host([type='cloud_sync']) span {
      margin-inline-start: 10px;
      margin-top: 8px;
      height: 12px;
      width: 12px;
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    'xf-icon': XfIcon;
  }
}
