// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Entry point for the Image Loader's offscreen document.
 */

import {ImageLoader} from './image_loader.js';
import type {LoadImageRequest, LoadImageResponse} from './load_image_request.js';

const EXTENSION_ID = 'pmfjbimdmchhbnneeidfognadeopoehp';

chrome.runtime.onMessage.addListener(
    (msg: LoadImageRequest, sender: chrome.runtime.MessageSender,
     sendResponse: (r: LoadImageResponse) => void) => {
      if ((sender.id !== EXTENSION_ID) || !msg.imageLoaderRequestId) {
        return false;
      }
      return ImageLoader.getInstance().handle(msg, sendResponse);
    });
