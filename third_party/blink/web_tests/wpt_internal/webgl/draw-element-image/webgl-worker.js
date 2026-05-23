import { SimpleGLProgram } from "/wpt_internal/resources/canvas-draw-element/util.js";
let prog;
self.onmessage = function(e) {
  try {
    if (e.data.canvas) {
      prog = new SimpleGLProgram(e.data.canvas.getContext('webgl2'));
    }
    if (e.data.elementImage) {
      prog.renderWithSize(e.data.elementImage, ...e.data.args);
      self.postMessage('done');
    }
  } catch (err) {
    self.postMessage('error: ' + err.message + '\n' + err.stack);
  }
};
