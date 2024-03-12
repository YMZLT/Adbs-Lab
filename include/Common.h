// common.h

#ifndef COMMON_H
#define COMMON_H

#include <mutex>
#include "assert.h" 
// #define DEBUG

using namespace std;


#define FRAMESIZE 4096 // 页框大小
#define PAGESIZE 4096  // 页面大小
#define THREAD_NUM 4   // 并发执行线程数
#define DB_FILENAME "../build/data.dbf"
#define DATA_FILENAME "../data/data-5w-50w-zipf.txt"

#ifdef DEBUG

#define DEFBUFSIZE 10         // 缓存最大页数
#define MAXPAGES 500          // 磁盘最大页数
#define NUM_PAGR_REQUEST 1000 // 请求次数

#else

#define DEFBUFSIZE 1024
#define MAXPAGES 50000
#define NUM_PAGR_REQUEST 500000

#endif

typedef int frame_id_t;
typedef int page_id_t;

struct bFrame
{
    char field[FRAMESIZE];
};

/* Replacement Policies */
enum Policy
{
    Invalid = 0,
    Lru,
    TwoQ,
    Clock
};

struct BCB
{
    int page_id;
    int frame_id;
    // int latex; // 内存锁
    int count;   // 引用计数
    int dirty;   // 是否修改
    BCB *next;

    BCB() : page_id(-1), frame_id(-1), count(0),
            dirty(0), next(nullptr){};
    BCB(int pid, int fid) : page_id(pid), frame_id(fid), count(1),
                            dirty(0), next(nullptr){};
};

class ReplaceAlg
{
public:
    ReplaceAlg() = default;
    virtual ~ReplaceAlg() = default;
    /* Pin the victim frame as defined by the replacement policy. */
    virtual frame_id_t Victim() = 0;
    /* Pins a frame, indicating that it should not be victimized until it is unpinned. */
    virtual void Pin(int frame_id) = 0;
    /* Unpins a frame, indicating that it can now be victimized. */
    virtual void Unpin(int frame_id) = 0;
};

#endif // COMMON_H