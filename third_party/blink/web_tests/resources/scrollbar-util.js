// Contains helpers for calculating the dimensions for the various
// scrollbar parts.

// Helper to calculate track-width for non-custom standard
// scrollbars.
function calculateScrollbarThickness() {
    var container = document.createElement("div");
    container.style.width = "100px";
    container.style.height = "100px";
    container.style.position = "absolute";
    container.style.visibility = "hidden";
    container.style.overflow = "auto";

    document.body.appendChild(container);

    var widthBefore = container.clientWidth;
    var longContent = document.createElement("div");
    longContent.style.height = "1000px";
    container.appendChild(longContent);

    var widthAfter = container.clientWidth;

    container.remove();

    return widthBefore - widthAfter;
}

// Resets scroll offsets (only supports LTR for now).
function resetScrollOffset(scrollElement) {
  if(scrollElement !== undefined) {
    if(scrollElement.scrollLeft !== undefined) {
      scrollElement.scrollLeft = 0;
    }
    if(scrollElement.scrollTop !== undefined) {
      scrollElement.scrollTop = 0;
    }
  }
}

// TODO(arakeri): Add helpers for arrow widths.