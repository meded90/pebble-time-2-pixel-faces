#include "fitness_math.h"

#include <stdio.h>
#include <string.h>

#define FITNESS_MAX_HR_MIN 100
#define FITNESS_MAX_HR_MAX 240
#define FITNESS_AGE_MIN 14
#define FITNESS_AGE_MAX 100
#define FITNESS_ZONE_HYSTERESIS_BPM 2
#define FITNESS_ZONE_CONFIRM_READINGS 2
#define FITNESS_HRV_PPI_MIN_MS 300
#define FITNESS_HRV_PPI_MAX_MS 2000
#define FITNESS_HRV_MIN_COVERAGE_MS 48000
#define FITNESS_HRV_MIN_PAIRS 30

void fitness_bpm_filter_reset(FitnessBpmFilter *filter) {
  if (filter) {
    memset(filter, 0, sizeof(*filter));
  }
}

uint16_t fitness_bpm_filter_update(FitnessBpmFilter *filter, uint16_t bpm) {
  if (!filter || bpm == 0) {
    return 0;
  }

  filter->samples[filter->next_index] = bpm;
  filter->next_index =
      (uint8_t)((filter->next_index + 1U) % FITNESS_BPM_FILTER_WINDOW);
  if (filter->count < FITNESS_BPM_FILTER_WINDOW) {
    ++filter->count;
  }

  if (filter->count == 1) {
    return filter->samples[0];
  }
  if (filter->count == 2) {
    return (uint16_t)(((uint32_t)filter->samples[0] +
                       filter->samples[1] + 1U) /
                      2U);
  }

  const uint16_t a = filter->samples[0];
  const uint16_t b = filter->samples[1];
  const uint16_t c = filter->samples[2];
  if ((a <= b && b <= c) || (c <= b && b <= a)) {
    return b;
  }
  if ((b <= a && a <= c) || (c <= a && a <= b)) {
    return a;
  }
  return c;
}

static bool bounds_are_valid(const FitnessZoneBounds *bounds) {
  if (!bounds || bounds->lower[0] == 0) {
    return false;
  }

  for (size_t i = 1; i < FITNESS_ZONE_COUNT; ++i) {
    if (bounds->lower[i] <= bounds->lower[i - 1]) {
      return false;
    }
  }
  return true;
}

static uint16_t percentage_ceiling(uint16_t value, uint8_t percent) {
  return (uint16_t)(((uint32_t)value * percent + 99U) / 100U);
}

static bool bounds_from_max_hr(uint16_t max_hr,
                               FitnessZoneBounds *out_bounds) {
  static const uint8_t percentages[FITNESS_ZONE_COUNT] = {
      50, 60, 70, 80, 90,
  };

  if (max_hr < FITNESS_MAX_HR_MIN || max_hr > FITNESS_MAX_HR_MAX) {
    return false;
  }

  for (size_t i = 0; i < FITNESS_ZONE_COUNT; ++i) {
    out_bounds->lower[i] = percentage_ceiling(max_hr, percentages[i]);
  }
  return bounds_are_valid(out_bounds);
}

bool fitness_zone_bounds_build(const FitnessZoneConfig *config,
                               FitnessZoneBounds *out_bounds) {
  if (!config || !out_bounds) {
    return false;
  }

  FitnessZoneBounds calculated;
  memset(&calculated, 0, sizeof(calculated));

  switch (config->mode) {
    case FITNESS_ZONE_MODE_MAX_HR:
      if (!bounds_from_max_hr(config->max_hr, &calculated)) {
        return false;
      }
      break;

    case FITNESS_ZONE_MODE_AGE: {
      if (config->age < FITNESS_AGE_MIN || config->age > FITNESS_AGE_MAX) {
        return false;
      }

      uint16_t estimated_max_hr;
      switch (config->age_formula) {
        case FITNESS_AGE_FORMULA_208:
          estimated_max_hr =
              (uint16_t)((2080U - 7U * config->age + 5U) / 10U);
          break;
        case FITNESS_AGE_FORMULA_220:
          estimated_max_hr = (uint16_t)(220U - config->age);
          break;
        default:
          return false;
      }

      if (!bounds_from_max_hr(estimated_max_hr, &calculated)) {
        return false;
      }
      break;
    }

    case FITNESS_ZONE_MODE_MANUAL:
      memcpy(calculated.lower, config->manual_lower,
             sizeof(calculated.lower));
      if (!bounds_are_valid(&calculated)) {
        return false;
      }
      break;

    default:
      return false;
  }

  *out_bounds = calculated;
  return true;
}

uint8_t fitness_classify_zone(const FitnessZoneBounds *bounds, uint16_t bpm) {
  if (bpm == 0 || !bounds_are_valid(bounds)) {
    return 0;
  }

  uint8_t zone = 0;
  for (size_t i = 0; i < FITNESS_ZONE_COUNT; ++i) {
    if (bpm < bounds->lower[i]) {
      break;
    }
    zone = (uint8_t)(i + 1);
  }
  return zone;
}

void fitness_zone_stabilizer_reset(FitnessZoneStabilizer *stabilizer) {
  if (stabilizer) {
    memset(stabilizer, 0, sizeof(*stabilizer));
  }
}

static uint8_t zone_with_hysteresis(const FitnessZoneStabilizer *stabilizer,
                                    const FitnessZoneBounds *bounds,
                                    uint16_t bpm) {
  const uint8_t raw_zone = fitness_classify_zone(bounds, bpm);
  const uint8_t stable_zone = stabilizer->stable_zone;

  if (stable_zone == 0 || stable_zone > FITNESS_ZONE_COUNT ||
      raw_zone == stable_zone) {
    return raw_zone;
  }

  if (raw_zone > stable_zone) {
    uint8_t target = stable_zone;
    for (uint8_t zone = (uint8_t)(stable_zone + 1); zone <= raw_zone; ++zone) {
      const uint32_t threshold =
          (uint32_t)bounds->lower[zone - 1] + FITNESS_ZONE_HYSTERESIS_BPM;
      if ((uint32_t)bpm >= threshold) {
        target = zone;
      }
    }
    return target;
  }

  const uint16_t boundary = bounds->lower[stable_zone - 1];
  const uint16_t lower_exit = boundary > FITNESS_ZONE_HYSTERESIS_BPM
                                  ? (uint16_t)(boundary -
                                               FITNESS_ZONE_HYSTERESIS_BPM)
                                  : 0;
  return bpm <= lower_exit ? raw_zone : stable_zone;
}

uint8_t fitness_zone_stabilizer_update(FitnessZoneStabilizer *stabilizer,
                                       const FitnessZoneBounds *bounds,
                                       uint16_t bpm) {
  if (!stabilizer || bpm == 0 || !bounds_are_valid(bounds)) {
    return 0;
  }

  const uint8_t target = zone_with_hysteresis(stabilizer, bounds, bpm);
  if (target == stabilizer->stable_zone) {
    stabilizer->candidate_zone = 0;
    stabilizer->candidate_readings = 0;
    return stabilizer->stable_zone;
  }

  if (target != stabilizer->candidate_zone) {
    stabilizer->candidate_zone = target;
    stabilizer->candidate_readings = 1;
  } else if (stabilizer->candidate_readings < FITNESS_ZONE_CONFIRM_READINGS) {
    ++stabilizer->candidate_readings;
  }

  if (stabilizer->candidate_readings >= FITNESS_ZONE_CONFIRM_READINGS) {
    stabilizer->stable_zone = target;
    stabilizer->candidate_zone = 0;
    stabilizer->candidate_readings = 0;
  }

  return stabilizer->stable_zone;
}

bool fitness_format_duration(uint32_t seconds, char *buffer,
                             size_t buffer_size) {
  if (!buffer || buffer_size == 0) {
    return false;
  }

  int written;
  if (seconds < 3600U) {
    written = snprintf(buffer, buffer_size, "%02lu:%02lu",
                       (unsigned long)(seconds / 60U),
                       (unsigned long)(seconds % 60U));
  } else {
    written = snprintf(buffer, buffer_size, "%lu:%02lu:%02lu",
                       (unsigned long)(seconds / 3600U),
                       (unsigned long)((seconds / 60U) % 60U),
                       (unsigned long)(seconds % 60U));
  }

  return written >= 0 && (size_t)written < buffer_size;
}

static uint32_t integer_sqrt_u64(uint64_t value) {
  uint64_t result = 0;
  uint64_t bit = (uint64_t)1 << 62;

  while (bit > value) {
    bit >>= 2;
  }

  while (bit != 0) {
    if (value >= result + bit) {
      value -= result + bit;
      result = (result >> 1) + bit;
    } else {
      result >>= 1;
    }
    bit >>= 2;
  }
  return (uint32_t)result;
}

FitnessHrvResult fitness_hrv_calculate_rmssd(const uint16_t *ppi_ms,
                                             size_t count) {
  FitnessHrvResult result;
  memset(&result, 0, sizeof(result));
  if (!ppi_ms || count == 0) {
    return result;
  }

  uint64_t coverage_ms = 0;
  uint64_t squared_difference_sum = 0;
  uint16_t previous = 0;
  bool have_previous = false;

  for (size_t i = 0; i < count; ++i) {
    const uint16_t current = ppi_ms[i];
    if (current < FITNESS_HRV_PPI_MIN_MS ||
        current > FITNESS_HRV_PPI_MAX_MS) {
      ++result.rejected_intervals;
      have_previous = false;
      continue;
    }

    ++result.valid_intervals;
    coverage_ms += current;

    if (have_previous) {
      const uint32_t difference = current > previous
                                      ? (uint32_t)(current - previous)
                                      : (uint32_t)(previous - current);
      if (difference * 100U > (uint32_t)previous * 20U) {
        ++result.discontinuities;
      } else {
        squared_difference_sum += (uint64_t)difference * difference;
        ++result.valid_pairs;
      }
    }

    previous = current;
    have_previous = true;
  }

  result.accepted_coverage_ms =
      coverage_ms > UINT32_MAX ? UINT32_MAX : (uint32_t)coverage_ms;
  if (coverage_ms < FITNESS_HRV_MIN_COVERAGE_MS ||
      result.valid_pairs < FITNESS_HRV_MIN_PAIRS) {
    return result;
  }

  result.valid = true;
  result.rmssd_ms =
      integer_sqrt_u64(squared_difference_sum / result.valid_pairs);
  return result;
}

bool fitness_median_u32(const uint32_t *values, size_t count,
                        uint32_t *out_median) {
  if (!values || !out_median || count == 0 ||
      count > FITNESS_HRV_BASELINE_MAX) {
    return false;
  }

  uint32_t sorted[FITNESS_HRV_BASELINE_MAX];
  memcpy(sorted, values, count * sizeof(*values));

  for (size_t i = 1; i < count; ++i) {
    const uint32_t value = sorted[i];
    size_t position = i;
    while (position > 0 && sorted[position - 1] > value) {
      sorted[position] = sorted[position - 1];
      --position;
    }
    sorted[position] = value;
  }

  if ((count & 1U) != 0) {
    *out_median = sorted[count / 2];
  } else {
    const uint64_t middle_sum =
        (uint64_t)sorted[count / 2 - 1] + sorted[count / 2];
    *out_median = (uint32_t)(middle_sum / 2U);
  }
  return true;
}
