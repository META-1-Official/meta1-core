#pragma once

#include "database_fixture.hpp"

namespace graphene {
   namespace chain {
      struct meta1_fixture : database_fixture {
         bool is_property_exists(const uint32_t property_id) const;

         property_create_operation create_property_operation(const string issuer,
                                                             const uint64_t appraised_property_value,
                                                             const uint32_t allocation_duration_minutes,
                                                             const string backed_by_asset_symbol,
                                                             const property_options common) const;


         /**
          * Calcuate the META1 valuation derived from a single property's appraised value and current allocation progress
          * @param appraised_value    Appraised value in some monetary unit
          * @param progress_numerator  Numerator of the allocation progress
          * @param progress_denominator  Denominator of the allocation progress
          * @return   Valuation in the same monetary unit
          */
         static const uint64_t calculate_meta1_valuation(uint64_t appraised_value, const uint64_t progress_numerator,
                                                         const uint64_t progress_denominator);

         /**
          * Calcuate the META1 valuation derived from a single property's appraised value and current allocation progress
          * @param appraised_value    Appraised value in some monetary unit
          * @param progress  Allocation progress as a rational value
          * @return   Valuation in the same monetary unit
          */
         static const uint64_t calculate_meta1_valuation(uint64_t appraised_value, const ratio_type progress);


         /**
          * Publish an external USD-price for a user-issued asset
          *
          * @param asset_symbol Asset symbol
          * @param price Price
          * @param account_id   Account ID
          * @param private_key Private key
          */
         void publish_asset_price(const string asset_symbol, const price_ratio price,
                                  const account_id_type authorizing_id,
                                  const private_key_type authorizing_private_key);


         /**
          * Create, approve, and advance the blocks until it the property is fully allocated
          * @param appraised_property_value Appraised property value in a monetary unit
          * @param allocation_duration_minutes  Allocation duration
          * @param options Property attributes
          * @param private_key Private key of META1 account
          * @return Identifier of the new property
          */
         const property_id_type allocate_property(const uint64_t appraised_property_value,
                                                  const uint32_t allocation_duration_minutes,
                                                  const property_options options,
                                                  const account_id_type authorizing_id,
                                                  const private_key_type authorizing_private_key);

         /**
          * Create, approve, and advance the blocks until it the property is fully allocated.
          * The property will be defined with dummy property attributes.
          *
          * @param appraised_property_value Appraised property value in a monetary unit
          * @param allocation_duration_minutes  Allocation duration
          * @param private_key Private key of META1 account
          * @return Identifier of the new property
          */
         const property_id_type allocate_property(const uint64_t appraised_property_value,
                                                  const uint32_t allocation_duration_minutes,
                                                  const account_id_type authorizing_id,
                                                  const private_key_type authorizing_private_key);

      private:
         /**
          * Initialize the allocation object for an asset that can be backed by a property
          */
         void initialize_allocation_object(const string asset_to_back, const account_id_type authorizing_id,
                                           const private_key_type authorizing_private_key);
      };
   }
}