#include <graphene/protocol/property_ops.hpp>

namespace graphene { namespace protocol {

void property_options::validate() const
{
   FC_ASSERT(!owner_contact_email.empty());
   FC_ASSERT(!description.empty());
   FC_ASSERT(!custodian.empty());
   FC_ASSERT(!title.empty());
   FC_ASSERT(!detailed_document_link.empty());
   FC_ASSERT(!status.empty());
   FC_ASSERT(!property_assignee.empty());
   FC_ASSERT(appraised_property_value    >= 0);
   FC_ASSERT(property_surety_bond_value  >= 0);
   FC_ASSERT(property_surety_bond_number >= 0);
   FC_ASSERT(!smooth_allocation_time.empty());
}

void property_create_operation::validate() const
{
   FC_ASSERT(fee.amount >= 0);
   common_options.validate();
}

void property_update_operation::validate() const
{
   FC_ASSERT(fee.amount >= 0);
   new_options.validate();
}
} }

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::property_options )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::property_create_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::property_update_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::property_create_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::property_update_operation )
