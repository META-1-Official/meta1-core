/*
 * Copyright META1 (c) 2020
 */

#include <random>

#include <boost/test/unit_test.hpp>

#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/allocation.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;


BOOST_FIXTURE_TEST_SUITE(smooth_allocation_tests, database_fixture)
   // TODO: Remove asset_limitation_ops?

   const uint64_t abs64(const uint64_t v1, const uint64_t v2) {
      return (v1 >= v2) ? (v1 -v2) : (v2 - v1);
   }

   const void check_close(const double v1, const double v2, const double tol) {
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

   /**
    * Calcuate the META1 valuation derived from a single property's appraised value and current allocation progress
    * @param appraised_value    Appraised value in some monetary unit
    * @param progress_numerator  Numerator of the allocation progress
    * @param progress_denominator  Denominator of the allocation progress
    * @return   Valuation in the same monetary unit
    */
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

      // The total appraisal sums the appraisal of every property
      // This test contains only one property

      fc::uint128_t appraised_value_128(appraised_value);

      // TODO: [Low] Check for overflow
      fc::uint128_t meta1_valuation_128 = appraised_value_128 * 10 * progress_numerator;
      meta1_valuation_128 /= progress_denominator;

      uint64_t meta1_valuation = static_cast<uint64_t>(meta1_valuation_128);

      return meta1_valuation;
   }

   /**
    * Calcuate the META1 valuation derived from a single property's appraised value and current allocation progress
    * @param appraised_value    Appraised value in some monetary unit
    * @param progress  Allocation progress as a rational value
    * @return   Valuation in the same monetary unit
    */
   const uint64_t calculate_meta1_valuation(uint64_t appraised_value, const ratio_type progress) {
      return calculate_meta1_valuation(appraised_value, progress.numerator(), progress.denominator());
   }


   property_create_operation create_property_operation(database &db, string issuer,
                                                       property_options common) {
      try {
         // const asset_object& backing_asset = get_asset(db, common.backed_by_asset_symbol);
         get_asset(db,
                   common.backed_by_asset_symbol); // Invoke for the function call to check the existence of the backed_by_asset_symbol
         // FC_ASSERT(*backing_asset == nullptr, "Asset with that symbol not exists!");
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
              "222",
              1000000000,
              1,
              33104,
              10080,
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

      // Check the property's allocation
      BOOST_CHECK_EQUAL(ratio_type(0, 4), property->get_allocation_progress());

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
      uint32_t allocation_duration_seconds = (property->approval_end_date.sec_since_epoch() -
                                              property->creation_date.sec_since_epoch());
      time_point_sec time_to_25_percent = property->creation_date + (allocation_duration_seconds * 1 / 4);
      generate_blocks(time_to_25_percent, false);
      set_expiration(db, trx);
      trx.operations.clear();

      property = db.get_property(prop_op.property_id);

      // Check the property's allocation
      BOOST_CHECK_EQUAL(ratio_type(1, 4), property->get_allocation_progress());

      // Check the sell_limit
      alo = *asset_limitation_idx.find(limit_symbol);
      expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 25, 100);
      uint64_t tol = 1; // The error at any time during the appreciation should be <= 1 USD * (number of properties)
      BOOST_CHECK_LE(abs64(expected_valuation, alo.cumulative_sell_limit), tol);


      //////
      // Advance to the 50% moment
      // The appreciation should be at 25%
      //////
      time_point_sec time_to_50_percent = property->creation_date + (allocation_duration_seconds / 2);
      generate_blocks(time_to_50_percent, false);
      set_expiration(db, trx);
      trx.operations.clear();

      property = db.get_property(prop_op.property_id);

      BOOST_CHECK_EQUAL(ratio_type(1, 4), property->get_allocation_progress());

      // Check the sell_limit
      alo = *asset_limitation_idx.find(limit_symbol);
      expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 25, 100);
      BOOST_CHECK_LE(abs64(expected_valuation, alo.cumulative_sell_limit), tol);


      //////
      // Advance to the 75% moment
      // The appreciation should be at 25%
      //////
      time_point_sec time_to_75_percent = property->creation_date + (allocation_duration_seconds * 3 / 4);
      generate_blocks(time_to_75_percent, false);
      set_expiration(db, trx);
      trx.operations.clear();

      property = db.get_property(prop_op.property_id);

      BOOST_CHECK_EQUAL(ratio_type(1, 4), property->get_allocation_progress());

      // Check the sell_limit
      alo = *asset_limitation_idx.find(limit_symbol);
      expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 25, 100);
      BOOST_CHECK_LE(abs64(expected_valuation, alo.cumulative_sell_limit), tol);


      //////
      // Advance to the 100% moment
      // The appreciation should be at 0%
      //////
      time_point_sec time_to_100_percent = property->creation_date +  (allocation_duration_seconds);
      generate_blocks(time_to_100_percent, false);
      set_expiration(db, trx);
      trx.operations.clear();

      property = db.get_property(prop_op.property_id);

      BOOST_CHECK_EQUAL(ratio_type(0, 4), property->get_allocation_progress());

      // Check the sell_limit
      alo = *asset_limitation_idx.find(limit_symbol);
      expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 0, 100);
      BOOST_CHECK_LE(abs64(expected_valuation, alo.cumulative_sell_limit), tol);


      //////
      // Advance to the 125% moment
      // The appreciation should be at 0%
      //////
      time_point_sec time_to_125_percent = property->creation_date + (allocation_duration_seconds * 5 / 4);
      generate_blocks(time_to_125_percent, false);
      set_expiration(db, trx);
      trx.clear();

      property = db.get_property(prop_op.property_id);

      BOOST_CHECK_EQUAL(ratio_type(0, 4), property->get_allocation_progress());

      // Check the sell_limit
      alo = *asset_limitation_idx.find(limit_symbol);
      expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 0, 100);
      BOOST_CHECK_LE(abs64(expected_valuation, alo.cumulative_sell_limit), tol);
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
                 "222",
                 1000000000,
                 1,
                 33104,
                 10080,
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

         // Check the property's allocation
         BOOST_CHECK_EQUAL(ratio_type(0, 4), property->get_allocation_progress());

         asset_limitation_object alo;
         const auto &asset_limitation_idx = db.get_index_type<asset_limitation_index>().indices().get<by_limit_symbol>();
         uint64_t expected_valuation;


         //////
         // Advance to the 10% moment
         // The appreciation should be at 10%
         //////
         uint32_t allocation_duration_seconds = (property->approval_end_date.sec_since_epoch() -
                                                 property->creation_date.sec_since_epoch());
         time_point_sec time_to_10_percent = property->creation_date + (allocation_duration_seconds) / 10;
         generate_blocks(time_to_10_percent, false);
         set_expiration(db, trx);
         trx.clear();

         property = db.get_property(prop_op.property_id);

         BOOST_CHECK_EQUAL(ratio_type(1, 10), property->get_allocation_progress());

         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 10, 100);
         // The error in the cumulative sell limit at any time during the appreciation
         // should be <= 1 USD * (number of properties)
         uint64_t tol = 1;
         BOOST_CHECK_LE(abs64(expected_valuation, alo.cumulative_sell_limit), tol);


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
         time_point_sec time_to_25_percent = property->creation_date + (allocation_duration_seconds) / 4;
         generate_blocks(time_to_25_percent, false);
         set_expiration(db, trx);
         trx.clear();

         property = db.get_property(prop_op.property_id);

         BOOST_CHECK_EQUAL(ratio_type(1, 4), property->get_allocation_progress());

         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 25, 100);
         BOOST_CHECK_LE(abs64(expected_valuation, alo.cumulative_sell_limit), tol);


         //////
         // Advance to the 50% moment
         // The appreciation should be at 50%
         //////
         time_point_sec time_to_50_percent = property->creation_date + (allocation_duration_seconds) / 2;
         generate_blocks(time_to_50_percent, false);
         set_expiration(db, trx);
         trx.clear();

         property = db.get_property(prop_op.property_id);

         BOOST_CHECK_EQUAL(ratio_type(1, 2), property->get_allocation_progress());

         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 50, 100);
         BOOST_CHECK_LE(abs64(expected_valuation, alo.cumulative_sell_limit), tol);


         //////
         // Advance to the 75% moment
         // The appreciation should be at 75%
         //////
         time_point_sec time_to_75_percent = property->creation_date + (allocation_duration_seconds) * 3 / 4;
         generate_blocks(time_to_75_percent, false);
         set_expiration(db, trx);
         trx.clear();

         property = db.get_property(prop_op.property_id);

         BOOST_CHECK_EQUAL(ratio_type(3, 4), property->get_allocation_progress());

         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 75, 100);
         BOOST_CHECK_LE(abs64(expected_valuation, alo.cumulative_sell_limit), tol);


         //////
         // Advance to the 100% moment
         // The appreciation should be at 100%
         //////
         time_point_sec time_to_100_percent = property->creation_date + (allocation_duration_seconds);
         generate_blocks(time_to_100_percent, false);
         set_expiration(db, trx);
         trx.clear();

         property = db.get_property(prop_op.property_id);

         BOOST_CHECK_EQUAL(ratio_type(1, 1), property->get_allocation_progress());

         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 100, 100);
         BOOST_CHECK_EQUAL(expected_valuation, alo.cumulative_sell_limit);


         //////
         // Advance to the 125% moment
         // The appreciation should be at 100%
         //////
         time_point_sec time_to_125_percent = property->creation_date + (allocation_duration_seconds) * 5 / 4;
         generate_blocks(time_to_125_percent, false);
         set_expiration(db, trx);
         trx.clear();

         property = db.get_property(prop_op.property_id);

         BOOST_CHECK_EQUAL(ratio_type(1, 1), property->get_allocation_progress());

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
                 "222",
                 1000000000,
                 1,
                 33104,
                 10080,
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

         // Check the property's allocation
         BOOST_CHECK_EQUAL(ratio_type(0, 4), property->get_allocation_progress());

         asset_limitation_object alo;
         const auto &asset_limitation_idx = db.get_index_type<asset_limitation_index>().indices().get<by_limit_symbol>();
         uint64_t expected_valuation;


         //////
         // Advance to the 65% moment
         // The appreciation should be at 25%
         //////
         uint32_t allocation_duration_seconds = (property->approval_end_date.sec_since_epoch() -
                                                 property->creation_date.sec_since_epoch());
         time_point_sec time_to_65_percent = property->creation_date + (allocation_duration_seconds) * 65 / 100;
         // time_point_sec time_approval = time_to_65_percent;
         generate_blocks(time_to_65_percent, false);
         set_expiration(db, trx);
         trx.clear();

         property = db.get_property(prop_op.property_id);

         BOOST_CHECK_EQUAL(ratio_type(1, 4), property->get_allocation_progress());

         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 25, 100);
         uint64_t tol = 1; // The error at any time during the appreciation should be <= 1 USD * (number of properties)
         BOOST_CHECK_LE(abs64(expected_valuation, alo.cumulative_sell_limit), tol);


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


         //////
         // Advance the blockchain to after the approval
         // This will be any time after the property's 65% time and before the 100% time
         //////
         //////
         // Advance to the 75% moment
         // The appreciation should be at approximately 46.4%
         //////
         time_point_sec time_to_75_percent = property->creation_date + (allocation_duration_seconds) * 3 / 4;
         ilog(" Current time: ${time}", ("time", db.head_block_time()));
         generate_blocks(time_to_75_percent, false);
         set_expiration(db, trx);
         trx.clear();

         property = db.get_property(prop_op.property_id);

         // The initial allocation is 100% = 1
         // The approval allocation is 10/(100-65) = 10/35
         // The combined allocation = 1/4*(1) + (3/4 * 10/35) = 1/4 + 30/140 = 35/140 + 30/140 = 65/140 = 13/28
         // Get allocation progress should return 13/28 ~= 46.4%
         BOOST_CHECK_EQUAL(ratio_type(13, 28), property->get_allocation_progress());

         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value,
                                                        property->get_allocation_progress());
         BOOST_CHECK_LE(abs64(expected_valuation, alo.cumulative_sell_limit), tol);


         //////
         // Advance to the 100% moment
         // The appreciation should be at 100%
         //////
         time_point_sec time_to_100_percent = property->creation_date + (allocation_duration_seconds);
         generate_blocks(time_to_100_percent, false);
         set_expiration(db, trx);
         trx.clear();

         property = db.get_property(prop_op.property_id);

         BOOST_CHECK_EQUAL(ratio_type(1, 1), property->get_allocation_progress());

         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 100, 100);
         BOOST_CHECK_EQUAL(expected_valuation, alo.cumulative_sell_limit);


         //////
         // Advance to the 125% moment
         // The appreciation should be at 100%
         //////
         time_point_sec time_to_125_percent = property->creation_date + (allocation_duration_seconds) * 5 / 4;
         generate_blocks(time_to_125_percent, false);
         set_expiration(db, trx);
         trx.clear();

         property = db.get_property(prop_op.property_id);

         BOOST_CHECK_EQUAL(ratio_type(1, 1), property->get_allocation_progress());

         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(property->options.appraised_property_value, 100, 100);
         BOOST_CHECK_EQUAL(expected_valuation, alo.cumulative_sell_limit);

      } FC_LOG_AND_RETHROW()
   }
    

   /**
   * Case B
   * Two or more unverified assets, 
   * that have different appraisal values, and are added at different times.
   * Progress of both should increase to 25%,
   * then remain at 25% until it drops to 0% after the full vesting period.
   */
   BOOST_AUTO_TEST_CASE(two_unverified_assets_allocation) {
      try {
         // Initialize the actors
         ACTORS((nathan)(meta1));
         upgrade_to_lifetime_member(meta1_id);
   
         // Advance to when the smooth allocation is activated
         generate_blocks(HARDFORK_CORE_21_TIME);
         generate_blocks(6);
   
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
         
         trx.clear();
         trx.operations.push_back(create_limitation_op);
         set_expiration(db, trx);
   
         // TODO: Test that only signatures by meta1 are sufficient
         sign(trx, meta1_private_key);
   
         PUSH_TX(db, trx);
   
         // appraisal value = 1000000000
         // vesting period  = 1 week
         const property_options first_property_ops = {
                 "some description 1",
                 "some title 1",
                 "my@email.com",
                 "you",
                 "https://fsf.com",
                 "https://purepng.com/metal-1701528976849tkdsl.png",
                 "222",
                 1000000000,
                 1,
                 33104,
                 10080,
                 "META1",
         };
   
         // appraisal value = 2000000000
         // vesting period  = 1 week
         const property_options second_property_ops = {
                 "some description 2",
                 "some title 2",
                 "my@email.com",
                 "you",
                 "https://fsf.com",
                 "https://purepng.com/metal-1701528976849tkdsl.png",
                 "222",
                 2000000000,
                 1,
                 33104,
                 10080,
                 "META1",
         };
   
         //////
         // Info:
         // 1)create first backing asset 
         // 2)create second backing asset at 10% timeline point from first backing asset creation
         //////
   
         // 1)
         property_create_operation first_prop_op = create_property_operation(db, "meta1", first_property_ops);
   
         trx.clear();
         trx.operations.push_back(first_prop_op);
         set_expiration(db, trx);
   
         // TODO: Test that only signatures by meta1 are sufficient
         sign(trx, meta1_private_key);
   
         PUSH_TX(db, trx);
         generate_blocks(1);
         generate_blocks(1);
   
         // Check the initial allocation of the asset before any blocks advance
         const graphene::chain::property_object *first_property = db.get_property(first_prop_op.property_id);
   
         // Check the property's allocation
         BOOST_CHECK_EQUAL(ratio_type(0, 4), first_property->get_allocation_progress());
   
         asset_limitation_object alo;
         const auto &asset_limitation_idx = db.get_index_type<asset_limitation_index>().indices().get<by_limit_symbol>();
         uint64_t expected_valuation;
   
         //////
         // Advance to the 10% moment of FIRST asset
         // Create second backing asset
         //
         // The appreciation of first asset  should be at 10%
         // The appreciation of second asset should be at 0 %
         // The appreciation of sell limit   should be at 10%
         //////
         uint32_t allocation_duration_seconds = (first_property->approval_end_date.sec_since_epoch() -
                                                 first_property->creation_date.sec_since_epoch());
         time_point_sec time_to_10_percent = first_property->creation_date + (allocation_duration_seconds) * 10 / 100;
         generate_blocks(time_to_10_percent, false);
         set_expiration(db, trx);
         trx.clear();
   
         // Create second property
         property_create_operation second_prop_op = create_property_operation(db, "meta1", second_property_ops);
   
         trx.operations.push_back(second_prop_op);
         set_expiration(db, trx);
   
         // TODO: Test that only signatures by meta1 are sufficient
         sign(trx, meta1_private_key);
   
         PUSH_TX(db, trx);
         generate_blocks(1);
         generate_blocks(1);
   
         // Check the initial allocation of the FIRST asset
         first_property = db.get_property(first_prop_op.property_id);
         BOOST_CHECK_EQUAL(ratio_type(1, 10), first_property->get_allocation_progress());
         
         // Check the initial allocation of the SECOND before any blocks advance
         const graphene::chain::property_object *second_property = db.get_property(second_prop_op.property_id);
         BOOST_REQUIRE_EQUAL(allocation_duration_seconds, (second_property->approval_end_date.sec_since_epoch() -
                                                     second_property->creation_date.sec_since_epoch()));
         BOOST_CHECK_EQUAL(ratio_type(0, 4), second_property->get_allocation_progress());
   
         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(first_property->options.appraised_property_value, 10, 100);
         uint64_t tol = 1; // The error at any time during the appreciation should be <= 1 USD * (number of properties)
         BOOST_CHECK_LE(abs64(expected_valuation, alo.cumulative_sell_limit), tol);
   
         //////
         // Advance to the 15% moment of SECOND asset && 25% of first one
         // The appreciation of first asset  should be at 25%
         // The appreciation of second asset should be at 15%
         // The appreciation of sell_limit   should be at 40%
         //////
         time_point_sec time_to_15_percent = second_property->creation_date + (allocation_duration_seconds) * 15 / 100;
         generate_blocks(time_to_15_percent, false);
         set_expiration(db, trx);
         trx.clear();  
   
         first_property = db.get_property(first_prop_op.property_id);
         second_property = db.get_property(second_prop_op.property_id);
   
         BOOST_CHECK_EQUAL(ratio_type(1, 4), first_property->get_allocation_progress());
         BOOST_CHECK_EQUAL(ratio_type(3, 20), second_property->get_allocation_progress());
       
         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(first_property->options.appraised_property_value, 25, 100) + 
                              calculate_meta1_valuation(second_property->options.appraised_property_value, 15, 100);
         BOOST_CHECK_LE(abs64(expected_valuation, alo.cumulative_sell_limit), tol);
     
         //////
         // Advance to the 40% moment of SECOND asset && 50% of first one
         // The appreciation of first asset  should be at 25%
         // The appreciation of second asset should be at 25%
         // The appreciation of sell_limit   should be at 50%
         //////
         time_point_sec time_to_40_percent = second_property->creation_date + (allocation_duration_seconds) * 40 / 100;
         generate_blocks(time_to_40_percent, false);
         set_expiration(db, trx);
         trx.clear();  
   
         first_property = db.get_property(first_prop_op.property_id);
         second_property = db.get_property(second_prop_op.property_id);
   
         BOOST_CHECK_EQUAL(ratio_type(1, 4), first_property->get_allocation_progress());
         BOOST_CHECK_EQUAL(ratio_type(1, 4), second_property->get_allocation_progress());
       
         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(first_property->options.appraised_property_value, 25, 100) + 
                              calculate_meta1_valuation(second_property->options.appraised_property_value, 25, 100);
         BOOST_CHECK_LE(abs64(expected_valuation, alo.cumulative_sell_limit), tol);
   
         //////
         // Advance to the 65% moment of SECOND asset && 75% of first one
         // The appreciation of first asset  should be at 25%
         // The appreciation of second asset should be at 25%
         // The appreciation of sell_limit   should be at 50%
         //////
         time_point_sec time_to_65_percent = second_property->creation_date + (allocation_duration_seconds) * 65 / 100;
         generate_blocks(time_to_65_percent, false);
         set_expiration(db, trx);
         trx.clear();  
   
         first_property = db.get_property(first_prop_op.property_id);
         second_property = db.get_property(second_prop_op.property_id);
   
         BOOST_CHECK_EQUAL(ratio_type(1, 4), first_property->get_allocation_progress());
         BOOST_CHECK_EQUAL(ratio_type(1, 4), second_property->get_allocation_progress());
       
         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(first_property->options.appraised_property_value, 25, 100) + 
                              calculate_meta1_valuation(second_property->options.appraised_property_value, 25, 100);
         BOOST_CHECK_LE(abs64(expected_valuation, alo.cumulative_sell_limit), tol);
   
         //////
         // Advance to the 90% moment of SECOND asset && 100% of first one
         // The appreciation of first asset  should be at 0%
         // The appreciation of second asset should be at 25%
         // The appreciation of sell_limit   should be at 25%
         //////
         time_point_sec time_to_90_percent = second_property->creation_date + (allocation_duration_seconds) * 90 / 100;
         generate_blocks(time_to_90_percent, false);
         set_expiration(db, trx);
         trx.clear();  
   
         first_property = db.get_property(first_prop_op.property_id);
         second_property = db.get_property(second_prop_op.property_id);
   
         BOOST_CHECK_EQUAL(ratio_type(0, 4), first_property->get_allocation_progress());
         BOOST_CHECK_EQUAL(ratio_type(1, 4), second_property->get_allocation_progress());
       
         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(first_property->options.appraised_property_value, 0, 100) + 
                              calculate_meta1_valuation(second_property->options.appraised_property_value, 25, 100);
         BOOST_CHECK_LE(abs64(expected_valuation, alo.cumulative_sell_limit), tol);
   
         //////
         // Advance to the 100% moment of SECOND asset && 110% of first one
         // The appreciation of first asset  should be at 0%
         // The appreciation of second asset should be at 0%
         // The appreciation of sell_limit   should be at 0%
         //////
         time_point_sec time_to_100_percent = second_property->creation_date + (allocation_duration_seconds) * 100 / 100;
         generate_blocks(time_to_100_percent, false);
         set_expiration(db, trx);
         trx.clear();  
   
         first_property = db.get_property(first_prop_op.property_id);
         second_property = db.get_property(second_prop_op.property_id);
   
         BOOST_CHECK_EQUAL(ratio_type(0, 4), first_property->get_allocation_progress());
         BOOST_CHECK_EQUAL(ratio_type(0, 4), second_property->get_allocation_progress());
       
         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(first_property->options.appraised_property_value, 0, 100) + 
                              calculate_meta1_valuation(second_property->options.appraised_property_value, 0, 100);
         BOOST_CHECK_LE(abs64(expected_valuation, alo.cumulative_sell_limit), tol);

      } FC_LOG_AND_RETHROW()
   }
   

   /**
   * Case D
   * Two or more assets that have different appraisal values,
   * are added at different times, and are verified at different times. 
   * Demonstrate that the "Smooth Allocation Smart Contract" increases the value over time.
   */
   BOOST_AUTO_TEST_CASE(two_verified_assets_allocation) {
      try {
         // Initialize the actors
         ACTORS((nathan)(meta1));
         upgrade_to_lifetime_member(meta1_id);
   
         // Advance to when the smooth allocation is activated
         generate_blocks(HARDFORK_CORE_21_TIME);
         generate_blocks(6);
   
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
         
         trx.clear();
         trx.operations.push_back(create_limitation_op);
         set_expiration(db, trx);
   
         // TODO: Test that only signatures by meta1 are sufficient
         sign(trx, meta1_private_key);
   
         PUSH_TX(db, trx);
   
         // appraisal value = 1000000000
         // vesting period  = 1 week
         const property_options first_property_ops = {
               "some description 1",
               "some title 1",
               "my@email.com",
               "you",
               "https://fsf.com",
               "https://purepng.com/metal-1701528976849tkdsl.png",
               "222",
               1000000000,
               1,
               33104,
               10080,
               "META1",
       };
   
         // appraisal value = 2000000000
         // vesting period  = 1 week
         const property_options second_property_ops = {
               "some description 2",
               "some title 2",
               "my@email.com",
               "you",
               "https://fsf.com",
               "https://purepng.com/metal-1701528976849tkdsl.png",
               "222",
               2000000000,
               1,
               33104,
               10080,
               "META1",
       };
 
         //////
         // Info:
         // 1) create  first  backing asset 
         // 2) create  second backing asset at 10% of first asset
         // 3) approve first  backing asset at 10% of first asset
         // 4) approve second backing asset at 75% of first asset or 65% of second asset
         //////
         
         // 1)
         property_create_operation first_prop_op = create_property_operation(db, "meta1", first_property_ops);
         trx.clear();
         trx.operations.push_back(first_prop_op);
         set_expiration(db, trx);
 
         // TODO: Test that only signatures by meta1 are sufficient
         sign(trx, meta1_private_key);
 
         PUSH_TX(db, trx);
         generate_blocks(1);
         generate_blocks(1);
 
         // Check the initial allocation of the asset before any blocks advance
         const graphene::chain::property_object *first_property = db.get_property(first_prop_op.property_id);
 
         // Check the property's allocation
         BOOST_CHECK_EQUAL(ratio_type(0, 4), first_property->get_allocation_progress());
 
         asset_limitation_object alo;
         const auto &asset_limitation_idx = db.get_index_type<asset_limitation_index>().indices().get<by_limit_symbol>();
         uint64_t expected_valuation;
   
   
         //////
         // Advance to the 10% moment of FIRST asset
         //
         // - Create second backing asset
         // - Approve first  backing asset
         //
         // The appreciation of first asset  should be at 10%
         // The appreciation of second asset should be at 0 %
         // The appreciation of sell limit   should be at 10%
         //////
         uint32_t allocation_duration_seconds = (first_property->approval_end_date.sec_since_epoch() -
                                                 first_property->creation_date.sec_since_epoch());
         time_point_sec time_to_10_percent = first_property->creation_date + (allocation_duration_seconds) * 10 / 100;
         generate_blocks(time_to_10_percent, false);
         set_expiration(db, trx);
         trx.clear();
 
         // 2)
         // Create second property
         property_create_operation second_prop_op = create_property_operation(db, "meta1", second_property_ops);
         trx.operations.push_back(second_prop_op);
 
         // 3)
         // Approve first backing asset by using the update operation
         property_approve_operation first_aop;
         first_aop.issuer = meta1.id;
         first_aop.property_to_approve = first_property->id;
         
         trx.operations.push_back(first_aop);
         
         sign(trx, meta1_private_key);
         PUSH_TX(db, trx);
 
         generate_blocks(1);
         generate_blocks(1);   
 
         // Check the initial allocation of the first asset
         first_property = db.get_property(first_prop_op.property_id);
         BOOST_CHECK_EQUAL(ratio_type(1, 10), first_property->get_allocation_progress());
         
         // Check the initial allocation of the second before any blocks advance
         const graphene::chain::property_object *second_property = db.get_property(second_prop_op.property_id);
         BOOST_REQUIRE_EQUAL(allocation_duration_seconds, (second_property->approval_end_date.sec_since_epoch() -
                                                     second_property->creation_date.sec_since_epoch()));
         BOOST_CHECK_EQUAL(ratio_type(0, 4), second_property->get_allocation_progress());
 
         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(first_property->options.appraised_property_value, 10, 100);
         uint64_t tol = 1; // The error at any time during the appreciation should be <= 1 USD * (number of properties)
         BOOST_CHECK_LE(abs64(expected_valuation, alo.cumulative_sell_limit), tol);
         
 
         //////
         // Advance to the 75% moment of FIRST asset
         // The appreciation of first  asset should be at 75%
         // The appreciation of second asset should be at 25%
         // The appreciation of sell limit   should be at 100%
         //////
         time_point_sec time_to_75_percent = first_property->creation_date + (allocation_duration_seconds) * 75 / 100;
         generate_blocks(time_to_75_percent, false);
         set_expiration(db, trx);
         trx.clear();  
 
         first_property = db.get_property(first_prop_op.property_id);
         second_property = db.get_property(second_prop_op.property_id);
 
         BOOST_CHECK_EQUAL(ratio_type(3, 4), first_property->get_allocation_progress());
         BOOST_CHECK_EQUAL(ratio_type(1, 4), second_property->get_allocation_progress());
 
         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(first_property->options.appraised_property_value, 75, 100) + 
                              calculate_meta1_valuation(second_property->options.appraised_property_value, 25, 100);
         BOOST_CHECK_LE(abs64(expected_valuation, alo.cumulative_sell_limit), tol);
 
         //////
         // 4)
         // Approve the second asset by using the update operation
         //////
         property_approve_operation second_aop;
         second_aop.issuer = meta1.id;
         second_aop.property_to_approve = second_property->id;
         trx.operations.push_back(second_aop);
         sign(trx, meta1_private_key);
         PUSH_TX(db, trx);
 
 
         //////
         // Advance to the 85% moment of FIRST asset
         // The appreciation of first  asset should be at 85%
         // The appreciation of second asset should be at 46.422499291%
         // The appreciation of sell limit   should be at 131.4%
         //////
         time_point_sec time_to_85_percent = first_property->creation_date + (allocation_duration_seconds) * 85 / 100;
         generate_blocks(time_to_85_percent, false);
         set_expiration(db, trx);
         trx.clear();  
 
         first_property = db.get_property(first_prop_op.property_id);
         second_property = db.get_property(second_prop_op.property_id);
 
         BOOST_CHECK_EQUAL(ratio_type(17, 20), first_property->get_allocation_progress());
         BOOST_CHECK_EQUAL(ratio_type(6553,14116), second_property->get_allocation_progress());
 
         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(first_property->options.appraised_property_value, 85, 100) + 
                              calculate_meta1_valuation(second_property->options.appraised_property_value, ratio_type(6553,14116));
         BOOST_CHECK_LE(abs64(expected_valuation, alo.cumulative_sell_limit), tol);
         
 
         //////
         // Advance to the 100% moment of SECOND asset
         // The appreciation of first  asset should be at 100%
         // The appreciation of second asset should be at 100%
         // The appreciation of sell limit   should be at 200%
         //////
         time_point_sec time_to_100_percent = second_property->creation_date + (allocation_duration_seconds) * 100 / 100;
         generate_blocks(time_to_100_percent, false);
         set_expiration(db, trx);
         trx.clear();  
 
         first_property = db.get_property(first_prop_op.property_id);
         second_property = db.get_property(second_prop_op.property_id);
 
         BOOST_CHECK_EQUAL(ratio_type(1, 1), first_property->get_allocation_progress());
         BOOST_CHECK_EQUAL(ratio_type(1, 1), second_property->get_allocation_progress());
 
         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(first_property->options.appraised_property_value, 1, 1) + 
                              calculate_meta1_valuation(second_property->options.appraised_property_value, ratio_type(1,1));
         BOOST_CHECK_LE(abs64(expected_valuation, alo.cumulative_sell_limit), tol);
 

         //////
         // Advance to the 135% moment of FIRST asset
         // The appreciation of first  asset should be at 100%
         // The appreciation of second asset should be at 100%
         // The appreciation of sell limit   should be at 200%
         //////
         time_point_sec time_to_135_percent = first_property->creation_date + (allocation_duration_seconds) * 135 / 100;
         generate_blocks(time_to_135_percent, false);
         set_expiration(db, trx);
         trx.clear();  
 
         first_property = db.get_property(first_prop_op.property_id);
         second_property = db.get_property(second_prop_op.property_id);
 
         BOOST_CHECK_EQUAL(ratio_type(1, 1), first_property->get_allocation_progress());
         BOOST_CHECK_EQUAL(ratio_type(1, 1), second_property->get_allocation_progress());
 
         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(first_property->options.appraised_property_value, 1, 1) + 
                              calculate_meta1_valuation(second_property->options.appraised_property_value, ratio_type(1,1));
         BOOST_CHECK_LE(abs64(expected_valuation, alo.cumulative_sell_limit), tol);

      } FC_LOG_AND_RETHROW()
   }


    /**
    * Case E
    * After Asset 1 vests to its full value, Asset 2 is asset as in Case A. 
    * Demonstrate that the smooth allocation plug-in smoothly allocates.
    */
   BOOST_AUTO_TEST_CASE(case_e) {
      try {
         // Initialize the actors
         ACTORS((nathan)(meta1));
         upgrade_to_lifetime_member(meta1_id);

         // Advance to when the smooth allocation is activated
         // TODO: Switch to HF time for smooth allocation in CORE
         generate_blocks(HARDFORK_CORE_21_TIME);
         generate_blocks(6);
   
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
   
         // appraisal value = 2000000000
         // vesting period  = 1 week
         const property_options first_property_ops = {
                 "some description 1",
                 "some title 1",
                 "my@email.com",
                 "you",
                 "https://fsf.com",
                 "https://purepng.com/metal-1701528976849tkdsl.png",
                 "222",
                 2000000000,
                 1,
                 33104,
                 10080,
                 "META1",
         };
   
         // appraisal value = 1000000000
         // vesting period  = 1 week
         const property_options second_property_ops = {
                 "some description 2",
                 "some title 2",
                 "my@email.com",
                 "you",
                 "https://fsf.com",
                 "https://purepng.com/metal-1701528976849tkdsl.png",
                 "222",
                 1000000000,
                 1,
                 33104,
                 10080,
                 "META1",
         };
         
         //////
         // Info:
         // 1) create  first  backing asset 
         // 2) approve first  backing asset
         // 3) first asset vests to its full value
         // 4) create second backing asset & not approve it like in CASE A.
         // 5) go to the end of second asset vesting period
         //////
         
         // 1) create  FIRST  backing asset 
         property_create_operation first_prop_op = create_property_operation(db, "meta1", first_property_ops);

         trx.clear();
         trx.operations.push_back(first_prop_op);
         set_expiration(db, trx);
   
         // TODO: Test that only signatures by meta1 are sufficient
         sign(trx, meta1_private_key);
   
         PUSH_TX(db, trx);
         generate_blocks(1);
         generate_blocks(1);
            
         // Check the initial allocation of the asset before any blocks advance
         const graphene::chain::property_object *first_property = db.get_property(first_prop_op.property_id);
 
         // Check the property's allocation
         BOOST_CHECK_EQUAL(ratio_type(0, 4), first_property->get_allocation_progress());
 
         asset_limitation_object alo;
         const auto &asset_limitation_idx = db.get_index_type<asset_limitation_index>().indices().get<by_limit_symbol>();
         uint64_t expected_valuation;

         // 2) approve first  backing asset
         property_approve_operation first_aop;
         first_aop.issuer = meta1.id;
         first_aop.property_to_approve = first_property->id;

         trx.clear();
         trx.operations.push_back(first_aop);
         
         sign(trx, meta1_private_key);
         PUSH_TX(db, trx);

         generate_blocks(1);
         generate_blocks(1);


         //////
         // 3) FIRST asset vests to its full value
         // Advance to the 100% moment of FIRST asset
         // The appreciation of first  asset should be at 100%
         //////
         uint32_t allocation_duration_seconds = (first_property->approval_end_date.sec_since_epoch() -
                                                 first_property->creation_date.sec_since_epoch());
         time_point_sec time_to_100_percent = first_property->creation_date + (allocation_duration_seconds) * 100 / 100;
         generate_blocks(time_to_100_percent, false);
         set_expiration(db, trx);
         trx.clear();

         // Check allocation value of the FIRST asset
         first_property = db.get_property(first_prop_op.property_id);
         BOOST_CHECK_EQUAL(ratio_type(1, 1), first_property->get_allocation_progress());

         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(first_property->options.appraised_property_value, 100, 100);
         uint64_t tol = 1; // The error at any time during the appreciation should be <= 1 USD * (number of properties)
         BOOST_CHECK_LE(abs64(expected_valuation, alo.cumulative_sell_limit), tol);
         
         //4) create second backing asset & not approve it like in CASE A.
         property_create_operation second_prop_op = create_property_operation(db, "meta1", second_property_ops);
         trx.operations.push_back(second_prop_op);
         sign(trx, meta1_private_key);
         PUSH_TX(db, trx);

         generate_blocks(1);
         generate_blocks(1);

         const graphene::chain::property_object *second_property = db.get_property(second_prop_op.property_id);
         BOOST_REQUIRE_EQUAL(allocation_duration_seconds, (second_property->approval_end_date.sec_since_epoch() -
                                                     second_property->creation_date.sec_since_epoch()));


         //////
         // Advance to the 25% moment of SECOND asset
         // The appreciation of first  asset should be at 100%
         // The appreciation of second asset should be at 25%
         //////
         time_point_sec time_to_25_percent = second_property->creation_date + (allocation_duration_seconds) * 25 / 100;
         generate_blocks(time_to_25_percent, false);
         set_expiration(db, trx);
         trx.clear(); 

         first_property = db.get_property(first_prop_op.property_id);
         second_property = db.get_property(second_prop_op.property_id);
 
         BOOST_CHECK_EQUAL(ratio_type(1, 1), first_property->get_allocation_progress());
         BOOST_CHECK_EQUAL(ratio_type(1, 4), second_property->get_allocation_progress());

         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(first_property->options.appraised_property_value, 100, 100) + 
                              calculate_meta1_valuation(second_property->options.appraised_property_value, 25, 100);
         BOOST_CHECK_LE(abs64(expected_valuation, alo.cumulative_sell_limit), tol);


         //////
         // Advance to the 75% moment of SECOND asset
         // The appreciation of first  asset should be at 100%
         // The appreciation of second asset should be at 25%
         //////
         time_point_sec time_to_75_percent = second_property->creation_date + (allocation_duration_seconds) * 75 / 100;
         generate_blocks(time_to_75_percent, false);
         set_expiration(db, trx);
         trx.clear(); 

         first_property = db.get_property(first_prop_op.property_id);
         second_property = db.get_property(second_prop_op.property_id);
 
         BOOST_CHECK_EQUAL(ratio_type(1, 1), first_property->get_allocation_progress());
         BOOST_CHECK_EQUAL(ratio_type(1, 4), second_property->get_allocation_progress());

         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(first_property->options.appraised_property_value, 100, 100) + 
                              calculate_meta1_valuation(second_property->options.appraised_property_value, 25, 100);
         BOOST_CHECK_LE(abs64(expected_valuation, alo.cumulative_sell_limit), tol);


         //////
         // 5) go to the end of SECOND asset vesting period
         // Advance to the 101% moment of second asset (expiration)
         // The appreciation of first  asset should be at 100%
         // The appreciation of second asset should be at 0%
         //////
         time_point_sec time_to_101_percent = second_property->creation_date + (allocation_duration_seconds) * 101 / 100;
         generate_blocks(time_to_101_percent, false);
         set_expiration(db, trx);
         trx.clear(); 

         first_property = db.get_property(first_prop_op.property_id);
         second_property = db.get_property(second_prop_op.property_id);
 
         BOOST_CHECK_EQUAL(ratio_type(1, 1), first_property->get_allocation_progress());
         BOOST_CHECK_EQUAL(ratio_type(0, 1), second_property->get_allocation_progress());

         // Check the sell_limit
         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = calculate_meta1_valuation(first_property->options.appraised_property_value, 100, 100) + 
                              calculate_meta1_valuation(second_property->options.appraised_property_value, 0, 100);
         BOOST_CHECK_LE(abs64(expected_valuation, alo.cumulative_sell_limit), tol);
         
        } FC_LOG_AND_RETHROW()
   }

   // TODO: Test for late approval on the same block as the approval deadline
   // TODO: Test for approval on the block before the approval deadline

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


   // TODO: [Medium] Evaluates the appreciation parameters when initializing and restarting after the 25% time of a 365-day vesting duration
   // TODO: [Medium] Evaluates the appreciation parameters when initializing and restarting after the 25% time of a 100-week vesting duration

BOOST_AUTO_TEST_SUITE_END()