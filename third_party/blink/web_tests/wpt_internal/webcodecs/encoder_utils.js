async function getImageAsBitmap(width, height) {
  const src = "pattern.png";

  var size = {
    resizeWidth: width,
    resizeHeight: height
  };

  return fetch(src)
      .then(response => response.blob())
      .then(blob => createImageBitmap(blob, size));
}

async function generateBitmap(width, height, text) {
  let img = await getImageAsBitmap(width, height);
  let cnv = new OffscreenCanvas(width, height);
  var ctx = cnv.getContext('2d');
  ctx.drawImage(img, 0, 0, width, height);
  img.close();
  ctx.font = '30px fantasy';
  ctx.fillText(text, 5, 40);
  return createImageBitmap(cnv);
}

async function createFrame(width, height, ts) {
  let imageBitmap = await generateBitmap(width, height, ts.toString());
  return new VideoFrame(imageBitmap, { timestamp: ts });
}

function delay(time_ms) {
  return new Promise((resolve, reject) => {
    setTimeout(resolve, time_ms);
  });
};
