importScripts('../../serviceworker/resources/worker-testharness.js');

function createPushMessageData(data)
{
    // The PushMessageData object does not expose a constructor, but we can get an object
    // initialized with our data by constructing a PushEvent.
    return new PushEvent('PushEvent', { data: data }).data;
}

test(function() {
    const textContents = 'Hello, world!';

    const runTest = pushEvent => {
        assert_not_equals(pushEvent.data, null);
        assert_equals(pushEvent.data.text(), textContents);
    };

    // JavaScript strings are UTF-16, whereas binary data for push message data will be
    // encoded as UTF-8. Convert accordingly.
    const bufferView = new TextEncoder('utf-8').encode(textContents);

    runTest(new PushEvent('PushEvent', { data: bufferView }));
    runTest(new PushEvent('PushEvent', { data: bufferView.buffer }));
    runTest(new PushEvent('PushEvent', { data: textContents }));

}, 'PushEvent can be initialized from ArrayBuffer, ArrayBufferView and USVStrings.');

test(function() {
    assert_equals(createPushMessageData(undefined), null);

}, 'PushMessageData is null by default.');

test(function() {
    const binaryContents = [1, 2, 3];
    const textContents = 'FOOBAR';

    const pushMessageDataBinary = createPushMessageData(new Uint8Array(binaryContents));
    const pushMessageDataString = createPushMessageData(textContents);

    const binaryBuffer = pushMessageDataBinary.arrayBuffer();
    const binaryBufferView = new Uint8Array(binaryBuffer);

    assert_equals(binaryBuffer.byteLength, binaryContents.length);
    assert_array_equals(binaryBufferView, binaryContents);

    const stringBuffer = pushMessageDataString.arrayBuffer();
    const stringBufferView = new Int8Array(stringBuffer);

    assert_equals(stringBufferView.length, textContents.length);
    assert_equals(stringBufferView[0], 70 /* F */);
    assert_equals(stringBufferView[3], 66 /* B */);

}, 'PushMessageData handling of ArrayBuffer content.');

async_test(function(test) {
    const textContents = 'Hello, world!';

    const pushMessageData = createPushMessageData(textContents);

    const blob = pushMessageData.blob();
    const reader = new FileReader();

    assert_equals(blob.size, textContents.length);
    assert_equals(blob.type, '');

    reader.addEventListener('load', () => {
        assert_equals(reader.result, textContents);
        test.done();
    });

    reader.readAsText(blob);

}, 'PushMessageData handling of Blob content.')

test(function () {
    const binaryContents = [1, 2, 3];
    const textContents = 'FOOBAR';

    const pushMessageDataBinary = createPushMessageData(new Uint8Array(binaryContents));
    const pushMessageDataString = createPushMessageData(textContents);

    const binaryBuffer = pushMessageDataBinary.bytes();

    assert_equals(binaryBuffer.byteLength, binaryContents.length);
    assert_equals(binaryBuffer.length, binaryContents.length);
    assert_array_equals(binaryBuffer, binaryContents);

    const stringBuffer = pushMessageDataString.bytes();

    assert_equals(stringBuffer.byteLength, textContents.length);
    assert_equals(stringBuffer.length, textContents.length);
    assert_equals(stringBuffer[0], 70 /* F */);
    assert_equals(stringBuffer[3], 66 /* B */);

}, 'PushMessageData handling of Uint8Array (bytes()) content.');

test(function() {
    const pushMessageDataArray = createPushMessageData('[5, 6, 7]');
    const pushMessageDataObject = createPushMessageData('{ "foo": { "bar": 42 } }');
    const pushMessageDataString = createPushMessageData('"foobar"');

    const array = pushMessageDataArray.json();
    assert_equals(array.length, 3);
    assert_equals(array[1], 6);

    const object = pushMessageDataObject.json();
    assert_equals(object.foo.bar, 42);

    const string = pushMessageDataString.json();
    assert_equals(string, 'foobar');

}, 'PushMessageData handling of valid JSON content.');

test(function() {
    // Note that we do not care about the exception code - it's pass through.
    assert_throws_js(SyntaxError, () => createPushMessageData('\\o/').json());

}, 'PushMessageData handling of invalid JSON content.');

test(function() {
    assert_throws_js(TypeError, () => new PushMessageData());
    assert_throws_js(TypeError, () => new PushMessageData('Hello, world!'));
    assert_throws_js(TypeError, () => new PushMessageData(new ArrayBuffer(8)));

}, 'PushMessageData should not be constructable.');

test(function() {
    const s = "e\u0328"; // 'e' + COMBINING OGONEK
    const data = createPushMessageData(s);
    assert_equals(data.text(), s, 'String should not be NFC-normalized.');

}, 'PushEventInit data is not normalized');

if (self.SharedArrayBuffer) {
    test(function() {
        assert_throws_js(TypeError, () => {
            createPushMessageData(new Uint8Array(new SharedArrayBuffer(16)));
        });
    }, 'PushMessageData throws when passed SharedArrayBuffer view.');
}
