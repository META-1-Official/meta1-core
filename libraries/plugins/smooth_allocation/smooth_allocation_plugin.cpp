#include <graphene/smooth_allocation/smooth_allocation_plugin.hpp>

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

const property_object &smooth_allocation_plugin::get_backed_asset(chain::database &db, protocol::property_id_type backed_asset_id_type) const
{
   const auto &idx = db.get_index_type<property_index>().indices().get<by_id>();
   for (const auto &b : idx)
   {
      if (b.get_id() == backed_asset_id_type)
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

void smooth_allocation_plugin::allocate_price_limitation(string backed_by_asset_symbol, uint32_t backed_asset_id, double_t value)
{
   try
   {
      double_t price_buf = 0.0;
      signed_transaction trx;
      const auto &asset_limitation_to_update = get_asset_limitation(database(), backed_by_asset_symbol);

      asset_limitation_object_update_operation update_op;
      update_op.issuer = asset_limitation_to_update.issuer;
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
      wlog("allocate_price_limitation 1");
      processed_transaction ptrx = database().push_transaction(precomputable_transaction(trx));
      wlog("allocate_price_limitation 2");
      increase_backed_asset_allocation_progress(backed_asset_id, value);
   }
   catch (const std::exception &e)
   {
      wlog(e.what());
   }
}

void smooth_allocation_plugin::increase_backed_asset_allocation_progress(uint32_t backed_asset_id, double_t increase_value)
{
   try
   {
      double_t allocation_progress = 0.0;
      property_update_operation update_op;
      signed_transaction trx;
      const auto &backed_asset_to_update = get_backed_asset(database(), backed_asset_id);

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
      processed_transaction ptrx = database().push_transaction(precomputable_transaction(trx));
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
      {
         privkey = graphene::utilities::wif_to_key(options["meta1-private-key"].as<string>());
         wlog("privKEy 1 : ${k}", ("k", privkey));
         wlog("privKEy 2 :${k}", ("k", options["meta1-private-key"].as<string>()));
      }

      ilog("smooth_allocation_plugin:  plugin_initialize() end");
   }
   FC_LOG_AND_RETHROW()
}

void smooth_allocation_plugin::synchronize_backed_assets(chain::database &db)
{
   auto all_backed_assets = get_all_backed_assets(db);
   double_t allocation_value;
   for (auto &backed_asset : all_backed_assets)
   {
      allocation_value = ((double_t)backed_asset.options.appraised_property_value / get_asset_supply(db, backed_asset.options.backed_by_asset_symbol)) * 0.25;
      if ((boost::lexical_cast<double_t>(backed_asset.options.allocation_progress) < allocation_value))
      {
         initial_smooth_backed_assets.push_back(get_backed_asset(database(), backed_asset.property_id));
      }

      if (backed_asset.options.status == "approved")
      {
         allocation_value = ((double_t)backed_asset.options.appraised_property_value / get_asset_supply(db, backed_asset.options.backed_by_asset_symbol));
         if ((boost::lexical_cast<double_t>(backed_asset.options.allocation_progress) < allocation_value))
         {
            approve_smooth_backed_assets.push_back(get_backed_asset(database(), backed_asset.property_id));
         }
      }
   }
   if (initial_smooth_backed_assets.size() > 0 || approve_smooth_backed_assets.size() > 0)
   {
      wlog("schedule_allocation_loop on ");
      _shutting_down = false;
      schedule_allocation_loop();
   }
}

void smooth_allocation_plugin::erase_backed_asset(vector<chain::property_object> &backed_asset_storage, protocol::property_id_type backed_asset_id_type)
{
   std::vector<property_object>::iterator backed_asset_iterator = std::find_if(backed_asset_storage.begin(), backed_asset_storage.end(),
                                                                               [backed_asset_id_type](const property_object &o) {
                                                                                  return o.get_id() == backed_asset_id_type;
                                                                               });
   if (backed_asset_iterator != backed_asset_storage.end())
   {
      backed_asset_storage.erase(backed_asset_iterator);
   }
}

void smooth_allocation_plugin::plugin_startup()
{
   try
   {
      ilog("smooth allocation plugin:  plugin_startup() begin");
      chain::database &d = database();
      synchronize_backed_assets(d);

      wlog("initial backed assets size:${s}", ("s", initial_smooth_backed_assets.size()));
      wlog("approve backed assets size:${s}", ("s", approve_smooth_backed_assets.size()));

      d.on_pending_transaction.connect([this](const signed_transaction &t) {
         for (auto &operation : t.operations)
         {
            if (operation.is_type<property_create_operation>())
            {
               const property_create_operation &backed_asset_create_op = operation.get<property_create_operation>();
               auto backed_asset_iterator = std::find_if(initial_smooth_backed_assets.begin(), initial_smooth_backed_assets.end(),
                                                         [backed_asset_create_op](const property_object &o) {
                                                            return o.property_id == backed_asset_create_op.property_id;
                                                         });

               if (backed_asset_iterator == initial_smooth_backed_assets.end())
               {
                  initial_smooth_backed_assets.push_back(get_backed_asset(database(), backed_asset_create_op.property_id));
                  if (initial_smooth_backed_assets.size() == 1 && approve_smooth_backed_assets.size() == 0)
                  {
                     _shutting_down = false;
                     wlog("schedule_allocation_loop on ");
                     schedule_allocation_loop();
                  }
               }
            }
            else if (operation.is_type<property_update_operation>())
            {
               const property_update_operation &backed_asset_update_op = operation.get<property_update_operation>();
               auto backed_asset_iterator = std::find_if(approve_smooth_backed_assets.begin(), approve_smooth_backed_assets.end(),
                                                         [backed_asset_update_op](const property_object &o) {
                                                            return o.get_id() == backed_asset_update_op.property_to_update;
                                                         });

               if (backed_asset_update_op.new_options.status == "approved" && backed_asset_iterator == approve_smooth_backed_assets.end())
               {
                  approve_smooth_backed_assets.push_back(get_backed_asset(database(), backed_asset_update_op.property_to_update));
                  if (approve_smooth_backed_assets.size() == 1 && initial_smooth_backed_assets.size() == 0)
                  {
                     wlog("schedule_allocation_loop on ");
                     _shutting_down = false;
                     schedule_allocation_loop();
                  }
               }
            }
            else if (operation.is_type<property_delete_operation>())
            {
               const property_delete_operation &backed_asset_delete_op = operation.get<property_delete_operation>();
               erase_backed_asset(initial_smooth_backed_assets, backed_asset_delete_op.property);
               erase_backed_asset(approve_smooth_backed_assets, backed_asset_delete_op.property);
            }
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
            //const auto &asset_limitation_to_update = get_asset_limitation(database(), backed_asset.options.backed_by_asset_symbol);
         }

         for (auto &backed_asset : approve_smooth_backed_assets)
         {
            result = maybe_allocate_price(backed_asset, 0.75, capture);
            result_viewer(result, capture);
            //const auto &asset_limitation_to_update = get_asset_limitation(database(), backed_asset.options.backed_by_asset_symbol);
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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                  //
// "Coin Appreciation" Smart Contract: This contract is defined as nine-times (9X) the newly added asset value plus //
// the original entire asset value, divided by the “fixed” number of coins (450,000,000), equals the value per coin.//
// In our pro forma financial projections, the coin will increase in value by approximately                         //
// $22.22 a coin per $1 billion in asset assignment. As the asset is “In Process” and then “Verified” in            //
// the two Smart Contracts, the META 1 Coin will realize 100% of the market value of the new asset.                 //
//                                                                                                                  //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                  //
// "Smooth Allocation" Smart Contract:                                                                              // 
// Smart Contract divides the market value of the asset, $1,000,000,000 / 52 weeks =                                // 
// $19,230,769 per week of asset value assigned to the META 1 Coin.                                                 //
//                                                                                                                  //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                  //
// "In Process" Smart Contract: During the IN-PROCESS                                                               //
// stage, 25% of the Market Value of the asset will be sent to the COIN APPRECIATION CONTRACT,                      //
// increasing the META 1 Coin values accordingly.                                                                   //
//                                                                                                                  // 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*White paper url https://www.meta1.io/Assets/PDFs/META1%20Coin%20White%20Paper.pdf*/

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
      if (boost::lexical_cast<double_t>(get_backed_asset(database(), backed_asset.property_id).options.allocation_progress) + increase_value > price)
         increase_value = price - boost::lexical_cast<double_t>(get_backed_asset(database(), backed_asset.property_id).options.allocation_progress);

      if (boost::lexical_cast<double_t>(get_backed_asset(database(), backed_asset.property_id).options.allocation_progress) < price)
      {
         allocate_price_limitation(backed_asset.options.backed_by_asset_symbol, backed_asset.property_id, increase_value);
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
         erase_backed_asset(initial_smooth_backed_assets, backed_asset.get_id());
         return smooth_allocation_condition::initial_allocation_completed;
      }
      else if (allocation_percent == 0.75)
      {
         erase_backed_asset(approve_smooth_backed_assets, backed_asset.get_id());
         return smooth_allocation_condition::approve_allocation_completed;
      }
   }
   catch (const std::exception &e)
   {
      capture("err", e.what());
      return smooth_allocation_condition::exception_allocation;
   }
}
