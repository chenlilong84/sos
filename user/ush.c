/**
 * User Shell
 */
#include "format.h"
#include "inet.h"
#include "string.h"
#include "sys/socket.h"
#include "syscall.h"

static char input[256];
static char *tokens[16];
static int argc;
static char data[2048]; /* socket data, should get dynamically alloced */

/*
 * Shell commands section. Each command is represented by a struct cmd, and
 * should have an implementation below, followed by an entry in the cmds array.
 */
struct cmd {
	char *name;
	char *help;
	int (*func)(int argc, char **argv);
};

static int echo(int argc, char **argv)
{
	for (unsigned int i = 0; argv[i]; i++)
		printf("Arg[%u]: \"%s\"\n", i, argv[i]);
	return 0;
}

static int cmd_exit(int argc, char **argv)
{
	exit(0);
	return 0;
}

static int cmd_run(int argc, char **argv)
{
	int rv;
	if (argc != 2) {
		puts("usage: run PROCNAME\n");
		return -1;
	}

	if ((rv = runproc(argv[1], RUNPROC_F_WAIT)) != 0)
		printf("failed: rv=0x%x\n", rv);
	return rv;
}

static int cmd_runp(int argc, char **argv)
{
	int rv;
	if (argc != 2) {
		puts("usage: run& PROCNAME\n");
		return -1;
	}

	if ((rv = runproc(argv[1], 0)) != 0)
		printf("failed: rv=0x%x\n", rv);
	return rv;
}

static int cmd_socket(int argc, char **argv)
{
	int rv;

	rv = socket(AF_INET, SOCK_DGRAM, 0);
	printf("socket() = %d\n", rv);
	return rv;
}

static int cmd_bind(int argc, char **argv)
{
	int rv, sockfd;
	struct sockaddr_in addr;

	if (argc != 4) {
		puts("usage: bind FD IP_AS_INT PORT\n");
		return -1;
	}

	sockfd = atoi(argv[1]);
	rv = inet_aton(argv[2], &addr.sin_addr.s_addr);
	if (rv != 1) {
		puts("error: bad IP\n");
		return -1;
	}
	addr.sin_port = atoi(argv[3]);
	rv = bind(sockfd, &addr, sizeof(struct sockaddr_in));
	printf("bind() = %d\n", rv);
	return rv;
}

static int cmd_connect(int argc, char **argv)
{
	int rv, sockfd;
	struct sockaddr_in addr;

	if (argc != 4) {
		puts("usage: connect FD IP_AS_INT PORT\n");
		return -1;
	}

	sockfd = atoi(argv[1]);
	rv = inet_aton(argv[2], &addr.sin_addr.s_addr);
	if (rv != 1) {
		puts("error: bad IP\n");
		return -1;
	}
	addr.sin_port = htons(atoi(argv[3]));
	rv = connect(sockfd, &addr, sizeof(struct sockaddr_in));
	printf("connect() = %d\n", rv);
	return rv;
}

static int cmd_send(int argc, char **argv)
{
	int rv, sockfd;

	if (argc != 3) {
		puts("usage: send FD STRING\n");
		return -1;
	}

	sockfd = atoi(argv[1]);
	rv = send(sockfd, argv[2], strlen(argv[2]) + 1, 0);
	printf("send() = %d\n", rv);
	return rv;
}

static int cmd_recv(int argc, char **argv)
{
	int rv, sockfd;

	if (argc != 2) {
		puts("usage: recv FD\n");
		return -1;
	}

	sockfd = atoi(argv[1]);
	rv = recv(sockfd, data, sizeof(data), 0);
	printf("recv() = %d\n", rv);
	data[rv] = '\0'; /* just in case */
	printf(" -> \"%s\"\n", data);
	return rv;
}

static int cmd_demo(int argc, char **argv)
{
	int i, count = 10;
	if (argc == 2)
		count = atoi(argv[1]);
	for (i = 0; i < count; i++)
		runproc("hello", 0);
	puts("Launched all processes!\n");
	return 0;
}

static int help(int argc, char **argv);
struct cmd cmds[] = {
	{ .name = "echo",
	  .func = echo,
	  .help = "print each arg, useful for debugging" },
	{ .name = "help", .func = help, .help = "show this help message" },
	{ .name = "run", .func = cmd_run, .help = "run a process" },
	{ .name = "run&",
	  .func = cmd_runp,
	  .help = "run a process without waiting for it to finish" },
	{ .name = "demo", .func = cmd_demo, .help = "run many processes" },
	{ .name = "socket", .func = cmd_socket, .help = "create socket" },
	{ .name = "bind", .func = cmd_bind, .help = "bind socket" },
	{ .name = "connect", .func = cmd_connect, .help = "connect socket" },
	{ .name = "send", .func = cmd_send, .help = "send data on socket" },
	{ .name = "recv", .func = cmd_recv, .help = "recv data from socket" },
	{ .name = "exit", .func = cmd_exit, .help = "exit this process" },
};
/*
 * help() goes after the cmds array so it can print out a listing
 */
static int help(int argc, char **argv)
{
	for (unsigned int i = 0; i < nelem(cmds); i++)
		printf("%s:\t%s\n", cmds[i].name, cmds[i].help);
	return 0;
}

/*
 * Parsing logic
 */
static void getline(void)
{
	unsigned int i = 0;

	puts("ush> ");

	do {
		input[i++] = getchar();
	} while (input[i - 1] != '\n' && input[i - 1] != '\r' &&
	         i < sizeof(input));
	input[i - 1] = '\0';
}

static void tokenize(void)
{
	unsigned int start = 0, tok = 0, i;
	for (i = 0; input[i]; i++) {
		if (input[i] == ' ' || input[i] == '\t' || input[i] == '\r' ||
		    input[i] == '\n') {
			if (i != start) {
				/* only complete a token if non-empty */
				tokens[tok++] = &input[start];
				input[i] = '\0';
			}
			start = i + 1;
		}
	}
	if (i != start) {
		tokens[tok++] = &input[start];
	}
	tokens[tok] = NULL;
	argc = tok;
}

static void execute(void)
{
	if (!tokens[0])
		return; /* avoid NULL dereference */
	for (unsigned int i = 0; i < nelem(cmds); i++) {
		if (strcmp(tokens[0], cmds[i].name) == 0) {
			cmds[i].func(argc, tokens);
			return;
		}
	}
	printf("command not found: \"%s\"\n", tokens[0]);
}

int main(void)
{
	int pid = getpid();
	printf("\nStephen's OS (user shell, pid=%u)\n", pid);
	puts("  This shell doesn't do much. Use the \"exit\" command to drop\n"
	     "  into a kernel shell which can do scary things!\n\n");
	while (true) {
		getline();
		tokenize();
		execute();
	}
	return 0;
}
