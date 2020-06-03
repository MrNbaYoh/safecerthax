#include "types.h"

extern u8 g_unitInfo;
extern u32 g_bootEnv;

void k9Sync(void);
void p9InitComms(void);
Result p9ReceiveNotification11(u32 *outNotificationId, u32 serviceId);
Result p9McShutdown(void);

Result firmlaunch(u64 firmlaunchTid);
