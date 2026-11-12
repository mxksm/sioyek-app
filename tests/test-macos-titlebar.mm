// Native regression checks. Build/run with tests/test-macos-titlebar.sh.
#ifdef __aarch64__
#include <arm_acle.h>
#endif
#include <cassert>
#include <cmath>
#include "../pdf_viewer/macos_specific.mm"

// Exercise Retina sizing even when the test runs on a 1x display.
@interface RetinaTitlebarTestWindow : NSWindow
@end
@implementation RetinaTitlebarTestWindow
- (CGFloat)backingScaleFactor { return 2.0; }
@end

static DocumentTitlebarBackground* background(NSWindow* window) {
    for (NSView* view in [window standardWindowButton:NSWindowCloseButton].superview.subviews) {
        if ([view isKindOfClass:DocumentTitlebarBackground.class]) return (DocumentTitlebarBackground*)view;
    }
    return nil;
}

int main() {
    @autoreleasepool {
        [NSApplication sharedApplication];
        RetinaTitlebarTestWindow* retinaWindow = [[RetinaTitlebarTestWindow alloc]
            initWithContentRect:NSMakeRect(0, 0, 1139, 613)
            styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
            backing:NSBackingStoreBuffered defer:NO];
        retinaWindow.releasedWhenClosed = NO;
        updateDocumentTitlebar((WId)retinaWindow.contentView, 1, 1, 1, true);
        // Qt stores window geometry in integer points, while AppKit can lay out
        // a half-point title bar. Those sizes must agree before Qt will flush GL.
        assert(documentTitlebarHeight(retinaWindow) == std::ceil(documentTitlebarHeight(retinaWindow)));
        assert(retinaWindow.contentView.frame.size.height == std::floor(retinaWindow.contentView.frame.size.height));
        [retinaWindow close];
        [retinaWindow release];
        NSWindow* window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 640, 480)
            styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
            backing:NSBackingStoreBuffered defer:NO];
        window.releasedWhenClosed = NO;
        WId id = (WId)window.contentView;
        auto mouse = [&](NSEventType type) {
            return [NSEvent mouseEventWithType:type location:NSMakePoint(100,100) modifierFlags:0
                timestamp:12.345 windowNumber:window.windowNumber context:nil eventNumber:1 clickCount:1 pressure:1];
        };
        assert(activationClickTimestamp(window, mouse(NSEventTypeLeftMouseDown)) == 12345);
        assert(activationClickTimestamp(window, mouse(NSEventTypeRightMouseDown)) == 12345);
        assert(activationClickTimestamp(window, mouse(NSEventTypeLeftMouseUp)) == 0);
        assert(activationClickTimestamp(nil, mouse(NSEventTypeLeftMouseDown)) == 0);
        assert(activationClickTimestamp(window, nil) == 0);
        int activations = 0;
        setDocumentActivationClickHandler(id, [&](unsigned long) { ++activations; });
        [NSNotificationCenter.defaultCenter postNotificationName:NSWindowDidBecomeKeyNotification object:window];
        assert(activations == 1);
        setDocumentActivationClickHandler(id, {});
        [NSNotificationCenter.defaultCenter postNotificationName:NSWindowDidBecomeKeyNotification object:window];
        assert(activations == 1 && documentActivationObservers.empty());
        // Exercise Cocoa events rather than synthetic Qt KeyPress events: real
        // Control-Tab can be consumed by AppKit before Qt sees that event.
        std::vector<int> cycles;
        setDocumentTabCycleHandler(id, [&](int direction) { cycles.push_back(direction); });
        assert(documentTabCycleMonitor != nil);
        auto key = [&](NSEventType type, NSEventModifierFlags flags, unsigned short code = 48, bool repeat = false) {
            return [NSEvent keyEventWithType:type location:NSZeroPoint modifierFlags:flags
                timestamp:0 windowNumber:window.windowNumber context:nil characters:@"\t"
                charactersIgnoringModifiers:@"\t" isARepeat:repeat keyCode:code];
        };
        assert(handleDocumentTabCycleEvent(key(NSEventTypeKeyDown, NSEventModifierFlagControl)) == nil);
        assert(handleDocumentTabCycleEvent(key(NSEventTypeKeyUp, NSEventModifierFlagControl)) == nil);
        assert(handleDocumentTabCycleEvent(key(NSEventTypeKeyDown, NSEventModifierFlagControl, 48, true)) == nil);
        assert(handleDocumentTabCycleEvent(key(NSEventTypeKeyDown,
            NSEventModifierFlagControl | NSEventModifierFlagShift)) == nil);
        handleDocumentTabCycleEvent(key(NSEventTypeFlagsChanged, 0, 59));
        assert((cycles == std::vector<int>{1, 1, -1, 0}));
        for (auto flags : std::initializer_list<NSEventModifierFlags>{NSEventModifierFlagCommand, NSEventModifierFlagShift,
                          NSEventModifierFlagControl | NSEventModifierFlagOption}) {
            auto event = key(NSEventTypeKeyDown, flags);
            assert(handleDocumentTabCycleEvent(event) == event);
        }
        assert(cycles.size() == 4);
        setDocumentTabCycleHandler(id, {});
        assert(documentTabCycleMonitor == nil);
        auto event = key(NSEventTypeKeyDown, NSEventModifierFlagControl);
        assert(activationClickTimestamp(window, event) == 0);
        assert(handleDocumentTabCycleEvent(event) == event);
        NSLog(@"PASS: native Control-Tab, repeat, reverse, release and unrelated shortcuts");
        window.title = @"first.pdf";
        updateDocumentTitlebar(id, 40.0/255, 44.0/255, 52.0/255, true);
        DocumentTitlebarBackground* bar = background(window);
        assert(bar);
        assert(fabs(bar.bounds.size.height - documentTitlebarHeight(window)) < 0.01);
        NSRect contentRect = [window contentRectForFrameRect:window.frame];
        assert(fabs(window.frame.size.height - contentRect.size.height - documentTitlebarHeight(window)) < 0.01);
        [bar layoutSubtreeIfNeeded];
        assert([bar.filename.stringValue isEqualToString:@"first.pdf"]);
        assert(fabs(NSMidX(bar.filename.frame) - NSMidX(bar.bounds)) < 0.5);
        assert(window.titleVisibility == NSWindowTitleHidden);
        assert([window.colorSpace isEqual:NSColorSpace.sRGBColorSpace]);
        assert([bar.fillColor.colorSpace isEqual:NSColorSpace.sRGBColorSpace]);
        assert(fabs(bar.fillColor.redComponent - 40.0/255) < 1e-6);
        for (NSNumber* kind in @[@(NSWindowCloseButton), @(NSWindowMiniaturizeButton), @(NSWindowZoomButton)])
            assert([window standardWindowButton:(NSWindowButton)kind.integerValue].hidden);

        window.title = @"/some/folder/second.pdf";
        updateDocumentTitlebar(id, 1, 1, 1, true);
        assert(background(window) == bar);
        assert([bar.filename.stringValue isEqualToString:@"second.pdf"]);
        assert(bar.fillColor.redComponent == 1);
        [window setContentSize:NSMakeSize(900, 480)];
        [bar layoutSubtreeIfNeeded];
        assert(fabs(NSMidX(bar.filename.frame) - NSMidX(bar.bounds)) < 0.5);

        int clicked = -1;
        QStringList paths = {"/a/alpha.pdf", "/b/研究.PDF", "/c/name.with.dots.pdf"};
        const float panelColor[] = {0.2f, 0.24f, 0.3f};
        const float textColor[] = {0.9f, 0.85f, 0.8f};
        updateDocumentTabs(id, paths, 1, [&](int index) { clicked = index; }, panelColor, textColor);
        assert(fabs(bar.tabScroll.fillColor.redComponent - panelColor[0]) < 1e-6);
        assert(fabs(bar.tabButtons[1].contentTintColor.redComponent - textColor[0]) < 1e-6);
        assert(fabs(NSMidX(bar.tabScroll.frame) - NSMidX(bar.bounds)) <= 0.5);
        assert(bar.tabScroll.frame.origin.x >= 8 && bar.tabScroll.frame.origin.y == 4);
        assert(bar.tabScroll.frame.size.height == bar.bounds.size.height - 8);
        assert(bar.tabScroll.frame.size.width < bar.bounds.size.width - 16);
        assert(NSMaxY(bar.tabButtons[1].frame) <= bar.tabScroll.bounds.size.height);
        assert(bar.filename.hidden);
        assert(bar.tabButtons.count == 3);
        assert([bar.tabButtons[0].title isEqualToString:@"1: alpha"]);
        assert([bar.tabButtons[1].title isEqualToString:@"2: 研究"]);
        assert([bar.tabButtons[2].title isEqualToString:@"3: name.with.dots"]);
        assert(bar.tabButtons[1].activeTab && !bar.tabButtons[0].activeTab);
        assert(fabs((NSMinX(bar.tabButtons[0].frame) + NSMaxX(bar.tabButtons[2].frame))/2 - bar.tabScroll.bounds.size.width/2) < 0.5);
        auto snapshot = [&](NSString* path) {
            [bar layoutSubtreeIfNeeded];
            NSBitmapImageRep* bitmap = [bar bitmapImageRepForCachingDisplayInRect:bar.bounds];
            [bar cacheDisplayInRect:bar.bounds toBitmapImageRep:bitmap];
            [[bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:path atomically:YES];
        };
        snapshot(@"/tmp/sioyek-floating-tabs-dark.png");
        [bar.tabButtons[2] performClick:nil];
        assert(clicked == 2);
        for (int i = 0; i < 20; ++i) paths << QString("/dir/long document %1.pdf").arg(i);
        updateDocumentTabs(id, paths, 22, [&](int index) { clicked = index; });
        assert(bar.tabScroll.documentView.frame.size.width > bar.tabScroll.bounds.size.width);
        assert(NSContainsRect(bar.tabScroll.bounds, bar.tabButtons[22].frame));
        paths.removeAt(1);
        updateDocumentTabs(id, paths, 0, [&](int index) { clicked = index; });
        assert(bar.tabButtons.count == 22);
        assert([bar.tabButtons[1].title isEqualToString:@"2: name.with.dots"]);
        assert(NSContainsRect(bar.tabScroll.bounds, bar.tabButtons[0].frame));
        updateDocumentTitlebar(id, 0.1, 0.1, 0.1, true);
        updateDocumentTabs(id, paths, 0, [&](int index) { clicked = index; });
        assert(bar.tabButtons.count == 22);
        assert(bar.tabButtons[0].contentTintColor.redComponent > 0.7);

        // Empty scratchpad has no floating panel. A single tab is compact again
        // after overflow, and a theme change updates both panel and label.
        updateDocumentTabs(id, {}, -1, {});
        assert(bar.tabScroll.hidden && bar.tabButtons.count == 0);
        const float lightPanel[] = {0.8f, 0.82f, 0.85f};
        const float darkText[] = {0.2f, 0.22f, 0.25f};
        updateDocumentTabs(id, {"/a/alpha.pdf"}, 0, {}, lightPanel, darkText);
        assert(!bar.tabScroll.hidden && bar.tabScroll.bounds.origin.x == 0);
        assert(bar.tabScroll.frame.size.width == bar.tabButtons[0].frame.size.width);
        assert(fabs(bar.tabScroll.fillColor.blueComponent-lightPanel[2]) < 1e-6);
        assert(fabs(bar.tabButtons[0].contentTintColor.blueComponent-darkText[2]) < 1e-6);
        updateDocumentTitlebar(id, 0.94, 0.95, 0.97, true);
        snapshot(@"/tmp/sioyek-floating-tabs-light.png");

        updateDocumentTitlebar(id, 0, 0, 0, false);
        assert(background(window) == nil);
        window.styleMask = NSWindowStyleMaskBorderless;
        updateDocumentTitlebar(id, 0, 0, 0, false);
        assert([window.colorSpace isEqual:NSColorSpace.sRGBColorSpace]);
        window.styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable;
        updateDocumentTitlebar(id, 0.1, 0.1, 0.1, true);
        assert(background(window));
        assert(fabs(background(window).bounds.size.height - documentTitlebarHeight(window)) < 0.01);
        [window close];
        [window release];
        NSLog(@"PASS: matching row height and frame margins, centered title, hidden controls, sRGB, resize, rename, numbered/clickable/scrolling tabs and hide/show");
    }
}
