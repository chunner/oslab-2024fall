#include <asm.h>
#include <csr.h>

ENTRY(setup_exception)

  la t0, exception_handler_entry//save exception_handler_entry into STVEC
  csrw stvec, t0

  li t0, SR_SIE       // enable interrupts globally
  or t0, t0, SR_SUM   // enable supervision access user memory
  csrs CSR_SSTATUS, t0

ENDPROC(setup_exception)
