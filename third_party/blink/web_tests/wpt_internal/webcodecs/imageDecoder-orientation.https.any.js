// META: global=window,dedicatedworker

// These tests depend on ImageDecoder exposing oriented images as oriented
// VideoFrames in a passthrough manner. They are not required by the
// specification (which places requirements on the result of rendering a frame
// according to its metadata, rather than the contents of the underlying
// buffer).
function decodeWithOrientation(orientation) {
  return fetch('four-colors.jpg')
      .then(response => response.arrayBuffer())
      .then(buffer => {
        let u8buffer = new Uint8Array(buffer);
        u8buffer[0x1F] = orientation;  // Location derived via diff.
        let decoder = new ImageDecoder({data: u8buffer, type: 'image/jpeg'});
        return decoder.decode();
      })
      .then(result => result.image);
}

promise_test(t => {
  return decodeWithOrientation(1)
      .then(image => {
        assert_equals(image.rotation, 0);
        assert_equals(image.flip, false);
      });
}, 'Test JPEG with EXIF orientation top-left');

promise_test(t => {
  return decodeWithOrientation(2)
      .then(image => {
        assert_equals(image.rotation, 0);
        assert_equals(image.flip, true);
      });
}, 'Test JPEG with EXIF orientation top-right');

promise_test(t => {
  return decodeWithOrientation(3)
      .then(image => {
        assert_equals(image.rotation, 180);
        assert_equals(image.flip, false);
      });
}, 'Test JPEG with EXIF orientation bottom-right');

promise_test(t => {
  return decodeWithOrientation(4)
      .then(image => {
        assert_equals(image.rotation, 180);
        assert_equals(image.flip, true);
      });
}, 'Test JPEG with EXIF orientation bottom-left');

promise_test(t => {
  return decodeWithOrientation(5)
      .then(image => {
        assert_equals(image.rotation, 90);
        assert_equals(image.flip, true);
      });
}, 'Test JPEG with EXIF orientation left-top');

promise_test(t => {
  return decodeWithOrientation(6)
      .then(image => {
        assert_equals(image.rotation, 90);
        assert_equals(image.flip, false);
      });
}, 'Test JPEG with EXIF orientation right-top');

promise_test(t => {
  return decodeWithOrientation(7)
      .then(image => {
        assert_equals(image.rotation, 270);
        assert_equals(image.flip, true);
      });
}, 'Test JPEG with EXIF orientation right-bottom');

promise_test(t => {
  return decodeWithOrientation(8)
      .then(image => {
        assert_equals(image.rotation, 270);
        assert_equals(image.flip, false);
      });
}, 'Test JPEG with EXIF orientation left-bottom');
