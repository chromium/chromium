// META: title=Language Model Prompt Multimodal
// META: script=resources/utils.js
// META: timeout=long

'use strict';

const kPrompt = 'describe this';
const kValidImagePath = 'resources/media/apple.jpg';
const kValidAudioPath = 'resources/media/speech.mp3';

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
  await ensureLanguageModel();

  const newImage = new Image();
  newImage.src = kValidImagePath;
  const session =
      await LanguageModel.create({expectedInputs: [{type: 'image'}]});

  // TODO(crbug.com/409615288): This should throw a TypeError according to the
  // spec.
  promise_rejects_dom(
      t, 'SyntaxError',
      session.prompt(messageWithContent(kPrompt, 'text', newImage)));
}, 'Prompt with type:"text" and image content should reject');

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
  await ensureLanguageModel();
  const blob = await (await fetch(kValidImagePath)).blob();
  const session =
      await LanguageModel.create({expectedInputs: [{type: 'image'}]});

  const result =
      await session.prompt(messageWithContent(kPrompt, 'image', blob));

  assert_regexp_match(result, /<image>/);
}, 'Prompt with Blob image content');

promise_test(async () => {
  await ensureLanguageModel();
  const blob = await (await fetch(kValidImagePath)).blob();
  const bitmap = await createImageBitmap(blob);
  const session =
      await LanguageModel.create({expectedInputs: [{type: 'image'}]});

  const result =
      await session.prompt(messageWithContent(kPrompt, 'image', bitmap));
  assert_regexp_match(result, /<image>/);
}, 'Prompt with ImageBitmap image content');

promise_test(async () => {
  await ensureLanguageModel();
  const blob = await (await fetch(kValidImagePath)).blob();
  const bitmap = await createImageBitmap(blob);
  const frame = new VideoFrame(bitmap, {timestamp: 1});
  const session =
      await LanguageModel.create({expectedInputs: [{type: 'image'}]});

  const result =
      await session.prompt(messageWithContent(kPrompt, 'image', frame));
  frame.close();  // Avoid JS garbage collection warning.
  assert_regexp_match(result, /<image>/);
}, 'Prompt with VideoFrame image content');

promise_test(async () => {
  await ensureLanguageModel();
  const canvas = new OffscreenCanvas(512, 512);
  // Requires a context to convert to a bitmap.
  var context = canvas.getContext('2d');
  context.fillRect(10, 10, 200, 200);
  const session =
      await LanguageModel.create({expectedInputs: [{type: 'image'}]});

  const result =
      await session.prompt(messageWithContent(kPrompt, 'image', canvas));
  assert_regexp_match(result, /<image>/);
}, 'Prompt with OffscreenCanvas image content');

promise_test(async () => {
  await ensureLanguageModel();
  const session =
      await LanguageModel.create({expectedInputs: [{type: 'image'}]});

  const result = await session.prompt(
      messageWithContent(kPrompt, 'image', new ImageData(256, 256)));
  assert_regexp_match(result, /<image>/);
}, 'Prompt with ImageData image content');

promise_test(async () => {
  await ensureLanguageModel();
  const newImage = new Image();
  newImage.src = kValidImagePath;
  const session =
      await LanguageModel.create({expectedInputs: [{type: 'image'}]});

  const result =
      await session.prompt(messageWithContent(kPrompt, 'image', newImage));
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
      await session.prompt(messageWithContent(kPrompt, 'image', canvas));
  assert_regexp_match(result, /<image>/);
}, 'Prompt with HTMLCanvasElement image content');

promise_test(async () => {
  await ensureLanguageModel();
  const image_data = await fetch(kValidImagePath);
  const session =
      await LanguageModel.create({expectedInputs: [{type: 'image'}]});

  const result = await session.prompt(
      messageWithContent(kPrompt, 'image', await image_data.arrayBuffer()));
  assert_regexp_match(result, /<image>/);
}, 'Prompt with ArrayBuffer image content');

promise_test(async () => {
  await ensureLanguageModel();
  const image_data = await fetch(kValidImagePath);
  const session =
      await LanguageModel.create({expectedInputs: [{type: 'image'}]});

  const result = await session.prompt(messageWithContent(
      kPrompt, 'image', new DataView(await image_data.arrayBuffer())));
  assert_regexp_match(result, /<image>/);
}, 'Prompt with ArrayBufferView image content');

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
  await ensureLanguageModel();
  const blob = await (await fetch(kValidAudioPath)).blob();
  const session =
      await LanguageModel.create({expectedInputs: [{type: 'audio'}]});

  const result =
      await session.prompt(messageWithContent(kPrompt, 'audio', blob));
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
      session.prompt(messageWithContent(kPrompt, 'audio', blob)));
}, 'Prompt audio with blob containing invalid audio data.');

promise_test(async () => {
  await ensureLanguageModel();
  const audio_data = await fetch(kValidAudioPath);
  const audioCtx = new AudioContext();
  const buffer = await audioCtx.decodeAudioData(await audio_data.arrayBuffer());
  const session =
      await LanguageModel.create({expectedInputs: [{type: 'audio'}]});

  const result =
      await session.prompt(messageWithContent(kPrompt, 'audio', buffer));
  assert_regexp_match(result, /<audio>/);
}, 'Prompt with AudioBuffer');

promise_test(async () => {
  await ensureLanguageModel();
  const audio_data = await fetch(kValidAudioPath);
  const session =
      await LanguageModel.create({expectedInputs: [{type: 'audio'}]});

  const result = await session.prompt(
      messageWithContent(kPrompt, 'audio', await audio_data.arrayBuffer()));
  assert_regexp_match(result, /<audio>/);
}, 'Prompt with BufferSource - ArrayBuffer');
