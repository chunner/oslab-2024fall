#include <asm.h>
#include <csr.h>

ENTRY(setup_exception)

  /* TODO: [p2-task3] save exception_handler_entry into STVEC */
  la t0, exception_handler_entry
  csrw stvec, t0

  li t0, SIE_SSIE
  csrs sie, t0
  /* TODO: [p2-task4] enable interrupts globally */
  addi sp, sp, -8
  sd ra, (sp)
  jal enable_interrupt
  ld ra, (sp)
  addi sp, sp, 8
  ret
ENDPROC(setup_exception)
