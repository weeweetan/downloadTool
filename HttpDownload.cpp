//
// Created by tanwei on 19-2-23.
//
#include <cstdio>
#include <iostream>
#include <limits>
#include <new>
#include <thread>
#include "HttpDownload.h"

static char ascii_lower(char value)
{
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

static bool ascii_equal(const char *left, const char *right, size_t length)
{
    for (size_t i = 0; i < length; ++i)
        if (ascii_lower(left[i]) != ascii_lower(right[i])) return false;
    return true;
}

static bool safe_file_name(const std::string &value)
{
    if (value.empty() || value == "." || value == "..") return false;
    for (unsigned char character : value)
        if (character < 32 || character == 127 || character == '/' || character == '\\')
            return false;
    return true;
}

HttpDownload::HttpDownload(const string& url, const string& downloadPath, const unsigned int& threadNum) {
    this->url = url;
    this->threadNums = threadNum;
    curl_easy_setopt(curlHandle, CURLOPT_URL, this->url.c_str());
    initFileName();
    string realPath = downloadPath + fileName;
    FILE * temp = fopen(realPath.c_str(), "wb");
    if (temp)
    {
        downloadFileHandle = temp;
    }
}

bool HttpDownload::multiThreadDownload() {
    thread threads[threadNums];
    fileSize = getFileSize();

    if (fileSize <= 0)
    {
        cout << "get the file length error..." << endl;
        return false;
    }
    cout << "fileSize:"<<fileSize<<endl;

    long partSize = fileSize / threadNums;

    for (int i = 0; i <= threadNums; i++)
    {
        auto *threadData = new ThreadData ();

        if (i < threadNums)
        {
            threadData->startPos = i * partSize;
            threadData->endPos = (i + 1) * partSize - 1;
        }
        else
        {
            if (fileSize % threadNums != 0)
            {
                threadData->startPos = i * partSize;
                threadData->endPos = fileSize - 1;
            }
            else
                break;
        }

        CURL *curl = curl_easy_init ();

        threadData->curlHandle = curl;
        threadData->fileHandle = downloadFileHandle;

        char range[64] = { 0 };
        snprintf (range, sizeof (range), "%lld-%lld", threadData->startPos, threadData->endPos);

        curl_easy_setopt (curl, CURLOPT_URL, url.c_str ());
        curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, writeData);
        curl_easy_setopt (curl, CURLOPT_WRITEDATA, (void *) threadData);
        curl_easy_setopt (curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt (curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
        curl_easy_setopt (curl, CURLOPT_LOW_SPEED_TIME, 5L);
        curl_easy_setopt (curl, CURLOPT_RANGE, range);

        threads[i] = thread(executeThread, threadData);
    }

    for (int i = 0; i < threadNums; ++i) {
        threads[i].join();
    }

    fclose (downloadFileHandle);

    cout << "download succeed!" << endl;
    return true;
}

HttpDownload::~HttpDownload() {
    if (downloadFileHandle) {
        fclose(downloadFileHandle);
    }
}


void HttpDownload::initFileName() {
    curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curlHandle, CURLOPT_HEADERFUNCTION, parseHeader);
    curl_easy_setopt(curlHandle, CURLOPT_HEADERDATA, &fileName);

    curl_easy_perform(curlHandle);

    if (fileName.empty())
    {
        getNameFromUrl();
    }

}


size_t HttpDownload::parseHeader(void *ptr, size_t size, size_t nmemb, void *stream) {
    if (size == 0 || nmemb == 0) return 0;
    if (nmemb > std::numeric_limits<size_t>::max() / size || ptr == nullptr || stream == nullptr)
        return 0;
    const size_t bytes = size * nmemb;
    const char tag[] = "Content-Disposition:";
    if (bytes >= sizeof tag - 1 && ascii_equal(static_cast<const char *>(ptr), tag, sizeof tag - 1)) {
        const char *value = static_cast<const char *>(ptr) + sizeof tag - 1;
        try {
            std::string name;
            if (getNameFromTag(std::string(value, bytes - (sizeof tag - 1)), name))
                *static_cast<std::string *>(stream) = name;
        } catch (const std::bad_alloc &) {
            return 0;
        }
    }
    return bytes;
}

string HttpDownload::getNameFromUrl() {
    string tempUrl = url;
    unsigned long lastPosition = tempUrl.find_last_of("/");
    fileName = tempUrl.substr(lastPosition+1);
    return fileName;
}


bool HttpDownload::getNameFromTag(const std::string &cd, std::string &name) {
    const std::string field = cd.substr(0, cd.find_first_of("\r\n"));
    for (unsigned char character : field)
        if (character < 32 && character != '\t') return false;
    size_t position = 0;
    while (true) {
        const size_t separator = field.find(';', position);
        if (separator == std::string::npos) return false;
        position = separator + 1;
        while (position < field.size() && (field[position] == ' ' || field[position] == '\t')) ++position;
        const size_t key_start = position;
        while (position < field.size() && field[position] != '=' && field[position] != ';' &&
               field[position] != ' ' && field[position] != '\t') ++position;
        const size_t key_length = position - key_start;
        while (position < field.size() && (field[position] == ' ' || field[position] == '\t')) ++position;
        if (position == field.size() || field[position] != '=') continue;
        ++position;
        while (position < field.size() && (field[position] == ' ' || field[position] == '\t')) ++position;

        std::string candidate;
        if (position < field.size() && field[position] == '"') {
            ++position;
            bool closed = false;
            while (position < field.size()) {
                const char character = field[position++];
                if (character == '"') { closed = true; break; }
                if (character == '\\' && position < field.size()) candidate += field[position++];
                else candidate += character;
            }
            if (!closed) return false;
            while (position < field.size() && (field[position] == ' ' || field[position] == '\t')) ++position;
            if (position < field.size() && field[position] != ';') return false;
        } else {
            const size_t end = field.find(';', position);
            candidate = field.substr(position, end - position);
            while (!candidate.empty() && (candidate.back() == ' ' || candidate.back() == '\t'))
                candidate.pop_back();
            position = end == std::string::npos ? field.size() : end;
        }
        if (key_length == 8 && ascii_equal(field.data() + key_start, "filename", 8)) {
            if (!safe_file_name(candidate)) return false;
            name = candidate;
            return true;
        }
    }
}
