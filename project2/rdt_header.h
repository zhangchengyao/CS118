#include <cstdint>

struct header {
    uint16_t sourcePort;
    uint16_t destPort;
    uint16_t seqNum;
    uint16_t ackNum;
    uint16_t length;
    uint16_t flags;
};