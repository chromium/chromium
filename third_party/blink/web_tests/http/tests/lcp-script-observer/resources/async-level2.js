window.__installLCP = async () => {
  return new Promise(resolve => {
  // Create LCP image in a Promise callback to ensure v8 microtasks are
  // accounted for.
  const img = document.createElement('img');
    img.src = '/resources/square200.png';
    document.body.append(img);
    resolve();
  });
};
