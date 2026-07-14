#pragma once

#include "data_structures/global_boards/entry.hpp"

#include <optional>
#include <chrono>
#include <fstream>
#include <unordered_map>
#include <iostream>
#include <filesystem>


#include <nlohmann/json.hpp>

using json = nlohmann::json;

constexpr const std::size_t DEFAULT_MAX_LEADERBOARD_SIZE = 300;

constexpr const char* LEADERBOARD_NAME_KEY = "name";
constexpr const char* LEADERBOARD_SCORE_KEY = "score";
constexpr const char* LEADERBOARD_TIMESTAMP_KEY = "timestamp";

/**
 * @brief Stores and manages a persistent leaderboard.
 * @tparam T Score type.
 */
template<typename T>
class leaderboard {
    private:
        const std::string filename;
        const std::size_t max_size;
        std::uint64_t next_id = 0;
        std::unordered_map<std::string, leaderboard_entry<T>> names_to_entry;
        std::multiset<leaderboard_entry<T>, leaderboard_entry_comparator<T>> ranking;


    public:
        /**
         * @brief Creates a leaderboard and loads saved data.
         * @param filename Leaderboard file name.
         * @param max_size Maximum number of entries.
         */
        explicit leaderboard(
            const std::filesystem::path& parent_directory,
            std::string filename, 
            std::size_t max_size = DEFAULT_MAX_LEADERBOARD_SIZE
        ) : max_size(max_size), filename(get_leaderboard_path(parent_directory, std::move(filename))) {
            load();
        }


        /**
         * @brief Adds or updates a player's score.
         * @param player Player name.
         * @param score Score to submit.
         * @return (optional) position where the new score was inserted
         */
        std::optional<std::size_t> submit_score(const std::string& player, T score) {
            auto now = static_cast<std::int64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

            auto get_index = [this](auto it) -> std::size_t {
                return static_cast<std::size_t>(std::distance(ranking.begin(), it));
            };

            auto it = names_to_entry.find(player);

            // Existing player
            if (it != names_to_entry.end()) {
                if (score <= it->second.score) {
                    return std::nullopt;
                }

                ranking.erase(it->second.ranking_position);

                it->second.score = score;
                it->second.timestamp = now;
                it->second.id = next_id++;

                auto ranking_it = ranking.insert(it->second);
                it->second.ranking_position = ranking_it;

                return get_index(ranking_it);
            }

            // New entry
            leaderboard_entry<T> entry;
            entry.name = player;
            entry.score = score;
            entry.timestamp = now;
            entry.id = next_id++;

            if (ranking.size() >= max_size) {
                auto worst = std::prev(ranking.end());

                if (!leaderboard_entry_comparator<T>{}(entry, *worst)) {
                    return std::nullopt;
                }
            }

            auto ranking_it = ranking.insert(entry);
            entry.ranking_position = ranking_it;

            names_to_entry[player] = entry;

            auto placement = get_rank(ranking_it);

            if (ranking.size() > max_size) {
                remove_last();
            }

            return placement;
        }


         /**
         * @brief Gets the highest ranked scores.
         * @param count Number of entries to return.
         * @return Top leaderboard entries.
         */
        std::vector<leaderboard_entry<T>> get_top_scores(std::size_t count) const {
            std::vector<leaderboard_entry<T>> result;

            count = std::min(count, ranking.size());

            auto it = ranking.begin();

            for (std::size_t i = 0; i < count; i++, it++) {
                result.push_back(*it);
            }

            return result;
        }


        /**
         * @brief Gets the lowest ranked scores.
         * @param count Number of entries to return.
         * @return Bottom leaderboard entries.
         */
        std::vector<leaderboard_entry<T>> get_bottom_scores(std::size_t count) const
        {
            std::vector<leaderboard_entry<T>> result;

            count = std::min(count, ranking.size());

            auto it = ranking.rbegin();

            for (std::size_t i = 0; i < count; i++, it++) {
                result.push_back(*it);
            }

            return result;
        }

         /**
         * @brief Gets entries between indices ranked from the top.
         * @param start Starting index.
         * @param end Ending index (excluded).
         * @return Entries in range.
         */
        std::vector<leaderboard_entry<T>> get_range_from_top(std::size_t start, std::size_t end) const {
            std::vector<leaderboard_entry<T>> result;

            if (start > end || start > ranking.size() || start == ranking.size()) {
                return result;
            }
                
            end = std::min(end, ranking.size());

            auto it = ranking.begin();

            std::advance(it, start);

            for (std::size_t rank = start; rank < end; rank++, it++) {
                result.push_back(*it);
            }

            return result;
        }

         /**
         * @brief Gets entries between indices ranked from the bottom.
         * @param start Starting index.
         * @param end Ending index (excluded).
         * @return Entries in range.
         */
        std::vector<leaderboard_entry<T>> get_range_from_bottom(std::size_t start, std::size_t end) const {
            std::vector<leaderboard_entry<T>> result;
            if (start > end || start > ranking.size()) {
                return result;
            }
                
            end = std::min(end, ranking.size());
            
            auto it = ranking.rbegin();

            std::advance(it, start);

            for (std::size_t rank = start; rank <= end; rank++, it++)
            {
                result.push_back(*it);
            }

            return result;
        }

        /**
         * @brief Saves leaderboard data to disk.
         * @return True if successful.
         */
        bool save() const {
            try {
                json j = json::array();

                for (const auto& entry : ranking) {
                    j.push_back({
                        {LEADERBOARD_NAME_KEY, entry.name},
                        {LEADERBOARD_SCORE_KEY, entry.score},
                        {LEADERBOARD_TIMESTAMP_KEY, entry.timestamp}
                    });
                }

                std::ofstream out(filename);

                if (!out) {
                    return false;
                }
                    
                out << j.dump(4);  
            } catch (...) {  
                return false;
            }
            return true;
        }

        /**
         * @brief Loads leaderboard data from disk.
         * @return True if successful.
         */
        bool load() {
            try {
                std::ifstream in(filename);

                if (!in) {
                    return false;
                }
                    
                json j;
                in >> j;

                names_to_entry.clear();
                ranking.clear();

                for (const auto& item : j) {
                    leaderboard_entry<T> e;
                    e.name = item.at(LEADERBOARD_NAME_KEY).get<std::string>(),
                    e.score = item.at(LEADERBOARD_SCORE_KEY).get<T>(),
                    e.timestamp =item.at(LEADERBOARD_TIMESTAMP_KEY).get<std::int64_t>(),
                    e.id = next_id++;
                    
                    auto ranking_it = ranking.insert(e);
                    e.ranking_position = ranking_it;

                    names_to_entry[e.name] = e;
                }
                
            } catch (...) {
                return false;
            }
            
            return true;
        }

    private:

        /**
         * @brief Creates the storage path for a leaderboard file.
         * @param parent_directory Base storage directory.
         * @param filename File name.
         * @return Full path to the leaderboard JSON file.
         */
        static std::string get_leaderboard_path(const std::filesystem::path& parent_directory, std::string_view filename) {
            namespace fs = std::filesystem;
            try {
                fs::path directory = parent_directory / "leaderboards";
                fs::create_directories(directory);     
                fs::path file(filename);

                if (file.extension() != ".json") {
                    file += ".json";
                } 
                
                return (directory / file).string();
            } catch (const fs::filesystem_error& e) {
                throw std::runtime_error("Unable to create leaderboard directory for filename: " + std::string(filename) + " - " + std::string(e.what()));
            }
        }

        /**
         * @brief Removes the lowest-ranked entry from the leaderboard.
         *
         * Does nothing if the leaderboard is empty.
         */
        void remove_last() {
            if (ranking.empty()) {
                return;
            }
            auto last = std::prev(ranking.end());

            names_to_entry.erase(last->name);
            ranking.erase(last);
        }
};