#include "filesystem.h"

#include <bits/getopt_core.h>
#include <bits/types/time_t.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <queue>
#include <string>

#include "copyfile.h"

using namespace std;
// 用这个文件模拟磁盘
string filename = "systemfile";
FileSystem::FileSystem() {}
FileSystem::~FileSystem() {}

Inode::Inode() {}
Inode::~Inode() {}

Line::Line() {}
Line::~Line() {}

/**
    测试方法 打印位图
*/
void printBinary(char c) {
  for (int i = 7; i >= 0; --i) {
    cout << ((c >> i) & 1);
  }
  cout << endl;
}
void printInodeBitMap() {
  FileSystem node;
  node.myread(0);
  int fd = open(filename.c_str(), O_RDONLY);
  if (fd < 0) {
    perror("open(0)");
    exit(1);
  }
  unsigned char buf[2];
  lseek(fd, node.inode_first, SEEK_SET);
  read(fd, buf, sizeof(buf));
  for (int i = 0; i < 2; i++) {
    printBinary(buf[i]);
  }
}
void printDataBitMap() {
  FileSystem node;
  node.myread(0);
  int fd = open(filename.c_str(), O_RDONLY);
  if (fd < 0) {
    perror("open(0)");
    exit(1);
  }
  unsigned char buf[32];
  lseek(fd, node.data_first, SEEK_SET);
  read(fd, buf, sizeof(buf));
  for (int i = 0; i < 32; i++) {
    printBinary(buf[i]);
  }
}

/**
    创建一个空洞文件
    @param filename 文件名 size 文件大小（以字节为单位）
    @return bool值
*/
bool createFile(string filename, off64_t size) {
  int fd = open(filename.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0777);
  if (fd == -1) {
    perror("无法创建文件");
    return false;
  }

  // 将文件扩展到指定大小
  if (lseek(fd, size - 1, SEEK_SET) == -1) {
    perror("lseek 失败");
    close(fd);
    return false;
  }

  // 在文件中写入一个字节，以确保文件占用磁盘空间
  if (write(fd, "\0", 1) != 1) {
    perror("写入失败");
    close(fd);
    return false;
  }

  close(fd);
  return true;
}

/**
  展示inode结构体
*/
void Inode::printStruct() {
  cout << "struct i_node{" << endl;
  cout << "    i_mode: " << this->i_mode << endl;
  cout << "    i_uid: " << this->i_uid << endl;
  cout << "    i_size: " << this->i_size << endl;
  cout << "    i_atime: " << this->i_atime << endl;
  cout << "    i_mtime: " << this->i_mtime << endl;
  cout << "    i_ctime: " << this->i_ctime << endl;
  cout << "    i_links_count: " << this->i_links_count << endl;
  cout << "    i_blocks: " << this->i_blocks << endl;
  cout << "    i_where: " << this->i_where << endl;
  cout << "}" << endl;
}

/**
  展示超级块结构体
*/
void FileSystem::printStruct() {
  cout << "struct superBlock{" << endl;
  cout << "    f_name: " << this->f_name << endl;
  cout << "    f_size: " << this->f_size << endl;
  cout << "    inode_size: " << this->inode_size << endl;
  cout << "    block_size: " << this->block_size << endl;
  cout << "    data_blocks: " << this->data_blocks << endl;
  cout << "    inode_first: " << this->inode_first << endl;
  cout << "    data_first: " << this->data_first << endl;
  cout << "    inode_leave: " << this->inode_leave << endl;
  cout << "    data_leave: " << this->data_leave << endl;
  cout << "}" << endl;
}

/**
    编写自己的方法把超级块写入文件中
*/
bool FileSystem::mywrite(off_t offsets) {
  int fd = open(filename.c_str(), O_WRONLY);
  if (fd < 0) {
    perror("open()");
    return false;
  }
  lseek(fd, offsets, SEEK_SET);
  write(fd, this->f_name, sizeof(this->f_name));
  write(fd, &(this->f_size), sizeof(this->f_size));
  write(fd, &(this->inode_size), sizeof(this->inode_size));
  write(fd, &(this->block_size), sizeof(this->block_size));
  write(fd, &(this->data_blocks), sizeof(this->data_blocks));
  write(fd, &(this->inode_first), sizeof(this->inode_first));
  write(fd, &(this->data_first), sizeof(this->data_first));
  write(fd, &(inode_leave), sizeof(this->inode_leave));
  write(fd, &(data_leave), sizeof(this->data_leave));
  close(fd);
  return true;
}

bool Inode::mywrite(off_t offsets) {
  int fd = open(filename.c_str(), O_WRONLY);
  if (fd < 0) {
    perror("open()");
    return false;
  }
  lseek(fd, offsets, SEEK_SET);
  write(fd, &(this->i_mode), sizeof(this->i_mode));
  write(fd, &(this->i_uid), sizeof(this->i_uid));
  write(fd, &(this->i_size), sizeof(this->i_size));
  write(fd, &(this->i_atime), sizeof(this->i_atime));
  write(fd, &(this->i_mtime), sizeof(this->i_mtime));
  write(fd, &(this->i_ctime), sizeof(this->i_ctime));
  write(fd, &(this->i_links_count), sizeof(this->i_links_count));
  write(fd, &(this->i_blocks), sizeof(this->i_blocks));
  for (int i = 0; i < SYSTEMSIZE / BLOCKSIZE; i++) {
    write(fd, &(this->i_where[i]), sizeof(this->i_where[i]));
  }
  close(fd);
  return true;
}

bool Line::mywrite(off_t offsets) {
  int fd = open(filename.c_str(), O_WRONLY);
  if (fd < 0) {
    perror("open()");
    return false;
  }
  lseek(fd, offsets, SEEK_SET);
  write(fd, &(this->inode_pos), sizeof(this->inode_pos));
  write(fd, this->file_name, sizeof(this->file_name));
  close(fd);
  return true;
}

/**
    从char数组中取特定串出来
    @param buf 待截取的串 startPos 起始位置 num 取的个数 res存入的串
*/
// void getSubStr(char* buf, int startPos, int num, char* res) {
//   int i;
//   for (i = 0; i < num; i++) {
//     res[i] = buf[startPos + i];
//   }
//   res[i] = '\0';
// }

/**
    编写自己的方法把超级块从文件中读取出来
*/
bool FileSystem::myread(off_t offsets) {
  int fd = open(filename.c_str(), O_RDONLY);
  if (fd < 0) {
    perror("open()");
    return false;
  }
  lseek(fd, offsets, SEEK_SET);
  read(fd, this->f_name, sizeof(this->f_name));
  read(fd, &(this->f_size), sizeof(this->f_size));
  read(fd, &(this->inode_size), sizeof(this->inode_size));
  read(fd, &(this->block_size), sizeof(this->block_size));
  read(fd, &(this->data_blocks), sizeof(this->data_blocks));
  read(fd, &(this->inode_first), sizeof(this->inode_first));
  read(fd, &(this->data_first), sizeof(this->data_first));
  read(fd, &(inode_leave), sizeof(this->inode_leave));
  read(fd, &(data_leave), sizeof(this->data_leave));
  close(fd);
  return true;
}

bool Inode::myread(off_t offsets) {
  int fd = open(filename.c_str(), O_RDONLY);
  if (fd < 0) {
    perror("open()");
    return false;
  }
  lseek(fd, offsets, SEEK_SET);
  read(fd, &(this->i_mode), sizeof(this->i_mode));
  read(fd, &(this->i_uid), sizeof(this->i_uid));
  read(fd, &(this->i_size), sizeof(this->i_size));
  read(fd, &(this->i_atime), sizeof(this->i_atime));
  read(fd, &(this->i_mtime), sizeof(this->i_mtime));
  read(fd, &(this->i_ctime), sizeof(this->i_ctime));
  read(fd, &(this->i_links_count), sizeof(this->i_links_count));
  read(fd, &(this->i_blocks), sizeof(this->i_blocks));
  for (int i = 0; i < SYSTEMSIZE / BLOCKSIZE; i++) {
    read(fd, &(this->i_where[i]), sizeof(this->i_where[i]));
  }
  close(fd);
  return true;
}

bool Line::myread(off_t offsets) {
  int fd = open(filename.c_str(), O_RDONLY);
  if (fd < 0) {
    perror("open()");
    return false;
  }
  lseek(fd, offsets, SEEK_SET);
  read(fd, &(this->inode_pos), sizeof(this->inode_pos));
  read(fd, this->file_name, sizeof(this->file_name));
  close(fd);
  return true;
}

/**
  展示inode和文件名对应表
*/
void Line::printStruct() {
  cout << "struct inode_filename{" << endl;
  cout << "    inode_pos: " << this->inode_pos << endl;
  cout << "    name: " << this->file_name << endl;
  cout << "}" << endl;
}

/**
    读取超级块
*/
bool ReadSuperBlock(FileSystem* node) {
  node->myread(0);
  return true;
}

/**
    初始化文件系统
    1、生成16G大小的空洞文件
    2、将文件系的超级块写入第一块中并初始化
    3、初始化inode节点位图以及数据块位图
    4、返回成功或失败
*/
bool InitFileSystem() {
  // 1、生成16G大小的空洞文件
  bool create_res = createFile(filename, SYSTEMSIZE);
  if (!create_res) {
    fprintf(stderr, "createFile failed\n");
    return false;
  }
  // 2、将文件系的超级块写入第一块中并初始化
  FileSystem fileSystem;
  char name[16] = "yzxFileSystem";
  memcpy(fileSystem.f_name, name, sizeof(name));
  fileSystem.f_size = SYSTEMSIZE;
  fileSystem.inode_size = 16;  // 写死inode节点个数
  fileSystem.block_size = BLOCKSIZE;
  fileSystem.data_blocks = SYSTEMSIZE / BLOCKSIZE - 1;
  fileSystem.inode_first = 64 + 48 * 1024;
  fileSystem.data_first = fileSystem.inode_first + 2;
  fileSystem.inode_leave = fileSystem.inode_size;
  fileSystem.data_leave = fileSystem.data_blocks;
  fileSystem.mywrite(0);
  // 3、初始化inode节点位图以及数据块位图
  //   cout << fileSystem.inode_first << endl;
  int fd = open(filename.c_str(), O_WRONLY);
  if (fd < 0) {
    perror("open");
    return false;
  }
  lseek(fd, fileSystem.inode_first, SEEK_SET);
  unsigned char buf1[2] = {0};
  write(fd, buf1, sizeof(buf1));

  lseek(fd, fileSystem.data_first, SEEK_SET);
  unsigned char buf2[32] = {0};
  write(fd, buf2, sizeof(buf2));
  lseek(fd, 0, SEEK_SET);
  close(fd);
  FileSystem node;
  ReadSuperBlock(&node);
  node.printStruct();
  cout << "文件系统初始化成功" << endl;
  return true;
}
// 设置位
void setBit(unsigned char& bitmap, int position) { bitmap |= (1 << position); }

// 清除位
void clearBit(unsigned char& bitmap, int position) {
  bitmap &= ~(1 << position);
}

// 检查位
bool checkBit(unsigned char bitmap, int position) {
  return (bitmap & (1 << position)) != 0;
}

/**
  在inode结点位图中寻找一个空位并返回相对位置
*/
int findInInodeBitMap() {
  FileSystem node;
  ReadSuperBlock(&node);
  int fd = open(filename.c_str(), O_RDWR);
  if (fd < 0) {
    perror("open()");
    return -1;
  }
  lseek(fd, node.inode_first, SEEK_SET);
  unsigned char buf[2];
  read(fd, buf, 2);
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 8; j++) {
      bool res = checkBit(buf[i], j);
      if (res == false) {
        setBit(buf[i], j);
        lseek(fd, node.inode_first, SEEK_SET);
        write(fd, buf, sizeof(buf));
        close(fd);
        return i * 8 + j;
      }
    }
  }
  close(fd);
  return -1;
}

/**
  把位图的相对位置转换成偏移量
*/
off64_t transformInode(int pos) { return 64 + pos * 3 * 1024; }

/**
  在data结点位图中寻找一个空位并返回相对位置
*/
int findInDataBitMap() {
  FileSystem node;
  ReadSuperBlock(&node);
  int fd = open(filename.c_str(), O_RDWR);
  if (fd < 0) {
    perror("open()");
    return -1;
  }
  lseek(fd, node.data_first, SEEK_SET);
  unsigned char buf[32];
  read(fd, buf, 32);
  for (int i = 0; i < 32; i++) {
    for (int j = 0; j < 8; j++) {
      bool res = checkBit(buf[i], j);
      if (res == false) {
        setBit(buf[i], j);
        lseek(fd, node.data_first, SEEK_SET);
        write(fd, buf, sizeof(buf));
        close(fd);
        return i * 8 + j;
      }
    }
  }
  close(fd);
  return -1;
}

/**
  把data位图的相对位置转换成偏移量
*/
off64_t transformDataBit(int pos) { return 64 * 1024 + pos * 64 * 1024; }

/**
  创建一个文件用于接收数据
*/
bool mymkfile(string name) {
  FileSystem node;
  ReadSuperBlock(&node);
  Line line;
  int flag = 0;
  for (int i = 0; i < (node.inode_size - node.inode_leave); i++) {
    line.myread(64 + 48 * 1024 + 2 + 32 + i * sizeof(line));
    if (line.file_name == name) {
      flag = 1;
      break;
    }
  }
  if (flag) {
    cout << "文件已经存在，不能再次创建" << endl;
    return false;
  }
  int resPos = findInInodeBitMap();
  if (resPos < 0) {
    return false;
  }
  int myumask = 013;
  off64_t offsetPos = transformInode(resPos);
  Inode inode;
  inode.i_mode = 511 & (~myumask);
  inode.i_uid = 135;
  inode.i_size = 0;
  inode.i_atime = time(NULL);
  inode.i_mtime = time(NULL);
  inode.i_ctime = time(NULL);
  inode.i_links_count = 1;
  inode.i_blocks = 0;
  for (int i = 0; i < 256; i++) {
    inode.i_where[i] = -1;
  }
  inode.mywrite(offsetPos);
  // 修改超级块中的inode节点剩余个数
  node.inode_leave -= 1;
  node.mywrite(0);
  // 往inode节点与filename相对应的表中填写
  line.inode_pos = resPos;
  memcpy(line.file_name, name.c_str(), sizeof(name));
  line.mywrite(64 + 48 * 1024 + 2 + 32 +
               (node.inode_size - node.inode_leave - 1) * sizeof(line));
  return true;
}
void printInode(Inode* node) {
  // 打印权限信息
  cout << "-";
  unsigned int mode = (unsigned)node->i_mode >> 1;
  bool res = checkBit(node->i_mode, 7);
  if (res) {
    cout << "r";
  } else {
    cout << "-";
  }
  for (int i = 0; i < 8; i++) {
    res = checkBit(node->i_mode, 7 - i);
    if (res) {
      if ((8 - i) % 3 == 0) {
        cout << "r";
      } else if ((8 - i) % 3 == 2) {
        cout << "w";
      } else {
        cout << "e";
      }
    } else {
      cout << "-";
    }
  }
  // 硬链接数
  cout << " " << node->i_links_count;
  // 用户
  cout << " " << node->i_uid << " " << node->i_uid;
  // 文件大小
  cout << " " << node->i_size;
  // atime
  time_t t = node->i_atime;
  char timestr[1024];
  strftime(timestr, 1024, " %m月 %H:%M:%S", localtime(&t));
  cout << timestr;
}

/**
  查看当前的文件信息类似于ls 与ll
*/
void fileinfo(string flag) {
  if (flag == "ls") {
    FileSystem node;
    ReadSuperBlock(&node);
    Line line;
    int flag = 0;
    for (int i = 0; i < (node.inode_size - node.inode_leave); i++) {
      line.myread(64 + 48 * 1024 + 2 + 32 + i * sizeof(line));
      cout << line.file_name << "\t";
      flag = 1;
    }
    if (flag) cout << endl;
  } else if (flag == "ll") {
    FileSystem node;
    ReadSuperBlock(&node);
    Line line;
    for (int i = 0; i < (node.inode_size - node.inode_leave); i++) {
      line.myread(64 + 48 * 1024 + 2 + 32 + i * sizeof(line));
      off64_t offsetPos = transformInode(line.inode_pos);
      Inode inode;
      inode.myread(offsetPos);
      printInode(&inode);
      cout << " " << line.file_name << endl;
    }
  }
}

/**
  删除文件
*/
bool deleteFile(string name) {
  FileSystem node;
  ReadSuperBlock(&node);
  Line line;
  Line deleteLine;
  int flag = 1;
  for (int i = 0; i < (node.inode_size - node.inode_leave); i++) {
    line.myread(64 + 48 * 1024 + 2 + 32 + i * sizeof(line));
    if (line.file_name == name) {
      deleteLine = line;
      flag = 0;
      break;
    }
  }
  if (flag) return false;
  // 用户提供的文件名存在，然后删除文件
  // 1、调整inode与filename对应表
  queue<Line> q;
  for (int i = 0; i < (node.inode_size - node.inode_leave); i++) {
    line.myread(64 + 48 * 1024 + 2 + 32 + i * sizeof(line));
    if (line.file_name != name) {
      q.push(line);
    }
  }
  for (int i = 0; i < (node.inode_size - node.inode_leave) - 1; i++) {
    line = q.front();
    q.pop();
    line.mywrite(64 + 48 * 1024 + 2 + 32 + i * sizeof(line));
  }
  // 修改数据块的位图
  int fd = open(filename.c_str(), O_RDWR);
  if (fd < 0) {
    perror("open");
    return false;
  }
  lseek(fd, node.data_first, SEEK_SET);
  unsigned char buf1[32];
  read(fd, buf1, sizeof(buf1));
  Inode inode;
  inode.myread(transformInode(deleteLine.inode_pos));
  int count = 0;
  for (int i = 0; i < 256; i++) {
    if (inode.i_where[i] == -1) break;
    int which_block = inode.i_where[i] / 8;
    int which_bit = inode.i_where[i] % 8;
    unsigned char t = buf1[which_block];
    clearBit(t, which_bit);
    buf1[which_block] = t;
    count++;
  }
  lseek(fd, node.data_first, SEEK_SET);
  write(fd, buf1, sizeof(buf1));
  close(fd);
  // 修改inode结点位图
  int which_block = deleteLine.inode_pos / 8;
  int which_bit = deleteLine.inode_pos % 8;
  fd = open(filename.c_str(), O_RDWR);
  if (fd < 0) {
    perror("open");
    return false;
  }
  lseek(fd, node.inode_first, SEEK_SET);
  unsigned char buf2[2];
  read(fd, buf2, sizeof(buf2));
  unsigned char t = buf2[which_block];
  clearBit(t, which_bit);
  buf2[which_block] = t;
  lseek(fd, node.inode_first, SEEK_SET);
  write(fd, buf2, sizeof(buf2));
  close(fd);
  // 修改超级块
  node.inode_leave++;
  node.data_leave += count;
  node.mywrite(0);
  return true;
}

/**
  菜单
*/
void menu(const string option, const string name) {
  if (option == "ls" || option == "ll") {
    fileinfo(option);
  } else if (option == "touch") {
    bool res = mymkfile(name);
    if (res == false) {
      cout << "文件创建失败" << endl;
    } else {
      cout << "文件创建成功" << endl;
    }
  } else if (option == "rm") {
    bool res = deleteFile(name);
    if (res == false) {
      cout << "文件删除失败" << endl;
    } else {
      cout << "文件删除成功" << endl;
    }
  } else if (option == "cat") {
    mycat(name);
  } else if (option == "vi") {
    transformData(name);
  } else if (option == "checki") {
    printInodeBitMap();
  } else if (option == "checkd") {
    printDataBitMap();
  } else {
    if (option == "") {
      return;
    }
    cout << "----------------------------------------------" << endl;
    cout << "no such comman! you can use the fellows!" << endl;
    cout << "ls | ll: check the information of files. eg: ll" << endl;
    cout << "touch:\t create a file. eg: touch <filename>" << endl;
    cout << "rm:\t delete a file. eg: rm <filename>" << endl;
    cout << "cat:\t check the content of the file. eg: cat <filename>" << endl;
    cout << "vi:\t edit the content of the file. eg: vi <filename>" << endl;
    cout << "checki:\t check the inode bit map. eg: checki" << endl;
    cout << "checkd:\t check the data bit map. eg: checkd" << endl;
    cout << "----------------------------------------------" << endl;
  }
}

/**
  根据文件名在对应表中查找
*/
int relativePos(string name) {
  FileSystem fileSystemNode;
  ReadSuperBlock(&fileSystemNode);
  Line line;
  for (int i = 0; i < (fileSystemNode.inode_size - fileSystemNode.inode_leave);
       i++) {
    line.myread(64 + 48 * 1024 + 2 + 32 + i * sizeof(line));
    if (line.file_name == name) {
      break;
    }
  }
  return line.inode_pos;
}

/**
  文件创建好之后就要模拟数据传输
*/
void transformData(string desFileName) {
  // 先要判断文件name存不存在
  FileSystem node;
  ReadSuperBlock(&node);
  Line line;
  int flag = 1;
  for (int i = 0; i < (node.inode_size - node.inode_leave); i++) {
    line.myread(64 + 48 * 1024 + 2 + 32 + i * sizeof(line));
    if (line.file_name == desFileName) {
      flag = 0;
      break;
    }
  }
  if (flag) {
    cout << "文件不存在，不能向一个不存在的文件传输数据" << endl;
    return;
  }
  CopyFileClass copyFile(3, 64 * 1024);
  copyFile.initFilePath(
      "/home/yzx/vscode/workspace/unixworks/work1/src/test.txt", desFileName);
  copyFile.initMutexAndCondition();
  copyFile.copyFile();
  copyFile.destroyMutexAndCondition();
}

/**
  使用cat命令查看文件
*/
void mycat(string name) {
  // 先要判断文件name存不存在
  FileSystem node;
  ReadSuperBlock(&node);
  Line line;
  int flag = 1;
  for (int i = 0; i < (node.inode_size - node.inode_leave); i++) {
    line.myread(64 + 48 * 1024 + 2 + 32 + i * sizeof(line));
    if (line.file_name == name) {
      flag = 0;
      break;
    }
  }
  if (flag) {
    cout << "文件不存在" << endl;
    return;
  }
  Inode inode;
  inode.myread(transformInode(line.inode_pos));
  char buf[BLOCKSIZE];
  int fd = open(filename.c_str(), O_RDONLY);
  if (fd < 0) {
    perror("open()");
    return;
  }
  for (int i = 0; i < inode.i_blocks; i++) {
    lseek(fd, (inode.i_where[i] + 1) * 64 * 1024, SEEK_SET);
    read(fd, buf, BLOCKSIZE);
    printf("%s", buf);
  }
  cout << endl;
}