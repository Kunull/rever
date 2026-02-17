#import <Cocoa/Cocoa.h>

static char s_pathBuf[4096];

extern "C" const char* openFileDialog() {
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        [panel setCanChooseFiles:YES];
        [panel setCanChooseDirectories:NO];
        [panel setAllowsMultipleSelection:NO];
        [panel setTitle:@"Open Binary"];

        if ([panel runModal] == NSModalResponseOK) {
            NSString* path = [[panel URL] path];
            strncpy(s_pathBuf, [path UTF8String], sizeof(s_pathBuf) - 1);
            s_pathBuf[sizeof(s_pathBuf) - 1] = '\0';
            return s_pathBuf;
        }
    }
    return nullptr;
}

extern "C" const char* saveFileDialog() {
    @autoreleasepool {
        NSSavePanel* panel = [NSSavePanel savePanel];
        [panel setTitle:@"Save Binary"];

        if ([panel runModal] == NSModalResponseOK) {
            NSString* path = [[panel URL] path];
            strncpy(s_pathBuf, [path UTF8String], sizeof(s_pathBuf) - 1);
            s_pathBuf[sizeof(s_pathBuf) - 1] = '\0';
            return s_pathBuf;
        }
    }
    return nullptr;
}
