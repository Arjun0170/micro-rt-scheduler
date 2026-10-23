# Design notes

## Scheduler

The 1 ms tick ISR only increments a counter. The foreground dispatcher scans a
fixed six-entry task table, releases due periodic jobs, chooses the ready job
with the lowest numeric priority, and runs one callback. There is no dynamic
allocation. Each task has a period, initial phase, relative deadline, and
separate counters for missed releases and missed deadlines.

Release times use unsigned 32-bit arithmetic; due checks cast the difference
to signed 32-bit. This is valid when a deadline or release is less than 2^31 ms
away and the dispatcher is called often enough to avoid a half-range ambiguity.
If multiple periods elapse before dispatch, the scheduler coalesces them into
one pending job and counts the skipped releases. It never tries to replay stale
jobs in a burst.

The event queue is a 16-byte ring with 15 usable slots. The ISR writes the byte
before publishing the head index. The foreground reads a byte before moving
the tail. Overflow drops the new event and increments a counter. This design
has one producer and one consumer; multiple producer ISRs would need a critical
section or a different queue. Compiler signal fences keep payload access on
the correct side of head/tail publication; the design still assumes the target
provides atomic 8-bit index accesses.

## Control loop

The PI controller uses Q8 gains: `gain / 256` converts the stored integer to
the effective gain. Temperature uses centi-degrees C; output uses 0 to 1000
permille. `int64_t` intermediates protect the gain and error multiplication.
The integral is bounded to the output range and is held when it would push an
already saturated output further into saturation. A fault latches the output
to zero until explicitly cleared.

The simulator runs a 20 ms safety task, a 10 ms control task, and a 100 ms
status task. Safety has the highest priority. One callback runs per simulated
millisecond. If callbacks took longer than the available CPU time on a real
target, the counters would expose missed releases or deadlines, but that
requires target timing measurements.
