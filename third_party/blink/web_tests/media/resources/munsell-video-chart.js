window.ignoreCyan = false;

function testImageColors(source, cyanInTargetGamut) {
  window.ignoreCyan = !cyanInTargetGamut;

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

  canvas.getContext('2d', {willReadFrequently: true}).drawImage(image, 0, 0, canvas.width, canvas.height);
  chartColorTransform(canvas);
}

function log(message) {
  document.getElementById('log').textContent += message + '\n';
}

function getCanvasColor(canvas, i) {
  var x = 40 + (i % 6) * (canvas.width / 6);
  var y = 40 + Math.floor(i / 6) * (canvas.height / 4 - 40);
  try {
    var data = canvas.getContext('2d', {willReadFrequently: true}).getImageData(x, y, 1, 1).data;
    if (data[3] == 255)
      return { rgb: [data[0], data[1], data[2]] };
    return { rgb: [0, 0, 0] };
  } catch (error) {
    console.error(error);
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

function getStudioColor(i) {
  if (!window.studio_srgb_colors) {
    // Studio video range white & black and proportions thereof, in sRGB space.
    window.studio_srgb_colors = new Array(
      { color: 'SG White',      rgb: [ 237, 237, 237 ] },
      { color: 'SG White 1/2',  rgb: [ 130, 130, 130 ] },
      { color: 'SG White 1/4',  rgb: [  74,  74,  74 ] },
      { color: 'SG Black 4x',   rgb: [  79,  79,  79 ] },
      { color: 'SG Black 2x',   rgb: [  48,  48,  48 ] },
      { color: 'SG Black',      rgb: [  31,  31,  31 ] }
    );
  }

  if (i < 0 && i >= studio_srgb_colors.length)
    return { color: 'invalid-color', rgb: [ 0, 0, 0 ] };
  return studio_srgb_colors[i];
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
   * per color error (Euclidean distance).
   */
  log(pad('Color') + pad('Actual') + pad('Expected') + 'dE');
  drawRule();

  var totalSquaredError = 0.0;

  /*
   * Report per color error dE, by comparing with the expected Munsell & Studio colors.
   */
  for (var i = 0; i < 30;) {
    var expected = (i < 24) ? getMunsellColor(i) : getStudioColor(i - 24);
    var actual = getCanvasColor(canvas, i);
    var dE = getColorError(actual.rgb, expected.rgb);

    log(pad(expected.color) + pad(actual.rgb.join(',')) + pad(expected.rgb.join(',')) + dE);

    if (ignoreCyan && (i + 1) == 18)
      ; // Do not include the Munsell Cyan (out-of-srgb-gamut) color error.
    else
      totalSquaredError += dE * dE;

    if (++i % 6 == 0 && i <= 24)
      drawRule();
  }

  /*
   * Report the total RMS color error: lower is better, and should be << 2, which is the
   * JND (Just Noticable Difference) perception threshold.  Above a JND, the color error
   * is noticable to the human eye.
   */
  drawRule();
  log('\nResult: total RMS color error: ' + Math.sqrt(totalSquaredError / 30.0).toFixed(2));

  if (window.testRunner)
    testRunner.notifyDone();
}
