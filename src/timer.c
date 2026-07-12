#include <arch/timer.h>
#include <kernel/panic.h>
#include <arch/csr.h>
#include <kernel/serial.h>

static u64 boot_time = 0;
static void (*alarm_callback)(void) = NULL;

u64 timer_read()
{
	return csr_read(CSR_TIME);
}

void timer_irq_enable()
{
	if (boot_time == 0) {
		boot_time = timer_read();
	}
	csr_set(CSR_SIE, CSR_SIE_STIE);
}

void timer_irq_disable()
{
	csr_clear(CSR_SIE, CSR_SIE_STIE);
}

void timer_set_alarm(u64 secs)
{
	u64 now = timer_read();
	u64 tick_in_secs = now + secs * TIMER_FREQ;
	csr_write(CSR_STIMECMP, tick_in_secs);
}

u64 timer_uptime()
{
	return (timer_read() - boot_time) / TIMER_FREQ;
}

void timer_set_alarm_callback(void (*cb)(void))
{
	alarm_callback = cb;
}

void timer_irq()
{
	/* The timer interrupt is triggered when TIME == STIMECMP. */
	
	if (alarm_callback) {
		alarm_callback();
	}

	/* Stop immediate re-triggering */
	csr_write(CSR_STIMECMP, 0xFFFFFFFFFFFFFFFFUL);
}
