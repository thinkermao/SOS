#include "console.h"
#include "debug.h"
#include "flags.h"
#include "traps.h"
#include "types.h"
#include "proc.h"
#include "segment.h"
#include "spinlock.h"
#include "x86.h"

#define IDT_DESC_CNT 0xff

#define IDT_DESC_P      1
#define IDT_DESC_DPL0   0
#define IDT_DESC_DPL3   3
#define IDT_DESC_32_TYPE 0xe
#define TRAP_DESC_32_TYPE 0xf

#define IDT_DESC_ATTR_DPL0 \
		((IDT_DESC_P << 7) + (IDT_DESC_DPL0 << 5) + IDT_DESC_32_TYPE)
#define IDT_DESC_ATTR_DPL3 \
		((IDT_DESC_P << 7) + (IDT_DESC_DPL3 << 5) + IDT_DESC_32_TYPE)

typedef void *intr_handler;

typedef struct gate_desc {
    uint16_t func_offset_low16;
    uint16_t selector;
    uint8_t  receved;
    uint8_t  attribute;
    uint16_t func_offset_high16;
} gate_desc;

extern void trap_ret(void);
extern void lapiceoi(void);
extern void kbd_intr(void);

static gate_desc idt[IDT_DESC_CNT];
extern intr_handler vectors[];
struct spinlock tickslock;
uint32_t ticks;

static void make_trap_vector(
    gate_desc *desc, uint8_t attr, intr_handler func)
{
    desc->func_offset_high16 = ((uint32_t)func & 0xffff0000) >> 16;
    desc->func_offset_low16 = (uint32_t)func & 0x0000ffff;
    desc->receved = 0;
    desc->selector = SEG_KCODE << 3;
    desc->attribute = attr;
}

void trap_vector_init(void)
{
    int i;
    for (i = 0; i < IDT_DESC_CNT; ++i) {
        make_trap_vector(&idt[i], IDT_DESC_ATTR_DPL0, vectors[i]);
    }

	init_lock(&tickslock, "ticks");
}

void idt_init() 
{
    lidt(idt, sizeof(idt));
}

void trap(struct trap_frame *tf)
{
	printk("vector: %d\n", tf->trapno);
    assert(tf->trapno != T_PGFLT);

	if(tf->trapno == T_SYSCALL){
		// if(proc->killed)
		//   exit();
		// proc->tf = tf;
		// syscall();
		// if(proc->killed)
		//   exit();
		return;
	}

	switch(tf->trapno){
	case T_IRQ0 + IRQ_TIMER:
		if(cpu->id == 0){
		  acquire(&tickslock);
		  ticks++;
		  //wakeup(&ticks);
		  release(&tickslock);
		}
		lapiceoi();
		break;
	case T_IRQ0 + IRQ_IDE:
		//ideintr();
		lapiceoi();
		break;
	case T_IRQ0 + IRQ_IDE+1:
		// Bochs generates spurious IDE1 interrupts.
		break;
	case T_IRQ0 + IRQ_KBD:
		kbd_intr();
		lapiceoi();
		break;
	case T_IRQ0 + IRQ_COM1:
		//uartintr();
		lapiceoi();
		break;
	case T_IRQ0 + 7:
	case T_IRQ0 + IRQ_SPURIOUS:
		printk("cpu%d: spurious interrupt at %x:%x\n",
		        cpu->id, tf->cs, tf->eip);
		lapiceoi();
		break;

	default:
		// if(proc == 0 || (tf->cs&3) == 0){
		//   // In kernel, it must be our mistake.
		//   cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
		//           tf->trapno, cpu->id, tf->eip, rcr2());
		//   panic("trap");
		// }
		// // In user space, assume process misbehaved.
		// cprintf("pid %d %s: trap %d err %d on cpu %d "
		//         "eip 0x%x addr 0x%x--kill proc\n",
		//         proc->pid, proc->name, tf->trapno, tf->err, cpu->id, tf->eip,
		//         rcr2());
		// proc->killed = 1;
		break;
	}

	// Force process exit if it has been killed and is in user space.
	// (If it is still executing in the kernel, let it keep running
// 	// until it gets to the regular system call return.)
//   if(proc && proc->killed && (tf->cs&3) == DPL_USER)
// 		exit();

// 	// Force process to give up CPU on clock tick.
// 	// If interrupts were on while locks held, would need to check nlock.
//   if(proc && proc->state == RUNNING && tf->trapno == T_IRQ0+IRQ_TIMER)
// 		yield();

// 	// Check if the process has been killed since we yielded
//   if(proc && proc->killed && (tf->cs&3) == DPL_USER)
// 		exit();
	
}