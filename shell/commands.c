#include "../fs/fas32q.h"
#include "commands.h"


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
// ── Forward declarations ───────────────────
void putln(const char* s, unsigned char fg, unsigned char bg);
void puts(const char* s, unsigned char fg, unsigned char bg);
void putc(char c, unsigned char fg, unsigned char bg);
void win_title(const char* title, unsigned char fg, unsigned char bg);
void win_end();
void cls();
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
    putln("  QRTOS Help", 1, 7);
    putln("  -------------------------", 0, 7);
    putln("  help        - this help", 0, 7);
    putln("  clear       - clear screen", 0, 7);
    putln("  sysinfo     - system info", 0, 7);
    putln("  ram         - memory usage", 0, 7);
    putln("  cpu         - CPU info", 0, 7);
    putln("  clock       - date and time", 0, 7);
    putln("  taskmgr     - task manager", 0, 7);
    putln("  version     - QRTOS version", 0, 7);
    putln("  qsf         - about QSF", 0, 7);
    putln("  calc N op N - calculator", 0, 7);
    putln("  echo <text> - print text", 0, 7);
    putln("  color       - color palette", 0, 7);
    putln("  reboot      - reboot QRTOS", 0, 7);
    putln("  -------------------------", 0, 7);
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

void run_command(char* cmd) {
    putln("TEST", 1, 7);  // ← add this line temporarily
    // rest of code...
    // manual inline comparisons - no function call
    char* p;
    
    p = "help";
    if (cmd[0]==p[0]&&cmd[1]==p[1]&&cmd[2]==p[2]&&cmd[3]==p[3]&&cmd[4]=='\0') { shell_help(); return; }
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
    if (cmd[0]=='s'&&cmd[1]=='y'&&cmd[2]=='s') { shell_sysinfo(); return; }
    if (cmd[0]=='t'&&cmd[1]=='a'&&cmd[2]=='s') { shell_taskmgr(); return; }
    if (cmd[0]=='v'&&cmd[1]=='e'&&cmd[2]=='r') { shell_version(); return; }
    if (cmd[0]=='q'&&cmd[1]=='s'&&cmd[2]=='f'&&cmd[3]=='\0') { shell_qsf(); return; }
    if (cmd[0]=='c'&&cmd[1]=='o'&&cmd[2]=='l') { shell_color(); return; }
    if (cmd[0]=='r'&&cmd[1]=='e'&&cmd[2]=='b') { shell_reboot(); return; }

    puts("|  Unknown: ",4,7); putln(cmd,0,7);
    putln("|  Type 'help' for commands.",8,7);
}

