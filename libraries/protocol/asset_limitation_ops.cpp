#include <graphene/protocol/asset_limitation_ops.hpp>

#include <fc/io/raw.hpp>

#include <locale>


namespace graphene
{
namespace protocol
{

void asset_limitation_object_create_operation::validate() const
{
    FC_ASSERT(fee.amount >= 0);
}

void asset_limitation_object_update_operation::validate() const
{
    FC_ASSERT(fee.amount >= 0);
}

   void price_ratio::validate() const {
      FC_ASSERT(numerator >= 0);
      FC_ASSERT(denominator > 0);
   }

   void asset_price_publish_operation::validate() const {
       FC_ASSERT(fee.amount >= 0);
       FC_ASSERT(!symbol.empty());
       usd_price.validate();
   }

} // namespace protocol
} // namespace graphene

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION(graphene::protocol::asset_limitation_object_create_operation::fee_params_t )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION(graphene::protocol::asset_limitation_object_update_operation::fee_params_t )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION(graphene::protocol::asset_price_publish_operation::fee_params_t )

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION(graphene::protocol::asset_limitation_object_create_operation)
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION(graphene::protocol::asset_limitation_object_update_operation)
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION(graphene::protocol::asset_price_publish_operation)