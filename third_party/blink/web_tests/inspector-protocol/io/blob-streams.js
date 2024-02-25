(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests Blob retrieval via IO.read');

  function createTextBlob() {
    const digits = '0123456789';
    const blob = new Blob([Array(100000 + 1).join(digits)], {type: 'text/plain'});
    return blob;
  }

  function createBinaryBlob() {
    const digits = '\0\1\2\3\4\5\6\7\10\11';
    const blob = new Blob([Array(100 + 1).join(digits)]);
    return blob;
  }

  async function blobId(createFunction) {
    const result = await dp.Runtime.evaluate({expression: `(${createFunction}())`});
    const objectId = result.result.result.objectId;
    return (await dp.IO.resolveBlob({objectId: objectId})).result.uuid;
  }

  function dumpResponse(title, response) {
    if (response.error) {
      testRunner.log(response.error, `${title}, got error: `);
      return;
    }
    testRunner.log(`${title}: "${response.result.data}" eof: ${response.result.eof}, encoded: ${response.result.base64Encoded}`);
  }

  const uuid = await blobId(createTextBlob);
  const handle = `blob:${uuid}`;

  response = await session.protocol.IO.read({handle: handle, offset: 0, size: 27});
  dumpResponse('First 27 bytes', response);

  response = await session.protocol.IO.read({handle: handle, size: 5});
  dumpResponse('Next 5 bytes', response);

  response = await session.protocol.IO.read({handle: handle});
  const data = response.result.data;
  testRunner.log(`Next chunk: ${data.substr(0, 5)}..${data.substr(-5)} (${data.length})`);

  response = await session.protocol.IO.read({handle: handle, offset: 999996, size: 10});
  dumpResponse('Seeking to 999996', response);

  response = await session.protocol.IO.read({handle: handle, offset: 0, size: 10});
  dumpResponse('Seeking to 0', response);

  response = await session.protocol.IO.read({handle: handle, offset: 0, size: -1});
  dumpResponse('Reading negative size', response);

  // Try multiple queued request
  var promises = [];
  for (var i = 0; i < 10; ++i)
    promises.push(session.protocol.IO.read({handle: handle, size: 10}));
  const responses = await Promise.all(promises);
  const allData = responses.map(r => r.result.data).join('');
  testRunner.log('From concurrent requests: ' + allData);

  // Now rewind and do the whole stream.
  var request = {handle: handle, offset: 0, size: 20000};
  var offset = 0;
  do {
    response = await session.protocol.IO.read(request);
    const data = response.result.data;
    for (var i = 0; i < data.length; ++i) {
      var expected = String.fromCharCode(('0').charCodeAt(0) + (offset + i) % 10);
      if (data[i] !== expected) {
        testRunner.log(`ERROR at offset ${offset + i}, expected ${expected}, got ${data[i]}`);
        break;
      }
    }
    offset += data.length;
    delete request.offset;
  } while (!response.result.eof);
  testRunner.log(`Total read: ${offset}`);

  response = await session.protocol.IO.close({handle: handle});
  testRunner.log(`Error from close: ${response.error}`);

  const binaryUUID = await blobId(createBinaryBlob);
  const binaryHandle = `blob:${binaryUUID}`;

  response = await session.protocol.IO.read({handle: binaryHandle, offset: 0, size: 16});
  dumpResponse('First 16 bytes of binary blob', response);
  const binarySequence = atob(response.result.data);
  const numbers = [].map.call(binarySequence, _ => _.charCodeAt(0));
  testRunner.log('Decoded: ' + numbers.join(','));

  testRunner.completeTest();
})
