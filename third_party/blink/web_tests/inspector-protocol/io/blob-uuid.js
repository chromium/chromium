(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests resolving blob to UUID through IO domain.');

  async function createBlob(content) {
    const result = await dp.Runtime.evaluate({expression: `new Blob(["${content}"])`});
    return result.result.result.objectId;
  }

  testRunner.log('Blobs:');
  const blobA = await createBlob('a');
  const blobB = await createBlob('b');
  const uuid_a = (await dp.IO.resolveBlob({'objectId': blobA})).result.uuid;
  const uuid_b = (await dp.IO.resolveBlob({'objectId': blobB})).result.uuid;

  testRunner.log('uuid_a: ' + uuid_a.replace(/[a-z0-9]/g, 'x'));
  testRunner.log('uuid_b: ' + uuid_b.replace(/[a-z0-9]/g, 'x'));
  testRunner.log('uuid_a != uuid_b: ' + (uuid_a != uuid_b));

  testRunner.log('Not a blob:');
  const objectId = (await dp.Runtime.evaluate({expression: '({})'})).result.result.objectId;
  var error = (await dp.IO.resolveBlob({'objectId': objectId})).error;
  testRunner.log('error:' + JSON.stringify(error));

  testRunner.log('Bad id:');
  error = (await dp.IO.resolveBlob({'objectId': 'boo'})).error;
  testRunner.log('error:' + JSON.stringify(error));

  testRunner.completeTest();
})
