# rts-capstone - Real-Time Systems Final Capstone

## Theme
A real-time system that works, built to demonstrate attitude process delays and how it affects queues for an avionics role.

## Demo
- Video: <YouTube / Wokwi link>
- Live Wokwi: ELHAOUAJI-FINAL-RTS26Summer (https://wokwi.com/projects/471033187914654721)

## Architecture
![Architecture Diagram](docs/architecture.svg)

Data flows from production task to consumer task through a 10 item queue. The queue's overflow policy is dropping the incoming packet until the queue depth has decreased. The coordinator and responder tasks are linked the task notifications, where the coordinator processes the bits set by production and consumer tasks to give the task notification to the responder.

## Tasks & timing (WCET evidence)
| Task |  BCET B  | WCET W | VET=B/W | Priority | 
|------|---------:|-------:|--------:|---------:|
| Prod |   50ms   |  50ms  |    1    |     8    |    
| Cons |   40ms   |  100ms |   0.4   |     8    |
| Cord |   90ms   |  150ms |   0.6   |     9    |
| Resp |   90ms   |  150ms |   0.6   |    12    |

Avg execution time variation = 0.65
Where towards 1 means execution time tends to be consistent with BCET and toward to 0 means execution time varies widely between BCET and WCET. This does not mean the rate of variation for ET, but the overall magnitude of difference with respect for the ETs.


## Hazard analysis & standard mapping
<hazard, effect, mitigation; mapped to the standard clause>

## Graceful degradation
<what fails, how it is detected, what the system does instead>

## Build & run
<toolchain, board, how to reproduce>

## Tailored for
<target role> — <why these choices fit that role>
