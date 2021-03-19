//
//  AppConfiguration.h
//  WechatExporter
//
//  Created by Matthew on 2021/3/18.
//  Copyright © 2021 Matthew. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN


typedef NS_ENUM(NSInteger, OUTPUT_FORMAT)
{
    OUTPUT_FORMAT_HTML = 0,
    OUTPUT_FORMAT_TEXT,
    OUTPUT_FORMAT_LAST
};

@interface AppConfiguration : NSObject

+ (void)setDescOrder:(BOOL)descOrder;
+ (BOOL)getDescOrder;
+ (NSInteger)getOutputFormat;
+ (BOOL)isHtmlMode;
+ (BOOL)isTextMode;
+ (void)setOutputFormat:(NSInteger)outputFormat;
+ (void)setSavingInSession:(BOOL)savingInSession;
+ (BOOL)getSavingInSession;

+ (void)setAsyncLoading:(BOOL)asyncLoading;
+ (BOOL)getAsyncLoading;
   
+ (void)setLastOutputDir:(NSString *)outputDir;
+ (NSString *)getLastOrDefaultOutputDir;
+ (NSString *)getDefaultOutputDir;

+ (void)setLastBackupDir:(NSString *)backupDir;
+ (NSString *)getLastBackupDir;
+ (NSString *)getDefaultBackupDir:(BOOL)checkExistence; // YES

+ (NSInteger)getLastCheckUpdateTime;
+ (void)setLastCheckUpdateTime;
+ (void)setLastCheckUpdateTime:(NSInteger)lastCheckUpdateTime;

+ (void)setCheckingUpdateDisabled:(BOOL)disabled;
+ (BOOL)isCheckingUpdateDisabled;

+ (void)setLoadingDataOnScroll; // YES
+ (void)setLoadingDataOnScroll:(BOOL)loadingDataOnScroll;
+ (BOOL)getLoadingDataOnScroll;

+ (void)setSupportingFilter:(BOOL)supportingFilter;
+ (BOOL)getSupportingFilter;

@end

NS_ASSUME_NONNULL_END
