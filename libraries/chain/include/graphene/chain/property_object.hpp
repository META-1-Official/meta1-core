#pragma once
#include <graphene/chain/types.hpp>
#include <graphene/db/generic_index.hpp>
#include <graphene/protocol/property_ops.hpp>

#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/random_access_index.hpp>

namespace graphene
{
namespace chain
{
using namespace graphene::db;

class property_object : public graphene::db::abstract_object<property_object>
{
public:
    static const uint8_t space_id = protocol_ids;
    static const uint8_t type_id = property_object_type;

    uint32_t property_id;
    account_id_type issuer;
    property_options options;

    // Only some of the following fields need to be serialized over the wire when properties are queried over an API
    time_point_sec date_creation;
    optional<time_point_sec> date_approval;
    uint64_t scaled_allocation_progress; // expressed in units where 10^12 units = 1.00

    // Derived value for expediting frequent calculations
    time_point_sec date_initial_end;
    time_point_sec date_approval_deadline;
    bool expired;
    uint64_t scaled_allocation_per_minute; // Rate of allocation
    time_point_sec date_next_allocation;

    property_id_type get_id() const { return id; }
    
};

struct by_property_id;
typedef multi_index_container<property_object,
                              indexed_by<
                                  ordered_unique<tag<by_id>, member<object, object_id_type, &object::id>>,
                                  ordered_unique<tag<by_property_id>, member<property_object, uint32_t, &property_object::property_id>>

                                  >>
    art_object_multi_index_type;
typedef generic_index<property_object, art_object_multi_index_type> property_index;

} // namespace chain
} // namespace graphene
MAP_OBJECT_ID_TO_TYPE(graphene::chain::property_object)

FC_REFLECT_TYPENAME(graphene::chain::property_object)

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION(graphene::chain::property_object)