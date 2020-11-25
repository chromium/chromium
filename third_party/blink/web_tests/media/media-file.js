var audioCodecs = [
    ["audio/wav", "wav"],
    ["audio/aac", "m4a"],
    ["audio/ogg", "oga"]
];

var videoCodecs = [
    ["video/mp4", "mp4"],
    ["video/ogg", "ogv"],
    ["video/webm","webm"]
];

function mimeTypeForExtension(extension) {
    for (var i = 0; i < videoCodecs.length; ++i) {
        if (extension == videoCodecs[i][1])
            return videoCodecs[i][0];
    }
    for (var i = 0; i < audioCodecs.length; ++i) {
        if (extension == audioCodecs[i][1])
            return audioCodecs[i][0];
    }

    return "";
}

function mimeTypeForFile(filename) {
 var lastPeriodIndex = filename.lastIndexOf(".");
  if (lastPeriodIndex > 0)
    return mimeTypeForExtension(filename.substring(lastPeriodIndex + 1));

  return "";
}

function setSrcByTagName(tagName, src) {
    var elements = document.getElementsByTagName(tagName);
    if (elements) {
        for (var i = 0; i < elements.length; ++i)
            elements[i].src = src;
    }
}

function videoPresentationPromise(video) {
    return new Promise(resolve => video.requestVideoFrameCallback(resolve));
}

// This function should be called before setting video.src to guarantee that we
// catch all new frames.
function allVideosPresentedPromise() {
    let videos = Array.from(document.getElementsByTagName('video'));
    return Promise.all(videos.map(video => videoPresentationPromise(video)));
}

function setSrcById(id, src) {
    var element = document.getElementById(id);
    if (element)
        element.src = src;
}

function stripExtension(filename) {
  var lastPeriodIndex = filename.lastIndexOf(".");
  if (lastPeriodIndex > 0)
    return filename.substring(0, lastPeriodIndex);
  return filename;
}
