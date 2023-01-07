// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// This code in inspired by the canvas arcs test on the animometer
// benchmark (https://pr.gg/animometer/developer.html).
// Javascript code: https://pr.gg/animometer/tests/master/resources/canvas-tests.js

function CanvasArc() {
  var x;
  var y;
  var radius;
  var width;
  var start_angle;
  var end_angle;
  var arc_speed;
  var counterclockwise;
  var color;
  var is_stroke;

  this.init = function(arc_speed, max_width, max_height) {
    // initialize parameters randomly


    num_positions_x = 7;
    position_incr_x = Math.floor(max_width / num_positions_x);
    this.x = Math.floor(Math.random() * num_positions_x) * position_incr_x;

    num_positions_y = 7;
    position_incr_y = Math.floor(max_height / num_positions_y);
    this.y = Math.floor(Math.random() * num_positions_y) * position_incr_y;

    var max_radius = max_height / 10;
    // radius is in [0.5*max_radius, max_radius)
    this.radius = ((Math.random() + 1) / 2) * max_radius;

    this.width = 1 + Math.pow(Math.random(), 4) * 30;

    var colors = ["#101010", "#808080", "#c0c0c0",
                  "#e01040", "#10c030", "#e05010"];
    this.color = colors[Math.floor(Math.random() * colors.length)];

    this.is_stroke = (Math.floor(Math.random() * 3) == 0);

    this.counterclockwise = (Math.floor(Math.random() * 2) == 0);

    this.arc_speed = (Math.random() - 0.5) * Math.PI / 10;

    this.start_angle = Math.random() * 2 * Math.PI;
    this.end_angle = Math.random() * 2 * Math.PI;
  }

  this.draw = function(context) {
    this.start_angle += this.arc_speed;
    this.end_angle += this.arc_speed / 2;

    // draw the canvas arc on the given context
    if (this.is_stroke) {
      context.strokeStyle = this.color;
      context.lineWidth = this.width;
      context.beginPath();
      context.arc(this.x, this.y, this.radius,
                  this.start_angle, this.end_angle, this.counterclockwise);
      context.stroke();

    } else {
      context.fillStyle = this.color;
      context.beginPath();
      context.lineTo(this.x, this,y);
      context.arc(this.x, this.y, this.radius, this.start_angle,
                  this.end_angle, this.couterclockwise);
      context.lineTo(this.x, this.y);
      context.fill();

    }
  }
}

var arcs = (function() {
  var initialized_arcs;
  var width;
  var height;

  var arcs = {};

  arcs.init = function(number_arcs, arc_speed, canvas_width, canvas_height) {
    // initialize arcs and store variables
    initialized_arcs = [];
    for(var i = 0; i < number_arcs; i++) {
      var new_arc = new CanvasArc();
      new_arc.init(arc_speed, canvas_width, canvas_height);
      initialized_arcs.push(new_arc)
    }

  };

  arcs.draw = function(context) {
    // draw all initialized_arcs on the context
    for(var i = 0; i < initialized_arcs.length; i++) {
      initialized_arcs[i].draw(context);
    }
  };

  return arcs;
})();


