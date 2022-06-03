/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.net.FileDownloaderTest');
goog.setTestOnly();

const ErrorCode = goog.require('goog.net.ErrorCode');
const FileDownloader = goog.require('goog.net.FileDownloader');
const FsError = goog.require('goog.fs.Error');
const FsFileSystem = goog.require('goog.testing.fs.FileSystem');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const TestCase = goog.require('goog.testing.TestCase');
const XhrIo = goog.require('goog.net.XhrIo');
const XhrIoPool = goog.require('goog.testing.net.XhrIoPool');
const dispose = goog.require('goog.dispose');
const testSuite = goog.require('goog.testing.testSuite');
const testingFs = goog.require('goog.testing.fs');

let dir;
let downloader;
let fs;
let xhr;
let xhrIoPool;

function assertMatches(expected, actual) {
  assert(`Expected "${actual}" to match ${expected}`, expected.test(actual));
}
testSuite({
  setUpPage() {
    testingFs.install(new PropertyReplacer());
    TestCase.getActiveTestCase().promiseTimeout = 10000;  // 10s
  },

  setUp() {
    xhrIoPool = new XhrIoPool();
    xhr = xhrIoPool.getXhr();
    fs = new FsFileSystem();
    dir = fs.getRoot();
    downloader = new FileDownloader(dir, xhrIoPool);
  },

  tearDown() {
    dispose(downloader);
  },

  testDownload() {
    const promise = downloader.download('/foo/bar').then((blob) => {
      const fileEntry = dir.getFileSync('`3fa/``2Ffoo`2Fbar/`bar');
      assertEquals('data', blob.toString());
      assertEquals('data', fileEntry.fileSync().toString());
    });

    xhr.simulateResponse(200, 'data');
    assertEquals('/foo/bar', xhr.getLastUri());
    assertEquals(XhrIo.ResponseType.ARRAY_BUFFER, xhr.getResponseType());

    return promise;
  },

  testGetDownloadedBlob() {
    const promise = downloader.download('/foo/bar')
                        .then(() => downloader.getDownloadedBlob('/foo/bar'))
                        .then((blob) => {
                          assertEquals('data', blob.toString());
                        });

    xhr.simulateResponse(200, 'data');
    return promise;
  },

  testGetLocalUrl() {
    const promise = downloader.download('/foo/bar')
                        .then(() => downloader.getLocalUrl('/foo/bar'))
                        .then((url) => {
                          assertMatches(/\/`bar$/, url);
                        });

    xhr.simulateResponse(200, 'data');
    return promise;
  },

  testLocalUrlWithContentDisposition() {
    const promise = downloader.download('/foo/bar')
                        .then(() => downloader.getLocalUrl('/foo/bar'))
                        .then((url) => {
                          assertMatches(/\/`qux`22bap$/, url);
                        });

    xhr.simulateResponse(
        200, 'data',
        {'Content-Disposition': 'attachment; filename="qux\\"bap"'});
    return promise;
  },

  testIsDownloaded() {
    const promise =
        downloader.download('/foo/bar')
            .then(() => downloader.isDownloaded('/foo/bar'))
            .then(assertTrue)
            .then((isDownloaded) => downloader.isDownloaded('/foo/baz'))
            .then(assertFalse);

    xhr.simulateResponse(200, 'data');
    return promise;
  },

  testRemove() {
    const promise =
        downloader.download('/foo/bar')
            .then(() => downloader.remove('/foo/bar'))
            .then(() => downloader.isDownloaded('/foo/bar'))
            .then(assertFalse)
            .then(() => downloader.getDownloadedBlob('/foo/bar'))
            .then(
                () => {
                  fail('Should not be able to download a missing blob.');
                },
                (err) => {
                  assertEquals(FsError.ErrorCode.NOT_FOUND, err.code);
                  const download = downloader.download('/foo/bar');
                  xhr.simulateResponse(200, 'more data');
                  return download;
                })
            .then(() => downloader.isDownloaded('/foo/bar'))
            .then(assertTrue)
            .then(() => downloader.getDownloadedBlob('/foo/bar'))
            .then((blob) => {
              assertEquals('more data', blob.toString());
            });

    xhr.simulateResponse(200, 'data');
    return promise;
  },

  testSetBlob() {
    return downloader.setBlob('/foo/bar', testingFs.getBlob('data'))
        .then(() => downloader.isDownloaded('/foo/bar'))
        .then(assertTrue)
        .then(() => downloader.getDownloadedBlob('/foo/bar'))
        .then((blob) => {
          assertEquals('data', blob.toString());
        });
  },

  testSetBlobWithName() {
    return downloader.setBlob('/foo/bar', testingFs.getBlob('data'), 'qux')
        .then(() => downloader.getLocalUrl('/foo/bar'))
        .then((url) => {
          assertMatches(/\/`qux$/, url);
        });
  },

  testDownloadDuringDownload() {
    const download1 = downloader.download('/foo/bar');
    const download2 = downloader.download('/foo/bar');

    const promise = download1.then(() => download2)
                        .then(() => downloader.getDownloadedBlob('/foo/bar'))
                        .then((blob) => {
                          assertEquals('data', blob.toString());
                        });

    // There should only need to be one response for both downloads, since the
    // second should return the same deferred as the first.
    xhr.simulateResponse(200, 'data');
    return promise;
  },

  testGetDownloadedBlobDuringDownload() {
    let hasDownloaded = false;
    downloader.download('/foo/bar').then(() => {
      hasDownloaded = true;
    });

    const promise = downloader.waitForDownload('/foo/bar')
                        .then(() => downloader.getDownloadedBlob('/foo/bar'))
                        .then((blob) => {
                          assertTrue(hasDownloaded);
                          assertEquals('data', blob.toString());
                        });

    xhr.simulateResponse(200, 'data');
    return promise;
  },

  testIsDownloadedDuringDownload() {
    let hasDownloaded = false;
    downloader.download('/foo/bar').then(() => {
      hasDownloaded = true;
    });

    const promise = downloader.waitForDownload('/foo/bar')
                        .then(() => downloader.isDownloaded('/foo/bar'))
                        .then(() => {
                          assertTrue(hasDownloaded);
                        });

    xhr.simulateResponse(200, 'data');
    return promise;
  },

  testRemoveDuringDownload() {
    let hasDownloaded = false;
    downloader.download('/foo/bar').then(() => {
      hasDownloaded = true;
    });

    const promise = downloader.waitForDownload('/foo/bar')
                        .then(() => downloader.remove('/foo/bar'))
                        .then(() => {
                          assertTrue(hasDownloaded);
                        })
                        .then(() => downloader.isDownloaded('/foo/bar'))
                        .then(assertFalse);

    xhr.simulateResponse(200, 'data');
    return promise;
  },

  testSetBlobDuringDownload() {
    const download = downloader.download('/foo/bar');

    const promise =
        downloader.waitForDownload('/foo/bar')
            .then(
                () => downloader.setBlob(
                    '/foo/bar', testingFs.getBlob('blob data')))
            .then(
                () => {
                  fail('Should not be able to set blob during a download.');
                },
                (err) => {
                  assertEquals(
                      FsError.ErrorCode.INVALID_MODIFICATION,
                      err.fileError.code);
                  return download;
                })
            .then(() => downloader.getDownloadedBlob('/foo/bar'))
            .then((b) => {
              assertEquals('xhr data', b.toString());
            });

    xhr.simulateResponse(200, 'xhr data');
    return promise;
  },

  testDownloadCanceledBeforeXhr() {
    const download = downloader.download('/foo/bar');

    const promise =
        download
            .then(
                () => {
                  fail('Download should have been canceled.');
                },
                () => {
                  assertEquals('/foo/bar', xhr.getLastUri());
                  assertEquals(ErrorCode.ABORT, xhr.getLastErrorCode());
                  assertFalse(xhr.isActive());

                  return downloader.isDownloaded('/foo/bar');
                })
            .then(assertFalse);

    download.cancel();
    return promise;
  },

  testDownloadCanceledAfterXhr() {
    const download = downloader.download('/foo/bar');
    xhr.simulateResponse(200, 'data');
    download.cancel();

    return download
        .then(
            () => {
              fail('Should not succeed after cancellation.');
            },
            () => {
              assertEquals('/foo/bar', xhr.getLastUri());
              assertEquals(ErrorCode.NO_ERROR, xhr.getLastErrorCode());
              assertFalse(xhr.isActive());

              return downloader.isDownloaded('/foo/bar');
            })
        .then(assertFalse);
  },

  testFailedXhr() {
    const promise =
        downloader.download('/foo/bar')
            .then(
                () => {
                  fail('Download should not have succeeded.');
                },
                (err) => {
                  assertEquals('/foo/bar', err.url);
                  assertEquals(404, err.xhrStatus);
                  assertEquals(ErrorCode.HTTP_ERROR, err.xhrErrorCode);
                  assertUndefined(err.fileError);

                  return downloader.isDownloaded('/foo/bar');
                })
            .then(assertFalse);

    xhr.simulateResponse(404);
    return promise;
  },

  testFailedDownloadSave() {
    const promise =
        downloader.download('/foo/bar')
            .then(() => {
              const download = downloader.download('/foo/bar');
              xhr.simulateResponse(200, 'data');
              return download;
            })
            .then(
                () => {
                  fail('Should not be able to modify an active download.');
                },
                (err) => {
                  assertEquals('/foo/bar', err.url);
                  assertUndefined(err.xhrStatus);
                  assertUndefined(err.xhrErrorCode);
                  assertEquals(
                      FsError.ErrorCode.INVALID_MODIFICATION,
                      err.fileError.code);
                });

    xhr.simulateResponse(200, 'data');
    return promise;
  },

  testFailedGetDownloadedBlob() {
    return downloader.getDownloadedBlob('/foo/bar')
        .then(
            () => {
              fail('Should not be able to get a missing blob.');
            },
            (err) => {
              assertEquals(FsError.ErrorCode.NOT_FOUND, err.code);
            });
  },

  testFailedRemove() {
    return downloader.remove('/foo/bar')
        .then(
            () => {
              fail('Should not be able to remove a missing file.');
            },
            (err) => {
              assertEquals(FsError.ErrorCode.NOT_FOUND, err.code);
            });
  },

  testIsDownloading() {
    assertFalse(downloader.isDownloading('/foo/bar'));
    const promise = downloader.download('/foo/bar').then(() => {
      assertFalse(downloader.isDownloading('/foo/bar'));
    });

    assertTrue(downloader.isDownloading('/foo/bar'));
    xhr.simulateResponse(200, 'data');
    return promise;
  },

  testIsDownloadingWhenCancelled() {
    assertFalse(downloader.isDownloading('/foo/bar'));
    const deferred = downloader.download('/foo/bar').addErrback(() => {
      assertFalse(downloader.isDownloading('/foo/bar'));
    });

    assertTrue(downloader.isDownloading('/foo/bar'));
    deferred.cancel();
  },
});
