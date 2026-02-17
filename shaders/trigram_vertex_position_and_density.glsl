#version 150
// Trigram vertex shader: positions from byte triples, pass file position and bin density.
in vec3 pos;
in float filePos;
in float density;
uniform mat4 mvp;
uniform float pointSize;
out float vFilePos;
out float vDensity;
void main() {
  gl_Position = mvp * vec4(pos, 1.0);
  gl_PointSize = pointSize;
  vFilePos = filePos;
  vDensity = density;
}
