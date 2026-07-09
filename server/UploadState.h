#pragma once
#include <cstdint>
#include <string>
#include <fstream>
#include <filesystem>

struct UploadState {
    uint16_t room_id = 0;
    std::string filename;
    std::string local_path;
    uint32_t expected_size = 0;
    uint32_t bytes_received = 0;
    std::ofstream file_stream;
    bool completed = false;

    UploadState() = default;
    UploadState(UploadState&&) = default;
    UploadState& operator=(UploadState&&) = default;

    ~UploadState() {
        if (file_stream.is_open()) {
            file_stream.close();
        }
        if (!completed && !local_path.empty()) {
            std::error_code ec;
            std::filesystem::remove(local_path, ec);
        }
    }
};
