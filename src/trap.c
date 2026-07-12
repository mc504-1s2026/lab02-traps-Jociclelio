#include <kernel/trap.h>
#include <kernel/panic.h>
#include <kernel/printf.h>
#include <arch/csr.h>
#include <arch/timer.h>
#include <kernel/serial.h>
#include <arch/plic.h>

/* defined in src/trap_entry.S */
extern void trap_entry();

void handle_irq()
{
	u64 cause = csr_read(CSR_SCAUSE);
	u64 irq_code = cause & ~TRAP_IRQ_BIT;

	if (irq_code == 0x5) {
		timer_irq();
	} else if (irq_code == 0x9) {
		u32 irq = plic_hart_claim_irq(0);
		if (irq == IRQ_SERIAL) {
			serial_irq();
		}
		plic_hart_complete_irq(0, irq);
	} else {
		error("unhandled interrupt: 0x%lx\n", cause);
		BUG();
	}
}

void handle_exception()
{
	u64 cause = csr_read(CSR_SCAUSE);
	u64 val = csr_read(CSR_STVAL);
	u64 epc = csr_read(CSR_SEPC);

	error("exception occurred!\n");
	error("cause: 0x%lx\n", cause);
	error("stval: 0x%lx\n", val);
	error("sepc: 0x%lx\n", epc);

	if (cause == EXCEPTION_INST_PAGE_FAULT || cause == EXCEPTION_LOAD_PAGE_FAULT || cause == EXCEPTION_STORE_PAGE_FAULT) {
		error("page fault at address 0x%lx\n", val);
	}

	panic("unhandled exception");
}

void trap_setup()
{
	csr_write(CSR_STVEC, (u64)trap_entry);
	hart_irq_disable();
}

void handle_trap()
{
	u64 cause = csr_read(CSR_SCAUSE);
	if (cause & TRAP_IRQ_BIT) {
		handle_irq();
	} else {
		serial_putc('E');
		handle_exception();
	}
}

void hart_irq_enable()
{
	csr_set(CSR_SSTATUS, CSR_SSTATUS_SIE);
}

u64 hart_irq_save()
{
	u64 sstatus = csr_read(CSR_SSTATUS);
	hart_irq_disable();
	return sstatus & CSR_SSTATUS_SIE;
}

void hart_irq_restore(u64 flags)
{
	if (flags) {
		hart_irq_enable();
	} else {
		hart_irq_disable();
	}
}

void hart_irq_disable()
{
	csr_clear(CSR_SSTATUS, CSR_SSTATUS_SIE);
}
