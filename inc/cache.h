#ifndef CACHE_H
#define CACHE_H

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "memory_class.h"

using namespace std;

// PAGE
extern uint32_t PAGE_TABLE_LATENCY, SWAP_LATENCY;

// CACHE TYPE
#define IS_ITLB 0
#define IS_DTLB 1
#define IS_STLB 2
#define IS_L1I 3
#define IS_L1D 4
//#define IS_L2C  5
#define IS_LLC 6

// INSTRUCTION TLB
#define ITLB_SET 16
#define ITLB_WAY 4
#define ITLB_RQ_SIZE 16
#define ITLB_WQ_SIZE 16
#define ITLB_PQ_SIZE 0
#define ITLB_MSHR_SIZE 8
#define ITLB_LATENCY 1

// DATA TLB
#define DTLB_SET 16
#define DTLB_WAY 4
#define DTLB_RQ_SIZE 16
#define DTLB_WQ_SIZE 16
#define DTLB_PQ_SIZE 0
#define DTLB_MSHR_SIZE 8
#define DTLB_LATENCY 1

// L1 INSTRUCTION CACHE
#define L1I_SET 128
#define L1I_WAY 8
#define L1I_RQ_SIZE 64
#define L1I_WQ_SIZE 64
#define L1I_PQ_SIZE 64
#define L1I_MSHR_SIZE 8
#define L1I_LATENCY 1

// L1 DATA CACHE
#define L1D_SET 128
#define L1D_WAY 8
#define L1D_RQ_SIZE 64
#define L1D_WQ_SIZE 64
#define L1D_PQ_SIZE 64
#define L1D_MSHR_SIZE 8
#define L1D_LATENCY 1

// L2 CACHE
// #define L2C_SET 512
// #define L2C_WAY 8
// #define L2C_RQ_SIZE 256
// #define L2C_WQ_SIZE 256
// #define L2C_PQ_SIZE 256
// #define L2C_MSHR_SIZE 128
// #define L2C_LATENCY 8  // 4 (L1I or L1D) + 8 = 12 cycles

// SECOND LEVEL TLB
#define STLB_SET NUM_CPUS * 128
#define STLB_WAY 16
#define STLB_RQ_SIZE (NUM_CPUS * (ITLB_MSHR_SIZE + DTLB_MSHR_SIZE))
#define STLB_WQ_SIZE (NUM_CPUS * (ITLB_MSHR_SIZE + DTLB_MSHR_SIZE))
#define STLB_PQ_SIZE (NUM_CPUS * (ITLB_MSHR_SIZE + DTLB_MSHR_SIZE))
#define STLB_MSHR_SIZE 16
#define STLB_LATENCY 1

// LAST LEVEL CACHE
#define LLC_SET NUM_CPUS * 2048
#define LLC_WAY 16
#define LLC_RQ_SIZE (NUM_CPUS * (L1I_MSHR_SIZE + L1D_MSHR_SIZE))
#define LLC_WQ_SIZE (NUM_CPUS * (L1I_MSHR_SIZE + L1D_MSHR_SIZE))
#define LLC_PQ_SIZE (NUM_CPUS * (L1I_MSHR_SIZE + L1D_MSHR_SIZE))
#define LLC_MSHR_SIZE 128
#define LLC_LATENCY 10 // 4 (L1I or L1D) + 8 + 20 = 32 cycles

class Stats {
  public:
    Stats() {
        for (int i = 0; i < NUM_CPUS; i += 1) {
            for (auto& metric : this->metrics) {
                this->roi_stats[i][metric] = 0;
                this->total_stats[i][metric] = 0;
            }
            this->cur_stats[i] = nullptr;
        }
    }

    void llc_prefetcher_operate(uint32_t cpu, uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type) {
        /* don't do anything if CPU is in warmup phase */
        if (!this->cur_stats[cpu])
            return;
        auto& stats = *this->cur_stats[cpu];
        addr <<= LOG2_BLOCK_SIZE;

        stats["Accesses"] += 1;
        stats["Misses"] += (1 - cache_hit);

        /* check prefetch hit */
        if (cache_hit == 1) {
            for (int i = 0; i < NUM_CPUS; i += 1) {
                int cnt = this->prefetched_blocks[i].erase(addr);
                if (cnt == 1) {
                    (*this->cur_stats[i])["Prefetch Hits"] += 1;
                    (*this->cur_stats[i])["Undecided Prefetches"] -= 1;
                }
            }
        }
    }

    void llc_prefetcher_cache_fill(uint32_t cpu, uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr) {
        /* don't do anything if CPU is in warmup phase */
        if (!this->cur_stats[cpu])
            return;
        auto& stats = *this->cur_stats[cpu];
        addr <<= LOG2_BLOCK_SIZE;
        evicted_addr <<= LOG2_BLOCK_SIZE;
        
        for (int i = 0; i < NUM_CPUS; i += 1) {
            int cnt = this->prefetched_blocks[i].erase(evicted_addr);
            if (cnt == 1) {
                (*this->cur_stats[i])["Non-useful Prefetches"] += 1;
                (*this->cur_stats[i])["Undecided Prefetches"] -= 1;
            }
        }

        if (prefetch == 1) {
            stats["Prefetches"] += 1;
            stats["Undecided Prefetches"] += 1;
            prefetched_blocks[cpu].insert(addr);
        }
    }

    void llc_prefetcher_inform_warmup_complete() {
        for (int i = 0; i < NUM_CPUS; i += 1)
            this->cur_stats[i] = &this->roi_stats[i];
    }

    void llc_prefetcher_inform_roi_complete(uint32_t cpu) {
        this->total_stats[cpu] = this->roi_stats[cpu]; /* copy roi_stats over to total_stats */
        this->cur_stats[cpu] = &this->total_stats[cpu];
    }

    void llc_prefetcher_final_stats(uint32_t cpu) {
        this->print_stats(this->total_stats, "Total", cpu);
    }

    void llc_prefetcher_roi_stats(uint32_t cpu) {
        this->print_stats(this->roi_stats, "ROI", cpu);
    }

    void llc_prefetcher_cur_stats(uint32_t cpu) {
        this->print_stats(*this->cur_stats, "Current", cpu);
    }

    void print_stats(unordered_map<string, uint64_t> stats[], string name, uint32_t cpu) {
        cout << "=== CPU " << cpu << " " << name << " Stats ===" << endl;
        for (auto &metric : this->metrics)
            cout << "* CPU " << cpu << " " << name << " " << metric << ": " << stats[cpu][metric] << endl;
    }

  private:
    vector<string> metrics = {"Accesses", "Misses", "Prefetches", "Prefetch Hits", "Non-useful Prefetches", "Undecided Prefetches"};
    unordered_map<string, uint64_t> roi_stats[NUM_CPUS], total_stats[NUM_CPUS];
    unordered_map<string, uint64_t> *cur_stats[NUM_CPUS];
    unordered_set<uint64_t> prefetched_blocks[NUM_CPUS];
};

class CACHE : public MEMORY {
  public:
    uint32_t cpu;
    const string NAME;
    const uint32_t NUM_SET, NUM_WAY, NUM_LINE, WQ_SIZE, RQ_SIZE, PQ_SIZE, MSHR_SIZE;
    uint32_t LATENCY;
    BLOCK **block;
    int fill_level;
    uint32_t MAX_READ, MAX_FILL;
    uint8_t cache_type;
    Stats stats;

    // prefetch stats
    uint64_t pf_requested, pf_issued, pf_useful, pf_useless, pf_fill;

    // queues
    PACKET_QUEUE WQ{NAME + "_WQ", WQ_SIZE},       // write queue
        RQ{NAME + "_RQ", RQ_SIZE},                // read queue
        PQ{NAME + "_PQ", PQ_SIZE},                // prefetch queue
        MSHR{NAME + "_MSHR", MSHR_SIZE},          // MSHR
        PROCESSED{NAME + "_PROCESSED", ROB_SIZE}; // processed queue

    uint64_t sim_access[NUM_CPUS][NUM_TYPES], sim_hit[NUM_CPUS][NUM_TYPES], sim_miss[NUM_CPUS][NUM_TYPES],
        roi_access[NUM_CPUS][NUM_TYPES], roi_hit[NUM_CPUS][NUM_TYPES], roi_miss[NUM_CPUS][NUM_TYPES];

    // constructor
    CACHE(string v1, uint32_t v2, int v3, uint32_t v4, uint32_t v5, uint32_t v6, uint32_t v7, uint32_t v8)
        : NAME(v1), NUM_SET(v2), NUM_WAY(v3), NUM_LINE(v4), WQ_SIZE(v5), RQ_SIZE(v6), PQ_SIZE(v7), MSHR_SIZE(v8) {

        LATENCY = 0;

        // cache block
        block = new BLOCK *[NUM_SET];
        for (uint32_t i = 0; i < NUM_SET; i++) {
            block[i] = new BLOCK[NUM_WAY];

            for (uint32_t j = 0; j < NUM_WAY; j++) {
                block[i][j].lru = j;
            }
        }

        for (uint32_t i = 0; i < NUM_CPUS; i++) {
            upper_level_icache[i] = NULL;
            upper_level_dcache[i] = NULL;

            for (uint32_t j = 0; j < NUM_TYPES; j++) {
                sim_access[i][j] = 0;
                sim_hit[i][j] = 0;
                sim_miss[i][j] = 0;
                roi_access[i][j] = 0;
                roi_hit[i][j] = 0;
                roi_miss[i][j] = 0;
            }
        }

        lower_level = NULL;
        extra_interface = NULL;
        fill_level = -1;
        MAX_READ = 1;
        MAX_FILL = 1;

        pf_requested = 0;
        pf_issued = 0;
        pf_useful = 0;
        pf_useless = 0;
        pf_fill = 0;
    };

    // destructor
    ~CACHE() {
        for (uint32_t i = 0; i < NUM_SET; i++)
            delete[] block[i];
        delete[] block;
    };

    // functions
    int add_rq(PACKET *packet),
        add_wq(PACKET *packet),
        add_pq(PACKET *packet);

    void return_data(PACKET *packet),
        operate(),
        increment_WQ_FULL(uint64_t address);

    uint32_t get_occupancy(uint8_t queue_type, uint64_t address),
        get_size(uint8_t queue_type, uint64_t address);

    int check_hit(PACKET *packet),
        invalidate_entry(uint64_t inval_addr), check_mshr(PACKET *packet),
        prefetch_line(uint32_t cpu, uint64_t ip, uint64_t base_addr, uint64_t pf_addr, int fill_level),
        kpc_prefetch_line(uint64_t base_addr, uint64_t pf_addr, int fill_level, int delta, int depth, int signature, int confidence);

    void handle_fill(), handle_writeback(), handle_read(), handle_prefetch();

    void add_mshr(PACKET *packet),
        update_fill_cycle(),
        llc_initialize_replacement(),
        update_replacement_state(uint32_t cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type, uint8_t hit),
        llc_update_replacement_state(uint32_t cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type, uint8_t hit),
        lru_update(uint32_t set, uint32_t way),
        fill_cache(uint32_t set, uint32_t way, PACKET *packet),
        replacement_final_stats(), llc_replacement_final_stats(),

        /* === LLC Prefetcher === */
        llc_prefetcher_initialize(uint32_t cpu),
        llc_prefetcher_operate(uint32_t cpu, uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type),
        llc_prefetcher_cache_fill(uint32_t cpu, uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr),
        llc_prefetcher_inform_warmup_complete(),
        llc_prefetcher_inform_roi_complete(uint32_t cpu),
        llc_prefetcher_roi_stats(uint32_t cpu),
        llc_prefetcher_final_stats(uint32_t cpu),
        
        llc_prefetcher_initialize_(uint32_t cpu),
        llc_prefetcher_operate_(uint32_t cpu, uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type),
        llc_prefetcher_cache_fill_(uint32_t cpu, uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr),
        llc_prefetcher_inform_warmup_complete_(),
        llc_prefetcher_inform_roi_complete_(uint32_t cpu),
        llc_prefetcher_roi_stats_(uint32_t cpu),
        llc_prefetcher_final_stats_(uint32_t cpu),
        /* ====================== */
        
        l1d_prefetcher_initialize(),
        // l2c_prefetcher_initialize(),
        prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type),
        l1d_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type),
        // l2c_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type),
        prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr),
        l1d_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr),
        // l2c_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t
        // evicted_addr),
        // prefetcher_final_stats(),
        l1d_prefetcher_final_stats();

    uint32_t get_set(uint64_t address),
        get_way(uint64_t address, uint32_t set),
        find_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK *current_set, uint64_t ip,
            uint64_t full_addr, uint32_t type),
        llc_find_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK *current_set, uint64_t ip,
            uint64_t full_addr, uint32_t type),
        lru_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK *current_set, uint64_t ip,
            uint64_t full_addr, uint32_t type);
};

#endif
