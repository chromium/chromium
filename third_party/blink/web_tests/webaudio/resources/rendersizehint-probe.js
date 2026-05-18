/**
 * Creates an AudioWorklet probe node and attaches it to a source node.
 * Returns a promise that resolves with the metrics collected from the probe's
 * construction and processing thread, leaving assertions to the test file.
 *
 * @param {AudioContext} context
 * @param {AudioNode} sourceNode
 * @param {string} processorPath - Relative path to rendersizehint-processor.js
 * @return {Promise<{
 *   probedRenderQuantumSize: number,
 *   processBlockSize: number
 * }>}
 */
async function probeRenderSizeHint(context, sourceNode, processorPath) {
  await context.audioWorklet.addModule(processorPath);
  const probe = new AudioWorkletNode(context, 'rendersizehint-processor');
  sourceNode.connect(probe).connect(context.destination);

  let probedRenderQuantumSize = null;

  return new Promise((resolve) => {
    probe.port.onmessage = (event) => {
      if (!event.data) {
        return;
      }
      if (event.data.type === 'constructor') {
        probedRenderQuantumSize = event.data.renderQuantumSize;
      } else if (event.data.type === 'process') {
        resolve({
          probedRenderQuantumSize: probedRenderQuantumSize,
          processBlockSize: event.data.length
        });
      }
    };
  });
}
