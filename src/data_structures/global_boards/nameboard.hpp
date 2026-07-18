#pragma once
#include "storage/file_helper.hpp"
#include "data_structures/global_boards/entry.hpp"
#include <algorithm>
#include <fstream>
#include <string>
#include <stdint.h>
#include <filesystem>
#include <optional>
#include <set>
#include <vector>
#include <list>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

constexpr const size_t DEFAULT_MAX_NAMEBOARD_SIZE = 10000;

constexpr const char* NAMEBOARD_NAME_KEY = "name";
constexpr const char* NAMEBOARD_TIMESTAMP_KEY = "timestamp";

constexpr const std::string NAMEBOARDS_SUBDIRECTORY_NAME = "nameboards";
/**
 * @class nameboard
 * @brief Stores and ranks unique names with optional persistence. 
 * New unique names are appended at the end.
 */
class nameboard {
    private:
        const std::filesystem::path file_path;
        const std::size_t max_size;

        std::uint64_t next_id = 0;

        std::list<entry> entries;
        std::multiset<entry*, entry_ptr_comparator> ranking;

    public:
        /**
         * @brief Creates a nameboard and loads existing data.
         * @param parent_directory Directory used for storage.
         * @param file_path Name of the JSON file.
         * @param max_size Maximum number of entries.
         */
        explicit nameboard(
            const std::filesystem::path& file_path,
            std::size_t max_size = DEFAULT_MAX_NAMEBOARD_SIZE
        ) : max_size(max_size), file_path(file_path) {
            load();
        }

        /**
         * @brief Adds a name to the board.
         * @param name Name to add.
         * @return Ranking position, or nullopt if the name already exists.
         */
        std::optional<std::size_t> add_name(const std::string& name) {
            auto now = static_cast<int64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

            entries.emplace_back(entry{
                name,
                now,
                next_id++
            });

            entry& e = entries.back();

            auto rank_it = ranking.insert(&e);
            e.position = rank_it;

            auto index = static_cast<size_t>(std::distance(ranking.begin(), rank_it));

            if (ranking.size() > max_size) {
                auto oldest = ranking.begin();
                entry* oldest_entry = *oldest;

                if (oldest == rank_it) {
                    ranking.erase(oldest);

                    for (auto it = entries.begin(); it != entries.end(); ++it) {
                        if (&*it == oldest_entry) {
                            entries.erase(it);
                            break;
                        }
                    }

                    return std::nullopt;
                }

                ranking.erase(oldest);

                for (auto it = entries.begin(); it != entries.end(); ++it) {
                    if (&*it == oldest_entry) {
                        entries.erase(it);
                        break;
                    }
                }
            }

            return index;
        }

         /**
         * @brief Checks if a name exists.
         * @param name Name to check.
         * @return True if found.
         */
        bool contains(const std::string& name) const {
        return std::any_of(
            entries.begin(),
            entries.end(),
            [&](const entry& e) {
                return e.name == name;
            });
        }

        /**
         * @brief Gets all entries in ranking order.
         * @return All stored entries.
         */
        std::vector<entry> get_all() const {
            std::vector<entry> result;
            result.reserve(ranking.size());

            for (auto e : ranking)
                result.push_back(*e);

            return result;
        }
        

        /**
         * @brief Gets the first ranked entries.
         * @param amount Number of entries to retrieve.
         * @return Ranked entries.
         */
        std::vector<entry> get_from_first(size_t amount) const {
            std::vector<entry> result;
            result.reserve(std::min(amount, ranking.size()));

            auto end = std::next(ranking.begin(), std::min(amount, ranking.size()));

            for (auto it = ranking.begin(); it != end; ++it) {
                result.push_back(**it);
            }
                
            return result;
        }

        /**
         * @brief Gets the last ranked entries.
         * @param amount Number of entries to retrieve.
         * @return Ranked entries, last to first.
         */
        std::vector<entry> get_from_last(size_t amount) const {
            std::vector<entry> result;
            result.reserve(std::min(amount, ranking.size()));

            auto end = std::next(ranking.rbegin(), std::min(amount, ranking.size()));

            for (auto it = ranking.rbegin(); it != end; ++it) {
                result.push_back(**it);
            }

            return result;
        }

        /**
         * @brief Gets entries in a ranking range.
         * @param start Starting index.
         * @param end Ending index (exclusive).
         * @return Entries in the specified range.
         */
        std::vector<entry> get_in_range_from_top(size_t start, size_t end) const {
            if (start >= ranking.size() || start > end) {
                return {};
            }   

            auto first = std::next(ranking.begin(), start);
            auto last  = std::next(ranking.begin(), std::min(end, ranking.size()));

            std::vector<entry> result;
            result.reserve(std::distance(first, last));

            for (auto it = first; it != last; ++it) {
                result.push_back(**it);
            }

            return result;
        }


        /**
         * @brief Gets entries in a ranking range counted from the bottom.
         * @param start Starting index from the bottom.
         * @param end Ending index from the bottom (exclusive).
         * @return Entries in the specified range, bottom to top.
         */
        std::vector<entry> get_in_range_from_bottom(size_t start, size_t end) const {
            if (start >= ranking.size() || start > end) {
                return {};
            }

            auto first = std::next(ranking.rbegin(), start);
            auto last  = std::next(ranking.rbegin(), std::min(end, ranking.size()));

            std::vector<entry> result;
            result.reserve(std::distance(first, last));

            for (auto it = first; it != last; ++it) {
                result.push_back(**it);
            }

            return result;
        }

         /**
         * @brief Saves the board to disk.
         * @return True if saved successfully.
         */
        bool save() const {
            try {
                 json j = json::array();

                for (const auto* e : ranking) {
                    j.push_back({
                        {NAMEBOARD_NAME_KEY, e->name},
                        {NAMEBOARD_TIMESTAMP_KEY, e->timestamp}
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

        /**
         * @brief Loads entries from disk.
         * @return True if loaded successfully.
         */
        bool load() {
            try {
                std::ifstream in(file_path);

                if (!in) {
                    return false;
                }    

                json j;
                in >> j;

                entries.clear();
                ranking.clear();
                next_id = 0;

                for (const auto& item : j) {
                    entries.emplace_back(entry{
                        item.at(NAMEBOARD_NAME_KEY).get<std::string>(),
                        item.at(NAMEBOARD_TIMESTAMP_KEY).get<int64_t>(),
                        next_id++
                    });

                    entry& e = entries.back();

                    auto rank_it = ranking.insert(&e);
                    e.position = rank_it;
                }
            } catch (...) {
                return false;
            }
        
            return true;
        }
};