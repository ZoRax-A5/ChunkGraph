#ifndef BLAZE_VERTEXMAP_H
#define BLAZE_VERTEXMAP_H

#include "galois/Galois.h"

namespace blaze {

template <typename F>
void vertexMap(Worklist<VID>* frontier, F&& f) {
    if (frontier->is_dense()) {
        Bitmap *b = frontier->get_dense();
        uint64_t num_words = b->get_num_words();
        uint64_t *words = (uint64_t *)b->ptr();
        uint64_t limit = b->get_size();
        galois::do_all(galois::iterate(Bitmap::iterator((uint64_t)0), Bitmap::iterator(num_words)),
                        [&](uint64_t pos) {
                            uint64_t word = words[pos];
                            if (word) {
                                uint64_t mask = 0x1;
                                for (uint64_t i = 0; i < 64; i++, mask <<= 1) {
                                    if (word & mask) {
                                        uint64_t node = Bitmap::get_pos(pos, i);
                                        if(node > limit) break;
                                        f(node);
                                    }
                                }
                            }
                        }, galois::no_stats(), galois::steal());
    } else {
        auto sparse = frontier->get_sparse();
        galois::do_all(galois::iterate(*sparse),
                        [&](const VID& node) {
                            f(node);
                        }, galois::no_stats(), galois::steal());
    }
}

template <typename G, typename F>
void vertexMap(G& graph, F&& f) {
    galois::do_all(galois::iterate(graph),
                    [&](const VID& node) {
                        f(node);
                    }, galois::no_stats());
}

} // namespace blaze

#endif // BLAZE_VERTEXMAP_H
