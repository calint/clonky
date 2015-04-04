#include "tmr.h"
#include<signal.h>
#include<string.h>
#include<sys/time.h>
void tmr(const int intervall_s,void f(int)){
	struct sigaction sa;
	memset(&sa,0,sizeof(sa));
	sa.sa_handler=f;
	sigaction(SIGALRM,&sa,NULL);
	struct itimerval timer;
	timer.it_value.tv_sec=intervall_s;
	timer.it_value.tv_usec=0;
	timer.it_interval.tv_sec=intervall_s;
	timer.it_interval.tv_usec=0;
	setitimer(ITIMER_REAL,&timer,NULL);
}
