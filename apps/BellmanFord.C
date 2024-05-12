// This code is part of the project "Ligra: A Lightweight Graph Processing
// Framework for Shared Memory", presented at Principles and Practice of 
// Parallel Programming, 2013.
// Copyright (c) 2013 Julian Shun and Guy Blelloch
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights (to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// #define WEIGHTED 0

#define MAX_WEIGHT 15
#define WEIGHTED_BIAS 0
#define RANDOM_WEIGHT(v_id,nebrs) ((((v_id)+(nebrs)+(WEIGHTED_BIAS)) & (MAX_WEIGHT)) + 1)

#include "ligra.h"

struct BF_F {
  intE* ShortestPathLen;
  int* Visited;
  BF_F(intE* _ShortestPathLen, int* _Visited) : 
    ShortestPathLen(_ShortestPathLen), Visited(_Visited) {}
  // inline bool update (uintE s, uintE d, intE edgeLen) { //Update ShortestPathLen if found a shorter path
  inline bool update (uintE s, uintE d) { //Update ShortestPathLen if found a shorter path
    uintE edgeLen = RANDOM_WEIGHT(s,d);
    intE newDist = ShortestPathLen[s] + edgeLen;
    if(ShortestPathLen[d] > newDist) {
      ShortestPathLen[d] = newDist;
      if(Visited[d] == 0) { Visited[d] = 1 ; return 1;}
    }
    return 0;
  }
  // inline bool updateAtomic (uintE s, uintE d, intE edgeLen){ //atomic Update
  inline bool updateAtomic (uintE s, uintE d){ //atomic Update
    uintE edgeLen = RANDOM_WEIGHT(s,d);
    intE newDist = ShortestPathLen[s] + edgeLen;
    return (writeMin(&ShortestPathLen[d],newDist) &&
	    CAS(&Visited[d],0,1));
  }
  inline bool cond (uintE d) { return cond_true(d); }
};

//reset visited vertices
struct BF_Vertex_F {
  int* Visited;
  BF_Vertex_F(int* _Visited) : Visited(_Visited) {}
  inline bool operator() (uintE i){
    Visited[i] = 0;
    return 1;
  }
};

template <class vertex>
void Compute(graph<vertex>& GA, commandLine P) {
  long start = P.getOptionLongValue("-r",0);
  long threshold = P.getOptionLongValue("-threshold", 20);
  long n = GA.n;
  //initialize ShortestPathLen to "infinity"
  intE* ShortestPathLen = newA(intE,n);
  {parallel_for(long i=0;i<n;i++) ShortestPathLen[i] = INT_MAX/2;}
  ShortestPathLen[start] = 0;

  int* Visited = newA(int,n);
  {parallel_for(long i=0;i<n;i++) Visited[i] = 0;}

  vertexSubset Frontier(n,start); //initial frontier

#ifdef DEBUG_EN
  std::string item = "Algo MetaData";
  memory_profiler.memory_usage[item] = 0;
  size_t size = sizeof(intE) * n;  // ShortestPathLen
  memory_profiler.memory_usage[item] += size;
  size = sizeof(int) * n;  // Visited
  memory_profiler.memory_usage[item] += size;

  size_t max_size = Frontier.getMemorySize();
#endif

#ifdef ITER_PROFILE_EN
  iteration_profiler.init_iostat();
#endif

  long round = 0;
  while(!Frontier.isEmpty()){
    std::cout << "round = " << round << ", number of activated vertices = " << Frontier.numNonzeros();
#ifdef DEBUG_EN
    size_t vm, rss;
    pid_t pid = getpid();
    process_mem_usage(pid, vm, rss);
    std::cout << "; memory usage: VM = " << B2GB(vm) << ", RSS = " << B2GB(rss);
    size = Frontier.getMemorySize();
    if (size > max_size) max_size = size;
#else
    std::cout << std::endl;
#endif
    if(round == n) {
      //negative weight cycle
      {parallel_for(long i=0;i<n;i++) ShortestPathLen[i] = -(INT_E_MAX/2);}
      break;
    }
    vertexSubset output = edgeMap(GA, Frontier, BF_F(ShortestPathLen,Visited), GA.m/threshold, dense_forward);
    vertexMap(output,BF_Vertex_F(Visited));
    Frontier.del();
    Frontier = output;
    round++;
#ifdef ITER_PROFILE_EN
    iteration_profiler.record_iostat();
#endif
  }
  Frontier.del(); free(Visited);
  free(ShortestPathLen);

#ifdef DEBUG_EN
  std::cout << "Frontier maximum memory usage = " << B2GB(max_size) << "GB" << std::endl;
  memory_profiler.memory_usage[item] += max_size;
#endif

#ifdef DEBUG_EN
  memory_profiler.print_memory_usage();
  memory_profiler.print_memory_usage_detail();

  edge_profiler.print_edge_access();
  edge_profiler.print_out_edge_access();
  edge_profiler.print_in_edge_access();
#endif
}
