
#include <eosiolib/transaction.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/contract.hpp>
#include <eosiolib/crypto.h>
#include <exception>
#include <math.h>
#include <eosio.token/eosio.token.hpp>
#include "tools.hpp"
#include "marketplace.hpp"

using namespace eosio;
using namespace std;

class marketplace : public eosio::contract{

public:

    const float PERCENTAGES = 0.05;

    marketplace(account_name self):eosio::contract(self),
                                     sales(_self,_self) {}

    //@abi action
    void createsale(account_name owner, asset price, uint64_t id){

        char buffer[512];
        int actual_size = read_transaction(buffer, 512);
        datastream<const char*> ds( buffer, actual_size);
        transaction tx;
        ds >> tx;
        eosio_assert((tx.actions.end()-tx.actions.begin()) == 2, "crpwz: wrong number of actions");
        eosio_assert(  tx.actions[0].account == N(wizardmarket), "crpwz: wrong 1st recipient");
        eosio_assert(  tx.actions[0].name == N(createsale), "crpwz: wrong 1st action");

        eosio_assert(  tx.actions[1].account == N(wizardstoken), "crpwz: wrong 2st recipient");
        eosio_assert(  tx.actions[1].name == N(transfer), "crpwz: wrong 2st action");

        wizardsT fromTable(N(wizardstoken), owner);
        auto wizard = fromTable.get(id);

        auto salesIndex = sales.get_index<N(bywizard)>();
        auto sale = salesIndex.find(id);
        while (sale != salesIndex.end()&&sale->wizard==id) {           
            eosio_assert(sale->state != 1, "sale already exists");
            sale++;            
        }
        auto minPrice = asset(1, eosio::string_to_symbol(4, "EOS"));
        eosio_assert( price.is_valid(), "invalid price" );
        eosio_assert( price >= minPrice, "price must be >= 0.0001 EOS" );

        sales.emplace(owner,[&](auto &sale){
            sale.id = sales.available_primary_key();
            sale.wizard = id;
            sale.owner = owner;
            sale.price = price;
            sale.state = 1;
            sale.createdat = now();
            sale.updatedat = now();
        });
    }

    void apply(uint64_t receiver, uint64_t code, uint64_t action){       
        if (code == N(eosio.token) && action == N(transfer)) {
            auto data = unpack_action_data<token::transfer_args>();
            if (data.from != _self) {
                this->sellwizard();
            }
        }
    }

    //@abi action
    void cancelsale(uint64_t id){
        char buffer[512];
        int actual_size = read_transaction(buffer, 512);
        datastream<const char*> ds( buffer, actual_size);
        transaction tx;
        ds >> tx;
        eosio_assert((tx.actions.end()-tx.actions.begin()) >= 2 &&
                     tx.actions[0].account == N(wizardmarket) && tx.actions[0].name == N(cancelsale) &&
                     tx.actions[1].account == N(wizardstoken) && tx.actions[1].name == N(getfrombuff), "invalid transaction" );

        auto wizard =  sales.get(id);
        require_auth(wizard.owner);
        eosio_assert( wizard.state != 2, "already sold");
        sales.modify(sales.get(id),wizard.owner,[&](auto &sale){
            sale.state = 2;
            sale.updatedat = now();
        });
        action(
                permission_level{_self, N(active)},
                N(wizardstoken), N(putinbuff),
                std::make_tuple(_self, wizard.owner, wizard.wizard)
        ).send();
    }

    //@abi action
    void clsalebyuser(account_name owner) {
        require_auth(owner);
        auto salesIndex = sales.get_index<N(byowner)>();
        auto itr = salesIndex.find(owner);
        while (itr != salesIndex.end() && itr->owner==owner) {
            if (itr->state == 2)

                itr = salesIndex.erase(itr);
            else itr++;
        }
    }

    //@abi action
    void clsalebydate(uint32_t date, uint32_t num) {
        require_auth(_self);
        auto salesIndex = sales.get_index<N(bydate)>();
        auto itr = salesIndex.begin();
        auto i=0;
        while (itr != salesIndex.end() && itr->createdat <=date && i<num) {
            if (itr->state == 2) itr = salesIndex.erase(itr);
            else itr++;
            i++;
        }
    }

      //@abi action
    void clsalebyid(uint32_t id) {
        require_auth(_self);
        auto sale = sales.get(id);
        eosio_assert (2 == sale.state, "can delete only finished sales");
        sales.erase(sales.get(id));       
    }


    void sellwizard(){
        char buffer[512];
        int actual_size = read_transaction(buffer, 512);
        datastream<const char*> ds( buffer, actual_size);
        transaction tx;
        ds >> tx;

        eosio_assert((tx.actions.end()-tx.actions.begin()) >= 2 &&
                     tx.actions[0].account == N(eosio.token) && tx.actions[0].name == N(transfer) &&
                     tx.actions[1].account == N(wizardstoken) && tx.actions[1].name == N(getfrombuff), "invalid transaction" );

        auto data = unpack_action_data<token::transfer_args>();
        if (data.from != _self) {
            auto id = stoi(data.memo);
            auto wizard =  sales.get(id);

            eosio_assert( data.quantity >= wizard.price, "insufficient funds");
            eosio_assert( wizard.state != 2, "already sold");

            auto toUs = data.quantity;
            auto toSeller = data.quantity;
            toSeller.amount = (uint64_t)trunc(data.quantity.amount/1.03);
            toUs.amount = data.quantity.amount - toSeller.amount;

            print(toSeller);

            sales.modify(sales.get(id),wizard.owner,[&](auto &sale){
                sale.state = 2;
                sale.updatedat = now();
            });

            action(
                    permission_level{_self, N(active)},
                    N(eosio.token), N(transfer),
                    std::make_tuple(_self, wizard.owner, toSeller, std::string(""))
            ).send();
            action(
                    permission_level{_self, N(active)},
                    N(wizards), N(putinbuff),
                    std::make_tuple(_self, data.from, wizard.wizard)
            ).send();
        }
    }

    //sales table type indexed by price
     eosio::multi_index<N(sales), sale,
             indexed_by<N(bywizard),eosio::const_mem_fun<sale,uint64_t,&sale::wizard_index>>,
             indexed_by<N(byprice),eosio::const_mem_fun<sale,uint64_t,&sale::price_index>>,
             indexed_by<N(byowner),eosio::const_mem_fun<sale,account_name,&sale::owner_index>>,
             indexed_by<N(bydate),eosio::const_mem_fun<sale,uint64_t,&sale::date_index>>> sales;
};

EOSIO_ABI(marketplace, (cancelsale)(createsale)(clsalebyuser)(clsalebydate)(clsalebyid))
