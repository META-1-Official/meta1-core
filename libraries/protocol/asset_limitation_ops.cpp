#include <graphene/protocol/asset_limitation_ops.hpp>

#include <fc/io/raw.hpp>

#include <locale>


namespace graphene
{
namespace protocol
{

void asset_limitation_options::validate() const
{
    FC_ASSERT(!sell_limit.empty());
    FC_ASSERT(!buy_limit.empty());
    FC_ASSERT(std::stod(sell_limit)>=0.0);
    FC_ASSERT(std::stod(buy_limit)>0.0);

}

void asset_limitation_object_create_operation::validate() const
{
    FC_ASSERT(fee.amount >= 0);
    common_options.validate();
}

void asset_limitation_object_update_operation::validate() const
{
    FC_ASSERT(fee.amount >= 0);
    new_options.validate();
}

   void price_ratio::validate() const {
      FC_ASSERT(numerator >= 0);
      FC_ASSERT(denominator > 0);
   }


} // namespace protocol
} // namespace graphene

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION(graphene::protocol::asset_limitation_options)
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION(graphene::protocol::asset_limitation_object_create_operation::fee_parameters_type)
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION(graphene::protocol::asset_limitation_object_update_operation::fee_parameters_type)
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION(graphene::protocol::asset_limitation_object_create_operation)
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION(graphene::protocol::asset_limitation_object_update_operation)