#ifndef ORBIT_EAGER_SEARCH_ENGINES_EAGER_ORBIT_EAGER_SEARCH_H
#define ORBIT_EAGER_SEARCH_ENGINES_EAGER_ORBIT_EAGER_SEARCH_H

#include "../search_engine.h"

#include "../open_lists/open_list.h"

#include <memory>
#include <vector>

class GlobalOperator;
class Heuristic;
class PruningMethod;
class ScalarEvaluator;

namespace options {
class Options;
}

namespace orbit_eager_search {
class OrbitEagerSearch : public SearchEngine {
    const bool reopen_closed_nodes;
    const bool use_multi_path_dependence;

    std::unique_ptr<StateOpenList> open_list;
    ScalarEvaluator *f_evaluator;

    std::vector<Heuristic *> heuristics;
    std::vector<Heuristic *> preferred_operator_heuristics;

    std::shared_ptr<PruningMethod> pruning_method;

    std::pair<SearchNode, bool> fetch_next_node();
    void start_f_value_statistics(EvaluationContext &eval_context);
    void update_f_value_statistics(const SearchNode &node);
    void reward_progress();
    void print_checkpoint_line(int g) const;


protected:
    virtual void initialize() override;
    virtual SearchStatus step() override;

public:
    explicit OrbitEagerSearch(const options::Options &opts);
    virtual ~OrbitEagerSearch() = default;

    virtual void print_statistics() const override;

    void dump_search_space() const;
};
}

#endif

