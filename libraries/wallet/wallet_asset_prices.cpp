/*
 * Copyright META1 (c) 2020-2021
 */

#include <graphene/wallet/wallet.hpp>
#include "wallet_api_impl.hpp"

namespace graphene { namespace wallet { namespace detail {

   signed_transaction wallet_api_impl::publish_asset_price(string publishing_account,
                                          string symbol,
                                          price_ratio usd_price,
                                          bool broadcast) {
      try {
         account_object publishing_account_obj = get_account(publishing_account);

         asset_price_publish_operation publish_op;
         publish_op.symbol = symbol;
         publish_op.usd_price = usd_price;
         publish_op.fee_paying_account = publishing_account_obj.id;

         signed_transaction tx;
         tx.operations.push_back(publish_op);

         set_operation_fees(tx, _remote_db->get_global_properties().parameters.get_current_fees());

         tx.validate();
         signed_transaction transaction_result = sign_transaction(tx, broadcast);
         return transaction_result;
      }
      FC_CAPTURE_AND_RETHROW((publishing_account)(symbol)(usd_price)(broadcast))

   }

   price_ratio wallet_api_impl::get_published_asset_price(const std::string &symbol) const {
      price_ratio pr = _remote_db->get_published_asset_price(symbol);

      return pr;
   }

}}} // namespace graphene::wallet::detail
