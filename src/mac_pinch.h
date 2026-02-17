#pragma once

#ifdef __APPLE__

// Install a local event monitor for trackpad pinch (NSEventTypeMagnify).
// Call once after the window exists (e.g. from main).
void MacInstallPinchMonitor(void);

// Return accumulated pinch magnification since last consume, then clear.
// Positive = zoom in. Call each frame from the view that uses pinch-to-zoom.
float MacPinchZoomDeltaConsume(void);

#else

inline void MacInstallPinchMonitor(void) {}
inline float MacPinchZoomDeltaConsume(void) { return 0.0f; }

#endif
