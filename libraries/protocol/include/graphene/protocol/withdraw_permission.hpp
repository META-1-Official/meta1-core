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
#include <graphene/protocol/base.hpp>
#include <graphene/protocol/asset.hpp>
#include <graphene/protocol/memo.hpp>

namespace graphene { namespace protocol { 

   /**
    * @brief Create a new withdrawal permission
    * @ingroup operations
    *
    * This operation creates a withdrawal permission, which allows some authorized account to withdraw from an
    * authorizing account. This operation is primarily useful for scheduling recurring payments.
    *
    * Withdrawal permissions define withdrawal periods, which is a span of time during which the authorized account may
    * make a withdrawal. Any number of withdrawals may be made so long as the total amount withdrawn per period does
    * not exceed the limit for any given period.
    *
    * Withdrawal permissions authorize only a specific pairing, i.e. a permission only authorizes one specified
    * authorized account to withdraw from one specified authorizing account. Withdrawals are limited and may not exceet
    * the withdrawal limit. The withdrawal must be made in the same asset as the limit; attempts with withdraw any
    * other asset type will be rejected.
    *
    * The fee for this operation is paid by withdraw_from_account, and this account is required to authorize this
    * operation.
    */
   struct withdraw_permission_create_operation : public base_operation
   {
      struct fee_parameters_type { uint64_t fee =  GRAPHENE_BLOCKCHAIN_PRECISION; };

      asset             fee;
      /// The account authorizing withdrawals from its balances
      account_id_type   withdraw_from_account;
      /// The account authorized to make withdrawals from withdraw_from_account
      account_id_type   authorized_account;
      /// The maximum amount authorized_account is allowed to withdraw in a given withdrawal period
      asset             withdrawal_limit;
      /// Length of the withdrawal period in seconds
      uint32_t          withdrawal_period_sec = 0;
      /// The number of withdrawal periods this permission is valid for
      uint32_t          periods_until_expiration = 0;
      /// Time at which the first withdrawal period begins; must be in the future
      time_point_sec    period_start_time;

      account_id_type fee_payer()const { return withdraw_from_account; }
      void            validate()const;
   };

   
   /**
    * @brief Withdraw from an account which has published a withdrawal permission
    * @ingroup operations
    *
    * This operation is used to withdraw from an account which has authorized such a withdrawal. It may be executed at
    * most once per withdrawal period for the given permission. On execution, amount_to_withdraw is transferred from
    * withdraw_from_account to withdraw_to_account, assuming amount_to_withdraw is within the withdrawal limit. The
    * withdrawal permission will be updated to note that the withdrawal for the current period has occurred, and
    * further withdrawals will not be permitted until the next withdrawal period, assuming the permission has not
    * expired. This operation may be executed at any time within the current withdrawal period.
    *
    * Fee is paid by withdraw_to_account, which is required to authorize this operation
    */
   struct withdraw_permission_claim_operation : public base_operation
   {
      struct fee_parameters_type { 
         uint64_t fee = 20*GRAPHENE_BLOCKCHAIN_PRECISION; 
         uint32_t price_per_kbyte = 10;
      };

      /// Paid by withdraw_to_account
      asset                       fee;
      /// ID of the permission authorizing this withdrawal
      withdraw_permission_id_type withdraw_permission;
      /// Must match withdraw_permission->withdraw_from_account
      account_id_type             withdraw_from_account;
      /// Must match withdraw_permision->authorized_account
      account_id_type             withdraw_to_account;
      /// Amount to withdraw. Must not exceed withdraw_permission->withdrawal_limit
      asset                       amount_to_withdraw;
      /// Memo for withdraw_from_account. Should generally be encrypted with withdraw_from_account->memo_key
      optional<memo_data>         memo;

      account_id_type fee_payer()const { return withdraw_to_account; }
      void            validate()const;
      share_type      calculate_fee(const fee_parameters_type& k)const;
   };

   
} } // graphene::protocol

FC_REFLECT( graphene::protocol::withdraw_permission_create_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::withdraw_permission_claim_operation::fee_parameters_type, (fee)(price_per_kbyte) )

FC_REFLECT( graphene::protocol::withdraw_permission_create_operation, (fee)(withdraw_from_account)(authorized_account)
            (withdrawal_limit)(withdrawal_period_sec)(periods_until_expiration)(period_start_time) )
FC_REFLECT( graphene::protocol::withdraw_permission_claim_operation, (fee)(withdraw_permission)(withdraw_from_account)(withdraw_to_account)(amount_to_withdraw)(memo) );

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::withdraw_permission_create_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::withdraw_permission_claim_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::withdraw_permission_create_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::withdraw_permission_claim_operation )
