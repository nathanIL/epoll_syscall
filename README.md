epoll_syscall_test
==================

Just testing epoll syscall and sharing for a friend.
Basically, it forks a sub-process, one creates the fifos and writes to them, the other multiplex them
for changes and prints the changes.

Tested on Fedora 20 (x86_64) and compiled with gcc version 4.8.2 20131212 (Red Hat 4.8.2-7) (GCC)

