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

namespace graphene { namespace protocol { 

    /**
    * op_wrapper is used to get around the circular definition of operation and rollup that contain them.
    */
    struct op_wrapper;
   

    struct rollup_create_operation : base_operation {
        struct fee_parameters_type { 
          uint64_t fee            = 20 * GRAPHENE_BLOCKCHAIN_PRECISION; 
          uint32_t price_per_kbyte = 10;
       };

        asset              fee;
        account_id_type    fee_paying_account;
        time_point_sec     expiration_time;

        vector<op_wrapper> rollup_ops;

        extensions_type    extensions;

       account_id_type fee_payer()const { return fee_paying_account; }
       void            validate()const;
       share_type      calculate_fee(const fee_parameters_type& k)const;
    }; 
}}

FC_REFLECT( graphene::protocol::rollup_create_operation::fee_parameters_type, (fee)(price_per_kbyte) )

FC_REFLECT( graphene::protocol::rollup_create_operation, (fee)(fee_paying_account)
            (expiration_time)(rollup_ops)(extensions) )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::rollup_create_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::rollup_create_operation )
