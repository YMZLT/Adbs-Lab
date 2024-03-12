#include "BMgr.h"
#include "omp.h"
#include "cmdline.h"

#include <iostream>

using namespace std;
void muti_thread_task(FILE *data_file, BMgr *bmgr, int thread_num)
{
    int op_type;
    int page_id;
#pragma omp parallel for num_threads(thread_num)
    for (int i = 0; i < NUM_PAGR_REQUEST; i++)
    {
        fscanf(data_file, "%d,%d", &op_type, &page_id);
        page_id = page_id % MAXPAGES;
        int frame_id = bmgr->FixPage(page_id, op_type);
        if (frame_id < 0)
            continue;
        bmgr->UnfixPage(page_id);
    }
}

void single_task(FILE *data_file, BMgr *bmgr)
{
    int op_type;
    int page_id;
    for (int i = 0; i < NUM_PAGR_REQUEST; i++)
    {
        fscanf(data_file, "%d,%d", &op_type, &page_id);
        page_id = page_id % MAXPAGES;
        int frame_id = bmgr->FixPage(page_id, op_type);
        if (frame_id < 0)
            continue;
        bmgr->UnfixPage(page_id);
    }
}

int main(int argc, char *argv[])
{
    // cmdline parser
    cmdline::parser cmd;
    cmd.add<string>("policy", 'p', "replacement policy: [lru|clock|2Q]", false,
                    "lru", cmdline::oneof<string>("lru", "clock", "2Q"));
    cmd.add<int>("thread", 't', "thread number", false, 1, cmdline::range(1, INT32_MAX));
    cmd.parse_check(argc, argv);
    string policy = "lru";
    int thread_num = 1;
    if (cmd.exist("policy"))
        policy = cmd.get<string>("policy");
    if (cmd.exist("thread"))
        thread_num = cmd.get<int>("thread");
    printf("policy: %s     thread: %d\n", policy.c_str(), thread_num);
    // init buffer pool manager
    BMgr *bmgr;
    if (policy == "clock")
        bmgr = new BMgr(DB_FILENAME, Policy::Clock);
    else if (policy == "2Q")
        bmgr = new BMgr(DB_FILENAME, Policy::TwoQ);
    else
        bmgr = new BMgr(DB_FILENAME, Policy::Lru);

    // init db file with MAXPAGES
    for (int i = bmgr->GetNumPages(); i < MAXPAGES; i++)
    {
        int page_id;
        bmgr->FixNewPage(page_id);
        bmgr->UnfixPage(page_id);
    }
    // read data file

    FILE *data_file = fopen(DATA_FILENAME, "r");
    if (data_file == NULL)
    {
        fprintf(stderr, "Error: file %s doesn't exist\n", DATA_FILENAME);
        exit(1);
    }
    // test alg
    clock_t start_time = clock();

    if (thread_num == 1)
        single_task(data_file, bmgr);
    else
        muti_thread_task(data_file, bmgr, thread_num);

    printf("-------------------------------------\n");
    printf("hit number: %d\n", bmgr->GetHitNum());
    printf("io number: %d\n", bmgr->GetIONum());
    printf("hit rate: %.3lf%%\n", bmgr->GetHitNum() * 100.0 / NUM_PAGR_REQUEST);
    printf("time taken: %.3fs\n",
           (double)(clock() - start_time) / CLOCKS_PER_SEC);
    printf("-------------------------------------\n");
    return 0;
}
