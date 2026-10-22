#include "Components.hpp"


int OrderComparator(uint64_t e1, const sw::components::CreationOrder* c1, uint64_t e2, const sw::components::CreationOrder* c2) {
    if (c1->index < c2->index) return -1;
    if (c1->index > c2->index) return 1;
    return 0;
}
