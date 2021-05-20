gdbmon.py 98025 10 5 (version 1.2)

=== 2021-05-19T19:33:05.367609+0000 

Thread 13 (Thread 0x7f4f30ff9700 (LWP 98038)):
#0  0x00007f4f40e94065 in futex_abstimed_wait_cancelable (private=<optimized out>, abstime=0x7f4f30ff8990, expected=0, futex_word=0x7f4f2c0042d0) at ../sysdeps/unix/sysv/linux/futex-internal.h:205
#1  __pthread_cond_wait_common (abstime=0x7f4f30ff8990, mutex=0x7f4f2c004280, cond=0x7f4f2c0042a8) at pthread_cond_wait.c:539
#2  __pthread_cond_timedwait (cond=0x7f4f2c0042a8, mutex=0x7f4f2c004280, abstime=0x7f4f30ff8990) at pthread_cond_wait.c:667
#3  0x00007f4f4134a393 in mongoc_server_monitor_wait () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f4134a650 in _server_monitor_rtt_thread () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f40e8d6db in start_thread (arg=0x7f4f30ff9700) at pthread_create.c:463
#6  0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 12 (Thread 0x7f4f317fa700 (LWP 98036)):
#0  bson_has_field (bson=0x7f4f317f9800, key=0x7f4f41373bae \"$db\") at /home/ubuntu/mongo-c-driver/src/libbson/src/bson/bson.c:2491
#1  0x00007f4f41339b8e in mongoc_cmd_parts_assemble () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f4f41325d60 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x000055a67a1c0446 in thread_find ()
#7  0x00007f4f40e8d6db in start_thread (arg=0x7f4f317fa700) at pthread_create.c:463
#8  0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 11 (Thread 0x7f4f31ffb700 (LWP 98035)):
#0  0x00007f4f40ba9cb9 in __GI___poll (fds=0x7f4f31ff9330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f4f4134be71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f4f4134ef47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f4134d66b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41308324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f4134cb9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f4134d66b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41308082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f41313689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f4f41314777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f4f41325d88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x000055a67a1c0446 in thread_find ()
#15 0x00007f4f40e8d6db in start_thread (arg=0x7f4f31ffb700) at pthread_create.c:463
#16 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 10 (Thread 0x7f4f2bfff700 (LWP 98034)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f4f40e90025 in __GI___pthread_mutex_lock (mutex=0x55a67b840658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f4f41350820 in mongoc_topology_select_server_id () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f41318272 in mongoc_cluster_stream_for_reads () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f413253cf in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41328c63 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x000055a67a1c0446 in thread_find ()
#8  0x00007f4f40e8d6db in start_thread (arg=0x7f4f2bfff700) at pthread_create.c:463
#9  0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 9 (Thread 0x7f4f327fc700 (LWP 98033)):
#0  __lll_unlock_wake () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:371
#1  0x00007f4f40e91852 in __pthread_mutex_unlock_usercnt (decr=1, mutex=0x55a67b840658) at pthread_mutex_unlock.c:56
#2  __GI___pthread_mutex_unlock (mutex=0x55a67b840658) at pthread_mutex_unlock.c:356
#3  0x00007f4f41315617 in _mongoc_cluster_create_server_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41317225 in _mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f4131795d in mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f41325395 in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f413259ed in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x000055a67a1c0446 in thread_find ()
#12 0x00007f4f40e8d6db in start_thread (arg=0x7f4f327fc700) at pthread_create.c:463
#13 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 8 (Thread 0x7f4f32ffd700 (LWP 98032)):
#0  0x00007f4f40ba9cb9 in __GI___poll (fds=0x7f4f32ffb330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f4f4134be71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f4f4134ef47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f4134d66b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41308324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f4134cb9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f4134d66b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41308082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f41313689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f4f41314777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f4f41325d88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x000055a67a1c0446 in thread_find ()
#15 0x00007f4f40e8d6db in start_thread (arg=0x7f4f32ffd700) at pthread_create.c:463
#16 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 7 (Thread 0x7f4f337fe700 (LWP 98031)):
#0  0x00007f4f40ba9cb9 in __GI___poll (fds=0x7f4f2c0063f0, nfds=1, timeout=500) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f4f4134b143 in mongoc_socket_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f4f4134ec1e in _mongoc_stream_socket_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f4134d959 in mongoc_stream_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f4134947a in _server_monitor_awaitable_ismaster_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f413499ea in mongoc_server_monitor_check_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f4134a462 in _server_monitor_thread () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f40e8d6db in start_thread (arg=0x7f4f337fe700) at pthread_create.c:463
#8  0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 6 (Thread 0x7f4f33fff700 (LWP 98030)):
#0  0x00007f4f40e986f7 in __libc_sendmsg (fd=12, msg=0x7f4f33ffd590, flags=16384) at ../sysdeps/unix/sysv/linux/sendmsg.c:28
#1  0x00007f4f4134c1a1 in mongoc_socket_sendv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f4f4134f214 in _mongoc_stream_socket_writev () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f4134dad4 in _mongoc_stream_writev_full () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f4131365c in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41314777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f41325d88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x000055a67a1c0446 in thread_find ()
#11 0x00007f4f40e8d6db in start_thread (arg=0x7f4f33fff700) at pthread_create.c:463
#12 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 5 (Thread 0x7f4f38bfa700 (LWP 98029)):
#0  0x00007f4f40ba9cb9 in __GI___poll (fds=0x7f4f38bf8330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f4f4134be71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f4f4134ef47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f4134d66b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41308324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f4134cb9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f4134d66b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41308082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f41313689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f4f41314777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f4f41325d88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x000055a67a1c0446 in thread_find ()
#15 0x00007f4f40e8d6db in start_thread (arg=0x7f4f38bfa700) at pthread_create.c:463
#16 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 4 (Thread 0x7f4f393fb700 (LWP 98028)):
#0  0x00007f4f41302ab0 in mongoc_set_rm@plt () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#1  0x00007f4f41348c2c in mongoc_client_session_destroy () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f4f41327a25 in _mongoc_cursor_start_reading_response () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f41327c4b in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x000055a67a1c0446 in thread_find ()
#7  0x00007f4f40e8d6db in start_thread (arg=0x7f4f393fb700) at pthread_create.c:463
#8  0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 3 (Thread 0x7f4f39bfc700 (LWP 98027)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f4f40e90025 in __GI___pthread_mutex_lock (mutex=0x55a67b840658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f4f41316be9 in _mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f4131795d in mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41325395 in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f413259ed in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x000055a67a1c0446 in thread_find ()
#10 0x00007f4f40e8d6db in start_thread (arg=0x7f4f39bfc700) at pthread_create.c:463
#11 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 2 (Thread 0x7f4f3a3fd700 (LWP 98026)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f4f40e90025 in __GI___pthread_mutex_lock (mutex=0x55a67b840658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f4f41350ffd in _mongoc_topology_pop_server_session () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f4130f966 in mongoc_client_start_session () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41325eff in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x000055a67a1c0446 in thread_find ()
#9  0x00007f4f40e8d6db in start_thread (arg=0x7f4f3a3fd700) at pthread_create.c:463
#10 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 1 (Thread 0x7f4f417a5740 (LWP 98025)):
#0  0x00007f4f40e8ed2d in __GI___pthread_timedjoin_ex (threadid=139978256406272, thread_return=0x0, abstime=0x0, block=<optimized out>) at pthread_join_common.c:89
#1  0x000055a67a1c0137 in main ()

times: stop 0.003 traces 0.166 cont 0.000

=== 2021-05-19T19:33:15.442787+0000 

Thread 13 (Thread 0x7f4f30ff9700 (LWP 98038)):
#0  0x00007f4f40e94065 in futex_abstimed_wait_cancelable (private=<optimized out>, abstime=0x7f4f30ff8990, expected=0, futex_word=0x7f4f2c0042d0) at ../sysdeps/unix/sysv/linux/futex-internal.h:205
#1  __pthread_cond_wait_common (abstime=0x7f4f30ff8990, mutex=0x7f4f2c004280, cond=0x7f4f2c0042a8) at pthread_cond_wait.c:539
#2  __pthread_cond_timedwait (cond=0x7f4f2c0042a8, mutex=0x7f4f2c004280, abstime=0x7f4f30ff8990) at pthread_cond_wait.c:667
#3  0x00007f4f4134a393 in mongoc_server_monitor_wait () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f4134a650 in _server_monitor_rtt_thread () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f40e8d6db in start_thread (arg=0x7f4f30ff9700) at pthread_create.c:463
#6  0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 12 (Thread 0x7f4f317fa700 (LWP 98036)):
#0  0x00007f4f40ba9cb9 in __GI___poll (fds=0x7f4f317f8330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f4f4134be71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f4f4134ef47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f4134d66b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41308324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f4134cb9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f4134d66b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41308082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f41313689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f4f41314777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f4f41325d88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x000055a67a1c0446 in thread_find ()
#15 0x00007f4f40e8d6db in start_thread (arg=0x7f4f317fa700) at pthread_create.c:463
#16 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 11 (Thread 0x7f4f31ffb700 (LWP 98035)):
#0  0x00007f4f40ba9cb9 in __GI___poll (fds=0x7f4f31ff9330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f4f4134be71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f4f4134ef47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f4134d66b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41308324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f4134cb9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f4134d66b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41308082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f41313689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f4f41314777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f4f41325d88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x000055a67a1c0446 in thread_find ()
#15 0x00007f4f40e8d6db in start_thread (arg=0x7f4f31ffb700) at pthread_create.c:463
#16 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 10 (Thread 0x7f4f2bfff700 (LWP 98034)):
#0  0x00007f4f40e986f7 in __libc_sendmsg (fd=16, msg=0x7f4f2bffd590, flags=16384) at ../sysdeps/unix/sysv/linux/sendmsg.c:28
#1  0x00007f4f4134c1a1 in mongoc_socket_sendv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f4f4134f214 in _mongoc_stream_socket_writev () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f4134dad4 in _mongoc_stream_writev_full () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f4131365c in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41314777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f41325d88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x000055a67a1c0446 in thread_find ()
#11 0x00007f4f40e8d6db in start_thread (arg=0x7f4f2bfff700) at pthread_create.c:463
#12 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 9 (Thread 0x7f4f327fc700 (LWP 98033)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f4f40e90025 in __GI___pthread_mutex_lock (mutex=0x55a67b840658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f4f4131489b in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f41325d88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x000055a67a1c0446 in thread_find ()
#8  0x00007f4f40e8d6db in start_thread (arg=0x7f4f327fc700) at pthread_create.c:463
#9  0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 8 (Thread 0x7f4f32ffd700 (LWP 98032)):
#0  0x00007ffeb9df9b62 in clock_gettime ()
#1  0x00007f4f40bc5d36 in __GI___clock_gettime (clock_id=clock_id@entry=1, tp=tp@entry=0x7f4f32ffc000) at ../sysdeps/unix/clock_gettime.c:115
#2  0x00007f4f410b9d61 in bson_get_monotonic_time () at /home/ubuntu/mongo-c-driver/src/libbson/src/bson/bson-clock.c:126
#3  0x00007f4f41343fef in mongoc_server_description_reset () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41345bf0 in mongoc_server_description_handle_ismaster () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41345e59 in mongoc_server_description_new_copy () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f413155f9 in _mongoc_cluster_create_server_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41317225 in _mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f413253cf in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f4f41328c63 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x000055a67a1c0446 in thread_find ()
#12 0x00007f4f40e8d6db in start_thread (arg=0x7f4f32ffd700) at pthread_create.c:463
#13 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 7 (Thread 0x7f4f337fe700 (LWP 98031)):
#0  0x00007f4f40ba9cb9 in __GI___poll (fds=0x7f4f2c0063f0, nfds=1, timeout=500) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f4f4134b143 in mongoc_socket_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f4f4134ec1e in _mongoc_stream_socket_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f4134d959 in mongoc_stream_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f4134947a in _server_monitor_awaitable_ismaster_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f413499ea in mongoc_server_monitor_check_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f4134a462 in _server_monitor_thread () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f40e8d6db in start_thread (arg=0x7f4f337fe700) at pthread_create.c:463
#8  0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 6 (Thread 0x7f4f33fff700 (LWP 98030)):
#0  0x00007f4f40ba9cb9 in __GI___poll (fds=0x7f4f33ffd330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f4f4134be71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f4f4134ef47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f4134d66b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41308324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f4134cb9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f4134d66b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41308082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f41313689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f4f41314777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f4f41325d88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x000055a67a1c0446 in thread_find ()
#15 0x00007f4f40e8d6db in start_thread (arg=0x7f4f33fff700) at pthread_create.c:463
#16 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 5 (Thread 0x7f4f38bfa700 (LWP 98029)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f4f40e90025 in __GI___pthread_mutex_lock (mutex=0x55a67b840658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f4f4131489b in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f41325d88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x000055a67a1c0446 in thread_find ()
#8  0x00007f4f40e8d6db in start_thread (arg=0x7f4f38bfa700) at pthread_create.c:463
#9  0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 4 (Thread 0x7f4f393fb700 (LWP 98028)):
#0  __lll_unlock_wake () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:371
#1  0x00007f4f40e91852 in __pthread_mutex_unlock_usercnt (decr=1, mutex=0x55a67b840658) at pthread_mutex_unlock.c:56
#2  __GI___pthread_mutex_unlock (mutex=0x55a67b840658) at pthread_mutex_unlock.c:356
#3  0x00007f4f41348c38 in mongoc_client_session_destroy () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41327a25 in _mongoc_cursor_start_reading_response () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41327c4b in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x000055a67a1c0446 in thread_find ()
#9  0x00007f4f40e8d6db in start_thread (arg=0x7f4f393fb700) at pthread_create.c:463
#10 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 3 (Thread 0x7f4f39bfc700 (LWP 98027)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f4f40e90025 in __GI___pthread_mutex_lock (mutex=0x55a67b840658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f4f41351161 in _mongoc_topology_push_server_session () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f41348c38 in mongoc_client_session_destroy () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41327a25 in _mongoc_cursor_start_reading_response () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41327c4b in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x000055a67a1c0446 in thread_find ()
#9  0x00007f4f40e8d6db in start_thread (arg=0x7f4f39bfc700) at pthread_create.c:463
#10 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 2 (Thread 0x7f4f3a3fd700 (LWP 98026)):
#0  0x00007f4f40ba9cb9 in __GI___poll (fds=0x7f4f3a3fb330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f4f4134be71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f4f4134ef47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f4134d66b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41308324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f4134cb9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f4134d66b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41308082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f41313689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f4f41314777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f4f41325d88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x000055a67a1c0446 in thread_find ()
#15 0x00007f4f40e8d6db in start_thread (arg=0x7f4f3a3fd700) at pthread_create.c:463
#16 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 1 (Thread 0x7f4f417a5740 (LWP 98025)):
#0  0x00007f4f40e8ed2d in __GI___pthread_timedjoin_ex (threadid=139978256406272, thread_return=0x0, abstime=0x0, block=<optimized out>) at pthread_join_common.c:89
#1  0x000055a67a1c0137 in main ()

times: stop 0.003 traces 0.067 cont 0.000

=== 2021-05-19T19:33:25.522837+0000 

Thread 13 (Thread 0x7f4f30ff9700 (LWP 98038)):
#0  0x00007f4f40e94065 in futex_abstimed_wait_cancelable (private=<optimized out>, abstime=0x7f4f30ff8990, expected=0, futex_word=0x7f4f2c0042d0) at ../sysdeps/unix/sysv/linux/futex-internal.h:205
#1  __pthread_cond_wait_common (abstime=0x7f4f30ff8990, mutex=0x7f4f2c004280, cond=0x7f4f2c0042a8) at pthread_cond_wait.c:539
#2  __pthread_cond_timedwait (cond=0x7f4f2c0042a8, mutex=0x7f4f2c004280, abstime=0x7f4f30ff8990) at pthread_cond_wait.c:667
#3  0x00007f4f4134a393 in mongoc_server_monitor_wait () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f4134a650 in _server_monitor_rtt_thread () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f40e8d6db in start_thread (arg=0x7f4f30ff9700) at pthread_create.c:463
#6  0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 12 (Thread 0x7f4f317fa700 (LWP 98036)):
#0  0x00007ffeb9df9b62 in clock_gettime ()
#1  0x00007f4f40bc5d36 in __GI___clock_gettime (clock_id=clock_id@entry=1, tp=tp@entry=0x7f4f317f90e0) at ../sysdeps/unix/clock_gettime.c:115
#2  0x00007f4f410b9d61 in bson_get_monotonic_time () at /home/ubuntu/mongo-c-driver/src/libbson/src/bson/bson-clock.c:126
#3  0x00007f4f41346ac2 in _mongoc_server_session_timed_out () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41351043 in _mongoc_topology_pop_server_session () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f4130f966 in mongoc_client_start_session () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f41325eff in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x000055a67a1c0446 in thread_find ()
#11 0x00007f4f40e8d6db in start_thread (arg=0x7f4f317fa700) at pthread_create.c:463
#12 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 11 (Thread 0x7f4f31ffb700 (LWP 98035)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f4f40e90025 in __GI___pthread_mutex_lock (mutex=0x55a67b840658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f4f41350820 in mongoc_topology_select_server_id () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f41318272 in mongoc_cluster_stream_for_reads () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f413253cf in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41328c63 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x000055a67a1c0446 in thread_find ()
#8  0x00007f4f40e8d6db in start_thread (arg=0x7f4f31ffb700) at pthread_create.c:463
#9  0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 10 (Thread 0x7f4f2bfff700 (LWP 98034)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f4f40e90025 in __GI___pthread_mutex_lock (mutex=0x55a67b840658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f4f41350745 in mongoc_topology_select_server_id () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f41318272 in mongoc_cluster_stream_for_reads () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f413253cf in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41328c63 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x000055a67a1c0446 in thread_find ()
#8  0x00007f4f40e8d6db in start_thread (arg=0x7f4f2bfff700) at pthread_create.c:463
#9  0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 9 (Thread 0x7f4f327fc700 (LWP 98033)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f4f40e90025 in __GI___pthread_mutex_lock (mutex=0x55a67b840658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f4f41351161 in _mongoc_topology_push_server_session () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f41348c38 in mongoc_client_session_destroy () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41327a25 in _mongoc_cursor_start_reading_response () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41327c4b in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x000055a67a1c0446 in thread_find ()
#9  0x00007f4f40e8d6db in start_thread (arg=0x7f4f327fc700) at pthread_create.c:463
#10 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 8 (Thread 0x7f4f32ffd700 (LWP 98032)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f4f40e90025 in __GI___pthread_mutex_lock (mutex=0x55a67b840658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f4f41316be9 in _mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f4131795d in mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41325395 in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f413259ed in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x000055a67a1c0446 in thread_find ()
#10 0x00007f4f40e8d6db in start_thread (arg=0x7f4f32ffd700) at pthread_create.c:463
#11 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 7 (Thread 0x7f4f337fe700 (LWP 98031)):
#0  0x00007f4f40ba9cb9 in __GI___poll (fds=0x7f4f2c0063f0, nfds=1, timeout=500) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f4f4134b143 in mongoc_socket_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f4f4134ec1e in _mongoc_stream_socket_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f4134d959 in mongoc_stream_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f4134947a in _server_monitor_awaitable_ismaster_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f413499ea in mongoc_server_monitor_check_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f4134a462 in _server_monitor_thread () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f40e8d6db in start_thread (arg=0x7f4f337fe700) at pthread_create.c:463
#8  0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 6 (Thread 0x7f4f33fff700 (LWP 98030)):
#0  0x00007f4f40e986f7 in __libc_sendmsg (fd=12, msg=0x7f4f33ffd590, flags=16384) at ../sysdeps/unix/sysv/linux/sendmsg.c:28
#1  0x00007f4f4134c1a1 in mongoc_socket_sendv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f4f4134f214 in _mongoc_stream_socket_writev () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f4134dad4 in _mongoc_stream_writev_full () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f4131365c in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41314777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f41325d88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x000055a67a1c0446 in thread_find ()
#11 0x00007f4f40e8d6db in start_thread (arg=0x7f4f33fff700) at pthread_create.c:463
#12 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 5 (Thread 0x7f4f38bfa700 (LWP 98029)):
#0  0x00007f4f40ba9cb9 in __GI___poll (fds=0x7f4f38bf8330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f4f4134be71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f4f4134ef47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f4134d66b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41308324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f4134cb9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f4134d66b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41308082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f41313689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f4f41314777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f4f41325d88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x000055a67a1c0446 in thread_find ()
#15 0x00007f4f40e8d6db in start_thread (arg=0x7f4f38bfa700) at pthread_create.c:463
#16 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 4 (Thread 0x7f4f393fb700 (LWP 98028)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f4f40e90025 in __GI___pthread_mutex_lock (mutex=0x55a67b840658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f4f41316be9 in _mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f4131795d in mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41325395 in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f413259ed in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x000055a67a1c0446 in thread_find ()
#10 0x00007f4f40e8d6db in start_thread (arg=0x7f4f393fb700) at pthread_create.c:463
#11 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 3 (Thread 0x7f4f39bfc700 (LWP 98027)):
#0  __lll_unlock_wake () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:371
#1  0x00007f4f40e91852 in __pthread_mutex_unlock_usercnt (decr=1, mutex=0x55a67b840658) at pthread_mutex_unlock.c:56
#2  __GI___pthread_mutex_unlock (mutex=0x55a67b840658) at pthread_mutex_unlock.c:356
#3  0x00007f4f41315617 in _mongoc_cluster_create_server_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41317225 in _mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f4131795d in mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f41325395 in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f413259ed in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x000055a67a1c0446 in thread_find ()
#12 0x00007f4f40e8d6db in start_thread (arg=0x7f4f39bfc700) at pthread_create.c:463
#13 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 2 (Thread 0x7f4f3a3fd700 (LWP 98026)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f4f40e90025 in __GI___pthread_mutex_lock (mutex=0x55a67b840658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f4f41351161 in _mongoc_topology_push_server_session () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f41348c38 in mongoc_client_session_destroy () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41327a25 in _mongoc_cursor_start_reading_response () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41327c4b in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x000055a67a1c0446 in thread_find ()
#9  0x00007f4f40e8d6db in start_thread (arg=0x7f4f3a3fd700) at pthread_create.c:463
#10 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 1 (Thread 0x7f4f417a5740 (LWP 98025)):
#0  0x00007f4f40e8ed2d in __GI___pthread_timedjoin_ex (threadid=139978256406272, thread_return=0x0, abstime=0x0, block=<optimized out>) at pthread_join_common.c:89
#1  0x000055a67a1c0137 in main ()

times: stop 0.003 traces 0.043 cont 0.000

=== 2021-05-19T19:33:35.578772+0000 

Thread 13 (Thread 0x7f4f30ff9700 (LWP 98038)):
#0  0x00007f4f40e94065 in futex_abstimed_wait_cancelable (private=<optimized out>, abstime=0x7f4f30ff8990, expected=0, futex_word=0x7f4f2c0042d0) at ../sysdeps/unix/sysv/linux/futex-internal.h:205
#1  __pthread_cond_wait_common (abstime=0x7f4f30ff8990, mutex=0x7f4f2c004280, cond=0x7f4f2c0042a8) at pthread_cond_wait.c:539
#2  __pthread_cond_timedwait (cond=0x7f4f2c0042a8, mutex=0x7f4f2c004280, abstime=0x7f4f30ff8990) at pthread_cond_wait.c:667
#3  0x00007f4f4134a393 in mongoc_server_monitor_wait () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f4134a650 in _server_monitor_rtt_thread () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f40e8d6db in start_thread (arg=0x7f4f30ff9700) at pthread_create.c:463
#6  0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 12 (Thread 0x7f4f317fa700 (LWP 98036)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f4f40e90025 in __GI___pthread_mutex_lock (mutex=0x55a67b840658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f4f413155e3 in _mongoc_cluster_create_server_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f41317225 in _mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f413253cf in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41328c63 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x000055a67a1c0446 in thread_find ()
#8  0x00007f4f40e8d6db in start_thread (arg=0x7f4f317fa700) at pthread_create.c:463
#9  0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 11 (Thread 0x7f4f31ffb700 (LWP 98035)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f4f40e90025 in __GI___pthread_mutex_lock (mutex=0x55a67b840658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f4f41350fb0 in _mongoc_topology_update_cluster_time () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f41313a18 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41314777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41325d88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x000055a67a1c0446 in thread_find ()
#10 0x00007f4f40e8d6db in start_thread (arg=0x7f4f31ffb700) at pthread_create.c:463
#11 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 10 (Thread 0x7f4f2bfff700 (LWP 98034)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f4f40e90025 in __GI___pthread_mutex_lock (mutex=0x55a67b840658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f4f41350fb0 in _mongoc_topology_update_cluster_time () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f41313a18 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41314777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41325d88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x000055a67a1c0446 in thread_find ()
#10 0x00007f4f40e8d6db in start_thread (arg=0x7f4f2bfff700) at pthread_create.c:463
#11 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 9 (Thread 0x7f4f327fc700 (LWP 98033)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f4f40e90025 in __GI___pthread_mutex_lock (mutex=0x55a67b840658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f4f41350ffd in _mongoc_topology_pop_server_session () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f4130f966 in mongoc_client_start_session () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41325eff in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x000055a67a1c0446 in thread_find ()
#9  0x00007f4f40e8d6db in start_thread (arg=0x7f4f327fc700) at pthread_create.c:463
#10 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 8 (Thread 0x7f4f32ffd700 (LWP 98032)):
#0  0x00007f4f40ba9cb9 in __GI___poll (fds=0x7f4f32ffb330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f4f4134be71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f4f4134ef47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f4134d66b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41308324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f4134cb9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f4134d66b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41308082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f41313689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f4f41314777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f4f41325d88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x000055a67a1c0446 in thread_find ()
#15 0x00007f4f40e8d6db in start_thread (arg=0x7f4f32ffd700) at pthread_create.c:463
#16 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 7 (Thread 0x7f4f337fe700 (LWP 98031)):
#0  0x00007f4f40ba9cb9 in __GI___poll (fds=0x7f4f2c0063f0, nfds=1, timeout=500) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f4f4134b143 in mongoc_socket_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f4f4134ec1e in _mongoc_stream_socket_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f4134d959 in mongoc_stream_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f4134947a in _server_monitor_awaitable_ismaster_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f413499ea in mongoc_server_monitor_check_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f4134a462 in _server_monitor_thread () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f40e8d6db in start_thread (arg=0x7f4f337fe700) at pthread_create.c:463
#8  0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 6 (Thread 0x7f4f33fff700 (LWP 98030)):
#0  0x00007ffeb9df9b62 in clock_gettime ()
#1  0x00007f4f40bc5d36 in __GI___clock_gettime (clock_id=clock_id@entry=1, tp=tp@entry=0x7f4f33ffd580) at ../sysdeps/unix/clock_gettime.c:115
#2  0x00007f4f410b9d61 in bson_get_monotonic_time () at /home/ubuntu/mongo-c-driver/src/libbson/src/bson/bson-clock.c:126
#3  0x00007f4f41343fef in mongoc_server_description_reset () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41345bf0 in mongoc_server_description_handle_ismaster () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41345e59 in mongoc_server_description_new_copy () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f413155f9 in _mongoc_cluster_create_server_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41317225 in _mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f4131795d in mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f4f41325395 in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f4f413259ed in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x000055a67a1c0446 in thread_find ()
#15 0x00007f4f40e8d6db in start_thread (arg=0x7f4f33fff700) at pthread_create.c:463
#16 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 5 (Thread 0x7f4f38bfa700 (LWP 98029)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f4f40e90025 in __GI___pthread_mutex_lock (mutex=0x55a67b840658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f4f41351161 in _mongoc_topology_push_server_session () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f41348c38 in mongoc_client_session_destroy () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41327a25 in _mongoc_cursor_start_reading_response () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41327c4b in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x000055a67a1c0446 in thread_find ()
#9  0x00007f4f40e8d6db in start_thread (arg=0x7f4f38bfa700) at pthread_create.c:463
#10 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 4 (Thread 0x7f4f393fb700 (LWP 98028)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f4f40e90025 in __GI___pthread_mutex_lock (mutex=0x55a67b840658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f4f41350745 in mongoc_topology_select_server_id () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f41318272 in mongoc_cluster_stream_for_reads () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f413253cf in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41328c63 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x000055a67a1c0446 in thread_find ()
#8  0x00007f4f40e8d6db in start_thread (arg=0x7f4f393fb700) at pthread_create.c:463
#9  0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 3 (Thread 0x7f4f39bfc700 (LWP 98027)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f4f40e90025 in __GI___pthread_mutex_lock (mutex=0x55a67b840658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f4f41350fb0 in _mongoc_topology_update_cluster_time () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f41313a18 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41314777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41325d88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x000055a67a1c0446 in thread_find ()
#10 0x00007f4f40e8d6db in start_thread (arg=0x7f4f39bfc700) at pthread_create.c:463
#11 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 2 (Thread 0x7f4f3a3fd700 (LWP 98026)):
#0  0x00007f4f40ba9cb9 in __GI___poll (fds=0x7f4f3a3fb330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f4f4134be71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f4f4134ef47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f4134d66b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41308324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f4134cb9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f4134d66b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41308082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f41313689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f4f41314777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f4f41325d88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x000055a67a1c0446 in thread_find ()
#15 0x00007f4f40e8d6db in start_thread (arg=0x7f4f3a3fd700) at pthread_create.c:463
#16 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 1 (Thread 0x7f4f417a5740 (LWP 98025)):
#0  0x00007f4f40e8ed2d in __GI___pthread_timedjoin_ex (threadid=139978256406272, thread_return=0x0, abstime=0x0, block=<optimized out>) at pthread_join_common.c:89
#1  0x000055a67a1c0137 in main ()

times: stop 0.003 traces 0.046 cont 0.000

=== 2021-05-19T19:33:45.638409+0000 

Thread 13 (Thread 0x7f4f30ff9700 (LWP 98038)):
#0  0x00007f4f40e94065 in futex_abstimed_wait_cancelable (private=<optimized out>, abstime=0x7f4f30ff8990, expected=0, futex_word=0x7f4f2c0042d0) at ../sysdeps/unix/sysv/linux/futex-internal.h:205
#1  __pthread_cond_wait_common (abstime=0x7f4f30ff8990, mutex=0x7f4f2c004280, cond=0x7f4f2c0042a8) at pthread_cond_wait.c:539
#2  __pthread_cond_timedwait (cond=0x7f4f2c0042a8, mutex=0x7f4f2c004280, abstime=0x7f4f30ff8990) at pthread_cond_wait.c:667
#3  0x00007f4f4134a393 in mongoc_server_monitor_wait () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f4134a650 in _server_monitor_rtt_thread () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f40e8d6db in start_thread (arg=0x7f4f30ff9700) at pthread_create.c:463
#6  0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 12 (Thread 0x7f4f317fa700 (LWP 98036)):
#0  0x00007ffeb9df9b62 in clock_gettime ()
#1  0x00007f4f40bc5d36 in __GI___clock_gettime (clock_id=clock_id@entry=1, tp=tp@entry=0x7f4f317f9000) at ../sysdeps/unix/clock_gettime.c:115
#2  0x00007f4f410b9d61 in bson_get_monotonic_time () at /home/ubuntu/mongo-c-driver/src/libbson/src/bson/bson-clock.c:126
#3  0x00007f4f41343fef in mongoc_server_description_reset () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41345bf0 in mongoc_server_description_handle_ismaster () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41345e59 in mongoc_server_description_new_copy () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f413155f9 in _mongoc_cluster_create_server_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41317225 in _mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f413253cf in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f4f41328c63 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x000055a67a1c0446 in thread_find ()
#12 0x00007f4f40e8d6db in start_thread (arg=0x7f4f317fa700) at pthread_create.c:463
#13 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 11 (Thread 0x7f4f31ffb700 (LWP 98035)):
#0  0x00007f4f40ba9cb9 in __GI___poll (fds=0x7f4f31ff9330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f4f4134be71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f4f4134ef47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f4134d66b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41308324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f4134cb9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f4134d66b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41308082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f41313689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f4f41314777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f4f41325d88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x000055a67a1c0446 in thread_find ()
#15 0x00007f4f40e8d6db in start_thread (arg=0x7f4f31ffb700) at pthread_create.c:463
#16 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 10 (Thread 0x7f4f2bfff700 (LWP 98034)):
#0  0x00007f4f40ba9cb9 in __GI___poll (fds=0x7f4f2bffd330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f4f4134be71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f4f4134ef47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f4134d66b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41308324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f4134cb9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f4134d66b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41308082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f41313689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f4f41314777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f4f41325d88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x000055a67a1c0446 in thread_find ()
#15 0x00007f4f40e8d6db in start_thread (arg=0x7f4f2bfff700) at pthread_create.c:463
#16 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 9 (Thread 0x7f4f327fc700 (LWP 98033)):
#0  0x00007f4f40ba9cb9 in __GI___poll (fds=0x7f4f327fa330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f4f4134be71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f4f4134ef47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f4134d66b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41308324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f4134cb9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f4134d66b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41308082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f41313689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f4f41314777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f4f41325d88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x000055a67a1c0446 in thread_find ()
#15 0x00007f4f40e8d6db in start_thread (arg=0x7f4f327fc700) at pthread_create.c:463
#16 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 8 (Thread 0x7f4f32ffd700 (LWP 98032)):
#0  __lll_unlock_wake () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:371
#1  0x00007f4f40e91852 in __pthread_mutex_unlock_usercnt (decr=1, mutex=0x55a67b840658) at pthread_mutex_unlock.c:56
#2  __GI___pthread_mutex_unlock (mutex=0x55a67b840658) at pthread_mutex_unlock.c:356
#3  0x00007f4f41315617 in _mongoc_cluster_create_server_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41317225 in _mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f413253cf in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f41328c63 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x000055a67a1c0446 in thread_find ()
#9  0x00007f4f40e8d6db in start_thread (arg=0x7f4f32ffd700) at pthread_create.c:463
#10 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 7 (Thread 0x7f4f337fe700 (LWP 98031)):
#0  0x00007f4f40ba9cb9 in __GI___poll (fds=0x7f4f2c0063f0, nfds=1, timeout=500) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f4f4134b143 in mongoc_socket_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f4f4134ec1e in _mongoc_stream_socket_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f4134d959 in mongoc_stream_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f4134947a in _server_monitor_awaitable_ismaster_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f413499ea in mongoc_server_monitor_check_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f4134a462 in _server_monitor_thread () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f40e8d6db in start_thread (arg=0x7f4f337fe700) at pthread_create.c:463
#8  0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 6 (Thread 0x7f4f33fff700 (LWP 98030)):
#0  0x00007f4f41350736 in mongoc_topology_select_server_id () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#1  0x00007f4f41318272 in mongoc_cluster_stream_for_reads () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f4f413253cf in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f41328c63 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x000055a67a1c0446 in thread_find ()
#6  0x00007f4f40e8d6db in start_thread (arg=0x7f4f33fff700) at pthread_create.c:463
#7  0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 5 (Thread 0x7f4f38bfa700 (LWP 98029)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f4f40e90025 in __GI___pthread_mutex_lock (mutex=0x55a67b840658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f4f41351161 in _mongoc_topology_push_server_session () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f41348c38 in mongoc_client_session_destroy () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41327a25 in _mongoc_cursor_start_reading_response () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41327c4b in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x000055a67a1c0446 in thread_find ()
#9  0x00007f4f40e8d6db in start_thread (arg=0x7f4f38bfa700) at pthread_create.c:463
#10 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 4 (Thread 0x7f4f393fb700 (LWP 98028)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f4f40e90025 in __GI___pthread_mutex_lock (mutex=0x55a67b840658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f4f413155e3 in _mongoc_cluster_create_server_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f41317225 in _mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f413253cf in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41328c63 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x000055a67a1c0446 in thread_find ()
#8  0x00007f4f40e8d6db in start_thread (arg=0x7f4f393fb700) at pthread_create.c:463
#9  0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 3 (Thread 0x7f4f39bfc700 (LWP 98027)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f4f40e90025 in __GI___pthread_mutex_lock (mutex=0x55a67b840658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f4f4131489b in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f41325d88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x000055a67a1c0446 in thread_find ()
#8  0x00007f4f40e8d6db in start_thread (arg=0x7f4f39bfc700) at pthread_create.c:463
#9  0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 2 (Thread 0x7f4f3a3fd700 (LWP 98026)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f4f40e90025 in __GI___pthread_mutex_lock (mutex=0x55a67b840658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f4f41350fb0 in _mongoc_topology_update_cluster_time () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f4f41313a18 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f4f41314777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f4f41325d88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f4f41327c22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f4f41328f74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f4f413262f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x000055a67a1c0446 in thread_find ()
#10 0x00007f4f40e8d6db in start_thread (arg=0x7f4f3a3fd700) at pthread_create.c:463
#11 0x00007f4f40bb671f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 1 (Thread 0x7f4f417a5740 (LWP 98025)):
#0  0x00007f4f40e8ed2d in __GI___pthread_timedjoin_ex (threadid=139978256406272, thread_return=0x0, abstime=0x0, block=<optimized out>) at pthread_join_common.c:89
#1  0x000055a67a1c0137 in main ()
