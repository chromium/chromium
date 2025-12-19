function resizeToPixelGrid(canvas) {
  return new Promise(resolve => {
    new ResizeObserver(entries => {
      canvas.width = entries[0].devicePixelContentBoxSize[0].inlineSize;
      canvas.height = entries[0].devicePixelContentBoxSize[0].blockSize;
      resolve();
    }).observe(canvas);
  });
}

function computeScaledDestinationSize(cvs, target, scaleX, scaleY, outsetWidth, outsetHeight) {
  let targetWidth, targetHeight;
  outsetWidth = outsetWidth || 0;
  outsetHeight = outsetHeight || 0;

  if (target instanceof Element) {
    if (getComputedStyle(target).boxSizing != "border-box") {
      throw new TypeError("'box-sizing:border-box' is required to compute" +
                          " accurate destination size.");
    }
    const canvasScaleX = cvs.width / cvs.clientWidth;
    const canvasScaleY = cvs.height / cvs.clientHeight;
    const style = getComputedStyle(target);
    targetWidth = canvasScaleX * (Number.parseFloat(style.width) + outsetWidth);
    targetHeight = canvasScaleY * (Number.parseFloat(style.height) + outsetHeight);
  }

  if (target instanceof ImageData) {
    targetWidth = target.width;
    targetHeight = target.height;
  }

  return [Math.ceil(targetWidth * scaleX), Math.ceil(targetHeight * scaleY)];
}

function computeExplicitDestinationSize(cvs, scaleX, scaleY, swidth, sheight) {
  const targetWidth = scaleX * swidth;
  const targetHeight = scaleY * sheight;

  // Scale factor from CSS pixels to canvas grid.
  const canvasScaleX = cvs.width / cvs.clientWidth;
  const canvasScaleY = cvs.height / cvs.clientHeight;

  // Destination size in canvas grid
  const destWidth = Math.ceil(targetWidth * canvasScaleX);
  const destHeight = Math.ceil(targetHeight * canvasScaleY);

  return [destWidth, destHeight];
}

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

  render(target, scaleX, scaleY, sx, sy, swidth, sheight) {
    const gl = this.gl;
    gl.useProgram(this.program);
    const cvs = gl.canvas;

    const explicitScale = (scaleX !== undefined && scaleY !== undefined);
    const explicitSourceRect = (sx !== undefined && sy !== undefined
                                && swidth !== undefined && sheight !== undefined);
    scaleX = explicitScale ? scaleX : 1;
    scaleY = explicitScale ? scaleY : 1;
    let destWidth, destHeight;
    if (explicitSourceRect) {
      [destWidth, destHeight] =
        computeExplicitDestinationSize(cvs, scaleX, scaleY, swidth, sheight);
    } else {
      [destWidth, destHeight] =
        computeScaledDestinationSize(cvs, target, scaleX, scaleY);
    }

    // Destination rect in GL clip space, placed at top left
    const xMin = -1;
    const xMax = (2 * destWidth / cvs.width) - 1.0;
    const yMin = 1.0 - (2 * destHeight / cvs.height);
    const yMax = 1;

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
    const level = 0;
    const internalformat = gl.RGBA;
    const format = gl.RGBA;
    const type = gl.UNSIGNED_BYTE;

    if (target instanceof Element) {
      if (explicitSourceRect) {
        if (explicitScale) {
          gl.texElementImage2D(
            gl.TEXTURE_2D, level, internalformat,
            sx, sy, swidth, sheight,
            destWidth, destHeight,
            format, type, target);
        } else {
          gl.texElementImage2D(
            gl.TEXTURE_2D, level, internalformat,
            sx, sy, swidth, sheight,
            format, type, target);
        }
      } else {
        if (explicitScale) {
          gl.texElementImage2D(
            gl.TEXTURE_2D, level, internalformat,
            destWidth, destHeight,
            format, type, target);
        } else {
          gl.texElementImage2D(
            gl.TEXTURE_2D, level, internalformat,
            format, type, target);
        }
      }
    }

    if (target instanceof ImageData) {
      gl.texImage2D(gl.TEXTURE_2D, level, internalformat,
                    destWidth, destHeight, 0,
                    format, type, target);
    }

    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    gl.uniform1i(this.texLoc, 0);

    gl.drawArrays(gl.TRIANGLES, 0, 6);
  }
}

function copyElementImageToWebGPUCanvas(queue, ctx, target, scaleX, scaleY,
                                        sx, sy, swidth, sheight) {
  if (scaleX !== undefined && scaleY !== undefined) {
    const [destWidth, destHeight] =
          computeScaledDestinationSize(ctx.canvas, target, scaleX, scaleY);
    queue.copyElementImageToTexture(
      target, destWidth, destHeight, { texture: ctx.getCurrentTexture() });
  } else if (sx !== undefined && sy !== undefined &&
             swidth !== undefined && sheight !== undefined) {
    queue.copyElementImageToTexture(
      target, sx, sy, swidth, sheight, { texture: ctx.getCurrentTexture() });
  } else {
    queue.copyElementImageToTexture(
      target, { texture: ctx.getCurrentTexture() });
  }
}

export { resizeToPixelGrid,
         computeScaledDestinationSize,
         SimpleGLProgram,
         copyElementImageToWebGPUCanvas };
