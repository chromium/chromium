// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// This code in inspired by the canvas fill shapes test on the animometer
// benchmark (https://trac.webkit.org/export/HEAD/trunk/PerformanceTests/Animometer/developer.html).
// Javascript code: https://pr.gg/animometer/tests/simple/resources/simple-canvas-paths.js

var shape_types = ["arc_to", "arc", "rectangle", "ellipse"];

function CanvasFillShape() {
  this.init = function(canvas_width, canvas_height) {
    this.shape_type = shape_types[Math.floor(Math.random() * 4)];
    this.color = get_random_color_string();

    var max_size = Math.floor(Math.random() * 160) + 20;

    var center = new Point(Math.floor(Math.random() * canvas_width),
                               Math.floor(Math.random() * canvas_height));

    this.p1 = new Point(
      Math.floor(Math.random() * max_size) - max_size/2 + center.x,
      Math.floor(Math.random() * max_size) - max_size/2 + center.y);

    this.p2 = new Point(
      Math.floor(Math.random() * max_size) - max_size/2 + center.x,
      Math.floor(Math.random() * max_size) - max_size/2 + center.y);

    this.p3 = new Point(
      Math.floor(Math.random() * max_size) - max_size/2 + center.x,
      Math.floor(Math.random() * max_size) - max_size/2 + center.y);

    switch(this.shape_type) {
      case "arc_to":
        this.width = Math.floor(Math.random() * 40) + 5;
        this.radius = Math.floor(Math.random() * 180) + 20;
        break;

      case "arc":
        this.start_angle = Math.random() * 2 * Math.PI;
        this.end_angle = Math.random() * 2 * Math.PI;
        this.countclockwise = Math.floor(Math.random * 2);

        this.width = Math.floor(Math.random() * 40) + 5;
        this.radius = Math.floor(Math.random() * 180) + 20;
        break;

      case "rectangle":
        this.width = Math.floor(Math.random() * 180) + 20;
        this.height = Math.floor(Math.random() * 180) + 20;
        this.line_width = Math.floor(Math.random() * 15);
        break;

      case "ellipse":
        this.radius_w = Math.floor(Math.random() * 180) + 20;
        this.radius_h = Math.floor(Math.random() * 180) + 20;

        this.countclockwise = Math.floor(Math.random * 2);
        this.line_width = Math.floor(Math.random() * 15);

        this.rotation = Math.random() * 2 * Math.PI;
        this.start_angle = Math.random() * 2 * Math.PI;
        this.end_angle = Math.random() * 2 * Math.PI;
        break;
    }
  }

  this.draw_arc_to_segment = function(context) {
    context.fillStyle = this.color;
    context.beginPath();
    context.moveTo(this.p1.x, this.p1.y);
    context.arcTo(this.p2.x, this.p2.y, this.p3.x, this.p3.y, this.radius);
    context.fill();
  }

  this.draw_arc_segment = function(context) {
    context.fillStyle = this.color;
    context.beginPath();
    context.arc(this.p1.x, this.p1.y, this.radius, this.start_angle,
                this.end_angle, this.counterclockwise);
    context.fill();
  }

  this.draw_rectangle_segment = function(context) {
    context.fillStyle = this.color;
    context.beginPath();
    context.rect(this.p1.x, this.p1.y, this.width, this.height);
    context.fill();
  }

  this.draw_ellipse_segment = function(context) {
    context.fillStyle = this.color;
    context.beginPath();
    context.ellipse(this.p1.x, this.p1.y, this.radius_w, this.radius_h,
                    this.rotation, this.start_angle, this.end_angle,
                    this.counterclockwise);
    context.fill();
  }

  this.draw = function(context) {
    switch(this.shape_type) {
      case "arc_to":
        this.draw_arc_to_segment(context);
        break;
      case "arc":
        this.draw_arc_segment(context);
        break;
      case "rectangle":
        this.draw_rectangle_segment(context);
        break;
      case "ellipse":
        this.draw_ellipse_segment(context);
        break;
    }
  }
}

var stage_fill_shapes = (function() {
  var stage = {};
  var shapes;

  stage.init = function(number_shapes, canvas_width, canvas_height) {
    shapes = [];
    for (var i = 0; i < number_shapes; i++) {
      var shape = new CanvasFillShape();
      shape.init(canvas_width, canvas_height);
      shapes.push(shape);
    }
  };

  stage.draw = function(context) {
    for (var i = 0; i < shapes.length; i++) {
      shapes[i].draw(context);
    }
  };

  return stage;
})();
