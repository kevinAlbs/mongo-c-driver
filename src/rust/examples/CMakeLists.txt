add_executable (mongoc-async-example-insert-find EXCLUDE_FROM_ALL insert_find.c)
target_link_libraries (mongoc-async-example-insert-find PRIVATE mongoc-rust mongoc::shared)

add_executable (mongoc-async-bench-bulk-insert EXCLUDE_FROM_ALL bench_bulk_insert.c)
target_link_libraries (mongoc-async-bench-bulk-insert PRIVATE mongoc-rust mongoc::shared)
