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

    void match(Order order) {
        std::map<Order, int, BidComparator> bidMatches;
        std::map<Order, int, AskComparator> askMatches;
        int qMatched = 0;

        int p = order.price;
        int qMatched = 0;
        // first check for FOK
        bool filled = false;
        if (order.side == Side::BUY && order.t == TimeInForce::FOK) {
            for (const auto& [price, orders] : asks) {
                if (p < price && !filled) {
                    return;
                }
                for (const auto& o : orders) {
                    if (p >= o.price || order.o == OrderType::MARKET) {
                        qMatched += o.quantity;
                    }
                    if (qMatched >= order.quantity) {
                        filled = true;
                        break;
                    }
                }
                if (filled) {
                    break;
                }
            }
        } else if (order.side == Side::SELL && order.t == TimeInForce::FOK) {
            for (const auto& [price, orders] : bids) {
                if (p > price && !filled) {
                    return;
                }
                for (const auto& o : orders) {
                    if (p <= o.price || order.o == OrderType::MARKET) {
                        qMatched += o.quantity;
                    }
                    if (qMatched >= order.quantity) {
                        filled = true;
                        break;
                    }
                }
                if (filled) {
                    break;
                }
            }
        }

        // 
        while (true) {
            if (order.side == Side::BUY) {
                if ((order.o == OrderType::MARKET && !asks.empty()) || (!asks.empty() && p >= asks.begin()->first)) {
                    Order a = asks.begin()->second.front();
                    if (order.quantity > a.quantity) {

                        order.quantity -= a.quantity;
                        // remove the first order completely of the asks
                        asks.begin()->second.pop_front();

                    } else if (a.quantity > order.quantity) {                        
                        asks.begin()->second.front().quantity -= qMatched;
                        order.quantity = 0;
                        break;
                    } else {
                        asks.begin()->second.pop_front();
                        order.quantity = 0;
                        break;
                    }

                    if (order.quantity == 0) {
                        break;
                    }

                    if (asks.begin()->second.empty()) {
                        asks.erase(asks.begin());
                    }
                // GTC adds unmatched back into OB only if limit order
                } else if (order.o == OrderType::LIMIT && order.t == TimeInForce::GTC) {
                    bids[order.price].push_back(order);
                }
            
            } else {
                if ((order.o == OrderType::MARKET && !asks.empty()) || !asks.empty() && p >= asks.begin()->first) {
                    Order a = asks.begin()->second.front();
                    if (order.quantity > a.quantity) {

                        order.quantity -= a.quantity;
                        // remove the first order completely of the asks
                        asks.begin()->second.pop_front();

                    } else if (a.quantity > order.quantity) {                        
                        asks.begin()->second.front().quantity -= qMatched;
                        order.quantity = 0;
                        break;
                    } else {
                        asks.begin()->second.pop_front();
                        order.quantity = 0;
                        break;
                    }

                    if (order.quantity == 0) {
                        break;
                    }

                    if (asks.begin()->second.empty()) {
                        asks.erase(asks.begin());
                    }
                // GTC adds unmatched back into OB
                } else if (order.o == OrderType::LIMIT && order.t == TimeInForce::GTC) {
                    bids[order.price].push_back(order);
                }
            }
        }
        for (auto& [o, q] : bidMatches) {
            std::cout << q << " sold at " << o.price << " at " << FormatTime(o.timestamp) << '\n';
        }
    }

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
        match(o);
        // printOB();
    }

    // void printOB() {
    //     std::cout << "          BIDS            |           ASKS           ";
    //     int size = std::min(bids.size(), asks.size());
    //     for (int i = 0; i < size; i++) {
    //         auto& it1 = bids.begin();
    //         auto& it2 = bids.begin();

    //         int bidP = bids.begin()->first;
    //         int asksP = asks.begin()->first;
    //         std::cout << "         " << bidP << "           |               " << asksP << '\n';
    //         it1 = it1.next
    //     }
    // }
};

int main() {
    OrderBook ob;

    std::cout << "Enter Order: {Price}, {Id}, {Quantity}, {BUY/SELL}, {GTC/IOC/FOK}, {LIMIT/MARKET} \n";

    while (true) {
        int price;
        uint64_t id, quantity;
        std::string side, timeIn, type;
        std::cin >> price >> id >> quantity >> side >> timeIn >> type;

        Side s = side == "BUY" ? Side::BUY : Side::SELL;
        TimeInForce t;
        if (timeIn == "GTC") {
            t = TimeInForce::GTC;
        } else if (timeIn == "IOC") {
            t = TimeInForce::IOC;
        } else if (timeIn == "FOK") {
            t = TimeInForce::FOK;
        }
        OrderType ty = type == "LIMIT" ? OrderType::LIMIT : OrderType::MARKET;

        ob.addOrder({price, id, quantity, s, t, ty, Clock::now()});
    }
    return 0;
}