#pragma once
#include "Track.h"
#include <string>
#include <memory>

class Mp3Decoder {
public:
    Mp3Decoder() = default;
    std::shared_ptr<Track> decode(const std::string& file_path) const;
};