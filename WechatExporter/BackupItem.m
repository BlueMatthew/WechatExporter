//
//  BackupItem.m
//  WechatExporter
//
//  Created by Matthew on 2020/9/29.
//  Copyright © 2020 Matthew. All rights reserved.
//

#import "BackupItem.h"

@implementation BackupItem

- (NSString *)description
{
    return [NSString stringWithFormat:@"%@ (%@)", self.displayName, self.lastBackupDate.description];
}

@end
