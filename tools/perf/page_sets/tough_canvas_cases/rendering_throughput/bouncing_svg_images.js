// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// This code in inspired by the bouncing svg images test on the animometer
// benchmark (https://trac.webkit.org/export/HEAD/trunk/PerformanceTests/Animometer/developer.html).
// Javascript code:
//  - https://trac.webkit.org/export/HEAD/trunk/PerformanceTests/Animometer/tests/bouncing-particles/resources/bouncing-particles.js
//  - https://trac.webkit.org/export/HEAD/trunk/PerformanceTests/Animometer/tests/bouncing-particles/resources/bouncing-canvas-particles.js
//  - https://trac.webkit.org/export/HEAD/trunk/PerformanceTests/Animometer/tests/bouncing-particles/resources/bouncing-canvas-shapes.js

function BouncingSVGImages(canvas_width, canvas_height, x_init,
                             y_init, velocity, size, image, angular_velocity) {
  this.canvas_width = canvas_width;
  this.canvas_height = canvas_height;
  this.x_position = x_init;
  this.y_position = y_init;
  this.velocity = velocity; // [speed in x, speed in y] in pixels/second
  this.color1 = get_random_color_string();
  this.color2 = get_random_color_string();
  this.size = size;
  this.last_time = Date.now();
  this.image = image;
  this.angular_position = 0;
  this.angular_velocity = angular_velocity; // in radians/second

  this.move = function() {
    var now = Date.now();
    var time_interval = (now - this.last_time) / 1000;
    this.last_time = now;

    // translate & bounce
    if (this.x_position < 0) {
      this.x_position = 0;
      this.velocity[0] = - this.velocity[0];
    }

    if (this.y_position < 0) {
      this.y_position = 0;
      this.velocity[1] = - this.velocity[1];
    }

    if (this.x_position + this.size > this.canvas_width) {
      this.x_position = this.canvas_width - this.size;
      this.velocity[0] = - this.velocity[0];
    }

    if (this.y_position + this.size > this.canvas_height) {
      this.y_position = this.canvas_height - this.size;
      this.velocity[1] = - this.velocity[1];
    }

    this.x_position += this.velocity[0] * time_interval;
    this.y_position += this.velocity[1] * time_interval;

    // rotate
    this.angular_position =
      (this.angular_position + this.angular_velocity * time_interval) % 10000;

    var rotation_center_x = this.x_position + this.size/2;
    var rotation_center_y = this.y_position + this.size/2;
    context.translate(rotation_center_x, rotation_center_y);
    context.rotate(this.angular_position);
    context.translate(-rotation_center_x, -rotation_center_y);
  }

  this.draw = function(context) {
    context.save();
    this.move();

    context.drawImage(this.image, this.x_position,
                      this.y_position, this.size, this.size);
    context.restore();
  }
}

var stage_bouncing_svg_images = (function() {
  var initialized_shapes;
  var width;
  var height;

  var stage = {};

  stage.init = function(number_shapes, canvas_width, canvas_height) {
    width = canvas_width;
    height = canvas_height;
    initialized_shapes = [];

    var image = document.getElementById("bouncing_image");

    for(var i = 0; i < number_shapes; i++) {
      var size = 80;

      var speed = 50;
      var speed_x = rand_sgn(Math.random() * speed);
      var speed_y = rand_sgn(Math.sqrt(speed*speed - speed_x*speed_x));
      var velocity = [speed_x, speed_y];

      var x_init = Math.floor(Math.random() * (canvas_width - size));
      var y_init = Math.floor(Math.random() * (canvas_height - size));

      var angular_speed = Math.PI/3;
      var angular_velocity = rand_sgn(angular_speed);

      var new_shape = new BouncingSVGImages(canvas_width, canvas_height,
                                            x_init, y_init, velocity,
                                            size, image, angular_velocity);
      initialized_shapes.push(new_shape);
    }

  };

  stage.draw = function(context) {
    for(var i = 0; i < initialized_shapes.length; i++) {
      initialized_shapes[i].draw(context);
    }
  };

  return stage;
})();


