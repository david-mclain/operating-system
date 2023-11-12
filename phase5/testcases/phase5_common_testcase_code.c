#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase3_usermode.h>
#include <phase4.h>
#include <phase5.h>

#include <assert.h>



void startup(int argc, char **argv)
{
    phase1_init();
    phase2_init();
    phase3_init();
    phase4_init();
    phase5_init();
    startProcesses();
}



/* force the testcase driver to priority 1, instead of the
 * normal priority for testcase_main
 */
int start5(char*);
static int start5_trampoline(char*);

int testcase_main()
{
    int pid_fork, pid_join;
    int status;

    pid_fork = fork1("start5", start5_trampoline, "start5", 4*USLOSS_MIN_STACK, 3);
    pid_join = join(&status);

    if (pid_join != pid_fork)
    {
        USLOSS_Console("*** TESTCASE FAILURE *** - the join() pid doesn't match the fork() pid.  %d/%d\n", pid_fork,pid_join);
        USLOSS_Halt(1);
    }

#if 0
    USLOSS_Console("-------- VmStats --------\n");
    USLOSS_Console("faults:         %d\n", vmStats.faults);
    USLOSS_Console("new:            %d\n", vmStats.new);
    USLOSS_Console("replaced:       %d\n", vmStats.replaced);
    USLOSS_Console("pageOuts:       %d\n", vmStats.pageOuts);
    USLOSS_Console("pageIns:        %d\n", vmStats.pageIns);
#endif

    return status;
}

static int start5_trampoline(char *arg)
{
    if (USLOSS_PsrSet(USLOSS_PSR_CURRENT_INT) != USLOSS_DEV_OK)
    {
        USLOSS_Console("ERROR: Could not disable kernel mode.\n");
        USLOSS_Halt(1);
    }

    int rc = start5(arg);

    Terminate(rc);
    return 0;    // Terminate() should never return
}



void finish(int argc, char **argv)
{
    USLOSS_Console("%s(): The simulation is now terminating.\n", __func__);
}

void test_setup  (int argc, char **argv) {}
void test_cleanup(int argc, char **argv) {}

