#include <graphene/chain/property_evaluator.hpp>
#include <graphene/chain/property_object.hpp>
#include <graphene/chain/asset_limitation_object.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/asset_limitation_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/is_authorized_asset.hpp>

#include <functional>

namespace graphene
{
namespace chain
{
void_result property_create_evaluator::do_evaluate(const property_create_operation &op)
{
    try
    {
        database &d = db();

        auto &property_indx = d.get_index_type<property_index>().indices().get<by_property_id>();
        auto property_id_itr = property_indx.find(op.property_id);
        FC_ASSERT(property_id_itr == property_indx.end());
        op.validate();

        //verify that asset_limitation_object exists for backed_by_asset_symbol
        const auto &idx = d.get_index_type<asset_limitation_index>().indices().get<by_limit_symbol>();
        auto asset_limitation_itr = idx.find(op.common_options.backed_by_asset_symbol);
        FC_ASSERT(asset_limitation_itr != idx.end(),"asset_limitation_object not exists for backed_by_asset_symbol");

        //verify that backed asset create only  with meta1 account 
        const auto& accounts_by_name = d.get_index_type<account_index>().indices().get<by_name>();
        auto itr = accounts_by_name.find("meta1");
        FC_ASSERT(itr != accounts_by_name.end(),"Unable to find meta1 account" );
        FC_ASSERT(itr->get_id() == op.issuer,"backed asset cannot create with this account , backed asset can be created only by meta1 account");

        return void_result();
    }
    FC_CAPTURE_AND_RETHROW((op))
}

object_id_type property_create_evaluator::do_apply(const property_create_operation &op)
{
    try
    {
        database &d = db();
        auto next_property_id = d.get_index_type<property_index>().get_next_id();
        const property_object &new_property =
            d.create<property_object>([&op, next_property_id](property_object &p) {
                p.issuer = op.issuer;
                p.options = op.common_options;
                p.options.allocation_progress = "0.0000000000";
                p.property_id = op.property_id;
            });
        FC_ASSERT(new_property.id == next_property_id, "Unexpected object database error, object id mismatch");
        return new_property.id;
    }
    FC_CAPTURE_AND_RETHROW((op))
}

void_result property_update_evaluator::do_evaluate(const property_update_operation &op)
{
    try
    {
        database &d = db();
        const property_object &property_ob = op.property_to_update(d);
        property_to_update = &property_ob;

        FC_ASSERT(op.issuer == property_ob.issuer,
                  "Incorrect issuer for backed asset! (${o.issuer} != ${a.issuer})",
                  ("o.issuer", op.issuer)("a.issuer", property_ob.issuer));
        op.validate();

        //verify that asset_limitation_object exists for backed_by_asset_symbol
        const auto &idx = d.get_index_type<asset_limitation_index>().indices().get<by_limit_symbol>();
        auto asset_limitation_itr = idx.find(property_ob.options.backed_by_asset_symbol);
        FC_ASSERT(asset_limitation_itr != idx.end(),"asset_limitation_object not exists for backed_by_asset_symbol");

        //verify that backed asset update only  with meta1 account 
        const auto& accounts_by_name = d.get_index_type<account_index>().indices().get<by_name>();
        auto itr = accounts_by_name.find("meta1");
        FC_ASSERT(itr->get_id() == op.issuer,"backed asset cannot update with this account , backed asset can be updated only by meta1 account");

        return void_result();
    }
    FC_CAPTURE_AND_RETHROW((op))
}

void_result property_update_evaluator::do_apply(const property_update_operation &op)
{
    try
    {
        database &d = db();
        d.modify(*property_to_update, [&op](property_object &p) {
            p.options = op.new_options;
        });
        return void_result();
    }
    FC_CAPTURE_AND_RETHROW((op))
}

void_result property_delete_evaluator::do_evaluate(const property_delete_operation& o)
{ try {
   database& d = db();

   _property = &o.property(d);
   FC_ASSERT( _property->issuer == o.fee_paying_account );

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

void_result property_delete_evaluator::do_apply(const property_delete_operation& o)
{ try {
    database &d = db();

    //Roll back asset_limitation value if we delete backed asset
    const asset_limitation_object *asset_limitaion = nullptr;
    const auto &idx = d.get_index_type<asset_limitation_index>().indices().get<by_limit_symbol>();
    auto itr = idx.find(o.property(d).options.backed_by_asset_symbol);
    if (itr != idx.end())
    {
        asset_limitaion = &*itr;
        double_t sell_limit = boost::lexical_cast<double_t>(asset_limitaion->options.sell_limit);
        double_t allocation_progress = boost::lexical_cast<double_t>(o.property(d).options.allocation_progress);
        string new_sell_limit = boost::lexical_cast<string>(sell_limit - allocation_progress);

        d.modify(*asset_limitaion, [&new_sell_limit](asset_limitation_object &a) {
            a.options.sell_limit = new_sell_limit;
        });
    }

    d.remove(*_property);
  
    return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

} // namespace chain
} // namespace graphene 
