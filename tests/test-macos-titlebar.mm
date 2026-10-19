// Native regression checks. Build/run with tests/test-macos-titlebar.sh.
#ifdef __aarch64__
#include <arm_acle.h>
#endif
#include <cassert>
#include <cmath>
#include "../pdf_viewer/macos_specific.mm"

static DocumentTitlebarBackground* background(NSWindow* window) {
    for (NSView* view in [window standardWindowButton:NSWindowCloseButton].superview.subviews) {
        if ([view isKindOfClass:DocumentTitlebarBackground.class]) return (DocumentTitlebarBackground*)view;
    }
    return nil;
}

int main() {
    @autoreleasepool {
        [NSApplication sharedApplication];
        NSWindow* window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 640, 480)
            styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
            backing:NSBackingStoreBuffered defer:NO];
        window.releasedWhenClosed = NO;
        WId id = (WId)window.contentView;
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
        updateDocumentTabs(id, paths, 1, [&](int index) { clicked = index; });
        assert(bar.filename.hidden);
        assert(bar.tabButtons.count == 3);
        assert([bar.tabButtons[0].title isEqualToString:@"1: alpha"]);
        assert([bar.tabButtons[1].title isEqualToString:@"2: 研究"]);
        assert([bar.tabButtons[2].title isEqualToString:@"3: name.with.dots"]);
        assert(bar.tabButtons[1].activeTab && !bar.tabButtons[0].activeTab);
        assert(fabs((NSMinX(bar.tabButtons[0].frame) + NSMaxX(bar.tabButtons[2].frame))/2 - bar.tabScroll.contentSize.width/2) < 0.5);
        [bar.tabButtons[2] performClick:nil];
        assert(clicked == 2);
        for (int i = 0; i < 20; ++i) paths << QString("/dir/long document %1.pdf").arg(i);
        updateDocumentTabs(id, paths, 22, [&](int index) { clicked = index; });
        assert(bar.tabScroll.documentView.frame.size.width > bar.tabScroll.contentSize.width);
        assert(NSContainsRect(bar.tabScroll.documentVisibleRect, bar.tabButtons[22].frame));
        paths.removeAt(1);
        updateDocumentTabs(id, paths, 0, [&](int index) { clicked = index; });
        assert(bar.tabButtons.count == 22);
        assert([bar.tabButtons[1].title isEqualToString:@"2: name.with.dots"]);
        assert(NSContainsRect(bar.tabScroll.documentVisibleRect, bar.tabButtons[0].frame));
        updateDocumentTitlebar(id, 0.1, 0.1, 0.1, true);
        updateDocumentTabs(id, paths, 0, [&](int index) { clicked = index; });
        assert(bar.tabButtons.count == 22);
        assert(bar.tabButtons[0].contentTintColor.redComponent > 0.7);

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
