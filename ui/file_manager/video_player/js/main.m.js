// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Main JS Module for Video Player. It replaces
 * video_player_scripts.js
 */

import {unload} from './video_player.m.js';

window.unload = unload;

console.log('VideoPlayer main.m.js loaded');
