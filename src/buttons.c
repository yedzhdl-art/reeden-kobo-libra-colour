#define _GNU_SOURCE
#include "buttons.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/input.h>

/* Kobo's grip keys are reported as KEY_F23 / KEY_F24 on the main NTX input
   node. KOReader calls them RPgBack/RPgFwd, but on the Libra Colour the
   physical keys are wired the other way round relative to Reeden's page-margin
   hit zones: pressing the "next" key must synthesise a tap on the opposite
   margin from the one KOReader's naming suggests. Verified on-device (390).
   Override with REEDEN_BTN_PREV / REEDEN_BTN_NEXT if a board differs.
   The generic navigation keys are kept as a fallback for other boards. */
static const unsigned short default_prev[]={
    KEY_F24,
    KEY_PAGEUP,KEY_LEFT,KEY_PREVIOUS,KEY_BACK,KEY_VOLUMEDOWN,BTN_LEFT,
    BTN_TRIGGER_HAPPY1,BTN_TRIGGER_HAPPY3,BTN_TRIGGER_HAPPY5,BTN_TRIGGER_HAPPY7
};
static const unsigned short default_next[]={
    KEY_F23,
    KEY_PAGEDOWN,KEY_RIGHT,KEY_NEXT,KEY_FORWARD,KEY_VOLUMEUP,BTN_RIGHT,
    BTN_TRIGGER_HAPPY2,BTN_TRIGGER_HAPPY4,BTN_TRIGGER_HAPPY6,BTN_TRIGGER_HAPPY8
};

#define WORDS(maxv) ((maxv + 8*sizeof(unsigned long) - 1)/(8*sizeof(unsigned long)))

static bool has_bit(const unsigned long *bits,size_t words,unsigned code){
    size_t i=code/(8*sizeof(unsigned long));
    if(i>=words)return false;
    return ((bits[i]>>(code%(8*sizeof(unsigned long)))&1UL)!=0);
}

static unsigned copy_codes(unsigned short *dst,unsigned cap,const unsigned short *src,unsigned n){
    if(n>cap)n=cap;memcpy(dst,src,n*sizeof(*src));return n;
}

static unsigned parse_codes(const char *s,unsigned short *dst,unsigned cap){
    unsigned n=0;
    while(*s&&n<cap){
        char *end=NULL;long v=strtol(s,&end,0);
        if(end==s){s++;continue;}
        if(v>0&&v<=KEY_MAX)dst[n++]=(unsigned short)v;
        s=end;while(*s==','||*s==' '||*s==';')s++;
    }
    return n;
}

static bool any_page_key(const unsigned long *kb,size_t words){
    for(unsigned i=0;i<sizeof(default_prev)/sizeof(*default_prev);i++)
        if(has_bit(kb,words,default_prev[i]))return true;
    for(unsigned i=0;i<sizeof(default_next)/sizeof(*default_next);i++)
        if(has_bit(kb,words,default_next[i]))return true;
    return false;
}

/* Never grab: Nickel is stopped while Reeden runs, and an exclusive grab on
   the wrong node would take the touchscreen or the accelerometer away. */
static bool candidate(int fd){
    unsigned long kb[WORDS(KEY_MAX)]={0},ab[WORDS(ABS_MAX)]={0};
    if(ioctl(fd,EVIOCGBIT(EV_KEY,sizeof(kb)),kb)<0)return false;
    ioctl(fd,EVIOCGBIT(EV_ABS,sizeof(ab)),ab);
    if(has_bit(ab,WORDS(ABS_MAX),ABS_MT_POSITION_X))return false;  /* touch */
    return any_page_key(kb,WORDS(KEY_MAX));
}

static void load_codes(Buttons *b){
    b->nprev=copy_codes(b->prev,24,default_prev,sizeof(default_prev)/sizeof(*default_prev));
    b->nnext=copy_codes(b->next,24,default_next,sizeof(default_next)/sizeof(*default_next));
    const char *p=getenv("REEDEN_BTN_PREV");if(p&&*p)b->nprev=parse_codes(p,b->prev,24);
    const char *n=getenv("REEDEN_BTN_NEXT");if(n&&*n)b->nnext=parse_codes(n,b->next,24);
}

static int open_forced(Buttons *b,const char *path){
    int fd=open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC);
    if(fd<0)return -1;
    if(!candidate(fd)){close(fd);return -1;}
    b->fd=fd;snprintf(b->path,sizeof(b->path),"%s",path);
    return 0;
}

static bool is_touch_path(const char *path){
    const char *t=getenv("REEDEN_TOUCH");
    if(t&&*t)return !strcmp(path,t);
    return !strcmp(path,"/dev/input/event1");
}

static int scan(Buttons *b){
    for(int i=0;i<32;i++){
        char path[64];snprintf(path,sizeof(path),"/dev/input/event%d",i);
        if(is_touch_path(path))continue;
        int fd=open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC);
        if(fd<0)continue;
        if(candidate(fd)){b->fd=fd;snprintf(b->path,sizeof(b->path),"%s",path);return 0;}
        close(fd);
    }
    return -1;
}

int buttons_open(Buttons *b){
    memset(b,0,sizeof(*b));b->fd=-1;
    const char *cfg=getenv("REEDEN_BUTTONS");
    if(cfg&&(!strcmp(cfg,"0")||!strcasecmp(cfg,"off"))){
        fprintf(stderr,"[buttons] disabled by REEDEN_BUTTONS\n");return -1;
    }
    b->debug=getenv("REEDEN_BTN_DEBUG")!=NULL;
    load_codes(b);
    int rc=(cfg&&!strncmp(cfg,"/dev/",5))?open_forced(b,cfg):scan(b);
    if(rc){fprintf(stderr,"[buttons] no page-turn key device; feature inactive\n");return -1;}
    fprintf(stderr,"[buttons] device=%s prev=%u next=%u (no grab)\n",b->path,b->nprev,b->nnext);
    return 0;
}

static const char *key_name(int code){
    switch(code){
        case KEY_POWER: return "KEY_POWER";
        case KEY_F23: return "KEY_F23(next)";
        case KEY_F24: return "KEY_F24(prev)";
        case KEY_PAGEUP: return "KEY_PAGEUP";
        case KEY_PAGEDOWN: return "KEY_PAGEDOWN";
        case KEY_LEFT: return "KEY_LEFT";
        case KEY_RIGHT: return "KEY_RIGHT";
        case KEY_HOME: return "KEY_HOME";
        case KEY_BACK: return "KEY_BACK";
        case KEY_F1: return "KEY_F1";
        case KEY_F2: return "KEY_F2";
        case KEY_H: return "KEY_H";
        default: return "unknown";
    }
}

int buttons_attach(Buttons *b,int fd,const char *path){
    if(!b)return -1;
    memset(b,0,sizeof(*b));b->fd=-1;
    const char *cfg=getenv("REEDEN_BUTTONS");
    if(cfg&&(!strcmp(cfg,"0")||!strcasecmp(cfg,"off"))){
        fprintf(stderr,"[buttons] disabled by REEDEN_BUTTONS\n");return -1;
    }
    if(fd<0)return -1;
    b->debug=getenv("REEDEN_BTN_DEBUG")!=NULL;
    load_codes(b);
    b->fd=-1;   /* the descriptor belongs to the host; never closed here */
    snprintf(b->path,sizeof(b->path),"%s",path?path:"shared");
    fprintf(stderr,"[buttons] attached to %s prev=%u next=%u (shared fd, %s)\n",
        b->path,b->nprev,b->nnext,(b->nprev||b->nnext)?"page keys listed":"no page keys");
    return 0;
}

static int classify(const Buttons *b,int code){
    for(unsigned i=0;i<b->nprev;i++)if(b->prev[i]==code)return -1;
    for(unsigned i=0;i<b->nnext;i++)if(b->next[i]==code)return 1;
    return 0;
}

void buttons_feed(Buttons *b,int type,int code,int value,ButtonTurnFn fn){
    if(!b||!fn||type!=EV_KEY)return;
    if(value!=1&&value!=0)return;
    if(value==1&&code!=KEY_POWER)
        fprintf(stderr,"[buttons] key code=%d name=%s\n",code,key_name(code));
    if(value!=1)return;   /* initial press only; ignore repeat and release */
    int dir=classify(b,code);
    if(dir){fprintf(stderr,"[buttons] turn=%s code=%d\n",dir>0?"next":"prev",code);fn(dir);}
}

void buttons_process(Buttons *b,ButtonTurnFn fn){
    if(!b||b->fd<0||!fn)return;
    struct input_event ev[16];ssize_t n;
    while((n=read(b->fd,ev,sizeof(ev)))>0)
        for(size_t i=0;i<(size_t)n/sizeof(*ev);i++)
            buttons_feed(b,ev[i].type,ev[i].code,ev[i].value,fn);
}

void buttons_close(Buttons *b){
    if(!b)return;
    if(b->fd>=0)close(b->fd);
    b->fd=-1;
}

int buttons_probe(void){
    fprintf(stderr,"[probe] input devices (buttons)\n");
    for(int i=0;i<32;i++){
        char path[64],name[128]={0};
        snprintf(path,sizeof(path),"/dev/input/event%d",i);
        int fd=open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC);
        if(fd<0)continue;
        if(ioctl(fd,EVIOCGNAME(sizeof(name)-1),name)<0)name[0]=0;
        unsigned long kb[WORDS(KEY_MAX)]={0},ab[WORDS(ABS_MAX)]={0},mb[WORDS(MSC_MAX)]={0};
        int k=ioctl(fd,EVIOCGBIT(EV_KEY,sizeof(kb)),kb);
        ioctl(fd,EVIOCGBIT(EV_ABS,sizeof(ab)),ab);
        int m=ioctl(fd,EVIOCGBIT(EV_MSC,sizeof(mb)),mb);
        size_t kw=WORDS(KEY_MAX),aw=WORDS(ABS_MAX);
        fprintf(stderr,
            "[probe] %-22s name=%-26s touch=%d accel=%d power=%d f23=%d f24=%d "
            "pageup=%d pagedown=%d happy1=%d happy2=%d msc_raw=%d\n",
            path,name,
            has_bit(ab,aw,ABS_MT_POSITION_X),
            has_bit(ab,aw,ABS_X)&&has_bit(ab,aw,ABS_Y)&&has_bit(ab,aw,ABS_Z),
            k>=0&&has_bit(kb,kw,KEY_POWER),
            k>=0&&has_bit(kb,kw,KEY_F23),k>=0&&has_bit(kb,kw,KEY_F24),
            k>=0&&has_bit(kb,kw,KEY_PAGEUP),k>=0&&has_bit(kb,kw,KEY_PAGEDOWN),
            k>=0&&has_bit(kb,kw,BTN_TRIGGER_HAPPY1),k>=0&&has_bit(kb,kw,BTN_TRIGGER_HAPPY2),
            m>=0&&has_bit(mb,WORDS(MSC_MAX),MSC_RAW));
        close(fd);
    }
    return 0;
}
