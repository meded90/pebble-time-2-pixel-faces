#include "fitness_math.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static FitnessZoneBounds manual_bounds(void) {
  const FitnessZoneConfig config = {
      .mode = FITNESS_ZONE_MODE_MANUAL,
      .manual_lower = {100, 120, 140, 160, 180},
  };
  FitnessZoneBounds bounds = {{0}};
  assert(fitness_zone_bounds_build(&config, &bounds));
  return bounds;
}

static void test_bpm_filter(void) {
  FitnessBpmFilter filter;
  fitness_bpm_filter_reset(&filter);
  assert(filter.count == 0);
  assert(filter.next_index == 0);

  assert(fitness_bpm_filter_update(NULL, 120) == 0);
  assert(fitness_bpm_filter_update(&filter, 0) == 0);
  assert(filter.count == 0);

  assert(fitness_bpm_filter_update(&filter, 100) == 100);
  assert(fitness_bpm_filter_update(&filter, 141) == 121);
  assert(fitness_bpm_filter_update(&filter, 110) == 110);
  assert(fitness_bpm_filter_update(&filter, 200) == 141);
  assert(fitness_bpm_filter_update(&filter, 90) == 110);

  fitness_bpm_filter_reset(&filter);
  assert(fitness_bpm_filter_update(&filter, 1) == 1);
  assert(fitness_bpm_filter_update(&filter, UINT16_MAX) == 32768);
  fitness_bpm_filter_reset(NULL);
}

static void test_zone_bounds(void) {
  FitnessZoneBounds bounds = {{11, 22, 33, 44, 55}};
  FitnessZoneConfig config = {
      .mode = FITNESS_ZONE_MODE_MAX_HR,
      .max_hr = 201,
  };
  assert(fitness_zone_bounds_build(&config, &bounds));
  const uint16_t expected_max[] = {101, 121, 141, 161, 181};
  assert(memcmp(bounds.lower, expected_max, sizeof(expected_max)) == 0);

  config.max_hr = 99;
  assert(!fitness_zone_bounds_build(&config, &bounds));
  assert(memcmp(bounds.lower, expected_max, sizeof(expected_max)) == 0);
  config.max_hr = 241;
  assert(!fitness_zone_bounds_build(&config, &bounds));

  config.mode = FITNESS_ZONE_MODE_AGE;
  config.age = 40;
  config.age_formula = FITNESS_AGE_FORMULA_208;
  assert(fitness_zone_bounds_build(&config, &bounds));
  const uint16_t expected_208[] = {90, 108, 126, 144, 162};
  assert(memcmp(bounds.lower, expected_208, sizeof(expected_208)) == 0);

  config.age_formula = FITNESS_AGE_FORMULA_220;
  assert(fitness_zone_bounds_build(&config, &bounds));
  assert(memcmp(bounds.lower, expected_208, sizeof(expected_208)) == 0);

  config.age = 30;
  config.age_formula = FITNESS_AGE_FORMULA_208;
  assert(fitness_zone_bounds_build(&config, &bounds));
  const uint16_t expected_age_30_208[] = {94, 113, 131, 150, 169};
  assert(memcmp(bounds.lower, expected_age_30_208,
                sizeof(expected_age_30_208)) == 0);

  config.age_formula = FITNESS_AGE_FORMULA_220;
  assert(fitness_zone_bounds_build(&config, &bounds));
  const uint16_t expected_age_30_220[] = {95, 114, 133, 152, 171};
  assert(memcmp(bounds.lower, expected_age_30_220,
                sizeof(expected_age_30_220)) == 0);

  config.age = 13;
  assert(!fitness_zone_bounds_build(&config, &bounds));
  config.age = 101;
  assert(!fitness_zone_bounds_build(&config, &bounds));
  config.age = 40;
  config.age_formula = (FitnessAgeFormula)99;
  assert(!fitness_zone_bounds_build(&config, &bounds));

  config.mode = FITNESS_ZONE_MODE_MANUAL;
  const uint16_t valid_manual[] = {80, 110, 135, 155, 175};
  memcpy(config.manual_lower, valid_manual, sizeof(valid_manual));
  assert(fitness_zone_bounds_build(&config, &bounds));
  assert(memcmp(bounds.lower, valid_manual, sizeof(valid_manual)) == 0);

  config.manual_lower[2] = config.manual_lower[1];
  assert(!fitness_zone_bounds_build(&config, &bounds));
  config.manual_lower[0] = 0;
  assert(!fitness_zone_bounds_build(&config, &bounds));
}

static void test_zone_classification(void) {
  const FitnessZoneBounds bounds = manual_bounds();
  assert(fitness_classify_zone(&bounds, 0) == 0);
  assert(fitness_classify_zone(&bounds, 99) == 0);
  assert(fitness_classify_zone(&bounds, 100) == 1);
  assert(fitness_classify_zone(&bounds, 119) == 1);
  assert(fitness_classify_zone(&bounds, 120) == 2);
  assert(fitness_classify_zone(&bounds, 179) == 4);
  assert(fitness_classify_zone(&bounds, 180) == 5);
  assert(fitness_classify_zone(&bounds, UINT16_MAX) == 5);

  FitnessZoneBounds invalid = {{100, 120, 120, 160, 180}};
  assert(fitness_classify_zone(&invalid, 150) == 0);
}

static void test_zone_stabilizer(void) {
  const FitnessZoneBounds bounds = manual_bounds();
  FitnessZoneStabilizer stabilizer;
  fitness_zone_stabilizer_reset(&stabilizer);

  assert(fitness_zone_stabilizer_update(&stabilizer, &bounds, 130) == 0);
  assert(fitness_zone_stabilizer_update(&stabilizer, &bounds, 130) == 2);

  /* The 140 boundary is held until +2 BPM, then needs two readings. */
  assert(fitness_zone_stabilizer_update(&stabilizer, &bounds, 140) == 2);
  assert(fitness_zone_stabilizer_update(&stabilizer, &bounds, 141) == 2);
  assert(fitness_zone_stabilizer_update(&stabilizer, &bounds, 142) == 2);
  assert(fitness_zone_stabilizer_update(&stabilizer, &bounds, 142) == 3);

  /* Z3 is held down to 139, and leaves at 138 after two readings. */
  assert(fitness_zone_stabilizer_update(&stabilizer, &bounds, 139) == 3);
  assert(fitness_zone_stabilizer_update(&stabilizer, &bounds, 138) == 3);
  assert(fitness_zone_stabilizer_update(&stabilizer, &bounds, 138) == 2);

  /* A changed candidate does not count as a consecutive confirmation. */
  assert(fitness_zone_stabilizer_update(&stabilizer, &bounds, 162) == 2);
  assert(fitness_zone_stabilizer_update(&stabilizer, &bounds, 182) == 2);
  assert(fitness_zone_stabilizer_update(&stabilizer, &bounds, 182) == 5);

  assert(fitness_zone_stabilizer_update(&stabilizer, &bounds, 98) == 5);
  assert(fitness_zone_stabilizer_update(&stabilizer, &bounds, 98) == 0);

  fitness_zone_stabilizer_reset(&stabilizer);
  assert(stabilizer.stable_zone == 0);
  assert(stabilizer.candidate_readings == 0);
}

static void test_duration_formatting(void) {
  char buffer[24];
  assert(fitness_format_duration(0, buffer, sizeof(buffer)));
  assert(strcmp(buffer, "00:00") == 0);
  assert(fitness_format_duration(5U * 60U + 7U, buffer, sizeof(buffer)));
  assert(strcmp(buffer, "05:07") == 0);
  assert(fitness_format_duration(47U * 60U, buffer, sizeof(buffer)));
  assert(strcmp(buffer, "47:00") == 0);
  assert(fitness_format_duration(3599, buffer, sizeof(buffer)));
  assert(strcmp(buffer, "59:59") == 0);
  assert(fitness_format_duration(3600, buffer, sizeof(buffer)));
  assert(strcmp(buffer, "1:00:00") == 0);
  assert(fitness_format_duration(6439, buffer, sizeof(buffer)));
  assert(strcmp(buffer, "1:47:19") == 0);

  char short_buffer[4];
  assert(!fitness_format_duration(3600, short_buffer, sizeof(short_buffer)));
  assert(!fitness_format_duration(0, NULL, 0));
}

static void test_hrv_quality_and_rmssd(void) {
  uint16_t steady[49];
  for (size_t i = 0; i < 49; ++i) {
    steady[i] = 1000;
  }
  FitnessHrvResult result = fitness_hrv_calculate_rmssd(steady, 49);
  assert(result.valid);
  assert(result.rmssd_ms == 0);
  assert(result.accepted_coverage_ms == 49000);
  assert(result.valid_pairs == 48);
  assert(result.rejected_intervals == 0);

  uint16_t alternating[49];
  for (size_t i = 0; i < 49; ++i) {
    alternating[i] = (i & 1U) == 0 ? 1000 : 1100;
  }
  result = fitness_hrv_calculate_rmssd(alternating, 49);
  assert(result.valid);
  assert(result.rmssd_ms == 100);
  assert(result.valid_pairs == 48);

  uint16_t discontinuous[49];
  for (size_t i = 0; i < 49; ++i) {
    discontinuous[i] = i < 25 ? 1000 : 1500;
  }
  result = fitness_hrv_calculate_rmssd(discontinuous, 49);
  assert(result.valid);
  assert(result.discontinuities == 1);
  assert(result.valid_pairs == 47);
  assert(result.rmssd_ms == 0);

  uint16_t with_invalid[51];
  for (size_t i = 0; i < 51; ++i) {
    with_invalid[i] = 1000;
  }
  with_invalid[25] = 0;
  with_invalid[26] = 250;
  result = fitness_hrv_calculate_rmssd(with_invalid, 51);
  assert(result.valid);
  assert(result.rejected_intervals == 2);
  assert(result.valid_intervals == 49);
  assert(result.valid_pairs == 47);

  result = fitness_hrv_calculate_rmssd(steady, 48);
  assert(result.valid);
  assert(result.accepted_coverage_ms == 48000);
  assert(result.valid_pairs == 47);

  result = fitness_hrv_calculate_rmssd(steady, 47);
  assert(!result.valid);
  assert(result.accepted_coverage_ms == 47000);

  uint16_t too_few_pairs[65];
  for (size_t i = 0; i < 65; ++i) {
    too_few_pairs[i] = (i & 1U) == 0 ? 500 : 1000;
  }
  result = fitness_hrv_calculate_rmssd(too_few_pairs, 65);
  assert(!result.valid);
  assert(result.accepted_coverage_ms >= 48000);
  assert(result.valid_pairs == 0);
  assert(result.discontinuities == 64);

  result = fitness_hrv_calculate_rmssd(NULL, 0);
  assert(!result.valid);
}

static void test_median(void) {
  uint32_t median = 0;
  const uint32_t odd[] = {38, 44, 31, 42, 40, 55, 39};
  assert(fitness_median_u32(odd, 7, &median));
  assert(median == 40);

  const uint32_t even[] = {UINT32_MAX, UINT32_MAX - 2};
  assert(fitness_median_u32(even, 2, &median));
  assert(median == UINT32_MAX - 1);

  assert(!fitness_median_u32(odd, 0, &median));
  assert(!fitness_median_u32(odd, 8, &median));
  assert(!fitness_median_u32(NULL, 1, &median));
  assert(!fitness_median_u32(odd, 1, NULL));
}

int main(void) {
  test_bpm_filter();
  test_zone_bounds();
  test_zone_classification();
  test_zone_stabilizer();
  test_duration_formatting();
  test_hrv_quality_and_rmssd();
  test_median();
  puts("fitness_math: all tests passed");
  return 0;
}
