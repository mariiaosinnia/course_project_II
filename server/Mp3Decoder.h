#pragma once
#include "Track.h"
#include <string>
#include <memory>

class Mp3Decoder {
public:
    Mp3Decoder() = default;
    void decode(Track& track) const;
};