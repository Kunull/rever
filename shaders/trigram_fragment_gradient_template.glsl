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
uniform int colorMode;  // 0 = single, 1 = Fyre (violet->..->red). Brightness scales whole gradient toward black.
uniform float c_brightness;
out vec4 oColor;

// Returns gradient color for position t in [0,1]. Add new gradients here.
vec3 getGradientColor(float t) {
  if (colorMode == 0) {
    return vec3(0.6, 0.6, 0.6);  // single neutral
  }
  if (colorMode == 1) {
    // Fyre: violet -> blue -> light blue -> white -> yellow -> red; purple and red get more range
    vec3 violet = vec3(0.58, 0.0, 0.83);
    vec3 blue   = vec3(0.2, 0.5, 1.0);
    vec3 lblue  = vec3(0.4, 0.75, 1.0);
    vec3 white  = vec3(1.0, 1.0, 1.0);
    vec3 yellow = vec3(1.0, 1.0, 0.3);
    vec3 red    = vec3(1.0, 0.15, 0.1);
    float t0 = 0.0, t1 = 0.22, t2 = 0.38, t3 = 0.54, t4 = 0.70, t5 = 1.0;
    if (t < t1) return mix(violet, blue, (t - t0) / (t1 - t0));
    if (t < t2) return mix(blue, lblue, (t - t1) / (t2 - t1));
    if (t < t3) return mix(lblue, white, (t - t2) / (t3 - t2));
    if (t < t4) return mix(white, yellow, (t - t3) / (t4 - t3));
    return mix(yellow, red, (t - t4) / (t5 - t4));
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
