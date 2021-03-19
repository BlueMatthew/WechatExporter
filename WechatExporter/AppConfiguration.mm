//
//  AppConfiguration.m
//  WechatExporter
//
//  Created by Matthew on 2021/3/18.
//  Copyright © 2021 Matthew. All rights reserved.
//

#import "AppConfiguration.h"
#include "Utils.h"

@implementation AppConfiguration

+ (void)setDescOrder:(BOOL)descOrder
{
    [[NSUserDefaults standardUserDefaults] setBool:descOrder forKey:@"DescOrder"];
}

+ (BOOL)getDescOrder
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:@"DescOrder"];
}

+ (NSInteger)getOutputFormat
{
    return [[NSUserDefaults standardUserDefaults] integerForKey:@"OutputFormat"];
}

+ (BOOL)isHtmlMode
{
    return [self getOutputFormat] == OUTPUT_FORMAT_HTML;
}

+ (BOOL)isTextMode
{
    return [self getOutputFormat] == OUTPUT_FORMAT_TEXT;
}

+ (void)setOutputFormat:(NSInteger)outputFormat
{
    [[NSUserDefaults standardUserDefaults] setInteger:outputFormat forKey:@"OutputFormat"];
}

+ (void)setSavingInSession:(BOOL)savingInSession
{
    [[NSUserDefaults standardUserDefaults] setBool:(!savingInSession) forKey:@"UniversalFolder"];
}

+ (BOOL)getSavingInSession
{
    return ![[NSUserDefaults standardUserDefaults] boolForKey:@"UniversalFolder"];
}

+ (void)setAsyncLoading:(BOOL)asyncLoading
{
    [[NSUserDefaults standardUserDefaults] setBool:(!asyncLoading) forKey:@"SyncLoading"];
}

+ (BOOL)getAsyncLoading
{
    return ![[NSUserDefaults standardUserDefaults] boolForKey:@"SyncLoading"];
}
   
+ (void)setLastOutputDir:(NSString *)outputDir
{
    [[NSUserDefaults standardUserDefaults] setObject:outputDir forKey:@"OutputDir"];
}

+ (NSString *)getLastOrDefaultOutputDir
{
    NSString *outputDir = [[NSUserDefaults standardUserDefaults] stringForKey:@"OutputDir"];
    if (nil != outputDir && outputDir.length > 0)
    {
        return outputDir;
    }

    return [self getDefaultOutputDir];
}

+ (NSString *)getDefaultOutputDir
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
    
    return [NSString pathWithComponents:components];
}

+ (void)setLastBackupDir:(NSString *)backupDir
{
    [[NSUserDefaults standardUserDefaults] setObject:backupDir forKey:@"BackupDir"];
}

+ (NSString *)getLastBackupDir
{
    return [[NSUserDefaults standardUserDefaults] stringForKey:@"BackupDir"];
}

+ (NSString *)getDefaultBackupDir:(BOOL)checkExistence // YES
{
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSURL *appSupport = [fileManager URLForDirectory:NSApplicationSupportDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:NO error:nil];
    
    NSArray *components = @[[appSupport path], @"MobileSync", @"Backup"];
    NSString *backupDir = [NSString pathWithComponents:components];
    if (!checkExistence)
    {
        return backupDir;
    }
    
    BOOL isDir = NO;
    if ([fileManager fileExistsAtPath:backupDir isDirectory:&isDir] && isDir)
    {
        return backupDir;
    }
    
    return nil;
}

+ (NSInteger)getLastCheckUpdateTime
{
    return [[NSUserDefaults standardUserDefaults] integerForKey:@"LastChkUpdateTime"];
}

+ (void)setLastCheckUpdateTime
{
    [self setLastCheckUpdateTime:0];
}

+ (void)setLastCheckUpdateTime:(NSInteger)lastCheckUpdateTime
{
    if (0 == lastCheckUpdateTime)
    {
        lastCheckUpdateTime = static_cast<NSInteger>(getUnixTimeStamp());
    }
    [[NSUserDefaults standardUserDefaults] setInteger:lastCheckUpdateTime forKey:@"LastChkUpdateTime"];
}

+ (void)setCheckingUpdateDisabled:(BOOL)disabled
{
    [[NSUserDefaults standardUserDefaults] setBool:disabled forKey:@"ChkUpdateDisabled"];
}

+ (BOOL)isCheckingUpdateDisabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:@"ChkUpdateDisabled"];
}

+ (void)setLoadingDataOnScroll // YES
{
    [self setLoadingDataOnScroll:YES];
}

+ (void)setLoadingDataOnScroll:(BOOL)loadingDataOnScroll
{
    [[NSUserDefaults standardUserDefaults] setBool:(!loadingDataOnScroll) forKey:@"LoadingDataOnce"];
}

+ (BOOL)getLoadingDataOnScroll
{
    return ![[NSUserDefaults standardUserDefaults] boolForKey:@"LoadingDataOnce"];
}

+ (void)setSupportingFilter:(BOOL)supportingFilter
{
    [[NSUserDefaults standardUserDefaults] setBool:(!supportingFilter) forKey:@"NoFilter"];
}

+ (BOOL)getSupportingFilter
{
    return ![[NSUserDefaults standardUserDefaults] boolForKey:@"NoFilter"];
}


@end
