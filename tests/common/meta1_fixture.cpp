/*
 * Copyright META1 (c) 2020
 */

#include <random>

#include <boost/test/unit_test.hpp>

#include <graphene/chain/hardfork.hpp>

#include "meta1_fixture.hpp"


using namespace graphene::chain;
using namespace graphene::chain::test;

namespace graphene {
   namespace chain {

      bool meta1_fixture::is_property_exists(uint32_t property_id) const {
         const auto &idx = db.get_index_type<property_index>().indices().get<by_property_id>();
         auto itr = idx.find(property_id);
         if (itr != idx.end())
            return true;
         else
            return false;
      }

      property_create_operation meta1_fixture::create_property_operation(const string issuer,
                                                                         const uint64_t appraised_property_value,
                                                                         const uint32_t allocation_duration_minutes,
                                                                         const string backed_by_asset_symbol,
                                                                         const property_options common) const {
         try {
            // Invoke get_asset to check the existence of the backed_by_asset_symbol
            get_asset(backed_by_asset_symbol);
            // FC_ASSERT(*backing_asset == nullptr, "Asset with that symbol not exists!");
            std::random_device rd;
            std::mt19937 mersenne(rd());
            uint32_t rand_id;
            const account_object &issuer_account = get_account(issuer);
            property_create_operation create_op;

            //generating new property id and regenerate if such id is exists
            while (true) {
               rand_id = mersenne();
               bool flag = is_property_exists(rand_id);
               if (!flag) { break; }
            }

            create_op.property_id = rand_id;
            create_op.issuer = issuer_account.id;
            create_op.appraised_property_value = appraised_property_value;
            create_op.allocation_duration_minutes = allocation_duration_minutes;
            create_op.backed_by_asset_symbol = backed_by_asset_symbol;
            create_op.common_options = common;

            return create_op;
         }
         FC_CAPTURE_AND_RETHROW()
      }


      const uint64_t
      meta1_fixture::calculate_meta1_valuation(uint64_t appraised_value,
                                               const uint64_t progress_numerator,
                                               const uint64_t progress_denominator) {
         BOOST_CHECK_LE(progress_numerator, progress_denominator);
         // The arithmetic will be sequenced to reduce loss of precision errors
         // Note: Precision is lost during division
         // Note: Multiplication can result in an overflow of the numerical type
         // Therefore the following arithmetic rules will be followed:
         // - variables that will be multiplied will expressed within a higher precision numerical type before multiplication
         // - after multiplication, check for overflow
         // - division is to be minimized, to the extent possible
         // - division is to be delayed, to the extent possible, until after all multiplications are completed

         // The total appraisal sums the appraisal of every property
         // This test contains only one property

         // Overflow checks during arithmetic is performed with the fc::safe struct
         const safe<fc::uint128_t> appraised_value_128(appraised_value);
         const safe<fc::uint128_t> safe_meta1_valuation_128 = (appraised_value_128 * 10 * progress_numerator);
         // "Un-safe" before division to avoid compiler warning
         fc::uint128_t meta1_valuation_128 = safe_meta1_valuation_128.value;
         meta1_valuation_128 /= progress_denominator;

         uint64_t meta1_valuation = static_cast<uint64_t>(meta1_valuation_128);

         return meta1_valuation;
      }


      const uint64_t meta1_fixture::calculate_meta1_valuation(uint64_t appraised_value, const ratio_type progress) {
         return calculate_meta1_valuation(appraised_value, progress.numerator(), progress.denominator());
      }


      void meta1_fixture::publish_asset_price(const string asset_symbol, const price_ratio price,
                                              const account_id_type authorizing_id,
                                              const private_key_type authorizing_private_key) {
         asset_price_publish_operation publish_op;
         publish_op.symbol = asset_symbol;
         publish_op.usd_price = price;
         publish_op.fee_paying_account = authorizing_id;

         trx.clear();
         trx.operations.push_back(publish_op);
         sign(trx, authorizing_private_key);

         PUSH_TX(db, trx);

         trx.clear();
      }


      void meta1_fixture::initialize_allocation_object(const string asset_to_back, const account_id_type authorizing_id,
                                                       const private_key_type authorizing_private_key) {
         const auto &asset_limitation_idx = db.get_index_type<asset_limitation_index>().indices().get<by_limit_symbol>();
         auto itr = asset_limitation_idx.find(asset_to_back);
         if (itr != asset_limitation_idx.end()) {
            // The object already exists
            return;
         }

         asset_limitation_object_create_operation creator;

         creator.issuer = authorizing_id;
         creator.fee = asset();
         creator.limit_symbol = asset_to_back;

         trx.clear();
         trx.operations.push_back(std::move(creator));
         sign(trx, authorizing_private_key);

         PUSH_TX(db, trx, ~0);

         trx.clear();
      }

      const property_id_type meta1_fixture::allocate_property(const uint64_t appraised_property_value,
                                                              const uint32_t allocation_duration_minutes,
                                                              const property_options options,
                                                              const account_id_type authorizing_id,
                                                              const private_key_type authorizing_private_key) {

         const string asset_to_back = GRAPHENE_SYMBOL;
         initialize_allocation_object(asset_to_back, authorizing_id, authorizing_private_key);

         const string issuer = "meta1";
         property_create_operation prop_op = create_property_operation(issuer, appraised_property_value,
                                                                       allocation_duration_minutes, asset_to_back,
                                                                       options);
         trx.clear();
         trx.operations.push_back(prop_op);
         set_expiration(db, trx);
         sign(trx, authorizing_private_key);

         PUSH_TX(db, trx);


         /**
          * Approve the asset by using the update operation
          */
         graphene::chain::property_object property = db.get_property(prop_op.property_id);
         const auto prop_id = property.get_id();
         property_approve_operation aop;
         aop.issuer = authorizing_id;
         aop.property_to_approve = property.id;

         trx.clear();
         trx.operations.push_back(aop);
         sign(trx, authorizing_private_key);

         PUSH_TX(db, trx);


         /**
          * Advance to the 100% moment
          * The appreciation should be at 100%
          */
         property = db.get_property(prop_op.property_id);
         const uint32_t allocation_duration_seconds = (property.approval_end_date.sec_since_epoch() -
                                                       property.creation_date.sec_since_epoch());
         const time_point_sec time_to_100_percent = property.creation_date + (allocation_duration_seconds) * 4 / 4;
         generate_blocks(time_to_100_percent, false);
         set_expiration(db, trx);
         trx.clear();

         return prop_id;
      }


      const property_id_type meta1_fixture::allocate_property(const uint64_t appraised_property_value,
                                                              const uint32_t allocation_duration_minutes,
                                                              const account_id_type authorizing_id,
                                                              const private_key_type authorizing_private_key) {
         // Create the property
         const property_options property_options = {
                 "some description",
                 "some title",
                 "my@email.com",
                 "you",
                 "https://fsf.com",
                 "https://purepng.com/metal-1701528976849tkdsl.png",
                 "222",
                 1,
                 33104,
         };

         return allocate_property(appraised_property_value, allocation_duration_minutes, property_options,
                                  authorizing_id, authorizing_private_key);
      }

   }
}
