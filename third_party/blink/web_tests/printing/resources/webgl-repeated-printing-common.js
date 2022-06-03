var gl;

function main()
{
  if (!window.testRunner) {
    testFailed("Requires window.testRunner");
  } else {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
    window.requestAnimationFrame(initTest);
  }
}

var testIndex = 0;
var testsAndExpectations = [
  { 'description': 'green', 'clearColor': [0, 1, 0, 1], 'expected': [  0, 255,   0] },
  { 'description': 'red',   'clearColor': [1, 0, 0, 1], 'expected': [255,   0,   0] },
  { 'description': 'blue',  'clearColor': [0, 0, 1, 1], 'expected': [  0,   0, 255] },
];
var tolerance = 1;

function initTest() {
  var canvas = document.getElementById("c");
  gl = initGL(canvas);
  if (!gl) {
    testFailed("Test requires WebGL");
    testRunner.notifyDone();
    return;
  }

   window.requestAnimationFrame(nextTest);
}

function nextTest() {
  if (testIndex >= testsAndExpectations.length) {
    // Without clearing this bit, the output comes out as a render
    // tree, which is difficult to read.
    testRunner.notifyDone();
    return;
  }

  var test = testsAndExpectations[testIndex];
  var color = test['clearColor'];
  try {
    draw(color[0], color[1], color[2], color[3]);
    testRunner.updateAllLifecyclePhasesAndCompositeThen(
      ()=>testRunner.capturePrintingPixelsThen(completionCallback));
  } catch (e) {
    debug('error in nextTest');
    debug(e);
    testRunner.notifyDone();
  }
}

var pixel;
function fetchPixelAt(x, y, width, height, snapshot) {
  var data = new Uint8Array(snapshot);
  pixel = [
    data[4 * (width * y + x) + 0],
    data[4 * (width * y + x) + 1],
    data[4 * (width * y + x) + 2],
    data[4 * (width * y + x) + 3]
  ];
}

function completionCallback(width, height, snapshot) {
  var test = testsAndExpectations[testIndex];
  debug('Snapshot width: ' + width + ' height: ' + height);
  debug('Test ' + testIndex + ': canvas should be ' + test['description']);
  try {
    var expectation = test['expected'];
    fetchPixelAt(50, 50, width, height, snapshot);
    shouldBeCloseTo('pixel[0]', expectation[0], tolerance);
    shouldBeCloseTo('pixel[1]', expectation[1], tolerance);
    shouldBeCloseTo('pixel[2]', expectation[2], tolerance);
  } catch (e) {
    debug('error in completionCallback');
    debug(e);
    testRunner.notifyDone();
    return;
  }

  ++testIndex;
  window.requestAnimationFrame(nextTest);
}

function draw(r, g, b, a)
{
  gl.clearColor(r, g, b, a);
  gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
}

main();
