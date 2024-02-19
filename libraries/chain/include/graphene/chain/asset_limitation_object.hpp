#pragma once
#include <graphene/chain/types.hpp>
#include <graphene/db/generic_index.hpp>
#include <graphene/protocol/asset_limitation_ops.hpp>

namespace graphene
{
namespace chain
{
using namespace graphene::db;

class asset_limitation_object : public graphene::db::abstract_object<asset_limitation_object,protocol_ids, impl_transaction_history_object_type>
{
public:
    static const uint8_t space_id = protocol_ids;
    static const uint8_t type_id = asset_limitation_object_type;

    string limit_symbol;
    account_id_type issuer;

    // The cumulative sell limit for an asset that is backed by other properties
    uint64_t cumulative_sell_limit = 0;
};

struct by_limit_symbol;
typedef multi_index_container<asset_limitation_object,
                              indexed_by<
                                  ordered_unique<tag<by_id>, member<object, object_id_type, &object::id>>,
                                  ordered_unique<tag<by_limit_symbol>, member<asset_limitation_object, string, &asset_limitation_object::limit_symbol>>>>
    asset_limitation_index_type;
typedef generic_index<asset_limitation_object, asset_limitation_index_type> asset_limitation_index;


   /**
    * Price of an external asset
    */
   class asset_price : public graphene::db::abstract_object<asset_price, implementation_ids, impl_asset_price_object_type> {
   public:
      static const uint8_t space_id = implementation_ids;
      static const uint8_t type_id = impl_asset_price_object_type;

      /// Ticker symbol for this asset
      string symbol;
      /// USD-price expressed as a ratio
      price_ratio usd_price;
      /// Block time of publication
      time_point_sec publication_time;
   };

   struct by_symbol;
   typedef multi_index_container<asset_price,
           indexed_by<
                   ordered_unique<tag<by_id>, member<object, object_id_type, &object::id>>,
                   ordered_unique<tag<by_symbol>, member<asset_price, string, &asset_price::symbol>>>>
           asset_price_index_type;
   typedef generic_index<asset_price, asset_price_index_type> asset_price_index;

} // namespace chain
} // namespace graphene
MAP_OBJECT_ID_TO_TYPE(graphene::chain::asset_limitation_object)
MAP_OBJECT_ID_TO_TYPE(graphene::chain::asset_price)

FC_REFLECT_TYPENAME(graphene::chain::asset_limitation_object)
FC_REFLECT_TYPENAME(graphene::chain::asset_price)

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION(graphene::chain::asset_limitation_object)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION(graphene::chain::asset_price)
