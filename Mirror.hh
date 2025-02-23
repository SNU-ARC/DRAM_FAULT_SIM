#ifndef __MIRROR_HH__
#define __MIRROR_HH__

#include <cstdio>
#include <cstdint>
#include <vector>
#include <list>

#define BUFFER_SIZE 1000
#define UNIT_MEMORY_UTIL 500 // page number at memory util 1%
#define FREE_SPACE_RATIO 10
#define KERNEL_PAGE_RATIO 3

#define MAX_PFN (1 << 25)
//#define MAX_PFN (1 << 28)
#define BITMAP_SIZE (MAX_PFN / 8)
#define MAX_LRU_SIZE 100000
#define MAX_LFU_SIZE 50000

//#define TOTAL_NODE_SIZE (UNIT_MEMORY_UTIL * FREE_SPACE_RATIO + MAX_LRU_SIZE + MAX_LFU_SIZE)
#define MAX_MIRROR_SIZE ((FREE_SPACE_RATIO - KERNEL_PAGE_RATIO) * MAX_PFN / 100)
#define LRU_LIST_RATIO 100
#define LFU_LIST_RATIO 0
#define MAX_LRU_MIRROR_SIZE (MAX_MIRROR_SIZE * LRU_LIST_RATIO / (LRU_LIST_RATIO + LFU_LIST_RATIO))
#define MAX_LFU_MIRROR_SIZE (MAX_MIRROR_SIZE * LFU_LIST_RATIO / (LRU_LIST_RATIO + LFU_LIST_RATIO))
#define TOTAL_NODE_SIZE (MAX_MIRROR_SIZE + MAX_LRU_SIZE + MAX_LFU_SIZE)

#define FREE 0
#define LRU_LIST 1
#define LFU_LIST 2
#define LRU_MIRROR 3
#define LFU_MIRROR 4

#define KERNEL_PAGE 0
#define ANON_PAGE 1
#define FILE_PAGE 2

#define WARMUP_PERIOD 1000000

class Node {
public:
    int list_type;
    uint64_t pfn;
    uint64_t age;
    uint64_t freq;
};

class Log {
public:
    uint64_t pfn;
    int page_type;
};

class MirrorModule {
public:
    MirrorModule();
    void reset_mirror();
    void init_mirror();
    //void access(uint64_t pfn, int page_type);
    bool access();
    uint64_t get_cur_trace_pfn();
    int get_cur_trace_page_type();
    uint64_t get_log_size();
    void set_trace_idx(uint64_t idx);
    void add_profile_buffer(uint64_t pfn, int page_type);
    void flush_profile_buffer();
    void process_pfn(uint64_t pfn, int page_type);
    std::list<Node*>::iterator search_list(uint64_t pfn, int list_type);
    void update_mirror_list();
    void select_top_n_lru(int n);
    void select_top_n_lfu(int n);
    void sort_lfu_mirror();
    void sort_lfu_list();
    void sort_lru_list();
    void insert_mirror(Node* candidate, int list_type);
    void insert_lfu_list(uint64_t pfn);
    void insert_lru_list(uint64_t pfn);
    void reset_lfu_list();
    Node* alloc_node();
    int has_mirror(uint64_t pfn);
    void set_mirror(uint64_t pfn);
    void remove_mirror(uint64_t pfn);
    void insert_log(uint64_t pfn, int page_type);
    void print_result();

private:
    std::list<Node*> free_list;
    std::list<Node*> lru_mirror;
    std::list<Node*> lfu_mirror;
    std::list<Node*> lru_list;
    std::list<Node*> lfu_list;
    std::list<std::pair<uint64_t, int>> profile_buffer; // (pfn, page_type)
    std::vector<Log> trace;
    uint64_t total_size_limit;
    uint64_t total_size;
    int free_size;
    uint8_t mirror_bitmap[BITMAP_SIZE];
    uint64_t num_anon_page;
    uint64_t trace_idx;
};

#endif