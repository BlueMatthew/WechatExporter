//
//  ViewController.h
//  WechatExporter
//
//  Created by Matthew on 2020/9/29.
//  Copyright © 2020 Matthew. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface ViewController : NSViewController

@property (weak) IBOutlet NSTextField *lblITunes;
@property (weak) IBOutlet NSButton *btnExport;
@property (weak) IBOutlet NSButton *btnCancel;
@property (weak) IBOutlet NSTextField *txtboxOutput;
@property (weak) IBOutlet NSPopUpButton *popupBackup;
@property (weak) IBOutlet NSPopUpButton *popupUsers;
@property (weak) IBOutlet NSButton *btnToggleAll;
@property (weak) IBOutlet NSScrollView *sclSessions;
@property (weak) IBOutlet NSTableView *tblSessions;


@property (weak) IBOutlet NSButton *btnBackup;
@property (weak) IBOutlet NSButton *btnOutput;

@property (weak) IBOutlet NSProgressIndicator *progressBar;
@property (weak) IBOutlet NSScrollView *sclViewLogs;
@property (unsafe_unretained) IBOutlet NSTextView *txtViewLogs;
@property (weak) IBOutlet NSButton *chkboxDesc;
@property (weak) IBOutlet NSButton *chkboxNoAudio;
@property (weak) IBOutlet NSButton *chkboxSaveFilesInSessionFolder;


- (void)writeLog:(NSString *)log;
- (void)onStart;
- (void)onComplete:(BOOL)cancelled;

@end

