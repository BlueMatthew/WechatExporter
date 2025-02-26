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
{
    BOOL m_pdfSupported;
}
@end

@implementation AppDelegate

- (void)applicationWillBecomeActive:(NSNotification *)notification
{
    
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    [AppConfiguration upgrade];
    
    Exporter::initializeExporter();
    
    m_pdfSupported = [AppConfiguration IsPdfSupported];
    
    NSMenu *mainMenu = [[NSApplication sharedApplication] mainMenu];
    NSMenuItem *fileMenu = [mainMenu itemAtIndex:1];
    for (NSMenuItem *menuItem in fileMenu.submenu.itemArray)
    {
        if ([@"updater" isEqual:menuItem.identifier])
        {
            menuItem.state = [AppConfiguration isCheckingUpdateDisabled] ? NSControlStateValueOff : NSControlStateValueOn;
        }
        else if ([@"openFolder" isEqual:menuItem.identifier])
        {
            menuItem.state = [AppConfiguration getOpenningFolderAfterExp] ? NSControlStateValueOn : NSControlStateValueOff;
        }
        else if ([@"outputDbgLogs" isEqual:menuItem.identifier])
        {
            menuItem.state = [AppConfiguration outputDebugLogs] ? NSControlStateValueOn : NSControlStateValueOff;
        }
    }
    
    BOOL htmlMode = [AppConfiguration isHtmlMode];
    NSMenuItem *formatMenu = [mainMenu itemAtIndex:2];
    for (NSMenuItem *menuItem in formatMenu.submenu.itemArray)
    {
        if ([@"htmlMode" isEqual:menuItem.identifier])
        {
            menuItem.state = htmlMode ? NSControlStateValueOn : NSControlStateValueOff;
        }
        else if ([@"textMode" isEqual:menuItem.identifier])
        {
            menuItem.state = [AppConfiguration isTextMode] ? NSControlStateValueOn : NSControlStateValueOff;
        }
        else if ([@"pdfMode" isEqual:menuItem.identifier])
        {
            menuItem.state = [AppConfiguration isPdfMode] && m_pdfSupported ? NSControlStateValueOn : NSControlStateValueOff;
        }
    }
    
    NSMenuItem *optionsMenu = [mainMenu itemAtIndex:3];
    for (NSMenuItem *menuItem in optionsMenu.submenu.itemArray)
    {
        if ([@"descOrder" isEqual:menuItem.identifier])
        {
            menuItem.state = [AppConfiguration getDescOrder] ? NSControlStateValueOn : NSControlStateValueOff;
        }
        else if ([@"asyncLoadingOnScroll" isEqual:menuItem.identifier])
        {
            menuItem.state = [AppConfiguration getLoadingDataOnScroll] ? NSControlStateValueOn : NSControlStateValueOff;
            menuItem.enabled = htmlMode;
        }
        else if ([@"asyncPagerNormal" isEqual:menuItem.identifier])
        {
            menuItem.state = [AppConfiguration getNormalPagination] ? NSControlStateValueOn : NSControlStateValueOff;
            menuItem.enabled = htmlMode;
        }
        else if ([@"asyncPagerYear" isEqual:menuItem.identifier])
        {
            menuItem.state = [AppConfiguration getPaginationOnYear] ? NSControlStateValueOn : NSControlStateValueOff;
            menuItem.enabled = htmlMode;
        }
        else if ([@"asyncPagerMonth" isEqual:menuItem.identifier])
        {
            menuItem.state = [AppConfiguration getPaginationOnMonth] ? NSControlStateValueOn : NSControlStateValueOff;
            menuItem.enabled = htmlMode;
        }
        else if ([@"filter" isEqual:menuItem.identifier])
        {
            menuItem.state = [AppConfiguration getSupportingFilter] ? NSControlStateValueOn : NSControlStateValueOff;
            menuItem.enabled = htmlMode;
        }
        else if ([@"incrementalExp" isEqual:menuItem.identifier])
        {
            menuItem.state = [AppConfiguration getIncrementalExporting] ? NSControlStateValueOn : NSControlStateValueOff;
        }
        else if ([@"includingSubscriptions" isEqual:menuItem.identifier])
        {
            menuItem.state = [AppConfiguration includeSubscriptions] ? NSControlStateValueOn : NSControlStateValueOff;
#ifndef NDEBUG
            menuItem.hidden = NO;
#endif
        }
        else
        {
            
        }
    }
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
    if ([sender.identifier isEqualToString:@"openFolder"])
    {
        [AppConfiguration setOpenningFolderAfterExp:(sender.state == NSControlStateValueOff)];
        sender.state = (sender.state == NSControlStateValueOff) ? NSControlStateValueOn : NSControlStateValueOff;
    }
    else if ([sender.identifier isEqualToString:@"outputDbgLogs"])
    {
        [AppConfiguration setOutputDebugLogs:(sender.state == NSControlStateValueOff)];
        sender.state = (sender.state == NSControlStateValueOff) ? NSControlStateValueOn : NSControlStateValueOff;
    }
    else if ([sender.identifier isEqualToString:@"updater"])
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
            
            NSMenuItem *menuItem = nil;
            NSMenu *mainMenu = [[NSApplication sharedApplication] mainMenu];

            NSMenuItem *formatMenu = [mainMenu itemAtIndex:2];
            for (NSInteger idx = 0; idx < 3; idx++)
            {
                menuItem = [formatMenu.submenu itemAtIndex:idx];
                menuItem.state = [menuItem.identifier isEqualToString:sender.identifier] ? NSControlStateValueOn : NSControlStateValueOff;
            }

            NSMenuItem *optionsMenu = [mainMenu itemAtIndex:3];
            for (NSMenuItem *menuItem in optionsMenu.submenu.itemArray)
            {
                if ([menuItem.identifier hasPrefix:@"async"])
                {
                    menuItem.enabled = htmlMode;
                }
                else if ([@"filter" isEqual:menuItem.identifier])
                {
                    menuItem.enabled = htmlMode;
                }
            }
        }
    }
}

- (IBAction)optionsMenuItemClick:(NSMenuItem *)sender
{
    BOOL newValue = sender.state == NSControlStateValueOff;
    // if (![sender.identifier hasPrefix:@"pager"])
    {
        sender.state = newValue ? NSControlStateValueOn : NSControlStateValueOff;
    }
    
    if ([sender.identifier isEqualToString:@"includingSubscriptions"])
    {
        [AppConfiguration setIncludingSubscriptions:newValue];
    }
    else if ([sender.identifier isEqualToString:@"descOrder"])
    {
        [AppConfiguration setDescOrder:newValue];
    }
    else if ([sender.identifier isEqualToString:@"savingInSessionFolder"])
    {
        [AppConfiguration setSavingInSession:newValue];
    }
    /*
    else if ([sender.identifier isEqualToString:@"asyncLoading"])
    {
        [AppConfiguration setAsyncLoading:newValue];

        NSMenu *mainMenu = [[NSApplication sharedApplication] mainMenu];
        NSMenuItem *optionsMenu = [mainMenu itemAtIndex:3];
        
        for (NSMenuItem *menuItem in optionsMenu.submenu.itemArray)
        {
            if ([@"loadingOnScroll" isEqual:menuItem.identifier])
            {
                menuItem.enabled = [AppConfiguration isHtmlMode] && [AppConfiguration getAsyncLoading];
            }
        }
    }
     
    else if ([sender.identifier isEqualToString:@"asyncLoadingOnScroll"])
    {
        [AppConfiguration setLoadingDataOnScroll:newValue];
    }
     */
    else if ([sender.identifier isEqualToString:@"filter"])
    {
        [AppConfiguration setSupportingFilter:newValue];
    }
    else if ([sender.identifier isEqualToString:@"incrementalExp"])
    {
        [AppConfiguration setIncrementalExporting:newValue];
    }
    else if ([sender.identifier hasPrefix:@"async"])
    {
        if (newValue)
        {
            for (NSMenuItem *menuItem in sender.parentItem.submenu.itemArray)
            {
                if ((menuItem != sender) && [menuItem.identifier hasPrefix:@"async"])
                {
                    if (menuItem.state == NSControlStateValueOn)
                    {
                        menuItem.state = NSControlStateValueOff;
                    }
                }
            }
            
            if ([sender.identifier isEqualToString:@"asyncLoadingOnScroll"])
            {
                [AppConfiguration setLoadingDataOnScroll];
            }
            else if ([sender.identifier isEqualToString:@"asyncPagerNormal"])
            {
                [AppConfiguration setNormalPagination];
            }
            else if ([sender.identifier isEqualToString:@"asyncPagerYear"])
            {
                [AppConfiguration setPaginationOnYear];
            }
            else if ([sender.identifier isEqualToString:@"asyncPagerMonth"])
            {
                [AppConfiguration setPaginationOnMonth];
            }
        }
        else
        {
            [AppConfiguration setSyncLoading];
        }
    }
}


@end
