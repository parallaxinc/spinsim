#ifndef __CONION_H__
#define __CONION_H__

#undef STD_CONSOLE_INPUT

int  kbhit(void);
int  getch(void);
void initialize_console_io(void);
void restore_console_io(void);

#endif
