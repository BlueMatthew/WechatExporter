//
//  AppDelegate.m
//  WechatExporter
//
//  Created by Matthew on 2020/9/29.
//  Copyright © 2020 Matthew. All rights reserved.
//

#import "AppDelegate.h"
#include "Exporter.h"
#import "AppConfiguration.h"

@interface AppDelegate ()

@end

@implementation AppDelegate

- (void)applicationWillBecomeActive:(NSNotification *)notification
{
    
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    Exporter::initializeExporter();
    
    NSMenuItem *menuItem = nil;
    NSMenu *mainMenu = [[NSApplication sharedApplication] mainMenu];
    NSMenuItem *fileMenu = [mainMenu itemAtIndex:1];
    menuItem = [fileMenu.submenu itemAtIndex:0];
    menuItem.state = [AppConfiguration isCheckingUpdateDisabled] ? NSControlStateValueOff : NSControlStateValueOn;
    
    NSMenuItem *formatMenu = [mainMenu itemAtIndex:2];
    menuItem = [formatMenu.submenu itemAtIndex:0];
    BOOL htmlMode = [AppConfiguration isHtmlMode];
    menuItem.state = htmlMode ? NSControlStateValueOn : NSControlStateValueOff;
    menuItem = [formatMenu.submenu itemAtIndex:1];
    menuItem.state = [AppConfiguration isTextMode] ? NSControlStateValueOn : NSControlStateValueOff;
    menuItem = [formatMenu.submenu itemAtIndex:2];
    menuItem.state = [AppConfiguration isPdfMode] ? NSControlStateValueOn : NSControlStateValueOff;
    
    NSMenuItem *optionsMenu = [mainMenu itemAtIndex:3];
    // [optionsMenu.submenu setAutoenablesItems:NO]; // Set in storyboard
    menuItem = [optionsMenu.submenu itemAtIndex:0];
    menuItem.state = [AppConfiguration getDescOrder] ? NSControlStateValueOn : NSControlStateValueOff;
    menuItem = [optionsMenu.submenu itemAtIndex:1];
    menuItem.state = [AppConfiguration getSavingInSession] ? NSControlStateValueOn : NSControlStateValueOff;
    
    menuItem = [optionsMenu.submenu itemAtIndex:3];
    menuItem.state = [AppConfiguration getAsyncLoading] ? NSControlStateValueOn : NSControlStateValueOff;
    menuItem.enabled = htmlMode;
    menuItem = [optionsMenu.submenu itemAtIndex:4];
    menuItem.state = [AppConfiguration getLoadingDataOnScroll] ? NSControlStateValueOn : NSControlStateValueOff;
    menuItem.enabled = htmlMode && [AppConfiguration getAsyncLoading];
    
    menuItem = [optionsMenu.submenu itemAtIndex:6];
    menuItem.state = [AppConfiguration getSupportingFilter] ? NSControlStateValueOn : NSControlStateValueOff;
    menuItem.enabled = htmlMode;
}

- (void)applicationWillTerminate:(NSNotification *)aNotification
{
    // Insert code here to tear down your application
    Exporter::uninitializeExporter();
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication
{
    return YES;
}

- (IBAction)fileMenuItemClick:(NSMenuItem *)sender
{
    if ([sender.identifier isEqualToString:@"updater"])
    {
        [AppConfiguration setCheckingUpdateDisabled:(sender.state == NSControlStateValueOn)];
        sender.state = (sender.state == NSControlStateValueOff) ? NSControlStateValueOn : NSControlStateValueOff;
    }
}


- (IBAction)formatMenuItemClick:(NSMenuItem *)sender
{
    if ([sender.identifier isEqualToString:@"htmlMode"] || [sender.identifier isEqualToString:@"textMode"] || [sender.identifier isEqualToString:@"pdfMode"])
    {
        if (sender.state == NSControlStateValueOff)
        {
            NSInteger outputFormat = [sender.identifier isEqualToString:@"htmlMode"] ? OUTPUT_FORMAT_HTML : ([sender.identifier isEqualToString:@"pdfMode"] ? OUTPUT_FORMAT_PDF : OUTPUT_FORMAT_TEXT);
            [AppConfiguration setOutputFormat:outputFormat];
            BOOL htmlMode = [AppConfiguration isHtmlMode];
            
            // sender.state = NSControlStateValueOn;
            
            NSMenuItem *menuItem = nil;
            NSMenu *mainMenu = [[NSApplication sharedApplication] mainMenu];

            NSMenuItem *formatMenu = [mainMenu itemAtIndex:2];
            for (NSInteger idx = 0; idx < 3; idx++)
            {
                menuItem = [formatMenu.submenu itemAtIndex:idx];
                menuItem.state = [menuItem.identifier isEqualToString:sender.identifier] ? NSControlStateValueOn : NSControlStateValueOff;
            }

            NSMenuItem *optionsMenu = [mainMenu itemAtIndex:3];
            menuItem = [optionsMenu.submenu itemAtIndex:3];
            menuItem.enabled = htmlMode;
            menuItem = [optionsMenu.submenu itemAtIndex:4];
            menuItem.enabled = htmlMode && [AppConfiguration getAsyncLoading];
            
            menuItem = [optionsMenu.submenu itemAtIndex:6];
            menuItem.enabled = htmlMode;
        }
    }
}

- (IBAction)optionsMenuItemClick:(NSMenuItem *)sender
{
    BOOL newValue = sender.state == NSControlStateValueOff;
    sender.state = newValue ? NSControlStateValueOn : NSControlStateValueOff;
    if ([sender.identifier isEqualToString:@"descOrder"])
    {
        [AppConfiguration setDescOrder:newValue];
    }
    else if ([sender.identifier isEqualToString:@"savingInSessionFolder"])
    {
        [AppConfiguration setSavingInSession:newValue];
    }
    else if ([sender.identifier isEqualToString:@"asyncLoading"])
    {
        [AppConfiguration setAsyncLoading:newValue];

        NSMenuItem *menuItem = nil;
        NSMenu *mainMenu = [[NSApplication sharedApplication] mainMenu];
        NSMenuItem *optionsMenu = [mainMenu itemAtIndex:3];
        
        menuItem = [optionsMenu.submenu itemAtIndex:4];
        menuItem.enabled = [AppConfiguration isHtmlMode] && [AppConfiguration getAsyncLoading];
    }
    else if ([sender.identifier isEqualToString:@"loadingOnScroll"])
    {
        [AppConfiguration setLoadingDataOnScroll:newValue];
    }
    else if ([sender.identifier isEqualToString:@"filter"])
    {
        [AppConfiguration setSupportingFilter:newValue];
    }
}

@end
