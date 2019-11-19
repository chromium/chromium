// Copyright 2018 The Immersive Web Community Group
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

import {Material, RENDER_ORDER} from '../core/material.js';
import {Primitive, PrimitiveAttribute} from '../core/primitive.js';
import {Node} from '../core/node.js';
import {vec3} from '../math/gl-matrix.js';

const GL = WebGLRenderingContext; // For enums

class PlaneMaterial extends Material {
  constructor(options = {}) {
    super();

    this.baseColor = this.defineUniform('baseColor', options.baseColor);

    this.renderOrder = RENDER_ORDER.TRANSPARENT;
    this.state.blend = true;
    this.state.blendFuncSrc = GL.ONE;
    this.state.blendFuncDst = GL.ONE_MINUS_SRC_ALPHA;
    this.state.depthFunc = GL.LEQUAL;
    this.state.depthMask = false;
    this.state.cullFace = false;
  }

  get materialName() {
    return 'PLANE';
  }

  get vertexSource() {
    return `
    attribute vec3 POSITION;

    varying vec3 vLight;

    const vec3 lightDir = vec3(0.75, 0.5, 1.0);
    const vec3 ambientColor = vec3(0.5, 0.5, 0.5);
    const vec3 lightColor = vec3(0.75, 0.75, 0.75);

    vec4 vertex_main(mat4 proj, mat4 view, mat4 model) {
      vec3 normalRotated = vec3(model * vec4(0.0, 0.0, 1.0, 0.0));
      float lightFactor = max(dot(normalize(lightDir), normalRotated), 0.0);
      vLight = ambientColor + (lightColor * lightFactor);
      return proj * view * model * vec4(POSITION, 1.0);
    }`;
  }

  get fragmentSource() {
      return `
      precision mediump float;

      uniform vec4 baseColor;

      varying vec3 vLight;

      vec4 fragment_main() {
        return vec4(vLight, 1.0) * baseColor;
      }`;
  }
}

export class PlaneNode extends Node {
  constructor(options = {}) {
    super();
    if(!options.polygon)
      throw new Error(`Plane polygon must be specified.`);

    if(!options.baseColor)
      throw new Error(`Plane base color must be specified.`);

    this.baseColor = options.baseColor;
    this.polygon = options.polygon;

    // ring buffer containing last 3 plane primitives (meshes)
    this.primitives = [null, null, null];
    this.primitiveIndex = -1;

    this._material = new PlaneMaterial({baseColor : options.baseColor});

    this._renderer = null;
  }

  createPlanePrimitive(polygon) {
    let vertices = [];
    let indices = [];
    let min = null;
    let max = null;

    // first, collect all polygon vertices
    polygon.forEach(vertex => {
      vertices.push(vertex.x, vertex.y, vertex.z);

      if(min) {
        min[0] = Math.min(min[0], vertex.x);
        min[1] = Math.min(min[1], vertex.y);
        min[2] = Math.min(min[2], vertex.z);
      } else {
        min = vec3.fromValues(vertex.x, vertex.y, vertex.z);
      }

      if(max) {
        max[0] = Math.min(min[0], vertex.x);
        max[1] = Math.min(max[1], vertex.y);
        max[2] = Math.min(max[2], vertex.z);
      } else {
        max = vec3.fromValues(vertex.x, vertex.y, vertex.z);
      }
    });

    // then indices
    for(let i = 0; i < polygon.length - 2; i++) {
      indices.push(0, i + 1, i + 2);
    }

    let newPrimitiveIndex = (this.primitiveIndex + 1) % this.primitives.length;
    if(this.primitives[newPrimitiveIndex]) {
      // update
      let oldPrimitive = this.primitives[newPrimitiveIndex];

      this._renderer.updateRenderBuffer(oldPrimitive.attributes[0].buffer, new Float32Array(vertices));
      this._renderer.updateRenderBuffer(oldPrimitive.indexBuffer, new Uint16Array(indices));

      // attribs are still set, no need to re-set them

      // index buffer is still set, no need to re-set it
      oldPrimitive.setBounds(min, max);
      oldPrimitive.elementCount = indices.length;
    } else {
      // add
      let vertexBuffer = this._renderer.createRenderBuffer(
        GL.ARRAY_BUFFER, new Float32Array(vertices));
      let indexBuffer = this._renderer.createRenderBuffer(
        GL.ELEMENT_ARRAY_BUFFER, new Uint16Array(indices));

      let position_attribute = new PrimitiveAttribute('POSITION', vertexBuffer, 3, GL.FLOAT, 12, 0);

      let newPrimitive = new Primitive([position_attribute], indices.length);
      newPrimitive.setIndexBuffer(indexBuffer);
      newPrimitive.setBounds(min, max);

      this.primitives[newPrimitiveIndex] = newPrimitive;
    }

    this.primitiveIndex = newPrimitiveIndex;
  }

  get primitive() {
    if(!this.primitives[this.primitiveIndex]) {
      throw new Error(`Primitive is not set! Call createPlanePrimitive first!`);
    }

    return this.primitives[this.primitiveIndex];
  }

  onRendererChanged(renderer) {
    if(!this.polygon)
      throw new Error(`Polygon is not set on a plane where it should be!`);

    this._renderer = renderer;

    this.createPlanePrimitive(this.polygon);

    this.planeNode = this._renderer.createRenderPrimitive(this.primitive, this._material);
    this.addRenderPrimitive(this.planeNode);

    this.polygon = null;

    return this.waitForComplete();
  }

  onPlaneChanged(polygon) {
    if(this.polygon)
      throw new Error(`Polygon is set on a plane where it shouldn't be!`);

    this.createPlanePrimitive(polygon);

    // eagerly clean up render primitive's VAO
    if(this.planeNode._vao) {
      this._renderer._vaoExt.deleteVertexArrayOES(this.planeNode._vao);
      this.planeNode._vao = null;
    }

    this.planeNode.setPrimitive(this.primitive);

    return this.planeNode.waitForComplete();
  }
}

