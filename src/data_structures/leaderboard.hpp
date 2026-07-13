#include <chrono>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <set>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

constexpr const std::size_t MAX_LEADERBOARD_SIZE = 300;

constexpr const char* NAME_KEY = "name";
constexpr const char* SCORE_KEY = "score";
constexpr const char* TIMESTAMP_KEY = "timestamp";

template<typename T>
struct entry {
    std::string name;
    T score;
    std::int64_t timestamp;
    
    std::uint64_t id; 

    typename std::multiset<entry<T>, entry_comparator<T>>::iterator ranking_position;
};


template<typename T>
struct entry_comparator {
    bool operator()(const entry<T>& a, const entry<T>& b) const
    {
        if (a.score != b.score)
            return a.score > b.score; // higher scores first

        if (a.timestamp != b.timestamp)
            return a.timestamp < b.timestamp; // earlier achievement first

        return a.id < b.id;
    }
};


template<typename T>
class leaderboard {
    private:
        const std::string filename;
        const std::size_t max_size;
        std::uint64_t next_id = 0;
        std::unordered_map<std::string, entry<T>> names_to_entry;
        std::multiset<entry<T>, entry_comparator<T>> ranking;


    public:
        explicit leaderboard(
            std::string filename, 
            std::size_t max_size = MAX_LEADERBOARD_SIZE
        ) : filename(get_leaderboard_path(filename)), max_size(max_size) {
            load();
        }


        void submit_score(const std::string& player, T score) {
            auto now = static_cast<std::int64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

            auto it = names_to_entry.find(player);

            // If the player already exists
            if (it != names_to_entry.end()) {
                if (score <= it->second.score) {
                    return;
                }
                
                ranking.erase(it->second.ranking_position);

                it->second.score = score;
                it->second.timestamp = now;
                it->second.id = next_id++;

                it->second.ranking_position = ranking.insert(it->second);
                return;
            }

            // New player
            entry<T> e{
                player,
                score,
                now,
                next_id++
            };

        
            if (ranking.size() >= max_size) {
                auto worst = std::prev(ranking.end());

                if (!entry_comparator<T>{}(e, *worst)) {
                    return;
                }    
            }

            auto ranking_it = ranking.insert(e);
            e.ranking_position = ranking_it;

            names_to_entry[player] = e;

            if (ranking.size() > max_size) {
                remove_last();
            } 
        }


        std::vector<entry<T>> get_top_scores(std::size_t count) const {
            std::vector<entry<T>> result;

            count = std::min(count, ranking.size());

            auto it = ranking.begin();

            for (std::size_t i = 0; i < count; i++, it++) {
                result.push_back(*it);
            }

            return result;
        }


        std::vector<entry<T>> get_bottom_scores(std::size_t count) const
        {
            std::vector<entry<T>> result;

            count = std::min(count, ranking.size());

            auto it = ranking.rbegin();

            for (std::size_t i = 0; i < count; i++, it++) {
                result.push_back(*it);
            }

            return result;
        }

        std::vector<entry<T>> get_rank_range_from_top(std::size_t start, std::size_t end) const {
            std::vector<entry<T>> result;

            if (start == 0 || start > end || start > ranking.size()) {
                return result;
            }
                
            end = std::min(m, ranking.size());

            auto it = ranking.begin();

            std::advance(it, start - 1);

            for (std::size_t rank = start; rank <= end; rank++, it++)
            {
                result.push_back(*it);
            }

            return result;
        }


        std::vector<entry<T>> get_rank_range_from_bottom(std::size_t start, std::size_t end) const {
            std::vector<entry<T>> result;

            if (start == 0 || start > end || start > ranking.size())
                return result;

            end = std::min(m, ranking.size());

            auto it = ranking.rbegin();

            std::advance(it, start - 1);

            for (std::size_t rank = start; rank <= end; rank++, it++)
            {
                result.push_back(*it);
            }

            return result;
        }

        bool save() const
        {
            json j = json::array();

            for (const auto& entry : ranking) {
                j.push_back({
                    {NAME_KEY, entry.name},
                    {SCORE_KEY, entry.score},
                    {TIMESTAMP_KEY, entry.timestamp}
                });
            }

            std::ofstream out(filename);

            if (!out)
                return false;

            out << j.dump(4);
            return true;
        }


        bool load() {
            std::ifstream in(filename);

            if (!in)
                return false;

            json j;
            in >> j;

            names_to_entry.clear();
            ranking.clear();

            for (const auto& item : j) {
                entry<T> entry{
                    item.at(NAME_KEY).get<std::string>(),
                    item.at(SCORE_KEY).get<T>(),
                    item.at(TIMESTAMP_KEY).get<std::int64_t>(),
                    next_id++
                };

                auto ranking_it = ranking.insert(entry);
                entry.ranking_position = ranking_it;

                names_to_entry[entry.name] = entry;
            }

            return true;
        }

    private:
        static std::string get_leaderboard_path(std::string filename) {
            namespace fs = std::filesystem;
            try {
                fs::path directory = fs::current_path() / "data" / "leaderboard";
                fs::create_directories(directory);     
                fs::path file(filename);

                if (file.extension() != ".json") {
                    file += ".json";
                } 
            } catch (const fs::filesystem_error& e) {
                throw std::runtime_error("Unable to create leaderboard directory for filename: " + filename + " - " + std::string(e.what()));
            }
            
            return (directory / file).string();
        }

        void remove_last() {
            if (ranking.empty()) {
                return;
            }
            auto last = std::prev(ranking.end());

            names_to_entry.erase(last->name);
            ranking.erase(last);
        }
};