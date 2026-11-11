/* Run on an isolated X server: xvfb-run -a make test-tray. */
#include <X11/Xlib.h>
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static pid_t child;
static char config[]="/tmp/alarm-tray-XXXXXX";

static void cleanup(void) {
    if(child>0) { kill(child,SIGTERM); waitpid(child,NULL,0); }
    char path[256];
    snprintf(path,sizeof path,"%s/alarm-applet/alarms.lock",config); unlink(path);
    snprintf(path,sizeof path,"%s/alarm-applet",config); rmdir(path);
    rmdir(config);
}
static void check(int condition,const char *message) {
    if(!condition) { fprintf(stderr,"Tray test: %s\n",message); exit(1); }
}
static void settle(void) {
    struct timespec delay={0,100000000}; nanosleep(&delay,NULL);
}
static void check_visible(Display *display,Window window,int visible) {
    XWindowAttributes attributes;
    for(int i=0;i<30;i++) {
        XSync(display,False);
        XGetWindowAttributes(display,window,&attributes);
        if(attributes.map_state==(visible?IsViewable:IsUnmapped)) return;
        settle();
    }
    check(0,visible?"window did not reopen":"window did not hide");
}
int main(void) {
    Display *d=XOpenDisplay(NULL); check(d!=NULL,"DISPLAY is unavailable");
    Window root=DefaultRootWindow(d);
    char selection_name[64];
    snprintf(selection_name,sizeof selection_name,"_NET_SYSTEM_TRAY_S%d",DefaultScreen(d));
    Atom selection=XInternAtom(d,selection_name,False);
    check(!XGetSelectionOwner(d,selection),"use an isolated X server without a tray");
    check(mkdtemp(config)!=NULL,"cannot create temporary configuration");
    atexit(cleanup);
    Window manager=XCreateSimpleWindow(d,root,600,600,50,50,0,0,0);
    XSetSelectionOwner(d,selection,manager,CurrentTime); XSync(d,False);
    child=fork(); check(child>=0,"cannot start app");
    if(!child) {
        setenv("XDG_CONFIG_HOME",config,1);
        execl("./alarm-applet","alarm-applet","--hidden",NULL); _exit(127);
    }
    Window icon=0,main_window=0;
    for(int i=0;i<30 && (!icon || !main_window);i++) {
        while(XPending(d)) {
            XEvent event; XNextEvent(d,&event);
            if(event.type==ClientMessage && event.xclient.message_type==XInternAtom(d,"_NET_SYSTEM_TRAY_OPCODE",False))
                icon=(Window)event.xclient.data.l[2];
        }
        Window returned,parent,*children=NULL; unsigned count=0;
        XQueryTree(d,root,&returned,&parent,&children,&count);
        for(unsigned j=0;j<count;j++) {
            char *name=NULL; XFetchName(d,children[j],&name);
            if(name && !strcmp(name,"Alarm Applet")) main_window=children[j];
            if(name) XFree(name);
        }
        if(children) XFree(children);
        settle();
    }
    check(icon && main_window,"app did not request docking");
    XReparentWindow(d,icon,manager,0,0);
    XEvent embed={0};
    embed.xclient.type=ClientMessage; embed.xclient.window=icon;
    embed.xclient.message_type=XInternAtom(d,"_XEMBED",False);
    embed.xclient.format=32; embed.xclient.data.l[3]=(long)manager;
    XSendEvent(d,icon,False,NoEventMask,&embed);
    XMapWindow(d,manager); XMapWindow(d,icon);
    XClearArea(d,icon,0,0,0,0,True); XSync(d,False); settle();
    int status;
    check(waitpid(child,&status,WNOHANG)==0,"app exited after XEmbed notification");
    check_visible(d,main_window,0);
    XEvent click={0};
    click.xbutton.type=ButtonPress; click.xbutton.display=d;
    click.xbutton.window=icon; click.xbutton.button=1;
    XSendEvent(d,icon,False,ButtonPressMask,&click);
    check_visible(d,main_window,1);
    XEvent close={0};
    close.xclient.type=ClientMessage; close.xclient.window=main_window;
    close.xclient.format=32;
    close.xclient.message_type=XInternAtom(d,"WM_PROTOCOLS",False);
    close.xclient.data.l[0]=(long)XInternAtom(d,"WM_DELETE_WINDOW",False);
    XSendEvent(d,main_window,False,NoEventMask,&close);
    check_visible(d,main_window,0);
    XSendEvent(d,icon,False,ButtonPressMask,&click);
    check_visible(d,main_window,1);
    XCloseDisplay(d);
    puts("Tray: XEmbed notification, hidden startup, reopen and close-to-hide passed.");
    return 0;
}
