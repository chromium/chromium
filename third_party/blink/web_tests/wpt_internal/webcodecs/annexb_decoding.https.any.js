// META: global=window,dedicatedworker
// META: script=/wpt_internal/webcodecs/encoder_utils.js

promise_test(async t => {
  let outputs = 0;
  const chunks = [
    {
      offset: 0,
      size: 4045
    },
    {
      offset: 4045,
      size: 926
    },
    {
      offset: 4971,
      size: 241
    },
    {
      offset: 5212,
      size: 97
    },
    {
      offset: 5309,
      size: 98
    },
    {
      offset: 5407,
      size: 624
    },
    {
      offset: 6031,
      size: 185
    },
    {
      offset: 6216,
      size: 94
    },
    {
      offset: 6310,
      size: 109
    },
    {
      offset: 6419,
      size: 281
    }
  ];

  let all_chunks_buffer = await (await fetch('annexb.h264')).arrayBuffer();
  let decoder = new VideoDecoder({
    error: () => t.unreached_func("Unexpected error"),
    output: (frame) => { outputs++; },
  });

  const config = {
    codec: "avc1.42001E",
    codedWidth: 320,
    codedHeight: 240
    /* no description means Annex B */
  };
  decoder.configure(config);
  let counter = 0;
  for (let chunk of chunks) {
    let buffer = new Uint8Array(all_chunks_buffer, chunk.offset, chunk.size);
    let encoded_chunk = new EncodedVideoChunk({
      type: (counter == 0) ? 'key' : 'delta',
      timestamp: counter * 1_00_000,
      data: buffer
    });
    counter++;
    decoder.decode(encoded_chunk);
  }

  await decoder.flush();
  decoder.close();
  assert_equals(outputs, chunks.length);
}, "Decode AnnexB");
