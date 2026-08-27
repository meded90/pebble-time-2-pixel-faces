#ifndef GYM_ZONES_FITNESS_MATH_H
#define GYM_ZONES_FITNESS_MATH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FITNESS_ZONE_COUNT 5
#define FITNESS_HRV_BASELINE_MAX 7
#define FITNESS_BPM_FILTER_WINDOW 3

typedef struct {
  uint16_t samples[FITNESS_BPM_FILTER_WINDOW];
  uint8_t count;
  uint8_t next_index;
} FitnessBpmFilter;

void fitness_bpm_filter_reset(FitnessBpmFilter *filter);

/*
 * Adds one non-zero BPM sample and returns a deterministic filtered value.
 * One sample is returned unchanged, two samples use their rounded mean, and
 * three samples use the median. A zero sample or NULL filter returns zero and
 * does not mutate the filter.
 */
uint16_t fitness_bpm_filter_update(FitnessBpmFilter *filter, uint16_t bpm);

typedef enum {
  FITNESS_ZONE_MODE_MAX_HR = 0,
  FITNESS_ZONE_MODE_AGE = 1,
  FITNESS_ZONE_MODE_MANUAL = 2,
} FitnessZoneMode;

typedef enum {
  FITNESS_AGE_FORMULA_208 = 0,
  FITNESS_AGE_FORMULA_220 = 1,
} FitnessAgeFormula;

typedef struct {
  FitnessZoneMode mode;
  uint16_t max_hr;
  uint8_t age;
  FitnessAgeFormula age_formula;
  uint16_t manual_lower[FITNESS_ZONE_COUNT];
} FitnessZoneConfig;

/* Inclusive lower BPM boundary for zones 1 through 5. */
typedef struct {
  uint16_t lower[FITNESS_ZONE_COUNT];
} FitnessZoneBounds;

/*
 * Validates config and calculates zone bounds. On failure, out_bounds is left
 * unchanged. Max-HR modes use the inclusive ceilings at 50/60/70/80/90%.
 * The 208 - 0.7 * age formula is rounded to the nearest whole BPM first.
 */
bool fitness_zone_bounds_build(const FitnessZoneConfig *config,
                               FitnessZoneBounds *out_bounds);

/* Returns 0 below Z1 (and for bpm == 0 or invalid bounds), otherwise 1..5. */
uint8_t fitness_classify_zone(const FitnessZoneBounds *bounds, uint16_t bpm);

typedef struct {
  uint8_t stable_zone;
  uint8_t candidate_zone;
  uint8_t candidate_readings;
} FitnessZoneStabilizer;

void fitness_zone_stabilizer_reset(FitnessZoneStabilizer *stabilizer);

/*
 * Applies a 2 BPM hysteresis around the current zone and requires two
 * consecutive readings for every transition. Only call for a valid, fresh
 * reading; call reset when the HR signal becomes stale.
 */
uint8_t fitness_zone_stabilizer_update(FitnessZoneStabilizer *stabilizer,
                                       const FitnessZoneBounds *bounds,
                                       uint16_t bpm);

/* Formats MM:SS below one hour, then H:MM:SS. */
bool fitness_format_duration(uint32_t seconds, char *buffer,
                             size_t buffer_size);

typedef struct {
  bool valid;
  uint32_t rmssd_ms;
  uint32_t accepted_coverage_ms;
  uint32_t valid_intervals;
  uint32_t rejected_intervals;
  uint32_t valid_pairs;
  uint32_t discontinuities;
} FitnessHrvResult;

/*
 * Calculates whole-millisecond RMSSD from PPI samples. PPIs outside
 * 300..2000 ms are rejected. A change greater than 20% starts a new segment,
 * so no squared difference crosses a gap. Quality requires at least 48 s of
 * accepted PPI coverage and at least 30 adjacent pairs. Squares and their sum
 * are accumulated in 64 bits.
 */
FitnessHrvResult fitness_hrv_calculate_rmssd(const uint16_t *ppi_ms,
                                             size_t count);

/* Median of 1..7 values without modifying the input; even counts are averaged. */
bool fitness_median_u32(const uint32_t *values, size_t count,
                        uint32_t *out_median);

#endif
