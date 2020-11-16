// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* eslint-disable no-var */

// #import {assert, assertInstanceof} from 'chrome://resources/js/assert.m.js'

/* #ignore */ 'use strict';

/**
 * Creates a sample canvas.
 * @return {!HTMLCanvasElement}
 */
/* #export */ function getSampleCanvas() {
  var canvas =
      assertInstanceof(document.createElement('canvas'), HTMLCanvasElement);
  canvas.width = 1920;
  canvas.height = 1080;

  var ctx = canvas.getContext('2d');
  ctx.fillStyle = '#000000';
  for (var i = 0; i < 10; i++) {
    ctx.fillRect(i * 30, i * 30, 20, 20);
  }

  return canvas;
}
