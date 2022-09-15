// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function SetupShaderProgram(gl) {
  // Create Vertex Shader.
  var vertex_shader = '' +
    'attribute vec4 vPosition;\n' +
    'attribute vec2 texCoord0;\n' +
    'varying vec2 texCoord;\n' +
    'void main() {\n' +
    '    gl_Position = vPosition;\n' +
    '    texCoord = texCoord0;\n' +
    '}\n';
  var vs = gl.createShader(gl.VERTEX_SHADER);
  gl.shaderSource(vs, vertex_shader);
  gl.compileShader(vs);

  // Create Fragment Shader.
  var fragment_shader = '' +
    '#ifdef GL_ES\n' +
    'precision mediump float;\n' +
    '#endif\n' +
    'uniform sampler2D tex;\n' +
    'varying vec2 texCoord;\n' +
    'void main() {\n' +
    '    gl_FragData[0] = texture2D(tex, texCoord);\n' +
    '}\n';
  var fs = gl.createShader(gl.FRAGMENT_SHADER);
  gl.shaderSource(fs, fragment_shader);
  gl.compileShader(fs);

  // Link Program.
  var program = gl.createProgram();
  gl.attachShader(program, vs);
  gl.attachShader(program, fs);

  gl.bindAttribLocation(program, 0, 'vPosition');
  gl.bindAttribLocation(program, 1, 'texCoord0');
  gl.linkProgram(program);

  gl.deleteShader(fs);
  gl.deleteShader(vs);
  gl.useProgram(program);
};

function SetupQuad(gl) {
  SetupShaderProgram(gl);

  // Setup unit quad
  var vertexObject = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, vertexObject);
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([
       1.0,  1.0, 0.0,
      -1.0,  1.0, 0.0,
      -1.0, -1.0, 0.0,
       1.0,  1.0, 0.0,
      -1.0, -1.0, 0.0,
       1.0, -1.0, 0.0]), gl.STATIC_DRAW);
  gl.enableVertexAttribArray(0);
  gl.vertexAttribPointer(0, 3, gl.FLOAT, false, 0, 0);

  var vertexObject = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, vertexObject);
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([
      1.0, 1.0,
      0.0, 1.0,
      0.0, 0.0,
      1.0, 1.0,
      0.0, 0.0,
      1.0, 0.0]), gl.STATIC_DRAW);
  gl.enableVertexAttribArray(1);
  gl.vertexAttribPointer(1, 2, gl.FLOAT, false, 0, 0);
};

function DoTextureUploadBenchmark(gl, dimension) {
  SetupShaderProgram(gl);
  SetupQuad(gl);

  var canvasTexture = gl.createTexture();
  gl.bindTexture(gl.TEXTURE_2D, canvasTexture);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);

  var frame = 0;
  var delta = 10;
  var pixels = new Uint8Array(dimension * dimension * 4);

  function update_texture() {
    requestAnimationFrame(update_texture);
    if (((frame + delta) < 0) || (frame + delta) >= 256 * 3)
      delta *= -1;
    frame += delta;

    for (var i = 0; i < dimension * dimension; ++i) {
      pixels[i*4] = Math.min(frame, 255);
      pixels[i*4+1] = Math.max(256, Math.min(511, frame)) - 256;
      pixels[i*4+2] = Math.max(512, frame) - 512;
      pixels[i*4+3] = 0xFF;
    }

    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, dimension, dimension, 0,
                  gl.RGBA, gl.UNSIGNED_BYTE, pixels);
    gl.drawArrays(gl.TRIANGLES, 0, 6);
  }
  update_texture();
}
