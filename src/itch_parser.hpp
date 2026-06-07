#pragma once
/**
 * @file itch_parser.hpp
 * @brief NASDAQ ITCH 5.0 Binary Protocol Parser — Header Only
 *
 * Parses the canonical exchange feed format used by NASDAQ, NYSE/Arca, and
 * dozens of other venues. This is the exact wire protocol that HFT firms
 * ingest at nanosecond granularity via kernel-bypass NICs.
 *
 * Spec: https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHSpecification.pdf
 *
 * Design:
 *   - Zero-copy parsing: we reinterpret_cast directly over the buffer.
 *   - Network byte order (big-endian) -> host order conversion via intrinsics.
 *   - All message structs are packed to match wire layout exactly.
 *   - No heap allocations in the hot path.
 */

#include <cstdint>
#include <cstring>

#ifdef _MSC_VER
#include <intrin.h>
#define ARCTIC_BSWAP16(x) _byteswap_ushort(x)
#define ARCTIC_BSWAP32(x) _byteswap_ulong(x)
#define ARCTIC_BSWAP64(x) _byteswap_uint64(x)
#define ARCTIC_PACKED
#pragma pack(push, 1)
#else
#define ARCTIC_BSWAP16(x) __builtin_bswap16(x)
#define ARCTIC_BSWAP32(x) __builtin_bswap32(x)
#define ARCTIC_BSWAP64(x) __builtin_bswap64(x)
#define ARCTIC_PACKED __attribute__((packed))
#endif

namespace arctic {
namespace itch {

// ═══════════════════════════════════════════════════════════════════════
// Byte-order utilities
// ═══════════════════════════════════════════════════════════════════════

inline uint16_t ntoh16(uint16_t x) { return ARCTIC_BSWAP16(x); }
inline uint32_t ntoh32(uint32_t x) { return ARCTIC_BSWAP32(x); }
inline uint64_t ntoh64(uint64_t x) { return ARCTIC_BSWAP64(x); }

// ITCH uses 6-byte big-endian timestamps (nanoseconds since midnight)
inline uint64_t ntoh48(const uint8_t* p) {
    uint64_t val = 0;
    val |= static_cast<uint64_t>(p[0]) << 40;
    val |= static_cast<uint64_t>(p[1]) << 32;
    val |= static_cast<uint64_t>(p[2]) << 24;
    val |= static_cast<uint64_t>(p[3]) << 16;
    val |= static_cast<uint64_t>(p[4]) << 8;
    val |= static_cast<uint64_t>(p[5]);
    return val;
}

// ═══════════════════════════════════════════════════════════════════════
// ITCH 5.0 Message Type Codes
// ═══════════════════════════════════════════════════════════════════════

enum class MessageType : char {
    SystemEvent       = 'S',  // 12 bytes
    StockDirectory    = 'R',  // 39 bytes
    AddOrder          = 'A',  // 36 bytes
    AddOrderMPID      = 'F',  // 40 bytes
    OrderExecuted     = 'E',  // 31 bytes
    OrderExecutedPrice= 'C',  // 36 bytes
    OrderCancel       = 'X',  // 23 bytes
    OrderDelete       = 'D',  // 19 bytes
    OrderReplace      = 'U',  // 35 bytes
    Trade             = 'P',  // 44 bytes
    CrossTrade        = 'Q',  // 40 bytes
    BrokenTrade       = 'B',  // 19 bytes
    NOII              = 'I',  // 50 bytes
    StockTradingAction= 'H',  // 25 bytes
    RegSHO            = 'Y',  // 20 bytes
    MarketParticipant = 'L',  // 26 bytes
    MWCBDecline       = 'V',  // 35 bytes
    MWCBStatus        = 'W',  // 12 bytes
    Luld              = 'J',  // 35 bytes
    Unknown           = '?',
};

// ═══════════════════════════════════════════════════════════════════════
// Wire-format message structs (packed, big-endian on wire)
//
// We define the raw layout, then provide host-order accessor methods.
// ═══════════════════════════════════════════════════════════════════════

// --- Add Order (no MPID) — 'A', 36 bytes ---
struct ARCTIC_PACKED AddOrderMsg {
    char     type;              // 'A'
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    uint64_t order_ref;
    char     side;              // 'B' = buy, 'S' = sell
    uint32_t shares;
    char     stock[8];          // Space-padded
    uint32_t price;             // Price * 10000 (4 decimal places)

    // Host-order accessors
    uint64_t get_order_ref() const { return ntoh64(order_ref); }
    uint32_t get_shares()    const { return ntoh32(shares); }
    uint32_t get_price_raw() const { return ntoh32(price); }
    double   get_price()     const { return ntoh32(price) / 10000.0; }
    bool     is_buy()        const { return side == 'B'; }
    uint64_t get_timestamp() const { return ntoh48(timestamp); }
    uint16_t get_locate()    const { return ntoh16(stock_locate); }
};

// --- Add Order with MPID — 'F', 40 bytes ---
struct ARCTIC_PACKED AddOrderMPIDMsg {
    char     type;              // 'F'
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    uint64_t order_ref;
    char     side;
    uint32_t shares;
    char     stock[8];
    uint32_t price;
    char     mpid[4];           // Market participant ID

    uint64_t get_order_ref() const { return ntoh64(order_ref); }
    uint32_t get_shares()    const { return ntoh32(shares); }
    double   get_price()     const { return ntoh32(price) / 10000.0; }
    bool     is_buy()        const { return side == 'B'; }
    uint64_t get_timestamp() const { return ntoh48(timestamp); }
    uint16_t get_locate()    const { return ntoh16(stock_locate); }
};

// --- Order Executed — 'E', 31 bytes ---
struct ARCTIC_PACKED OrderExecutedMsg {
    char     type;              // 'E'
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    uint64_t order_ref;
    uint32_t shares;
    uint64_t match_number;

    uint64_t get_order_ref() const { return ntoh64(order_ref); }
    uint32_t get_shares()    const { return ntoh32(shares); }
    uint64_t get_timestamp() const { return ntoh48(timestamp); }
};

// --- Order Executed with Price — 'C', 36 bytes ---
struct ARCTIC_PACKED OrderExecutedPriceMsg {
    char     type;              // 'C'
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    uint64_t order_ref;
    uint32_t shares;
    uint64_t match_number;
    char     printable;
    uint32_t price;

    uint64_t get_order_ref() const { return ntoh64(order_ref); }
    uint32_t get_shares()    const { return ntoh32(shares); }
    double   get_price()     const { return ntoh32(price) / 10000.0; }
    uint64_t get_timestamp() const { return ntoh48(timestamp); }
};

// --- Order Cancel — 'X', 23 bytes ---
struct ARCTIC_PACKED OrderCancelMsg {
    char     type;              // 'X'
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    uint64_t order_ref;
    uint32_t cancelled_shares;

    uint64_t get_order_ref()       const { return ntoh64(order_ref); }
    uint32_t get_cancelled_shares() const { return ntoh32(cancelled_shares); }
    uint64_t get_timestamp()       const { return ntoh48(timestamp); }
};

// --- Order Delete — 'D', 19 bytes ---
struct ARCTIC_PACKED OrderDeleteMsg {
    char     type;              // 'D'
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    uint64_t order_ref;

    uint64_t get_order_ref() const { return ntoh64(order_ref); }
    uint64_t get_timestamp() const { return ntoh48(timestamp); }
};

// --- Order Replace — 'U', 35 bytes ---
struct ARCTIC_PACKED OrderReplaceMsg {
    char     type;              // 'U'
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    uint64_t original_order_ref;
    uint64_t new_order_ref;
    uint32_t shares;
    uint32_t price;

    uint64_t get_original_ref() const { return ntoh64(original_order_ref); }
    uint64_t get_new_ref()      const { return ntoh64(new_order_ref); }
    uint32_t get_shares()       const { return ntoh32(shares); }
    double   get_price()        const { return ntoh32(price) / 10000.0; }
    uint64_t get_timestamp()    const { return ntoh48(timestamp); }
};

// --- Trade (non-cross) — 'P', 44 bytes ---
struct ARCTIC_PACKED TradeMsg {
    char     type;              // 'P'
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    uint64_t order_ref;
    char     side;
    uint32_t shares;
    char     stock[8];
    uint32_t price;
    uint64_t match_number;

    uint64_t get_order_ref() const { return ntoh64(order_ref); }
    uint32_t get_shares()    const { return ntoh32(shares); }
    double   get_price()     const { return ntoh32(price) / 10000.0; }
    uint64_t get_timestamp() const { return ntoh48(timestamp); }
};

// --- System Event — 'S', 12 bytes ---
struct ARCTIC_PACKED SystemEventMsg {
    char     type;              // 'S'
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    char     event_code;        // 'O','S','Q','M','E','C'

    uint64_t get_timestamp() const { return ntoh48(timestamp); }
};

#ifdef _MSC_VER
#pragma pack(pop)
#endif

// ═══════════════════════════════════════════════════════════════════════
// Message size lookup (for skipping unknown types)
// ═══════════════════════════════════════════════════════════════════════

inline int message_size(char type) {
    switch (type) {
        case 'S': return 12;
        case 'R': return 39;
        case 'A': return 36;
        case 'F': return 40;
        case 'E': return 31;
        case 'C': return 36;
        case 'X': return 23;
        case 'D': return 19;
        case 'U': return 35;
        case 'P': return 44;
        case 'Q': return 40;
        case 'B': return 19;
        case 'I': return 50;
        case 'H': return 25;
        case 'Y': return 20;
        case 'L': return 26;
        case 'V': return 35;
        case 'W': return 12;
        case 'J': return 35;
        default:  return -1; // Unknown
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Callback interface — user implements this to receive parsed messages
// ═══════════════════════════════════════════════════════════════════════

struct MessageHandler {
    virtual ~MessageHandler() = default;

    virtual void on_add_order(const AddOrderMsg&) {}
    virtual void on_add_order_mpid(const AddOrderMPIDMsg&) {}
    virtual void on_order_executed(const OrderExecutedMsg&) {}
    virtual void on_order_executed_price(const OrderExecutedPriceMsg&) {}
    virtual void on_order_cancel(const OrderCancelMsg&) {}
    virtual void on_order_delete(const OrderDeleteMsg&) {}
    virtual void on_order_replace(const OrderReplaceMsg&) {}
    virtual void on_trade(const TradeMsg&) {}
    virtual void on_system_event(const SystemEventMsg&) {}
};

// ═══════════════════════════════════════════════════════════════════════
// Core parser — processes a contiguous buffer of ITCH messages
//
// Each message in a .bin file is preceded by a 2-byte big-endian length.
// Returns the number of messages parsed.
// ═══════════════════════════════════════════════════════════════════════

struct ParseStats {
    uint64_t messages_parsed = 0;
    uint64_t add_orders      = 0;
    uint64_t executions      = 0;
    uint64_t cancels         = 0;
    uint64_t deletes         = 0;
    uint64_t replaces        = 0;
    uint64_t trades          = 0;
    uint64_t bytes_consumed  = 0;
    uint64_t unknown_types   = 0;
};

inline ParseStats parse_buffer(const uint8_t* data, size_t len, MessageHandler& handler) {
    ParseStats stats;
    size_t offset = 0;

    while (offset + 2 < len) {
        // 2-byte big-endian message length prefix
        uint16_t msg_len = ntoh16(*reinterpret_cast<const uint16_t*>(data + offset));
        offset += 2;

        if (offset + msg_len > len || msg_len == 0) break;

        const uint8_t* msg_ptr = data + offset;
        char type = static_cast<char>(msg_ptr[0]);

        switch (type) {
            case 'A':
                handler.on_add_order(*reinterpret_cast<const AddOrderMsg*>(msg_ptr));
                stats.add_orders++;
                break;
            case 'F':
                handler.on_add_order_mpid(*reinterpret_cast<const AddOrderMPIDMsg*>(msg_ptr));
                stats.add_orders++;
                break;
            case 'E':
                handler.on_order_executed(*reinterpret_cast<const OrderExecutedMsg*>(msg_ptr));
                stats.executions++;
                break;
            case 'C':
                handler.on_order_executed_price(*reinterpret_cast<const OrderExecutedPriceMsg*>(msg_ptr));
                stats.executions++;
                break;
            case 'X':
                handler.on_order_cancel(*reinterpret_cast<const OrderCancelMsg*>(msg_ptr));
                stats.cancels++;
                break;
            case 'D':
                handler.on_order_delete(*reinterpret_cast<const OrderDeleteMsg*>(msg_ptr));
                stats.deletes++;
                break;
            case 'U':
                handler.on_order_replace(*reinterpret_cast<const OrderReplaceMsg*>(msg_ptr));
                stats.replaces++;
                break;
            case 'P':
                handler.on_trade(*reinterpret_cast<const TradeMsg*>(msg_ptr));
                stats.trades++;
                break;
            case 'S':
                handler.on_system_event(*reinterpret_cast<const SystemEventMsg*>(msg_ptr));
                break;
            default:
                stats.unknown_types++;
                break;
        }

        stats.messages_parsed++;
        offset += msg_len;
    }

    stats.bytes_consumed = offset;
    return stats;
}

} // namespace itch
} // namespace arctic
