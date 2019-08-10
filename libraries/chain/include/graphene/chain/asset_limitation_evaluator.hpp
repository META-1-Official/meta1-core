#pragma once
#include <graphene/protocol/operations.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/database.hpp>

namespace graphene
{
namespace chain
{
class asset_limitation_create_evaluator : public evaluator<asset_limitation_create_evaluator>
{
public:
    typedef asset_limitation_object_create_operation operation_type;

    void_result do_evaluate(const asset_limitation_object_create_operation &o);
    object_id_type do_apply(const asset_limitation_object_create_operation &o);
};

class asset_limitation_update_evaluator : public evaluator<asset_limitation_update_evaluator>
{
public:
    typedef asset_limitation_object_update_operation operation_type;

    void_result do_evaluate(const asset_limitation_object_update_operation &o);
    void_result do_apply(const asset_limitation_object_update_operation &o);

    const asset_limitation_object *asset_limitation_object_to_update = nullptr;
};
} // namespace chain
} // namespace graphene