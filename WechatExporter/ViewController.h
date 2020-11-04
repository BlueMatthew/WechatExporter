//
//  ViewController.h
//  WechatExporter
//
//  Created by Matthew on 2020/9/29.
//  Copyright © 2020 Matthew. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface ViewController : NSViewController

@property (weak) IBOutlet NSButton *btnExport;
@property (weak) IBOutlet NSTextField *txtboxOutput;
@property (weak) IBOutlet NSComboBox *cmbboxBackup;

@property (weak) IBOutlet NSButton *btnBackup;
@property (weak) IBOutlet NSButton *btnOutput;

@property (weak) IBOutlet NSTextView *txtViewLogs;
@property (weak) IBOutlet NSScrollView *sclViewLogs;
@property (weak) IBOutlet NSProgressIndicator *progressBar;

@end

