#include <cstdint>

#define MAX_DATA_SIZE 512

//                             header format

//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                        Sequence Number                        |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                      Acknowledge Number                       |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |A|S|F|                       |          Reserved               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

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