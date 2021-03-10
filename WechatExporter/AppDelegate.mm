//
//  AppDelegate.m
//  WechatExporter
//
//  Created by Matthew on 2020/9/29.
//  Copyright © 2020 Matthew. All rights reserved.
//

#import "AppDelegate.h"
#include "Exporter.h"

@interface AppDelegate ()

@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    Exporter::initializeExporter();
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
    // Insert code here to tear down your application
    Exporter::uninitializeExporter();
    
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication {
    return YES;
}

@end
