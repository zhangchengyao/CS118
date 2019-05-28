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

struct packet {
    header hd;
    char data[MAX_DATA_SIZE];
};

inline bool isACK(uint16_t flags) {
    return (flags >> 15) & 1;
}
inline bool isSYN(uint16_t flags) {
    return (flags >> 14) & 1;
}
inline bool isFIN(uint16_t flags) {
    return (flags >> 13) & 1;
}

void printPacketInfo(const packet &pkt, int cwnd, int ssthresh, bool isSent) {
    if(isSent) {
        std::cout << "SEND ";
    } else {
        std::cout << "RECV ";
    }

    std::cout << pkt.hd.seqNum << " "
                << pkt.hd.ackNum << " "
                << "<cwnd> "
                << "<ssthresh> ";

    if(isACK(pkt.hd.flags)) {
        // ACK
        std::cout << "ACK";
        if(isSYN(pkt.hd.flags)) {
            std::cout << " SYN";
        } else if(isFIN(pkt.hd.flags)) {
            std::cout << " FIN";
        }
    } else if(isSYN(pkt.hd.flags)) {
        // SYN
        std::cout << "SYN";
    } else if(isFIN(pkt.hd.flags)) {
        // FIN
        std::cout << "FIN";
    }

    std::cout << std::endl;
}