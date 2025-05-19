// META: title=Language Model Prompt Multimodal
// META: script=resources/utils.js
// META: timeout=long

'use strict';

/*****************************************
 * Image tests
 *****************************************/

const kValidImagePath = 'resources/media/apple.jpg';
const kImagePrompt = 'describe this image';

promise_test(async (t) => {
  await ensureLanguageModel();
  const newImage = new Image();
  newImage.src = kValidImagePath;
  const session = await LanguageModel.create();

  promise_rejects_dom(
      t, 'NotSupportedError',
      session.prompt([kImagePrompt, {type: 'image', content: newImage}]));
}, 'Prompt image without `image` expectedInput');

promise_test(async () => {
  await ensureLanguageModel();

  const blob = await (await fetch(kValidImagePath)).blob();
  // Start a new session.
  const session =
      await LanguageModel.create({expectedInputs: [{type: 'image'}]});

  const result =
      await session.prompt([kImagePrompt, {type: 'image', content: blob}]);
  assert_regexp_match(result, /<image>/);
}, 'Prompt with Blob image content');


promise_test(async () => {
  await ensureLanguageModel();
  const blob = await (await fetch(kValidImagePath)).blob();
  const bitmap = await createImageBitmap(blob);
  const session =
      await LanguageModel.create({expectedInputs: [{type: 'image'}]});

  const result =
      await session.prompt([kImagePrompt, {type: 'image', content: bitmap}]);
  assert_regexp_match(result, /<image>/);
}, 'Prompt with ImageBitmap image content');

promise_test(async () => {
  await ensureLanguageModel();
  const session =
      await LanguageModel.create({expectedInputs: [{type: 'image'}]});

  const result = await session.prompt(
      [kImagePrompt, {type: 'image', content: new ImageData(256, 256)}]);
  assert_regexp_match(result, /<image>/);
}, 'Prompt with ImageData image content');

promise_test(async () => {
  await ensureLanguageModel();
  const blob = await (await fetch(kValidImagePath)).blob();
  const bitmap = await createImageBitmap(blob);
  const frame = new VideoFrame(bitmap, {timestamp: 1});
  const session =
      await LanguageModel.create({expectedInputs: [{type: 'image'}]});

  const result =
      await session.prompt([kImagePrompt, {type: 'image', content: frame}]);
  assert_regexp_match(result, /<image>/);
}, 'Prompt with VideoFrame image content');

promise_test(async () => {
  await ensureLanguageModel();
  const canvas = new OffscreenCanvas(512, 512);
  // Requires a context to convert to a bitmap.
  var context = canvas.getContext('2d');
  context.fillRect(10, 10, 200, 200);

  // Start a new session.
  const session =
      await LanguageModel.create({expectedInputs: [{type: 'image'}]});

  const result =
      await session.prompt([kImagePrompt, {type: 'image', content: canvas}]);
  assert_regexp_match(result, /<image>/);
}, 'Prompt with OffscreenCanvas image content');


promise_test(async () => {
  await ensureLanguageModel();
  const newImage = new Image();
  newImage.src = kValidImagePath;
  const session =
      await LanguageModel.create({expectedInputs: [{type: 'image'}]});

  const result =
      await session.prompt([kImagePrompt, {type: 'image', content: newImage}]);
  assert_regexp_match(result, /<image>/);
}, 'Prompt with HTMLImageElement image content');

promise_test(async () => {
  await ensureLanguageModel();
  var canvas = document.createElement('canvas');
  canvas.width = 1224;
  canvas.height = 768;
  const session =
      await LanguageModel.create({expectedInputs: [{type: 'image'}]});

  const result =
      await session.prompt([kImagePrompt, {type: 'image', content: canvas}]);
  assert_regexp_match(result, /<image>/);
}, 'Prompt with HTMLCanvasElement image content');

promise_test(async () => {
  await ensureLanguageModel();
  const image_data = await fetch(kValidImagePath);
  const session =
      await LanguageModel.create({expectedInputs: [{type: 'image'}]});

  const result = await session.prompt(
      [kImagePrompt, {type: 'image', content: await image_data.arrayBuffer()}]);
  assert_regexp_match(result, /<image>/);
}, 'Prompt with ArrayBuffer image content');

/*****************************************
 * Audio tests
 *****************************************/

const kValidAudioPath = 'resources/media/speech.mp3';

promise_test(async (t) => {
  await ensureLanguageModel();
  const blob = await (await fetch(kValidAudioPath)).blob();
  const session = await LanguageModel.create();

  promise_rejects_dom(
      t, 'NotSupportedError',
      session.prompt([kImagePrompt, {type: 'audio', content: blob}]));
}, 'Prompt audio without `audio` expectedInput');

promise_test(async () => {
  await ensureLanguageModel();
  const blob = await (await fetch(kValidAudioPath)).blob();
  const session =
      await LanguageModel.create({expectedInputs: [{type: 'audio'}]});

  const result =
      await session.prompt([kImagePrompt, {type: 'audio', content: blob}]);
  assert_regexp_match(result, /<audio>/);
}, 'Prompt with Blob audio content');

promise_test(async (t) => {
  await ensureLanguageModel();

  const blob = await (await fetch(kValidImagePath)).blob();
  const session =
      await LanguageModel.create({expectedInputs: [{type: 'audio'}]});

  // TODO(crbug.com/409615288): This should throw a TypeError according to the
  // spec.
  promise_rejects_dom(
      t, 'DataError',
      session.prompt([kImagePrompt, {type: 'audio', content: blob}]));
}, 'Prompt audio with blob containing invalid audio data.');

promise_test(async () => {
  await ensureLanguageModel();
  const audio_data = await fetch(kValidAudioPath);
  const audioCtx = new AudioContext();
  const buffer = await audioCtx.decodeAudioData(await audio_data.arrayBuffer());

  const session =
      await LanguageModel.create({expectedInputs: [{type: 'audio'}]});

  const result =
      await session.prompt([kImagePrompt, {type: 'audio', content: buffer}]);
  assert_regexp_match(result, /<audio>/);
}, 'Prompt with AudioBuffer');

promise_test(async () => {
  await ensureLanguageModel();
  const audio_data = await fetch(kValidAudioPath);

  const session =
      await LanguageModel.create({expectedInputs: [{type: 'audio'}]});

  const result = await session.prompt(
      [kImagePrompt, {type: 'audio', content: await audio_data.arrayBuffer()}]);
  assert_regexp_match(result, /<audio>/);
}, 'Prompt with BufferSource - ArrayBuffer');
