/*
 * Copyright META1 (c) 2020
 */


namespace graphene {
   namespace chain {

      void database::update_smooth_allocation() {
         try {
            vector <property_object> result;

            // Get the property index
            const auto &properties_idx = get_index_type<property_index>().indices().get<by_id>();

            // Get the asset limitation index
            const auto &asset_limitation_idx = get_index_type<asset_limitation_index>().indices().get<by_limit_symbol>();

            // Get the asset index
            const auto &asset_idx = get_index_type<asset_index>().indices().get<by_symbol>();

            // Build the map of asset limitation
            std::map <std::string, fc::int128_t> mapSymbolToContribution;
            std::map <std::string, uint64_t> mapSymbolToAssetSupply;
            for (const auto &a : asset_limitation_idx) {
               const string symbol = a.limit_symbol;

               const auto itr = asset_idx.find(symbol);
               assert(itr != idx.end());
               const asset_object &asset = *itr;

               uint64_t asset_supply = asset.options.max_supply.value / std::pow(10, asset.precision);
               mapSymbolToAssetSupply.insert(std::make_pair(symbol, asset_supply));
            }


            const time_point_sec hbt = head_block_time();
            for (const auto &p : properties_idx) {
               // If expired, skip
               if (p.expired) {
                  continue;
               }

               // If after the initial allocation and if not yet approved, expire
               if (!p.expired && hbt >= p.date_approval_deadline) {
                  if (!p.date_approval.valid()) {
                     // The unapproved property has expired
                     // Eliminate the allocation
                     modify(p, [](property_object &p) {
                        p.expired = true;
                        p.scaled_allocation_progress = 0;
                     });

                     ilog("Smooth allocation expiration in core ${blocknum} at ${blocktime} for ${property}",
                          ("blocknum", head_block_num())("blocktime", hbt)("property", p));

                  } else {
                     // The approved property has matured
                     modify(p, [](property_object &p) {
                        p.expired = true;
                        p.scaled_allocation_progress = META1_SCALED_ALLOCATION_PRECISION;
                     });

                     ilog("Smooth allocation completed in core ${blocknum} at ${blocktime} for ${property}",
                          ("blocknum", head_block_num())("blocktime", hbt)("property", p));
                  }

                  // TODO: Remove from future consideration by update smooth allocation
                  // TODO: Remove propoerty from property set?
                  continue;
               }

               // If after the initial allocation and if not yet approved, skip
               if (hbt > p.date_initial_end && !p.date_approval.valid()) {
                  continue;
               }

               // Increase the allocation
               // Use a while loop to correct for missed allocation due to missed blocks
               while (hbt >= p.date_next_allocation) {
                  modify(p, [](property_object &p) {
                     p.date_next_allocation += 60; // Next allocation should occur in 60 seconds
                     p.scaled_allocation_progress += p.scaled_allocation_per_minute;
                  });

                  result.push_back(p);
               }
            } // end of looping through properties


            // Calculate the asset limitation's cumulative_sell_limit
            // TODO: Optimize the calculation of the contribution to not re-calculate on every block
            for (const auto &p : properties_idx) {
               fc::uint128_t contribution_numerator(p.options.appraised_property_value);
               contribution_numerator *= p.scaled_allocation_progress;

               mapSymbolToContribution[p.options.backed_by_asset_symbol] += contribution_numerator;
            }

            // Update the asset limitation's cumulative_sell_limit
            std::map<std::string, fc::int128_t>::iterator it2;
            for (it2 = mapSymbolToContribution.begin(); it2 != mapSymbolToContribution.end(); it2++) {
               const string symbol = it2->first;
               const fc::int128_t contribution_numerator = it2->second;

               const fc::int128_t contribution_128 = contribution_numerator * 10 / META1_SCALED_ALLOCATION_PRECISION;
               const int64_t contribution = static_cast<int64_t>(contribution_128);

               auto itr = asset_limitation_idx.find(symbol);
               assert(itr != idx.end());
               const asset_limitation_object &alo = *itr;

               modify(alo, [&contribution](asset_limitation_object &alo) {
                  alo.cumulative_sell_limit = contribution;
               });
            } // end of looping through contributions

         }
         catch (const fc::exception &e) {
            // TODO: What should be done when an exception is encountered?
//            smooth_allocation_condition::smooth_allocation_condition_enum result;
//            result = smooth_allocation_condition::exception_allocation;
//            capture("err", e.what());
//            result_viewer(result, capture);
         }


         // TODO: Create API call to calculate current value of asset
         // TODO: Create wallet call to calculate current value of asset
      }

      // TODO: Move this function to API?
      const property_object *database::get_property(uint32_t property_id) const {
         const auto &idx = get_index_type<property_index>().indices().get<by_property_id>();
         auto itr = idx.find(property_id);
         if (itr != idx.end()) {
            const property_object *property = &*itr;
            return property;
         } else {
            return nullptr;
         }
      }

   }
}