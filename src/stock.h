#ifndef TAX_FREE_STOCK_MARKET
#define TAX_FREE_STOCK_MARKET
#include <iostream>
#include <vector>
#include <memory>
#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include <utility>

using namespace std;

class Stock;
class Account;
class Transaction;
class Logs;



const double inf = 1e10;
const double float_eps = 1e-7;

//bool less2(pair<double, shared_ptr<Order> >a, pair<double, shared_ptr<Order> > b);
//bool more2(pair<double, shared_ptr<Order> >a, pair<double, shared_ptr<Order> > b);

typedef pair<shared_ptr<Stock>, int> StockShare;

class Order
{
public:
    typedef enum
    {
        BUY,
        SELL
    } OrderType;

private:
    double price;
    StockShare item;
    shared_ptr<Account> usr;

public:
    Order(StockShare _item, shared_ptr<Account> _usr, double _price, OrderType _type);
    OrderType order_type;
    void add_share(int num_share);
    void reduce_share(int num_share);
    int get_shares();
    double get_price();
    double get_total_money();

    

    shared_ptr<Account> get_usr();
    shared_ptr<Stock> get_stock();
    void debug();
};



struct less2 {
    bool operator() (pair<double, shared_ptr<Order> > a, pair<double, shared_ptr<Order> > b) const 
    {
        if (a.first < b.first)
            return true;
        return false;
    }
};

struct more2 {
    bool operator() (pair<double, shared_ptr<Order> > a, pair<double, shared_ptr<Order> > b) const 
    {
        if (a.first > b.first)
            return true;
        return false;
    }
};

void display(set<pair<double, shared_ptr<Order> >, less2 > &x);
void display(set<pair<double, shared_ptr<Order> >, more2 > &x);


typedef vector<shared_ptr<Order> > OrderList;
typedef pair<int, shared_ptr<Order> > OrderResult;

class Stock : public enable_shared_from_this<Stock>
{
    set<pair<double, shared_ptr<Order> >, more2 > buy_orders;//first element is price
    set<pair<double, shared_ptr<Order> >, less2 > sell_orders;
    shared_ptr<Logs> logger;
    bool freeze;
    int stock_id;
    double st_price_now;
    double st_today_price;
    Stock() = delete;

    //friend int Market::usr_buy(int usr_id, int stock_id, int num_share, double price);

public:
    Stock(int _id, shared_ptr<Logs> _logger, double price );
    OrderResult buy(shared_ptr<Account> usr, int num_share, double price);
    OrderResult sell(shared_ptr<Account> usr, int num_share, double price);
    void add_buy_order_when_undo(shared_ptr<Order> buyorder);
    void add_sell_order_when_undo(shared_ptr<Order> sellorder);
    void del_buy_order_when_undo(shared_ptr<Order> buyorder);
    void del_sell_order_when_undo(shared_ptr<Order> sellorder);
    void add_orders(OrderList orders);
    int get_id();
    double get_price();
    void set_start_price(double price);
    bool is_freeze();
    bool update_price_and_check_freeze();//if get frozen, return true;return false else
    void reset_freeze();
    void get_new_day();

    void debug();
};

class Transaction
{
public:
    typedef enum
    {
        BUYANDSELL,
        ADDORDER,
        DELORDER,
        FREEZE,
    } TransactionType;
    int transaction_id;
    TransactionType transaction_type;
    Transaction() = delete;
    Transaction(int transaction_id);
    virtual void undo(){return;}
    virtual ~Transaction(){};
};

class BuyAndSellTransaction : public Transaction
{
public:
    shared_ptr<Account> buyer, seller;
    int num_share;
    shared_ptr<Order> order;
    BuyAndSellTransaction(shared_ptr<Account> _buyer, shared_ptr<Account> _seller, int _num, shared_ptr<Order> order, int _id);
    virtual void undo();
    virtual ~BuyAndSellTransaction(){};
};

class AddOrderTransaction : public Transaction
{
private:
    shared_ptr<Order> order;

public:
    AddOrderTransaction(shared_ptr<Order> _order, int _id);
    virtual void undo();
    virtual ~AddOrderTransaction(){};
};

class DelOrderTransaction : public Transaction
{
public:
    shared_ptr<Order> order;
    DelOrderTransaction(shared_ptr<Order> _order, int _id);
    virtual void undo();
    virtual ~DelOrderTransaction(){};
};

class ReduceOrderTransaction :public Transaction
{
public:
    shared_ptr<Order> order;
    int reduced_share_number;

    ReduceOrderTransaction(shared_ptr<Order> _order, int _reduced_share_number, int _id);
    virtual void undo();
    virtual ~ReduceOrderTransaction(){} ;
};

class FreezeTransaction : public Transaction
{
public:
    shared_ptr<Stock> stock;
    FreezeTransaction(shared_ptr<Stock> _stock, int _id);
    virtual void undo();
    virtual ~FreezeTransaction() {};
};

class Logs
{
private:
    vector<shared_ptr<Transaction> > contents; // Operation stack
    int idx;                                  // Assign id to transactions. The same ids indicate they belong to the same transaction.

public:
    Logs() = default;
    void push_back(shared_ptr<Transaction> log);
    shared_ptr<Transaction> back();
    void pop_back();
    void clear();
    int get_id();
    int size();

    void new_id();
};  

class Account
{
    int usr_id;
    double remain;
    unordered_map<int, int> holds; // (stock_id, num_share)
    unordered_set<shared_ptr<Order> > orders;

public:
    Account() = delete;
    Account(int _id, double _remain);
    int get_id();
    void add_order(shared_ptr<Order> order);
    void del_order(shared_ptr<Order> order);
    double evaluate();
    double get_remain();
    double get_fluid();
    void add_money(double x);
    void add_share(int stock_id, int num);
    int get_share(int stock_id);

    int get_available_share(int stock_id);

    void debug();
    void display(unordered_map<int, int> &x);
    void display(unordered_set<shared_ptr<Order> > &x);
};

class Market // Please DO NOT change the interface here
{
    unordered_map<int, shared_ptr<Stock> > stocks;
    unordered_map<int, shared_ptr<Account> > accounts;
    shared_ptr<Logs> logger;
    shared_ptr<Account> market_account;

public:
    /**
     * @brief Construct a new Market object. Do something here
     *
     */
    Market();

    /**
     * @brief Begin a new day
     *
     */
    void new_day();

    /**
     * @brief Add a new user
     *
     * @param usr_id user id
     * @param remain The initial money user holds.
     */
    void add_usr(int usr_id, double remain);

    /**
     * @brief Add a new stock
     *
     * @param stock_id  stock id
     * @param num_share the number of shares initially holded by market account
     * @param price     initial price
     */
    void add_stock(int stock_id, int num_share, double price);

    /**
     * @brief Add an order under certain stock
     *
     * @param usr_id        user id
     * @param stock_id      stock id
     * @param num_share     the number of shares
     * @param price         price of order
     * @param order_type    0 for buy, 1 for sell
     * @return              true: successfully added the order, false: fail to add the order
     */
    bool add_order(int usr_id, int stock_id, int num_share, double price, bool order_type);

    /**
     * @brief Buy a stock, the rest is left as an order
     *
     * @param usr_id    user id
     * @param stock_id  stock id
     * @param num_share the number of shares
     * @param price     the desire price
     * @return          the amount of shares successfully trades, -1 for aborting.
     */
    int usr_buy(int usr_id, int stock_id, int num_share, double price); // buy a stock, return the amount of shares successfully traded, the rest is left as an order. -1 for Abort.

    /**
     * @brief Sell a stock, the rest is left as an order
     *
     * @param usr_id        user id
     * @param stock_id      stock id
     * @param num_share     the number of shares
     * @param price         the desire price
     * @return              the amount of shares successfully trades, -1 for aborting.
     */
    int usr_sell(int usr_id, int stock_id, int num_share, double price); // sell a stock, return the amount of shares successfully traded, the rest is left as an order. -1 for Abort

    /**
     * @brief the stock with highest price
     *
     * @return (stock_id, price)
     */
    pair<int, double> best_stock();

    /**
     * @brief the stock with lowest price
     *
     * @return (stock_id, price)
     */
    pair<int, double> worst_stock();

    /**
     * @brief Evaluate the value of an account
     * (how much money it could hold suppose it could successfully sell all existing selling orders, neglect the buying orders and shares not for selling yet)
     *
     * @param usr_id    user id
     * @return          the value of an account
     */
    double evaluate(int usr_id); //

    /**
     * @brief Undo the last transaction
     *
     */
    void undo();

    void debug();
};
#endif
