#import <AppKit/AppKit.h>
#import <objc/runtime.h>
#include <cstdio>
#include <cstdlib>

static int foregroundRequests = 0;
static BOOL (*originalSetPolicy)(id, SEL, NSApplicationActivationPolicy);

static BOOL recordPolicy(id app, SEL selector, NSApplicationActivationPolicy policy) {
    if (policy == NSApplicationActivationPolicyRegular) ++foregroundRequests;
    return originalSetPolicy(app, selector, policy);
}

void installStartupProbe() {
    Method method = class_getInstanceMethod(NSApplication.class, @selector(setActivationPolicy:));
    originalSetPolicy = (decltype(originalSetPolicy))method_setImplementation(method, (IMP)recordPolicy);
}

void checkStartupPolicy(bool primary) {
    if (foregroundRequests != (primary ? 1 : 0) ||
        (primary != (NSApp.activationPolicy == NSApplicationActivationPolicyRegular))) {
        std::fprintf(stderr, "FAIL: %s requested foreground policy %d times (policy=%ld)\n",
            primary ? "primary" : "secondary", foregroundRequests, (long)NSApp.activationPolicy);
        std::exit(1);
    }
    std::fprintf(stderr, "PASS: %s foreground policy\n", primary ? "primary" : "secondary");
}
