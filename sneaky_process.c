#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>

int main(void){
	int pid = fork();
	char *cmd1[] = {"cp", "/etc/passwd", "/tmp/passwd", 0}; //'cp /etc/passwd /tmp/passwd' cmd

	switch (pid){
		case -1: 
			printf("Fork failed!\n");
			break;
		case 0:
			printf("This is the child\n");
			int pid2 = fork();

			if (pid2==0){

			//copy the /etc/passwd file to a new file /tmp/passwd
			//open the /etc/passwd file and print a new line to the end of the file that contains
			//sneakyuser:abc123:2000:2000:sneakyuser;/root:bash
			execvp(cmd1[0], cmd1);

			}
			else{
			//LOOP KEY INPUTS HERE AND QUIT WHEN "Q" IS RECEIVED
			char word[256];
			while(1){
				if(fgets(word, sizeof(word), stdin) != NULL){
					//printf("INPUT: %s\n", word);
					char * tmp = malloc(sizeof(word));
					strcpy(tmp, word);
					//printf("TMP: %s\n", tmp);
					if(strcmp(tmp, "q\n") == 0){
						//printf("Q FOUND Input: %s\n", tmp);
						free(tmp);
						exit(0);
					}
					int ret = system(tmp);
					free(tmp);
				}
			}
			}
		default:
			printf("This is the parent waiting for child\n");
			waitpid(pid, NULL, 0);
			printf("Child has returned\n");
	}
	return 0;
}
