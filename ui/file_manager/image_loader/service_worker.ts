// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Entry point for the Image Loader's service worker.
 */

import {assert} from 'chrome://resources/js/assert.js';

import type {LoadImageRequest, LoadImageResponse} from './load_image_request.js';
import type {PrivateApi} from './sw_od_messages.js';

const EXTENSION_ID = 'pmfjbimdmchhbnneeidfognadeopoehp';

const ALLOW_LISTED_FILE_MANAGER_SWA = 'chrome://file-manager';
const ALLOW_LISTED_NATIVE = 'com.google.ash_thumbnail_loader';

// setupOffscreenDocument is based on
// https://developer.chrome.com/docs/extensions/reference/api/offscreen#maintain_the_lifecycle_of_an_offscreen_document
let creatingOffscreenDocument: Promise<void>|undefined;
async function setupOffscreenDocument() {
  const url = chrome.runtime.getURL('offscreen.html');
  const existingContexts = await chrome.runtime.getContexts({
    contextTypes: [chrome.runtime.ContextType.OFFSCREEN_DOCUMENT],
    documentUrls: [url],
  });
  if (existingContexts.length > 0) {
    return;
  }

  if (creatingOffscreenDocument) {
    await creatingOffscreenDocument;
    return;
  }

  creatingOffscreenDocument = chrome.offscreen.createDocument({
    url: url,
    // 'USER_MEDIA' isn't quite right, since we never call getUserMedia, but
    // it's the closest listed reason from
    // https://developer.chrome.com/docs/extensions/reference/api/offscreen#type-Reason
    reasons: [chrome.offscreen.Reason.USER_MEDIA],
    justification: 'Lets us use <img>, <canvas> and XMLHttpRequest',
  });
  await creatingOffscreenDocument;
  creatingOffscreenDocument = undefined;
}

// Forwards a message from the service worker to the offscreen document.
//
// sendResponse will not be called if msg.cancel is truthy. See the
// ImageLoader.handle method.
async function sendToOffscreenDocument(
    msg: LoadImageRequest, senderOrigin: string,
    sendResponse: (r: LoadImageResponse) => void) {
  assert(msg);
  await setupOffscreenDocument();
  msg.imageLoaderRequestId = senderOrigin + ':' + msg.taskId;
  chrome.runtime.sendMessage(
      null, msg, undefined, (response: LoadImageResponse) => {
        sendResponse(response);
      });
}

// Compare the hard-coded-at-compile-or-package-time script version with the
// loaded-at-runtime manifest version. Chrome has a service worker script
// cache, which is usually flushed whenever a Chrome extension is upgraded to a
// new version. But the ChromeOS Files App (and its image loader Chrome
// extension) is unusual (for a Chrome extension) in that it's built in to the
// OS image and does not go through an extension's typical "uninstall the old;
// install the new" mechanisms. Even when the OS image ships a newer version,
// the service worker script cache will serve the older, stale version. This
// conditional chrome.runtime.reload() call works around that.
//
// https://groups.google.com/a/google.com/g/chromeos-files-syd/c/dBhXhPGGkg0/m/pw3Gv3TLAAAJ
//
// When updating this '0.4' string, keep it synchronized with what's in
// manifest.json near the §.
if (chrome.runtime.getManifest().version !== '0.4') {
  chrome.runtime.reload();

} else {
  // Handle imageLoaderPrivate calls on behalf of the offscreen document.
  chrome.runtime.onMessage.addListener(
      (msg: PrivateApi, sender: chrome.runtime.MessageSender,
       sendResponse: (thumbnailDataUrl: string) => void) => {
        if (sender.id !== EXTENSION_ID) {
          return false;
        } else if (msg.apiMethod === 'getArcDocumentsProviderThumbnail') {
          chrome.imageLoaderPrivate.getArcDocumentsProviderThumbnail(
              msg.params.url,
              msg.params.widthHint,
              msg.params.heightHint,
              sendResponse,
          );
          return true;
        } else if (msg.apiMethod === 'getDriveThumbnail') {
          chrome.imageLoaderPrivate.getDriveThumbnail(
              msg.params.url,
              msg.params.cropToSquare,
              sendResponse,
          );
          return true;
        } else if (msg.apiMethod === 'getPdfThumbnail') {
          chrome.imageLoaderPrivate.getPdfThumbnail(
              msg.params.url,
              msg.params.width,
              msg.params.height,
              sendResponse,
          );
          return true;
        }
        return false;
      });

  // Listen to messages from the Files app SWA.
  chrome.runtime.onMessageExternal.addListener(
      (msg: LoadImageRequest, sender: chrome.runtime.MessageSender,
       sendResponse: (r: LoadImageResponse) => void) => {
        if (!msg || (sender.origin !== ALLOW_LISTED_FILE_MANAGER_SWA)) {
          return false;
        }
        sendToOffscreenDocument(
            msg, ALLOW_LISTED_FILE_MANAGER_SWA, sendResponse);
        // Return true (or false) if the runtime should keep sendResponse
        // valid-to-call (or invalid) after this closure returns.
        //
        // If msg.cancel is truthy then sendToOffscreenDocument will not call
        // sendResponse, so we can return false.
        //
        // If msg.cancel is falsy then sendToOffscreenDocument will call it,
        // but also asynchronously, so return true.
        return !msg.cancel;
      });

  // Listen to messages from ash-chrome itself, e.g. thumbnails in the "tote"
  // (also known as the "holding space"), after taking a screenshot.
  //
  // Each native connection is expected to send a single request only, so we
  // disconnect after handling a message.
  chrome.runtime.onConnectNative.addListener((port: chrome.runtime.Port) => {
    assert(port.sender);
    if (port.sender.nativeApplication !== ALLOW_LISTED_NATIVE) {
      port.disconnect();
      return;
    }

    port.onMessage.addListener((msg: LoadImageRequest) => {
      if (!msg) {
        port.disconnect();
        return;
      } else if (msg.cancel) {
        port.disconnect();
      }
      sendToOffscreenDocument(
          msg, ALLOW_LISTED_NATIVE, (response: LoadImageResponse) => {
            assert(!msg.cancel);
            port.postMessage(response);
            port.disconnect();
          });
    });
  });
}
