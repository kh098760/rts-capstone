# Tasks & timing (Variable Execution Time = Best / Worse)
| Task |  BCET B  | WCET W | VET=B/W | Priority | 
|------|---------:|-------:|--------:|---------:|
| Prod |   50ms   |  50ms  |    1    |     8    |    
| Cons |   40ms   |  100ms |   0.4   |     8    |
| Cord |   90ms   |  150ms |   0.6   |     9    |
| Resp |   90ms   |  150ms |   0.6   |    12    |

Avg execution time variation = 0.65
Where towards 1 means execution time tends to be consistent with BCET and toward to 0 means execution time varies widely between BCET and WCET. This does not mean the rate of variation for ET, but the overall magnitude of difference with respect for the ETs.

Evidence of the times are based on the delays for each task and how it compounds for the next task. The producer always outputs data items at 50ms. The consumer can, at best, consume at 40ms or at worst, 100ms. This means the coordinator that relies on these two tasks takes anywhere from 90ms (best of prod and cons) or 150ms (worst of prod and cons) to coordinate with the responder. The responder is simply blocking until a notification is received from the coordinator, so it follows the same time as the coordinator. 
