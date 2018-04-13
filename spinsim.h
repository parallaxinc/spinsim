#include <stdint.h>

int step_chip(void);
//int CheckSerialIn(SerialT *serial);
void CheckCommand(void);
void putchx(int32_t val);
//void CheckSerialOut(SerialT *serial);
void spinsim_exit(int32_t exitcode);

#define WAIT_CNT    01
#define WAIT_CORDIC 02
#define WAIT_PIN    03
#define WAIT_HUB    16
#define WAIT_CACHE  17
#define WAIT_FLAG   18

#ifdef __MINGW32__
#define NEW_LINE "\n"
#else
#define NEW_LINE "\r\n"
#endif
