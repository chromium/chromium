precision mediump float;

uniform sampler2D uDepthTexture;
uniform mat4 uUvTransform;

varying vec2 vTexCoord;

float DepthGetMillimeters(in sampler2D depth_texture, in vec2 depth_uv) {
  // Depth is packed into the luminance and alpha components of its texture.
  // The texture is a normalized format, storing millimeters.
  vec2 packedDepthAndVisibility = texture2D(depth_texture, depth_uv).ra;
  return dot(packedDepthAndVisibility, vec2(255.0, 256.0 * 255.0));
}

const highp float kMaxDepth = 8000.0; // In millimeters.
const float kInvalidDepthThreshold = 0.01;

vec3 TurboColormap(in float x);

// Returns a color corresponding to the depth passed in. Colors range from red
// to green to blue, where red is closest and blue is farthest.
//
// Uses Turbo color mapping:
// https://ai.googleblog.com/2019/08/turbo-improved-rainbow-colormap-for.html
vec3 DepthGetColorVisualization(in float x) {
  return step(kInvalidDepthThreshold, x) * TurboColormap(x);
}

void main(void) {
  vec2 texCoord = (uUvTransform * vec4(vTexCoord.xy, 0, 1)).xy;

  highp float normalized_depth = clamp(
    DepthGetMillimeters(uDepthTexture, texCoord) / kMaxDepth, 0.0, 1.0);
  gl_FragColor = vec4(DepthGetColorVisualization(normalized_depth), 0.75);
}

// Insert turbo.glsl here.
