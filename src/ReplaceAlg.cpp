#include "ReplaceAlg.h"

LPUReplaceAlg::LPUReplaceAlg()
{
  head_ = new ListNode();
  tail_ = new ListNode();
  set_pointer(head_, tail_);

  for (size_t i = 0; i < DEFBUFSIZE; i++)
  {
    fid_to_node_[i] = nullptr;
  }
}

LPUReplaceAlg::~LPUReplaceAlg()
{
  ListNode *p = head_, *temp;
  for (; p != nullptr; p = temp)
  {
    temp = p->next;
    delete p;
  }
}

frame_id_t LPUReplaceAlg::Victim()
{
  lock_guard<mutex> lock(latch_);
  frame_id_t ret = tail_->prev->frame_id; // 选择最后一个节点替换掉
  pop_node();
  return ret;
}

/* 插入节点表示页面访问结束 */
void LPUReplaceAlg::Unpin(int frame_id)
{
  lock_guard<mutex> lock(latch_);
  ListNode *temp = fid_to_node_[frame_id];
  // 1. 插入节点曾经访问过
  if (temp != nullptr)
  {
    set_pointer(temp->prev, temp->next); // 取下该节点
    push_node(temp);                     // 放到链表头
    return;
  }
  // 2. 新插入节点
  push_node(new ListNode(frame_id));
}

/* 移除节点表示页面正在访问 此时无法被选为victim */
void LPUReplaceAlg::Pin(int frame_id)
{
  lock_guard<mutex> lock(latch_);
  ListNode *temp = fid_to_node_[frame_id];
  if (temp != nullptr)
  {
    set_pointer(temp->prev, temp->next); // 取下该节点
    fid_to_node_[frame_id] = nullptr;
    return;
  }
}

/* 删除链表末尾节点 */
void LPUReplaceAlg::pop_node()
{
  ListNode *temp = tail_->prev;
  if (temp == head_)
    return;
  set_pointer(temp->prev, tail_);
  fid_to_node_[temp->frame_id] = nullptr;
  delete temp;
}

/* 将节点插入到头部 */
void LPUReplaceAlg::push_node(ListNode *new_node)
{
  fid_to_node_[new_node->frame_id] = new_node;

  ListNode *temp = head_->next;
  set_pointer(head_, new_node);
  set_pointer(new_node, temp);
}

TwoQReplaceAlg::TwoQReplaceAlg()
{
  h_head_ = new ListNode();
  h_tail_ = new ListNode();
  set_pointer(h_head_, h_tail_);

  c_head_ = new ListNode();
  c_tail_ = new ListNode();
  set_pointer(c_head_, c_tail_);

  for (size_t i = 0; i < DEFBUFSIZE; i++)
  {
    fid_to_node_[i] = nullptr;
  }
}

TwoQReplaceAlg::~TwoQReplaceAlg()
{
  freeList(c_head_);
  freeList(h_head_);
}

frame_id_t TwoQReplaceAlg::Victim()
{
  lock_guard<mutex> lock(latch_);
  // 从历史队列尾部向前查找unpin节点
  // 优先淘汰历史队列末尾节点
  ListNode *temp;
  for (temp = h_tail_->prev; temp != h_head_; temp = temp->prev)
  {
    if (temp->isPin)
      continue;
    int ret = temp->frame_id;
    set_pointer(temp->prev, temp->next); // 取下该节点并删除
    delete temp;
    fid_to_node_[ret] = nullptr;
    return ret;
  }

  // 历史队列为空 则淘汰缓存队列末尾节点
  temp = c_tail_->prev;
  for (temp = c_tail_->prev; temp != c_head_; temp = temp->prev)
  {
    if (temp->isPin)
      continue;
    int ret = temp->frame_id;
    set_pointer(temp->prev, temp->next); // 取下该节点并删除
    delete temp;
    fid_to_node_[ret] = nullptr;
    return ret;
  }
  return -1;
}

/* 表示页面正在访问 无法被替换 */
void TwoQReplaceAlg::Pin(int frame_id)
{
  lock_guard<mutex> lock(latch_);

  ListNode *cur = fid_to_node_[frame_id];
  if (cur != nullptr) // 该页面已经在回收站中 更新访问信息
  {
    cur->isPin = true; // 标记该页面不能被替换
  }
}

/* 表示页面访问完毕 */
void TwoQReplaceAlg::Unpin(int frame_id)
{
  lock_guard<mutex> lock(latch_);
  ListNode *temp = fid_to_node_[frame_id];
  // 1. 插入节点未访问过 插入到历史队列头部
  if (temp == nullptr)
  {
    temp = new ListNode(frame_id);
    push_node(temp, h_head_);
    temp->isPin = false;
    return;
  }

  // 2. 插入节点只访问过一次 在历史队列中
  if (!temp->inCache)
  {
    set_pointer(temp->prev, temp->next); // 从历史队列取下该节点
    push_node(temp, c_head_);            // 放到缓存队列头部
    temp->inCache = true;
    temp->isPin = false;
    return;
  }
  // 3. 插入节点访问过多次 在缓存队列中
  set_pointer(temp->prev, temp->next); // 从缓存队列取下该节点
  push_node(temp, c_head_);            // 放到缓存队列头部
  temp->isPin = false;
  return;
}

void TwoQReplaceAlg::freeList(ListNode *head)
{
  ListNode *p, *temp;
  p = head;
  while (p != nullptr)
  {
    temp = p;
    p = p->next;
    delete temp;
  }
}

/* 向链表头部插入新节点 */
void TwoQReplaceAlg::push_node(ListNode *new_node, ListNode *head)
{
  fid_to_node_[new_node->frame_id] = new_node;

  ListNode *temp = head->next;
  set_pointer(head, new_node);
  set_pointer(new_node, temp);
  // printf("push frame[%d].\n", new_node->frame_id);
}

ClockReplaceAlg::ClockReplaceAlg()
{
  cptr_ = nullptr;
  for (int i = 0; i < DEFBUFSIZE; i++)
  {
    fid_to_node_[i] = nullptr;
  }
}

ClockReplaceAlg::~ClockReplaceAlg()
{
  if (cptr_ == nullptr)
    return;
  ListNode *p, *temp;
  for (p = cptr_->next; p != cptr_; p = temp)
  {
    temp = p->next;
    delete p;
  }
  delete cptr_;
}

frame_id_t ClockReplaceAlg::Victim()
{
  lock_guard<mutex> lock(latch_);

  // 循环查找可以被置换的页
  for (; cptr_ != nullptr; cptr_ = cptr_->next)
  {
    if (cptr_->isPin) // 当前页面正在被引用，禁止置换
      continue;
    if (cptr_->ref == false) // 当前页面不在被引用且最近未使用 可以被置换
    {
      int ret = cptr_->frame_id;

      ListNode *temp = cptr_;
      cptr_ = cptr_->next;
      set_pointer(temp->prev, cptr_); // 取下该节点
      delete temp;
      fid_to_node_[ret] = nullptr;
      return ret;
    }
    cptr_->ref = false; // 每次扫描都要把最近使用的页面置为false
  }
  return -1;
}

/* 表示页面正在访问 无法被替换 */
void ClockReplaceAlg::Pin(int frame_id)
{
  lock_guard<mutex> lock(latch_);
  ListNode *cur = fid_to_node_[frame_id]; // 查找页面是否在链表中
  if (cur != nullptr)                     // 该页面已经在链表中 更新访问信息
  {
    cur->isPin = true; // 标记该页面不能被替换
  }
}

/* 表示页面已经访问完毕 */
void ClockReplaceAlg::Unpin(int frame_id)
{
  lock_guard<mutex> lock(latch_);
  ListNode *cur = fid_to_node_[frame_id];
  // 1.该页面已经在回收站中 则更新访问信息
  if (cur != nullptr)
  {
    cur->isPin = false;
    cur->ref = true;
    return;
  }
  // 2.该页面不在回收站中 且环上无节点
  if (cptr_ == nullptr)
  {
    cptr_ = new ListNode(frame_id);
    set_pointer(cptr_, cptr_);
    cptr_->isPin = false;
    cptr_->ref = true;
    fid_to_node_[frame_id] = cptr_;
    return;
  }
  // 3.该页面不在回收站中 且环上有节点 加入环上cptr上一个节点
  ListNode *new_node = new ListNode(frame_id);
  new_node->isPin = false;
  new_node->ref = true;
  set_pointer(cptr_->prev, new_node);
  set_pointer(new_node, cptr_);
  fid_to_node_[frame_id] = new_node;
}
