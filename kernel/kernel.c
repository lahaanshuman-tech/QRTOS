#include "fs/fas32q.h"
void run_command(char* cmd);
// IDT structures
struct idt_entry {
    unsigned short base_low;
    unsigned short selector;
    unsigned char zero;
    unsigned char flags;
    unsigned short base_high;
} __attribute__((packed));

struct idt_ptr {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

struct idt_entry idt[256];
struct idt_ptr idtp;

extern void idt_flush(unsigned int);
extern void irq1_wrapper();

volatile unsigned char key_buffer[256];
volatile int key_head = 0;
volatile int key_tail = 0;

volatile unsigned short* vga;
int cx;
int cy;
char input_buf[256];
int input_len;
int shift_pressed;

unsigned short mkchar(char c, unsigned char fg, unsigned char bg) {
    return ((unsigned short)((bg << 4) | fg) << 8) | (unsigned short)c;
}

void cls() {
    int i;
    for (i = 0; i < 80*24; i++)
        vga[i] = mkchar(' ', 0, 7);
    cx = 0; cy = 0;
}

void draw_taskbar() {
    int i;
    for (i = 0; i < 80; i++)
        vga[24 * 80 + i] = mkchar(' ', 0, 7);
    char* start = " [QRTOS] ";
    for (i = 0; start[i]; i++)
        vga[24 * 80 + i] = mkchar(start[i], 0, 7);
    unsigned char val;
    unsigned char sec, min, hour;
    __asm__ volatile("outb %0, %1" : : "a"((unsigned char)0x00), "Nd"((unsigned short)0x70));
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"((unsigned short)0x71));
    sec = ((val >> 4) * 10) + (val & 0x0F);
    __asm__ volatile("outb %0, %1" : : "a"((unsigned char)0x02), "Nd"((unsigned short)0x70));
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"((unsigned short)0x71));
    min = ((val >> 4) * 10) + (val & 0x0F);
    __asm__ volatile("outb %0, %1" : : "a"((unsigned char)0x04), "Nd"((unsigned short)0x70));
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"((unsigned short)0x71));
    hour = ((val >> 4) * 10) + (val & 0x0F);
    char time[9];
    time[0]='0'+hour/10; time[1]='0'+hour%10; time[2]=':';
    time[3]='0'+min/10;  time[4]='0'+min%10;  time[5]=':';
    time[6]='0'+sec/10;  time[7]='0'+sec%10;  time[8]='\0';
    for (i = 0; time[i]; i++)
        vga[24 * 80 + 71 + i] = mkchar(time[i], 0, 7);
}

void scroll() {
    int i;
    for (i = 0; i < 80*23; i++)
        vga[i] = vga[i + 80];
    for (i = 80*23; i < 80*24; i++)
        vga[i] = mkchar(' ', 0, 7);
    cy = 23;
}

void putc(char c, unsigned char fg, unsigned char bg) {
    if (c == '\n') { cx = 0; cy++; if (cy >= 24) scroll(); return; }
    if (cy >= 24) scroll();
    vga[cy * 80 + cx] = mkchar(c, fg, bg);
    cx++;
    if (cx >= 80) { cx = 0; cy++; if (cy >= 24) scroll(); }
}

void puts(const char* s, unsigned char fg, unsigned char bg) {
    int i;
    for (i = 0; s[i]; i++) putc(s[i], fg, bg);
}

void putln(const char* s, unsigned char fg, unsigned char bg) {
    puts(s, fg, bg); putc('\n', fg, bg);
}

void hline(char c, unsigned char fg, unsigned char bg) {
    int i;
    for (i = 0; i < 80; i++) putc(c, fg, bg);
    putc('\n', fg, bg);  // ← this pushes cx to 0 but cy might be wrong
}

void win_title(const char* title, unsigned char fg, unsigned char bg) {
    putc('\n', 0, 7);
    int i; int len = 0;
    while (title[len]) len++;
    putc('+', fg, bg);
    for (i = 0; i < 78; i++) putc('-', fg, bg);
    putc('+', fg, bg); putc('\n', 0, 7);
    putc('|', fg, bg); putc(' ', fg, bg);
    puts(title, fg, bg);
    for (i = len+2; i < 79; i++) putc(' ', fg, bg);
    putc('|', fg, bg); putc('\n', 0, 7);
    putc('+', fg, bg);
    for (i = 0; i < 78; i++) putc('-', fg, bg);
    putc('+', fg, bg); putc('\n', 0, 7);
}

void win_end() {
    int i;
    putc('+', 8, 7);
    for (i = 0; i < 78; i++) putc('-', 8, 7);
    putc('+', 8, 7); putc('\n', 0, 7);
}

void update_cursor() {
    unsigned short pos = cy * 80 + cx;
    __asm__ volatile("outb %0, %1" : : "a"((unsigned char)0x0F), "Nd"((unsigned short)0x3D4));
    __asm__ volatile("outb %0, %1" : : "a"((unsigned char)(pos & 0xFF)), "Nd"((unsigned short)0x3D5));
    __asm__ volatile("outb %0, %1" : : "a"((unsigned char)0x0E), "Nd"((unsigned short)0x3D4));
    __asm__ volatile("outb %0, %1" : : "a"((unsigned char)(pos >> 8)), "Nd"((unsigned short)0x3D5));
}

unsigned char inb(unsigned short port) {
    unsigned char val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

void outb(unsigned short port, unsigned char val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

void idt_set_gate(unsigned char num, unsigned int base,
                  unsigned short sel, unsigned char flags) {
    idt[num].base_low  = (base & 0xFFFF);
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].selector  = sel;
    idt[num].zero      = 0;
    idt[num].flags     = flags;
}

void idt_install() {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base  = (unsigned int)&idt;
    int i;
    for (i = 0; i < 256; i++)
        idt_set_gate(i, 0, 0x08, 0x8E);
    idt_set_gate(0x21, (unsigned int)irq1_wrapper, 0x08, 0x8E);
    idt_flush((unsigned int)&idtp);
}

void irq1_handler() {
    unsigned char sc = inb(0x60);
    outb(0x20, 0x20);
    key_buffer[key_head] = sc;
    key_head = (key_head + 1) % 256;
}

static const char scancode_to_ascii[] = {
    0,0,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,' '
};

static const char scancode_to_ascii_shift[] = {
    0,0,'!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?',0,
    '*',0,' '
};

void keyboard_init() {
    idt_install();
    while (inb(0x64) & 0x01) inb(0x60);
    __asm__ volatile("sti");
}

char read_key() {
    while (key_head == key_tail);
    unsigned char sc = key_buffer[key_tail];
    key_tail = (key_tail + 1) % 256;
    if (sc == 0x2A || sc == 0x36) { shift_pressed = 1; return 0; }
    if (sc == 0xAA || sc == 0xB6) { shift_pressed = 0; return 0; }
    if (sc & 0x80) return 0;
    if (sc < 58 && scancode_to_ascii[sc] != 0) {
        if (shift_pressed) return scancode_to_ascii_shift[sc];
        return scancode_to_ascii[sc];
    }
    return 0;
}

char read_key_nowait() {
    if (key_head == key_tail) return 0;
    unsigned char sc = key_buffer[key_tail];
    key_tail = (key_tail + 1) % 256;
    if (sc == 0x2A || sc == 0x36) { shift_pressed = 1; return 0; }
    if (sc == 0xAA || sc == 0xB6) { shift_pressed = 0; return 0; }
    if (sc & 0x80) return 0;
    if (sc < 58 && scancode_to_ascii[sc] != 0) {
        if (shift_pressed) return scancode_to_ascii_shift[sc];
        return scancode_to_ascii[sc];
    }
    return 0;
}

int streq(const char* a, const char* b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == b[i];
}

int startswith(const char* str, const char* prefix) {
    int i = 0;
    while (prefix[i]) {
        if (str[i] != prefix[i]) return 0;
        i++;
    }
    return 1;
}

int str_to_int(const char* s) {
    int result = 0; int i = 0; int neg = 0;
    if (s[0] == '-') { neg = 1; i = 1; }
    for (; s[i] >= '0' && s[i] <= '9'; i++)
        result = result * 10 + (s[i] - '0');
    return neg ? -result : result;
}

void int_to_str(int n, char* buf) {
    int i = 0; int neg = 0;
    if (n == 0) { buf[0]='0'; buf[1]='\0'; return; }
    if (n < 0)  { neg=1; n=-n; }
    char tmp[32];
    while (n > 0) { tmp[i++]='0'+(n%10); n/=10; }
    if (neg) tmp[i++]='-';
    int j = 0;
    while (i > 0) buf[j++]=tmp[--i];
    buf[j]='\0';
}

void pad_zero(int n, char* buf) {
    if (n < 10) { buf[0]='0'; buf[1]='0'+n; buf[2]='\0'; }
    else int_to_str(n, buf);
}

void draw_bar(int percent, unsigned char fg, unsigned char bg) {
    int i; int filled = percent / 5;
    puts("| [", 8, 7);
    for (i = 0; i < 20; i++) {
        if (i < filled) putc('#', fg, bg);
        else putc('-', 8, 7);
    }
    puts("] ", 8, 7);
    char buf[8]; int_to_str(percent, buf);
    puts(buf, fg, bg); puts("%\n", fg, bg);
}

unsigned char read_cmos(unsigned char reg) {
    outb(0x70, reg);
    return inb(0x71);
}

unsigned char bcd_to_bin(unsigned char bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}



void print_prompt() {
    draw_taskbar();
    puts("C:\\QRTOS> ", 0, 7);
    update_cursor();
}

void draw_boot_screen() {
    int i;
    for (i=0; i<80*25; i++) vga[i]=mkchar(' ',0,7);
    cx=0; cy=0;

    putln("  QRTOS",1,7);
    putln("  Qourtra Software Foundation | v1 x86 2026",8,7);
    putln("  ======================================",8,7);
    putln("  [OK] Bootloader complete",2,7);
    putln("  [OK] Protected mode active",2,7);
    putln("  [OK] VGA driver loaded",2,7);
    putln("  [OK] Keyboard IRQ1 installed",2,7);
    putln("  [OK] FAS20Q filesystem ready",2,7);
    putln("  [OK] QRTOS shell ready",2,7);
    putln("  ======================================",8,7);
    putln("  Welcome to QRTOS! Type 'help'",0,7);
}

void kernel_main() {
    vga = (volatile unsigned short*)0xB8000;
    cx = 0;
    cy = 0;
    input_len = 0;
    shift_pressed = 0;
    draw_boot_screen();
    keyboard_init();
    

    // NO cls() here! just go straight to prompt
    print_prompt();

    while (1) {
        char c = read_key();
        if (!c) continue;
        draw_taskbar();

        if (c == '\n') {
            input_buf[input_len] = '\0';
            putc('\n', 0, 7);
            if (input_len > 0) run_command(input_buf);
            input_len = 0;
            print_prompt();
        } else if (c == '\b') {
            if (input_len > 0) {
                input_len--;
                if (cx > 0) cx--;
                vga[cy * 80 + cx] = mkchar(' ', 0, 7);
                update_cursor();
            }
        } else {
            if (input_len < 255) {
                input_buf[input_len++] = c;
                putc(c, 0, 7);
                update_cursor();
            }
        }
    }
}

static int cmd_streq(const char* a, const char* b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == b[i];
}

static int cmd_startswith(const char* str, const char* prefix) {
    int i = 0;
    while (prefix[i]) {
        if (str[i] != prefix[i]) return 0;
        i++;
    }
    return 1;
}
#define MAX_PROC 7
char* proc_names[MAX_PROC] = {
    "kernel","shell","vga_driver",
    "kb_driver","fas20q_fs","clock_svc","idle"
};
int proc_state[MAX_PROC] = {1,1,1,1,1,1,1};
int proc_cpu[MAX_PROC]   = {2,1,1,0,1,0,95};
int proc_mem[MAX_PROC]   = {8,2,1,1,2,1,1};

void shell_taskmgr() {
    win_title("QRTOS Task Manager", 0, 7);
    putln("|  PID  Name           State    CPU%  MEM(KB)", 8, 7);
    putln("|  ------------------------------------------------", 8, 7);
    int i; char buf[16];
    for (i = 0; i < MAX_PROC; i++) {
        puts("|  ", 8, 7);
        int_to_str(i, buf); puts(buf, 1, 7);
        puts("      ", 0, 7);
        int j = 0;
        while (proc_names[i][j]) { putc(proc_names[i][j], 0, 7); j++; }
        for (; j < 15; j++) putc(' ', 0, 7);
        if (proc_state[i]) puts("Running  ", 2, 7);
        else                puts("Stopped  ", 4, 7);
        int_to_str(proc_cpu[i], buf); puts(buf, 1, 7); puts("%     ", 0, 7);
        int_to_str(proc_mem[i], buf); puts(buf, 1, 7); putln(" KB", 8, 7);
    }
    putln("|  ------------------------------------------------", 8, 7);
    puts("|  Total CPU: 100%   Total MEM: ", 0, 7);
    int total = 0;
    for (i = 0; i < MAX_PROC; i++) total += proc_mem[i];
    int_to_str(total, buf);
    puts(buf, 1, 7); putln(" KB used", 8, 7);
    win_end();
}

void shell_clock() {
    win_title("QRTOS Clock", 0, 7);
    unsigned char sec  = bcd_to_bin(read_cmos(0x00));
    unsigned char min  = bcd_to_bin(read_cmos(0x02));
    unsigned char hour = bcd_to_bin(read_cmos(0x04));
    unsigned char day  = bcd_to_bin(read_cmos(0x07));
    unsigned char mon  = bcd_to_bin(read_cmos(0x08));
    unsigned char year = bcd_to_bin(read_cmos(0x09));
    char sh[4]; char sm[4]; char ss[4];
    char sd[4]; char smo[4]; char sy[8];
    pad_zero(hour,sh); pad_zero(min,sm); pad_zero(sec,ss);
    pad_zero(day,sd);  pad_zero(mon,smo);
    int_to_str(2000+year,sy);
    puts("|  Time:  ",0,7);
    puts(sh,1,7); puts(":",8,7); puts(sm,1,7); puts(":",8,7); putln(ss,1,7);
    puts("|  Date:  ",0,7);
    puts(sd,1,7); puts("/",8,7); puts(smo,1,7); puts("/",8,7); putln(sy,1,7);
    win_end();
}

void shell_ram() {
    win_title("QRTOS Memory Manager",0,7);
    int total_kb=640; int kernel_kb=8;
    int stack_kb=4;   int used_kb=kernel_kb+stack_kb;
    int free_kb=total_kb-used_kb;
    int used_pct=(used_kb*100)/total_kb;
    int free_pct=100-used_pct;
    char buf[32];
    puts("|  Total:  ",0,7); int_to_str(total_kb,buf); puts(buf,1,7); putln(" KB",8,7);
    puts("|  Used:   ",0,7); int_to_str(used_kb,buf);  puts(buf,4,7); putln(" KB",8,7);
    puts("|  Free:   ",0,7); int_to_str(free_kb,buf);  puts(buf,2,7); putln(" KB",8,7);
    putln("|",8,7);
    puts("|  Used: ",0,7); draw_bar(used_pct,4,7);
    puts("|  Free: ",0,7); draw_bar(free_pct,2,7);
    putln("|",8,7);
    putln("|  Memory Map:",0,7);
    putln("|  0x00000  BIOS + IVT",8,7);
    putln("|  0x01000  QRTOS Kernel",1,7);
    putln("|  0x03000  Free memory",2,7);
    putln("|  0x7FF00  Stack",4,7);
    putln("|  0xB8000  VGA buffer",3,7);
    win_end();
}

void shell_cpu() {
    win_title("QRTOS CPU Monitor",0,7);
    unsigned int eax,ebx,ecx,edx;
    char vendor[13];
    __asm__ volatile("cpuid"
        : "=a"(eax),"=b"(ebx),"=c"(ecx),"=d"(edx) : "a"(0));
    vendor[0]=(ebx>>0)&0xFF;  vendor[1]=(ebx>>8)&0xFF;
    vendor[2]=(ebx>>16)&0xFF; vendor[3]=(ebx>>24)&0xFF;
    vendor[4]=(edx>>0)&0xFF;  vendor[5]=(edx>>8)&0xFF;
    vendor[6]=(edx>>16)&0xFF; vendor[7]=(edx>>24)&0xFF;
    vendor[8]=(ecx>>0)&0xFF;  vendor[9]=(ecx>>8)&0xFF;
    vendor[10]=(ecx>>16)&0xFF;vendor[11]=(ecx>>24)&0xFF;
    vendor[12]='\0';
    puts("|  Vendor:    ",0,7); putln(vendor,1,7);
    __asm__ volatile("cpuid"
        : "=a"(eax),"=b"(ebx),"=c"(ecx),"=d"(edx) : "a"(1));
    char buf[32];
    puts("|  Family:    ",0,7); int_to_str((eax>>8)&0xF,buf); putln(buf,1,7);
    puts("|  Model:     ",0,7); int_to_str((eax>>4)&0xF,buf); putln(buf,1,7);
    puts("|  Stepping:  ",0,7); int_to_str((eax>>0)&0xF,buf); putln(buf,1,7);
    puts("|  Mode:      ",0,7); putln("32-bit Protected Mode",2,7);
    puts("|  Features:  ",0,7);
    if (edx&(1<<25)) puts("SSE ",2,7);
    if (edx&(1<<26)) puts("SSE2 ",2,7);
    if (ecx&(1<<0))  puts("SSE3 ",2,7);
    if (edx&(1<<4))  puts("TSC ",2,7);
    if (edx&(1<<23)) puts("MMX ",2,7);
    putc('\n',0,7);
    win_end();
}

void shell_calc(const char* expr) {
    int i=0; char op=0;
    char num1[32]; char num2[32];
    int n1=0; int n2=0;
    while (expr[i]==' ') i++;
    if (expr[i]=='-') num1[n1++]=expr[i++];
    while (expr[i]>='0'&&expr[i]<='9') num1[n1++]=expr[i++];
    num1[n1]='\0';
    while (expr[i]==' ') i++;
    if (expr[i]=='+'||expr[i]=='-'||
        expr[i]=='*'||expr[i]=='/') { op=expr[i++]; }
    else { putln("|  Error: invalid expression",4,7); return; }
    while (expr[i]==' ') i++;
    if (expr[i]=='-') num2[n2++]=expr[i++];
    while (expr[i]>='0'&&expr[i]<='9') num2[n2++]=expr[i++];
    num2[n2]='\0';
    if (n1==0||n2==0) { putln("|  Error: missing number",4,7); return; }
    int a=str_to_int(num1); int b=str_to_int(num2);
    int result=0; int rem=0;
    if      (op=='+') result=a+b;
    else if (op=='-') result=a-b;
    else if (op=='*') result=a*b;
    else if (op=='/') {
        if (b==0) { putln("|  Error: division by zero!",4,7); return; }
        result=a/b; rem=a%b;
    }
    char sa[32]; char sb[32]; char sr[32]; char srm[32];
    int_to_str(a,sa); int_to_str(b,sb); int_to_str(result,sr);
    win_title("QRTOS Calculator",0,7);
    puts("|  ",0,7);
    puts(sa,1,7); puts(" ",0,7); putc(op,1,7); puts(" ",0,7);
    puts(sb,1,7); puts(" = ",0,7); puts(sr,2,7);
    if (op=='/'&&rem!=0) {
        int_to_str(rem,srm);
        puts("  (rem: ",8,7); puts(srm,1,7); puts(")",8,7);
    }
    putc('\n',0,7);
    win_end();
}

void shell_echo(const char* text) {
    puts("|  ",8,7); putln(text,0,7);
}

void shell_color() {
    win_title("QRTOS Color Palette",0,7);
    int i;
    for (i=0; i<16; i++) {
        puts("|  ",8,7);
        puts("  Color  ",i,7);
        puts("  ",0,7);
        char buf[4]; int_to_str(i,buf);
        puts("(",8,7); puts(buf,8,7); puts(")\n",8,7);
    }
    win_end();
}

void shell_reboot() {
    putln("|  Rebooting...",4,7);
    unsigned char good=0x02;
    while (good&0x02) good=inb(0x64);
    outb(0x64, 0xFE);
    while(1) {}
}

void shell_help() {
    putln("  QRTOS v1 Commands:", 1, 7);
    putln("  help2 sysinfo ram cpu clock", 8, 7);
    putln("  taskmgr version qsf color", 8, 7);
    putln("  calc N op N | echo <text>", 8, 7);
    putln("  reboot clear | help2=FS cmds", 8, 7);
}

void shell_help2() {
    putln("  FAS32Q Commands:", 1, 7);
    putln("  ls mkdir <n> touch <n>", 8, 7);
    putln("  del <n> recycle restore <n>", 8, 7);
}

void shell_sysinfo() {
    win_title("QRTOS System Information",0,7);
    putln("|  OS:         QRTOS v1",0,7);
    putln("|  Arch:       x86 32-bit protected mode",0,7);
    putln("|  VGA:        80x25 text mode 16 colors",0,7);
    putln("|  Filesystem: FAS20Q v1",0,7);
    putln("|  Kernel:     v1 build 1",0,7);
    putln("|  Dev:        Qourtra Software Foundation",0,7);
    putln("|  Year:       2026",0,7);
    win_end();
}

void shell_qsf() {
    win_title("Qourtra Software Foundation",0,7);
    putln("|  Building QRTOS from scratch",0,7);
    putln("|  FAS20Q custom filesystem",0,7);
    putln("|  Est. 2026",0,7);
    win_end();
}

void shell_version() {
    win_title("QRTOS Version",0,7);
    putln("|  QRTOS v1",0,7);
    putln("|  Bootloader:  v1",0,7);
    putln("|  Kernel:      v1 build 1",0,7);
    putln("|  Filesystem:  FAS20Q v1",0,7);
    putln("|  Shell:       v1",0,7);
    win_end();
}

void shell_ls() {
    win_title("FAS32Q - Files", 1, 7);
    int i; int count = 0;
    for (i = 0; i < FAS32Q_MAX_FILES; i++) {
        if (ram_files[i].flags == 0) continue;
        if (ram_files[i].flags & FAS32Q_FLAG_DELETED) continue;
        puts("|  ", 8, 7);
        if (ram_files[i].flags & FAS32Q_FLAG_FOLDER)
            puts("[DIR] ", 3, 7);
        else
            puts("[FILE] ", 2, 7);
        putln(ram_files[i].name, 0, 7);
        count++;
    }
    if (count == 0) putln("|  No files yet.", 8, 7);
    win_end();
}

void shell_mkdir(const char* name) {
    int r = fas32q_ram_create(name, FAS32Q_FLAG_FOLDER);
    if (r == -1) putln("|  Error: could not create folder!", 4, 7);
    else { puts("|  Created folder: ", 2, 7); putln(name, 0, 7); }
}

void shell_touch(const char* name) {
    int r = fas32q_ram_create(name, FAS32Q_FLAG_FILE);
    if (r == -1) putln("|  Error: could not create file!", 4, 7);
    else { puts("|  Created file: ", 2, 7); putln(name, 0, 7); }
}

void shell_del(const char* name) {
    int r = fas32q_ram_delete(name);
    if (r == -1) putln("|  Error: file not found!", 4, 7);
    else { puts("|  Deleted: ", 4, 7); putln(name, 0, 7); }
}

void shell_recycle() {
    win_title("FAS32Q - Recycle Bin", 4, 7);
    int i; int count = 0;
    for (i = 0; i < FAS32Q_MAX_FILES; i++) {
        if (!(ram_files[i].flags & FAS32Q_FLAG_DELETED)) continue;
        puts("|  ", 8, 7);
        putln(ram_files[i].name, 4, 7);
        count++;
    }
    if (count == 0) putln("|  Recycle bin is empty.", 8, 7);
    win_end();
}

void shell_restore(const char* name) {
    int r = fas32q_ram_restore(name);
    if (r == -1) putln("|  Error: file not found in recycle bin!", 4, 7);
    else { puts("|  Restored: ", 2, 7); putln(name, 0, 7); }
}
void run_command(char* cmd) {
    putln("TEST", 1, 7);  // ← add this line temporarily
    // rest of code...
    // manual inline comparisons - no function call
    char* p;
    
    p = "help";
    if (cmd[0]==p[0]&&cmd[1]==p[1]&&cmd[2]==p[2]&&cmd[3]==p[3]&&cmd[4]=='\0') { shell_help(); return; }
    if (cmd[0]=='h'&&cmd[1]=='e'&&cmd[2]=='l'&&cmd[3]=='p'&&cmd[4]=='2') { shell_help2(); return; }
    p = "clear";
    if (cmd[0]=='c'&&cmd[1]=='l'&&cmd[2]=='e'&&cmd[3]=='a'&&cmd[4]=='r'&&cmd[5]=='\0') { cls(); return; }
    p = "ram";
    if (cmd[0]=='r'&&cmd[1]=='a'&&cmd[2]=='m'&&cmd[3]=='\0') { shell_ram(); return; }
    p = "cpu";
    if (cmd[0]=='c'&&cmd[1]=='p'&&cmd[2]=='u'&&cmd[3]=='\0') { shell_cpu(); return; }
    p = "clock";
    if (cmd[0]=='c'&&cmd[1]=='l'&&cmd[2]=='o'&&cmd[3]=='c'&&cmd[4]=='k'&&cmd[5]=='\0') { shell_clock(); return; }
    if (cmd[0]=='c'&&cmd[1]=='a'&&cmd[2]=='l'&&cmd[3]=='c'&&cmd[4]==' ') { shell_calc(cmd+5); return; }
    if (cmd[0]=='e'&&cmd[1]=='c'&&cmd[2]=='h'&&cmd[3]=='o'&&cmd[4]==' ') { shell_echo(cmd+5); return; }
    if (cmd[0]=='t'&&cmd[1]=='e'&&cmd[2]=='s'&&cmd[3]=='t') {
    putln("LINE1", 1, 7);
    putln("LINE2", 2, 7);
    putln("LINE3", 4, 7);
    return;
}
    // FAS32Q commands
    if (cmd[0]=='l'&&cmd[1]=='s'&&cmd[2]=='\0') { shell_ls(); return; }
    if (cmd[0]=='m'&&cmd[1]=='k'&&cmd[2]=='d') { shell_mkdir(cmd+6); return; }
    if (cmd[0]=='t'&&cmd[1]=='o'&&cmd[2]=='u') { shell_touch(cmd+6); return; }
    if (cmd[0]=='d'&&cmd[1]=='e'&&cmd[2]=='l') { shell_del(cmd+4); return; }
    if (cmd[0]=='r'&&cmd[1]=='e'&&cmd[2]=='c') { shell_recycle(); return; }
    if (cmd[0]=='r'&&cmd[1]=='e'&&cmd[2]=='s') { shell_restore(cmd+8); return; }
    if (cmd[0]=='s'&&cmd[1]=='y'&&cmd[2]=='s') { shell_sysinfo(); return; }
    if (cmd[0]=='t'&&cmd[1]=='a'&&cmd[2]=='s') { shell_taskmgr(); return; }
    if (cmd[0]=='v'&&cmd[1]=='e'&&cmd[2]=='r') { shell_version(); return; }
    if (cmd[0]=='q'&&cmd[1]=='s'&&cmd[2]=='f'&&cmd[3]=='\0') { shell_qsf(); return; }
    if (cmd[0]=='c'&&cmd[1]=='o'&&cmd[2]=='l') { shell_color(); return; }
    if (cmd[0]=='r'&&cmd[1]=='e'&&cmd[2]=='b') { shell_reboot(); return; }

    puts("|  Unknown: ",4,7); putln(cmd,0,7);
    putln("|  Type 'help' for commands.",8,7);
}

