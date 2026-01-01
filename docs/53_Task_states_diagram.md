# FreeRTOS task state diagram 

1. **Running**: The task is currently being executed by the CPU.
2. **Ready**: The task is ready to run, but it is not currently executing because another task is running.
3. **Blocked**: The task is waiting for some event or condition to occur (e.g., a semaphore or a time delay).
4. **Suspended**: The task is temporarily paused and won't run until explicitly resumed.
5. **Deleted**: The task is deleted and no longer exists.

Here's a simplified version of the task states diagram in FreeRTOS:

```
            +----------------+
            |     Running    |
            +----------------+
                    ^
                    |
                    |
          +------------------+
          |     Ready        | <-------------------+
          +------------------+                     |
                    |                              |
                    |                              |
              +-----------+                 +-------------------+
              |  Blocked  | <---------------|   Suspended       |
              +-----------+                 +-------------------+
                    |
                    v
          +------------------+
          |     Deleted      |
          +------------------+
```

### Task States in Detail:

1. **Running**:

   * This is when a task is actively being executed by the CPU.
   * Only one task can be in the "Running" state at a time (depending on the scheduling).

2. **Ready**:

   * Tasks that are ready to run but are not currently executing are in this state.
   * A task moves from "Ready" to "Running" when the scheduler decides to run it.

3. **Blocked**:

   * A task can enter the "Blocked" state when it is waiting for some condition to be met. For example, waiting for a semaphore, waiting for a message from a queue, or waiting for a timeout to expire.
   * Once the condition is satisfied (e.g., semaphore becomes available), the task moves back to "Ready".

4. **Suspended**:

   * A task can be suspended manually via an API like `vTaskSuspend()`. In this state, the task will not execute until it is resumed.
   * The task can be resumed by calling `vTaskResume()`, and it will move to the "Ready" state.

5. **Deleted**:

   * A task is deleted using the `vTaskDelete()` API.
   * Once deleted, the task is no longer in existence.

### Transitioning Between States:

* A task can transition between these states depending on the system’s needs and the API calls made in the application.
* For example:

  * From **Ready** to **Running** when the scheduler picks the task.
  * From **Running** to **Blocked** when a task waits on a semaphore or timer.
  * From **Blocked** to **Ready** when the task's waiting condition is met.
  * From **Suspended** to **Ready** when the task is resumed.
  * From any state to **Deleted** when the task is deleted.

This diagram helps to visualize how tasks can transition between different states in FreeRTOS. Would you like more details about how task scheduling works in FreeRTOS or any specific state transitions?
