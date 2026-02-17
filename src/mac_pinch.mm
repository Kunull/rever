#ifdef __APPLE__

#import <AppKit/AppKit.h>

static float s_pinchAccum = 0.0f;
static id s_monitor = nil;

float MacPinchZoomDeltaConsume(void) {
  float v = s_pinchAccum;
  s_pinchAccum = 0.0f;
  return v;
}

void MacInstallPinchMonitor(void) {
  if (s_monitor != nil) return;
  s_monitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskMagnify
                                                    handler:^NSEvent *(NSEvent *event) {
    s_pinchAccum += (float)[event magnification];
    return event;  // let the event continue
  }];
}

#endif
