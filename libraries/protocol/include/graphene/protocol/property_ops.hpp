#pragma once
#include <graphene/protocol/asset.hpp>
#include <graphene/protocol/base.hpp>
#include <graphene/protocol/memo.hpp>
#include <graphene/protocol/types.hpp>

namespace graphene
{
namespace protocol
{

//options used to describe object
struct property_options
{
    string description;
    string title;
    string owner_contact_email;
    string custodian;
    string detailed_document_link;
    string image_url;
    string status;
    string property_assignee;

    uint64_t appraised_property_value;
    uint64_t property_surety_bond_value;
    uint64_t property_surety_bond_number;
    string smooth_allocation_time;

    string backed_by_asset_symbol;

    void validate() const;
};

//backed asset create smart contract
struct property_create_operation : public base_operation
{
    struct fee_parameters_type
    {
        uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
    };
    asset fee;

    uint32_t property_id;
    account_id_type issuer;
    property_options common_options;

    account_id_type fee_payer() const { return issuer; }
    void validate() const;
};
//backed asset update smart contract
struct property_update_operation : public base_operation
{
    struct fee_parameters_type
    {
        uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
    };

    property_update_operation() {}

    asset fee;
    account_id_type issuer;
    property_id_type property_to_update;
    property_options new_options;

    account_id_type fee_payer() const { return issuer; }
    void validate() const;
};

struct property_delete_operation : public base_operation
{
    struct fee_parameters_type { uint64_t fee = 0; };

    asset fee;
    property_id_type property;
    account_id_type fee_paying_account;

    account_id_type fee_payer() const { return fee_paying_account; }
    void            validate() const;
};

} // namespace chain
} // namespace graphene


FC_REFLECT(graphene::protocol::property_options,
           (description)
           (title)
           (owner_contact_email)
           (custodian)
           (detailed_document_link)
           (image_url)
           (status)
           (property_assignee)
           (appraised_property_value)
           (property_surety_bond_value)
           (property_surety_bond_number)
           (smooth_allocation_time)
           (backed_by_asset_symbol)
           )


FC_REFLECT(graphene::protocol::property_create_operation::fee_parameters_type, (fee))
FC_REFLECT(graphene::protocol::property_update_operation::fee_parameters_type, (fee))
FC_REFLECT(graphene::protocol::property_delete_operation::fee_parameters_type, (fee))

FC_REFLECT(graphene::protocol::property_create_operation,
           (fee)
           (property_id)
           (issuer)
           (common_options)
           )

FC_REFLECT(graphene::protocol::property_update_operation,
           (fee)
           (issuer)
           (property_to_update)
           (new_options)
           )

FC_REFLECT(graphene::protocol::property_delete_operation,
            (fee)
            (property)
            (fee_paying_account)
            )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::property_options )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::property_update_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::property_create_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::property_delete_operation::fee_parameters_type )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::property_create_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::property_update_operation ) 
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::property_delete_operation )
