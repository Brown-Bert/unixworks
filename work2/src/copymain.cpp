#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "copyfile.h"
using namespace std;
int test(string sourceFileName, string destination, int thread_num,
         int blocksize, int bufsize) {
  auto start = std::chrono::high_resolution_clock::now();
  CopyFileClass copyFile(thread_num, blocksize);
  copyFile.initFilePath(sourceFileName, destination);
  copyFile.initMutexAndCondition();
  copyFile.copyFile(bufsize);
  copyFile.destroyMutexAndCondition();
  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  std::cout << duration.count() << " ms" << std::endl;
  // 测试两个文件是否相同
  // bool res = compare(sourceFileName, destination);
  // if (res) {
  //   std::cout << "相同" << endl;
  // } else {
  //   std::cout << "不相同" << endl;
  // }
  return duration.count();
}
int main() {
  std::string sourceFileName =
      "/home/yzx/vscode/workspace/unixworks/work2/src/test.txt";
  std::string destination =
      "/home/yzx/vscode/workspace/unixworks/work2/src/testcopy.txt";
  FILE* fp =
      fopen("/home/yzx/vscode/workspace/unixworks/work2/src/testdata.txt", "w");
  if (fp == NULL) {
    perror("fopen()");
    exit(1);
  }
  for (int i = 1; i <= 30; i++) {
    int res = test(sourceFileName, destination, i, 64 * 1024 * 1024, 10);
    fprintf(fp, "%d\n", res);
  }
  fclose(fp);
  // test(sourceFileName, destination, 3, 1024);
  return 0;
}