/*
 * Declarations worth keeping :)
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "alloc.h"
#include "cpu.h"
#include "errno.h"
#include "format.h"
#include "list.h"
#include "wait.h"

#include "config.h"

/* Useful macros for the kernel to use */
#define nelem(x) (sizeof(x) / sizeof(x[0]))

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) < (y) ? (y) : (x))

#define ALIGN(x, a)           __ALIGN_MASK(x, (typeof(x))(a)-1)
#define __ALIGN_MASK(x, mask) (((x) + (mask)) & ~(mask))

#define WRITE32(_reg, _val)                                                    \
	do {                                                                   \
		register uint32_t __myval__ = (_val);                          \
		*(volatile uint32_t *)&(_reg) = __myval__;                     \
	} while (0)
#define READ32(_reg) (*(volatile uint32_t *)&(_reg))
#define READ64(_reg) (*(volatile uint64_t *)&(_reg))

struct ctx;
struct process;

#define SOS_VERSION "0.1"

#define __nopreempt __attribute__((section(".nopreempt")))

/*
 * Linker symbols for virtual memory addresses.
 */
extern uint8_t code_start[];
extern uint8_t code_end[];
extern uint8_t data_start[];
extern uint8_t bss_start[];
extern uint8_t data_end[];
extern uint8_t stack_start[];
extern uint8_t stack_end[];
extern uint8_t unused_start[];
extern uint8_t dynamic_start[];
/* NB: this is actually a uint32_t array because it points to a table of words
 */
extern uint32_t first_level_table[];

/*
 * Address of the UART is stored as a variable.
 */
extern uint32_t uart_base;

/*
 * Some well-known addresses which we determine at runtime.
 */
extern void *second_level_table;

extern void *fiq_stack;
extern void *irq_stack;
extern void *abrt_stack;
extern void *undf_stack;
extern void *svc_stack;

/*
 * Basic I/O (see uart.s and format.c for details)
 */
void puts(char *string);
void nputs(char *string, int n);
void putc(char c);
int getc_blocking(void);
int getc_spinning(void);
void uart_init(void);
void uart_init_irq(void);
void uart_remap(void);
void uart_wait(struct process *p);
void uart_isr(uint32_t intid, struct ctx *ctx);
uint32_t snprintf(char *buf, uint32_t size, const char *format, ...);
uint32_t printf(const char *format, ...);

/*
 * Initialize kernel memory system (see mem.c for details)
 */
void kmem_init(uint32_t phys);

/*
 * Return pages of kernel memory, already mapped and everything!
 */
void *kmem_get_pages(uint32_t bytes, uint32_t align);
void *kmem_get_page(void);

/*
 * Free that memory.
 */
void kmem_free_pages(void *virt_ptr, uint32_t len);
void kmem_free_page(void *ptr);

/*
 * Look up the physical address corresponding to a virtual address
 */
uint32_t kmem_lookup_phys(void *virt_ptr);
uint32_t umem_lookup_phys(struct process *p, void *virt_ptr);

/*
 * Map physical memory into the virtual memory space.
 */
void kmem_map_pages(uint32_t virt, uint32_t phys, uint32_t len, uint32_t attrs);
void umem_map_pages(struct process *p, uint32_t virt, uint32_t phys,
                    uint32_t len, uint32_t attrs);
uint32_t kmem_remap_periph(uint32_t addr);

/*
 * Unmap memory
 */
void kmem_unmap_pages(uint32_t virt, uint32_t len);
void umem_unmap_pages(struct process *p, uint32_t virt, uint32_t len);

/*
 * Print it!
 */
void kmem_print(uint32_t start, uint32_t stop);
void umem_print(struct process *p, uint32_t start, uint32_t stop);

extern void *phys_allocator;
extern void *kern_virt_allocator;

/*
 * MMU Constants
 */
#define SLD__AP2  (1 << 9)
#define SLD__AP1  (1 << 5)
#define SLD__AP0  (1 << 4)
#define SLD__C    (1 << 3)
#define SLD__B    (1 << 2)
#define SLD__TEX0 (1 << 6)
#define SLD__TEX1 (1 << 7)
#define SLD__TEX2 (1 << 8)
#define SLD__S    (1 << 10)

/* first level descriptor types */
#define FLD_UNMAPPED 0x00
#define FLD_COARSE   0x01
#define FLD_SECTION  0x02

#define FLD_MASK 0x03

/* second level descriptor types */
#define SLD_UNMAPPED 0x00
#define SLD_LARGE    0x01
#define SLD_SMALL    0x02

#define SLD_MASK 0x03

#define SLD_NG (1 << 11)

/* access control for second level */
#define NOT_GLOBAL    (0x1 << 11)
#define PRW_UNA       (SLD__AP0)            /* AP=0b001 */
#define PRW_URO       (SLD__AP1)            /* AP=0b010 */
#define PRW_URW       (SLD__AP1 | SLD__AP0) /* AP=0b011 */
#define PRO_UNA       (SLD__AP2 | SLD__AP0) /* AP=0b101 */
#define PRO_URO       (SLD__AP2 | SLD__AP1) /* AP=0b110 */
#define EXECUTE_NEVER 0x01

/* Memory attributes: Choose either:
 * - DEVICE_SHAREABLE
 * - DEVICE_NONSHAREABLE
 * - NORMAL_SHAREABLE
 * - NORMAL_NONSHAREABLE
 *
 * Normal memory can further select cache attributes.
 */
#define DEVICE_SHAREABLE    (SLD__B)
#define DEVICE_NONSHAREABLE (SLD__TEX1)
#define NORMAL_SHAREABLE    (SLD__TEX2 | SLD__S)
#define NORMAL_NONSHAREABLE (SLD__TEX2)
#define CACHE_INNER_NC      (0)
#define CACHE_INNER_WBWA    (SLD__TEX0)
#define CACHE_INNER_WT      (SLD__TEX1)
#define CACHE_INNER_WB      (SLD__TEX1 | SLD__TEX0)
#define CACHE_OUTER_NC      (0)
#define CACHE_OUTER_WBWA    (SLD__B)
#define CACHE_OUTER_WT      (SLD__C)
#define CACHE_OUTER_WB      (SLD__C | SLD__B)

/* Default for kernel memory is to be shareable and non cacheable. Someday I'll
 * think about enabling the caches. */
#define KMEM_ATTR_DEFAULT (NORMAL_SHAREABLE | CACHE_INNER_WB | CACHE_OUTER_WB)
#define KMEM_PERM_DATA    (PRW_UNA | EXECUTE_NEVER)
#define KMEM_PERM_CODE    (PRO_UNA)

/* permissions and attrs for umem, default */
#define UMEM_DEFAULT (NORMAL_SHAREABLE | PRW_URW | NOT_GLOBAL)

/*
 * KMalloc
 */
void *kmalloc(uint32_t size);
void kfree(void *ptr, uint32_t size);
void kmalloc_init(void);

/*
 * System info debugging command (see sysinfo.c for details)
 */
void sysinfo(void);

/*
 * Set up the stack for all the other modes ahead of time, so we can use them.
 */
void setup_stacks(void *location);

/**
 * This "process" is hardly a process. It runs in user mode, but shares its
 * memory space with the kernel. It has a separate stack and separate registers,
 * and it can call a "relinquish()" method which swi's back into svc mode. This
 * simulates a process without actually doing anything.
 */
struct process {
	/* For kernel thread, the stack */
	void *kstack;

	struct ctx context;

	struct {
		int pr_ready : 1;  /* ready to be scheduled? */
		int pr_kernel : 1; /* is a kernel thread? */
	} flags;

	/** Global process list entry. */
	struct list_head list;

	/** List of sockets */
	struct list_head sockets;

	uint32_t max_fildes;

	/** Basically a pid */
	uint32_t id;

	/** Size of the process image file. */
	uint32_t size;

	/** Physical address of process image. */
	uint32_t phys;

	/** Allocator for the process address space. */
	void *vmem_allocator;

	/** First-level page table and shadow page table. */
	uint32_t ttbr1;
	uint32_t *first;
	uint32_t **shadow;

	/** Waitlist for when the process ends */
	struct waitlist endlist;
};

/* Create a process */
struct process *create_process(uint32_t binary);
struct process *create_kthread(void (*func)(void *), void *arg);
#define BIN_SALUTATIONS 0
#define BIN_HELLO       1
#define BIN_USH         2
int32_t process_image_lookup(char *name);

/* Destroy the current process and reschedule. Does not return. */
void destroy_current_process(void);

/* Schedule (i.e. choose and contextswitch a new process) */
void schedule(void);
void context_switch(struct process *new_process);
bool timer_can_reschedule(struct ctx *ctx);
void irq_schedule(struct ctx *ctx);
extern bool preempt_enabled;
static inline void preempt_disable(void)
{
	preempt_enabled = false;
}
static inline void preempt_enable(void)
{
	preempt_enabled = true;
}

/* The current process */
extern struct process *current;
extern struct list_head process_list;

/* Initialize process subsystem */
void process_init(void);

/*
 * Pre-built binary processes
 */
extern uint32_t process_salutations_start[];
extern uint32_t process_salutations_end[];
extern uint32_t process_hello_start[];
extern uint32_t process_hello_end[];
extern uint32_t process_ush_start[];
extern uint32_t process_ush_end[];

/* "uncomment" this if you want to debug page allocations */
#ifdef DEBUG_PAGE_ALLOCATOR_CALLS
#define alloc_pages(allocator, bytes, align)                                   \
	({                                                                     \
		uint32_t rv = alloc_pages(allocator, bytes, align);            \
		printf("%s:%u: (in %s) alloc_pages(%s, 0x%x, %u) = 0x%x\n",    \
		       __FILE__, __LINE__, __func__, #allocator, bytes, align, \
		       rv);                                                    \
		rv;                                                            \
	})
#endif

void dtb_init(uint32_t phys);

/* ksh commands */
int virtio_net_cmd_status(int argc, char **argv);
int virtio_net_cmd_dhcpdiscover(int argc, char **argv);
int dhcp_cmd_discover(int argc, char **argv);
int ip_cmd_show_arptable(int argc, char **argv);

/* GIC Driver */
typedef void (*isr_t)(uint32_t, struct ctx *);
void gic_init(void);
void gic_enable_interrupt(uint8_t int_id);
uint32_t gic_interrupt_acknowledge(void);
void gic_end_interrupt(uint32_t int_id);
void gic_register_isr(uint32_t intid_start, uint32_t intid_count, isr_t isr,
                      char *name);
isr_t gic_get_isr(uint32_t intid);
char *gic_get_name(uint32_t intid);

/* timer */
void timer_init(void);
void timer_isr(uint32_t intid, struct ctx *ctx);

/* special exectuion functions, see entry.s */
int __nopreempt setctx(struct ctx *ctx);
void __nopreempt resctx(uint32_t rv, struct ctx *ctx);

/* Virtio */
void virtio_init(void);

struct virtio_net;
struct packet;
void virtio_net_send(struct virtio_net *dev, struct packet *pkt);
struct netif;
extern struct netif nif;

void packet_init(void);
struct packet *udp_wait(uint16_t port);

int copy_from_user(void *kerndst, const void *usersrc, size_t n);
int copy_to_user(void *userdst, const void *kernsrc, size_t n);

void dhcp_kthread_start(void);

// debug
void backtrace(void);
void backtrace_ctx(struct ctx *ctx);
void panic(struct ctx *ctx);
