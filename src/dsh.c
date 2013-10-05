#include "dsh.h"

void seize_tty(pid_t callingprocess_pgid); /* Grab control of the terminal for the calling process pgid.  */
void continue_job(job_t *j); /* resume a stopped job */
void spawn_job(job_t *j, bool fg); /* spawn a new job */

job_t * job_list; /* keeps track of running/stopped jobs*/
job_t * job_list; 
char init_dir[] = "~";
char * PWD = init_dir;

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

         p->pid = getpid();

         /* also establish child process group in child to avoid race (if parent has not done it yet). */
         set_child_pgid(j, p);


         if(fg) // if fg is set
	   {
	     seize_tty(j->pgid); // assign the terminal
	   }
	 
         /* Set the handling for job control signals back to the default. */
         signal(SIGTTOU, SIG_DFL);
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
	int fd[2]={0,0}; //might need this for later io redirection

	for(p = j->first_process; p; p = p->next) {

	  /* YOUR CODE HERE? */
	  /* Builtin commands are already taken care earlier */
	  
	  switch (pid = fork()) {

          case -1: /* fork failure */
            perror("fork");
            exit(EXIT_FAILURE);

          case 0: /* child process  */
            p->pid = getpid();	    
            new_child(j, p, fg);
	    
	    //print args to terminal
	    fprintf(stdout,"\n%d(Launched): ",p->pid);
	    int i;
	    for(i=0;i<p->argc;i++)
	      {
		fprintf(stdout,"%s ",p->argv[i]);
	      }
	    fprintf(stdout,"\n");
	    //
	    
	    //TODO: REDIRECT STDIN AND STDOUT

	    //redirect input (<)
	    if (p->ifile)
	      {
		if((fd[0]=open(p->ifile,O_RDONLY))<0)
		  {
		    perror("failed to open ifile");
		    exit(EXIT_FAILURE);
		  }
		if((dup2(fd[0],0)<0)){
		    perror("failed ifile dup");
		    exit(EXIT_FAILURE);
		  }
	      }
	       //redirect input (<)
	    if (p->ofile)
	      {
		if((fd[1]=open(p->ifile,O_CREAT&O_RDWR))<0)
		  {
		    perror("failed to open ofile");
		    exit(EXIT_FAILURE);
		  }
		if((dup2(fd[1],1)<0)){
		    perror("failed ofile dup");
		    exit(EXIT_FAILURE);
		  }
	      }

	    
	    /*
	    if (p->next)
	      {
		
	    if (pipe(fd)==-1)
	      {
		perror("pipe");
		exit(EXIT_FAILURE);
	      }
	      }
	    */
	    
	    

	    execvp(p->argv[0],p->argv); //TODO: change to execvP for cd purposes

	    //an error occurred in execvp
            perror("execvp: ");
            exit(EXIT_FAILURE);  /* NOT REACHED */
            break;    /* NOT REACHED */

          default: /* parent */
            /* establish child process group */
            p->pid = pid;
            set_child_pgid(j, p);
	    int pipefd;
	    
	    
	    
	    
          } //end switch

	  seize_tty(getpid()); //assign the terminal back to dsh
	  
	} //end loop

}

/* Sends SIGCONT signal to wake up the blocked job */
void continue_job(job_t *j) 
{
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

        if (!strcmp(argv[0], "quit")) {
            /* Your code here */
	  
            exit(EXIT_SUCCESS);
	}
        else if (!strcmp("jobs", argv[0])) {
	  /* Your code here */
	  print_job(job_list->next);
	  return true;
        }
	else if (!strcmp("cd", argv[0])) {
            /* Your code here */
	  char * path = argv[1];
	  PWD = path;	  
	  return true; //can improve
        }
        else if (!strcmp("bg", argv[0])) {
            /* Your code here */
	  //To-do

	  return true;
        }
        else if (!strcmp("fg", argv[0])) {
            /* Your code here */
	  //Bring into foreground job given pgid
	  job_t * j;
	  if(argv[1] == NULL){
	    //find last suspended job
	    for(j=job_list; j; j= j->next){
	      job_t * last_suspended_j;
	      if(job_is_stopped(j)){
		last_suspended_j = j;
	      }
	      continue_job(last_suspended_j);
	      seize_tty(last_suspended_j->pgid);
	      return true;
	  }

	  /*if pgid is specified*/
	    pid_t pgid = atol(argv[1]);
	    for(j=job_list; j; j= j->next){
	      if(j->pgid == pgid){
		break;
	      }
	    }
	    if(j==NULL){
	      //log this error!
	      FILE * f = NULL;
	      f = fopen("dsh.log", "a+");
	      fprintf(f, "pgid: %ld doesn't exist\n",(long)pgid);
	      fclose(f);
	      exit(1);
	    }
	    continue_job(j);
	    seize_tty(pgid);
	    return true;
	  }
	}
        return false;       /* not a builtin command */
}
/* Build prompt messaage */
char* promptmsg() 
{
    /* Modify this to include pid */
  static  char prompt[MAX_LEN_CMDLINE]; //bit arbitrary in length

  pid_t pid;
  pid = getpid();
  
  sprintf(prompt, "dsh -:%d$:%s ", pid, PWD);
  return prompt;
}


int main() 
{

	init_dsh();
	DEBUG("Successfully initialized\n");

	while(1) {
	  job_t *j = NULL;
	  if(!(j = readcmdline(promptmsg()))) {
	    if (feof(stdin)) { /* End of file (ctrl-d) */
	      fflush(stdout);
	      printf("\n");
	      exit(EXIT_SUCCESS);
	    }
	    continue; /* NOOP; user entered return or spaces with return */
	  }

        /* Only for debugging purposes to show parser output; turn off in the
         * final code */
	          if(PRINT_INFO) print_job(j);


	/*add spawned job to job_list */
	if(job_list == NULL){
	  job_list = malloc(sizeof(job_t));
	  job_list->next=NULL;
	}
	else{
	  job_list = realloc(job_list, sizeof(job_t) + sizeof(job_list));
	
	}
	job_t *last_job  = find_last_job(job_list);
	last_job->next = j;

	while(j)
	  {
	    process_t *p=j->first_process;
	    // printf("\n ==== \njob: %s\n",j->commandinfo);
	    //a built in will always be its own job ie a process group of 1
	    if(!builtin_cmd(j,p->argc,p->argv)){
	      
		spawn_job(j,!(j->bg));
		
		//check job status
		if (j->bg) {
		    waitpid(-1,&p->status,WNOHANG);
		    printf("run job in bg\n");
		    
		    seize_tty(getpid()); //do not wait for job to finish
		} else {
		  waitpid(-1,&p->status,0);
		  //fprintf(stdout,"finished,returning to dsh %d\n",getpid());
		  seize_tty(getpid()); // assign the terminal back to dsh
		}

		//TODO: MOVE THESE TO OTHER AREA FOR DEALING WITH BG/FG CMDS?
		if (WIFEXITED(p->status)) {
		  //printf("exited properly\n");
		  p->completed=true;
		  job_t * tmpj=j->next;
		  delete_job(j,job_list);
		  j=tmpj;
		  continue;
		}
		else if (WIFSTOPPED(p->status)) {
		  //printf("stopped child of pgid %d\n",j->pgid);
		  j->notified=true;
		  p->stopped=true;
		}
		
	      } //end large if
	    else 
	      { //TODO: EXTRACT THIS INTO SEPARATE FXN
		job_t * tmpj=j->next;
		delete_job(j,job_list);
		j=tmpj;
		continue;
		
	      }
	    j=j->next;
	    
	  } //end while
	  
	} //end infinite while
}
