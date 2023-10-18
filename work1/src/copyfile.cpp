#include "copyfile.h"

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "filesystem.h"
int writeBlockedSize = 0;  // 用于统计写阻塞的线程个数
int readBlockedSize = 0;   // 用于统计写阻塞的线程个数
// 用这个文件模拟磁盘
extern string filename;
int _taskSize = 0;  // 目前任务队列中实际上有多少个任务
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
    readBlockedSize--;
    if (_fileFlag) {
      // 文件读完了
      break;
    }
    int flag = 1;
    // printf("size: %d\n", _taskSize);
    if (_taskSize == 0) {
      // 说明任务队列中已经没有任务了，需要往任务队列中放任务
      for (int i = 0; i < data->_threadNum; i++) {
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
          puts("input task");
        }
      }
      pthread_mutex_unlock(&(_mut));
      while (1) {
        if (writeBlockedSize == data->_threadNum) {
          pthread_mutex_lock(&(_mut));
          pthread_cond_broadcast(&(_condStoD));
          if (flag == 0) _fileFlag = true;
          pthread_mutex_unlock(&(_mut));
          break;
        }
      }
    }
    if (flag == 0) break;
  }
  close(fd);
  pthread_exit(NULL);
}

/**
  开启写线程， 从任务队列中拿数据写入目的文件
*/
static void *openDesFileAndWrite(void *p) {
  conveyData *data = (conveyData *)p;
  FileSystem fileSystemNode;
  ReadSuperBlock(&fileSystemNode);
  int fd;
  fd = open(filename.c_str(), O_WRONLY);
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
    writeBlockedSize++;
    pthread_cond_wait(&(_condStoD), &(_mut));
    writeBlockedSize--;
    while (1) {
      if (_taskSize == 0) {
        pthread_spin_lock(&(sp));
        count++;
        pthread_spin_unlock(&(sp));
        if (count == data->_threadNum) {
          if (!_fileFlag) {
            pthread_mutex_unlock(&(_mut));
            while (1) {
              if (readBlockedSize == 1) {
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
          // _fileFlag = false;
        }
        break;
      } else {
        node *oneNode = _tasks.front();
        _tasks.pop();
        _taskSize--;
        sumSize += oneNode->_realsize;
        // lseek(fd, _offset + oneNode._index * data->_blockSize, SEEK_SET);
        // write(fd, oneNode._buf, oneNode._realsize);

        int pos = relativePos(data->_desFileName);
        Inode inode;
        inode.myread(transformInode(pos));
        // 去位图中找一块空的数据块
        int respos = findInDataBitMap();
        // puts("找到");
        if (respos < 0) {
          // 申请数据块失败
          fprintf(stderr, "apply for data block failed\n");
          _fileFlag = true;
          pthread_exit(NULL);
        }
        // 修改inode节点信息
        inode.i_size += oneNode->_realsize;
        inode.i_mtime = time(NULL);
        inode.i_ctime = time(NULL);
        inode.i_blocks++;
        for (int i = 0; i < 256; i++) {
          if (inode.i_where[i] == -1) {
            inode.i_where[i] = respos;
            break;
          }
        }
        inode.mywrite(transformInode(pos));
        // 修改超级块的信息
        fileSystemNode.data_leave--;
        fileSystemNode.mywrite(0);
        pthread_mutex_unlock(&(_mut));
        // 写入数据
        pwrite(fd, oneNode->_buf, oneNode->_realsize, transformDataBit(respos));
        // 释放堆上的内存
        free(oneNode->_buf);
        free(oneNode);
        // puts("output task");
        printf("output task: tid = %d\n", gettid());
      }
      pthread_mutex_lock(&(_mut));
    }
    pthread_mutex_unlock(&(_mut));
    if (_fileFlag) {
      break;
    }
  }
  close(fd);
  pthread_exit(NULL);
}

void CopyFileClass::copyFile() {
  // 开启线程，打开文件往任务队列中按照数据块的大小往里面写
  pthread_t souTid;
  int res;
  conveyData resData;
  resData._blockSize = this->_blockSize;
  resData._threadNum = this->_threadNum;
  resData._desFileName = this->_desFileName;
  resData._souFileName = this->_souFileName;
  res = pthread_create(&souTid, NULL, openSouFileAndRead, &resData);
  if (res) {
    fprintf(stderr, "pthread_create: %s", strerror(res));
    exit(1);
  }
  // 发送信号
  while (1) {
    if (readBlockedSize == 1) {
      pthread_mutex_lock(&(_mut));
      pthread_cond_signal(&_condDtoS);
      pthread_mutex_unlock(&(_mut));
      break;
    }
  }
  // 开启写线程，从任务队列里面拿取任务写入目标文件
  pthread_t desTid[this->_threadNum];
  for (int i = 0; i < this->_threadNum; i++) {
    res = pthread_create(&desTid[i], NULL, openDesFileAndWrite, &resData);
    if (res) {
      fprintf(stderr, "pthread_create: %s", strerror(res));
      exit(1);
    }
  }
  // 收尸
  pthread_join(souTid, NULL);
  for (int i = 0; i < this->_threadNum; i++) {
    pthread_join(desTid[i], NULL);
  }
}