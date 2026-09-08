#include "utils/RandomGen.hpp"

using namespace geode::prelude;
using namespace rl;

geode::utils::random::Generator rl::makeRNG() {
    geode::utils::random::Generator g;
    g.seed(geode::utils::random::secureU64());  // seed once
    return g;
}

geode::utils::random::Generator* rl::globalRNG() {
    static auto g = rl::makeRNG();
    return &g;
}
