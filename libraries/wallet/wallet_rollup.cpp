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
#include "wallet_api_impl.hpp"

namespace graphene { namespace wallet { namespace detail {

    void wallet_api_impl::rollup_build(transaction_handle_type transaction_handle, const operation& op)
   {
      wallet_api_impl::add_operation_to_builder_transaction(transaction_handle, op);
   }

    signed_transaction wallet_api_impl::sign_rollup_w_ops(transaction_handle_type 
         transaction_handle, time_point_sec expiration, string fee_asset, bool broadcast)
   {
      FC_ASSERT(_builder_transactions.count(transaction_handle));

      //build rollup op
      rollup_create_operation op;
      op.expiration_time = expiration;
      signed_transaction& trx = _builder_transactions[transaction_handle];
      std::transform(trx.operations.begin(), trx.operations.end(), std::back_inserter(op.rollup_ops),
                     [](const operation& op) -> op_wrapper { return op; });
      trx.operations = {op};

      //set fee
      auto fee_asset_obj = get_asset(fee_asset);
      if( fee_asset_obj.get_id() != asset_id_type() )
      {
         _remote_db->get_global_properties().parameters.get_current_fees().set_fee( 
            trx.operations.front(), fee_asset_obj.options.core_exchange_rate );
      }
      else
      {
         _remote_db->get_global_properties().parameters.get_current_fees().set_fee( trx.operations.front() );
      }

      
      //sign transaction
      trx = sign_rollup_transaction(trx);

      //broadcast signed transaction
      if(broadcast)
      {
         try
            {
               _remote_net_broadcast->broadcast_transaction( trx );
            }
            catch (const fc::exception& e)
            {
               elog("Caught exception while broadcasting tx ${id}:  ${e}",
                  ("id", trx.id().str())("e", e.to_detail_string()) );
               throw;
            }
      }
      return trx;
   }

    signed_transaction wallet_api_impl::rollup_transactions_push(vector<signed_transaction> trxs, time_point_sec expiration)
    {
        _rollup_handler->rollup_transactions_handle(trxs);
    }
}}} // namespace graphene::wallet::detail



//{"ref_block_num": 1356,"ref_block_prefix": 1425121125,"expiration": "2024-03-08T09:05:28","operations": [[0,{"fee": {"amount": 2000000,"asset_id": "1.3.0"},"from": "1.2.6","to": "1.2.9","amount": {"amount": 10,"asset_id": "1.3.0"},"extensions": []}],[0,{"fee": {"amount": 2000000,"asset_id": "1.3.0"},"from": "1.2.6","to": "1.2.9","amount": {"amount": 10,"asset_id": "1.3.0"},"extensions": []}]],"extensions": [],"signatures": ["20664771d8f408d20f41a965128ddf298509d1b8d301df5eaad840d8bb2fff359531711e6a2e5e1cb49f0dfa35ecad603a8ee4b2cc301ba3e5e5f8a8e71e9b74db"]}