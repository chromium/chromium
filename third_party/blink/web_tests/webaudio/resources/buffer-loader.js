// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// BufferLoader – utility for fetching & decoding multiple audio files.
function BufferLoader(context, urlList, callback, reject) {
  this.context = context;
  this.urlList = urlList;
  this.onload = callback;
  this.onerror = reject;
  this.bufferList = new Array();
  this.loadCount = 0;
}

// BufferLoader – utility for fetching & decoding multiple audio files.
BufferLoader.prototype.loadBuffer = function(url, index) {
  // Load buffer asynchronously
  let request = new XMLHttpRequest();
  request.open('GET', url, true);
  request.responseType = 'arraybuffer';

  let loader = this;

  request.onload = function() {
    loader.context.decodeAudioData(
        request.response,
        function(decodedAudio) {
          try {
            loader.bufferList[index] = decodedAudio;
            if (++loader.loadCount === loader.urlList.length)
              loader.onload(loader.bufferList);
          } catch (e) {
            loader.onerror(
                'BufferLoader: unable to load buffer ' + index +
                ', url: ' + loader.urlList[index] + ' error: ' + e);
          }
        },
        function() {
          loader.onerror('error decoding file data: ' + url);
        });
  };

  request.onerror = function() {
    loader.onerror('BufferLoader: XHR error');
  };

  request.send();
};

BufferLoader.prototype.load = function() {
  for (let i = 0; i < this.urlList.length; ++i)
    this.loadBuffer(this.urlList[i], i);
};

// Returns a promise that resolves with an array of AudioBuffers once all
// resources have loaded.
function loadBuffers(context, urls) {
  return new Promise((resolve, reject) => {
    const loader = new BufferLoader(context, urls, resolve, reject);
    loader.load();
  });
}
