//
// Created by tanwei on 19-2-23.
//
#include <iostream>
#include <climits>
#include <limits>
#include "Download.h"
static mutex writeMutex;
Download::Download() {
    curlHandle = curl_easy_init();
}

Download::~Download() {
    cout<<"~Download"<< endl;
    curl_easy_cleanup(curlHandle);
}

bool Download::multiThreadDownload() { return false; }

long long Download::getFileSize() {
    double FileLength = 0;
    curl_easy_setopt (curlHandle, CURLOPT_URL, url.c_str());
    curl_easy_setopt (curlHandle, CURLOPT_HEADER, 1);
    curl_easy_setopt (curlHandle, CURLOPT_NOBODY, 1);
    if (curl_easy_perform (curlHandle) == CURLE_OK)
    {
        curl_easy_getinfo (curlHandle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &FileLength);
    }
    else
    {
        FileLength = -1;
    }
    return static_cast<long long>(FileLength);
}


size_t Download::writeData(void *ptr, size_t size, size_t count, void *threadData)
{
    if (size == 0 || count == 0) return 0;
    if (ptr == nullptr || threadData == nullptr ||
        count > numeric_limits<size_t>::max() / size) return 0;
    const size_t bytes = size * count;
    auto *data = static_cast<ThreadData *>(threadData);
    lock_guard<mutex> guard(writeMutex);
    if (data->fileHandle == nullptr || data->startPos < 0 ||
        data->endPos < data->startPos || data->startPos > LONG_MAX) return 0;

    const unsigned long long remaining =
        static_cast<unsigned long long>(data->endPos - data->startPos) + 1ULL;
    if (bytes > remaining ||
        bytes > static_cast<unsigned long long>(LLONG_MAX - data->startPos)) return 0;
    if (fseek(data->fileHandle, static_cast<long>(data->startPos), SEEK_SET) != 0)
        return 0;
    // fwrite with a one-byte element size reports bytes, including short writes.
    const size_t written = fwrite(ptr, 1, bytes, data->fileHandle);
    data->startPos += static_cast<long long>(written);
    return written;
}

void Download::executeThread(void *arg) {
    auto *threadData = (ThreadData *) arg;

    curl_easy_perform (threadData->curlHandle);
    curl_easy_cleanup (threadData->curlHandle);
    delete threadData;


}

