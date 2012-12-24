#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>

/*
 * The following macros and the recvfd function are from plan9port
 * (http://code.swtch.com/plan9port/src/318b2fddcd9a/src/lib9/sendfd.c)
 * and are available under the following license:
 *
 * Copyright 2001-2007 Russ Cox.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef CMSG_ALIGN
#	ifdef __sun__
#		define CMSG_ALIGN _CMSG_DATA_ALIGN
#	else
#		define CMSG_ALIGN(len) (((len)+sizeof(long)-1) & ~(sizeof(long)-1))
#	endif
#endif

#ifndef CMSG_SPACE
#	define CMSG_SPACE(len) (CMSG_ALIGN(sizeof(struct cmsghdr))+CMSG_ALIGN(len))
#endif

#ifndef CMSG_LEN
#	define CMSG_LEN(len) (CMSG_ALIGN(sizeof(struct cmsghdr))+(len))
#endif

static int recvfd(int s) {
	int n;
	int fd;
	char buf[1];
	struct iovec iov;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	char cms[CMSG_SPACE(sizeof(int))];

	iov.iov_base = buf;
	iov.iov_len = 1;

	memset(&msg, 0, sizeof msg);
	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	msg.msg_control = (caddr_t)cms;
	msg.msg_controllen = sizeof cms;

	if((n=recvmsg(s, &msg, 0)) < 0)
		return -1;
	if(n == 0){
		return -2;
	}
	cmsg = CMSG_FIRSTHDR(&msg);
	memmove(&fd, CMSG_DATA(cmsg), sizeof(int));
	return fd;
}

int main(int argc, char * * argv) {
	char buf = 'a';
	int arg_count = 2;
	size_t read_count;
	ssize_t res;
	int stdin_write, stderr_read, stdout_read;
	int child_fd, master_fd = atoi(getenv("NIX_IMPURITY_SOCKET"));
	char * out = getenv("out");
	char stdout_buf[sizeof "impure-command.sharg1arg2"];
	struct stat st;

	stdout_buf[sizeof stdout_buf - 1] = 0;

	if (write(master_fd, &buf, sizeof buf) < 0) {
		perror("Writing master fd byte");
		return 1;
	}

	child_fd = recvfd(master_fd);
	if (child_fd < 0) {
		perror("Recieving child socket fd");
		return 1;
	}

	if (write(child_fd, "impure-command.sh", sizeof "impure-command.sh") < 0) {
		perror("Writing command");
		return 1;
	}
	if (write(child_fd, &arg_count, sizeof arg_count) < 0) {
		perror("Writing command");
		return 1;
	}
	if (write(child_fd, "arg1", sizeof "arg1") < 0) {
		perror("Writing command");
		return 1;
	}
	if (write(child_fd, "arg2", sizeof "arg2") < 0) {
		perror("Writing command");
		return 1;
	}

	stdin_write = recvfd(child_fd);
	stdout_read = recvfd(child_fd);
	stderr_read = recvfd(child_fd);
	if (stdin_write < 0 || stdout_read < 0 || stderr_read < 0) {
		perror("Recieving stdio pipes");
		return 1;
	}

	if (write(stdin_write, &buf, sizeof buf) < 0) {
		perror("Writing to stdin");
		return 1;
	}
	if (read(stderr_read, &buf, sizeof buf) < 0) {
		perror("Reading from stderr");
		return 1;
	}
	assert(buf == 'a');

	while (read_count < sizeof stdout_buf - 1) {
		res = read(stdout_read, stdout_buf + read_count, sizeof stdout_buf - read_count);
		if (res == 0) {
			fprintf(stderr, "Unexpected EOF");
			return 1;
		} else if (res < 0) {
			perror("Reading stdout");
			return 1;
		}
		read_count += res;
	}

	assert(strcmp(stdout_buf, "impure-command.sharg1arg2") == 0);

	assert(stat("./temp-file", &st) == 0);
	assert(stat(out, &st) == 0);
	assert(S_ISREG(st.st_mode));

	res = read(child_fd, &buf, sizeof buf);
	if (res == 0) {
		fprintf(stderr, "Unexpected EOF");
		return 1;
	} else if (res < 0) {
		perror("Reading exit status");
		return 1;
	}
	assert(buf == 0);
	assert(read(child_fd, &buf, sizeof buf) == 0);

	return 0;
}
