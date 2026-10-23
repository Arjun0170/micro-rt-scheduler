# Test plan

Run the commands in the README. `ctest` must report one passing suite.

| Case | Procedure | Expected |
| --- | --- | --- |
| Priority | Release two jobs at the same tick | Higher-priority callback runs first |
| Missed work | Advance ticks without dispatch | Skipped releases and late deadlines counted |
| Tick wrap | Start near `UINT32_MAX` | Job releases after wrap at the intended tick |
| Queue capacity | Post 16 events before consuming | First 15 preserved, last dropped and counted |
| PI saturation | Apply a large positive error repeatedly | Output at most 1000, integral does not wind up |
| Fault latch | Latch, step, clear, step | Output zero while latched, resumes after clear |
| Invalid measurement | Use out-of-range temperature | Controller latches fault and output becomes zero |

The simulation trace should show output zero during the injected fault interval
and no deadline misses under the configured virtual workload. Those are
simulation observations, not hardware timing measurements.
