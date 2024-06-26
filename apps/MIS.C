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
//
// This is an implementation of the MIS algorithm from "Greedy
// Sequential Maximal Independent Set and Matching are Parallel on
// Average", Proceedings of the ACM Symposium on Parallelism in
// Algorithms and Architectures (SPAA), 2012 by Guy Blelloch, Jeremy
// Fineman and Julian Shun.  Note: this is not the most efficient
// implementation. For a more efficient implementation, see
// http://www.cs.cmu.edu/~pbbs/benchmarks.html.

#include "ligra.h"

//For flags array to store status of each vertex
enum {UNDECIDED,CONDITIONALLY_IN,OUT,IN};

//Uncomment the following line to enable checking for
//correctness. Currently the checker does not work with Ligra+.

//#define CHECK 1

#ifdef CHECK
template<class vertex>
bool checkMis(graph<vertex>& G, int* flags) {
  const intE n = G.n;
  bool correct = true;
  parallel_for (int i = 0; i < n; i++) {
    intE outDeg = G.V[i].getOutDegree();
    intE numConflict = 0;
    intE numInNgh = 0;
    for (int j = 0; j < outDeg; j++) {
      intE ngh = G.V[i].getOutNeighbor(j);
      if (flags[i] == IN && flags[ngh] == IN) {
        numConflict++;
      }
      else if (flags[ngh] == IN) {
        numInNgh++;
      }
    }
    if (numConflict > 0) {
      if(correct) CAS(&correct,true,false);
    } 
    if (flags[i] != IN && numInNgh == 0) {
      if(correct) CAS(&correct,true,false);
    }
  }
  return correct;
}
#endif

struct MIS_Update {
  int* flags;
  MIS_Update(int* _flags) : flags(_flags) {}
  inline bool update (uintE s, uintE d) {
    //if neighbor is in MIS, then we are out
    if(flags[d] == IN) {if(flags[s] != OUT) flags[s] = OUT;}
    //if neighbor has higher priority (lower ID) and is undecided, then so are we
    else if(d < s && flags[s] == CONDITIONALLY_IN && flags[d] < OUT)
      flags[s] = UNDECIDED;
    return 1;
  }
  inline bool updateAtomic (uintE s, uintE d) {
    if(flags[d] == IN) {if(flags[s] != OUT) flags[s] = OUT;}
    else if(d < s && flags[s] == CONDITIONALLY_IN && flags[d] < OUT) 
      flags[s] = UNDECIDED;
    return 1;
  }
  inline bool cond (uintE i) {return cond_true(i);}
};

struct MIS_Filter {
  int* flags;
  MIS_Filter(int* _flags) : flags(_flags) {}
  inline bool operator () (uintE i) {
    if(flags[i] == CONDITIONALLY_IN) { flags[i] = IN; return 0; } //vertex in MIS
    else if(flags[i] == OUT) return 0; //vertex not in MIS
    else { flags[i] = CONDITIONALLY_IN; return 1; } //vertex undecided, move to next round
  }
};

//Takes a symmetric graph as input; priority of a vertex is its ID.
template <class vertex>
void Compute(graph<vertex>& GA, commandLine P) {
  const intE n = GA.n;
  bool checkCorrectness = P.getOptionValue("-checkCorrectness");

  //flags array: UNDECIDED means "undecided", CONDITIONALLY_IN means
  //"conditionally in MIS", OUT means "not in MIS", IN means "in MIS"
  int* flags = newA(int,n);
  bool* frontier_data = newA(bool, n);
  {parallel_for(long i=0;i<n;i++) {
    flags[i] = CONDITIONALLY_IN;
    frontier_data[i] = 1;
  }}
  long round = 0;
  vertexSubset Frontier(n, frontier_data);

#ifdef DEBUG_EN
  std::string item = "Algo MetaData";
  memory_profiler.memory_usage[item] = 0;
  size_t size = sizeof(int) * n;  // flags
  memory_profiler.memory_usage[item] += size;
  size = sizeof(bool) * n;  // frontier_data
  memory_profiler.memory_usage[item] += size;

  size_t max_size = Frontier.getMemorySize();
#endif

#ifdef ITER_PROFILE_EN
  iteration_profiler.init_iostat();
#endif

  while (!Frontier.isEmpty()) {
#ifdef DEBUG_EN
    size_t vm, rss;
    pid_t pid = getpid();
    process_mem_usage(pid, vm, rss);
    std::cout << "round = " << round << ", number of activated vertices = " << Frontier.numNonzeros()
              << "; memory usage: VM = " << B2GB(vm) << ", RSS = " << B2GB(rss);
    size = Frontier.getMemorySize();
    if (size > max_size) max_size = size;
#endif
    edgeMap(GA, Frontier, MIS_Update(flags), -1, no_output);
    vertexSubset output = vertexFilter(Frontier, MIS_Filter(flags));
    Frontier.del();
    Frontier = output;
    round++;
#ifdef ITER_PROFILE_EN
    iteration_profiler.record_iostat();
#endif
#ifdef VERTEXCUT_PROFILE_EN
    vertexcut_profiler.record_vertexcut();
    vertexcut_profiler.record_vertex_accessed();
#endif
  }
#ifdef CHECK
  if (checkCorrectness) {
    if(checkMis(GA,flags)) cout << "correct\n";
    else cout << "incorrect\n";
  }
#endif
  free(flags);
  Frontier.del();

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

  stat_profiler.print_total_accessed_edges();
#endif

#ifdef VERTEXCUT_PROFILE_EN
  vertexcut_profiler.print_vertexcut_rate();
#endif
}
