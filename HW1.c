#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  

#define READ_END   0
#define WRITE_END  1
#define MAX_LINE 4096

#define RED   "\x1b[31m"
#define RESET "\x1b[0m"


static int my_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}
static char *my_strdup(const char *s) {
    size_t n = strlen(s);
    char *d = malloc(n + 1);
    if (!d) return NULL;
    for (size_t i = 0; i <= n; i++)  
        d[i] = s[i];
    return d;
}

static int substr_idx(const char *s, const char *p) {
    int n = strlen(s), m = strlen(p);
    for (int i = 0; i + m <= n; i++) {
        int k = 0;
        while (k < m && s[i + k] == p[k]) k++;
        if (k == m) return i;
    }
    return -1;
}

static void add_unique(char ***arr, int *cnt, const char *tok) {
    for (int i = 0; i < *cnt; i++) {
        if (my_strcmp((*arr)[i], tok) == 0)
            return;
    }
    char **tmp = realloc(*arr, (*cnt + 1) * sizeof(char *));
    if (!tmp) return;
    *arr = tmp;
    (*arr)[*cnt] = my_strdup(tok);
    (*cnt)++;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <cmd> [args...] <search_word>\n", argv[0]);
        return 1;
    }

    char *search = argv[argc - 1];
    int cmdc = argc - 2;

    // execvp용 인자 배열 생성
    char **cmd = malloc((cmdc + 1) * sizeof(char *));
    if (!cmd) { perror("malloc"); return 1; }
    for (int i = 0; i < cmdc; i++) {
        cmd[i] = argv[i + 1];
    }
    cmd[cmdc] = NULL;

    int fd[2];
    if (pipe(fd) < 0) { perror("pipe"); return 1; }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        close(fd[READ_END]);
        dup2(fd[WRITE_END], STDOUT_FILENO);
        close(fd[WRITE_END]);
        execvp(cmd[0], cmd);
        perror("execvp");
        exit(1);
    }

    close(fd[WRITE_END]);
    FILE *in = fdopen(fd[READ_END], "r");
    if (!in) { perror("fdopen"); return 1; }

    char  line[MAX_LINE];
    int   lineno = 1, total = 0;
    char **unique = NULL;
    int   uniq_cnt = 0;

    while (fgets(line, sizeof(line), in)) {
        int pos = substr_idx(line, search);
        if (pos >= 0) {
            printf("%d: ", lineno);
            int offset = 0;
            while ((pos = substr_idx(line + offset, search)) >= 0) {
                fwrite(line + offset, 1, pos, stdout);
                printf(RED "%s" RESET, search);
                offset += pos + strlen(search);
                total++;
            }
            fputs(line + offset, stdout);

            char *copy = my_strdup(line);
            char *tok = strtok(copy, " \t\n");
            while (tok) {
                if (substr_idx(tok, search) >= 0)
                    add_unique(&unique, &uniq_cnt, tok);
                tok = strtok(NULL, " \t\n");
            }
            free(copy);
        }
        lineno++;
    }

    fclose(in);
    wait(NULL);

    if (uniq_cnt > 0) {
        printf("\nUnique words matched (%d): ", uniq_cnt);
        for (int i = 0; i < uniq_cnt; i++) {
            printf("%s%s", unique[i], (i+1<uniq_cnt? ", " : ""));
            free(unique[i]);
        }
        printf("\n");
    }

    free(unique);
    free(cmd);
    return 0;
}
