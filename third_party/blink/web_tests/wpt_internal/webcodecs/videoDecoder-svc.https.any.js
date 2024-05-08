// META: global=window,dedicatedworker

promise_test(async t => {
  let data = await (await fetch('svc.vp9')).arrayBuffer();

  let outputs = 0;
  let output_width = 0;

  let decoder = new VideoDecoder({
    error: () => t.unreached_func('Unexpected error'),
    output: (frame) => {
      outputs += 1;
      output_width = frame.visibleRect.width;
      frame.close();
    },
  });
  decoder.configure({
    codec: 'vp09.00.20.08',
    codedWidth: 640,
    codedHeight: 480,
    hardwareAcceleration: 'prefer-software'
  });

  decoder.decode(new EncodedVideoChunk({
    type: 'key',
    timestamp: 0,
    data
  }));
  await decoder.flush();
  decoder.close();

  // Only one of the two streams should be output.
  assert_equals(outputs, 1, 'output frames');
  // It should be the stream with higher spatial resolution.
  assert_equals(output_width, 640, 'frame width');
}, 'Decode SVC VP9');

promise_test(async t => {
  let data = await (await fetch('svc.av1')).arrayBuffer();

  let outputs = 0;
  let output_width = 0;

  let decoder = new VideoDecoder({
    error: () => t.unreached_func('Unexpected error'),
    output: (frame) => {
      outputs += 1;
      output_width = frame.visibleRect.width;
      frame.close();
    },
  });
  decoder.configure({
    codec: 'av01.0.04M.08',
    codedWidth: 640,
    codedHeight: 480,
    hardwareAcceleration: 'prefer-software'
  });

  decoder.decode(new EncodedVideoChunk({
    type: 'key',
    timestamp: 0,
    data
  }));
  await decoder.flush();
  decoder.close();

  // Only one of the two streams should be output.
  assert_equals(outputs, 1, 'output frames');
  // It should be the stream with higher spatial resolution.
  assert_equals(output_width, 640, 'frame width');
}, 'Decode SVC AV1');
