#include <graphene/chain/asset_limitation_object.hpp>
#include <fc/io/raw.hpp>

using namespace graphene::chain;

FC_REFLECT_DERIVED_NO_TYPENAME(graphene::chain::asset_limitation_object, (graphene::db::object),
                               (limit_symbol)(issuer)(options))
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION(graphene::chain::asset_limitation_object)

FC_REFLECT_DERIVED_NO_TYPENAME(graphene::chain::asset_price, (graphene::db::object),
                               (symbol)(usd_price)(publication_time))
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION(graphene::chain::asset_price)
