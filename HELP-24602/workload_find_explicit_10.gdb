gdbmon.py 98675 10 5 (version 1.2)

=== 2021-05-19T20:50:46.087488+0000 

Thread 13 (Thread 0x7f9c58de7700 (LWP 98688)):
#0  0x00007f9c693b9065 in futex_abstimed_wait_cancelable (private=<optimized out>, abstime=0x7f9c58de6990, expected=0, futex_word=0x7f9c540042d0) at ../sysdeps/unix/sysv/linux/futex-internal.h:205
#1  __pthread_cond_wait_common (abstime=0x7f9c58de6990, mutex=0x7f9c54004280, cond=0x7f9c540042a8) at pthread_cond_wait.c:539
#2  __pthread_cond_timedwait (cond=0x7f9c540042a8, mutex=0x7f9c54004280, abstime=0x7f9c58de6990) at pthread_cond_wait.c:667
#3  0x00007f9c6986f393 in mongoc_server_monitor_wait () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6986f650 in _server_monitor_rtt_thread () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c693b26db in start_thread (arg=0x7f9c58de7700) at pthread_create.c:463
#6  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 12 (Thread 0x7f9c597fa700 (LWP 98686)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f9c693b5025 in __GI___pthread_mutex_lock (mutex=0x558c14142658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f9c6983989b in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x0000558c13e2f446 in thread_find ()
#8  0x00007f9c693b26db in start_thread (arg=0x7f9c597fa700) at pthread_create.c:463
#9  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 11 (Thread 0x7f9c59ffb700 (LWP 98685)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f9c693b5025 in __GI___pthread_mutex_lock (mutex=0x558c14142658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f9c6983a5e3 in _mongoc_cluster_create_server_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6983c225 in _mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6983c95d in mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984a395 in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984a9ed in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x0000558c13e2f446 in thread_find ()
#11 0x00007f9c693b26db in start_thread (arg=0x7f9c59ffb700) at pthread_create.c:463
#12 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 10 (Thread 0x7f9c5a7fc700 (LWP 98684)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f9c693b5025 in __GI___pthread_mutex_lock (mutex=0x558c14142658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f9c69875745 in mongoc_topology_select_server_id () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6983d272 in mongoc_cluster_stream_for_reads () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6984a3cf in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984dc63 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x0000558c13e2f446 in thread_find ()
#8  0x00007f9c693b26db in start_thread (arg=0x7f9c5a7fc700) at pthread_create.c:463
#9  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 9 (Thread 0x7f9c5affd700 (LWP 98683)):
#0  0x00007f9c690cecb9 in __GI___poll (fds=0x7f9c5affb330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f9c69870e71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c69873f47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6982d324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c69871b9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6982d082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c69838689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f9c69839777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x0000558c13e2f446 in thread_find ()
#15 0x00007f9c693b26db in start_thread (arg=0x7f9c5affd700) at pthread_create.c:463
#16 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 8 (Thread 0x7f9c5b7fe700 (LWP 98682)):
#0  0x00007f9c6986ae28 in mongoc_server_description_new_copy () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#1  0x00007f9c6983a5f9 in _mongoc_cluster_create_server_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c6983c225 in _mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6984a3cf in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6984dc63 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x0000558c13e2f446 in thread_find ()
#7  0x00007f9c693b26db in start_thread (arg=0x7f9c5b7fe700) at pthread_create.c:463
#8  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 7 (Thread 0x7f9c5bfff700 (LWP 98681)):
#0  0x00007f9c690cecb9 in __GI___poll (fds=0x7f9c540063f0, nfds=1, timeout=500) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f9c69870143 in mongoc_socket_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c69873c1e in _mongoc_stream_socket_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c69872959 in mongoc_stream_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6986e47a in _server_monitor_awaitable_ismaster_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6986e9ea in mongoc_server_monitor_check_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6986f462 in _server_monitor_thread () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c693b26db in start_thread (arg=0x7f9c5bfff700) at pthread_create.c:463
#8  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 6 (Thread 0x7f9c6091e700 (LWP 98680)):
#0  0x00007f9c693bd6f7 in __libc_sendmsg (fd=15, msg=0x7f9c6091c590, flags=16384) at ../sysdeps/unix/sysv/linux/sendmsg.c:28
#1  0x00007f9c698711a1 in mongoc_socket_sendv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c69874214 in _mongoc_stream_socket_writev () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c69872ad4 in _mongoc_stream_writev_full () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6983865c in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c69839777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x0000558c13e2f446 in thread_find ()
#11 0x00007f9c693b26db in start_thread (arg=0x7f9c6091e700) at pthread_create.c:463
#12 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 5 (Thread 0x7f9c6111f700 (LWP 98679)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f9c693b5025 in __GI___pthread_mutex_lock (mutex=0x558c14142658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f9c69875745 in mongoc_topology_select_server_id () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6983d272 in mongoc_cluster_stream_for_reads () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6984a3cf in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984dc63 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x0000558c13e2f446 in thread_find ()
#8  0x00007f9c693b26db in start_thread (arg=0x7f9c6111f700) at pthread_create.c:463
#9  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 4 (Thread 0x7f9c61920700 (LWP 98678)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f9c693b5025 in __GI___pthread_mutex_lock (mutex=0x558c14142658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f9c69875745 in mongoc_topology_select_server_id () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6983d272 in mongoc_cluster_stream_for_reads () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6984a3cf in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984dc63 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x0000558c13e2f446 in thread_find ()
#8  0x00007f9c693b26db in start_thread (arg=0x7f9c61920700) at pthread_create.c:463
#9  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 3 (Thread 0x7f9c62121700 (LWP 98677)):
#0  0x00007f9c690cecb9 in __GI___poll (fds=0x7f9c6211f330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f9c69870e71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c69873f47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6982d324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c69871b9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6982d082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c69838689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f9c69839777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x0000558c13e2f446 in thread_find ()
#15 0x00007f9c693b26db in start_thread (arg=0x7f9c62121700) at pthread_create.c:463
#16 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 2 (Thread 0x7f9c62922700 (LWP 98676)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f9c693b5025 in __GI___pthread_mutex_lock (mutex=0x558c14142658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f9c69875745 in mongoc_topology_select_server_id () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6983d272 in mongoc_cluster_stream_for_reads () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6984a3cf in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984dc63 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x0000558c13e2f446 in thread_find ()
#8  0x00007f9c693b26db in start_thread (arg=0x7f9c62922700) at pthread_create.c:463
#9  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 1 (Thread 0x7f9c69cca740 (LWP 98675)):
#0  0x00007f9c693b3d2d in __GI___pthread_timedjoin_ex (threadid=140309645371136, thread_return=0x0, abstime=0x0, block=<optimized out>) at pthread_join_common.c:89
#1  0x0000558c13e2f137 in main ()

times: stop 0.003 traces 0.051 cont 0.000

=== 2021-05-19T20:50:56.152058+0000 

Thread 13 (Thread 0x7f9c58de7700 (LWP 98688)):
#0  0x00007f9c693b9065 in futex_abstimed_wait_cancelable (private=<optimized out>, abstime=0x7f9c58de6990, expected=0, futex_word=0x7f9c540042d0) at ../sysdeps/unix/sysv/linux/futex-internal.h:205
#1  __pthread_cond_wait_common (abstime=0x7f9c58de6990, mutex=0x7f9c54004280, cond=0x7f9c540042a8) at pthread_cond_wait.c:539
#2  __pthread_cond_timedwait (cond=0x7f9c540042a8, mutex=0x7f9c54004280, abstime=0x7f9c58de6990) at pthread_cond_wait.c:667
#3  0x00007f9c6986f393 in mongoc_server_monitor_wait () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6986f650 in _server_monitor_rtt_thread () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c693b26db in start_thread (arg=0x7f9c58de7700) at pthread_create.c:463
#6  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 12 (Thread 0x7f9c597fa700 (LWP 98686)):
#0  0x00007f9c690cecb9 in __GI___poll (fds=0x7f9c597f8330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f9c69870e71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c69873f47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6982d324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c69871b9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6982d082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c69838689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f9c69839777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x0000558c13e2f446 in thread_find ()
#15 0x00007f9c693b26db in start_thread (arg=0x7f9c597fa700) at pthread_create.c:463
#16 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 11 (Thread 0x7f9c59ffb700 (LWP 98685)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f9c693b5025 in __GI___pthread_mutex_lock (mutex=0x558c14142658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f9c6983bbe9 in _mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6984a3cf in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6984dc63 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x0000558c13e2f446 in thread_find ()
#7  0x00007f9c693b26db in start_thread (arg=0x7f9c59ffb700) at pthread_create.c:463
#8  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 10 (Thread 0x7f9c5a7fc700 (LWP 98684)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f9c693b5025 in __GI___pthread_mutex_lock (mutex=0x558c14142658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f9c69875745 in mongoc_topology_select_server_id () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6983d272 in mongoc_cluster_stream_for_reads () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6984a3cf in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984dc63 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x0000558c13e2f446 in thread_find ()
#8  0x00007f9c693b26db in start_thread (arg=0x7f9c5a7fc700) at pthread_create.c:463
#9  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 9 (Thread 0x7f9c5affd700 (LWP 98683)):
#0  __strcmp_sse2_unaligned () at ../sysdeps/x86_64/multiarch/strcmp-sse2-unaligned.S:34
#1  0x00007f9c6986a9a5 in mongoc_server_description_handle_ismaster.part () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c6986ae59 in mongoc_server_description_new_copy () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6983a5f9 in _mongoc_cluster_create_server_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6983c225 in _mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6983c95d in mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984a395 in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6984a9ed in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x0000558c13e2f446 in thread_find ()
#12 0x00007f9c693b26db in start_thread (arg=0x7f9c5affd700) at pthread_create.c:463
#13 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 8 (Thread 0x7f9c5b7fe700 (LWP 98682)):
#0  0x00007f9c693bc91e in __libc_recv (fd=16, buf=0x7f9c3c004b60, len=1024, flags=0) at ../sysdeps/unix/sysv/linux/recv.c:28
#1  0x00007f9c69870dbb in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c69873f47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6982d324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c69871b9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6982d082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c69838689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f9c69839777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x0000558c13e2f446 in thread_find ()
#15 0x00007f9c693b26db in start_thread (arg=0x7f9c5b7fe700) at pthread_create.c:463
#16 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 7 (Thread 0x7f9c5bfff700 (LWP 98681)):
#0  0x00007f9c690cecb9 in __GI___poll (fds=0x7f9c540063f0, nfds=1, timeout=500) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f9c69870143 in mongoc_socket_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c69873c1e in _mongoc_stream_socket_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c69872959 in mongoc_stream_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6986e47a in _server_monitor_awaitable_ismaster_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6986e9ea in mongoc_server_monitor_check_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6986f462 in _server_monitor_thread () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c693b26db in start_thread (arg=0x7f9c5bfff700) at pthread_create.c:463
#8  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 6 (Thread 0x7f9c6091e700 (LWP 98680)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f9c693b5025 in __GI___pthread_mutex_lock (mutex=0x558c14142658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f9c69875fb0 in _mongoc_topology_update_cluster_time () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c69838a18 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c69839777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x0000558c13e2f446 in thread_find ()
#10 0x00007f9c693b26db in start_thread (arg=0x7f9c6091e700) at pthread_create.c:463
#11 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 5 (Thread 0x7f9c6111f700 (LWP 98679)):
#0  0x00007f9c690cecb9 in __GI___poll (fds=0x7f9c6111d330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f9c69870e71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c69873f47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6982d324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c69871b9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6982d082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c69838689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f9c69839777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x0000558c13e2f446 in thread_find ()
#15 0x00007f9c693b26db in start_thread (arg=0x7f9c6111f700) at pthread_create.c:463
#16 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 4 (Thread 0x7f9c61920700 (LWP 98678)):
#0  0x00007f9c693bd6f7 in __libc_sendmsg (fd=7, msg=0x7f9c6191e590, flags=16384) at ../sysdeps/unix/sysv/linux/sendmsg.c:28
#1  0x00007f9c698711a1 in mongoc_socket_sendv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c69874214 in _mongoc_stream_socket_writev () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c69872ad4 in _mongoc_stream_writev_full () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6983865c in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c69839777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x0000558c13e2f446 in thread_find ()
#11 0x00007f9c693b26db in start_thread (arg=0x7f9c61920700) at pthread_create.c:463
#12 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 3 (Thread 0x7f9c62121700 (LWP 98677)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f9c693b5025 in __GI___pthread_mutex_lock (mutex=0x558c14142658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f9c6983989b in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x0000558c13e2f446 in thread_find ()
#8  0x00007f9c693b26db in start_thread (arg=0x7f9c62121700) at pthread_create.c:463
#9  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 2 (Thread 0x7f9c62922700 (LWP 98676)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f9c693b5025 in __GI___pthread_mutex_lock (mutex=0x558c14142658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f9c6983989b in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x0000558c13e2f446 in thread_find ()
#8  0x00007f9c693b26db in start_thread (arg=0x7f9c62922700) at pthread_create.c:463
#9  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 1 (Thread 0x7f9c69cca740 (LWP 98675)):
#0  0x00007f9c693b3d2d in __GI___pthread_timedjoin_ex (threadid=140309645371136, thread_return=0x0, abstime=0x0, block=<optimized out>) at pthread_join_common.c:89
#1  0x0000558c13e2f137 in main ()

times: stop 0.003 traces 0.047 cont 0.000

=== 2021-05-19T20:51:06.212431+0000 

Thread 13 (Thread 0x7f9c58de7700 (LWP 98688)):
#0  0x00007f9c693b9065 in futex_abstimed_wait_cancelable (private=<optimized out>, abstime=0x7f9c58de6990, expected=0, futex_word=0x7f9c540042d0) at ../sysdeps/unix/sysv/linux/futex-internal.h:205
#1  __pthread_cond_wait_common (abstime=0x7f9c58de6990, mutex=0x7f9c54004280, cond=0x7f9c540042a8) at pthread_cond_wait.c:539
#2  __pthread_cond_timedwait (cond=0x7f9c540042a8, mutex=0x7f9c54004280, abstime=0x7f9c58de6990) at pthread_cond_wait.c:667
#3  0x00007f9c6986f393 in mongoc_server_monitor_wait () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6986f650 in _server_monitor_rtt_thread () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c693b26db in start_thread (arg=0x7f9c58de7700) at pthread_create.c:463
#6  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 12 (Thread 0x7f9c597fa700 (LWP 98686)):
#0  0x00007ffd57ebab62 in clock_gettime ()
#1  0x00007f9c690ead36 in __GI___clock_gettime (clock_id=clock_id@entry=1, tp=tp@entry=0x7f9c597f8dd0) at ../sysdeps/unix/clock_gettime.c:115
#2  0x00007f9c695ded61 in bson_get_monotonic_time () at /home/ubuntu/mongo-c-driver/src/libbson/src/bson/bson-clock.c:126
#3  0x00007f9c6985ecd2 in mongoc_cmd_parts_assemble () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6984ad60 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x0000558c13e2f446 in thread_find ()
#9  0x00007f9c693b26db in start_thread (arg=0x7f9c597fa700) at pthread_create.c:463
#10 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 11 (Thread 0x7f9c59ffb700 (LWP 98685)):
#0  0x00007f9c690cecb9 in __GI___poll (fds=0x7f9c59ff9330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f9c69870e71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c69873f47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6982d324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c69871b9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6982d082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c69838689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f9c69839777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x0000558c13e2f446 in thread_find ()
#15 0x00007f9c693b26db in start_thread (arg=0x7f9c59ffb700) at pthread_create.c:463
#16 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 10 (Thread 0x7f9c5a7fc700 (LWP 98684)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f9c693b5025 in __GI___pthread_mutex_lock (mutex=0x558c14142658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f9c69875745 in mongoc_topology_select_server_id () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6983d272 in mongoc_cluster_stream_for_reads () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6984a3cf in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984dc63 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x0000558c13e2f446 in thread_find ()
#8  0x00007f9c693b26db in start_thread (arg=0x7f9c5a7fc700) at pthread_create.c:463
#9  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 9 (Thread 0x7f9c5affd700 (LWP 98683)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f9c693b5025 in __GI___pthread_mutex_lock (mutex=0x558c14142658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f9c6983bbe9 in _mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6984a3cf in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6984dc63 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x0000558c13e2f446 in thread_find ()
#7  0x00007f9c693b26db in start_thread (arg=0x7f9c5affd700) at pthread_create.c:463
#8  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 8 (Thread 0x7f9c5b7fe700 (LWP 98682)):
#0  0x00007f9c690cecb9 in __GI___poll (fds=0x7f9c5b7fc330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f9c69870e71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c69873f47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6982d324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c69871b9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6982d082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c69838689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f9c69839777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x0000558c13e2f446 in thread_find ()
#15 0x00007f9c693b26db in start_thread (arg=0x7f9c5b7fe700) at pthread_create.c:463
#16 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 7 (Thread 0x7f9c5bfff700 (LWP 98681)):
#0  0x00007f9c690cecb9 in __GI___poll (fds=0x7f9c540063f0, nfds=1, timeout=500) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f9c69870143 in mongoc_socket_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c69873c1e in _mongoc_stream_socket_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c69872959 in mongoc_stream_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6986e47a in _server_monitor_awaitable_ismaster_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6986e9ea in mongoc_server_monitor_check_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6986f462 in _server_monitor_thread () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c693b26db in start_thread (arg=0x7f9c5bfff700) at pthread_create.c:463
#8  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 6 (Thread 0x7f9c6091e700 (LWP 98680)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f9c693b5025 in __GI___pthread_mutex_lock (mutex=0x558c14142658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f9c69875745 in mongoc_topology_select_server_id () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6983d272 in mongoc_cluster_stream_for_reads () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6984a3cf in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984dc63 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x0000558c13e2f446 in thread_find ()
#8  0x00007f9c693b26db in start_thread (arg=0x7f9c6091e700) at pthread_create.c:463
#9  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 5 (Thread 0x7f9c6111f700 (LWP 98679)):
#0  0x00007f9c690cecb9 in __GI___poll (fds=0x7f9c6111d330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f9c69870e71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c69873f47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6982d324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c69871b9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6982d082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c69838689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f9c69839777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x0000558c13e2f446 in thread_find ()
#15 0x00007f9c693b26db in start_thread (arg=0x7f9c6111f700) at pthread_create.c:463
#16 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 4 (Thread 0x7f9c61920700 (LWP 98678)):
#0  0x00007f9c690cecb9 in __GI___poll (fds=0x7f9c6191e330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f9c69870e71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c69873f47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6982d324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c69871b9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6982d082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c69838689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f9c69839777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x0000558c13e2f446 in thread_find ()
#15 0x00007f9c693b26db in start_thread (arg=0x7f9c61920700) at pthread_create.c:463
#16 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 3 (Thread 0x7f9c62121700 (LWP 98677)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f9c693b5025 in __GI___pthread_mutex_lock (mutex=0x558c14142658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f9c69875820 in mongoc_topology_select_server_id () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6983d272 in mongoc_cluster_stream_for_reads () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6984a3cf in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984dc63 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x0000558c13e2f446 in thread_find ()
#8  0x00007f9c693b26db in start_thread (arg=0x7f9c62121700) at pthread_create.c:463
#9  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 2 (Thread 0x7f9c62922700 (LWP 98676)):
#0  0x00007f9c693bd6f7 in __libc_sendmsg (fd=8, msg=0x7f9c62920590, flags=16384) at ../sysdeps/unix/sysv/linux/sendmsg.c:28
#1  0x00007f9c698711a1 in mongoc_socket_sendv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c69874214 in _mongoc_stream_socket_writev () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c69872ad4 in _mongoc_stream_writev_full () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6983865c in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c69839777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x0000558c13e2f446 in thread_find ()
#11 0x00007f9c693b26db in start_thread (arg=0x7f9c62922700) at pthread_create.c:463
#12 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 1 (Thread 0x7f9c69cca740 (LWP 98675)):
#0  0x00007f9c693b3d2d in __GI___pthread_timedjoin_ex (threadid=140309645371136, thread_return=0x0, abstime=0x0, block=<optimized out>) at pthread_join_common.c:89
#1  0x0000558c13e2f137 in main ()

times: stop 0.003 traces 0.053 cont 0.000

=== 2021-05-19T20:51:16.278426+0000 

Thread 13 (Thread 0x7f9c58de7700 (LWP 98688)):
#0  0x00007f9c693b9065 in futex_abstimed_wait_cancelable (private=<optimized out>, abstime=0x7f9c58de6990, expected=0, futex_word=0x7f9c540042d0) at ../sysdeps/unix/sysv/linux/futex-internal.h:205
#1  __pthread_cond_wait_common (abstime=0x7f9c58de6990, mutex=0x7f9c54004280, cond=0x7f9c540042a8) at pthread_cond_wait.c:539
#2  __pthread_cond_timedwait (cond=0x7f9c540042a8, mutex=0x7f9c54004280, abstime=0x7f9c58de6990) at pthread_cond_wait.c:667
#3  0x00007f9c6986f393 in mongoc_server_monitor_wait () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6986f650 in _server_monitor_rtt_thread () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c693b26db in start_thread (arg=0x7f9c58de7700) at pthread_create.c:463
#6  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 12 (Thread 0x7f9c597fa700 (LWP 98686)):
#0  0x00007f9c690cecb9 in __GI___poll (fds=0x7f9c597f8330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f9c69870e71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c69873f47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6982d324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c69871b9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6982d082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c69838689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f9c69839777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x0000558c13e2f446 in thread_find ()
#15 0x00007f9c693b26db in start_thread (arg=0x7f9c597fa700) at pthread_create.c:463
#16 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 11 (Thread 0x7f9c59ffb700 (LWP 98685)):
#0  0x00007f9c690cecb9 in __GI___poll (fds=0x7f9c59ff9330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f9c69870e71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c69873f47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6982d324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c69871b9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6982d082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c69838689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f9c69839777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x0000558c13e2f446 in thread_find ()
#15 0x00007f9c693b26db in start_thread (arg=0x7f9c59ffb700) at pthread_create.c:463
#16 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 10 (Thread 0x7f9c5a7fc700 (LWP 98684)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f9c693b5025 in __GI___pthread_mutex_lock (mutex=0x558c14142658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f9c6983bbe9 in _mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6984a3cf in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6984dc63 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x0000558c13e2f446 in thread_find ()
#7  0x00007f9c693b26db in start_thread (arg=0x7f9c5a7fc700) at pthread_create.c:463
#8  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 9 (Thread 0x7f9c5affd700 (LWP 98683)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f9c693b5025 in __GI___pthread_mutex_lock (mutex=0x558c14142658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f9c6983bbe9 in _mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6983c95d in mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6984a395 in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984a9ed in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x0000558c13e2f446 in thread_find ()
#10 0x00007f9c693b26db in start_thread (arg=0x7f9c5affd700) at pthread_create.c:463
#11 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 8 (Thread 0x7f9c5b7fe700 (LWP 98682)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f9c693b5025 in __GI___pthread_mutex_lock (mutex=0x558c14142658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f9c69875fb0 in _mongoc_topology_update_cluster_time () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c69838a18 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c69839777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x0000558c13e2f446 in thread_find ()
#10 0x00007f9c693b26db in start_thread (arg=0x7f9c5b7fe700) at pthread_create.c:463
#11 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 7 (Thread 0x7f9c5bfff700 (LWP 98681)):
#0  0x00007f9c690cecb9 in __GI___poll (fds=0x7f9c540063f0, nfds=1, timeout=500) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f9c69870143 in mongoc_socket_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c69873c1e in _mongoc_stream_socket_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c69872959 in mongoc_stream_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6986e47a in _server_monitor_awaitable_ismaster_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6986e9ea in mongoc_server_monitor_check_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6986f462 in _server_monitor_thread () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c693b26db in start_thread (arg=0x7f9c5bfff700) at pthread_create.c:463
#8  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 6 (Thread 0x7f9c6091e700 (LWP 98680)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f9c693b5025 in __GI___pthread_mutex_lock (mutex=0x558c14142658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f9c6983989b in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x0000558c13e2f446 in thread_find ()
#8  0x00007f9c693b26db in start_thread (arg=0x7f9c6091e700) at pthread_create.c:463
#9  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 5 (Thread 0x7f9c6111f700 (LWP 98679)):
#0  0x00007f9c690cecb9 in __GI___poll (fds=0x7f9c6111d330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f9c69870e71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c69873f47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6982d324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c69871b9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6982d082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c69838689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f9c69839777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x0000558c13e2f446 in thread_find ()
#15 0x00007f9c693b26db in start_thread (arg=0x7f9c6111f700) at pthread_create.c:463
#16 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 4 (Thread 0x7f9c61920700 (LWP 98678)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f9c693b5025 in __GI___pthread_mutex_lock (mutex=0x558c14142658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f9c6983bbe9 in _mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6983c95d in mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6984a395 in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984a9ed in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x0000558c13e2f446 in thread_find ()
#10 0x00007f9c693b26db in start_thread (arg=0x7f9c61920700) at pthread_create.c:463
#11 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 3 (Thread 0x7f9c62121700 (LWP 98677)):
#0  0x00007f9c693bd6f7 in __libc_sendmsg (fd=13, msg=0x7f9c6211f590, flags=16384) at ../sysdeps/unix/sysv/linux/sendmsg.c:28
#1  0x00007f9c698711a1 in mongoc_socket_sendv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c69874214 in _mongoc_stream_socket_writev () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c69872ad4 in _mongoc_stream_writev_full () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6983865c in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c69839777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x0000558c13e2f446 in thread_find ()
#11 0x00007f9c693b26db in start_thread (arg=0x7f9c62121700) at pthread_create.c:463
#12 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 2 (Thread 0x7f9c62922700 (LWP 98676)):
#0  0x00007f9c690cecb9 in __GI___poll (fds=0x7f9c62920330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f9c69870e71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c69873f47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6982d324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c69871b9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6982d082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c69838689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f9c69839777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x0000558c13e2f446 in thread_find ()
#15 0x00007f9c693b26db in start_thread (arg=0x7f9c62922700) at pthread_create.c:463
#16 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 1 (Thread 0x7f9c69cca740 (LWP 98675)):
#0  0x00007f9c693b3d2d in __GI___pthread_timedjoin_ex (threadid=140309645371136, thread_return=0x0, abstime=0x0, block=<optimized out>) at pthread_join_common.c:89
#1  0x0000558c13e2f137 in main ()

times: stop 0.003 traces 0.048 cont 0.000

=== 2021-05-19T20:51:26.340180+0000 

Thread 13 (Thread 0x7f9c58de7700 (LWP 98688)):
#0  0x00007f9c693b9065 in futex_abstimed_wait_cancelable (private=<optimized out>, abstime=0x7f9c58de6990, expected=0, futex_word=0x7f9c540042d0) at ../sysdeps/unix/sysv/linux/futex-internal.h:205
#1  __pthread_cond_wait_common (abstime=0x7f9c58de6990, mutex=0x7f9c54004280, cond=0x7f9c540042a8) at pthread_cond_wait.c:539
#2  __pthread_cond_timedwait (cond=0x7f9c540042a8, mutex=0x7f9c54004280, abstime=0x7f9c58de6990) at pthread_cond_wait.c:667
#3  0x00007f9c6986f393 in mongoc_server_monitor_wait () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6986f650 in _server_monitor_rtt_thread () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c693b26db in start_thread (arg=0x7f9c58de7700) at pthread_create.c:463
#6  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 12 (Thread 0x7f9c597fa700 (LWP 98686)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f9c693b5025 in __GI___pthread_mutex_lock (mutex=0x558c14142658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f9c6983bbe9 in _mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6984a3cf in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6984dc63 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x0000558c13e2f446 in thread_find ()
#7  0x00007f9c693b26db in start_thread (arg=0x7f9c597fa700) at pthread_create.c:463
#8  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 11 (Thread 0x7f9c59ffb700 (LWP 98685)):
#0  0x00007f9c690cecb9 in __GI___poll (fds=0x7f9c59ff9330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f9c69870e71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c69873f47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6982d324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c69871b9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6982d082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c69838689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f9c69839777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x0000558c13e2f446 in thread_find ()
#15 0x00007f9c693b26db in start_thread (arg=0x7f9c59ffb700) at pthread_create.c:463
#16 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 10 (Thread 0x7f9c5a7fc700 (LWP 98684)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f9c693b5025 in __GI___pthread_mutex_lock (mutex=0x558c14142658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f9c69875fb0 in _mongoc_topology_update_cluster_time () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c69838a18 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c69839777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x0000558c13e2f446 in thread_find ()
#10 0x00007f9c693b26db in start_thread (arg=0x7f9c5a7fc700) at pthread_create.c:463
#11 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 9 (Thread 0x7f9c5affd700 (LWP 98683)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f9c693b5025 in __GI___pthread_mutex_lock (mutex=0x558c14142658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f9c69875820 in mongoc_topology_select_server_id () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6983d272 in mongoc_cluster_stream_for_reads () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6984a3cf in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984dc63 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x0000558c13e2f446 in thread_find ()
#8  0x00007f9c693b26db in start_thread (arg=0x7f9c5affd700) at pthread_create.c:463
#9  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 8 (Thread 0x7f9c5b7fe700 (LWP 98682)):
#0  0x00007f9c690cecb9 in __GI___poll (fds=0x7f9c5b7fc330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f9c69870e71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c69873f47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6982d324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c69871b9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6982d082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c69838689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f9c69839777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x0000558c13e2f446 in thread_find ()
#15 0x00007f9c693b26db in start_thread (arg=0x7f9c5b7fe700) at pthread_create.c:463
#16 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 7 (Thread 0x7f9c5bfff700 (LWP 98681)):
#0  0x00007f9c690cecb9 in __GI___poll (fds=0x7f9c540063f0, nfds=1, timeout=500) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f9c69870143 in mongoc_socket_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c69873c1e in _mongoc_stream_socket_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c69872959 in mongoc_stream_poll () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6986e47a in _server_monitor_awaitable_ismaster_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6986e9ea in mongoc_server_monitor_check_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6986f462 in _server_monitor_thread () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c693b26db in start_thread (arg=0x7f9c5bfff700) at pthread_create.c:463
#8  0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 6 (Thread 0x7f9c6091e700 (LWP 98680)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f9c693b5025 in __GI___pthread_mutex_lock (mutex=0x558c14142658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f9c69875fb0 in _mongoc_topology_update_cluster_time () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c69838a18 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c69839777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x0000558c13e2f446 in thread_find ()
#10 0x00007f9c693b26db in start_thread (arg=0x7f9c6091e700) at pthread_create.c:463
#11 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 5 (Thread 0x7f9c6111f700 (LWP 98679)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f9c693b5025 in __GI___pthread_mutex_lock (mutex=0x558c14142658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f9c6983a5e3 in _mongoc_cluster_create_server_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6983c225 in _mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6983c95d in mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984a395 in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984a9ed in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x0000558c13e2f446 in thread_find ()
#11 0x00007f9c693b26db in start_thread (arg=0x7f9c6111f700) at pthread_create.c:463
#12 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 4 (Thread 0x7f9c61920700 (LWP 98678)):
#0  __lll_lock_wait () at ../sysdeps/unix/sysv/linux/x86_64/lowlevellock.S:135
#1  0x00007f9c693b5025 in __GI___pthread_mutex_lock (mutex=0x558c14142658) at ../nptl/pthread_mutex_lock.c:80
#2  0x00007f9c6983bbe9 in _mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6983c95d in mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6984a395 in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6984a9ed in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x0000558c13e2f446 in thread_find ()
#10 0x00007f9c693b26db in start_thread (arg=0x7f9c61920700) at pthread_create.c:463
#11 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 3 (Thread 0x7f9c62121700 (LWP 98677)):
#0  0x00007f9c695dcce2 in memcpy (__len=128, __src=0x558c141424d0, __dest=0x7f9c4c007660) at /usr/include/x86_64-linux-gnu/bits/string_fortified.h:34
#1  bson_copy_to (src=0x558c141424d0, dst=0x7f9c4c007660) at /home/ubuntu/mongo-c-driver/src/libbson/src/bson/bson.c:2205
#2  0x00007f9c6986aeb6 in mongoc_server_stream_new () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6983a60c in _mongoc_cluster_create_server_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6983c225 in _mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c6983c95d in mongoc_cluster_stream_for_server () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6984a395 in _mongoc_cursor_fetch_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6984a9ed in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x0000558c13e2f446 in thread_find ()
#12 0x00007f9c693b26db in start_thread (arg=0x7f9c62121700) at pthread_create.c:463
#13 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 2 (Thread 0x7f9c62922700 (LWP 98676)):
#0  0x00007f9c690cecb9 in __GI___poll (fds=0x7f9c62920330, nfds=1, timeout=299999) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f9c69870e71 in mongoc_socket_recv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#2  0x00007f9c69873f47 in _mongoc_stream_socket_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#3  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#4  0x00007f9c6982d324 in _mongoc_buffer_fill () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#5  0x00007f9c69871b9d in mongoc_stream_buffered_readv () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#6  0x00007f9c6987266b in mongoc_stream_read () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#7  0x00007f9c6982d082 in _mongoc_buffer_append_from_stream () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#8  0x00007f9c69838689 in mongoc_cluster_run_opmsg () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#9  0x00007f9c69839777 in mongoc_cluster_run_command_monitored () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#10 0x00007f9c6984ad88 in _mongoc_cursor_run_command () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#11 0x00007f9c6984cc22 in _mongoc_cursor_response_refresh () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#12 0x00007f9c6984df74 in _prime () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#13 0x00007f9c6984b2f1 in mongoc_cursor_next () from /home/ubuntu/mongo-c-driver/cmake-build-quickfix2/src/libmongoc/libmongoc-1.0.so.0
#14 0x0000558c13e2f446 in thread_find ()
#15 0x00007f9c693b26db in start_thread (arg=0x7f9c62922700) at pthread_create.c:463
#16 0x00007f9c690db71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95

Thread 1 (Thread 0x7f9c69cca740 (LWP 98675)):
#0  0x00007f9c693b3d2d in __GI___pthread_timedjoin_ex (threadid=140309645371136, thread_return=0x0, abstime=0x0, block=<optimized out>) at pthread_join_common.c:89
#1  0x0000558c13e2f137 in main ()
