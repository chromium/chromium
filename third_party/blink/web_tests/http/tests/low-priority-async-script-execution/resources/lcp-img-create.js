logScript('lcp-img-create.js');
window.__lcpImage = new Promise(resolve => {
  const img = document.createElement('img');
  img.src = '/resources/square200.png';
  resolve(img);
});
