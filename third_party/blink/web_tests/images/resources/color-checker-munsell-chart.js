function log(message) {
  document.getElementById('log').textContent += message + '\n';
}

function testImageColors(source) {
  var image = document.querySelector('img');

  image.onload = function() {
    runAfterLayoutAndPaint(drawImageToCanvas);
  };

  image.src = source;
}

function drawImageToCanvas() {
  var image = document.querySelector('img');

  var canvas = document.querySelector('canvas');
  canvas.width = image.width;
  canvas.height = image.height;

  var context2D = canvas.getContext('2d');
  if (context2D) {
    context2D.drawImage(image, 0, 0, canvas.width, canvas.height);
    chartColorTransform(canvas);
    return;
  }

  console.error('FAIL: 2d <canvas> is required for this test');
  if (window.testRunner)
    testRunner.notifyDone();
}

function getCanvasPixelDataAtPoint(canvas, x, y) {
  var context2D = canvas.getContext('2d');
  if (context2D)
    return context2D.getImageData(x, y, 1, 1).data;

  var gl = canvas.getContext('webgl');
  if (gl.getParameter(gl.UNPACK_FLIP_Y_WEBGL))
    y = canvas.height - y;
  var data = new Uint8Array(4);
  gl.readPixels(x, y, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, data);
  return data;
}

function getCanvasColor(canvas, i) {
  /*
   * Convert the Munsell color index i to a point x,y in the chart image.
   */
  var x = 40 + (i % 6) * (canvas.width / 6);
  var y = 40 + Math.floor(i / 6) * (canvas.height / 4);

  /*
   * Read the RGBA pixel from the canvas at the point x,y then return the
   * RGB values (the Munsell chart test image has no alpha channel).
   */
  try {
    var data = getCanvasPixelDataAtPoint(canvas, x, y);
    if (data[3] == 255)
      return { rgb: [data[0], data[1], data[2]] };
    console.error('FAIL: invalid canvas pixel alpha channel: ' + data[3]);
    return { rgb: [0, 0, 0] };
  } catch (error) {
    console.error('FAIL: ' + error);  // security error: tainted <canvas>
    return { rgb: [255, 255, 255] };
  }
}

function getMunsellColor(i) {
  if (!window.munsell_srgb_colors) {
    window.munsell_srgb_colors = new Array( // Munsell colors in sRGB space.
      { color: 'Dark Skin',     rgb: [ 115,  80,  64 ] },
      { color: 'Light Skin',    rgb: [ 195, 151, 130 ] },
      { color: 'Blue Sky',      rgb: [  94, 123, 156 ] },
      { color: 'Foliage',       rgb: [  88, 108,  65 ] },
      { color: 'Blue Flower',   rgb: [ 130, 129, 177 ] },
      { color: 'Bluish Green',  rgb: [ 100, 190, 171 ] },
      { color: 'Orange',        rgb: [ 217, 122,  37 ] },
      { color: 'Purplish Blue', rgb: [  72,  91, 165 ] },
      { color: 'Moderate Red',  rgb: [ 194,  84,  98 ] },
      { color: 'Purple',        rgb: [  91,  59, 107 ] },
      { color: 'Yellow Green',  rgb: [ 160, 188,  60 ] },
      { color: 'Orange Yellow', rgb: [ 230, 163,  42 ] },
      { color: 'Blue',          rgb: [  46,  60, 153 ] },
      { color: 'Green',         rgb: [  71, 150,  69 ] },
      { color: 'Red',           rgb: [ 177,  44,  56 ] },
      { color: 'Yellow',        rgb: [ 238, 200,  27 ] },
      { color: 'Magenta',       rgb: [ 187,  82, 148 ] },
      { color: 'Cyan (*)',      rgb: [ /* -49 */ 0, 135, 166 ] },
      { color: 'White',         rgb: [ 243, 242, 237 ] },
      { color: 'Neutral 8',     rgb: [ 201, 201, 201 ] },
      { color: 'Neutral 6.5',   rgb: [ 161, 161, 161 ] },
      { color: 'Neutral 5',     rgb: [ 122, 122, 121 ] },
      { color: 'Neutral 3.5',   rgb: [  83,  83,  83 ] },
      { color: 'Black',         rgb: [  50,  49,  50 ] }
    );
  }

  if (i < 0 && i >= munsell_srgb_colors.length)
    return { color: 'invalid-color', rgb: [ 0, 0, 0 ] };
  return munsell_srgb_colors[i];
}

function getColorError(cx, cy) {
  var dr = (cx[0] - cy[0]);
  var dg = (cx[1] - cy[1]);
  var db = (cx[2] - cy[2]);
  return Math.round(Math.sqrt((dr * dr) + (dg * dg) + (db * db)));
}

function pad(string, size) {
  size = size || 14;
  if (string.length < size)
    string += ' '.repeat(size - string.length);
  return string;
}

function drawRule(size) {
  log('-'.repeat(size || 44));
}

function chartColorTransform(canvas) {
  /*
   * Add header over table of color names, actual and expected values, and the
   * per color error dE (Euclidean distance).
   */
  log(pad('Color') + pad('Actual') + pad('Expected') + 'dE');
  drawRule();

  var totalSquaredError = 0.0;

  /*
   * Report per color error dE, by comparing with the expected Munsell colors,
   * and accumulate dE * dE in the totalSquaredError.
   */
  for (var i = 0; i < 24;) {
    var expected = getMunsellColor(i);
    var actual = getCanvasColor(canvas, i);
    var dE = getColorError(actual.rgb, expected.rgb);

    log(pad(expected.color) + pad(actual.rgb.join(',')) + pad(expected.rgb.join(',')) + dE);
    totalSquaredError += dE * dE;

    if (++i % 6 == 0 && i < 24)
      drawRule();
  }

  /*
   * Report the total RMS color error neglecting out-of-srgb-gamut color Cyan.
   */
  drawRule();
  log('\nResult: total RMS color error: ' + Math.sqrt(totalSquaredError / 24.0).toFixed(2));

  if (window.testRunner)
    testRunner.notifyDone();
}
