//
//  BackupItem.h
//  WechatExporter
//
//  Created by Matthew on 2020/9/29.
//  Copyright © 2020 Matthew. All rights reserved.
//

#import <Foundation/Foundation.h>

#ifndef BackupItem_h
#define BackupItem_h


@interface BackupItem : NSObject

// public string DeviceName;

// public bool custom = false;         // backup loaded from a custom directory

@property (strong) NSString *displayName;
@property (strong) NSString *deviceName;
@property (strong) NSDate *lastBackupDate;
@property (strong) NSString *backupPath;


@end


#endif /* BackupItem_h */
