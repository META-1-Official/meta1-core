#include <graphene/chain/property_evaluator.hpp>
#include <graphene/chain/property_object.hpp>
#include <graphene/chain/asset_limitation_object.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/asset_limitation_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/is_authorized_asset.hpp>
#include <graphene/chain/allocation.hpp>

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

        // Validate that the allocation duration is positive
        FC_ASSERT(op.common_options.allocation_duration_minutes >= 4); // Minimum requirement of 4 minutes

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
               p.property_id = op.property_id;
               p.expired = false;

               uint32_t full_duration_minutes = op.common_options.allocation_duration_minutes;
               // TODO: [Low] Overflow check
                uint32_t full_duration_seconds = full_duration_minutes * 60;

               // Calculate the appreciation period parameters
               const time_point_sec &start_time = calc_meta1_next_start_time(d.head_block_time());
               const appreciation_period_parameters period =
                       calc_meta1_allocation_initial_parameters(start_time, full_duration_seconds);
               p.creation_date = start_time;
               p.next_allocation_date = start_time + META1_INTERVAL_BETWEEN_ALLOCATION_SECONDS;

               p.initial_end_date = period.time_to_25_percent;
               p.initial_counter = 0;
               p.initial_counter_max = period.time_to_25_percent_intervals;

               p.approval_end_date = period.time_to_100_percent;
               p.approval_counter = 0;
               p.approval_counter_max = period.time_to_100_percent_intervals - period.time_to_25_percent_intervals;

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
            p.options = op.new_options;
        });
        return void_result();
    }
    FC_CAPTURE_AND_RETHROW((op))
}

   void_result property_approve_evaluator::do_evaluate(const property_approve_operation &op) {
      try {
         database &d = db();
         const property_object &property_ob = op.property_to_approve(d);
         property_to_approve = &property_ob;

         FC_ASSERT(op.issuer == property_ob.issuer,
                   "Incorrect issuer for backed asset! (${o.issuer} != ${a.issuer})",
                   ("o.issuer", op.issuer)("a.issuer", property_ob.issuer));
         op.validate();

         // Prohibit approvals after the approval deadline
         FC_ASSERT(d.head_block_time() < property_ob.approval_end_date,
                   "The approval deadline passed on ${deadline}", ("deadline", property_ob.approval_end_date) );
         // TODO: Add test for approving after approval end date

         // Prohibit multiple approvals
         // TODO: [Low] Add test for multiple approval attempts
         FC_ASSERT(!property_ob.approval_date.valid(), "Backing asset is already approved!");

         return void_result();
      }
      FC_CAPTURE_AND_RETHROW((op))
   }

   void_result property_approve_evaluator::do_apply(const property_approve_operation &op) {
      try {
         database &d = db();
         // auto next_property_id = d.get_index_type<property_index>().get_next_id();
         const time_point_sec& now = d.head_block_time();
         d.modify(*property_to_approve, [&now](property_object &p) {
            // Set a new appreciation rate if the approval is after the initial allocation period
            if (now < p.initial_end_date) {
               ilog("Approving property during initial phase");

               // Approval is occurring during the initial phase
               // The only field that needs to be updated is the approval date
               // The remaining fields were sufficiently initialized when the property was created
               p.approval_date = now;

            } else {
               ilog("Approving property during approval phase");

               // Calculate the appreciation restart time
               appreciation_restart_parameters restart =
                       calc_meta1_allocation_restart_parameters(now, p.approval_end_date);

               // Set the approval date
               p.approval_date = restart.restart_time;
               FC_ASSERT(p.approval_counter == 0, "The approval phase counter was not at zero as expected");
               p.approval_counter_max = restart.intervals_to_end;

               p.next_allocation_date = restart.restart_time + META1_INTERVAL_BETWEEN_ALLOCATION_SECONDS;
            }

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

   void_result property_delete_evaluator::do_apply(const property_delete_operation &o) {
      try {
         database &d = db();

         //Roll back asset_limitation value if we delete backed asset
         const asset_limitation_object *asset_limitation = nullptr;
         const auto &idx = d.get_index_type<asset_limitation_index>().indices().get<by_limit_symbol>();
         auto itr = idx.find(_property->options.backed_by_asset_symbol);
         if (itr != idx.end()) {
            asset_limitation = &*itr;

            // TODO: [High] Test property delete
            const uint64_t contribution = calc_meta1_contribution(*_property);
            const uint64_t new_sell_limit = asset_limitation->cumulative_sell_limit - contribution;

            d.modify(*asset_limitation, [&new_sell_limit](asset_limitation_object &alo) {
               alo.cumulative_sell_limit = new_sell_limit;
            });
         }

         d.remove(*_property);

         return void_result();
      } FC_CAPTURE_AND_RETHROW((o))
   }

} // namespace chain
} // namespace graphene 
