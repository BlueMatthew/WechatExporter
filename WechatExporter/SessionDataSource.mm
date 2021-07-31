//
//  SessionDataSource.m
//  WechatExporter
//
//  Created by Matthew on 2021/2/1.
//  Copyright © 2021 Matthew. All rights reserved.
//

#import "SessionDataSource.h"

@implementation SessionItem

- (NSComparisonResult)orgIndexCompare:(SessionItem *)sessionItem ascending:(BOOL)ascending
{
    if (self.orgIndex < sessionItem.orgIndex)
        return NSOrderedAscending;
    else if (self.orgIndex > sessionItem.orgIndex)
        return NSOrderedDescending;
    
    return NSOrderedSame;
}

- (NSComparisonResult)displayNameCompare:(SessionItem *)sessionItem ascending:(BOOL)ascending
{
    NSComparisonResult result = [self.displayName caseInsensitiveCompare:sessionItem.displayName];
    if (result == NSOrderedSame)
    {
        result = [self orgIndexCompare:sessionItem ascending:ascending];
    }
    else
    {
        if (!ascending)
        {
            result = (result == NSOrderedAscending) ? NSOrderedDescending : NSOrderedAscending;
        }
    }
    
    return result;
}

- (NSComparisonResult)recordCountCompare:(SessionItem *)sessionItem ascending:(BOOL)ascending
{
    if (self.recordCount < sessionItem.recordCount)
        return ascending ? NSOrderedAscending : NSOrderedDescending;
    else if (self.recordCount > sessionItem.recordCount)
        return ascending ? NSOrderedDescending : NSOrderedAscending;
    
    return [self orgIndexCompare:sessionItem ascending:ascending];
}

- (NSComparisonResult)userIndexCompare:(SessionItem *)sessionItem ascending:(BOOL)ascending
{
    if (self.userIndex < sessionItem.userIndex)
        return ascending ? NSOrderedAscending : NSOrderedDescending;
    else if (self.userIndex > sessionItem.userIndex)
        return ascending ? NSOrderedDescending : NSOrderedAscending;
    
    return [self orgIndexCompare:sessionItem ascending:ascending];
}

@end

@interface SessionDataSource()
{
    const std::vector<std::pair<Friend, std::vector<Session>>> *m_usersAndSessions;
    NSInteger m_indexOfSelectedUser;
    NSArray<SessionItem *> *m_sessions;
}
@end

@implementation SessionDataSource

- (instancetype)init
{
    if (self = [super init])
    {
        m_usersAndSessions = NULL;
        m_indexOfSelectedUser = -1;
        m_sessions = nil;
    }
    
    return self;
}

- (void)getSelectedUserAndSessions:(std::map<std::string, std::map<std::string, void *>>&)usersAndSessions
{
    for (SessionItem *sessionItem in m_sessions)
    {
        if (sessionItem.checked)
        {
            std::string usrName = [sessionItem.usrName UTF8String];
            std::string sessionUsrName = [sessionItem.sessionUsrName UTF8String];
            
            std::map<std::string, std::map<std::string, void *>>::iterator it = usersAndSessions.find(usrName);
            if (it == usersAndSessions.end())
            {
                it = usersAndSessions.insert(usersAndSessions.end(), std::pair<std::string, std::map<std::string, void *>>(usrName, std::map<std::string, void *>()));
            }
            
            it->second.insert(std::pair<std::string, void *>(sessionUsrName, NULL));
        }
    }
}

- (void)loadData:(const std::vector<std::pair<Friend, std::vector<Session>>> *)usersAndSessions withAllUsers:(BOOL)allUsers indexOfSelectedUser:(NSInteger)indexOfSelectedUser
{
    m_usersAndSessions = usersAndSessions;
    m_indexOfSelectedUser = indexOfSelectedUser;
    m_sessions = nil;
    
    if (!allUsers && indexOfSelectedUser == -1)
    {
        return;
    }
    
    NSInteger orgIndex = 0;
    NSInteger userIndex = 0;
    
    NSMutableArray<SessionItem *> *sessions = [NSMutableArray<SessionItem *> array];
    for (std::vector<std::pair<Friend, std::vector<Session>>>::const_iterator it = usersAndSessions->cbegin(); it != usersAndSessions->cend(); ++it, ++userIndex)
    {
        if (!allUsers)
        {
            if (userIndex != indexOfSelectedUser)
            {
                continue;
            }
        }
        
        for (std::vector<Session>::const_iterator it2 = it->second.cbegin(); it2 != it->second.cend(); ++it2, ++orgIndex)
        {
            SessionItem *sessionItem = [[SessionItem alloc] init];
            sessionItem.orgIndex = orgIndex;
            sessionItem.userIndex = userIndex;
            sessionItem.checked = YES;
            sessionItem.displayName = [NSString stringWithUTF8String:it2->getDisplayName().c_str()];
            if (it2->isDeleted())
            {
                sessionItem.displayName = [sessionItem.displayName stringByAppendingString:NSLocalizedString(@"session-deleted", comment: "")];
            }
            sessionItem.sessionUsrName = [NSString stringWithUTF8String:it2->getUsrName().c_str()];
            sessionItem.recordCount = it2->getRecordCount();
            sessionItem.usrName = [NSString stringWithUTF8String:it->first.getUsrName().c_str()];
            sessionItem.userDisplayName = [NSString stringWithUTF8String:it->first.getDisplayName().c_str()];
            
            [sessions addObject:sessionItem];
        }
    }
    
    m_sessions = sessions;
}

- (void)checkAllSessions:(BOOL)checked
{
    for (NSInteger idx = 0; idx < m_sessions.count; ++idx)
    {
        SessionItem *sessionItem = [m_sessions objectAtIndex:idx];
        sessionItem.checked = checked;
    }
}

- (NSControlStateValue)updateCheckStateAtRow:(NSInteger)row
{
    if (row < m_sessions.count)
    {
        SessionItem *sessionItem = [m_sessions objectAtIndex:row];
        if (sessionItem != nil)
        {
            sessionItem.checked = !sessionItem.checked;
        }
        
        BOOL checked = sessionItem.checked;
        BOOL allSame = YES;
        
        for (NSInteger idx = 0; idx < m_sessions.count; ++idx)
        {
            SessionItem *sessionItem = [m_sessions objectAtIndex:idx];
            if (sessionItem.checked != checked)
            {
                allSame = NO;
                break;
            }
        }
        
        return allSame ? (checked ? NSControlStateValueOn : NSControlStateValueOff) : NSControlStateValueMixed;
    }
    
    return NSControlStateValueMixed;
}

- (void)clearSort
{
    m_sessions = [m_sessions sortedArrayUsingComparator: ^(SessionItem *item1, SessionItem *item2) {
        return [item1 orgIndexCompare:item2 ascending:YES];
    }];
}

- (void)sortOnDisplayName:(BOOL)ascending
{
    __block BOOL localAsc = ascending;
    
    m_sessions = [m_sessions sortedArrayUsingComparator: ^(SessionItem *item1, SessionItem *item2) {
        return [item1 displayNameCompare:item2 ascending:localAsc];
    }];
}

- (void)sortOnRecordCount:(BOOL)ascending
{
    __block BOOL localAsc = ascending;
    
    m_sessions = [m_sessions sortedArrayUsingComparator: ^(SessionItem *item1, SessionItem *item2) {
        return [item1 recordCountCompare:item2 ascending:localAsc];
    }];
}

- (void)sortOnUserName:(BOOL)ascending
{
    __block BOOL localAsc = ascending;
    
    m_sessions = [m_sessions sortedArrayUsingComparator: ^(SessionItem *item1, SessionItem *item2) {
        return [item1 userIndexCompare:item2 ascending:localAsc];
    }];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    return m_sessions.count;
}

/*
- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    return nil;
}
*/

- (void)tableView:(NSTableView *)tableView sortDescriptorsDidChange:(NSArray<NSSortDescriptor *> *)oldDescriptors
{
    if (tableView.sortDescriptors.count > 0)
    {
        NSSortDescriptor *sortDescriptor = [tableView.sortDescriptors objectAtIndex:0];
        if ([sortDescriptor.key isEqualToString:@"columnName"])
        {
            [self sortOnDisplayName:sortDescriptor.ascending];
        }
        else if ([sortDescriptor.key isEqualToString:@"columnRecordCount"])
        {
            [self sortOnRecordCount:sortDescriptor.ascending];
        }
        else if ([sortDescriptor.key isEqualToString:@"columnUser"])
        {
            [self sortOnUserName:sortDescriptor.ascending];
        }
    }
    else
    {
        [self clearSort];
    }
    
    [tableView reloadData];
}

- (void)bindCellView:(NSTableCellView *)cellView atRow:(NSInteger)row andColumnId:(NSString *)identifier
{
    SessionItem *sessionItem = [m_sessions objectAtIndex:row];
    
    if ([identifier isEqualToString:@"columnCheck"])
    {
        NSButton *btn = (NSButton *)cellView.subviews.firstObject;
        if (btn)
        {
            btn.state = sessionItem.checked;
        }
    }
    else if([identifier isEqualToString:@"columnName"])
    {
        cellView.textField.stringValue = sessionItem.displayName;
    }
    else if([identifier isEqualToString:@"columnRecordCount"])
    {
        cellView.textField.stringValue = [NSString stringWithFormat:@"%ld", (long)sessionItem.recordCount];
    }
    else if([identifier isEqualToString:@"columnUser"])
    {
        cellView.textField.stringValue = sessionItem.userDisplayName;
    }
}


@end
