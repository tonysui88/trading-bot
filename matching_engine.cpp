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

    void addOrder(const Order& o) {
        if (o.side == Side::BUY) {
            // find price level, push to end of linked list
            bids[o.price].push_back(o);
        } else {
            asks[o.price].push_back(o);
        }
    }

    void match() {
        std::map<Order, int, BidComparator> bidMatches;
        std::map<Order, int, AskComparator> askMatches;
        while (true) {
            if (!bids.empty() && !asks.empty() && bids.begin()->first >= asks.begin()->first) {
                Order b = bids.begin()->second.front();
                Order a = asks.begin()->second.front();

                int qMatched = 0;
                if (b.quantity > a.quantity) {
                    // remove the first order completely of the asks
                    asks.begin()->second.pop_front();
                    qMatched = a.quantity;
                    bids.begin()->second.front().quantity -= qMatched;
                } else if (a.quantity > b.quantity) {
                    bids.begin()->second.pop_front();
                    qMatched = b.quantity;
                    asks.begin()->second.front().quantity -= qMatched;
                } else {
                    // pop order from both completely
                    asks.begin()->second.pop_front();
                    bids.begin()->second.pop_front();
                    qMatched = a.quantity;
                }

                bidMatches[b] += qMatched;
                askMatches[a] += qMatched;

                // if price level empty, remove the price level
                if (bids.begin()->second.empty()) {
                    bids.erase(bids.begin());
                }
                if (asks.begin()->second.empty()) {
                    asks.erase(asks.begin());
                }
            } else {
                break;
            }
        }
        for (auto& [o, q] : bidMatches) {
            std::cout << q << " sold at " << o.price << " at " << FormatTime(o.timestamp) << '\n';
        }
    }
};

int main() {
    return 0;
}