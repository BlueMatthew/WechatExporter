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
#include "ExportNotifierImpl.h"
#include "RawMessage.h"
#include "Utils.h"
#include "Exporter.h"

#include <sqlite3.h>
#include <fstream>

void errorLogCallback(void *pArg, int iErrCode, const char *zMsg)
{
    NSString *log = [NSString stringWithUTF8String:zMsg];
    
    NSLog(@"SQLITE3: %@", log);
}


@interface ViewController()
{
    ShellImpl* m_shell;
    LoggerImpl* m_logger;
    ExportNotifierImpl *m_notifier;
    Exporter* m_exp;
    
    std::vector<BackupManifest> m_manifests;
    NSInteger m_selectedIndex;
    
}
@end

@implementation ViewController

-(void)dealloc
{
    [self stopExporting];
}

- (void)stopExporting
{
    if (NULL != m_exp)
    {
        m_exp->cancel();
        m_exp->waitForComplition();
        delete m_exp;
        m_exp = NULL;
    }
    if (NULL != m_notifier)
    {
        delete m_notifier;
        m_notifier = NULL;
    }
    if (NULL != m_logger)
    {
        delete m_logger;
        m_logger = NULL;
    }
    if (NULL != m_shell)
    {
        delete m_shell;
        m_shell = NULL;
    }
    
    [self.btnBackup setAction:nil];
    [self.btnOutput setAction:nil];
    [self.btnExport setAction:nil];
    [self.btnCancel setAction:nil];
}

- (void)viewDidLoad {
    [super viewDidLoad];
    
#ifndef NDEBUG
    self.chkboxNoAudio.hidden = NO;
#endif
    sqlite3_config(SQLITE_CONFIG_LOG, errorLogCallback, NULL);

    [self.view.window center];
    
    // self.txtboxLogs.
    
    m_selectedIndex = 0;
    m_shell = new ShellImpl();
    m_logger = new LoggerImpl(self);
    m_notifier = new ExportNotifierImpl(self);
    m_exp = NULL;
    
    [self.btnBackup setAction:@selector(btnBackupClicked:)];
    [self.btnOutput setAction:@selector(btnOutputClicked:)];
    [self.btnExport setAction:@selector(btnExportClicked:)];
    [self.btnCancel setAction:@selector(btnCancelClicked:)];
    [self.chkboxDesc setAction:@selector(btnDescClicked:)];
    [self.chkboxNoAudio setAction:@selector(btnIgnoreAudioClicked:)];
    
    BOOL descOrder = [[NSUserDefaults standardUserDefaults] boolForKey:@"Desc"];
    self.chkboxDesc.state = descOrder ? NSOnState : NSOffState;
    
    BOOL ignoreAudio = [[NSUserDefaults standardUserDefaults] boolForKey:@"IgnoreAudio"];
    self.chkboxNoAudio.state = ignoreAudio ? NSOnState : NSOffState;

    NSFileManager *fileManager = [NSFileManager defaultManager];
    
    NSString *outputDir = [[NSUserDefaults standardUserDefaults] objectForKey:@"OutputDir"];
    if (nil == outputDir || [outputDir isEqualToString:@""])
    {
        NSMutableArray *components = [NSMutableArray array];
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
        if (nil == paths && paths.count > 0)
        {
            [components addObject:[paths objectAtIndex:0]];
        }
        else
        {
            [components addObject:NSHomeDirectory()];
            [components addObject:@"Documents"];
        }
        [components addObject:@"WechatHistory"];
        
        outputDir = [NSString pathWithComponents:components];
    }
    
    self.txtboxOutput.stringValue = outputDir;
    
    NSURL *appSupport = [fileManager URLForDirectory:NSApplicationSupportDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:NO error:nil];
    
    NSArray *components = @[[appSupport path], @"MobileSync", @"Backup"];
    NSString *backupDir = [NSString pathWithComponents:components];
    BOOL isDir = NO;
    if ([fileManager fileExistsAtPath:backupDir isDirectory:&isDir] && isDir)
    {
        NSString *backupDir = [NSString pathWithComponents:components];

        ManifestParser parser([backupDir UTF8String], "Info.plist", m_shell);
        std::vector<BackupManifest> manifests = parser.parse();
        [self updateBackups:manifests];
    }
}

- (void)updateBackups:(const std::vector<BackupManifest>&) manifests
{
    int selectedIndex = -1;
    if (!manifests.empty())
    {
        // Add default backup folder
        for (std::vector<BackupManifest>::const_iterator it = manifests.cbegin(); it != manifests.cend(); ++it)
        {
            std::vector<BackupManifest>::const_iterator it2 = std::find(m_manifests.cbegin(), m_manifests.cend(), *it);
            if (it2 != m_manifests.cend())
            {
                if (selectedIndex == -1)
                {
                    selectedIndex = static_cast<int>(std::distance(it2, m_manifests.cbegin()));
                }
            }
            else
            {
                m_manifests.push_back(*it);
                if (selectedIndex == -1)
                {
                    selectedIndex = static_cast<int>(m_manifests.size() - 1);
                }
            }
        }
        
        // update
        [self.cmbboxBackup removeAllItems];
        for (std::vector<BackupManifest>::const_iterator it = m_manifests.cbegin(); it != m_manifests.cend(); ++it)
        {
            std::string itemTitle = it->toString();
            NSString* item = [NSString stringWithUTF8String:itemTitle.c_str()];
            [self.cmbboxBackup addItemWithObjectValue:item];
        }
        if (selectedIndex != -1 && selectedIndex < self.cmbboxBackup.numberOfItems)
        {
            [self.cmbboxBackup selectItemAtIndex:selectedIndex];
        }
    }
}

- (void)setRepresentedObject:(id)representedObject {
    [super setRepresentedObject:representedObject];

    // Update the view, if already loaded.
}

- (void)btnBackupClicked:(id)sender
{
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = NO;
    panel.canChooseDirectories = YES;
    panel.allowsMultipleSelection = NO;
    panel.canCreateDirectories = NO;
    panel.showsHiddenFiles = YES;
    
    [panel setDirectoryURL:[NSURL URLWithString:NSHomeDirectory()]]; // Set panel's default directory.
    // [panel setDirectoryURL:[NSURL URLWithString:@"/Users/matthew/Documents/reebes/Backup"]]; // Set panel's default directory.
    
    [panel beginSheetModalForWindow:[self.view window] completionHandler: (^(NSInteger result){
        if (result == NSOKButton)
        {
            NSURL *backupUrl = panel.directoryURL;
            
            if ([backupUrl.absoluteString hasSuffix:@"/Backup"] || [backupUrl.absoluteString hasSuffix:@"/Backup/"])
            {
                ManifestParser parser([backupUrl.path UTF8String], "Info.plist", self->m_shell);
                std::vector<BackupManifest> manifests = parser.parse();
                [self updateBackups:manifests];
            }
        }
    })];
}

- (void)btnOutputClicked:(id)sender
{
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = NO;
    panel.canChooseDirectories = YES;
    panel.allowsMultipleSelection = NO;
    panel.canCreateDirectories = YES;
    panel.showsHiddenFiles = NO;
    
    NSString *outputPath = self.txtboxOutput.stringValue;
    if (nil == outputPath || [outputPath isEqualToString:@""])
    {
        [panel setDirectoryURL:[NSURL URLWithString:NSHomeDirectory()]]; // Set panel's default directory.
    }
    else
    {
        [panel setDirectoryURL:[NSURL fileURLWithPath:outputPath]];
    }
    
    [panel beginSheetModalForWindow:[self.view window] completionHandler: (^(NSInteger result){
        if (result == NSOKButton)
        {
            NSURL *url = panel.directoryURL;
            [[NSUserDefaults standardUserDefaults] setObject:url.path forKey:@"OutputDir"];
            
            self.txtboxOutput.stringValue = url.path;
        }
    })];
}

- (void)btnExportClicked:(id)sender
{
    if (NULL != m_exp)
    {
        [self msgBox:@"导出已经在执行。"];
        return;
    }
    
    if (self.cmbboxBackup.indexOfSelectedItem == -1 || self.cmbboxBackup.indexOfSelectedItem >= m_manifests.size())
    {
        [self msgBox:@"请选择iTunes备份目录。"];
        return;
    }
    std::string backup = m_manifests[self.cmbboxBackup.indexOfSelectedItem].getPath();
    NSString *backupPath = [NSString stringWithUTF8String:backup.c_str()];
    BOOL isDir = NO;
    if (![[NSFileManager defaultManager] fileExistsAtPath:backupPath isDirectory:&isDir] || !isDir)
    {
        [self msgBox:@"iTunes备份目录不存在。"];
        return;
    }
    
    NSString *outputPath = self.txtboxOutput.stringValue;
    if (nil == outputPath || [outputPath isEqualToString:@""])
    {
        [self msgBox:@"请选择输出目录。"];
        return;
    }
    
    if (![[NSFileManager defaultManager] fileExistsAtPath:outputPath isDirectory:&isDir] || !isDir)
    {
        [self msgBox:@"输出目录不存在。"];
        // self.txtboxOutput focus
        return;
    }
    
    BOOL descOrder = (self.chkboxDesc.state == NSOnState);
    BOOL ignoreAudio = (self.chkboxNoAudio.state == NSOnState);
    
    self.txtViewLogs.string = @"";
    [self onStart];
    NSDictionary *dict = @{@"backup": backupPath, @"output": outputPath, @"descOrder": @(descOrder), @"ignoreAudio": @(ignoreAudio) };
    [NSThread detachNewThreadSelector:@selector(run:) toTarget:self withObject:dict];
}

- (void)btnCancelClicked:(id)sender
{
    if (NULL == m_exp)
    {
        // [self msgBox:@"当前未执行导出。"];
        return;
    }
    
    m_exp->cancel();
    [self.btnCancel setEnabled:NO];
}

- (void)btnDescClicked:(id)sender
{
    BOOL descOrder = (self.chkboxDesc.state == NSOnState);
    [[NSUserDefaults standardUserDefaults] setBool:descOrder forKey:@"Desc"];
}

- (void)btnIgnoreAudioClicked:(id)sender
{
    BOOL ignoreAudio = (self.chkboxNoAudio.state == NSOnState);
    [[NSUserDefaults standardUserDefaults] setBool:ignoreAudio forKey:@"IgnoreAudio"];
}

- (void)run:(NSDictionary *)dict
{
    NSString *backup = [dict objectForKey:@"backup"];
    NSString *output = [dict objectForKey:@"output"];
    
    if (backup == nil || output == nil)
    {
        [self msgBox:@"参数错误。"];
        return;
    }
    
    NSNumber *ignoreAudio = [dict objectForKey:@"ignoreAudio"];
    NSNumber *descOrder = [dict objectForKey:@"descOrder"];
    
    NSString *workDir = [[NSFileManager defaultManager] currentDirectoryPath];
    
    workDir = [[NSBundle mainBundle] resourcePath];
    
    m_exp = new Exporter([workDir UTF8String], [backup UTF8String], [output UTF8String], m_shell, m_logger);
    if (nil != ignoreAudio && [ignoreAudio boolValue])
    {
        m_exp->ignoreAudio();
    }
    if (nil != descOrder && [descOrder boolValue])
    {
        m_exp->setOrder(false);
    }

    m_exp->setNotifier(m_notifier);
    
    m_exp->run();
}

- (void)msgBox:(NSString *)msg
{
    __block NSString *localMsg = [NSString stringWithString:msg];
    __block NSString *title = [NSRunningApplication currentApplication].localizedName;
    dispatch_async(dispatch_get_main_queue(), ^{
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = localMsg;
        alert.window.title = title;
        [alert runModal];
    });
}

- (void)onStart
{
    self.view.window.styleMask &= ~NSClosableWindowMask;
    [self.cmbboxBackup setEnabled:NO];
    [self.btnOutput setEnabled:NO];
    [self.btnBackup setEnabled:NO];
    [self.btnExport setEnabled:NO];
    [self.btnCancel setEnabled:YES];
    [self.progressBar startAnimation:nil];
}

- (void)onComplete:(BOOL)cancelled
{
    self.view.window.styleMask |= NSClosableWindowMask;
    [self.btnExport setEnabled:YES];
    [self.btnCancel setEnabled:NO];
    [self.cmbboxBackup setEnabled:YES];
    [self.btnOutput setEnabled:YES];
    [self.btnBackup setEnabled:YES];
    [self.progressBar stopAnimation:nil];
}

- (void)writeLog:(NSString *)log
{
    NSString *newLog = nil;
   
    if (nil == self.txtViewLogs.string || self.txtViewLogs.string.length == 0)
    {
        self.txtViewLogs.string = [log copy];
    }
    else
    {
        newLog = [NSString stringWithFormat:@"%@\n%@", self.txtViewLogs.string, log];
        self.txtViewLogs.string = newLog;
    }

    NSPoint newScrollOrigin;
    // assume that the scrollview is an existing variable
    if ([[self.sclViewLogs documentView] isFlipped])
    {
        newScrollOrigin = NSMakePoint(0.0, NSMaxY([[self.sclViewLogs documentView] frame])
                                       -NSHeight([[self.sclViewLogs contentView] bounds]));
    }
    else
    {
        newScrollOrigin = NSMakePoint(0.0,0.0);
    }

    [[self.sclViewLogs documentView] scrollPoint:newScrollOrigin];
}

@end
