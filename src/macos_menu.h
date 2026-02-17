#pragma once

#ifdef __APPLE__

#ifdef __cplusplus
extern "C" {
#endif

// Key equivalent for native macOS menu (e.g. Cmd+O).
// valid=0 means no shortcut. key is the character (e.g. 'o').
typedef struct MacosKeyEquivalent {
  int valid;
  int ctrl, opt, cmd, shift;
  int key;
} MacosKeyEquivalent;

void macosMenuBarInit(void);
void macosClearMenu(void);
int macosBeginMainMenuBar(void);
void macosEndMainMenuBar(void);

int macosBeginMenu(const char* label, int enabled);
void macosEndMenu(void);

// Returns true if this item was clicked this frame.
int macosMenuItem(const char* label, MacosKeyEquivalent keyEq, int selected, int enabled);
int macosMenuItemSelect(const char* label, MacosKeyEquivalent keyEq, int* selected, int enabled);

void macosSeparator(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
static inline MacosKeyEquivalent macosKey(char key, int cmd, int shift = 0, int ctrl = 0, int opt = 0) {
  MacosKeyEquivalent k = { 1, ctrl, opt, cmd, shift, (int)(unsigned char)key };
  return k;
}
static inline MacosKeyEquivalent macosKeyNone(void) {
  MacosKeyEquivalent k = { 0, 0, 0, 0, 0, 0 };
  return k;
}
#endif

#endif
