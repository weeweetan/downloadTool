# downloadTool

使用 C++11 和 libcurl 实现的 HTTP 分片下载练习。

## 构建与本地验证

需要 Linux / macOS、C++11 编译器、CMake 3.13+ 和 libcurl 开发文件。Ubuntu / Debian 可安装 `build-essential cmake libcurl4-openssl-dev`；macOS 可使用 Xcode Command Line Tools 提供的编译器和 libcurl，以及单独安装的 CMake。

```sh
git clone https://github.com/weeweetan/downloadTool.git
cd downloadTool
cmake -S . -B build-local -DCMAKE_BUILD_TYPE=Debug
cmake --build build-local --parallel 2
(cd build-local && ctest --output-on-failure)
```

生成物位于 `build-local/`。在另一台电脑上重新克隆并执行上述命令即可，不依赖仓库中历史构建目录记录的本机路径。

## 分片写入回调

`ThreadData.startPos` 是下一个待写位置，`endPos` 是分片最后一个字节的位置，二者使用闭区间约定。成功写完分片后，`startPos` 为 `endPos + 1`；后续非空写入会返回 0。超过剩余区间的整个数据块会被拒绝，已有文件内容保持不变。

`Download::writeData` 返回 `fwrite` 实际接受的字节数，位置只增加该字节数。空块返回 0；参数无效、字节数或位置计算溢出、偏移不能由 `fseek` 表示或定位失败时返回 0。检查、定位和写入共享一把互斥锁，使多个分片使用同一个 FILE 流时不会交错定位和写入。回调返回字节数的要求见 [libcurl 写回调文档](https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html)。

CTest 的 8 组用例使用临时文件和本地管道，验证字节计数、单字节边界、完成后重复调用、越界块、无效输入、定位和写入错误，以及并发写入。完整 HTTP 下载流程需要另行集成验证。

## 内存与未定义行为检查

```sh
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON
cmake --build build-sanitize --parallel 2
(cd build-sanitize && ctest --output-on-failure)
```

GitHub Actions 在 Linux GCC、Linux Clang 和 macOS Clang 上构建并运行上述回归；Clang 配置启用 AddressSanitizer 和 UndefinedBehaviorSanitizer，Linux 同时检查泄漏。
