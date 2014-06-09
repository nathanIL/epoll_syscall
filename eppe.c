#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#define MAX_EP_EVENTS     64
#define PIPE_EVENT_BUFFER 1024
#define NUM_PIPES         16
#define PIPE_NAME_TMPL    "fifo-%d"

int init_epoll_ctx() {
	int epfd = epoll_create1(0);

	if ( -1 == epfd ) {
		perror("Could not create epoll fd");
		exit(EXIT_FAILURE);
	}
	return epfd;
}

void xprintf(char* fmt, ...) {
    char* pmsg;
	va_list args;

	asprintf(&pmsg,"[%d]: %s", getpid(), fmt);
    va_start(args,pmsg);
    vprintf(pmsg,args);
    va_end(args);
    free(pmsg);
}

/* As we deal only with FIFO's / Pipes here */
int is_fifo(const char* file) {
	struct stat fstat;

	if ( stat(file,&fstat) < 0 ) {
		perror("Could not get file stat");
		exit(EXIT_FAILURE);
	}
	return S_ISFIFO(fstat.st_mode);
}


void add_epoll_fds(int epfd) {
	struct epoll_event epevent;
	char* pipe;
	int fd;

	for (int i=0; i<NUM_PIPES; i++) {
		asprintf(&pipe,PIPE_NAME_TMPL,i);
		if ( (fd = open(pipe,O_RDONLY | O_NONBLOCK)) == -1 ) {
		     perror("Could not open");
		     free(pipe);
		     continue;
		}
		free(pipe);
		epevent.data.fd  = fd;
		epevent.events   = EPOLLIN;
		if ( epoll_ctl(epfd,EPOLL_CTL_ADD,fd,&epevent) != 0 ) {
			perror("Could not add fd to epfd");
			continue;
		}
	}
}

void echo_pipe_data(int fd) {
     char buffer[PIPE_EVENT_BUFFER];

     memset(buffer,0, sizeof(char) * PIPE_EVENT_BUFFER);
     if ( read(fd,buffer, sizeof(char) * PIPE_EVENT_BUFFER) == -1 ) {
    	 perror("Could not read");
         return;
     }
     printf("%s",buffer);
}

void wait_for_events(int epfd) {
	struct epoll_event epevent[MAX_EP_EVENTS];
	int rfds;

	while(1) {
		rfds = epoll_wait(epfd,epevent,MAX_EP_EVENTS,-1);
		if ( -1 == rfds ) {
			perror("Got error:");
		    exit(EXIT_FAILURE);
		}
		for (int i=0; i<rfds; i++) {
			if (epevent[i].events & EPOLLHUP) {
				xprintf("SIGHUP [fd: %d]\n",epevent[i].data.fd);
				epoll_ctl(epfd,EPOLL_CTL_DEL,epevent[i].data.fd,NULL);
			} else if(epevent[i].events & EPOLLIN) {
				echo_pipe_data(epevent[i].data.fd);
			}
		}
	}
}

void write_to_fifos() {
    int fds[NUM_PIPES];
    int hasWriters = NUM_PIPES;
    int rfd;
    char* pipe;
    char* pmsg;

    memset(fds,0,sizeof(int) * NUM_PIPES);
	for(int i=0; i<NUM_PIPES; i++) {
		asprintf(&pipe,PIPE_NAME_TMPL,i);
		unlink(pipe); // remove if exists
        if ( (mkfifo(pipe,S_IWUSR | S_IRUSR) ) < 0 ) {
        	perror("Could not create a pipe");
        	free(pipe);
        	continue;
        }
        free(pipe);
	}
	for (int i=0;  i<NUM_PIPES; i++) {
		asprintf(&pipe,PIPE_NAME_TMPL,i);
        if ( (fds[i] = open(pipe,O_WRONLY ) ) < 0 ) {
        	perror("Could not open file");
        }
        free(pipe);
	}

	while (hasWriters > 0) {
        rfd = fds[ rand() % NUM_PIPES ];
        if ( -1 == rfd || 0 == rfd ) {
        	hasWriters--;
        	continue;
        }
        asprintf(&pmsg,"%s from fd %i\n", ( rand() % 2 == 0 ) ?
        		       "Hello Miss:" :
        		       "Hello Sir:", rfd );
        write(rfd,pmsg, (size_t) strlen(pmsg) );
        free(pmsg);
		sleep(1);
	}
}


int main(int argc, char** argv) {
   int epfd;
   int pid;

   /* Lets spawn a new sub-process that will create the pipes and write to them */
   // Parent
   if ( (pid = fork()) > 0 ) {
	   sleep(1);
	   epfd = init_epoll_ctx();
	   add_epoll_fds(epfd);
	   wait_for_events(epfd);
	   exit(EXIT_SUCCESS);
   // Child
   } else if (pid == 0) {
	  srand( time(NULL) ^ getpid() << 16 );
	  write_to_fifos();
	  exit(EXIT_SUCCESS);
   } else {
	   perror("Could not spawn a process");
	   exit(EXIT_FAILURE);
   }

}
