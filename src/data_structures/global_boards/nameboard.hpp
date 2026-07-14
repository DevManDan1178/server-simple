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
#include <unordered_map>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

constexpr const size_t DEFAULT_MAX_NAMEBOARD_SIZE = 10000;

constexpr const char* NAMEBOARD_NAME_KEY = "name";
constexpr const char* NAMEBOARD_TIMESTAMP_KEY = "timestamp";

constexpr const std::string NAMEBOARDS_SUBDIRECTORY_NAME = "nameboards";
/**
 * @class nameboard
 * @brief Stores and ranks unique names with optional persistence.
 */
class nameboard {
    private:
        const std::string filename;
        const std::size_t max_size;

        std::uint64_t next_id = 0;

        std::unordered_map<std::string, entry> names_to_entry;
        std::multiset<entry, entry_comparator> ranking;

    public:
        /**
         * @brief Creates a nameboard and loads existing data.
         * @param parent_directory Directory used for storage.
         * @param filename Name of the JSON file.
         * @param max_size Maximum number of entries.
         */
        explicit nameboard(const std::filesystem::path& parent_directory, 
            std::string filename, 
            std::size_t max_size = DEFAULT_MAX_NAMEBOARD_SIZE
        ) : max_size(max_size), filename(file_helper::get_file_path(parent_directory, NAMEBOARDS_SUBDIRECTORY_NAME, std::move(filename))) {
            load();
        }

        /**
         * @brief Adds a name to the board.
         * @param name Name to add.
         * @return Ranking position, or nullopt if the name already exists.
         */
        std::optional<std::size_t> add_name(const std::string& name) {
            if (names_to_entry.count(name) != 0) {
                return std::nullopt;
            } 

            auto now = static_cast<std::int64_t>(
                std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())
            );

            entry entry {
                name,
                now,
                next_id++
            };

            auto it = ranking.insert(entry);
            entry.position = it;

            names_to_entry[name] = entry;

            auto index = static_cast<std::size_t>(
                std::distance(ranking.begin(), it)
            );

            // Erase oldest if over max length
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
         * @brief Removes a name from the board.
         * @param name Name to remove.
         * @return True if removed, false if not found.
         */
        bool remove_name(const std::string& name) {
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
         * @param name Name to check.
         * @return True if found.
         */
        bool contains(const std::string& name) const {
            return names_to_entry.count(name) != 0;
        }

        /**
         * @brief Gets all entries in ranking order.
         * @return All stored entries.
         */
        std::vector<entry> get_all() const {
            return {ranking.begin(), ranking.end()};
        }
        

         /**
         * @brief Gets the first ranked entries.
         * @param amount Number of entries to retrieve.
         * @return Ranked entries.
         */
        std::vector<entry> get_from_first(size_t amount) const {
            auto end = std::next(ranking.begin(), std::min(amount, ranking.size()));
            return {ranking.begin(), end};
        }

        /**
         * @brief Gets entries in a ranking range.
         * @param start Starting index.
         * @param end Ending index (exclusive).
         * @return Entries in the specified range.
         */
        std::vector<entry> get_in_bounds(size_t start, size_t end) const {
            if (start > ranking.size() || start > end || start == ranking.size()) {
                return {};
            }

            auto start_it = std::next(ranking.begin(), start);
            auto end_it = std::next(ranking.begin(), std::min(end, ranking.size()));
            return {start_it, end_it};
        }

         /**
         * @brief Saves the board to disk.
         * @return True if saved successfully.
         */
        bool save() const {
            json j = json::array();

            for (const auto& entry : ranking) {
                j.push_back({
                    {NAMEBOARD_NAME_KEY, entry.name},
                    {NAMEBOARD_TIMESTAMP_KEY, entry.timestamp}
                });
            }

            std::ofstream out(filename);

            if (!out) {
                return false;
            }
                
            out << j.dump(4);
            return true;
        }


    private:

        /**
         * @brief Loads entries from disk.
         * @return True if loaded successfully.
         */
        bool load() {
            std::ifstream in(filename);

            if (!in) {
                return false;
            }    

            json j;
            in >> j;

            names_to_entry.clear();
            ranking.clear();
            next_id = 0;

            for (const auto& item : j) {
                entry entry{
                    item.at(NAMEBOARD_NAME_KEY).get<std::string>(),
                    item.at(NAMEBOARD_TIMESTAMP_KEY).get<std::int64_t>(),
                    next_id++
                };

                auto it = ranking.insert(entry);
                entry.position = it;

                names_to_entry[entry.name] = entry;
            }

            return true;
        }
};