#include <iostream>
#include <chrono>


enum class OrderType {
    LIMIT,
    MARKET
};

enum class TimeInForce {
    GTC,
    IOC,
    FOK
};

enum class Side {
    BUY,
    SELL
};

using Clock = std::chrono::steady_clock;

struct Order {
    int price;
    u_int64_t id;
    u_int64_t quantity;
    Side side;
    TimeInForce t;
    OrderType o;
    Clock::time_point timestamp;
};

int main() {
    return 0;
}