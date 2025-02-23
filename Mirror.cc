#include <iostream>
#include <cstring>
#include <cassert>
#include <algorithm>
#include "Mirror.hh"

MirrorModule::MirrorModule() {
    init_mirror();
}

void MirrorModule::init_mirror() {
    for (int i = 0; i < TOTAL_NODE_SIZE; i++) {
        Node* node = new Node;
        memset(node, 0, sizeof(Node));
        free_list.push_back(node);
    }

    total_size = 0;
    total_size_limit = 0;
    num_anon_page = 0;
    trace_idx = 0;
}

void MirrorModule::reset_mirror() {
    memset(mirror_bitmap, 0, BITMAP_SIZE * sizeof(uint8_t));
    //assert(lru_mirror.size() + lfu_mirror.size() + lru_list.size() + lfu_list.size() + free_list.size() == TOTAL_NODE_SIZE);
    while (!lru_mirror.empty()) {
        Node* node = lru_mirror.front();
        lru_mirror.pop_front();
        memset(node, 0, sizeof(Node));
        free_list.push_back(node);
    }
    while (!lfu_mirror.empty()) {
        Node* node = lfu_mirror.front();
        lfu_mirror.pop_front();
        memset(node, 0, sizeof(Node));
        free_list.push_back(node);
    }
    while (!lru_list.empty()) {
        Node* node = lru_list.front();
        lru_list.pop_front();
        memset(node, 0, sizeof(Node));
        free_list.push_back(node);
    }
    while (!lfu_list.empty()) {
        Node* node = lfu_list.front();
        lfu_list.pop_front();
        memset(node, 0, sizeof(Node));
        free_list.push_back(node);
    }
    //while (free_list.size()) {
    //    Node* node = free_list.front();
    //    free_list.pop_front();
    //    delete node;
    //}
    //for (int i = free_list.size(); i < TOTAL_NODE_SIZE; i++) {
    //    Node* node = new Node;
    //    memset(node, 0, sizeof(Node));
    //    free_list.push_back(node);
    //}

    //lru_list_map.clear();
    //lfu_list_map.clear();

    for(int i = 0; i < HASH_BUCKET_SIZE; i++){
        lru_list_hash_bucket[i].clear();
        lru_mirror_hash_bucket[i].clear();
    }

    total_size = 0;
    total_size_limit = 0;
    num_anon_page = 0;
    trace_idx = 0;
}

//void MirrorModule::access(uint64_t pfn, int page_type) {
//    add_profile_buffer(pfn, page_type);
//}

bool MirrorModule::access() {
    uint64_t pfn = trace[trace_idx].pfn;
    int page_type = trace[trace_idx].page_type;
    if(page_type == ANON_PAGE)
        add_profile_buffer(pfn, page_type);
    //trace_idx = (trace_idx + 1) % trace.size();
    trace_idx++;
    return trace_idx >= trace.size();
}

uint64_t MirrorModule::get_cur_trace_pfn() {
    return trace[trace_idx].pfn;
}

int MirrorModule::get_cur_trace_page_type() {
    return trace[trace_idx].page_type;
}

void MirrorModule::set_trace_idx(uint64_t idx) {
    trace_idx = idx;
}

uint64_t MirrorModule::get_log_size() {
    return trace.size();
}

void MirrorModule::add_profile_buffer(uint64_t pfn, int page_type) {
    if (profile_buffer.size() < BUFFER_SIZE)
        profile_buffer.push_back({ pfn, page_type });
    else {
        total_size_limit += 100;
        flush_profile_buffer();
    }
}

void MirrorModule::flush_profile_buffer() {
    for (auto it = profile_buffer.begin(); it != profile_buffer.end(); it++)
        process_pfn(it->first, it->second);

    update_mirror_list();
    reset_lfu_list();
    profile_buffer.clear();
}

void MirrorModule::process_pfn(uint64_t pfn, int page_type) {
    if (page_type == ANON_PAGE) {
        num_anon_page++;
        auto cur = search_list(pfn, LRU_MIRROR);
        auto end = lru_mirror.end();
        if (cur != end) {
            (*cur)->freq++;
            (*cur)->age = num_anon_page;
            Node* cur_node = *cur;
            lru_mirror.erase(cur);
            lru_mirror.push_front(cur_node);
            for(auto& i: lru_mirror_hash_bucket[cur_node->pfn % HASH_BUCKET_SIZE]) {
                if(i.first == cur_node->pfn) {
                    i.second = lru_mirror.begin();
                    break;
                }
            }
            insert_lru_list(pfn);
            return;
        }

        cur = search_list(pfn, LFU_MIRROR);
        end = lfu_mirror.end();
        if (cur != end) {
            (*cur)->freq++;
            (*cur)->age = num_anon_page;
            insert_lru_list(pfn);
        }
        else {
            insert_lru_list(pfn);
            insert_lfu_list(pfn);
        }
    }
}

std::list<Node*>::iterator MirrorModule::search_list(uint64_t pfn, int list_type) {
    std::list<Node*>::iterator cur, end;
    int idx;

    switch (list_type) {
    case LRU_LIST:
        //cur = lru_list.begin();
        end = lru_list.end();

        idx = pfn % HASH_BUCKET_SIZE;
        for(auto i: lru_list_hash_bucket[idx]) {
            if(i.first == pfn)
                return i.second;
        }
        return end;
    case LFU_LIST:
        cur = lfu_list.begin();
        end = lfu_list.end();
        break;
    case LRU_MIRROR:
        //cur = lru_mirror.begin();
        end = lru_mirror.end();

        idx = pfn % HASH_BUCKET_SIZE;
        for(auto i: lru_mirror_hash_bucket[idx]) {
            if(i.first == pfn)
                return i.second;
        }
        return end;
    case LFU_MIRROR:
        cur = lfu_mirror.begin();
        end = lfu_mirror.end();
        break;
    }

    while (cur != end) {
        if ((*cur)->pfn == pfn) {
            //if (list_type == LRU_MIRROR) {
            //    Node* cur_node = *cur;
            //    lru_mirror.erase(cur);
            //    lru_mirror.push_front(cur_node);
            //    cur = lru_mirror.begin();
            //    end = lru_mirror.end();
            //}
            break;
        }
        cur = std::next(cur);
    }

    if (cur == end)
        return end;
    return cur;
}

void MirrorModule::update_mirror_list() {
    sort_lfu_list();
    sort_lfu_mirror();
    //sort_lru_list();
    select_top_n_lfu(LFU_LIST_RATIO);
    select_top_n_lru(LRU_LIST_RATIO);
}

void MirrorModule::select_top_n_lru(int n) {
    std::list<Node*>::iterator cur = lru_list.begin();
    std::list<Node*>::iterator end = lru_list.end();
    int count = 0;

    while (cur != end && count < n) {
        if (has_mirror((*cur)->pfn)) {
            cur = std::next(cur);
            continue;
        }
        insert_mirror(*cur, LRU_LIST);
        count++;
        cur = std::next(cur);
    }
}

void MirrorModule::select_top_n_lfu(int n) {
    std::list<Node*>::iterator cur = lfu_list.begin();
    std::list<Node*>::iterator end = lfu_list.end();
    int count = 0;

    while (cur != end && count < n) {
        if (has_mirror((*cur)->pfn)) {
            cur = std::next(cur);
            continue;
        }
        insert_mirror(*cur, LFU_LIST);
        count++;
        cur = std::next(cur);
    }
}

void MirrorModule::sort_lfu_mirror() {
    if (lfu_mirror.empty())
        return;

    std::list<Node*>::iterator cur = lfu_mirror.begin();
    std::list<Node*>::iterator end = lfu_mirror.end();

    while (cur != end) {
        auto max_node = cur;
        auto it = std::next(cur);

        while (it != end) {
            if ((*it)->freq > (*max_node)->freq)
                max_node = it;
            it = std::next(it);
        }

        if (max_node != cur) {
            Node* tmp = *max_node;
            lfu_mirror.erase(max_node);
            lfu_mirror.insert(cur, tmp);
            continue;
        }
        cur = std::next(cur);
    }
}

void MirrorModule::sort_lfu_list() {
    if (lfu_list.empty())
        return;

    std::list<Node*>::iterator cur = lfu_list.begin();
    std::list<Node*>::iterator end = lfu_list.end();

    while (cur != end) {
        auto max_node = cur;
        auto it = std::next(cur);

        while (it != end) {
            if ((*it)->freq > (*max_node)->freq)
                max_node = it;
            it = std::next(it);
        }

        if (max_node != cur) {
            Node* tmp = *max_node;
            lfu_list.erase(max_node);
            lfu_list.insert(cur, tmp);
            continue;
        }

        cur = std::next(cur);
    }
}

void MirrorModule::insert_mirror(Node* candidate, int list_type) {
    std::list<Node*>::iterator cur, end;

    if (list_type == LFU_LIST) {
        if (lfu_mirror.size() < MAX_LFU_MIRROR_SIZE) {
            Node* new_node = alloc_node();
            new_node->pfn = candidate->pfn;
            new_node->age = candidate->age;
            new_node->freq = candidate->freq;
            new_node->list_type = LFU_MIRROR;

            cur = lfu_mirror.begin();
            end = lfu_mirror.end();

            while (cur != end && (*cur)->freq > new_node->freq)
                cur = std::next(cur);

            lfu_mirror.insert(cur, new_node);
            set_mirror(new_node->pfn);
        }
        else {
            cur = lfu_mirror.begin();
            end = lfu_mirror.end();

            auto min_freq_node = cur;

            while (cur != end) {
                if ((*cur)->freq < (*min_freq_node)->freq)
                    min_freq_node = cur;
                cur = std::next(cur);
            }

            if (candidate->freq > (*min_freq_node)->freq) {
                remove_mirror((*min_freq_node)->pfn);
                set_mirror(candidate->pfn);
                (*min_freq_node)->pfn = candidate->pfn;
                (*min_freq_node)->age = candidate->age;
                (*min_freq_node)->freq = candidate->freq;
            }
        }
    }
    else if (list_type == LRU_LIST) {
        if (lru_mirror.size() < MAX_LRU_MIRROR_SIZE) {
            Node* new_node = alloc_node();
            new_node->pfn = candidate->pfn;
            new_node->age = candidate->age;
            new_node->freq = candidate->freq;
            new_node->list_type = LRU_MIRROR;

            cur = lru_mirror.begin();
            end = lru_mirror.end();

            while (cur != end && (*cur)->age > new_node->age)
                cur = std::next(cur);

            lru_mirror.insert(cur, new_node);
            lru_mirror_hash_bucket[new_node->pfn % HASH_BUCKET_SIZE].push_back({new_node->pfn, std::prev(cur)});
            set_mirror(new_node->pfn);
        }
        else {
            cur = lfu_mirror.begin();
            end = lfu_mirror.end();

            auto min_age_node = cur;

            while (cur != end) {
                if ((*cur)->age < (*min_age_node)->age)
                    min_age_node = cur;
                cur = std::next(cur);
            }

            if (candidate->age > (*min_age_node)->age) {
                remove_mirror((*min_age_node)->pfn);
                for(auto it = lru_mirror_hash_bucket[(*min_age_node)->pfn % HASH_BUCKET_SIZE].begin(); 
                    it != lru_mirror_hash_bucket[(*min_age_node)->pfn % HASH_BUCKET_SIZE].end(); it++) {
                    if(it->first == (*min_age_node)->pfn) {
                        lru_mirror_hash_bucket[(*min_age_node)->pfn % HASH_BUCKET_SIZE].erase(it);
                        break;
                    }
                }
                set_mirror(candidate->pfn);
                (*min_age_node)->pfn = candidate->pfn;
                (*min_age_node)->age = candidate->age;
                (*min_age_node)->freq = candidate->freq;
                lru_mirror_hash_bucket[candidate->pfn % HASH_BUCKET_SIZE].push_back({candidate->pfn, cur});
            }
        }
    }
}

void MirrorModule::sort_lru_list() {
    if (lru_list.empty())
        return;

    std::list<Node*>::iterator cur = lru_list.begin();
    std::list<Node*>::iterator end = lru_list.end();

    while(cur != end) {
        auto max_node = cur;
        auto it = std::next(cur);

        while(it != end) {
            if((*it)->age > (*max_node)->age)
                max_node = it;
            it = std::next(it);
        }

        if(max_node != cur) {
            Node* tmp = *max_node;
            lru_list.erase(max_node);
            lru_list.insert(cur, tmp);
            continue;
        }

        cur = std::next(cur);
    }
}

void MirrorModule::insert_lfu_list(uint64_t pfn) {
    auto cur = search_list(pfn, LFU_LIST);
    auto end = lfu_list.end();

    if (cur == end) {
        Node* new_node = alloc_node();
        new_node->pfn = pfn;
        new_node->freq = 1;
        new_node->age = num_anon_page;
        new_node->list_type = LFU_LIST;
        lfu_list.push_front(new_node);
    }
    else {
        (*cur)->freq++;
        (*cur)->age = num_anon_page;
    }
}

void MirrorModule::insert_lru_list(uint64_t pfn) {
    auto cur = search_list(pfn, LRU_LIST);
    auto end = lru_list.end();

    if (cur == end) {
        Node* new_node = alloc_node();
        new_node->pfn = pfn;
        new_node->freq = 1;
        new_node->age = num_anon_page;
        new_node->list_type = LRU_LIST;
        lru_list.push_front(new_node);
        lru_list_hash_bucket[pfn % HASH_BUCKET_SIZE].push_back({pfn, lru_list.begin()});
    }
    else {
        (*cur)->freq++;
        (*cur)->age = num_anon_page;
        Node* cur_node = *cur;
        lru_list.erase(cur);
        lru_list.push_front(cur_node);
        for(auto& i: lru_list_hash_bucket[pfn % HASH_BUCKET_SIZE]) {
            if(i.first == pfn) {
                i.second = lru_list.begin();
                break;
            }
        }
    }
    //lru_list_map[pfn] = lru_list.begin();
}

void MirrorModule::reset_lfu_list() {
    for (int i = 0; i < lfu_list.size(); i++) {
        Node* node = new Node;
        memset(node, 0, sizeof(Node));
        free_list.push_back(node);
    }
    while (lfu_list.size()) {
        Node* node = lfu_list.front();
        lfu_list.pop_front();
        delete node;
    }
}

Node* MirrorModule::alloc_node() {
    if (free_list.empty())
        return NULL;

    Node* ret = free_list.front();
    free_list.pop_front();
    return ret;
}

int MirrorModule::has_mirror(uint64_t pfn) {
    return mirror_bitmap[pfn / 8] & (1 << (pfn % 8));
}

void MirrorModule::set_mirror(uint64_t pfn) {
    mirror_bitmap[pfn / 8] |= (1 << (pfn % 8));
}

void MirrorModule::remove_mirror(uint64_t pfn) {
    mirror_bitmap[pfn / 8] &= ~(1 << (pfn % 8));
}

void MirrorModule::insert_log(uint64_t pfn, int page_type) {
    Log log;
    log.pfn = pfn;
    log.page_type = page_type;
    trace.push_back(log);
}

void MirrorModule::print_result() {
    std::cout << "LRU_MIRROR: " << lru_mirror.size() << std::endl;
    //for(auto i: lru_mirror)
    //    std::cout << "pfn: " << std::hex << i->pfn << ", age: " << std::dec << i->age << std::endl;
    //std::cout << "LFU_MIRROR: " << lfu_mirror.size() << std::endl;
    //for(auto i: lfu_mirror)
    //    std::cout << "pfn: " << std::hex << i->pfn << ", freq: " << std::dec << i->freq << std::endl;
    std::cout << "LRU_LIST: " << lru_list.size() << std::endl;
    //for(auto i: lru_list)
    //    std::cout << "pfn: " << std::hex << i->pfn << ", age: " << std::dec << i->age << std::endl;
    //std::cout << "LFU_LIST: " << lfu_list.size() << std::endl;
    //for(auto i: lfu_list)
    //    std::cout << "pfn: " << std::hex << i->pfn << ", freq: " << std::dec << i->freq << std::endl;
}