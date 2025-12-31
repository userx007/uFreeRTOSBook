# uFreeRTOSBook


**Core Fundamentals (1-10)**

[1. **RTOS Concepts and Scheduling Theory** - Understanding preemptive vs cooperative scheduling, time-slicing, context switching, and how real-time operating systems differ from general-purpose operating systems.](docs/1_RTOS_Concepts_and_Scheduling_Theory.md)<br>

[2. **FreeRTOS Architecture** - The overall structure of FreeRTOS, including its layered design, portability layer, and how it interacts with hardware through abstraction.](docs/2_FreeRTOS_Architecture.md)<br>

[3. **Task Creation and Management** - Creating tasks using xTaskCreate() and vTaskDelete(), understanding task control blocks (TCB), task states (running, ready, blocked, suspended), and the lifecycle of tasks.](docs/3_Task_Creation_and_Management.md)<br>

[4. **Priority-Based Scheduling** - How FreeRTOS implements fixed-priority preemptive scheduling, priority assignment strategies, and the behavior of the scheduler when multiple tasks have the same priority.](docs/4_Priority_Based_Scheduling.md)<br>

[5. **Idle Task and Hook Functions** - The role of the idle task in FreeRTOS, implementing idle hooks for background processing, and understanding when the idle task runs.](docs/5_Idle_Task_and_Hook_Functions.md)<br>

[6. **Task Delays and Time Management** - Using vTaskDelay() and vTaskDelayUntil() for periodic task execution, understanding tick interrupts, configTICK_RATE_HZ, and achieving precise timing.](docs/6_Task_Delays_and_Time_Management.md)<br>

[7. **FreeRTOSConfig.h Configuration** - Mastering all configuration options in this critical header file, including memory allocation schemes, stack sizes, priority levels, and feature enables.](docs/7_FreeRTOSConfig.h_Configuration.md)<br>

[8. **Memory Management Schemes** - Understanding heap_1 through heap_5 implementations, their trade-offs, when to use each, and implementing custom memory allocation schemes.](docs/8_Memory_Management_Schemes.md)<br>

[9. **Stack Overflow Detection** - Configuring and using stack overflow checking methods (method 1 and 2), understanding how stack growth works on different architectures, and debugging stack issues.](docs/9_Stack_Overflow_Detection.md)<br>

[10. **Context Switching Mechanics** - Deep understanding of how context switching works at the assembly level, what gets saved/restored, and the overhead involved in task switches.](docs/10_Context_Switching_Mechanics.md)<br>

**Synchronization and Communication (11-20)**

[11. **Queue Fundamentals** - Creating and using queues for inter-task communication, queue operations (send, receive, peek), blocking times, and queue design patterns.](docs/11_Queue_Fundamentals.md)<br>

[12. **Queue Sets** - Combining multiple queues and semaphores into a single blocking operation, use cases, and implementation patterns for complex synchronization scenarios.](docs/12_Queue_Sets.md)<br>

[13. **Binary Semaphores** - Using binary semaphores for task synchronization, signaling between tasks and interrupts, and understanding the difference between semaphores and mutexes.](docs/13_Binary_Semaphores.md)<br>

[14. **Counting Semaphores** - Implementing resource counting, managing multiple instances of resources, and typical use cases like limiting access to shared resources.](docs/14_Counting_Semaphores.md)<br>

[15. **Mutexes and Priority Inheritance** - Preventing priority inversion using mutexes with priority inheritance, understanding the priority inversion problem, and recursive mutex usage.](docs/15_Mutexes_and_Priority_Inheritance.md)<br>

[16. **Event Groups (Event Flags)** - Synchronizing multiple tasks based on multiple conditions, using event bits for complex synchronization patterns, and understanding atomic operations on event groups.](docs/16_Event_Groups_Event_Flags.md)<br>

[17. **Direct-to-Task Notifications** - Using the lightweight task notification mechanism as an alternative to queues and semaphores, understanding performance benefits and limitations.](docs/17_Direct_to_Task_Notifications.md)<br>

[18. **Stream Buffers** - Implementing byte streams between tasks, typical use cases for serial data handling, and differences from message buffers.](docs/18_Stream_Buffers.md)<br>

[19. **Message Buffers** - Sending variable-length messages between tasks, understanding the internal implementation, and when to use message buffers vs queues.](docs/19_Message_Buffers.md)<br>

[20. **Critical Sections** - Implementing critical sections using taskENTER_CRITICAL() and taskEXIT_CRITICAL(), understanding interrupt disabling, and nested critical sections.](docs/20_Critical_Sections.md)<br>

**Interrupt Handling (21-25)**

[21. **Interrupt Service Routines (ISRs)** - Writing FreeRTOS-compatible ISRs, using FromISR() API variants, and understanding interrupt priorities relative to FreeRTOS.](docs/21_Interrupt_Service_Routines.md)<br>

[22. **Deferred Interrupt Processing** - Implementing interrupt handlers that defer work to tasks using semaphores or task notifications, achieving low interrupt latency.](docs/22_Deferred_Interrupt_Processing.md)<br>

[23. **Interrupt Nesting and Priority** - Configuring configMAX_SYSCALL_INTERRUPT_PRIORITY, understanding interrupt masking, and managing nested interrupts safely.](docs/23_Interrupt_Nesting_and_Priority.md)<br>

[24. **Interrupt-Safe API Functions** - Knowing which FreeRTOS APIs can be called from ISRs, using the xHigherPriorityTaskWoken mechanism, and forcing context switches from ISRs.](docs/24_Interrupt_Safe_API_Functions.md)<br>

[25. **Hardware Timer Integration** - Integrating hardware timers for system tick generation, using timers for precise timing operations, and handling timer interrupts.](docs/25_Hardware_Timer_Integration.md)<br>

**Software Timers (26-28)**

[26. **Software Timer Creation and Management** - Creating one-shot and auto-reload timers, understanding the timer service task, and timer callback functions.](docs/26_Software_Timer_Creation_and_Management.md)<br>

[27. **Timer Command Queue** - How timer commands are queued and processed, understanding timer task priority, and managing timer operations from tasks and ISRs.](docs/27_Timer_Command_Queue.md)<br>

[28. **Timer Accuracy and Limitations** - Understanding timer resolution based on tick rate, jitter considerations, and when to use hardware timers instead.](docs/28_Timer_Accuracy_and_Limitations.md)<br>

**Advanced Task Management (29-33)**

[29. **Task Priorities and Priority Assignment** - Strategic priority assignment, rate-monotonic vs deadline-monotonic scheduling, and avoiding priority inversion scenarios.](docs/29_Task_Priorities_and_Priority_Assignment.md)<br>

[30. **Task Suspension and Resumption** - Using vTaskSuspend() and vTaskResume(), understanding the suspended state, and use cases for task suspension.](docs/30_Task_Suspension_and_Resumption.md)<br>

[31. **Task Parameters and Return Values** - Passing parameters to tasks during creation, sharing data between tasks safely, and managing task-local storage.](docs/31_Task_Parameters_and_Return_Values.md)<br>

[32. **Co-routines** - Understanding the lightweight co-routine implementation, differences from tasks, and when co-routines are appropriate for memory-constrained systems.](docs/32_Co_routines.md)<br>

[33. **Task Run-Time Statistics** - Enabling and collecting CPU usage statistics, analyzing task execution patterns, and optimizing system performance based on statistics.](docs/33_Task_Run_Time_Statistics.md)<br>

**Memory and Resource Management (34-38)**

[34. **Dynamic vs Static Allocation** - Understanding xTaskCreate() vs xTaskCreateStatic(), trade-offs between dynamic and static allocation, and implementing fully static systems.](docs/34_Dynamic_vs_Static_Allocation.md)<br>

[35. **Thread Local Storage (TLS)** - Implementing task-local storage pointers, use cases for per-task data, and managing TLS arrays.](docs/35_Thread_Local_Storage.md)<br>

[36. **Resource Pools and Object Pools** - Implementing efficient resource management patterns, pre-allocating objects to avoid dynamic allocation at runtime.](docs/36_Resource_Pools_and_Object_Pools.md)<br>

[37. **Memory Fragmentation** - Understanding fragmentation issues with heap_2 and heap_4, strategies to minimize fragmentation, and monitoring heap usage.](docs/37_Memory_Fragmentation.md)<br>

[38. **Watchdog Timer Integration** - Integrating hardware watchdog timers with FreeRTOS, feeding the watchdog from appropriate tasks, and handling watchdog failures.](docs/38_Watchdog_Timer_Integration.md)<br>

**Porting and Platform-Specific (39-43)**

[39. **Port Layer Architecture** - Understanding the portable.h and portmacro.h files, how FreeRTOS abstracts hardware differences, and the port-specific implementations.](docs/39_Port_Layer_Architecture.md)<br>

[40. **Assembly Language Port Files** - Reading and understanding the assembly code in port.c files, including context switching and PendSV handler implementations (especially for ARM Cortex-M).](docs/40_Assembly_Language_Port_Files.md)<br>

[41. **MPU (Memory Protection Unit) Support** - Configuring and using FreeRTOS MPU ports, creating privileged and unprivileged tasks, and memory region definitions.](docs/41_MPU_Memory_Protection_Unit_Support.md)<br>

[42. **SMP (Symmetric Multiprocessing) Support** - Understanding FreeRTOS SMP for multi-core systems, core affinity, spinlocks, and inter-core synchronization.](docs/42_SMP_Symmetric_Multiprocessing_Support.md)<br>

[43. **Custom Port Development** - Creating a FreeRTOS port for new architectures, implementing required functions, and testing a new port thoroughly.](docs/43_Custom_Port_Development.md)<br>

**Debugging and Analysis (44-47)**

[44. **Trace Functionality** - Using FreeRTOS trace macros, integrating with tools like Tracealyzer or SystemView, and analyzing system behavior through traces.](docs/44_Trace_Functionality.md)<br>

[45. **Assert and Error Handling** - Implementing configASSERT() effectively, handling allocation failures, and creating robust error recovery mechanisms.](docs/45_Assert_and_Error_Handling.md)<br>

[46. **Debugging Techniques** - Using JTAG debuggers with FreeRTOS-aware plugins, inspecting task states, queues, and semaphores at runtime.](docs/46_Debugging_Techniques.md)<br>

[47. **Performance Optimization** - Reducing context switch overhead, optimizing interrupt latency, choosing appropriate tick rates, and memory optimization techniques.](docs/47_Performance_Optimization.md)<br>

**Real-World Implementation (48-50)**

[48. **Power Management** - Implementing tickless idle mode for low-power applications, using sleep modes, and balancing responsiveness with power consumption.](docs/48_Power_Management.md)<br>

[49. **Safety and Certification** - Understanding FreeRTOS SafeRTOS for safety-critical applications, MISRA C compliance, certification standards (IEC 61508, DO-178C), and coding for reliability.](docs/49_Safety_and_Certification.md)<br>

[50. **Design Patterns and Best Practices** - Implementing common RTOS design patterns (producer-consumer, event-driven, pipeline), avoiding deadlocks and race conditions, and structuring applications for maintainability and scalability.](docs/50_Design_Patterns_and_Best_Practices.md)<br>

[51. **Supported processors**](docs/51_Processors.md)<br>

[52. **Building and using FreeRTOS as a Linux Application for x86 architecture** ](docs/52_Building_and_Using_FreeRTOS_as_a_Linux_Application_for_x86.md)<br>

