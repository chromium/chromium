// META: global=window,dedicatedworker
// META: script=/webcodecs/utils.js

// TODO(crbug.com/1274220): Move these back to the public wpt test set once
// HDR canvas support has actually launched. Currently there's no way to feature
// detect HDR canvas support from a worker.

promise_test(() => {
  return testImageBitmapToAndFromVideoFrame(
      48, 36, kP3Pixel, kCanvasOptionsP3Uint8, {colorSpaceConversion: 'none'},
      kImageSettingOptionsP3Uint8);
}, 'ImageBitmap<->VideoFrame with canvas(48x36 display-p3 uint8).');

promise_test(() => {
  return testImageBitmapToAndFromVideoFrame(
      480, 360, kP3Pixel, kCanvasOptionsP3Uint8, {colorSpaceConversion: 'none'},
      kImageSettingOptionsP3Uint8);
}, 'ImageBitmap<->VideoFrame with canvas(480x360 display-p3 uint8).');

promise_test(() => {
  return testImageBitmapToAndFromVideoFrame(
      48, 36, kRec2020Pixel, kCanvasOptionsRec2020Uint8,
      {colorSpaceConversion: 'none'}, kImageSettingOptionsRec2020Uint8);
}, 'ImageBitmap<->VideoFrame with canvas(48x36 rec2020 uint8).');

promise_test(() => {
  return testImageBitmapToAndFromVideoFrame(
      480, 360, kRec2020Pixel, kCanvasOptionsRec2020Uint8,
      {colorSpaceConversion: 'none'}, kImageSettingOptionsRec2020Uint8);
}, 'ImageBitmap<->VideoFrame with canvas(480x360 rec2020 uint8).');
