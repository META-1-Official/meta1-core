#pragma once
#include <graphene/protocol/operations.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/database.hpp>

#include <graphene/chain/hardfork.hpp>
#include <locale>

namespace graphene
{
namespace chain
{
class property_create_evaluator : public evaluator<property_create_evaluator>
{
public:
    typedef property_create_operation operation_type;

    void_result do_evaluate(const property_create_operation &o);
    object_id_type do_apply(const property_create_operation &o);
};

class property_update_evaluator : public evaluator<property_update_evaluator>
{
public:
    typedef property_update_operation operation_type;

    void_result do_evaluate(const property_update_operation &o);
    void_result do_apply(const property_update_operation &o);

    const property_object *property_to_update = nullptr;
};
} // namespace chain
} // namespace graphene 