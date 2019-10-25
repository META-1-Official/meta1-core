#include <graphene/smooth_allocation/smooth_allocation_plugin.hpp>
#include <graphene/chain/asset_limitation_object.hpp>

#include <fc/thread/thread.hpp>
#include <algorithm>

#include <iostream>

using namespace graphene::smooth_allocation;
using std::string;
using std::vector;
using namespace graphene::chain;

namespace bpo = boost::program_options;

const asset_limitation_object &smooth_allocation_plugin::get_asset_limitation(chain::database &db) const
{
   const auto &idx = db.get_index_type<asset_limitation_index>().indices().get<by_limit_symbol>();
   auto itr = idx.find("META1");
   assert(itr != idx.end());
   return *itr;
}

vector<property_object> smooth_allocation_plugin::get_all_backed_assets(chain::database &db) const
{
   vector<property_object> result;
   const auto &properties_idx = db.get_index_type<property_index>().indices().get<by_id>();
   for (const auto &p : properties_idx)
   {
      result.push_back(p);
   }
   return result;
}

void smooth_allocation_plugin::force_initial_smooth(property_object &backed_asset)
{
   try
   {
      double price_buf = 0.0;
      const double price = (backed_asset.options.appraised_property_value / 45000000) * 0.25;

      const auto &asset_limitation_to_update = get_asset_limitation(this->database());
      asset_limitation_object_update_operation update_op;

      update_op.issuer = asset_limitation_to_update.issuer;
      update_op.asset_limitation_object_to_update = asset_limitation_to_update.id;

      asset_limitation_options asset_limitation_ops = asset_limitation_to_update.options;
      //sell limit

      price_buf = std::stod(asset_limitation_ops.sell_limit);
      price_buf += price;
      ilog("force_initial_smooth begin 3 ${p}", ("p", price_buf));
      asset_limitation_ops.sell_limit = std::to_string(price_buf);

      update_op.new_options = asset_limitation_ops;

      signed_transaction tx;
      tx.operations.push_back(update_op);
      tx.validate();

      tx.set_expiration( fc::time_point::now()+ fc::seconds(3000) );

      this->database().push_transaction(precomputable_transaction(tx), ~0);
   }
   catch (const std::exception &e)
   {
      ilog(e.what());
   }
}

void smooth_allocation_plugin::plugin_set_program_options(
    boost::program_options::options_description &command_line_options,
    boost::program_options::options_description &config_file_options)
{
}

std::string smooth_allocation_plugin::plugin_name() const
{
   return "smooth allocation";
}

void smooth_allocation_plugin::plugin_initialize(const boost::program_options::variables_map &options)
{
   try
   {
      ilog("smooth allocation plugin:  plugin_initialize() begin");
      _options = &options;
      ilog("smooth_allocation_plugin:  plugin_initialize() end");
   }
   FC_LOG_AND_RETHROW()
}

void smooth_allocation_plugin::plugin_startup()
{
   try
   {
      ilog("smooth allocation plugin:  plugin_startup() begin");
      chain::database &d = database();
      this->backed_assets = get_all_backed_assets(d);
      ilog("Backed assets count detected: ${s}", ("s", backed_assets.size()));

      d.applied_block.connect([this](const chain::signed_block &b) {
         try
         {
            std::vector<property_object>::iterator backed_asset_iterator;
            auto all_backed_assets = get_all_backed_assets(database());

            if (all_backed_assets.size() > backed_assets.size())
            {
               for (auto &backed_asset : all_backed_assets)
               {
                  backed_asset_iterator = std::find_if(backed_assets.begin(), backed_assets.end(),
                                                       [backed_asset](const property_object &o) {
                                                          return o.id == backed_asset.id;
                                                       });

                  if (backed_asset_iterator == backed_assets.end())
                  {
                     ilog("New backed asset detected: ${p}", ("p", backed_asset.id));

                     if (backed_asset.options.smooth_allocation_time == "0" ||
                         backed_asset.options.smooth_allocation_time.size() == 0)
                     {
                        force_initial_smooth(backed_asset);
                     }
                  }
               }
               backed_assets = all_backed_assets;
            }
         }
         catch (const std::exception &e)
         {
            ilog(e.what());
         }
      });
      schedule_allocation_loop();
      ilog("smooth allocation:  plugin_startup() end");
   }
   FC_CAPTURE_AND_RETHROW()
}

void smooth_allocation_plugin::plugin_shutdown()
{
   stop_allocation();
}

void smooth_allocation_plugin::stop_allocation()
{
   _shutting_down = true;

   try
   {
      if (_smooth_allocation_task.valid())
         _smooth_allocation_task.cancel_and_wait(__FUNCTION__);
   }
   catch (fc::canceled_exception &)
   {
      //Expected exception. Move along.
   }
   catch (fc::exception &e)
   {
      edump((e.to_detail_string()));
   }
}

void smooth_allocation_plugin::schedule_allocation_loop()
{
   if (_shutting_down)
      return;

   // If we would wait less than 5000ms, wait for the whole min.
   fc::time_point now = fc::time_point::now();
   int64_t time_to_next_allocation = 60000000 - (now.time_since_epoch().count() % 1000000); // 1 min = 60000000
   if (time_to_next_allocation < 5000000)                                                   // we must sleep for at least 5000ms
      time_to_next_allocation += 60000000;

   fc::time_point next_wakeup(now + fc::microseconds(time_to_next_allocation));
   ilog("now: ${e}", ("e", now));
   ilog("time_to_next_second: ${e}", ("e", time_to_next_allocation));
   _smooth_allocation_task = fc::schedule([this] { allocation_loop(); },
                                          next_wakeup, "price smooth allocation");
}

smooth_allocation_condition::smooth_allocation_condition_enum smooth_allocation_plugin::allocation_loop()
{
   smooth_allocation_condition::smooth_allocation_condition_enum result;
   fc::limited_mutable_variant_object capture(GRAPHENE_MAX_NESTED_OBJECTS);

   if (_shutting_down)
   {
      result = smooth_allocation_condition::shutdown;
   }
   else
   {
      try
      {
         result = maybe_allocate_price(capture);
      }
      catch (const fc::canceled_exception &)
      {
         //We're trying to exit. Go ahead and let this one out.
         throw;
      }
      catch (const fc::exception &e)
      {
         elog("Got exception while smooth allocation :\n${e}", ("e", e.to_detail_string()));
         result = smooth_allocation_condition::exception_allocation;
      }
   }

   switch (result)
   {
   case smooth_allocation_condition::produced:
      ilog("Generated block #${n} with ${x} transaction(s) and timestamp ${t} at time ${c}", (capture));
      break;
   case smooth_allocation_condition::not_synced:
      ilog("Not producing block because production is disabled until we receive a recent block "
           "(see: --enable-stale-production)");
      break;
   case smooth_allocation_condition::not_my_turn:
      break;
   case smooth_allocation_condition::not_time_yet:
      break;
   case smooth_allocation_condition::low_participation:
      elog("Not producing block because node appears to be on a minority fork with only ${pct}% witness participation",
           (capture));
      break;
   case smooth_allocation_condition::lag:
      elog("Not producing block because node didn't wake up within 2500ms of the slot time.");
      break;
   case smooth_allocation_condition::exception_allocation:
      elog("exception producing block");
      break;
   case smooth_allocation_condition::shutdown:
      ilog("shutdown producing block");
      return result;
   default:
      elog("unknown condition ${result} while producing block", ("result", (unsigned char)result));
      break;
   }

   schedule_allocation_loop();
   return result;
}

smooth_allocation_condition::smooth_allocation_condition_enum smooth_allocation_plugin::maybe_allocate_price(
    fc::limited_mutable_variant_object &capture)
{
   chain::database &db = database();
   fc::time_point now_fine = fc::time_point::now();
   fc::time_point_sec now = now_fine + fc::microseconds(500000);
   // is anyone scheduled to produce now or one second in the future?
   uint32_t slot = db.get_slot_at_time(now);
   if (slot == 0)
   {
      capture("next_time", db.get_slot_time(1));
      return smooth_allocation_condition::not_time_yet;
   }

   /*signed_transaction trx;
            trx.operations.emplace_back(transfer_operation(asset(1), account_id_type(i + 11), account_id_type(), asset(1), memo_data()));*/

   /*capture("n", block.block_num())("t", block.timestamp)("c", now)("x", block.transactions.size());
   fc::async( [this,block](){ p2p_node().broadcast(net::block_message(block)); } );*/

   return smooth_allocation_condition::produced;
}
