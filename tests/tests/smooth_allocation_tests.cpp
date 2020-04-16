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
      //calc supply for smooth allocation formula supply of coin / 10
      // INCORRECT: return asset.options.max_supply.value / 10 / std::pow(10, asset.precision);
      return asset.options.max_supply.value / std::pow(10, asset.precision); // CORRECT
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
         // TODO: Create tests to check safe changing options
         property_update_operation uop;
         uop.issuer = meta1.id;
         uop.property_to_update = property->id;
         uop.new_options = property->options;

         trx.operations.push_back(uop);
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
         // TODO: Create tests to check safe changing options
         //////
         property_update_operation uop;
         uop.issuer = meta1.id;
         uop.property_to_update = property->id;
         uop.new_options = property->options;

         trx.operations.push_back(uop);
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

BOOST_AUTO_TEST_SUITE_END()