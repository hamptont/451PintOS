#ifndef _FORK_UTILS_
#define _FORK_UTILS_

uint32_t *pagedir_duplicate (uint32_t *src_pd);
struct thread *create_child_thread (void);
void jump_to_intr_exit (struct intr_frame frame);
void setup_thread_to_return_from_fork (struct thread *new_thread, struct intr_frame *f);

#endif
