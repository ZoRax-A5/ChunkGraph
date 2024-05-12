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

// Parallel implementation of K-Core decomposition of a symmetric
// graph.
#include "ligra.h"

struct Update_Deg {
  intE* Degrees;
  Update_Deg(intE* _Degrees) : Degrees(_Degrees) {}
  inline bool update (uintE s, uintE d) { 
    Degrees[d]--;
    return 1;
  }
  inline bool updateAtomic (uintE s, uintE d){
    writeAdd(&Degrees[d],-1);
    return 1;
  }
  inline bool cond (uintE d) { return Degrees[d] > 0; }
};

template<class vertex>
struct Deg_LessThan_K {
  vertex* V;
  uintE* coreNumbers;
  intE* Degrees;
  uintE k;
  Deg_LessThan_K(vertex* _V, intE* _Degrees, uintE* _coreNumbers, uintE _k) : 
    V(_V), k(_k), Degrees(_Degrees), coreNumbers(_coreNumbers) {}
  inline bool operator () (uintE i) {
    if(Degrees[i] < k) { coreNumbers[i] = k-1; Degrees[i] = 0; return true; }
    else return false;
  }
};

template<class vertex>
struct Deg_AtLeast_K {
  vertex* V;
  intE *Degrees;
  uintE k;
  Deg_AtLeast_K(vertex* _V, intE* _Degrees, uintE _k) : 
    V(_V), k(_k), Degrees(_Degrees) {}
  inline bool operator () (uintE i) {
    return Degrees[i] >= k;
  }
};

//assumes symmetric graph
// 1) iterate over all remaining active vertices
// 2) for each active vertex, remove if induced degree < k. Any vertex removed has
//    core-number (k-1) (part of (k-1)-core, but not k-core)
// 3) stop once no vertices are removed. Vertices remaining are in the k-core.
template <class vertex>
void Compute(graph<vertex>& GA, commandLine P) {
  const long n = GA.n;
  bool* active = newA(bool,n);
  {parallel_for(long i=0;i<n;i++) active[i] = 1;}
  vertexSubset Frontier(n, n, active);
  uintE* coreNumbers = newA(uintE,n);
  intE* Degrees = newA(intE,n);
  {parallel_for(long i=0;i<n;i++) {
      coreNumbers[i] = 0;
      Degrees[i] = GA.V[i].getOutDegree();
    }}
  long largestCore = -1;

#ifdef DEBUG_EN
  std::string item = "Algo MetaData";
  memory_profiler.memory_usage[item] = 0;
  size_t size = sizeof(bool) * n; // active
  memory_profiler.memory_usage[item] += size;
  size = sizeof(uintE) * n; // coreNumbers, Degrees
  memory_profiler.memory_usage[item] += size;
  memory_profiler.memory_usage[item] += size;

  size_t max_size = Frontier.getMemorySize();
#endif

#ifdef ITER_PROFILE_EN
  iteration_profiler.init_iostat();
#endif

  // long max_k = n > 10 ? 10 : n;
  long max_k = P.getOptionLongValue("-maxk", n > 10 ? 10 : n);
  for (long k = 1; k <= max_k; k++) {
    while (true) {
      vertexSubset toRemove 
	= vertexFilter(Frontier,Deg_LessThan_K<vertex>(GA.V,Degrees,coreNumbers,k));
      vertexSubset remaining = vertexFilter(Frontier,Deg_AtLeast_K<vertex>(GA.V,Degrees,k));
      Frontier.del();
      Frontier = remaining;
#ifdef DEBUG_EN
      size = sizeof(vertexSubset) + remaining.getMemorySize();
      if (size > max_size) max_size = size;
#endif
      if (0 == toRemove.numNonzeros()) { // fixed point. found k-core
	toRemove.del();
        break;
      }
      else {
	edgeMap(GA,toRemove,Update_Deg(Degrees), -1, no_output);
	toRemove.del();
      }
    }
    std::cout << "k = " << k << ", number of activated vertices = " << Frontier.numNonzeros();
#ifdef DEBUG_EN
    size_t vm, rss;
    pid_t pid = getpid();
    process_mem_usage(pid, vm, rss);
    std::cout << "; memory usage: VM = " << B2GB(vm) << ", RSS = " << B2GB(rss);
#else
    std::cout << std::endl;
#endif

#ifdef ITER_PROFILE_EN
    iteration_profiler.record_iostat();
#endif
#ifdef VERTEXCUT_PROFILE_EN
    vertexcut_profiler.record_vertexcut();
    vertexcut_profiler.record_vertex_accessed();
#endif

    if(Frontier.numNonzeros() == 0) { largestCore = k-1; break; }
  }
  cout << "largestCore was " << largestCore << endl;

#ifdef DEBUG_EN
  std::cout << "max_size = " << max_size << std::endl;
  memory_profiler.memory_usage[item] += max_size;
#endif

  Frontier.del(); free(coreNumbers); free(Degrees);

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
