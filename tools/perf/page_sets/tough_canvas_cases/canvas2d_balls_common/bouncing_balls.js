// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// demo parameters
// maybe overridden in test file
var numBalls = parseInt(getArgValue('ball_count'));
if (numBalls == 0 || isNaN(numBalls)) {
    numBalls = 500;
}
var ballDiameter = 30;
var gravity = 0.5; //screen heights per second^2
var maxInitVelocity = 0.2;
var maxAngularVelocity = 5; // rad per second
var elasticity = 0.7;
var joltInterval = 1.5;

// globals
var balls = [];
var canvasWidth;
var canvasHeight;
var borderX;
var borderY;
var timeOfLastJolt = 0;

function include(filename) {
  var head = document.getElementsByTagName('head')[0];
  var script = document.createElement('script');
  script.src = filename;
  script.type = 'text/javascript';
  head.appendChild(script)
}

include("bouncing_balls_draw_ball_as_" + getArgValue('ball') + ".js");
include("bouncing_balls_draw_back_as_" + getArgValue('back') + ".js");

window.requestAnimFrame = (function(){
  return window.requestAnimationFrame    ||
      window.webkitRequestAnimationFrame ||
      window.mozRequestAnimationFrame    ||
      window.oRequestAnimationFrame      ||
      window.msRequestAnimationFrame     ||
      function( callback ){
        window.setTimeout(callback, 1000 / 60);
      };
})();

window.onload = init;

function init(){
  handleResize();
  for (var i = 0; i < numBalls; i++) {
    balls.push(new Ball());
  }
  window.addEventListener("resize", handleResize, false);
  drawBallInit(ballDiameter); // externally defined
  window.requestAnimFrame(updateFrame);
}

function handleResize() {
  canvasWidth = window.innerWidth;
  canvasHeight = window.innerHeight;
  canvas.setAttribute('width', canvasWidth);
  canvas.setAttribute('height', canvasHeight);
  borderX = ballDiameter/canvasWidth;
  borderY = ballDiameter/canvasHeight;
  prepareBackground(); // externally defined
}

function updateFrame() {
  var now = new Date().getTime() / 1000;
  var jolt = false;
  if (now - timeOfLastJolt > joltInterval) {
    jolt = true;
    timeOfLastJolt = now;
  }
  drawBackground(); // externally defined
  for (var i = 0; i < numBalls; i++) {
    balls[i].step(jolt);
  }
  window.requestAnimFrame(updateFrame);
}

function Ball(){
  var x = borderX + Math.random()*(1-2*borderX);
  var y = borderY + Math.random()*(1-2*borderY);
  var angle = Math.PI * 2 * Math.random();
  var velocityY = Math.random()*maxInitVelocity*2 - maxInitVelocity;
  var velocityX = Math.random()*maxInitVelocity*2 - maxInitVelocity;
  var angularVelocity = Math.random()*maxAngularVelocity*2 -
      maxAngularVelocity;
  var previousFrameTime = new Date().getTime();
  var previousBounceTime = 0;
  var alive = true;
  function step(jolt) {
    var curTime = new Date().getTime();
    var timeStep = (curTime - previousFrameTime) / 1000;
    previousFrameTime = curTime;

    // handle balls that are no longer bouncing
    if (!alive) {
      if (jolt) {
        // If a jolt is applied, bump the rollong balls enough for them to
        // reach between 0.75x and 1x the height of the window
        velocityY = -Math.sqrt(2 * gravity * (1-2 * borderY) * (0.75 + 0.25 *
            Math.random()))
        velocityX = Math.random()*maxInitVelocity*2 - maxInitVelocity;
        angularVelocity = Math.random()*maxAngularVelocity*2 -
            maxAngularVelocity;
        alive = true;
      } else {
        // rolling on the ground
        angularVelocity = 2*velocityX*canvasWidth/ballDiameter;
      }
    }

    // Compute angular motion
    angle += timeStep*angularVelocity;
    // Compute horizontal motion
    var remainingTime = timeStep;
    var deltaX = velocityX*remainingTime;
    while ((x+deltaX) < borderX || (x+deltaX) > (1-borderX)){
      if ((x+deltaX) < borderX) {
        // left side bounce
        remainingTime -= (borderX - x)/velocityX;
        x = borderX;
      } else {
        // right side bounce
        remainingTime -= ((1-borderX) - x)/velocityX;
        x = 1 - borderX;
      }
      velocityX = -elasticity*velocityX;
      deltaX = velocityX*remainingTime;
    }
    x += deltaX;

    // Compute vertical motion
    remainingTime = timeStep;
    var deltaY = alive ? velocityY*timeStep+gravity*timeStep*timeStep/2 : 0;
    //Handle floor bounces
    //To make sure the floor is air tight, we must be able to process multiple
    //bounces per time step to avoid the "tunnel effect".
    while ((y + deltaY) > (1 - borderY) && alive) {
      // time to hit floor
      var c = y-(1-borderY);
      var b = velocityY;
      var a = gravity/2;
      // The greater root is always the right one
      var subStep = (-b + Math.sqrt(b*b-4*a*c))/(2*a);
      //velocity after floor hit
      velocityY = -elasticity*(velocityY + gravity*subStep);
      remainingTime -= subStep;
      var bounceTime = curTime - remainingTime;
      if (bounceTime - previousBounceTime < 0.005){
        // The number of iterations may not be finite within a timestep
        // with elasticity < 1. This is due to power series convergence.
        // To gard against hanging, we treat the ball as rolling on the ground
        // once time between bounces is less than 5ms
        alive = false;
        deltaY = 0;
      } else {
        deltaY = velocityY*remainingTime+gravity*remainingTime*remainingTime/2;
      }
      previousBounceTime = bounceTime;
      y = (1 - borderY);
    }
    y += deltaY;
    velocityY += gravity*remainingTime;

    drawBall(x * canvasWidth, y * canvasHeight, angle); // externally defined
  }

  return {
    step: step
  }
}
