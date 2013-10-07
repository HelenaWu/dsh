#include "dsh.h"

void seize_tty(pid_t callingprocess_pgid); /* Grab control of the terminal for the calling process pgid.  */
void continue_job(job_t *j); /* resume a stopped job */
void spawn_job(job_t *j, bool fg); /* spawn a new job */
void setifile(process_t *p); /* opens file and dups to process stdin */
void setofile(process_t *p); /* opens file and dups to process stdout */


bool LOG_TO_FILE=true;
job_t * job_list; /* keeps track of running/stopped jobs*/
job_t * job_list; 
char init_dir[MAX_LEN_FILENAME] = "~";
char * PWD = init_dir;
FILE * LOG = NULL; /* for dsh.log */

//redirect input (<)
void setifile(process_t *p)
{
  int fd;
  if (p->ifile)
    {
      if((fd=open(p->ifile,O_RDONLY))<0)
	{
	  //fprintf(LOG, "failed to open ifile %s\n",p->ifile);
	  perror("failed to open ifile");
	  exit(EXIT_FAILURE);
	}
      close(0);
      if((dup2(fd,0)<0)){
	perror("Error");
	exit(EXIT_FAILURE);
      }
      close(fd);	
    }
}

//redirect output (>)
void setofile(process_t *p)
{
  int fd;
  if (p->ofile)
    {
      if((fd=open(p->ofile,(O_RDWR|O_CREAT),(S_IRWXU|S_IRWXG|S_IROTH)))<0)
	{
	  //fprintf(LOG,"failed to open ofile %s\n",p->ofile);
	  perror("failed to open ofile");
	  exit(EXIT_FAILURE);
	}
      close(1);
      if((dup2(fd,1)<0)){
	perror("Error");
	exit(EXIT_FAILURE);
      }
      close(fd);
    }
}

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
  int fdp[2]; //file descriptors for pipe
  int fdprev_read;
  
  fdp[0]=STDIN_FILENO;
  fdp[1]=STDOUT_FILENO;
  
  fdprev_read=fdp[0];

  for(p = j->first_process; p; p = p->next) {
    
    /* Builtin commands are already taken care earlier */
    pipe(fdp);
    
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
      
      if (p->next)
	{
	  if(p==j->first_process) //first process
	    {
	      //fprintf(LOG,"setting up first process %s\n",p->argv[0]);
	      setifile(p);
	      close(1);
	      dup2(fdp[1],1); //stdout end
	      close(fdp[1]);
	      close(fdp[0]); 
	      if(p->ofile) fprintf(stderr,"first process in pipeline not allowed output file\n");
	      
	    }
	  else { //middle process
	    //fprintf(LOG,"setting up mid process %s\n",p->argv[0]);
	    
	    close(0);
	    close(1);
	    
	    dup2(fdprev_read,0);
	    dup2(fdp[1],1);

	    close(fdprev_read);
	    close(fdp[1]);
	    close(fdp[0]);
	    if (p->ifile || p->ofile) fprintf(stderr,"process in middle of pipeline not allowed input or output file\n");
	    
	  }
	  
	}
      else if(fdprev_read!=STDIN_FILENO) //last process
	{
	  //fprintf(LOG,"last process %s\n",p->argv[0]);
	  setofile(p);
	  close(0);
	  dup2(fdprev_read,0);
	  close(fdprev_read);
	  close(fdp[0]);
	  close(fdp[1]);
	  if (p->ifile) fprintf(stderr,"tail of pipeline not allowed input file\n"); 
	}
      
      else { //freestanding process
	//fprintf(LOG,"free proc %s\n",p->argv[0]);
	
	close(fdp[0]);
	close(fdp[1]);
	setifile(p);
	setofile(p);
      }

      execvp(p->argv[0],p->argv); //TODO: change to execvP for cd purposes?


      //an error occurred in execvp
      perror("execvp: ");
      exit(EXIT_FAILURE);  /* NOT REACHED */
      break;    /* NOT REACHED */

    default: /* parent */
      /* establish child process group */
      p->pid = pid;
      set_child_pgid(j, p);
      
      if(fdprev_read!=STDIN_FILENO)
	{
	  close(fdprev_read);
	  fdprev_read=fdp[0];
	}
      else if (!p->next)
	{
	  close(fdp[0]);
	} else {
	fdprev_read=fdp[0];
      }
      
      close(fdp[1]);
    } //end switch

    seize_tty(getpid()); //assign the terminal back to dsh

  } //end loop

}

/* Sends SIGCONT signal to wake up the blocked job */
void continue_job(job_t *j) 
{
  if(kill(-j->pgid, SIGCONT) < 0)
    {
      perror("kill(SIGCONT)");
    }
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
    fprintf(stderr,"quitting %d",getpid());
    fflush(stdout);
    printf("\n");
    exit(EXIT_SUCCESS);
  }
  else if (!strcmp("jobs", argv[0])) {
    print_job(job_list->next); //skip past the empty header
    return true;
  }
  else if (!strcmp("cd", argv[0])) {
    char * path = argv[1];
    strcpy(PWD,path);

    if(chdir(path)<0)
      {
	perror("failed cd");
	exit(EXIT_FAILURE);
      }
    char buf[50];
    getcwd(buf,50);
    printf("changed to path: %s\n",buf);
    return true; 
  }
  else if (!strcmp("bg", argv[0])) {
    /* Your code here */
    //To-do

    return true;
  }
  else if (!strcmp("fg", argv[0])) {
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
	last_suspended_j->bg=false;
	//fprintf(LOG,"fg pgid: %ld\n",(long)last_suspended_j->pgid);
	      
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
	fprintf(stderr, "In fg: pgid: %ld doesn't exist\n",(long)pgid);
	exit(1);
      }
      continue_job(j);
      j->bg=false;

      fprintf(stderr,"Bringing job %d to foreground\n",j->pgid);
      seize_tty(pgid);
      return true;
    }
  }
  else if (endswith(argv[0],".c")){
    //hard code in two new jobs - a gcc and call to new executable
      job_t *nxt=last_job->next;
      job_t *compiler=malloc(sizeof(job_t));
      job_t *executer=malloc(sizeof(job_t));
      init_job(compiler);
      init_job(executer);
      last_job->next=compiler;
      compiler->next=executer;
      executer->next=nxt;

      char buf[MAX_LEN_CMDLINE];
      sprintf(buf,"gcc -o devil %s",argv[0]);
      strcpy(compiler->commandinfo,buf);
      
      compiler->first_process=malloc(sizeof(process_t));
      init_process(compiler->first_process);

      compiler->first_process->argc=4;
      compiler->first_process->argv[0]=(char *)calloc(MAX_LEN_CMDLINE,sizeof(char));
      strcpy(compiler->first_process->argv[0],"gcc");
      compiler->first_process->argv[1]=(char *)calloc(MAX_LEN_CMDLINE,sizeof(char));
      strcpy(compiler->first_process->argv[1],"-o");
      compiler->first_process->argv[2]=(char *)calloc(MAX_LEN_CMDLINE,sizeof(char));
      strcpy(compiler->first_process->argv[2],"devil");
      compiler->first_process->argv[3]=(char *)calloc(MAX_LEN_CMDLINE,sizeof(char));
      strcpy(compiler->first_process->argv[3],argv[0]);
      compiler->first_process->argv[4]=NULL;
      
      strcpy(executer->commandinfo,"./devil");

      executer->first_process=malloc(sizeof(process_t));
      init_process(executer->first_process);

      executer->first_process->argc=1;
      executer->first_process->argv[0]=(char *)calloc(MAX_LEN_CMDLINE,sizeof(char));
      strcpy(executer->first_process->argv[0],"./devil");
      executer->first_process->argv[1]=NULL;
      
      return true;
    }
    
  return false;       /* not a builtin command */
}
/* Build prompt message */
char* promptmsg() 
{
  static char prompt[MAX_LEN_CMDLINE]; //bit arbitrary in length

  pid_t pid;
  pid = getpid();
  
  sprintf(prompt, "dsh -:%d$:%s ", pid, PWD);
  return prompt;
}

int main() 
{

  init_dsh();
  DEBUG("Successfully initialized\n");
  if (LOG_TO_FILE)
    {
      int fd=open("dsh.log",(O_RDWR|O_CREAT),(S_IRWXU|S_IRWXG|S_IROTH));
      close(2);
      dup2(fd,2);
      close(fd);
      fprintf(stderr,"\n===\n BEGIN LOG OF DSH %d\n===\n",getpid());
    }

  while(1) {
    job_t *j = NULL;
    if(!(j = readcmdline(promptmsg()))) {
      if (feof(stdin)) { /* End of file (ctrl-d) */

    fprintf(stderr,"quitting %d\n",getpid());
    fflush(stdout);
    printf("\n");
    exit(EXIT_SUCCESS);
      }
      continue; /* NOOP; user entered return or spaces with return */
    }

    /* Only for debugging purposes to show parser output; turn off in the
     * final code */
    //    if(PRINT_INFO) print_job(j);


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

	//a built in will always be its own job ie a process group of 1
	if(!builtin_cmd(j,p->argc,p->argv)){
	      
	  spawn_job(j,!(j->bg));
	  //check job status

	  if (j->bg) { //do not wait for job to finish
	    //fprintf(LOG,"job %s:%d is in bg, curr:%d\n",p->argv[0],j->pgid,getpid());
	    
	    waitpid(-1,&p->status,WNOHANG);
	    seize_tty(getpid());
	  } else {
	    //fprintf(LOG,"job %s:%d is in fg, curr:%d\n",p->argv[0],j->pgid,getpid());
	    pid_t child;
	    //TODO: THE SUSPENDED JOB CASE
	    while ((child=waitpid(-j->pgid,&p->status,0))>0)
	    {
	      while(p)
		{
		  if(p->pid==child) {
		    p->completed=true;
		    break;
		  }
		  p=p->next;
		}
	    }
	    if(errno==ECHILD)
	      {
		printf("finished with job %d\n",j->pgid);
	      }
	    
	    seize_tty(getpid()); // assign the terminal back to dsh
	  }

	  //TODO: MOVE THESE TO OTHER AREA FOR DEALING WITH BG/FG CMDS?
	if (job_is_completed(j)) {
	    printf("%d of %d exited \n",p->pid,j->pgid);
	    //fprintf(LOG,"%d:%s exited \n",p->pid,p->argv[0]);
	    job_t * tmpj=j->next;
	    delete_job(j,job_list);
	    j=tmpj;
	    continue;
	  }
	  else if (WIFSTOPPED(p->status)) {
	    printf("stopped child of pgid %d\n",j->pgid);
	    //fprintf(LOG,"%d:%s was stopped\n",p->pid,p->argv[0]);
		  
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

    //close dsh.log
  //fprintf(LOG,"\n===\n END LOG\n===\n");
  //fclose(LOG);
  exit(1);
}
