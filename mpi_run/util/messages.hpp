#ifndef MESSAGES_HPP
#define MESSAGES_HPP

struct LeaderMsg {
    int target_node;
    int candidate_id;
};

struct DistMsg {
    int target_node;
    long long proposed_distance;
};

#endif