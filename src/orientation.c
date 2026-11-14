#define _GNU_SOURCE
#include "orientation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/input.h>

/* No kernel framebuffer writes here. The 180-degree flip is applied in
   software to the presented pixels and to pointer coordinates, so the
   picture and the taps always stay aligned and no persistent device state
   can be left behind after a crash or a forced reboot. */

/* Kobo's gyro driver (kx122 / mma8x5x) does not publish a usable ABS stream to
   the app nodes. It synthesises EV_MSC/MSC_RAW orientation codes on the main
   NTX input node instead -- the same mechanism KOReader relies on. */
#define GSENSOR_PORTRAIT_DOWN   0x17
#define GSENSOR_PORTRAIT_UP     0x18
#define GSENSOR_LANDSCAPE_RIGHT 0x19
#define GSENSOR_LANDSCAPE_LEFT  0x1a
#define GSENSOR_BACK            0x1b
#define GSENSOR_FRONT           0x1c

static uint64_t now_ns(void){
    struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);
    return (uint64_t)ts.tv_sec*1000000000ULL+(uint64_t)ts.tv_nsec;
}

void orientation_init(Orientation *o){
    memset(o,0,sizeof(*o));
    const char *cfg=getenv("REEDEN_ROTATE_AUTO");
    o->enabled=!(cfg&&(!strcmp(cfg,"0")||!strcasecmp(cfg,"off")));
    o->flip=getenv("REEDEN_ROTATE_FLIP")!=NULL;
    o->debug=getenv("REEDEN_ACCEL_DEBUG")!=NULL;
    o->hold_ns=1500000000ULL;
    o->msc_debounce_ns=400000000ULL;
    const char *hold=getenv("REEDEN_ROTATE_HOLD_MS");
    if(hold){long ms=strtol(hold,NULL,10);if(ms>=200&&ms<=15000)o->hold_ns=(uint64_t)ms*1000000ULL;}
    const char *deb=getenv("REEDEN_ROTATE_DEBOUNCE_MS");
    if(deb){long ms=strtol(deb,NULL,10);if(ms>=0&&ms<=3000)o->msc_debounce_ns=(uint64_t)ms*1000000ULL;}
    o->msc_rotation=0;
    fprintf(stderr,"[orient] auto=%d hold_ms=%llu debounce_ms=%llu flip=%d (userspace rotation only)\n",
        o->enabled,(unsigned long long)(o->hold_ns/1000000ULL),
        (unsigned long long)(o->msc_debounce_ns/1000000ULL),o->flip);
}

void orientation_feed(Orientation *o,int type,int code,int value){
    if(!o||!o->enabled)return;
    if(type==EV_MSC&&code==MSC_RAW){
        int rot=-1;
        switch(value){
            case GSENSOR_PORTRAIT_UP:   rot=0;break;
            case GSENSOR_PORTRAIT_DOWN: rot=180;break;
            default: break;   /* landscape / face up / face down: keep portrait */
        }
        if(o->debug)fprintf(stderr,"[orient] msc_raw=0x%02x rot=%d\n",value,rot);
        if(rot<0)return;
        if(o->flip)rot=rot?0:180;
        o->use_msc=true;
        if(rot!=o->msc_rotation){
            o->msc_rotation=rot;
            o->msc_changed_at=now_ns();
            fprintf(stderr,"[orient] gyro reports portrait %s\n",rot?"down (180)":"up (0)");
        }
        return;
    }
    if(o->use_msc)return;   /* the synthetic stream wins; ignore noisy ABS */
    if(type==EV_ABS){
        if(code==ABS_X)o->px=value;
        else if(code==ABS_Y)o->py=value;
        else if(code==ABS_Z)o->pz=value;
        else return;
        o->pending=true;
        return;
    }
    if(type==EV_SYN&&code==SYN_REPORT&&o->pending){
        o->x=o->px;o->y=o->py;o->z=o->pz;
        o->pending=false;o->have=true;o->reports++;
        if(o->debug&&o->reports<=6)
            fprintf(stderr,"[orient] sample x=%d y=%d z=%d\n",o->x,o->y,o->z);
    }
}

int orientation_tick(Orientation *o){
    if(!o||!o->enabled)return -1;
    if(o->use_msc){
        if(now_ns()-o->msc_changed_at<o->msc_debounce_ns)return -1;
        return o->msc_rotation;
    }
    if(!o->have)return -1;
    long ax=labs(o->x),ay=labs(o->y),az=labs(o->z);
    /* Only decide while held upright in portrait: the long (Y) axis carries
       gravity and clearly dominates. Flat on a table (Z dominant) and
       landscape (X dominant) are deliberately ignored. */
    if(ay<=16||ay<=ax||ay<=az){
        o->stable_sign=0;o->stable_since=0;
        return -1;
    }
    int sign=o->y>0?1:-1;
    if(o->flip)sign=-sign;
    if(!o->baseline_sign){
        o->baseline_sign=sign;
        fprintf(stderr,"[orient] baseline calibrated (rotation 0), sign=%d\n",sign);
        return -1;
    }
    int want=(sign==o->baseline_sign)?0:180;
    if(sign!=o->stable_sign){o->stable_sign=sign;o->stable_since=now_ns();return -1;}
    if(now_ns()-o->stable_since<o->hold_ns)return -1;
    return want;
}

#define WORDS(maxv) ((maxv + 8*sizeof(unsigned long) - 1)/(8*sizeof(unsigned long)))
static bool has_bit(const unsigned long *bits,size_t words,unsigned code){
    size_t i=code/(8*sizeof(unsigned long));
    if(i>=words)return false;
    return ((bits[i]>>(code%(8*sizeof(unsigned long)))&1UL)!=0);
}

int orientation_probe(void){
    fprintf(stderr,"[probe] input devices (accelerometer / gyro)\n");
    for(int i=0;i<32;i++){
        char path[64],name[128]={0};
        snprintf(path,sizeof(path),"/dev/input/event%d",i);
        int fd=open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC);
        if(fd<0)continue;
        if(ioctl(fd,EVIOCGNAME(sizeof(name)-1),name)<0)name[0]=0;
        unsigned long ab[WORDS(ABS_MAX)]={0},mb[WORDS(MSC_MAX)]={0};
        int a=ioctl(fd,EVIOCGBIT(EV_ABS,sizeof(ab)),ab);
        int m=ioctl(fd,EVIOCGBIT(EV_MSC,sizeof(mb)),mb);
        bool accel=a>=0&&has_bit(ab,WORDS(ABS_MAX),ABS_X)&&
                   has_bit(ab,WORDS(ABS_MAX),ABS_Y)&&has_bit(ab,WORDS(ABS_MAX),ABS_Z);
        fprintf(stderr,"[probe] %-22s name=%-28s accel3=%d msc_raw=%d\n",path,name,accel,
                m>=0&&has_bit(mb,WORDS(MSC_MAX),MSC_RAW));
        close(fd);
    }
    return 0;
}
