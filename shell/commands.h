#ifndef COMMANDS_H
#define COMMANDS_H

// kernel functions used by commands
void putln(const char* s, unsigned char fg, unsigned char bg);
void puts(const char* s, unsigned char fg, unsigned char bg);
void putc(char c, unsigned char fg, unsigned char bg);
void win_title(const char* title, unsigned char fg, unsigned char bg);
void win_end();
void cls();
void draw_bar(int percent, unsigned char fg, unsigned char bg);
void int_to_str(int n, char* buf);
void str_to_int_decl(const char* s);
void pad_zero(int n, char* buf);
int str_to_int(const char* s);
int streq(const char* a, const char* b);
int startswith(const char* str, const char* prefix);
unsigned char inb(unsigned short port);
void outb(unsigned short port, unsigned char val);
unsigned char read_cmos(unsigned char reg);
unsigned char bcd_to_bin(unsigned char bcd);

// shell commands
void shell_help();
void shell_sysinfo();
void shell_ram();
void shell_cpu();
void shell_clock();
void shell_taskmgr();
void shell_version();
void shell_qsf();
void shell_color();
void shell_reboot();
void shell_calc(const char* expr);
void shell_echo(const char* text);
void run_command(char* cmd);

#endif
