# uFreeRTOSBook

**FreeRTOS** is a lightweight, open-source **real-time operating system (RTOS)** designed for **microcontrollers and small embedded systems**.

* **Key features:** preemptive and cooperative scheduling, deterministic real-time behavior, tasks/threads, queues, semaphores, mutexes, timers, and optional memory management schemes.
* **Domains of usage:** IoT devices, automotive ECUs, industrial controllers, medical devices, consumer electronics, and low-power embedded products.
* **What makes it special:** very **small footprint**, **high portability** (supports dozens of MCU architectures), simple API, and strong real-time determinism.
* **Ecosystem:** widely adopted, well-documented, MIT-licensed, and extended by **Amazon FreeRTOS** with cloud, networking, and security libraries.

In short, FreeRTOS provides reliable real-time multitasking for resource-constrained embedded systems.


## Core Fundamentals (1-10)

[1. **RTOS Concepts and Scheduling Theory**](docs/01_RTOS_Concepts_and_Scheduling_Theory.md)<br>
Understanding preemptive vs cooperative scheduling, time-slicing, context switching, and how real-time operating systems differ from general-purpose operating systems.

[2. **FreeRTOS Architecture**](docs/02_FreeRTOS_Architecture.md)<br>
The overall structure of FreeRTOS, including its layered design, portability layer, and how it interacts with hardware through abstraction.

[3. **Task Creation and Management**](docs/03_Task_Creation_and_Management.md)<br>
Creating tasks using xTaskCreate() and vTaskDelete(), understanding task control blocks (TCB), task states (running, ready, blocked, suspended), and the lifecycle of tasks.

[4. **Priority-Based Scheduling**](docs/04_Priority_Based_Scheduling.md)<br>
How FreeRTOS implements fixed-priority preemptive scheduling, priority assignment strategies, and the behavior of the scheduler when multiple tasks have the same priority.

[5. **Idle Task and Hook Functions**](docs/05_Idle_Task_and_Hook_Functions.md)<br>
The role of the idle task in FreeRTOS, implementing idle hooks for background processing, and understanding when the idle task runs.

[6. **Task Delays and Time Management**](docs/06_Task_Delays_and_Time_Management.md)<br>
Using vTaskDelay() and vTaskDelayUntil() for periodic task execution, understanding tick interrupts, configTICK_RATE_HZ, and achieving precise timing.

[7. **FreeRTOSConfig.h Configuration**](docs/07_FreeRTOSConfig.h_Configuration.md)<br>
Mastering all configuration options in this critical header file, including memory allocation schemes, stack sizes, priority levels, and feature enables

[8. **Memory Management Schemes**](docs/08_Memory_Management_Schemes.md)<br>
Understanding heap_1 through heap_5 implementations, their trade-offs, when to use each, and implementing custom memory allocation schemes

[9. **Stack Overflow Detection**](docs/09_Stack_Overflow_Detection.md)<br>
Configuring and using stack overflow checking methods (method 1 and 2), understanding how stack growth works on different architectures, and debugging stack issues

[10. **Context Switching Mechanics**](docs/10_Context_Switching_Mechanics.md)<br>
Deep understanding of how context switching works at the assembly level, what gets saved/restored, and the overhead involved in task switches

## Synchronization and Communication (11-20)

[11. **Queue Fundamentals**](docs/11_Queue_Fundamentals.md)<br>
Creating and using queues for inter-task communication, queue operations (send, receive, peek), blocking times, and queue design patterns

[12. **Queue Sets**](docs/12_Queue_Sets.md)<br>
Combining multiple queues and semaphores into a single blocking operation, use cases, and implementation patterns for complex synchronization scenarios

[13. **Binary Semaphores**](docs/13_Binary_Semaphores.md)<br>
Using binary semaphores for task synchronization, signaling between tasks and interrupts, and understanding the difference between semaphores and mutexes

[14. **Counting Semaphores**](docs/14_Counting_Semaphores.md)<br>
Implementing resource counting, managing multiple instances of resources, and typical use cases like limiting access to shared resources

[15. **Mutexes and Priority Inheritance**](docs/15_Mutexes_and_Priority_Inheritance.md)<br>
Preventing priority inversion using mutexes with priority inheritance, understanding the priority inversion problem, and recursive mutex usage

[16. **Event Groups (Event Flags)**](docs/16_Event_Groups_Event_Flags.md)<br>
Synchronizing multiple tasks based on multiple conditions, using event bits for complex synchronization patterns, and understanding atomic operations on event groups

[17. **Direct-to-Task Notifications**](docs/17_Direct_to_Task_Notifications.md)<br>
Using the lightweight task notification mechanism as an alternative to queues and semaphores, understanding performance benefits and limitations

[18. **Stream Buffers**](docs/18_Stream_Buffers.md)<br>
Implementing byte streams between tasks, typical use cases for serial data handling, and differences from message buffers

[19. **Message Buffers**](docs/19_Message_Buffers.md)<br>
Sending variable-length messages between tasks, understanding the internal implementation, and when to use message buffers vs queues

[20. **Critical Sections**](docs/20_Critical_Sections.md)<br>
Implementing critical sections using taskENTER_CRITICAL() and taskEXIT_CRITICAL(), understanding interrupt disabling, and nested critical sections

## Interrupt Handling (21-25)

[21. **Interrupt Service Routines (ISRs)**](docs/21_Interrupt_Service_Routines.md)<br>
Writing FreeRTOS-compatible ISRs, using FromISR() API variants, and understanding interrupt priorities relative to FreeRTOS

[22. **Deferred Interrupt Processing**](docs/22_Deferred_Interrupt_Processing.md)<br>
Implementing interrupt handlers that defer work to tasks using semaphores or task notifications, achieving low interrupt latency

[23. **Interrupt Nesting and Priority**](docs/23_Interrupt_Nesting_and_Priority.md)<br>
Configuring configMAX_SYSCALL_INTERRUPT_PRIORITY, understanding interrupt masking, and managing nested interrupts safely

[24. **Interrupt-Safe API Functions**](docs/24_Interrupt_Safe_API_Functions.md)<br>
Knowing which FreeRTOS APIs can be called from ISRs, using the xHigherPriorityTaskWoken mechanism, and forcing context switches from ISRs

[25. **Hardware Timer Integration**](docs/25_Hardware_Timer_Integration.md)<br>
Integrating hardware timers for system tick generation, using timers for precise timing operations, and handling timer interrupts

## Software Timers (26-28)

[26. **Software Timer Creation and Management**](docs/26_Software_Timer_Creation_and_Management.md)<br>
Creating one-shot and auto-reload timers, understanding the timer service task, and timer callback functions

[27. **Timer Command Queue**](docs/27_Timer_Command_Queue.md)<br>
How timer commands are queued and processed, understanding timer task priority, and managing timer operations from tasks and ISRs

[28. **Timer Accuracy and Limitations**](docs/28_Timer_Accuracy_and_Limitations.md)<br>
Understanding timer resolution based on tick rate, jitter considerations, and when to use hardware timers instead

## Advanced Task Management (29-33)

[29. **Task Priorities and Priority Assignment**](docs/29_Task_Priorities_and_Priority_Assignment.md)<br>
Strategic priority assignment, rate-monotonic vs deadline-monotonic scheduling, and avoiding priority inversion scenarios

[30. **Task Suspension and Resumption**](docs/30_Task_Suspension_and_Resumption.md)<br>
Using vTaskSuspend() and vTaskResume(), understanding the suspended state, and use cases for task suspension

[31. **Task Parameters and Return Values**](docs/31_Task_Parameters_and_Return_Values.md)<br>
Passing parameters to tasks during creation, sharing data between tasks safely, and managing task-local storage

[32. **Co-routines**](docs/32_Co_routines.md)<br>
Understanding the lightweight co-routine implementation, differences from tasks, and when co-routines are appropriate for memory-constrained systems

[33. **Task Run-Time Statistics**](docs/33_Task_Run_Time_Statistics.md)<br>
Enabling and collecting CPU usage statistics, analyzing task execution patterns, and optimizing system performance based on statistics

## Memory and Resource Management (34-38)

[34. **Dynamic vs Static Allocation**](docs/34_Dynamic_vs_Static_Allocation.md)<br>
Understanding xTaskCreate() vs xTaskCreateStatic(), trade-offs between dynamic and static allocation, and implementing fully static systems

[35. **Thread Local Storage (TLS)**](docs/35_Thread_Local_Storage.md)<br>
Implementing task-local storage pointers, use cases for per-task data, and managing TLS arrays

[36. **Resource Pools and Object Pools**](docs/36_Resource_Pools_and_Object_Pools.md)<br>
Implementing efficient resource management patterns, pre-allocating objects to avoid dynamic allocation at runtime

[37. **Memory Fragmentation**](docs/37_Memory_Fragmentation.md)<br>
Understanding fragmentation issues with heap_2 and heap_4, strategies to minimize fragmentation, and monitoring heap usage

[38. **Watchdog Timer Integration**](docs/38_Watchdog_Timer_Integration.md)<br>
Integrating hardware watchdog timers with FreeRTOS, feeding the watchdog from appropriate tasks, and handling watchdog failures

## Porting and Platform-Specific (39-43)

[39. **Port Layer Architecture**](docs/39_Port_Layer_Architecture.md)<br>
Understanding the portable.h and portmacro.h files, how FreeRTOS abstracts hardware differences, and the port-specific implementations

[40. **Assembly Language Port Files**](docs/40_Assembly_Language_Port_Files.md)<br>
Reading and understanding the assembly code in port.c files, including context switching and PendSV handler implementations (especially for ARM Cortex-M)

[41. **MPU (Memory Protection Unit) Support**](docs/41_MPU_Memory_Protection_Unit_Support.md)<br>
Configuring and using FreeRTOS MPU ports, creating privileged and unprivileged tasks, and memory region definitions

[42. **SMP (Symmetric Multiprocessing) Support**](docs/42_SMP_Symmetric_Multiprocessing_Support.md)<br>
Understanding FreeRTOS SMP for multi-core systems, core affinity, spinlocks, and inter-core synchronization

[43. **Custom Port Development**](docs/43_Custom_Port_Development.md)<br>
Creating a FreeRTOS port for new architectures, implementing required functions, and testing a new port thoroughly

## Debugging and Analysis (44-47)

[44. **Trace Functionality**](docs/44_Trace_Functionality.md)<br>
Using FreeRTOS trace macros, integrating with tools like Tracealyzer or SystemView, and analyzing system behavior through traces

[45. **Assert and Error Handling**](docs/45_Assert_and_Error_Handling.md)<br>
Implementing configASSERT() effectively, handling allocation failures, and creating robust error recovery mechanisms

[46. **Debugging Techniques**](docs/46_Debugging_Techniques.md)<br>
Using JTAG debuggers with FreeRTOS-aware plugins, inspecting task states, queues, and semaphores at runtime

[47. **Performance Optimization**](docs/47_Performance_Optimization.md)<br>
Reducing context switch overhead, optimizing interrupt latency, choosing appropriate tick rates, and memory optimization techniques

## Real-World Implementation (48-50)

[48. **Power Management**](docs/48_Power_Management.md)<br>
Implementing tickless idle mode for low-power applications, using sleep modes, and balancing responsiveness with power consumption

[49. **Safety and Certification**](docs/49_Safety_and_Certification.md)<br>
Understanding FreeRTOS SafeRTOS for safety-critical applications, MISRA C compliance, certification standards (IEC 61508, DO-178C), and coding for reliability

[50. **Design Patterns and Best Practices**](docs/50_Design_Patterns_and_Best_Practices.md)<br>
Implementing common RTOS design patterns (producer-consumer, event-driven, pipeline), avoiding deadlocks and race conditions, and structuring applications for maintainability and scalability

## Miscelaneous

[51. **Supported processors**](docs/51_Processors.md)<br>
[52. **Building and using FreeRTOS as a Linux Application for x86 architecture** ](docs/52_Building_and_Using_FreeRTOS_as_a_Linux_Application_for_x86.md)<br>
[53. **Task states diagram**](docs/53_Task_states_diagram.md)<br>
[54. **FreeRTOS APIs**](docs/54_FreeRTOS_APIs.md)<br>
[55. **FreeRTOS Data Types Reference**](docs/55_FreeRTOS_Data_Types_Reference.md)<br>
[56. **FreeRTOS Task context switch**](docs/56_Task_Context_Switch.md)<br>
[57. **FreeRTOS Naming Convention - Complete Prefix Guide**](docs/57_Prefix_conventions.md)<br>
[58. **What "Port-Specific" Means in FreeRTOS**](docs/58_Port_Specific_Meaning.md)<br>
[59. **FreeRTOS configuration parameters**](docs/59_Configuration_Parameters.md)<br>