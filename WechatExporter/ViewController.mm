//
//  ViewController.m
//  WechatExporter
//
//  Created by Matthew on 2020/9/29.
//  Copyright © 2020 Matthew. All rights reserved.
//

#import "ViewController.h"
#include "ITunesParser.h"

#import "BackupItem.h"

#include "LoggerImpl.h"
#include "ShellImpl.h"
#include "RawMessage.h"
#include "Utils.h"
#include "Exporter.h"

#include <fstream>

@interface ViewController()
{
    
}
@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    [self.view.window center];
    
    [self.btnBackup setAction:@selector(btnBackupClicked:)];
    [self.btnOutput setAction:@selector(btnOutputClicked:)];
    [self.btnExport setAction:@selector(btnExportClicked:)];
    
    // btnBackupPicker.all
    // Do any additional setup after loading the view.
}


- (void)setRepresentedObject:(id)representedObject {
    [super setRepresentedObject:representedObject];

    // Update the view, if already loaded.
}

- (void)btnBackupClicked:(id)sender
{
    
    // LoginInfo2Parser li(NULL);
    
    
    
    std::string json;
    RawMessage msg;
     
    std::string pbFile;
    
    pbFile = "/Users/matthew/Documents/Programs/Github/WechatExporter/WechatExporter/bb.bin";
    pbFile = "/Users/matthew/Documents/reebes/iPhoneSE/com.tencent.xin/Documents/LoginInfo2.dat";
    
    
    pbFile = "/Users/matthew/Documents/reebes/iPhoneSE/com.tencent.xin/Documents/MMappedKV/mmsetting.archive.wxid_2gix66ls0aq611";
    
    std::vector<unsigned char> data;
    readFile(pbFile, data);
    
    const char *p = reinterpret_cast<const char*>(&data[0]);
    const char *limit = p + data.size();
    uint32_t value = 0;
    int idx  = 0;
    // while (p != NULL)
    {
        value = GetLittleEndianInteger(&data[0]);
        
        
        std::string newpath = pbFile + "." + std::to_string(idx++);
        std::ofstream myFile (newpath, std::ios::out | std::ios::binary);
        myFile.write (p + 8, value - 4);
        myFile.close();
        p += value;
    }

    
    
    
    if (msg.mergeFile(pbFile))
    {
        msg.parse("1", json);
        // msg.parse("4", json);
        int len = json.size();
        
        // std::vector<unsigned char> buffer;
        // buffer.resize(len);
        
        uint32_t aa = 0;
        const char* buf = json.c_str();
        const char* res = calcVarint32Ptr(buf, buf + len, &aa);
        
        std::ofstream myFile ("/Users/matthew/Documents/reebes/iPhoneSE/com.tencent.xin/Documents/LoginInfo2.dat.f1", std::ios::out | std::ios::binary);
        myFile.write (json.c_str() + 2, 446);
        myFile.close();
    }
     
    
    // parseFieldValueFromProtobuf("/Users/matthew/Documents/Programs/Github/WechatExporter/WechatExporter/bb.bin", "3", json);

    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = NO;
    panel.canChooseDirectories = YES;
    panel.allowsMultipleSelection = NO;
    panel.canCreateDirectories = NO;
    panel.showsHiddenFiles = YES;
    
    // [panel setDirectoryURL:[NSURL URLWithString:NSHomeDirectory()]]; // Set panel's default directory.
    [panel setDirectoryURL:[NSURL URLWithString:@"/Users/matthew/Documents/reebes/Backup"]]; // Set panel's default directory.
    
    [panel beginSheetModalForWindow:[self.view window] completionHandler: (^(NSInteger result){
        if(result == NSOKButton) {
            NSURL *backupUrl = panel.directoryURL;
            
            if ([backupUrl.absoluteString hasSuffix:@"/Backup"] || [backupUrl.absoluteString hasSuffix:@"/Backup/"])
            {
                NSFileManager *fileManager = [NSFileManager defaultManager];
                
                NSError *error = nil;
                NSString *backupPath = [backupUrl.absoluteString substringFromIndex:7];
                NSArray<NSString *> * subFiles = [fileManager contentsOfDirectoryAtPath:backupPath error:&error];
                
                NSMutableArray<NSString *> *uids = [NSMutableArray array];
                
                for (NSString *file in subFiles)
                {
                    if (file.length == 32)
                    {
                        [uids addObject:file];
                    }
                }
                
                NSString *infoPlist = [NSString stringWithFormat:@"%@/info.plist", backupUrl];
                

                NSString *pathForFile;

                if ([fileManager fileExistsAtPath:infoPlist])
                {
                    BackupItem *backupItem = [[BackupItem alloc] init];
                    backupItem.backupPath = @"";
                    backupItem.displayName = @"iPhone SE";
                    backupItem.lastBackupDate = [NSDate date];
                    // backupItem.title = @"Test";
                    
                    [self.cmbboxBackup addItemWithObjectValue:backupItem];
                }
            }
            // backupUrl.
            
            // panel.dire
            // NSLog(backupUrl);
        }
    })];
}

- (void)btnOutputClicked:(id)sender
{
    
}

- (void)btnExportClicked:(id)sender
{
    self.txtViewLogs.string = @"";
    
    [self.progressBar startAnimation:nil];
    
    [NSThread detachNewThreadSelector:@selector(run) toTarget:self withObject:nil];
}

- (void)run
{
    ShellImpl shell;
    LoggerImpl logger(self.txtViewLogs);
    
    std::string backup = "/Users/matthew/Documents/reebes/iTunes/MobileSync/Backup/11833774f1a5eed6ca84c0270417670f1483deae/";
    std::string output = "/Users/matthew/Documents/reebes/wx_android_h5/";
    
    NSString *workDir = [[NSFileManager defaultManager] currentDirectoryPath];
    
    workDir = [[NSBundle mainBundle] resourcePath];
    
    Exporter exp([workDir UTF8String], backup, output, &shell, &logger);
    exp.run();
}

@end
