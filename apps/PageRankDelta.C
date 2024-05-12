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
#include "ligra.h"
#include "math.h"

// #define PR_DEBUG_EN

// #define PR_VALUE_TYPE double
#define PR_VALUE_TYPE float

template <class vertex>
struct PR_F {
  vertex* V;
  PR_VALUE_TYPE* Delta, *nghSum;
  PR_F(vertex* _V, PR_VALUE_TYPE* _Delta, PR_VALUE_TYPE* _nghSum) : 
    V(_V), Delta(_Delta), nghSum(_nghSum) {}
  inline bool update(uintE s, uintE d){
    PR_VALUE_TYPE oldVal = nghSum[d];
    nghSum[d] += Delta[s]/V[s].getOutDegree();
    return oldVal == 0;
  }
  inline bool updateAtomic (uintE s, uintE d) {
    volatile PR_VALUE_TYPE oldV, newV; 
    do { //basically a fetch-and-add
      oldV = nghSum[d]; newV = oldV + Delta[s]/V[s].getOutDegree();
    } while(!CAS(&nghSum[d],oldV,newV));
    return oldV == 0.0;
  }
  inline bool cond (uintE d) { return cond_true(d); }};

struct PR_Vertex_F_FirstRound {
  PR_VALUE_TYPE damping, addedConstant, one_over_n, epsilon2;
  PR_VALUE_TYPE* p, *Delta, *nghSum;
  PR_Vertex_F_FirstRound(PR_VALUE_TYPE* _p, PR_VALUE_TYPE* _Delta, PR_VALUE_TYPE* _nghSum, PR_VALUE_TYPE _damping, PR_VALUE_TYPE _one_over_n,PR_VALUE_TYPE _epsilon2) :
    p(_p),
    damping(_damping), Delta(_Delta), nghSum(_nghSum), one_over_n(_one_over_n),
    addedConstant((1-_damping)*_one_over_n),
    epsilon2(_epsilon2) {}
  inline bool operator () (uintE i) {
    Delta[i] = damping*nghSum[i]+addedConstant;
    p[i] += Delta[i];
    Delta[i]-=one_over_n; //subtract off delta from initialization
    return (fabs(Delta[i]) > epsilon2 * p[i]);
  }
};

struct PR_Vertex_F {
  PR_VALUE_TYPE damping, epsilon2;
  PR_VALUE_TYPE* p, *Delta, *nghSum;
  PR_Vertex_F(PR_VALUE_TYPE* _p, PR_VALUE_TYPE* _Delta, PR_VALUE_TYPE* _nghSum, PR_VALUE_TYPE _damping, PR_VALUE_TYPE _epsilon2) :
    p(_p),
    damping(_damping), Delta(_Delta), nghSum(_nghSum), 
    epsilon2(_epsilon2) {}
  inline bool operator () (uintE i) {
    Delta[i] = nghSum[i]*damping;
    if (fabs(Delta[i]) > epsilon2*p[i]) { p[i]+=Delta[i]; return 1;}
    else return 0;
  }
};

struct PR_Vertex_Reset {
  PR_VALUE_TYPE* nghSum;
  PR_Vertex_Reset(PR_VALUE_TYPE* _nghSum) :
    nghSum(_nghSum) {}
  inline bool operator () (uintE i) {
    nghSum[i] = 0.0;
    return 1;
  }
};

template <class vertex>
void Compute(graph<vertex>& GA, commandLine P) {
  long maxIters = P.getOptionLongValue("-maxiters",10);
  const long n = GA.n;
  const PR_VALUE_TYPE damping = 0.85;
  const PR_VALUE_TYPE epsilon = 0.0000001;
  const PR_VALUE_TYPE epsilon2 = 0.01;

  PR_VALUE_TYPE one_over_n = 1/(PR_VALUE_TYPE)n;
  PR_VALUE_TYPE* p = newA(PR_VALUE_TYPE,n), *Delta = newA(PR_VALUE_TYPE,n), 
    *nghSum = newA(PR_VALUE_TYPE,n);
  bool* frontier = newA(bool,n);
  parallel_for(long i=0;i<n;i++) {
    p[i] = 0.0;//one_over_n;
    Delta[i] = one_over_n; //initial delta propagation from each vertex
    nghSum[i] = 0.0;
    frontier[i] = 1;
  }

  vertexSubset Frontier(n,n,frontier);
  bool* all = newA(bool,n);
  {parallel_for(long i=0;i<n;i++) all[i] = 1;}
  vertexSubset All(n,n,all); //all vertices

#ifdef DEBUG_EN
  std::cout << "maxIters = " << maxIters << std::endl;
  std::string item = "Algo MetaData";
  memory_profiler.memory_usage[item] = 0;
  size_t size = sizeof(PR_VALUE_TYPE) * n;  // p, Delta, nghSum
  memory_profiler.memory_usage[item] += size;
  memory_profiler.memory_usage[item] += size;
  memory_profiler.memory_usage[item] += size;
  size = sizeof(bool) * n;  // frontier, all
  memory_profiler.memory_usage[item] += size;
  memory_profiler.memory_usage[item] += size;

  size_t max_size = Frontier.getMemorySize();
#endif

#ifdef ITER_PROFILE_EN
  iteration_profiler.init_iostat();
#endif

  long round = 0;
  while(round++ < maxIters) {
#ifdef PR_DEBUG_EN
    time_t now = time(0);
    char* dt = ctime(&now);
    std::cout << "The local date and time is: " << dt;
#endif
    edgeMap(GA,Frontier,PR_F<vertex>(GA.V,Delta,nghSum),GA.m/20, no_output | dense_forward);
    vertexSubset active 
      = (round == 1) ? 
      vertexFilter(All,PR_Vertex_F_FirstRound(p,Delta,nghSum,damping,one_over_n,epsilon2)) :
      vertexFilter(All,PR_Vertex_F(p,Delta,nghSum,damping,epsilon2));
    //compute L1-norm (use nghSum as temp array)
    {parallel_for(long i=0;i<n;i++) {
      nghSum[i] = fabs(Delta[i]); }}
    PR_VALUE_TYPE L1_norm = sequence::plusReduce(nghSum,n);
    if(L1_norm < epsilon) break;
    //reset
    vertexMap(All,PR_Vertex_Reset(nghSum));

    std::cout << "iteration = " << round << ", L1_norm_prev = " << L1_norm << std::endl;
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

    Frontier.del();
    Frontier = active;

#ifdef ITER_PROFILE_EN
    iteration_profiler.record_iostat();
#endif
#ifdef VERTEXCUT_PROFILE_EN
    vertexcut_profiler.record_vertexcut();
    vertexcut_profiler.record_vertex_accessed();
#endif
  }

#ifdef DEBUG_EN
  std::cout << "Frontier maximum memory usage = " << B2GB(max_size) << "GB" << std::endl;
  memory_profiler.memory_usage[item] += max_size;
#endif
  Frontier.del(); free(p); free(Delta); free(nghSum); All.del();
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
