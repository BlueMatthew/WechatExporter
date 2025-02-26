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
    OUTPUT_FORMAT_PDF,
    OUTPUT_FORMAT_LAST
};

@interface AppConfiguration : NSObject

+ (void)setDescOrder:(BOOL)descOrder;
+ (BOOL)getDescOrder;
+ (BOOL)IsPdfSupported;
+ (NSInteger)getOutputFormat;
+ (BOOL)isHtmlMode;
+ (BOOL)isTextMode;
+ (BOOL)isPdfMode;
+ (void)setOutputFormat:(NSInteger)outputFormat;
+ (void)setSavingInSession:(BOOL)savingInSession;
+ (BOOL)getSavingInSession;

+ (void)setSyncLoading;
+ (BOOL)getSyncLoading;

+ (void)setIncrementalExporting:(BOOL)incrementalExp;
+ (BOOL)getIncrementalExporting;
   
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

+ (void)setLoadingDataOnScroll;
+ (BOOL)getLoadingDataOnScroll;

+ (void)setNormalPagination;
+ (BOOL)getNormalPagination;
+ (void)setPaginationOnYear;
+ (BOOL)getPaginationOnYear;
+ (void)setPaginationOnMonth;
+ (BOOL)getPaginationOnMonth;

+ (void)setSupportingFilter:(BOOL)supportingFilter;
+ (BOOL)getSupportingFilter;

+ (void)setOutputDebugLogs:(BOOL)dbgLogs;
+ (BOOL)outputDebugLogs;

+ (void)setIncludingSubscriptions:(BOOL)includingSubscriptions;
+ (BOOL)includeSubscriptions;

+ (void)setOpenningFolderAfterExp:(BOOL)openningFolderAfterExp;
+ (BOOL)getOpenningFolderAfterExp;

+ (void)setSkipGuide:(BOOL)skipGuide;
+ (BOOL)getSkipGuide;

+ (void)upgrade;
+ (uint64_t)buildOptions;


@end

NS_ASSUME_NONNULL_END
