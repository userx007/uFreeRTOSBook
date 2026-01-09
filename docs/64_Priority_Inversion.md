# Priority Inversion in FreeRTOS

Priority inversion is a problematic scheduling scenario where a high-priority task is blocked waiting for a resource held by a low-priority task, while a medium-priority task preempts the low-priority task and runs instead. This effectively inverts the intended priority scheme.

## How It Happens

Consider three tasks with priorities: High > Medium > Low

1. **Low-priority task** acquires a mutex/semaphore
2. **High-priority task** preempts and tries to acquire the same mutex - gets blocked
3. **Medium-priority task** becomes ready and preempts the low-priority task
4. The medium-priority task runs while the high-priority task waits, even though it should have higher priority

The high-priority task is now stuck waiting for the low-priority task to finish, but the low-priority task can't run because the medium-priority task keeps preempting it. This is priority inversion.

## Real-World Impact

The classic example is the Mars Pathfinder incident in 1997, where priority inversion caused system resets. A low-priority meteorological task held a shared resource needed by a high-priority communication task, while medium-priority tasks prevented the low-priority task from completing.

## Solutions in FreeRTOS

### 1. **Priority Inheritance**

FreeRTOS mutexes support priority inheritance by default when you use `xSemaphoreCreateMutex()` or `xSemaphoreCreateRecursiveMutex()`.

**How it works:** When a high-priority task blocks on a mutex held by a low-priority task, the low-priority task temporarily inherits the high-priority task's priority. This allows it to complete quickly and release the mutex.

```c
// Create a mutex (has priority inheritance built-in)
SemaphoreHandle_t xMutex = xSemaphoreCreateMutex();

// Low priority task
void vLowPriorityTask(void *pvParameters) {
    xSemaphoreTake(xMutex, portMAX_DELAY);
    // Critical section - if high priority task blocks here,
    // this task inherits its priority
    // ... work ...
    xSemaphoreGive(xMutex);
}
```

**Important:** Binary semaphores and counting semaphores do NOT support priority inheritance - only mutexes do.

### 2. **Priority Ceiling Protocol**

Set the mutex holder's priority to a predetermined ceiling priority (higher than all tasks that might use it). FreeRTOS doesn't directly implement this, but you can manually adjust task priorities.

### 3. **Avoid Long Critical Sections**

Keep mutex-protected sections as short as possible to minimize blocking time:

```c
xSemaphoreTake(xMutex, portMAX_DELAY);
// Only protect the minimal critical data access
sharedVariable++;
xSemaphoreGive(xMutex);
// Do other work outside the mutex
```

### 4. **Disable Preemption for Critical Sections**

For very short critical sections, use task scheduler suspension:

```c
vTaskSuspendAll();
// Critical section - no task switching occurs
sharedVariable++;
xTaskResumeAll();
```

This prevents priority inversion but stops all task switching, so use sparingly and only for very brief operations.

### 5. **Design Considerations**

- Minimize resource sharing between tasks of different priorities
- Use message queues instead of shared memory when possible
- Carefully consider task priority assignments
- Avoid having low-priority tasks hold resources that high-priority tasks need

## Key Takeaways

Priority inversion is insidious because it only manifests under specific timing conditions, making it hard to debug. In FreeRTOS, always use mutexes (not binary semaphores) for shared resource protection when tasks of different priorities are involved, as they automatically provide priority inheritance. Keep critical sections short and consider your system's priority architecture carefully during design.

---

## Graphical representation

```
| Task   | Priority   | Behavior                  |
| ------ | ---------- | ------------------------- |
| Task H | 3 (high)   | Needs mutex M             |
| Task M | 2 (medium) | CPU-bound                 |
| Task L | 1 (low)    | Holds mutex M for a while |

**Initial state:** Task L acquires the mutex first.
```


## With Priority Inversion

```
Time →→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→
Priority ↑
---Timeline--|    1   |    2    |    3    |    4    |    5     |
-------------+--------+---------+---------+---------+----------+
P3 | Task H  |        | BLOCKED | BLOCKED | BLOCKED | RUNNING  |
P2 | Task M  |        | RUNNING | RUNNING | RUNNING | READY    |
P1 | Task L  | RUNNING| READY   | READY   | READY   | READY    |
                 |<----mutex M held by L----------->| 
                      |<----mutex M needed by H --->|
```


### Step-by-Step Analysis

1. **Time 1**

   * **Task L (P1)** is **RUNNING**

     * It starts first, acquires **mutex M**
   * **Task H (P3)** has not tried to run yet
   * **Task M (P2)** is **READY** but CPU is occupied by L

2. **Time 2**

   * **Task H (P3)** becomes ready → wants **mutex M**
   * H cannot run because **mutex M is held by L** → **BLOCKED**
   * **Task M (P2)** is higher priority than L (P2 > P1) → **preempts L**
   * L moves from **RUNNING → READY** (preempted)
   * CPU → **Task M RUNNING**

3. **Time 3**

   * **Task M** continues running → CPU still with M
   * **Task H** still **BLOCKED** (waiting for mutex M)
   * **Task L** still **READY** (cannot run, priority too low)

   **Observation:** **True priority inversion occurs**:

   * High task (H) waiting for low task (L)
   * Medium task (M) delays L from running → keeps H blocked longer

4. **Time 4**

   * **Task M** continues CPU-bound work → still **RUNNING**
   * **Task H** still **BLOCKED**
   * **Task L** still **READY**

   **Note:** The low-priority task (L) cannot run because the medium-priority task (M) is continuously ready → high task H keeps waiting

5. **Time 5**

   * **Task M** finishes → CPU scheduler picks next highest ready task
   * **Task L (P1, READY)** → now **RUNNING** → can finish and release **mutex M**
   * **Task H (P3)** still **BLOCKED** but will soon acquire mutex after L releases it


## With Priority Inheritance Enabled

```
Time →→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→
Priority ↑
---Timeline--|    1    |    2        |    3        |    4    |    5     |
-------------+---------+-------------+-------------+---------+----------+  
P3 | Task H  |         | BLOCKED     | BLOCKED     | RUNNING | RUNNING  |
P2 | Task M  |         | READY       | READY       | READY   | READY    |
P1 | Task L  | RUNNING | RUNNING as 3| RUNNING as 3| READY   | READY    | 
                 |<------mutex M held by L-------->|     
                       |<---mutex M needed by H--->|
```

### Step-by-step 

1. **Time 1:**

   * L (P1) starts running → acquires **mutex M**

2. **Time 2:**

   * H (P3) becomes ready → wants **mutex M** → **BLOCKED**
   * L inherits **H’s priority** → **RUNNING as 3**
   * M (P2) is READY but **preempted by L boosted to 3**

3. **Time 3:**

   * L still RUNNING as 3 → continues work → will release mutex
   * H still BLOCKED
   * M still READY

4. **Time 4:**

   * L releases mutex → H takes mutex → RUNNING
   * L reverts to original priority (P1) → READY
   * M still READY

5. **Time 5:**

   * H continues RUNNING
   * L READY / can run later
   * M READY → can run after H
