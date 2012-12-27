#include "util.hh"
#include "serialise.hh"

#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <errno.h>

/*
 * The following macros and the sendfd function are from plan9port
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

using namespace nix;

static int sendfd(int s, int fd) {
    char buf[1];
    struct iovec iov;
    struct msghdr msg;
    struct cmsghdr *cmsg;
    int n;
    char cms[CMSG_SPACE(sizeof(int))];

    buf[0] = 0;
    iov.iov_base = buf;
    iov.iov_len = 1;

    memset(&msg, 0, sizeof msg);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = (caddr_t)cms;
    msg.msg_controllen = CMSG_LEN(sizeof(int));

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    memmove(CMSG_DATA(cmsg), &fd, sizeof(int));

    if((n=sendmsg(s, &msg, 0)) != iov.iov_len)
        throw SysError("Sending fd");
    return 0;
}

static void sigChldHandler(int sigNo)
{
    /* Reap all dead children. */
    while (waitpid(-1, 0, WNOHANG) > 0) ;
}


static void setSigChldAction(bool autoReap)
{
    struct sigaction act, oact;
    act.sa_handler = autoReap ? sigChldHandler : SIG_DFL;
    sigfillset(&act.sa_mask);
    act.sa_flags = 0;
    if (sigaction(SIGCHLD, &act, &oact))
        throw SysError("setting SIGCHLD handler");
}

static Path drvFile;
static Path tmpDir;
static Path chrootDir;
static Path commandsDir;

static void processRequest(AutoCloseFD socket_fd) {
    unsigned char buf;
    Path name = commandsDir + '/';

    FdSource socket(socket_fd);

    while (socket.read(&buf, sizeof buf)) {
        if (!buf)
            break;

        if (buf == '/')
            name = commandsDir + '/';
        else
            name.push_back(buf);
    }

    std::vector<string> args(4);

    args[0] = name;
    args[1] = drvFile;
    args[2] = tmpDir;
    args[3] = chrootDir;

    try {
        while (socket.read(&buf, sizeof buf)) {
            string arg;

            do {
                if (!buf)
                    break;
                arg.push_back(buf);
            } while (socket.read(&buf, sizeof buf));

            args.push_back(arg);
        }
    } catch (EndOfFile e) {
    }

    Pipe in;
    Pipe out;
    Pipe err;

    in.create();
    out.create();
    err.create();

    sendfd(socket_fd, in.writeSide);
    sendfd(socket_fd, out.readSide);
    sendfd(socket_fd, err.readSide);

    Pipe error_comm;
    error_comm.create();

    Pid child;
    child = fork();

    switch (child) {

    case -1:
        throw SysError("unable to fork");

    case 0:
        std::vector<string>::size_type argc = args.size();
        error_comm.readSide.close();
        dup2(in.readSide, STDIN_FILENO);
        dup2(out.writeSide, STDOUT_FILENO);
        dup2(err.writeSide, STDERR_FILENO);

        char * * argv = new char * [argc + 1];
        argv[argc] = NULL;
        for (argc--; argc; argc--) {
            argv[argc] = (char *) args[argc].c_str();
        }
        argv[argc] = (char *) args[argc].c_str();

        execv(name.c_str(), argv);
        delete [] argv;
        char zero = 0;
        write(socket_fd, &zero, sizeof zero);
        write(socket_fd, &errno, sizeof errno);
        write(error_comm.writeSide, &buf, sizeof buf);
        _exit(1);
    }

    error_comm.writeSide.close();
    int status = child.wait(true);

    if (WIFEXITED(status)) {
        if (!read(error_comm.readSide, &buf, sizeof buf)) {
            char exit = WEXITSTATUS(status);
            write(socket_fd, &exit, sizeof exit);
        }
    }
    else if (WIFSIGNALED(status)) {
        char one = 1;
        int sig = WTERMSIG(status);
        write(socket_fd, &one, sizeof one);
        write(socket_fd, &sig, sizeof sig);
    }
}

static void run(int argc, char * * argv) 
{
    AutoCloseFD fdSocket = atoi(argv[1]);
    drvFile = argv[2];
    tmpDir = argv[3];
    chrootDir = argv[4];
    commandsDir = argv[5];

    closeOnExec(fdSocket);

    setSigChldAction(true);

    while (1) {
        char ignored;
        int sockets[2];
        AutoCloseFD socket_fds[2];

        /* Read a socket request. */
        ssize_t count = read(fdSocket, &ignored, sizeof ignored);
        if (count == 0)
            exit(0);
        else if (count == -1) {
            if (errno == EINTR)
                continue;
            throw SysError("Reading master socket");
        }

        /* Create and send a processing socket */
        int res = socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
        if (res == -1)
            throw SysError("socketpair");
        socket_fds[0] = sockets[0];
        socket_fds[1] = sockets[1];

        sendfd(fdSocket, socket_fds[0]);

        closeOnExec(socket_fds[1]);

        /* Fork a child to handle the command request. */
        switch (fork()) {

        case -1:
            throw SysError("unable to fork");

        case 0:
            try { /* child */
                fdSocket.close();
                socket_fds[0].close();

                /* Restore normal handling of SIGCHLD. */
                setSigChldAction(false);
                processRequest(socket_fds[1].borrow());

            } catch (std::exception & e) {
                writeToStderr("child error: " + string(e.what()) + "\n");
            }
            _exit(0);
        }
    }
}


int main(int argc, char * * argv)
{
    try {
        run(argc, argv);
    } catch (Error & e) {
        writeToStderr("Error: " + e.msg() + "\n");
        return 1;
    }
}
