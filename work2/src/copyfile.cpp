#include "copyfile.h"

#include <fcntl.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <queue>

int writeBlockedSize = 0;  // 用于统计写阻塞的线程个数
int readBlockedSize = 0;   // 用于统计写阻塞的线程个数
int _taskSize = 0;         // 目前任务队列中实际上有多少个任务
// typedef struct node {
//   char *_buf;     // 存放数据的buf
//   int _realsize;  // 数据的实际大小
//   int _index;     // 数据块的顺序
// } node;
pthread_mutex_t _mut;  // 文件上锁
off_t _offset = 0;     // 写文件的时候需要同步文件偏移量
off_t sumSize = 0;     // 用于累积每一轮中的增量
int count = 0;  // 统计一轮中有多少个线程已经完成写入的操作
queue<node *> _tasks;  // 任务队列
bool _fileFlag = false;  // false表示这个文件还没有读完，true表示这个文件读完了

pthread_cond_t
    _condStoD;  // 源文件读取线程往目的文件写入线程发通知所需的条件变量
pthread_cond_t
    _condDtoS;  // 目的文件写入线程往源文件读取线程发送通知所需的条件变量
pthread_spinlock_t
    sp;  // 设置一个自旋锁用于通知多个线程在一轮操作中已经全部完成

typedef struct conveyData {
  int _threadNum;       // 表示开几个写线程
  int _blockSize;       // 表示一个块的大小 以字节为单位
  string _souFileName;  // 源文件位置
  string _desFileName;  // 目的文件位置
  int fd;               // 文件描述符
  int bufsize;
} conveyData;

CopyFileClass::CopyFileClass(int _threadNum, int _blockSize) {
  this->_threadNum = _threadNum;
  this->_blockSize = _blockSize;
}

void CopyFileClass::initFilePath(string _souFileName, string _desFileName) {
  this->_souFileName = _souFileName;
  this->_desFileName = _desFileName;
}
/**
    初始化锁
  */
void CopyFileClass::initMutexAndCondition() {
  pthread_mutex_init(&(_mut), NULL);
  pthread_cond_init(&(_condStoD), NULL);
  pthread_cond_init(&(_condDtoS), NULL);
  pthread_spin_init(&(sp), 0);
  // 把数据初始化
  _taskSize = 0;
  _offset = 0;
  sumSize = 0;
  count = 0;
  _fileFlag = false;
  writeBlockedSize = 0;
  readBlockedSize = 0;
}

/**
  销毁锁
*/
void CopyFileClass::destroyMutexAndCondition() {
  pthread_mutex_destroy(&(_mut));
  pthread_cond_destroy(&(_condStoD));
  pthread_cond_destroy(&(_condDtoS));
  pthread_spin_destroy(&(sp));
}

/**
    读取一块任务
    @parameter fd: 文件描述符
    @return 读取成功与否
  */
static bool readOneBlock(int fd, node *oneNode, int _blockSize) {
  oneNode->_buf = (char *)malloc(_blockSize);
  if (oneNode->_buf == NULL) {
    return false;
  }
  int realSize;
  realSize = read(fd, oneNode->_buf, _blockSize);
  if (realSize == -1) {
    perror("read()");
    return false;
  }
  oneNode->_realsize = realSize;
  // printf("size: %d\n", realSize);
  return true;
}

/**
    开启线程，打开文件往任务队列中按照数据块的大小往里面写
  */
static void *openSouFileAndRead(void *p) {
  conveyData *data = (conveyData *)p;
  int fd;
  fd = open(data->_souFileName.c_str(), O_RDONLY);
  if (fd < 0) {
    perror("open()");
    // 文件打开异常，直接退出进程
    exit(1);
  }
  struct timespec exceedTime;
  exceedTime.tv_nsec = 0;
  exceedTime.tv_sec = 1;
  while (1) {
    pthread_mutex_lock(&(_mut));
    readBlockedSize++;
    pthread_cond_wait(&(_condDtoS), &(_mut));
    // puts("789");
    readBlockedSize--;
    int flag = 1;
    if (_taskSize == 0) {
      // 说明任务队列中已经没有任务了，需要往任务队列中放任务
      for (int i = 0; i < data->bufsize; i++) {
        node *oneNode = new node();
        oneNode->_index = i;
        bool res;
        while (1) {
          res = readOneBlock(fd, oneNode, data->_blockSize);
          if (res) {
            break;
          }
        }
        if (oneNode->_realsize == 0) {
          // 文件读完了
          flag = 0;
          break;
        } else {
          _tasks.push(oneNode);
          _taskSize++;
          // printf("size: %d\n", _taskSize);
          // puts("input task");
        }
      }
    }
    pthread_mutex_unlock(&(_mut));
    // 当所有的线程都进入阻塞队列才发送广播
    while (1) {
      // std::cout << "write: " << writeBlockedSize << std::endl;
      if (writeBlockedSize == data->_threadNum) {
        // std::cout << "blockedSize: " << writeBlockedSize << std::endl;
        pthread_mutex_lock(&(_mut));
        pthread_cond_broadcast(&(_condStoD));
        if (flag == 0) _fileFlag = true;
        pthread_mutex_unlock(&(_mut));
        break;
      }
    }
    if (flag == 0) break;
  }
  close(fd);
  // puts("jh");
  pthread_exit(NULL);
}

/**
  开启写线程， 从任务队列中拿数据写入目的文件
*/
static void *openDesFileAndWrite(void *p) {
  conveyData *data = (conveyData *)p;
  int fd = data->fd;
  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);
  struct timespec exceedTime;
  exceedTime.tv_nsec = now.tv_nsec;
  exceedTime.tv_sec = now.tv_sec + 1;
  // printf("%d\n", gettid());
  while (1) {
    pthread_mutex_lock(&(_mut));
    writeBlockedSize++;
    pthread_cond_wait(&(_condStoD), &(_mut));
    // printf("%d\n", gettid());
    // std::cout << "write: " << writeBlockedSize << std::endl;
    writeBlockedSize--;
    while (1) {
      if (_taskSize == 0) {
        pthread_spin_lock(&(sp));
        count++;
        pthread_spin_unlock(&(sp));
        if (count == data->_threadNum) {
          if (!_fileFlag) {
            // 发送信号
            pthread_mutex_unlock(&(_mut));
            while (1) {
              if (readBlockedSize == 1) {
                // cout << "read: " << readBlockedSize << endl;
                pthread_mutex_lock(&(_mut));
                pthread_cond_signal(&_condDtoS);
                pthread_mutex_unlock(&(_mut));
                break;
              }
            }
            pthread_mutex_lock(&(_mut));
          }
          _offset += sumSize;
          sumSize = 0;
          count = 0;
        }
        break;
      } else {
        node *oneNode = _tasks.front();
        _tasks.pop();
        // printf("tasksize: %d\n", _taskSize);
        _taskSize--;
        sumSize += oneNode->_realsize;
        pthread_mutex_unlock(&(_mut));
        // lseek(fd, _offset + oneNode._index * data->_blockSize, SEEK_SET);
        // write(fd, oneNode._buf, oneNode._realsize);
        pwrite(fd, oneNode->_buf, oneNode->_realsize,
               _offset + oneNode->_index * data->_blockSize);

        // 释放堆上的内存
        free(oneNode->_buf);
        free(oneNode);
        // puts("output task");
      }
      pthread_mutex_lock(&(_mut));
    }
    pthread_mutex_unlock(&(_mut));
    if (_fileFlag) {
      break;
    }
  }
  // printf("sd:%d\n", gettid());
  pthread_exit(NULL);
}

void CopyFileClass::copyFile(int bufsize) {
  // 开启线程，打开文件往任务队列中按照数据块的大小往里面写
  pthread_t souTid;
  int res;
  conveyData resData;
  resData._blockSize = this->_blockSize;
  resData._threadNum = this->_threadNum;
  resData._desFileName = this->_desFileName;
  resData._souFileName = this->_souFileName;
  resData.bufsize = bufsize;
  res = pthread_create(&souTid, NULL, openSouFileAndRead, &resData);
  if (res) {
    fprintf(stderr, "pthread_create: %s", strerror(res));
    exit(1);
  }
  // 开启写线程，从任务队列里面拿取任务写入目标文件
  int fd;
  fd = open(this->_desFileName.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (fd < 0) {
    perror("open()");
    // 文件打开异常，直接退出进程
    exit(1);
  }
  resData.fd = fd;
  pthread_t desTid[this->_threadNum];
  for (int i = 0; i < this->_threadNum; i++) {
    res = pthread_create(&desTid[i], NULL, openDesFileAndWrite, &resData);
    if (res) {
      fprintf(stderr, "pthread_create: %s", strerror(res));
      exit(1);
    }
  }
  // sleep(1);
  // 发送信号
  while (1) {
    if (readBlockedSize == 1) {
      pthread_mutex_lock(&(_mut));
      pthread_cond_signal(&_condDtoS);
      pthread_mutex_unlock(&(_mut));
      break;
    }
  }
  // 收尸
  pthread_join(souTid, NULL);
  // pthread_mutex_unlock(&(_mut));
  for (int i = 0; i < this->_threadNum; i++) {
    // printf("%d\n", desTid[i]);
    pthread_join(desTid[i], NULL);
  }
  close(fd);
}

std::string calculateMD5(const std::string &input) {
  EVP_MD_CTX *md5Context = EVP_MD_CTX_new();
  if (md5Context == nullptr) {
    std::cerr << "无法创建 MD5 上下文。" << std::endl;
    return "";
  }

  if (EVP_DigestInit_ex(md5Context, EVP_md5(), nullptr) != 1) {
    std::cerr << "无法初始化 MD5 上下文。" << std::endl;
    EVP_MD_CTX_free(md5Context);
    return "";
  }

  if (EVP_DigestUpdate(md5Context, input.c_str(), input.length()) != 1) {
    std::cerr << "无法更新 MD5 上下文。" << std::endl;
    EVP_MD_CTX_free(md5Context);
    return "";
  }

  unsigned char digest[MD5_DIGEST_LENGTH];
  if (EVP_DigestFinal_ex(md5Context, digest, nullptr) != 1) {
    std::cerr << "无法获取 MD5 哈希值。" << std::endl;
    EVP_MD_CTX_free(md5Context);
    return "";
  }

  EVP_MD_CTX_free(md5Context);

  std::stringstream ss;
  for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
    ss << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<int>(digest[i]);
  }

  return ss.str();
}

bool compare(const std::string &filename1, const std::string &filename2) {
  int fd1 = open(filename1.c_str(), O_RDONLY);
  if (fd1 < 0) {
    perror("open()");
    exit(1);
  }
  int fd2 = open(filename2.c_str(), O_RDONLY);
  if (fd2 < 0) {
    close(fd1);
    perror("open()");
    exit(1);
  }
  int flag = 1;
  while (1) {
    char buf1[1024] = "";
    char buf2[1024] = "";
    read(fd1, buf1, 1000);
    read(fd2, buf2, 1000);
    if (strlen(buf1) == 0 && strlen(buf2) == 0) {
      return true;
    }
    std::string res1 = calculateMD5(buf1);
    std::string res2 = calculateMD5(buf2);
    if (res1 != res2) {
      flag = 0;
      break;
    }
  }
  if (flag) {
    return true;
  } else {
    return false;
  }
}