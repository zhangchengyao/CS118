#include <cstdint>

#define MAX_DATA_SIZE 512

struct header {
    uint32_t seqNum;
    uint32_t ackNum;
    uint16_t flags;
    uint16_t reserved;
    header() : seqNum(0), ackNum(0), flags(0), reserved(0) {}
};

struct buffer {
    header hd;
    char data[MAX_DATA_SIZE];
};