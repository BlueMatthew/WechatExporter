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
    CGFloat xPos = NSWidth(window.screen.frame)/2 - NSWidth(window.frame)/2;
    CGFloat yPos = NSHeight(window.screen.frame)/2 - NSHeight(window.frame)/2;
    [window setFrame:NSMakeRect(xPos, yPos, NSWidth(window.frame), NSHeight(window.frame)) display:YES];
}


- (void)applicationWillTerminate:(NSNotification *)aNotification {
    // Insert code here to tear down your application
}


@end
