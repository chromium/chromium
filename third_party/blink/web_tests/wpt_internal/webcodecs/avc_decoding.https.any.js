// META: global=window,dedicatedworker

promise_test(async t => {
  const chunks = [{offset: 48, size: 4140}, {offset: 4188, size: 604}];
  const data = await (await fetch('h264.mp4')).arrayBuffer();

  // avcC for a similar but larger (640x480) video.
  const description = new Uint8Array([
    1, 100, 0, 22, 255, 225, 0, 25, 103, 100, 0, 22, 172, 178, 1, 64, 123, 96,
    34, 0, 0, 3, 0, 2, 0, 0, 3, 0, 41, 30, 44, 92, 144, 1, 0, 6, 104, 235,
    195, 203, 34, 192, 253, 248, 248, 0
  ]);

  // Correct parameter set for the stream.
  const parameters = new Uint8Array([
    0, 0, 0, 24, 103, 100, 0, 11, 172, 178, 2, 131, 246, 2, 32, 0, 0, 3, 0, 32,
    0, 0, 3, 2, 145, 226, 133, 73,
    0, 0, 0, 6, 104, 235, 195, 203, 34, 192
  ]);

  let outputs = 0;
  let decoder = new VideoDecoder({
    error: () => t.unreached_func("Unexpected error"),
    output: (frame) => { outputs++; frame.close(); }
  });

  const config = {
    codec: 'avc1.64000b',
    codedWidth: 320,
    codedHeight: 240,
    description: description
  };
  decoder.configure(config);

  for (let i = 0; i < chunks.length; i++) {
    let chunk_data = new Uint8Array(data, chunks[i].offset, chunks[i].size);

    if (i == 0) {
      // Insert correct parameter set in-band.
      let modified_chunk_data =
        new Uint8Array(parameters.byteLength + chunk_data.byteLength);
      modified_chunk_data.set(parameters, 0);
      modified_chunk_data.set(chunk_data, parameters.byteLength);
      chunk_data = modified_chunk_data;
    }

    decoder.decode(new EncodedVideoChunk({
      type: (i == 0) ? 'key' : 'delta',
      timestamp: i * 100_000,
      data: chunk_data
    }));
  }

  await decoder.flush();
  assert_equals(outputs, chunks.length);
}, 'Test decoding with mismatched avcC');
