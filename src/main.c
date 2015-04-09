#define APP "clonky system overview"
#include"tmr.h"
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
static struct dc*dc;
static struct graph*graphcpu;
//static struct graph*graphbat;
static struct graph*graphmem;
static struct graphd*graphwifi;
static struct graph*graphhd;
static unsigned int counter=0;
static unsigned int cpu_total_last=0;
static unsigned int cpu_usage_last=0;
static char bbuf[1024];
static char line[256];
static char sys_cls_pwr_bat[16];
static char sys_cls_net_wlan[16];
static void sysvaluestr(char*path,char*value,int size){
	FILE*file=fopen(path,"r");
	if(!file){
		*value=0;
		return;
	}
	char fmt[16];
	sprintf(fmt,"%%%ds\\n",size);
	fscanf(file,fmt,value);
	char*p=value;
	while(*p){
		*p=tolower(*p);
		p++;
	}
	fclose(file);
}
static void sysvalueint(char*path,int*num){
	FILE*file=fopen(path,"r");
	if(!file){
		*num=0;
		return;
	}
	fscanf(file,"%d\n",num);
	fclose(file);
}
static void sysvaluelng(char*path,long long*num){
	FILE*file=fopen(path,"r");
	if(!file){
		*num=0;
		return;
	}
	fscanf(file,"%llu\n",num);
	fclose(file);
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
static void qdir(char*path,void f(char*)){
	DIR*dir=opendir(path);
	if(dir==NULL)
		return;
	struct dirent *dirent;
	while((dirent=readdir(dir))!=NULL){
		f(dirent->d_name);
	}
	closedir(dir);
}
static void netdir(char*filename){
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
	long long rx;
	sysvaluelng(path,&rx);

	*path=0;
	strcat(path,"/sys/class/net/");
	strcat(path,filename);
	strcat(path,"/statistics/tx_bytes");
	long long tx;
	sysvaluelng(path,&tx);

	if(!strcmp(operstate,"up")){
		sprintf(bbuf,"%s %s %lld/%lld KB",filename,operstate,tx>>10,rx>>10);
	}else if(!strcmp(operstate,"dormant")){
		sprintf(bbuf,"%s %s",filename,operstate);
	}else if(!strcmp(operstate,"down")){
		sprintf(bbuf,"%s %s",filename,operstate);
	}else if(!strcmp(operstate,"unknown")){
		sprintf(bbuf,"%s %s %lld/%lld KB",filename,operstate,tx>>10,rx>>10);
	}
	dccr(dc);
	dcdrwstr(dc,bbuf,strlen(bbuf));
}
static void _rendlid(){
	FILE*file=fopen("/proc/acpi/button/lid/LID/state","r");
	if(file){
		fgets(line,sizeof(line),file);
		strcompactspaces(line);
		sprintf(bbuf,"lid %s",line);
		dccr(dc);
		dcdrwstr(dc,bbuf,strlen(bbuf)-1);
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
	int charge_full_design;
	sysvalueint(p,&charge_full_design);

	strcpy(p+pl,"charge_now");
	int charge_now;
	sysvalueint(p,&charge_now);

	strcpy(p+pl,"status");
	char state[32];
	sysvaluestr(path,state,sizeof(state));

//	dcyinc(dc,default_graph_height);
//	graphaddvalue(graphbat,charge_now);
//	graphdraw2(graphbat,dc,default_graph_height,charge_full_design);
	dccr(dc);
	sprintf(bbuf,"battery %s  %d/%d mAh",state,charge_now/1000,charge_full_design/1000);
	dcdrwstr(dc,bbuf,strlen(bbuf));
	if(charge_full_design)
		dcdrwhr1(dc,width*charge_now/charge_full_design);
}
static void _rendcpuload(){
	FILE*file=fopen("/proc/stat","r");
	if(file){
		// user: normal processes executing in user mode
		// nice: niced processes executing in user mode
		// system: processes executing in kernel mode
		// idle: twiddling thumbs
		// iowait: waiting for I/O to complete
		// irq: servicing interrupts
		// softirq: servicing softirqs
		int user,nice,system,idle,iowait,irq,softirq;
		fscanf(file,"%s %d %d %d %d %d %d %d\n",bbuf,&user,&nice,&system,&idle,&iowait,&irq,&softirq);
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
		fclose(file);
	}
}
static void _rendhelloclonky(){
	sprintf(bbuf,"%d hello%sclonky",counter,counter!=1?"s ":" ");
	dccr(dc);
	dcdrwstr(dc,bbuf,strlen(bbuf));
}
static void _rendmeminfo(){
	FILE*file=fopen("/proc/meminfo","r");
	if(!file)return;
	char name[64],unit[32];
	long long memtotal,memavail;
	fgets(bbuf,sizeof(bbuf),file);//	MemTotal:        1937372 kB
	sscanf(bbuf,"%s %llu %s",name,&memtotal,unit);
	fgets(bbuf,sizeof(bbuf),file);//	MemFree:           99120 kB
	fgets(bbuf,sizeof(bbuf),file);//	MemAvailable:     887512 kB
	fclose(file);
	sscanf(bbuf,"%s %llu %s",name,&memavail,unit);
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
	sprintf(bbuf,"freemem %llu of %llu %s",memavail,memtotal,unit);
	dccr(dc);
	dcdrwstr(dc,bbuf,strlen(bbuf));
}
static void _rendnet(){
	qdir("/sys/class/net",netdir);
}
static void _rendwifitraffic(){
	dcyinc(dc,default_graph_height+dyhr);
	sprintf(bbuf,"/sys/class/net/%s/statistics/tx_bytes",sys_cls_net_wlan);
	long long wifi_tx;
	sysvaluelng(bbuf,&wifi_tx);
	sprintf(bbuf,"/sys/class/net/%s/statistics/rx_bytes",sys_cls_net_wlan);
	long long wifi_rx;
	sysvaluelng(bbuf,&wifi_rx);
	graphdaddvalue(graphwifi,wifi_tx+wifi_rx);
	graphddraw(graphwifi,dc,default_graph_height,wifigraphmax);
}
static void pl(const char*str){
	dccr(dc);
	dcdrwstr(dc,str,strlen(str));
}
static void _rendcheetsheet(){
	char**strptr=keysheet;
	while(*strptr){
		dccr(dc);
		dcdrwstr(dc,*strptr,strlen(*strptr));
		strptr++;
	}
}
static void _renddf(){
	char buf[1024];
	int buflen=1024;
	int r=system("df -h>.clonky.sh5");
	if(r)
		return;
	FILE*f=fopen(".clonky.sh5","r");
	while(1){
		if(!fgets(buf,buflen,f))
			break;
		strcompactspaces(buf);
//		if(!strncmp(buf,"none ",5))
//			continue;
		if(buf[0]!='/')
			continue;
		pl(buf);
	}
	fclose(f);
}
static void _rendiostat(){
	static long long unsigned int last_kb_read=0,last_kb_wrtn=0;
	char buf[1024];
	const int r=system("iostat -d>.clonky.sh8");
	if(r)return;
	FILE*f=fopen(".clonky.sh8","r");
	//	Linux 3.11.0-14-generic (vaio) 	03/12/2014 	_x86_64_	(2 CPU)
	//
	//	Device:            tps    kB_read/s    kB_wrtn/s    kB_read    kB_wrtn
	//	sda               7.89        25.40        80.46     914108    2896281
	fgets(buf,sizeof buf,f);
	fgets(buf,sizeof buf,f);
	fgets(buf,sizeof buf,f);
	char dev[64];
	float tps,kb_read_s,kb_wrtn_s;
	long long unsigned int kb_read,kb_wrtn;
	fscanf(f,"%s %f %f %f %llu %llu",dev,&tps,&kb_read_s,&kb_wrtn_s,&kb_read,&kb_wrtn);
	fclose(f);
	const char*unit="kB";
	sprintf(buf,"read %llu %s wrote %llu %s",kb_read-last_kb_read,unit,kb_wrtn-last_kb_wrtn,unit);
	pl(buf);
	last_kb_read=kb_read;
	last_kb_wrtn=kb_wrtn;
}
static void _renddmsg(){
	char buf[1024];
	int buflen=1024;
	int r=system("dmesg|tail -n 10 >.clonky.sh6");
	if(r)
		return;
	FILE*f=fopen(".clonky.sh6","r");
	while(1){
		if(!fgets(buf,buflen,f))
			break;
		pl(buf);
	}
	fclose(f);
}
static void _renddatetime(){
	const time_t t=time(NULL);
	const struct tm *local=localtime(&t);
	sprintf(bbuf,"%s",asctime(local));
	dccr(dc);
	dcdrwstr(dc,bbuf,strlen(bbuf));
}
static void _rendtherm(){
	char buf[1024];
	int buflen=1024;
	pl("therm");
	system("echo>.clonky.sh7&&for f in `ls /sys/class/thermal/`;do cat /sys/class/thermal/$f/* >.clonky.sh7 2>/dev/null;done");
	FILE*f=fopen(".clonky.sh7","r");
	while(1){
		if(!fgets(buf,buflen,f))
			break;
		strcompactspaces(buf);
		pl(buf);
	}
	fclose(f);
}
static void _rendcputhrottles(){
	char buf[1024];
	char buf2[1024];
	int min=0;
	int max=0;
	int n;
//	pl("throttles");
	FILE*f=fopen("/sys/devices/system/cpu/present","r");
	if(!f)return;
	fscanf(f,"%d-%d",&min,&max);
	fclose(f);
//	printf(" %d  %d\n",min,max);
	strcpy(buf2,"throttles ");
	for(n=min;n<=max;n++){
//		printf("  %d\n",n);
		int cur_freq,max_freq;
		sprintf(buf,"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq",n);
		sysvalueint(buf,&max_freq);
		sprintf(buf,"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq",n);
		sysvalueint(buf,&cur_freq);
		sprintf(buf," %d%% ",(cur_freq*100)/max_freq);
//		pl(buf);
		strcat(buf2,buf);
//		puts(buf2);
	}
	pl(buf2);
}
static void on_draw(){
	counter++;
	dcyset(dc,ytop);
	dcclrbw(dc);
	_renddatetime();
	_rendhr();
	_rendbattery();
	_rendcputhrottles();
	_rendcpuload();
	_rendhelloclonky();
	_rendmeminfo();
	_rendwifitraffic();
	_rendnet();
	_rendhr();
	_renddf();
	_rendhr();
	_rendiostat();
	_rendhr();
	_rendlid();
	_rendhr();
	_renddmsg();
	_rendhr();
	_rendhr();
	_rendcheetsheet();
	_rendhr();
	_rendtherm();
	_rendhr();
	dcflush(dc);
}
static int sh(const char*sh){
	printf("[ĸ] ");
	puts(sh);
	return system(sh);
}
static void autoconfig_bat(){
	sh("ls /sys/class/power_supply>.clonky.sh2");
	FILE*f=fopen(".clonky.sh2","r");
	int done=0;
	while(fgets((char*)sys_cls_pwr_bat,32,f)){
		if(!strncmp("BAT",sys_cls_pwr_bat,3)){
			sys_cls_pwr_bat[strlen(sys_cls_pwr_bat)-1]=0;
			done=1;
			break;
		}
	}
	fclose(f);
	if(done){
		printf("· graphs battery: %s\n",sys_cls_pwr_bat);
	}else{
		puts("[!] no battery");
		*sys_cls_pwr_bat=0;
	}
}
static int strendswith(const char*str,const char*compare){
	const int len1=strlen(str);
	const int len2=strlen(compare);
	const int diff=len1-len2;
	if(diff<0)return 0;
	const int eq=strcmp(str+diff,compare);
	return eq==0?1:0;
}
static int is_wlan_device(const char*sys_cls_net_wlan){
	char buf[256];
	sprintf(buf,"/sbin/iwconfig %s 2>.clonky.sh1",sys_cls_net_wlan);//? s len
	sh(buf);
	*buf=0;
	FILE*f=fopen(".clonky.sh1","r");
	if(!f)puts("cannot open file");
	fgets(buf,256,f);
	fclose(f);
	if(strendswith(buf,"no wireless extensions.\n"))
		return 0;
	//? No such device
	return 1;
}
static void autoconfig_wifi(){
	sh("ls /sys/class/net>.clonky.sh3");
	FILE*f=fopen(".clonky.sh3","r");
	int done=0;
	while(fgets((char*)sys_cls_net_wlan,32,f)){
		sys_cls_net_wlan[strlen(sys_cls_net_wlan)-1]=0;
		if(!is_wlan_device(sys_cls_net_wlan)){
			continue;
		}
		done=1;
		break;
	}
	fclose(f);
	if(done){
		printf("· graphs network device: ");
		puts(sys_cls_net_wlan);
	}else{
		puts("[!] no wireless device found using /sys/class/net and iwconfig");
		*sys_cls_pwr_bat=0;
	}
}
static void autoconfig(){
	autoconfig_bat();
	autoconfig_wifi();
}
int main(){
	puts(APP);
	if(!(dc=dcnew()))return 1;
	dcwset(dc,width);
	if(align==1)
		dcxlftset(dc,dcwscrget(dc)-width);
	graphcpu=graphnew(width);
//	graphbat=graphnew(width);
	graphmem=graphnew(width);
	graphwifi=graphdnew(width);
	graphhd=graphnew(width);
	autoconfig();
	tmr(1,&on_draw);
	while(1)
		sleep(60*60*24);
}
