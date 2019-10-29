#include <graphene/smooth_allocation/smooth_allocation_plugin.hpp>

#include <fc/thread/thread.hpp>
#include <algorithm>

#include <iostream>

using namespace graphene::smooth_allocation;
using std::string;
using std::vector;
using namespace graphene::chain;

namespace bpo = boost::program_options;

const asset_limitation_object &smooth_allocation_plugin::get_asset_limitation(chain::database &db,string symbol) const
{
   const auto &idx = db.get_index_type<asset_limitation_index>().indices().get<by_limit_symbol>();
   auto itr = idx.find(symbol);
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
void smooth_allocation_plugin::allocate_price_limitation(property_object &backed_asset,double_t value)
{
   try
   {
      double_t price_buf = 0.0;
      signed_transaction trx;
      const auto &asset_limitation_to_update = get_asset_limitation(database(),backed_asset.options.backed_by_asset_symbol);

      asset_limitation_object_update_operation update_op;

      update_op.issuer = meta1_id;
      update_op.asset_limitation_object_to_update = asset_limitation_to_update.id;

      asset_limitation_options asset_limitation_ops = asset_limitation_to_update.options;
      //sell limit
      price_buf = std::stod(asset_limitation_ops.sell_limit);
      price_buf += value;
      asset_limitation_ops.sell_limit = std::to_string(price_buf);

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

void smooth_allocation_plugin::force_initial_smooth(property_object &backed_asset)
{
   try
   {
      double_t price_buf = 0.0;
      const double_t price = (backed_asset.options.appraised_property_value / 45000000) * 0.25;
      allocate_price_limitation(backed_asset,price);
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
      property_update_operation update_op;
      signed_transaction trx;

      update_op.issuer = backed_asset.issuer;
      update_op.property_to_update = backed_asset.id;

      double_t allocation_progress = std::stod(backed_asset.options.allocation_progress)+ increase_value;
      backed_asset.options.allocation_progress = boost::lexical_cast<std::string>(allocation_progress);

      update_op.new_options = backed_asset.options;

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
      privkey = graphene::utilities::wif_to_key("5HuCDiMeESd86xrRvTbexLjkVg2BEoKrb7BAA5RLgXizkgV3shs");

      ilog("smooth_allocation_plugin:  plugin_initialize() end");
   }
   FC_LOG_AND_RETHROW()
}

void smooth_allocation_plugin::plugin_startup()
{
   try
   {
      ilog("smooth allocation plugin:  plugin_startup() begin");
      meta1_id = database().get_index_type<account_index>().indices().get<by_name>().find("meta1")->id;
      chain::database &d = database();
      this->backed_assets = get_all_backed_assets(d);
      wlog("Backed assets count detected: ${s}", ("s", backed_assets.size()));

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
                     wlog("New backed asset detected: ${p}", ("p", backed_asset.id));

                     if (backed_asset.options.smooth_allocation_time == "0" ||
                         backed_asset.options.smooth_allocation_time.size() == 0)
                     {
                        force_initial_smooth(backed_asset);
                     }
                     else
                     {
                        initial_smooth_backed_assets.push_back(backed_asset);
                        wlog("initial_smooth_backed_assets count: ${c}", ("c", initial_smooth_backed_assets.size()));

                        if (initial_smooth_backed_assets.size() == 1)
                           schedule_allocation_loop();
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
                                          next_wakeup, "initial smooth allocation");
}

smooth_allocation_condition::smooth_allocation_condition_enum smooth_allocation_plugin::allocation_loop()
{
   smooth_allocation_condition::smooth_allocation_condition_enum result;
   if (_shutting_down)
   {
      result = smooth_allocation_condition::shutdown;
   }
   else
   {
      try
      {
         for (auto &backed_asset : initial_smooth_backed_assets)
         {
            wlog("backed assets loop: ${i}", ("i", backed_asset.id));
            maybe_allocate_price(backed_asset);
         }
      }
      catch (const fc::exception &e)
      {
         elog("Got exception while smooth allocation :\n${e}", ("e", e.to_detail_string()));
         result = smooth_allocation_condition::exception_allocation;
      }
   }

   /*switch (result)
   {
   default:
      elog("unknown condition ${result} while producing block", ("result", (unsigned char)result));
      break;
   }*/

   schedule_allocation_loop();
   return result;
}

smooth_allocation_condition::smooth_allocation_condition_enum smooth_allocation_plugin::maybe_allocate_price(property_object &backed_asset)
{
   try
   {
      const double_t price = (backed_asset.options.appraised_property_value / 45000000) * 0.25;
      const double_t timeline = std::stod(backed_asset.options.smooth_allocation_time) * 7 * 24 * 60 * 0.25;
      const double_t increase_value = price / timeline;


      if (std::stod(backed_asset.options.allocation_progress) < price)
      {
         wlog("Backed asset: ${p}", ("p", backed_asset.id));
         wlog("Backed asset progress: ${p}", ("p", backed_asset.options.allocation_progress));
         wlog("Backed asset price increase_value: ${p}", ("p", increase_value));
         wlog("Backed asset price to allocate: ${p}", ("p", price));
         wlog("Backed asset price timeline: ${p}", ("p", timeline));
         allocate_price_limitation(backed_asset,increase_value);
         wlog("Backed asset progress: ${p}", ("p", backed_asset.options.allocation_progress));
      }
      else
      {
         std::vector<property_object>::iterator backed_asset_iterator = std::find_if(initial_smooth_backed_assets.begin(), initial_smooth_backed_assets.end(),
                                                                                     [backed_asset](const property_object &o) {
                                                                                        return o.id == backed_asset.id;
                                                                                     });
         initial_smooth_backed_assets.erase(backed_asset_iterator);
      }

      /*let final = 0;
		let loop = setInterval(() => {
			final += INCREASE;
			if (final >= PRICE) {
				clearInterval(loop);
			}
			Price.getPriceById((err, priceTrue) => {
				if (err) {
					throw err;
				}
				console.log(INCREASE);
				Price.updatePrice(INCREASE, (err, price) => {
					if (err) {
						throw err;
					}
				});
			});
		}*/
      return smooth_allocation_condition::produced;
   }
   catch (const std::exception &e)
   {
      wlog(e.what());
   }
}
