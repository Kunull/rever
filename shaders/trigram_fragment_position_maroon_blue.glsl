#version 150
// brightnessMode 0: Veles (c_brightness * v_factor). brightnessMode 1: Uniform (v_factor * brightness_scale).
in float vFilePos;
in float vDensity;
uniform int invertColors;
uniform int brightnessMode;
uniform float c_brightness;
uniform float brightness_scale;
out vec4 oColor;
void main() {
  float t = clamp(vFilePos, 0.0, 1.0);
  if (invertColors != 0) t = 1.0 - t;
  vec3 c_color_begin = vec3(1.0, 0.498, 0.0);
  vec3 c_color_end   = vec3(0.0, 0.498, 1.0);
  vec3 color = t * c_color_end + (1.0 - t) * c_color_begin;
  float v_factor = 0.2 + 0.8 * vDensity;
  float scale = (brightnessMode == 1) ? brightness_scale : c_brightness;
  vec3 outColor = color * v_factor * scale;
  oColor = vec4(clamp(outColor, 0.0, 1.0), 1.0);
}
