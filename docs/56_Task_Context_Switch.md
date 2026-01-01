# FreeRTOS task context switch process 

**The Five-Step Process:**
1. **Interrupt/Tick** - SysTick timer or another interrupt triggers the switch
2. **Save Context** - Current task's CPU registers and state are saved to its stack
3. **Scheduler** - Selects the highest priority ready task
4. **Restore Context** - New task's registers are restored from its stack
5. **Resume** - Execution continues from where the new task left off

**Key Details Shown:**
- **Stack Operations**: How CPU registers (PC, PSR, R0-R12, LR) are pushed/popped
- **TCB Role**: Task Control Block maintains the stack pointer and task state
- **PendSV Handler**: The lowest-priority interrupt that performs the actual context switch
- **Task States**: Transitions between Running, Ready, and Blocked states

The context switch is fundamental to FreeRTOS's ability to provide multitasking on a single CPU core. It happens extremely quickly (microseconds) and is completely transparent to the running tasks.

```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>FreeRTOS Context Switch</title>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
            margin: 0;
            padding: 20px;
        }
        .container {
            background: white;
            border-radius: 20px;
            padding: 40px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            max-width: 1200px;
            width: 100%;
        }
        h1 {
            text-align: center;
            color: #333;
            margin-bottom: 10px;
        }
        .subtitle {
            text-align: center;
            color: #666;
            margin-bottom: 40px;
            font-size: 14px;
        }
        .diagram {
            position: relative;
            margin: 30px 0;
        }
        .phase {
            margin: 40px 0;
            padding: 20px;
            border-radius: 12px;
            background: #f8f9fa;
            border-left: 5px solid #667eea;
        }
        .phase-title {
            font-weight: bold;
            color: #667eea;
            font-size: 18px;
            margin-bottom: 15px;
        }
        .task-box {
            display: inline-block;
            padding: 15px 25px;
            margin: 10px;
            border-radius: 8px;
            font-weight: bold;
            text-align: center;
            min-width: 120px;
        }
        .task-running {
            background: #10b981;
            color: white;
            box-shadow: 0 4px 15px rgba(16, 185, 129, 0.4);
        }
        .task-ready {
            background: #3b82f6;
            color: white;
            box-shadow: 0 4px 15px rgba(59, 130, 246, 0.4);
        }
        .task-blocked {
            background: #6b7280;
            color: white;
        }
        .arrow {
            display: inline-block;
            margin: 0 15px;
            font-size: 24px;
            color: #667eea;
            vertical-align: middle;
        }
        .stack-visual {
            display: flex;
            justify-content: space-around;
            margin: 20px 0;
            flex-wrap: wrap;
        }
        .stack {
            border: 2px solid #667eea;
            border-radius: 8px;
            padding: 15px;
            background: white;
            margin: 10px;
            min-width: 200px;
        }
        .stack-title {
            font-weight: bold;
            margin-bottom: 10px;
            color: #667eea;
            text-align: center;
        }
        .stack-item {
            background: #e0e7ff;
            padding: 8px;
            margin: 5px 0;
            border-radius: 4px;
            font-size: 13px;
            text-align: center;
        }
        .register-state {
            background: #fef3c7;
            border-left: 3px solid #f59e0b;
        }
        .flow-container {
            display: flex;
            align-items: center;
            justify-content: center;
            flex-wrap: wrap;
            margin: 20px 0;
        }
        .step-box {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 20px;
            border-radius: 10px;
            margin: 10px;
            min-width: 180px;
            text-align: center;
            box-shadow: 0 4px 15px rgba(0,0,0,0.2);
        }
        .step-number {
            background: rgba(255,255,255,0.3);
            border-radius: 50%;
            width: 30px;
            height: 30px;
            display: inline-flex;
            align-items: center;
            justify-content: center;
            margin-bottom: 10px;
            font-weight: bold;
        }
        .legend {
            display: flex;
            justify-content: center;
            flex-wrap: wrap;
            margin-top: 30px;
            padding: 20px;
            background: #f8f9fa;
            border-radius: 10px;
        }
        .legend-item {
            margin: 5px 15px;
            display: flex;
            align-items: center;
        }
        .legend-color {
            width: 20px;
            height: 20px;
            border-radius: 4px;
            margin-right: 8px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>FreeRTOS Task Context Switch</h1>
        <div class="subtitle">Mechanism for switching CPU execution between tasks</div>

        <div class="phase">
            <div class="phase-title">🔄 Context Switch Overview</div>
            <div class="flow-container">
                <div class="step-box">
                    <div class="step-number">1</div>
                    <div>Interrupt or<br>Tick Occurs</div>
                </div>
                <span class="arrow">→</span>
                <div class="step-box">
                    <div class="step-number">2</div>
                    <div>Save Current<br>Task Context</div>
                </div>
                <span class="arrow">→</span>
                <div class="step-box">
                    <div class="step-number">3</div>
                    <div>Scheduler<br>Selects Task</div>
                </div>
                <span class="arrow">→</span>
                <div class="step-box">
                    <div class="step-number">4</div>
                    <div>Restore New<br>Task Context</div>
                </div>
                <span class="arrow">→</span>
                <div class="step-box">
                    <div class="step-number">5</div>
                    <div>Resume<br>Execution</div>
                </div>
            </div>
        </div>

        <div class="phase">
            <div class="phase-title">📊 Task State Transition</div>
            <div class="flow-container">
                <div class="task-box task-running">Task A<br>(Running)</div>
                <span class="arrow">→</span>
                <div class="task-box task-ready">Task A<br>(Ready)</div>
            </div>
            <div style="text-align: center; margin: 20px 0; font-weight: bold; color: #667eea;">
                CONTEXT SWITCH
            </div>
            <div class="flow-container">
                <div class="task-box task-ready">Task B<br>(Ready)</div>
                <span class="arrow">→</span>
                <div class="task-box task-running">Task B<br>(Running)</div>
            </div>
        </div>

        <div class="phase">
            <div class="phase-title">💾 Stack Context Saving & Restoration</div>
            <div class="stack-visual">
                <div class="stack">
                    <div class="stack-title">Task A Stack (Saved)</div>
                    <div class="stack-item register-state">PC (Program Counter)</div>
                    <div class="stack-item register-state">PSR (Status Register)</div>
                    <div class="stack-item register-state">R0-R3, R12</div>
                    <div class="stack-item register-state">R4-R11</div>
                    <div class="stack-item register-state">LR (Link Register)</div>
                    <div class="stack-item">Local Variables</div>
                    <div class="stack-item">Stack Pointer → TCB</div>
                </div>

                <div class="stack">
                    <div class="stack-title">Scheduler Decision</div>
                    <div class="stack-item" style="background: #ddd6fe;">Check Ready List</div>
                    <div class="stack-item" style="background: #ddd6fe;">Priority Selection</div>
                    <div class="stack-item" style="background: #ddd6fe;">Round-Robin (if equal)</div>
                    <div class="stack-item" style="background: #ddd6fe;">Update pxCurrentTCB</div>
                    <div class="stack-item" style="background: #ddd6fe;">→ Point to Task B</div>
                </div>

                <div class="stack">
                    <div class="stack-title">Task B Stack (Restored)</div>
                    <div class="stack-item">← Stack Pointer from TCB</div>
                    <div class="stack-item">Local Variables</div>
                    <div class="stack-item register-state">LR (Link Register)</div>
                    <div class="stack-item register-state">R4-R11</div>
                    <div class="stack-item register-state">R0-R3, R12</div>
                    <div class="stack-item register-state">PSR (Status Register)</div>
                    <div class="stack-item register-state">PC (Program Counter)</div>
                </div>
            </div>
        </div>

        <div class="phase">
            <div class="phase-title">⚙️ Key Components</div>
            <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 15px; margin-top: 15px;">
                <div style="padding: 15px; background: #e0e7ff; border-radius: 8px;">
                    <strong style="color: #667eea;">TCB (Task Control Block)</strong>
                    <ul style="margin: 10px 0; padding-left: 20px; font-size: 13px;">
                        <li>Stack pointer</li>
                        <li>Task priority</li>
                        <li>Task state</li>
                        <li>Task name</li>
                    </ul>
                </div>
                <div style="padding: 15px; background: #fef3c7; border-radius: 8px;">
                    <strong style="color: #f59e0b;">PendSV Handler</strong>
                    <ul style="margin: 10px 0; padding-left: 20px; font-size: 13px;">
                        <li>Lowest priority interrupt</li>
                        <li>Performs context switch</li>
                        <li>Called by scheduler</li>
                        <li>Saves/restores context</li>
                    </ul>
                </div>
                <div style="padding: 15px; background: #d1fae5; border-radius: 8px;">
                    <strong style="color: #10b981;">SysTick Timer</strong>
                    <ul style="margin: 10px 0; padding-left: 20px; font-size: 13px;">
                        <li>Periodic tick interrupt</li>
                        <li>Triggers scheduler</li>
                        <li>Time slice management</li>
                        <li>Delay handling</li>
                    </ul>
                </div>
            </div>
        </div>

        <div class="legend">
            <div class="legend-item">
                <div class="legend-color" style="background: #10b981;"></div>
                <span>Running Task</span>
            </div>
            <div class="legend-item">
                <div class="legend-color" style="background: #3b82f6;"></div>
                <span>Ready Task</span>
            </div>
            <div class="legend-item">
                <div class="legend-color" style="background: #6b7280;"></div>
                <span>Blocked Task</span>
            </div>
            <div class="legend-item">
                <div class="legend-color" style="background: #fef3c7;"></div>
                <span>CPU Registers</span>
            </div>
        </div>
    </div>
</body>
</html>
```
