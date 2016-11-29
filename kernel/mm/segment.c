#include <x86.h>
#include <mm/segment.h>

extern char bootstack[], bootstacktop[];

/* *
 * Task State Segment:
 *
 * The TSS may reside anywhere in memory. A special segment register called
 * the Task Register (TR) holds a segment selector that points a valid TSS
 * segment descriptor which resides in the GDT. Therefore, to use a TSS
 * the following must be done in function gdt_init:
 *   - create a TSS descriptor entry in GDT
 *   - add enough information to the TSS in memory as needed
 *   - load the TR register with a segment selector for that segment
 *
 * There are several fileds in TSS for specifying the new stack pointer when a
 * privilege level change happens. But only the fields SS0 and ESP0 are useful
 * in our os kernel.
 *
 * The field SS0 contains the stack segment selector for CPL = 0, and the ESP0
 * contains the new ESP value for CPL = 0. When an interrupt happens in protected
 * mode, the x86 CPU will look in the TSS for SS0 and ESP0 and load their value
 * into SS and ESP respectively.
 * */
static TaskState ts = {0};

/**
 * Global Descriptor Table:
 *
 * The kernel and user segments are identical (except for the DPL). To load
 * the %ss register, the CPL must equal the DPL. Thus, we must duplicate the
 * segments for the user and the kernel. Defined as follows:
 *   - 0x0 :  unused (always faults -- for trapping NULL far pointers)
 *   - 0x8 :  kernel code segment
 *   - 0x10:  kernel data segment
 *   - 0x18:  user code segment
 *   - 0x20:  user data segment
 *   - 0x28:  defined for tss, initialized in gdt_init
 */
static SegmentDescriptor gdt[NSEGS] = {{0}};

/* gdt_init - initialize the default GDT and TSS */
void GDTInitialize(void) 
{
    // set boot kernel stack and default SS0
    // load_esp0((uintptr_t)bootstacktop);
    ts.ss0 = KERNEL_DS;
    ts.esp0 = (uint32_t)bootstacktop;

    gdt[0] = SEG_NULL,
    gdt[SEG_KCODE] = SEG(STA_X | STA_R, 0x0, 0xFFFFFFFF, DPL_KERNEL);
    gdt[SEG_KDATA] = SEG(STA_W, 0x0, 0xFFFFFFFF, DPL_KERNEL);
    gdt[SEG_UCODE] = SEG(STA_X | STA_R, 0x0, 0xFFFFFFFF, DPL_USER);
    gdt[SEG_UDATA] = SEG(STA_W, 0x0, 0xFFFFFFFF, DPL_USER);
    gdt[SEG_TSS]   = SEG_NULL;
    // initialize the TSS filed of the gdt
    gdt[SEG_TSS] = SEGTSS(STS_T32A, (uintptr_t)&ts, sizeof(ts), DPL_KERNEL);

    // reload all segment registers
    lgdt(gdt, sizeof(gdt));
    loadgs(USER_DS);
    loadfs(USER_DS);
    loades(KERNEL_DS);
    loadds(KERNEL_DS);
    loadss(KERNEL_DS);

    // reload cs
    asm volatile ("ljmp %0, $1f\n 1:\n" :: "i" (KERNEL_CS));

    // load the TSS
    ltr(GD_TSS);
}