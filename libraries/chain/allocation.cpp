/*
 * Copyright META1 (c) 2020
 */

#pragma once

#include <graphene/chain/allocation.hpp>

namespace graphene {
   namespace chain {
      time_point_sec calc_next_start_time(const time_point_sec &current_time, const time_point_sec &epoch,
                                          const uint32_t interval_seconds) {
         const uint32_t duration_since_epoch_seconds =
                 current_time.sec_since_epoch() - epoch.sec_since_epoch();
         const uint32_t intervals_since_epoch = duration_since_epoch_seconds / interval_seconds;
         const uint32_t next_interval = intervals_since_epoch + 1;
         const uint32_t duration_to_next_interval_seconds = next_interval * interval_seconds;

         const time_point_sec next_start_time = epoch + duration_to_next_interval_seconds;

         return next_start_time;
      }

      time_point_sec calc_preceding_start_time(const time_point_sec &current_time, const time_point_sec &epoch,
                                               const uint32_t interval_seconds) {
         const uint32_t duration_since_epoch_seconds =
                 current_time.sec_since_epoch() - epoch.sec_since_epoch();
         const uint32_t intervals_since_epoch = duration_since_epoch_seconds / interval_seconds;
         const uint32_t duration_to_preceding_interval_seconds = intervals_since_epoch * interval_seconds;

         const time_point_sec next_start_time = epoch + duration_to_preceding_interval_seconds;

         return next_start_time;
      }


      time_point_sec calc_meta1_next_start_time(const time_point_sec current_time) {
         return calc_next_start_time(current_time, META1_REFERENCE_TIME, META1_INTERVAL_BETWEEN_ALLOCATION_SECONDS);
      }

      time_point_sec calc_meta1_preceding_start_time(const time_point_sec current_time) {
         return calc_preceding_start_time(current_time, META1_REFERENCE_TIME,
                                          META1_INTERVAL_BETWEEN_ALLOCATION_SECONDS);
      }

      appreciation_period_parameters
      calc_meta1_allocation_initial_parameters(const time_point_sec &start_time, const uint32_t duration_seconds) {
         // Check the start time
         const uint32_t duration_since_reference_seconds =
                 start_time.sec_since_epoch() - META1_REFERENCE_TIME.sec_since_epoch();
         const uint32_t remainder_of_intervals_since_reference_time =
                 duration_since_reference_seconds % META1_INTERVAL_BETWEEN_ALLOCATION_SECONDS;
         FC_ASSERT(remainder_of_intervals_since_reference_time == 0);

         // Check the duration
         const uint32_t remainder_of_duration = duration_seconds % META1_INTERVAL_BETWEEN_ALLOCATION_SECONDS;
         FC_ASSERT(remainder_of_duration == 0);

         // Calculate the standard META1 parameters
         appreciation_period_parameters p;

         // Calculate parameters for the 100% time
         const time_point_sec end_time = start_time + duration_seconds;
         p.time_to_100_percent = end_time;
         p.time_to_100_percent_intervals = duration_seconds / META1_INTERVAL_BETWEEN_ALLOCATION_SECONDS;

         // Calculate parameters for the 25% time
         const uint32_t duration_to_25_percent = duration_seconds / 4;
         // TODO: ?Check if the duration is a multiple of the standard appreciation intervals?; test with a duration that is not evenly divisible by 4
         p.time_to_25_percent = start_time + duration_to_25_percent;
         p.time_to_25_percent_intervals = p.time_to_100_percent_intervals / 4;

         return p;
      }


      appreciation_restart_parameters
      calc_meta1_allocation_restart_parameters(const time_point_sec &current_time, const time_point_sec &end_time) {
         // Check for invalid current times
         FC_ASSERT(current_time.sec_since_epoch() < end_time.sec_since_epoch());

         // Calculate the implied preceding "start" time
         time_point_sec restart_time = calc_meta1_preceding_start_time(current_time);
         const uint32_t duration_seconds = end_time.sec_since_epoch() - restart_time.sec_since_epoch();

         // Calculate the intervals to the end
         const uint32_t intervals_to_end = duration_seconds / META1_INTERVAL_BETWEEN_ALLOCATION_SECONDS;

         // Package the parameters
         appreciation_restart_parameters p;
         p.restart_time = restart_time;
         p.intervals_to_end = intervals_to_end;

         return p;
      }

   }
}
