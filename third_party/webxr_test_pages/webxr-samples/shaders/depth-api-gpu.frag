precision mediump float;

uniform sampler2D uDepthTexture;
uniform mat4 uUvTransform;
uniform float uRawValueToMeters;
uniform float uAlpha;

varying vec2 vTexCoord;

float DepthGetMeters(in sampler2D depth_texture, in vec2 depth_uv) {
  // Depth is packed into the luminance and alpha components of its texture.
  // The texture is in a normalized format, storing raw values that need to be
  // converted to meters.
  vec2 packedDepthAndVisibility = texture2D(depth_texture, depth_uv).ra;
  return dot(packedDepthAndVisibility, vec2(255.0, 256.0 * 255.0)) * uRawValueToMeters;
}

const highp float kMaxDepthInMeters = 8.0;
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
  vec4 texCoord = uUvTransform * vec4(vTexCoord, 0, 1);

  highp float normalized_depth = clamp(
    DepthGetMeters(uDepthTexture, texCoord.xy) / kMaxDepthInMeters, 0.0, 1.0);
  gl_FragColor = vec4(DepthGetColorVisualization(normalized_depth), uAlpha);
}

// Insert turbo.glsl here.
