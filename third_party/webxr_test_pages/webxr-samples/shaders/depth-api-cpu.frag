precision mediump float;

varying float vDepthDistance;

const highp float kMaxDepth = 8.0; // In meters.
const float kInvalidDepthThreshold = 0.01;

vec3 TurboColormap(in float x);

// Returns a color corresponding to the depth passed in. Colors range from red
// to green to blue, where red is closest and blue is farthest.
//
// Uses Turbo color mapping :
// https://ai.googleblog.com/2019/08/turbo-improved-rainbow-colormap-for.html
vec3 DepthGetColorVisualization(in float x) {
  return step(kInvalidDepthThreshold, x) * TurboColormap(x);
}

void main(void) {
  highp float normalized_depth = clamp(vDepthDistance / 8.0, 0.0, 1.0);
  gl_FragColor = vec4(DepthGetColorVisualization(normalized_depth), 0.75);
}

// Insert turbo.glsl here.
