#ifndef WIN32
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <assert.h>
#include "ecore_private.h"
#include "Ecore.h"

/* make mono happy - this is evil though... */
#undef SIGPWR
/* valgrind in some versions/setups uses SIGRT's... hmmm */
/* #undef SIGRTMIN */

typedef void (*Signal_Handler)(int sig, siginfo_t *si, void *foo);

static void _ecore_signal_callback_set(int sig, Signal_Handler func); 
static void _ecore_signal_callback_ignore(int sig, siginfo_t *si, void *foo);
static void _ecore_signal_callback_sigchld(int sig, siginfo_t *si, void *foo);
static void _ecore_signal_callback_sigusr1(int sig, siginfo_t *si, void *foo);
static void _ecore_signal_callback_sigusr2(int sig, siginfo_t *si, void *foo);
static void _ecore_signal_callback_sighup(int sig, siginfo_t *si, void *foo);
static void _ecore_signal_callback_sigquit(int sig, siginfo_t *si, void *foo);
static void _ecore_signal_callback_sigint(int sig, siginfo_t *si, void *foo);
static void _ecore_signal_callback_sigterm(int sig, siginfo_t *si, void *foo);
#ifdef SIGPWR
static void _ecore_signal_callback_sigpwr(int sig, siginfo_t *si, void *foo);
#endif

#ifdef SIGRTMIN
static void _ecore_signal_callback_sigrt(int sig, siginfo_t *si, void *foo);
#endif

static int _ecore_signal_exe_exit_delay(void *data);

static volatile sig_atomic_t sig_count = 0;
static volatile sig_atomic_t sigchld_count = 0;
static volatile sig_atomic_t sigusr1_count = 0;
static volatile sig_atomic_t sigusr2_count = 0;
static volatile sig_atomic_t sighup_count = 0;
static volatile sig_atomic_t sigquit_count = 0;
static volatile sig_atomic_t sigint_count = 0;
static volatile sig_atomic_t sigterm_count = 0;

static volatile siginfo_t sigchld_info;
static volatile siginfo_t sigusr1_info;
static volatile siginfo_t sigusr2_info;
static volatile siginfo_t sighup_info;
static volatile siginfo_t sigquit_info;
static volatile siginfo_t sigint_info;
static volatile siginfo_t sigterm_info;

#ifdef SIGPWR
static volatile sig_atomic_t sigpwr_count = 0;
static volatile siginfo_t sigpwr_info = {0};
#endif

#ifdef SIGRTMIN
static volatile sig_atomic_t *sigrt_count = NULL;
static volatile siginfo_t *sigrt_info = NULL;
#endif

void
_ecore_signal_shutdown(void)
{
#ifdef SIGRTMIN
   int i, num = SIGRTMAX - SIGRTMIN;
#endif

   _ecore_signal_callback_set(SIGPIPE, (Signal_Handler) SIG_DFL);
   _ecore_signal_callback_set(SIGALRM, (Signal_Handler) SIG_DFL);
   _ecore_signal_callback_set(SIGCHLD, (Signal_Handler) SIG_DFL);
   _ecore_signal_callback_set(SIGUSR1, (Signal_Handler) SIG_DFL);
   _ecore_signal_callback_set(SIGUSR2, (Signal_Handler) SIG_DFL);
   _ecore_signal_callback_set(SIGHUP,  (Signal_Handler) SIG_DFL);
   _ecore_signal_callback_set(SIGQUIT, (Signal_Handler) SIG_DFL);
   _ecore_signal_callback_set(SIGINT,  (Signal_Handler) SIG_DFL);
   _ecore_signal_callback_set(SIGTERM, (Signal_Handler) SIG_DFL);
#ifdef SIGPWR
   _ecore_signal_callback_set(SIGPWR, (Signal_Handler) SIG_DFL);
   sigpwr_count = 0;
#endif
   sigchld_count = 0;
   sigusr1_count = 0;
   sigusr2_count = 0;
   sighup_count = 0;
   sigquit_count = 0;
   sigint_count = 0;
   sigterm_count = 0;
   sig_count = 0;

#ifdef SIGRTMIN
   for (i = 0; i < num; i++)
     {
	_ecore_signal_callback_set(SIGRTMIN + i, (Signal_Handler) SIG_DFL);
	sigrt_count[i] = 0;
     }

   if (sigrt_count)
     {
	free((sig_atomic_t *) sigrt_count);
	sigrt_count = NULL;
     }

   if (sigrt_info)
     {
	free((siginfo_t *) sigrt_info);
	sigrt_info = NULL;
     }
#endif
}

void
_ecore_signal_init(void)
{
#ifdef SIGRTMIN
   int i, num = SIGRTMAX - SIGRTMIN;
#endif

   _ecore_signal_callback_set(SIGPIPE, _ecore_signal_callback_ignore);
   _ecore_signal_callback_set(SIGALRM, _ecore_signal_callback_ignore);
   _ecore_signal_callback_set(SIGCHLD, _ecore_signal_callback_sigchld);
   _ecore_signal_callback_set(SIGUSR1, _ecore_signal_callback_sigusr1);
   _ecore_signal_callback_set(SIGUSR2, _ecore_signal_callback_sigusr2);
   _ecore_signal_callback_set(SIGHUP,  _ecore_signal_callback_sighup);
   _ecore_signal_callback_set(SIGQUIT, _ecore_signal_callback_sigquit);
   _ecore_signal_callback_set(SIGINT,  _ecore_signal_callback_sigint);
   _ecore_signal_callback_set(SIGTERM, _ecore_signal_callback_sigterm);
#ifdef SIGPWR
   _ecore_signal_callback_set(SIGPWR,  _ecore_signal_callback_sigpwr);
#endif

#ifdef SIGRTMIN
   sigrt_count = calloc(1, sizeof(sig_atomic_t) * num);
   assert(sigrt_count);

   sigrt_info = calloc(1, sizeof(siginfo_t) * num);
   assert(sigrt_info);
   
   for (i = 0; i < num; i++)
      _ecore_signal_callback_set(SIGRTMIN + i, _ecore_signal_callback_sigrt);
#endif
}

int
_ecore_signal_count_get(void)
{
   return sig_count;
}

void
_ecore_signal_call(void)
{
#ifdef SIGRTMIN
   int i, num = SIGRTMAX - SIGRTMIN;
#endif

   while (sigchld_count > 0)
     {
	pid_t pid;
	int status;
	
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
	  {
	     Ecore_Exe_Event_Del *e;
	     
	     /* FIXME: If this process is set respawn, respawn with a suitable backoff
	      * period for those that need too much respawning. 
	      */
	     e = _ecore_event_exe_exit_new();
	     if (e)
	       {
		  if (WIFEXITED(status))
		    {
		       e->exit_code = WEXITSTATUS(status);
		       e->exited = 1;
		    }
		  else if (WIFSIGNALED(status))
		    {
		       e->exit_signal = WTERMSIG(status);
		       e->signalled = 1;
		    }
		  e->pid = pid;
		  e->exe = _ecore_exe_find(pid);
		  
		  if (sigchld_info.si_signo)
		    e->data = sigchld_info; /* FIXME: I'm not sure, but maybe we should clone this.  I don't know if anybody uses it. */
		  
                  if ((e->exe) && (e->exe->flags & (ECORE_EXE_PIPE_READ | ECORE_EXE_PIPE_ERROR)))
                     {
		        /* We want to report the Last Words of the exe, so delay this event.
			 * This is twice as relevant for stderr.
			 * There are three possibilities here -
			 *  1 There are no Last Words.
			 *  2 There are Last Words, they are not ready to be read.
			 *  3 There are Last Words, they are ready to be read.
			 *
			 * For 1 we don't want to delay, for 3 we want to delay.
			 * 2 is the problem.  If we check for data now and there 
			 * is none, then there is no way to differentiate 1 and 2.
			 * If we don't delay, we may loose data, but if we do delay, 
			 * there may not be data and the exit event never gets sent.
			 * 
			 * Any way you look at it, there has to be some time passed 
			 * before the exit event gets sent.  So the strategy here is
			 * to setup a timer event that will send the exit event after
			 * an arbitrary, but brief, time.
			 *
			 * This is probably paranoid, for the less paraniod, we could 
			 * check to see for Last Words, and only delay if there are any.
			 * This has it's own set of problems.
			 */
			printf("Delaying exit event for %s.\n", e->exe->cmd);
                        IF_FN_DEL(ecore_timer_del, e->exe->doomsday_clock);
                        e->exe->doomsday_clock = ecore_timer_add(0.1, _ecore_signal_exe_exit_delay, e);
                     }
		  else
		    {
		       if (e->exe) printf("Sending exit event for %s.\n", e->exe->cmd);
		       _ecore_event_add(ECORE_EXE_EVENT_DEL, e, 
				   _ecore_event_exe_exit_free, NULL);
		    }
	       }
	  }
	sigchld_count--;
	sig_count--;
     }
   while (sigusr1_count > 0)
     {
	Ecore_Event_Signal_User *e;
	
	e = _ecore_event_signal_user_new();
	if (e)
	  {
	     e->number = 1;

	     if (sigusr1_info.si_signo)
	       e->data = sigusr1_info;	 
	     
	     ecore_event_add(ECORE_EVENT_SIGNAL_USER, e, NULL, NULL);
	  }
	sigusr1_count--;
	sig_count--;
     }
   while (sigusr2_count > 0)
     {
	Ecore_Event_Signal_User *e;
	
	e = _ecore_event_signal_user_new();
	if (e)
	  {
	     e->number = 2;
	     
	     if (sigusr2_info.si_signo)
	       e->data = sigusr2_info;	 
	     
	     ecore_event_add(ECORE_EVENT_SIGNAL_USER, e, NULL, NULL);
	  }
	sigusr2_count--;
	sig_count--;
     }
   while (sighup_count > 0)
     {
	Ecore_Event_Signal_Hup *e;
	
	e = _ecore_event_signal_hup_new();
	if (e)
	  {
	     if (sighup_info.si_signo)
	       e->data = sighup_info;
	     
	     ecore_event_add(ECORE_EVENT_SIGNAL_HUP, e, NULL, NULL);
	  }
	sighup_count--;
	sig_count--;
     }
   while (sigquit_count > 0)
     {
	Ecore_Event_Signal_Exit *e;
	
	e = _ecore_event_signal_exit_new();
	if (e)
	  {
	     e->quit = 1;

	     if (sigquit_info.si_signo)
	       e->data = sigquit_info;
	     
	     ecore_event_add(ECORE_EVENT_SIGNAL_EXIT, e, NULL, NULL);
	  }
	sigquit_count--;
	sig_count--;
     }
   while (sigint_count > 0)
     {
	Ecore_Event_Signal_Exit *e;
	
	e = _ecore_event_signal_exit_new();
	if (e)
	  {
	     e->interrupt = 1;
	     
	     if (sigint_info.si_signo)
	       e->data = sigint_info;
	     
	     ecore_event_add(ECORE_EVENT_SIGNAL_EXIT, e, NULL, NULL);
	  }
	sigint_count--;
	sig_count--;
     }
   while (sigterm_count > 0)
     {
	Ecore_Event_Signal_Exit *e;
	
	e = _ecore_event_signal_exit_new();
	if (e)
	  {
	     e->terminate = 1;
	     
	     if (sigterm_info.si_signo)
	       e->data = sigterm_info;
	     
	     ecore_event_add(ECORE_EVENT_SIGNAL_EXIT, e, NULL, NULL);
	  }
	sigterm_count--;
	sig_count--;
     }
#ifdef SIGPWR
   while (sigpwr_count > 0)
     {
	Ecore_Event_Signal_Power *e;
	
	e = _ecore_event_signal_power_new();
	if (e)
	  {
	     if (sigpwr_info.si_signo)
	       e->data = sigpwr_info;
	     
	     ecore_event_add(ECORE_EVENT_SIGNAL_POWER, e, NULL, NULL);
	  }
	sigpwr_count--;
	sig_count--;
     }
#endif

#ifdef SIGRTMIN
   for (i = 0; i < num; i++)
     {
	while (sigrt_count[i] > 0)
	  {
	     Ecore_Event_Signal_Realtime *e;
	     
	     if ((e = _ecore_event_signal_realtime_new()))
	       {
		  e->num = i;
		  
		  if (sigrt_info[i].si_signo)
		    e->data = sigrt_info[i];
		  
		  ecore_event_add(ECORE_EVENT_SIGNAL_REALTIME, e, NULL, NULL);
	       }
	     
	     sigrt_count[i]--;
	     sig_count--;
	  }
     }
#endif
}

static void
_ecore_signal_callback_set(int sig, Signal_Handler func)
{
   struct sigaction  sa;

   sa.sa_sigaction = func;
   sa.sa_flags = SA_RESTART | SA_SIGINFO;
   sigemptyset(&sa.sa_mask);
   sigaction(sig, &sa, NULL); 
}

static void
_ecore_signal_callback_ignore(int sig __UNUSED__, siginfo_t *si __UNUSED__, void *foo __UNUSED__)
{
}

static void
_ecore_signal_callback_sigchld(int sig __UNUSED__, siginfo_t *si, void *foo __UNUSED__)
{
   if (si)
     sigchld_info = *si;
   else
     sigchld_info.si_signo = 0;
   
   sigchld_count++;
   sig_count++;
}

static void
_ecore_signal_callback_sigusr1(int sig __UNUSED__, siginfo_t *si, void *foo __UNUSED__)
{
   if (si)
     sigusr1_info = *si;
   else
     sigusr1_info.si_signo = 0;
   
   sigusr1_count++;
   sig_count++;
}

static void
_ecore_signal_callback_sigusr2(int sig __UNUSED__, siginfo_t *si, void *foo __UNUSED__)
{
   if (si)
     sigusr2_info = *si;
   else
     sigusr2_info.si_signo = 0;
   
   sigusr2_count++;
   sig_count++;
}

static void
_ecore_signal_callback_sighup(int sig __UNUSED__, siginfo_t *si, void *foo __UNUSED__)
{
   if (si)
     sighup_info = *si;
   else
     sighup_info.si_signo = 0;
   
   sighup_count++;
   sig_count++;
}

static void
_ecore_signal_callback_sigquit(int sig __UNUSED__, siginfo_t *si, void *foo __UNUSED__)
{
   if (si)
     sigquit_info = *si;
   else
     sigquit_info.si_signo = 0;

   sigquit_count++;
   sig_count++;
}

static void
_ecore_signal_callback_sigint(int sig __UNUSED__, siginfo_t *si, void *foo __UNUSED__)
{
   if (si)
     sigint_info = *si;
   else
     sigint_info.si_signo = 0;

   sigint_count++;
   sig_count++;
}

static void
_ecore_signal_callback_sigterm(int sig __UNUSED__, siginfo_t *si, void *foo __UNUSED__)
{
   if (si)
     sigterm_info = *si;
   else
     sigterm_info.si_signo = 0;

   sigterm_count++;
   sig_count++;
}

#ifdef SIGPWR
static void
_ecore_signal_callback_sigpwr(int sig __UNUSED__, siginfo_t *si, void *foo __UNUSED__)
{
   if (si)
     sigpwr_info = *si;
   else
     sigpwr_info.si_signo = 0;
   
   sigpwr_count++;
   sig_count++;
}
#endif

#ifdef SIGRTMIN
static void
_ecore_signal_callback_sigrt(int sig, siginfo_t *si, void *foo __UNUSED__)
{
   if (si)
     sigrt_info[sig - SIGRTMIN] = *si;
   else
     sigrt_info[sig - SIGRTMIN].si_signo = 0;
   
   sigrt_count[sig - SIGRTMIN]++;
   sig_count++;
}
#endif

static int
_ecore_signal_exe_exit_delay(void *data)
{
   Ecore_Exe_Event_Del *e;
   
   e = data;
   if (e)
     {
	printf("Sending delayed exit event for %s.\n", e->exe->cmd);
	_ecore_event_add(ECORE_EXE_EVENT_DEL, e, 
			 _ecore_event_exe_exit_free, NULL);
     }
   return 0;
}
#endif
