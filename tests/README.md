# Wrapper Host Tests

These executables compile the actual `src/ESP32-TWAI-CAN.cpp` and inline public
methods twice. Only the ESP-IDF/FreeRTOS boundary is stubbed. No production
test hooks, alternative wrapper algorithm, Arduino framework or device is used.

From the Gauge.S root on Windows:

```powershell
cmake -S lib/ESP32-TWAI-CAN/tests -B debug/twai-wrapper-host -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER="C:/msys64/ucrt64/bin/g++.exe" -DCMAKE_MAKE_PROGRAM="C:/msys64/ucrt64/bin/mingw32-make.exe"
cmake --build debug/twai-wrapper-host --parallel 4
ctest --test-dir debug/twai-wrapper-host --output-on-failure
```

On a native GCC/Clang host, configure with `cmake -S tests -B debug/host` from
the library root; build and run CTest against that directory. Tests use C++17.

## Coverage

- Legacy installation/start failure, a foreign installed driver, stopped/bus-off
  cleanup, uninstall failure/retry, and rejection of teardown during recovery.
- Recovery/restart true/false results and failed status reads.
- Exact infinite-wait sentinel preservation; zero and ordinary millisecond
  waits; pointer/reference overloads and null frame pointers.
- Pins, queue-size sentinels, legacy acceptance filters, timing selection,
  and named frame fields (not binary equivalence across driver paths).
- New-node, shadow allocation, RX queue allocation, callback registration and
  enable failures. Resource counters check unwind before successful retry.
  RX allocation failure must neither register callbacks nor enable the node.
- Failed new-node deletion retains callback storage until successful deletion.
- All diagnostic getters after each failed initialization, and after failed
  node deletion with missing shadow/RX allocations or a pending TX slot. The
  added shadow-allocation cases reproduced two null-pointer crashes before the
  diagnostic guard/count-reset fix.
- Legacy caller-owned status snapshots, direct caller-buffer forwarding,
  independent snapshots, null pointers, SDK failure and absent-driver results.
- Compile-time driver-selection assertions with new headers present: SOC1 must
  remain legacy, SOC2 must select the new driver. These assertions are also
  compiled against the real SDK, not just host stubs.
- Recovery completion followed immediately by another bus-off without an
  intervening status poll, using the registered state-change callback.
- TX payload lifetime, no busy-slot reuse, out-of-order successful/failed
  completions, rejected-write release, diagnostics, and eight concurrent host
  writers competing for four slots. Driver acceptance is not wire completion.
- RX observer context/direction/time, short payload zero-fill, and RTR payloads.

The original `dd9fc881b3fe68dcb3087a1a2a1e3cf361138b7d` wrapper was tested
before modification: 11 of 20 cases failed. Those failures covered legacy
ownership/retries/sentinels and new-driver RX allocation, delete failure, and
repeated bus-off. Existing TX ownership cases passed before and after the fixes.

## Real SDK Compile Check

An existing CanBridge C6 IDF 5.5+ compilation database can check both branches
against the installed SDK, without running PIO or modifying the neighbor:

```powershell
python lib/ESP32-TWAI-CAN/tests/compile_sdk.py ../CanBridge.S/.pio/build/canbridge-v10/compile_commands.json debug/twai-wrapper-sdk
```

The script substitutes this checkout's wrapper source, redirects object output,
and removes dependency-file generation from the copied compiler command. The
legacy pass overrides only the controller-count capability macro. A separate
translation unit asserts the selected branch and compiles the legacy snapshot
call against the real SDK types. It does not
compile the host SDK stubs. The database must already exist; this script never
generates it or changes the SDK. It expects Windows GCC command-line quoting.

## Limits

Stub queues do not wait, SDK failure results are injected, and state/RX/TX IRQs
are dispatched deterministically from the test thread. Host mutexes protect
real concurrent writers but do not emulate interrupt masking, FreeRTOS
scheduling, hardware register races, cache-off execution, or IRAM placement.
The SDK stubs are an API subset, not a replacement for a target build. The real
SDK compile check uses C6 flags, not the Gauge.S Xtensa/Arduino build.

Lifecycle/configuration still requires serialization against all frame I/O.
The suite does not certify deleting a queue with a task blocked on it. The new
path's exhausted shadow pool still rejects immediately even with an infinite
timeout; only the SDK submission receives the timeout after a slot is claimed.
The SDK second-chance TX queue backport remains the consuming app's concern.
