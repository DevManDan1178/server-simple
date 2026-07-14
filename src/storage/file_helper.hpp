#pragma once
#include "logger.hpp"
#include <string_view>
#include <filesystem>
namespace file_helper {
    inline bool is_valid_filename(std::string_view name) {
        if (name.empty())
            return false;

        std::filesystem::path p(name);

        if (p.is_absolute())
            return false;

        for (const auto& part : p) {
            if (part == "..")
                return false;
        }

        return name.find('/') == std::string_view::npos &&
            name.find('\\') == std::string_view::npos;
    }

    /**
     * @brief Creates the storage path for a file.
     * @param parent_directory Base storage directory.
     * @param subdirectory_name Name of the subdirectory in the base directory
     * @param filename File name.
     * @return Full path to the JSON file.
     */
    inline static std::string get_file_path(const std::filesystem::path& parent_directory, std::string_view subdirectory_name, std::string_view filename) {
        namespace fs = std::filesystem;
        try {
            if (!is_valid_filename(filename)) {
                throw std::invalid_argument("Invalid filename - file_helper");
            }

            fs::path directory = parent_directory / subdirectory_name;
            fs::create_directories(directory);     
            fs::path file(filename);

            if (file.extension() != ".json") {
                file += ".json";
            } 
            
            return (directory / file).string();
        } catch (const fs::filesystem_error& e) {
            throw std::runtime_error("Unable to create " + std::string(subdirectory_name) + " directory for filename: " + std::string(filename) +
            + "under " + std::string(parent_directory) + "/" + std::string(subdirectory_name) +
            " - " + std::string(e.what()));
        }
    }

    /**
     * @brief Creates the storage path for a file.
     * @param parent_directory Base storage directory.
     * @param filename File name.
     * @return Full path to the JSON file.
     */
    inline static std::string get_file_path(const std::filesystem::path& parent_directory, std::string_view filename) {
        namespace fs = std::filesystem;
        try {
            if (!is_valid_filename(filename)) {
                throw std::invalid_argument("Invalid filename - file_helper");
            }

            fs::create_directories(parent_directory);     
            fs::path file(filename);

            if (file.extension() != ".json") {
                file += ".json";
            } 
            
            return (parent_directory / file).string();
        } catch (const fs::filesystem_error& e) {
            throw std::runtime_error("Unable to create directory for filename: " + std::string(filename) + 
                "under " + std::string(parent_directory) + 
                " - " + std::string(e.what()));
        }
    }
}