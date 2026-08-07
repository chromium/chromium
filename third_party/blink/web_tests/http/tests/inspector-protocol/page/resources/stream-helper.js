(() => {
  async function readStream(dp, stream) {
    let decoded = '';
    for (;;) {
      const readResponse = await dp.IO.read({handle: stream, size: 10000});
      if (readResponse.error) {
        throw new Error('Error: ' + readResponse.error.message);
      }
      const response = readResponse.result;
      if (response.base64Encoded) {
        decoded += atob(response.data);
      }
      if (response.eof)
        break;
    }
    return decoded;
  }

  function assertMp4Stream(testRunner, streamData) {
    if (streamData) {
      testRunner.log(streamData.includes('ftyp') ? 'stream data is mp4' :
                                                   'stream data is not mp4');
    } else {
      testRunner.log('stream data is empty');
    }
  }

  return {readStream, assertMp4Stream};
})()
