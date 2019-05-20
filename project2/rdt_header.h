#include <cstdint>

struct header {
    uint32_t seqNum;
    uint32_t ackNum;
    uint16_t flags;
    uint16_t reserved;
};