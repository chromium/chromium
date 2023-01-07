// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// This code in inspired by the canvas clipped rectangles test on the animometer
// benchmark (https://trac.webkit.org/export/HEAD/trunk/PerformanceTests/Animometer/developer.html).
// Javascript code:
//  - https://trac.webkit.org/export/HEAD/trunk/PerformanceTests/Animometer/tests/bouncing-particles/resources/bouncing-particles.js
//  - https://trac.webkit.org/export/HEAD/trunk/PerformanceTests/Animometer/tests/bouncing-particles/resources/bouncing-canvas-particles.js
//  - https://trac.webkit.org/export/HEAD/trunk/PerformanceTests/Animometer/tests/bouncing-particles/resources/bouncing-canvas-shapes.js

function BouncingClippedRect(canvas_width, canvas_height, x_init,
                             y_init, velocity, angular_velocity, size) {
  this.canvas_width = canvas_width;
  this.canvas_height = canvas_height;
  this.x_position = x_init;
  this.y_position = y_init;
  this.velocity = velocity; // [speed in x, speed in y] in pixels/second
  this.angular_position = 0;
  this.angular_velocity = angular_velocity; // in radians/second
  this.color = get_random_color_string();
  this.clipping_path = [
      new Point(0.50, 0.00),
      new Point(0.38, 0.38),
      new Point(0.00, 0.38),
      new Point(0.30, 0.60),
      new Point(0.18, 1.00),
      new Point(0.50, 0.75),
      new Point(0.82, 1.00),
      new Point(0.70, 0.60),
      new Point(1.00, 0.38),
      new Point(0.62, 0.38)];
  this.size = size;
  this.last_time = Date.now();

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
      (this.angular_position + this.angular_velocity*time_interval) % 10000;

    var rotation_center_x = this.x_position + this.size/2;
    var rotation_center_y = this.y_position + this.size/2;
    context.translate(rotation_center_x, rotation_center_y);
    context.rotate(this.angular_position);
    context.translate(-rotation_center_x, -rotation_center_y);
  }

  this.draw = function(context) {
    context.save();

    this.move();

    // apply clipping
    context.beginPath();
    for (var i = 0; i < this.clipping_path.length; i++) {
      var point = this.clipping_path[i];
      var x = this.x_position + point.x * this.size;
      var y = this.y_position + point.y * this.size;
      if (i == 0) {
        context.moveTo(x, y);
      } else {
        context.lineTo(x, y);
      }
    }
    context.closePath()
    context.clip();

    // draw rectangle
    context.fillStyle = this.color;
    context.beginPath();
    context.rect(0, 0, this.canvas_width, this.canvas_height);
    context.fill();

    context.restore();
  }
}

var stage_bouncing_clipped_rectangles = (function() {
  var initialized_shapes;
  var width;
  var height;

  var stage = {};

  stage.init = function(number_shapes, canvas_width, canvas_height) {
    width = canvas_width;
    height = canvas_height;
    initialized_shapes = [];

    for(var i = 0; i < number_shapes; i++) {
      var star_size = 80;

      var speed = 50;
      var speed_x = rand_sgn(Math.random() * speed);
      var speed_y = rand_sgn(Math.sqrt(speed*speed - speed_x*speed_x));
      var velocity = [speed_x, speed_y];

      var angular_speed = Math.PI/3;
      var angular_velocity = rand_sgn(angular_speed);

      var x_init = Math.floor(Math.random() * (canvas_width - star_size));
      var y_init = Math.floor(Math.random() * (canvas_height - star_size));

      var new_shape = new BouncingClippedRect(canvas_width, canvas_height,
                                              x_init, y_init, velocity,
                                              angular_velocity, star_size);
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


