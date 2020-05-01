/*
 * Copyright META1 (c) 2020
 */
#pragma once

#include <graphene/protocol/types.hpp>

using fc::time_point_sec;

namespace graphene {
   namespace chain {
      /**
       * The initial parameters for an appreciation period of property with a given vesting duration
       * will defined such that increments of the appreciation from 0% to 100% will occur at "standard one-minute intervals"
       * which are defined as occurring on the "0 second" mark of a minute.
       *
       * This implies that the discrete quantity of increments (total_increments) can be pre-calculated,
       * and thereafter all processing on the "0 second" mark will increment the appreciation (progress) by 1
       * such that the fractional progress, at a conceptual level, can be calculated as
       *
       * fractional_progress = progress / total_increments.
       *
       * This fractional progress is a rational fraction (i.e. without any rounding or truncation).
       *
       * The general concept becomes more intricate given that another requirement of the smart contracts permits
       * the appreciation to be paused at 25% if the property's valuation is not approved by 25% of the timeline
       * (i.e. the entire vesting duration).  Subsequent approval may be granted at any time between 25% and 100%
       * of the timeline at which time the appreciation restarts but at an accelerated rate
       * such that the remaining 75% of appreciation should complete by 100% of the timeline (i.e. the approval deadline).
       *
       * To prevent any numerical loss for "late approvals", an additional discrete quantity of increments
       * for the approval period shall be calculated (total_approval_increments)
       * separate from the quantity required during the initial period (total_initial_increments).  Similarly, two separate
       * progress variables will be during the initial period (progress_initial) and the approval period (progress_approval).
       *
       * With this arrangement, the fractional progress can be calculated at any time as a rational fraction according to
       *
       * fractional_progress = progress_initial / total_increments_initial + progress_approval / total_increments_approval
       *
       * If a "late approval" occurs, the total_increments_approval will be appropriate calculated
       * for the time of the late approval.
       */

      /**
       * Parameters for initializing an appreciation period for a property
       *
       * The parameters for the 100% initial period are valid if the appraisal approval is granted before the initial
       * period completes.  In which case smooth appreciation can continue without any interruption.
       */
      struct appreciation_period_parameters {
         time_point_sec time_to_25_percent;
         uint32_t time_to_25_percent_intervals;

         time_point_sec time_to_100_percent;
         uint32_t time_to_100_percent_intervals;
      };

      /**
       * Parameters for restarting appreciation after the 25% time for a property
       */
      struct appreciation_restart_parameters {
         time_point_sec restart_time;
         uint32_t intervals_to_end;
      };

      static const time_point_sec META1_REFERENCE_TIME = time_point_sec(); // 1970-01-01T00:00:00Z
      static const uint32_t META1_INTERVAL_BETWEEN_ALLOCATION_SECONDS = 60;

      /**
       * Calculate the next start time based on intervals that are calculated relative to 1970-01-01T00:00:00Z
       * @param current_time   Current time
       * @param epoch  Epoch time
       * @param interval_seconds   Interval that should be used relative to reference time
       * @return   Next start time
       */
      time_point_sec calc_next_start_time(const time_point_sec &current_time, const time_point_sec &epoch,
                                          const uint32_t interval_seconds);

      /**
       * Calculate the preceding start time based on intervals that are calculated relative to 1970-01-01T00:00:00Z
       * @param current_time   Current time
       * @param epoch   Epoch time
       * @param interval_seconds   Interval that should be used relative to reference time
       * @return   Next start time
       */
      time_point_sec calc_preceding_start_time(const time_point_sec &current_time, const time_point_sec &epoch,
                                               const uint32_t interval_seconds);


      /**
       * Calculate the standard META1 next start time
       *
       * @param current_time   Current time
       * @return   Next standard start time
       */
      time_point_sec calc_meta1_next_start_time(const time_point_sec current_time);

      /**
       * Calculate the standard META1 preceding start time
       *
       * @param current_time   Current time
       * @return   Preceding standard start time
       */
      time_point_sec calc_meta1_preceding_start_time(const time_point_sec current_time);

      /**
       * Calculate a start time based on intervals that are calculated relative to 1970-01-01T00:00:00Z
       * @param current_time   Current time
       * @param duration_seconds Duration of vesting
       * @return
       */
      appreciation_period_parameters
      calc_meta1_allocation_initial_parameters(const time_point_sec &start_time, const uint32_t duration_seconds);

      /**
       * Calculate new parameters for restarting the allocation
       * @param current_time   Current time
       * @param end_time Scheduled end time
       * @return Parameters that are appropriate between now and the scheduled end time
       */
      appreciation_restart_parameters
      calc_meta1_allocation_restart_parameters(const time_point_sec &current_time, const time_point_sec &end_time);
   }
}