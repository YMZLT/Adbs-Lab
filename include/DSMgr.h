// DSMgr.h

#ifndef DSMGR_H
#define DSMGR_H

#include "Common.h"

#include <iostream>
#include <string>
#include <cstdio>

using namespace std;


class DSMgr
{
public:
    DSMgr(string filename);
    ~DSMgr();

    int OpenFile(string filename);
    int CloseFile();
    bFrame ReadPage(int page_id);
    int WritePage(int page_id, const bFrame &frm);
    page_id_t NewPage();
    int Seek(int offset, int pos);
    FILE *GetFile();
    void IncNumPages();
    int GetNumPages();
    void SetUse(int index, int use_bit);
    int GetUse(int index);

private:
    FILE *curr_file_;
    int num_pages_;
    int pages_table_[MAXPAGES];
};

#endif // DSMGR_H