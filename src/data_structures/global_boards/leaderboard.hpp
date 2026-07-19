#pragma once
#include "storage/file_helper.hpp"
#include "data_structures/global_boards/entry.hpp"

#include <optional>
#include <chrono>
#include <fstream>
#include <unordered_map>
#include <iostream>
#include <filesystem>
#include <tuple>


#include <nlohmann/json.hpp>

using json = nlohmann::json;

constexpr const std::size_t DEFAULT_MAX_LEADERBOARD_SIZE = 300;

constexpr const char* LEADERBOARD_NAME_KEY = "name";
constexpr const char* LEADERBOARD_SCORE_KEY = "score";
constexpr const char* LEADERBOARD_TIMESTAMP_KEY = "timestamp";
constexpr const std::string LEADERBOARDS_SUBDIRECTORY_NAME = "leaderboards";


/**
 * @brief Stores and manages a leaderboard.
 *
 * Maintains player scores in ranked order and limits the number of stored
 * entries. Existing players are updated only when the new score improves
 * their ranking.
 *
 * @tparam T Score type.
 */
template<typename T>
class leaderboard {
    private:
        using entry_type = leaderboard_entry<T>;
        using entry_ptr = entry_type*;
        using ptr_comparator = leaderboard_entry_ptr_comparator<T>;

        const std::filesystem::path file_path;
        

        

        std::unordered_map<std::string,std::unique_ptr<entry_type>> entries;


        // Only stores pointers, does not own entries
        std::multiset<entry_ptr, ptr_comparator> ranking;
        
        std::uint64_t next_id = 0;
        const std::size_t max_size;


    public:
        /**
         * @brief Creates a leaderboard and loads saved data.
         *
         * @param file_path JSON storage file.
         * @param max_size Maximum number of entries.
         * @param highest_first Sort direction.
         */
        explicit leaderboard(
            const std::filesystem::path& file_path,       
            std::size_t max_size = DEFAULT_MAX_LEADERBOARD_SIZE,
            bool highest_first = true
        ) : file_path(file_path), max_size(max_size), ranking(leaderboard_entry_ptr_comparator<T>{highest_first}) {
            load();
        }


         /**
         * @brief Adds or updates a player score.
         *
         * @return Tuple of (Player ranking position, or std::nullopt if rejected) and (Removed past index, or std::nullopt if none)
         * The returned past index is that before the change
         */
        std::tuple<std::optional<std::size_t>, std::optional<std::size_t>> submit_score(const std::string& player, T score) {
            auto now = static_cast<std::int64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

            auto get_index = [this](auto it) {
                    return static_cast<std::size_t>(
                        std::distance(
                            ranking.begin(),
                            it
                        )
                    );
                };

            auto existing = entries.find(player);


            if (existing != entries.end()) {
                auto& entry = existing->second;

                entry_type candidate;
                candidate.score = score;
                
                if (!ranking.key_comp()(&candidate, entry.get())) {
                    return {std::nullopt, std::nullopt};
                }
                
                auto previous_position = entry->ranking_position;
                size_t previous_index = get_index(previous_position);
                ranking.erase(previous_position);

                entry->score = score;
                entry->timestamp = now;
                entry->id = next_id++;


                auto ranking_it = ranking.insert(entry.get());
                entry->ranking_position = ranking_it;

                return {get_index(ranking_it), previous_index};
            }

            auto entry = std::make_unique<entry_type>();

            entry->name = player;
            entry->score = score;
            entry->timestamp = now;
            entry->id = next_id++;

            if (ranking.size() >= max_size) {
                auto worst = std::prev(ranking.end());

                if (!ranking.key_comp()(entry.get(), *worst)) {
                    return {std::nullopt, std::nullopt};
                }
            }

            entry_ptr raw = entry.get();

            auto ranking_it = ranking.insert(raw);
            raw->ranking_position = ranking_it;


            entries.emplace(player, std::move(entry));

            auto placement = get_index(ranking_it);

            if (ranking.size() > max_size) {
                remove_last();
            }

            return {placement, std::nullopt};
        }


        /**
         * @brief Gets top ranked entries.
         */
        std::vector<entry_type> get_top_scores(std::size_t count) const {
            std::vector<entry_type> result;
            count = std::min(count, ranking.size());
            
            auto it = ranking.begin();
            for (std::size_t i = 0; i < count; i++, ++it){
                result.push_back(**it);
            }

            return result;
        }




        /**
         * @brief Gets lowest ranked entries.
         */
        std::vector<entry_type> get_bottom_scores(std::size_t count) const {
            std::vector<entry_type> result;

            count = std::min(count, ranking.size());

            auto it = ranking.rbegin();

            for (std::size_t i = 0; i < count; i++, ++it){
                result.push_back(**it);
            }

            return result;
        }



        /**
         * @brief Gets a ranking range from the top.
         */
        std::vector<entry_type> get_range_from_top(std::size_t start, std::size_t end) const {
            std::vector<entry_type> result;

            if (start >= ranking.size() || start > end) {
                return result;
            }
                
            end = std::min(end,ranking.size());
            
            auto it = ranking.begin();
            std::advance(it, start);

            for (std::size_t i = start; i < end; ++i, ++it) {
                result.push_back(**it);
            }

            return result;
        }

         /**
         * @brief Gets a ranking range from the bottom.
         */
        std::vector<entry_type> get_range_from_bottom(std::size_t start, std::size_t end) const {
            std::vector<entry_type> result;


            if (start >= ranking.size() || start > end) {
                return result;
            }

            end = std::min(end, ranking.size());

            auto it = ranking.rbegin();
            std::advance(it, start);

            for (std::size_t i = start; i < end; ++i, ++it) {
                result.push_back(**it);
            }

            return result;
        }




        /**
         * @brief Saves leaderboard data to JSON.
         */
        bool save() const {
            try {
                std::ofstream out(file_path);
        
                if (!out) {
                    return false;
                }
                
                json j = json::array();

                for (auto ptr : ranking){
                    j.push_back({
                        {LEADERBOARD_NAME_KEY, ptr->name},
                        {LEADERBOARD_SCORE_KEY,ptr->score},
                        {LEADERBOARD_TIMESTAMP_KEY,ptr->timestamp},
                    });
                }       
                    
                out << j.dump(4);

            }
            catch (...) {
                return false;
            }
            return true;
        }


        size_t size() {
            return ranking.size();
        }
    private:

        /**
         * @brief Saves leaderboard data to JSON.
         */
        bool load() {
            try {
                std::ifstream in(file_path);
                if (!in) {
                    return false;
                }

                json j;

                in >> j;

                for (const auto& item : j) {
                    auto entry = std::make_unique<entry_type>();
                    entry->name = item.at(LEADERBOARD_NAME_KEY).get<std::string>();
                    entry->score = item.at(LEADERBOARD_SCORE_KEY).get<T>();
                    entry->timestamp = item.at(LEADERBOARD_TIMESTAMP_KEY).get<std::int64_t>();
                    entry->id = next_id++;
                    entry_ptr raw = entry.get();
                    auto ranking_it = ranking.insert(raw);
                    raw->ranking_position = ranking_it;
                    entries.emplace(raw->name,std::move(entry));
                }
            }
            catch (...) {
                return false;
            }


            return true;
        }





        void remove_last() {
            if (ranking.empty()) {
                return;
            }
                
            auto it = std::prev(ranking.end());

            entry_ptr entry = *it;

            ranking.erase(it);

            entries.erase(entry->name);
        }
    };