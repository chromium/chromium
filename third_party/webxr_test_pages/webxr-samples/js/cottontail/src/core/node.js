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

import {Ray} from '../math/ray.js';
import {mat4, vec3, quat} from '../math/gl-matrix.js';

const DEFAULT_TRANSLATION = new Float32Array([0, 0, 0]);
const DEFAULT_ROTATION = new Float32Array([0, 0, 0, 1]);
const DEFAULT_SCALE = new Float32Array([1, 1, 1]);

let tmpRayMatrix = mat4.create();

export class Node {
  constructor() {
    this.name = null; // Only for debugging
    this.children = [];
    this.parent = null;
    this.visible = true;
    this.selectable = false;

    this._matrix = null;

    this._dirtyTRS = false;
    this._translation = null;
    this._rotation = null;
    this._scale = null;

    this._dirtyWorldMatrix = false;
    this._worldMatrix = null;

    this._activeFrameId = -1;
    this._hoverFrameId = -1;
    this._renderPrimitives = null;
    this._renderer = null;

    this._selectHandler = null;
  }

  _setRenderer(renderer) {
    if (this._renderer == renderer) {
      return;
    }

    if (this._renderer) {
      // Changing the renderer removes any previously attached renderPrimitives
      // from a different renderer.
      this.clearRenderPrimitives();
    }

    this._renderer = renderer;
    if (renderer) {
      this.onRendererChanged(renderer);

      for (let child of this.children) {
        child._setRenderer(renderer);
      }
    }
  }

  onRendererChanged(renderer) {
    // Override in other node types to respond to changes in the renderer.
  }

  // Create a clone of this node and all of it's children. Does not duplicate
  // RenderPrimitives, the cloned nodes will be treated as new instances of the
  // geometry.
  clone() {
    let cloneNode = new Node();
    cloneNode.name = this.name;
    cloneNode.visible = this.visible;
    cloneNode._renderer = this._renderer;

    cloneNode._dirtyTRS = this._dirtyTRS;
    cloneNode._dirtyWorldMatrix = this._dirtyWorldMatrix;

    if (this._translation) {
      cloneNode._translation = vec3.create();
      vec3.copy(cloneNode._translation, this._translation);
    }

    if (this._rotation) {
      cloneNode._rotation = quat.create();
      quat.copy(cloneNode._rotation, this._rotation);
    }

    if (this._scale) {
      cloneNode._scale = vec3.create();
      vec3.copy(cloneNode._scale, this._scale);
    }

    if (this._matrix) {
      cloneNode._matrix = mat4.create();
      mat4.copy(cloneNode._matrix, this._matrix);
    }

    if (this._worldMatrix) {
      cloneNode._worldMatrix = mat4.create();
      mat4.copy(cloneNode._worldMatrix, this._worldMatrix);
    }

    this.waitForComplete().then(() => {
      if (this._renderPrimitives) {
        for (let primitive of this._renderPrimitives) {
          cloneNode.addRenderPrimitive(primitive);
        }
      }

      for (let child of this.children) {
        cloneNode.addNode(child.clone());
      }
      cloneNode.setMatrixDirty();
    });

    return cloneNode;
  }

  markActive(frameId) {
    if (this.visible && this._renderPrimitives) {
      this._activeFrameId = frameId;
      for (let primitive of this._renderPrimitives) {
        primitive.markActive(frameId);
      }
    }

    for (let child of this.children) {
      if (child.visible) {
        child.markActive(frameId);
      }
    }
  }

  addNode(value) {
    if (!value || value.parent == this) {
      return;
    }

    if (value.parent) {
      value.parent.removeNode(value);
    }
    value.parent = this;

    this.children.push(value);

    if (this._renderer) {
      value._setRenderer(this._renderer);
    }
  }

  removeNode(value) {
    let i = this.children.indexOf(value);
    if (i > -1) {
      this.children.splice(i, 1);
      value.parent = null;
    }
  }

  clearNodes() {
    for (let child of this.children) {
      child.parent = null;
    }
    this.children = [];
  }

  setMatrixDirty() {
    if (!this._dirtyWorldMatrix) {
      this._dirtyWorldMatrix = true;
      for (let child of this.children) {
        child.setMatrixDirty();
      }
    }
  }

  _updateLocalMatrix() {
    if (!this._matrix) {
      this._matrix = mat4.create();
    }

    if (this._dirtyTRS) {
      this._dirtyTRS = false;
      mat4.fromRotationTranslationScale(
        this._matrix,
        this._rotation || DEFAULT_ROTATION,
        this._translation || DEFAULT_TRANSLATION,
        this._scale || DEFAULT_SCALE);
    }

    return this._matrix;
  }

  set matrix(value) {
    if (value) {
      if (!this._matrix) {
        this._matrix = mat4.create();
      }
      mat4.copy(this._matrix, value);
    } else {
      this._matrix = null;
    }
    this.setMatrixDirty();
    this._dirtyTRS = false;
    this._translation = null;
    this._rotation = null;
    this._scale = null;
  }

  get matrix() {
    this.setMatrixDirty();

    return this._updateLocalMatrix();
  }

  get worldMatrix() {
    if (!this._worldMatrix) {
      this._worldMatrix = mat4.create();
      this.setMatrixDirty();
    }

    if (this._dirtyWorldMatrix || this._dirtyTRS) {
      if (this.parent) {
        // TODO: Some optimizations that could be done here if the node matrix
        // is an identity matrix.
        mat4.mul(this._worldMatrix, this.parent.worldMatrix, this._updateLocalMatrix());
      } else {
        mat4.copy(this._worldMatrix, this._updateLocalMatrix());
      }
      this._dirtyWorldMatrix = false;
    }

    return this._worldMatrix;
  }

  // TODO: Decompose matrix when fetching these?
  set translation(value) {
    if (value != null) {
      this._dirtyTRS = true;
      this.setMatrixDirty();
    }
    this._translation = value;
  }

  get translation() {
    this._dirtyTRS = true;
    this.setMatrixDirty();
    if (!this._translation) {
      this._translation = vec3.clone(DEFAULT_TRANSLATION);
    }
    return this._translation;
  }

  set rotation(value) {
    if (value != null) {
      this._dirtyTRS = true;
      this.setMatrixDirty();
    }
    this._rotation = value;
  }

  get rotation() {
    this._dirtyTRS = true;
    this.setMatrixDirty();
    if (!this._rotation) {
      this._rotation = quat.clone(DEFAULT_ROTATION);
    }
    return this._rotation;
  }

  set scale(value) {
    if (value != null) {
      this._dirtyTRS = true;
      this.setMatrixDirty();
    }
    this._scale = value;
  }

  get scale() {
    this._dirtyTRS = true;
    this.setMatrixDirty();
    if (!this._scale) {
      this._scale = vec3.clone(DEFAULT_SCALE);
    }
    return this._scale;
  }

  waitForComplete() {
    let childPromises = [];
    for (let child of this.children) {
      childPromises.push(child.waitForComplete());
    }
    if (this._renderPrimitives) {
      for (let primitive of this._renderPrimitives) {
        childPromises.push(primitive.waitForComplete());
      }
    }
    return Promise.all(childPromises).then(() => this);
  }

  get renderPrimitives() {
    return this._renderPrimitives;
  }

  addRenderPrimitive(primitive) {
    if (!this._renderPrimitives) {
      this._renderPrimitives = [primitive];
    } else {
      this._renderPrimitives.push(primitive);
    }
    primitive._instances.push(this);
  }

  removeRenderPrimitive(primitive) {
    if (!this._renderPrimitives) {
      return;
    }

    let index = this._renderPrimitives.indexOf(primitive);
    if (index > -1) {
      this._renderPrimitives.splice(index, 1);

      index = primitive._instances.indexOf(this);
      if (index > -1) {
        primitive._instances.splice(index, 1);
      }

      if (!this._renderPrimitives.length) {
        this._renderPrimitives = null;
      }
    }
  }

  clearRenderPrimitives() {
    if (this._renderPrimitives) {
      for (let primitive of this._renderPrimitives) {
        let index = primitive._instances.indexOf(this);
        if (index > -1) {
          primitive._instances.splice(index, 1);
        }
      }
      this._renderPrimitives = null;
    }
  }

  _hitTestSelectableNode(ray) {
    if (this._renderPrimitives) {
      let localRay = null;
      for (let primitive of this._renderPrimitives) {
        if (primitive._min) {
          if (!localRay) {
            mat4.invert(tmpRayMatrix, this.worldMatrix);
            mat4.multiply(tmpRayMatrix, tmpRayMatrix, ray.matrix);
            localRay = new Ray(tmpRayMatrix);
          }
          let intersection = localRay.intersectsAABB(primitive._min, primitive._max);
          if (intersection) {
            vec3.transformMat4(intersection, intersection, this.worldMatrix);
            return intersection;
          }
        }
      }
    }
    for (let child of this.children) {
      let intersection = child._hitTestSelectableNode(ray);
      if (intersection) {
        return intersection;
      }
    }
    return null;
  }

  hitTest(ray) {
    if (this.selectable && this.visible) {
      let intersection = this._hitTestSelectableNode(ray);

      if (intersection) {
        let origin = vec3.fromValues(ray.origin.x, ray.origin.y, ray.origin.z);
        return {
          node: this,
          intersection: intersection,
          distance: vec3.distance(origin, intersection),
        };
      }
      return null;
    }

    let result = null;
    for (let child of this.children) {
      let childResult = child.hitTest(ray);
      if (childResult) {
        if (!result || result.distance > childResult.distance) {
          result = childResult;
        }
      }
    }
    return result;
  }

  onSelect(value) {
    this._selectHandler = value;
  }

  get selectHandler() {
    return this._selectHandler;
  }

  // Called when a selectable node is selected.
  handleSelect() {
    if (this._selectHandler) {
      this._selectHandler();
    }
  }

  // Called when a selectable element is pointed at.
  onHoverStart() {

  }

  // Called when a selectable element is no longer pointed at.
  onHoverEnd() {

  }

  _update(timestamp, frameDelta) {
    this.onUpdate(timestamp, frameDelta);

    for (let child of this.children) {
      child._update(timestamp, frameDelta);
    }
  }

  // Called every frame so that the nodes can animate themselves
  onUpdate(timestamp, frameDelta) {

  }
}
