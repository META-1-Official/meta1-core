#pragma once
#include <graphene/protocol/asset.hpp>
#include <graphene/protocol/base.hpp>
#include <graphene/protocol/memo.hpp>
#include <graphene/protocol/types.hpp>

namespace graphene
{
namespace protocol
{

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

    // For future expansion
    extensions_type extensions;

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
    asset_limitation_id_type asset_limitation_object_to_update;

    // For future expansion
    extensions_type extensions;

    account_id_type fee_payer() const { return issuer; }
    void validate() const;
};


   /**
    * Price denominated in units of a monetary unit
    */
   struct price_ratio
   {
      uint32_t numerator;
      uint32_t denominator;

      price_ratio() : numerator(0), denominator(1) {}
      price_ratio(uint32_t n, uint32_t d) : numerator(n), denominator(d) {}

      void validate() const;
   };


   /**
    * Publish the USD-denominated price of an external asset
    */
   struct asset_price_publish_operation : public base_operation
   {
      struct fee_parameters_type
      {
         uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
      };

      asset_price_publish_operation() {}

      asset fee;
      account_id_type fee_paying_account;
      string symbol;
      price_ratio usd_price;

      // For future expansion
      extensions_type extensions;

      account_id_type fee_payer() const { return fee_paying_account; }
      void validate() const;
   };

} // namespace protocol
} // namespace graphene

FC_REFLECT(graphene::protocol::asset_limitation_object_create_operation::fee_parameters_type, (fee))
FC_REFLECT(graphene::protocol::asset_limitation_object_update_operation::fee_parameters_type, (fee))

FC_REFLECT(graphene::protocol::asset_limitation_object_create_operation,
           (fee)(limit_symbol)(issuer)(extensions))
FC_REFLECT(graphene::protocol::asset_limitation_object_update_operation,
           (fee)(issuer)(asset_limitation_object_to_update)(extensions))

FC_REFLECT(graphene::protocol::price_ratio, (numerator)(denominator))

FC_REFLECT(graphene::protocol::asset_price_publish_operation::fee_parameters_type, (fee))
FC_REFLECT(graphene::protocol::asset_price_publish_operation,
           (fee)(fee_paying_account)(symbol)(usd_price)(extensions))

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION(graphene::protocol::asset_limitation_object_create_operation::fee_parameters_type)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION(graphene::protocol::asset_limitation_object_update_operation::fee_parameters_type)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION(graphene::protocol::asset_price_publish_operation::fee_parameters_type)

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION(graphene::protocol::asset_limitation_object_create_operation)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION(graphene::protocol::asset_limitation_object_update_operation)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION(graphene::protocol::asset_price_publish_operation)