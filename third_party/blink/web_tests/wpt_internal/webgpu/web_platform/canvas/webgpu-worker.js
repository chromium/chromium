let queue;
let ctx;
let devicePromise = null;

self.onmessage = async function(e) {
  try {
    if (e.data.canvas) {
      if (!devicePromise) {
        devicePromise = (async () => {
          const adapter = await navigator.gpu.requestAdapter();
          const device = await adapter.requestDevice();
          queue = device.queue;
          return device;
        })();
      }
      const device = await devicePromise;

      const cvs = e.data.canvas;
      cvs.clientWidth = e.data.clientWidth;
      cvs.clientHeight = e.data.clientHeight;

      ctx = cvs.getContext("webgpu");
      ctx.configure({
        device: device,
        format: navigator.gpu.getPreferredCanvasFormat(),
        usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.RENDER_ATTACHMENT,
        alphaMode: "premultiplied",
      });
    }

    if (e.data.elementImage) {
      if (devicePromise) await devicePromise;
      const args = e.data.args || [];
      const target = e.data.elementImage;
      const destination = { texture: ctx.getCurrentTexture() };

      const sourceDict = { source: target };
      const destDict = { destination: destination };

      if (args.length === 6) {
        sourceDict.sx = args[0];
        sourceDict.sy = args[1];
        sourceDict.swidth = args[2];
        sourceDict.sheight = args[3];
        destDict.width = args[4];
        destDict.height = args[5];
      } else if (args.length === 4) {
        sourceDict.sx = args[0];
        sourceDict.sy = args[1];
        sourceDict.swidth = args[2];
        sourceDict.sheight = args[3];
      } else if (args.length === 2) {
        destDict.width = args[0];
        destDict.height = args[1];
      }

      queue.copyElementImageToTexture(sourceDict, destDict);
      self.postMessage('done');
    }
  } catch (err) {
    self.postMessage('error: ' + err.message + '\n' + err.stack);
  }
};
