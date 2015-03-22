int step_chip(void);
int CheckSerialIn(void);
void CheckCommand(void);
void putchx(int32_t val);
void CheckSerialOut(void);
void spinsim_exit(int32_t exitcode);

#define WAIT_CNT   01
#define WAIT_MULT  02
#define WAIT_PIN   03
#define WAIT_HUB   16
#define WAIT_CACHE 17

