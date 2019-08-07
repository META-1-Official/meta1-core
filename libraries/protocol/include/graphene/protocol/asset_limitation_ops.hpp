#pragma once
#include <graphene/protocol/asset.hpp>
#include <graphene/protocol/base.hpp>
#include <graphene/protocol/memo.hpp>
#include <graphene/protocol/types.hpp>

namespace graphene
{
namespace protocol
{

struct asset_limitation_options
{
    string sell_limit = "0.00000";
    string buy_limit = "0.00000";

    void validate() const;
};

//backed asset create smart contract
struct asset_limitation_object_create_operation : public base_operation
{
    struct fee_parameters_type
    {
        uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
    };
    asset fee;

    string limit_symbol;
    account_id_type issuer;
    asset_limitation_options common_options;

    account_id_type fee_payer() const { return issuer; }
    void validate() const;
};
//backed asset update smart contract
struct asset_limitation_object_update_operation : public base_operation
{
    struct fee_parameters_type
    {
        uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
    };

    asset_limitation_object_update_operation() {}

    asset fee;
    account_id_type issuer;
    asset_limitation_object_id_type asset_limitation_object_to_update;
    asset_limitation_options new_options;

    asset_limitation_options common_options;
    account_id_type fee_payer() const { return issuer; }
    void validate() const;
};

} // namespace protocol
} // namespace graphene

FC_REFLECT(graphene::protocol::asset_limitation_options,
           (sell_limit)(buy_limit))
FC_REFLECT(graphene::protocol::asset_limitation_object_create_operation::fee_parameters_type, (fee))
FC_REFLECT(graphene::protocol::asset_limitation_object_update_operation::fee_parameters_type, (fee))

FC_REFLECT(graphene::protocol::asset_limitation_object_create_operation,
           (fee)(limit_symbol)(issuer)(common_options))
FC_REFLECT(graphene::protocol::asset_limitation_object_update_operation,
           (fee)(issuer)(asset_limitation_object_to_update)(new_options))
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION(graphene::protocol::asset_limitation_options)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION(graphene::protocol::asset_limitation_object_create_operation::fee_parameters_type)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION(graphene::protocol::asset_limitation_object_update_operation::fee_parameters_type)

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION(graphene::protocol::asset_limitation_object_create_operation)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION(graphene::protocol::asset_limitation_object_update_operation)