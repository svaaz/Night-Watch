#ifndef _PW_INC_H_
#define _PW_INC_H_

#define PW_SOCK_PATH "/tmp/keepAlive"

#define PW_EVENT_SIZE  ( sizeof (struct inotify_event) )

#define PW_MAX_LEN 120

#define PW_CONF_FILE "pw.conf"

#define PW_FILE_NAME_LEN 120

#define PW_MAX_CLIENTS 1

#define PW_MAX_MSG_LEN  512

#define PW_DFLT_FREQUENCY 5

#define PW_DFLT_THRESHOLD 120

typedef struct keepalive
{
    int msg_type;
    int frequency;
    int action;
    int threshold;
    int syslog_ip;
    int reserved;

}keepalive_t;


enum 
{
	PW_KEEP_ALIVE = 0,
	PW_KEEP_ALIVE_MODIFIED = 1,
	PW_BOOT_UP_SUCCESS = 2,
	PW_UNEXPECTED_EXIT = 3,
	PW_DISABLE_KEEP_ALIVE =0xFF
};

enum
{
	PW_ACTION_NONE,
	PW_ACTION_RESTART,
	PW_ACTION_SYSLOG,
	PW_ACTION_BOTH
};


void SystemRestart (void);

void ThresholdTimerHandler (void);

char* getTimeString (void);


#endif
