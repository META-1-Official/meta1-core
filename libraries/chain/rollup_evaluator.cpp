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
#include <graphene/chain/rollup_evaluator.hpp>
#include <graphene/chain/rollup_object.hpp>
#include <graphene/chain/hardfork.hpp>

namespace graphene { namespace chain {

void_result rollup_create_evaluator::do_evaluate(const rollup_create_operation& o)
{
try {
   const database& d = db();

   const auto& global_parameters = d.get_global_properties().parameters;

   FC_ASSERT( o.expiration_time > block_time, "Rollup has already expired on creation." );

   for( const op_wrapper& op : o.rollup_ops )
      _proposed_trx.operations.push_back(op.op);

   _proposed_trx.validate();

return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

object_id_type rollup_create_evaluator::do_apply(const rollup_create_operation& o)
{
    try
    {
        database& d = db();

        const rollup_object& rollup = d.create<rollup_object>([&](rollup_object& rollup) {
            _proposed_trx.expiration = o.expiration_time;
            rollup.proposed_transaction = _proposed_trx;
            rollup.expiration_time = o.expiration_time;
            rollup.proposer = o.fee_paying_account;
        });
    return rollup.id;
} FC_CAPTURE_AND_RETHROW( (o) ) }
}}