#include <pthread.h>

/* TODO:[P4-task4] pthread_create/wait */
void pthread_create(pthread_t *thread,
    void (*start_routine)(void *),
    void *arg)
{
    /* TODO: [p4-task4] implement pthread_create */
    /* global pthread id */
    static uintptr_t pthread_id = 0;

    *thread = ++pthread_id;
    sys_pthread_create(thread, start_routine, arg, (uint64_t) sys_exit);

}

int pthread_join(pthread_t thread)
{
    /* TODO: [p4-task4] implement pthread_join */
    sys_pthread_join(thread);
}