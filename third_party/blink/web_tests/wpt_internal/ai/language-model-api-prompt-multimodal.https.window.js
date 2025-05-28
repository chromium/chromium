// META: title=Language Model Prompt Multimodal
// META: script=resources/utils.js
// META: timeout=long

'use strict';

const kPrompt = 'describe this';
const kValidImagePath = 'resources/media/apple.jpg';
const kValidAudioPath = 'resources/media/speech.mp3';
const kValidSVGImagePath = 'resources/media/wikipedia-logo.svg';
const kValidVideoPath = 'resources/media/video.mp4';

const kImageOptions = {expectedInputs: [{type: 'image'}]};
const kAudioOptions = {expectedInputs: [{type: 'audio'}]};

function messageWithContent(prompt, type, value) {
  return [{
    role: 'user',
    content: [{type: 'text', value: prompt}, {type: type, value: value}]
  }];
}

/*****************************************
 * General tests
 *****************************************/

promise_test(async t => {
  await ensureLanguageModel(kImageOptions);
  const newImage = new Image();
  newImage.src = kValidImagePath;
  const session = await LanguageModel.create(kImageOptions);
  // TODO(crbug.com/409615288): Expect a TypeError according to the spec.
  promise_rejects_dom(
      t, 'SyntaxError',
      session.prompt(messageWithContent(kPrompt, 'text', newImage)));
}, 'Prompt with type:"text" and image content should reject');

promise_test(async t => {
  await ensureLanguageModel(kImageOptions);
  const newImage = new Image();
  newImage.src = kValidImagePath;
  const session = await LanguageModel.create(kImageOptions);
  promise_rejects_dom(t, 'NotSupportedError', session.prompt([
    {role: 'assistant', content: [{type: 'image', value: newImage}]}
  ]));
}, 'Prompt with assistant role should reject with multimodal input');

/*****************************************
 * Image tests
 *****************************************/

promise_test(async (t) => {
  await ensureLanguageModel();
  const newImage = new Image();
  newImage.src = kValidImagePath;
  const session = await LanguageModel.create();
  promise_rejects_dom(
      t, 'NotSupportedError',
      session.prompt(messageWithContent(kPrompt, 'image', newImage)));
}, 'Prompt image without `image` expectedInput');

promise_test(async () => {
  await ensureLanguageModel(kImageOptions);
  const blob = await (await fetch(kValidImagePath)).blob();
  const session = await LanguageModel.create(kImageOptions);
  const result =
      await session.prompt(messageWithContent(kPrompt, 'image', blob));
  assert_regexp_match(result, /<image>/);
}, 'Prompt with Blob image content');

promise_test(async () => {
  await ensureLanguageModel(kImageOptions);
  const blob = await (await fetch(kValidImagePath)).blob();
  const bitmap = await createImageBitmap(blob);
  const session = await LanguageModel.create(kImageOptions);
  const result =
      await session.prompt(messageWithContent(kPrompt, 'image', bitmap));
  assert_regexp_match(result, /<image>/);
}, 'Prompt with ImageBitmap image content');

promise_test(async () => {
  await ensureLanguageModel(kImageOptions);
  const blob = await (await fetch(kValidImagePath)).blob();
  const bitmap = await createImageBitmap(blob);
  const frame = new VideoFrame(bitmap, {timestamp: 1});
  const session = await LanguageModel.create(kImageOptions);
  const result =
      await session.prompt(messageWithContent(kPrompt, 'image', frame));
  frame.close();  // Avoid JS garbage collection warning.
  assert_regexp_match(result, /<image>/);
}, 'Prompt with VideoFrame image content');

promise_test(async () => {
  await ensureLanguageModel(kImageOptions);
  const canvas = new OffscreenCanvas(512, 512);
  // Requires a context to convert to a bitmap.
  var context = canvas.getContext('2d');
  context.fillRect(10, 10, 200, 200);
  const session = await LanguageModel.create(kImageOptions);
  const result =
      await session.prompt(messageWithContent(kPrompt, 'image', canvas));
  assert_regexp_match(result, /<image>/);
}, 'Prompt with OffscreenCanvas image content');

promise_test(async () => {
  await ensureLanguageModel(kImageOptions);
  const session = await LanguageModel.create(kImageOptions);
  const result = await session.prompt(
      messageWithContent(kPrompt, 'image', new ImageData(256, 256)));
  assert_regexp_match(result, /<image>/);
}, 'Prompt with ImageData image content');

promise_test(async () => {
  await ensureLanguageModel(kImageOptions);
  const newImage = new Image();
  newImage.src = kValidImagePath;
  const session = await LanguageModel.create(kImageOptions);
  const result =
      await session.prompt(messageWithContent(kPrompt, 'image', newImage));
  assert_regexp_match(result, /<image>/);
}, 'Prompt with HTMLImageElement image content');

promise_test(async () => {
  await ensureLanguageModel(kImageOptions);
  var canvas = document.createElement('canvas');
  canvas.width = 1224;
  canvas.height = 768;
  const session = await LanguageModel.create(kImageOptions);
  const result =
      await session.prompt(messageWithContent(kPrompt, 'image', canvas));
  assert_regexp_match(result, /<image>/);
}, 'Prompt with HTMLCanvasElement image content');

promise_test(async () => {
  await ensureLanguageModel(kImageOptions);
  const image_data = await fetch(kValidImagePath);
  const session = await LanguageModel.create(kImageOptions);
  const result = await session.prompt(
      messageWithContent(kPrompt, 'image', await image_data.arrayBuffer()));
  assert_regexp_match(result, /<image>/);
}, 'Prompt with ArrayBuffer image content');

promise_test(async () => {
  await ensureLanguageModel(kImageOptions);
  const image_data = await fetch(kValidImagePath);
  const session = await LanguageModel.create(kImageOptions);
  const result = await session.prompt(messageWithContent(
      kPrompt, 'image', new DataView(await image_data.arrayBuffer())));
  assert_regexp_match(result, /<image>/);
}, 'Prompt with ArrayBufferView image content');

promise_test(async () => {
  await ensureLanguageModel(kImageOptions);
  const newImage = new Image();
  newImage.src = kValidSVGImagePath;
  const session = await LanguageModel.create(kImageOptions);
  const result =
      await session.prompt(messageWithContent(
        kPrompt, 'image', newImage));
  assert_regexp_match(result, /<image>/);
}, 'Prompt with HTMLImageElement image content (with SVG)');


promise_test(async () => {
  await ensureLanguageModel(kImageOptions);
  const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
  svg.setAttribute('width', '100');
  svg.setAttribute('height', '100');
  const svgImage =
      document.createElementNS('http://www.w3.org/2000/svg', 'image');
  svgImage.setAttribute('href', kValidImagePath);
  svgImage.setAttribute('decoding', 'sync');
  svg.appendChild(svgImage);
  document.body.appendChild(svg);

  // Must wait for the SVG and image to load first.
  // TODO(crbug.com/417260923): Make prompt Api await the image to be loaded.
  const {promise, resolve} = Promise.withResolvers();
  svgImage.addEventListener('load', resolve);
  await promise;
  const session = await LanguageModel.create(kImageOptions);
  const result =
      await session.prompt(messageWithContent(
        kPrompt, 'image', svgImage));
  assert_regexp_match(result, /<image>/);
}, 'Prompt with SVGImageElement image content');

promise_test(async () => {
  await ensureLanguageModel(kImageOptions);
  var video = document.createElement('video');
  video.src = kValidVideoPath;
  video.width = 1224;
  video.height = 768;
  // Video must have frames fetched. See crbug.com/417249941#comment3
  await video.play();
  const session = await LanguageModel.create(kImageOptions);
  const result =
      await session.prompt(messageWithContent(
        kPrompt, 'image', video));
  assert_regexp_match(result, /<image>/);
}, 'Prompt with HTMLVideoElement image content');

/*****************************************
 * Audio tests
 *****************************************/

promise_test(async (t) => {
  await ensureLanguageModel();
  const blob = await (await fetch(kValidAudioPath)).blob();
  const session = await LanguageModel.create();
  promise_rejects_dom(
      t, 'NotSupportedError',
      session.prompt(messageWithContent(kPrompt, 'audio', blob)));
}, 'Prompt audio without `audio` expectedInput');

promise_test(async () => {
  await ensureLanguageModel(kAudioOptions);
  const blob = await (await fetch(kValidAudioPath)).blob();
  const session = await LanguageModel.create(kAudioOptions);
  const result =
      await session.prompt(messageWithContent(kPrompt, 'audio', blob));
  assert_regexp_match(result, /<audio>/);
}, 'Prompt with Blob audio content');

promise_test(async (t) => {
  await ensureLanguageModel(kAudioOptions);
  const blob = await (await fetch(kValidImagePath)).blob();
  const session = await LanguageModel.create(kAudioOptions);
  // TODO(crbug.com/409615288): Expect a TypeError according to the spec.
  promise_rejects_dom(
      t, 'DataError',
      session.prompt(messageWithContent(kPrompt, 'audio', blob)));
}, 'Prompt audio with blob containing invalid audio data.');

promise_test(async () => {
  await ensureLanguageModel(kAudioOptions);
  const audio_data = await fetch(kValidAudioPath);
  const audioCtx = new AudioContext();
  const buffer = await audioCtx.decodeAudioData(await audio_data.arrayBuffer());
  const session = await LanguageModel.create(kAudioOptions);
  const result =
      await session.prompt(messageWithContent(kPrompt, 'audio', buffer));
  assert_regexp_match(result, /<audio>/);
}, 'Prompt with AudioBuffer');

promise_test(async () => {
  await ensureLanguageModel(kAudioOptions);
  const audio_data = await fetch(kValidAudioPath);
  const session = await LanguageModel.create(kAudioOptions);
  const result = await session.prompt(
      messageWithContent(kPrompt, 'audio', await audio_data.arrayBuffer()));
  assert_regexp_match(result, /<audio>/);
}, 'Prompt with BufferSource - ArrayBuffer');
