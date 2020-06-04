#include <graphene/chain/property_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/hardfork.hpp>

#include <fc/io/raw.hpp>
#include <fc/uint128.hpp>

using namespace graphene::chain;

namespace graphene {
   namespace chain {
      ratio_type property_object::get_allocation_progress() const {
         // TOOD: [Low] Review for computational optimization
         ratio_type initial = ratio_type(initial_counter, initial_counter_max);
         ratio_type approval = ratio_type(approval_counter, approval_counter_max);

         ratio_type a = initial / 4;
         ratio_type b = (approval * 3) / 4;

         ratio_type ratio = a + b;

         return ratio;
      }

   }
}

FC_REFLECT_DERIVED_NO_TYPENAME(graphene::chain::property_object, (graphene::db::object),
                   (property_id)
                   (issuer)
                   (appraised_property_value)
                   (allocation_duration_minutes)
                   (backed_by_asset_symbol)
                   (options)
                   (creation_date)
                   (initial_end_date)
                   (initial_counter)
                   (initial_counter_max)
                   (approval_date)
                   (approval_end_date)
                   (approval_counter)
                   (approval_counter_max)
                   (expired)
                   (next_allocation_date)
                  )
// TODO: Review which fields need reflection
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::property_object )