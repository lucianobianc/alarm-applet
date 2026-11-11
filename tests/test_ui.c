/* Exercise the real Motif controls and callbacks on an isolated X server. */
#define main applet_main
#include "../src/main.c"
#undef main
#include <assert.h>

int main(int argc,char **argv) {
    char config[]="/tmp/alarm-ui-XXXXXX"; assert(mkdtemp(config));
    snprintf(data_path,sizeof data_path,"%s/alarms",config);
    shell=XtVaAppInitialize(&app,"AlarmAppletTest",NULL,0,&argc,argv,NULL,XmNmappedWhenManaged,False,NULL);
    list=XmCreateScrolledList(shell,"alarms",NULL,0); XtManageChild(list); XtRealizeWidget(shell);
    open_edit(NULL,NULL,NULL);
    assert(number(frequency_input)==1000);
    XmToggleButtonSetState(twelve,True,True);
    XmTextFieldSetString(hour_input,"12"); XmTextFieldSetString(minute_input,"15");
    XmTextFieldSetString(duration_input,"2"); XmTextFieldSetString(frequency_input,"1500");
    for(int d=0;d<7;d++) XmToggleButtonSetState(days[d],d==0 || d==6,False);
    commit_edit(NULL,NULL,NULL);
    assert(count==1 && !editor && alarms[0].hour==0 && alarms[0].days==65);
    alarms[0].last_fired=1794130200;
    XmListSelectPos(list,1,False); open_edit(NULL,(XtPointer)1,NULL);
    XmTextFieldSetString(frequency_input,"2000"); commit_edit(NULL,NULL,NULL);
    assert(alarms[0].last_fired==1794130200 && alarms[0].frequency==2000);
    XmListSelectPos(list,1,False); open_edit(NULL,(XtPointer)1,NULL);
    XmToggleButtonSetState(twelve,True,True); assert(number(hour_input)==12);
    XmToggleButtonSetState(pm,True,False);
    XmToggleButtonSetState(twelve,False,True); assert(number(hour_input)==12);
    XmToggleButtonSetState(twelve,True,True); assert(XmToggleButtonGetState(pm));
    cancel_edit(NULL,NULL,NULL); assert(count==1 && alarms[0].hour==0);
    /* Closing the editor through the WM must discard it, not retain stale state. */
    open_edit(NULL,NULL,NULL); cancel_edit(NULL,NULL,NULL); assert(!editor);
    Alarm restored[MAX_ALARMS]; int restored_count;
    assert(alarms_load(data_path,restored,&restored_count)==0);
    assert(restored_count==1 && restored[0].frequency==2000 && restored[0].last_fired==1794130200);
    XmListSelectPos(list,1,False); remove_alarm(NULL,NULL,NULL);
    assert(count==0); assert(alarms_load(data_path,restored,&restored_count)==0 && restored_count==0);
    XtDestroyWidget(shell); unlink(data_path); rmdir(config);
    puts("GUI: add, day selection, AM/PM conversion, frequency, edit, cancel, persistence and delete passed.");
}
