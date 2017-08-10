#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <time.h>
#include <dirent.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define INFTIM -1
#define MAX 0x10000
#define BUF_SIZE 0x10000
#define DBG fprintf(stderr, "%s:%d\n", __FUNCTION__,__LINE__)
#define DUMP_FDS {fprintf(stderr, "%s(%d)", __FUNCTION__, fd); dump(); fprintf(stderr, "\n");}

void create_new_compress_thread(void);

struct private_data{
    int tx_count;
    int fd;
};
typedef void (*handle) (struct pollfd*, struct private_data*);

int front[2];
int back[2];
int gzip_fd;
int exiting = 0;

struct pollfd fd_list[100];
handle handles[100];
struct private_data pdata[100];
int fd_count=0;

void dump(void)
{
    int i;
    fprintf(stderr, "{");
    for(i=0;i<fd_count;i++) {
        fprintf(stderr, "%d, ", fd_list[i].fd);
    }
    fprintf(stderr, "|%d, ", fd_count);
    fprintf(stderr, "}");
}

void insert_fd(int fd, handle h, struct private_data* data)
{
    //TODO: should check duplicate
    //DUMP_FDS;
    handles[fd_count] = h;
    fd_list[fd_count].fd=fd;
    fd_list[fd_count].events=POLLIN;
    fd_list[fd_count].revents=0;
    pdata[fd_count]=*data;
    fd_count++;
    //DUMP_FDS;
}

void erase_fd(int fd)
{
    int i;
    int j;
    //DUMP_FDS;
    for(i=0;i<fd_count;i++) {
        if(fd_list[i].fd == fd) {
            for(j=i;j<fd_count-1;j++) {
                fd_list[j]=fd_list[j+1];
                pdata[j]=pdata[j+1];
                handles[j]=handles[j+1];
            }
            fd_count--;
            break;
        }
    }
    //DUMP_FDS;
}

void my_poll(void)
{
    poll(fd_list, fd_count, -1);
    int i;
    for(i=0;i<fd_count;i++) {
        if(fd_list[i].revents != 0) {
            //fprintf(stderr,"handle fd=%d\n", fd_list[i].fd);
            handles[i](fd_list+i, pdata+i);
        }
    }
}

void process_input(struct pollfd * fd, struct private_data * d)
{
    char s[ BUF_SIZE ];
    int len;
    if(fd->revents & (POLLIN|POLLERR|POLLHUP)) {
        len=read(fd->fd, s, BUF_SIZE);
        //fprintf(stderr, "%s:%d, len=%d\n", __FUNCTION__, __LINE__, len);
        if(len==0) { //Finished?
            exiting=1;
            erase_fd(fd->fd);
            close(gzip_fd);
            gzip_fd=-1;
        }
        write(gzip_fd, s, len);
    }
}

void process_output(struct pollfd * fd, struct private_data * d)
{
    char s[BUF_SIZE];
    int len;
    if(fd->revents & (POLLIN|POLLERR|POLLHUP))
    {
        len=read(fd->fd, s, BUF_SIZE);
        if(len==0) {
            close(fd->fd);
            close(d->fd);
            erase_fd(fd->fd);
            if(exiting) exit(0);
        }
        fprintf(stderr, "write %d len=%d\n", d->fd, len);
        write(d->fd, s, len);
        if(d->tx_count>=0){
            d->tx_count+=len;
        }
        if(d->tx_count>MAX) {
            close(gzip_fd);
            d->tx_count=-1;
            create_new_compress_thread();
        }
    }
}

int open_newlog(void)
{ //TODO: Create new file here
    static int nu=0;
    char name[1000];
    sprintf(name, "%02d.gz", nu);
    int fd=open(name, O_CREAT|O_RDWR, 0644);
    fprintf(stderr, "create file %s:%d\n", name,fd);
    nu++;
    return fd;
}

void create_new_compress_thread(void)
{
    int front[2];
    int back[2];
    pipe(front);
    //fprintf(stderr, "creat front pipe[%d,%d]\n", front[0], front[1]);
    pipe(back);
    //fprintf(stderr, "creat back pipe[%d,%d]\n", back[0], back[1]);

    int pid=fork();
    if(pid>0) {
        close(front[0]);
        close(back[1]);
        //Save fd list
        gzip_fd = front[1];
        struct private_data d;
        d.fd=open_newlog();
        d.tx_count=0;
        insert_fd(back[0],process_output, &d);

    }
    else {
        close(front[1]);
        close(back[0]);
        dup2(front[0], STDIN_FILENO);
        dup2(back[1], STDOUT_FILENO);
        execl("/bin/gzip","gzip","-c", "-f",(char*)0);
    }
}

void main(void)
{
    struct private_data d;
    d.tx_count=0;
    d.fd=0;
    create_new_compress_thread();
    insert_fd(STDIN_FILENO, process_input, &d);
    while(1) {
        my_poll();
    }
}
