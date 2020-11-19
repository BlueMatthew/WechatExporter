//
//  AppDelegate.m
//  WechatExporter
//
//  Created by Matthew on 2020/9/29.
//  Copyright © 2020 Matthew. All rights reserved.
//

#import "AppDelegate.h"

@interface AppDelegate ()

@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    // Insert code here to initialize your application
    NSWindow * window = NSApplication.sharedApplication.windows[0];
    CGRect frame = window.frame;
    frame.size.height = NSHeight(window.screen.frame) * NSWidth(frame) / NSWidth(window.screen.frame);
    CGFloat xPos = NSWidth(window.screen.frame)/2 - NSWidth(frame)/2;
    CGFloat yPos = NSHeight(window.screen.frame)/2 - NSHeight(frame)/2;
    [window setFrame:NSMakeRect(xPos, yPos, NSWidth(frame), NSHeight(frame)) display:YES];
}


- (void)applicationWillTerminate:(NSNotification *)aNotification {
    // Insert code here to tear down your application
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication {
    return YES;
}

@end
