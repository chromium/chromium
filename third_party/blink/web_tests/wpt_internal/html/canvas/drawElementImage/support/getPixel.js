// Extract a pixel's RGBA array at (x, y) from an ImageData object.
function getPixel(imageData, x, y) {
  const index = (y * imageData.width + x) * 4;
  return imageData.data.slice(index, index + 4);
}
