# FreeRTOS task delay functions 

## vTaskDelay()

This creates a **relative delay** from when the function is called. If you call `vTaskDelay(100)`, the task will be blocked for 100 ticks from that moment, regardless of when it last ran.

**Key characteristic:** The timing drifts over time because it doesn't account for how long your task's code takes to execute.

**Example:**
```c
void vTask(void *pvParameters) {
    while(1) {
        // Do some work (takes 10 ticks)
        processData();
        
        // Delay for 100 ticks from NOW
        vTaskDelay(100);
        
        // Total period = 10 + 100 = 110 ticks
    }
}
```

## vTaskDelayUntil()

This creates an **absolute delay** to achieve a fixed periodic execution. It calculates the delay needed to wake up at a specific future time, compensating for execution time.

**Key characteristic:** Maintains precise periodic timing without drift, perfect for tasks that need consistent intervals.

**Example:**
```c
void vTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    while(1) {
        // Do some work (takes 10 ticks)
        processData();
        
        // Delay until 100 ticks after LAST wake time
        vTaskDelayUntil(&xLastWakeTime, 100);
        
        // Total period = exactly 100 ticks
    }
}
```

## When to use which?

- **Use vTaskDelay()** when you just need a simple pause between operations, and precise timing isn't critical (e.g., "wait a bit before retrying")

- **Use vTaskDelayUntil()** when you need periodic execution at fixed intervals (e.g., sampling sensors every 100ms, updating displays, control loops) where timing accuracy matters and you want to prevent drift

The difference becomes especially important in long-running tasks where even small execution time variations would accumulate into significant timing drift with `vTaskDelay()`.

---

## Core difference (one sentence)

> `vTaskDelayUntil()` creates a *strictly periodic* task with bounded jitter,
> while `vTaskDelay()` creates a *relative delay* that accumulates drift and jitter.**

---

## What each API actually means

### `vTaskDelayUntil()` → **absolute time**

* Delays task **until a specific tick count**
* Uses a reference time (`xLastWakeTime`)
* Keeps period constant even if execution time varies

Mathematically:

```
next_wakeup = last_wakeup + period
```

---

### `vTaskDelay()` → **relative time**

* Delays task **relative to when it calls the function**
* Execution time is added to the period

Mathematically:

```
next_wakeup = now + delay
```

---

## Timeline example (10 ms vs 20 ms)

Assume:

* Tick = 1 ms
* Task execution time = 2 ms (sometimes more)

---

### Task using `vTaskDelayUntil()` (10 ms period)

```
Time:     0   10  20  30  40
Task:     |---|---|---|---|
           run run run run
```

Even if one iteration takes 3 ms:

* Next wakeup is still at **exact multiples of 10 ms**
* Jitter is bounded to ±1 tick

✔ **Hard real-time friendly**

---

### Task using `vTaskDelay()` (20 ms delay)

```
Time:     0    22   44   67
Task:     |----|----|----|
           run  run   run
```

Execution time **adds to the delay**:

* Period slowly drifts
* Any preemption increases jitter

✖ **Not deterministic**

---

## Why this matters in RMS / fixed-priority systems

### `vHighSpeedSensorTask` (vTaskDelayUntil)

This task:

* Is **periodic**
* Has a **fixed activation rate**
* Matches RMS assumptions perfectly

✅ Suitable for:

* Sensors
* Control loops
* Sampling tasks
* Motor control
* DSP

---

### `vUrgentDataHandler` (vTaskDelay)

This task:

* Is **best-effort periodic**
* Not tied to a strict schedule
* Period depends on:

  * Execution time
  * Preemption
  * Interrupt load

⚠ Better described as:

* A **polling task**
* Or a throttled background task



## Summary table

| Aspect           | `vTaskDelayUntil()` | `vTaskDelay()` |
| ---------------- | ------------------- | -------------- |
| Timing reference | Absolute            | Relative       |
| Period stability | ✔ Fixed             | ✖ Drifts       |
| Jitter           | Minimal             | Unbounded      |
| RMS-compatible   | ✔ Yes               | ✖ No           |
| Best use         | Periodic RT tasks   | Simple delays  |
| Control systems  | ✔                   | ✖              |

---

## Final takeaway

> **If a task has a defined period → always use `vTaskDelayUntil()`**
> **If a task reacts to events → don’t delay at all, block on the event**
