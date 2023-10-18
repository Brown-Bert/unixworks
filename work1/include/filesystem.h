#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_
#include <sys/types.h>

#include <cstdint>
#include <string>
#define SYSTEMSIZE (16L * 1024L * 1024L)  // 字节
#define BLOCKSIZE (64 * 1024)             // 字节
using namespace std;

/**
  基类
*/
class Base {
 public:
  virtual void printStruct() = 0;
  virtual bool mywrite(off_t offsets) = 0;
  virtual bool myread(off_t offsets) = 0;
};

/**
    超级块结构体的信息
*/
class FileSystem : public Base {
 public:
  char f_name[16];     // 文件系统的名称
  int32_t f_size;      // 文件系统的大小（以字节为单位）
  int32_t inode_size;  // 文件系统中inode节点总共有多少个
  int32_t block_size;  // 一块的大小
  int32_t data_blocks;  // 数据块的个数（要除去超级块，inode节点，位图等大小）
  int32_t inode_first;  // inode位图的起始位置
  int32_t data_first;   // 数据块位图的起始位置
  int32_t inode_leave;  // inode节点剩余的个数
  int32_t data_leave;   // 数据块剩余个数
 public:
  FileSystem();
  ~FileSystem();
  void printStruct() override;
  bool mywrite(off_t offsets) override;
  bool myread(off_t offsets) override;
};

/**
    inode节点结构体
*/
class Inode : public Base {
 public:
  int32_t
      i_mode;  // 文件类型以及权限（从左向右空四位）（权限9位+3位文件类型说明）
  int32_t i_uid;          //用户id
  int32_t i_size;         // 一个文件的大小（以字节为单位）
  int32_t i_atime;        // 访问时间，文件最后一次被访问的时间
  int32_t i_mtime;        // 文件最后一次被修改的时间
  int32_t i_ctime;        // 文件元数据最后一次被修改的时间
  int32_t i_links_count;  // 硬链接数
  int32_t i_blocks;       // 一个文件占的块数
  int32_t i_where[SYSTEMSIZE / BLOCKSIZE];  // 一个文件具体占哪几块
 public:
  Inode();
  ~Inode();
  void printStruct() override;
  bool mywrite(off_t offsets) override;
  bool myread(off_t offsets) override;
};

class Line : public Base {
 public:
  int32_t inode_pos;   // inode节点的相对位置
  char file_name[60];  // 文件名
 public:
  Line();
  ~Line();
  void printStruct() override;
  bool mywrite(off_t offsets) override;
  bool myread(off_t offsets) override;
};

/**
    测试方法 打印位图
*/
void printBinary(char c);
void printInodeBitMap();
void printDataBitMap();

/**
    创建一个空洞文件
    @param filename 文件名 size 文件大小（以字节为单位）
    @return bool值
*/
bool createFile(std::string filename, off64_t size);

/**
    读取超级块
*/
bool ReadSuperBlock(FileSystem* node);

/**
    初始化文件系统
    1、生成16G大小的空洞文件
    2、将文件系的超级块写入第一块中并初始化
    3、初始化inode节点位图以及数据块位图
    4、返回成功或失败
*/
bool InitFileSystem();

/**
  创建一个文件用于接收数据
*/
bool mymkfile(string name);

/**
  在inode结点位图中寻找一个空位并返回相对位置
*/
int findInInodeBitMap();

/**
  把inode位图的相对位置转换成偏移量
*/
off64_t transformInode(int pos);

/**
  在data结点位图中寻找一个空位并返回相对位置
*/
int findInDataBitMap();

/**
  把data位图的相对位置转换成偏移量
*/
off64_t transformDataBit(int pos);

/**
  查看当前的文件信息类似于ls 与ll
*/
void fileinfo(string flag);

/**
  删除文件
*/
bool deleteFile(string name);
/**
  根据文件名在对应表中查找
*/
int relativePos(string name);

/**
  菜单
*/
void menu(const string option, const string name);

/**
  文件创建好之后就要模拟数据传输
*/
void transformData(string desFileName);

/**
  使用cat命令查看文件
*/
void mycat(string name);

#endif