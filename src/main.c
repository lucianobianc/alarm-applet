#include "alarm.h"
#include "sound.h"
#include "storage.h"
#include <signal.h>
#include <sys/wait.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h>
#include <Xm/TextF.h>
#include <Xm/List.h>
#include <Xm/Label.h>
#include <Xm/MessageB.h>
#include <Xm/Protocols.h>
#include <X11/Xatom.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static XtAppContext app;
static Widget shell, list, editor, hour_input, minute_input, duration_input, frequency_input, twelve, pm, days[7];
static Alarm alarms[MAX_ALARMS];
static int count, editing=-1, ringing;
static pid_t sound_pid;
static struct timespec ring_end;
static char data_path[4096];
static const char *day_names[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static Window tray, tray_manager;
static int tray_embedded;
static volatile sig_atomic_t window_requested, stop_requested;
static void process_signal(int signal_number) {
    if(signal_number==SIGUSR1) window_requested=1;
    else if(signal_number==SIGUSR2) window_requested=-1;
    else stop_requested=1;
}
static void show_window(void) {
    XtMapWidget(shell); XRaiseWindow(XtDisplay(shell),XtWindow(shell));
}
static XtEventDispatchProc previous_dispatch[LASTEvent];
static int start_hidden;
static int dock(void);
static void error_message(const char *message) {
    XmString s=XmStringCreateLocalized((char *)message);
    Widget dialog=XmCreateErrorDialog(shell,"error",NULL,0);
    XtVaSetValues(dialog,XmNmessageString,s,NULL); XmStringFree(s);
    XtUnmanageChild(XmMessageBoxGetChild(dialog,XmDIALOG_HELP_BUTTON));
    XtUnmanageChild(XmMessageBoxGetChild(dialog,XmDIALOG_CANCEL_BUTTON));
    XtManageChild(dialog);
}
static int save(void) {
    if(alarms_save(data_path,alarms,count)<0) {
        error_message("Cannot save alarms."); return 0;
    }
    return 1;
}

static void refresh(void) {
    XmListDeleteAllItems(list);
    for(int i=0;i<count;i++) {
        char row[256], selected[64]="";
        for(int d=0;d<7;d++) if(alarms[i].days&(1<<d)) { strcat(selected,day_names[d]); strcat(selected," "); }
        snprintf(row,sizeof row,"%02d:%02d   %s  %d s",alarms[i].hour,alarms[i].minute,selected,alarms[i].duration);
        XmString s=XmStringCreateLocalized(row); XmListAddItemUnselected(list,s,0); XmStringFree(s);
    }
}
static int selected_index(void) {
    int *positions=NULL,n=0,index=-1;
    if(XmListGetSelectedPos(list,&positions,&n) && n) index=positions[0]-1;
    if(positions) XtFree((char *)positions);
    return index;
}
static int number(Widget input) {
    char *s=XmTextFieldGetString(input),*end;
    errno=0; long n=strtol(s,&end,10);
    int value=(!*s || *end || errno || n<0 || n>20000)?-1:(int)n;
    XtFree(s); return value;
}
static void cancel_edit(Widget w,XtPointer p,XtPointer c) { (void)w;(void)p;(void)c; XtDestroyWidget(editor); editor=NULL; }
static void commit_edit(Widget w,XtPointer p,XtPointer c) {
    (void)w;(void)p;(void)c;
    Alarm a={0};
    a.hour=alarm_hour(number(hour_input),XmToggleButtonGetState(twelve),XmToggleButtonGetState(pm));
    a.minute=number(minute_input); a.duration=number(duration_input); a.frequency=number(frequency_input);
    for(int d=0;d<7;d++) if(XmToggleButtonGetState(days[d])) a.days|=1<<d;
    if(!alarm_valid(&a)) { error_message("Enter a valid time, select at least one day, use a duration of 1-3600 seconds and a frequency of 20-20000 Hz."); return; }
    Alarm old={0}; int old_count=count;
    if(editing<0) alarms[count++]=a; else {
        old=alarms[editing];
        if(a.hour==old.hour && a.minute==old.minute) a.last_fired=old.last_fired;
        alarms[editing]=a;
    }
    if(!save()) { count=old_count; if(editing>=0) alarms[editing]=old; return; }
    refresh(); cancel_edit(w,p,c);
}
static Widget field(Widget parent,const char *label,int value) {
    XtVaCreateManagedWidget(label,xmLabelWidgetClass,parent,NULL);
    char text[16]; snprintf(text,sizeof text,"%d",value);
    return XtVaCreateManagedWidget("value",xmTextFieldWidgetClass,parent,XmNvalue,text,XmNcolumns,6,NULL);
}
static void format_changed(Widget w,XtPointer p,XtPointer c) {
    (void)w;(void)p;(void)c;
    int h=number(hour_input), is12=XmToggleButtonGetState(twelve);
    char text[16];
    if(is12 && h>=0 && h<24) { XmToggleButtonSetState(pm,h>=12,False); h=h%12?h%12:12; }
    else if(!is12 && h>=1 && h<=12) h=h%12+(XmToggleButtonGetState(pm)?12:0);
    if(h>=0) { snprintf(text,sizeof text,"%d",h); XmTextFieldSetString(hour_input,text); }
    XtSetSensitive(pm,is12);
}
static void open_edit(Widget w,XtPointer p,XtPointer c) {
    (void)w;(void)c;
    if(editor) { XtManageChild(editor); return; }
    editing=p?selected_index():-1;
    if(p && editing<0) return;
    if(editing<0 && count==MAX_ALARMS) { error_message("The alarm limit is 128."); return; }
    Alarm a=editing<0?(Alarm){9,0,127,5,1000,0}:alarms[editing];
    editor=XmCreateFormDialog(shell,"Alarm",NULL,0);
    XtVaSetValues(editor,XmNdialogStyle,XmDIALOG_FULL_APPLICATION_MODAL,NULL);
    Atom close_editor=XmInternAtom(XtDisplay(shell),"WM_DELETE_WINDOW",False);
    XtVaSetValues(XtParent(editor),XmNdeleteResponse,XmDO_NOTHING,NULL);
    XmAddWMProtocolCallback(XtParent(editor),close_editor,cancel_edit,NULL);
    Widget column=XtVaCreateManagedWidget("fields",xmRowColumnWidgetClass,editor,NULL);
    hour_input=field(column,"Hour",a.hour); minute_input=field(column,"Minute",a.minute);
    twelve=XtVaCreateManagedWidget("12-hour format",xmToggleButtonWidgetClass,column,NULL);
    pm=XtVaCreateManagedWidget("PM (unchecked: AM)",xmToggleButtonWidgetClass,column,XmNsensitive,False,NULL);
    XtAddCallback(twelve,XmNvalueChangedCallback,format_changed,NULL);
    Widget row=XtVaCreateManagedWidget("Days",xmRowColumnWidgetClass,column,XmNorientation,XmHORIZONTAL,NULL);
    for(int d=0;d<7;d++) days[d]=XtVaCreateManagedWidget(day_names[d],xmToggleButtonWidgetClass,row,XmNset,(Boolean)!!(a.days&(1<<d)),NULL);
    duration_input=field(column,"Beep duration (seconds)",a.duration);
    frequency_input=field(column,"Beep frequency (Hz)",a.frequency);
    Widget button=XtVaCreateManagedWidget("OK",xmPushButtonWidgetClass,column,NULL); XtAddCallback(button,XmNactivateCallback,commit_edit,NULL);
    button=XtVaCreateManagedWidget("Cancel",xmPushButtonWidgetClass,column,NULL); XtAddCallback(button,XmNactivateCallback,cancel_edit,NULL);
    XtVaSetValues(editor,XmNdefaultButton,XtNameToWidget(column,"OK"),XmNcancelButton,button,NULL);
    XtManageChild(editor);
}
static void remove_alarm(Widget w,XtPointer p,XtPointer c) {
    (void)w;(void)p;(void)c; int i=selected_index(); if(i<0)return;
    Alarm old[MAX_ALARMS]; memcpy(old,alarms,sizeof old);
    memmove(&alarms[i],&alarms[i+1],(size_t)(count-i-1)*sizeof *alarms); count--;
    if(!save()) { memcpy(alarms,old,sizeof old); count++; } refresh();
}
static void stop(Widget w,XtPointer p,XtPointer c) { (void)w;(void)p;(void)c; ringing=0; sound_stop(&sound_pid); }
static void tick(XtPointer p,XtIntervalId *id) {
    (void)p;(void)id;
    if(stop_requested) { sound_stop(&sound_pid); XtAppSetExitFlag(app); return; }
    if(window_requested) {
        sig_atomic_t requested=window_requested; window_requested=0;
        if(requested>0) show_window(); else XtUnmapWidget(shell);
    }
    dock();
    time_t now=time(NULL); struct timespec mono; clock_gettime(CLOCK_MONOTONIC,&mono);
    int fired=0;
    for(int i=0;i<count;i++) if(alarm_due(&alarms[i],now)) {
        fired=1;
        time_t end=mono.tv_sec+alarms[i].duration;
        if(!ringing || end>ring_end.tv_sec) {
            ring_end.tv_sec=end; ring_end.tv_nsec=mono.tv_nsec;
            ringing=1;
            sound_stop(&sound_pid);
            sound_pid=sound_start((unsigned)alarms[i].duration,(unsigned)alarms[i].frequency);
            if(sound_pid<0) { ringing=0; error_message("Cannot start audio playback."); }
        }
    }
    if(fired) save();
    if(ringing && (mono.tv_sec>ring_end.tv_sec || (mono.tv_sec==ring_end.tv_sec && mono.tv_nsec>=ring_end.tv_nsec))) ringing=0;
    if(sound_pid>0) {
        int status;
        if(waitpid(sound_pid,&status,WNOHANG)==sound_pid) {
            sound_pid=0; ringing=0;
            if(!WIFEXITED(status) || WEXITSTATUS(status)) error_message("Cannot play alarm audio. Check the system audio output.");
        }
    }
    XtAppAddTimeOut(app,500,tick,NULL);
}
static void hide(Widget w,XtPointer p,XtPointer c) { (void)w;(void)p;(void)c; if(tray_embedded) XtUnmapWidget(shell); else XIconifyWindow(XtDisplay(shell),XtWindow(shell),DefaultScreen(XtDisplay(shell))); }
/* Tray windows are raw X11 windows. Never dispatch their XEmbed messages
 * through the Motif shell: its protocol handler requires the shell's window. */
static Boolean dispatch_event(XEvent *event) {
    if(tray && event->xany.window==tray) {
        if(event->type==ClientMessage &&
           event->xclient.message_type==XInternAtom(XtDisplay(shell),"_XEMBED",False) &&
           event->xclient.data.l[1]==0) {
            tray_embedded=1;
            XMapWindow(XtDisplay(shell),tray);
        }
        if(event->type==ButtonPress) show_window();
        if(event->type==DestroyNotify) { tray=0; tray_embedded=0; }
        if(event->type==Expose) {
            Display *d=XtDisplay(shell); GC gc=XCreateGC(d,tray,0,NULL);
            XSetForeground(d,gc,BlackPixel(d,DefaultScreen(d)));
            XDrawArc(d,tray,gc,3,3,16,16,0,360*64);
            XDrawLine(d,tray,gc,11,6,11,11);
            XDrawLine(d,tray,gc,11,11,15,13);
            XFreeGC(d,gc);
        }
        return True;
    }
    if(event->type>=0 && event->type<LASTEvent && previous_dispatch[event->type])
        return previous_dispatch[event->type](event);
    return False;
}
static int ignore_destroyed_tray(Display *display,XErrorEvent *event) {
    (void)display;
    if(event->error_code==BadWindow && event->resourceid==tray) return 0;
    char message[256]; XGetErrorText(display,event->error_code,message,sizeof message);
    fprintf(stderr,"X11: %s\n",message); return 0;
}
static int dock(void) {
    Display *d=XtDisplay(shell); char name[64]; snprintf(name,sizeof name,"_NET_SYSTEM_TRAY_S%d",DefaultScreen(d));
    Window manager=XGetSelectionOwner(d,XInternAtom(d,name,False));
    if(manager==tray_manager && tray) return tray_embedded;
    if(tray) {
        /* The previous panel may already have destroyed its child window. */
        int (*previous)(Display *,XErrorEvent *)=XSetErrorHandler(ignore_destroyed_tray);
        XDestroyWindow(d,tray); XSync(d,False); XSetErrorHandler(previous);
        tray=0;
    }
    tray_manager=manager; tray_embedded=0;
    if(!manager) return 0;
    tray=XCreateSimpleWindow(d,DefaultRootWindow(d),0,0,24,24,0,0,WhitePixel(d,DefaultScreen(d)));
    XSizeHints size={0};
    size.flags=PMinSize|PMaxSize|PBaseSize|PSize;
    size.min_width=size.min_height=24;
    size.max_width=size.max_height=24;
    size.base_width=size.base_height=24;
    size.width=size.height=24;
    XSetWMNormalHints(d,tray,&size);
    XClassHint identity={"alarm-applet","AlarmApplet"};
    XSetClassHint(d,tray,&identity);
    XStoreName(d,tray,"Alarm Applet Tray");
    unsigned long info[2]={0,1}; Atom embed=XInternAtom(d,"_XEMBED_INFO",False);
    XChangeProperty(d,tray,embed,embed,32,PropModeReplace,(unsigned char *)info,2);
    XSelectInput(d,tray,ExposureMask|ButtonPressMask|StructureNotifyMask);
    XEvent event={0}; event.xclient.type=ClientMessage; event.xclient.window=manager;
    event.xclient.message_type=XInternAtom(d,"_NET_SYSTEM_TRAY_OPCODE",False); event.xclient.format=32;
    event.xclient.data.l[0]=CurrentTime; event.xclient.data.l[1]=0; event.xclient.data.l[2]=(long)tray;
    XSendEvent(d,manager,False,NoEventMask,&event);
    XFlush(d); return 1;
}
int main(int argc,char **argv) {
    int hidden=0,remaining=1;
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--hidden")) hidden=1;
        else if(!strcmp(argv[i],"--help")) {
            puts("Usage: alarm-applet [--hidden] [Xt options]\n--hidden  Start without opening the alarm window.");
            return 0;
        }
        else argv[remaining++]=argv[i];
    }
    argc=remaining; argv[argc]=NULL; start_hidden=hidden;
    struct sigaction action={0};
    action.sa_handler=process_signal; action.sa_flags=SA_RESTART; sigemptyset(&action.sa_mask);
    sigaction(SIGUSR1,&action,NULL); sigaction(SIGUSR2,&action,NULL); sigaction(SIGTERM,&action,NULL); sigaction(SIGINT,&action,NULL);
    static String defaults[]={
        "*fontList: -*-helvetica-medium-r-normal--18-*-*-*-*-*-iso8859-1",
        NULL
    };
    shell=XtVaAppInitialize(&app,"AlarmApplet",NULL,0,&argc,argv,defaults,XmNtitle,"Alarm Applet",XmNmappedWhenManaged,False,NULL);
    if(argc>1) { fprintf(stderr,"Unknown argument: %s\nUse --help for usage.\n",argv[1]); XtDestroyWidget(shell); return 2; }
    const char *home=getenv("HOME"),*config=getenv("XDG_CONFIG_HOME"); char directory[4096];
    int path_length;
    if(config && *config=='/') path_length=snprintf(directory,sizeof directory,"%s/alarm-applet",config);
    else if(home) path_length=snprintf(directory,sizeof directory,"%s/.config/alarm-applet",home);
    else { fputs("HOME or XDG_CONFIG_HOME is required.\n",stderr); return 1; }
    if(path_length<0 || path_length>=(int)sizeof directory) { fputs("Configuration path is too long.\n",stderr); return 1; }
    if(make_directory(directory)<0) {perror(directory);return 1;}
    if(snprintf(data_path,sizeof data_path,"%s/alarms",directory)>=(int)sizeof data_path) return 1;
    char lock_path[4120]; snprintf(lock_path,sizeof lock_path,"%s.lock",data_path);
    int lock_fd=open(lock_path,O_CREAT|O_RDWR,0600);
    struct flock lock={.l_type=F_WRLCK,.l_whence=SEEK_SET};
    if(lock_fd<0) {perror("Alarm data lock");return 1;}
    if(fcntl(lock_fd,F_SETLK,&lock)<0) {
        if(errno==EACCES || errno==EAGAIN) {
            lock.l_type=F_WRLCK;
            if(fcntl(lock_fd,F_GETLK,&lock)==0 && lock.l_type!=F_UNLCK) {
                char expected[80],actual[80]={0};
                int length=snprintf(expected,sizeof expected,"alarm-applet-ipc-v1 %ld\n",(long)lock.l_pid);
                ssize_t bytes=pread(lock_fd,actual,sizeof actual-1,0);
                if(bytes!=length || memcmp(actual,expected,(size_t)length)) {
                    fputs("An older instance is running. Stop it before using this version.\n",stderr);
                    close(lock_fd); return 1;
                }
                int result=kill(lock.l_pid,hidden?SIGUSR2:SIGUSR1);
                close(lock_fd); return result<0?1:0;
            }
        }
        perror("Alarm data lock"); close(lock_fd); return 1;
    }
    char identity[80];
    int identity_length=snprintf(identity,sizeof identity,"alarm-applet-ipc-v1 %ld\n",(long)getpid());
    if(ftruncate(lock_fd,0)<0 || pwrite(lock_fd,identity,(size_t)identity_length,0)!=identity_length) {
        perror("Alarm data lock"); close(lock_fd); return 1;
    }
    if(alarms_load(data_path,alarms,&count)<0) {
        fprintf(stderr,"Cannot read alarm data: %s\n",data_path); close(lock_fd); return 1;
    }
    Widget column=XtVaCreateManagedWidget("main",xmRowColumnWidgetClass,shell,NULL);
    list=XmCreateScrolledList(column,"alarms",NULL,0); XtVaSetValues(list,XmNvisibleItemCount,8,XmNwidth,480,NULL); XtManageChild(list);
    Widget row=XtVaCreateManagedWidget("actions",xmRowColumnWidgetClass,column,XmNorientation,XmHORIZONTAL,NULL);
    const char *labels[]={"Add...","Edit...","Delete","Stop alarm","Hide"};
    XtCallbackProc callbacks[]={open_edit,open_edit,remove_alarm,stop,hide};
    for(size_t i=0;i<sizeof labels/sizeof *labels;i++) { Widget b=XtVaCreateManagedWidget(labels[i],xmPushButtonWidgetClass,row,NULL); XtAddCallback(b,XmNactivateCallback,callbacks[i],i==1?(XtPointer)1:NULL); }
    refresh(); XtRealizeWidget(shell);
    Atom close_atom=XmInternAtom(XtDisplay(shell),"WM_DELETE_WINDOW",False);
    XtVaSetValues(shell,XmNdeleteResponse,XmDO_NOTHING,NULL); XmAddWMProtocolCallback(shell,close_atom,hide,NULL);
    for(int type=2;type<LASTEvent;type++)
        previous_dispatch[type]=XtSetEventDispatcher(XtDisplay(shell),type,dispatch_event);
    dock();
    if(!start_hidden) show_window();
    tick(NULL,NULL); XtAppMainLoop(app); XtDestroyWidget(shell); close(lock_fd); return 0;
}
