#pragma once

#include "storage/file_helper.hpp"
#include "data_structures/global_boards/entry.hpp"
#include "data_structures/thread_safe/thread_safe_queue.hpp"

#include <optional>
#include <chrono>
#include <fstream>
#include <filesystem>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

constexpr std::size_t DEFAULT_MAX_SCORE_STREAM_SIZE = 300;

constexpr const char* SCORE_STREAM_NAME_KEY = "name";
constexpr const char* SCORE_STREAM_SCORE_KEY = "score";
constexpr const char* SCORE_STREAM_TIMESTAMP_KEY = "timestamp";

constexpr const char* SCORE_STREAM_SUBDIRECTORY_NAME = "score_streams";


/**
 * @brief Stores a stream of scores.
 *
 * Names are not required to be unique.
 * New scores are appended to the start of the stream.
 * Can query for most recent scores
 * 
 * @tparam T Score type.
 */
template<typename T>
class score_stream {
private:
    const std::filesystem::path file_path;
    const std::size_t max_size;

    std::uint64_t next_id = 0;

    thread_safe_queue<score_stream_entry<T>> entries;


public:

    explicit score_stream(
        const std::filesystem::path& file_path,
        std::size_t max_size = DEFAULT_MAX_SCORE_STREAM_SIZE
    )
        : file_path(file_path),
          max_size(max_size)
    {
        load();
    }


    /**
     * @brief Appends a new score to the front of the stream.
     */
    void submit_score(
        const std::string& name,
        T score
    ) {
        auto now = static_cast<std::int64_t>(
            std::chrono::system_clock::to_time_t(
                std::chrono::system_clock::now()
            )
        );


        score_stream_entry<T> entry;
        entry.name = name;
        entry.score = score;
        entry.timestamp = now;
        entry.id = next_id;

        entries.push_front(entry);


        while (entries.size() > max_size) {
            entries.pop_back();
        }
    }

    /**
     * @brief Gets all entries in stream order.
     */
    std::vector<score_stream_entry<T>> get_all() const
    {
        return entries.to_vector();
    }



    /**
     * @brief Gets first entries.
     */
    std::vector<score_stream_entry<T>> get_from_first(
        std::size_t amount
    ) const
    {
        auto all = entries.to_vector();

        if (amount < all.size()) {
            all.resize(amount);
        }

        return all;
    }



    /**
     * @brief Gets entries in range from the start.
     */
    std::vector<score_stream_entry<T>> get_in_bounds(std::size_t start, std::size_t end) const {
        auto all = entries.to_vector();

        if (start >= all.size() || start >= end) {
            return {};
        }


        end = std::min(end, all.size());


        return {
            all.begin() + start,
            all.begin() + end
        };
    }



    /**
     * @brief Saves score stream data.
     */
    bool save() const
    {
        try {
            json j = json::array();


            for (const auto& entry : entries.to_vector()) {

                j.push_back({
                    {SCORE_STREAM_NAME_KEY, entry.name},
                    {SCORE_STREAM_SCORE_KEY, entry.score},
                    {SCORE_STREAM_TIMESTAMP_KEY, entry.timestamp}
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


            entries.clear();
            next_id = 0;


            for (const auto& item : j) {

                score_stream_entry<T> entry;
                entry.name = item.at(SCORE_STREAM_NAME_KEY).get<std::string>();
                entry.score = item.at(SCORE_STREAM_SCORE_KEY).get<T>();
                entry.timestamp = item.at(SCORE_STREAM_TIMESTAMP_KEY).get<std::int64_t>();
                entry.id = next_id++;
                


                entries.push_front(entry);
            }


            while (entries.size() > max_size) {
                entries.pop_back();
            }


        } catch (...) {
            return false;
        }


        return true;
    }
};