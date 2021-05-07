//
//  PdfConverter.h
//  WechatExporter
//
//  Created by Matthew on 2021/4/14.
//  Copyright © 2021 Matthew. All rights reserved.
//

#ifndef PdfConverter_h
#define PdfConverter_h

class PdfConverter
{
public:
    virtual bool makeUserDirectory(const std::string& dirName) = 0;
    virtual bool convert(const std::string& htmlPath, const std::string& pdfPath) = 0;
    virtual ~PdfConverter() {}
};

#endif /* PdfConverter_h */
