// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying cellular EID and QR code
 */

// The size of each tile in pixels.
const QR_CODE_TILE_SIZE = 5;
// Amount of padding around the QR code in pixels.
const QR_CODE_PADDING = 4 * QR_CODE_TILE_SIZE;
// Styling for filled tiles in the QR code.
const QR_CODE_FILL_STYLE = '#000000';

Polymer({
  is: 'cellular-eid-popup',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /**
     * The euicc object whose EID and QRCode should be shown in the popup.
     */
    euicc: Object,

    /** @private */
    canvasSize_: Number,

    /** @private */
    eid_: String,
  },

  /**
   * @private {?CanvasRenderingContext2D}
   */
  canvasContext_: null,

  /** @override */
  attached() {
    if (!this.euicc) {
      return;
    }
    this.euicc.getEidQRCode().then(this.updateQRCode_.bind(this));
    this.euicc.getProperties().then(this.updateEid_.bind(this));
  },

  /** @override */
  focus() {
    this.$$('.dialog').focus();
  },

  /**@private */
  onCloseTap_() {
    this.fire('close-eid-popup');
  },

  /**
   * @private
   * @param {{qrCode: chromeos.cellularSetup.mojom.QRCode} | null} response
   */
  updateQRCode_(response) {
    if (!response || !response.qrCode) {
      return;
    }
    this.canvasSize_ =
        response.qrCode.size * QR_CODE_TILE_SIZE + 2 * QR_CODE_PADDING;
    Polymer.dom.flush();
    const context = this.getCanvasContext_();
    context.clearRect(0, 0, this.canvasSize_, this.canvasSize_);
    context.fillStyle = QR_CODE_FILL_STYLE;
    let index = 0;
    for (let x = 0; x < response.qrCode.size; x++) {
      for (let y = 0; y < response.qrCode.size; y++) {
        if (response.qrCode.data[index]) {
          context.fillRect(
              x * QR_CODE_TILE_SIZE + QR_CODE_PADDING,
              y * QR_CODE_TILE_SIZE + QR_CODE_PADDING, QR_CODE_TILE_SIZE,
              QR_CODE_TILE_SIZE);
        }
        index++;
      }
    }
  },

  /**
   * @private
   * @param {{properties: chromeos.cellularSetup.mojom.EuiccProperties}}
   *     response
   */
  updateEid_(response) {
    if (!response || !response.properties) {
      return;
    }
    this.eid_ = response.properties.eid;
  },

  /**
   * @private
   * @return {CanvasRenderingContext2D}
   */
  getCanvasContext_() {
    if (this.canvasContext_) {
      return this.canvasContext_;
    }
    return this.$.qrCodeCanvas.getContext('2d');
  },

  /**
   * @param {CanvasRenderingContext2D} canvasContext
   */
  setCanvasContextForTest(canvasContext) {
    this.canvasContext_ = canvasContext;
  }
});
