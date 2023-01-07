// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// This code in inspired by the canvas lines test on the animometer
// benchmark (https://trac.webkit.org/export/HEAD/trunk/PerformanceTests/Animometer/developer.html).
// Javascript code: https://pr.gg/animometer/tests/master/resources/canvas-tests.js

function CanvasLine() {

  this.init = function(circle_x, circle_y, circle_radius, min_length,
                       max_length, theta, omega, color, width) {
    this.theta = theta; // orientation angle
    this.omega = omega; // speed; oscillations/second

    this.color = color;

    this.min_length = min_length;
    this.max_length = max_length;

    this.x = circle_x + circle_radius * Math.cos(theta);
    this.y = circle_y + circle_radius * Math.sin(theta);
    this.direction = Math.floor(Math.random() * 2) * 2 - 1; // 1 or -1

    this.width = width;
    this.init_time = Date.now();
  }

  this.draw = function(context) {
    var seconds = (Date.now() - this.init_time) / 1000;
    var length = this.min_length +
                (this.max_length - this.min_length) *
                Math.abs(Math.sin(seconds * this.omega * 2 * Math.PI));

    context.strokeStyle = this.color;
    context.lineWidth = this.width;

    context.beginPath();
    context.moveTo(this.x, this.y);
    context.lineTo(this.x + this.direction * length * Math.cos(this.theta),
                   this.y + this.direction * length * Math.sin(this.theta));
    context.stroke();
  }
}

var stage_lines = (function() {
  var initialized_lines;
  var width;
  var height;

  var circles_x;
  var circles_y;
  var circle_radius;

  var stage = {};

  stage.init = function(number_lines, canvas_width, canvas_height) {
    width = canvas_width;
    height = canvas_height;
    initialized_lines = [];

    circle_radius = Math.min(canvas_width / 8,
                             canvas_height / 8);
    circles_x = [canvas_width * 0.2, canvas_width * 0.4,
                     canvas_width * 0.6, canvas_width * 0.8];
    circles_y = [canvas_height * 0.3, canvas_height * 0.6,
                     canvas_height * 0.3, canvas_height * 0.6];

    circle_color = ["#e01040", "#10c030", "#744CBA", "#e05010"];

    for(var i = 0; i < number_lines; i++) {
      var new_line = new CanvasLine();

      var circle_number = Math.floor(Math.random() * circles_x.length);
      var theta = Math.random() * 2 * Math.PI;
      var omega = Math.random() / 2 + 0.2;
      var width = Math.random() * 8 + 1
      var max_length = Math.random() * canvas_width / 15;
      var min_length =
        Math.min(Math.random() * canvas_width / 30, max_length /3)

      new_line.init(circles_x[circle_number], circles_y[circle_number],
                    circle_radius, min_length, max_length, theta, omega,
                    circle_color[circle_number], width);
      initialized_lines.push(new_line)
    }

  };

  stage.draw = function(context) {
    // draw the circles
    for (var i = 0; i < circles_x.length; i++) {
      context.strokeStyle = circle_color[i];
      context.beginPath();
      context.arc(circles_x[i], circles_y[i], circle_radius, 0, 2 * Math.PI);
      context.stroke();
      context.fillStyle = "#000000";
      context.fill();
    }

    // draw all initialized_lines on the context
    for(var i = 0; i < initialized_lines.length; i++) {
      initialized_lines[i].draw(context);
    }
  };

  return stage;
})();


