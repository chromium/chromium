// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kBackgroundFetchId = 'bg-fetch-id';
const kBackgroundFetchResource =
    ['/weblayer/test/data/background_fetch/types_of_cheese.txt'];

function RegisterServiceWorker() {
  navigator.serviceWorker.register('sw.js').then(() => {
    console.log('service worker registered');
  });
}

// Starts a Background Fetch request for a single to-be-downloaded file.
function StartSingleFileDownload() {
  navigator.serviceWorker.ready
      .then(swRegistration => {
        const options = {title: 'Single-file Background Fetch'};

        return swRegistration.backgroundFetch.fetch(
            kBackgroundFetchId, kBackgroundFetchResource, options);
      })
      .then(bgFetchRegistration => {
        console.log('bg fetch started');
      })
      .catch(error => {
        console.log(error);
      });
}

document.addEventListener('touchend', function(e) {
  RegisterServiceWorker();
  StartSingleFileDownload();
}, false);
