#include <asm.h>
#include <csr.h>

ENTRY(setup_exception)
  la t0, exception_handler_entry//save exception_handler_entry into STVEC
  csrw stvec, t0

  li t0, SR_SIE       // enable interrupts globally
  li t1, SR_SUM   // enable supervision access user memory
  or t0, t1, t0
  csrs sstatus, t0
  
  jr ra
ENDPROC(setup_exception)
