#include "search_space.h"

#include "global_operator.h"
#include "global_state.h"
#include "globals.h"
#include "../successor_generator.h"
#include "utils/planvis.h"

#include "symmetries/graph_creator.h"
#include "symmetries/permutation.h"

#include <cassert>
#include "search_node_info.h"

using namespace std;

SearchNode::SearchNode(StateID state_id_, SearchNodeInfo &info_,
                       OperatorCost cost_type_)
    : state_id(state_id_), info(info_), cost_type(cost_type_) {
    assert(state_id != StateID::no_state);
}

GlobalState SearchNode::get_state() const {
    return g_state_registry->lookup_state(state_id);
}

bool SearchNode::is_open() const {
    return info.status == SearchNodeInfo::OPEN;
}

bool SearchNode::is_closed() const {
    return info.status == SearchNodeInfo::CLOSED;
}

bool SearchNode::is_dead_end() const {
    return info.status == SearchNodeInfo::DEAD_END;
}

bool SearchNode::is_new() const {
    return info.status == SearchNodeInfo::NEW;
}

ap_float SearchNode::get_g() const {
    assert(info.g >= 0);
    return info.g;
}

ap_float SearchNode::get_real_g() const {
    return info.real_g;
}

void SearchNode::open_initial() {
    assert(info.status == SearchNodeInfo::NEW);
    info.status = SearchNodeInfo::OPEN;
    info.g = 0;
    info.real_g = 0;
    info.parent_state_id = StateID::no_state;
    info.creating_operator = 0;
}

void SearchNode::open(const SearchNode &parent_node,
                      const GlobalOperator *parent_op) {
    assert(info.status == SearchNodeInfo::NEW);
    info.status = SearchNodeInfo::OPEN;
    info.g = parent_node.info.g + get_adjusted_action_cost(*parent_op, cost_type);
    info.real_g = parent_node.info.real_g + parent_op->get_cost();
    info.parent_state_id = parent_node.get_state_id();
    info.creating_operator = parent_op;
}

void SearchNode::reopen(const SearchNode &parent_node,
                        const GlobalOperator *parent_op) {
    assert(info.status == SearchNodeInfo::OPEN ||
           info.status == SearchNodeInfo::CLOSED);

    // The latter possibility is for inconsistent heuristics, which
    // may require reopening closed nodes.
    info.status = SearchNodeInfo::OPEN;
    info.g = parent_node.info.g + get_adjusted_action_cost(*parent_op, cost_type);
    info.real_g = parent_node.info.real_g + parent_op->get_cost();
    info.parent_state_id = parent_node.get_state_id();
    info.creating_operator = parent_op;
}

// like reopen, except doesn't change status
void SearchNode::update_parent(const SearchNode &parent_node,
                               const GlobalOperator *parent_op) {
    assert(info.status == SearchNodeInfo::OPEN ||
           info.status == SearchNodeInfo::CLOSED);
    // The latter possibility is for inconsistent heuristics, which
    // may require reopening closed nodes.
    info.g = parent_node.info.g + get_adjusted_action_cost(*parent_op, cost_type);
    info.real_g = parent_node.info.real_g + parent_op->get_cost();
    info.parent_state_id = parent_node.get_state_id();
    info.creating_operator = parent_op;
}

void SearchNode::close() {
    assert(info.status == SearchNodeInfo::OPEN);
    info.status = SearchNodeInfo::CLOSED;
}

void SearchNode::mark_as_dead_end() {
    info.status = SearchNodeInfo::DEAD_END;
}

void SearchNode::dump() const {
    cout << state_id << ": ";
    g_state_registry->lookup_state(state_id).dump_fdr();
    if (info.creating_operator) {
        cout << " created by " << info.creating_operator->get_name()
             << " from " << info.parent_state_id << endl;
    } else {
        cout << " no parent" << endl;
    }
}

SearchSpace::SearchSpace(OperatorCost cost_type_)
    : cost_type(cost_type_) {
}

SearchNode SearchSpace::get_node(const GlobalState &state) {
    return SearchNode(state.get_id(), search_node_infos[state], cost_type);
}

void SearchSpace::trace_path(const GlobalState &goal_state,
                             vector<const GlobalOperator *> &path) const {
    if (g_symmetry_graph != nullptr)
        return trace_path_symmetry(goal_state, path);

    GlobalState current_state = goal_state;
    if (PLAN_VIS_LOG == latex_only)
    	g_plan_logger->log_latex(current_state.get_numeric_state_vals_string());
    assert(path.empty());
    for (;;) {
        const SearchNodeInfo &info = search_node_infos[current_state];
        const GlobalOperator *op = info.creating_operator;
        if (op == 0) {
            assert(info.parent_state_id == StateID::no_state);
            break;
        }
        path.push_back(op);
        current_state = g_state_registry->lookup_state(info.parent_state_id);
        if (PLAN_VIS_LOG == latex_only)
        	g_plan_logger->log_latex(current_state.get_numeric_state_vals_string());
    }
    reverse(path.begin(), path.end());
}

void SearchSpace::trace_path_symmetry(const GlobalState &goal_state,
                                      vector<const GlobalOperator *> &path) const {
    assert(path.empty());
    std::vector<Permutation> perms;
    std::vector<GlobalState> state_trace;
    GlobalState current_state = goal_state;
    for (;;) {
        const SearchNodeInfo &info = search_node_infos[current_state];
        const GlobalOperator *op = info.creating_operator;

        state_trace.push_back(current_state);

        Permutation p;
        if (op == 0) {
            assert(info.parent_state_id == StateID::no_state);
            GlobalState new_state = g_initial_state();
            if (new_state.get_id() != current_state.get_id() && !new_state.same_values(current_state)) {
                p = g_symmetry_graph->create_permutation_from_state_to_state(current_state, new_state);
            }
            perms.push_back(p);
            break;
        } else {
            GlobalState parent_state = g_state_registry->lookup_state(info.parent_state_id);
            GlobalState new_state = g_state_registry->get_successor_state(parent_state, *op);
            if (new_state.get_id() != current_state.get_id() && !new_state.same_values(current_state)) {
                p = g_symmetry_graph->create_permutation_from_state_to_state(current_state, new_state);
            }
            perms.push_back(p);
            current_state = parent_state;
        }
    }
    reverse(perms.begin(), perms.end());
    reverse(state_trace.begin(), state_trace.end());

    if (perms.size() == 0) return;

    Permutation tmp_p = perms[0];
    std::vector<GlobalState> true_state_trace;
    true_state_trace.push_back(g_initial_state());
    for (size_t i = 1; i < perms.size(); ++i) {
        Permutation p = perms[i];
        tmp_p = Permutation(p, tmp_p);

        GlobalState state = state_trace[i];
        std::vector<container_int> values(g_variable_domain.size());
        for (size_t j = 0; j < g_variable_domain.size(); ++j) {
            values[j] = state[j];
        }
        std::vector<ap_float> numeric_values = state.get_numeric_vars();
        tmp_p.permutation_on_state(values, numeric_values);
        GlobalState true_state = g_state_registry->register_state(values, numeric_values);
        true_state_trace.push_back(true_state);
    }

    for (size_t i = 1; i < true_state_trace.size(); ++i) {
        GlobalState state = true_state_trace[i];
        GlobalState parent_state = true_state_trace[i - 1];
        vector<const GlobalOperator *> applicable_ops;
        g_successor_generator->generate_applicable_ops(parent_state, applicable_ops);

        const GlobalOperator *min_op = nullptr;
        ap_float min_cost = std::numeric_limits<ap_float>::max();
        for (auto op : applicable_ops) {
            GlobalState new_state = g_state_registry->get_successor_state(parent_state, *op);
            if (new_state.get_id() == state.get_id() || new_state.same_values(state)) {
                if (min_op == nullptr || op->get_cost() < min_cost) {
                    min_op = op;
                }
            } 
        }
        if (min_op == nullptr) {
            cout << "No operator is found!!!" << endl;
            cout << "Cannot reach the state " << state.dump_plan_vis_log() << endl;
            cout << "From the state" << parent_state.dump_plan_vis_log() << endl;
            exit(1);
        }

        path.push_back(min_op);
    }
}

void SearchSpace::dump() const {
    for (PerStateInformation<SearchNodeInfo>::const_iterator it =
             search_node_infos.begin(g_state_registry);
         it != search_node_infos.end(g_state_registry); ++it) {
        StateID id = *it;
        GlobalState s = g_state_registry->lookup_state(id);
        const SearchNodeInfo &node_info = search_node_infos[s];
        cout << id << ": ";
        s.dump_fdr();
        if (node_info.creating_operator && node_info.parent_state_id != StateID::no_state) {
            cout << " created by " << node_info.creating_operator->get_name()
                 << " from " << node_info.parent_state_id << endl;
        } else {
            cout << "has no parent" << endl;
        }
    }
}

void SearchSpace::print_statistics() const {
    cout << "Number of registered states: "
         << g_state_registry->size() << endl;
}
