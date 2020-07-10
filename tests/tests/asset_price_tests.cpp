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
 * Test of external asset prices
 */
BOOST_FIXTURE_TEST_SUITE(asset_price_tests, meta1_fixture)

   /**
    * Publish the price of an external asset
    */
   BOOST_AUTO_TEST_CASE(publish_asset_price_test) {
      try {
         /**
          * Initialize
          */
         set_expiration(db, trx);

         time_point_sec now = db.head_block_time();

         // Initialize the actors
         ACTORS((nathan)(meta1));
         upgrade_to_lifetime_member(meta1_id);

         // Ensure that no price yet exists for the asset that is being tests
         const auto &idx = db.get_index_type<asset_price_index>().indices().get<by_symbol>();
         auto itr = idx.find("BTC");
         BOOST_CHECK(itr == idx.end());


         // Ensure that only meta1 can publish these external asset feed prices
         asset_price_publish_operation publish_op;
         publish_op.symbol = "BTC";
         publish_op.usd_price = price_ratio(2000, 1); // 2000 USD per BTC
         publish_op.fee_paying_account = nathan_id;

         trx.clear();
         trx.operations.push_back(publish_op);
         sign(trx, nathan_private_key);

         REQUIRE_EXCEPTION_WITH_TEXT(PUSH_TX(db, trx), "only by meta1 account");


         // Ensure that only meta1 can publish these external asset feed prices
         publish_op.fee_paying_account = nathan_id;

         trx.clear();
         trx.operations.push_back(publish_op);
         sign(trx, meta1_private_key);

         GRAPHENE_REQUIRE_THROW(PUSH_TX(db, trx), graphene::chain::tx_missing_active_auth);


         // Ensure that only meta1 can publish these external asset feed prices
         publish_op.fee_paying_account = meta1_id;

         trx.clear();
         trx.operations.push_back(publish_op);
         sign(trx, nathan_private_key);

         GRAPHENE_REQUIRE_THROW(PUSH_TX(db, trx), graphene::chain::tx_missing_active_auth);


         // Publish the USD-price of a token
         // This should fail because the asset does not exist on the blockchain
         publish_op.fee_paying_account = meta1_id;

         trx.clear();
         trx.operations.push_back(publish_op);
         sign(trx, meta1_private_key);

         REQUIRE_EXCEPTION_WITH_TEXT(PUSH_TX(db, trx), "should exist on the blockchain");


         // Publish the USD-price of a token after the asset exists
         // This should succeed
         trx.clear();
         create_user_issued_asset("BTC");

         trx.clear();
         trx.operations.push_back(publish_op);
         sign(trx, meta1_private_key);

         PUSH_TX(db, trx);


         // Confirm that the price can be retrieved
         itr = idx.find("BTC");
         BOOST_CHECK(itr != idx.end());
         asset_price price_of_btc = *itr;
         BOOST_CHECK_EQUAL(price_of_btc.symbol, "BTC");
         BOOST_CHECK_EQUAL(price_of_btc.usd_price.numerator, 2000);
         BOOST_CHECK_EQUAL(price_of_btc.usd_price.denominator, 1);
         BOOST_CHECK(price_of_btc.publication_time == now);


         // Publish an updated price
         publish_op.usd_price = price_ratio(2200, 1); // 2200 USD per BTC

         trx.clear();
         trx.operations.push_back(publish_op);
         sign(trx, meta1_private_key);

         PUSH_TX(db, trx);

         // Confirm that the new feed can be retrieved
         itr = idx.find("BTC");
         BOOST_CHECK(itr != idx.end());
         price_of_btc = *itr;
         BOOST_CHECK_EQUAL(price_of_btc.symbol, "BTC");
         BOOST_CHECK_EQUAL(price_of_btc.usd_price.numerator, 2200);
         BOOST_CHECK_EQUAL(price_of_btc.usd_price.denominator, 1);
         BOOST_CHECK(price_of_btc.publication_time == now);


         // Advance the blocks
         generate_blocks(10);
         set_expiration(db, trx);
         now = db.head_block_time();

         // Publish an updated price
         publish_op.usd_price = price_ratio(1500, 1); // 1500 USD per BTC

         trx.clear();
         trx.operations.push_back(publish_op);
         sign(trx, meta1_private_key);

         PUSH_TX(db, trx);

         // Confirm that the new feed can be retrieved
         itr = idx.find("BTC");
         BOOST_CHECK(itr != idx.end());
         price_of_btc = *itr;
         BOOST_CHECK_EQUAL(price_of_btc.symbol, "BTC");
         BOOST_CHECK_EQUAL(price_of_btc.usd_price.numerator, 1500);
         BOOST_CHECK_EQUAL(price_of_btc.usd_price.denominator, 1);
         BOOST_CHECK(price_of_btc.publication_time == now);

         // Check unknown assets
         itr = idx.find("USDT");
         BOOST_CHECK(itr == idx.end());
      }
      FC_LOG_AND_RETHROW()
   }


   /**
    * Test the minimum prices for new limit orders
    */
   BOOST_AUTO_TEST_CASE(test_minimum_prices_for_new_limit_orders) {
      try {
         /**
          * Initialize
          */
         set_expiration(db, trx);

         // Initialize the actors
         ACTORS((alice)(meta1));
         upgrade_to_lifetime_member(meta1_id);

         // Initialize the assets
         create_user_issued_asset("BTC");
         asset_object BTC = get_asset("BTC");
         const asset_id_type BTC_ID = BTC.id;
         asset_object META1 = get_asset("META1");
         const asset_id_type META1_ID = META1.id;

         account_object issuer_account = get_account("meta1");

         // Fund the actors
         const asset alice_initial_meta1 = META1.amount(5000 * asset::scaled_precision(META1.precision).value);
         transfer(committee_account, alice.id, alice_initial_meta1);
         BOOST_CHECK_EQUAL(get_balance(alice, META1), alice_initial_meta1.amount.value);
         verify_asset_supplies(db);

         auto _issue_uia = [&](const account_object &recipient, asset amount) {
            asset_issue_operation op;
            op.issuer = amount.asset_id(db).issuer;
            op.asset_to_issue = amount;
            op.issue_to_account = recipient.id;
            transaction tx;
            tx.operations.push_back(op);
            set_expiration(db, tx);
            PUSH_TX(db, tx, database::skip_tapos_check | database::skip_transaction_signatures);
         };
         const asset alice_initial_btc = BTC.amount(30 * asset::scaled_precision(BTC.precision).value);
         _issue_uia(alice, alice_initial_btc);


         /**
          * Create the asset limitation
          */
         const string limit_symbol = "META1";
         asset_limitation_object_create_operation create_limitation_op;
         create_limitation_op.limit_symbol = limit_symbol;
         create_limitation_op.issuer = issuer_account.id;

         trx.clear();
         trx.operations.push_back(create_limitation_op);
         sign(trx, meta1_private_key);

         PUSH_TX(db, trx);


         /**
          * Add a backing asset appraised at 900,000,000 USD
          */
         // Create the property
         const property_options property_ops = {
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
         property_create_operation prop_op = create_property_operation("meta1", 900000000, 10080, "META1",
                                                                       property_ops);

         trx.clear();
         trx.operations.push_back(prop_op);
         set_expiration(db, trx);
         sign(trx, meta1_private_key);

         PUSH_TX(db, trx);

         // Check the initial allocation of the asset before any blocks advance
         graphene::chain::property_object property = db.get_property(prop_op.property_id);

         // Check the property's allocation
         BOOST_CHECK_EQUAL(ratio_type(0, 4), property.get_allocation_progress());


         /**
          * Approve the asset by using the update operation
          */
         property_approve_operation aop;
         aop.issuer = meta1.id;
         aop.property_to_approve = property.id;

         trx.clear();
         trx.operations.push_back(aop);
         sign(trx, meta1_private_key);

         PUSH_TX(db, trx);


         /**
          * Advance to the 125% moment
          * The appreciation should be at 100%
          */
         const uint32_t allocation_duration_seconds = (property.approval_end_date.sec_since_epoch() -
                                                       property.creation_date.sec_since_epoch());
         time_point_sec time_to_125_percent = property.creation_date + (allocation_duration_seconds) * 5 / 4;
         generate_blocks(time_to_125_percent, false);
         set_expiration(db, trx);
         trx.clear();

         property = db.get_property(prop_op.property_id);

         BOOST_CHECK_EQUAL(ratio_type(1, 1), property.get_allocation_progress());

         // Check the sell_limit
         asset_limitation_object alo;
         const auto &asset_limitation_idx = db.get_index_type<asset_limitation_index>().indices().get<by_limit_symbol>();
         uint64_t expected_valuation;

         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = meta1_fixture::calculate_meta1_valuation(property.appraised_property_value, 100, 100);
         BOOST_CHECK_EQUAL(expected_valuation, alo.cumulative_sell_limit);


         /**
          * Publish an USD-denominated price for BTC
          */
         asset_price_publish_operation publish_op;
         publish_op.symbol = "BTC";
         publish_op.usd_price = price_ratio(2000, 1); // 2000 USD per BTC
         publish_op.fee_paying_account = meta1_id;

         trx.clear();
         trx.operations.push_back(publish_op);
         sign(trx, meta1_private_key);

         PUSH_TX(db, trx);
         time_point_sec now = db.head_block_time();

         // Confirm that the price can be retrieved
         const auto &idx = db.get_index_type<asset_price_index>().indices().get<by_symbol>();
         auto itr = idx.find("BTC");
         itr = idx.find("BTC");
         BOOST_CHECK(itr != idx.end());
         asset_price price_of_btc = *itr;
         BOOST_CHECK_EQUAL(price_of_btc.symbol, "BTC");
         BOOST_CHECK_EQUAL(price_of_btc.usd_price.numerator, 2000);
         BOOST_CHECK_EQUAL(price_of_btc.usd_price.denominator, 1);
         BOOST_CHECK(price_of_btc.publication_time == now);


         /**
          * The current valuation of all META1 tokens is 10 x 900,000,000 USD = 9,000,000,000 USD
          * There are 450,000,000 META1 tokens = 45,000,000,000,000 satoshi META1 tokens
          * Therefore, each META1 token is valued at 20 USD per META1 token.
          *
          * BTC is priced at 2000 USD per BTC
          *
          * Therefore, any limit order should price 1 META1 token greater than or equal to 20 USD per META1 token.
          * This is equivalent to greater than or equal to 1/100 BTC per META1 token = 1 BTC per 100 META1 token
          */
         limit_order_id_type order_id;
         limit_order_object loo;

         // Attempt to sell META1 for too low of a price; it should fail
         asset meta1_to_sell = META1_ID(db).amount(101 * asset::scaled_precision(META1.precision).value);
         asset btc_to_buy = BTC_ID(db).amount(1 * asset::scaled_precision(BTC.precision).value);
         REQUIRE_EXCEPTION_WITH_TEXT(create_sell_order(alice, meta1_to_sell, btc_to_buy),
                                     "valuation for the META1 token is too low");

         // Attempt to buy META1 for too low of a price; it should fail
         asset meta1_to_buy = meta1_to_sell;
         asset btc_to_sell = btc_to_buy;
         REQUIRE_EXCEPTION_WITH_TEXT(create_sell_order(alice, btc_to_sell, meta1_to_buy),
                                     "valuation for the META1 token is too low");

         // Attempt to sell META1 for high-enough of a price; it should succeed
         meta1_to_sell = META1_ID(db).amount(100 * asset::scaled_precision(META1.precision).value);
         btc_to_buy = BTC_ID(db).amount(1 * asset::scaled_precision(BTC.precision).value);
         loo = *create_sell_order(alice, meta1_to_sell, btc_to_buy);
         order_id = loo.id;
         // Check source
         BOOST_CHECK_EQUAL(get_balance(alice, META1_ID(db)),
                           alice_initial_meta1.amount.value - meta1_to_sell.amount.value);
         BOOST_CHECK_EQUAL(get_balance(alice, BTC_ID(db)), alice_initial_btc.amount.value);
         // Check destination
         BOOST_CHECK(loo.amount_for_sale() == meta1_to_sell);
         BOOST_CHECK(loo.amount_to_receive() == btc_to_buy);
         // Clean up
         cancel_limit_order(order_id(db));

         // Attempt to buy META1 for high-enough of a price; it should succeed
         meta1_to_buy = meta1_to_sell;
         btc_to_sell = btc_to_buy;
         loo = *create_sell_order(alice, btc_to_sell, meta1_to_buy);
         order_id = loo.id;
         // Check source
         BOOST_CHECK_EQUAL(get_balance(alice, META1_ID(db)), alice_initial_meta1.amount.value);
         BOOST_CHECK_EQUAL(get_balance(alice, BTC_ID(db)), alice_initial_btc.amount.value - btc_to_sell.amount.value);
         // Check destination
         BOOST_CHECK(loo.amount_for_sale() == btc_to_sell);
         BOOST_CHECK(loo.amount_to_receive() == meta1_to_buy);
         // Clean up
         cancel_limit_order(order_id(db));


         /**
          * Publish an updated USD-denominated price for BTC
          * 3000 USD per 1 BTC
          */
         generate_blocks(1000);
         set_expiration(db, trx);
         now = db.head_block_time();

         publish_op.usd_price = price_ratio(3000, 1); // 3000 USD per BTC

         trx.clear();
         trx.operations.push_back(publish_op);
         sign(trx, meta1_private_key);

         PUSH_TX(db, trx);

         // Confirm that the price can be retrieved
         itr = idx.find("BTC");
         BOOST_CHECK(itr != idx.end());
         price_of_btc = *itr;
         BOOST_CHECK_EQUAL(price_of_btc.symbol, "BTC");
         BOOST_CHECK_EQUAL(price_of_btc.usd_price.numerator, 3000);
         BOOST_CHECK_EQUAL(price_of_btc.usd_price.denominator, 1);
         BOOST_CHECK(price_of_btc.publication_time == now);


         /**
          * The current valuation of all META1 tokens is 10 x 900,000,000 USD = 9,000,000,000 USD
          * There are 450,000,000 META1 tokens = 45,000,000,000,000 satoshi META1 tokens
          * Therefore, each META1 token is valued at 20 USD per META1 token.
          *
          * BTC is priced at 3000 USD per BTC
          *
          * Therefore, any limit order should price 1 META1 token greater than or equal to 20 USD per META1 token.
          * This is equivalent to greater than or equal to 1/150 BTC per META1 token = 1 BTC per 150 META1 token
          */
         // Attempt to sell META1 for too low of a price; it should fail
         meta1_to_sell = META1_ID(db).amount(151 * asset::scaled_precision(META1.precision).value);
         btc_to_buy = BTC_ID(db).amount(1 * asset::scaled_precision(BTC.precision).value);
         REQUIRE_EXCEPTION_WITH_TEXT(create_sell_order(alice, meta1_to_sell, btc_to_buy),
                                     "valuation for the META1 token is too low");

         // Attempt to buy META1 for too low of a price; it should fail
         meta1_to_buy = meta1_to_sell;
         btc_to_sell = btc_to_buy;
         REQUIRE_EXCEPTION_WITH_TEXT(create_sell_order(alice, btc_to_sell, meta1_to_buy),
                                     "valuation for the META1 token is too low");

         // Attempt to sell META1 for high-enough of a price; it should succeed
         meta1_to_sell = META1_ID(db).amount(150 * asset::scaled_precision(META1.precision).value);
         btc_to_buy = BTC_ID(db).amount(1 * asset::scaled_precision(BTC.precision).value);
         loo = *create_sell_order(alice, meta1_to_sell, btc_to_buy);
         order_id = loo.id;
         // Check source
         BOOST_CHECK_EQUAL(get_balance(alice, META1_ID(db)),
                           alice_initial_meta1.amount.value - meta1_to_sell.amount.value);
         BOOST_CHECK_EQUAL(get_balance(alice, BTC_ID(db)), alice_initial_btc.amount.value);
         // Check destination
         BOOST_CHECK(loo.amount_for_sale() == meta1_to_sell);
         BOOST_CHECK(loo.amount_to_receive() == btc_to_buy);
         // Clean up
         cancel_limit_order(order_id(db));

         // Attempt to buy META1 for high-enough of a price; it should succeed
         meta1_to_buy = meta1_to_sell;
         btc_to_sell = btc_to_buy;
         loo = *create_sell_order(alice, btc_to_sell, meta1_to_buy);
         order_id = loo.id;
         // Check source
         BOOST_CHECK_EQUAL(get_balance(alice, META1_ID(db)), alice_initial_meta1.amount.value);
         BOOST_CHECK_EQUAL(get_balance(alice, BTC_ID(db)), alice_initial_btc.amount.value - btc_to_sell.amount.value);
         // Check destination
         BOOST_CHECK(loo.amount_for_sale() == btc_to_sell);
         BOOST_CHECK(loo.amount_to_receive() == meta1_to_buy);
         // Clean up
         cancel_limit_order(order_id(db));

      }
      FC_LOG_AND_RETHROW()
   }


   /**
    * Test the requirements for new limit orders
    */
   BOOST_AUTO_TEST_CASE(test_requirements_for_new_limit_orders) {
      try {
         /**
          * Initialize
          */
         set_expiration(db, trx);

         // Initialize the actors
         ACTORS((alice)(meta1));
         upgrade_to_lifetime_member(meta1_id);

         // Initialize the assets
         create_user_issued_asset("BTC");
         asset_object BTC = get_asset("BTC");
         const asset_id_type BTC_ID = BTC.id;
         asset_object META1 = get_asset("META1");
         const asset_id_type META1_ID = META1.id;

         account_object issuer_account = get_account("meta1");

         // Fund the actors
         const asset alice_initial_meta1 = META1.amount(5000 * asset::scaled_precision(META1.precision).value);
         transfer(committee_account, alice.id, alice_initial_meta1);
         BOOST_CHECK_EQUAL(get_balance(alice, META1), alice_initial_meta1.amount.value);
         verify_asset_supplies(db);

         auto _issue_uia = [&](const account_object &recipient, asset amount) {
            asset_issue_operation op;
            op.issuer = amount.asset_id(db).issuer;
            op.asset_to_issue = amount;
            op.issue_to_account = recipient.id;
            transaction tx;
            tx.operations.push_back(op);
            set_expiration(db, tx);
            PUSH_TX(db, tx, database::skip_tapos_check | database::skip_transaction_signatures);
         };
         const asset alice_initial_btc = BTC.amount(30 * asset::scaled_precision(BTC.precision).value);
         _issue_uia(alice, alice_initial_btc);


         /**
          * Create the asset limitation
          */
         const string limit_symbol = "META1";
         asset_limitation_object_create_operation create_limitation_op;
         create_limitation_op.limit_symbol = limit_symbol;
         create_limitation_op.issuer = issuer_account.id;

         signed_transaction tx;
         tx.operations.push_back(create_limitation_op);
         set_expiration(db, tx);

         // TODO: Test that only signatures by meta1 are sufficient
         sign(tx, meta1_private_key);

         PUSH_TX(db, tx);


         /**
          * Add a backing asset
          */
         // Create the property
         const property_options property_ops = {
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
         property_create_operation prop_op = create_property_operation("meta1", 1000000000, 10080, "META1",
                                                                       property_ops);

         tx.clear();
         tx.operations.push_back(prop_op);
         set_expiration(db, tx);

         // TODO: Test that only signatures by meta1 are sufficient
         sign(tx, meta1_private_key);

         PUSH_TX(db, tx);

         // Check the initial allocation of the asset before any blocks advance
         graphene::chain::property_object property = db.get_property(prop_op.property_id);

         // Check the property's allocation
         BOOST_CHECK_EQUAL(ratio_type(0, 4), property.get_allocation_progress());


         /**
          * Approve the asset by using the update operation
          */
         property_approve_operation aop;
         aop.issuer = meta1.id;
         aop.property_to_approve = property.id;

         trx.clear();
         trx.operations.push_back(aop);
         sign(trx, meta1_private_key);

         PUSH_TX(db, trx);


         /**
          * Advance to the 125% moment
          * The appreciation should be at 100%
          */
         const uint32_t allocation_duration_seconds = (property.approval_end_date.sec_since_epoch() -
                                                       property.creation_date.sec_since_epoch());
         time_point_sec time_to_125_percent = property.creation_date + (allocation_duration_seconds) * 5 / 4;
         generate_blocks(time_to_125_percent, false);
         set_expiration(db, trx);
         trx.clear();

         property = db.get_property(prop_op.property_id);

         BOOST_CHECK_EQUAL(ratio_type(1, 1), property.get_allocation_progress());

         // Check the sell_limit
         asset_limitation_object alo;
         const auto &asset_limitation_idx = db.get_index_type<asset_limitation_index>().indices().get<by_limit_symbol>();
         uint64_t expected_valuation;

         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = meta1_fixture::calculate_meta1_valuation(property.appraised_property_value, 100, 100);
         BOOST_CHECK_EQUAL(expected_valuation, alo.cumulative_sell_limit);


         /**
          * Publish an USD-denominated price for BTC
          */
         asset_price_publish_operation publish_op;
         publish_op.symbol = "BTC";
         publish_op.usd_price = price_ratio(2000, 1); // 2000 USD per BTC
         publish_op.fee_paying_account = meta1_id;

         trx.clear();
         trx.operations.push_back(publish_op);
         sign(trx, meta1_private_key);

         PUSH_TX(db, trx);
         time_point_sec now = db.head_block_time();

         // Confirm that the price can be retrieved
         const auto &idx = db.get_index_type<asset_price_index>().indices().get<by_symbol>();
         auto itr = idx.find("BTC");
         itr = idx.find("BTC");
         BOOST_CHECK(itr != idx.end());
         asset_price price_of_btc = *itr;
         BOOST_CHECK_EQUAL(price_of_btc.symbol, "BTC");
         BOOST_CHECK_EQUAL(price_of_btc.usd_price.numerator, 2000);
         BOOST_CHECK_EQUAL(price_of_btc.usd_price.denominator, 1);
         BOOST_CHECK(price_of_btc.publication_time == now);


         /**
          * Attempt to sell META1 without a backing asset but without any recent price; it should fail
          */
         generate_blocks(db.head_block_time() + GRAPHENE_DEFAULT_PRICE_FEED_LIFETIME);
         generate_block(); // Advance one more block
         set_expiration(db, trx);

         asset meta1_to_sell = META1_ID(db).amount(2 * asset::scaled_precision(META1.precision).value);
         asset btc_to_buy = BTC_ID(db).amount(3 * asset::scaled_precision(BTC.precision).value);
         REQUIRE_EXCEPTION_WITH_TEXT(create_sell_order(alice, meta1_to_sell, btc_to_buy),
                                     "is too old");

         asset meta1_to_buy = meta1_to_sell;
         asset btc_to_sell = btc_to_buy;
         REQUIRE_EXCEPTION_WITH_TEXT(create_sell_order(alice, btc_to_sell, meta1_to_buy),
                                     "is too old");


         /**
          * Publish an USD-denominated price for BTC
          */
         trx.clear();
         trx.operations.push_back(publish_op);
         sign(trx, meta1_private_key);

         PUSH_TX(db, trx);


         /**
          * The current valuation of all META1 tokens is 10 x 1,000,000,000 USD = 10,000,000,000 USD
          * There are 450,000,000 META1 tokens = 45,000,000,000,000 satoshi META1 tokens
          * Therefore, each META1 token is valued at 22.22222 USD per META1 token.
          *
          * BTC is priced at 2000 USD per BTC
          *
          * Therefore, any limit order should price 1 META1 token greater than or equal to 22.2222 USD per META1 token.
          * This is equivalent to greater than or equal to 1/90 BTC per META1 token = 1 BTC per 90 META1 token
          */
         limit_order_id_type order_id;
         limit_order_object loo;

         // Attempt to sell META1 for too low of a price; it should fail
         meta1_to_sell = META1_ID(db).amount(91 * asset::scaled_precision(META1.precision).value);
         btc_to_buy = BTC_ID(db).amount(1 * asset::scaled_precision(BTC.precision).value);
         REQUIRE_EXCEPTION_WITH_TEXT(create_sell_order(alice, meta1_to_sell, btc_to_buy),
                                     "valuation for the META1 token is too low");

         // Attempt to buy META1 for too low of a price; it should fail
         meta1_to_buy = meta1_to_sell;
         btc_to_sell = btc_to_buy;
         REQUIRE_EXCEPTION_WITH_TEXT(create_sell_order(alice, btc_to_sell, meta1_to_buy),
                                     "valuation for the META1 token is too low");

         // Attempt to sell META1 for high-enough of a price; it should succeed
         meta1_to_sell = META1_ID(db).amount(89 * asset::scaled_precision(META1.precision).value);
         btc_to_buy = BTC_ID(db).amount(1 * asset::scaled_precision(BTC.precision).value);
         loo = *create_sell_order(alice, meta1_to_sell, btc_to_buy);
         order_id = loo.id;
         // Check source
         BOOST_CHECK_EQUAL(get_balance(alice, META1_ID(db)),
                           alice_initial_meta1.amount.value - meta1_to_sell.amount.value);
         BOOST_CHECK_EQUAL(get_balance(alice, BTC_ID(db)), alice_initial_btc.amount.value);
         // Check destination
         BOOST_CHECK(loo.amount_for_sale() == meta1_to_sell);
         BOOST_CHECK(loo.amount_to_receive() == btc_to_buy);
         // Clean up
         cancel_limit_order(order_id(db));

         // Attempt to buy META1 for high-enough of a price; it should succeed
         meta1_to_buy = meta1_to_sell;
         btc_to_sell = btc_to_buy;
         loo = *create_sell_order(alice, btc_to_sell, meta1_to_buy);
         order_id = loo.id;
         // Check source
         BOOST_CHECK_EQUAL(get_balance(alice, META1_ID(db)), alice_initial_meta1.amount.value);
         BOOST_CHECK_EQUAL(get_balance(alice, BTC_ID(db)), alice_initial_btc.amount.value - btc_to_sell.amount.value);
         // Check destination
         BOOST_CHECK(loo.amount_for_sale() == btc_to_sell);
         BOOST_CHECK(loo.amount_to_receive() == meta1_to_buy);
         // Clean up
         cancel_limit_order(order_id(db));

      }
      FC_LOG_AND_RETHROW()
   }


   /**
    * Basic overflow test
    */
   BOOST_AUTO_TEST_CASE(overflow_test) {
      try {
         safe<fc::uint128_t> s1(5);
         fc::uint128_t max_u128 = std::numeric_limits<fc::uint128_t>::max();
         s1 + (max_u128 - 10); // Addition should not overflow
         REQUIRE_EXCEPTION_WITH_TEXT(s1 + max_u128, "Integer Overflow"); // Addition should overflow

         safe<fc::uint128_t> s_4 = max_u128 / 4;
         s_4 * 2; // Multiplication should not overflow
         s_4 * 3; // Multiplication should not overflow
         s_4 * 4; // Multiplication should not overflow
         REQUIRE_EXCEPTION_WITH_TEXT(s_4 * 5, "Integer Overflow"); // Multiplication should overflow
      }
      FC_LOG_AND_RETHROW()
   }


   /**
    * Test overflow detection in the IMPLEMENTATION of the implied price check for new limit orders
    * in limit_order_create_evaluator::do_evaluate()
    * This tests overflow detection in the calculation of the right-hand side (RHS) of Eq. 4b.
    * Overflow in the calculation of the left-hand side (LHS) of Eq. 4b is not possible when the maximum supply of the
    * CORE token is below 7*10^23.
    */
   BOOST_AUTO_TEST_CASE(test_overflow_of_implied_price_for_new_limit_orders) {
      try {
         /**
          * Initialize
          */
         set_expiration(db, trx);

         // Initialize the actors
         ACTORS((alice)(meta1));
         upgrade_to_lifetime_member(meta1_id);

         // Initialize the assets
         {
            // Create an asset with a large precision
            asset_create_operation creator;
            creator.issuer = account_id_type();
            creator.fee = asset();
            creator.symbol = "LARGE";
            creator.precision = 12;
            creator.common_options.core_exchange_rate = price(asset(1, asset_id_type(1)), asset(1));
            creator.common_options.max_supply = GRAPHENE_MAX_SHARE_SUPPLY;
            creator.common_options.flags = charge_market_fee;
            creator.common_options.issuer_permissions = charge_market_fee;
            trx.operations.push_back(std::move(creator));
            trx.validate();
            processed_transaction ptx = PUSH_TX(db, trx, ~0);
         }
         asset_object LARGE = get_asset("LARGE");
         const asset_id_type LARGE_ID = LARGE.id;
         asset_object META1 = get_asset("META1");
         const asset_id_type META1_ID = META1.id;

         account_object issuer_account = get_account("meta1");

         // Fund the actors
         const asset alice_initial_meta1 = META1.amount(400000000 * asset::scaled_precision(META1.precision).value);
         trx.clear();
         transfer(committee_account, alice.id, alice_initial_meta1);
         BOOST_CHECK_EQUAL(get_balance(alice, META1), alice_initial_meta1.amount.value);
         verify_asset_supplies(db);

         auto _issue_uia = [&](const account_object &recipient, asset amount) {
            asset_issue_operation op;
            op.issuer = amount.asset_id(db).issuer;
            op.asset_to_issue = amount;
            op.issue_to_account = recipient.id;
            transaction tx;
            tx.operations.push_back(op);
            set_expiration(db, tx);
            PUSH_TX(db, tx, database::skip_tapos_check | database::skip_transaction_signatures);
         };
         const asset alice_initial_large = LARGE.amount(30 * asset::scaled_precision(LARGE.precision).value);
         trx.clear();
         _issue_uia(alice, alice_initial_large);


         /**
          * Create the asset limitation
          */
         const string limit_symbol = "META1";
         asset_limitation_object_create_operation create_limitation_op;
         create_limitation_op.limit_symbol = limit_symbol;
         create_limitation_op.issuer = issuer_account.id;

         trx.clear();
         trx.operations.push_back(create_limitation_op);
         sign(trx, meta1_private_key);

         PUSH_TX(db, trx);


         /**
          * Add a backing asset appraised at 9*10^15 = 9,000,000,000,000,000 USD
          */
         // Create the property
         const property_options property_ops = {
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
         property_create_operation prop_op = create_property_operation("meta1", 900000000000000, 10080, "META1",
                                                                       property_ops);

         trx.clear();
         trx.operations.push_back(prop_op);
         set_expiration(db, trx);
         sign(trx, meta1_private_key);

         PUSH_TX(db, trx);

         // Check the initial allocation of the asset before any blocks advance
         const graphene::chain::property_object *property = db.get_property(prop_op.property_id);

         // Check the property's allocation
         BOOST_CHECK_EQUAL(ratio_type(0, 4), property->get_allocation_progress());


         /**
          * Approve the asset by using the update operation
          */
         property_approve_operation aop;
         aop.issuer = meta1.id;
         aop.property_to_approve = property->id;

         trx.clear();
         trx.operations.push_back(aop);
         sign(trx, meta1_private_key);

         PUSH_TX(db, trx);


         /**
          * Advance to the 125% moment
          * The appreciation should be at 100%
          */
         const uint32_t allocation_duration_seconds = (property->approval_end_date.sec_since_epoch() -
                                                       property->creation_date.sec_since_epoch());
         time_point_sec time_to_125_percent = property->creation_date + (allocation_duration_seconds) * 5 / 4;
         generate_blocks(time_to_125_percent, false);
         set_expiration(db, trx);
         trx.clear();

         property = db.get_property(prop_op.property_id);

         BOOST_CHECK_EQUAL(ratio_type(1, 1), property->get_allocation_progress());

         // Check the sell_limit
         asset_limitation_object alo;
         const auto &asset_limitation_idx = db.get_index_type<asset_limitation_index>().indices().get<by_limit_symbol>();
         uint64_t expected_valuation;

         alo = *asset_limitation_idx.find(limit_symbol);
         expected_valuation = meta1_fixture::calculate_meta1_valuation(property->appraised_property_value, 100, 100);
         BOOST_CHECK_EQUAL(expected_valuation, alo.cumulative_sell_limit);


         /**
          * Publish an USD-denominated price for LARGE
          */
         asset_price_publish_operation publish_op;
         publish_op.symbol = "LARGE";
         publish_op.usd_price = price_ratio(1, 1073741824ll); // 1/(2^30) USD per LARGE
         publish_op.fee_paying_account = meta1_id;

         trx.clear();
         trx.operations.push_back(publish_op);
         sign(trx, meta1_private_key);

         PUSH_TX(db, trx);
         time_point_sec now = db.head_block_time();

         // Confirm that the price can be retrieved
         const auto &idx = db.get_index_type<asset_price_index>().indices().get<by_symbol>();
         auto itr = idx.find("LARGE");
         itr = idx.find("LARGE");
         BOOST_CHECK(itr != idx.end());
         asset_price price_of_large = *itr;
         BOOST_CHECK_EQUAL(price_of_large.symbol, "LARGE");
         BOOST_CHECK_EQUAL(price_of_large.usd_price.numerator, 1);
         BOOST_CHECK_EQUAL(price_of_large.usd_price.denominator, 1073741824ll);
         BOOST_CHECK(price_of_large.publication_time == now);


         /**
          * The current valuation of all META1 tokens is 10 x 900,000,000,000,000 USD = 9,000,000,000,000,000 USD
          * There are 1,000,000,000 META1 tokens = 100,000,000,000,000 satoshi META1 tokens
          * Therefore, each META1 token is valued at 9,000,000 USD per META1 token.
          *
          * LARGE is priced at 1/(2^30) USD per LARGE
          *
          * Therefore, any limit order should price 1 META1 token greater than or equal to 9 USD per META1 token.
          * This is equivalent to greater than or equal to 1/222.222 LARGE per META1 token = 1 LARGE per 222.222 META1 token
          */
         limit_order_id_type order_id;
         limit_order_object loo;

         // Attempt to sell META1 for too low of a price
         // The determination of this violation should trigger an overflow in the IMPLEMENTATION
         // that checks for this potential violation.
         // Details can be found in limit_order_create_evaluator::do_evaluate().
         // The violation check should detect an overflow in the calculation of the right-hand side (RHS) of Eq. 4b
         const static fc::uint128_t max_u128 = std::numeric_limits<fc::uint128_t>::max();
         const fc::uint128_t delta_precision = asset::scaled_precision(LARGE.precision - META1.precision).value;
         const fc::uint128_t max_m = max_u128 / delta_precision / 9000000000000000ll / 1073741824ll;

         const uint64_t m_overflow = static_cast<uint64_t>(max_m + 1); // This sets RHS of Eq. 4b to require more than 64-bits

         asset meta1_to_sell = META1_ID(db).amount(m_overflow);
         asset large_to_buy = LARGE_ID(db).amount(1 * asset::scaled_precision(LARGE.precision).value);
         REQUIRE_EXCEPTION_WITH_TEXT(create_sell_order(alice, meta1_to_sell, large_to_buy), "Integer Overflow");

         // Attempt to sell META1 for too low of a price
         // The determination of this violation should NOT trigger an overflow in the IMPLEMENTATION
         // that checks for this potential violation.  Therefore, the formal violation should be able to be checked.
         // Details can be found in limit_order_create_evaluator::do_evaluate()
         // The violation check should detect NO overflow in the calculation of the right-hand side (RHS) of Eq. 4b
         meta1_to_sell = META1_ID(db).amount(max_m);
         large_to_buy = LARGE_ID(db).amount(1 * asset::scaled_precision(LARGE.precision).value);
         REQUIRE_EXCEPTION_WITH_TEXT(create_sell_order(alice, meta1_to_sell, large_to_buy),
                                     "valuation for the META1 token is too low");

      }
      FC_LOG_AND_RETHROW()
   }

BOOST_AUTO_TEST_SUITE_END()