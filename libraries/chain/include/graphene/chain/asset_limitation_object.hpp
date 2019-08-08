#pragma once
#include <graphene/chain/types.hpp>
#include <graphene/db/generic_index.hpp>
#include <graphene/protocol/asset_limitation_ops.hpp>

namespace graphene
{
namespace chain
{
using namespace graphene::db;

class asset_limitation_object : public graphene::db::abstract_object<asset_limitation_object>
{
public:
    static const uint8_t space_id = protocol_ids;
    static const uint8_t type_id = asset_limitation_object_type;

    string limit_symbol;
    account_id_type issuer;
    asset_limitation_options options;

    asset_limitation_id_type get_id() const { return id; }
};

struct by_limit_symbol;
typedef multi_index_container<asset_limitation_object,
                              indexed_by<
                                  ordered_unique<tag<by_id>, member<object, object_id_type, &object::id>>,
                                  ordered_unique<tag<by_limit_symbol>, member<asset_limitation_object, string, &asset_limitation_object::limit_symbol>>>>
    asset_limitation_index_type;
typedef generic_index<asset_limitation_object, asset_limitation_index_type> asset_limitation_index;

} // namespace chain
} // namespace graphene
MAP_OBJECT_ID_TO_TYPE(graphene::chain::asset_limitation_object)

FC_REFLECT_TYPENAME(graphene::chain::asset_limitation_object)

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION(graphene::chain::asset_limitation_object)