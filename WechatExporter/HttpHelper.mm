//
//  HttpHelper.m
//  WechatExporter
//
//  Created by Matthew on 2021/3/9.
//  Copyright © 2021 Matthew. All rights reserved.
//

#import "HttpHelper.h"

#if defined(__ppc__) || defined(__ppc64__)
#define PROCESSOR "PPC"
#elif defined(__i386__) || defined(__x86_64__)
#define PROCESSOR "Intel"
#else
#error Unknown architecture
#endif

@implementation HttpHelper

static inline int callGestalt(OSType selector)
{
    SInt32 value = 0;
    Gestalt(selector, &value);
    return value;
}

// Uses underscores instead of dots because if "4." ever appears in a user agent string, old DHTML libraries treat it as Netscape 4.
+ (NSString *)macOSXVersionString
{
    // Can't use -[NSProcessInfo operatingSystemVersionString] because it has too much stuff we don't want.
    int major = callGestalt(gestaltSystemVersionMajor);
    // ASSERT(major);

    int minor = callGestalt(gestaltSystemVersionMinor);
    int bugFix = callGestalt(gestaltSystemVersionBugFix);
    if (bugFix)
        return [NSString stringWithFormat:@"%d_%d_%d", major, minor, bugFix];
    if (minor)
        return [NSString stringWithFormat:@"%d_%d", major, minor];
    return [NSString stringWithFormat:@"%d", major];
}

+ (NSString *)userVisibleWebKitVersionString
{
    // If the version is 4 digits long or longer, then the first digit represents
    // the version of the OS. Our user agent string should not include this first digit,
    // so strip it off and report the rest as the version. <rdar://problem/4997547>
    NSString *fullVersion = [[NSBundle bundleForClass:NSClassFromString(@"WKView")] objectForInfoDictionaryKey:(NSString *)kCFBundleVersionKey];
    NSRange nonDigitRange = [fullVersion rangeOfCharacterFromSet:[[NSCharacterSet decimalDigitCharacterSet] invertedSet]];
    if (nonDigitRange.location == NSNotFound && [fullVersion length] >= 4)
        return [fullVersion substringFromIndex:1];
    if (nonDigitRange.location != NSNotFound && nonDigitRange.location >= 4)
        return [fullVersion substringFromIndex:1];
    return fullVersion;
}

+ (NSString *)standardUserAgent
{
    // https://opensource.apple.com/source/WebKit2/WebKit2-7536.26.14/UIProcess/mac/WebPageProxyMac.mm.auto.html
    NSString *osVersion = [self macOSXVersionString];
    NSString *webKitVersion = [self userVisibleWebKitVersionString];
    
    return [NSString stringWithFormat:@"Mozilla/5.0 (Macintosh; %@ Mac OS X %@) AppleWebKit/%@ (KHTML, like Gecko) ", @PROCESSOR, osVersion, webKitVersion];
}


@end
