// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(() => {
  // Map from urlSuffix to list of resolve functions.
  const frameNavigatedDelegatesMap = new Map();
  // Map frameId to list of resolve functions.
  const frameStoppedLoadingDelegatesMap = new Map();

  async function initProtocolRecursively(dp, session, initDelegate) {
    await dp.Target.setAutoAttach(
        {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

    dp.Target.onAttachedToTarget(async event => {
      const childDp = session.createChild(
          event.params.sessionId).protocol;
      await initProtocolRecursively(childDp, session, initDelegate);
    });

    dp.Page.onFrameNavigated(event => {
      // Check if there are any frameNavigated delegates that need to be
      // resolved.
      frameNavigatedDelegatesMap.keys().forEach(urlSuffix => {
        if (event.params.frame.url.endsWith(urlSuffix)) {
          for (const resolve of frameNavigatedDelegatesMap.get(urlSuffix)) {
            resolve(event.params.frameId);
          }
          frameNavigatedDelegatesMap.delete(urlSuffix);
        }
      });
    });

    dp.Page.onFrameStoppedLoading(event => {
      // Check if there are any frameStoppedLoading delegates that need to be
      // resolved.
      (frameStoppedLoadingDelegatesMap.get(event.frameId) ?? []).forEach(
          resolve => {
            resolve();
          });
    });

    await initDelegate(dp);

    await dp.Page.enable();
    await dp.Runtime.runIfWaitingForDebugger();
  };

  function onceFrameNavigated(urlSuffix) {
    return new Promise(resolve => {
      if (frameNavigatedDelegatesMap.get(urlSuffix) === undefined) {
        frameNavigatedDelegatesMap.set(urlSuffix, []);
      }
      frameNavigatedDelegatesMap.get(urlSuffix).push(resolve);
    });
  }

  async function onceFrameStoppedLoading(urlSuffix) {
    const frameId = await onceFrameNavigated(urlSuffix);
    return new Promise(resolve => {
      if (frameStoppedLoadingDelegatesMap.get(frameId) === undefined) {
        frameStoppedLoadingDelegatesMap.set(frameId, []);
      }
      frameStoppedLoadingDelegatesMap.get(frameId).push(resolve);
    });
  }

  return {
    // Call the initDelegate function on the given session and all the attached
    // in future targets.
    initProtocolRecursively,
    // Returns a promise that resolves when a frame with the given urlSuffix
    // has been navigated.
    onceFrameNavigated,
    // Returns a promise that resolves when a frame with the given urlSuffix
    // has been navigated and stopped loading.
    onceFrameStoppedLoading
  };
})()