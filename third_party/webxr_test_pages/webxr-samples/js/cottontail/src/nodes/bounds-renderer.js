// Copyright 2018 The Immersive Web Community Group
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

/*
This file renders a passed in XRBoundedReferenceSpace object and attempts
to render geometry on the floor to indicate where the bounds is.
The bounds `geometry` is a series of DOMPointReadOnlys in
clockwise-order.
*/

import {Material, RENDER_ORDER} from '../core/material.js';
import {Node} from '../core/node.js';
import {Primitive, PrimitiveAttribute} from '../core/primitive.js';

const GL = WebGLRenderingContext; // For enums

class BoundsMaterial extends Material {
  constructor() {
    super();

    this.renderOrder = RENDER_ORDER.ADDITIVE;
    this.state.blend = true;
    this.state.blendFuncSrc = GL.SRC_ALPHA;
    this.state.blendFuncDst = GL.ONE;
    this.state.depthTest = false;
    this.state.cullFace = false;
  }

  get materialName() {
    return 'BOUNDS_RENDERER';
  }

  get vertexSource() {
    return `
    attribute vec3 POSITION;
    varying vec3 v_pos;
    vec4 vertex_main(mat4 proj, mat4 view, mat4 model) {
      v_pos = POSITION;
      return proj * view * model * vec4(POSITION, 1.0);
    }`;
  }

  get fragmentSource() {
    return `
    precision mediump float;
    varying vec3 v_pos;
    vec4 fragment_main() {
      return vec4(1.0, 0.0, 0.0, (1.0 - v_pos.y) * 0.5);
    }`;
  }
}

export class BoundsRenderer extends Node {
  constructor(refSpace) {
    super();

    this._resetListener = (ev) => {
      console.log('Got a reset event');
      this.onReset(this._boundedRefSpace);
    };

    this.boundedRefSpace = refSpace;
  }

  onRendererChanged(renderer) {
    this._material = new BoundsMaterial();
    this.onReset(this._boundedRefSpace);
  }

  get boundedRefSpace() {
    return this._boundedRefSpace;
  }

  set boundedRefSpace(refSpace) {
    if (this._boundedRefSpace != refSpace) {
      if (this._boundedRefSpace) {
        this._boundedRefSpace.removeEventListener('reset', this._resetListener);
      }
      if (refSpace) {
        refSpace.addEventListener('reset', this._resetListener);
      }
      this.onReset(refSpace);
    }
  }

  onReset(refSpace) {
    if (this._boundedRefSpace) {
      this.clearRenderPrimitives();
    }
    this._boundedRefSpace = refSpace;
    if (!refSpace || refSpace.boundsGeometry.length === 0 || !this._renderer) {
      return;
    }

    let geometry = refSpace.boundsGeometry;

    let verts = [];
    let indices = [];

    // Tessellate the bounding points from XRStageBounds and connect
    // each point to a neighbor and 0,0,0.
    const pointCount = geometry.length;
    let lastIndex = -1;
    for (let i = 0; i < pointCount; i++) {
      const point = geometry[i];
      verts.push(point.x, 0, point.z);
      verts.push(point.x, 1, point.z);

      lastIndex += 2;
      if (i > 0) {
        indices.push(lastIndex, lastIndex-1, lastIndex-2);
        indices.push(lastIndex-2, lastIndex-1, lastIndex-3);
      }
    }

    if (pointCount > 1) {
      indices.push(1, 0, lastIndex);
      indices.push(lastIndex, 0, lastIndex-1);
    }

    let vertexBuffer = this._renderer.createRenderBuffer(GL.ARRAY_BUFFER, new Float32Array(verts));
    let indexBuffer = this._renderer.createRenderBuffer(GL.ELEMENT_ARRAY_BUFFER, new Uint16Array(indices));

    let attribs = [
      new PrimitiveAttribute('POSITION', vertexBuffer, 3, GL.FLOAT, 12, 0),
    ];

    let primitive = new Primitive(attribs, indices.length);
    primitive.setIndexBuffer(indexBuffer);

    let renderPrimitive = this._renderer.createRenderPrimitive(primitive, this._material);
    this.addRenderPrimitive(renderPrimitive);
  }
}
