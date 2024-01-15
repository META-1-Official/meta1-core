/*
 * Copyright (c) 2017 Cryptonomex, Inc., and contributors.
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

#include <graphene/app/api_objects.hpp>

#include <graphene/protocol/types.hpp>

#include <graphene/chain/database.hpp>

#include <graphene/chain/balance_object.hpp>
#include <graphene/chain/chain_property_object.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/confidential_object.hpp>
#include <graphene/chain/liquidity_pool_object.hpp>
#include <graphene/chain/credit_offer_object.hpp>
#include <graphene/chain/operation_history_object.hpp>
#include <graphene/chain/samet_fund_object.hpp>
#include <graphene/chain/ticket_object.hpp>
#include <graphene/chain/worker_object.hpp>
#include <graphene/chain/witness_object.hpp>

#include <fc/api.hpp>
#include <fc/variant_object.hpp>

#include <boost/container/flat_set.hpp>

#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace graphene { namespace app {

using namespace graphene::chain;
using namespace graphene::market_history;
using std::string;
using std::vector;
using std::map;

class database_api_impl;

/**
 * @brief The database_api class implements the RPC API for the chain database.
 *
 * This API exposes accessors on the database which query state tracked by a blockchain validating node. This API is
 * read-only; all modifications to the database must be performed via transactions. Transactions are broadcast via
 * the @ref network_broadcast_api.
 */
class database_api
{
   public:
      database_api(graphene::chain::database& db, const application_options* app_options = nullptr );
      ~database_api();

      /////////////
      // Objects //
      /////////////

      /**
       * @brief Get the objects corresponding to the provided IDs
       * @param ids IDs of the objects to retrieve
       * @param subscribe @a true to subscribe to the queried objects, @a false to not subscribe,
       *                  @a null to subscribe or not subscribe according to current auto-subscription setting
       *                  (see @ref set_auto_subscription)
       * @return The objects retrieved, in the order they are mentioned in ids
       * @note operation_history_object (1.11.x) and account_history_object (2.9.x)
       *       can not be subscribed.
       *
       * If any of the provided IDs does not map to an object, a null variant is returned in its position.
       */
      fc::variants get_objects( const vector<object_id_type>& ids,
                                optional<bool> subscribe = optional<bool>() )const;

      ///////////////////
      // Subscriptions //
      ///////////////////

      /**
       * @brief Register a callback handle which then can be used to subscribe to object database changes
       * @param cb The callback handle to register
       * @param notify_remove_create Whether subscribe to universal object creation and removal events.
       *        If this is set to true, the API server will notify all newly created objects and ID of all
       *        newly removed objects to the client, no matter whether client subscribed to the objects.
       *        By default, API servers don't allow subscribing to universal events, which can be changed
       *        on server startup.
       *
       * Note: auto-subscription is enabled by default and can be disabled with @ref set_auto_subscription API.
       */
      void set_subscribe_callback( std::function<void(const variant&)> cb, bool notify_remove_create );
      /**
       * @brief Set auto-subscription behavior of follow-up API queries
       * @param enable whether follow-up API queries will automatically subscribe to queried objects
       *
        * Impacts behavior of these APIs:
       * - get_accounts
       * - get_assets
       * - get_objects
       * - lookup_accounts
       * - get_full_accounts
       * - get_htlc
       * - get_liquidity_pools
       * - get_liquidity_pools_by_share_asset
       *
       * Note: auto-subscription is enabled by default
       *
       * @see @ref set_subscribe_callback
       */
      void set_auto_subscription( bool enable );
      /**
       * @brief Register a callback handle which will get notified when a transaction is pushed to database
       * @param cb The callback handle to register
       *
       * Note: a transaction can be pushed to database and be popped from database several times while
       *   processing, before and after included in a block. Everytime when a push is done, the client will
       *   be notified.
       */
      void set_pending_transaction_callback( std::function<void(const variant& signed_transaction_object)> cb );
      /**
       * @brief Register a callback handle which will get notified when a block is pushed to database
       * @param cb The callback handle to register
       */
      void set_block_applied_callback( std::function<void(const variant& block_id)> cb );
      /**
       * @brief Stop receiving any notifications
       *
       * This unsubscribes from all subscribed markets and objects.
       */
      void cancel_all_subscriptions();

      /////////////////////////////
      // Blocks and transactions //
      /////////////////////////////

      /**
       * @brief Retrieve a block header
       * @param block_num Height of the block whose header should be returned
       * @param with_witness_signature Whether to return witness signature. Optional.
       *                               If omitted or is @a false, will not return witness signature.
       * @return header of the referenced block, or null if no matching block was found
       */
      optional<maybe_signed_block_header> get_block_header(
            uint32_t block_num,
            const optional<bool>& with_witness_signature = optional<bool>() )const;

      /**
       * @brief Retrieve multiple block headers by block numbers
       * @param block_nums vector containing heights of the blocks whose headers should be returned
       * @param with_witness_signatures Whether to return witness signatures. Optional.
       *                                If omitted or is @a false, will not return witness signatures.
       * @return array of headers of the referenced blocks, or null if no matching block was found
       */
      map<uint32_t, optional<maybe_signed_block_header>> get_block_header_batch(
            const vector<uint32_t>& block_nums,
            const optional<bool>& with_witness_signatures = optional<bool>() )const;

      /**
       * @brief Retrieve a full, signed block
       * @param block_num Height of the block to be returned
       * @return the referenced block, or null if no matching block was found
       */
      optional<signed_block> get_block(uint32_t block_num)const;

      /**
       * @brief used to fetch an individual transaction.
       * @param block_num height of the block to fetch
       * @param trx_in_block the index (sequence number) of the transaction in the block, starts from 0
       * @return the transaction at the given position
       */
      processed_transaction get_transaction( uint32_t block_num, uint32_t trx_in_block )const;

      /**
       * If the transaction has not expired, this method will return the transaction for the given ID or
       * it will return NULL if it is not known.  Just because it is not known does not mean it wasn't
       * included in the blockchain.
       *
       * @param txid hash of the transaction
       * @return the corresponding transaction if found, or null if not found
       */
      optional<signed_transaction> get_recent_transaction_by_id( const transaction_id_type& txid )const;

      /////////////
      // Globals //
      /////////////

      /**
       * @brief Retrieve the @ref graphene::chain::chain_property_object associated with the chain
       */
      chain_property_object get_chain_properties()const;

      /**
       * @brief Retrieve the current @ref graphene::chain::global_property_object
       */
      global_property_object get_global_properties()const;

      /**
       * @brief Retrieve compile-time constants
       */
      fc::variant_object get_config()const;

      /**
       * @brief Get the chain ID
       */
      chain_id_type get_chain_id()const;

      /**
       * @brief Retrieve the current @ref graphene::chain::dynamic_global_property_object
       */
      dynamic_global_property_object get_dynamic_global_properties()const;

      /**
       * @brief Get the next object ID in an object space
       * @param space_id The space ID
       * @param type_id The type ID
       * @param with_pending_transactions Whether to include pending transactions
       * @return The next object ID to be assigned
       * @throw fc::exception If the object space does not exist, or @p with_pending_transactions is @a false but
       *                      the api_helper_indexes plugin is not enabled
       */
      object_id_type get_next_object_id( uint8_t space_id, uint8_t type_id, bool with_pending_transactions )const;

      //////////
      // Keys //
      //////////

      /**
       * @brief Get all accounts that refer to the specified public keys in their owner authority, active authorities
       *        or memo key
       * @param keys a list of public keys to query,
       *              the quantity should not be greater than the configured value of
       *              @a api_limit_get_key_references
       * @return ID of all accounts that refer to the specified keys
       */
      vector<flat_set<account_id_type>> get_key_references( vector<public_key_type> keys )const;

      /**
       * Determine whether a textual representation of a public key
       * (in Base-58 format) is *currently* linked
       * to any *registered* (i.e. non-stealth) account on the blockchain
       * @param public_key Public key
       * @return Whether a public key is known
       */
      bool is_public_key_registered(string public_key) const;

      //////////////
      // Accounts //
      //////////////

      /**
       * @brief Get account object from a name or ID
       * @param name_or_id name or ID of the account
       * @return Account ID
       *
       */
      account_id_type get_account_id_from_string(const std::string& name_or_id) const;

      /**
       * @brief Get a list of accounts by names or IDs
       * @param account_names_or_ids names or IDs of the accounts to retrieve
       * @param subscribe @a true to subscribe to the queried account objects, @a false to not subscribe,
       *                  @a null to subscribe or not subscribe according to current auto-subscription setting
       *                  (see @ref set_auto_subscription)
       * @return The accounts corresponding to the provided names or IDs
       *
       * This function has semantics identical to @ref get_objects
       */
      vector<optional<account_object>> get_accounts( const vector<std::string>& account_names_or_ids,
                                                     optional<bool> subscribe = optional<bool>() )const;

      /**
       * @brief Fetch objects relevant to the specified accounts and optionally subscribe to updates
       * @param names_or_ids Each item must be the name or ID of an account to retrieve,
       *              the quantity should not be greater than the configured value of
       *              @a api_limit_get_full_accounts
       * @param subscribe @a true to subscribe to the queried full account objects, @a false to not subscribe,
       *                  @a null to subscribe or not subscribe according to current auto-subscription setting
       *                  (see @ref set_auto_subscription)
       * @return Map of string from @p names_or_ids to the corresponding account
       *
       * This function fetches relevant objects for the given accounts, and subscribes to updates to the given
       * accounts. If any of the strings in @p names_or_ids cannot be tied to an account, that input will be
       * ignored. Other accounts will be retrieved and subscribed.
       * @note The maximum number of accounts allowed to subscribe per connection is configured by the
       *       @a api_limit_get_full_accounts_subscribe option. Exceeded subscriptions will be ignored.
       * @note For each object type, the maximum number of objects to return is configured by the
       *       @a api_limit_get_full_accounts_lists option. Exceeded objects need to be queried with other APIs.
       *
       */
      std::map<string,full_account> get_full_accounts( const vector<string>& names_or_ids,
                                                       optional<bool> subscribe = optional<bool>() );

      /**
       * @brief Returns vector of voting power sorted by reverse vp_active
       * @param limit Maximum number of accounts to retrieve, must not exceed the configured value of
       *              @a api_limit_get_top_voters
       * @return Desc Sorted voting power vector
       */
      vector<account_statistics_object> get_top_voters(uint32_t limit)const;

      /**
       * @brief Get info of an account by name
       * @param name Name of the account to retrieve
       * @return The account holding the provided name
       */
      optional<account_object> get_account_by_name( string name )const;

      /**
       * @brief Get all accounts that refer to the specified account in their owner or active authorities
       * @param account_name_or_id Account name or ID to query
       * @return all accounts that refer to the specified account in their owner or active authorities
       */
      vector<account_id_type> get_account_references( const std::string account_name_or_id )const;

      /**
       * @brief Get a list of accounts by name
       * @param account_names Names of the accounts to retrieve
       * @return The accounts holding the provided names
       *
       * This function has semantics identical to @ref get_objects, but doesn't subscribe.
       */
      vector<optional<account_object>> lookup_account_names(const vector<string>& account_names)const;

      /**
       * @brief Get names and IDs for registered accounts
       * @param lower_bound_name Lower bound of the first name to return
       * @param limit Maximum number of results to return, must not exceed the configured value of
       *              @a api_limit_lookup_accounts
       * @param subscribe @a true to subscribe to the queried account objects, @a false to not subscribe,
       *                  @a null to subscribe or not subscribe according to current auto-subscription setting
       *                  (see @ref set_auto_subscription)
       * @return Map of account names to corresponding IDs
       *
       * @note In addition to the common auto-subscription rules,
       *       this API will subscribe to the returned account only if @p limit is 1.
       */
      map<string, account_id_type, std::less<>> lookup_accounts( const string& lower_bound_name,
                                                   uint32_t limit,
                                                   const optional<bool>& subscribe = optional<bool>() )const;

      //////////////
      // Balances //
      //////////////

      /**
       * @brief Get an account's balances in various assets
       * @param account_name_or_id name or ID of the account to get balances for
       * @param assets IDs of the assets to get balances of; if empty, get all assets account has a balance in
       * @return Balances of the account
       */
      vector<asset> get_account_balances( const std::string& account_name_or_id,
                                          const flat_set<asset_id_type>& assets )const;

      /// Semantically equivalent to @ref get_account_balances.
      vector<asset> get_named_account_balances(const std::string& name, const flat_set<asset_id_type>& assets)const;

      /**
       * @brief Return all unclaimed balance objects for a list of addresses
       * @param addrs a list of addresses
       * @return all unclaimed balance objects for the addresses
       */
      vector<balance_object> get_balance_objects( const vector<address>& addrs )const;

      /**
       * @brief Calculate how much assets in the given balance objects are able to be claimed at current head
       *        block time
       * @param objs a list of balance object IDs
       * @return a list indicating how much asset in each balance object is available to be claimed
       */
      vector<asset> get_vested_balances( const vector<balance_id_type>& objs )const;

      /**
       * @brief Return all vesting balance objects owned by an account
       * @param account_name_or_id name or ID of an account
       * @return all vesting balance objects owned by the account
       */
      vector<vesting_balance_object> get_vesting_balances( const std::string account_name_or_id )const;

      /**
       * @brief Get the total number of accounts registered with the blockchain
       */
      uint64_t get_account_count()const;

      ////////////////////
      //  Backed Assets //
      ////////////////////

      vector<optional<property_object>> get_properties(const vector<uint32_t>& properties_ids)const;
      bool is_property_exists(uint32_t property_id)const;
      vector<property_object> get_all_properties() const;
      vector<property_object>  get_properties_by_backed_asset_symbol(string symbol) const;
      optional<property_object> get_property_by_id( uint32_t id )const;

      /////////////////////
      //Assets Limitation//
      /////////////////////

      bool is_asset_limitation_exists(string limit_symbol)const;
      optional<asset_limitation_object> get_asset_limitaion_by_symbol( string limit_symbol )const;
      /**
       * @brief Get the cumulative asset limitation of a backed asset
       * @param asset_symbol symbol names or IDs of the assets to retrieve
       * @return USD-denominated value if the asset is recognized and if it is recognized backed asset,
       * otherwise throws an exception
       */
      uint64_t get_asset_limitation_value( const string symbol_or_id )const;

      ////////////
      // Assets //
      ////////////

      /**
       * @brief Get asset ID from an asset symbol or ID
       * @param symbol_or_id symbol name or ID of the asset
       * @return asset ID
       */
      asset_id_type get_asset_id_from_string(const std::string& symbol_or_id) const;

      /**
       * @brief Get a list of assets by symbol names or IDs
       * @param asset_symbols_or_ids symbol names or IDs of the assets to retrieve
       * @param subscribe @a true to subscribe to the queried asset objects, @a false to not subscribe,
       *                  @a null to subscribe or not subscribe according to current auto-subscription setting
       *                  (see @ref set_auto_subscription)
       * @return The assets corresponding to the provided symbol names or IDs
       *
       * This function has semantics identical to @ref get_objects
       */
      vector<optional<extended_asset_object>> get_assets( const vector<std::string>& asset_symbols_or_ids,
                                                          optional<bool> subscribe = optional<bool>() )const;

      /**
       * @brief Get assets alphabetically by symbol name
       * @param lower_bound_symbol Lower bound of symbol names to retrieve
       * @param limit Maximum number of assets to fetch, must not exceed the configured value of
       *              @a api_limit_get_assets
       * @return The assets found
       */
      vector<extended_asset_object> list_assets(const string& lower_bound_symbol, uint32_t limit)const;

      /**
       * @brief Get a list of assets by symbol names or IDs
       * @param symbols_or_ids symbol names or IDs of the assets to retrieve
       * @return The assets corresponding to the provided symbols or IDs
       *
       * This function has semantics identical to @ref get_objects, but doesn't subscribe
       */
      vector<optional<extended_asset_object>> lookup_asset_symbols(const vector<string>& symbols_or_ids)const;

      /**
       * @brief Get assets count
       * @return The assets count
       */
      uint64_t get_asset_count()const;

      /**
       * @brief Get assets issued (owned) by a given account
       * @param issuer_name_or_id Account name or ID to get objects from
       * @param start Asset objects(1.3.X) before this ID will be skipped in results. Pagination purposes.
       * @param limit Maximum number of assets to retrieve, must not exceed the configured value of
       *              @a api_limit_get_assets
       * @return The assets issued (owned) by the account
       */
      vector<extended_asset_object> get_assets_by_issuer(const std::string& issuer_name_or_id,
                                                         asset_id_type start, uint32_t limit)const;

      /////////////////////
      // Markets / feeds //
      /////////////////////

      /**
       * @brief Get limit orders in a given market
       * @param a symbol or ID of asset being sold
       * @param b symbol or ID of asset being purchased
       * @param limit Maximum number of orders to retrieve, must not exceed the configured value of
       *              @a api_limit_get_limit_orders
       * @return The limit orders, ordered from least price to greatest
       */
      vector<limit_order_object> get_limit_orders(std::string a, std::string b, uint32_t limit)const;

      /**
       * @brief Fetch open limit orders in all markets relevant to the specified account, ordered by ID
       *
       * @param account_name_or_id  The name or ID of an account to retrieve
       * @param limit  The limitation of items each query can fetch, not greater than the configured value of
       *               @a api_limit_get_limit_orders_by_account
       * @param start_id  Start order id, fetch orders whose IDs are greater than or equal to this order
       *
       * @return List of limit orders of the specified account
       *
       * @note
       * 1. If @p account_name_or_id cannot be tied to an account, an error will be returned
       * 2. @p limit can be omitted or be @a null, if so the configured value of
       *      @a api_limit_get_limit_orders_by_account will be used
       * 3. @p start_id can be omitted or be null, if so the api will return the "first page" of orders
       * 4. One or more optional parameters can be omitted from the end of the parameter list, and the optional
       *    parameters in the middle cannot be omitted (but can be @a null).
       */
      vector<limit_order_object> get_limit_orders_by_account(
            const string& account_name_or_id,
            const optional<uint32_t>& limit = optional<uint32_t>(),
            const optional<limit_order_id_type>& start_id = optional<limit_order_id_type>() );

      /**
       * @brief Fetch all orders relevant to the specified account and specified market, result orders
       *        are sorted descendingly by price
       *
       * @param account_name_or_id  The name or ID of an account to retrieve
       * @param base  Base asset
       * @param quote  Quote asset
       * @param limit  The limitation of items each query can fetch, not greater than the configured value of
       *               @a api_limit_get_account_limit_orders
       * @param ostart_id  Start order id, fetch orders which price lower than this order,
       *                   or price equal to this order but order ID greater than this order
       * @param ostart_price  Fetch orders with price lower than or equal to this price
       *
       * @return List of orders from @p account_name_or_id to the corresponding account
       *
       * @note
       * 1. If @p account_name_or_id cannot be tied to an account, an error will be returned
       * 2. @p ostart_id and @p ostart_price can be empty, if so the api will return the "first page" of orders;
       *    if @p ostart_id is specified, its price will be used to do page query preferentially,
       *    otherwise the @p ostart_price will be used;
       *    @p ostart_id and @p ostart_price may be used cooperatively in case of the order specified by @p ostart_id
       *    was just canceled accidentally, in such case, the result orders' price may lower or equal to
       *    @p ostart_price, but orders' id greater than @p ostart_id
       */
      vector<limit_order_object> get_account_limit_orders( const string& account_name_or_id,
            const string &base,
            const string &quote,
            uint32_t limit = application_options::get_default().api_limit_get_account_limit_orders,
            optional<limit_order_id_type> ostart_id = optional<limit_order_id_type>(),
            optional<price> ostart_price = optional<price>());

      /**
       * @brief Get call orders (aka margin positions) for a given asset
       * @param a symbol name or ID of the debt asset
       * @param limit Maximum number of orders to retrieve, must not exceed the configured value of
       *              @a api_limit_get_call_orders
       * @return The call orders, ordered from earliest to be called to latest
       */
      vector<call_order_object> get_call_orders(const std::string& a, uint32_t limit)const;

      /**
       * @brief Get call orders (aka margin positions) of a given account
       * @param account_name_or_id Account name or ID to get objects from
       * @param start Asset objects(1.3.X) before this ID will be skipped in results. Pagination purposes.
       * @param limit Maximum number of orders to retrieve, must not exceed the configured value of
       *              @a api_limit_get_call_orders
       * @return The call orders of the account
       */
      vector<call_order_object> get_call_orders_by_account(const std::string& account_name_or_id,
                                                           asset_id_type start, uint32_t limit)const;

      /**
       * @brief Get forced settlement orders in a given asset
       * @param a Symbol or ID of asset being settled
       * @param limit Maximum number of orders to retrieve, must not exceed the configured value of
       *              @a api_limit_get_settle_orders
       * @return The settle orders, ordered from earliest settlement date to latest
       */
      vector<force_settlement_object> get_settle_orders(const std::string& a, uint32_t limit)const;

      /**
       * @brief Get forced settlement orders of a given account
       * @param account_name_or_id Account name or ID to get objects from
       * @param start Force settlement objects(1.4.X) before this ID will be skipped in results. Pagination purposes.
       * @param limit Maximum number of orders to retrieve, must not exceed the configured value of
       *              @a api_limit_get_settle_orders
       * @return The settle orders, ordered from earliest settlement date to latest
       * @return The settle orders of the account
       */
      vector<force_settlement_object> get_settle_orders_by_account( const std::string& account_name_or_id,
                                                                    force_settlement_id_type start,
                                                                    uint32_t limit )const;

      /**
       * @brief Get collateral_bid_objects for a given asset
       * @param a Symbol or ID of asset
       * @param limit Maximum number of objects to retrieve, must not exceed the configured value of
       *              @a api_limit_get_collateral_bids
       * @param start skip that many results
       * @return The settle orders, ordered from earliest settlement date to latest
       */
      vector<collateral_bid_object> get_collateral_bids(const std::string& a, uint32_t limit, uint32_t start)const;

      /**
       * @brief Get open margin positions of a given account
       * @param account_name_or_id name or ID of an account
       * @return open margin positions of the account
       *
       * Similar to @ref get_call_orders_by_account, but only the first page will be returned, the page size is
       * the configured value of @a api_limit_get_call_orders.
       */
      vector<call_order_object> get_margin_positions( const std::string& account_name_or_id )const;

      /**
       * @brief Request notification when the active orders in the market between two assets changes
       * @param callback Callback method which is called when the market changes
       * @param a symbol name or ID of the first asset
       * @param b symbol name or ID of the second asset
       *
       * Callback will be passed a variant containing a vector<pair<operation, operation_result>>. The vector will
       * contain, in order, the operations which changed the market, and their results.
       */
      void subscribe_to_market(std::function<void(const variant&)> callback,
                               const std::string& a, const std::string& b);

      /**
       * @brief Unsubscribe from updates to a given market
       * @param a symbol name or ID of the first asset
       * @param b symbol name or ID of the second asset
       */
      void unsubscribe_from_market( const std::string& a, const std::string& b );

      /**
       * @brief Returns the ticker for the market assetA:assetB
       * @param base symbol name or ID of the base asset
       * @param quote symbol name or ID of the quote asset
       * @return The market ticker for the past 24 hours.
       */
      market_ticker get_ticker( const string& base, const string& quote )const;

      /**
       * @brief Returns the 24 hour volume for the market assetA:assetB
       * @param base symbol name or ID of the base asset
       * @param quote symbol name or ID of the quote asset
       * @return The market volume over the past 24 hours
       */
      market_volume get_24_volume( const string& base, const string& quote )const;

      /**
       * @brief Returns the order book for the market base:quote
       * @param base symbol name or ID of the base asset
       * @param quote symbol name or ID of the quote asset
       * @param limit depth of the order book to retrieve, for bids and asks each, capped at the configured value of
       *              @a api_limit_get_order_book and @a api_limit_get_limit_orders
       * @return Order book of the market
       */
      order_book get_order_book( const string& base, const string& quote,
            uint32_t limit = application_options::get_default().api_limit_get_order_book )const;

      /**
       * @brief Returns vector of tickers sorted by reverse base_volume
       * @note this API is experimental and subject to change in next releases
       * @param limit Max number of results, must not exceed the configured value of
       *              @a api_limit_get_top_markets
       * @return Desc Sorted ticker vector
       */
      vector<market_ticker> get_top_markets(uint32_t limit)const;

      /**
       * @brief Get market transactions occurred in the market base:quote, ordered by time, most recent first.
       * @param base symbol or ID of the base asset
       * @param quote symbol or ID of the quote asset
       * @param start Start time as a UNIX timestamp, the latest transactions to retrieve
       * @param stop Stop time as a UNIX timestamp, the earliest transactions to retrieve
       * @param limit Maximum quantity of transactions to retrieve, capped at the configured value of
       *              @a api_limit_get_trade_history
       * @return Transactions in the market
       * @note The time must be UTC, timezone offsets are not supported. The range is [stop, start].
       *       In case when there are more transactions than @p limit occurred in the same second,
       *       this API only returns the most recent records, the rest records can be retrieved
       *       with the @ref get_trade_history_by_sequence API.
       */
      vector<market_trade> get_trade_history( const string& base, const string& quote,
            fc::time_point_sec start, fc::time_point_sec stop,
            uint32_t limit = application_options::get_default().api_limit_get_trade_history )const;

      /**
       * @brief Get market transactions occurred in the market base:quote, ordered by time, most recent first.
       * @param base symbol or ID of the base asset
       * @param quote symbol or ID of the quote asset
       * @param start Start sequence as an Integer, the latest transaction to retrieve
       * @param stop Stop time as a UNIX timestamp, the earliest transactions to retrieve
       * @param limit Maximum quantity of transactions to retrieve, capped at the configured value of
       *              @a api_limit_get_trade_history_by_sequence
       * @return Transactions in the market
       * @note The time must be UTC, timezone offsets are not supported. The range is [stop, start].
       */
      vector<market_trade> get_trade_history_by_sequence( const string& base, const string& quote,
            int64_t start, fc::time_point_sec stop,
            uint32_t limit = application_options::get_default().api_limit_get_trade_history_by_sequence )const;


      /////////////////////
      // Liquidity pools //
      /////////////////////

      /**
       * @brief Get a list of liquidity pools
       * @param limit The limitation of items each query can fetch, not greater than the configured value of
       *              @a api_limit_get_liquidity_pools
       * @param start_id Start liquidity pool id, fetch pools whose IDs are greater than or equal to this ID
       * @param with_statistics Whether to return statistics
       * @return The liquidity pools
       *
       * @note
       * 1. @p limit can be omitted or be @a null, if so the configured value of
       *       @a api_limit_get_liquidity_pools will be used
       * 2. @p start_id can be omitted or be @a null, if so the api will return the "first page" of pools
       * 3. One or more optional parameters can be omitted from the end of the parameter list, and the optional
       *    parameters in the middle cannot be omitted (but can be @a null).
       */
      vector<extended_liquidity_pool_object> list_liquidity_pools(
            const optional<uint32_t>& limit = optional<uint32_t>(),
            const optional<liquidity_pool_id_type>& start_id = optional<liquidity_pool_id_type>(),
            const optional<bool>& with_statistics = false )const;

      /**
       * @brief Get a list of liquidity pools by the symbol or ID of the first asset in the pool
       * @param asset_symbol_or_id symbol name or ID of the asset
       * @param limit The limitation of items each query can fetch, not greater than the configured value of
       *              @a api_limit_get_liquidity_pools
       * @param start_id Start liquidity pool id, fetch pools whose IDs are greater than or equal to this ID
       * @param with_statistics Whether to return statistics
       * @return The liquidity pools
       *
       * @note
       * 1. If @p asset_symbol_or_id cannot be tied to an asset, an error will be returned
       * 2. @p limit can be omitted or be @a null, if so the configured value of
       *       @a api_limit_get_liquidity_pools will be used
       * 3. @p start_id can be omitted or be @a null, if so the api will return the "first page" of pools
       * 4. One or more optional parameters can be omitted from the end of the parameter list, and the optional
       *    parameters in the middle cannot be omitted (but can be @a null).
       */
      vector<extended_liquidity_pool_object> get_liquidity_pools_by_asset_a(
            const std::string& asset_symbol_or_id,
            const optional<uint32_t>& limit = optional<uint32_t>(),
            const optional<liquidity_pool_id_type>& start_id = optional<liquidity_pool_id_type>(),
            const optional<bool>& with_statistics = false )const;

      /**
       * @brief Get a list of liquidity pools by the symbol or ID of the second asset in the pool
       * @param asset_symbol_or_id symbol name or ID of the asset
       * @param limit The limitation of items each query can fetch, not greater than the configured value of
       *              @a api_limit_get_liquidity_pools
       * @param start_id Start liquidity pool id, fetch pools whose IDs are greater than or equal to this ID
       * @param with_statistics Whether to return statistics
       * @return The liquidity pools
       *
       * @note
       * 1. If @p asset_symbol_or_id cannot be tied to an asset, an error will be returned
       * 2. @p limit can be omitted or be @a null, if so the configured value of
       *       @a api_limit_get_liquidity_pools will be used
       * 3. @p start_id can be omitted or be @a null, if so the api will return the "first page" of pools
       * 4. One or more optional parameters can be omitted from the end of the parameter list, and the optional
       *    parameters in the middle cannot be omitted (but can be @a null).
       */
      vector<extended_liquidity_pool_object> get_liquidity_pools_by_asset_b(
            const std::string& asset_symbol_or_id,
            const optional<uint32_t>& limit = optional<uint32_t>(),
            const optional<liquidity_pool_id_type>& start_id = optional<liquidity_pool_id_type>(),
            const optional<bool>& with_statistics = false )const;

      /**
       * @brief Get a list of liquidity pools by the symbol or ID of one asset in the pool
       * @param asset_symbol_or_id symbol name or ID of the asset
       * @param limit The limitation of items each query can fetch, not greater than the configured value of
       *              @a api_limit_get_liquidity_pools
       * @param start_id Start liquidity pool id, fetch pools whose IDs are greater than or equal to this ID
       * @param with_statistics Whether to return statistics
       * @return The liquidity pools
       *
       * @note
       * 1. If @p asset_symbol_or_id cannot be tied to an asset, an error will be returned
       * 2. @p limit can be omitted or be @a null, if so the configured value of
       *       @a api_limit_get_liquidity_pools will be used
       * 3. @p start_id can be omitted or be @a null, if so the api will return the "first page" of pools
       * 4. One or more optional parameters can be omitted from the end of the parameter list, and the optional
       *    parameters in the middle cannot be omitted (but can be @a null).
       */
      vector<extended_liquidity_pool_object> get_liquidity_pools_by_one_asset(
            const std::string& asset_symbol_or_id,
            const optional<uint32_t>& limit = optional<uint32_t>(),
            const optional<liquidity_pool_id_type>& start_id = optional<liquidity_pool_id_type>(),
            const optional<bool>& with_statistics = false )const;

      /**
       * @brief Get a list of liquidity pools by the symbols or IDs of the two assets in the pool
       * @param asset_symbol_or_id_a symbol name or ID of one asset
       * @param asset_symbol_or_id_b symbol name or ID of the other asset
       * @param limit The limitation of items each query can fetch, not greater than the configured value of
       *              @a api_limit_get_liquidity_pools
       * @param start_id Start liquidity pool id, fetch pools whose IDs are greater than or equal to this ID
       * @param with_statistics Whether to return statistics
       * @return The liquidity pools
       *
       * @note
       * 1. If @p asset_symbol_or_id_a or @p asset_symbol_or_id_b cannot be tied to an asset,
       *    an error will be returned
       * 2. @p limit can be omitted or be @a null, if so the configured value of
       *       @a api_limit_get_liquidity_pools will be used
       * 3. @p start_id can be omitted or be @a null, if so the api will return the "first page" of pools
       * 4. One or more optional parameters can be omitted from the end of the parameter list, and the optional
       *    parameters in the middle cannot be omitted (but can be @a null).
       */
      vector<extended_liquidity_pool_object> get_liquidity_pools_by_both_assets(
            const std::string& asset_symbol_or_id_a,
            const std::string& asset_symbol_or_id_b,
            const optional<uint32_t>& limit = optional<uint32_t>(),
            const optional<liquidity_pool_id_type>& start_id = optional<liquidity_pool_id_type>(),
            const optional<bool>& with_statistics = false )const;

      /**
       * @brief Get a list of liquidity pools by their IDs
       * @param ids IDs of the liquidity pools,
       *              the quantity should not be greater than the configured value of
       *              @a api_limit_get_liquidity_pools
       * @param subscribe @a true to subscribe to the queried objects, @a false to not subscribe,
       *                  @a null to subscribe or not subscribe according to current auto-subscription setting
       *                  (see @ref set_auto_subscription)
       * @param with_statistics Whether to return statistics
       * @return The liquidity pools
       *
       * @note if an ID in the list can not be found,
       *       the corresponding data in the returned list is null.
       */
      vector<optional<extended_liquidity_pool_object>> get_liquidity_pools(
            const vector<liquidity_pool_id_type>& ids,
            const optional<bool>& subscribe = optional<bool>(),
            const optional<bool>& with_statistics = false )const;

      /**
       * @brief Get a list of liquidity pools by their share asset symbols or IDs
       * @param asset_symbols_or_ids symbol names or IDs of the share assets,
       *              the quantity should not be greater than the configured value of
       *              @a api_limit_get_liquidity_pools
       * @param subscribe @a true to subscribe to the queried objects, @a false to not subscribe,
       *                  @a null to subscribe or not subscribe according to current auto-subscription setting
       *                  (see @ref set_auto_subscription)
       * @param with_statistics Whether to return statistics
       * @return The liquidity pools that the assets are for
       *
       * @note if an asset in the list can not be found or is not a share asset of any liquidity pool,
       *       the corresponding data in the returned list is null.
       */
      vector<optional<extended_liquidity_pool_object>> get_liquidity_pools_by_share_asset(
            const vector<std::string>& asset_symbols_or_ids,
            const optional<bool>& subscribe = optional<bool>(),
            const optional<bool>& with_statistics = false )const;

      /**
       * @brief Get a list of liquidity pools by the name or ID of the owner account
       * @param account_name_or_id name or ID of the owner account
       * @param limit The limitation of items each query can fetch, not greater than the configured value of
       *              @a api_limit_get_liquidity_pools
       * @param start_id Start share asset id, fetch pools whose share asset IDs are greater than or equal to this ID
       * @param with_statistics Whether to return statistics
       * @return The liquidity pools
       *
       * @note
       * 1. If @p account_name_or_id cannot be tied to an account, an error will be returned
       * 2. @p limit can be omitted or be @a null, if so the configured value of
       *       @a api_limit_get_liquidity_pools will be used
       * 3. @p start_id can be omitted or be @a null, if so the api will return the "first page" of pools
       * 4. One or more optional parameters can be omitted from the end of the parameter list, and the optional
       *    parameters in the middle cannot be omitted (but can be @a null).
       */
      vector<extended_liquidity_pool_object> get_liquidity_pools_by_owner(
            const std::string& account_name_or_id,
            const optional<uint32_t>& limit = optional<uint32_t>(),
            const optional<asset_id_type>& start_id = optional<asset_id_type>(),
            const optional<bool>& with_statistics = false )const;


      /////////////////////
      /// SameT Funds
      /// @{

      /**
       * @brief Get a list of SameT Funds
       * @param limit The limitation of items each query can fetch, not greater than the configured value of
       *              @a api_limit_get_samet_funds
       * @param start_id Start SameT Fund id, fetch items whose IDs are greater than or equal to this ID
       * @return The SameT Funds
       *
       * @note
       * 1. @p limit can be omitted or be @a null, if so the configured value of
       *       @a api_limit_get_samet_funds will be used
       * 2. @p start_id can be omitted or be @a null, if so the api will return the "first page" of data
       * 3. One or more optional parameters can be omitted from the end of the parameter list, and the optional
       *    parameters in the middle cannot be omitted (but can be @a null).
       */
      vector<samet_fund_object> list_samet_funds(
            const optional<uint32_t>& limit = optional<uint32_t>(),
            const optional<samet_fund_id_type>& start_id = optional<samet_fund_id_type>() )const;

      /**
       * @brief Get a list of SameT Funds by the name or ID of the owner account
       * @param account_name_or_id name or ID of the owner account
       * @param limit The limitation of items each query can fetch, not greater than the configured value of
       *              @a api_limit_get_samet_funds
       * @param start_id Start SameT Fund id, fetch items whose IDs are greater than or equal to this ID
       * @return The SameT Funds
       *
       * @note
       * 1. If @p account_name_or_id cannot be tied to an account, an error will be returned
       * 2. @p limit can be omitted or be @a null, if so the configured value of
       *       @a api_limit_get_samet_funds will be used
       * 3. @p start_id can be omitted or be @a null, if so the api will return the "first page" of data
       * 4. One or more optional parameters can be omitted from the end of the parameter list, and the optional
       *    parameters in the middle cannot be omitted (but can be @a null).
       */
      vector<samet_fund_object> get_samet_funds_by_owner(
            const std::string& account_name_or_id,
            const optional<uint32_t>& limit = optional<uint32_t>(),
            const optional<samet_fund_id_type>& start_id = optional<samet_fund_id_type>() )const;

      /**
       * @brief Get a list of SameT Funds by the symbol or ID of the asset type
       * @param asset_symbol_or_id symbol or ID of the asset type
       * @param limit The limitation of items each query can fetch, not greater than the configured value of
       *              @a api_limit_get_samet_funds
       * @param start_id Start SameT Fund id, fetch items whose IDs are greater than or equal to this ID
       * @return The SameT Funds
       *
       * @note
       * 1. If @p asset_symbol_or_id cannot be tied to an asset, an error will be returned
       * 2. @p limit can be omitted or be @a null, if so the configured value of
       *       @a api_limit_get_samet_funds will be used
       * 3. @p start_id can be omitted or be @a null, if so the api will return the "first page" of data
       * 4. One or more optional parameters can be omitted from the end of the parameter list, and the optional
       *    parameters in the middle cannot be omitted (but can be @a null).
       */
      vector<samet_fund_object> get_samet_funds_by_asset(
            const std::string& asset_symbol_or_id,
            const optional<uint32_t>& limit = optional<uint32_t>(),
            const optional<samet_fund_id_type>& start_id = optional<samet_fund_id_type>() )const;
      /// @}


      ////////////////////////////////////
      /// Credit offers and credit deals
      /// @{

      /**
       * @brief Get a list of credit offers
       * @param limit The limitation of items each query can fetch, not greater than the configured value of
       *              @a api_limit_get_credit_offers
       * @param start_id Start credit offer id, fetch items whose IDs are greater than or equal to this ID
       * @return The credit offers
       *
       * @note
       * 1. @p limit can be omitted or be @a null, if so the configured value of
       *       @a api_limit_get_credit_offers will be used
       * 2. @p start_id can be omitted or be @a null, if so the api will return the "first page" of data
       * 3. One or more optional parameters can be omitted from the end of the parameter list, and the optional
       *    parameters in the middle cannot be omitted (but can be @a null).
       */
      vector<credit_offer_object> list_credit_offers(
            const optional<uint32_t>& limit = optional<uint32_t>(),
            const optional<credit_offer_id_type>& start_id = optional<credit_offer_id_type>() )const;

      /**
       * @brief Get a list of credit offers by the name or ID of the owner account
       * @param account_name_or_id name or ID of the owner account
       * @param limit The limitation of items each query can fetch, not greater than the configured value of
       *              @a api_limit_get_credit_offers
       * @param start_id Start credit offer id, fetch items whose IDs are greater than or equal to this ID
       * @return The credit offers
       *
       * @note
       * 1. If @p account_name_or_id cannot be tied to an account, an error will be returned
       * 2. @p limit can be omitted or be @a null, if so the configured value of
       *       @a api_limit_get_credit_offers will be used
       * 3. @p start_id can be omitted or be @a null, if so the api will return the "first page" of data
       * 4. One or more optional parameters can be omitted from the end of the parameter list, and the optional
       *    parameters in the middle cannot be omitted (but can be @a null).
       */
      vector<credit_offer_object> get_credit_offers_by_owner(
            const std::string& account_name_or_id,
            const optional<uint32_t>& limit = optional<uint32_t>(),
            const optional<credit_offer_id_type>& start_id = optional<credit_offer_id_type>() )const;

      /**
       * @brief Get a list of credit offers by the symbol or ID of the asset type
       * @param asset_symbol_or_id symbol or ID of the asset type
       * @param limit The limitation of items each query can fetch, not greater than the configured value of
       *              @a api_limit_get_credit_offers
       * @param start_id Start credit offer id, fetch items whose IDs are greater than or equal to this ID
       * @return The credit offers
       *
       * @note
       * 1. If @p asset_symbol_or_id cannot be tied to an asset, an error will be returned
       * 2. @p limit can be omitted or be @a null, if so the configured value of
       *       @a api_limit_get_credit_offers will be used
       * 3. @p start_id can be omitted or be @a null, if so the api will return the "first page" of data
       * 4. One or more optional parameters can be omitted from the end of the parameter list, and the optional
       *    parameters in the middle cannot be omitted (but can be @a null).
       */
      vector<credit_offer_object> get_credit_offers_by_asset(
            const std::string& asset_symbol_or_id,
            const optional<uint32_t>& limit = optional<uint32_t>(),
            const optional<credit_offer_id_type>& start_id = optional<credit_offer_id_type>() )const;

      /**
       * @brief Get a list of credit deals
       * @param limit The limitation of items each query can fetch, not greater than the configured value of
       *              @a api_limit_get_credit_offers
       * @param start_id Start credit deal id, fetch items whose IDs are greater than or equal to this ID
       * @return The credit deals
       *
       * @note
       * 1. @p limit can be omitted or be @a null, if so the configured value of
       *       @a api_limit_get_credit_offers will be used
       * 2. @p start_id can be omitted or be @a null, if so the api will return the "first page" of data
       * 3. One or more optional parameters can be omitted from the end of the parameter list, and the optional
       *    parameters in the middle cannot be omitted (but can be @a null).
       */
      vector<credit_deal_object> list_credit_deals(
            const optional<uint32_t>& limit = optional<uint32_t>(),
            const optional<credit_deal_id_type>& start_id = optional<credit_deal_id_type>() )const;

      /**
       * @brief Get a list of credit deals by the ID of a credit offer
       * @param offer_id ID of the credit offer
       * @param limit The limitation of items each query can fetch, not greater than the configured value of
       *              @a api_limit_get_credit_offers
       * @param start_id Start credit deal id, fetch items whose IDs are greater than or equal to this ID
       * @return The credit deals
       *
       * @note
       * 1. If @p offer_id cannot be tied to a credit offer, an empty list will be returned
       * 2. @p limit can be omitted or be @a null, if so the configured value of
       *       @a api_limit_get_credit_offers will be used
       * 3. @p start_id can be omitted or be @a null, if so the api will return the "first page" of data
       * 4. One or more optional parameters can be omitted from the end of the parameter list, and the optional
       *    parameters in the middle cannot be omitted (but can be @a null).
       */
      vector<credit_deal_object> get_credit_deals_by_offer_id(
            const credit_offer_id_type& offer_id,
            const optional<uint32_t>& limit = optional<uint32_t>(),
            const optional<credit_deal_id_type>& start_id = optional<credit_deal_id_type>() )const;

      /**
       * @brief Get a list of credit deals by the name or ID of a credit offer owner account
       * @param account_name_or_id name or ID of the credit offer owner account
       * @param limit The limitation of items each query can fetch, not greater than the configured value of
       *              @a api_limit_get_credit_offers
       * @param start_id Start credit deal id, fetch items whose IDs are greater than or equal to this ID
       * @return The credit deals
       *
       * @note
       * 1. If @p account_name_or_id cannot be tied to an account, an error will be returned
       * 2. @p limit can be omitted or be @a null, if so the configured value of
       *       @a api_limit_get_credit_offers will be used
       * 3. @p start_id can be omitted or be @a null, if so the api will return the "first page" of data
       * 4. One or more optional parameters can be omitted from the end of the parameter list, and the optional
       *    parameters in the middle cannot be omitted (but can be @a null).
       */
      vector<credit_deal_object> get_credit_deals_by_offer_owner(
            const std::string& account_name_or_id,
            const optional<uint32_t>& limit = optional<uint32_t>(),
            const optional<credit_deal_id_type>& start_id = optional<credit_deal_id_type>() )const;

      /**
       * @brief Get a list of credit deals by the name or ID of a borrower account
       * @param account_name_or_id name or ID of the borrower account
       * @param limit The limitation of items each query can fetch, not greater than the configured value of
       *              @a api_limit_get_credit_offers
       * @param start_id Start credit deal id, fetch items whose IDs are greater than or equal to this ID
       * @return The credit deals
       *
       * @note
       * 1. If @p account_name_or_id cannot be tied to an account, an error will be returned
       * 2. @p limit can be omitted or be @a null, if so the configured value of
       *       @a api_limit_get_credit_offers will be used
       * 3. @p start_id can be omitted or be @a null, if so the api will return the "first page" of data
       * 4. One or more optional parameters can be omitted from the end of the parameter list, and the optional
       *    parameters in the middle cannot be omitted (but can be @a null).
       */
      vector<credit_deal_object> get_credit_deals_by_borrower(
            const std::string& account_name_or_id,
            const optional<uint32_t>& limit = optional<uint32_t>(),
            const optional<credit_deal_id_type>& start_id = optional<credit_deal_id_type>() )const;

      /**
       * @brief Get a list of credit deals by the symbol or ID of the debt asset type
       * @param asset_symbol_or_id symbol or ID of the debt asset type
       * @param limit The limitation of items each query can fetch, not greater than the configured value of
       *              @a api_limit_get_credit_offers
       * @param start_id Start credit deal id, fetch items whose IDs are greater than or equal to this ID
       * @return The credit deals
       *
       * @note
       * 1. If @p asset_symbol_or_id cannot be tied to an asset, an error will be returned
       * 2. @p limit can be omitted or be @a null, if so the configured value of
       *       @a api_limit_get_credit_offers will be used
       * 3. @p start_id can be omitted or be @a null, if so the api will return the "first page" of data
       * 4. One or more optional parameters can be omitted from the end of the parameter list, and the optional
       *    parameters in the middle cannot be omitted (but can be @a null).
       */
      vector<credit_deal_object> get_credit_deals_by_debt_asset(
            const std::string& asset_symbol_or_id,
            const optional<uint32_t>& limit = optional<uint32_t>(),
            const optional<credit_deal_id_type>& start_id = optional<credit_deal_id_type>() )const;

      /**
       * @brief Get a list of credit deals by the symbol or ID of the collateral asset type
       * @param asset_symbol_or_id symbol or ID of the collateral asset type
       * @param limit The limitation of items each query can fetch, not greater than the configured value of
       *              @a api_limit_get_credit_offers
       * @param start_id Start credit deal id, fetch items whose IDs are greater than or equal to this ID
       * @return The credit deals
       *
       * @note
       * 1. If @p asset_symbol_or_id cannot be tied to an asset, an error will be returned
       * 2. @p limit can be omitted or be @a null, if so the configured value of
       *       @a api_limit_get_credit_offers will be used
       * 3. @p start_id can be omitted or be @a null, if so the api will return the "first page" of data
       * 4. One or more optional parameters can be omitted from the end of the parameter list, and the optional
       *    parameters in the middle cannot be omitted (but can be @a null).
       */
      vector<credit_deal_object> get_credit_deals_by_collateral_asset(
            const std::string& asset_symbol_or_id,
            const optional<uint32_t>& limit = optional<uint32_t>(),
            const optional<credit_deal_id_type>& start_id = optional<credit_deal_id_type>() )const;
      /// @}


      ///////////////
      // Witnesses //
      ///////////////

      /**
       * @brief Get a list of witnesses by ID
       * @param witness_ids IDs of the witnesses to retrieve
       * @return The witnesses corresponding to the provided IDs
       *
       * This function has semantics identical to @ref get_objects, but doesn't subscribe
       */
      vector<optional<witness_object>> get_witnesses(const vector<witness_id_type>& witness_ids)const;

      /**
       * @brief Get the witness owned by a given account
       * @param account_name_or_id The name or ID of the account whose witness should be retrieved
       * @return The witness object, or null if the account does not have a witness
       */
      fc::optional<witness_object> get_witness_by_account(const std::string& account_name_or_id)const;

      /**
       * @brief Get names and IDs for registered witnesses
       * @param lower_bound_name Lower bound of the first name to return
       * @param limit Maximum number of results to return, must not exceed the configured value of
       *              @a api_limit_lookup_witness_accounts
       * @return Map of witness names to corresponding IDs
       */
      map<string, witness_id_type, std::less<>> lookup_witness_accounts( const string& lower_bound_name,
                                                                         uint32_t limit )const;

      /**
       * @brief Get the total number of witnesses registered with the blockchain
       */
      uint64_t get_witness_count()const;

      ///////////////////////
      // Committee members //
      ///////////////////////

      /**
       * @brief Get a list of committee_members by ID
       * @param committee_member_ids IDs of the committee_members to retrieve
       * @return The committee_members corresponding to the provided IDs
       *
       * This function has semantics identical to @ref get_objects, but doesn't subscribe
       */
      vector<optional<committee_member_object>> get_committee_members(
            const vector<committee_member_id_type>& committee_member_ids)const;

      /**
       * @brief Get the committee_member owned by a given account
       * @param account_name_or_id The name or ID of the account whose committee_member should be retrieved
       * @return The committee_member object, or null if the account does not have a committee_member
       */
      fc::optional<committee_member_object> get_committee_member_by_account( const string& account_name_or_id )const;

      /**
       * @brief Get names and IDs for registered committee_members
       * @param lower_bound_name Lower bound of the first name to return
       * @param limit Maximum number of results to return, must not exceed the configured value of
       *              @a api_limit_lookup_committee_member_accounts
       * @return Map of committee_member names to corresponding IDs
       */
      map<string, committee_member_id_type, std::less<>> lookup_committee_member_accounts(
            const string& lower_bound_name,
            uint32_t limit )const;

      /**
       * @brief Get the total number of committee registered with the blockchain
      */
      uint64_t get_committee_count()const;


      ///////////////////////
      // Worker proposals  //
      ///////////////////////

      /**
       * @brief Get workers
       * @param is_expired null for all workers, true for expired workers only, false for non-expired workers only
       * @return A list of worker objects
       *
      */
      vector<worker_object> get_all_workers( const optional<bool>& is_expired = optional<bool>() )const;

      /**
       * @brief Get the workers owned by a given account
       * @param account_name_or_id The name or ID of the account whose worker should be retrieved
       * @return A list of worker objects owned by the account
       */
      vector<worker_object> get_workers_by_account(const std::string& account_name_or_id)const;

      /**
       * @brief Get the total number of workers registered with the blockchain
      */
      uint64_t get_worker_count()const;



      ///////////
      // Votes //
      ///////////

      /**
       * @brief Given a set of votes, return the objects they are voting for
       * @param votes a list of vote IDs,
       *              the quantity should not be greater than the configured value of
       *              @a api_limit_lookup_vote_ids
       * @return the referenced objects
       *
       * This will be a mixture of committee_member_objects, witness_objects, and worker_objects
       *
       * The results will be in the same order as the votes.  Null will be returned for
       * any vote IDs that are not found.
       */
      vector<variant> lookup_vote_ids( const vector<vote_id_type>& votes )const;

      ////////////////////////////
      // Authority / validation //
      ////////////////////////////

      /**
       * @brief Get a hexdump of the serialized binary form of a transaction
       * @param trx a transaction to get hexdump from
       * @return the hexdump of the transaction
       */
      std::string get_transaction_hex(const signed_transaction& trx)const;

      /**
       * @brief Get a hexdump of the serialized binary form of a signatures-stripped transaction
       * @param trx a transaction to get hexdump from
       * @return the hexdump of the transaction without the signatures
       */
      std::string get_transaction_hex_without_sig( const transaction &trx ) const;

      /**
       *  This API will take a partially signed transaction and a set of public keys that the owner
       *  has the ability to sign for and return the minimal subset of public keys that should add
       *  signatures to the transaction.
       *
       *  @param trx the transaction to be signed
       *  @param available_keys a set of public keys
       *  @return a subset of @p available_keys that could sign for the given transaction
       */
      set<public_key_type> get_required_signatures( const signed_transaction& trx,
                                                    const flat_set<public_key_type>& available_keys )const;

      /**
       *  This method will return the set of all public keys that could possibly sign for a given transaction.
       *  This call can be used by wallets to filter their set of public keys to just the relevant subset prior
       *  to calling @ref get_required_signatures to get the minimum subset.
       *
       *  @param trx the transaction to be signed
       *  @return a set of public keys that could possibly sign for the given transaction
       */
      set<public_key_type> get_potential_signatures( const signed_transaction& trx )const;

      /**
       *  This method will return the set of all addresses that could possibly sign for a given transaction.
       *
       *  @param trx the transaction to be signed
       *  @return a set of addresses that could possibly sign for the given transaction
       */
      set<address> get_potential_address_signatures( const signed_transaction& trx )const;

      /**
       * Check whether a transaction has all of the required signatures
       * @param trx a transaction to be verified
       * @return true if the @p trx has all of the required signatures, otherwise throws an exception
       */
      bool           verify_authority( const signed_transaction& trx )const;

      /**
       * @brief Verify that the public keys have enough authority to approve an operation for this account
       * @param account_name_or_id name or ID of an account to check
       * @param signers the public keys
       * @return true if the passed in keys have enough authority to approve an operation for this account
       */
      bool verify_account_authority( const string& account_name_or_id,
                                     const flat_set<public_key_type>& signers )const;

      /**
       * @brief Validates a transaction against the current state without broadcasting it on the network
       * @param trx a transaction to be validated
       * @return a processed_transaction object if the transaction passes the validation,
                 otherwise an exception will be thrown
       */
      processed_transaction validate_transaction( const signed_transaction& trx )const;

      /**
       * @brief For each operation calculate the required fee in the specified asset type
       * @param ops a list of operations to be query for required fees
       * @param asset_symbol_or_id symbol name or ID of an asset that to be used to pay the fees
       * @return a list of objects which indicates required fees of each operation
       */
      vector< fc::variant > get_required_fees( const vector<operation>& ops,
                                               const std::string& asset_symbol_or_id )const;

      ///////////////////////////
      // Proposed transactions //
      ///////////////////////////

      /**
       * @brief return a set of proposed transactions (aka proposals) that the specified account
       *        can add approval to or remove approval from
       * @param account_name_or_id The name or ID of an account
       * @return a set of proposed transactions that the specified account can act on
       */
      vector<proposal_object> get_proposed_transactions( const std::string account_name_or_id )const;

      //////////////////////
      // Blinded balances //
      //////////////////////

      /**
       * @brief return the set of blinded balance objects by commitment ID
       * @param commitments a set of commitments to query for
       * @return the set of blinded balance objects by commitment ID
       */
      vector<blinded_balance_object> get_blinded_balances( const flat_set<commitment_type>& commitments )const;

      /////////////////
      // Withdrawals //
      /////////////////

      /**
       *  @brief Get non expired withdraw permission objects for a giver(ex:recurring customer)
       *  @param account_name_or_id Account name or ID to get objects from
       *  @param start Withdraw permission objects(1.12.X) before this ID will be skipped in results.
       *               Pagination purposes.
       *  @param limit Maximum number of objects to retrieve, must not exceed the configured value of
       *               @a api_limit_get_withdraw_permissions_by_giver
       *  @return Withdraw permission objects for the account
       */
      vector<withdraw_permission_object> get_withdraw_permissions_by_giver( const std::string account_name_or_id,
                                                                            withdraw_permission_id_type start,
                                                                            uint32_t limit )const;

      /**
       *  @brief Get non expired withdraw permission objects for a recipient(ex:service provider)
       *  @param account_name_or_id Account name or ID to get objects from
       *  @param start Withdraw permission objects(1.12.X) before this ID will be skipped in results.
       *               Pagination purposes.
       *  @param limit Maximum number of objects to retrieve, must not exceed the configured value of
       *               @a api_limit_get_withdraw_permissions_by_recipient
       *  @return Withdraw permission objects for the account
       */
      vector<withdraw_permission_object> get_withdraw_permissions_by_recipient( const std::string account_name_or_id,
                                                                                withdraw_permission_id_type start,
                                                                                uint32_t limit )const;

      //////////
      // HTLC //
      //////////

      /**
       *  @brief Get HTLC object
       *  @param id HTLC contract id
       *  @param subscribe @a true to subscribe to the queried HTLC objects, @a false to not subscribe,
       *                   @a null to subscribe or not subscribe according to current auto-subscription setting
       *                   (see @ref set_auto_subscription)
       *  @return HTLC object for the id
       */
      optional<htlc_object> get_htlc( htlc_id_type id, optional<bool> subscribe = optional<bool>() ) const;

      /**
       *  @brief Get non expired HTLC objects using the sender account
       *  @param account_name_or_id Account name or ID to get objects from
       *  @param start htlc objects before this ID will be skipped in results. Pagination purposes.
       *  @param limit Maximum number of objects to retrieve, must not exceed the configured value of
       *               @a api_limit_get_htlc_by
       *  @return HTLC objects for the account
       */
      vector<htlc_object> get_htlc_by_from( const std::string account_name_or_id,
                                            htlc_id_type start,
                                            uint32_t limit ) const;

      /**
       *  @brief Get non expired HTLC objects using the receiver account
       *  @param account_name_or_id Account name or ID to get objects from
       *  @param start htlc objects before this ID will be skipped in results. Pagination purposes.
       *  @param limit Maximum number of objects to retrieve, must not exceed the configured value of
       *               @a api_limit_get_htlc_by
       *  @return HTLC objects for the account
      */
      vector<htlc_object> get_htlc_by_to( const std::string account_name_or_id,
                                          htlc_id_type start,
                                          uint32_t limit ) const;

      /**
       * @brief Get all HTLCs
       * @param start Lower bound of htlc id to start getting results
       * @param limit Maximum number of htlc objects to fetch, must not exceed the configured value of
       *              @a api_limit_list_htlcs
       * @return The htlc object list
      */
      vector<htlc_object> list_htlcs(const htlc_id_type start, uint32_t limit) const;


      /////////////
      // Tickets //
      /////////////

      /**
       * @brief Get a list of tickets
       * @param limit The limitation of items each query can fetch, not greater than the configured value of
       *              @a api_limit_get_tickets
       * @param start_id Start ticket id, fetch tickets whose IDs are greater than or equal to this ID
       * @return The tickets
       *
       * @note
       * 1. @p limit can be omitted or be @a null, if so the configured value of
       *       @a api_limit_get_tickets will be used
       * 2. @p start_id can be omitted or be @a null, if so the api will return the "first page" of tickets
       * 3. One or more optional parameters can be omitted from the end of the parameter list, and the optional
       *    parameters in the middle cannot be omitted (but can be @a null).
       */
      vector<ticket_object> list_tickets(
            const optional<uint32_t>& limit = optional<uint32_t>(),
            const optional<ticket_id_type>& start_id = optional<ticket_id_type>() )const;

      /**
       * @brief Get a list of tickets by the name or ID of the owner account
       * @param account_name_or_id name or ID of the owner account
       * @param limit The limitation of items each query can fetch, not greater than the configured value of
       *              @a api_limit_get_tickets
       * @param start_id Start ticket id, fetch tickets whose IDs are greater than or equal to this ID
       * @return The tickets
       *
       * @note
       * 1. If @p account_name_or_id cannot be tied to an account, an error will be returned
       * 2. @p limit can be omitted or be @a null, if so the configured value of
       *       @a api_limit_get_tickets will be used
       * 3. @p start_id can be omitted or be @a null, if so the api will return the "first page" of tickets
       * 4. One or more optional parameters can be omitted from the end of the parameter list, and the optional
       *    parameters in the middle cannot be omitted (but can be @a null).
       */
      vector<ticket_object> get_tickets_by_account(
            const std::string& account_name_or_id,
            const optional<uint32_t>& limit = optional<uint32_t>(),
            const optional<ticket_id_type>& start_id = optional<ticket_id_type>() )const;

private:
      std::shared_ptr< database_api_impl > my;
};

} }

extern template class fc::api<graphene::app::database_api>;

FC_API(graphene::app::database_api,
   // Objects
   (get_objects)

   // Subscriptions
   (set_subscribe_callback)
   (set_auto_subscription)
   (set_pending_transaction_callback)
   (set_block_applied_callback)
   (cancel_all_subscriptions)

   // Blocks and transactions
   (get_block_header)
   (get_block_header_batch)
   (get_block)
   (get_transaction)
   (get_recent_transaction_by_id)

   // Globals
   (get_chain_properties)
   (get_global_properties)
   (get_config)
   (get_chain_id)
   (get_dynamic_global_properties)
   (get_next_object_id)

   // Keys
   (get_key_references)
   (is_public_key_registered)

   // Accounts
   (get_account_id_from_string)
   (get_accounts)
   (get_full_accounts)
   (get_top_voters)
   (get_account_by_name)
   (get_account_references)
   (lookup_account_names)
   (lookup_accounts)
   (get_account_count)

   // Balances
   (get_account_balances)
   (get_named_account_balances)
   (get_balance_objects)
   (get_vested_balances)
   (get_vesting_balances)

   // Assets
   (get_assets)
   (list_assets)
   (lookup_asset_symbols)
   (get_asset_count)
   (get_assets_by_issuer)
   (get_asset_id_from_string)

    //backed asset
   (get_properties)
   (is_property_exists)
   (get_all_properties)
   (get_properties_by_backed_asset_symbol)
   (get_property_by_id)
   
   //asset limitation
   (is_asset_limitation_exists)
   (get_asset_limitaion_by_symbol)
   (get_asset_limitation_value)

   // Markets / feeds
   (get_order_book)
   (get_limit_orders)
   (get_limit_orders_by_account)
   (get_account_limit_orders)
   (get_call_orders)
   (get_call_orders_by_account)
   (get_settle_orders)
   (get_settle_orders_by_account)
   (get_margin_positions)
   (get_collateral_bids)
   (subscribe_to_market)
   (unsubscribe_from_market)
   (get_ticker)
   (get_24_volume)
   (get_top_markets)
   (get_trade_history)
   (get_trade_history_by_sequence)

   // Liquidity pools
   (list_liquidity_pools)
   (get_liquidity_pools_by_asset_a)
   (get_liquidity_pools_by_asset_b)
   (get_liquidity_pools_by_one_asset)
   (get_liquidity_pools_by_both_assets)
   (get_liquidity_pools)
   (get_liquidity_pools_by_share_asset)
   (get_liquidity_pools_by_owner)

   // SameT Funds
   (list_samet_funds)
   (get_samet_funds_by_owner)
   (get_samet_funds_by_asset)

   // Credit offers and credit deals
   (list_credit_offers)
   (get_credit_offers_by_owner)
   (get_credit_offers_by_asset)
   (list_credit_deals)
   (get_credit_deals_by_offer_id)
   (get_credit_deals_by_offer_owner)
   (get_credit_deals_by_borrower)
   (get_credit_deals_by_debt_asset)
   (get_credit_deals_by_collateral_asset)

   // Witnesses
   (get_witnesses)
   (get_witness_by_account)
   (lookup_witness_accounts)
   (get_witness_count)

   // Committee members
   (get_committee_members)
   (get_committee_member_by_account)
   (lookup_committee_member_accounts)
   (get_committee_count)

   // workers
   (get_all_workers)
   (get_workers_by_account)
   (get_worker_count)

   // Votes
   (lookup_vote_ids)

   // Authority / validation
   (get_transaction_hex)
   (get_transaction_hex_without_sig)
   (get_required_signatures)
   (get_potential_signatures)
   (get_potential_address_signatures)
   (verify_authority)
   (verify_account_authority)
   (validate_transaction)
   (get_required_fees)

   // Proposed transactions
   (get_proposed_transactions)

   // Blinded balances
   (get_blinded_balances)

   // Withdrawals
   (get_withdraw_permissions_by_giver)
   (get_withdraw_permissions_by_recipient)

   // HTLC
   (get_htlc)
   (get_htlc_by_from)
   (get_htlc_by_to)
   (list_htlcs)

   // Tickets
   (list_tickets)
   (get_tickets_by_account)
)
