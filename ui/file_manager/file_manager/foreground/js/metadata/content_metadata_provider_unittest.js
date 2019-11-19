// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function makeFileEntryFromDataURL(name, dataUrl) {
  const mimeString = dataUrl.split(',')[0].split(':')[1].split(';')[0];
  const data = atob(dataUrl.split('base64,')[1]);
  const dataArray = [];
  for (let i = 0; i < data.length; ++i) {
    dataArray.push(data.charCodeAt(i));
  }

  const blob = new Blob([new Uint8Array(dataArray)], {type: mimeString});
  return {
    name: name,
    isDirectory: false,
    url: dataUrl,
    file: function(callback) {
      callback(blob);
    },
    toURL: function() {
      return dataUrl;
    }
  };
}
// clang-format off
const entryA = makeFileEntryFromDataURL(
    'image.jpg',
    'data:image/jpeg;base64,/9j/4QDcRXhpZgAATU0AKgAAAAgABwESAAMAAAABAA' +
    'EAAAEaAAUAAAABAAAAYgEbAAUAAAABAAAAagEoAAMAAAABAAIAAAEyAAIAAAAUAAA' +
    'AcgITAAMAAAABAAEAAIdpAAQAAAABAAAAhgAAAAAAAABIAAAAAQAAAEgAAAABMjAx' +
    'NTowMzoxNyAxMjoyNjowMwAABpAAAAcAAAAEMDIxMJEBAAcAAAAEAQIDAKAAAAcAA' +
    'AAEMDEwMKABAAMAAAAB//8AAKACAAQAAAABAAAAAKADAAQAAAABAAAAAAAAAAD/4A' +
    'AQSkZJRgABAQIARwBHAAD/2wBDAAMCAgICAgMCAgIDAwMDBAYEBAQEBAgGBgUGCQg' +
    'KCgkICQkKDA8MCgsOCwkJDRENDg8QEBEQCgwSExIQEw8QEBD/2wBDAQMDAwQDBAgE' +
    'BAgQCwkLEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQE' +
    'BAQEBAQEBD/wAARCAEAAQADASIAAhEBAxEB/8QAGgABAQEBAQEBAAAAAAAAAAAAAA' +
    'gGBwECCf/EACMQAQACAQQCAgMBAAAAAAAAAAABAgMEBQYRITESUQcTMqH/xAAXAQE' +
    'BAQEAAAAAAAAAAAAAAAAAAwgE/8QAIREBAAICAAYDAAAAAAAAAAAAAAEDAgQFETFh' +
    'kfBBUaH/2gAMAwEAAhEDEQA/AP1DAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' +
    'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' +
    'AAABwjmHMNy5BuWetdVkx6LHeaYcNLdVmseO569zPvz9s2DO+3t27tuV9+XPKffCX' +
    'UAc4AAAAAA9ra1Zi1ZmJj1MNXw7ne67Jr8On1eryajQZLxTJjyW+XwiZ/qsz5jr31' +
    '6n/WTHXp7t+hbF1GUxMfvafuDopsBoVUAAAAAAAAAAAAAAAAABMgDN6QAAAAAAAAP' +
    'a1taYrWJmZ9RDV8O4Juu96/DqNXpMmn0GO8XyZMlfj84if5rE+Z79d+o/x16elfv2' +
    'xTRjMzP53n6g6u3gNCqgAAAAAAAAAAAAAAAAAJkAZvSAAU2A0gqAAAAAAAAAAAAAA' +
    'AAAAAAAAAAAAmQBm9IABTYDSCoAAAAAAAAAAAAAAAAAAAAAAAAACZAGb0gAAAH3jy' +
    '5cNvnhy3pb7raYlseHfkPd9r12DR7pqr6vQ5LRjt+23dsXc9fKLT56j6nx0xY7dHi' +
    'Gxw+2LdfKYmPE9pj5gieSmwGg1QAAAAAAAAAAAAAAAAAAAAAEyAM3pAAAAA+8eLLm' +
    't8MOK97fVazMtjw78ebvumuwazdNLfSaHHaMlv216tl6nv4xWfPU/c+Onbo8P2OIW' +
    'xVr4zMz4jvM/EERzdnAaDVAAAAAAAAAAAAAAAAAAAAAATIAzekAApsBpBUAAAAAAA' +
    'AAAAAAAAAAAAAAAAAABMgDN6QACmwGkFQAAAAAAAAAAAAAAAAAAAAAAAAAEyAM3pA' +
    'AKbAaQVAAAAAAAAAAAAAAAAAAAAAAAAAATINJzDh+5cf3LPaulyZNFkvN8Oale6xW' +
    'fPU9epj15+mbZ329S3Styovx5ZR74S6ANJw/h+5cg3LBa2lyY9FjvF82a9eqzWPPU' +
    'd+5n14+zU1Ld23GijHnlPvg6u7gNEKgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' +
    'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' +
    'AAAAAAAAAAAAP/Z');
// clang-format on

const entryB = makeFileEntryFromDataURL('empty.jpg', 'data:image/jpeg;base64,');

function testExternalMetadataProviderBasic(callback) {
  // Mocking SharedWorker's port.
  const port = /** @type {!MessagePort} */ ({
    postMessage: function(message) {
      if (message.verb === 'request') {
        port.onmessage(/** @type {!MessageEvent} */ ({
          data: {
            verb: 'result',
            arguments: [
              message.arguments[0], {
                thumbnailURL: message.arguments[0] + ',url',
                thumbnailTransform: message.arguments[0] + ',transform'
              }
            ]
          }
        }));
      }
    },
    start: function() {}
  });

  // TODO(ryoh): chrome.mediaGalleries API is not available in unit tests.
  const provider = new ContentMetadataProvider(port);
  reportPromise(
      provider
          .get([
            new MetadataRequest(
                entryA, ['contentThumbnailUrl', 'contentThumbnailTransform']),
            new MetadataRequest(
                entryB, ['contentThumbnailUrl', 'contentThumbnailTransform']),
          ])
          .then(results => {
            assertEquals(2, results.length);
            assertEquals(entryA.url + ',url', results[0].contentThumbnailUrl);
            assertEquals(
                entryA.url + ',transform',
                results[0].contentThumbnailTransform);
            assertEquals(entryB.url + ',url', results[1].contentThumbnailUrl);
            assertEquals(
                entryB.url + ',transform',
                results[1].contentThumbnailTransform);
          }),
      callback);
}
