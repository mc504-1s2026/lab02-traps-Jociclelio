#include <kernel/serial.h>
#include <kernel/panic.h>
#include <arch/spinlock.h>
#include <arch/plic.h>
#include <arch/csr.h>

#define SERIAL_BUF_SIZE 1024

struct serialdev {
	char buf[SERIAL_BUF_SIZE];
	size_t head;
	size_t tail;
	struct spinlock lock;
} dev;

static inline void serial_write_reg(u8 reg, u8 val)
{
	volatile u8 *addr = (volatile u8 *)((u8 *)SERIAL_BASE + reg);
	*addr = val;
}

static inline u8 serial_read_reg(u8 reg)
{
	volatile u8 *addr = (volatile u8 *)((u8 *)SERIAL_BASE + reg);
	return *addr;
}

u8 serial_read_byte()
{
	while (!(serial_read_reg(SERIAL_LSR) & SERIAL_LSR_DTR));
	return serial_read_reg(SERIAL_RBR);
}

void serial_init()
{
	spin_init(&dev.lock);
	dev.head = 0;
	dev.tail = 0;

	/* 8 bits, no parity, one stop bit */
	serial_write_reg(SERIAL_LCR, 0x03);
	/* Enable FIFOs, clear RX and TX FIFOs */
	serial_write_reg(SERIAL_FCR, SERIAL_FCR_FIFO_ENABLE | SERIAL_FCR_RX_FIFO_CLEAR | SERIAL_FCR_TX_FIFO_CLEAR);
}

void serial_irq_enable()
{
	/* Enable Received Data Available Interrupt */
	serial_write_reg(SERIAL_IER, SERIAL_IER_ERBFI);

	/* Configure PLIC for serial interrupt */
	plic_irq_set_priority(IRQ_SERIAL, 7);
	plic_hart_enable_irq(0, IRQ_SERIAL);
	plic_hart_set_threshold(0, 0);

	/* Enable external interrupts in sie */
	csr_set(CSR_SIE, CSR_SIE_SEIE);
}

void serial_irq_disable()
{
	serial_write_reg(SERIAL_IER, 0);
	csr_clear(CSR_SIE, CSR_SIE_SEIE);
}

void serial_irq()
{
	/* Check if it's Received Data Available */
	while (serial_read_reg(SERIAL_LSR) & SERIAL_LSR_DTR) {
		u8 c = serial_read_reg(SERIAL_RBR);
		
		size_t next = (dev.head + 1) % SERIAL_BUF_SIZE;
		if (next != dev.tail) {
			dev.buf[dev.head] = c;
			dev.head = next;
		}
	}
}

size_t serial_read(char *buf)
{
	size_t count = 0;
	u64 flags = spin_lock_irqsave(&dev.lock);
	while (dev.tail != dev.head) {
		buf[count++] = dev.buf[dev.tail];
		dev.tail = (dev.tail + 1) % SERIAL_BUF_SIZE;
	}
	spin_unlock_irqrestore(&dev.lock, flags);

	return count;
}

void serial_putc(char c)
{
	/* Wait until transmitter holding register is empty */
	while (!(serial_read_reg(SERIAL_LSR) & SERIAL_LSR_THRE));
	serial_write_reg(SERIAL_THR, c);
}

void serial_puts(char *str)
{
	while (*str) {
		serial_putc(*str++);
	}
}
