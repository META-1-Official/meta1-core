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

        //verify that backed asset create only with meta1 account
        const auto& accounts_by_name = d.get_index_type<account_index>().indices().get<by_name>();
        auto itr = accounts_by_name.find("meta1");
        FC_ASSERT(itr != accounts_by_name.end(),"Unable to find meta1 account" );
        FC_ASSERT(itr->get_id() == op.issuer,"backed asset cannot create with this account , backed asset can be created only by meta1 account");

        // TODO: Validate that the allocation duration is positive

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
            d.create<property_object>([&op, next_property_id, &d](property_object &p) {
                p.issuer = op.issuer;
                p.options = op.common_options;
                p.options.allocation_progress = "0.0000000000";
                p.property_id = op.property_id;

                p.date_creation = d.head_block_time();
                uint32_t qty_weeks = boost::lexical_cast<double_t>(p.options.smooth_allocation_time);
                uint32_t full_duration_minutes = qty_weeks * 7 * 24 * 60;
                uint32_t full_duration_seconds = full_duration_minutes * 60;
                p.date_approval_deadline = d.head_block_time() + full_duration_seconds;
                uint32_t initial_duration_seconds = full_duration_seconds / 4;
                p.date_initial_end = d.head_block_time() + initial_duration_seconds; // 25% of full duration

                uint32_t allocation_interval_seconds = 60; // Allocations every minute
                p.date_next_allocation = p.date_creation + allocation_interval_seconds;

                // Calculate allocation
                string symbol = op.common_options.backed_by_asset_symbol;
                // get_asset_supply()
                const auto &idx = d.get_index_type<asset_index>().indices().get<by_symbol>();
                auto itr_supply = idx.find(symbol);
                assert(itr_supply != idx.end());
                const asset_object& backing_asset = *itr_supply;
                //calc supply for smooth allocation formula supply of coin / 10
                int64_t backing_asset_supply = backing_asset.options.max_supply.value / std::pow(10, backing_asset.precision);
                // TODO: Safely convert allocation to uint64_t representation
                uint64_t full_allocation = 10 * p.options.appraised_property_value;
                uint64_t full_allocation_cents = 100 * full_allocation;
//                uint64_t full_allocation_cents_per_minute = floor(full_allocation_cents / full_duration_minutes);
                double_t full_allocation_per_backing_asset = (10 * (double)p.options.appraised_property_value / backing_asset_supply);

                p.expired = false;
                p.scaled_allocation_progress = 0;
                p.scaled_allocation_per_minute = floor(META1_SCALED_ALLOCATION_PRECISION / full_duration_minutes);

                ilog("Property create: p.options.appraised_property_value: ${x}", ("x", p.options.appraised_property_value));
                ilog("Property create: backing_asset_supply: ${x}", ("x", backing_asset_supply));
                ilog("Property create: full_allocation_per_backing_asset: ${x}", ("x", full_allocation_per_backing_asset));
                ilog("Property create: full_duration_minutes: ${x}", ("x", full_duration_minutes));
                ilog("Property create: allocation cents per minute: ${x}", ("x", ((double)full_allocation_cents) / full_duration_minutes));
                ilog("Property create: allocation cents per minute (int): ${x}", ("x", p.scaled_allocation_per_minute));
                ilog("Property create: fractional allocation per minute: ${x}", ("x", ((double)META1_SCALED_ALLOCATION_PRECISION) / full_duration_minutes ));
                ilog("Property create: fractional allocation per minute (int): ${x}", ("x", p.scaled_allocation_per_minute));

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

       // TODO: Prohibit updating an asset after the approval deadline

        //verify that asset_limitation_object exists for backed_by_asset_symbol
        const auto &idx = d.get_index_type<asset_limitation_index>().indices().get<by_limit_symbol>();
        auto asset_limitation_itr = idx.find(property_ob.options.backed_by_asset_symbol);
        FC_ASSERT(asset_limitation_itr != idx.end(),"asset_limitation_object not exists for backed_by_asset_symbol");

       // TODO: Verify that the backed by asset symbol is not changing!

        //verify that backed asset update only with meta1 account
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
       d.modify(*property_to_update, [&op, &d](property_object &p) {
            time_point_sec current_time = d.head_block_time();

            // Set the approval date
            p.date_approval = current_time;

            // Set a new appreciation rate if the approval is after the initial allocation period
            if (current_time > p.date_initial_end) {
               ilog("Property Approving");
               ilog("Property scaled rate before: ${x}", ("x", p.scaled_allocation_per_minute));

               // Calculate the new allocation rate if the current time is after the initial end time
               uint64_t remaining_scaled_allocation = floor(META1_SCALED_ALLOCATION_PRECISION * 3 / 4);
               ilog("Remaining scaled allocation: ${x}", ("x", remaining_scaled_allocation));

               uint64_t remaining_duration_seconds = (p.date_approval_deadline - current_time).to_seconds();
               uint64_t remaining_duration_minutes = floor(remaining_duration_seconds / 60);
               ilog("Remaining duration [min]: ${x}", ("x", remaining_duration_minutes));

               p.scaled_allocation_per_minute = floor(remaining_scaled_allocation / remaining_duration_minutes);
               ilog("Property scaled rate after: ${x}", ("x", p.scaled_allocation_per_minute));

               p.date_next_allocation = current_time + 60;
            }

            // TODO: Permit updates to options during an approval?
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
