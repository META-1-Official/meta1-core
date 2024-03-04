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
#include <graphene/chain/types.hpp>
#include <graphene/protocol/transaction.hpp>
#include <graphene/protocol/block.hpp>
#include <graphene/db/generic_index.hpp>

#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/random_access_index.hpp>

namespace graphene
{
namespace chain
{
using namespace graphene::db;

class rollup_object : public graphene::db::abstract_object<rollup_object>
{
public:
    static const uint8_t space_id = protocol_ids;
    static const uint8_t type_id = rollup_object_type;

    transaction                   proposed_transaction;
    account_id_type               proposer;
    std::string                   fail_reason;

    time_point_sec                expiration_time; //expiration time of rollup call to put ops on chain
};

class rollup_transaction_object : public graphene::db::abstract_object<rollup_transaction_object>
{
public:
   static const uint8_t space_id = protocol_ids;
   static const uint8_t type_id = rollup_transaction_object_type;

   signed_block proposed_block;
   account_id_type               proposer;
   std::string                   fail_reason;
   time_point_sec                expiration_time;
};

struct by_expiration;
typedef boost::multi_index_container<
    rollup_object,
    indexed_by<
      ordered_unique< tag< by_id >, member< object, object_id_type, &object::id > >,
      ordered_unique<tag<by_expiration>,
         composite_key<rollup_object,
         member<rollup_object, time_point_sec, &rollup_object::expiration_time>,
            member< object, object_id_type, &object::id >
         >
      >
   >
> rollup_multi_index_container;
typedef generic_index<rollup_object, rollup_multi_index_container> rollup_index;


typedef boost::multi_index_container<
    rollup_transaction_object,
    indexed_by<
      ordered_unique< tag< by_id >, member< object, object_id_type, &object::id > >,
      ordered_unique<tag<by_expiration>,
         composite_key<rollup_transaction_object,
         member<rollup_transaction_object, time_point_sec, &rollup_transaction_object::expiration_time>,
            member< object, object_id_type, &object::id >
         >
      >
   >
> rollup_multi_index_container;
typedef generic_index<rollup_transaction_object, rollup_multi_index_container> rollup_transaction_index;
}}

MAP_OBJECT_ID_TO_TYPE(graphene::chain::rollup_object)
MAP_OBJECT_ID_TO_TYPE(graphene::chain::rollup_transaction_object)

FC_REFLECT_TYPENAME( graphene::chain::rollup_object )
FC_REFLECT_TYPENAME( graphene::chain::rollup_transaction_object )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::rollup_object )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::rollup_transaction_object )