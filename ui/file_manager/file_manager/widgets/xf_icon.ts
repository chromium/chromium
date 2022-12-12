// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {classMap, css, customElement, html, property, PropertyValues, XfBase} from './xf_base.js';

@customElement('xf-icon')
export class XfIcon extends XfBase {
  /** The icon size, can be "small" or "large" (from `XfIcon.size`). */
  @property({type: String, reflect: true}) size: string = XfIcon.sizes.SMALL;

  /**
   * The icon type, different type will render different SVG file
   * (from `XfIcon.types`).
   */
  @property({type: String, reflect: true}) type = '';

  static get sizes() {
    return {
      SMALL: 'small',
      LARGE: 'large',
    } as const;
  }

  static get types() {
    return {
      ANDROID_FILES: 'android_files',
      ARCHIVE: 'archive',
      AUDIO: 'audio',
      BRUSCHETTA: 'bruschetta',
      CAMERA_FOLDER: 'camera_folder',
      COMPUTER: 'computer',
      COMPUTERS_GRAND_ROOT: 'computers_grand_root',
      CROSTINI: 'crostini',
      DOWNLOADS: 'downloads',
      DRIVE_OFFLINE: 'drive_offline',
      DRIVE_RECENT: 'drive_recent',
      DRIVE_SHARED_WITH_ME: 'drive_shared_with_me',
      DRIVE: 'drive',
      EXCEL: 'excel',
      EXTERNAL_MEDIA: 'external_media',
      FOLDER: 'folder',
      GENERIC: 'generic',
      GOOGLE_DOC: 'gdoc',
      GOOGLE_DRAW: 'gdraw',
      GOOGLE_FORM: 'gform',
      GOOGLE_LINK: 'glink',
      GOOGLE_MAP: 'gmap',
      GOOGLE_SHEET: 'gsheet',
      GOOGLE_SITE: 'gsite',
      GOOGLE_SLIDES: 'gslides',
      GOOGLE_TABLE: 'gtable',
      IMAGE: 'image',
      MTP: 'mtp',
      MY_FILES: 'my_files',
      OPTICAL: 'optical',
      PDF: 'pdf',
      PLUGIN_VM: 'plugin_vm',
      POWERPOINT: 'ppt',
      RAW: 'raw',
      RECENT: 'recent',
      REMOVABLE: 'removable',
      SCRIPT: 'script',
      SD_CARD: 'sd',
      SERVICE_DRIVE: 'service_drive',
      SHARED_DRIVE: 'shared_drive',
      SHARED_DRIVES_GRAND_ROOT: 'shared_drives_grand_root',
      SHARED_FOLDER: 'shared_folder',
      SHORTCUT: 'shortcut',
      SITES: 'sites',
      SMB: 'smb',
      TEAM_DRIVE: 'team_drive',
      THUMBNAIL_GENERIC: 'thumbnail_generic',
      TINI: 'tini',
      TRASH: 'trash',
      UNKNOWN_REMOVABLE: 'unknown_removable',
      USB: 'usb',
      VIDEO: 'video',
      WORD: 'word',
    };
  }

  static override get styles() {
    return getCSS();
  }

  override render() {
    const shouldKeepColor = [
      XfIcon.types.EXCEL,
      XfIcon.types.POWERPOINT,
      XfIcon.types.WORD,
    ].includes(this.type);
    const spanClass = {'keep-color': shouldKeepColor};

    return html`
      <span class=${classMap(spanClass)}></span>
    `;
  }

  override updated(changedProperties: PropertyValues<this>) {
    if (changedProperties.has('type')) {
      this.validateTypeProperty_(this.type);
    }
  }

  private validateTypeProperty_(type: string) {
    if (!type) {
      console.warn('Empty type will result in an square being rendered.');
      return;
    }
    const validTypes = Object.values(XfIcon.types);
    if (!validTypes.find((t) => t === type)) {
      console.warn(
          `Type ${type} is not a valid icon type, please check XfIcon.types.`);
    }
  }
}

function getCSS() {
  return css`
    :host {
      --xf-icon-color: var(--cros-sys-on_surface);
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

    :host([size="small"]) span {
      height: 20px;
      width: 20px;
    }

    :host([size="small"]) span:not(.keep-color) {
      -webkit-mask-size: 20px;
    }

    :host([size="small"]) span.keep-color {
      background-size: 20px;
    }

    :host([size="large"]) span {
      height: 48px;
      width: 48px;
    }

    :host([size="large"]) span:not(.keep-color) {
      -webkit-mask-size: 48px;
    }

    :host([size="large"]) span.keep-color {
      background-size: 48px;
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

    :host([type="bruschetta"]) span, :host([type="crostini"]) span {
      -webkit-mask-image: url(../foreground/images/volumes/linux_files.svg);
    }

    :host([type="camera_folder"]) span {
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

    :host([type="drive_offline"]) span {
      -webkit-mask-image: url(../foreground/images/volumes/offline.svg);
    }

    :host([type="drive_shared_with_me"]) span {
      -webkit-mask-image: url(../foreground/images/volumes/shared.svg);
    }

    :host([type="excel"]) span {
      background-image: url(../foreground/images/filetype/filetype_excel.svg);
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
      background-image: url(../foreground/images/filetype/filetype_ppt.svg);
    }

    :host([type="script"]) span {
      -webkit-mask-image: url(../foreground/images/filetype/filetype_script.svg);
    }

    :host([type="sd"]) span {
      -webkit-mask-image: url(../foreground/images/volumes/sd.svg);
    }

    :host([type="service_drive"]) span, :host([type="drive"]) span {
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
      background-image: url(../foreground/images/filetype/filetype_word.svg);
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    'xf-icon': XfIcon;
  }
}
