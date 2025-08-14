#include <iostream>
#include <chrono>
#include <map>
#include <list>
#include <string>
#include <ctime>
#include <sstream>
#include <iomanip>

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

using Clock = std::chrono::system_clock;

struct Order {
    int price;
    u_int64_t id;
    u_int64_t quantity;
    Side side;
    TimeInForce t;
    OrderType o;
    Clock::time_point timestamp;
};

enum class Level {
    QUANTITY,
    PRICE
};

struct BidComparator {
    bool operator()(const Order& a, const Order& b) const {
        if (a.price == b.price) {
            return a.timestamp < b.timestamp;
        }
        return a.price > b.price;
    }
};

struct AskComparator {
    bool operator()(const Order& a, const Order& b) const {
        if (a.price == b.price) {
            return a.timestamp < b.timestamp;
        }
        return a.price < b.price;
    }
};

std::string FormatTime(const Clock::time_point& t) {
    auto time = Clock::to_time_t(t);
    std::tm tm = *std::localtime(&time);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

class OrderBook {
    private:
    std::map<int, std::list<Order>, std::greater<>> bids; // lists -> price levels
    std::map<int, std::list<Order>> asks;

    public:
    OrderBook() {

    };

    void match(int n) {
        std::map<Order, int, BidComparator> bidMatches;
        std::map<Order, int, AskComparator> askMatches;
        for (int i = 0; i < n; i++) {
            bool matched = false;
            if (!bids.empty() && !asks.empty() && bids.begin()->first >= asks.begin()->first) {
                Order b = bids.begin()->second.front();
                Order a = asks.begin()->second.front();

                bids.begin()->second.pop_front(); // pop from front of the price level
                asks.begin()->second.pop_front();

                bidMatches[b]++;
                askMatches[a]++;
                matched = true;

                // if price level empty, remove the price level
                if (bids.begin()->second.empty()) {
                    bids.erase(bids.begin());
                }
                if (asks.begin()->second.empty()) {
                    asks.erase(asks.begin());
                }
            }

            if (!matched) {
                break;
            }
        }
        for (auto& [o, q] : bidMatches) {
            std::cout << q << " sold at " << o.price << " at " << FormatTime(o.timestamp);
        }
    }
};

int main() {
    return 0;
}