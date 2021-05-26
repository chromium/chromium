// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Main JS Module for Audio Player. It replaces
 * audio_player_scripts.js
 */

import {AudioPlayer, reload, unload} from './audio_player.js';

window.reload = reload;
window.unload = unload;
window.AudioPlayer = AudioPlayer;

console.log('AudioPlayer main.js loaded');
