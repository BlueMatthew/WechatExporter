//
//  ViewController.mm
//  WechatExporter
//
//  Created by Matthew on 2020/9/29.
//  Copyright © 2020 Matthew. All rights reserved.
//

#import "ViewController.h"
#import "SessionDataSource.h"
#import "HttpHelper.h"
#import "AppConfiguration.h"

#include "LoggerImpl.h"
#include "ExportNotifierImpl.h"
#include "PdfConverterImpl.h"
#include "Utils.h"
#include "Exporter.h"
#include "Updater.h"

@interface ViewController() <NSTableViewDelegate>
{
    LoggerImpl* m_logger;
    ExportNotifierImpl *m_notifier;
    PdfConverterImpl *m_pdfConverter;
    
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
    if (NULL != m_pdfConverter)
    {
        delete m_pdfConverter;
        m_pdfConverter = NULL;
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
    [self.popupBackup setAction:nil];
    [self.popupBackup setTarget:nil];
    [self.popupUsers setAction:nil];
    [self.popupUsers setTarget:nil];
    [self.btnToggleAll setAction:nil];
    [self.btnToggleAll setTarget:nil];
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    
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

    self.txtboxOutput.stringValue = [AppConfiguration getLastOrDefaultOutputDir];

    NSString *backupDir = [AppConfiguration getDefaultBackupDir:YES];
    if (nil != backupDir)
    {
        ManifestParser parser([backupDir UTF8String]);
        std::vector<BackupManifest> manifests;
        if (parser.parse(manifests))
        {
            NSString *previoudBackupDir = [AppConfiguration getLastBackupDir];
            [self updateBackups:manifests withPreviousPath:previoudBackupDir];
        }
    }
    
    BOOL checkUpdateDisabled = [AppConfiguration isCheckingUpdateDisabled];
    NSInteger lastChkUpdateTime = [AppConfiguration getLastCheckUpdateTime];
#ifndef NDEBUG
    lastChkUpdateTime = 0;
#endif
    if (!checkUpdateDisabled && ((getUnixTimeStamp() - (uint32_t)lastChkUpdateTime) > 86400))
    {
#ifndef NDEBUG
        [self performSelector:@selector(checkUpdate) withObject:nil afterDelay:1];
#else
        [self performSelector:@selector(checkUpdate) withObject:nil afterDelay:5];
#endif
    }
}

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
        self.txtViewLogs.string = @"";
        // Clear Users and Sessions
        [m_dataSource loadData:&m_usersAndSessions withAllUsers:YES indexOfSelectedUser:-1];
        [self.tblSessions reloadData];
        
        if (self.popupBackup.indexOfSelectedItem == -1 || self.popupBackup.indexOfSelectedItem >= m_manifests.size())
        {
            return;
        }
        
        const BackupManifest& manifest = m_manifests[self.popupBackup.indexOfSelectedItem];
            
        if (manifest.isEncrypted())
        {
            [self msgBox:NSLocalizedString(@"err-encrypted-bkp-not-supported", comment: "")];
            return;
        }
        
        NSString *backupPath = [NSString stringWithUTF8String:manifest.getPath().c_str()];
#ifndef NDEBUG
        [AppConfiguration setLastBackupDir:backupPath];
#endif
        [self performSelector:@selector(loadDataForBackup:) withObject:backupPath afterDelay:0.016];
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
        self.btnToggleAll.state = NSControlStateValueOn;
        [self.tblSessions reloadData];
    }
}

- (void)loadDataForBackup:(NSString *)backupPath
{
#ifndef NDEBUG
    m_logger->write("Start loading users and sessions.");
#endif

    __block NSString *backupDir = [NSString stringWithString:backupPath];
    __block NSString *workDir = [[NSBundle mainBundle] resourcePath];
    typeof(self) __weak weakSelf = self;
    
    [self setUIEnabled:NO withCancellable:NO];
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        
        __strong typeof(weakSelf) strongSelf = weakSelf;  // strong by default
        if (nil != strongSelf)
        {
            Exporter exp([workDir UTF8String], [backupDir UTF8String], "", strongSelf->m_logger, NULL);
            exp.loadUsersAndSessions();
            exp.swapUsersAndSessions(strongSelf->m_usersAndSessions);
        }
        
        // update UI on the main thread
        dispatch_async(dispatch_get_main_queue(), ^{
            __strong typeof(weakSelf) strongSelf = weakSelf;  // strong by default
            if (strongSelf) {
    #ifndef NDEBUG
                strongSelf->m_logger->write("Data Loaded.");
    #endif
                [strongSelf loadUsers];
                [strongSelf setUIEnabled:YES withCancellable:NO];
            }
        });
    });
    
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
            
            ManifestParser parser([backupUrl.path UTF8String]);
            std::vector<BackupManifest> manifests;
            if (parser.parse(manifests) && !manifests.empty())
            {
                [self updateBackups:manifests withPreviousPath:nil];
            }
            else
            {
                [self msgBox:NSLocalizedString(@"err-failed-to-parse-backup", comment: "")];
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
            [AppConfiguration setLastOutputDir:url.path];
            
            self.txtboxOutput.stringValue = url.path;
        }
    })];
}

- (void)btnExportClicked:(id)sender
{
    if (NULL != m_exporter)
    {
        [self msgBox:NSLocalizedString(@"err-exp-is-running", comment: "")];
        return;
    }
    
    if (self.popupBackup.indexOfSelectedItem == -1 || self.popupBackup.indexOfSelectedItem >= m_manifests.size())
    {
        [self msgBox:NSLocalizedString(@"err-no-backup-dir", comment: "")];
        return;
    }
    
    const BackupManifest& manifest = m_manifests[self.popupBackup.indexOfSelectedItem];
    if (manifest.isEncrypted())
    {
        [self msgBox:NSLocalizedString(@"err-encrypted-bkp-not-supported", comment: "")];
        return;
    }
    
    std::string backup = manifest.getPath();
    NSString *backupPath = [NSString stringWithUTF8String:backup.c_str()];
    BOOL isDir = NO;
    if (![[NSFileManager defaultManager] fileExistsAtPath:backupPath isDirectory:&isDir] || !isDir)
    {
        [self msgBox:NSLocalizedString(@"err-backup-dir-doesnt-exist", comment: "")];
        return;
    }
    
    NSString *outputPath = self.txtboxOutput.stringValue;
    if (nil == outputPath || [outputPath isEqualToString:@""])
    {
        [self msgBox:NSLocalizedString(@"err-no-output-dir", comment: "")];
        return;
    }
    
    if (![[NSFileManager defaultManager] fileExistsAtPath:outputPath isDirectory:&isDir] || !isDir)
    {
        [self msgBox:NSLocalizedString(@"err-output-dir-doesnt-exist", comment: "")];
        // self.txtboxOutput focus
        return;
    }

    BOOL descOrder = [AppConfiguration getDescOrder];
    BOOL textMode = [AppConfiguration isTextMode];
    BOOL syncLoading = ![AppConfiguration getAsyncLoading];
    BOOL saveFilesInSessionFolder = [AppConfiguration getSavingInSession];
    
    self.txtViewLogs.string = @"";
    [self onStart];
    NSDictionary *dict = @{@"backup": backupPath, @"output": outputPath, @"descOrder": @(descOrder), @"textMode": @(textMode), @"syncLoading": @(syncLoading), @"saveFilesInSessionFolder": @(saveFilesInSessionFolder)};
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

- (void)run:(NSDictionary *)dict
{
    NSString *backup = [dict objectForKey:@"backup"];
    NSString *output = [dict objectForKey:@"output"];

    if (backup == nil || output == nil)
    {
        [self msgBox:NSLocalizedString(@"err-wrong-param", comment: "")];
        return;
    }
    
    // NSString *iTunesVersion = [dict objectForKey:@"iTunesVersion"];
    NSNumber *textMode = [dict objectForKey:@"textMode"];
    NSNumber *descOrder = [dict objectForKey:@"descOrder"];
    NSNumber *syncLoading = [dict objectForKey:@"syncLoading"];
    NSNumber *saveFilesInSessionFolder = [dict objectForKey:@"saveFilesInSessionFolder"];
    
    NSString *workDir = [[NSFileManager defaultManager] currentDirectoryPath];
    
    workDir = [[NSBundle mainBundle] resourcePath];
    
    std::map<std::string, std::map<std::string, void *>> usersAndSessions;
    [m_dataSource getSelectedUserAndSessions:usersAndSessions];
    
#ifndef NDEBUG
    if ([AppConfiguration isPdfMode])
    {
        if (NULL == m_pdfConverter)
        {
            m_pdfConverter = new PdfConverterImpl();
            m_pdfConverter->setWorkDir(output);
        }
    }
#endif
    m_exporter = new Exporter([workDir UTF8String], [backup UTF8String], [output UTF8String], m_logger, m_pdfConverter);
    if (nil != descOrder && [descOrder boolValue])
    {
        m_exporter->setOrder(false);
    }
    if (nil != saveFilesInSessionFolder && [saveFilesInSessionFolder boolValue])
    {
        m_exporter->saveFilesInSessionFolder();
    }
    if (nil != syncLoading && [syncLoading boolValue])
    {
        m_exporter->setSyncLoading();
    }
    else
    {
        m_exporter->setLoadingDataOnScroll([AppConfiguration getLoadingDataOnScroll]);
    }

    if (nil != textMode && textMode.boolValue)
    {
        m_exporter->setTextMode();
        m_exporter->setExtName("txt");
        m_exporter->setTemplatesName("templates_txt");
    }
    
#ifndef NDEBUG
    if ([AppConfiguration isPdfMode])
    {
        m_exporter->setPdfMode();
    }
#endif
    m_exporter->supportsFilter([AppConfiguration getSupportingFilter]);
    
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
    
    [self.popupUsers addItemWithTitle:NSLocalizedString(@"txt-all-wechat-users", comment: "")];
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

- (void)setUIEnabled:(BOOL)enabled withCancellable:(BOOL)cancellable
{
    if (enabled)
    {
        self.view.window.styleMask |= NSClosableWindowMask;
        [self.progressBar stopAnimation:nil];
    }
    else
    {
        self.view.window.styleMask &= ~NSClosableWindowMask;
        [self.progressBar startAnimation:nil];
    }
    
    [self.popupBackup setEnabled:enabled];
    [self.btnOutput setEnabled:enabled];
    [self.btnBackup setEnabled:enabled];
    [self.btnExport setEnabled:enabled];
    [self.btnCancel setEnabled:!enabled && cancellable];
    [self.btnCancel setHidden:enabled];
    [self.btnQuit setHidden:!enabled];
    // [self.chkboxDesc setEnabled:enabled];
    // [self.chkboxTextMode setEnabled:enabled];
    // [self.chkboxSaveFilesInSessionFolder setEnabled:enabled];
}

- (void)onStart
{
    [self setUIEnabled:NO withCancellable:YES];
}

- (void)onComplete:(BOOL)cancelled
{
    [self setUIEnabled:YES withCancellable:YES];
    
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

- (void)showNewVersion:(NSString *)version withUrl:(NSString *)url
{
    NSAlert *alert = [[NSAlert alloc] init];
    
    NSString *message = [NSString stringWithFormat:NSLocalizedString(@"prompt-new-version-found", comment: ""), version];
    NSString *title = [NSRunningApplication currentApplication].localizedName;

    [alert addButtonWithTitle:NSLocalizedString(@"btn-yes", comment: "")];
    [alert addButtonWithTitle:NSLocalizedString(@"btn-no", comment: "")];
    alert.messageText = message;
    alert.window.title = title;
    [alert beginSheetModalForWindow:self.view.window completionHandler:^(NSModalResponse returnCode) {
        if (returnCode == NSModalResponseOK)
        {
            [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:url]];
        }
    }];
    
}
    
- (void)checkUpdate
{
    NSBundle *bundle = [NSBundle mainBundle];
    NSDictionary *info = [bundle infoDictionary];
    NSString *shortVersion = info[@"CFBundleShortVersionString"];
    NSString *build = info[@"CFBundleVersion"];
    
    __block NSString *version = [NSString stringWithFormat:@"%@.%@", shortVersion, build];
    __block typeof(self) __weak weakSelf = self;
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        
        NSString *userAgent = [HttpHelper standardUserAgent];
        Updater updater([version UTF8String]);
        updater.setUserAgent([userAgent UTF8String]);
        bool hasNewVersion = updater.checkUpdate();
        NSInteger lastChkUpdateTime = static_cast<NSInteger>(getUnixTimeStamp());
        [AppConfiguration setLastCheckUpdateTime:lastChkUpdateTime];
        
        if (!hasNewVersion)
        {
            return;
        }
        __strong typeof(weakSelf) strongSelf = weakSelf;  // strong by default
        if (nil == strongSelf)
        {
            return;
        }

        // update UI on the main thread
        dispatch_async(dispatch_get_main_queue(), ^{
            __strong typeof(weakSelf) strongSelf = weakSelf;  // strong by default
            if (strongSelf) {
                NSString *newVersion = [NSString stringWithUTF8String:updater.getNewVersion().c_str()];
                NSString *url = [NSString stringWithUTF8String:updater.getUpdateUrl().c_str()];
                
                [strongSelf showNewVersion:newVersion withUrl:url];
            }
        });
        
    });
}


@end
