#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>

// Must match macos_menu.h
typedef struct MacosKeyEquivalent {
  int valid;
  int ctrl, opt, cmd, shift;
  int key;
} MacosKeyEquivalent;

static const NSInteger MenuBeginTag = 1;
static NSInteger s_currTag = MenuBeginTag;
static NSInteger s_selectedTag = -1;

@interface ReverMenuItemHandler : NSObject
- (void)onClick:(id)sender;
@end

@implementation ReverMenuItemHandler
- (void)onClick:(id)sender {
  NSMenuItem* item = sender;
  s_selectedTag = item.tag;
}
@end

static NSMenu* s_menuStack[64];
static int s_menuStackSize = 0;
static ReverMenuItemHandler* s_handler;
static int s_constructingMenu = 0;
static int s_resetNeeded = 1;

void macosMenuBarInit(void) {
  s_menuStackSize = 0;
  s_menuStack[0] = NSApp.mainMenu;
  s_menuStackSize = 1;
  s_handler = [[ReverMenuItemHandler alloc] init];
}

void macosClearMenu(void) {
  while (s_menuStack[0].itemArray.count > 2) {
    [s_menuStack[0] removeItemAtIndex:1];
  }
  s_currTag = (NSInteger)MenuBeginTag;
}

int macosBeginMainMenuBar(void) {
  if (s_resetNeeded) {
    macosClearMenu();
    s_resetNeeded = 0;
  }
  return 1;
}

void macosEndMainMenuBar(void) {
  s_constructingMenu = 0;
}

int macosBeginMenu(const char* label, int enabled) {
  NSString* title = [NSString stringWithUTF8String:label];
  NSInteger idx = [s_menuStack[s_menuStackSize - 1] indexOfItemWithTitle:title];
  if (idx == -1) {
    s_constructingMenu = 1;
    NSMenu* sub = [[NSMenu alloc] init];
    sub.autoenablesItems = NO;
    sub.title = title;
    NSMenuItem* item = [[NSMenuItem alloc] init];
    item.title = title;
    item.submenu = sub;
    item.enabled = enabled;
    NSInteger insertIdx = [s_menuStack[s_menuStackSize - 1] numberOfItems];
    if (s_menuStackSize == 1)
      insertIdx = insertIdx > 0 ? insertIdx - 1 : 0;
    [s_menuStack[s_menuStackSize - 1] insertItem:item atIndex:insertIdx];
    idx = insertIdx;
  }
  NSMenuItem* item = [s_menuStack[s_menuStackSize - 1] itemAtIndex:idx];
  if (item && item.submenu) {
    item.enabled = (BOOL)enabled;
    s_menuStack[s_menuStackSize] = item.submenu;
    s_menuStackSize++;
    return 1;
  }
  return 0;
}

void macosEndMenu(void) {
  if (s_menuStackSize > 0)
    s_menuStackSize--;
}

static void applyKeyEquivalent(NSMenuItem* item, MacosKeyEquivalent k) {
  if (!k.valid) {
    item.keyEquivalent = @"";
    item.keyEquivalentModifierMask = 0;
    return;
  }
  NSEventModifierFlags flags = 0;
  if (k.ctrl) flags |= NSEventModifierFlagControl;
  if (k.opt)  flags |= NSEventModifierFlagOption;
  if (k.cmd)  flags |= NSEventModifierFlagCommand;
  if (k.shift) flags |= NSEventModifierFlagShift;
  item.keyEquivalentModifierMask = flags;
  item.keyEquivalent = [NSString stringWithFormat:@"%c", (char)k.key];
}

int macosMenuItem(const char* label, MacosKeyEquivalent keyEq, int selected, int enabled) {
  NSString* title = [NSString stringWithUTF8String:label];
  NSMenu* menu = s_menuStack[s_menuStackSize - 1];
  if (s_constructingMenu) {
    NSMenuItem* item = [[NSMenuItem alloc] init];
    item.title = title;
    item.action = @selector(onClick:);
    item.target = s_handler;
    item.tag = s_currTag++;
    item.enabled = (BOOL)enabled;
    item.state = selected ? NSControlStateValueOn : NSControlStateValueOff;
    applyKeyEquivalent(item, keyEq);
    [menu addItem:item];
  }
  NSInteger idx = [menu indexOfItemWithTitle:title];
  if (idx >= 0 && idx < (NSInteger)menu.numberOfItems) {
    NSMenuItem* item = [menu itemAtIndex:idx];
    item.enabled = (BOOL)enabled;
    item.state = selected ? NSControlStateValueOn : NSControlStateValueOff;
    if (enabled && item.tag == s_selectedTag) {
      s_selectedTag = -1;
      return 1;
    }
  } else {
    s_resetNeeded = 1;
  }
  return 0;
}

int macosMenuItemSelect(const char* label, MacosKeyEquivalent keyEq, int* selected, int enabled) {
  int sel = selected ? *selected : 0;
  if (macosMenuItem(label, keyEq, sel, enabled)) {
    if (selected)
      *selected = !*selected;
    return 1;
  }
  return 0;
}

void macosSeparator(void) {
  if (s_constructingMenu) {
    [s_menuStack[s_menuStackSize - 1] addItem:[NSMenuItem separatorItem]];
  }
}
