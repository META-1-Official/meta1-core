/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#pragma once

#include <fc/io/json.hpp>

#include <graphene/protocol/types.hpp>
#include <graphene/protocol/market.hpp>

#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/liquidity_pool_object.hpp>
#include <graphene/chain/worker_object.hpp>
#include <graphene/chain/asset_limitation_object.hpp>
#include <graphene/chain/operation_history_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/app/application.hpp>
#include <graphene/market_history/market_history_plugin.hpp>

#include <iostream>

using namespace graphene::db;

extern uint32_t GRAPHENE_TESTING_GENESIS_TIMESTAMP;

#define PUSH_TX \
   graphene::chain::test::_push_transaction

#define PUSH_BLOCK \
   graphene::chain::test::_push_block

// See below
#define REQUIRE_OP_VALIDATION_SUCCESS( op, field, value ) \
{ \
   const auto temp = op.field; \
   op.field = value; \
   op.validate(); \
   op.field = temp; \
}
#define REQUIRE_OP_EVALUATION_SUCCESS( op, field, value ) \
{ \
   const auto temp = op.field; \
   op.field = value; \
   trx.operations.back() = op; \
   op.field = temp; \
   db.push_transaction( trx, ~0 ); \
}

#define GRAPHENE_REQUIRE_THROW( expr, exc_type )          \
{                                                         \
   std::string req_throw_info = fc::json::to_string(      \
      fc::mutable_variant_object()                        \
      ("source_file", __FILE__)                           \
      ("source_lineno", __LINE__)                         \
      ("expr", #expr)                                     \
      ("exc_type", #exc_type)                             \
      );                                                  \
   if( fc::enable_record_assert_trip )                    \
      std::cout << "GRAPHENE_REQUIRE_THROW begin "        \
         << req_throw_info << std::endl;                  \
   BOOST_REQUIRE_THROW( expr, exc_type );                 \
   if( fc::enable_record_assert_trip )                    \
      std::cout << "GRAPHENE_REQUIRE_THROW end "          \
         << req_throw_info << std::endl;                  \
}

#define GRAPHENE_CHECK_THROW( expr, exc_type )            \
{                                                         \
   std::string req_throw_info = fc::json::to_string(      \
      fc::mutable_variant_object()                        \
      ("source_file", __FILE__)                           \
      ("source_lineno", __LINE__)                         \
      ("expr", #expr)                                     \
      ("exc_type", #exc_type)                             \
      );                                                  \
   if( fc::enable_record_assert_trip )                    \
      std::cout << "GRAPHENE_CHECK_THROW begin "          \
         << req_throw_info << std::endl;                  \
   BOOST_CHECK_THROW( expr, exc_type );                   \
   if( fc::enable_record_assert_trip )                    \
      std::cout << "GRAPHENE_CHECK_THROW end "            \
         << req_throw_info << std::endl;                  \
}

#define REQUIRE_OP_VALIDATION_FAILURE_2( op, field, value, exc_type ) \
{ \
   const auto temp = op.field; \
   op.field = value; \
   GRAPHENE_REQUIRE_THROW( op.validate(), exc_type ); \
   op.field = temp; \
}
#define REQUIRE_OP_VALIDATION_FAILURE( op, field, value ) \
   REQUIRE_OP_VALIDATION_FAILURE_2( op, field, value, fc::exception )

#define REQUIRE_EXCEPTION_WITH_TEXT(op, exc_text)                 \
{                                                                 \
   try                                                            \
   {                                                              \
      op;                                                         \
      BOOST_FAIL(std::string("Expected an exception with \"") +   \
         std::string(exc_text) +                                  \
         std::string("\" but none thrown"));                      \
   }                                                              \
   catch (fc::exception& ex)                                      \
   {                                                              \
      std::string what = ex.to_string(                            \
            fc::log_level(fc::log_level::all));                   \
      if (what.find(exc_text) == std::string::npos)               \
      {                                                           \
         BOOST_FAIL( std::string("Expected \"") +                 \
            std::string(exc_text) +                               \
            std::string("\" but got \"") +                        \
            std::string(what) );                                  \
      }                                                           \
   }                                                              \
}                                                                 \

#define REQUIRE_THROW_WITH_VALUE_2(op, field, value, exc_type) \
{ \
   auto bak = op.field; \
   op.field = value; \
   trx.operations.back() = op; \
   op.field = bak; \
   GRAPHENE_REQUIRE_THROW(db.push_transaction(trx, ~0), exc_type); \
}

#define REQUIRE_THROW_WITH_VALUE( op, field, value ) \
   REQUIRE_THROW_WITH_VALUE_2( op, field, value, fc::exception )

///This simply resets v back to its default-constructed value. Requires v to have a working assingment operator and
/// default constructor.
#define RESET(v) v = decltype(v)()
///This allows me to build consecutive test cases. It's pretty ugly, but it works well enough for unit tests.
/// i.e. This allows a test on update_account to begin with the database at the end state of create_account.
#define INVOKE(test) ((struct test*)this)->test_method(); trx.clear()

#define PREP_ACTOR(name) \
   fc::ecc::private_key name ## _private_key = generate_private_key(BOOST_PP_STRINGIZE(name));   \
   graphene::chain::public_key_type name ## _public_key = name ## _private_key.get_public_key(); \
   BOOST_CHECK( name ## _public_key != public_key_type() );

#define ACTOR(name) \
   PREP_ACTOR(name) \
   const auto name = create_account(BOOST_PP_STRINGIZE(name), name ## _public_key); \
   graphene::chain::account_id_type name ## _id = name.id; (void)name ## _id;

#define GET_ACTOR(name) \
   fc::ecc::private_key name ## _private_key = generate_private_key(BOOST_PP_STRINGIZE(name)); \
   const account_object& name = get_account(BOOST_PP_STRINGIZE(name)); \
   graphene::chain::account_id_type name ## _id = name.id; \
   (void)name ##_id

#define ACTORS_IMPL(r, data, elem) ACTOR(elem)
#define ACTORS(names) BOOST_PP_SEQ_FOR_EACH(ACTORS_IMPL, ~, names)

#define INITIAL_WITNESS_COUNT (9u)
#define INITIAL_COMMITTEE_MEMBER_COUNT INITIAL_WITNESS_COUNT

namespace graphene { namespace chain {

class clearable_block : public signed_block {
public:
   /** @brief Clears internal cached values like ID, signing key, Merkle root etc. */
   void clear();
};

namespace test {
/// set a reasonable expiration time for the transaction
void set_expiration( const database& db, transaction& tx );

bool _push_block( database& db, const signed_block& b, uint32_t skip_flags = 0 );
processed_transaction _push_transaction( database& db, const signed_transaction& tx, uint32_t skip_flags = 0 );
} // namespace test

struct database_fixture {
   // the reason we use an app is to exercise the indexes of built-in
   //   plugins
   graphene::app::application app;
   genesis_state_type genesis_state;
   chain::database &db;
   signed_transaction trx;
   public_key_type committee_key;
   account_id_type committee_account;
   fc::ecc::private_key private_key = fc::ecc::private_key::generate();
   fc::ecc::private_key init_account_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("null_key")) );
   public_key_type init_account_pub_key;

   optional<fc::temp_directory> data_dir;
   bool skip_key_index_test = false;
   uint32_t anon_acct_count;
   bool hf1270 = false;

    database_fixture(const fc::time_point_sec &initial_timestamp =
                        fc::time_point_sec(GRAPHENE_TESTING_GENESIS_TIMESTAMP));
   ~database_fixture();

   static fc::ecc::private_key generate_private_key(string seed);
   string generate_anon_acct_name();
   static void verify_asset_supplies( const database& db );
   void open_database();
   void vote_for_committee_and_witnesses(uint16_t num_committee, uint16_t num_witness);
   signed_block generate_block(uint32_t skip = ~0,
                               const fc::ecc::private_key& key = generate_private_key("null_key"),
                               int miss_blocks = 0);

   /**
    * @brief Generates block_count blocks
    * @param block_count number of blocks to generate
    */
   void generate_blocks(uint32_t block_count);

   /**
    * @brief Generates blocks until the head block time matches or exceeds timestamp
    * @param timestamp target time to generate blocks until
    * @return number of blocks generated
    */
   uint32_t generate_blocks(fc::time_point_sec timestamp, bool miss_intermediate_blocks = true, uint32_t skip = ~0);

   account_create_operation make_account(
      const std::string& name = "nathan",
      public_key_type = public_key_type()
      );

   account_create_operation make_account(
      const std::string& name,
      const account_object& registrar,
      const account_object& referrer,
      uint16_t referrer_percent = 100,
      public_key_type key = public_key_type()
      );

   void force_global_settle(const asset_object& what, const price& p);
   operation_result force_settle(account_id_type who, asset what)
   { return force_settle(who(db), what); }
   operation_result force_settle(const account_object& who, asset what);
   void update_feed_producers(asset_id_type mia, flat_set<account_id_type> producers)
   { update_feed_producers(mia(db), producers); }
   void update_feed_producers(const asset_object& mia, flat_set<account_id_type> producers);
   void publish_feed(asset_id_type mia, account_id_type by, const price_feed& f)
   { publish_feed(mia(db), by(db), f); }

   /***
    * @brief helper method to add a price feed
    *
    * Adds a price feed for asset2, pushes the transaction, and generates the block
    *
    * @param publisher who is publishing the feed
    * @param asset1 the base asset
    * @param amount1 the amount of the base asset
    * @param asset2 the quote asset
    * @param amount2 the amount of the quote asset
    * @param core_id id of core (helps with core_exchange_rate)
    */
   void publish_feed(const account_id_type& publisher,
         const asset_id_type& asset1, int64_t amount1,
         const asset_id_type& asset2, int64_t amount2,
         const asset_id_type& core_id);

   void publish_feed(const asset_object& mia, const account_object& by, const price_feed& f);

   const call_order_object* borrow( account_id_type who, asset what, asset collateral,
                                    optional<uint16_t> target_cr = {} )
   { return borrow(who(db), what, collateral, target_cr); }
   const call_order_object* borrow( const account_object& who, asset what, asset collateral,
                                    optional<uint16_t> target_cr = {} );
   void cover(account_id_type who, asset what, asset collateral_freed,
                                    optional<uint16_t> target_cr = {} )
   { cover(who(db), what, collateral_freed, target_cr); }
   void cover(const account_object& who, asset what, asset collateral_freed,
                                    optional<uint16_t> target_cr = {} );
   void bid_collateral(const account_object& who, const asset& to_bid, const asset& to_cover);

   const asset_object& get_asset( const string& symbol )const;
   const account_object& get_account( const string& name )const;
   const asset_object& create_bitasset(const string& name,
                                       account_id_type issuer = GRAPHENE_WITNESS_ACCOUNT,
                                       uint16_t market_fee_percent = 100 /*1%*/,
                                       uint16_t flags = charge_market_fee,
                                       uint16_t precision = 2,
                                       asset_id_type backing_asset = {},
                                       share_type max_supply = GRAPHENE_MAX_SHARE_SUPPLY );
   const asset_object& create_prediction_market(const string& name,
                                       account_id_type issuer = GRAPHENE_WITNESS_ACCOUNT,
                                       uint16_t market_fee_percent = 100 /*1%*/,
                                       uint16_t flags = charge_market_fee,
                                       uint16_t precision = GRAPHENE_BLOCKCHAIN_PRECISION_DIGITS,
                                       asset_id_type backing_asset = {});
   const asset_object& create_user_issued_asset( const string& name );
   const asset_object& create_user_issued_asset( const string& name,
                                                 const account_object& issuer,
                                                 uint16_t flags,
                                                 const price& core_exchange_rate = price(asset(1, asset_id_type(1)), asset(1)),
                                                 uint8_t precision = 2 /* traditional precision for tests */,
                                                 uint16_t market_fee_percent = 0,
                                                 additional_asset_options_t options = additional_asset_options_t());
   void issue_uia( const account_object& recipient, asset amount );
   void issue_uia( account_id_type recipient_id, asset amount );
   void reserve_asset( account_id_type account, asset amount );

   const account_object& create_account(
      const string& name,
      const public_key_type& key = public_key_type()
      );

   const account_object& create_account(
      const string& name,
      const account_object& registrar,
      const account_object& referrer,
      uint16_t referrer_percent = 100,
      const public_key_type& key = public_key_type()
      );

   const account_object& create_account(
      const string& name,
      const private_key_type& key,
      const account_id_type& registrar_id = account_id_type(),
      const account_id_type& referrer_id = account_id_type(),
      uint16_t referrer_percent = 100
      );

   const committee_member_object& create_committee_member( const account_object& owner );
   const witness_object& create_witness(account_id_type owner,
                                        const fc::ecc::private_key& signing_private_key = generate_private_key("null_key"),
                                        uint32_t skip_flags = ~0);
   const witness_object& create_witness(const account_object& owner,
                                        const fc::ecc::private_key& signing_private_key = generate_private_key("null_key"),
                                        uint32_t skip_flags = ~0);
   const worker_object& create_worker(account_id_type owner, const share_type daily_pay = 1000, const fc::microseconds& duration = fc::days(2));
   template<typename T>
   proposal_create_operation make_proposal_create_op( const T& op, account_id_type proposer = GRAPHENE_TEMP_ACCOUNT,
                                                      uint32_t timeout = 300, uint32_t review_period = 0 ) const
   {
      proposal_create_operation cop;
      cop.fee_paying_account = proposer;
      cop.expiration_time = db.head_block_time() + timeout;
      cop.review_period_seconds = review_period;
      cop.proposed_ops.emplace_back( op );
      for( auto& o : cop.proposed_ops ) db.current_fee_schedule().set_fee(o.op);
      return cop;
   }
   template<typename T>
   const proposal_object& propose( const T& op, account_id_type proposer = GRAPHENE_TEMP_ACCOUNT,
                                   uint32_t timeout = 300, uint32_t review_period = 0 )
   {
      proposal_create_operation cop = make_proposal_create_op( op, proposer, timeout, review_period );
      trx.operations.clear();
      trx.operations.push_back( cop );
      for( auto& o : trx.operations ) db.current_fee_schedule().set_fee(o);
      trx.validate();
      test::set_expiration( db, trx );
      processed_transaction ptx = PUSH_TX(db, trx, ~0);
      const operation_result& op_result = ptx.operation_results.front();
      trx.operations.clear();
      verify_asset_supplies(db);
      return db.get<proposal_object>( op_result.get<object_id_type>() );
   }
   uint64_t fund( const account_object& account, const asset& amount = asset(500000) );
   digest_type digest( const transaction& tx );
   void sign( signed_transaction& trx, const fc::ecc::private_key& key );
   const limit_order_object* create_sell_order( account_id_type user, const asset& amount, const asset& recv,
                                                const time_point_sec order_expiration = time_point_sec::maximum(),
                                                const price& fee_core_exchange_rate = price::unit_price() );
   const limit_order_object* create_sell_order( const account_object& user, const asset& amount, const asset& recv,
                                                const time_point_sec order_expiration = time_point_sec::maximum(),
                                                const price& fee_core_exchange_rate = price::unit_price() );
   asset cancel_limit_order( const limit_order_object& order );
   void  delete_property( const property_object& property);
   void transfer( account_id_type from, account_id_type to, const asset& amount, const asset& fee = asset() );
   void transfer( const account_object& from, const account_object& to, const asset& amount, const asset& fee = asset() );
   void fund_fee_pool( const account_object& from, const asset_object& asset_to_fund, const share_type amount );

   liquidity_pool_create_operation make_liquidity_pool_create_op( account_id_type account, asset_id_type asset_a,
                                                  asset_id_type asset_b, asset_id_type share_asset,
                                                  uint16_t taker_fee_percent, uint16_t withdrawal_fee_percent )const;
   const liquidity_pool_object& create_liquidity_pool( account_id_type account, asset_id_type asset_a,
                                                  asset_id_type asset_b, asset_id_type share_asset,
                                                  uint16_t taker_fee_percent, uint16_t withdrawal_fee_percent );
   liquidity_pool_delete_operation make_liquidity_pool_delete_op( account_id_type account,
                                                  liquidity_pool_id_type pool )const;
   generic_operation_result delete_liquidity_pool( account_id_type account, liquidity_pool_id_type pool );
   liquidity_pool_deposit_operation make_liquidity_pool_deposit_op( account_id_type account,
                                                  liquidity_pool_id_type pool, const asset& amount_a,
                                                  const asset& amount_b )const;
   generic_exchange_operation_result deposit_to_liquidity_pool( account_id_type account,
                                                  liquidity_pool_id_type pool, const asset& amount_a,
                                                  const asset& amount_b );
   liquidity_pool_withdraw_operation make_liquidity_pool_withdraw_op( account_id_type account,
                                                  liquidity_pool_id_type pool, const asset& share_amount )const;
   generic_exchange_operation_result withdraw_from_liquidity_pool( account_id_type account,
                                                  liquidity_pool_id_type pool, const asset& share_amount );
   liquidity_pool_exchange_operation make_liquidity_pool_exchange_op( account_id_type account,
                                                  liquidity_pool_id_type pool, const asset& amount_to_sell,
                                                  const asset& min_to_receive )const;
   generic_exchange_operation_result exchange_with_liquidity_pool( account_id_type account,
                                                  liquidity_pool_id_type pool, const asset& amount_to_sell,
                                                  const asset& min_to_receive );

   /**
    * NOTE: This modifies the database directly. You will probably have to call this each time you
    * finish creating a block
    */
   void enable_fees();
   void change_fees( const fee_parameters::flat_set_type& new_params, uint32_t new_scale = 0 );
   void upgrade_to_lifetime_member( account_id_type account );
   void upgrade_to_lifetime_member( const account_object& account );
   void upgrade_to_annual_member( account_id_type account );
   void upgrade_to_annual_member( const account_object& account );
   void print_market( const string& syma, const string& symb )const;
   string pretty( const asset& a )const;
   void print_limit_order( const limit_order_object& cur )const;
   void print_call_orders( )const;
   void print_joint_market( const string& syma, const string& symb )const;
   int64_t get_balance( account_id_type account, asset_id_type a )const;
   int64_t get_balance( const account_object& account, const asset_object& a )const;

   int64_t get_market_fee_reward( account_id_type account, asset_id_type asset )const;
   int64_t get_market_fee_reward( const account_object& account, const asset_object& asset )const;

   vector< operation_history_object > get_operation_history( account_id_type account_id )const;
   vector< graphene::market_history::order_history_object > get_market_order_history( asset_id_type a, asset_id_type b )const;

   const property_object& get_property(uint32_t property_id) const;
   const asset_limitation_object& get_asset_limitation(string limit_symbol) const;

   bool validation_current_test_name_for_setting_api_limit( const string& current_test_name )const;

   /****
    * @brief return htlc fee parameters
    */
   flat_map< uint64_t, graphene::chain::fee_parameters > get_htlc_fee_parameters();
   /****
    * @brief push through a proposal that sets htlc parameters and fees
    */
   void set_htlc_committee_parameters();
   /****
    * Hash the preimage and put it in a vector
    * @param preimage the preimage
    * @returns a vector that cointains the sha256 hash of the preimage
    */
   template<typename H>
   H hash_it(std::vector<char> preimage)
   {
      return H::hash( (char*)preimage.data(), preimage.size() );
   }

};

} }
