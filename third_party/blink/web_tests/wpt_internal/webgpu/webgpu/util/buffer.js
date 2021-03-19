/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export function makeBufferWithContents(device, dataArray, usage) {
  const buffer = device.createBuffer({
    mappedAtCreation: true,
    size: dataArray.byteLength,
    usage,
  });

  const mappedBuffer = buffer.getMappedRange();
  const constructor = dataArray.constructor;
  new constructor(mappedBuffer).set(dataArray);
  buffer.unmap();
  return buffer;
}
