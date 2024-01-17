/*
 * Copyright META1 (c) 2020
 */

#include <random>

#include <boost/test/unit_test.hpp>

#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/market_object.hpp>

#include "../common/meta1_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;


/**
 * Test of maximum supply of non-core assets
 */
BOOST_FIXTURE_TEST_SUITE(max_supply_tests, meta1_fixture)

   /**
    * Create a market-issued asset (MIA) with a maximum supply equal to GRAPHENE_MAX_SHARE_SUPPLY
    */
   BOOST_AUTO_TEST_CASE(create_mia_with_max_supply) {
      try {
         /**
          * Initialize
          */
         ACTORS((nathan)(dan)(ben)(vikram));

         /**
          * Advance past hardfork
          */
         generate_blocks(HARDFORK_CORE_1465_TIME);
         set_expiration(db, trx);

         /**
          * Create the bitasset
          *
          * 1 CORE = 10^5 satoshi CORE
          * 1 LARGE = 1 satoshi LARGE (because precision = 0)
          */
         asset_id_type large_id = create_bitasset("LARGE", nathan_id, 100, charge_market_fee, 0U,
                                                  asset_id_type{}, GRAPHENE_MAX_SHARE_SUPPLY).get_id();
         BOOST_CHECK_EQUAL(0, large_id(db).dynamic_data(db).current_supply.value);
         BOOST_CHECK_EQUAL(GRAPHENE_MAX_SHARE_SUPPLY, large_id(db).options.max_supply.value);

         generate_block();
         trx.clear();
         set_expiration(db, trx);

         /**
          * Define the feed publishers
          */
         {
            asset_update_feed_producers_operation op;
            op.asset_to_update = large_id;
            op.issuer = nathan_id;
            op.new_feed_producers = {dan_id, ben_id, vikram_id};
            trx.operations.push_back(op);
            sign(trx, nathan_private_key);
            PUSH_TX(db, trx);
         }

         generate_block();
         trx.clear();
         set_expiration(db, trx);
         large_id = get_asset("LARGE").get_id();
         asset_id_type core_id = get_asset(GRAPHENE_SYMBOL).get_id();

         /**
          * Publish a feed price
          * 1 satoshi CORE = 5000 satoshi LARGE --> 10^5 CORE = 5*10^8 LARGE
          */
         {
            price_feed current_feed;
            current_feed.settlement_price = large_id(db).amount(5000) / core_id(db).amount(1);
            current_feed.maintenance_collateral_ratio = 1750; // Need to set this explicitly, testnet has a different default
            publish_feed(large_id(db), vikram, current_feed);
         }

         /**
          * Borrow all the LARGE into existence
          */
         {
            const int64_t GRAPHENE_CORE_MAX_SHARE_SUPPLY= get_asset(GRAPHENE_SYMBOL).options.max_supply.value;
            transfer(committee_account, dan_id, asset(GRAPHENE_CORE_MAX_SHARE_SUPPLY/3));
            borrow(dan_id(db), large_id(db).amount(GRAPHENE_MAX_SHARE_SUPPLY),
                   core_id(db).amount(GRAPHENE_MAX_SHARE_SUPPLY / 5000 * 2)); // CR = 2

            BOOST_CHECK_EQUAL(GRAPHENE_MAX_SHARE_SUPPLY, large_id(db).dynamic_data(db).current_supply.value);
            BOOST_REQUIRE_EQUAL(get_balance(dan, large_id(db)), GRAPHENE_MAX_SHARE_SUPPLY);
            BOOST_REQUIRE_EQUAL(get_balance(dan, core_id(db)),
                                GRAPHENE_CORE_MAX_SHARE_SUPPLY/3 - GRAPHENE_MAX_SHARE_SUPPLY / 5000 * 2);
         }

         // The supply of the MIA should be at the maximum value
         BOOST_CHECK_EQUAL(large_id(db).dynamic_asset_data_id(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY);

         generate_block();
      }
      FC_LOG_AND_RETHROW()
   }

   /**
    * Test a market sale of the entire supply in a limit order
    */
   BOOST_AUTO_TEST_CASE(sell_max_supply_1) {
      INVOKE(create_mia_with_max_supply);

      /**
       * Re-initialize
       */
      trx.clear();
      set_expiration(db, trx);
      asset_id_type core_id = get_asset(GRAPHENE_SYMBOL).get_id();
      asset_id_type large_id = get_asset("LARGE").get_id();
      GET_ACTOR(dan);

      const int64_t GRAPHENE_CORE_MAX_SHARE_SUPPLY= get_asset(GRAPHENE_SYMBOL).options.max_supply.value;
      BOOST_REQUIRE_EQUAL(get_balance(dan, large_id(db)), GRAPHENE_MAX_SHARE_SUPPLY);
      BOOST_REQUIRE_EQUAL(get_balance(dan, core_id(db)),
                          GRAPHENE_CORE_MAX_SHARE_SUPPLY / 3 - GRAPHENE_MAX_SHARE_SUPPLY / 5000 * 2);


      /**
       * Dan places an order to sell all the LARGE for 10 CORE
       */
      limit_order_object dan_sell_all
         = *create_sell_order(dan, large_id(db).amount(GRAPHENE_MAX_SHARE_SUPPLY), core_id(db).amount(1));
      limit_order_id_type dan_sell_all_id = dan_sell_all.get_id();
      // The order should remain on the books without being matched
      BOOST_REQUIRE_EQUAL(get_balance(dan, large_id(db)), 0); // Dan's balance should be zero
      BOOST_REQUIRE_EQUAL(get_balance(dan, core_id(db)),
                          GRAPHENE_CORE_MAX_SHARE_SUPPLY / 3 - (GRAPHENE_MAX_SHARE_SUPPLY / 5000 * 2));


      /**
       * Yanna places an order to sell 1 CORE for 5 LARGE
       */
      ACTOR(yanna);
      const asset yanna_initial_core_balance = core_id(db).amount(3 * 10000);
      trx.clear();
      fund(yanna, yanna_initial_core_balance);

      create_sell_order(yanna, core_id(db).amount(1), large_id(db).amount(5));


      /**
       * Check the order match and fill
       * Yanna's order should be matched to Dan's.
       * Yanna's order should be filled.  Dan's order should be partially filled.
       * The issue should collect 1% of the fill.
       */
      const uint64_t expected_fill_large = GRAPHENE_MAX_SHARE_SUPPLY;
      const uint64_t expected_issuer_fee_large = GRAPHENE_MAX_SHARE_SUPPLY / 100; // Asset's market fee is 1%
      const uint64_t expected_receipt_large = expected_fill_large - expected_issuer_fee_large;

      BOOST_REQUIRE_EQUAL(get_balance(yanna, large_id(db)), expected_receipt_large);
      BOOST_REQUIRE_EQUAL(get_balance(yanna, core_id(db)), (3 * 10000) - 1);

      BOOST_REQUIRE_EQUAL(get_balance(dan, large_id(db)), 0); // Dan's balance should be zero
      BOOST_REQUIRE_EQUAL(get_balance(dan, core_id(db)),
                          GRAPHENE_CORE_MAX_SHARE_SUPPLY / 3 - (GRAPHENE_MAX_SHARE_SUPPLY / 5000 * 2) + 1);

      // Check the asset issuer's accumulated fees
      BOOST_CHECK(large_id(db).dynamic_asset_data_id(db).accumulated_fees == expected_issuer_fee_large);

   }


   /**
    * Test a margin call of the entire supply that SHOULD trigger a global settlement
    * because of **the absence of offers** to purchase the margin call
    */
   BOOST_AUTO_TEST_CASE(margin_call_global_settlement_1) {
      INVOKE(create_mia_with_max_supply);

      /**
       * Re-initialize
       */
      trx.clear();
      set_expiration(db, trx);
      asset_id_type core_id = get_asset(GRAPHENE_SYMBOL).get_id();
      asset_id_type large_id = get_asset("LARGE").get_id();
      GET_ACTOR(dan);

      const int64_t GRAPHENE_CORE_MAX_SHARE_SUPPLY= get_asset(GRAPHENE_SYMBOL).options.max_supply.value;
      BOOST_REQUIRE_EQUAL(get_balance(dan, large_id(db)), GRAPHENE_MAX_SHARE_SUPPLY);
      BOOST_REQUIRE_EQUAL(get_balance(dan, core_id(db)),
                          GRAPHENE_CORE_MAX_SHARE_SUPPLY / 3 - GRAPHENE_MAX_SHARE_SUPPLY / 5000 * 2);


      BOOST_CHECK(!large_id(db).bitasset_data(db).has_settlement()); // No global settlement yet


      /**
       * Publish a feed price that should trigger a margin call by increasing the MCR
       * 1 satoshi CORE = 5000 satoshi LARGE --> 10^5 CORE = 5*10^8 LARGE
       */
      {
         // Publish a new feed that requires a higher MCR than is available in the outstanding debt position.
         // It should be sufficient to trigger a margin call.
         // However, there are no counter-offers that can match and completely fill the offer.
         // Furthermore, because the 200% CR of the outstanding debt position is below the NEW MSSR of 300%,
         // a global settlement should be triggered.
         GET_ACTOR(vikram);
         price_feed current_feed;
         current_feed.settlement_price = large_id(db).amount(5000) / core_id(db).amount(1);
         current_feed.maintenance_collateral_ratio = 1750 * 2; // Initially 1750
         current_feed.maximum_short_squeeze_ratio = 3000;
         publish_feed(large_id(db), vikram, current_feed);

      }

      /**
       * Check the state
       */
      // A global settlement should have been triggered
      BOOST_CHECK(large_id(db).bitasset_data(db).has_settlement());
   }


   /**
    * Test a margin call of the entire supply that SHOULD trigger a global settlement
    * because of an existing offer, **created in the same block** as the updated price feed,
    * to buy the margin call **will not match because it is too high**
    */
   BOOST_AUTO_TEST_CASE(margin_call_global_settlement_2a) {
      INVOKE(create_mia_with_max_supply);

      /**
       * Re-initialize
       */
      trx.clear();
      set_expiration(db, trx);
      asset_id_type core_id = get_asset(GRAPHENE_SYMBOL).get_id();
      asset_id_type large_id = get_asset("LARGE").get_id();
      GET_ACTOR(dan);

      const int64_t GRAPHENE_CORE_MAX_SHARE_SUPPLY= get_asset(GRAPHENE_SYMBOL).options.max_supply.value;
      BOOST_REQUIRE_EQUAL(get_balance(dan, large_id(db)), GRAPHENE_MAX_SHARE_SUPPLY);
      BOOST_REQUIRE_EQUAL(get_balance(dan, core_id(db)),
                          GRAPHENE_CORE_MAX_SHARE_SUPPLY / 3 - GRAPHENE_MAX_SHARE_SUPPLY / 5000 * 2);


      BOOST_CHECK(!large_id(db).bitasset_data(db).has_settlement()); // No global settlement yet


      /**
       * Create a sell order of CORE for the MIA that is should match the future margin call
       * and be large enough to completely fill the margin call.
       * Dan places an order to sell all of his borrowed LARGE at a price of
       * 1 satoshi LARGE = 5/5000 satoshi CORE --> 5000 satoshi LARGE = 5 satoshi CORE --> 5000 LARGE = 5*10^-5 CORE
       * = 5 * feed_price denominated in CORE/LARGE
       */
      trx.clear();
      limit_order_object dan_sell_order
         = *create_sell_order(dan, large_id(db).amount(GRAPHENE_MAX_SHARE_SUPPLY),
                              core_id(db).amount(GRAPHENE_MAX_SHARE_SUPPLY / 5000 * 5));
      limit_order_id_type dan_sell_order_id = dan_sell_order.get_id();


      /**
       * Publish a feed price that should trigger a margin call by increasing the MCR
       * 1 satoshi CORE = 5000 satoshi LARGE --> 10^5 CORE = 5*10^8 LARGE
       */
      {
         // Publish a new feed that requires a higher MCR than is available in the outstanding debt position.
         // It should be sufficient to trigger a margin call.
         // However, there are no counter-offers that can match and completely fill the offer.
         // Furthermore, because the 200% CR of the outstanding debt position is below the NEW MSSR of 300%,
         // a global settlement should be triggered.
         GET_ACTOR(vikram);
         price_feed current_feed;
         current_feed.settlement_price = large_id(db).amount(5000) / core_id(db).amount(1);
         current_feed.maintenance_collateral_ratio = 1750 * 2; // Initially 1750
         current_feed.maximum_short_squeeze_ratio = 3000;
         publish_feed(large_id(db), vikram, current_feed);

      }

      /**
       * Check the state
       */
      // A global settlement should have been triggered
      BOOST_CHECK(large_id(db).bitasset_data(db).has_settlement());
   }


   /**
    * Test a margin call of the entire supply that SHOULD trigger a global settlement
    * because of an existing offer, **created in a previous block** as the updated price feed,
    * to buy the margin call **will not match because it is too high**
    */
   BOOST_AUTO_TEST_CASE(margin_call_global_settlement_2b) {
      INVOKE(create_mia_with_max_supply);

      /**
       * Re-initialize
       */
      trx.clear();
      set_expiration(db, trx);
      asset_id_type core_id = get_asset(GRAPHENE_SYMBOL).get_id();
      asset_id_type large_id = get_asset("LARGE").get_id();
      GET_ACTOR(dan);

      const int64_t GRAPHENE_CORE_MAX_SHARE_SUPPLY= get_asset(GRAPHENE_SYMBOL).options.max_supply.value;
      BOOST_REQUIRE_EQUAL(get_balance(dan, large_id(db)), GRAPHENE_MAX_SHARE_SUPPLY);
      BOOST_REQUIRE_EQUAL(get_balance(dan, core_id(db)),
                          GRAPHENE_CORE_MAX_SHARE_SUPPLY / 3 - GRAPHENE_MAX_SHARE_SUPPLY / 5000 * 2);


      BOOST_CHECK(!large_id(db).bitasset_data(db).has_settlement()); // No global settlement yet


      /**
       * Create a sell order of CORE for the MIA that is should match the future margin call
       * and be large enough to completely fill the margin call.
       * Dan places an order to sell all of his borrowed LARGE at a price of
       * 1 satoshi LARGE = 5/5000 satoshi CORE --> 5000 satoshi LARGE = 5 satoshi CORE --> 5000 LARGE = 5*10^-5 CORE
       * = 5 * feed_price denominated in CORE/LARGE
       */
      trx.clear();
      limit_order_object dan_sell_order
         = *create_sell_order(dan, large_id(db).amount(GRAPHENE_MAX_SHARE_SUPPLY),
                              core_id(db).amount(GRAPHENE_MAX_SHARE_SUPPLY / 5000 * 5));
      limit_order_id_type dan_sell_order_id = dan_sell_order.get_id();


      /**
       * Generate a block between the limit order and the updated price feed
       */
      generate_block();


      /**
       * Publish a feed price that should trigger a margin call by increasing the MCR
       * 1 satoshi CORE = 5000 satoshi LARGE --> 10^5 CORE = 5*10^8 LARGE
       */
      {
         // Publish a new feed that requires a higher MCR than is available in the outstanding debt position.
         // It should be sufficient to trigger a margin call.
         // However, there are no counter-offers that can match and completely fill the offer.
         // Furthermore, because the 200% CR of the outstanding debt position is below the NEW MSSR of 300%,
         // a global settlement should be triggered.
         GET_ACTOR(vikram);
         price_feed current_feed;
         current_feed.settlement_price = large_id(db).amount(5000) / core_id(db).amount(1);
         current_feed.maintenance_collateral_ratio = 1750 * 2; // Initially 1750
         current_feed.maximum_short_squeeze_ratio = 3000;
         publish_feed(large_id(db), vikram, current_feed);

      }

      /**
       * Check the state
       */
      // A global settlement should have been triggered
      BOOST_CHECK(large_id(db).bitasset_data(db).has_settlement());
   }


   /**
    * Test a margin call of the entire supply that should NOT trigger a global settlement
    * because there exists an offer, **created in the same block** as the updated price feed,
    * that can match and completely fill the margin call
    */
   BOOST_AUTO_TEST_CASE(margin_call_no_global_settlement_1a) {
      INVOKE(create_mia_with_max_supply);

      /**
       * Re-initialize
       */
      trx.clear();
      set_expiration(db, trx);
      asset_id_type core_id = get_asset(GRAPHENE_SYMBOL).get_id();
      asset_id_type large_id = get_asset("LARGE").get_id();
      GET_ACTOR(dan);

      const int64_t GRAPHENE_CORE_MAX_SHARE_SUPPLY= get_asset(GRAPHENE_SYMBOL).options.max_supply.value;
      BOOST_REQUIRE_EQUAL(get_balance(dan, large_id(db)), GRAPHENE_MAX_SHARE_SUPPLY);
      BOOST_REQUIRE_EQUAL(get_balance(dan, core_id(db)),
                          GRAPHENE_CORE_MAX_SHARE_SUPPLY / 3 - GRAPHENE_MAX_SHARE_SUPPLY / 5000 * 2);

      BOOST_CHECK(!large_id(db).bitasset_data(db).has_settlement()); // No global settlement yet


      /**
       * Create a sell order of CORE for the MIA that is should match the future margin call
       * and be large enough to completely fill the margin call.
       * Dan places an order to sell all of his borrowed LARGE at a price of
       * 1 satoshi LARGE = 1/5000 satoshi CORE --> 5000 satoshi LARGE = 1 satoshi CORE --> 5000 LARGE = 1*10^-5 CORE
       * = 1 * feed_price denominated in CORE/LARGE
       */
      trx.clear();
      limit_order_object dan_sell_order
         = *create_sell_order(dan, large_id(db).amount(GRAPHENE_MAX_SHARE_SUPPLY),
                              core_id(db).amount(GRAPHENE_MAX_SHARE_SUPPLY / 5000 * 1));
      limit_order_id_type dan_sell_order_id = dan_sell_order.get_id();


      /**
       * Publish a feed price that should trigger a margin call by increasing the MCR
       * The feed price will remain unchanged from the previous
       * 1 satoshi CORE = 5000 satoshi LARGE --> 10^5 CORE = 5*10^8 LARGE
       */
      {
         // Publish a new feed that requires a higher MCR than is available in the outstanding debt position
         // It should be sufficient to trigger a margin call.
         // However, the are existing counter-offers should match and completely fill the offer
         // to avoid a global settlement.
         GET_ACTOR(vikram);
         price_feed current_feed;
         current_feed.settlement_price = large_id(db).amount(5000) / core_id(db).amount(1);
         current_feed.maintenance_collateral_ratio = 1750 * 2; // Initially 1750
         current_feed.maximum_short_squeeze_ratio = 3000;
         publish_feed(large_id(db), vikram, current_feed);
      }

      /**
       * Dan's margin call should be matched against his own limit order.
       */
      // A global settlement should have been triggered
      BOOST_CHECK(!large_id(db).bitasset_data(db).has_settlement());

      // Confirm that the limit order no longer exists
      BOOST_CHECK(!db.find(dan_sell_order_id));

      // Dan's balances should be restored to their initial values
      BOOST_REQUIRE_EQUAL(get_balance(dan, large_id(db)), 0);
      BOOST_REQUIRE_EQUAL(get_balance(dan, core_id(db)), GRAPHENE_CORE_MAX_SHARE_SUPPLY/3);

      // The supply of the MIA should be zero
      BOOST_CHECK_EQUAL(large_id(db).dynamic_asset_data_id(db).current_supply.value, 0);

   }


   /**
    * Test a margin call of the entire supply that should NOT trigger a global settlement
    * because there exists an offer, **created in a previous block** as the updated price feed,
    * that can match and completely fill the margin call
    */
   BOOST_AUTO_TEST_CASE(margin_call_no_global_settlement_1b) {
      INVOKE(create_mia_with_max_supply);

      /**
       * Re-initialize
       */
      trx.clear();
      set_expiration(db, trx);
      asset_id_type core_id = get_asset(GRAPHENE_SYMBOL).get_id();
      asset_id_type large_id = get_asset("LARGE").get_id();
      GET_ACTOR(dan);

      const int64_t GRAPHENE_CORE_MAX_SHARE_SUPPLY= get_asset(GRAPHENE_SYMBOL).options.max_supply.value;
      BOOST_REQUIRE_EQUAL(get_balance(dan, large_id(db)), GRAPHENE_MAX_SHARE_SUPPLY);
      BOOST_REQUIRE_EQUAL(get_balance(dan, core_id(db)),
                          GRAPHENE_CORE_MAX_SHARE_SUPPLY / 3 - GRAPHENE_MAX_SHARE_SUPPLY / 5000 * 2);

      BOOST_CHECK(!large_id(db).bitasset_data(db).has_settlement()); // No global settlement yet


      /**
       * Create a sell order of CORE for the MIA that is should match the future margin call
       * and be large enough to completely fill the margin call.
       * Dan places an order to sell all of his borrowed LARGE at a price of
       * 1 satoshi LARGE = 1/5000 satoshi CORE --> 5000 satoshi LARGE = 1 satoshi CORE --> 5000 LARGE = 1*10^-5 CORE
       * = 1 * feed_price denominated in CORE/LARGE
       */
      trx.clear();
      limit_order_object dan_sell_order
         = *create_sell_order(dan, large_id(db).amount(GRAPHENE_MAX_SHARE_SUPPLY),
                              core_id(db).amount(GRAPHENE_MAX_SHARE_SUPPLY / 5000 * 1));
      limit_order_id_type dan_sell_order_id = dan_sell_order.get_id();


      /**
       * Generate a block between the limit order and the updated price feed
       */
       generate_block();


      /**
       * Publish a feed price that should trigger a margin call by increasing the MCR
       * The feed price will remain unchanged from the previous
       * 1 satoshi CORE = 5000 satoshi LARGE --> 10^5 CORE = 5*10^8 LARGE
       */
      {
         // Publish a new feed that requires a higher MCR than is available in the outstanding debt position
         // It should be sufficient to trigger a margin call.
         // However, the are existing counter-offers should match and completely fill the offer
         // to avoid a global settlement.
         GET_ACTOR(vikram);
         price_feed current_feed;
         current_feed.settlement_price = large_id(db).amount(5000) / core_id(db).amount(1);
         current_feed.maintenance_collateral_ratio = 1750 * 2; // Initially 1750
         current_feed.maximum_short_squeeze_ratio = 3000;
         publish_feed(large_id(db), vikram, current_feed);
      }

      /**
       * Dan's margin call should be matched against his own limit order.
       */
      // A global settlement should have been triggered
      BOOST_CHECK(!large_id(db).bitasset_data(db).has_settlement());

      // Confirm that the limit order no longer exists
      BOOST_CHECK(!db.find(dan_sell_order_id));

      // Dan's balances should be restored to their initial values
      BOOST_REQUIRE_EQUAL(get_balance(dan, large_id(db)), 0);
      BOOST_REQUIRE_EQUAL(get_balance(dan, core_id(db)), GRAPHENE_CORE_MAX_SHARE_SUPPLY/3);

      // The supply of the MIA should be zero
      BOOST_CHECK_EQUAL(large_id(db).dynamic_asset_data_id(db).current_supply.value, 0);

   }


BOOST_AUTO_TEST_SUITE_END()