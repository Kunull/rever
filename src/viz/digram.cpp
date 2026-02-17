#include "viz/digram.hpp"
#include "core/app_state.hpp"
#include "viz/gl_helpers.hpp"
#include "ui/ui_legends.hpp"
#include "imgui.h"
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif
#include <algorithm>
#include <cstdio>
#include <vector>

namespace {

static const char* digramVS = R"(#version 150
in vec2 aPos;
out vec2 vCoord;
void main() {
  gl_Position = vec4(aPos * 2.0 - 1.0, 0.0, 1.0);
  vCoord = aPos;
})";

static const char* digramFS = R"(#version 150
in vec2 vCoord;
out vec4 oColor;
uniform sampler2D tx;
uniform int colorByDensity;
uniform float maxFreq;
vec3 fireColor(float t) {
  t = clamp(t, 0.0, 1.0);
  if (t <= 0.0) return vec3(0.0, 0.0, 0.0);
  if (t >= 1.0) return vec3(1.0, 1.0, 1.0);
  if (t < 0.2) return mix(vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.4), t * 5.0);
  if (t < 0.4) return mix(vec3(0.0, 0.0, 0.4), vec3(0.0, 0.6, 0.9), (t - 0.2) * 5.0);
  if (t < 0.6) return mix(vec3(0.0, 0.6, 0.9), vec3(0.0, 0.9, 0.2), (t - 0.4) * 5.0);
  if (t < 0.8) return mix(vec3(0.0, 0.9, 0.2), vec3(1.0, 0.85, 0.0), (t - 0.6) * 5.0);
  return mix(vec3(1.0, 0.85, 0.0), vec3(1.0, 1.0, 1.0), (t - 0.8) * 5.0);
}
vec3 blackwall2077Color(float t) {
  t = clamp(t, 0.0, 1.0);
  vec3 maroon = vec3(0.35, 0.02, 0.08);
  vec3 peacock = vec3(0.0, 0.28, 0.42);
  vec3 mid = vec3(0.22, 0.06, 0.2);
  if (t < 0.5) return mix(maroon, mid, t * 2.0);
  return mix(mid, peacock, (t - 0.5) * 2.0);
}
void main() {
  vec4 t = texture(tx, vCoord);
  float clr = t.x;
  float ch  = t.y;
  if (colorByDensity == 2) {
    float d = (maxFreq > 0.0) ? clamp(clr / maxFreq, 0.0, 1.0) : 0.0;
    oColor = vec4(fireColor(d), 1.0);
  } else if (colorByDensity == 1) {
    if (clr > 0.0) ch /= clr;
    clr *= 4096.0;
    vec3 c = blackwall2077Color(ch);
    oColor = vec4(c * clr, 1.0);
  } else {
    if (clr > 0.0) ch /= clr;
    clr *= 4096.0;
    oColor = vec4(clr * (1.0 - ch), clr * 0.5, clr * ch, 1.0);
  }
})";

}  // namespace

void initDigramGL() {
  if (g.digramShader) return;
  GLuint vs = compileShader(GL_VERTEX_SHADER, digramVS);
  GLuint fs = compileShader(GL_FRAGMENT_SHADER, digramFS);
  g.digramShader = linkProgram(vs, fs);
  if (!g.digramShader) return;
  float quad[] = {0, 0, 0, 1, 1, 0, 1, 1};
  glGenVertexArrays(1, &g.digramVAO);
  glGenBuffers(1, &g.digramVBO);
  glBindVertexArray(g.digramVAO);
  glBindBuffer(GL_ARRAY_BUFFER, g.digramVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
  GLint loc = glGetAttribLocation(g.digramShader, "aPos");
  glEnableVertexAttribArray(loc);
  glVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  glBindVertexArray(0);
}

void renderDigramFBO() {
  if (!g.digramFBO || !g.digramRenderTex || !g.digramShader) return;
  GLint prevFBO;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
  GLint prevViewport[4];
  glGetIntegerv(GL_VIEWPORT, prevViewport);
  GLint prevProgram;
  glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
  GLint prevVAO;
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVAO);
  GLint prevTex;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);

  glBindFramebuffer(GL_FRAMEBUFFER, g.digramFBO);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.digramRenderTex, 0);
  glViewport(0, 0, 256, 256);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);

  glUseProgram(g.digramShader);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, g.digramTex);
  glUniform1i(glGetUniformLocation(g.digramShader, "tx"), 0);
  glUniform1i(glGetUniformLocation(g.digramShader, "colorByDensity"), g.bigramColorMode);
  float maxFreqVal = (g.bytes.size() > 0 && g.digramMaxCount > 0)
                         ? (float)g.digramMaxCount / (float)g.bytes.size()
                         : 0.0f;
  glUniform1f(glGetUniformLocation(g.digramShader, "maxFreq"), maxFreqVal);
  glBindVertexArray(g.digramVAO);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  glBindVertexArray(prevVAO);
  glUseProgram(prevProgram);
  glBindTexture(GL_TEXTURE_2D, prevTex);
  glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
  glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
}

void buildDigramTexture() {
  if (g.bytes.size() < 2) return;
  size_t focusEnd = g.focusEnd != 0 ? g.focusEnd : g.bytes.size();
  size_t focusStart = g.focusStart;
  size_t end = g.vizRangeEnd != 0 ? g.vizRangeEnd : focusEnd;
  size_t start = g.vizRangeStart;
  start = std::max(start, focusStart);
  end = std::min(end, focusEnd);
  if (start >= end || end - start < 2) return;
  size_t sz = end - start;
  std::vector<uint64_t> bigtab(256 * 256 * 2, 0);
  for (size_t i = start; i + 1 < end; i++) {
    size_t idx = g.bytes[i] * 512 + g.bytes[i + 1] * 2;
    bigtab[idx]++;
    bigtab[idx + 1] += (i - start);
  }
  uint64_t maxPairCount = 0;
  for (size_t i = 0; i < 256 * 256 * 2; i += 2)
    if (bigtab[i] > maxPairCount) maxPairCount = bigtab[i];
  g.digramMaxCount = maxPairCount;

  std::vector<float> ftab(256 * 256 * 2);
  for (int i = 0; i < 256; i++) {
    for (int j = 0; j < 256; j++) {
      int idx = i * 512 + j * 2;
      ftab[idx] = float(bigtab[idx]) / float(sz);
      ftab[idx + 1] = float(bigtab[idx + 1]) / float(sz) / float(sz);
    }
  }
  if (!g.digramTex) glGenTextures(1, &g.digramTex);
  glBindTexture(GL_TEXTURE_2D, g.digramTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, 256, 256, 0, GL_RG, GL_FLOAT, ftab.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  if (!g.digramRenderTex) glGenTextures(1, &g.digramRenderTex);
  glBindTexture(GL_TEXTURE_2D, g.digramRenderTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  if (!g.digramFBO) glGenFramebuffers(1, &g.digramFBO);
  renderDigramFBO();
}

void drawBigram() {
  if (!ImGui::Begin("Bigram", &g.showBigram)) {
    ImGui::End();
    return;
  }
  if (!g.fileLoaded || !g.digramRenderTex) {
    ImGui::TextDisabled("No data");
    ImGui::End();
    return;
  }

  const char* colorNames[] = {"Color: Position in file", "Color: Blackwall2077", "Color: Density (fire)"};
  if (ImGui::Combo("Color", &g.bigramColorMode, colorNames, 3)) {
    renderDigramFBO();
  }

  {
    ImVec2 barPos = ImGui::GetCursorScreenPos();
    float barW = std::min(ImGui::GetContentRegionAvail().x, 300.0f);
    float barH = 10.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (g.bigramColorMode == 0)
      drawLegendBar(dl, barPos, barW, barH, bigramLegendColor, 0.0f, "File start", "File end");
    else if (g.bigramColorMode == 1)
      drawLegendBar(dl, barPos, barW, barH, bigramBlackwall2077LegendColor, 0.0f, "File start", "File end");
    else {
      char bigramMaxStr[32];
      snprintf(bigramMaxStr, sizeof(bigramMaxStr), "%llu", (unsigned long long)g.digramMaxCount);
      drawLegendBar(dl, barPos, barW, barH, bigramFireLegendColor, 0.0f, "0", bigramMaxStr);
    }
    ImGui::Dummy(ImVec2(barW, barH + 16));
  }

  ImGui::TextColored(
      ImVec4(0.35f, 0.35f, 0.35f, 1),
      g.bigramColorMode == 0
          ? "Byte N (X) \xe2\x86\x92 Byte N+1 (Y)  |  Red=start  Blue=end"
          : g.bigramColorMode == 1
              ? "Byte N (X) \xe2\x86\x92 Byte N+1 (Y)  |  Maroon=start  Peacock=end"
              : "Byte N (X) \xe2\x86\x92 Byte N+1 (Y)  |  Fire = pair frequency");

  ImVec2 avail = ImGui::GetContentRegionAvail();
  float side = std::min(avail.x, avail.y);
  if (side < 16) {
    ImGui::Dummy(avail);
    ImGui::End();
    return;
  }
  ImGui::Image((ImTextureID)(intptr_t)g.digramRenderTex, ImVec2(side, side));
  ImGui::End();
}
