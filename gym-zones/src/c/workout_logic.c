#include "workout_logic.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static int64_t saturating_add_seconds(int64_t value, uint32_t seconds) {
  if (value > INT64_MAX - (int64_t)seconds) {
    return INT64_MAX;
  }
  return value + (int64_t)seconds;
}

static uint16_t next_generation(uint16_t generation) {
  return generation == UINT16_MAX ? 1U : (uint16_t)(generation + 1U);
}

void workout_session_lifecycle_init(WorkoutSessionLifecycle *session) {
  if (session != NULL) {
    memset(session, 0, sizeof(*session));
  }
}

bool workout_session_start(WorkoutSessionLifecycle *session,
                           int64_t started_at) {
  if (session == NULL || session->active) {
    return false;
  }

  memset(session, 0, sizeof(*session));
  session->active = true;
  session->started_at = started_at;
  return true;
}

int64_t workout_session_close(WorkoutSessionLifecycle *session,
                              int64_t closed_at) {
  if (session == NULL || !session->active) {
    return 0;
  }
  if (session->closed) {
    return session->auto_finish_deadline;
  }

  session->closed = true;
  session->closed_at = closed_at;
  session->auto_finish_deadline = saturating_add_seconds(
      closed_at, WORKOUT_AUTO_FINISH_GRACE_SECONDS);
  return session->auto_finish_deadline;
}

WorkoutReopenResult workout_session_reopen(WorkoutSessionLifecycle *session,
                                           int64_t reopened_at,
                                           int64_t *out_finished_at) {
  if (out_finished_at != NULL) {
    *out_finished_at = 0;
  }
  if (session == NULL || !session->active) {
    return WORKOUT_REOPEN_NO_ACTIVE_SESSION;
  }
  if (!session->closed) {
    return WORKOUT_REOPEN_ALREADY_OPEN;
  }

  if (reopened_at < session->auto_finish_deadline) {
    session->closed = false;
    session->closed_at = 0;
    session->auto_finish_deadline = 0;
    return WORKOUT_REOPEN_RESUMED;
  }

  if (out_finished_at != NULL) {
    *out_finished_at = session->auto_finish_deadline;
  }
  session->active = false;
  session->closed = false;
  return WORKOUT_REOPEN_AUTO_FINISHED;
}

void workout_rest_lifecycle_init(WorkoutRestLifecycle *rest) {
  if (rest != NULL) {
    memset(rest, 0, sizeof(*rest));
  }
}

uint16_t workout_rest_start(WorkoutRestLifecycle *rest, int64_t now,
                            uint32_t duration_seconds) {
  if (rest == NULL || duration_seconds == 0U) {
    return 0;
  }

  rest->generation = next_generation(rest->generation);
  rest->active = true;
  rest->alert_delivered = false;
  rest->deadline = saturating_add_seconds(now, duration_seconds);
  return rest->generation;
}

uint16_t workout_rest_extend(WorkoutRestLifecycle *rest, int64_t now,
                             uint32_t extension_seconds) {
  if (rest == NULL || !rest->active || extension_seconds == 0U) {
    return 0;
  }

  const int64_t anchor = now > rest->deadline ? now : rest->deadline;
  rest->generation = next_generation(rest->generation);
  rest->deadline = saturating_add_seconds(anchor, extension_seconds);
  rest->alert_delivered = false;
  return rest->generation;
}

uint16_t workout_rest_cancel(WorkoutRestLifecycle *rest) {
  if (rest == NULL) {
    return 0;
  }

  rest->active = false;
  rest->alert_delivered = false;
  rest->deadline = 0;
  return rest->generation;
}

bool workout_rest_accepts_cookie(const WorkoutRestLifecycle *rest,
                                 uint16_t cookie) {
  return rest != NULL && rest->active && cookie != 0U &&
         cookie == rest->generation;
}

bool workout_rest_claim_alert(WorkoutRestLifecycle *rest, int64_t now,
                              uint16_t cookie) {
  if (!workout_rest_accepts_cookie(rest, cookie) || rest->alert_delivered ||
      now < rest->deadline) {
    return false;
  }

  rest->alert_delivered = true;
  return true;
}

uint64_t workout_rest_overtime_seconds(const WorkoutRestLifecycle *rest,
                                       int64_t now) {
  if (rest == NULL || !rest->active || now <= rest->deadline) {
    return 0U;
  }
  return (uint64_t)now - (uint64_t)rest->deadline;
}

void workout_graph_init(WorkoutGraphRing *graph) {
  if (graph != NULL) {
    memset(graph, 0, sizeof(*graph));
  }
}

static void graph_clear_slot(WorkoutGraphRing *graph, uint32_t minute) {
  WorkoutGraphSlot *slot =
      &graph->slots[minute % (uint32_t)WORKOUT_GRAPH_SLOT_COUNT];
  memset(slot, 0, sizeof(*slot));
}

bool workout_graph_advance_to(WorkoutGraphRing *graph, uint32_t minute) {
  if (graph == NULL) {
    return false;
  }
  if (!graph->has_latest) {
    graph->has_latest = true;
    graph->latest_minute = minute;
    return true;
  }
  if (minute < graph->latest_minute) {
    return false;
  }

  const uint32_t distance = minute - graph->latest_minute;
  if (distance >= (uint32_t)WORKOUT_GRAPH_SLOT_COUNT) {
    memset(graph->slots, 0, sizeof(graph->slots));
  } else {
    for (uint32_t offset = 1U; offset <= distance; ++offset) {
      graph_clear_slot(graph, graph->latest_minute + offset);
    }
  }
  graph->latest_minute = minute;
  return true;
}

bool workout_graph_put(WorkoutGraphRing *graph, uint32_t minute, uint16_t bpm,
                       WorkoutGraphSource source) {
  if (graph == NULL || bpm == 0U ||
      (source != WORKOUT_GRAPH_BACKFILL && source != WORKOUT_GRAPH_LOCAL)) {
    return false;
  }
  if (!graph->has_latest || minute > graph->latest_minute) {
    if (!workout_graph_advance_to(graph, minute)) {
      return false;
    }
  }
  if (minute > graph->latest_minute ||
      graph->latest_minute - minute >=
          (uint32_t)WORKOUT_GRAPH_SLOT_COUNT) {
    return false;
  }

  WorkoutGraphSlot *slot =
      &graph->slots[minute % (uint32_t)WORKOUT_GRAPH_SLOT_COUNT];
  if (slot->source != WORKOUT_GRAPH_EMPTY && slot->minute != minute) {
    memset(slot, 0, sizeof(*slot));
  }
  if (slot->source == WORKOUT_GRAPH_LOCAL &&
      source == WORKOUT_GRAPH_BACKFILL) {
    return false;
  }

  slot->minute = minute;
  slot->bpm = bpm;
  slot->source = source;
  return true;
}

bool workout_graph_get(const WorkoutGraphRing *graph, uint32_t minute,
                       uint16_t *out_bpm, WorkoutGraphSource *out_source) {
  if (graph == NULL || !graph->has_latest || minute > graph->latest_minute ||
      graph->latest_minute - minute >=
          (uint32_t)WORKOUT_GRAPH_SLOT_COUNT) {
    return false;
  }

  const WorkoutGraphSlot *slot =
      &graph->slots[minute % (uint32_t)WORKOUT_GRAPH_SLOT_COUNT];
  if (slot->source == WORKOUT_GRAPH_EMPTY || slot->minute != minute ||
      slot->bpm == 0U) {
    return false;
  }
  if (out_bpm != NULL) {
    *out_bpm = slot->bpm;
  }
  if (out_source != NULL) {
    *out_source = slot->source;
  }
  return true;
}

void workout_hrv_lifecycle_init(WorkoutHrvLifecycle *measurement) {
  if (measurement != NULL) {
    memset(measurement, 0, sizeof(*measurement));
  }
}

bool workout_hrv_start(WorkoutHrvLifecycle *measurement, int64_t now) {
  if (measurement == NULL || measurement->request_active ||
      measurement->phase == WORKOUT_HRV_CAPTURE_READY) {
    return false;
  }

  memset(measurement, 0, sizeof(*measurement));
  measurement->phase = WORKOUT_HRV_WAITING_FOR_PPI;
  measurement->request_active = true;
  measurement->requested_at = now;
  measurement->wait_deadline =
      saturating_add_seconds(now, WORKOUT_HRV_WAIT_SECONDS);
  return true;
}

static WorkoutHrvTransition hrv_capture_ready(
    WorkoutHrvLifecycle *measurement) {
  measurement->phase = WORKOUT_HRV_CAPTURE_READY;
  measurement->request_active = false;
  return WORKOUT_HRV_TRANSITION_CAPTURE_READY;
}

WorkoutHrvTransition workout_hrv_on_usable_ppi(
    WorkoutHrvLifecycle *measurement, int64_t now) {
  if (measurement == NULL || !measurement->request_active) {
    return WORKOUT_HRV_TRANSITION_NONE;
  }

  if (measurement->phase == WORKOUT_HRV_WAITING_FOR_PPI) {
    if (now > measurement->wait_deadline) {
      measurement->phase = WORKOUT_HRV_NO_SIGNAL;
      measurement->request_active = false;
      return WORKOUT_HRV_TRANSITION_WAIT_TIMEOUT;
    }
    measurement->phase = WORKOUT_HRV_CAPTURING;
    measurement->first_ppi_at = now;
    measurement->capture_deadline =
        saturating_add_seconds(now, WORKOUT_HRV_CAPTURE_SECONDS);
    return WORKOUT_HRV_TRANSITION_FIRST_PPI;
  }

  if (measurement->phase == WORKOUT_HRV_CAPTURING &&
      now >= measurement->capture_deadline) {
    return hrv_capture_ready(measurement);
  }
  return WORKOUT_HRV_TRANSITION_NONE;
}

WorkoutHrvTransition workout_hrv_poll(WorkoutHrvLifecycle *measurement,
                                      int64_t now) {
  if (measurement == NULL || !measurement->request_active) {
    return WORKOUT_HRV_TRANSITION_NONE;
  }

  if (measurement->phase == WORKOUT_HRV_WAITING_FOR_PPI &&
      now >= measurement->wait_deadline) {
    measurement->phase = WORKOUT_HRV_NO_SIGNAL;
    measurement->request_active = false;
    return WORKOUT_HRV_TRANSITION_WAIT_TIMEOUT;
  }
  if (measurement->phase == WORKOUT_HRV_CAPTURING &&
      now >= measurement->capture_deadline) {
    return hrv_capture_ready(measurement);
  }
  return WORKOUT_HRV_TRANSITION_NONE;
}

bool workout_hrv_finish(WorkoutHrvLifecycle *measurement, bool quality_ok) {
  if (measurement == NULL ||
      measurement->phase != WORKOUT_HRV_CAPTURE_READY) {
    return false;
  }

  measurement->phase =
      quality_ok ? WORKOUT_HRV_COMPLETE : WORKOUT_HRV_LOW_SIGNAL;
  measurement->request_active = false;
  return true;
}

bool workout_hrv_cancel(WorkoutHrvLifecycle *measurement) {
  if (measurement == NULL ||
      (measurement->phase != WORKOUT_HRV_WAITING_FOR_PPI &&
       measurement->phase != WORKOUT_HRV_CAPTURING)) {
    return false;
  }

  measurement->phase = WORKOUT_HRV_CANCELLED;
  measurement->request_active = false;
  return true;
}
