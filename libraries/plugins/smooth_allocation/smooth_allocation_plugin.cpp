#include <graphene/smooth_allocation/smooth_allocation_plugin.hpp>

#include <fc/thread/thread.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>

using namespace graphene::smooth_allocation;
using std::string;
using std::vector;
using namespace graphene::chain;

namespace bpo = boost::program_options;

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

const asset_limitation_object &smooth_allocation_plugin::get_asset_limitation(chain::database &db, string symbol) const
{
   const auto &idx = db.get_index_type<asset_limitation_index>().indices().get<by_limit_symbol>();
   auto itr = idx.find(symbol);
   assert(itr != idx.end());
   return *itr;
}

const property_object &smooth_allocation_plugin::get_backed_asset(chain::database &db, uint32_t backed_asset_id) const
{
   const auto &idx = db.get_index_type<property_index>().indices().get<by_id>();
   for (const auto &b : idx)
   {
      if (b.property_id == backed_asset_id)
         return b;
   }
}

const int64_t smooth_allocation_plugin::get_asset_supply(chain::database &db, string symbol) const
{
   const auto &idx = db.get_index_type<asset_index>().indices().get<by_symbol>();
   auto itr = idx.find(symbol);
   assert(itr != idx.end());
   //calc supply for smooth allocation formula supply of coin / 10
   return itr->options.max_supply.value / 10 / std::pow(10, itr->precision);
}

void smooth_allocation_plugin::allocate_price_limitation(property_object &backed_asset, double_t value)
{
   try
   {
      double_t price_buf = 0.0;
      signed_transaction trx;
      const auto &asset_limitation_to_update = get_asset_limitation(database(), backed_asset.options.backed_by_asset_symbol);

      asset_limitation_object_update_operation update_op;

      update_op.issuer = meta1_account_id;
      update_op.asset_limitation_object_to_update = asset_limitation_to_update.id;

      asset_limitation_options asset_limitation_ops = asset_limitation_to_update.options;
      //sell limit
      price_buf = boost::lexical_cast<double_t>(asset_limitation_ops.sell_limit);
      price_buf += value;
      asset_limitation_ops.sell_limit = boost::lexical_cast<std::string>(price_buf);

      update_op.new_options = asset_limitation_ops;

      trx.operations.push_back(update_op);
      database().current_fee_schedule().set_fee(trx.operations.back());
      trx.set_expiration(fc::time_point::now() + fc::seconds(3000));
      trx.sign(*privkey, database().get_chain_id());
      trx.validate();
      processed_transaction ptrx = database().push_transaction(precomputable_transaction(trx), ~0);
      increase_backed_asset_allocation_progress(backed_asset, value);
   }
   catch (const std::exception &e)
   {
      wlog(e.what());
   }
}

void smooth_allocation_plugin::increase_backed_asset_allocation_progress(property_object &backed_asset, double_t increase_value)
{
   try
   {
      double_t allocation_progress = 0.0;
      property_update_operation update_op;
      signed_transaction trx;
      const auto &backed_asset_to_update = get_backed_asset(database(), backed_asset.property_id);

      update_op.issuer = backed_asset_to_update.issuer;
      update_op.property_to_update = backed_asset_to_update.id;
      allocation_progress = boost::lexical_cast<double_t>(backed_asset_to_update.options.allocation_progress);
      allocation_progress += increase_value;

      update_op.new_options = backed_asset_to_update.options;
      update_op.new_options.allocation_progress = boost::lexical_cast<std::string>(allocation_progress);

      trx.operations.push_back(update_op);
      database().current_fee_schedule().set_fee(trx.operations.back());
      trx.set_expiration(fc::time_point::now() + fc::seconds(3000));
      trx.sign(*privkey, database().get_chain_id());
      trx.validate();
      processed_transaction ptrx = database().push_transaction(precomputable_transaction(trx), ~0);
   }
   catch (const std::exception &e)
   {
      wlog(e.what());
   }
}

void smooth_allocation_plugin::plugin_set_program_options(
    boost::program_options::options_description &command_line_options,
    boost::program_options::options_description &config_file_options)
{
   command_line_options.add_options()("meta1-private-key", bpo::value<string>()->default_value("5HuCDiMeESd86xrRvTbexLjkVg2BEoKrb7BAA5RLgXizkgV3shs"),
                                      "meta1 account private wif key");
   config_file_options.add(command_line_options);
}

std::string smooth_allocation_plugin::plugin_name() const
{
   return "smooth_allocation";
}

void smooth_allocation_plugin::plugin_initialize(const boost::program_options::variables_map &options)
{
   try
   {
      ilog("smooth allocation plugin:  plugin_initialize() begin");
      _options = &options;

      //meta1 key for signing trx's
      if (options.count("meta1-private-key"))   
         privkey = graphene::utilities::wif_to_key(options["meta1-private-key"].as<string>());

      ilog("smooth_allocation_plugin:  plugin_initialize() end");
   }
   FC_LOG_AND_RETHROW()
}

void smooth_allocation_plugin::plugin_startup()
{
   try
   {
      ilog("smooth allocation plugin:  plugin_startup() begin");
      meta1_account_id = database().get_index_type<account_index>().indices().get<by_name>().find("meta1")->id;
      chain::database &d = database();
      backed_assets_local_storage = get_all_backed_assets(d);
      ilog("Backed assets count detected: ${s}", ("s", backed_assets_local_storage.size()));

      //check for new backed assets or approve backed assets
      d.applied_block.connect([this](const chain::signed_block &b) {
         try
         {
            std::vector<property_object>::iterator backed_asset_iterator;
            auto all_backed_assets = get_all_backed_assets(database());
            for (auto &backed_asset : all_backed_assets)
            {
               backed_asset_iterator = std::find_if(backed_assets_local_storage.begin(), backed_assets_local_storage.end(),
                                                    [backed_asset](const property_object &o) {
                                                       return o.id == backed_asset.id;
                                                    });

               if (backed_asset_iterator == backed_assets_local_storage.end())
               {
                  if (backed_asset.options.smooth_allocation_time == "0" ||
                      backed_asset.options.smooth_allocation_time.size() == 0)
                  {
                     //wlog("force initial");
                     const double_t price = ((double)backed_asset.options.appraised_property_value / get_asset_supply(database(), backed_asset.options.backed_by_asset_symbol)) * 0.25;

                     allocate_price_limitation(backed_asset, price);
                  }
                  else
                  {
                     //wlog("smooth initial");
                     initial_smooth_backed_assets.push_back(backed_asset);
                     if (initial_smooth_backed_assets.size() == 1 && approve_smooth_backed_assets.size() == 0)
                     {
                        _shutting_down = false;
                        schedule_allocation_loop();
                     }
                  }
               }
               else if (backed_asset_iterator->options.status == "not approved" && backed_asset.options.status == "approved")
               {
                  if (backed_asset.options.smooth_allocation_time == "0" ||
                      backed_asset.options.smooth_allocation_time.size() == 0)
                  {
                     //wlog("force approve");
                     const double_t price = ((double)backed_asset.options.appraised_property_value / get_asset_supply(database(), backed_asset.options.backed_by_asset_symbol)) * 0.75;

                     allocate_price_limitation(backed_asset,price);
                  }
                  else
                  {
                    //wlog("smooth approve");
                     approve_smooth_backed_assets.push_back(backed_asset);
                     if (approve_smooth_backed_assets.size() == 1 && initial_smooth_backed_assets.size() == 0)
                     {
                        _shutting_down = false;
                        schedule_allocation_loop();
                     }
                  }
               }
            }
            backed_assets_local_storage = all_backed_assets;
         }
         catch (const std::exception &e)
         {
            ilog(e.what());
         }
      });
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
   _smooth_allocation_task = fc::schedule([this] { allocation_loop(); },
                                          next_wakeup, "smooth allocation");
}

void smooth_allocation_plugin::result_viewer(smooth_allocation_condition::smooth_allocation_condition_enum result, fc::limited_mutable_variant_object &capture)
{
   switch (result)
   {
   case smooth_allocation_condition::initial_allocation_produced:
      wlog("Initial allocation produced for backed_asset: ${backed_asset} with progress: ${progress}", (capture));
      break;
   case smooth_allocation_condition::approve_allocation_produced:
      wlog("Approve allocation produced for backed_asset: ${backed_asset} with progress: ${progress}", (capture));
      break;
   case smooth_allocation_condition::initial_allocation_completed:
      wlog("Initial allocation completed for: ${backed_asset}", (capture));
      break;
   case smooth_allocation_condition::approve_allocation_completed:
      wlog("Approve allocation completed for: ${backed_asset}", (capture));
      break;
   case smooth_allocation_condition::exception_allocation:
      elog("Exception in allocation error log: ${err}", (capture));
      break;
   case smooth_allocation_condition::stop_smooth_allocation:
      break;
   default:
      wlog("unknown condition");
      break;
   }
}

void smooth_allocation_plugin::allocation_loop()
{
   smooth_allocation_condition::smooth_allocation_condition_enum result;
   fc::limited_mutable_variant_object capture(GRAPHENE_MAX_NESTED_OBJECTS);

   if (initial_smooth_backed_assets.size() == 0 && approve_smooth_backed_assets.size() == 0)
   {
      result = smooth_allocation_condition::stop_smooth_allocation;
      result_viewer(result, capture);
      stop_allocation();
   }
   else
   {
      try
      {
         for (auto &backed_asset : initial_smooth_backed_assets)
         {
            result = maybe_allocate_price(backed_asset, 0.25, capture);
            result_viewer(result, capture);
            const auto &asset_limitation_to_update = get_asset_limitation(database(), backed_asset.options.backed_by_asset_symbol);
            //wlog("limit 1: ${l}",("l",asset_limitation_to_update.options.sell_limit));
         }

         for (auto &backed_asset : approve_smooth_backed_assets)
         {
            result = maybe_allocate_price(backed_asset, 0.75, capture);
            result_viewer(result, capture);
            const auto &asset_limitation_to_update = get_asset_limitation(database(), backed_asset.options.backed_by_asset_symbol);
            //wlog("limit 2: ${l}",("l",asset_limitation_to_update.options.sell_limit));
         }
      }
      catch (const fc::exception &e)
      {
         result = smooth_allocation_condition::exception_allocation;
         capture("err", e.what());
         result_viewer(result, capture);
      }
   }

   schedule_allocation_loop();
}

smooth_allocation_condition::smooth_allocation_condition_enum smooth_allocation_plugin::maybe_allocate_price(property_object &backed_asset, double_t allocation_percent, fc::limited_mutable_variant_object &capture)
{
   try
   {
      double_t price;
      if (allocation_percent == 0.25)
         price = ((double)backed_asset.options.appraised_property_value / get_asset_supply(database(), backed_asset.options.backed_by_asset_symbol)) * allocation_percent;
      else
         price = (double)backed_asset.options.appraised_property_value / get_asset_supply(database(), backed_asset.options.backed_by_asset_symbol);

      const double_t timeline = boost::lexical_cast<double_t>(backed_asset.options.smooth_allocation_time) * 7 * 24 * 60 * allocation_percent;
      double_t increase_value = price / timeline;
      capture("backed_asset", backed_asset.id);

      //if +=increase_value out of range (price) we should change increase value
      if (boost::lexical_cast<double_t>(get_backed_asset(database(), backed_asset.property_id).options.allocation_progress)+increase_value > price)
            increase_value = price - boost::lexical_cast<double_t>(get_backed_asset(database(), backed_asset.property_id).options.allocation_progress);

      if (boost::lexical_cast<double_t>(get_backed_asset(database(), backed_asset.property_id).options.allocation_progress) < price)
      {
         allocate_price_limitation(backed_asset, increase_value);
         capture("progress", get_backed_asset(database(), backed_asset.property_id).options.allocation_progress);
         if (allocation_percent == 0.25)
         {
            return smooth_allocation_condition::initial_allocation_produced;
         }
         else
         {
            return smooth_allocation_condition::approve_allocation_produced;
         }
      }
      else if (allocation_percent == 0.25)
      {
         std::vector<property_object>::iterator backed_asset_iterator = std::find_if(initial_smooth_backed_assets.begin(), initial_smooth_backed_assets.end(),
                                                                                     [backed_asset](const property_object &o) {
                                                                                        return o.id == backed_asset.id;
                                                                                     });
         initial_smooth_backed_assets.erase(backed_asset_iterator);
         return smooth_allocation_condition::initial_allocation_completed;
      }
      else if (allocation_percent == 0.75)
      {
         std::vector<property_object>::iterator backed_asset_iterator = std::find_if(approve_smooth_backed_assets.begin(), approve_smooth_backed_assets.end(),
                                                                                     [backed_asset](const property_object &o) {
                                                                                        return o.id == backed_asset.id;
                                                                                     });
         approve_smooth_backed_assets.erase(backed_asset_iterator);
         return smooth_allocation_condition::approve_allocation_completed;
      }
   }
   catch (const std::exception &e)
   {
      capture("err", e.what());
      return smooth_allocation_condition::exception_allocation;
   }
}
