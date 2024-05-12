#pragma once

#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <unordered_map>
#include <string>
#include <utility>
#include <vector>

// #define PROFILE_EN
// #define DEBUG_EN
// #define ITER_PROFILE_EN
// #define VERTEXCUT_PROFILE_EN

class profiler_t {
public:
   profiler_t() {}
   profiler_t(int n) : nthreads(n) {
      num_load_chunk = (size_t*)calloc(n, sizeof(size_t));
      num_get_chunk = (size_t*)calloc(n, sizeof(size_t));
   }

   ~profiler_t() {
      if (num_load_chunk) free(num_load_chunk);
      if (num_get_chunk) free(num_get_chunk);
   }

   inline size_t get_num_load_chunk(int tid) { return num_load_chunk[tid]; }
   inline size_t get_num_get_chunk(int tid) { return num_get_chunk[tid]; }

   inline void profile_load_chunk(int tid) { num_load_chunk[tid]++; }
   inline void profile_load_chunk(int tid, uint32_t count) { num_load_chunk[tid] += count; }
   inline void profile_get_chunk(int tid) { num_get_chunk[tid]++; }

   inline void print_page_miss_ratio() {
      size_t sum_num_load_chunk = 0, sum_num_get_chunk = 0;
      for (int i = 0; i < nthreads; ++i) sum_num_load_chunk += num_load_chunk[i];
      for (int i = 0; i < nthreads; ++i) sum_num_get_chunk += num_get_chunk[i];
      
      printf("num_load_chunk = %lu, num_get_chunk = %lu, ratio = %.2f\n", sum_num_load_chunk, sum_num_get_chunk, (double)sum_num_load_chunk / (double)sum_num_get_chunk);
   }

private:
   size_t *num_load_chunk, *num_get_chunk;
   int nthreads = 0;
};

// only the runtime memory usage is recorded
struct memory_profiler_t {
   #define MAX_THREAD_NUM 96
   // item, memory usage
   std::unordered_map<std::string, size_t> memory_usage;
   // memory usage of edge in chunk (multi-thread)
   size_t edge_memory_usage[MAX_THREAD_NUM] = {0};

   void print_memory_usage() {
      size_t total_memory_usage = 0;
      for (auto it = memory_usage.begin(); it != memory_usage.end(); ++it) {
         total_memory_usage += it->second;
      }
      for (int i = 0; i < MAX_THREAD_NUM; ++i) {
         total_memory_usage += edge_memory_usage[i];
      }
      printf("total_memory_usage = %.2lfGB\n", total_memory_usage / 1024.0 / 1024.0 / 1024.0);
   }

   void print_memory_usage_detail() {
      for (auto it = memory_usage.begin(); it != memory_usage.end(); ++it) {
         printf("\t%s = %.2lfGB\n", it->first.c_str(), it->second / 1024.0 / 1024.0 / 1024.0);
      }
      size_t total_edge_memory_usage = 0;
      for (int i = 0; i < MAX_THREAD_NUM; ++i) {
         total_edge_memory_usage += edge_memory_usage[i];
         // printf("edge_memory_usage[%d] = %.2lfGB\n", i, edge_memory_usage[i] / 1024.0 / 1024.0 / 1024.0);
      }
      printf("\tedge_memory_usage = %.2lfGB\n", total_edge_memory_usage / 1024.0 / 1024.0 / 1024.0);
   }
};

struct edge_profiler_t {
   #define MAX_THREAD_NUM 96
   
   // memory usage of edge in chunk (multi-thread)
   size_t edge_access[MAX_THREAD_NUM] = {0};
   size_t out_edge_access[MAX_THREAD_NUM] = {0};
   size_t in_edge_access[MAX_THREAD_NUM] = {0};

   void print_edge_access() {
      size_t total_edge_access = 0;
      for (int i = 0; i < MAX_THREAD_NUM; ++i) {
         total_edge_access += edge_access[i];
      }
      printf("total_edge_access = %lu\n", total_edge_access);
   }

   void print_out_edge_access() {
      size_t total_out_edge_access = 0;
      for (int i = 0; i < MAX_THREAD_NUM; ++i) {
         total_out_edge_access += out_edge_access[i];
      }
      printf("total_out_edge_access = %lu\n", total_out_edge_access);
   }

   void print_in_edge_access() {
      size_t total_in_edge_access = 0;
      for (int i = 0; i < MAX_THREAD_NUM; ++i) {
         total_in_edge_access += in_edge_access[i];
      }
      printf("total_in_edge_access = %lu\n", total_in_edge_access);
   }
};

struct stat_profiler_t {
   uint64_t _total_accessed_edges = 0;
   uint64_t _total_read_KB[3] = {0};

   void print_total_accessed_edges() {
      printf("total_accessed_edges = %lu\n", _total_accessed_edges);
      printf("total_accessed_edges = %.2lfGB\n", _total_accessed_edges * sizeof(uintE) / 1024.0 / 1024.0 / 1024.0);
   }

   void record_read_KB(int index) {
      std::string iocommand = "iostat | grep nvme0n1 | awk '{print $6}'";
      FILE *fp = popen(iocommand.c_str(), "r");
      char buf[128];
      char* res = fgets(buf, sizeof(buf), fp);
      pclose(fp);
      // convert buf to uint64_t
      uint64_t read_KB = 0;
      for (int i = 0; buf[i] != '\0'; ++i) {
         if (buf[i] >= '0' && buf[i] <= '9') {
            read_KB = read_KB * 10 + (buf[i] - '0');
         }
      }
      _total_read_KB[index] = read_KB;
   }

   void print_read_KB() {
      // print out GB with 2 decimal points
      printf("Total bytes read by iostat during the whole procedure: %lu KB (%.2lf GB)\n", _total_read_KB[2]-_total_read_KB[0], (_total_read_KB[2]-_total_read_KB[0]) / 1024.0 / 1024.0);
      printf("Total bytes read by iostat during the compute procedure: %lu KB (%.2lf GB)\n", _total_read_KB[2]-_total_read_KB[1], (_total_read_KB[2]-_total_read_KB[1]) / 1024.0 / 1024.0);
      printf("Average IO Utilization during the compute procedure: %.2lf%%\n", (_total_accessed_edges * sizeof(uintE)) / ((_total_read_KB[2]-_total_read_KB[1]) * 1024.0) * 100);
   }
};

struct iteration_profiler_t {
   uint64_t _total_iteration = 0;
   uint64_t _curr_accessed_edges = 0;
   size_t iostat_read_KB = 0;

   void print_total_iteration() {
      printf("total_iteration = %lu\n", _total_iteration);
   }

   void print_curr_accessed_edges() {
      printf("curr_accessed_edges = %lu\n", _curr_accessed_edges);
   }

   void print_iostat_read_KB() {
      printf("iostat_read_KB = %lu\n", iostat_read_KB);
   }

   void init_iostat() {
      std::string iocommand = "iostat | grep nvme0n1 | awk '{print $6}'";
      FILE *fp = popen(iocommand.c_str(), "r");
      char buf[128];
      char* res = fgets(buf, sizeof(buf), fp);
      pclose(fp);
      // convert buf to uint64_t
      uint64_t read_KB = 0;
      for (int i = 0; buf[i] != '\0'; ++i) {
         if (buf[i] >= '0' && buf[i] <= '9') {
            read_KB = read_KB * 10 + (buf[i] - '0');
         }
      }
      iostat_read_KB = read_KB;
   }

   void record_iostat() {
      std::string iocommand = "iostat | grep nvme0n1 | awk '{print $6}'";
      FILE *fp = popen(iocommand.c_str(), "r");
      char buf[128];
      char* res = fgets(buf, sizeof(buf), fp);
      pclose(fp);
      // convert buf to uint64_t
      uint64_t read_KB = 0;
      for (int i = 0; buf[i] != '\0'; ++i) {
         if (buf[i] >= '0' && buf[i] <= '9') {
            read_KB = read_KB * 10 + (buf[i] - '0');
         }
      }
      uint64_t increment = read_KB - iostat_read_KB;
      std::cout << "edge = " << _curr_accessed_edges << ", iostat_read_KB = " << increment;
      // print out ratio as percentage
      if (increment) std::cout << ", ratio = " << (double)(_curr_accessed_edges * sizeof(uintE)) / ((double)increment * 1024.0) * 100 << "%" << std::endl;
      else std::cout << ", ratio = NaN" << std::endl;
      iostat_read_KB = read_KB;
   }
};

struct vertexcut_profiler_t {
   
   #define MAX_THREAD_NUM 96
   
   uint64_t vertex_cut[MAX_THREAD_NUM] = {0};
   uint64_t _total_vertexcut;
   std::vector<uint64_t>  _curr_vertexcut;

   uint64_t vertex_accessed[MAX_THREAD_NUM] = {0};
   uint64_t _total_vertex_accessed;
   std::vector<uint64_t>  _curr_vertex_accessed;

   uintE* out_graph_base_addr = NULL;
   uintE* in_graph_base_addr = NULL;

   void check_vertexcut(uintE* neighbors, uintE degree, bool inGraph, int thread_id) {
      uintE* base_addr = inGraph ? in_graph_base_addr : out_graph_base_addr;
      uint64_t offset = (uint64_t)neighbors - (uint64_t)base_addr;
      uint64_t start_page = offset / PAGE_SIZE;
      uint64_t end_page = (offset + degree * sizeof(uintE)) / PAGE_SIZE;
      if (start_page != end_page) {
         vertex_cut[thread_id]++;
      }
   }

   void record_vertexcut() {
      uint64_t __curr_vertexcut = 0;
      for (int i = 0; i < MAX_THREAD_NUM; ++i) {
         __curr_vertexcut += vertex_cut[i];
         vertex_cut[i] = 0;
      }
      _curr_vertexcut.push_back(__curr_vertexcut);
   }

   void record_vertex_accessed() {
      uint64_t __curr_vertex_accessed = 0;
      for (int i = 0; i < MAX_THREAD_NUM; ++i) {
         __curr_vertex_accessed += vertex_accessed[i];
         vertex_accessed[i] = 0;
      }
      _curr_vertex_accessed.push_back(__curr_vertex_accessed);
   }

   void print_vertexcut_rate() {
      for (int i = 0; i < _curr_vertexcut.size(); ++i) {
         _total_vertexcut += _curr_vertexcut[i];
         _total_vertex_accessed += _curr_vertex_accessed[i];
         printf("iteration %d: vertexcut = %lu, vertex_accessed = %lu, ratio = %.2lf%%\n", i, _curr_vertexcut[i], _curr_vertex_accessed[i], (double)_curr_vertexcut[i] / (double)_curr_vertex_accessed[i] * 100);
      }
      printf("total_vertexcut = %lu, total_vertex_accessed = %lu, ratio = %.2lf%%\n", _total_vertexcut, _total_vertex_accessed, (double)_total_vertexcut / (double)_total_vertex_accessed * 100);
   }

};

#ifdef PROFILE_EN
profiler_t profiler = profiler_t(96);
#endif

#ifdef DEBUG_EN
memory_profiler_t memory_profiler;
edge_profiler_t edge_profiler;
stat_profiler_t stat_profiler;
#endif

#ifdef ITER_PROFILE_EN
iteration_profiler_t iteration_profiler;
#endif

#ifdef VERTEXCUT_PROFILE_EN
vertexcut_profiler_t vertexcut_profiler;
#endif