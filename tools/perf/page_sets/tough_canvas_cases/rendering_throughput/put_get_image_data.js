// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// This code in inspired by the put/get image data test on the animometer
// benchmark (https://trac.webkit.org/export/HEAD/trunk/PerformanceTests/Animometer/developer.html).
// Javascript code: https://trac.webkit.org/export/HEAD/trunk/PerformanceTests/Animometer/tests/simple/resources/tiled-canvas-image.js

function ImageTile(x, y, width, height) {
  this.source_x = x;
  this.source_y = y;
  this.width = width;
  this.height = height;

  this.draw = function(context, x, y) {
    var image_data = context.getImageData(this.source_x, this.source_y,
                                          this.width, this.height);
    context.putImageData(image_data, x, y);
  }
}

var stage_put_get_image_data = (function() {
  var initialized_shapes;
  var tile_output_locations;
  var width;
  var height;

  var stage = {};

  stage.init = function(canvas_width, canvas_height) {
    width = canvas_width;
    height = canvas_height;

    tile_output_locations = [];
    initialized_shapes = [];

    var tiles_per_row = 40;
    var tiles_per_column = 50;

    var tile_width = Math.floor(canvas_width / tiles_per_row);
    var tile_height = Math.floor(canvas_height / tiles_per_column);

    for(var i = 0; i < tiles_per_row * tiles_per_column; i++) {
      var x = (i % tiles_per_row) * tile_width;
      var y = Math.floor(i / tiles_per_row) * tile_height;
      tile_output_locations.push(new Point(x, y));
      var new_shape = new ImageTile(x, y, tile_width, tile_height);
      initialized_shapes.push(new_shape);
    }
  };

  stage.draw = function(context) {
    // draw backgound
    var gradient = context.createLinearGradient(0, 0, width, 0);
    gradient.addColorStop(0, "blue");
    gradient.addColorStop(1, "white");
    context.fillStyle = gradient;
    context.fillRect(0, 0, width, height);

    // draw tiles
    shuffle(tile_output_locations);
    for(var i = 0; i < initialized_shapes.length; i++) {
      var p = tile_output_locations[i];
      initialized_shapes[i].draw(context, p.x, p.y);
    }
  };

  return stage;
})();

