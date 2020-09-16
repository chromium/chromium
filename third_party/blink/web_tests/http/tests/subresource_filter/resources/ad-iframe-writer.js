// Creates and iframe and appends it to the body element. Make sure the caller
// has a body element!
function createAdFrame() {
  let ad_frame = document.createElement('iframe');
  document.body.appendChild(ad_frame);
  internals.setIsAdSubframe(ad_frame);
  return ad_frame;
}
