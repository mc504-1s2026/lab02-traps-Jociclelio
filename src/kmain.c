#include <kernel/printf.h>
#include <kernel/mm.h>
#include <arch/timer.h>
#include <kernel/trap.h>
#include <kernel/serial.h>
#include <kernel/string.h>

extern int _hartid[];

void alarm_callback()
{
	serial_puts("\nalarm\n> ");
}

void execute_command(char *cmd)
{
	if (strcmp(cmd, "uptime") == 0) {
		u64 uptime = timer_uptime();
		char buf[32];
		int i = 0;
		if (uptime == 0) {
			buf[i++] = '0';
		} else {
			while (uptime > 0) {
				buf[i++] = (uptime % 10) + '0';
				uptime /= 10;
			}
		}
		buf[i] = '\0';
		for (int j = 0; j < i / 2; j++) {
			char tmp = buf[j];
			buf[j] = buf[i - 1 - j];
			buf[i - 1 - j] = tmp;
		}
		serial_puts(buf);
		serial_puts("s\n");
	} else if (strncmp(cmd, "echo ", 5) == 0) {
		serial_puts(cmd + 5);
		serial_puts("\n");
	} else if (strncmp(cmd, "alarm ", 6) == 0) {
		char *time_str = cmd + 6;
		u64 secs = 0;
		while (*time_str >= '0' && *time_str <= '9') {
			secs = secs * 10 + (*time_str - '0');
			time_str++;
		}
		timer_set_alarm_callback(alarm_callback);
		timer_set_alarm(secs);
	} else if (cmd[0] != '\0') {
		serial_puts("unknown command\n");
	}
	serial_puts("> ");
}

void kmain()
{
	printk_set_level(LOG_DEBUG);
	info("entered S-mode\n");
	info("booting on hart %d\n", _hartid[0]);
	info("setting up virtual memory...\n");
	vm_init();

	info("enabling traps...\n");
	trap_setup();
	info("enabling timer...\n");
	timer_irq_enable();
	info("enabling serial...\n");
	serial_init();
	serial_irq_enable();
	hart_irq_enable();

	serial_puts("Welcome to the kernel shell!\n");
	serial_puts("> ");

	char line[256];
	int pos = 0;

	while (1) {
		char buf[32];
		size_t n = serial_read(buf);
		for (size_t i = 0; i < n; i++) {
			char c = buf[i];
			if (c == '\r') {
				line[pos] = '\0';
				serial_puts("\n");
				execute_command(line);
				pos = 0;
			} else if (c == '\b') {
				if (pos > 0) {
					pos--;
					serial_puts("\b \b");
				}
			} else {
				if (pos < 255) {
					line[pos++] = c;
					serial_putc(c);
				}
			}
		}
	}
}
