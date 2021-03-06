#!/bin/sh

cd `dirname $0`

if [ "$JAVA_HOME" = "" ]; then JAVA_HOME=/usr/java/default; fi

CORE_FILES="\
src/fs-poll.c \
src/inet.c \
src/threadpool.c \
src/uv-common.c \
src/version.c \
src/unix/async.c \
src/unix/core.c \
src/unix/dl.c \
src/unix/fs.c \
src/unix/getaddrinfo.c \
src/unix/getnameinfo.c \
src/unix/linux-core.c \
src/unix/linux-inotify.c \
src/unix/linux-syscalls.c \
src/unix/loop.c \
src/unix/loop-watcher.c \
src/unix/pipe.c \
src/unix/poll.c \
src/unix/process.c \
src/unix/procfs-exepath.c \
src/unix/proctitle.c \
src/unix/signal.c \
src/unix/stream.c \
src/unix/sysinfo-loadavg.c \
src/unix/sysinfo-memory.c \
src/unix/tcp.c \
src/unix/thread.c \
src/unix/timer.c \
src/unix/tty.c \
src/unix/udp.c \
src/jni.c \
"

TEST_FILES="\
test/blackhole-server.c \
test/dns-server.c \
test/echo-server.c \
test/runner.c \
test/runner-unix.c \
test/run-tests.c \
test/test-active.c \
test/test-async.c \
test/test-async-null-cb.c \
test/test-barrier.c \
test/test-callback-order.c \
test/test-callback-stack.c \
test/test-close-fd.c \
test/test-close-order.c \
test/test-condvar.c \
test/test-connection-fail.c \
test/test-cwd-and-chdir.c \
test/test-default-loop-close.c \
test/test-delayed-accept.c \
test/test-dlerror.c \
test/test-eintr-handling.c \
test/test-embed.c \
test/test-emfile.c \
test/test-env-vars.c \
test/test-error.c \
test/test-fail-always.c \
test/test-fork.c \
test/test-fs.c \
test/test-fs-copyfile.c \
test/test-fs-event.c \
test/test-fs-poll.c \
test/test-getaddrinfo.c \
test/test-get-currentexe.c \
test/test-gethostname.c \
test/test-get-loadavg.c \
test/test-get-memory.c \
test/test-getnameinfo.c \
test/test-get-passwd.c \
test/test-getsockname.c \
test/test-handle-fileno.c \
test/test-homedir.c \
test/test-hrtime.c \
test/test-idle.c \
test/test-ip4-addr.c \
test/test-ip6-addr.c \
test/test-ipc.c \
test/test-ipc-send-recv.c \
test/test-loop-alive.c \
test/test-loop-close.c \
test/test-loop-configure.c \
test/test-loop-handles.c \
test/test-loop-stop.c \
test/test-loop-time.c \
test/test-multiple-listen.c \
test/test-mutexes.c \
test/test-osx-select.c \
test/test-pass-always.c \
test/test-ping-pong.c \
test/test-pipe-bind-error.c \
test/test-pipe-close-stdout-read-stdin.c \
test/test-pipe-connect-error.c \
test/test-pipe-connect-multiple.c \
test/test-pipe-connect-prepare.c \
test/test-pipe-getsockname.c \
test/test-pipe-pending-instances.c \
test/test-pipe-sendmsg.c \
test/test-pipe-server-close.c \
test/test-pipe-set-non-blocking.c \
test/test-platform-output.c \
test/test-poll.c \
test/test-poll-close.c \
test/test-poll-close-doesnt-corrupt-stack.c \
test/test-poll-closesocket.c \
test/test-poll-oob.c \
test/test-process-title.c \
test/test-queue-foreach-delete.c \
test/test-ref.c \
test/test-run-nowait.c \
test/test-run-once.c \
test/test-semaphore.c \
test/test-shutdown-close.c \
test/test-shutdown-eof.c \
test/test-shutdown-twice.c \
test/test-signal.c \
test/test-signal-multiple-loops.c \
test/test-socket-buffer-size.c \
test/test-spawn.c \
test/test-stdio-over-pipes.c \
test/test-tcp-alloc-cb-fail.c \
test/test-tcp-bind6-error.c \
test/test-tcp-bind-error.c \
test/test-tcp-close.c \
test/test-tcp-close-accept.c \
test/test-tcp-close-while-connecting.c \
test/test-tcp-connect6-error.c \
test/test-tcp-connect-error.c \
test/test-tcp-connect-error-after-write.c \
test/test-tcp-connect-timeout.c \
test/test-tcp-create-socket-early.c \
test/test-tcp-flags.c \
test/test-tcp-oob.c \
test/test-tcp-open.c \
test/test-tcp-read-stop.c \
test/test-tcp-shutdown-after-write.c \
test/test-tcp-try-write.c \
test/test-tcp-unexpected-read.c \
test/test-tcp-write-after-connect.c \
test/test-tcp-writealot.c \
test/test-tcp-write-fail.c \
test/test-tcp-write-queue-order.c \
test/test-tcp-write-to-half-open-connection.c \
test/test-thread.c \
test/test-thread-equal.c \
test/test-threadpool.c \
test/test-threadpool-cancel.c \
test/test-timer.c \
test/test-timer-again.c \
test/test-timer-from-check.c \
test/test-tmpdir.c \
test/test-tty.c \
test/test-udp-alloc-cb-fail.c \
test/test-udp-bind.c \
test/test-udp-create-socket-early.c \
test/test-udp-dgram-too-big.c \
test/test-udp-ipv6.c \
test/test-udp-multicast-interface.c \
test/test-udp-multicast-interface6.c \
test/test-udp-multicast-join.c \
test/test-udp-multicast-join6.c \
test/test-udp-multicast-ttl.c \
test/test-udp-open.c \
test/test-udp-options.c \
test/test-udp-send-and-recv.c \
test/test-udp-send-hang-loop.c \
test/test-udp-send-immediate.c \
test/test-udp-send-unreachable.c \
test/test-udp-try-send.c \
test/test-walk-handles.c \
test/test-watcher-cross-stop.c \
"

COMPILE="gcc -std=gnu89 -DNDEBUG -DENABLE_JNI -Iinclude -Isrc -I${JAVA_HOME}/include -I${JAVA_HOME}/include/linux -m64 -O3 -ffast-math -fweb -fomit-frame-pointer -fmerge-all-constants -fPIC -pipe -pthread -ldl -lrt -lutil"

echo building libuvjni64.so ...
$COMPILE -DBUILDING_UV_SHARED -shared -fvisibility=hidden -Wl,-soname -Wl,libuvjni64.so -o libuvjni64.so $CORE_FILES

echo building libuv_tests64 ...
$COMPILE -o libuv_tests64 $CORE_FILES $TEST_FILES
