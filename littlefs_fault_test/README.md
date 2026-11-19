# Standalone byteREEL LittleFS fault test

This sketch is intended only for a spare ESP32/flash with the same 4 MiB Huge
APP partition layout. Build it with:

```sh
sh littlefs_fault_test/build.sh
```

Uploading and running the test are manual and are not part of the build script.
The sketch mounts only with `begin(false, ...)`, rejects flash smaller than
4 MiB, and refuses to continue if `/br26-fault-test` already exists. It never
formats the filesystem and uses only its own temporary directory, which it
removes at the end.

The test runs commits through production `br_commit_slot` and the same
`BrLittleFsStore` adapter used by firmware. It injects truncation at each byte
of the inactive candidate, remounts, and verifies that the previous complete
pair remains selected. It then simulates lost acknowledgement after a full
write and verifies the new generation. It does **not** physically cut power.
ENOSPC is approximated with short writes; filling a user filesystem is
deliberately avoided. Mount errors are reported without formatting.

Physical power-cut and ENOSPC testing requires a separate, backed-up test
flash. Do not run those tests on the only copy of user slots.
