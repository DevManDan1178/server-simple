#pragma once

#include "storage/file_helper.hpp"
#include "data_structures/global_boards/entry.hpp"

#include <deque>
#include <optional>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <ranges>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

constexpr std::size_t DEFAULT_MAX_SCORE_STREAM_SIZE = 300;

constexpr const char* SCORE_STREAM_NAME_KEY = "name";
constexpr const char* SCORE_STREAM_SCORE_KEY = "score";
constexpr const char* SCORE_STREAM_TIMESTAMP_KEY = "timestamp";

constexpr const char* SCORE_STREAM_SUBDIRECTORY_NAME = "score-streams";


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
        std::deque<score_stream_entry<T>> entries;


    public:

        explicit score_stream(
            const std::filesystem::path& file_path,
            std::size_t max_size = DEFAULT_MAX_SCORE_STREAM_SIZE
        ) : file_path(file_path), max_size(max_size) {
            load();
        }


        /**
         * @brief Appends a new score to the front of the stream.
         */
        void submit_score(const std::string& name, T score) {
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
        std::vector<score_stream_entry<T>> get_all() const {
            return std::vector<score_stream_entry<T>>(entries.begin(),entries.end());
        }

        /**
         * @brief Gets first entries.
         */
        std::vector<score_stream_entry<T>> get_from_first(std::size_t amount) const {
            auto all = get_all();

            if (amount < all.size()) {
                all.resize(amount);
            }

            return all;
        }



        /**
         * @brief Gets entries in range from the start (newest).
         */
        std::vector<score_stream_entry<T>> get_in_range_from_top(std::size_t start, std::size_t end) const {
            auto all = get_all();

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
         * @brief Gets entries in range from the bottom (oldest).
         */
        std::vector<score_stream_entry<T>> get_in_range_from_bottom(std::size_t start, std::size_t end) const {
            auto all = get_all();

            if (start >= all.size() || start >= end) {
                return {};
            }

            end = std::min(end, all.size());

            std::vector<score_stream_entry<T>> result;
            result.reserve(end - start);

            // Convert bottom index to top index.
            auto bottom_start = all.size() - end;
            auto bottom_end = all.size() - start;

            return {
                all.begin() + bottom_start,
                all.begin() + bottom_end
            };
        }

        size_t size() {
            return entries.size();
        }


        /**
         * @brief Saves score stream data to disk.
         * JSON save order is from least to most recent (highest) timestamp
         * Creates a temporary .tmp file to write all the data
         * After successfully writing, replaces the original file with the new data
         * @return true if saved successfully
         */
        bool save() const {
            const std::string temp_path = std::string(file_path) + ".tmp";
            try {
                json j = json::array();

                for (const auto& entry : get_all() | std::views::reverse) {
                    j.push_back({
                        {SCORE_STREAM_NAME_KEY, entry.name},
                        {SCORE_STREAM_SCORE_KEY, entry.score},
                        {SCORE_STREAM_TIMESTAMP_KEY, entry.timestamp}
                    });
                }

                {
                    std::ofstream out(temp_path, std::ios::binary | std::ios::trunc);

                    if (!out) {
                        return false;
                    }

                    out << j.dump(4);
                    out.flush();

                    if (!out) {
                        return false;
                    }
                }

                std::filesystem::rename(temp_path, file_path);

                return true;
            }
            catch (...) {
                std::error_code ec;
                std::filesystem::remove(temp_path, ec);
                return false;
            }
        }

    private:

        /**
         * @brief Loads entries from disk
         * JSON saved order should be from least to most recent (highest) timestamp
         * Creates temporary new values to write into
         * Once the write is completed successfully, replaces the data with the values in the temporary data
         * @return true if successfully loaded
         */
        bool load() {
            try {

                std::ifstream in(file_path);

                if (!in) {
                    return false;
                }


                json j;
                in >> j;

                if (!j.is_array()) {
                    return false;
                }

                std::deque<score_stream_entry<T>> new_entries;
                std:uint64_t new_next_id = 0;

                std::size_t item_count = 0;
                for (const auto& item : j) {
                    if (++item_count > max_size) {
                        break;
                    }

                    if (!item.is_object()) {
                        return false;
                    }

                    score_stream_entry<T> entry;
                    entry.name = item.at(SCORE_STREAM_NAME_KEY).get<std::string>();
                    entry.score = item.at(SCORE_STREAM_SCORE_KEY).get<T>();
                    entry.timestamp = item.at(SCORE_STREAM_TIMESTAMP_KEY).get<std::int64_t>();
                    entry.id = new_next_id++;
                    
                    new_entries.push_front(entry); 
                }

                entries = std::move(new_entries);
                next_id = new_next_id;

            } catch (...) {
                return false;
            }

            return true;
        }
};