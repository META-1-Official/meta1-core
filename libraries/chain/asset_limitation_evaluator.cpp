#include <graphene/chain/asset_limitation_evaluator.hpp>
#include <graphene/chain/asset_limitation_object.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>

#include <functional>

namespace graphene
{
namespace chain
{
void_result asset_limitation_create_evaluator::do_evaluate(const asset_limitation_object_create_operation &op)
{
    try
    {
        database &d = db();
        auto &asset_limitation_indx = d.get_index_type<asset_limitation_index>().indices().get<by_limit_symbol>();
        auto asset_limitation_id_itr = asset_limitation_indx.find(op.limit_symbol);
        FC_ASSERT(asset_limitation_id_itr == asset_limitation_indx.end());

        //verify that limit asset create only  with meta1 account
        const auto &accounts_by_name = d.get_index_type<account_index>().indices().get<by_name>();
        auto itr = accounts_by_name.find("meta1");
        FC_ASSERT(itr != accounts_by_name.end(), "Unable to find meta1 account");
        FC_ASSERT(itr->get_id() == op.issuer, "limit asset cannot create with this account , limit asset can be created only by meta1 account");

        return void_result();
    }
    FC_CAPTURE_AND_RETHROW((op))
}

object_id_type asset_limitation_create_evaluator::do_apply(const asset_limitation_object_create_operation &op)
{
    try
    {
        database &d = db();
        auto next_asset_limitation_id = d.get_index_type<asset_limitation_index>().get_next_id();
        const asset_limitation_object &new_asset_limitation =
            d.create<asset_limitation_object>([&op, next_asset_limitation_id](asset_limitation_object &p) {
                p.issuer = op.issuer;
                p.limit_symbol = op.limit_symbol;
            });
        FC_ASSERT(new_asset_limitation.id == next_asset_limitation_id, "Unexpected object database error, object id mismatch");
        return new_asset_limitation.id;
    }
    FC_CAPTURE_AND_RETHROW((op))
}

void_result asset_limitation_update_evaluator::do_evaluate(const asset_limitation_object_update_operation &op)
{
    try
    {
        database &d = db();
        const asset_limitation_object &asset_limitation_ob = op.asset_limitation_object_to_update(d);
        asset_limitation_object_to_update = &asset_limitation_ob;

        FC_ASSERT(op.issuer == asset_limitation_ob.issuer,
                  "Incorrect issuer for limitation asset! (${o.issuer} != ${a.issuer})",
                  ("o.issuer", op.issuer)("a.issuer", asset_limitation_ob.issuer));
        //verify that limitation asset update only  with meta1 account
        const auto &accounts_by_name = d.get_index_type<account_index>().indices().get<by_name>();
        auto itr = accounts_by_name.find("meta1");
        FC_ASSERT(itr->get_id() == op.issuer, "backed asset cannot update with this account , backed asset can be updated only by meta1 account");

        return void_result();
    }
    FC_CAPTURE_AND_RETHROW((op))
}
void_result asset_limitation_update_evaluator::do_apply(const asset_limitation_object_update_operation &op)
{
    try
    {
        database &d = db();
        d.modify(*asset_limitation_object_to_update, [&op](asset_limitation_object &p) {
        });
        return void_result();
    }
    FC_CAPTURE_AND_RETHROW((op))
}

   void_result asset_price_publish_evaluator::do_evaluate(const asset_price_publish_operation &op) {
       try {
           database &d = db();

           // Non-negative price for the external asset is checked by the asset_price_publish_operation.validate()

           // Verify that limitation asset update only with specially-named accounts
           // which may or may not exist on the blockchain at the time of this evaluation
           const auto &accounts_by_name = d.get_index_type<account_index>().indices().get<by_name>();
           const account_id_type fee_payer = op.fee_paying_account;

           // A helper function to check whether the operation's fee paying account has a special name
           auto publisher_is = [&accounts_by_name, &fee_payer](const std::string &account_name ) -> bool
           {
               auto itr_name = accounts_by_name.find(account_name);
               if (itr_name == accounts_by_name.end()) {
                   return false;
               }
               bool match = itr_name->get_id() == fee_payer;
               return match;
            };

            if( d.head_block_time() < HF_ASSET_PRICE_PUBLISHERS_TIME ) {
               FC_ASSERT(publisher_is("meta1"),
                         "Asset prices can be updated only by approved accounts");

            } else {
                FC_ASSERT(publisher_is("meta1") ||
                             publisher_is("freedom") || publisher_is("peace") || publisher_is("love") ||
                             publisher_is("unity") || publisher_is("abundance") || publisher_is("victory") ||
                             publisher_is("awareness") || publisher_is("destiny") || publisher_is("strength") ||
                             publisher_is("clarity") || publisher_is("truth"),
                          "Asset prices can be updated only by approved accounts");

            }

           // Verify that the asset is an existing asset
           const auto &asset_by_symbol = d.get_index_type<asset_index>().indices().get<by_symbol>();
           auto asset_itr = asset_by_symbol.find(op.symbol);
           FC_ASSERT(asset_itr != asset_by_symbol.end(),
                     "${symbol} should exist on the blockchain", ("symbol", op.symbol));
           const asset_object &asset = *asset_itr;
           FC_ASSERT(!asset.is_market_issued(), "${symbol} must not be a market-issued asset", ("symbol", op.symbol));
           FC_ASSERT(asset.id != d.get_core_asset().id, "${symbol} must not be the core asset", ("symbol", op.symbol));

           return void_result();
       }
       FC_CAPTURE_AND_RETHROW((op))
   }

   void_result asset_price_publish_evaluator::do_apply(const asset_price_publish_operation &op) {
       try {
           database &d = db();

           const time_point_sec current_time = d.head_block_time();
           const string &asset_symbol = op.symbol;
           const auto &asset_price_idx = d.get_index_type<asset_price_index>().indices().get<by_symbol>();
           auto itr = asset_price_idx.find(asset_symbol);
           if (itr == asset_price_idx.end()) {
               // Create the asset price if it does not exist
               d.create<asset_price>([&op, &current_time](asset_price &p) {
                  p.symbol = op.symbol;
                  p.usd_price = op.usd_price;
                  p.publication_time = current_time;
               });

           } else {
               // Update the asset price, if the object already exists
               const asset_price *asset_price_to_update = &*itr;
               d.modify(*asset_price_to_update, [&op, &current_time](asset_price &p) {
                  p.usd_price = op.usd_price;
                  p.publication_time = current_time;
               });
           }

           return void_result();
       }
       FC_CAPTURE_AND_RETHROW((op))
   }

} // namespace chain
} // namespace graphene
