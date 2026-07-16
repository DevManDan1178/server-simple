#pragma once

#include "storage/file_helper.hpp"
#include "data_structures/global_boards/entry.hpp"

#include <optional>
#include <chrono>
#include <fstream>
#include <unordered_map>
#include <filesystem>
#include <set>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

constexpr std::size_t DEFAULT_MAX_SCOREBOARD_SIZE = 300;

constexpr const char* SCOREBOARD_NAME_KEY = "name";
constexpr const char* SCOREBOARD_SCORE_KEY = "score";
constexpr const char* SCOREBOARD_TIMESTAMP_KEY = "timestamp";

constexpr const char* SCOREBOARD_SUBDIRECTORY_NAME = "scoreboards";
/**
 * @brief Stores unique names with associated scores.
 * 
 * Scores are stored but do not affect ranking.
 * Ranking is determined by entry_comparator.
 *
 * @tparam T Score type.
 */
template<typename T>
class scoreboard {
private:
    const std::filesystem::path file_path;
    const std::size_t max_size;

    std::uint64_t next_id = 0;

    std::unordered_map<std::string, scoreboard_entry<T>> names_to_entry;
    std::multiset<scoreboard_entry<T>, entry_comparator> ranking;


public:

    /**
     * @brief Creates a scoreboard and loads existing data.
     */
    explicit scoreboard(
        const std::filesystem::path& file_path,
        std::size_t max_size = DEFAULT_MAX_SCOREBOARD_SIZE
    )
        : file_path(file_path),
          max_size(max_size)
    {
        load();
    }


    /**
     * @brief Adds a new entry.
     *
     * @return Ranking position, or nullopt if name already exists.
     */
    std::optional<std::size_t> add_name(const std::string& name, T score) {
        if (names_to_entry.count(name) != 0) {
            return std::nullopt;
        }

        auto now = static_cast<std::int64_t>(
            std::chrono::system_clock::to_time_t(
                std::chrono::system_clock::now()
            )
        );

        scoreboard_entry<T> entry{
            name,
            score,
            now,
            next_id++
        };


        auto it = ranking.insert(entry);
        entry.position = it;

        names_to_entry[name] = entry;


        auto index = static_cast<std::size_t>(
            std::distance(ranking.begin(), it)
        );


        if (ranking.size() > max_size) {
            auto oldest = ranking.begin();

            names_to_entry.erase(oldest->name);
            ranking.erase(oldest);

            if (it == oldest) {
                return std::nullopt;
            }
        }

        return index;
    }


    /**
     * @brief Updates an existing score without changing ranking rules.
     */
    bool update_score(const std::string& name, T score) {
        auto it = names_to_entry.find(name);

        if (it == names_to_entry.end()) {
            return false;
        }


        ranking.erase(it->second.position);


        it->second.score = score;

        auto now = static_cast<std::int64_t>(
            std::chrono::system_clock::to_time_t(
                std::chrono::system_clock::now()
            )
        );

        it->second.timestamp = now;
        it->second.id = next_id++;


        auto ranking_it = ranking.insert(it->second);

        it->second.position = ranking_it;

        return true;
    }


    /**
     * @brief Removes a name.
     */
    bool remove_name(const std::string& name)
    {
        auto it = names_to_entry.find(name);

        if (it == names_to_entry.end()) {
            return false;
        }

        ranking.erase(it->second.position);
        names_to_entry.erase(it);

        return true;
    }


    /**
     * @brief Checks if a name exists.
     */
    bool contains(const std::string& name) const
    {
        return names_to_entry.count(name) != 0;
    }


    /**
     * @brief Gets all entries in ranking order.
     */
    std::vector<scoreboard_entry<T>> get_all() const
    {
        return {
            ranking.begin(),
            ranking.end()
        };
    }


    /**
     * @brief Gets first ranked entries.
     */
    std::vector<scoreboard_entry<T>> get_from_first(
        std::size_t amount
    ) const
    {
        auto end = std::next(
            ranking.begin(),
            std::min(amount, ranking.size())
        );

        return {
            ranking.begin(),
            end
        };
    }


    /**
     * @brief Gets entries in range.
     */
    std::vector<scoreboard_entry<T>> get_in_bounds(
        std::size_t start,
        std::size_t end
    ) const
    {
        if (
            start >= ranking.size() ||
            start > end
        ) {
            return {};
        }


        auto start_it = std::next(
            ranking.begin(),
            start
        );

        auto end_it = std::next(
            ranking.begin(),
            std::min(end, ranking.size())
        );


        return {
            start_it,
            end_it
        };
    }



    /**
     * @brief Saves scoreboard data.
     */
    bool save() const
    {
        try {
            json j = json::array();

            for (const auto& entry : ranking) {
                j.push_back({
                    {SCOREBOARD_NAME_KEY, entry.name},
                    {SCOREBOARD_SCORE_KEY, entry.score},
                    {SCOREBOARD_TIMESTAMP_KEY, entry.timestamp}
                });
            }


            std::ofstream out(file_path);

            if (!out) {
                return false;
            }

            out << j.dump(4);

        } catch (...) {
            return false;
        }

        return true;
    }



private:


    bool load()
    {
        try {
            std::ifstream in(file_path);

            if (!in) {
                return false;
            }


            json j;
            in >> j;


            names_to_entry.clear();
            ranking.clear();
            next_id = 0;


            for (const auto& item : j) {

                scoreboard_entry<T> entry{
                    item.at(SCOREBOARD_NAME_KEY).get<std::string>(),
                    item.at(SCOREBOARD_SCORE_KEY).get<T>(),
                    item.at(SCOREBOARD_TIMESTAMP_KEY).get<std::int64_t>(),
                    next_id++
                };


                auto it = ranking.insert(entry);

                entry.position = it;

                names_to_entry[entry.name] = entry;
            }


        } catch (...) {
            return false;
        }

        return true;
    }
};