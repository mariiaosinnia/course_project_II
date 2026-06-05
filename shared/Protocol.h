#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

constexpr size_t USERNAME_MAX_LEN = 32;
constexpr size_t ROOM_NAME_MAX_LEN = 32;
constexpr size_t MESSAGE_MAX_LEN = 64;

enum class PacketType : uint8_t {
    // client → server
    Connect = 0x01,
    Disconnect = 0x02,
    CreateRoom = 0x03,
    JoinRoom = 0x04,
    LeaveRoom = 0x05,
    ListRooms = 0x06,
    TrackSelect = 0x07,
    VoiceStart = 0x08,
    VoiceStop = 0x09,
    Ping = 0x0A,

    // server → client
    Connected = 0x10,
    RoomCreated = 0x11,
    RoomJoined = 0x12,
    RoomLeft = 0x13,
    RoomList = 0x14,
    UserJoined = 0x15,
    UserLeft = 0x16,
    TrackChanged = 0x17,
    TrackSync = 0x18,
    VoiceStarted = 0x19,
    VoiceStopped = 0x1A,
    Pong = 0x1B,
    Error = 0x1F,
};

enum class ErrorCode : uint8_t {
    NoError = 0x00,
    RoomNotFound = 0x01,
    RoomFull = 0x02,
    AlreadyInRoom = 0x03,
    NotInRoom = 0x04,
    Unknown
    //InvalidTrack = 0x05,
    //UsernameTaken = 0x06,
};

#pragma pack(push, 1)

struct PacketHeader {
    uint8_t type;
    uint16_t payload_size;
};

struct ResultPacket {
    bool success;
    PacketType packet_type;
    std::vector<uint8_t> data;
    ErrorCode error_code;
};

// Client → Server

struct ConnectBody {
    char username[USERNAME_MAX_LEN];
};

struct CreateRoomBody {
    char name[ROOM_NAME_MAX_LEN];
};

struct JoinRoomBody {
    uint16_t room_id;
};

//struct TrackSelectBody {
//    uint8_t track_id;
//};

// LeaveRoom, ListRooms, VoiceStart, VoiceStop, Ping, Disconnect — no body

// Server → Client

struct ConnectedBody {
    uint32_t client_id;
};
 
struct RoomCreatedBody {
    uint16_t room_id;
};
 
struct UserInfo {
    uint32_t client_id;
    char username[USERNAME_MAX_LEN];
};

// RoomJoined — dynamic payload
// [ room_id: 2 ][ track_id: 1 ][ track_position_ms: 4 ][ user_count: 1 ][ UserInfo × user_count ]
struct RoomJoinedHeader {
    uint16_t room_id;
    uint8_t track_id;
    uint32_t track_position_ms;
    uint8_t user_count;
};
 
struct RoomListEntry {
    uint16_t room_id;
    char name[ROOM_NAME_MAX_LEN];
    uint8_t user_count;
};
 
// RoomList — dynamic payload
// [ room_count: 1 ][ RoomListEntry × room_count ]
struct RoomListHeader {
    uint8_t room_count;
};
 
struct UserJoinedBody {
    uint32_t client_id;
    char username[USERNAME_MAX_LEN];
};
 
struct UserLeftBody {
    uint32_t client_id;
};
 
//struct TrackSyncBody {
//    uint8_t track_id;
//    uint32_t track_position_ms;
//};
 
struct VoiceStartedBody {
    uint32_t client_id;
};
 
struct VoiceStoppedBody {
    uint32_t client_id;
};

struct ErrorBody {
    uint8_t error_code;
};
 
// RoomLeft, Pong — no body

#pragma pack(pop)