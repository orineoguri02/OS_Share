
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define READ_END  0
#define WRITE_END 1
#define MAX_LINE 256

int find_substr(const char *str, const char *sub) {
    int n = strlen(str);
    int m = strlen(sub);
    for (int i = 0; i <= n - m; i++) {
        int k = 0;
        while (k < m && str[i + k] == sub[k]) {
            k++;
        }
        if (k == m) {
            return i;
        }
    }
    return -1;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <command> <search_word>\n", argv[0]);
        return 1;
    }
    char *cmd = argv[1];
    char *word = argv[2];

    int fd[2];
    if (pipe(fd) == -1) {
        perror("pipe 실패");
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork 실패");
        return 1;
    }

    if (pid == 0) {
        close(fd[READ_END]);               
        dup2(fd[WRITE_END], STDOUT_FILENO);
        close(fd[WRITE_END]);             

        execlp(cmd, cmd, NULL);
        perror("exec 실패");
        exit(1);
    } else {
 
        close(fd[WRITE_END]);             
        FILE *stream = fdopen(fd[READ_END], "r");
        if (!stream) {
            perror("fdopen 실패");
            return 1;
        }

        char line[MAX_LINE];
        int lineno = 1;
        int match_count = 0;

        while (fgets(line, MAX_LINE, stream)) {
            int pos = find_substr(line, word);
            if (pos >= 0) {
               
                printf("%d: ", lineno);
                int i = 0;
                while ((pos = find_substr(&line[i], word)) >= 0) {
                 
                    fwrite(&line[i], 1, pos, stdout);
                   
                    printf("\033[31m%s\033[0m", word);
                    i += pos + strlen(word);
                    match_count++;
                }
           
                fputs(&line[i], stdout);
            }
            lineno++;
        }

        fclose(stream);
   
        wait(NULL);
        printf("\nTotal matched lines: %d\n", match_count);
    }

    return 0;
}
