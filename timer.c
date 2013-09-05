#include "taskimpl.h"

static Channel* ctimer; /* chan(Timer*)[100] */
static Timer *timer;

static
uint
msec(void)
{
    return nsec()/1000000;
}

void
timerstop(Timer *t)
{
    t->next = timer;
    timer = t;
}

void
timercancel(Timer *t)
{
    t->cancel = 1;
}

static
void
timertask(void *v)
{
    int i, nt, na, dt, del;
    Timer **t, *x;
    uint old, new;

    USED(v);
    threadsetname("timertask");
    t = nil;
    na = 0;
    nt = 0;
    old = msec();
    for(;;){
        sleep(1);   /* will sleep minimum incr */
        new = msec();
        dt = new-old;
        old = new;
        if(dt < 0)  /* timer wrapped; go around, losing a tick */
            continue;
        for(i=0; i<nt; i++){
            x = t[i];
            x->dt -= dt;
            del = 0;
            if(x->cancel){
                timerstop(x);
                del = 1;
            }else if(x->dt <= 0){
                /*
                 * avoid possible deadlock if client is
                 * now sending on ctimer
                 */
                if(nbsendul(x->c, 0) > 0)
                    del = 1;
            }
            if(del){
                memmove(&t[i], &t[i+1], (nt-i-1)*sizeof t[0]);
                --nt;
                --i;
            }
        }
        if(nt == 0){
            x = recvp(ctimer);
    gotit:
            if(nt == na){
                na += 10;
                t = realloc(t, na*sizeof(Timer*));
                if(t == nil)
                    error("timer realloc failed");
            }
            t[nt++] = x;
            old = msec();
        }
        if(nbrecv(ctimer, &x) > 0)
            goto gotit;
    }
}

void
timerinit(void)
{
    ctimer = chancreate(sizeof(Timer*), 100);
    chansetname(ctimer, "ctimer");
    taskcreate(timertask, nil, STACK);
}

Timer*
timerstart(int dt)
{
    Timer *t;

    t = timer;
    if(t)
        timer = timer->next;
    else{
        t = emalloc(sizeof(Timer));
        t->c = chancreate(sizeof(int), 0);
        chansetname(t->c, "tc%p", t->c);
    }
    t->next = nil;
    t->dt = dt;
    t->cancel = 0;
    sendp(ctimer, t);
    return t;
}
