#include <graphene/chain/property_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/hardfork.hpp>

#include <fc/io/raw.hpp>
#include <fc/uint128.hpp>

using namespace graphene::chain;

FC_REFLECT_DERIVED_NO_TYPENAME(graphene::chain::property_object, (graphene::db::object),
                   (property_id)
                   (issuer)
                   (options)
                   (date_creation)
                   (date_approval)
                   (scaled_allocation_progress)
                  )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::property_object )