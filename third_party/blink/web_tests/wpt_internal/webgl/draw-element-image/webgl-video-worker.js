class SimpleGLProgram {
  static vertShaderSrc = `#version 300 es
precision mediump float;
in vec2 a_pos;
in vec2 a_uv;
out vec2 v_uv;
void main(){
  gl_Position = vec4(a_pos, 0.0, 1.0);
  v_uv = a_uv;
}`;

  static fragShaderSrc = `#version 300 es
precision mediump float;
in vec2 v_uv;
uniform sampler2D u_tex;
out vec4 fragColor;
void main(){
  fragColor = texture(u_tex, v_uv);
}`;

  constructor(gl) {
    this.gl = gl;
    this.vertShader = gl.createShader(gl.VERTEX_SHADER);
    gl.shaderSource(this.vertShader, SimpleGLProgram.vertShaderSrc);
    gl.compileShader(this.vertShader);
    this.fragShader = gl.createShader(gl.FRAGMENT_SHADER);
    gl.shaderSource(this.fragShader, SimpleGLProgram.fragShaderSrc);
    gl.compileShader(this.fragShader);

    this.program = gl.createProgram();
    gl.attachShader(this.program, this.vertShader);
    gl.attachShader(this.program, this.fragShader);
    gl.linkProgram(this.program);
    if (!gl.getProgramParameter(this.program, gl.LINK_STATUS)) {
      console.error(`Link failed: ${gl.getProgramInfoLog(this.program)}`);
      console.error(`vs info-log: ${gl.getShaderInfoLog(this.vertShader)}`);
      console.error(`fs info-log: ${gl.getShaderInfoLog(this.fragShader)}`);
      return;
    }

    this.vertArray = gl.createVertexArray();
    this.vertBuf = gl.createBuffer();
    this.tex = gl.createTexture();
    this.positionLoc = gl.getAttribLocation(this.program, 'a_pos');
    this.texOffsetLoc = gl.getAttribLocation(this.program, 'a_uv');
    this.texLoc = gl.getUniformLocation(this.program, 'u_tex');
  }

  render(target, x, y) {
    x = Number.parseInt(x) || 0;
    y = Number.parseInt(y) || 0;
    const gl = this.gl;
    gl.useProgram(this.program);

    // Destination rect in GL clip space, placed at (x, y) in canvas grid
    const xMin = -1 + (2 * x / gl.drawingBufferWidth);
    const xMax = (2 * (target.width + x) / gl.drawingBufferWidth) - 1.0;
    const yMin = 1.0 - (2 * (target.height + y) / gl.drawingBufferHeight);
    const yMax = 1 - (2 * y / gl.drawingBufferHeight);

    gl.bindVertexArray(this.vertArray);
    gl.bindBuffer(gl.ARRAY_BUFFER, this.vertBuf);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([
      xMin, yMin, 0, 0,
      xMax, yMin, 1, 0,
      xMin, yMax, 0, 1,
      xMin, yMax, 0, 1,
      xMax, yMin, 1, 0,
      xMax, yMax, 1, 1
    ]), gl.STATIC_DRAW);
    gl.enableVertexAttribArray(this.positionLoc);
    gl.vertexAttribPointer(this.positionLoc, 2, gl.FLOAT, false, 16, 0);
    gl.enableVertexAttribArray(this.texOffsetLoc);
    gl.vertexAttribPointer(this.texOffsetLoc, 2, gl.FLOAT, false, 16, 8);

    gl.bindTexture(gl.TEXTURE_2D, this.tex);
    gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true);
    // Allocate texture backing
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, target.width, target.height, 0,
                  gl.RGBA, gl.UNSIGNED_BYTE, null);
    gl.texElementSubImage2D(gl.TEXTURE_2D, /*level*/ 0, /*xoffset*/ 0,
                            /*yoffset*/ 0, target);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    gl.uniform1i(this.texLoc, 0);
    gl.drawArrays(gl.TRIANGLES, 0, 6);
  }
}

// This script supports two modes: reftest and pixeltest, which are enabled by
// sending a message containing 'reftest' or 'pixeltest' respectively.
//
// In reftest mode, the script accumulates three frames, clears the canvas to
// white, and renders the frames in a 2x2 grid (top-left, top-right, and
// bottom-left quadrants). Once rendering is complete, it sends a 'done'
// message to the main thread to trigger the screenshot.
//
// In pixeltest mode, the script renders each incoming frame individually and
// reads back the pixel color at coordinates (25, 25), sending the color data
// back to the main thread for assertion.
let reftest = false, pixeltest = false;
let gl;
let prog;
let frames = [];
self.onmessage = (e) => {
  if (e.data.reftest) {
    reftest = true;
  }
  if (e.data.pixeltest) {
    pixeltest = true;
  }
  if (e.data.canvas) {
    gl = e.data.canvas.getContext('webgl2');
    prog = new SimpleGLProgram(gl);
    self.postMessage({gl_ready: true});
  }
  if (e.data.elementImage) {
    if (reftest) {
      frames.push(e.data.elementImage);
      if (frames.length == 3) {
        gl.clearColor(1, 1, 1, 1);
        gl.clear(gl.COLOR_BUFFER_BIT);
        prog.render(frames[0], 0, 0);
        prog.render(frames[1], 100, 0);
        prog.render(frames[2], 0, 100);
        gl.finish();
        self.postMessage({done: true});
      }
    }
    if (pixeltest) {
      prog.render(e.data.elementImage);
      const imageData = new Uint8Array(4);
      gl.readPixels(25, 25, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, imageData);
      self.postMessage({pixel: Array.from(imageData)});
    }
  }
};
