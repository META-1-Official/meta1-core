#include <graphene/protocol/property_ops.hpp>
#include <string>
#include <regex>

namespace graphene { namespace protocol {

bool is_email_valid(const std::string& email)
{
   const std::regex pattern
      ("(\\w+)(\\.|_)?(\\w*)@(\\w+)(\\.(\\w+))+");
   return std::regex_match(email, pattern);
}

void property_options::validate() const
{
   FC_ASSERT(!owner_contact_email.empty());
   FC_ASSERT(is_email_valid(owner_contact_email));
   FC_ASSERT(!description.empty());
   FC_ASSERT(!custodian.empty());
   FC_ASSERT(!title.empty());
   FC_ASSERT(!detailed_document_link.empty());
   FC_ASSERT(!status.empty());
   FC_ASSERT(status == "not approved" || status == "approved");
   FC_ASSERT(!property_assignee.empty());
   FC_ASSERT(appraised_property_value    >= 0);
   FC_ASSERT(property_surety_bond_value  >= 0);
   FC_ASSERT(property_surety_bond_number >= 0);
   FC_ASSERT(!smooth_allocation_time.empty());
   FC_ASSERT(std::stod(smooth_allocation_time)>0);
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

void property_delete_operation::validate() const
{
   FC_ASSERT(fee.amount >= 0);
}
} }

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::property_options )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::property_create_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::property_update_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::property_delete_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::property_create_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::property_update_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::property_delete_operation )

