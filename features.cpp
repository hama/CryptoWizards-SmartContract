#include "feature.hpp"
#include "wizContract.hpp"
#include "wizard.hpp"
#include <algorithm>

void letitplay_wizards::addrace(vector<uint8_t> stats) {
	require_auth(_self);
	races.emplace(_self, [&](auto &race) {
		race.num = races.available_primary_key();
		race.possible = stats;
	});
}

void letitplay_wizards::addatt(uint8_t race, uint8_t index) {
	require_auth(_self);
	auto selrace = races.find(race);
	eosio_assert(index < selrace->possible.size(), "attribute is out of range (0-7)");
	races.modify(selrace, _self, [&](auto &race) {
		race.possible[index] += 1;
	});
};

void letitplay_wizards::addrag(uint8_t part, uint32_t var, uint32_t num) {
	require_auth(_self);
	if (ragDistributions.begin() == ragDistributions.end()) {
		auto rgshop = ragsshop.begin();
		while (rgshop != ragsshop.end()) {
			ragsshop.erase(rgshop);
			rgshop++;
		}
		for (int i = 0; i < 10; i++) {
			ragDistributions.emplace(_self, [&](auto &distr) {
				distr.num = i;
				distr.possible = vector<uint32_t>();
				distr.sum = 0;
			});
			ragsshop.emplace(_self, [&](auto &shop) {
				shop.num = i;
				shop.possible = vector<uint32_t>();
				shop.bought = vector<uint32_t>();
			});
		}
	}
	eosio_assert(part < 10, "part is out of range (0-9)");
	auto ragvar = ragDistributions.find(part);
	ragDistributions.modify(ragvar, _self, [&](auto &distr) {
		distr.possible.push_back(var);
		distr.sum += var;
	});
	auto rag = ragsshop.find(part);
	ragsshop.modify(rag, _self, [&](auto &shop) {
		shop.possible.push_back(num);
		shop.bought.push_back(0);
	});
};

void letitplay_wizards::updatestate(uint8_t bgcount) {
	require_auth(_self);
	auto currst = currentState.get_or_create(_self);
	currst.bgmax += bgcount;
	currentState.set(currst, _self);
}


void letitplay_wizards::updateseed(uint32_t seedinc) {
	require_auth(N(pingsender12));
	auto currst = currentState.get_or_create(_self);
	currst.seed += seedinc;
	currentState.set(currst, _self);
}


void letitplay_wizards::setsecret(uint64_t secret1,uint64_t secret2,uint64_t secret3,uint64_t secret4,uint64_t secret5) {
	require_auth(N(pingsender12));
	auto currst = currentSecret.get_or_create(_self);
	currst.secret1 = secret1;
	currst.secret2 = secret2;
	currst.secret3 = secret3;
	currst.secret4 = secret4;
	currst.secret5 = secret5;
	currentSecret.set(currst, _self);
}

void letitplay_wizards::swap(uint8_t part, uint64_t wizid1, uint64_t wizid2, account_name owner, asset price) {
	require_auth(owner);
    wizardsT usertable(_self, owner);
    auto var = ragDistributions.get(part);

	auto wizard1 = usertable.get(wizid1).phenotype[10 + part];
	auto wizard2 = usertable.get(wizid2).phenotype[10 + part];

    auto amount1 = (uint64_t)trunc((PRICE.amount * (((double)var.sum)/var.possible[wizard1]/20)));
    auto amount2 = (uint64_t)trunc((PRICE.amount * (((double)var.sum)/var.possible[wizard2]/20)));

    auto ragprice = asset(trunc((amount1 + amount2)/20.0), eosio::string_to_symbol(4, "EOS"));

    eosio_assert(ragprice.amount <= price.amount, "transfer is not equal to price");

    usertable.modify(usertable.find(wizid1), 0, [&](auto &wiz){
        wiz.phenotype[10 + part] = wizard2;
    });
	usertable.modify(usertable.find(wizid2), 0, [&](auto &wiz){
		wiz.phenotype[10 + part] = wizard1;
	});
}

void letitplay_wizards::buy(uint8_t part, uint8_t rag, uint64_t wizid, account_name owner, asset price) {
   	wizardsT usertable(_self, owner);
    

	auto wizard = usertable.find(wizid);
    auto ragvar = ragsshop.find(part);
    auto var = ragDistributions.get(part);
   
    auto amount = calc_rag_price(var, rag);
    auto ragprice = asset(amount, eosio::string_to_symbol(4, "EOS"));
    printi(ragprice.amount);
    printi(price.amount);

    eosio_assert(price >= ragprice, "transfer is not equal to price"); 

    eosio_assert(ragvar->possible[rag] != 0, "no rags in shop");	
    if (ragvar != ragsshop.end()) {
		
        ragsshop.modify(ragvar, 0, [&](auto &variant) {
            variant.possible[rag]-=1;
            variant.bought[rag] += 1;
        });
		
        usertable.modify(wizard, 0, [&](auto &wiz){		

        	wiz.phenotype[10 + part] = rag;
        });
    }
}
