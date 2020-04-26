/*
 * Copyright META1 (c) 2020
 */

#include <stdlib.h>
#include <random>

#include <boost/test/unit_test.hpp>

#include <graphene/app/database_api.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/hardfork.hpp>

#include <graphene/chain/balance_object.hpp>
#include <graphene/chain/budget_record_object.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/withdraw_permission_object.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/worker_object.hpp>

#include <graphene/witness/witness.hpp>
#include <graphene/smooth_allocation/smooth_allocation_plugin.hpp>

#include <graphene/utilities/tempdir.hpp>

#include <fc/uint128.hpp>
#include <fc/crypto/digest.hpp>

#include "../common/database_fixture.hpp"

#define META1_SCALED_ALLOCATION_TOLERANCE int64_t(100000000l)

using namespace graphene::chain;
using namespace graphene::chain::test;


BOOST_FIXTURE_TEST_SUITE(smooth_allocation_tests, database_fixture)

   const void check_close(double v1, double v2, double tol) {
      BOOST_CHECK_LE(abs(v1 - v2), tol);
   }

   const int64_t get_asset_supply(asset_object asset) {
      //  Platform- and compiler-independent calculation
      return asset.options.max_supply.value / asset::scaled_precision(asset.precision).value;
   }

   // TODO [7]: Use existing functions in database_api or elsewhere
   const asset_object &get_asset(database &db, const string &symbol) {
      const auto &idx = db.get_index_type<asset_index>().indices().get<by_symbol>();
      const auto itr = idx.find(symbol);
      assert(itr != idx.end());
      return *itr;
   }

   const account_object &get_account(database &db, const string &name) {
      const auto &idx = db.get_index_type<account_index>().indices().get<by_name>();
      const auto itr = idx.find(name);
      assert(itr != idx.end());
      return *itr;
   }

   bool is_property_exists(database &db, uint32_t property_id) {
      // const property_object *property = nullptr;
      const auto &idx = db.get_index_type<property_index>().indices().get<by_property_id>();
      auto itr = idx.find(property_id);
      if (itr != idx.end())
         return true;
      else
         return false;
   }

   const uint64_t calculate_meta1_valuation(uint64_t appraised_value, const uint64_t progress_numerator,
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

      // Let the fractional progress be composed of a numerator, N_i, for propoerty "i",
      // and a common denominator, D, which equals META1_SCALED_ALLOCATION_PRECISION.
      // Let appraisal_numerator = appraised_value * numerator_progress
      fc::uint128_t appraised_value_128(appraised_value);

      // TODO: Check overflow of progress_fraction
//      uint64_t progress_fraction = (META1_SCALED_ALLOCATION_PRECISION * progress_numerator) / progress_denominator;
      fc::uint128_t progress_fraction(META1_SCALED_ALLOCATION_PRECISION);
      progress_fraction *= progress_numerator;
      progress_fraction /= progress_denominator;

      fc::uint128_t appraisal_numerator = appraised_value_128 * progress_fraction;
      // The total appraisal sums the appraisal of every property
      // This test contains only one property
      fc::uint128_t total_appraisal_numerator = appraisal_numerator;
      fc::uint128_t meta1_valuation_numerator = 10 * total_appraisal_numerator;
      fc::uint128_t meta1_valuation_128 = meta1_valuation_numerator / META1_SCALED_ALLOCATION_PRECISION;
      uint64_t meta1_valuation = static_cast<uint64_t>(meta1_valuation_128);

      return meta1_valuation;
   }

   property_create_operation create_property_operation(database &db, string issuer,
                                                       property_options common) {
      try {
         // const asset_object& backing_asset = get_asset(db, common.backed_by_asset_symbol);
         get_asset(db,
                   common.backed_by_asset_symbol); // Invoke for the function call to check the existence of the backed_by_asset_symbol
//         FC_ASSERT(*backing_asset == nullptr, "Asset with that symbol not exists!");
         std::random_device rd;
         std::mt19937 mersenne(rd());
         uint32_t rand_id;
         const account_object &issuer_account = get_account(db, issuer);
         property_create_operation create_op;

         //generating new property id and regenerate if such id is exists
         while (true) {
            rand_id = mersenne();
            bool flag = is_property_exists(db, rand_id);
            if (!flag) { break; }
         }

         create_op.property_id = rand_id;
         create_op.issuer = issuer_account.id;
         create_op.common_options = common;

         return create_op;
      }
      FC_CAPTURE_AND_RETHROW()
   }


   /**
    * Case A / Case C4
    * A property is created but never approved
    * Its progress should increase to 25%,
    * then remain at 25% until it drops to 0% after the full vesting period of 1 week
    */
   BOOST_AUTO_TEST_CASE(appreciate_initial_allocation) {
      // Initialize the actors
      ACTORS((nathan)(meta1));
      upgrade_to_lifetime_member(meta1_id);

      // Advance to when the smooth allocation is activated
      generate_blocks(HARDFORK_CORE_21_TIME);
      generate_blocks(6); // Seems to prime the test fixture better

      // Create the asset limitation
      const string limit_symbol = "META1";
      const asset_limitation_options asset_limitation_ops = {
              "0.00000000000000",
              "0.0000000000000001",
      };

      account_object issuer_account = get_account("meta1");

      asset_limitation_object_create_operation create_limitation_op;
      create_limitation_op.limit_symbol = limit_symbol;
      create_limitation_op.issuer = issuer_account.id;
      create_limitation_op.common_options = asset_limitation_ops;

      signed_transaction tx;
      tx.operations.push_back(create_limitation_op);
      set_expiration(db, tx);

      // TODO: Test that only signatures by meta1 are sufficient
      sign(tx, meta1_private_key);

      PUSH_TX(db, tx);


      // Create the property
      const property_options property_ops = {
              "some description",
              "some title",
              "my@email.com",
              "you",
              "https://fsf.com",
              "https://purepng.com/metal-1701528976849tkdsl.png",
              "not approved",
              "222",
              1000000000,
              1,
              33104,
              "1",
              "0.000000000000",
              "META1",
      };
      property_create_operation prop_op = create_property_operation(db, "meta1", property_ops);

      tx.clear();
      tx.operations.push_back(prop_op);
      set_expiration(db, tx);

      // TODO: Test that only signatures by meta1 are sufficient
      sign(tx, meta1_private_key);

      PUSH_TX(db, tx);
      generate_blocks(1);
      generate_blocks(1);

      // Check the initial allocation of the asset before any blocks advance
      const graphene::chain::property_object *property = db.get_property(prop_op.property_id);

      double_t progress = 0.0;
      BOOST_CHECK(boost::lexical_cast<double_t>(property->options.allocation_progress) == progress);

      asset_limitation_object alo;
      const auto &asset_limitation_idx = db.get_index_type<asset_limitation_index>().indices().get<by_limit_symbol>();
      uint64_t expected_valuation;


      //////
      // Advance the blockchain to after the allocation of the initial value
      // This will be any time after the property's 25% time and before the 100% time
      //////

      //////
      // Advance to the 25% moment
      // The appreciation should be at 25%
      //////
      // TODO: Change smooth_allocation_time to uint and rename to weeks
      uint32_t initial_duration_seconds =
              0.25 * (boost::lexical_cast<double_t>(property->options.smooth_allocation_time) * 7 * 24 * 60 * 60);
      time_point_sec time_to_25_percent = property->date_creation + initial_duration_seconds;
      generate_blocks(time_to_25_percent, false);
      set_expiration(db, trx);
      trx.operations.clear();

      property = db.get_property(prop_op.property_id);

      // const asset_object& backing_asset = get_asset(limit_symbol);
      // const double_t price_25_percent = 0.25 * ((double)property->options.appraised_property_value / get_asset_supply(backing_asset));
      const uint64_t scaled_25_percent = 0.25 * META1_SCALED_ALLOCATION_PRECISION;
      check_close(scaled_25_percent, property->scaled_allocation_progress, 0.25 * META1_SCALED_ALLOCATION_TOLERANCE);

      // Check the sell_limit
      alo = *asset_limitation_idx.find(limit_symbol);
      expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 25, 100);
      fc::uint128_t value128(property->options.appraised_property_value);
      uint64_t tol = value128 * META1_SCALED_ALLOCATION_TOLERANCE / META1_SCALED_ALLOCATION_PRECISION;
      check_close(expected_valuation, alo.cumulative_sell_limit, tol);


      //////
      // Advance to the 50% moment
      // The appreciation should be at 25%
      //////
      time_point_sec time_to_50_percent = property->date_creation + (0.5 * (boost::lexical_cast<double_t>(
              property->options.smooth_allocation_time) * 7 * 24 * 60 * 60));
      generate_blocks(time_to_50_percent, false);
      set_expiration(db, trx);
      trx.operations.clear();

      property = db.get_property(prop_op.property_id);

      check_close(scaled_25_percent, property->scaled_allocation_progress, 0.25 * META1_SCALED_ALLOCATION_TOLERANCE);

      // Check the sell_limit
      alo = *asset_limitation_idx.find(limit_symbol);
      expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 25, 100);
      value128 = property->options.appraised_property_value;
      tol = value128 * META1_SCALED_ALLOCATION_TOLERANCE / META1_SCALED_ALLOCATION_PRECISION;
      check_close(expected_valuation, alo.cumulative_sell_limit, tol);


      //////
      // Advance to the 75% moment
      // The appreciation should be at 25%
      //////
      time_point_sec time_to_75_percent = property->date_creation + (0.75 * (boost::lexical_cast<double_t>(
              property->options.smooth_allocation_time) * 7 * 24 * 60 * 60));
      generate_blocks(time_to_75_percent, false);
      set_expiration(db, trx);
      trx.operations.clear();

      property = db.get_property(prop_op.property_id);

      check_close(scaled_25_percent, property->scaled_allocation_progress, 0.25 * META1_SCALED_ALLOCATION_TOLERANCE);

      // Check the sell_limit
      alo = *asset_limitation_idx.find(limit_symbol);
      expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 25, 100);
      value128 = property->options.appraised_property_value;
      tol = value128 * META1_SCALED_ALLOCATION_TOLERANCE / META1_SCALED_ALLOCATION_PRECISION;
      check_close(expected_valuation, alo.cumulative_sell_limit, tol);


      //////
      // Advance to the 100% moment
      // The appreciation should be at 0%
      //////
      time_point_sec time_to_100_percent = property->date_creation + (1.00 * (boost::lexical_cast<double_t>(
              property->options.smooth_allocation_time) * 7 * 24 * 60 * 60));
      generate_blocks(time_to_100_percent, false);
      set_expiration(db, trx);
      trx.operations.clear();

      property = db.get_property(prop_op.property_id);

      const uint64_t scaled_0_percent = 0.0 * META1_SCALED_ALLOCATION_PRECISION;
      check_close(scaled_0_percent, property->scaled_allocation_progress, 0.00 * META1_SCALED_ALLOCATION_TOLERANCE);

      // Check the sell_limit
      alo = *asset_limitation_idx.find(limit_symbol);
      expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 0, 100);
      value128 = property->options.appraised_property_value;
      tol = value128 * META1_SCALED_ALLOCATION_TOLERANCE / META1_SCALED_ALLOCATION_PRECISION;
      check_close(expected_valuation, alo.cumulative_sell_limit, tol);


      //////
      // Advance to the 125% moment
      // The appreciation should be at 0%
      //////
      time_point_sec time_to_125_percent = property->date_creation + (1.25 * (boost::lexical_cast<double_t>(
              property->options.smooth_allocation_time) * 7 * 24 * 60 * 60));
      generate_blocks(time_to_125_percent, false);
      set_expiration(db, trx);
      trx.clear();

      property = db.get_property(prop_op.property_id);

      check_close(scaled_0_percent, property->scaled_allocation_progress, 0.00 * META1_SCALED_ALLOCATION_TOLERANCE);

      // Check the sell_limit
      alo = *asset_limitation_idx.find(limit_symbol);
      expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 0, 100);
      value128 = property->options.appraised_property_value;
      tol = value128 * META1_SCALED_ALLOCATION_TOLERANCE / META1_SCALED_ALLOCATION_PRECISION;
      check_close(expected_valuation, alo.cumulative_sell_limit, tol);
   }


   /**
    * Case C1
    * A property is created and approved during the inital allocation period.
    * Its progress should increase to 100%
    * at the same rate during the entire vesting period of 1 week
    */
   BOOST_AUTO_TEST_CASE(approve_during_initial_allocation) {
      try {
         // Initialize the actors
         ACTORS((nathan)(meta1));
         upgrade_to_lifetime_member(meta1_id);

         // Advance to when the smooth allocation is activated
         generate_blocks(HARDFORK_CORE_21_TIME);
         generate_blocks(6); // Seems to prime the test fixture better

         // Create the asset limitation
         const string limit_symbol = "META1";
         const asset_limitation_options asset_limitation_ops = {
                 "0.00000000000000",
                 "0.0000000000000001",
         };

         account_object issuer_account = get_account("meta1");

         asset_limitation_object_create_operation create_limitation_op;
         create_limitation_op.limit_symbol = limit_symbol;
         create_limitation_op.issuer = issuer_account.id;
         create_limitation_op.common_options = asset_limitation_ops;

         trx.operations.push_back(create_limitation_op);
         set_expiration(db, trx);

         // TODO: Test that only signatures by meta1 are sufficient
         sign(trx, meta1_private_key);

         PUSH_TX(db, trx);


         // Create the property
         const property_options property_ops = {
                 "some description",
                 "some title",
                 "my@email.com",
                 "you",
                 "https://fsf.com",
                 "https://purepng.com/metal-1701528976849tkdsl.png",
                 "not approved",
                 "222",
                 1000000000,
                 1,
                 33104,
                 "1",
                 "0.000000000000",
                 "META1",
         };
         property_create_operation prop_op = create_property_operation(db, "meta1", property_ops);

         trx.clear();
         trx.operations.push_back(prop_op);
         set_expiration(db, trx);

         // TODO: Test that only signatures by meta1 are sufficient
         sign(trx, meta1_private_key);

         PUSH_TX(db, trx);
         generate_blocks(1);
         generate_blocks(1);


         // Check the initial allocation of the asset before any blocks advance
         const graphene::chain::property_object *property = db.get_property(prop_op.property_id);

         double_t progress = 0.0;
         BOOST_CHECK(boost::lexical_cast<double_t>(property->options.allocation_progress) == progress);

         asset_limitation_object alo;
         const auto &asset_limitation_idx = db.get_index_type<asset_limitation_index>().indices().get<by_limit_symbol>();
         uint64_t expected_valuation;


         //////
         // Advance to the 10% moment
         // The appreciation should be at 10%
         //////
         time_point_sec time_to_10_percent = property->date_creation + 0.1 * (boost::lexical_cast<double_t>(
                 property->options.smooth_allocation_time) * 7 * 24 * 60 * 60);
         generate_blocks(time_to_10_percent, false);
         set_expiration(db, trx);
         trx.clear();

         property = db.get_property(prop_op.property_id);

         const uint64_t scaled_10_percent = 0.10 * META1_SCALED_ALLOCATION_PRECISION;
         // TODO: Refine check after reducing error in progress
         check_close(scaled_10_percent, property->scaled_allocation_progress, 0.10 * META1_SCALED_ALLOCATION_TOLERANCE);

         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 10, 100);
//         BOOST_CHECK_EQUAL(expected_valuation, alo.cumulative_sell_limit);
         fc::uint128_t value128(property->options.appraised_property_value);
         uint64_t tol = value128 * META1_SCALED_ALLOCATION_TOLERANCE / META1_SCALED_ALLOCATION_PRECISION;
         check_close(expected_valuation, alo.cumulative_sell_limit, tol);


         // Approve the asset by using the update operation
         property_approve_operation aop;
         aop.issuer = meta1.id;
         aop.property_to_approve = property->id;

         trx.operations.push_back(aop);
         sign(trx, meta1_private_key);

         PUSH_TX(db, trx);


         //////
         // Advance the blockchain to after the allocation of the initial value
         // This will be any time after the property's 25% time and before the 100% time
         //////

         //////
         // Advance to the 25% moment
         // The appreciation should be at 25%
         //////
         // TODO: Change smooth_allocation_time to uint and rename to weeks
         uint32_t initial_duration_seconds =
                 0.25 * (boost::lexical_cast<double_t>(property->options.smooth_allocation_time) * 7 * 24 * 60 * 60);
         time_point_sec time_to_25_percent = property->date_creation + initial_duration_seconds;
         generate_blocks(time_to_25_percent, false);
         set_expiration(db, trx);
         trx.clear();

         property = db.get_property(prop_op.property_id);

         // const asset_object& backing_asset = get_asset(limit_symbol);
         // const double_t price_25_percent = 0.25 * ((double)property->options.appraised_property_value / get_asset_supply(backing_asset));
         const uint64_t scaled_25_percent = 0.25 * META1_SCALED_ALLOCATION_PRECISION;
         // TODO: Refine check after reducing error in progress
         check_close(scaled_25_percent, property->scaled_allocation_progress, 0.25 * META1_SCALED_ALLOCATION_TOLERANCE);

         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 25, 100);
//         BOOST_CHECK_EQUAL(expected_valuation, alo.cumulative_sell_limit);
         value128 = property->options.appraised_property_value;
         tol = value128 * META1_SCALED_ALLOCATION_TOLERANCE / META1_SCALED_ALLOCATION_PRECISION;
         check_close(expected_valuation, alo.cumulative_sell_limit, tol);


         //////
         // Advance to the 50% moment
         // The appreciation should be at 50%
         //////
         time_point_sec time_to_50_percent = property->date_creation + (0.5 * (boost::lexical_cast<double_t>(
                 property->options.smooth_allocation_time) * 7 * 24 * 60 * 60));
         generate_blocks(time_to_50_percent, false);
         set_expiration(db, trx);
         trx.clear();

         property = db.get_property(prop_op.property_id);

         const uint64_t scaled_50_percent = 0.50 * META1_SCALED_ALLOCATION_PRECISION;
         // TODO: Refine check after reducing error in progress
         check_close(scaled_50_percent, property->scaled_allocation_progress, 0.50 * META1_SCALED_ALLOCATION_TOLERANCE);

         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 50, 100);
//         BOOST_CHECK_EQUAL(expected_valuation, alo.cumulative_sell_limit);
         value128 = property->options.appraised_property_value;
         tol = value128 * META1_SCALED_ALLOCATION_TOLERANCE / META1_SCALED_ALLOCATION_PRECISION;
         check_close(expected_valuation, alo.cumulative_sell_limit, tol);


         //////
         // Advance to the 75% moment
         // The appreciation should be at 75%
         //////
         time_point_sec time_to_75_percent = property->date_creation + (0.75 * (boost::lexical_cast<double_t>(
                 property->options.smooth_allocation_time) * 7 * 24 * 60 * 60));
         generate_blocks(time_to_75_percent, false);
         set_expiration(db, trx);
         trx.clear();

         property = db.get_property(prop_op.property_id);

         const uint64_t scaled_75_percent = 0.75 * META1_SCALED_ALLOCATION_PRECISION;
         // TODO: Refine check after reducing error in progress
         check_close(scaled_75_percent, property->scaled_allocation_progress, 0.75 * META1_SCALED_ALLOCATION_TOLERANCE);

         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 75, 100);
//         BOOST_CHECK_EQUAL(expected_valuation, alo.cumulative_sell_limit);
         value128 = property->options.appraised_property_value;
         tol = value128 * META1_SCALED_ALLOCATION_TOLERANCE / META1_SCALED_ALLOCATION_PRECISION;
         check_close(expected_valuation, alo.cumulative_sell_limit, tol);


         //////
         // Advance to the 100% moment
         // The appreciation should be at 100%
         //////
         time_point_sec time_to_100_percent = property->date_creation + (1.00 * (boost::lexical_cast<double_t>(
                 property->options.smooth_allocation_time) * 7 * 24 * 60 * 60));
         generate_blocks(time_to_100_percent, false);
         set_expiration(db, trx);
         trx.clear();

         property = db.get_property(prop_op.property_id);

         const uint64_t scaled_100_percent = 1.0 * META1_SCALED_ALLOCATION_PRECISION;
         check_close(scaled_100_percent, property->scaled_allocation_progress,
                     1.00 * META1_SCALED_ALLOCATION_TOLERANCE);

         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 100, 100);
         BOOST_CHECK_EQUAL(expected_valuation, alo.cumulative_sell_limit);


         //////
         // Advance to the 125% moment
         // The appreciation should be at 100%
         //////
         time_point_sec time_to_125_percent = property->date_creation + (1.25 * (boost::lexical_cast<double_t>(
                 property->options.smooth_allocation_time) * 7 * 24 * 60 * 60));
         generate_blocks(time_to_125_percent, false);
         set_expiration(db, trx);
         trx.clear();

         property = db.get_property(prop_op.property_id);

         check_close(scaled_100_percent, property->scaled_allocation_progress,
                     1.00 * META1_SCALED_ALLOCATION_TOLERANCE);

         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 100, 100);
         BOOST_CHECK_EQUAL(expected_valuation, alo.cumulative_sell_limit);

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Case C2
    * A property is created and approved after the inital allocation period at 65% of the vesting timeline.
    * Its progress should increase to 25% progress by 25% of the timeline.
    * Progress should remain at 25% from 25%-65% of the timeline.
    * Progress should increase from 25% through 100% during the 65%-100% of the timeline.
    */
   BOOST_AUTO_TEST_CASE(approve_after_initial_allocation) {
      try {
         // Initialize the actors
         ACTORS((nathan)(meta1));
         upgrade_to_lifetime_member(meta1_id);

         // Advance to when the smooth allocation is activated
         generate_blocks(HARDFORK_CORE_21_TIME);
         generate_blocks(6); // Seems to prime the test fixture better

         // Create the asset limitation
         const string limit_symbol = "META1";
         const asset_limitation_options asset_limitation_ops = {
                 "0.00000000000000",
                 "0.0000000000000001",
         };

         account_object issuer_account = get_account("meta1");

         asset_limitation_object_create_operation create_limitation_op;
         create_limitation_op.limit_symbol = limit_symbol;
         create_limitation_op.issuer = issuer_account.id;
         create_limitation_op.common_options = asset_limitation_ops;

         trx.operations.push_back(create_limitation_op);
         set_expiration(db, trx);

         // TODO: Test that only signatures by meta1 are sufficient
         sign(trx, meta1_private_key);

         PUSH_TX(db, trx);


         // Create the property
         const property_options property_ops = {
                 "some description",
                 "some title",
                 "my@email.com",
                 "you",
                 "https://fsf.com",
                 "https://purepng.com/metal-1701528976849tkdsl.png",
                 "not approved",
                 "222",
                 1000000000,
                 1,
                 33104,
                 "1",
                 "0.000000000000",
                 "META1",
         };
         property_create_operation prop_op = create_property_operation(db, "meta1", property_ops);

         trx.clear();
         trx.operations.push_back(prop_op);
         set_expiration(db, trx);

         // TODO: Test that only signatures by meta1 are sufficient
         sign(trx, meta1_private_key);

         PUSH_TX(db, trx);
         generate_blocks(1);
         generate_blocks(1);


         // Check the initial allocation of the asset before any blocks advance
         const graphene::chain::property_object *property = db.get_property(prop_op.property_id);

         double_t progress = 0.0;
         BOOST_CHECK(boost::lexical_cast<double_t>(property->options.allocation_progress) == progress);

         asset_limitation_object alo;
         const auto &asset_limitation_idx = db.get_index_type<asset_limitation_index>().indices().get<by_limit_symbol>();
         uint64_t expected_valuation;


         //////
         // Advance to the 65% moment
         // The appreciation should be at 25%
         //////
         const uint64_t scaled_25_percent = 0.25 * META1_SCALED_ALLOCATION_PRECISION;

         time_point_sec time_to_65_percent = property->date_creation + 0.65 * (boost::lexical_cast<double_t>(
                 property->options.smooth_allocation_time) * 7 * 24 * 60 * 60);
         time_point_sec time_approval = time_to_65_percent;
         generate_blocks(time_to_65_percent, false);
         set_expiration(db, trx);
         trx.clear();

         property = db.get_property(prop_op.property_id);

         check_close(scaled_25_percent, property->scaled_allocation_progress, 0.25 * META1_SCALED_ALLOCATION_TOLERANCE);

         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 25, 100);
         fc::uint128_t value128(property->options.appraised_property_value);
         uint64_t tol = value128 * META1_SCALED_ALLOCATION_TOLERANCE / META1_SCALED_ALLOCATION_PRECISION;
         check_close(expected_valuation, alo.cumulative_sell_limit, tol);


         //////
         // Approve the asset by using the update operation
         //////
         property_approve_operation aop;
         aop.issuer = meta1.id;
         aop.property_to_approve = property->id;

         trx.operations.push_back(aop);
         sign(trx, meta1_private_key);

         PUSH_TX(db, trx);

         // Calculate the new rate of allocation after approval
         generate_blocks(1);
         uint64_t scaled_remaining_allocation = floor(0.75 * META1_SCALED_ALLOCATION_PRECISION);
         uint64_t remaining_allocation_duration_seconds = (property->date_approval_deadline -
                                                           time_approval).to_seconds();
         uint64_t remaining_allocation_duration_minutes = remaining_allocation_duration_seconds / 60;
         uint64_t accelerated_scaled_allocation_per_minute =
                 scaled_remaining_allocation / remaining_allocation_duration_minutes;


         //////
         // Advance the blockchain to after the approval
         // This will be any time after the property's 65% time and before the 100% time
         //////
         //////
         // Advance to the 75% moment
         // The appreciation should be at approximately 46.4%
         //////
         time_point_sec time_to_75_percent = property->date_creation + (0.75 * (boost::lexical_cast<double_t>(
                 property->options.smooth_allocation_time) * 7 * 24 * 60 * 60));
         ilog(" Current time: ${time}", ("time", db.head_block_time()));
         generate_blocks(time_to_75_percent, false);
         set_expiration(db, trx);
         trx.clear();

         property = db.get_property(prop_op.property_id);

         // const uint64_t scaled_75_percent = (0.25 + 0.214285714286) * META1_SCALED_ALLOCATION_PRECISION; // Manual calculation
         double duration_since_approval_minutes = (db.head_block_time() - time_approval).to_seconds() / 60.0;
         double scaled_allocation_since_approval =
                 duration_since_approval_minutes * accelerated_scaled_allocation_per_minute;
         const uint64_t scaled_75_percent =
                 (0.25 * META1_SCALED_ALLOCATION_PRECISION) + scaled_allocation_since_approval;
         check_close(scaled_75_percent, property->scaled_allocation_progress, 1.0 * META1_SCALED_ALLOCATION_TOLERANCE);

         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, scaled_75_percent,
                                                        META1_SCALED_ALLOCATION_PRECISION);
         value128 = property->options.appraised_property_value;
         tol = value128 * META1_SCALED_ALLOCATION_TOLERANCE / META1_SCALED_ALLOCATION_PRECISION;
         check_close(expected_valuation, alo.cumulative_sell_limit, tol);


         //////
         // Advance to the 100% moment
         // The appreciation should be at 100%
         //////
         time_point_sec time_to_100_percent = property->date_creation + (1.00 * (boost::lexical_cast<double_t>(
                 property->options.smooth_allocation_time) * 7 * 24 * 60 * 60));
         generate_blocks(time_to_100_percent, false);
         set_expiration(db, trx);
         trx.clear();

         property = db.get_property(prop_op.property_id);

         const uint64_t scaled_100_percent = 1.0 * META1_SCALED_ALLOCATION_PRECISION;
         check_close(scaled_100_percent, property->scaled_allocation_progress,
                     1.00 * META1_SCALED_ALLOCATION_TOLERANCE);

         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 100, 100);
         BOOST_CHECK_EQUAL(expected_valuation, alo.cumulative_sell_limit);


         //////
         // Advance to the 125% moment
         // The appreciation should be at 100%
         //////
         time_point_sec time_to_125_percent = property->date_creation + (1.25 * (boost::lexical_cast<double_t>(
                 property->options.smooth_allocation_time) * 7 * 24 * 60 * 60));
         generate_blocks(time_to_125_percent, false);
         set_expiration(db, trx);
         trx.clear();

         property = db.get_property(prop_op.property_id);

         check_close(scaled_100_percent, property->scaled_allocation_progress,
                     1.00 * META1_SCALED_ALLOCATION_TOLERANCE);

         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 100, 100);
         BOOST_CHECK_EQUAL(expected_valuation, alo.cumulative_sell_limit);

      } FC_LOG_AND_RETHROW()
   }

   // TODO: Test for late approval on the same block as the approval deadline
   // TODO: Test for approval on the block before the approval deadline

   /**
    * The initial parameters for an appreciation period of property with a given vesting duration
    * will defined such that increments of the appreciation from 0% to 100% will occur at "standard one-minute intervals"
    * which are defined as occurring on the "0 second" mark of a minute.
    *
    * This implies that the discrete quantity of increments (total_increments) can be pre-calculated,
    * and thereafter all processing on the "0 second" mark will increment the appreciation (progress) by 1
    * such that the fractional progress, at a conceptual level, can be calculated as
    *
    * fractional_progress = progress / total_increments.
    *
    * This fractional progress is a rational fraction (i.e. without any rounding or truncation).
    *
    * The general concept becomes more intricate given that another requirement of the smart contracts permits
    * the appreciation to be paused at 25% if the property's valuation is not approved by 25% of the timeline
    * (i.e. the entire vesting duration).  Subsequent approval may be granted at any time between 25% and 100%
    * of the timeline at which time the appreciation restarts but at an accelerated rate
    * such that the remaining 75% of appreciation should complete by 100% of the timeline (i.e. the approval deadline).
    *
    * To prevent any numerical loss for "late approvals", an additional discrete quantity of increments
    * for the approval period shall be calculated (total_approval_increments)
    * separate from the quantity required during the initial period (total_initial_increments).  Similarly, two separate
    * progress variables will be during the initial period (progress_initial) and the approval period (progress_approval).
    *
    * With this arrangement, the fractional progress can be calculated at any time as a rational fraction according to
    *
    * fractional_progress = progress_initial / total_increments_initial + progress_approval / total_increments_approval
    *
    * If a "late approval" occurs, the total_increments_approval will be appropriate calculated
    * for the time of the late approval.
    */

   /**
    * Parameters for initializing an appreciation period for a property
    *
    * The parameters for the 100% initial period are valid if the appraisal approval is granted before the initial
    * period completes.  In which case smooth appreciation can continue without any interruption.
    */
   struct appreciation_period_parameters {
      time_point_sec time_to_25_percent;
      uint32_t time_to_25_percent_intervals;

      time_point_sec time_to_100_percent;
      uint32_t time_to_100_percent_intervals;
   };

   /**
    * Calculate the next start time based on intervals that are calculated relative to 1970-01-01T00:00:00Z
    * @param current_time   Current time
    * @param epoch  Epoch time
    * @param interval_seconds   Interval that should be used relative to reference time
    * @return   Next start time
    */
   static time_point_sec calc_next_start_time(const time_point_sec& current_time, const time_point_sec &epoch,
                                              const uint32_t interval_seconds) {
      const uint32_t duration_since_epoch_seconds =
              current_time.sec_since_epoch() - epoch.sec_since_epoch();
      const uint32_t intervals_since_epoch = duration_since_epoch_seconds / interval_seconds;
      const uint32_t next_interval = intervals_since_epoch + 1;
      const uint32_t duration_to_next_interval_seconds = next_interval * interval_seconds;

      const time_point_sec next_start_time = epoch + duration_to_next_interval_seconds;

      return next_start_time;
   }

   /**
    * Calculate the preceding start time based on intervals that are calculated relative to 1970-01-01T00:00:00Z
    * @param current_time   Current time
    * @param epoch   Epoch time
    * @param interval_seconds   Interval that should be used relative to reference time
    * @return   Next start time
    */
   static time_point_sec calc_preceding_start_time(const time_point_sec& current_time, const time_point_sec &epoch,
                                              const uint32_t interval_seconds) {
      const uint32_t duration_since_epoch_seconds =
              current_time.sec_since_epoch() - epoch.sec_since_epoch();
      const uint32_t intervals_since_epoch = duration_since_epoch_seconds / interval_seconds;
      const uint32_t duration_to_preceding_interval_seconds = intervals_since_epoch * interval_seconds;

      const time_point_sec next_start_time = epoch + duration_to_preceding_interval_seconds;

      return next_start_time;
   }


   static const time_point_sec META1_REFERENCE_TIME = time_point_sec(); // 1970-01-01T00:00:00Z
   static const uint32_t META1_INTERVAL_BETWEEN_ALLOCATION_SECONDS = 60;

   /**
    * Calculate the standard META1 next start time
    *
    * @param current_time   Current time
    * @return   Next standard start time
    */
   static time_point_sec calc_meta1_next_start_time(const time_point_sec current_time) {
      return calc_next_start_time(current_time, META1_REFERENCE_TIME, META1_INTERVAL_BETWEEN_ALLOCATION_SECONDS);
   }

   /**
    * Calculate the standard META1 preceding start time
    *
    * @param current_time   Current time
    * @return   Preceding standard start time
    */
   static time_point_sec calc_meta1_preceding_start_time(const time_point_sec current_time) {
      return calc_preceding_start_time(current_time, META1_REFERENCE_TIME, META1_INTERVAL_BETWEEN_ALLOCATION_SECONDS);
   }

   /**
    * Calculate a start time based on intervals that are calculated relative to 1970-01-01T00:00:00Z
    * @param current_time   Current time
    * @param duration_seconds Duration of vesting
    * @return
    */
   static appreciation_period_parameters
   calc_meta1_allocation_initial_parameters(const time_point_sec &start_time, const uint32_t duration_seconds) {
      // Check the start time
      const uint32_t duration_since_reference_seconds =
              start_time.sec_since_epoch() - META1_REFERENCE_TIME.sec_since_epoch();
      const uint32_t remainder_of_intervals_since_reference_time =
              duration_since_reference_seconds % META1_INTERVAL_BETWEEN_ALLOCATION_SECONDS;
      FC_ASSERT(remainder_of_intervals_since_reference_time == 0);

      // Check the duration
      const uint32_t remainder_of_duration = duration_seconds % META1_INTERVAL_BETWEEN_ALLOCATION_SECONDS;
      FC_ASSERT(remainder_of_duration == 0);

      // Calculate the standard META1 parameters
      appreciation_period_parameters p;

      // Calculate parameters for the 100% time
      const time_point_sec end_time = start_time + duration_seconds;
      p.time_to_100_percent = end_time;
      p.time_to_100_percent_intervals = duration_seconds / META1_INTERVAL_BETWEEN_ALLOCATION_SECONDS;

      // Calculate parameters for the 25% time
      const uint32_t duration_to_25_percent = duration_seconds / 4;
      // TODO: ?Check if the duration is a multiple of the standard appreciation intervals?; test with a duration that is not evenly divisible by 4
      p.time_to_25_percent = start_time + duration_to_25_percent;
      p.time_to_25_percent_intervals = p.time_to_100_percent_intervals / 4;

      return p;
   }


   /**
    * Parameters for restarting appreciation after the 25% time for a property
    */
   struct appreciation_restart_parameters {
      time_point_sec restart_time;
      uint32_t intervals_to_end;
   };


   /**
    * Calculate new parameters for restarting the allocation
    * @param current_time   Current time
    * @param end_time Scheduled end time
    * @return Parameters that are appropriate between now and the scheduled end time
    */
   static appreciation_restart_parameters
   calc_meta1_allocation_restart_parameters(const time_point_sec &current_time, const time_point_sec &end_time) {
      // Check for invalid current times
      FC_ASSERT(current_time.sec_since_epoch() < end_time.sec_since_epoch());

      // Calculate the implied preceding "start" time
      time_point_sec restart_time = calc_meta1_preceding_start_time(current_time);
      const uint32_t duration_seconds = end_time.sec_since_epoch() - restart_time.sec_since_epoch();

      // Calculate the intervals to the end
      const uint32_t intervals_to_end = duration_seconds / META1_INTERVAL_BETWEEN_ALLOCATION_SECONDS;

      // Package the parameters
      appreciation_restart_parameters p;
      p.restart_time = restart_time;
      p.intervals_to_end = intervals_to_end;

      return p;
   }


   /**
    * Evaluates the appreciation parameters when initializing and restarting after the 25% time
    * of a 52-week vesting duration
    *
    * Values can be checked with Linux commands similar to
    * date -u --date='00:00:00 January 1, 2020' +%s
    * date -u --date='@1577836800'
    */
   BOOST_AUTO_TEST_CASE(parameters_for_52_week_vesting) {
      try {
         time_point_sec calculation_time;
         time_point_sec start_time;

         const time_point_sec& _2019_12_31_23_59_57 = time_point_sec(1577836797); // 2019-12-31T23:59:57Z
         const time_point_sec& _2020_01_01_00_00_00 = time_point_sec(1577836800); // 2020-01-01T00:00:00Z
         const time_point_sec& _2020_01_01_00_00_03 = time_point_sec(1577836803); // 2020-01-01T00:00:03Z
         const time_point_sec& _2020_01_01_00_01_00 = time_point_sec(1577836860); // 2020-01-01T00:01:00Z
         const time_point_sec& _2020_04_01_00_00_00 = time_point_sec(1585699200); // 2020-04-01T00:00:00Z
         const time_point_sec& _2020_12_30_00_00_00 = time_point_sec(1609286400); // 2020-12-30T00:00:00Z

         /**
          * Test the start times when initializing at different times around the :00 second mark
          */
         calculation_time = _2020_01_01_00_00_00;
         const uint32_t duration_52_weeks_in_seconds = 52 * 7 * 24 * 60 * 60; // 31449600

         time_point_sec calculated_time;
         calculated_time = calc_meta1_next_start_time(_2019_12_31_23_59_57);
         BOOST_CHECK_EQUAL(_2020_01_01_00_00_00.sec_since_epoch(), calculated_time.sec_since_epoch());

         BOOST_CHECK_EQUAL(_2020_01_01_00_00_00.sec_since_epoch(),
                           calc_meta1_next_start_time(_2019_12_31_23_59_57).sec_since_epoch());
         BOOST_CHECK_EQUAL(_2020_01_01_00_01_00.sec_since_epoch(),
                           calc_meta1_next_start_time(_2020_01_01_00_00_00).sec_since_epoch());
         BOOST_CHECK_EQUAL(_2020_01_01_00_01_00.sec_since_epoch(),
                           calc_meta1_next_start_time(_2020_01_01_00_00_03).sec_since_epoch());

         start_time = calc_meta1_next_start_time(_2019_12_31_23_59_57);
         BOOST_CHECK_EQUAL(_2020_01_01_00_00_00.sec_since_epoch(), calc_meta1_next_start_time(_2019_12_31_23_59_57).sec_since_epoch());


         /**
          * Test the initial parameters for the appreciation period
          */
         appreciation_period_parameters period = calc_meta1_allocation_initial_parameters(start_time,
                                                                                          duration_52_weeks_in_seconds);
         BOOST_CHECK_EQUAL(_2020_12_30_00_00_00.sec_since_epoch(), period.time_to_100_percent.sec_since_epoch());

         uint32_t expected_intervals_to_100_percent = 524160; // = 52 * 7 * 24 * 60
         BOOST_CHECK_EQUAL(expected_intervals_to_100_percent, period.time_to_100_percent_intervals);

         uint32_t expected_intervals_to_25_percent = 131040; // = 52 * 7 * 24 * 60 / 4
         BOOST_CHECK_EQUAL(expected_intervals_to_25_percent, period.time_to_25_percent_intervals);

         BOOST_CHECK_EQUAL(_2020_04_01_00_00_00.sec_since_epoch(),period.time_to_25_percent.sec_since_epoch());


         /**
          * Test restarting the appreciation after the 25% time
          */
         appreciation_restart_parameters restart;

         // Count the quantity of intervals to the 100% mark of a 52 week duration
         // when starting at 0 seconds past the minute of 30% time
         // 30% time = 1577836800 + 31449600*30/100 = 1577836800 + 9434880 = 1587271680 = 2020-04-19T04:48:00Z
         const time_point_sec& time_30pct = time_point_sec(1587271680); // 2020-04-19T04:48:00Z
         restart = calc_meta1_allocation_restart_parameters(time_30pct, period.time_to_100_percent);
         BOOST_CHECK_EQUAL(time_30pct.sec_since_epoch(), restart.restart_time.sec_since_epoch());
         BOOST_CHECK_EQUAL(366912, restart.intervals_to_end);

         // Count the quantity of intervals to the 100% mark of a 52 week 100% duration
         // when restarting at 0 seconds past the minute of 45% time
         // 45% time = 1577836800 + 31449600*45/100 = 1577836800 + 14152320 = 1591989120 = 2020-04-19T04:48:00Z
         const time_point_sec& time_45pct = time_point_sec(1591989120); // 2020-06-12T19:12:00Z
         restart = calc_meta1_allocation_restart_parameters(time_45pct, period.time_to_100_percent);
         BOOST_CHECK_EQUAL(time_45pct.sec_since_epoch(), restart.restart_time.sec_since_epoch());
         BOOST_CHECK_EQUAL(288288, restart.intervals_to_end);

         // Count the quantity of intervals to the 100% mark of a 52 week 100% duration
         // when restarting at 0 seconds past the minute of 60% time
         // 60% time = 1577836800 + 31449600*60/100 = 1577836800 + 18869760 = 1596706560 = 2020-08-06T09:36:00Z
         const time_point_sec& time_60pct= time_point_sec(1596706560); // 2020-08-06T09:36:00Z
         restart = calc_meta1_allocation_restart_parameters(time_60pct, period.time_to_100_percent);
         BOOST_CHECK_EQUAL(time_60pct.sec_since_epoch(), restart.restart_time.sec_since_epoch());
         BOOST_CHECK_EQUAL(209664, restart.intervals_to_end);

         // Count the quantity of intervals to the 100% mark of a 52 week 100% duration
         // when restarting at 0 seconds past the minute of 75% time
         // 75% time = 1577836800 + 31449600*75/100 = 1577836800 + 23587200 = 1601424000 = 2020-09-30T00:00:00Z
         const time_point_sec& time_75pct= time_point_sec(1601424000); // 2020-09-30T00:00:00Z
         restart = calc_meta1_allocation_restart_parameters(time_75pct, period.time_to_100_percent);
         BOOST_CHECK_EQUAL(time_75pct.sec_since_epoch(), restart.restart_time.sec_since_epoch());
         BOOST_CHECK_EQUAL(131040, restart.intervals_to_end);

         // Count the quantity of intervals to the 100% mark of a 52 week 100% duration
         // when restarting at 0 seconds past the minute of 90% time
         // 90% time = 1577836800 + 31449600*90/100 = 1577836800 + 28304640 = 1606141440 = 2020-11-23T14:24:00Z
         const time_point_sec& time_90pct= time_point_sec(1606141440); // 2020-11-23T14:24:00Z
         restart = calc_meta1_allocation_restart_parameters(time_90pct, period.time_to_100_percent);
         BOOST_CHECK_EQUAL(time_90pct.sec_since_epoch(), restart.restart_time.sec_since_epoch());
         BOOST_CHECK_EQUAL(52416, restart.intervals_to_end);

         // Count the quantity of intervals to the 100% mark of a 52 week 100% duration
         // when restarting at 0 seconds past the minute of 99% time
         // 99% time = 1577836800 + 31449600*99/100 = 1577836800 + 31135104 = 1608971904 = 2020-12-26T08:38:24Z
         const time_point_sec& time_99pct= time_point_sec(1608971904); // 2020-12-26T08:38:24Z
         const time_point_sec& _2020_12_26_08_38_00= time_point_sec(1608971880); // 2020-12-26T08:38:00Z
         restart = calc_meta1_allocation_restart_parameters(time_99pct, period.time_to_100_percent);
         BOOST_CHECK_EQUAL(_2020_12_26_08_38_00.sec_since_epoch(), restart.restart_time.sec_since_epoch());
         BOOST_CHECK_EQUAL(5242, restart.intervals_to_end);

         // Count the quantity of intervals to the 100% mark of a 52 week 100% duration
         // when restarting with 3 minutes remaining
         const time_point_sec& time_3m_before = time_point_sec(1609286220); // 2020-12-29T23:57:00Z
         restart = calc_meta1_allocation_restart_parameters(time_3m_before, period.time_to_100_percent);
         BOOST_CHECK_EQUAL(time_3m_before.sec_since_epoch(), restart.restart_time.sec_since_epoch());
         BOOST_CHECK_EQUAL(3, restart.intervals_to_end);

         // Count the quantity of intervals to the 100% mark of a 52 week 100% duration
         // when restarting with 2 minutes remaining
         const time_point_sec& time_2m_before = time_point_sec(1609286280); // 2020-12-29T23:58:00Z
         restart = calc_meta1_allocation_restart_parameters(time_2m_before, period.time_to_100_percent);
         BOOST_CHECK_EQUAL(time_2m_before.sec_since_epoch(), restart.restart_time.sec_since_epoch());
         BOOST_CHECK_EQUAL(2, restart.intervals_to_end);

         // Count the quantity of intervals to the 100% mark of a 52 week 100% duration
         // when restarting with 1 minutes remaining
         const time_point_sec& time_1m_before = time_point_sec(1609286340); // 2020-12-29T23:59:00Z
         restart = calc_meta1_allocation_restart_parameters(time_1m_before, period.time_to_100_percent);
         BOOST_CHECK_EQUAL(time_1m_before.sec_since_epoch(), restart.restart_time.sec_since_epoch());
         BOOST_CHECK_EQUAL(1, restart.intervals_to_end);

         // Count the quantity of intervals to the 100% mark of a 52 week 100% duration
         // when restarting with less than 1 minute remaining
         const time_point_sec& time_3s_before = time_point_sec(1609286397); // 2020-12-29T23:59:57Z
         restart = calc_meta1_allocation_restart_parameters(time_3s_before, period.time_to_100_percent);
         BOOST_CHECK_EQUAL(time_1m_before.sec_since_epoch(), restart.restart_time.sec_since_epoch());
         BOOST_CHECK_EQUAL(1, restart.intervals_to_end);

         // Check the restart parameters for an invalid restart time (i.e. at the deadline)
         BOOST_REQUIRE_THROW(
                 calc_meta1_allocation_restart_parameters(period.time_to_100_percent, period.time_to_100_percent),
                 fc::exception);

         // Check the restart parameters for an invalid restart time (i.e. after deadline)
         const time_point_sec &_2020_12_30_00_00_03 = time_point_sec(1609286403); // 2020-12-30T00:00:03Z
         BOOST_REQUIRE_THROW(calc_meta1_allocation_restart_parameters(_2020_12_30_00_00_03, period.time_to_100_percent),
                             fc::exception);

      } FC_LOG_AND_RETHROW()
   }


   // TODO: Evaluates the appreciation parameters when initializing and restarting after the 25% time of a 365-day vesting duration
   // TODO: Evaluates the appreciation parameters when initializing and restarting after the 25% time of a 100-week vesting duration

BOOST_AUTO_TEST_SUITE_END()