#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/privileged.hpp>
#include <vector>
#include <algorithm>

using namespace eosio;
using namespace std;

class [[eosio::contract("dappmetadata")]] metadata : public contract {

private:

	struct [[eosio::table, eosio::contract("eosio.system")]] producer_info {
		name                  owner;
		double                total_votes = 0;
		uint64_t primary_key() const { return owner.value; }
	};
	typedef eosio::multi_index<"producers"_n,producer_info> producers_table;

	struct [[eosio::table, eosio::contract("eosio.system")]] eosio_global_state : eosio::blockchain_parameters {
		uint64_t free_ram() const { return max_ram_size - total_ram_bytes_reserved; }
		uint64_t             max_ram_size;
		uint64_t             total_ram_bytes_reserved;
		int64_t              total_ram_stake;
		block_timestamp      last_producer_schedule_update;
		time_point           last_pervote_bucket_fill;
		int64_t              pervote_bucket;
		int64_t              perblock_bucket;
		uint32_t             total_unpaid_blocks;
		int64_t              total_activated_stake;
		time_point           thresh_activated_stake_time;
		uint16_t             last_producer_schedule_size;
		double               total_producer_vote_weight;
		block_timestamp      last_name_close;
	};
	typedef eosio::singleton<"global"_n, eosio_global_state> gs_singleton;

	struct [[eosio::table]] resources {
		string ram_payer;
		uint32_t avg_ram_usage;
		uint32_t avg_cpu_us;
		uint32_t avg_net;
	};

	struct [[eosio::table]] act {
		capi_name action_name;
		string short_desc;
		string long_desc;
		resources resources;
	};

	struct [[eosio::table]] initact {
		capi_name action_name;
		string short_desc;
		string long_desc;
		string ram_payer;
	};

	struct [[eosio::table]] insertdapp {
		name account;
		string title;
		string description;
		string source_code;
		string website;
		string logo;
		vector<string> tags;
		vector<initact> actions;
		EOSLIB_SERIALIZE(insertdapp,(account)(title)(description)(source_code)(website)(logo)(tags)(actions))
	};

	struct [[eosio::table]] dapp {
		name account;
		string title;
		string description;
		string source_code;
		string website;
		string logo;
		vector<string> tags;
		capi_checksum256 code_hash;
		capi_name last_validator;
		vector<act> actions;
		uint64_t updated_on;
		uint64_t primary_key() const { return account.value; }
		EOSLIB_SERIALIZE(dapp,(account)(title)(description)(source_code)(website)(logo)(tags)(code_hash)(last_validator)(actions)(updated_on))
	};
	typedef multi_index<"dapps"_n,dapp> dapps_table;

	struct [[eosio::table]] validator {
		capi_name account;
		string url;
		double weight;
		vector<capi_name> approved_by;
		uint64_t primary_key() const { return account; }
		EOSLIB_SERIALIZE(validator,(account)(url)(weight)(approved_by))
	};
	typedef multi_index<"validators"_n,validator> validators_table;

	struct [[eosio::table]] request {
		uint64_t id;
		capi_name payer;
		capi_name contract;
		capi_name validator;
		double min_rep;
		asset bounty;
		bool accepted;
		uint64_t accepted_on;
		uint64_t expiration;
		uint64_t primary_key() const { return id; }
		EOSLIB_SERIALIZE(request,(id)(payer)(contract)(validator)(min_rep)(bounty)(accepted)(accepted_on)(expiration))
	};
	typedef multi_index<"requests"_n,request> requests_table;

	struct [[eosio::table]] validation {
		capi_name validator;
		uint64_t version;
		capi_checksum256 code_hash;
		uint64_t timestamp;
		string tag;
		string notes;
		uint64_t approvals;
		uint64_t primary_key() const { return validator; }
		EOSLIB_SERIALIZE(validation,(validator)(version)(code_hash)(timestamp)(tag)(notes)(approvals))
	};
	typedef multi_index<"validations"_n,validation> validations_table;

	struct [[eosio::table]] receipt {
		capi_name owner;
		asset amount;
		uint64_t primary_key() const { return owner; }
		EOSLIB_SERIALIZE(receipt,(owner)(amount))
	};
	typedef multi_index<"receipts"_n,receipt> receipts_table;

	validators_table _validators;
	dapps_table _dapps;
	producers_table _producers;

public:

	using contract::contract;

	metadata(name s, name code, datastream<const char*> ds)
	:contract(s,code,ds),
	_validators(_self,_self.value),
	_dapps(_self,_self.value),
	_producers("eosio"_n,"eosio"_n.value)
	{}

	double sum_votes(vector<capi_name> vec) {
		double sum = 0;
		gs_singleton _global("eosio"_n,"eosio"_n.value);
		auto gs = _global.get();
		auto total_votes = gs.total_producer_vote_weight;
		for (capi_name & p : vec) {
			auto tp = _producers.find(p);
			sum += tp->total_votes;
		}
		return sum / (total_votes);
	}

	[[eosio::action]]
	void regvalidator(capi_name owner, string url) {
		require_auth(owner);
		auto itr = _validators.find(owner);
		eosio_assert(itr == _validators.end(),"Account already added!");
		_validators.emplace(name(owner), [&](validator& o) {
			o.account = owner;
			o.url = url;
		});
	}

	[[eosio::action]]
	void rmvalidator(capi_name owner) {
		require_auth(owner);
		auto itr = _validators.find(owner);
		eosio_assert(itr != _validators.end(),"Account not found");
		_validators.erase(itr);
	}

	[[eosio::action]]
	void update(name contract, capi_name action, uint64_t net, uint64_t cpu, uint64_t ram) {
		require_auth(_self);
		auto itr = _dapps.find(contract.value);
		eosio_assert(itr != _dapps.end(),"contract not found");
		_dapps.modify(itr, contract, [&](dapp& o) {
			for (auto & a : o.actions) {
				if(action == a.action_name) {
					a.resources.avg_net = net;
					a.resources.avg_cpu_us = cpu;
					a.resources.avg_ram_usage = ram;
				}
			}
		});
	}

	[[eosio::action]]
	void validate(capi_name validator, name contract, capi_checksum256 code_hash, string tag, string notes, int64_t id) {
		require_auth(validator);
		eosio_assert(is_account(contract.value),"Invalid account");
		auto index = _dapps.find(contract.value);
		eosio_assert(index != _dapps.end(),"contract not found");

		// use -1 to bypass
		if(id >= 0) {
			
			requests_table _requests(_self,contract.value);
			auto req = _requests.find(uint64_t(id));
			eosio_assert(req != _requests.end(),"request not found");
			auto ft = (req->expiration) + (req->accepted_on);
			eosio_assert( ft > current_time(), "request has expired");

			// pay bounty to the validator
			string memo = "contract validation bounty";

			action(
				permission_level{ _self, "active"_n },
				name("eosio.token"),
				name("transfer"),
				make_tuple(_self.value, req->validator, req->bounty, memo)
				).send();

			_requests.erase(req);

		}

		validations_table _validations(_self,contract.value);
		auto itr = _validations.find(validator);
		if(itr != _validations.end()) {
			_validations.modify(itr, name(validator), [&](validation& o) {
				o.version += 1;
				o.code_hash = code_hash;
				o.tag = tag;
				o.notes = notes;
				o.approvals = 0;
				o.timestamp = current_time();
			});
		} else {
			_validations.emplace(name(validator), [&](validation& o) {
				o.validator = validator;
				o.version = 1;
				o.code_hash = code_hash;
				o.tag = tag;
				o.notes = notes;
				o.approvals = 0;
				o.timestamp = current_time();
			});
		}
	}

	[[eosio::action]]	
	void rmvalidation(capi_name validator, name contract) {
		require_auth(validator);
		eosio_assert(is_account(validator),"Invalid account");
		eosio_assert(is_account(contract.value),"Invalid account");
		validations_table _validations(_self,contract.value);
		auto itr = _validations.find(validator);
		eosio_assert(itr != _validations.end(),"entry not found");
		_validations.erase(itr);
	}

	[[eosio::action]]
	void approve(capi_name producer, capi_name val_account) {

		// require signature from block producer
		require_auth(producer);

		// check producer reg
		auto prod = _producers.find(producer);
		eosio_assert(prod != _producers.end(),"Producer not registered");

		auto itr = _validators.find(val_account);
		eosio_assert(itr != _validators.end(),"validator not found");

		auto vecit = find(itr->approved_by.begin(), itr->approved_by.end(), producer);
		eosio_assert(vecit == itr->approved_by.end(),"already approved");

		_validators.modify(itr, _self, [&](validator& o) {
			// include approval
			o.approved_by.push_back(producer);
			// update votes
			o.weight = sum_votes(o.approved_by);
		});
	}

	[[eosio::action]]
	void unapprove(capi_name producer, capi_name val_account) {

		// require signature from block producer
		require_auth(producer);

		// check producer reg
		auto prod = _producers.find(producer);
		eosio_assert(prod != _producers.end(),"Producer not registered");

		auto itr = _validators.find(val_account);
		eosio_assert(itr != _validators.end(),"validator not found");

		auto vecit = find(itr->approved_by.begin(), itr->approved_by.end(), producer);
		eosio_assert(vecit != itr->approved_by.end(),"did not approve yet");

		_validators.modify(itr, _self, [&](validator& o) {
			// remove approval entry
			o.approved_by.erase(vecit);
			// update votes
			o.weight = sum_votes(o.approved_by);
		});
	}

	[[eosio::action]]
	void addrequest(capi_name payer, capi_name contract, capi_name val_account, asset bounty, uint64_t expiration, double min_rep) {

		require_auth(payer);

		// check receipt balance
		receipts_table _receipts(_self,_self.value);
		auto p = _receipts.find(payer);
		eosio_assert(p != _receipts.end(),"payer not found, please make a deposit first");
		eosio_assert(p->amount >= bounty,"not enough credits");

		_receipts.modify(p, _self, [&](receipt& o) {
			o.amount -= bounty;
		});

		requests_table _requests(_self,contract);
		_requests.emplace(name(payer), [&](request& o) {
			o.id = _requests.available_primary_key();
			o.payer = payer;
			o.contract = contract;
			o.validator = val_account;
			o.accepted = false;
			o.expiration = expiration;
			o.min_rep = min_rep;
			o.bounty = bounty;
		});
	}

	[[eosio::action]]
	void cancelreq(capi_name payer, capi_name contract, uint64_t id) {
		require_auth(payer);
		requests_table _requests(_self,contract);
		receipts_table _receipts(_self,_self.value);
		auto req = _requests.find(id);
		eosio_assert(req != _requests.end(),"request not found");
		if (req->accepted == true) {
			auto ft = (req->expiration) + (req->accepted_on);
			eosio_assert( ft < current_time(), "request is still active");
		}
		
		// refund payer
		auto p = _receipts.find(req->payer);
		_receipts.modify(p, _self, [&](receipt& o) {
			o.amount += req->bounty;
		});

		// delete request
		_requests.erase(req);
	}

	[[eosio::action]]
	void refund(capi_name payer, asset quantity) {
		receipts_table _receipts(_self,_self.value);
		auto p = _receipts.find(payer);
		eosio_assert(p != _receipts.end(), "balance not found");
		eosio_assert(p->amount >= quantity, "not enough balance");

		if(p->amount == quantity) {
			_receipts.erase(p);
		} else {
			_receipts.modify(p, _self, [&](receipt& o) {
				o.amount -= quantity;
			});
		}

		string memo = "dappmetadata refund";

		action(
			permission_level{ _self, "active"_n },
			name("eosio.token"),
			name("transfer"),
			make_tuple(_self.value, payer, quantity, memo)
			).send();
	}

	[[eosio::action]]
	void acceptreq(capi_name val_account, capi_name contract, uint64_t id) {
		require_auth(val_account);
		auto itr = _validators.find(val_account);
		eosio_assert(itr != _validators.end(),"validator not found");

		requests_table _requests(_self,contract);
		auto req = _requests.find(id);
		eosio_assert(req != _requests.end(),"request not found");
		
		// check validator, if assigned
		if(req->validator != ""_n.value) {
			eosio_assert(req->validator == val_account, "validator do not match");
		}
		
		// update votes
		_validators.modify(itr, _self, [&](validator& o) {
			o.weight = sum_votes(o.approved_by);
		});
		// check minimum reputation
		eosio_assert(itr->weight >= req->min_rep,"not enough reputation to accept");

		_requests.modify(req, name(val_account), [&](request& o){
			o.accepted = true;
			o.accepted_on = current_time();
		});
	}


	[[eosio::action]]
	void insert( insertdapp data ) {
		require_auth( data.account );
		eosio_assert(is_account(data.account),"Invalid account");
		// Check string sizes
		eosio_assert(data.title.length() <= 32, "Title too long");
		eosio_assert(data.description.length() <= 1024, "Description too long");
		eosio_assert(data.source_code.length() <= 128, "Source code url too long");
		eosio_assert(data.website.length() <= 128, "Website url too long");
		eosio_assert(data.logo.length() <= 128, "Logo url too long");
		// Check tags
		eosio_assert(data.tags.size() <= 10, "Cannot add more than 10 tags");
		for (auto & tag : data.tags) {
			eosio_assert(tag.length() <= 16, "Tag must have less than 16 chars");
		}
		// Check actions
		vector<string> ram_payer_opts = {"user","contract","both"};
		for (auto & action : data.actions) {
			// Check ram payer types
			eosio_assert(any_of(ram_payer_opts.begin(), ram_payer_opts.end(), [&](const string & str){
				return str == action.ram_payer;
			}),"Invalid ram payer");
			// Check descriptions
			eosio_assert(action.short_desc.length() <= 256, "Short description too long");
			eosio_assert(action.long_desc.length() <= 512, "Long description too long");
		}

		auto itr = _dapps.find(data.account.value);
		if(itr != _dapps.end()) {
			// Update entry
			_dapps.modify(itr, data.account, [&](dapp& o) {
				o.title = data.title;
				o.description = data.description;
				o.source_code = data.source_code;
				o.website = data.website;
				o.logo = data.logo;
				o.tags = data.tags;
				o.updated_on = current_time();
				auto temp = o.actions;
				o.actions.clear();
				for (auto & action : data.actions) {
					resources rec = resources{
						.ram_payer = action.ram_payer,
						.avg_net = 0,
						.avg_cpu_us = 0,
						.avg_ram_usage = 0
					};
					for (auto & tempact : temp) {
						if(tempact.action_name == action.action_name) {
							rec.avg_net = tempact.resources.avg_net;
							rec.avg_cpu_us = tempact.resources.avg_cpu_us;
							rec.avg_ram_usage = tempact.resources.avg_ram_usage;
						}
					}
					o.actions.push_back({
						.action_name = action.action_name,
						.short_desc = action.short_desc,
						.long_desc = action.long_desc,
						.resources = rec
					});
				}
			});
		} else {
			// Add new entry
			_dapps.emplace(data.account, [&](dapp& o) {
				o.account = data.account;
				o.title = data.title;
				o.description = data.description;
				o.source_code = data.source_code;
				o.website = data.website;
				o.logo = data.logo;
				o.tags = data.tags;
				o.last_validator = 0;
				o.updated_on = current_time();
				for (auto & action : data.actions) {
					o.actions.push_back({
						.action_name = action.action_name,
						.short_desc = action.short_desc,
						.long_desc = action.long_desc,
						.resources = {
							.ram_payer = action.ram_payer,
							.avg_net = 0,
							.avg_cpu_us = 0,
							.avg_ram_usage = 0
						}
					});
				}
			});
		}
	}

	void received_token(const name& from, const name& to, const asset& quantity, const string& memo) {
		if(to == "dappmetadata"_n) {
			receipts_table _receipts(_self,_self.value);
			auto itr = _receipts.find(from.value);
			if(itr != _receipts.end()) {
				_receipts.modify(itr, _self, [&](receipt& o) {
					o.amount += quantity;
				});
			} else {
				_receipts.emplace(_self, [&](receipt& o){
					o.amount = quantity;
					o.owner = from.value;
				});
			}
		}
	}
};

extern "C" {
	[[noreturn]] void apply(uint64_t receiver, uint64_t code, uint64_t action) {
		if (action == "transfer"_n.value and code == "eosio.token"_n.value) {
			execute_action<metadata>(name(receiver),name(code),&metadata::received_token);
		}
		if(code == receiver) {
			switch(action) {
				EOSIO_DISPATCH_HELPER(metadata,
					(regvalidator)
					(rmvalidator)
					(addrequest)
					(cancelreq)
					(acceptreq)
					(refund)
					(update)
					(validate)
					(rmvalidation)
					(approve)
					(unapprove)
					(insert))
			};
		}
		eosio_exit(0);
	}
}