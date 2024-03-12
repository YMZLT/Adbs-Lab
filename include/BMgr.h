// BMgr.h

#ifndef BMGR_H
#define BMGR_H

#include "Common.h"
#include "DSMgr.h"
#include "ReplaceAlg.h"

#include <iostream>
#include <string>

using namespace std;

class BMgr
{

private:
    bFrame buffer_pool_[DEFBUFSIZE]; // buffer pool
    int fid_to_pid_[DEFBUFSIZE];     // Hash Table: frame_id to page_id, default value -1
    BCB *pid_to_bcb_[DEFBUFSIZE];    // Hash Table: page_id to BCB chain head

    ReplaceAlg *replace_alg_;
    DSMgr *disk_manager_;
    int num_io_;
    int num_hits_;
    mutex latch_;

    // Internal Functions
    frame_id_t NewFrame();
    frame_id_t SelectVictim();
    int Hash(int page_id);
    bool _is_Valid_Page(int page_id);
    BCB *LoadPage(int page_id, int frame_id);
    bool DeletePage(int frame_id);
    BCB *CreateBCB(int page_id, int frame_id);
    bool RemoveBCB(int frame_id);
    BCB *SearchBCB(int frame_id);
    BCB *SearchPage(int page_id);
    void SetDirty(int frame_id);
    void UnsetDirty(int frame_id);
    void WriteDirtys();
    bool FlushPage(int page_id);
    void PrintFrame(int frame_id);

public:
    BMgr(string filename, int alg = Policy::Lru);
    ~BMgr();

    // Interface functions
    frame_id_t FixPage(int page_id, int op_type);
    frame_id_t FixNewPage(int &page_id);
    frame_id_t UnfixPage(int page_id);

    int NumFreeFrames();
    int GetIONum() { return num_io_; }
    int GetHitNum() { return num_hits_; }
    int GetNumPages() { return disk_manager_->GetNumPages(); }
};

#endif // BMGR_H