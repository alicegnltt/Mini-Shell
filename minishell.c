/*******************************************************************************
 * Name        : minishell.c version 3.0 - no leaks
 * Author      : Alice Agnoletto
 ******************************************************************************/

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <ctype.h>
#include <sys/wait.h>
#include <errno.h>

#define BLUE "\x1b[34;1m"
#define DEFAULT "\x1b[0m"

volatile sig_atomic_t interrupted = 0;

void handle_sigint(int sig)
{
    interrupted = 1;
}

/* check if the directory is a number */
int is_digit_dir(struct dirent *dirp)
{
    char *ptr;
    for (ptr = dirp->d_name; *ptr; ptr++)
    {
        if (!isdigit(*ptr))
            return 0;
    }
    return 1;
}

/* get the path to the home dir */
const char *usr_home_dir()
{
    static char home[512];
    snprintf(home, sizeof(home) - 1, "%s", getpwuid(getuid())->pw_dir);
    // add null terminator
    home[sizeof(home) - 1] = '\0';

    return (const char *)home;
}

/* run the path through this to fix the path in case it includes ~ to refer to home dir */
char *get_actual_path(char *path)
{
    if (path == NULL || path[0] == '\0')
    {
        const char *home_dir = usr_home_dir();
        return strdup(home_dir);
    }
    if (path[0] == '~')
    {
        const char *home_dir = usr_home_dir();
        char *newpath = malloc(strlen(home_dir) + strlen(path));
        if (newpath)
        {
            // replace the ~ with the path to home instead
            strcpy(newpath, home_dir);
            // skip ~ char
            strcat(newpath, path + 1);
            return newpath;
        }
    }
    return strdup(path);
}

/*
    cd gets the path from the user input adn changes directory
*/
void newcd(char *input)
{
    char *path = get_actual_path(input);
    // change directive and check for error
    if (chdir(path) != 0)
    {
        fprintf(stderr, "Error: Cannot change directory to %s. %s.\n", path, strerror(errno));
    }
    free(path);
}

/* pwd gets the working directory and prints it */
void newpwd(char *input)
{
    char *pathname = getcwd(NULL, 0);
    if (pathname == NULL)
    {
        fprintf(stderr, "Error: Cannot get current working directory. %s.\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    printf("%s\n", pathname);
    free(pathname);
}

/* ls function */
void newlf(char *input)
{
    DIR *dir;
    struct dirent *dirp;

    char *pathname = getcwd(NULL, 0);

    // from textbook
    dir = opendir(pathname);
    if (dir == NULL)
    {
        fprintf(stderr, "Cannot open %s\n", pathname);
        free(pathname);
        return;
    }

    /* Use readdir in a loop until it returns NULL */
    while ((dirp = readdir(dir)) != NULL)
    {
        // print only if it's not . or ..
        if ((strcmp(dirp->d_name, "..") != 0) && (strcmp(dirp->d_name, ".") != 0))
        {
            printf("%s\n", dirp->d_name);
        }
    }
    closedir(dir);
    free(pathname);
}

/*
    list all the current processes in the system in the format: <PID> <USER> <COMMAND>
*/
void newlp()
{
    DIR *procdir;
    struct dirent *dirp;
    struct passwd *p;
    uid_t uid = getuid();
    ;
    char path[1024];

    // Open /proc directory.
    procdir = opendir("/proc");
    if (!procdir)
    {
        fprintf(stderr, "opendir failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    while ((dirp = readdir(procdir)) != NULL)
    {
        if (dirp->d_type == DT_DIR && is_digit_dir(dirp))
        {
            if ((p = getpwuid(uid)) == NULL)
            {
                fprintf(stderr, "getpwuid() error: %s\n", strerror(errno));
                continue;
            }
            else
            {
                snprintf(path, sizeof(path), "/proc/%s/cmdline", dirp->d_name);
                FILE *cmdfile = fopen(path, "r");
                if (!cmdfile)
                {
                    fprintf(stderr, "Failed to open cmdline file: %s\n", strerror(errno));

                    continue;
                }
                // read command file
                size_t size = fread(path, 1, sizeof(path) - 1, cmdfile);
                fclose(cmdfile);
                if (size > 0)
                {
                    // path[size - 1] = '\0';
                    for (int i = 0; i < size - 1; ++i)
                    {
                        if (path[i] == '\0')
                            path[i] = ' ';
                    }
                    // null terminate
                    path[size - 1] = '\0';
                    // print usong the format <PID> <USER> <COMMAND>
                    printf("%s %s %s\n", dirp->d_name, p->pw_name, path);
                }
                else
                {
                    // Dont print cmd part if it was unable to be read
                    printf("%s %s \n", dirp->d_name, p->pw_name);
                }
            }
        }
    }
    closedir(procdir);
}

/* execute any other command. Use fork and exec */
void forkother(char *input)
{
    pid_t pid;
    int status;
    char *args[1024];
    int i = 0;
    char *inputcopy = strdup(input);

    // split the command into pieces separated by space
    args[i] = strtok(input, " ");
    while (args[i] != NULL)
    {
        i++;
        args[i] = strtok(NULL, " ");
    }
    args[i] = NULL;

    // fork
    pid = fork();

    if (pid < 0)
    {
        fprintf(stderr, "Error: fork() failed. %s.\n", strerror(errno));
        free(inputcopy);
        return;
    }
    else if (pid == 0)
    {
        /* child */
        // execute the command which is the first element in the list
        if (execvp(args[0], args) == -1)
        {
            fprintf(stderr, "Error: exec() failed. %s.\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        // Parent process
        do
        {
            if (waitpid(pid, &status, 0) == -1)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                else
                {
                    fprintf(stderr, "Error: wait() failed. %s.\n", strerror(errno));
                    break;
                }
            }
            else
            {
                break;
            }
        } while (true);
    }
    free(inputcopy);
}

int main(int argc, char *argv[])
{
    char *temp = NULL;
    char delim[] = " ";
    char *ptr;

    char input[1024];
    char *inputcopy;

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_sigint;
    sigaction(SIGINT, &action, NULL);

    char *command;

    if (argc != 1)
    {
        fprintf(stderr, "Error: one arg expected. Found: %d\n", argc);
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        if (interrupted)
        {
            printf("\n");
            interrupted = 0;
            continue;
        }
        // new shell (get the cwd first)
        temp = getcwd(NULL, 0);
        if (temp != NULL)
        {
            printf("%s[%s]> %s", BLUE, temp, DEFAULT);
            free(temp);
            temp = NULL;
        }
        else
        {
            fprintf(stderr, "getcwd failed: %s\n", strerror(errno));

        }

        // get the user input
        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            // check for signal interruption
            if (interrupted)
            {
                // reset to 0
                printf("\n");
                interrupted = 0;
                clearerr(stdin);
                continue;
            }
            else
            {
                break;
            }
        }

        // Remove newline character if present
        input[strcspn(input, "\n")] = 0;

        // Check if input is empty after removing newline
        if (input[0] == '\0')
        {
            temp = NULL;
            continue;
        }

        // input holds the whole string of words that the user inputted
        // command holds the first piece (febore the first space) which holds the command
        inputcopy = strdup(input);
        // REMOVE LATER BELOW
        if (!inputcopy)
        {
            fprintf(stderr, "Memory allocation failed for input copy: %s\n", strerror(errno));
            continue;
        }

        command = strtok(inputcopy, delim);

        // exit the shell
        if ((strcmp(input, "exit") == 0))
        {
            free(inputcopy);
            break;
        }
        else if ((strcmp(command, "cd") == 0))
        {
            // get what's after the command
            ptr = strtok(input, delim);
            ptr = strtok(NULL, delim);
            // if there is anything else after the 1st extra arg, error
            if (strtok(NULL, delim) != NULL)
            {
                fprintf(stderr, "Error: Too many arguments to cd.\n");
            }
            else
            {
                newcd(ptr);
            }
        }
        else if ((strcmp(command, "pwd") == 0))
        {
            newpwd(input);
        }
        else if ((strcmp(command, "lf") == 0))
        {
            newlf(input);
        }
        else if ((strcmp(command, "lp") == 0))
        {
            newlp();
        }
        else
        {
            // if the user inputs any command that was not implemented
            forkother(input);
        }
        free(inputcopy);
    }
    exit(EXIT_SUCCESS);
}