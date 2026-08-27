#ifndef GYM_ZONES_WORKOUT_LOGIC_H
#define GYM_ZONES_WORKOUT_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#define WORKOUT_AUTO_FINISH_GRACE_SECONDS 600
#define WORKOUT_REST_EXTENSION_SECONDS 30
#define WORKOUT_GRAPH_SLOT_COUNT 60
#define WORKOUT_HRV_WAIT_SECONDS 30
#define WORKOUT_HRV_CAPTURE_SECONDS 60

/*
 * Pure workout state helpers. All timestamps are caller-provided UTC seconds;
 * this module owns no clocks, persistence, Pebble services, or UI state.
 */

typedef struct {
  bool active;
  bool closed;
  int64_t started_at;
  int64_t closed_at;
  int64_t auto_finish_deadline;
} WorkoutSessionLifecycle;

typedef enum {
  WORKOUT_REOPEN_NO_ACTIVE_SESSION = 0,
  WORKOUT_REOPEN_ALREADY_OPEN,
  WORKOUT_REOPEN_RESUMED,
  WORKOUT_REOPEN_AUTO_FINISHED,
} WorkoutReopenResult;

void workout_session_lifecycle_init(WorkoutSessionLifecycle *session);
bool workout_session_start(WorkoutSessionLifecycle *session,
                           int64_t started_at);

/*
 * Starts one grace window. Repeated close notifications while already closed
 * do not move its deadline. Returns zero if there is no active session.
 */
int64_t workout_session_close(WorkoutSessionLifecycle *session,
                              int64_t closed_at);

/*
 * Reopening strictly before the deadline resumes the session and clears the
 * grace window. Reopening at or after it auto-finishes at the old deadline,
 * never at the later reopen time. out_finished_at may be NULL.
 */
WorkoutReopenResult workout_session_reopen(WorkoutSessionLifecycle *session,
                                           int64_t reopened_at,
                                           int64_t *out_finished_at);

typedef struct {
  bool active;
  bool alert_delivered;
  int64_t deadline;
  uint16_t generation;
} WorkoutRestLifecycle;

void workout_rest_lifecycle_init(WorkoutRestLifecycle *rest);

/* Starts/restarts rest and returns the non-zero cookie for its wakeup. */
uint16_t workout_rest_start(WorkoutRestLifecycle *rest, int64_t now,
                            uint32_t duration_seconds);

/*
 * Adds time from max(now, old deadline), so extending during overtime always
 * creates a future deadline. Rescheduling advances the generation/cookie.
 */
uint16_t workout_rest_extend(WorkoutRestLifecycle *rest, int64_t now,
                             uint32_t extension_seconds);

/* Cancels rest and invalidates every previously issued cookie. */
uint16_t workout_rest_cancel(WorkoutRestLifecycle *rest);

bool workout_rest_accepts_cookie(const WorkoutRestLifecycle *rest,
                                 uint16_t cookie);

/*
 * Shared one-shot gate for foreground timer and WakeupService paths. Exactly
 * one caller can claim the alert for the current generation at/after deadline.
 */
bool workout_rest_claim_alert(WorkoutRestLifecycle *rest, int64_t now,
                              uint16_t cookie);

/* Returns zero before the deadline or when rest is inactive. */
uint64_t workout_rest_overtime_seconds(const WorkoutRestLifecycle *rest,
                                       int64_t now);

typedef enum {
  WORKOUT_GRAPH_EMPTY = 0,
  WORKOUT_GRAPH_BACKFILL = 1,
  WORKOUT_GRAPH_LOCAL = 2,
} WorkoutGraphSource;

typedef struct {
  uint32_t minute;
  uint16_t bpm;
  WorkoutGraphSource source;
} WorkoutGraphSlot;

typedef struct {
  bool has_latest;
  uint32_t latest_minute;
  WorkoutGraphSlot slots[WORKOUT_GRAPH_SLOT_COUNT];
} WorkoutGraphRing;

void workout_graph_init(WorkoutGraphRing *graph);

/* Advances the 60-minute window and leaves every skipped minute as a gap. */
bool workout_graph_advance_to(WorkoutGraphRing *graph, uint32_t minute);

/*
 * Stores a non-zero minute value in the current window. Local data supersedes
 * backfill; backfill can never replace an already local value.
 */
bool workout_graph_put(WorkoutGraphRing *graph, uint32_t minute, uint16_t bpm,
                       WorkoutGraphSource source);

bool workout_graph_get(const WorkoutGraphRing *graph, uint32_t minute,
                       uint16_t *out_bpm, WorkoutGraphSource *out_source);

typedef enum {
  WORKOUT_HRV_IDLE = 0,
  WORKOUT_HRV_WAITING_FOR_PPI,
  WORKOUT_HRV_CAPTURING,
  WORKOUT_HRV_CAPTURE_READY,
  WORKOUT_HRV_COMPLETE,
  WORKOUT_HRV_LOW_SIGNAL,
  WORKOUT_HRV_NO_SIGNAL,
  WORKOUT_HRV_CANCELLED,
} WorkoutHrvPhase;

typedef enum {
  WORKOUT_HRV_TRANSITION_NONE = 0,
  WORKOUT_HRV_TRANSITION_FIRST_PPI,
  WORKOUT_HRV_TRANSITION_WAIT_TIMEOUT,
  WORKOUT_HRV_TRANSITION_CAPTURE_READY,
} WorkoutHrvTransition;

typedef struct {
  WorkoutHrvPhase phase;
  bool request_active;
  int64_t requested_at;
  int64_t wait_deadline;
  int64_t first_ppi_at;
  int64_t capture_deadline;
} WorkoutHrvLifecycle;

void workout_hrv_lifecycle_init(WorkoutHrvLifecycle *measurement);
bool workout_hrv_start(WorkoutHrvLifecycle *measurement, int64_t now);

/*
 * The caller invokes this only for a PPI that passed its value-level gate.
 * A first PPI at the exact wait deadline is accepted if processed before poll.
 */
WorkoutHrvTransition workout_hrv_on_usable_ppi(
    WorkoutHrvLifecycle *measurement, int64_t now);

WorkoutHrvTransition workout_hrv_poll(WorkoutHrvLifecycle *measurement,
                                      int64_t now);

/* Marks a capture-ready measurement as saved or rejected by quality gates. */
bool workout_hrv_finish(WorkoutHrvLifecycle *measurement, bool quality_ok);

/* Back, screen exit, and app close all use this cleanup path while measuring. */
bool workout_hrv_cancel(WorkoutHrvLifecycle *measurement);

#endif
