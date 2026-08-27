#include "workout_logic.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void test_session_close_reopen_and_auto_finish(void) {
  WorkoutSessionLifecycle session;
  workout_session_lifecycle_init(&session);
  assert(!session.active);
  assert(workout_session_close(&session, 1000) == 0);

  assert(workout_session_start(&session, 100));
  assert(!workout_session_start(&session, 200));
  assert(workout_session_close(&session, 1000) == 1600);
  /* Duplicate lifecycle notifications must not extend the grace period. */
  assert(workout_session_close(&session, 1200) == 1600);

  int64_t finished_at = -1;
  assert(workout_session_reopen(&session, 1599, &finished_at) ==
         WORKOUT_REOPEN_RESUMED);
  assert(finished_at == 0);
  assert(session.active);
  assert(!session.closed);
  assert(session.auto_finish_deadline == 0);

  /* A later real close owns a fresh ten-minute window. */
  assert(workout_session_close(&session, 2000) == 2600);
  assert(workout_session_reopen(&session, 2600, &finished_at) ==
         WORKOUT_REOPEN_AUTO_FINISHED);
  assert(finished_at == 2600);
  assert(!session.active);
  assert(workout_session_reopen(&session, 3000, &finished_at) ==
         WORKOUT_REOPEN_NO_ACTIVE_SESSION);

  workout_session_lifecycle_init(&session);
  assert(workout_session_start(&session, 5000));
  assert(workout_session_reopen(&session, 5001, NULL) ==
         WORKOUT_REOPEN_ALREADY_OPEN);
  assert(workout_session_close(&session, INT64_MAX - 100) == INT64_MAX);
  assert(workout_session_reopen(&session, INT64_MAX, &finished_at) ==
         WORKOUT_REOPEN_AUTO_FINISHED);
  assert(finished_at == INT64_MAX);
}

static void test_rest_generation_overtime_and_one_shot_alert(void) {
  WorkoutRestLifecycle rest;
  workout_rest_lifecycle_init(&rest);
  assert(workout_rest_extend(&rest, 100, 30) == 0);

  const uint16_t first = workout_rest_start(&rest, 1000, 120);
  assert(first != 0);
  assert(rest.deadline == 1120);
  assert(workout_rest_accepts_cookie(&rest, first));
  assert(!workout_rest_claim_alert(&rest, 1119, first));

  /* Before overtime, +30 remains anchored to the existing deadline. */
  const uint16_t rescheduled = workout_rest_extend(&rest, 1050, 30);
  assert(rescheduled != first);
  assert(rest.deadline == 1150);
  assert(!workout_rest_accepts_cookie(&rest, first));
  assert(!workout_rest_claim_alert(&rest, 1150, first));

  /* Foreground wins the race; Wakeup with the same cookie is suppressed. */
  assert(workout_rest_claim_alert(&rest, 1150, rescheduled));
  assert(!workout_rest_claim_alert(&rest, 1150, rescheduled));
  assert(!workout_rest_claim_alert(&rest, 1200, rescheduled));
  assert(workout_rest_overtime_seconds(&rest, 1150) == 0);
  assert(workout_rest_overtime_seconds(&rest, 1157) == 7);

  /* In overtime, +30 is anchored at now rather than the expired deadline. */
  const uint16_t second = workout_rest_extend(&rest, 1200, 30);
  assert(second != rescheduled);
  assert(rest.deadline == 1230);
  assert(!rest.alert_delivered);
  assert(!workout_rest_accepts_cookie(&rest, rescheduled));
  assert(!workout_rest_claim_alert(&rest, 1230, rescheduled));
  assert(workout_rest_claim_alert(&rest, 1230, second));

  const uint16_t cancelled = workout_rest_cancel(&rest);
  assert(cancelled == second);
  assert(!rest.active);
  assert(!workout_rest_accepts_cookie(&rest, second));
  assert(!workout_rest_claim_alert(&rest, 9999, second));

  rest.generation = UINT16_MAX;
  assert(workout_rest_start(&rest, 0, 1) == 1);
}

static void test_graph_window_gaps_and_source_priority(void) {
  WorkoutGraphRing graph;
  workout_graph_init(&graph);
  assert(workout_graph_advance_to(&graph, 1000));

  uint16_t bpm = 777;
  WorkoutGraphSource source = WORKOUT_GRAPH_LOCAL;
  assert(!workout_graph_get(&graph, 1000, &bpm, &source));
  assert(bpm == 777);

  assert(workout_graph_put(&graph, 950, 101, WORKOUT_GRAPH_BACKFILL));
  assert(workout_graph_get(&graph, 950, &bpm, &source));
  assert(bpm == 101);
  assert(source == WORKOUT_GRAPH_BACKFILL);

  assert(workout_graph_put(&graph, 950, 130, WORKOUT_GRAPH_LOCAL));
  assert(!workout_graph_put(&graph, 950, 99, WORKOUT_GRAPH_BACKFILL));
  assert(workout_graph_get(&graph, 950, &bpm, &source));
  assert(bpm == 130);
  assert(source == WORKOUT_GRAPH_LOCAL);

  assert(!workout_graph_put(&graph, 940, 120, WORKOUT_GRAPH_LOCAL));
  assert(!workout_graph_put(&graph, 999, 0, WORKOUT_GRAPH_LOCAL));
  assert(!workout_graph_put(&graph, 999, 120, WORKOUT_GRAPH_EMPTY));

  /* Advancing leaves skipped 1001..1004 empty. */
  assert(workout_graph_advance_to(&graph, 1005));
  for (uint32_t minute = 1001; minute <= 1005; ++minute) {
    assert(!workout_graph_get(&graph, minute, NULL, NULL));
  }
  assert(workout_graph_get(&graph, 950, &bpm, NULL));

  /* The 60-slot window scrolls and modulo reuse cannot expose stale data. */
  assert(workout_graph_put(&graph, 1005, 150, WORKOUT_GRAPH_LOCAL));
  assert(workout_graph_advance_to(&graph, 1010));
  assert(!workout_graph_get(&graph, 950, NULL, NULL));
  assert(workout_graph_get(&graph, 1005, &bpm, NULL));
  assert(bpm == 150);

  assert(workout_graph_advance_to(&graph, 1070));
  assert(!workout_graph_get(&graph, 1005, NULL, NULL));
  assert(!workout_graph_advance_to(&graph, 1069));

  WorkoutGraphRing auto_advanced;
  workout_graph_init(&auto_advanced);
  assert(workout_graph_put(&auto_advanced, 42, 88, WORKOUT_GRAPH_LOCAL));
  assert(auto_advanced.latest_minute == 42);
  assert(workout_graph_get(&auto_advanced, 42, &bpm, NULL));
  assert(bpm == 88);
}

static void test_hrv_wait_capture_finish_and_cleanup(void) {
  WorkoutHrvLifecycle hrv;
  workout_hrv_lifecycle_init(&hrv);
  assert(hrv.phase == WORKOUT_HRV_IDLE);
  assert(!hrv.request_active);

  assert(workout_hrv_start(&hrv, 100));
  assert(!workout_hrv_start(&hrv, 101));
  assert(hrv.phase == WORKOUT_HRV_WAITING_FOR_PPI);
  assert(hrv.request_active);
  assert(hrv.wait_deadline == 130);
  assert(workout_hrv_poll(&hrv, 129) == WORKOUT_HRV_TRANSITION_NONE);
  assert(workout_hrv_on_usable_ppi(&hrv, 125) ==
         WORKOUT_HRV_TRANSITION_FIRST_PPI);
  assert(hrv.phase == WORKOUT_HRV_CAPTURING);
  assert(hrv.capture_deadline == 185);
  assert(workout_hrv_poll(&hrv, 184) == WORKOUT_HRV_TRANSITION_NONE);
  assert(workout_hrv_poll(&hrv, 185) ==
         WORKOUT_HRV_TRANSITION_CAPTURE_READY);
  assert(hrv.phase == WORKOUT_HRV_CAPTURE_READY);
  assert(!hrv.request_active);
  assert(!workout_hrv_start(&hrv, 186));
  assert(workout_hrv_finish(&hrv, true));
  assert(hrv.phase == WORKOUT_HRV_COMPLETE);
  assert(!hrv.request_active);

  /* Waiting exactly 30 seconds without a sample becomes NO SIGNAL. */
  assert(workout_hrv_start(&hrv, 200));
  assert(workout_hrv_poll(&hrv, 230) ==
         WORKOUT_HRV_TRANSITION_WAIT_TIMEOUT);
  assert(hrv.phase == WORKOUT_HRV_NO_SIGNAL);
  assert(!hrv.request_active);

  /* If dispatched first, a usable sample on the boundary starts capture. */
  assert(workout_hrv_start(&hrv, 300));
  assert(workout_hrv_on_usable_ppi(&hrv, 330) ==
         WORKOUT_HRV_TRANSITION_FIRST_PPI);
  assert(hrv.request_active);
  assert(workout_hrv_cancel(&hrv));
  assert(hrv.phase == WORKOUT_HRV_CANCELLED);
  assert(!hrv.request_active);
  assert(!workout_hrv_cancel(&hrv));

  /* A late first sample closes the request through the timeout path. */
  assert(workout_hrv_start(&hrv, 400));
  assert(workout_hrv_on_usable_ppi(&hrv, 431) ==
         WORKOUT_HRV_TRANSITION_WAIT_TIMEOUT);
  assert(!hrv.request_active);

  assert(workout_hrv_start(&hrv, 500));
  assert(workout_hrv_on_usable_ppi(&hrv, 501) ==
         WORKOUT_HRV_TRANSITION_FIRST_PPI);
  assert(workout_hrv_on_usable_ppi(&hrv, 561) ==
         WORKOUT_HRV_TRANSITION_CAPTURE_READY);
  assert(!hrv.request_active);
  assert(workout_hrv_finish(&hrv, false));
  assert(hrv.phase == WORKOUT_HRV_LOW_SIGNAL);
  assert(!hrv.request_active);
  assert(!workout_hrv_finish(&hrv, true));
}

int main(void) {
  test_session_close_reopen_and_auto_finish();
  test_rest_generation_overtime_and_one_shot_alert();
  test_graph_window_gaps_and_source_priority();
  test_hrv_wait_capture_finish_and_cleanup();
  puts("workout_logic: all tests passed");
  return 0;
}
