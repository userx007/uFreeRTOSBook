# Rate Monotonic Analysis in FreeRTOS

## Detailed Description

Rate Monotonic Analysis (RMA) is a mathematical framework for analyzing the schedulability of periodic real-time tasks in priority-based preemptive scheduling systems. It provides theoretical guarantees about whether a set of tasks will meet their deadlines.

### Core Concepts

**Rate Monotonic Scheduling (RMS)** is a fixed-priority scheduling algorithm where:
- Tasks are assigned priorities inversely proportional to their periods
- Shorter period = Higher priority
- Tasks must be periodic with deadlines equal to their periods
- Tasks are independent (no resource sharing considered in basic RMA)

**Key Assumptions:**
1. All tasks are periodic
2. Tasks are independent (no shared resources)
3. Deadline = Period for each task
4. Context switch overhead is negligible or accounted for
5. Tasks are preemptable

### CPU Utilization Bound

The fundamental theorem states that for n periodic tasks, the task set is schedulable if:

**U = Σ(Ci/Ti) ≤ n(2^(1/n) - 1)**

Where:
- Ci = Worst-case execution time of task i
- Ti = Period of task i
- U = Total CPU utilization

For large n, this bound approaches ln(2) ≈ 0.693 (69.3%)

### Schedulability Tests

1. **Sufficient Test (Utilization Bound)**: If U ≤ n(2^(1/n) - 1), the task set is guaranteed schedulable
2. **Necessary Test**: If U > 1, the task set is definitely not schedulable
3. **Response Time Analysis**: More exact test for cases where utilization falls between bounds

## Programming Examples

### C/C++ Implementation

```c
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <math.h>

// Task characteristics structure
typedef struct {
    const char* name;
    uint32_t period_ms;        // Period in milliseconds
    uint32_t wcet_ms;          // Worst-case execution time
    uint32_t priority;         // FreeRTOS priority
    TaskHandle_t handle;
} PeriodicTaskSpec_t;

// Calculate utilization bound for n tasks
double calculate_utilization_bound(int n) {
    return n * (pow(2.0, 1.0/n) - 1.0);
}

// Calculate total CPU utilization
double calculate_total_utilization(PeriodicTaskSpec_t* tasks, int num_tasks) {
    double utilization = 0.0;
    for (int i = 0; i < num_tasks; i++) {
        utilization += (double)tasks[i].wcet_ms / (double)tasks[i].period_ms;
    }
    return utilization;
}

// Assign Rate Monotonic priorities (shorter period = higher priority)
void assign_rate_monotonic_priorities(PeriodicTaskSpec_t* tasks, int num_tasks) {
    // Sort tasks by period (ascending)
    for (int i = 0; i < num_tasks - 1; i++) {
        for (int j = 0; j < num_tasks - i - 1; j++) {
            if (tasks[j].period_ms > tasks[j+1].period_ms) {
                PeriodicTaskSpec_t temp = tasks[j];
                tasks[j] = tasks[j+1];
                tasks[j+1] = temp;
            }
        }
    }
    
    // Assign priorities (highest FreeRTOS priority to shortest period)
    for (int i = 0; i < num_tasks; i++) {
        tasks[i].priority = configMAX_PRIORITIES - 1 - i;
    }
}

// Response time analysis for exact schedulability test
uint32_t calculate_response_time(PeriodicTaskSpec_t* task, 
                                  PeriodicTaskSpec_t* tasks, 
                                  int task_index,
                                  int num_tasks) {
    uint32_t response_time = task->wcet_ms;
    uint32_t prev_response_time = 0;
    
    // Iterative calculation until convergence
    while (response_time != prev_response_time) {
        prev_response_time = response_time;
        response_time = task->wcet_ms;
        
        // Add interference from higher priority tasks
        for (int i = 0; i < task_index; i++) {
            response_time += (uint32_t)ceil((double)prev_response_time / 
                                           (double)tasks[i].period_ms) * 
                            tasks[i].wcet_ms;
        }
    }
    
    return response_time;
}

// Perform complete RMA schedulability analysis
int analyze_schedulability(PeriodicTaskSpec_t* tasks, int num_tasks) {
    printf("\n=== Rate Monotonic Analysis ===\n");
    
    // Calculate utilization
    double total_utilization = calculate_total_utilization(tasks, num_tasks);
    double utilization_bound = calculate_utilization_bound(num_tasks);
    
    printf("Total Utilization: %.4f (%.2f%%)\n", 
           total_utilization, total_utilization * 100);
    printf("Utilization Bound: %.4f (%.2f%%)\n", 
           utilization_bound, utilization_bound * 100);
    
    // Necessary condition
    if (total_utilization > 1.0) {
        printf("RESULT: NOT SCHEDULABLE (U > 1)\n");
        return 0;
    }
    
    // Sufficient condition
    if (total_utilization <= utilization_bound) {
        printf("RESULT: SCHEDULABLE (Sufficient condition met)\n");
        return 1;
    }
    
    // Between bounds - need response time analysis
    printf("Between bounds - performing response time analysis...\n");
    
    for (int i = 0; i < num_tasks; i++) {
        uint32_t response_time = calculate_response_time(&tasks[i], tasks, i, num_tasks);
        printf("Task %s: Response time = %lu ms, Deadline = %lu ms",
               tasks[i].name, response_time, tasks[i].period_ms);
        
        if (response_time <= tasks[i].period_ms) {
            printf(" [OK]\n");
        } else {
            printf(" [MISSED]\n");
            printf("RESULT: NOT SCHEDULABLE (Deadline miss detected)\n");
            return 0;
        }
    }
    
    printf("RESULT: SCHEDULABLE (Response time analysis passed)\n");
    return 1;
}

// Generic periodic task implementation
void periodic_task_function(void* pvParameters) {
    PeriodicTaskSpec_t* spec = (PeriodicTaskSpec_t*)pvParameters;
    TickType_t last_wake_time = xTaskGetTickCount();
    
    for (;;) {
        TickType_t start_time = xTaskGetTickCount();
        
        // Simulate work (WCET)
        // In real application, this would be actual task work
        vTaskDelay(pdMS_TO_TICKS(spec->wcet_ms));
        
        TickType_t execution_time = xTaskGetTickCount() - start_time;
        
        // Check for deadline overrun
        if (execution_time > pdMS_TO_TICKS(spec->wcet_ms)) {
            printf("WARNING: Task %s exceeded WCET!\n", spec->name);
        }
        
        // Wait for next period
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(spec->period_ms));
    }
}

// Example usage
void setup_rate_monotonic_system(void) {
    // Define task set
    PeriodicTaskSpec_t tasks[] = {
        {"Task1", 100, 20, 0, NULL},  // Period=100ms, WCET=20ms
        {"Task2", 200, 40, 0, NULL},  // Period=200ms, WCET=40ms
        {"Task3", 400, 60, 0, NULL},  // Period=400ms, WCET=60ms
    };
    int num_tasks = sizeof(tasks) / sizeof(tasks[0]);
    
    // Assign Rate Monotonic priorities
    assign_rate_monotonic_priorities(tasks, num_tasks);
    
    // Perform schedulability analysis
    if (!analyze_schedulability(tasks, num_tasks)) {
        printf("ERROR: Task set is not schedulable!\n");
        return;
    }
    
    // Create tasks
    for (int i = 0; i < num_tasks; i++) {
        xTaskCreate(
            periodic_task_function,
            tasks[i].name,
            configMINIMAL_STACK_SIZE,
            (void*)&tasks[i],
            tasks[i].priority,
            &tasks[i].handle
        );
    }
    
    printf("All tasks created successfully\n");
}
```

### Rust Implementation

```rust
use core::cmp::Ordering;

#[derive(Debug, Clone)]
struct PeriodicTask {
    name: &'static str,
    period_ms: u32,
    wcet_ms: u32,
    priority: u32,
}

impl PeriodicTask {
    fn utilization(&self) -> f64 {
        self.wcet_ms as f64 / self.period_ms as f64
    }
}

struct RateMonotonicAnalyzer {
    tasks: Vec<PeriodicTask>,
}

impl RateMonotonicAnalyzer {
    fn new(mut tasks: Vec<PeriodicTask>) -> Self {
        // Sort by period (ascending) for Rate Monotonic priority assignment
        tasks.sort_by(|a, b| a.period_ms.cmp(&b.period_ms));
        
        // Assign priorities (lower period = higher priority value)
        for (i, task) in tasks.iter_mut().enumerate() {
            task.priority = (tasks.len() - i) as u32;
        }
        
        Self { tasks }
    }
    
    fn utilization_bound(&self) -> f64 {
        let n = self.tasks.len() as f64;
        n * (2_f64.powf(1.0 / n) - 1.0)
    }
    
    fn total_utilization(&self) -> f64 {
        self.tasks.iter().map(|t| t.utilization()).sum()
    }
    
    fn calculate_response_time(&self, task_index: usize) -> u32 {
        let task = &self.tasks[task_index];
        let mut response_time = task.wcet_ms;
        let mut prev_response_time;
        
        // Iterative calculation until convergence
        loop {
            prev_response_time = response_time;
            response_time = task.wcet_ms;
            
            // Add interference from higher priority tasks
            for i in 0..task_index {
                let higher_priority_task = &self.tasks[i];
                let preemptions = (prev_response_time as f64 / 
                                  higher_priority_task.period_ms as f64).ceil() as u32;
                response_time += preemptions * higher_priority_task.wcet_ms;
            }
            
            if response_time == prev_response_time {
                break;
            }
            
            // Safety check for convergence
            if response_time > task.period_ms * 10 {
                return u32::MAX; // Indicate non-convergence
            }
        }
        
        response_time
    }
    
    fn analyze(&self) -> SchedulabilityResult {
        let total_util = self.total_utilization();
        let util_bound = self.utilization_bound();
        
        println!("\n=== Rate Monotonic Analysis ===");
        println!("Total Utilization: {:.4} ({:.2}%)", 
                total_util, total_util * 100.0);
        println!("Utilization Bound: {:.4} ({:.2}%)", 
                util_bound, util_bound * 100.0);
        
        // Necessary condition
        if total_util > 1.0 {
            println!("RESULT: NOT SCHEDULABLE (U > 1)");
            return SchedulabilityResult::NotSchedulable;
        }
        
        // Sufficient condition
        if total_util <= util_bound {
            println!("RESULT: SCHEDULABLE (Sufficient condition met)");
            return SchedulabilityResult::Schedulable;
        }
        
        // Between bounds - response time analysis
        println!("Between bounds - performing response time analysis...");
        
        for (i, task) in self.tasks.iter().enumerate() {
            let response_time = self.calculate_response_time(i);
            
            print!("Task {}: Response time = {} ms, Deadline = {} ms",
                   task.name, response_time, task.period_ms);
            
            if response_time <= task.period_ms {
                println!(" [OK]");
            } else {
                println!(" [MISSED]");
                println!("RESULT: NOT SCHEDULABLE (Deadline miss detected)");
                return SchedulabilityResult::NotSchedulable;
            }
        }
        
        println!("RESULT: SCHEDULABLE (Response time analysis passed)");
        SchedulabilityResult::Schedulable
    }
    
    fn print_task_details(&self) {
        println!("\n=== Task Set Details ===");
        for task in &self.tasks {
            println!("Task: {}", task.name);
            println!("  Period: {} ms", task.period_ms);
            println!("  WCET: {} ms", task.wcet_ms);
            println!("  Priority: {}", task.priority);
            println!("  Utilization: {:.4}", task.utilization());
        }
    }
}

#[derive(Debug, PartialEq)]
enum SchedulabilityResult {
    Schedulable,
    NotSchedulable,
}

// Example usage with FreeRTOS-rust bindings
#[cfg(feature = "freertos")]
mod freertos_impl {
    use super::*;
    use freertos_rust::*;
    
    pub fn create_rate_monotonic_tasks(analyzer: &RateMonotonicAnalyzer) {
        if analyzer.analyze() != SchedulabilityResult::Schedulable {
            panic!("Task set is not schedulable!");
        }
        
        for task_spec in &analyzer.tasks {
            let period = Duration::ms(task_spec.period_ms);
            let wcet = Duration::ms(task_spec.wcet_ms);
            let priority = task_spec.priority as u8;
            
            Task::new()
                .name(task_spec.name)
                .priority(TaskPriority(priority))
                .start(move || {
                    let mut last_wake_time = FreeRtosUtils::get_tick_count();
                    
                    loop {
                        let start_time = FreeRtosUtils::get_tick_count();
                        
                        // Simulate work (in real app, do actual work)
                        FreeRtosUtils::delay(wcet);
                        
                        let execution_time = FreeRtosUtils::get_tick_count() - start_time;
                        
                        if execution_time > wcet {
                            println!("WARNING: Task {} exceeded WCET!", task_spec.name);
                        }
                        
                        // Wait for next period
                        FreeRtosUtils::delay_until(&mut last_wake_time, period);
                    }
                })
                .unwrap();
        }
    }
}

// Standalone example
fn main() {
    let tasks = vec![
        PeriodicTask { name: "Task1", period_ms: 100, wcet_ms: 20, priority: 0 },
        PeriodicTask { name: "Task2", period_ms: 200, wcet_ms: 40, priority: 0 },
        PeriodicTask { name: "Task3", period_ms: 400, wcet_ms: 60, priority: 0 },
    ];
    
    let analyzer = RateMonotonicAnalyzer::new(tasks);
    analyzer.print_task_details();
    analyzer.analyze();
}
```

## Summary

**Rate Monotonic Analysis** is a powerful theoretical framework for ensuring real-time guarantees in FreeRTOS applications:

- **Priority Assignment**: Tasks with shorter periods receive higher priorities
- **Schedulability Bounds**: Utilization ≤ 69.3% guarantees schedulability for sufficient test
- **Analysis Methods**: Utilization bound test (sufficient), response time analysis (exact), and U > 1 (necessary)
- **Practical Application**: Validates task sets before deployment, preventing deadline misses in production
- **Limitations**: Assumes independent tasks with deadline = period; extensions exist for more complex scenarios

The technique provides mathematical proof that periodic tasks will meet their deadlines, making it essential for safety-critical and hard real-time systems. While conservative, it offers predictability and guarantees that empirical testing alone cannot provide.