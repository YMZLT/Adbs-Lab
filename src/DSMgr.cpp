#include "DSMgr.h"

DSMgr::DSMgr(string filename)
{
    OpenFile(filename);
    int index;
    for (index = 0; index < MAXPAGES; index++)
    {
        if (index < num_pages_)
            pages_table_[index] = 1;
        else
            pages_table_[index] = 0;
    }
}

DSMgr::~DSMgr()
{
    CloseFile();
}

int DSMgr::OpenFile(string filename)
{
    // printf("open file %s ....\n", filename.c_str());

    // curr_file_ = fopen(filename.c_str(), "r+"); // file pointer is in SEEK_SET
    // if (curr_file_ == nullptr)                  // if file is not exist, return null
    // {
    //     curr_file_ = fopen(filename.c_str(), "w");                // create new empty file
    //     curr_file_ = freopen(filename.c_str(), "r+", curr_file_); // open this empty file again for changing the mode
    // }
    // else
    // {
    //     fseek(curr_file_, 0L, SEEK_END); // set file pointer to end
    //     long len = ftell(curr_file_);    // return current position
    //     num_pages_ = len / PAGESIZE;     // calculate the number of pages_table_
    // }

    curr_file_ = fopen(filename.c_str(), "w");                // create new empty file even if file is exist
    curr_file_ = freopen(filename.c_str(), "r+", curr_file_); // open this empty file again for changing the mode
    fseek(curr_file_, 0L, SEEK_END);                          // set file pointer to end
    long len = ftell(curr_file_);                             // return current position
    num_pages_ = len / PAGESIZE;                              // calculate the number of pages_table_

    return curr_file_ != nullptr;
}

int DSMgr::CloseFile()
{
    // printf("close file.\n");
    return fclose(curr_file_);
}

bFrame DSMgr::ReadPage(int page_id)
{
    bFrame bf;

    if (Seek(page_id * PAGESIZE, SEEK_SET) != 0)
    {
        fprintf(stderr, "Error: cannot find page: %d\n", page_id);
        exit(1);
    }
    fread(bf.field, 1, PAGESIZE, curr_file_); // read 1B * PAGESIZE from file to bframe
    return bf;
}

int DSMgr::WritePage(int page_id, const bFrame &frm)
{
    if (Seek(page_id * PAGESIZE, SEEK_SET) != 0)
    {
        fprintf(stderr, "Error[WritePage]: Cannot find page: %d\n", page_id);
        exit(1);
    }
    // printf("write page [%d].\n", page_id);
    return fwrite(frm.field, 1, PAGESIZE, curr_file_);
}

page_id_t DSMgr::NewPage()
{
    if (num_pages_ < MAXPAGES) // still has free pages_table_
    {
        int page_id = num_pages_;
        IncNumPages();

        char field[PAGESIZE];
        fseek(curr_file_, 0L, SEEK_END);
        fwrite(field, 1, PAGESIZE, curr_file_); // add this new page to file end

        SetUse(page_id, 1);
        return page_id;
    }

    for (int i = 0; i < MAXPAGES; i++) // recycle page
    {
        if (pages_table_[i] == 0) // this page is reusable
        {
            SetUse(i, 1);
            return i;
        }
    }
    fprintf(stderr, "Error[NewPage]: No page can be used.[MAXPAGES=%d]\n", MAXPAGES);
    exit(1);
}

int DSMgr::Seek(int offset, int pos)
{
    return fseek(curr_file_, (long)offset, pos); // if success return 0 ,else return not 0
}

FILE *DSMgr::GetFile()
{
    return curr_file_;
}

void DSMgr::IncNumPages()
{
    if (num_pages_ == MAXPAGES)
    {
        fprintf(stderr, "Error[IncNumPages]: The number of page is already up to MAX %d.\n", MAXPAGES);
        exit(1);
    }
    num_pages_++;
}

int DSMgr::GetNumPages()
{
    return num_pages_;
}

void DSMgr::SetUse(int index, int use_bit)
{
    pages_table_[index] = use_bit;
}

int DSMgr::GetUse(int index)
{
    return pages_table_[index];
}
