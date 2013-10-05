#include "dsh.h"

void seize_tty(pid_t callingprocess_pgid); /* Grab control of the terminal for the calling process pgid.  */
void continue_job(job_t *j); /* resume a stopped job */
void spawn_job(job_t *j, bool fg); /* spawn a new job */



/* Sets the process group id for a given job and process */
int set_child_pgid(job_t *j, process_t *p)
{
    if (j->pgid < 0) /* first child: use its pid for job pgid */
        j->pgid = p->pid;
    return(setpgid(p->pid,j->pgid));
}

/* Creates the context for a new child by setting the pid, pgid and tcsetpgrp */
void new_child(job_t *j, process_t *p, bool fg)
{
         /* establish a new process group, and put the child in
          * foreground if requested
          */

         /* Put the process into the process group and give the process
          * group the terminal, if appropriate.  This has to be done both by
          * the dsh and in the individual child processes because of
          * potential race conditions.  
          * */

/*dsh did it in spawn_job */

         p->pid = getpid();

         /* also establish child process group in child to avoid race (if parent has not done it yet). */
         set_child_pgid(j, p);

         if(fg) // if fg is set
		seize_tty(j->pgid); // assign the terminal

//NOTE: SEIZE_TTY IS IMPLEMENTED IN HELPER.C. 

         /* Set the handling for job control signals back to the default. */
         signal(SIGTTOU, SIG_DFL);
//SIGTTOU -STOP- TERMINAL OUTPUT FOR BG PROCESS
}

/* Spawning a process with job control. fg is true if the 
 * newly-created process is to be placed in the foreground. 
 * (This implicitly puts the calling process in the background, 
 * so watch out for tty I/O after doing this.) pgid is -1 to 
 * create a new job, in which case the returned pid is also the 
 * pgid of the new job.  Else pgid specifies an existing job's 
 * pgid: this feature is used to start the second or 
 * subsequent processes in a pipeline.
 * */

void spawn_job(job_t *j, bool fg) 
{

	pid_t pid;
	process_t *p;
	
	int fd[2];

	for(p = j->first_process; p; p = p->next) {

	  /* YOUR CODE HERE? */

	  /* Builtin commands are already taken care earlier */
	  
	  switch (pid = fork()) {

          case -1: /* fork failure */
            perror("fork");
            exit(EXIT_FAILURE);

          case 0: /* child process  */
            p->pid = getpid();	    
	//print status
            new_child(j, p, fg);
            
		
	//set up input and output
	//redirection - close(stdout) open(file) gets you a fd, read()/write(), dup2()
	//TODO: SEE ABOUT INPUT_FD AND OUTPUT_FD MACROS

	//piping - what about race?
	//pipe(fd);
	//dup2(fd[0],STDIN_FILENO); //close stdin, dup read of pipe to stdin

		execvp(p->argv[0],p->argv);
	//do some error checking
	    /* YOUR CODE HERE  Child-side code for new process. */

            perror("New child should have done an exec");
            exit(EXIT_FAILURE);  /* NOT REACHED */
            break;    /* NOT REACHED */

          default: /* parent - dsh */
            /* establish child process group */
            p->pid = pid;
            set_child_pgid(j, p);

//how exactly pipe with the execs?
	
            /* YOUR CODE HERE?  Parent-side code for new process.  */
          } //end switch


/* why is it within the loop? */
	    seize_tty(getpid()); // assign the terminal back to dsh

	} //end loop

//waitpid for status change?

} //end spawn_job

/* Sends SIGCONT signal to wake up the blocked job */
void continue_job(job_t *j) 
{
//TODO: FIGURE OUT WHY THERE'S A NEG SIGN BEFORE
     if(kill(-j->pgid, SIGCONT) < 0)
          perror("kill(SIGCONT)");
}


/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 * it immediately.  
 */
bool builtin_cmd(job_t *last_job, int argc, char **argv) 
{

	    /* check whether the cmd is a built in command
        */
//TODO: add in checking for *.c, will call gcc -o devil "*.c" using spawn_job, then ./devil

//strcmp returns 0 if equal

        if (!strcmp(argv[0], "quit")) {
            /* Your code here */
//delete current job (see helper.c)
//write to dsh.log
		fflush(stdout);
		printf("\n");
            exit(EXIT_SUCCESS);
	}
        else if (!strcmp("jobs", argv[0])) {
            /* Your code here */
//loop through jobs linked list, print job number (->PGID) and status (bg/fg), waitpid(WNOHANG)
            return true;
        }
	else if (!strcmp("cd", argv[0])) {
            /* Your code here */
        }
        else if (!strcmp("bg", argv[0])) {
            /* Your code here */
//1 argument required: job # to continue in bg
//call continue_job w/o seize_tty, change ->bg
        }
        else if (!strcmp("fg", argv[0])) {
            /* Your code here */
//if 1 argument, continues that job # in fg
//it seizes tty
//no arg continues last job stopped/suspended in jobs list
//jobs suspended with ctrl-z - implement in readcmdline loop (main)
        }
        return false;       /* not a builtin command */
}

/* Build prompt messaage */
char* promptmsg() 
{
	char prompt[20]; //bit arbitrary in length

	pid_t pid;
	pid = getpid();

	sprintf(prompt, "dsh - %d$", pid);
	
	return prompt;
}

int main() 
{

	init_dsh();
	DEBUG("Successfully initialized\n");

	// init dsh.log by open a file descriptor (global) - not a rerouted stderr

	while(1) {
        job_t *j = NULL;
		if(!(j = readcmdline(promptmsg()))) {
			if (feof(stdin)) { /* End of file (ctrl-d) */
			//TODO: REROUTE TO BUILTIN_CMD QUIT
				fflush(stdout);
				printf("\n");
				exit(EXIT_SUCCESS);
           		}
			continue; /* NOOP; user entered return or spaces with return */
		}

        /* Only for debugging purposes to show parser output; turn off in the
         * final code */
        if(PRINT_INFO) print_job(j);

        /* Your code goes here */
        /* You need to loop through job's process list since a command line can contain ;*/
        /* Check for built-in commands */
        /* If not built-in */
            /* If job j runs in foreground */
            /* spawn_job(j,true) */
            /* else */
            /* spawn_job(j,false) */


    }

//TODO: BATCH MODE
//TODO: LOGGING ALL ERRORS, WHEN CHILD PROCESSES EXIT AND EXIT STATUS

}
