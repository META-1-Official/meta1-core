#include <graphene/protocol/property_ops.hpp>

#include <fc/io/raw.hpp>

#include <locale>

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
   FC_ASSERT(!property_assignee.empty());
   FC_ASSERT(property_surety_bond_value  >= 0);
   FC_ASSERT(property_surety_bond_number >= 0);
}

void property_create_operation::validate() const
{
   FC_ASSERT(fee.amount >= 0);
   FC_ASSERT(appraised_property_value >= 0);
   FC_ASSERT(allocation_duration_minutes >= 4); // Minimum requirement of 4 minutes

   // Multiple of 4 minutes to eliminate rounding errors for allocation
   FC_ASSERT((allocation_duration_minutes % 4) == 0, "The allocation duration should be a multiple of 4 minutes");

   common_options.validate();
}

void property_update_operation::validate() const
{
   FC_ASSERT(fee.amount >= 0);
   new_options.validate();
}

void property_approve_operation::validate() const
{
   FC_ASSERT(fee.amount >= 0);
}

void property_delete_operation::validate() const
{
   FC_ASSERT(fee.amount >= 0);
}
} }

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::property_options )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::property_create_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::property_update_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::property_approve_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::property_delete_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::property_create_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::property_update_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::property_approve_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::property_delete_operation )

