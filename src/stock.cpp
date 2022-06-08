#include "stock.h"
#include <cassert>


#include <utility>


#include <iostream>
#include <vector>
#include <memory>
#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

using namespace std;



// Feel free to change everything here.

Order::Order(StockShare _item, shared_ptr<Account> _usr, double _price, OrderType _type)
{
    item = _item, usr = _usr, order_type = _type, this->price = _price;
}

void Order::debug()
{
    printf("---------------------------------------------\nOrder debug: ");
    if (order_type == Order::BUY)
        printf("OrderType: BUY, ");
    else
        printf("OrderType: SELL, ");
    printf("User ID: %d, Stock ID: %d, NumofShare: %d, Price: %.3lf\n", usr->get_id(), this->item.first->get_id(), this->item.second, price);
    printf("\n---------------------------------------------\n");
}

void Order::add_share(int num_share) { item.second += num_share; }
void Order::reduce_share(int num_share) { item.second -= num_share; }
int Order::get_shares() { return item.second; }
double Order::get_price() { return price; }
double Order::get_total_money(){ return (item.second * price);}
shared_ptr<Account> Order::get_usr() { return usr; }
shared_ptr<Stock> Order::get_stock() { return item.first; }

Stock::Stock(int _id, shared_ptr<Logs> _logger, double price)
:stock_id(_id)
,logger(_logger)
,freeze(false)
{
    set_start_price(price);
}

int Stock::get_id() { return stock_id; }
bool Stock::is_freeze() { return freeze; }

double Stock::get_price()
{
    if (sell_orders.empty())
    {
        return 0;
    }
    else
    {
        return sell_orders.begin()->first;
    }
}

typedef set<pair<double, shared_ptr<Order> > >::iterator iter;

void Stock::add_buy_order_when_undo(shared_ptr<Order> buyorder)
{
    //shared_ptr<AddOrderTransaction> transaction = make_shared<AddOrderTransaction>(buyorder, logger->get_id());
    //logger->push_back(static_pointer_cast<Transaction>(transaction));
    buy_orders.insert(make_pair(buyorder->get_price(), buyorder));
    //buyorder->get_usr()->add_order(buyorder);
}
void Stock::add_sell_order_when_undo(shared_ptr<Order> sellorder)
{
    // TODOne
    sell_orders.insert(make_pair(sellorder->get_price(),sellorder));
    //sellorder->get_usr()->add_order(sellorder);
}
void Stock::del_buy_order_when_undo(shared_ptr<Order> buyorder)
{
    // TODOne
    buy_orders.erase(make_pair(buyorder->get_price(),buyorder));
    //if not exist, it will be modified in the account's member function
}
void Stock::del_sell_order_when_undo(shared_ptr<Order> sellorder)
{
    // TODO
    sell_orders.erase(make_pair(sellorder->get_price(),sellorder));
}

OrderResult Stock::buy(shared_ptr<Account> usr, int num_share, double price)
{
    // if somebody has posted the same order at the same price , the return -1 directively
    logger->new_id();
    shared_ptr<Order> result = nullptr;
    if (freeze)
        return make_pair(-1, result);
    if (price <= buy_orders.begin()->first)
    {
        bool exists= false;
        for (set<pair<double, shared_ptr<Order> >,more2 >::iterator it = buy_orders.begin(); it != buy_orders.end(); ++it)
        {
            if ((*it).first == price)
            {
                exists = true;
                (*it).second->add_share(num_share);
                return make_pair(0, (*it).second);
            }
        }    
    }
    
    set<pair<double, shared_ptr<Order> >,more2 >::iterator it = sell_orders.begin();
    double tmp_money = 0;
    int tmp_share = 0;
    double used_money = 0;
    
    int remaining_share = num_share;
    bool totally_bought= false;

    while (it != sell_orders.end() && (*it).first <= price  && remaining_share > 0)
    {
        
        tmp_share = (*it).second->get_shares();
        if (tmp_share < remaining_share)
        {
            remaining_share -= tmp_share;
            tmp_money = (*it).second->get_total_money();
            used_money += tmp_money;
            (*it).second->get_usr()->add_money(tmp_money);
            (*it).second->get_usr()->del_order((*it).second);
            (*it).second->get_usr()->add_share(stock_id,-tmp_share); 
            //give the seller money and  delete order(from both side) and delete their share
        
            shared_ptr<BuyAndSellTransaction> tmp1 = make_shared<BuyAndSellTransaction>(usr,(*it).second->get_usr(),tmp_share,(*it).second,logger->get_id());
            logger->push_back(tmp1);

            shared_ptr<DelOrderTransaction> tmp2 = make_shared<DelOrderTransaction>((*it).second,logger->get_id());
            logger->push_back(tmp2);
        
        }
        else
        {
            tmp_money = (remaining_share * ((*it).first ));
            used_money += tmp_money;
            
            //give the seller money and reduce order and delete their share
            (*it).second->get_usr()->add_money(tmp_money);
            (*it).second->reduce_share(remaining_share);
            (*it).second->get_usr()->add_share(stock_id,-remaining_share);
            
            shared_ptr<Order> tmp_order =  make_shared<Order>(make_pair(make_shared<Stock>(*this),remaining_share),(*it).second->get_usr(),(*it).first ,Order::BUY);
            shared_ptr<BuyAndSellTransaction> tmp1 = make_shared<BuyAndSellTransaction>(usr,(*it).second->get_usr(),remaining_share,tmp_order,logger->get_id());
            logger->push_back(tmp1);
            //only buy and sell transaction is not enough for we use order -reduce_share here
            
            shared_ptr<ReduceOrderTransaction> tmp2 = make_shared<ReduceOrderTransaction>((*it).second,remaining_share,logger->get_id());
            logger->push_back(tmp2);
            
            
            remaining_share = 0;
            totally_bought = true;
            break;
        }

        ++it;
    }
    int got_share = num_share - remaining_share; 

    //delete the sell_orders //if totally bought, the ++it will implemented once more
    sell_orders.erase(sell_orders.begin(), it);

    usr->add_money( -used_money );
    usr->add_share(stock_id ,got_share);
    if (!totally_bought)
    { 
        shared_ptr<Order> new_order = make_shared<Order>(make_pair(make_shared<Stock>(*this),remaining_share) , usr,price,Order::BUY);
        buy_orders.insert(make_pair(price,new_order));
        usr->add_order(new_order);

        shared_ptr<AddOrderTransaction> tmp3 = make_shared<AddOrderTransaction>(new_order, logger->get_id());
        logger->push_back(tmp3);

        update_price_and_check_freeze();
        return make_pair(got_share,new_order);
    }
    // get  money  from the buyer and post the order
    update_price_and_check_freeze();
    return make_pair(got_share,result);
}

OrderResult Stock::sell(shared_ptr<Account> usr, int num_share, double price)
{
    logger->new_id();
    shared_ptr<Order> result = nullptr;
    if (freeze)
        return make_pair(-1, result);
    //done
    
    if (price >= sell_orders.begin()->first )
    {
        for (set<pair<double, shared_ptr<Order> >,less2 >::iterator it = sell_orders.begin(); it != sell_orders.end(); ++it)
        {
            if ((*it).first == price)
            {
                (*it).second->add_share(num_share);
                return make_pair(0, (*it).second);
            }
        }
    } //they said that there will not appear such cases but in case?
    set<pair<double, shared_ptr<Order> >,more2 >::iterator it = buy_orders.begin();
    double tmp_money = 0;
    int tmp_share = 0;
    double new_got_money = 0;
    
    int remaining_share = num_share;
    bool totally_sold= false;

    while (it != buy_orders.end() && (*it).first >= price  && remaining_share > 0)
    {
        
        tmp_share = (*it).second->get_shares();
        if (tmp_share < remaining_share)
        {
            remaining_share -= tmp_share;
            tmp_money = (*it).second->get_total_money();
            new_got_money += tmp_money;
            (*it).second->get_usr()->add_money(-tmp_money);
            (*it).second->get_usr()->del_order((*it).second);
            (*it).second->get_usr()->add_share(stock_id, tmp_share);
            
            //get money from the buyer  and  delete order at the buyer side and give them shares
            //sell and buy transaction and del order transaction
            shared_ptr<BuyAndSellTransaction> tmp1 = make_shared<BuyAndSellTransaction>((*it).second->get_usr(),usr,tmp_share,(*it).second,logger->get_id());
            logger->push_back(tmp1);

            shared_ptr<DelOrderTransaction> tmp2 = make_shared<DelOrderTransaction>((*it).second,logger->get_id());
            logger->push_back(tmp2);

        }
        else
        {
            tmp_money = (remaining_share * ((*it).first ));
            new_got_money += tmp_money;
            
            //get money from the buyer  and  delete order at the buyer side and give them shares
            (*it).second->get_usr()->add_money(-tmp_money);
            (*it).second->reduce_share(remaining_share);
            (*it).second->get_usr()->add_share(stock_id,remaining_share);
            
            shared_ptr<Order> tmp_order =  make_shared<Order>(make_pair(make_shared<Stock>(*this),remaining_share),(*it).second->get_usr(),(*it).first ,Order::SELL);
            shared_ptr<BuyAndSellTransaction> tmp1 = make_shared<BuyAndSellTransaction>((*it).second->get_usr(),usr,remaining_share,tmp_order,logger->get_id());
            logger->push_back(tmp1);
            //only buy and sell transaction is not enough for we use order -reduce_share here
            
            shared_ptr<ReduceOrderTransaction> tmp2 = make_shared<ReduceOrderTransaction>((*it).second,remaining_share,logger->get_id());
            logger->push_back(tmp2);

            remaining_share = 0;
            totally_sold = true;
            
            break;
        }

        ++it;
    }
    int sold_share = num_share - remaining_share; 

    buy_orders.erase(buy_orders.begin(), it);

    usr->add_money( new_got_money );
    usr->add_share(stock_id ,-sold_share);
    if (!totally_sold)
    { 
        shared_ptr<Order> new_order = make_shared<Order>(make_pair(make_shared<Stock>(*this),remaining_share) , usr,price,Order::SELL);
        sell_orders.insert(make_pair(price,new_order));
        usr->add_order(new_order);

        //add an addOrder transaction
        shared_ptr<AddOrderTransaction> tmp3 = make_shared<AddOrderTransaction>(new_order, logger->get_id());
        logger->push_back(tmp3);

        update_price_and_check_freeze();
        
        return make_pair(sold_share,new_order);
    }
    // get  money  from the buyer and post the order
    update_price_and_check_freeze();
    return make_pair(sold_share, result);

}
void Stock::set_start_price(double price)
{
    st_price_now = price;
    st_today_price = price;
}

bool Stock::update_price_and_check_freeze()
{
    st_price_now = sell_orders.begin()->first;
    if (st_price_now <0.9*st_today_price || st_price_now >1.1*st_today_price )
    {
        freeze = true;
        shared_ptr<FreezeTransaction> tmp = make_shared<FreezeTransaction>(make_shared<Stock>(*this),logger->get_id());
        logger->push_back(tmp);
    }
    return freeze;
}
void Stock::reset_freeze()
{
    freeze = false;
    return;
}

void Stock::get_new_day()
{
    freeze = false;
    st_today_price = st_price_now;
    return;
}

Transaction::Transaction(int transaction_id)
{
    this->transaction_id = transaction_id;
}
/*
Transaction::~Transaction()
{
}
BuyAndSellTransaction::~BuyAndSellTransaction()
{
}
AddOrderTransaction::~AddOrderTransaction()
{
}
DelOrderTransaction::~DelOrderTransaction()
{
}
ReduceOrderTransaction::~ReduceOrderTransaction()
{
}
FreezeTransaction::~FreezeTransaction()
{
}*/

BuyAndSellTransaction::BuyAndSellTransaction(shared_ptr<Account> _buyer, shared_ptr<Account> _seller, int _num, shared_ptr<Order> _order, int _id) : Transaction(_id)
{
    this->buyer = _buyer, this->seller = _seller;
    this->num_share = _num, this->order = _order;
    this->transaction_type = Transaction::BUYANDSELL;
}
void BuyAndSellTransaction::undo()
{
    double money = order->get_total_money();
    buyer->add_money(money);
    buyer->add_share(order->get_stock()->get_id(), -num_share);
    seller->add_money(-money);
    seller->add_share(order->get_stock()->get_id(), num_share);
}
AddOrderTransaction::AddOrderTransaction(shared_ptr<Order> _order, int _id) : Transaction(_id)
{
    this->order = _order;
    this->transaction_type = Transaction::ADDORDER;
}
void AddOrderTransaction::undo()
{
    // TODOne
    shared_ptr<Account> user = order->get_usr();
    shared_ptr<Stock> add_to = order->get_stock();
    Order::OrderType type = order->order_type;

    user->del_order(order);
    if (type == Order::BUY)
    {
        add_to->del_buy_order_when_undo(order);
    }
    else if (type == Order::SELL)
    {
        add_to->del_sell_order_when_undo(order);
    }
    return;
}

DelOrderTransaction::DelOrderTransaction(shared_ptr<Order> _order, int _id) : Transaction(_id)
{
    this->order = _order;
    this->transaction_type = Transaction::DELORDER;
}
void DelOrderTransaction::undo()
{
    // TODOne
    shared_ptr<Account> user = order->get_usr();
    shared_ptr<Stock> add_to = order->get_stock();
    Order::OrderType type = order->order_type;

    user->add_order(order);
    if (type == Order::BUY)
    {
        add_to->add_buy_order_when_undo(order);
    }
    else if (type == Order::SELL)
    {
        add_to->add_sell_order_when_undo(order);
    }
    return;

}

ReduceOrderTransaction::ReduceOrderTransaction(shared_ptr<Order> _order, int _reduced_share_number, int _id)
:Transaction(_id)
,order(_order)
,reduced_share_number(_reduced_share_number)
{
}

void ReduceOrderTransaction::undo()
{
    order->add_share(reduced_share_number);
    return;
}



FreezeTransaction::FreezeTransaction(shared_ptr<Stock> _stock, int _id) : Transaction(_id)
{
    this->stock = _stock;
    this->transaction_type = Transaction::FREEZE;
}
void FreezeTransaction::undo()
{
    // TODOne
    stock->reset_freeze();
    return;
}

int Account::get_id() { return usr_id; }

Account::Account(int _id, double _remain)
:usr_id(_id)
,remain(_remain)
{
    // TODOne
}
void Account::add_order(shared_ptr<Order> order)
{
    orders.insert(order);
}
void Account::del_order(shared_ptr<Order> order)
{
    int k = orders.erase(order);
    if ( k == 0)
    {
        unordered_set<shared_ptr<Order> >::iterator it = orders.begin();
        while (it != orders.end())
        {
            if ((*it)->order_type == order->order_type && (*it)->get_price() == order->get_price())
            {
                (*it)->add_share(- order->get_shares());
                break;
            }
            ++it;
        }
    }
}
int Account::get_share(int stock_id)
{
    unordered_map<int,int>::iterator it = holds.find(stock_id);
    if (it == holds.end())
        return 0;
    else
        return it->second;
}
double Account::get_remain() { return remain; }
double Account::get_fluid()
{
    // TODOne
    double planned_money = 0;
    unordered_set<shared_ptr<Order> >::iterator it = orders.begin();
    while ( it != orders.end())
    {
        if ((*it)->order_type == Order::BUY)
        {
            planned_money += (*it)->get_total_money();
        }
        ++it;
    }
    return remain - planned_money;
}
void Account::add_money(double x) { remain += x; }
void Account::add_share(int stock_id, int num)
{
    unordered_map<int,int>::const_iterator got = holds.find(stock_id);
    if (got == holds.end() )
    {
        holds[stock_id] = num;
    }
    else
    {
        holds[stock_id] += num; 
    }        
}
double Account::evaluate()
{
    // TODOne
    double expected_money = remain;
    unordered_set<shared_ptr<Order> >::iterator it = orders.begin();
    while ( it != orders.end())
    {
        if ((*it)->order_type == Order::SELL)
        {
            expected_money += (*it)->get_total_money();
        }
        ++it;
    }
    return expected_money;
}

int Account::get_available_share(int stock_id)
{
    int available_share = get_share(stock_id);
    unordered_set<shared_ptr<Order> >::iterator it = orders.begin();
    while(it != orders.end())
    {
        if ((*it)->order_type == Order::SELL && (*it)->get_stock()->get_id() == stock_id)
        {
            available_share -= (*it)->get_shares();
        }
        ++it;
    }
    return available_share;
}


void Logs::push_back(shared_ptr<Transaction> log) { contents.push_back(log); }
shared_ptr<Transaction> Logs::back() { return contents.back(); }
void Logs::pop_back() { contents.pop_back(); }
int Logs::get_id() { return idx; }
void Logs::new_id(){ idx++ ;}
void Logs::clear()
{
    idx = 0;
    contents.clear();
}
int Logs::size()
{
    return contents.size();
}

Market::Market()
{
    // TODOne


    logger = make_shared<Logs>();
    market_account = make_shared<Account>(0, 0);

    //accounts = unordered_map<int, shared_ptr<Account> >();
    accounts.insert(pair<int, shared_ptr<Account> >(0, market_account));
    //accounts.insert({{0,market_account} , {1 , make_shared<Account>(1,1)} });

}

void Market::new_day()
{
    // TODOne
    unordered_map<int, shared_ptr<Stock> >::iterator it = stocks.begin();
    while (it != stocks.end())
    {
        (*it).second->get_new_day();
        ++it;
    }
    logger->clear();
    return;
}

void Market::add_usr(int usr_id, double remain)
{
    // TODOne
    accounts.insert(pair<int, shared_ptr<Account> >(usr_id, make_shared<Account>(usr_id, remain)));
    return;
}

void Market::add_stock(int stock_id, int num_share, double price)
{
    // TODOne
    shared_ptr<Stock> new_stock  = make_shared<Stock>(stock_id , logger,price);
    //new_stock->set_start_price(price);
    stocks.insert (make_pair(stock_id, new_stock)   );
    market_account->add_share(stock_id, num_share);
    //market_account->add_order(make_shared<Order>(,market_account,price,));
    
    usr_sell(0,stock_id,num_share,price);
    return;
}

bool Market::add_order(int usr_id, int stock_id, int num_share, double price, bool order_type) // retrun success or not
// if order_type is true, add a sell order 
{
    // TODOne
    /*
    shared_ptr<Stock> stock_to = stocks[stock_id];
    shared_ptr<Account> op_account = accounts[usr_id];
    if (order_type)
    {
        shared_ptr<Order> new_order = make_shared<Order>(StockShare(stock_to,num_share),usr_id,price,Order::BUY);
        stock_to->add_buy_order(new_order);
        
        op_account->add_order(new_order);
        shared_ptr<AddOrderTransaction> transc= make_shared<AddOrderTransaction>(new_order,logger->get_id());
        //logger->push_back(static_pointer_cast<Transaction>(transc));
        logger->push_back(transc);
    }
    else
    {
        shared_ptr<Order> new_order = make_shared<Order>(StockShare(stock_to,num_share),usr_id,price,Order::SELL);
        stock_to->add_sell_order(new_order);
        
        op_account->add_order(new_order);
        shared_ptr<AddOrderTransaction> transc= make_shared<AddOrderTransaction>(new_order,logger->get_id());
        //logger->push_back(static_pointer_cast<Transaction>(transc));
        logger->push_back(transc);
    }
    return true;*/
    int result = -1;
    if (order_type)
    {
        result =  usr_sell(usr_id,stock_id,num_share,price);
    }
    else
    {
        result =  usr_buy(usr_id,stock_id,num_share,price);
    }
    if (result == -1)
    {
        return false;
    }
    else
    {
        return true;
    }
}

double Market::evaluate(int usr_id)
{
    return accounts[usr_id]->evaluate();
}

int Market::usr_buy(int usr_id, int stock_id, int num_share, double price)
{
    // TODOne
    shared_ptr<Account> user = accounts[usr_id];
    double fluid = user->get_fluid();
    if ((fluid - num_share*price ) < float_eps)
    {
        return -1;
    }
    shared_ptr<Stock> to_stock = stocks[stock_id];
    OrderResult result =  to_stock->buy(user,num_share,price);
    return result.first;
}

int Market::usr_sell(int usr_id, int stock_id, int num_share, double price)
{
    // TODOne
    shared_ptr<Account> user = accounts[usr_id];
    int available_share = user->get_available_share(stock_id);
    if (available_share < num_share)
    {
        return -1;
    }
    shared_ptr<Stock> to_stock = stocks[stock_id];
    OrderResult result =  to_stock->sell(user,num_share,price);
    return result.first;
}

pair<int, double> Market::best_stock()
{
    // TODOne
    unordered_map<int, shared_ptr<Stock> >::iterator it = stocks.begin();
    pair<int, double> result = make_pair((*it).first,(*it).second->get_price() );
    while (it != stocks.end())
    {
        if ((*it).second->get_price() >  result.second)
        {
            result = make_pair((*it).first,(*it).second->get_price() );
        }
        ++it;
    }
    return result;
}

pair<int, double> Market::worst_stock()
{
    // TODOne
    unordered_map<int, shared_ptr<Stock> >::iterator it = stocks.begin();
    pair<int, double> result = make_pair((*it).first,(*it).second->get_price() );
    while (it != stocks.end())
    {
        if ((*it).second->get_price() <  result.second)
        {
            result = make_pair((*it).first,(*it).second->get_price() );
        }
        ++it;
    }
    return result;
}

void Market::undo()
{
    int _id = logger->back()->transaction_id;
    while (logger->size())
    {
        shared_ptr<Transaction> transaction = logger->back();
        if (transaction->transaction_id != _id)
            break;
        transaction->undo();
        logger->pop_back();
    }
    return;
}


void Stock::debug()
{
    cout << "------------debug Stock------------------ "<< endl;
    cout << "stock_id is "<<stock_id <<'\n';
    cout <<" freeze is "<< freeze<< '\n';
    cout << "price now is "<< st_price_now<<'\n';
    cout << "price today is "<< st_today_price<<'\n';
    cout << "buy orders :"<<endl;
    display(buy_orders);
    cout << "sell orders : "<<endl;
    display(sell_orders);
    cout << "----------------------------------------- "<<endl;
}

void display(set<pair<double, shared_ptr<Order> > ,less2> &x)
{
    cout << "display the set \n";
    set<pair<double, shared_ptr<Order> > >::iterator it = x.begin();
    while (it != x.cend())
    {
        cout<<(*it).first << ' ' << endl << "second : \n";
        (*it).second->debug();
        ++it;
    }
}
void display(set<pair<double, shared_ptr<Order> > ,more2> &x)
{
    cout << "display the set \n";
    set<pair<double, shared_ptr<Order> > >::iterator it = x.begin();
    while (it != x.cend())
    {
        cout<<(*it).first << ' ' << endl << "second : \n";
        (*it).second->debug();
        ++it;
    }
}

void Account::debug()
{
    cout<<"id is "<< usr_id<<"\nremain is "<<remain<<"\n";
    display(holds);
    display(orders);
}
void Account::display(unordered_map<int, int> &x)
{
    cout << "display the holds \n";
    unordered_map<int, int>::iterator it = x.begin();
    while (it != x.cend())
    {
        cout<<'<'<<(*it).first << " , " <<  (*it).second<<">\n";
    
        ++it;
    }
}
void Account::display(unordered_set<shared_ptr<Order> > &x)
{
    cout << "display the orders \n";
    unordered_set<shared_ptr<Order> >::iterator it = x.begin();
    while (it != x.cend())
    {
        (*it)->debug();
        ++it;
    }
}
void Market::debug()
{
    unordered_map<int, shared_ptr<Stock> >::iterator it1=stocks.begin();
    cout<<"stocks"<<endl;
    while (it1 != stocks.end())
    {
        
        it1->second->debug();
        ++it1;
    }
    unordered_map<int, shared_ptr<Account> >::iterator it2 = accounts.begin();
    cout<<"accounts"<<endl;
    while (it2 != accounts.end())
    {
        it2->second->debug();
        ++it2;

    }
}
