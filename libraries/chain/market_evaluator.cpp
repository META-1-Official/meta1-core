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
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/asset_limitation_object.hpp>

#include <graphene/chain/market_evaluator.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>

#include <graphene/protocol/market.hpp>

#include <fc/uint128.hpp>
#include <iostream>
#include <string> 
#include <curl/curl.h>
#include <fc/io/json.hpp>

namespace graphene { namespace chain {

std::size_t callback(const char *in, std::size_t size, std::size_t num, std::string *out)
{
   const std::size_t totalBytes = size * num;
   out->append(in, totalBytes);
   return totalBytes;
}

         void_result limit_order_create_evaluator::do_evaluate(const limit_order_create_operation &op)
         {
            try
            {
               const database &d = db();

               FC_ASSERT(op.expiration >= d.head_block_time());

               _seller = this->fee_paying_account;
               _sell_asset = &op.amount_to_sell.asset_id(d);
               _receive_asset = &op.min_to_receive.asset_id(d);

               if( _sell_asset->options.whitelist_markets.size() )
               {
                  GRAPHENE_ASSERT( _sell_asset->options.whitelist_markets.find(_receive_asset->id)
                                      != _sell_asset->options.whitelist_markets.end(),
                                   limit_order_create_market_not_whitelisted,
                                   "This market has not been whitelisted by the selling asset", );
               }
               if( _sell_asset->options.blacklist_markets.size() )
               {
                  GRAPHENE_ASSERT( _sell_asset->options.blacklist_markets.find(_receive_asset->id)
                                      == _sell_asset->options.blacklist_markets.end(),
                                   limit_order_create_market_blacklisted,
                                   "This market has been blacklisted by the selling asset", );
               }

               GRAPHENE_ASSERT( is_authorized_asset( d, *_seller, *_sell_asset ),
                                limit_order_create_selling_asset_unauthorized,
                                "The account is not allowed to transact the selling asset", );

               GRAPHENE_ASSERT( is_authorized_asset( d, *_seller, *_receive_asset ),
                                limit_order_create_receiving_asset_unauthorized,
                                "The account is not allowed to transact the receiving asset", );

               GRAPHENE_ASSERT( d.get_balance( *_seller, *_sell_asset ) >= op.amount_to_sell,
                                limit_order_create_insufficient_balance,
                                "insufficient balance",
                                ("balance",d.get_balance(*_seller,*_sell_asset))("amount_to_sell",op.amount_to_sell) );

               // Check if selling or buy the META1 asset
               const asset_object &META1 = d.get_core_asset();
               const asset_id_type &meta1_id = META1.id;
               const bool selling_meta1 = (op.amount_to_sell.asset_id == meta1_id);
               const bool buying_meta1 = (op.min_to_receive.asset_id == meta1_id);
               if (selling_meta1 || buying_meta1) {
                  // Check the implied price of META1
                  FC_ASSERT(selling_meta1 != buying_meta1, "The order must not sell and buy META1");

                  // The non-META1 asset must be known
                  const auto &asset_idx_by_id = d.get_index_type<asset_index>().indices().get<by_id>();
                  asset_id_type other_id;
                  if (selling_meta1) {
                     other_id = op.min_to_receive.asset_id;
                  } else {
                     other_id = op.amount_to_sell.asset_id;
                  }
                  auto asset_itr = asset_idx_by_id.find(other_id);
                  FC_ASSERT(asset_itr != asset_idx_by_id.end(), "Other asset is unknown");
                  const asset_object &O = *asset_itr;

                  // The minimum price check is only performed if an external price has been set
                  const auto &asset_price_idx_by_symbol = d.get_index_type<asset_price_index>().indices().get<by_symbol>();
                  auto asset_price_itr = asset_price_idx_by_symbol.find(O.symbol);
                  if (asset_price_itr != asset_price_idx_by_symbol.end()) {
                     const asset_price &asset_price = *asset_price_itr;
                     // Check for recent price
                     const uint32_t price_age_seconds = d.head_block_time().sec_since_epoch()
                                                        - asset_price.publication_time.sec_since_epoch();
                     // TODO: [Low] Customize the the maximum age of the asset price
                     FC_ASSERT(price_age_seconds <= GRAPHENE_DEFAULT_PRICE_FEED_LIFETIME,
                               "The most recent published price for ${symbol} is too old (${age} seconds)",
                               ("symbol", O.symbol)("age", price_age_seconds));
                     const uint32_t usd_price_num = asset_price.usd_price.numerator;
                     const uint32_t usd_price_den = asset_price.usd_price.denominator;

                     // Get the total valuation for all META1 tokens
                     const auto &asset_limitation_idx = d.get_index_type<asset_limitation_index>().indices().get<by_limit_symbol>();
                     const asset_limitation_object &alo = *asset_limitation_idx.find(META1.symbol);
                     const uint64_t cumulative = alo.cumulative_sell_limit;

                     // Check the implied price for META1
                     /**
                      * **Implied USD-Price for META1**
                      *
                      * The USD-price for a META1 token implied from a limit order,
                      * which aims to buy or sell another token called Other (O),
                      * should be greater than or equal to the derived valuation of all META1 tokens
                      * based on 10x the appraisal value of the properties backing the META1 asset type.
                      *
                      * This can be expressed as
                      *
                      *  M1_Price_Order >= M1_Price_Backing                                             (1)
                      *
                      * where
                      *
                      *   M1_Price_Order = (O / M) * USD_Price_Of_O,
                      *                O = Quantity of Other tokens,
                      *                M = Quantity of META1 tokens,
                      *   USD_Price_Of_O = USD-price of the other token
                      *                  = USD-price Numerator / USD-price Denominator
                      *                  = O_num / O_den
                      *
                      * and
                      *
                      * M1_Price_Backing = Cumulative / N
                      *       Cumulative = 10 * Cumulate USD-valuation of all backing properties
                      *                N = Quantity of META1 tokens
                      *
                      * Equation 1 can be re-expressed as
                      *
                      * (O / M) * O_num / O_den >= Cumulative / N                                       (2)
                      */

                     /**
                      * **Implied USD-Price for META1 in Graphene terms**
                      *
                      * Equation (2) needs to be re-expressed to account for the fact that 1 whole of a token
                      * is actually represented as multiples of satoshies of that token.
                      * 1 token is represented by 10^X satoshi where the X is conventionally
                      * called the asset type's precision, and is a non-negative integer.
                      *
                      * For example, the META1 token has a precision that will be called p_M1, has a value of 5,
                      * so 10^5 = 10,000.
                      *
                      * For example, the "Other" token will have a precision called p_O.
                      *
                      * Similarly, quantities of the M token can be expressed as
                      *
                      * M = m / 10^p_M1
                      *
                      * where m is the quantity of satoshi META1.
                      *
                      * Similarly, quantities of the O token can be expressed as
                      *
                      * O = o / 10^p_O
                      *
                      * where o is the quantity of satoshi Other.
                      *
                      *
                      * Therefore, Equation 2 cann be re-expressed as
                      *
                      * ((o / 10^p_O) / (m / 10^p_M1)) * O_num / O_den >= Cumulative / N            (3)
                      */

                     /**
                      * **Implied USD-Price for META1 in Graphene terms to avoid rounding errors**
                      *
                      * Rounding errors can be eliminated by removing all arithmetic divisions from Equation 3.
                      * This can be accomplished by re-arranging all items in the denominators of the equation.
                      *
                      * o * 10^p_M1 * O_num * N >= m * 10^p_O * O_den * Cumulative                  (4)
                      */

                     /**
                      * **Implied USD-Price for META1 in Graphene terms to avoid rounding errors, reduce computations,
                      * and reduce likelihood of overflow**
                      *
                      * Equation (3) can be re-expressed to reduce the likelihood of an overflow by combining
                      * the terms 10^p_M1 and 10^p_O.
                      *
                      *
                      * When p_M1 >= p_0, Equation 4 can be arranged as
                      *
                      * 10^(p_M1 - p_O) * o * O_num * N >= m * O_den * Cumulative                   (4a)
                      *
                      *
                      * When p_M1 < p_0, Equation 4 can be arranged as
                      *
                      * o * O_num * N >= 10^(p_O - p_M1) * m * O_den * Cumulative                   (4b)
                      *
                      */
                     // TODO: [Medium] Overflow check
                     const int64_t o = selling_meta1 ? op.min_to_receive.amount.value : op.amount_to_sell.amount.value;
                     const int64_t m = selling_meta1 ? op.amount_to_sell.amount.value : op.min_to_receive.amount.value;

                     const int64_t META1_SUPPLY =
                             META1.options.max_supply.value / asset::scaled_precision(META1.precision).value;
                     fc::uint128_t LHS = fc::uint128_t(o) * usd_price_num * META1_SUPPLY;
                     fc::uint128_t RHS = fc::uint128_t(m) * usd_price_den * cumulative;
                     if (META1.precision >= O.precision) {
                        const uint8_t precision_delta = META1.precision - O.precision;
                        const int64_t ten_to_precision_delta = asset::scaled_precision(precision_delta).value;
                        LHS *= ten_to_precision_delta;

                     } else {
                        const uint8_t precision_delta = O.precision - META1.precision;
                        const int64_t ten_to_precision_delta = asset::scaled_precision(precision_delta).value;
                        RHS *= ten_to_precision_delta;

                     }
                     FC_ASSERT(LHS >= RHS, "The implied valuation for the META1 token is too low: ${LHS} >= ${RHS}",
                               ("LHS", LHS)("RHS", RHS));
                  }
               }

               return void_result();
            }
            FC_CAPTURE_AND_RETHROW((op))
         }

void limit_order_create_evaluator::convert_fee()
{
   if( db().head_block_time() <= HARDFORK_CORE_604_TIME )
      generic_evaluator::convert_fee();
   else
      if( !trx_state->skip_fee )
      {
         if( fee_asset->get_id() != asset_id_type() )
         {
            db().modify(*fee_asset_dyn_data, [this](asset_dynamic_data_object& d) {
               d.fee_pool -= core_fee_paid;
            });
         }
      }
}

void limit_order_create_evaluator::pay_fee()
{
   if( db().head_block_time() <= HARDFORK_445_TIME )
      generic_evaluator::pay_fee();
   else
   {
      _deferred_fee = core_fee_paid;
      if( db().head_block_time() > HARDFORK_CORE_604_TIME && fee_asset->get_id() != asset_id_type() )
         _deferred_paid_fee = fee_from_account;
   }
}

object_id_type limit_order_create_evaluator::do_apply(const limit_order_create_operation& op)
{ try {
   if( op.amount_to_sell.asset_id == asset_id_type() )
   {
      db().modify( _seller->statistics(db()), [&op](account_statistics_object& bal) {
         bal.total_core_in_orders += op.amount_to_sell.amount;
      });
   }

   db().adjust_balance(op.seller, -op.amount_to_sell);

   const auto& new_order_object = db().create<limit_order_object>([&](limit_order_object& obj){
       obj.seller   = _seller->id;
       obj.for_sale = op.amount_to_sell.amount;
       obj.sell_price = op.get_price();
       obj.expiration = op.expiration;
       obj.deferred_fee = _deferred_fee;
       obj.deferred_paid_fee = _deferred_paid_fee;
   });
   limit_order_id_type order_id = new_order_object.id; // save this because we may remove the object by filling it
   bool filled;
   if( db().get_dynamic_global_properties().next_maintenance_time <= HARDFORK_CORE_625_TIME )
      filled = db().apply_order_before_hardfork_625( new_order_object );
   else
      filled = db().apply_order( new_order_object );

   GRAPHENE_ASSERT( !op.fill_or_kill || filled,
                    limit_order_create_kill_unfilled,
                    "Killing limit order ${op} due to unable to fill",
                    ("op",op) );

   return order_id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result limit_order_cancel_evaluator::do_evaluate(const limit_order_cancel_operation& o)
{ try {
   database& d = db();

   _order = d.find( o.order );

   GRAPHENE_ASSERT( _order != nullptr,
                    limit_order_cancel_nonexist_order,
                    "Limit order ${oid} does not exist",
                    ("oid", o.order) );

   GRAPHENE_ASSERT( _order->seller == o.fee_paying_account,
                    limit_order_cancel_owner_mismatch,
                    "Limit order ${oid} is owned by someone else",
                    ("oid", o.order) );

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

asset limit_order_cancel_evaluator::do_apply(const limit_order_cancel_operation& o)
{ try {
   database& d = db();

   auto base_asset = _order->sell_price.base.asset_id;
   auto quote_asset = _order->sell_price.quote.asset_id;
   auto refunded = _order->amount_for_sale();

   d.cancel_limit_order(*_order, false /* don't create a virtual op*/);

   if( d.get_dynamic_global_properties().next_maintenance_time <= HARDFORK_CORE_606_TIME )
   {
      // Possible optimization: order can be called by canceling a limit order iff the canceled order was at the top of the book.
      // Do I need to check calls in both assets?
      d.check_call_orders(base_asset(d));
      d.check_call_orders(quote_asset(d));
   }

   return refunded;
} FC_CAPTURE_AND_RETHROW( (o) ) }

void_result call_order_update_evaluator::do_evaluate(const call_order_update_operation& o)
{ try {
   database& d = db();

   auto next_maintenance_time = d.get_dynamic_global_properties().next_maintenance_time;

   _paying_account = &o.funding_account(d);
   _debt_asset     = &o.delta_debt.asset_id(d);
   FC_ASSERT( _debt_asset->is_market_issued(), "Unable to cover ${sym} as it is not a collateralized asset.",
              ("sym", _debt_asset->symbol) );

   _dynamic_data_obj = &_debt_asset->dynamic_asset_data_id(d);

   /***
    * We have softfork code already in production to prevent exceeding MAX_SUPPLY between 2018-12-21 until HF 1465.
    * But we must allow this in replays until 2018-12-21. The HF 1465 code will correct the problem.
    * After HF 1465, we MAY be able to remove the cleanup code IF it never executes. We MAY be able to clean
    * up the softfork code IF it never executes. We MAY be able to turn the hardfork code into regular code IF
    * noone ever attempted this before HF 1465.
    */
   if (next_maintenance_time <= SOFTFORK_CORE_1465_TIME)
   {
      if ( _dynamic_data_obj->current_supply + o.delta_debt.amount > _debt_asset->options.max_supply )
         ilog("Issue 1465... Borrowing and exceeding MAX_SUPPLY. Will be corrected at hardfork time.");
   }
   else
   {
      FC_ASSERT( _dynamic_data_obj->current_supply + o.delta_debt.amount <= _debt_asset->options.max_supply,
            "Borrowing this quantity would exceed MAX_SUPPLY" );
   }
   
   FC_ASSERT( _dynamic_data_obj->current_supply + o.delta_debt.amount >= 0,
         "This transaction would bring current supply below zero.");

   _bitasset_data  = &_debt_asset->bitasset_data(d);

   /// if there is a settlement for this asset, then no further margin positions may be taken and
   /// all existing margin positions should have been closed va database::globally_settle_asset
   FC_ASSERT( !_bitasset_data->has_settlement(), "Cannot update debt position when the asset has been globally settled" );

   FC_ASSERT( o.delta_collateral.asset_id == _bitasset_data->options.short_backing_asset,
              "Collateral asset type should be same as backing asset of debt asset" );

   if( _bitasset_data->is_prediction_market )
      FC_ASSERT( o.delta_collateral.amount == o.delta_debt.amount,
                 "Debt amount and collateral amount should be same when updating debt position in a prediction market" );
   else if( _bitasset_data->current_feed.settlement_price.is_null() )
      FC_THROW_EXCEPTION(insufficient_feeds, "Cannot borrow asset with no price feed.");

   // Note: there was code here checking whether the account has enough balance to increase delta collateral,
   //       which is now removed since the check is implicitly done later by `adjust_balance()` in `do_apply()`.

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }


object_id_type call_order_update_evaluator::do_apply(const call_order_update_operation& o)
{ try {
   database& d = db();

   if( o.delta_debt.amount != 0 )
   {
      d.adjust_balance( o.funding_account, o.delta_debt );

      // Deduct the debt paid from the total supply of the debt asset.
      d.modify(*_dynamic_data_obj, [&](asset_dynamic_data_object& dynamic_asset) {
         dynamic_asset.current_supply += o.delta_debt.amount;
      });
   }

   if( o.delta_collateral.amount != 0 )
   {
      d.adjust_balance( o.funding_account, -o.delta_collateral  );

      // Adjust the total core in orders accodingly
      if( o.delta_collateral.asset_id == asset_id_type() )
      {
         d.modify(_paying_account->statistics(d), [&](account_statistics_object& stats) {
               stats.total_core_in_orders += o.delta_collateral.amount;
         });
      }
   }

   const auto next_maint_time = d.get_dynamic_global_properties().next_maintenance_time;
   bool before_core_hardfork_1270 = ( next_maint_time <= HARDFORK_CORE_1270_TIME ); // call price caching issue

   auto& call_idx = d.get_index_type<call_order_index>().indices().get<by_account>();
   auto itr = call_idx.find( boost::make_tuple(o.funding_account, o.delta_debt.asset_id) );
   const call_order_object* call_obj = nullptr;
   call_order_id_type call_order_id;

   optional<price> old_collateralization;
   optional<share_type> old_debt;

   if( itr == call_idx.end() ) // creating new debt position
   {
      FC_ASSERT( o.delta_collateral.amount > 0, "Delta collateral amount of new debt position should be positive" );
      FC_ASSERT( o.delta_debt.amount > 0, "Delta debt amount of new debt position should be positive" );

      call_obj = &d.create<call_order_object>( [&o,this,before_core_hardfork_1270]( call_order_object& call ){
         call.borrower = o.funding_account;
         call.collateral = o.delta_collateral.amount;
         call.debt = o.delta_debt.amount;
         if( before_core_hardfork_1270 ) // before core-1270 hard fork, calculate call_price here and cache it
            call.call_price = price::call_price( o.delta_debt, o.delta_collateral,
                                                 _bitasset_data->current_feed.maintenance_collateral_ratio );
         else // after core-1270 hard fork, set call_price to 1
            call.call_price = price( asset( 1, o.delta_collateral.asset_id ), asset( 1, o.delta_debt.asset_id ) );
         call.target_collateral_ratio = o.extensions.value.target_collateral_ratio;
      });
      call_order_id = call_obj->id;
   }
   else // updating existing debt position
   {
      call_obj = &*itr;
      auto new_collateral = call_obj->collateral + o.delta_collateral.amount;
      auto new_debt = call_obj->debt + o.delta_debt.amount;
      call_order_id = call_obj->id;

      if( new_debt == 0 )
      {
         FC_ASSERT( new_collateral == 0, "Should claim all collateral when closing debt position" );
         d.remove( *call_obj );
         return call_order_id;
      }

      FC_ASSERT( new_collateral > 0 && new_debt > 0,
                 "Both collateral and debt should be positive after updated a debt position if not to close it" );

      old_collateralization = call_obj->collateralization();
      old_debt = call_obj->debt;

      d.modify( *call_obj, [&o,new_debt,new_collateral,this,before_core_hardfork_1270]( call_order_object& call ){
         call.collateral = new_collateral;
         call.debt       = new_debt;
         if( before_core_hardfork_1270 ) // don't update call_price after core-1270 hard fork
         {
            call.call_price  =  price::call_price( call.get_debt(), call.get_collateral(),
                                                   _bitasset_data->current_feed.maintenance_collateral_ratio );
         }
         call.target_collateral_ratio = o.extensions.value.target_collateral_ratio;
      });
   }

   // then we must check for margin calls and other issues
   if( !_bitasset_data->is_prediction_market )
   {
      // check to see if the order needs to be margin called now, but don't allow black swans and require there to be
      // limit orders available that could be used to fill the order.
      // Note: due to https://github.com/bitshares/bitshares-core/issues/649, before core-343 hard fork,
      //       the first call order may be unable to be updated if the second one is undercollateralized.
      if( d.check_call_orders( *_debt_asset, false, false, _bitasset_data ) ) // don't allow black swan, not for new limit order
      {
         call_obj = d.find(call_order_id);
         // before hard fork core-583: if we filled at least one call order, we are OK if we totally filled.
         // after hard fork core-583: we want to allow increasing collateral
         //   Note: increasing collateral won't get the call order itself matched (instantly margin called)
         //   if there is at least a call order get matched but didn't cause a black swan event,
         //   current order must have got matched. in this case, it's OK if it's totally filled.
         GRAPHENE_ASSERT(
            !call_obj,
            call_order_update_unfilled_margin_call,
            "Updating call order would trigger a margin call that cannot be fully filled"
            );
      }
      else
      {
         call_obj = d.find(call_order_id);
         // we know no black swan event has occurred
         FC_ASSERT( call_obj, "no margin call was executed and yet the call object was deleted" );
         if( d.head_block_time() <= HARDFORK_CORE_583_TIME )
         {
            // We didn't fill any call orders.  This may be because we
            // aren't in margin call territory, or it may be because there
            // were no matching orders.  In the latter case, we throw.
            GRAPHENE_ASSERT(
               // we know core-583 hard fork is before core-1270 hard fork, it's ok to use call_price here
               ~call_obj->call_price < _bitasset_data->current_feed.settlement_price,
               call_order_update_unfilled_margin_call,
               "Updating call order would trigger a margin call that cannot be fully filled",
               // we know core-583 hard fork is before core-1270 hard fork, it's ok to use call_price here
               ("a", ~call_obj->call_price )("b", _bitasset_data->current_feed.settlement_price)
               );
         }
         else // after hard fork, always allow call order to be updated if collateral ratio is increased and debt is not increased
         {
            // We didn't fill any call orders.  This may be because we
            // aren't in margin call territory, or it may be because there
            // were no matching orders. In the latter case,
            // if collateral ratio is not increased or debt is increased, we throw.
            // be here, we know no margin call was executed,
            // so call_obj's collateral ratio should be set only by op
            FC_ASSERT( ( !before_core_hardfork_1270
                            && call_obj->collateralization() > _bitasset_data->current_maintenance_collateralization )
                       || ( before_core_hardfork_1270 && ~call_obj->call_price < _bitasset_data->current_feed.settlement_price )
                       || ( old_collateralization.valid() && call_obj->debt <= *old_debt
                                                          && call_obj->collateralization() > *old_collateralization ),
               "Can only increase collateral ratio without increasing debt if would trigger a margin call that "
               "cannot be fully filled",
               ("old_debt", old_debt)
               ("new_debt", call_obj->debt)
               ("old_collateralization", old_collateralization)
               ("new_collateralization", call_obj->collateralization() )
               );
         }
      }
   }

   return call_order_id;
} FC_CAPTURE_AND_RETHROW( (o) ) }

void_result bid_collateral_evaluator::do_evaluate(const bid_collateral_operation& o)
{ try {
   database& d = db();

   FC_ASSERT( d.head_block_time() > HARDFORK_CORE_216_TIME, "Not yet!" );

   _paying_account = &o.bidder(d);
   _debt_asset     = &o.debt_covered.asset_id(d);
   FC_ASSERT( _debt_asset->is_market_issued(), "Unable to cover ${sym} as it is not a collateralized asset.",
              ("sym", _debt_asset->symbol) );

   _bitasset_data  = &_debt_asset->bitasset_data(d);

   FC_ASSERT( _bitasset_data->has_settlement() );

   FC_ASSERT( o.additional_collateral.asset_id == _bitasset_data->options.short_backing_asset );

   FC_ASSERT( !_bitasset_data->is_prediction_market, "Cannot bid on a prediction market!" );

   if( o.additional_collateral.amount > 0 )
   {
      FC_ASSERT( d.get_balance(*_paying_account, _bitasset_data->options.short_backing_asset(d)) >= o.additional_collateral,
                 "Cannot bid ${c} collateral when payer only has ${b}", ("c", o.additional_collateral.amount)
                 ("b", d.get_balance(*_paying_account, o.additional_collateral.asset_id(d)).amount) );
   }

   const collateral_bid_index& bids = d.get_index_type<collateral_bid_index>();
   const auto& index = bids.indices().get<by_account>();
   const auto& bid = index.find( boost::make_tuple( o.debt_covered.asset_id, o.bidder ) );
   if( bid != index.end() )
      _bid = &(*bid);
   else
       FC_ASSERT( o.debt_covered.amount > 0, "Can't find bid to cancel?!");

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }


void_result bid_collateral_evaluator::do_apply(const bid_collateral_operation& o)
{ try {
   database& d = db();

   if( _bid )
      d.cancel_bid( *_bid, false );

   if( o.debt_covered.amount == 0 ) return void_result();

   d.adjust_balance( o.bidder, -o.additional_collateral  );

   _bid = &d.create<collateral_bid_object>([&]( collateral_bid_object& bid ) {
      bid.bidder = o.bidder;
      bid.inv_swan_price = o.additional_collateral / o.debt_covered;
   });

   // Note: CORE asset in collateral_bid_object is not counted in account_stats.total_core_in_orders

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

} } // graphene::chain
