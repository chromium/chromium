setTimeout(async () => {
  const img = await window.__lcpImage;
  document.body.append(img);
}, 0);
