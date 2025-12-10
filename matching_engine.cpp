#include <iostream>
#include <chrono>
#include <map>
#include <vector>
#include <deque>
#include <string>
#include <ctime>
#include <sstream>
#include <numeric>
#include <random>
#include <iomanip>
#include <fstream>

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

struct latencyStats {
    std::vector<long long> fillLat;       // How long orders wait
    std::vector<long long> processLat;    // How fast the engine thinks

    void recordFill(long long l) { fillLat.push_back(l); }
    void recordProcess(long long l) { processLat.push_back(l); }

    void summary() {
        if (processLat.empty()) return;
        // 1. Calculate Fill Stats (Wait Time)
        long long totalFill = std::accumulate(fillLat.begin(), fillLat.end(), 0LL);
        double avgFill = fillLat.empty() ? 0.0 : (double)totalFill / fillLat.size();

        // 2. Calculate Processing Stats (Engine Speed)
        long long totalProc = std::accumulate(processLat.begin(), processLat.end(), 0LL);
        double avgProc = (double)totalProc / processLat.size();

        // Calculate P99 (Tail Latency) for Processing
        std::sort(processLat.begin(), processLat.end());
        long long p99Proc = processLat[processLat.size() * 0.99];
        long long maxProc = processLat.back();

        std::cout << "\n-- BENCHMARK RESULTS -- \n";
        std::cout << "Orders Processed:   " << processLat.size() << '\n';
        std::cout << "--------------------------------\n";
        std::cout << "Avg Processing Time: " << avgProc << " us (Wire-to-Wire)\n";
        std::cout << "P99 Processing Time: " << p99Proc << " us\n";
        std::cout << "Max Processing Time: " << maxProc << " us\n";
        std::cout << "--------------------------------\n";
        std::cout << "Avg Fill Wait Time:  " << avgFill << " us (Market Dynamics)\n";
    }
};

enum class Level {
    QUANTITY,
    PRICE
};

class OrderBook {
    private:
    std::map<int, std::deque<Order>, std::greater<>> bids; // lists -> price levels
    std::map<int, std::deque<Order>> asks;

    latencyStats stats;
    bool verbose = true;
    std::vector<std::pair<int, int>> history; 

    bool canFill(const Order& order) {
        long long qty = 0;
        if (order.side == Side::BUY) {
            for (const auto& [price, level] : asks) {
                if (order.price < price && order.o == OrderType::LIMIT) {
                    break;
                } 
                for (const Order& o : level) {
                    qty += o.quantity;
                }
                if (qty >= order.quantity) {
                    return true;
                }
            }
        } else {
            for (const auto& [price, level] : bids) {
                if (order.price > price && order.o == OrderType::LIMIT) {
                    break;
                } 
                for (const Order& o : level) {
                    qty += o.quantity;
                }
                if (qty >= order.quantity) {
                    return true;
                }
            }
        }
        return false;
    }

    void match(Order& order) {
        while (order.quantity > 0) {
            if (order.side == Side::BUY) {
                if (asks.empty()) {
                    return;
                }
                auto bestPrice = asks.begin();
                auto bestAsk = bestPrice->second.front();
                if (order.o == OrderType::LIMIT && bestAsk.price > order.price) {
                    break;
                }
                processMatch(order, bestPrice, asks);
            } else {
                if (bids.empty()) {
                    return;
                }
                auto bestPrice = bids.begin();
                auto bestBid = bestPrice->second.front();
                if (order.o == OrderType::LIMIT && bestBid.price < order.price) {
                    break;
                }
                processMatch(order, bestPrice, bids);
            }
        }
    }

    template<typename MapType, typename IterType>
    void processMatch(Order& order, IterType& entry, MapType& book) {
        std::deque<Order>& level = entry->second;
        while (order.quantity > 0 && level.size() > 0) {
            Order& resting = level.front();
            int matchingQty = std::min(order.quantity, resting.quantity);

            auto now = Clock::now();
            auto lat = std::chrono::duration_cast<std::chrono::microseconds>(now - resting.timestamp).count();
            stats.recordFill(lat);

            if (verbose) {
                std::cout << ">> TRADE: " << matchingQty << " @ " << resting.price << "\n";
            }
            order.quantity -= matchingQty;
            resting.quantity -= matchingQty;
            if (resting.quantity == 0) {
                level.pop_front();
            }
            if (level.size() == 0) {
                book.erase(entry);
                break;
            }
        }
        return;
    }

    public:

    OrderBook() {

    };

    void setVerbose(bool v) { verbose = v; }
    void printStats() { stats.summary(); }

    void recordPrice(int step, int price) {
        history.push_back({step, price});
    }

    void saveToCSV() {
        std::ofstream file("market_data.csv");
        file << "TimeStep,FairPrice\n";
        for (const auto& p : history) {
            file << p.first << "," << p.second << "\n";
        }
        file.close();
        std::cout << "\n[!] Data saved to 'market_data.csv'. Open in Excel to view graph.\n";
    }

    void addOrder(const Order& order) {
        auto start = Clock::now();
        Order o = order;
        if (order.t == TimeInForce::FOK && !canFill(order)) {
            if (verbose) printOB();
            auto end = Clock::now();
            auto execTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            stats.recordProcess(execTime / 1000);
            return;
        }
        match(o);
        if (o.quantity > 0 && o.t != TimeInForce::IOC && o.t != TimeInForce::FOK) {
            if (o.side == Side::BUY) {
                bids[o.price].push_back(o);
            } else {
                asks[o.price].push_back(o);
            }
        }
        auto end = Clock::now();
        auto execTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        stats.recordProcess(execTime / 1000);
        if (verbose) printOB();
    }

    void printOB() {
        std::cout << "\n------------------ ORDER BOOK ------------------\n";
        std::cout << "    BID QTY |   PRICE  ||   PRICE  |  ASK QTY   \n";
        std::cout << "------------|----------||----------|------------\n";
        auto itAsk = asks.begin();
        auto itBid = bids.begin();
        for (int i = 0; i < 10; i++) {
            if (itBid != bids.end()) {
                int bidQ = 0;
                for (const Order& o : itBid->second) {
                    bidQ += o.quantity;
                }
                if (bidQ) {
                    std::cout << std::setw(11) << bidQ << " | " << std::setw(8) << itBid->first << " || ";
                    itBid++;
                }
            } else {
                std::cout << "            |          || ";
            }

            if (itAsk != asks.end()) {
                long long askQ = 0;
                for (const auto& o : itAsk->second) {
                    askQ += o.quantity;
                }

                std::cout << std::setw(8) << itAsk->first << " | " 
                          << std::setw(10) << askQ << "\n";
                itAsk++;
            } else {
                std::cout << "         |            \n";
            }
        }
        std::cout << "------------------------------------------------\n";
        
    }
};

void runSimulation(OrderBook& ob, int n) {
    ob.setVerbose(false);
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> priceVar(-2, 2);     // Price moves up/down by small amount
    std::uniform_int_distribution<int> spreadVar(1, 5);     // Spread between bid/ask
    std::uniform_int_distribution<int> quantityVar(1, 100); // Order size
    std::uniform_int_distribution<int> typeProb(0, 100);    // Random % for Order Types
    int fairValue = 1000;

    auto start = Clock::now();

    for (int i = 0; i < n; ++i) {
        // simulate random walk
        fairValue += priceVar(rng);
        if (fairValue < 10) {
            fairValue = 10;
        }
        ob.recordPrice(i, fairValue);

        Side side = (typeProb(rng) < 50) ? Side::BUY : Side::SELL;
        int spread = spreadVar(rng);
        int price;

        // pricing logic
        if (side == Side::BUY) {
            price = fairValue - spread;
        } else {
            price = fairValue + spread;
        }
        
        OrderType type = OrderType::LIMIT;
        TimeInForce tif = TimeInForce::GTC;
        
        int p = typeProb(rng);
        // 10% chance to be a Market Order (Crossing the spread)
        if (p < 10) {
            type = OrderType::MARKET;
            price = (side == Side::BUY) ? 100000 : 0; 
            tif = TimeInForce::IOC;
        }
        // 5% FOK Orders (All or Nothing)
        else if (p < 15) {
            type = OrderType::LIMIT;
            tif = TimeInForce::FOK;
        }
        ob.addOrder({price, (uint64_t)i, (uint64_t)quantityVar(rng), side, tif, type, Clock::now()});
    }

    auto end = Clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    if (duration == 0) duration = 1;

    std::cout << "Simulation Complete.\n";
    std::cout << "Time taken: " << duration << " ms\n";
    std::cout << "Throughput: " << (n * 1000.0 / duration) << " orders/sec\n";
    
    ob.printStats();
    ob.saveToCSV();
}

int main() {
    OrderBook ob;

    int choice;
    
    std::cout << "1. Manual Mode\n2. Market Simulator Mode\nSelect: ";
    std::cin >> choice;
    if (choice == 2) {
        int n;
        std::cout << "How many orders? (e.g. 100000): ";
        std::cin >> n;
        runSimulation(ob, n);
    } else {
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
    }
    return 0;
}