#version 150
// TRIGRAM FRAGMENT SHADER - GRADIENT TEMPLATE (canonical reference)
// Runtime copy is embedded in src/viz/trigram.cpp (trigramFS).
// ============================================
// Inputs:  vFilePos (0..1 along file), vDensity (0..1).
// Uniforms: invertColors, colorMode, c_brightness.
// Output:  oColor (RGBA). Same brightness/density formula for all gradients.
//
// To add a new gradient:
// 1. In app_state.hpp: extend trigramColorMode comment (e.g. 3 = "My gradient").
// 2. In this file AND in trigram.cpp trigramFS: add a case in getGradientColor(float t)
//    returning vec3 RGB. Omit green by keeping G <= max(R,B) in mid ranges if desired.
// 3. In trigram.cpp: add the option to gradientNames[] and increase Combo count.

in float vFilePos;
in float vDensity;
uniform int invertColors;
uniform int colorMode;  // 0 = single, 1 = Fyre (violet->..->orange). Brightness scales whole gradient toward black.
uniform float c_brightness;
out vec4 oColor;

// Returns gradient color for position t in [0,1]. Add new gradients here.
vec3 getGradientColor(float t) {
  if (colorMode == 0) {
    return vec3(0.6, 0.6, 0.6);  // single neutral
  }
  if (colorMode == 1) {
    // Fyre: violet -> blue -> light blue -> white -> yellow -> red -> orange (no green)
    vec3 violet = vec3(0.58, 0.0, 0.83);
    vec3 blue   = vec3(0.2, 0.5, 1.0);
    vec3 lblue  = vec3(0.4, 0.75, 1.0);
    vec3 white  = vec3(1.0, 1.0, 1.0);
    vec3 yellow = vec3(1.0, 1.0, 0.3);
    vec3 red    = vec3(1.0, 0.15, 0.1);
    vec3 orange = vec3(1.0, 0.45, 0.0);
    float s = 1.0 / 6.0;
    if (t < s) return mix(violet, blue, t / s);
    if (t < 2.0*s) return mix(blue, lblue, (t - s) / s);
    if (t < 3.0*s) return mix(lblue, white, (t - 2.0*s) / s);
    if (t < 4.0*s) return mix(white, yellow, (t - 3.0*s) / s);
    if (t < 5.0*s) return mix(yellow, red, (t - 4.0*s) / s);
    return mix(red, orange, (t - 5.0*s) / s);
  }
  return vec3(0.5, 0.5, 0.5);
}

void main() {
  float t = clamp(vFilePos, 0.0, 1.0);
  if (invertColors != 0) t = 1.0 - t;
  vec3 color = getGradientColor(t);
  float v_factor = 0.2 + 0.8 * vDensity;
  vec3 outColor = color * v_factor * c_brightness;
  oColor = vec4(clamp(outColor, 0.0, 1.0), 1.0);
}
