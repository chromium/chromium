"use strict";

class MockTextDetection {
  constructor() {
    this.bindingSet_ =
        new mojo.BindingSet(shapeDetection.mojom.TextDetection);

    this.interceptor_ = new MojoInterfaceInterceptor(
        shapeDetection.mojom.TextDetection.name, "context", true);
    this.interceptor_.oninterfacerequest =
        e => this.bindingSet_.addBinding(this, e.handle);
    this.interceptor_.start();
  }

  detect(bitmapData) {
    this.bufferData_ =
        new Uint32Array(getArrayBufferFromBigBuffer(bitmapData.pixelData));
    return Promise.resolve({
      results: [
        {
          rawValue : "cats",
          boundingBox: { x: 1.0, y: 1.0, width: 100.0, height: 100.0 },
          cornerPoints: [
            { x: 1.0, y: 1.0 },
            { x: 101.0, y: 1.0 },
            { x: 101.0, y: 101.0 },
            { x: 1.0, y: 101.0 }
          ]
        },
        {
          rawValue : "dogs",
          boundingBox: { x: 2.0, y: 2.0, width: 50.0, height: 50.0 },
          cornerPoints: [
            { x: 2.0, y: 2.0 },
            { x: 52.0, y: 2.0 },
            { x: 52.0, y: 52.0 },
            { x: 2.0, y: 52.0 }
          ]
        },
      ],
    });
  }

  getFrameData() {
    return this.bufferData_;
  }
}

let mockTextDetection = new MockTextDetection();
