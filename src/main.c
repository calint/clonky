#define APP "clonky system overview"
//#include"tmr.h"
#include"dc.h"
#include"graph.h"
#include"graphd.h"
#include"main-cfg.h"
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<unistd.h>
#include<ctype.h>
#include<dirent.h>
#include<time.h>
#include<sys/stat.h>
#include<signal.h>
static struct dc*dc;
static struct graph*graphcpu;
static struct graph*graphmem;
static struct graphd*graphwifi;
#define bbuf_len 1024
static char bbuf[bbuf_len];
#define line_len 256
static char line[line_len];
#define sys_cls_pwr_bat_len 16
static char sys_cls_pwr_bat[sys_cls_pwr_bat_len];
#define sys_cls_net_wlan_len 16
static char sys_cls_net_wlan[sys_cls_net_wlan_len];
static void sysvaluestr(const char*path,char*value,int size){
	FILE*file=fopen(path,"r");
	if(!file){
		*value=0;
		return;
	}
	char fmt[16];
	snprintf(fmt,16,"%%%ds\\n",size);
	fscanf(file,fmt,value);
	char*p=value;
	while(*p){
		*p=tolower(*p);
		p++;
	}
	fclose(file);
}
static int sysvalueint(const char*path){
	FILE*file=fopen(path,"r");
	if(!file)return 0;
	int num;
	fscanf(file,"%d",&num);
	fclose(file);
	return num;
}
static long long sysvaluelng(const char*path){
	FILE*file=fopen(path,"r");
	if(!file)return 0;
	long long num;
	fscanf(file,"%llu\n",&num);
	fclose(file);
	return num;
}
static void strcompactspaces(char *s){
	char*d=s;
	int last_char_was_space=0;
	while(*s){
		*d=*s;
		d++;
		if(*s==' ')
			last_char_was_space=1;
		else
			last_char_was_space=0;
		s++;
		if(last_char_was_space){
			while(*s==' ')
				s++;
			last_char_was_space=0;
		}
	}
	*d=0;
}
static void qdir(const char*path,void f(const char*)){
	DIR*dir=opendir(path);
	if(dir==NULL)
		return;
	struct dirent *dirent;
	while((dirent=readdir(dir))!=NULL){
		f(dirent->d_name);
	}
	closedir(dir);
}
static void netdir(const char*filename){
	if(!filename)
		return;
	if(*filename=='.')
		return;
	char path[256];

	*path=0;
	strcat(path,"/sys/class/net/");
	strcat(path,filename);
	strcat(path,"/operstate");
	char operstate[64];
	sysvaluestr(path,operstate,64);

	*path=0;
	strcat(path,"/sys/class/net/");
	strcat(path,filename);
	strcat(path,"/statistics/rx_bytes");
	long long rx=sysvaluelng(path);

	*path=0;
	strcat(path,"/sys/class/net/");
	strcat(path,filename);
	strcat(path,"/statistics/tx_bytes");
	long long tx=sysvaluelng(path);

	if(!strcmp(operstate,"up")){
		snprintf(bbuf,bbuf_len,"%s %s %lld/%lld KB",filename,operstate,tx>>10,rx>>10);
	}else if(!strcmp(operstate,"dormant")){
		snprintf(bbuf,bbuf_len,"%s %s",filename,operstate);
	}else if(!strcmp(operstate,"down")){
		snprintf(bbuf,bbuf_len,"%s %s",filename,operstate);
	}else if(!strcmp(operstate,"unknown")){
		snprintf(bbuf,bbuf_len,"%s %s %lld/%lld KB",filename,operstate,tx>>10,rx>>10);
	}
	dccr(dc);
	dcdrwstr(dc,bbuf);
}
static void _rendlid(){
	FILE*file=fopen("/proc/acpi/button/lid/LID/state","r");
	if(file){
		fgets(line,sizeof(line),file);
		strcompactspaces(line);
		snprintf(bbuf,bbuf_len,"lid %s",line);
		dccr(dc);
		dcdrwstr(dc,bbuf);
		fclose(file);
	}
}
//static void _rendtherm2(){
//	file=fopen("/proc/acpi/thermal_zone/THM/temperature","r");
//	if(file){
//		fgets(line,sizeof(line),file);
//		strcompactspaces(line);
//		sprintf(bbuf,"%s",line);
//		dccr(dc);
//		dcdrwstr(dc,bbuf,strlen(bbuf)-1);
//		fclose(file);
//	}
//}
static void _rendhr(){
	dcdrwhr(dc);
}
const char sys_cls_pwr[]="/sys/class/power_supply/";
static void _rendbattery(){
	char path[256]="";
	char*p=strcat(strcat(strcat(path,sys_cls_pwr),sys_cls_pwr_bat),"/");
	const int pl=strlen(p);

	strcpy(p+pl,"charge_full_design");
	int charge_full_design=sysvalueint(p);
	strcpy(p+pl,"charge_now");
	int charge_now=sysvalueint(p);
	strcpy(p+pl,"status");
	char state[32];
	sysvaluestr(path,state,sizeof(state));

//	dcyinc(dc,default_graph_height);
//	graphaddvalue(graphbat,charge_now);
//	graphdraw2(graphbat,dc,default_graph_height,charge_full_design);
	dccr(dc);
	snprintf(bbuf,bbuf_len,"battery %s  %d/%d mAh",state,charge_now/1000,charge_full_design/1000);
	dcdrwstr(dc,bbuf);
	if(charge_full_design)
		dcdrwhr1(dc,width*charge_now/charge_full_design);
}
static void _rendcpuload(){
	static int cpu_total_last=0;
	static int cpu_usage_last=0;

	FILE*file=fopen("/proc/stat","r");
	if(!file)return;
	// user: normal processes executing in user mode
	// nice: niced processes executing in user mode
	// system: processes executing in kernel mode
	// idle: twiddling thumbs
	// iowait: waiting for I/O to complete
	// irq: servicing interrupts
	// softirq: servicing softirqs
	int user,nice,system,idle,iowait,irq,softirq;
	fscanf(file,"%1024s %d %d %d %d %d %d %d\n",bbuf,&user,&nice,&system,&idle,&iowait,&irq,&softirq);
	fclose(file);
	int total=(user+nice+system+idle+iowait+irq+softirq);
	int usage=total-idle;
	long long dtotal=total-cpu_total_last;
	cpu_total_last=total;
	int dusage=usage-cpu_usage_last;
	cpu_usage_last=usage;
	int usagepercent=dusage*100/dtotal;
	graphaddvalue(graphcpu,usagepercent);
	dcyinc(dc,dyhr);
	dcyinc(dc,default_graph_height);
	graphdraw2(graphcpu,dc,default_graph_height,100);
}
static void _rendhelloclonky(){
	static long long unsigned counter;
	counter++;
	snprintf(bbuf,bbuf_len,"%llu hello%sclonky",counter,counter!=1?"s ":" ");
	dccr(dc);
	dcdrwstr(dc,bbuf);
}
static void _rendmeminfo(){
	FILE*file=fopen("/proc/meminfo","r");
	if(!file)return;
	char name[64],unit[32];
	long long memtotal,memavail;
	fgets(bbuf,bbuf_len,file);//	MemTotal:        1937372 kB
	sscanf(bbuf,"%64s %lld %32s",name,&memtotal,unit);
	fgets(bbuf,bbuf_len,file);//	MemFree:           99120 kB
	fgets(bbuf,bbuf_len,file);//	MemAvailable:     887512 kB
	fclose(file);
	sscanf(bbuf,"%64s %llu %32s",name,&memavail,unit);
	int proc=(memtotal-memavail)*100/memtotal;
	graphaddvalue(graphmem,proc);
	dcyinc(dc,dyhr);
	dcyinc(dc,default_graph_height);
	graphdraw(graphmem,dc,2);
	if(memavail>>10!=0){
		memavail>>=10;
		memtotal>>=10;
		strcpy(unit,"MB");
	}
	snprintf(bbuf,bbuf_len,"freemem %llu of %llu %s",memavail,memtotal,unit);
	dccr(dc);
	dcdrwstr(dc,bbuf);
}
static void _rendnet(){
	qdir("/sys/class/net",netdir);
}
static void _rendwifitraffic(){
	dcyinc(dc,default_graph_height+dyhr);
	snprintf(bbuf,bbuf_len,"/sys/class/net/%s/statistics/tx_bytes",sys_cls_net_wlan);
	long long wifi_tx=sysvaluelng(bbuf);
	snprintf(bbuf,bbuf_len,"/sys/class/net/%s/statistics/rx_bytes",sys_cls_net_wlan);
	long long wifi_rx=sysvaluelng(bbuf);
	graphdaddvalue(graphwifi,wifi_tx+wifi_rx);
	graphddraw(graphwifi,dc,default_graph_height,wifigraphmax);
}
static void pl(const char*str){
	dccr(dc);
	dcdrwstr(dc,str);
}
static void _rendcheetsheet(){
static char*keysheet[]={
	"ĸey",
	"+c               console",
	"+f                 files",
	"+e                editor",
	"+m                 media",
	"+v                 mixer",
	"+i              internet",
	"+x                sticky",
	"+q              binaries",
	"+prtsc          snapshot",
	"",
	"đesktop",
	"+up                   up",
	"+down               down",
	"+shift+up        move-up",
	"+shift+down    move-down",
	"",
	"window",
	"+esc               close",
	"+b                  bump",
	"+s                center",
	"+w                 wider",
	"+W               thinner",
	"+r                resize",
	"+3            fullscreen",
	"+4           full height",
	"+5            full width",
	"+6   i-am-bored-surprise",
	"...                  ...",
	NULL};

	char**strptr=keysheet;
	while(*strptr){
		dccr(dc);
		dcdrwstr(dc,*strptr);
		strptr++;
	}
}
static void _renddf(){
	FILE*f=popen("df -h 2>/dev/null","r");
	if(!f)return;
	while(1){
		if(!fgets(bbuf,bbuf_len,f))
			break;
		strcompactspaces(bbuf);
//		if(!strncmp(buf,"none ",5))
//			continue;
		if(bbuf[0]!='/')
			continue;
		pl(bbuf);
	}
	pclose(f);
}
static void _rendiostat(){
	static long long unsigned int last_kb_read=0,last_kb_wrtn=0;

	FILE*f=popen("iostat -d","r");
	if(!f)return;
	//	Linux 3.11.0-14-generic (vaio) 	03/12/2014 	_x86_64_	(2 CPU)
	//
	//	Device:            tps    kB_read/s    kB_wrtn/s    kB_read    kB_wrtn
	//	sda               7.89        25.40        80.46     914108    2896281
	fgets(bbuf,bbuf_len,f);
	fgets(bbuf,bbuf_len,f);
	fgets(bbuf,bbuf_len,f);
	float tps,kb_read_s,kb_wrtn_s;
	long long int kb_read,kb_wrtn;
	char dev[64];
	fscanf(f,"%64s %f %f %f %lld %lld",dev,&tps,&kb_read_s,&kb_wrtn_s,&kb_read,&kb_wrtn);
	pclose(f);
	const char*unit="kB";
	snprintf(bbuf,bbuf_len,"read %lld %s wrote %lld %s",kb_read-last_kb_read,unit,kb_wrtn-last_kb_wrtn,unit);
	pl(bbuf);
	last_kb_read=kb_read;
	last_kb_wrtn=kb_wrtn;
}
static void _renddmsg(){
	FILE*f=popen("dmesg -t|tail -n10","r");
//	FILE*f=popen("tail -n10 /var/log/syslog","r");
	if(!f)return;
	while(1){
		if(!fgets(bbuf,bbuf_len,f))
			break;
		pl(bbuf);
	}
	pclose(f);
}
static void _rendsyslog(){
	FILE*f=popen("tail /var/log/syslog","r");
	if(!f)return;
	while(1){
		if(!fgets(bbuf,bbuf_len,f))
			break;
		pl(bbuf);
	}
	pclose(f);
}
static void _renddatetime(){
	const time_t t=time(NULL);
	const struct tm *local=localtime(&t);
	snprintf(bbuf,bbuf_len,"%s",asctime(local));
	dccr(dc);
	dcdrwstr(dc,bbuf);
}
static void _rendcputhrottles(){
	FILE*f=fopen("/sys/devices/system/cpu/present","r");
	if(!f)return;
	int min,max;
	fscanf(f,"%d-%d",&min,&max);
	fclose(f);
//	printf(" %d  %d\n",min,max);
	const int bbl=1024;
	char bb[bbl];
	strcpy(bb,"throttle ");
	int n;
	for(n=min;n<=max;n++){
		snprintf(bbuf,bbuf_len,"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq",n);
		int max_freq=sysvalueint(bbuf);
		snprintf(bbuf,bbuf_len,"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq",n);
		int cur_freq=sysvalueint(bbuf);
		snprintf(bbuf,bbuf_len," %d%%",(cur_freq*100)/max_freq);
		strcat(bb,bbuf);//? bufferoverrun
	}
	pl(bb);
}
static void fmtbytes(long long bytes,char*buf,int buflen){
	const long long kb=bytes>>10;
	if(kb==0){
		snprintf(buf,buflen,"%lld B",bytes);
		return;
	}
	const long long mb=kb>>10;
	if(mb==0){
		snprintf(buf,buflen,"%lld KB",kb);
		return;
	}
	snprintf(buf,buflen,"%lld MB",mb);
	return;
}
static void _rendswaps(){
	FILE*f=fopen("/proc/swaps","r");
	if(!f)return;
//	Filename				Type		Size	Used	Priority
//	/dev/mmcblk0p3                          partition	2096124	16568	-1
	fgets(bbuf,bbuf_len,f);
	char dev[64],type[32];
	long long size,used;
	if(!fscanf(f,"%64s %32s %lld %lld",dev,type,&size,&used))return;
	fclose(f);
	const int bblen=64;
	char bb[bblen];
	fmtbytes(used<<10,bb,bblen);
	snprintf(bbuf,bbuf_len,"swapped %s",bb);
	pl(bbuf);
}
static int strstartswith(const char*string,const char*prefix){
	while(*prefix)if(*prefix++!=*string++)return 0;
	return 1;
}
static void autoconfig_bat(){
	DIR*dp=opendir("/sys/class/power_supply");
	if(!dp){
		puts("[!] battery: cannot open find dir /sys/class/power_supply");
		return;
	}
	struct dirent*ep;
	*sys_cls_net_wlan=0;
	while((ep=readdir(dp))){
		if(ep->d_name[0]=='.')continue;
		char cb[512];
		snprintf(cb,sizeof(cb),"/sys/class/power_supply/%s/type",ep->d_name);
		//puts(cb);
		sysvaluestr(cb,cb,sizeof(cb));
		//puts(cb);
                if(strcmp(cb,"battery"))continue;
		//if(!strstartswith(ep->d_name,"BAT"))continue;
		strncpy(sys_cls_pwr_bat,ep->d_name,sys_cls_pwr_bat_len);
		break;
	}
	(void)closedir(dp);
	if(!*sys_cls_pwr_bat){
		puts("[!] no battery found in /sys/class/power_supply");
	}
	printf("· graphs battery: ");
	puts(sys_cls_pwr_bat);
	return;
}
static int is_wlan_device(const char*sys_cls_net_wlan){
	snprintf(bbuf,bbuf_len,"/sys/class/net/%s/wireless",sys_cls_net_wlan);
	struct stat s;
	if(stat(bbuf,&s))return 0;
	return 1;
}
static void autoconfig_wifi(){
	DIR*dp=opendir("/sys/class/net");
	if(!dp){
		puts("[!] wifi: cannot open find dir /sys/class/net");
		return;
	}
	struct dirent*ep;
	*sys_cls_net_wlan=0;
	while((ep=readdir(dp))){
		if(ep->d_name[0]=='.')continue;
		if(!is_wlan_device(ep->d_name))continue;
		strncpy(sys_cls_net_wlan,ep->d_name,sys_cls_net_wlan_len);
		break;
	}
	(void)closedir(dp);
	if(!*sys_cls_net_wlan){
		puts("[!] no wireless device found in /sys/class/net");
	}
	printf("· graphs network device: ");
	puts(sys_cls_net_wlan);
	return;
}
static void autoconfig(){
	autoconfig_bat();
	autoconfig_wifi();
}
static void on_draw(){
	dcyset(dc,ytop);
	dcclrbw(dc);
	_renddatetime();
	_rendcpuload();
	_rendhelloclonky();
	_rendmeminfo();
	_rendswaps();
	_rendwifitraffic();
	_rendnet();
	_rendhr();
	_rendiostat();
	_renddf();
	_rendhr();
	_rendcputhrottles();
	_rendbattery();
	_rendlid();
	_rendhr();
	_renddmsg();
//	_rendsyslog();
	_rendhr();
	_rendhr();
	_rendcheetsheet();
	_rendhr();
	_rendhr();
	dcflush(dc);
}
#define exit_clean 1
static void sigexit(int i){
	puts("exiting");
	dcdel(dc);
	signal(SIGINT,SIG_DFL);
	kill(getpid(),SIGINT);
	exit(i);
}
int main(){
	signal(SIGINT,sigexit);
	puts(APP);
	if(!(dc=dcnew()))exit(1);
	dcwset(dc,width);
	if(align==1)
		dcxlftset(dc,dcwscrget(dc)-width);
	graphcpu=graphnew(width);
	graphmem=graphnew(width);
	graphwifi=graphdnew(width);
	autoconfig();
	//tmr(1,&on_draw);
	while(1){
		sleep(1);
		on_draw();
	}
}
