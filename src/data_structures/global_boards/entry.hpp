#pragma once

#include <stdint.h>
#include <string>
#include <set>
#include <nlohmann/json.hpp>

struct entry {
    std::string name;
    int64_t timestamp;
    uint64_t id;

    // Not serialized
    std::multiset<entry>::iterator position;
};

struct entry_comparator {
    bool operator()(const entry& a, const entry& b) const
    {
        if (a.timestamp != b.timestamp)
            return a.timestamp < b.timestamp;

        return a.id < b.id;
    }
};

template<typename T>
struct leaderboard_entry;

template<typename T>
struct leaderboard_entry_ptr_comparator;


template<typename T>
struct leaderboard_entry_comparator : entry_comparator {
    bool operator()(const leaderboard_entry<T>& a, const leaderboard_entry<T>& b) const {
        if (a.score != b.score)
            return a.score > b.score;

        return entry_comparator::operator()(a, b);
    }
};


template<typename T>
struct leaderboard_entry_ptr_comparator{
    bool operator()(const leaderboard_entry<T>* a, const leaderboard_entry<T>* b) const {
        return leaderboard_entry_comparator<T>{}(*a, *b);
    }
};


template<typename T>
struct leaderboard_entry : entry {
    T score;

    typename std::multiset<
        leaderboard_entry<T>*,
        leaderboard_entry_ptr_comparator<T>
    >::iterator ranking_position;
};


template<typename T>
struct score_stream_entry : entry {
    T score;
};


inline void to_json(nlohmann::json& j, const entry& e)
{
    j = {
        {"name", e.name},
        {"timestamp", e.timestamp},
        {"id", e.id}
    };
}

inline void from_json(const nlohmann::json& j, entry& e)
{
    j.at("name").get_to(e.name);
    j.at("timestamp").get_to(e.timestamp);
    j.at("id").get_to(e.id);
}


template<typename T>
inline void to_json(nlohmann::json& j, const leaderboard_entry<T>& e)
{
    j = {
        {"name", e.name},
        {"timestamp", e.timestamp},
        {"score", e.score}
    };
}

template<typename T>
inline void from_json(const nlohmann::json& j, leaderboard_entry<T>& e)
{
    j.at("name").get_to(e.name);
    j.at("timestamp").get_to(e.timestamp);
    j.at("score").get_to(e.score);
}


template<typename T>
inline void to_json(nlohmann::json& j, const score_stream_entry<T>& e)
{
    j = {
        {"name", e.name},
        {"timestamp", e.timestamp},
        {"score", e.score}
    };
}


template<typename T>
inline void from_json(const nlohmann::json& j, score_stream_entry<T>& e)
{
    j.at("name").get_to(e.name);
    j.at("timestamp").get_to(e.timestamp);
    j.at("score").get_to(e.score);
}