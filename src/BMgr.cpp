#include "BMgr.h"

// Interface functions

BMgr::BMgr(string filename, int alg)
{
    for (int i = 0; i < DEFBUFSIZE; i++)
    {
        fid_to_pid_[i] = -1;
        pid_to_bcb_[i] = new BCB(); // empty head node
    }
    // choose replace algorithm
    switch (alg)
    {
    case Policy::Lru:
        replace_alg_ = new LPUReplaceAlg();
        break;
    case Policy::Clock:
        replace_alg_ = new ClockReplaceAlg();
        break;
    case Policy::TwoQ:
        replace_alg_ = new TwoQReplaceAlg();
        break;
    default:
        fprintf(stderr, "Error:Policy is unvalid.\n");
        exit(1);
    }
    disk_manager_ = new DSMgr(filename);
    num_hits_ = 0;
    num_io_ = 0;
}

BMgr::~BMgr()
{
    delete replace_alg_;
    // write all dirty pages to disk and free BCB
    for (int i = 0; i < DEFBUFSIZE; i++)
    {
        BCB *head = pid_to_bcb_[i];
        while (head->next != nullptr)
        {
            BCB *cur = head->next;
            if (cur->dirty)
            {
                int page_id = cur->page_id;
                bFrame bf = buffer_pool_[cur->frame_id];
                disk_manager_->WritePage(page_id, bf);
                num_io_++;
            }
            head->next = cur->next;
            delete cur;
        }
        delete head;
    }

    delete disk_manager_;
}

/* 请求访问页面[Pin] */
frame_id_t BMgr::FixPage(int page_id, int op_type)
{
    lock_guard<mutex> lock(latch_); // 自动进行上锁解锁操作，避免死锁，保证线程安全
    int frame_id;
    // 1. 页面在缓冲区
    BCB *cur = SearchPage(page_id);
    if (cur != nullptr)
    {
        num_hits_++;
        frame_id = cur->frame_id;
        if (cur->count == 0)             // 没有其他线程访问 则本程序负责
            replace_alg_->Pin(frame_id); // 通知垃圾回收站 该页面正在被访问
        cur->count++;
        cur->dirty = (cur->dirty == 0) ? op_type : 1;
#ifdef DEBUG
        printf("page [%d] is in buffer and frame is [%d].\n", page_id, frame_id);
#endif
        return cur->frame_id;
    }

    // 2. 页面不在缓冲区
    frame_id = NewFrame();
    if (frame_id >= 0)
    {
#ifdef DEBUG
        printf("page [%d] is not in buffer and allocate frames [%d].\n", page_id, frame_id);
#endif
        DeletePage(frame_id);        // 将页面从缓冲区删除 如果有更新则写入到磁盘中
        replace_alg_->Pin(frame_id); // 本线程第一次读入该页面 所以负责通知回收站
        bFrame bf = disk_manager_->ReadPage(page_id);
        num_io_++;
        buffer_pool_[frame_id] = bf;
        BCB *newBCB = CreateBCB(page_id, frame_id);
        newBCB->dirty = op_type;
        return frame_id;
    }

#ifdef DEBUG
    printf("there are no free slots (i.e., all the pages are pinned).\n");
#endif
    return -1;
}

/* 请求创建一个新的页面[Pin] */
frame_id_t BMgr::FixNewPage(int &page_id)
{
    lock_guard<mutex> lock(latch_); // 使用自动获取和释放锁
    page_id = disk_manager_->NewPage();
    int frame_id = NewFrame();
    if (frame_id >= 0)
    {
#ifdef DEBUG
        printf("page [%d] is not in buffer and allocate frames [%d].\n", page_id, frame_id);
#endif
        DeletePage(frame_id);                       // write victim back if page is victim and dirty
        replace_alg_->Pin(frame_id);                // 本线程第一次读入该页面 所以负责通知回收站
        BCB *newBCB = CreateBCB(page_id, frame_id); // 数据在内存中产生 不需要从磁盘读入
        newBCB->dirty = 1;                          // 表示写入数据
        return frame_id;
    }

#ifdef DEBUG
    printf("there are no free slots (i.e., all the pages are pinned).\n");
#endif
    return frame_id;
}

/* 结束访问页面[Unpin] */
frame_id_t BMgr::UnfixPage(int page_id)
{
    lock_guard<mutex> lock(latch_);
    int frame_id = -1;
    // 页面在缓冲区
    BCB *cur = SearchPage(page_id);
    if (cur != nullptr)
    {
        frame_id = cur->frame_id;
        cur->count--;
        if (cur->count == 0)               // 最后一个使用该页面的线程负责
            replace_alg_->Unpin(frame_id); // 将该页面放入回收站
        return frame_id;
    }
    return -1; // 页面不在缓冲区中则返回-1
}

/* 返回缓存空闲页框数 */
int BMgr::NumFreeFrames()
{
    int answer = 0;
    for (int i = 0; i < DEFBUFSIZE; i++)
    {
        if (fid_to_pid_[i] == -1)
        {
            answer += 1;
        }
    }
    return answer;
}

// Internal Functions

/* 从磁盘中加载页面到缓冲区 */
BCB *BMgr::LoadPage(int page_id, int frame_id)
{
    // lock_guard<mutex> lock(latch_);
    bFrame bf = disk_manager_->ReadPage(page_id);
    num_io_++;
    buffer_pool_[frame_id] = bf;
    return CreateBCB(page_id, frame_id);
}

/* 将页面从缓冲区删除 如果有更新则写入到磁盘中 */
bool BMgr::DeletePage(int frame_id)
{
    // lock_guard<mutex> lock(latch_);
    int page_id = fid_to_pid_[frame_id];
    if (page_id == -1)
        return false;
    BCB *oldBCB = SearchPage(page_id);
    if (oldBCB == nullptr)
        return true;
    if (oldBCB->count != 0)
        return false;
    if (oldBCB->dirty)
    {
        disk_manager_->WritePage(page_id, buffer_pool_[frame_id]);
        oldBCB->dirty = 0;
        num_io_++;
    }
    return RemoveBCB(frame_id);
}

/* 分配缓冲区页框 没有空闲页框则选择一个页面进行替换 */
frame_id_t BMgr::NewFrame()
{
    for (int i = 0; i < DEFBUFSIZE; i++)
    {
        if (fid_to_pid_[i] == -1)
        {
            return i; // return free frame_id
        }
    }
    return replace_alg_->Victim(); // select an unpinned Page as the "victim" page.
}

BCB *BMgr::CreateBCB(int page_id, int frame_id)
{
    BCB *newBCB = new BCB(page_id, frame_id);
    BCB *pb = pid_to_bcb_[Hash(page_id)]; // find head of BCB hash table
    while (pb->next != nullptr)           // add new page BCB to hash table
        pb = pb->next;
    pb->next = newBCB;
    fid_to_pid_[frame_id] = page_id;
    return newBCB;
}

bool BMgr::RemoveBCB(int frame_id)
{
    int page_id = fid_to_pid_[frame_id];
    BCB *pb = pid_to_bcb_[Hash(page_id)];

    while (pb->next != nullptr)
    {
        BCB *cur = pb->next;
        if (cur->page_id == page_id)
        {
            pb->next = cur->next;
            delete cur;
            fid_to_pid_[frame_id] = -1;
            return true;
        }
        pb = pb->next;
    }
    return false;
}

BCB *BMgr::SearchBCB(int frame_id)
{
    int page_id = fid_to_pid_[frame_id];
    BCB *pb = pid_to_bcb_[Hash(page_id)];
    while (pb->next != nullptr)
    {
        BCB *cur = pb->next;
        if (cur->page_id == page_id)
        {
            assert(cur->frame_id == frame_id);
            return cur;
        }
        pb = pb->next;
    }
    return nullptr;
}

BCB *BMgr::SearchPage(int page_id)
{
    BCB *pb = pid_to_bcb_[Hash(page_id)];
    while (pb->next != nullptr)
    {
        BCB *cur = pb->next;
        if (cur->page_id == page_id)
        {
            // assert(fid_to_pid_[cur->frame_id] == page_id);
            return cur;
        }
        pb = pb->next;
    }
    return nullptr;
}

void BMgr::SetDirty(int frame_id)
{
    BCB *pb = SearchBCB(frame_id);
    pb->dirty = 1;
}

void BMgr::UnsetDirty(int frame_id)
{
    BCB *pb = SearchBCB(frame_id);
    pb->dirty = 0;
}

/* write all dirty pages to disk but don't free BCB */
void BMgr::WriteDirtys()
{
    lock_guard<mutex> lock(latch_);
    for (int i = 0; i < DEFBUFSIZE; i++)
    {
        BCB *pb = pid_to_bcb_[i];
        while (pb->next != nullptr)
        {
            BCB *cur = pb->next;
            if (cur->dirty)
            {
                int page_id = cur->page_id;
                bFrame bf = buffer_pool_[cur->frame_id];
                disk_manager_->WritePage(page_id, bf);
                num_io_++;
                cur->dirty = 0;
            }
            pb = pb->next;
        }
    }
}

bool BMgr::FlushPage(int page_id)
{
    lock_guard<mutex> lock(latch_);
    BCB *pb = pid_to_bcb_[Hash(page_id)];
    while (pb->next != nullptr)
    {
        BCB *cur = pb->next;
        if (cur->page_id == page_id)
        {
            if (cur->dirty)
            {
                bFrame bf = buffer_pool_[cur->frame_id];
                disk_manager_->WritePage(page_id, bf);
                num_io_++;
                cur->dirty = 0;
            }
            return true;
        }
        pb = pb->next;
    }
    return false;
}

void BMgr::PrintFrame(int frame_id)
{
    BCB *pb = SearchBCB(frame_id);
    printf("frame_id: %d\n", frame_id);
    printf("page_id : %d\n", pb->page_id);
    printf("dirty   : %d\n", pb->dirty);
    printf("count   : %d\n", pb->count);
    printf("\n");
}

int BMgr::Hash(int page_id)
{
    return page_id % DEFBUFSIZE;
}

bool BMgr::_is_Valid_Page(int page_id)
{
    if (page_id >= MAXPAGES || page_id < 0)
    {
        fprintf(stderr, "Error: page_id [%d] is unvalid. The valid range is 0~%d.\n",
                page_id, MAXPAGES - 1);
        return false;
    }
    return true;
}
