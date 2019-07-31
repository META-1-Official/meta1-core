#include <graphene/chain/property_evaluator.hpp>
#include <graphene/chain/property_object.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>

#include <functional>

#include <locale>

namespace graphene
{
namespace chain
{
void_result property_create_evaluator::do_evaluate(const property_create_operation &op)
{
    try
    {
        database &d = db();
        const auto &chain_parameters = d.get_global_properties().parameters;
        auto &property_indx = d.get_index_type<property_index>().indices().get<by_property_id>();
        auto property_id_itr = property_indx.find(op.property_id);

        FC_ASSERT(property_id_itr == property_indx.end());

        return void_result();
    }
    FC_CAPTURE_AND_RETHROW((op))
}

object_id_type property_create_evaluator::do_apply(const property_create_operation &op)
{
    try
    {
        database &d = db();
        bool hf_429 = d.head_block_time() > HARDFORK_CORE_429_TIME;
        auto next_property_id = d.get_index_type<property_index>().get_next_id();
        const property_object &new_property =
            d.create<property_object>([&op, next_property_id](property_object &p) {
                p.issuer = op.issuer;
                p.options = op.common_options;
                p.property_id = op.property_id;
            });
        FC_ASSERT(new_property.id == next_property_id, "Unexpected object database error, object id mismatch");
        return new_property.id;
    }
    FC_CAPTURE_AND_RETHROW((op))
}

void_result property_update_evaluator::do_evaluate(const property_update_operation &op)
{
    try
    {
        database &d = db();
        const property_object &property_ob = op.property_to_update(d);

        property_to_update = &property_ob;

        FC_ASSERT(op.issuer == property_ob.issuer,
                  "Incorrect issuer for asset! (${o.issuer} != ${a.issuer})",
                  ("o.issuer", op.issuer)("a.issuer", property_ob.issuer));

        return void_result();
    }
    FC_CAPTURE_AND_RETHROW((op))
}
void_result property_update_evaluator::do_apply(const property_update_operation &op)
{
    try
    {
        database &d = db();
        d.modify(*property_to_update, [&op](property_object &p) {
            if (op.new_issuer)
                p.issuer = *op.new_issuer;
            p.options = op.new_options;
        });
        return void_result();
    }
    FC_CAPTURE_AND_RETHROW((op))
}

} // namespace chain
} // namespace graphene 
