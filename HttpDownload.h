//
// Created by tanwei on 19-2-21.
//

#ifndef DOWNLOADTOOL_HTTPDOWNLOAD_H
#define DOWNLOADTOOL_HTTPDOWNLOAD_H

#include "Download.h"

class HttpDownload : public Download
{
public:
    HttpDownload(const string &url, const string &downloadPath, const unsigned int& threadNum);
    ~HttpDownload();
    bool multiThreadDownload();

    /**
     * @function initFileName
     * @description 初始化文件名
     */
    void initFileName();

    /**
     * @function parseHeader
     * @description 解析http头
     * @param ptr 包含http头的指针
     * @param size 单位数据块大小
     * @param nmemb 数据块数量
     * @param stream 待填充文件名的 std::string 指针
     * @return 已处理的头部字节数；无效回调参数返回 0
     */
    static size_t parseHeader(void *ptr, size_t size, size_t nmemb, void *stream);

    /**
     * @function getNameFromTag
     * @description 从Content-Disposition字段中获取文件名
     * @param cd Content-Disposition 字段值（不含字段名）
     * @param name 成功时填充的文件名
     * @return 是否找到安全且非空的 filename 参数
     */
    static bool getNameFromTag(const std::string &cd, std::string &name);

    /**
     * @function getNameFromUrl
     * @description 从URL中解析文件名
     * @return 文件名
     */
    string getNameFromUrl();
private:
    unsigned int threadNums;
};


#endif //DOWNLOADTOOL_HTTPDOWNLOAD_H
