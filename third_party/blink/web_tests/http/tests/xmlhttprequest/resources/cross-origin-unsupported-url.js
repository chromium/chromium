if (self.importScripts) {
    importScripts('/resources/testharness.js');
}

function testSync(url, contentType) {
  test((t) => {
    const xhr = new XMLHttpRequest();
    xhr.open('POST', url, false);
    xhr.onerror = t.unreached_func('onerror');
    if (contentType) {
      xhr.setRequestHeader('Content-Type', contentType);
    }
    assert_throws_dom('NetworkError', () => xhr.send());
  }, `sync test for url=${url}, contentType=${contentType}`);
}

function testAsync(url, contentType) {
  promise_test((t) => {
    return new Promise(resolve => {
      const xhr = new XMLHttpRequest();
      xhr.open('POST', url, true);
      xhr.onerror = t.step_func((e) => {
        assert_equals(e.type, 'error');
        resolve();
      });
      if (contentType) {
        xhr.setRequestHeader('Content-Type', contentType);
      }
      xhr.send();
    });
  }, `async test for url=${url}, contentType=${contentType}`);
}

const urls = [
  'mailto:foo@bar.com',
  'localhost:8080/',
  'tel:1234',
];

for (const url of urls) {
  testSync(url);
  testSync(url, 'application/json');
  testAsync(url);
  testAsync(url, 'application/json');
}

done();
