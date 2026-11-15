//
// Created by tanwei on 19-2-23.
//

#ifndef DOWNLOADTOOL_DOWNLOAD_H
#define DOWNLOADTOOL_DOWNLOAD_H

#include<curl/curl.h>
#include <cstdio>
#include <string>
#include <mutex>
using namespace std;
typedef struct ThreadData {
    CURL *curlHandle;
    FILE *fileHandle;
    long long startPos; // Next byte to write; endPos + 1 after completion.
    long long endPos;   // Inclusive end of this thread's assigned range.
} ThreadData;

class Download {
public:
    Download();
    virtual ~Download();
    virtual bool multiThreadDownload();

    /**
     * @function getFileSize
     * @description 获取需要下载的文件大小
     * @return 文件大小
     */
    long long getFileSize();

    /**
     * @function writeData
     * @description 写数据到文件
     * @param ptr 数据指针
     * @param size  数据块大小
     * @param nmemb 数据量
     * @param threadData ThreadData类型的指针
     * @return fwrite 实际接受的字节数；拒绝越界块或定位失败时返回 0
     */
    static size_t writeData(void *ptr, size_t size, size_t nmemb, void *threadData);

    /**
     * @function executeThread
     * @description 执行curl函数
     * @param arg ThreadData类型指针
     */
    static void executeThread(void *arg);
protected:
    //全局curl指针，用来获取文件大小和文件名
    CURL *curlHandle;
    //需要下载的文件的url
    string url;
    //文件名
    string fileName;
    //文件大小
    long long fileSize;
    //下载文件指针
    FILE * downloadFileHandle;

};

#endif //DOWNLOADTOOL_DOWNLOAD_H
