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
                                                  asset_id_type{}, GRAPHENE_MAX_SHARE_SUPPLY).id;
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
         large_id = get_asset("LARGE").id;
         asset_id_type core_id = get_asset(GRAPHENE_SYMBOL).id;

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
            transfer(committee_account, dan_id, asset(GRAPHENE_CORE_MAX_SHARE_SUPPLY));
            borrow(dan_id(db), large_id(db).amount(GRAPHENE_MAX_SHARE_SUPPLY),
                   core_id(db).amount(GRAPHENE_MAX_SHARE_SUPPLY / 5000 * 2)); // CR = 2

            BOOST_CHECK_EQUAL(GRAPHENE_MAX_SHARE_SUPPLY, large_id(db).dynamic_data(db).current_supply.value);
            BOOST_REQUIRE_EQUAL(get_balance(dan, large_id(db)), GRAPHENE_MAX_SHARE_SUPPLY);
            BOOST_REQUIRE_EQUAL(get_balance(dan, core_id(db)),
                                GRAPHENE_CORE_MAX_SHARE_SUPPLY - GRAPHENE_MAX_SHARE_SUPPLY / 5000 * 2);
         }

         generate_block();
      }
      FC_LOG_AND_RETHROW()
   }

BOOST_AUTO_TEST_SUITE_END()