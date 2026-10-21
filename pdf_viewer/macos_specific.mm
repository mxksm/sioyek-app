#include <AppKit/AppKit.h>
#include <QWidget>
#import <Cocoa/Cocoa.h>
#import <objc/message.h>
#include <cmath>
#include <QStringList>
#include <functional>

extern "C" void showLookupForString(WId winId, const char* text, double x, double y) {
    if (winId == 0 || text == nullptr) return;

    NSView* view = (NSView*)winId;
    NSString* nsText = [NSString stringWithUTF8String:text];

    NSPoint point = NSMakePoint(x, y);

    [view showDefinitionForAttributedString:[[NSAttributedString alloc] initWithString:nsText]
                                    atPoint:point];
}

extern "C" void changeTitlebarColor(WId winId, double red, double green, double blue, double alpha){
    if (winId == 0) return;
    NSView* view = (NSView*)winId;
    NSWindow* window = [view window];
    window.titlebarAppearsTransparent = YES;
    window.backgroundColor = [NSColor colorWithSRGBRed:red green:green blue:blue alpha: alpha];
}

// Match WezTerm's independent 12pt tab font / 28pt fancy tab bar.
// Keep whole logical points: fractional content heights are rounded differently
// by Qt's geometry and expose events and can prevent OpenGL frame flushing.
static CGFloat documentTitlebarHeight(NSWindow*) {
    return 28.0;
}

static void applyDocumentTitlebarHeight(NSWindow* window) {
    NSView* frame = window.contentView.superview;
    NSView* titlebar = [window standardWindowButton:NSWindowCloseButton].superview;
    CGFloat height = documentTitlebarHeight(window);
    // AppKit exposes no public API to shrink its native title bar. This guarded
    // frame hook lets AppKit lay out the content and report the correct margins
    // to Qt, preserving PDF hit testing. Fall back to the native size if absent.
    SEL setter = NSSelectorFromString(@"setCustomTitlebarHeight:");
    if (titlebar && [frame respondsToSelector:setter] &&
        std::abs(titlebar.bounds.size.height - height) > 0.01) {
        ((void (*)(id, SEL, CGFloat))objc_msgSend)(frame, setter, height);
        [frame layoutSubtreeIfNeeded];
    }
}

static void dragDocumentWindow(NSWindow* window, NSEvent* event) {
    if (event.clickCount == 2) [window zoom:nil];
    else [window performWindowDragWithEvent:event];
}

@interface DocumentTabButton : NSButton
@property(nonatomic) BOOL activeTab;
@end
@implementation DocumentTabButton
- (BOOL)acceptsFirstResponder { return NO; }
- (BOOL)mouseDownCanMoveWindow { return NO; }
@end

@interface DocumentTabStrip : NSView
@end
@implementation DocumentTabStrip
- (void)mouseDown:(NSEvent*)event { dragDocumentWindow(self.window, event); }
@end

// Keep scrolling local to this small viewport. NSScrollView participates in
// AppKit's automatic content-inset/layout machinery, which is inappropriate
// inside a native title bar above Qt's OpenGL surface.
@interface DocumentTabViewport : NSView
@property(nonatomic, retain) NSView* documentView;
- (void)scrollToX:(CGFloat)x;
- (void)revealRect:(NSRect)rect;
@end
@implementation DocumentTabViewport
- (BOOL)clipsToBounds { return YES; }
- (NSView*)hitTest:(NSPoint)point {
    if (!NSPointInRect([self convertPoint:point fromView:self.superview], self.bounds)) return nil;
    return [super hitTest:point];
}
- (void)setDocumentView:(NSView*)view {
    if (_documentView == view) return;
    [_documentView removeFromSuperview];
    [_documentView release];
    _documentView = [view retain];
    if (view) [self addSubview:view];
}
- (void)mouseDown:(NSEvent*)event { dragDocumentWindow(self.window, event); }
- (void)scrollToX:(CGFloat)x {
    CGFloat limit = MAX(0, self.documentView.frame.size.width - self.bounds.size.width);
    NSPoint origin = NSMakePoint(MAX(0, MIN(limit, x)), 0);
    if (!NSEqualPoints(origin, self.bounds.origin)) {
        [self setBoundsOrigin:origin];
        self.needsDisplay = YES;
    }
}
- (void)revealRect:(NSRect)rect {
    CGFloat x = self.bounds.origin.x;
    if (NSMinX(rect) < x) x = NSMinX(rect);
    else if (NSMaxX(rect) > NSMaxX(self.bounds)) x = NSMaxX(rect) - self.bounds.size.width;
    [self scrollToX:x];
}
- (void)scrollWheel:(NSEvent*)event {
    CGFloat delta = fabs(event.scrollingDeltaX) > fabs(event.scrollingDeltaY)
        ? event.scrollingDeltaX : event.scrollingDeltaY;
    [self scrollToX:self.bounds.origin.x - delta * (event.hasPreciseScrollingDeltas ? 1 : 12)];
}
- (void)dealloc {
    [_documentView release];
    [super dealloc];
}
@end

static void setDocumentViewFrame(NSView* view, NSRect frame) {
    if (view && !NSEqualRects(view.frame, frame)) view.frame = frame;
}

@interface DocumentTitlebarBackground : NSView {
@public
    std::function<void(int)> selectTab;
}
@property(nonatomic, retain) NSColor* fillColor;
@property(nonatomic, retain) NSColor* edgeColor;
@property(nonatomic, retain) NSTextField* filename;
@property(nonatomic, retain) DocumentTabViewport* tabScroll;
@property(nonatomic, retain) NSMutableArray<DocumentTabButton*>* tabButtons;
@property(nonatomic) NSInteger activeTab;
@end
@implementation DocumentTitlebarBackground
- (void)mouseDown:(NSEvent*)event { dragDocumentWindow(self.window, event); }
- (void)tabClicked:(NSButton*)sender {
    auto callback = selectTab;
    if (callback) callback((int)sender.tag);
}
- (void)viewDidChangeBackingProperties {
    [super viewDidChangeBackingProperties];
    applyDocumentTitlebarHeight(self.window);
}
- (void)drawRect:(NSRect)dirtyRect {
    [self.fillColor setFill];
    NSRectFill(self.bounds);
    [self.edgeColor setFill];
    NSRect edge = self.bounds;
    edge.size.height = 1.0;
    if (self.isFlipped) edge.origin.y = NSMaxY(self.bounds) - 1.0;
    NSRectFill(edge);
}
- (void)layout {
    [super layout];
    CGFloat height = self.filename.intrinsicContentSize.height;
    setDocumentViewFrame(self.filename, NSMakeRect(12, (self.bounds.size.height - height) / 2.0,
        MAX(0, self.bounds.size.width - 24), height));
    setDocumentViewFrame(self.tabScroll, NSInsetRect(self.bounds, 6, 0));
    CGFloat totalWidth = 0;
    for (DocumentTabButton* button in self.tabButtons) totalWidth += button.frame.size.width;
    CGFloat width = MAX(totalWidth, self.tabScroll.bounds.size.width);
    setDocumentViewFrame(self.tabScroll.documentView, NSMakeRect(0, 0, width, self.bounds.size.height));
    CGFloat x = MAX(0, (width - totalWidth) / 2);
    for (DocumentTabButton* button in self.tabButtons) {
        setDocumentViewFrame(button, NSMakeRect(x, 1, button.frame.size.width, self.bounds.size.height - 1));
        x += button.frame.size.width;
    }
    [self.tabScroll scrollToX:self.tabScroll.bounds.origin.x];
    for (NSNumber* kind in @[@(NSWindowCloseButton), @(NSWindowMiniaturizeButton), @(NSWindowZoomButton)]) {
        [[self.window standardWindowButton:(NSWindowButton)kind.integerValue] setHidden:YES];
    }
}
- (void)dealloc {
    [_tabButtons release];
    [_tabScroll release];
    [_filename release];
    [_fillColor release];
    [_edgeColor release];
    [super dealloc];
}
@end

extern "C" void updateDocumentTitlebar(WId winId, double red, double green, double blue, bool visible) {
    if (winId == 0) return;
    NSWindow* window = [(NSView*)winId window];
    if (!window) return;
    // Apply this even with st hidden: the PDF surface must stay in sRGB.
    window.colorSpace = NSColorSpace.sRGBColorSpace;
    if (!(window.styleMask & NSWindowStyleMaskTitled)) return;
    NSView* titlebar = [window standardWindowButton:NSWindowCloseButton].superview;
    if (!titlebar) return;
    DocumentTitlebarBackground* background = nil;
    for (NSView* child in titlebar.subviews) {
        if ([child isKindOfClass:[DocumentTitlebarBackground class]]) {
            background = (DocumentTitlebarBackground*)child;
            break;
        }
    }
    if (!visible) {
        [background removeFromSuperview];
        return;
    }
    applyDocumentTitlebarHeight(window);
    changeTitlebarColor(winId, red, green, blue, 1.0);
    bool dark = 0.2126 * red + 0.7152 * green + 0.0722 * blue < 0.5;
    window.appearance = [NSAppearance appearanceNamed:dark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua];
    window.titlebarSeparatorStyle = NSTitlebarSeparatorStyleNone;
    window.titleVisibility = NSWindowTitleHidden;
    for (NSNumber* kind in @[@(NSWindowCloseButton), @(NSWindowMiniaturizeButton), @(NSWindowZoomButton)]) {
        [[window standardWindowButton:(NSWindowButton)kind.integerValue] setHidden:YES];
    }
    if (!background) {
        background = [[DocumentTitlebarBackground alloc] initWithFrame:titlebar.bounds];
        background.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        background.filename = [NSTextField labelWithString:@""];
        background.filename.font = [NSFont systemFontOfSize:12 weight:NSFontWeightMedium];
        background.filename.alignment = NSTextAlignmentCenter;
        background.filename.lineBreakMode = NSLineBreakByTruncatingMiddle;
        background.filename.maximumNumberOfLines = 1;
        [background addSubview:background.filename];
        [titlebar addSubview:background positioned:NSWindowAbove relativeTo:nil];
        [background release];
    }
    background.filename.stringValue = window.title.lastPathComponent ?: @"";
    background.filename.textColor = [NSColor colorWithSRGBRed:dark ? 0.78 : 0.25
        green:dark ? 0.78 : 0.25 blue:dark ? 0.80 : 0.27 alpha:1.0];
    [background setNeedsLayout:YES];
    // Use explicit sRGB for both native chrome and the OpenGL surface.
    background.fillColor = [NSColor colorWithSRGBRed:red green:green blue:blue alpha:1.0];
    double foreground = dark ? 1.0 : 0.0;
    background.edgeColor = [NSColor colorWithSRGBRed:red * 0.84 + foreground * 0.16
        green:green * 0.84 + foreground * 0.16 blue:blue * 0.84 + foreground * 0.16 alpha:1.0];
    [background setNeedsDisplay:YES];
}

// QStringList keeps filenames Unicode-safe; the callback is owned by the view.
void updateDocumentTabs(WId winId, const QStringList& paths, int active, std::function<void(int)> select) {
    if (!winId) return;
    NSWindow* window = [(NSView*)winId window];
    if (!(window.styleMask & NSWindowStyleMaskTitled)) return;
    DocumentTitlebarBackground* bar = nil;
    for (NSView* child in [window standardWindowButton:NSWindowCloseButton].superview.subviews) {
        if ([child isKindOfClass:DocumentTitlebarBackground.class]) { bar = (DocumentTitlebarBackground*)child; break; }
    }
    if (!bar) return;
    bar->selectTab = std::move(select);
    if (!bar.tabScroll) {
        bar.tabScroll = [[[DocumentTabViewport alloc] initWithFrame:bar.bounds] autorelease];
        bar.tabScroll.documentView = [[[DocumentTabStrip alloc] initWithFrame:bar.bounds] autorelease];
        bar.tabButtons = [NSMutableArray array];
        bar.activeTab = -1;
        [bar addSubview:bar.tabScroll];
    }
    BOOL selectionChanged = bar.activeTab != active || bar.tabButtons.count != (NSUInteger)paths.size();
    bar.activeTab = active;
    bar.filename.hidden = !paths.empty();
    bar.tabScroll.hidden = paths.empty();
    while (bar.tabButtons.count > (NSUInteger)paths.size()) {
        [bar.tabButtons.lastObject removeFromSuperview];
        [bar.tabButtons removeLastObject];
    }
    for (int i = 0; i < paths.size(); ++i) {
        DocumentTabButton* button;
        if (i < (int)bar.tabButtons.count) button = bar.tabButtons[i];
        else {
            button = [[[DocumentTabButton alloc] initWithFrame:NSZeroRect] autorelease];
            button.bordered = NO;
            button.focusRingType = NSFocusRingTypeNone;
            button.buttonType = NSButtonTypeMomentaryChange;
            button.target = bar;
            button.action = @selector(tabClicked:);
            button.cell.lineBreakMode = NSLineBreakByTruncatingMiddle;
            [bar.tabButtons addObject:button];
            [bar.tabScroll.documentView addSubview:button];
        }
        NSString* path = [NSString stringWithUTF8String:paths[i].toUtf8().constData()];
        NSString* name = path.lastPathComponent;
        if ([name.pathExtension caseInsensitiveCompare:@"pdf"] == NSOrderedSame) name = name.stringByDeletingPathExtension;
        NSString* label = [NSString stringWithFormat:@"%d: %@", i + 1, name];
        button.tag = i;
        button.toolTip = path;
        button.activeTab = i == active;
        button.font = [NSFont monospacedSystemFontOfSize:12 weight:button.activeTab ? NSFontWeightSemibold : NSFontWeightRegular];
        NSColor* color = [bar.filename.textColor colorWithAlphaComponent:button.activeTab ? 1 : 0.65];
        button.contentTintColor = color;
        NSDictionary* attributes = @{NSFontAttributeName:button.font, NSForegroundColorAttributeName:color};
        button.attributedTitle = [[[NSAttributedString alloc] initWithString:label attributes:attributes] autorelease];
        [button setAccessibilityLabel:label];
        [button setAccessibilityValue:button.activeTab ? @"Selected" : @""];
        button.frame = NSMakeRect(0, 0, std::ceil(MIN(240, MAX(70, [label sizeWithAttributes:attributes].width + 24))), bar.bounds.size.height);
        button.needsDisplay = YES;
    }
    bar.needsLayout = YES;
    [bar layoutSubtreeIfNeeded];
    if (selectionChanged && active >= 0 && active < (int)bar.tabButtons.count)
        [bar.tabScroll revealRect:bar.tabButtons[active].frame];
}

@interface DraggableTitleView : NSView
@end

@implementation DraggableTitleView

// Handle mouse click events
- (void)mouseDown:(NSEvent *)event {
    // double-click to zoom
    if ([event clickCount] == 2) {
        [self.window zoom:nil];
    } else {
        // drag Window
        [self.window performWindowDragWithEvent:event];
    }
}

- (void)updateTrackingAreas {
    [self initTrackingArea];
}

-(void) initTrackingArea {
    NSTrackingAreaOptions options = (NSTrackingActiveAlways | NSTrackingInVisibleRect |
            NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved);

    NSTrackingArea *area = [[NSTrackingArea alloc] initWithRect:[self bounds]
        options:options
        owner:self
        userInfo:nil];

    [self addTrackingArea:area];
}
-(void)mouseEntered:(NSEvent *)event {
    if((self.window.styleMask & NSWindowStyleMaskFullScreen) == 0) {
        [[self.window standardWindowButton: NSWindowCloseButton] setHidden:NO];
        [[self.window standardWindowButton: NSWindowMiniaturizeButton] setHidden:NO];
        [[self.window standardWindowButton: NSWindowZoomButton] setHidden:NO];
    }
}

-(void)mouseExited:(NSEvent *)event {
    if((self.window.styleMask & NSWindowStyleMaskFullScreen) == 0) {
        [[self.window standardWindowButton: NSWindowCloseButton] setHidden:YES];
        [[self.window standardWindowButton: NSWindowMiniaturizeButton] setHidden:YES];
        [[self.window standardWindowButton: NSWindowZoomButton] setHidden:YES];
    }
}

@end

extern "C" void hideWindowTitleBar(WId winId) {
    if (winId == 0) return;

    NSView* nativeView = reinterpret_cast<NSView*>(winId);
    NSWindow* nativeWindow = [nativeView window];

    if(nativeWindow.titleVisibility == NSWindowTitleHidden){
        return;
    }

    [[nativeWindow standardWindowButton: NSWindowCloseButton] setHidden:YES];
    [[nativeWindow standardWindowButton: NSWindowMiniaturizeButton] setHidden:YES];
    [[nativeWindow standardWindowButton: NSWindowZoomButton] setHidden:YES];
    NSRect contentViewBounds = nativeWindow.contentView.bounds;

    DraggableTitleView *titleBarView = [[DraggableTitleView alloc] initWithFrame:NSMakeRect(0, 0, contentViewBounds.size.width, 22)];
    titleBarView.autoresizingMask = NSViewWidthSizable;
    titleBarView.wantsLayer = YES;
    titleBarView.layer.backgroundColor = [[NSColor clearColor] CGColor];

    [nativeWindow.contentView addSubview:titleBarView];

    [nativeWindow setTitleVisibility:NSWindowTitleHidden];
    [nativeWindow setStyleMask:[nativeWindow styleMask] | NSWindowStyleMaskFullSizeContentView];
    [nativeWindow setTitlebarAppearsTransparent:YES];
}
