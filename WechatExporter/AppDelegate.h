//
//  AppDelegate.h
//  WechatExporter
//
//  Created by Matthew on 2020/9/29.
//  Copyright © 2020 Matthew. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface AppDelegate : NSObject <NSApplicationDelegate>
- (IBAction)fileMenuItemClick:(NSMenuItem *)sender;
- (IBAction)formatMenuItemClick:(NSMenuItem *)sender;
- (IBAction)optionsMenuItemClick:(NSMenuItem *)sender;

@end

