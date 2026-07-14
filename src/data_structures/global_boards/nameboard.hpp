#pragma once
#include "data_structures/global_boards/entry.hpp"
#include <algorithm>
#include <fstream>
#include <string>
#include <stdint.h>
#include <filesystem>
#include <set>
#include <vector>
#include <unordered_map>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

constexpr const size_t DEFAULT_MAX_NAMEBOARD_SIZE = 10000;

constexpr const char* NAMEBOARD_NAME_KEY = "name";
constexpr const char* NAMEBOARD_TIMESTAMP_KEY = "timestamp";

class nameboard {
private:
    const std::string filename;
    const std::size_t max_length;

    std::uint64_t next_id = 0;

    std::unordered_map<std::string, entry> names_to_entry;
    std::multiset<entry, entry_comparator> ranking;

public:
    explicit nameboard(std::string filename, std::size_t max_length = DEFAULT_MAX_NAMEBOARD_SIZE)
        : filename(get_nameboard_path(std::move(filename))), max_length(max_length) {
        load();
    }

    bool add_name(const std::string& name) {
        if (names_to_entry.count(name) != 0) {
            return false;
        } 

        auto now = static_cast<std::int64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        entry entry {
            name,
            now,
            next_id++
        };

        auto it = ranking.insert(entry);
        entry.position = it;

        names_to_entry[name] = entry;

        // Erase oldest if over max length
        if (ranking.size() > max_length) {
            auto oldest = ranking.begin(); 
            names_to_entry.erase(oldest->name);
            ranking.erase(oldest);
        }

        return true;
    }

    bool remove_name(const std::string& name) {
        auto it = names_to_entry.find(name);

        if (it == names_to_entry.end()) {
            return false;
        }
            
        ranking.erase(it->second.position);
        names_to_entry.erase(it);

        return true;
    }

    bool contains(const std::string& name) const {
        return names_to_entry.count(name) != 0;
    }

    std::vector<entry> get_all() const {
        return {ranking.begin(), ranking.end()};
    }
    

    std::vector<entry> get_from_first(size_t amount) const {
        auto end = std::next(ranking.begin(), std::min(amount, ranking.size()));
        return {ranking.begin(), end};
    }

    std::vector<entry> get_in_bounds(size_t start, size_t end) const {
        if (start > ranking.size() || start > end || start == ranking.size()) {
            return {};
        }

        auto start_it = std::next(ranking.begin(), start);
        auto end_it = std::next(ranking.begin(), std::min(end, ranking.size()));
        return {start_it, end_it};
    }

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
    static std::string get_nameboard_path(std::string filename) {
        namespace fs = std::filesystem;
        try {
            fs::path directory = fs::current_path() / "data" / "nameboards";
            fs::create_directories(directory);     
            fs::path file(filename);

            if (file.extension() != ".json") {
                file += ".json";
            } 
            return (directory / file).string();
        } catch (const fs::filesystem_error& e) {
            throw std::runtime_error("Unable to create nameboard directory for filename: " + filename + " - " + std::string(e.what()));
        }
    }


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