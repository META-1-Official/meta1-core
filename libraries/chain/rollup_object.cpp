/*
 * Copyright (c) 2015-2018 Cryptonomex, Inc., and contributors.
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
#include <graphene/chain/database.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/transaction_evaluation_state.hpp>
#include <graphene/chain/rollup_object.hpp>

namespace graphene { namespace chain {

    //is check required here for is allowed to execute?
}}

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::rollup_object, (graphene::chain::object),
                    (proposed_transaction)(proposer)(fail_reason)(expiration_time) )

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::rollup_transaction_object, (graphene::chain::object),
                    (proposed_block)(proposer)(fail_reason)(expiration_time) )

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::rollup_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::rollup_transaction_object )