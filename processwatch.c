// Server
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>


#include <linux/inotify.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <syslog.h>
#include <unistd.h>
#include <time.h>

#include "processwatch.h"

int mon_sock_id = -1;
int acc_sock_id = -1;
int gProcessWatchStatus = 1;

static int counter = 0;
keepalive_t gProcessWatchConfig;
static int conn_accepted = 0;

struct itimerval thresholTmr;


void ProcessWatchPrintConfig(void)
{
	/* This will print in the syslog as a debug level 
	 * log during config update and bootup */
	syslog (LOG_NOTICE,  "ProcessWatch Config Enabled   : %d    ; Syslog IP : %d"
			" Threshold : %d Frequency : %d    ; Action    : %d\n"
			,gProcessWatchStatus, gProcessWatchConfig.syslog_ip, gProcessWatchConfig.threshold,
			gProcessWatchConfig.frequency, gProcessWatchConfig.action);

}

char* getTimeString ()
{
	time_t rawtime;
	struct tm * timeinfo;

	time ( &rawtime );
	timeinfo = localtime ( &rawtime );
	return (asctime (timeinfo));
}

void ProcessWatchConfigUpdate(keepalive_t* pkt)
{
	gProcessWatchConfig.syslog_ip = pkt->syslog_ip;
	gProcessWatchConfig.frequency = pkt->frequency;
	gProcessWatchConfig.action = pkt->action;

	/* if Threshold value has changed update the timer */
	gProcessWatchConfig.threshold = pkt->threshold;

	return;
}

void ProcessWatchConfigInit(void)
{
	FILE               *fp = NULL;
	char               au1Buf[PW_MAX_LEN];
	char               au1FileName[PW_FILE_NAME_LEN];

	memset (au1Buf, 0, PW_MAX_LEN);

	memset (au1FileName, 0, sizeof(au1FileName));
	snprintf(au1FileName, PW_FILE_NAME_LEN, "%s%s", "/tmp/", PW_CONF_FILE);

	fp = fopen (au1FileName, "w+");

	if (fp == NULL)
	{
		return ;
	}
	gProcessWatchConfig.syslog_ip = 0;
	gProcessWatchConfig.frequency = PW_DFLT_FREQUENCY;
	gProcessWatchConfig.threshold = PW_DFLT_THRESHOLD;
	gProcessWatchConfig.action = PW_ACTION_SYSLOG;

	/* Initialize config with defaults */
	fprintf(fp,"%s %d %s", "frequency", gProcessWatchConfig.frequency, "\n");
	fprintf(fp,"%s %d %s", "action-on-fail", gProcessWatchConfig.action, "\n");
	fprintf(fp,"%s %d %s", "watch-threshold", gProcessWatchConfig.threshold, "\n");
	fprintf(fp,"%s %d %s", "on", gProcessWatchStatus, "\n");
	
	fclose(fp);
}


void ProcessWatchConfigRead(void)
{
    FILE               *fp = NULL;
    char               au1Buf[PW_MAX_LEN];
    char               au1ReadStr[PW_MAX_LEN];
    char               au1TmpValue[PW_MAX_LEN];
    char               au1FileName[PW_FILE_NAME_LEN];

    memset (au1Buf, 0, PW_MAX_LEN);

    memset (au1FileName, 0, sizeof(au1FileName));
    snprintf(au1FileName, PW_FILE_NAME_LEN, "%s%s", "/root/", PW_CONF_FILE);

    fp = fopen (au1FileName, "r");

	/* First check whether the file exist or not */
	if (fp == NULL)
	{
		ProcessWatchConfigInit();
		return ;
	}

    while (!feof (fp))
    {
        fgets (au1Buf, PW_MAX_LEN, fp);

        memset (au1ReadStr, 0, PW_MAX_LEN);
        memset (au1TmpValue, 0, PW_MAX_LEN);

        sscanf (au1Buf, "%s%s", au1ReadStr, au1TmpValue);

        if ((strcmp (au1ReadStr, "log-ip")) == 0)
        {
            gProcessWatchConfig.syslog_ip = atoi(au1TmpValue);
        }
        else if ((strcmp (au1ReadStr, "frequency")) == 0)
        {
            gProcessWatchConfig.frequency = atoi(au1TmpValue);
        }
        else if ((strcmp (au1ReadStr, "action-on-fail")) == 0)
        {
            gProcessWatchConfig.action = atoi(au1TmpValue);
        }
        else if ((strcmp (au1ReadStr, "on")) == 0)
        {
            gProcessWatchStatus = atoi(au1TmpValue);
        }
        else if ((strcmp (au1ReadStr, "watch-threshold")) == 0)
        {
            gProcessWatchConfig.threshold = atoi(au1TmpValue);
        }
    }

    fclose(fp);

    return;
}


void ProcessWatchInit(void)
{
	system("rm -rf /tmp/keepAlive");
    memset(&gProcessWatchConfig, 0, sizeof(gProcessWatchConfig));
	openlog ("PW", LOG_PID, LOG_USER | LOG_SYSLOG | LOG_PERROR);
	syslog (LOG_NOTICE, "Program PW started !");
	/* Read config or create new config file with defaults*/
    ProcessWatchConfigRead();
    ProcessWatchPrintConfig();
}


void ProcessWatchSockInit()
{
    struct sockaddr_un local;
	int len = 0;

    if ((mon_sock_id = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) 
    {
		syslog (LOG_ERR, "%s",strerror(errno));
        return;
    }

    bzero(&local, sizeof(local));

    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, PW_SOCK_PATH);
    unlink(local.sun_path);
    len = strlen(local.sun_path) + sizeof(local.sun_family);

    if (bind(mon_sock_id, (struct sockaddr *)&local, len) == -1) 
    {
		syslog (LOG_ERR, "%s",strerror(errno));
        return;
    }

    if (listen(mon_sock_id, PW_MAX_CLIENTS) == -1) 
    {
		syslog (LOG_ERR, "%s",strerror(errno));
        return;
    }

}

void *ProcessWatchProcessMsg(void *arg)
{
    int t;
    struct sockaddr_un  remote;
    char msg[PW_MAX_MSG_LEN];
    int msg_type =0;
	int no_of_messages = 0;
	char *msg_ptr = NULL;

	(void)arg;

	for(;;)
	{
		int  done, len;
		t = sizeof(remote);

		if ((acc_sock_id = accept(mon_sock_id, (struct sockaddr *)&remote, &t)) == -1) 
		{
			syslog (LOG_ERR, "%s",strerror(errno));
			conn_accepted = 0;
			return NULL;
		}

		done = 0;

		do
		{
			len = recv(acc_sock_id, msg, PW_MAX_MSG_LEN, 0);

			if (len <= 0) 
			{
				if (len < 0) 
				{
					syslog (LOG_ERR, "%s",strerror(errno));
				}
				done = 1;
			}
			else if(len == 0)
			{
				done = 1;
			}

			msg_ptr = msg;

			/* figure out how many message has to be processed 
			 * and process them at once, we are processing messages every 
			 * 5 seconds this makes it possible to recive more than 5 messages
			 * per recv call */

			no_of_messages = len/sizeof(keepalive_t);

			while(no_of_messages)
			{
				keepalive_t *pkt = (struct keepalive_t *) msg_ptr;
				msg_type = pkt->msg_type;
				/* Send syslog message, if this is not a regular keep alive*/
				if(msg_type != 0)
				{
					syslog(LOG_NOTICE, "Received Keep-Alive Message with message type set msg_type :" 
						   "%d frequency : %d, threshold : %d action : %d syslog ip : %d", (pkt->msg_type),
						   pkt->frequency, pkt->threshold, pkt->action, pkt->syslog_ip);
				}

				switch(msg_type)
				{
					case PW_BOOT_UP_SUCCESS:
						syslog(LOG_NOTICE, "Received Boot up success from Process");
						ProcessWatchConfigUpdate(pkt);
						break;
					case PW_KEEP_ALIVE:
						gProcessWatchStatus = 1;
						break;
					case PW_KEEP_ALIVE_MODIFIED: 
						gProcessWatchStatus = 1;
						syslog(LOG_NOTICE, "Received regular Keep Alive (M)");
						ProcessWatchConfigUpdate(pkt);
						break;
					case PW_DISABLE_KEEP_ALIVE:
						syslog(LOG_NOTICE, "Received Disable keep alive from Process");
						gProcessWatchStatus = 0;
						ProcessWatchConfigUpdate(pkt);
						break;
				}

				no_of_messages --;
				msg_ptr += sizeof(keepalive_t);
				counter++;
			};/* While ends */

			/* I will process keep alives only every 5 seconds 
			 * and read them as a bunch*/
			sleep(5);  

		}while(!done);

		close(acc_sock_id);
	}

	return NULL;
}


void SystemRestart (void)
{
	syslog (LOG_ERR, "Process Down! May Day! May Day! Reboot Requested by configuration. Rebooting in 10 seconds! ");
	sleep(10);
	system("reboot");
}


void startThresholdTimer()
{
	int index = 0;
	
	thresholTmr.it_interval.tv_sec  = 0;
	thresholTmr.it_interval.tv_usec = 0;
	thresholTmr.it_value.tv_sec  = gProcessWatchConfig.threshold;
	thresholTmr.it_value.tv_usec = 0;

	if ((index = setitimer (ITIMER_REAL, &thresholTmr, NULL)) != 0)
	{
		syslog (LOG_ERR, "Timer creation failed, exit!");
		exit(1);
	}

}

void ThresholdTimerHandler ()
{
	/* If counter is 0 then we have not received any messages in this threshold
	 * interval based on the configuration perform the operation */
	//printf("%s %d %d %s\n", __func__, counter, gProcessWatchConfig.action, getTimeString());

	if(counter == 0 && gProcessWatchStatus)
	{
		switch (gProcessWatchConfig.action)
		{
			case PW_ACTION_NONE:
				syslog (LOG_ERR, "threshold reached at %s , Process is Down!", 
						getTimeString());
				break;
			case PW_ACTION_RESTART:
				syslog (LOG_ERR, " threshold reached at %s, Process is Down! Requested action is Restart!", 
						getTimeString());
				sleep(2);
				SystemRestart();
				break;
			case PW_ACTION_SYSLOG:
				syslog (LOG_ERR, "No keep-alives threshold reached at %s, Process is Down! ", 
						getTimeString());
				break;
			case PW_ACTION_BOTH:
				syslog (LOG_ERR, "No keep-alives threshold reached at %s, Process is Down! ", 
						getTimeString());
				break;
		}
	}
	
	/* restart the timer */
	startThresholdTimer();

	counter = 0;
}

int main(void)
{
	int rc;
	pthread_t timer_thread;
	int index;
	sigset_t sigset;
	int signum;

	/* Block all signals so the spawned threads don't receive any. */
	sigemptyset(&sigset);
	sigfillset(&sigset);
	pthread_sigmask(SIG_BLOCK, &sigset, NULL);

	ProcessWatchInit();

	ProcessWatchSockInit();

	rc = pthread_create(&timer_thread,
						(pthread_attr_t *)NULL,
						ProcessWatchProcessMsg,
						(void *)NULL);
	if (rc) 
	{
		syslog (LOG_ERR, "Unable to create packet processing thread in process-watcher, exit!");
		exit(1);
	}

	/* Install ThresholdTimerHandler as the signal handler for SIGVTALRM. */

	startThresholdTimer();

	sigfillset(&sigset);

	while (1)
	{
		/* waits in sigwait for signals, only way to exit process-watcher is to kill 
		 * it with signal 9 */
		sigwait(&sigset, &signum);

		switch (signum) 
		{
			case SIGALRM:
				{
					/* when it catches SIGALRM due to timer expiry 
					 * it calls ThresholdTimerHandler */
					ThresholdTimerHandler();
				}
				break;
			case SIGTERM:
				break;
			case SIGINT:
				break;
			default:
				break; 
		}
	}
}
