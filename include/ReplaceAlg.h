// ReplaceAlg.h

#ifndef REPLACEALG_H
#define REPLACEALG_H

#include "Common.h"
#include "BMgr.h"

class LPUReplaceAlg : public ReplaceAlg
{
    struct ListNode // 双向链表节点
    {
        int frame_id;
        ListNode *prev, *next;

        ListNode(int v = -1) : frame_id(v), prev(nullptr), next(nullptr) {}
    };

public:
    LPUReplaceAlg();
    ~LPUReplaceAlg();
    frame_id_t Victim();
    void Pin(int frame_id);
    void Unpin(int frame_id);

private:
    ListNode *fid_to_node_[DEFBUFSIZE]; // hash table : frame_id to Listnode pointer
    ListNode *head_, *tail_;            // 双向链表头尾指针
    mutex latch_;                       // 为了线程安全需要加的锁

    void pop_node();
    void push_node(ListNode *new_node);
    inline void set_pointer(ListNode *p, ListNode *q)
    {
        p->next = q; //
        q->prev = p;
    }
};

/**
 * 2Q算法：历史队列(FIFO的淘汰策略)和缓存队列(LRU-1的淘汰策略)
 * 新访问的数据插入到FIFO队列
 * 如果数据在FIFO队列中一直没有被再次访问，则最终按照FIFO规则淘汰
 * 如果数据在FIFO队列中被再次访问，则将数据移到LRU队列头部
 * 如果数据在LRU队列再次被访问，则将数据移到LRU队列头部
 * LRU队列淘汰末尾的数据
 */
class TwoQReplaceAlg : public ReplaceAlg
{
    struct ListNode
    {
        int frame_id;
        ListNode *prev, *next;
        bool inCache; // 是否在缓存队列中
        bool isPin;    // 是否在访问

        ListNode(int v = -1) : frame_id(v), prev(nullptr), next(nullptr),
                               inCache(false), isPin(true){};
    };

public:
    TwoQReplaceAlg();
    ~TwoQReplaceAlg();
    frame_id_t Victim();
    void Pin(int frame_id);
    void Unpin(int frame_id);

private:
    ListNode *h_head_, *h_tail_;        // history_ list
    ListNode *c_head_, *c_tail_;        // cache_ list
    ListNode *fid_to_node_[DEFBUFSIZE]; // hash table: frame_id to node pointer
    mutex latch_;

    inline void set_pointer(ListNode *p, ListNode *q)
    {
        p->next = q;
        q->prev = p;
    }
    void freeList(ListNode *head);
    void push_node(ListNode *new_node, ListNode *head);
};

class ClockReplaceAlg : public ReplaceAlg
{
    struct ListNode
    {
        int frame_id;
        ListNode *prev, *next;
        bool ref;   // ref 表示当前的 frame 最近是否被使用过
        bool isPin; // isPin 表示当前 frame 是正在被引用

        ListNode(int v = -1) : frame_id(v), prev(nullptr), next(nullptr), ref(false), isPin(true) {}
        // isPin = true 页面加载到 Buffer pool 时，一定是因为 page 被引用了
        // ref = false 因为当前这个 page 的引用还没有结束
    };

public:
    ClockReplaceAlg();
    ~ClockReplaceAlg();
    frame_id_t Victim();
    void Pin(int frame_id);
    void Unpin(int frame_id);

private:
    ListNode *cptr_;                    // clock 指针当前所指位置
    ListNode *fid_to_node_[DEFBUFSIZE]; // hash table: frame_id to node pointer
    mutex latch_;

    bool exists(int frame_id);
    inline void set_pointer(ListNode *p, ListNode *q)
    {
        p->next = q;
        q->prev = p;
    }
};

#endif // REPLACEALG_H