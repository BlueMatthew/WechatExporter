//
//  ViewController.m
//  WechatExporter
//
//  Created by Matthew on 2020/9/29.
//  Copyright © 2020 Matthew. All rights reserved.
//

#import "ViewController.h"
#import "SessionDataSource.h"
#include "ITunesParser.h"

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


@interface ViewController() <NSTableViewDelegate>
{
    ShellImpl* m_shell;
    LoggerImpl* m_logger;
    ExportNotifierImpl *m_notifier;
    Exporter* m_exporter;
    
    std::vector<BackupManifest> m_manifests;
    std::vector<std::pair<Friend, std::vector<Session>>> m_usersAndSessions;
    
    SessionDataSource   *m_dataSource;
    
}
@end

@implementation ViewController

- (instancetype)init
{
    if (self = [super init])
    {
        m_dataSource = [[SessionDataSource alloc] init];
    }
    
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    if (self = [super initWithCoder:coder])
    {
        m_dataSource = [[SessionDataSource alloc] init];
    }
    
    return self;
}

- (instancetype)initWithNibName:(NSNibName)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    if (self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil])
    {
        m_dataSource = [[SessionDataSource alloc] init];
    }
    
    return self;
}

-(void)dealloc
{
    [self stopExporting];
}

- (void)stopExporting
{
    if (NULL != m_exporter)
    {
        m_exporter->cancel();
        m_exporter->waitForComplition();
        delete m_exporter;
        m_exporter = NULL;
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
    [self.btnBackup setTarget:nil];
    [self.btnOutput setAction:nil];
    [self.btnOutput setTarget:nil];
    [self.btnExport setAction:nil];
    [self.btnExport setTarget:nil];
    [self.btnCancel setAction:nil];
    [self.btnCancel setTarget:nil];
    [self.btnQuit setAction:nil];
    [self.btnQuit setTarget:nil];
    [self.chkboxDesc setAction:nil];
    [self.chkboxDesc setTarget:nil];
    [self.chkboxNoAudio setAction:nil];
    [self.chkboxNoAudio setTarget:nil];
    [self.popupBackup setAction:nil];
    [self.popupBackup setTarget:nil];
    [self.popupUsers setAction:nil];
    [self.popupUsers setTarget:nil];
    [self.btnToggleAll setAction:nil];
    [self.btnToggleAll setTarget:nil];
}

- (void)viewDidLoad {
    [super viewDidLoad];
    
#ifndef NDEBUG
    self.chkboxNoAudio.hidden = NO;
#endif
    sqlite3_config(SQLITE_CONFIG_LOG, errorLogCallback, NULL);

    [self.view.window center];
    
    m_shell = new ShellImpl();
    m_logger = new LoggerImpl(self);
    m_notifier = new ExportNotifierImpl(self);
    m_exporter = NULL;
    
    [self.btnBackup setTarget:self];
    [self.btnBackup setAction:@selector(btnBackupClicked:)];
    [self.btnOutput setTarget:self];
    [self.btnOutput setAction:@selector(btnOutputClicked:)];
    [self.btnExport setTarget:self];
    [self.btnExport setAction:@selector(btnExportClicked:)];
    [self.btnCancel setTarget:self];
    [self.btnCancel setAction:@selector(btnCancelClicked:)];
    [self.btnQuit setTarget:self];
    [self.btnQuit setAction:@selector(btnQuitClicked:)];
    [self.chkboxDesc setTarget:self];
    [self.chkboxDesc setAction:@selector(btnDescClicked:)];
    [self.chkboxNoAudio setTarget:self];
    [self.chkboxNoAudio setAction:@selector(btnIgnoreAudioClicked:)];
    [self.popupBackup setTarget:self];
    [self.popupBackup setAction:@selector(handlePopupButton:)];
    [self.popupUsers setTarget:self];
    [self.popupUsers setAction:@selector(handlePopupButton:)];
    [self.btnToggleAll setTarget:self];
    [self.btnToggleAll setAction:@selector(toggleAllSessions:)];
    
    NSRect frame = [self.tblSessions.headerView headerRectOfColumn:0];
    NSRect btnFrame = self.btnToggleAll.frame;
    btnFrame.size.width = btnFrame.size.height;
    btnFrame.origin.x = (frame.size.width - btnFrame.size.width) / 2;
    btnFrame.origin.y = (frame.size.height - btnFrame.size.height) / 2;
    
    self.btnToggleAll.frame = btnFrame;
    
    [self.tblSessions.headerView addSubview:self.btnToggleAll];
    for (NSTableColumn *tableColumn in self.tblSessions.tableColumns)
    {
        NSSortDescriptor *sortDescriptor = [NSSortDescriptor sortDescriptorWithKey:tableColumn.identifier ascending:YES selector:@selector(compare:)];
        [tableColumn setSortDescriptorPrototype:sortDescriptor];
    }
    
    self.tblSessions.dataSource = m_dataSource;
    self.tblSessions.delegate = self;
    
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

        ManifestParser parser([backupDir UTF8String], m_shell);
        std::vector<BackupManifest> manifests;
        if (parser.parse(manifests))
        {
            NSString *previoudBackupDir = [[NSUserDefaults standardUserDefaults] objectForKey:@"BackupDir"];
            [self updateBackups:manifests withPreviousPath:previoudBackupDir];
        }
    }
    
    self.popupBackup.autoresizingMask = NSViewMinYMargin | NSViewWidthSizable;
    self.btnBackup.autoresizingMask = NSViewMinXMargin | NSViewMinYMargin;
    
    self.txtboxOutput.autoresizingMask = NSViewMinYMargin | NSViewWidthSizable;
    self.btnOutput.autoresizingMask = NSViewMinXMargin | NSViewMinYMargin;
    
    self.popupUsers.autoresizingMask = NSViewMinYMargin;
    self.tblSessions.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    self.sclSessions.autoresizingMask = NSViewHeightSizable;
    
    self.sclViewLogs.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    
    self.progressBar.autoresizingMask = NSViewMaxYMargin;
    self.btnCancel.autoresizingMask = NSViewMinXMargin | NSViewMaxYMargin;
    self.btnQuit.autoresizingMask = NSViewMinXMargin | NSViewMaxYMargin;
    self.btnExport.autoresizingMask = NSViewMinXMargin | NSViewMaxYMargin;
};

- (void)updateBackups:(const std::vector<BackupManifest>&) manifests withPreviousPath:(NSString *)previousPath
{
    if (manifests.empty())
    {
        return;
    }
    
    size_t selectedIndex = (size_t)self.popupBackup.indexOfSelectedItem;
    for (std::vector<BackupManifest>::const_iterator it = manifests.cbegin(); it != manifests.cend(); ++it)
    {
        std::vector<BackupManifest>::const_iterator it2 = std::find(m_manifests.cbegin(), m_manifests.cend(), *it);
        if (it2 == m_manifests.cend())
        {
            m_manifests.push_back(*it);
        }
    }
    
    // update
    [self.popupBackup removeAllItems];
    for (std::vector<BackupManifest>::const_iterator it = m_manifests.cbegin(); it != m_manifests.cend(); ++it)
    {
        std::string itemTitle = it->toString();
        NSString* item = [NSString stringWithUTF8String:itemTitle.c_str()];
        [self.popupBackup addItemWithTitle:item];
        
        if (selectedIndex == -1)
        {
            if (nil != previousPath && ![previousPath isEqualToString:@""])
            {
                NSString* itemPath = [NSString stringWithUTF8String:it->getPath().c_str()];
                if ([previousPath isEqualToString:itemPath])
                {
                    selectedIndex = std::distance(m_manifests.cbegin(), it);
                }
            }
        }
    }
    if (selectedIndex == -1 && self.popupBackup.numberOfItems > 0)
    {
        selectedIndex = 0;
    }
    if (selectedIndex != -1 && selectedIndex < [self.popupBackup numberOfItems])
    {
        [self setPopupButton:self.popupBackup selectedItemAt:selectedIndex];
    }
}

- (IBAction)handlePopupButton:(NSPopUpButton *)popupButton
{
    if (popupButton == self.popupBackup)
    {
        m_usersAndSessions.clear();
        [self.popupUsers removeAllItems];
        
        if (self.popupBackup.indexOfSelectedItem == -1 || self.popupBackup.indexOfSelectedItem >= m_manifests.size())
        {
            // Clear Users and Sessions
            [m_dataSource loadData:&m_usersAndSessions withAllUsers:YES indexOfSelectedUser:-1];
            [self.tblSessions reloadData];
            return;
        }
        
        const BackupManifest& manifest = m_manifests[self.popupBackup.indexOfSelectedItem];
        std::string backup = manifest.getPath();
#ifndef NDEBUG
        NSString *backupPath = [NSString stringWithUTF8String:backup.c_str()];
        [[NSUserDefaults standardUserDefaults] setObject:backupPath forKey:@"BackupDir"];
#endif
            
        if (manifest.isEncrypted())
        {
            [m_dataSource loadData:&m_usersAndSessions withAllUsers:YES indexOfSelectedUser:-1];
            [self.tblSessions reloadData];

            [self msgBox:@"不支持加密的iTunes备份。"];
            return;
        }

#ifndef NDEBUG
        m_logger->write("Start loading users and sessions.");
#endif

        NSString *workDir = [[NSBundle mainBundle] resourcePath];

        Exporter exp([workDir UTF8String], backup, "", m_shell, m_logger);
        exp.loadUsersAndSessions(m_usersAndSessions);
        
#ifndef NDEBUG
        m_logger->write("Data Loaded.");
#endif

        [self loadUsers];

    }
    else if (popupButton == self.popupUsers)
    {
        NSInteger indexOfSelectedItem = self.popupUsers.indexOfSelectedItem;
        BOOL allUsers = (indexOfSelectedItem == 0);
        if (indexOfSelectedItem != -1)
        {
            indexOfSelectedItem--;
        }
        [m_dataSource loadData:&m_usersAndSessions withAllUsers:allUsers indexOfSelectedUser:indexOfSelectedItem];
        [self.tblSessions reloadData];
    }
}

- (void)toggleAllSessions:(id)sender
{
    NSButton *btn = (NSButton *)sender;
    if (btn.state == NSControlStateValueMixed)
    {
        [self.btnToggleAll setNextState];
    }
        
    if (btn.state == NSControlStateValueOn)
    {
        [m_dataSource checkAllSessions:YES];
    }
    else if (btn.state == NSControlStateValueOff)
    {
        [m_dataSource checkAllSessions:NO];
    }
    
    [self.tblSessions reloadData];
}

- (void)checkButtonTapped:(id)sender
{
    NSButton *btn = (NSButton *)sender;
    NSControlStateValue state = [m_dataSource updateCheckStateAtRow:btn.tag];
    self.btnToggleAll.state = state;
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
    
    [panel beginSheetModalForWindow:[self.view window] completionHandler: (^(NSInteger result) {
        if (result == NSOKButton)
        {
            NSURL *backupUrl = panel.directoryURL;
            
            ManifestParser parser([backupUrl.path UTF8String], self->m_shell);
            std::vector<BackupManifest> manifests;
            if (parser.parse(manifests) && !manifests.empty())
            {
                [self updateBackups:manifests withPreviousPath:nil];
            }
            else
            {
                [self msgBox:@"解析iTunes Backup文件失败。"];
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
    if (NULL != m_exporter)
    {
        [self msgBox:@"导出已经在执行。"];
        return;
    }
    
    if (self.popupBackup.indexOfSelectedItem == -1 || self.popupBackup.indexOfSelectedItem >= m_manifests.size())
    {
        [self msgBox:@"请选择iTunes备份目录。"];
        return;
    }
    
    const BackupManifest& manifest = m_manifests[self.popupBackup.indexOfSelectedItem];
    if (manifest.isEncrypted())
    {
        [self msgBox:@"不支持加密的iTunes Backup。请使用不加密形式备份iPhone/iPad设备。"];
        return;
    }
    
    std::string backup = manifest.getPath();
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
    BOOL saveFilesInSessionFolder = (self.chkboxSaveFilesInSessionFolder.state == NSOnState);
    
    self.txtViewLogs.string = @"";
    [self onStart];
    NSDictionary *dict = @{@"backup": backupPath, @"output": outputPath, @"descOrder": @(descOrder), @"ignoreAudio": @(ignoreAudio), @"saveFilesInSessionFolder": @(saveFilesInSessionFolder)};
    [NSThread detachNewThreadSelector:@selector(run:) toTarget:self withObject:dict];
}

- (void)btnCancelClicked:(id)sender
{
    if (NULL == m_exporter)
    {
        // [self msgBox:@"当前未执行导出。"];
        return;
    }
    
    m_exporter->cancel();
    [self.btnCancel setEnabled:NO];
}

- (void)btnQuitClicked:(id)sender
{
    [self.view.window.windowController close];
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
    
    // NSString *iTunesVersion = [dict objectForKey:@"iTunesVersion"];
    NSNumber *ignoreAudio = [dict objectForKey:@"ignoreAudio"];
    NSNumber *descOrder = [dict objectForKey:@"descOrder"];
    NSNumber *saveFilesInSessionFolder = [dict objectForKey:@"saveFilesInSessionFolder"];
    
    NSString *workDir = [[NSFileManager defaultManager] currentDirectoryPath];
    
    workDir = [[NSBundle mainBundle] resourcePath];
    
    std::map<std::string, std::set<std::string>> usersAndSessions;
    [m_dataSource getSelectedUserAndSessions:usersAndSessions];
    
    m_exporter = new Exporter([workDir UTF8String], [backup UTF8String], [output UTF8String], m_shell, m_logger);
    if (nil != ignoreAudio && [ignoreAudio boolValue])
    {
        m_exporter->ignoreAudio();
    }
    if (nil != descOrder && [descOrder boolValue])
    {
        m_exporter->setOrder(false);
    }
    if (nil != saveFilesInSessionFolder && [saveFilesInSessionFolder boolValue])
    {
        m_exporter->saveFilesInSessionFolder();
    }

    m_exporter->setNotifier(m_notifier);
    
    m_exporter->filterUsersAndSessions(usersAndSessions);
    
    m_exporter->run();
}

- (void)loadUsers
{
    [self.popupUsers removeAllItems];
    if (m_usersAndSessions.empty())
    {
        return;
    }
    
    [self.popupUsers addItemWithTitle:@"所有微信账户的聊天记录"];
    // CComboBox cbmBox = GetDlgItem(IDC_USERS);
    for (std::vector<std::pair<Friend, std::vector<Session>>>::const_iterator it = m_usersAndSessions.cbegin(); it != m_usersAndSessions.cend(); ++it)
    {
        NSString *displayName = [NSString stringWithUTF8String:it->first.getDisplayName().c_str()];
        [self.popupUsers addItemWithTitle:displayName];
    }
    if ([self.popupUsers numberOfItems] > 0)
    {
        [self setPopupButton:self.popupUsers selectedItemAt:0];
    }
}

- (void)setPopupButton:(NSPopUpButton *)popupButton selectedItemAt:(NSInteger)index
{
    [popupButton.menu performActionForItemAtIndex:index];
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
    [self.popupBackup setEnabled:NO];
    [self.btnOutput setEnabled:NO];
    [self.btnBackup setEnabled:NO];
    [self.btnExport setEnabled:NO];
    [self.btnCancel setEnabled:YES];
    [self.btnCancel setHidden:NO];
    [self.btnQuit setHidden:YES];
    [self.chkboxDesc setEnabled:NO];
    [self.chkboxNoAudio setEnabled:NO];
    [self.chkboxSaveFilesInSessionFolder setEnabled:NO];
    [self.progressBar startAnimation:nil];
}

- (void)onComplete:(BOOL)cancelled
{
    self.view.window.styleMask |= NSClosableWindowMask;
    [self.btnExport setEnabled:YES];
    [self.btnQuit setHidden:NO];
    [self.btnCancel setEnabled:NO];
    [self.btnCancel setHidden:YES];
    [self.popupBackup setEnabled:YES];
    [self.btnOutput setEnabled:YES];
    [self.btnBackup setEnabled:YES];
    [self.chkboxDesc setEnabled:YES];
    [self.chkboxNoAudio setEnabled:YES];
    [self.chkboxSaveFilesInSessionFolder setEnabled:YES];
    [self.progressBar stopAnimation:nil];
    
    if (m_exporter)
    {
        m_exporter->waitForComplition();
        delete m_exporter;
        m_exporter = NULL;
    }
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

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    NSTableCellView *cellView = [tableView makeViewWithIdentifier:[tableColumn identifier] owner:self];
    if ([[tableColumn identifier] isEqualToString:@"columnCheck"])
    {
        NSButton *btn = (NSButton *)cellView.subviews.firstObject;
        if (btn)
        {
            [btn setTarget:self];
            [btn setAction:@selector(checkButtonTapped:)];
            btn.tag = row;
        }
    }
    
    [m_dataSource bindCellView:cellView atRow:row andColumnId:[tableColumn identifier]];
    return cellView;
}

- (void)tableViewColumnDidResize:(NSNotification *)notification
{
    if ([notification.name isEqualToString:NSTableViewColumnDidResizeNotification])
    {
        NSTableView *tableView = (NSTableView *)notification.object;
        NSTableColumn *column = [notification.userInfo objectForKey:@"NSTableColumn"];
        
        if (tableView == self.tblSessions && [column.identifier isEqualToString:@"columnCheck"])
        {
            NSRect frame = [self.tblSessions.headerView headerRectOfColumn:0];
            NSRect btnFrame = self.btnToggleAll.frame;
            btnFrame.origin.x = (frame.size.width - btnFrame.size.width) / 2;
            btnFrame.origin.y = (frame.size.height - btnFrame.size.height) / 2;
            
            self.btnToggleAll.frame = btnFrame;
        }
        
    }
}


@end
