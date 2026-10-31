/*******************************************************************************
 * Copyright (c) 2026 Ian Craggs
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v2.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    https://www.eclipse.org/legal/epl-2.0/
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * AI Disclosure: This file was partly AI-generated. The AI-generated
 * portions are made available under CC0-1.0 and not subject to the
 * project's licence. The human contributor has reviewed and verified
 * that the code is correct.
 *
 * SPDX-License-Identifier: EPL-2.0 and CC0-1.0
 *
 * Contributors:
 *    Ian Craggs - initial implementation
 *******************************************************************************/

/**
 * @file
 * Test for issue 1700: log spamming and CPU use while a TCP connect is pending.
 *
 * While a TCP connect is in progress, Socket_getReadySocket sets the poll
 * timeout to 0, so the receive thread spins until the connect completes,
 * logging "TCP connect is pending" on every pass.
 *
 * To get a TCP connect which stays pending without relying on the network,
 * the test listens on a local port and fills that listener's accept queue
 * without ever accepting. Further SYNs are then dropped by the TCP stack, so
 * the client's connect stays pending until the MQTT connect timeout expires.
 * Where that doesn't work (macOS resets the connection instead), it connects
 * to an unroutable address (192.0.2.1, TEST-NET-1) instead. If neither gives
 * a pending connect, the test is skipped.
 *
 * The test counts the "TCP connect is pending" trace entries and measures
 * the process CPU time used during that period, and fails if either is
 * excessive. No broker is needed.
 */

#include "MQTTAsync.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#if !defined(_WINDOWS)
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#else
#include <windows.h>
#define snprintf _snprintf
#endif

void usage(void)
{
	printf("Options:\n");
	printf("  --connect_timeout <s>   MQTT connect timeout (default 3)\n");
	printf("  --blackhole <addr>      unroutable IPv4 address, used if the full listener can't be (default 192.0.2.1)\n");
	printf("  --max_pending_logs <n>  maximum allowed \"TCP connect is pending\" entries (default 50)\n");
	printf("  --max_cpu_percent <n>   maximum allowed CPU use while connecting (default 20)\n");
	printf("  --verbose\n");
	exit(EXIT_FAILURE);
}

struct Options
{
	char* blackhole;
	int verbose;
	int test_no;
	int connect_timeout;
	int max_pending_logs;
	int max_cpu_percent;
} options =
{
	"192.0.2.1",
	0,
	0,
	3,
	50,
	20,
};

void getopts(int argc, char** argv)
{
	int count = 1;

	while (count < argc)
	{
		if (strcmp(argv[count], "--test_no") == 0)
		{
			if (++count < argc)
				options.test_no = atoi(argv[count]);
			else
				usage();
		}
		else if (strcmp(argv[count], "--connect_timeout") == 0)
		{
			if (++count < argc)
				options.connect_timeout = atoi(argv[count]);
			else
				usage();
		}
		else if (strcmp(argv[count], "--blackhole") == 0)
		{
			if (++count < argc)
				options.blackhole = argv[count];
			else
				usage();
		}
		else if (strcmp(argv[count], "--max_pending_logs") == 0)
		{
			if (++count < argc)
				options.max_pending_logs = atoi(argv[count]);
			else
				usage();
		}
		else if (strcmp(argv[count], "--max_cpu_percent") == 0)
		{
			if (++count < argc)
				options.max_cpu_percent = atoi(argv[count]);
			else
				usage();
		}
		else if (strcmp(argv[count], "--verbose") == 0)
			options.verbose = 1;
		else if (strcmp(argv[count], "--help") == 0 || strcmp(argv[count], "-h") == 0)
			usage();
		count++;
	}
}


#define LOGA_DEBUG 0
#define LOGA_INFO 1
#include <sys/timeb.h>
void MyLog(int LOGA_level, char* format, ...)
{
	static char msg_buf[256];
	va_list args;
#if defined(_WIN32) || defined(_WINDOWS)
	struct timeb ts;
#else
	struct timeval ts;
#endif
	struct tm *timeinfo;

	if (LOGA_level == LOGA_DEBUG && options.verbose == 0)
	  return;

#if defined(_WIN32) || defined(_WINDOWS)
	ftime(&ts);
	timeinfo = localtime(&ts.time);
#else
	gettimeofday(&ts, NULL);
	timeinfo = localtime(&ts.tv_sec);
#endif
	strftime(msg_buf, 80, "%Y%m%d %H%M%S", timeinfo);

#if defined(_WIN32) || defined(_WINDOWS)
	sprintf(&msg_buf[strlen(msg_buf)], ".%.3hu ", ts.millitm);
#else
	sprintf(&msg_buf[strlen(msg_buf)], ".%.3lu ", ts.tv_usec / 1000L);
#endif

	va_start(args, format);
	vsnprintf(&msg_buf[strlen(msg_buf)], sizeof(msg_buf) - strlen(msg_buf), format, args);
	va_end(args);

	printf("%s\n", msg_buf);
	fflush(stdout);
}

void MySleep(long milliseconds)
{
#if defined(_WIN32)
	Sleep(milliseconds);
#else
	usleep(milliseconds*1000);
#endif
}

/* monotonic clock in milliseconds */
long long now_ms(void)
{
#if defined(_WIN32)
	return (long long)GetTickCount64();
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

#define assert(a, b, c, ...) myassert(__FILE__, __LINE__, a, b, c, __VA_ARGS__)

int tests = 0;
int failures = 0;

void myassert(char* filename, int lineno, char* description, int value,
		char* format, ...)
{
	++tests;
	if (!value)
	{
		va_list args;

		++failures;
		MyLog(LOGA_INFO, "Assertion failed, file %s, line %d, description: %s", filename,
				lineno, description);

		va_start(args, format);
		vprintf(format, args);
		va_end(args);
	}
	else
		MyLog(LOGA_DEBUG, "Assertion succeeded, file %s, line %d, description: %s",
				filename, lineno, description);
}


#if !defined(_WINDOWS)

/* user + system CPU time used by this process, in milliseconds */
long long cpu_ms(void)
{
	struct rusage ru;

	getrusage(RUSAGE_SELF, &ru);
	return (long long)(ru.ru_utime.tv_sec + ru.ru_stime.tv_sec) * 1000 +
		(ru.ru_utime.tv_usec + ru.ru_stime.tv_usec) / 1000;
}

#define MAX_FILLERS 16

int listener = -1;
int fillers[MAX_FILLERS];
int filler_count = 0;

void close_listener(void)
{
	int i;

	for (i = 0; i < filler_count; ++i)
		close(fillers[i]);
	filler_count = 0;
	if (listener != -1)
		close(listener);
	listener = -1;
}

/**
 * Start a non-blocking TCP connect, and see if it is still pending after 500ms.
 * The socket is left open in the fillers array.
 * @param addr the address to connect to
 * @return 1 if the connect is still pending, 0 otherwise
 */
int connect_stays_pending(struct sockaddr_in* addr)
{
	struct pollfd pfd;
	int fd = socket(AF_INET, SOCK_STREAM, 0);

	if (fd == -1 || filler_count == MAX_FILLERS)
		return 0;
	fillers[filler_count++] = fd;
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
	if (connect(fd, (struct sockaddr*)addr, sizeof(*addr)) == 0)
		return 0;
	pfd.fd = fd;
	pfd.events = POLLOUT;
	pfd.revents = 0;
	return poll(&pfd, 1, 500) == 0;
}

/**
 * Listen on an ephemeral localhost port, and connect to it without accepting
 * until a connect stays pending, i.e. the accept queue is full. This works on
 * Linux, which drops SYNs when the accept queue is full, but not on macOS,
 * which resets the connection.
 * @param uri set to the URI to connect to
 * @return 1 if a pending connect was obtained, 0 otherwise
 */
int start_full_listener(char* uri, size_t urilen)
{
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	int pending = 0;

	listener = socket(AF_INET, SOCK_STREAM, 0);
	if (listener == -1)
		return 0;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = 0;
	if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) != 0 ||
		listen(listener, 1) != 0 ||
		getsockname(listener, (struct sockaddr*)&addr, &addrlen) != 0)
		return 0;

	while (!pending && filler_count < MAX_FILLERS)
		pending = connect_stays_pending(&addr);
	MyLog(LOGA_INFO, "Listening on port %d, %d connects to fill the accept queue, pending %d",
			ntohs(addr.sin_port), filler_count, pending);
	if (pending)
		snprintf(uri, urilen, "tcp://127.0.0.1:%d", ntohs(addr.sin_port));
	return pending;
}

/**
 * Check that a connect to an unroutable address stays pending.
 * @param uri set to the URI to connect to
 * @return 1 if a pending connect was obtained, 0 otherwise
 */
int check_blackhole(char* uri, size_t urilen)
{
	struct sockaddr_in addr;
	int pending = 0;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(1883);
	if (inet_pton(AF_INET, options.blackhole, &addr.sin_addr) != 1)
		return 0;
	pending = connect_stays_pending(&addr);
	MyLog(LOGA_INFO, "Connect to %s pending %d", options.blackhole, pending);
	if (pending)
		snprintf(uri, urilen, "tcp://%s:1883", options.blackhole);
	return pending;
}

#endif /* !_WINDOWS */


volatile int connected = 0;
volatile int connect_failed = 0;
volatile int pending_logs = 0;

void traceCallback(enum MQTTASYNC_TRACE_LEVELS level, char* message)
{
	if (strstr(message, "TCP connect is pending"))
		pending_logs++;
	if (options.verbose)
		printf("Trace : %d, %s\n", level, message);
}

void onConnect(void* context, MQTTAsync_successData* response)
{
	MyLog(LOGA_INFO, "Connected - unexpected");
	connected = 1;
}

void onConnectFailure(void* context, MQTTAsync_failureData* response)
{
	MyLog(LOGA_INFO, "Connect failed, rc %d", response ? response->code : -1);
	connect_failed = 1;
}

int test1(struct Options options)
{
	char* testname = "test1700 - CPU and log use while a TCP connect is pending";
#if defined(_WINDOWS)
	/* Windows refuses connects when the accept queue is full, rather than dropping
	 * the SYN, so a pending connect can't be set up this way */
	MyLog(LOGA_INFO, "Skipping %s on Windows", testname);
#else
	MQTTAsync c;
	MQTTAsync_connectOptions opts = MQTTAsync_connectOptions_initializer;
	char uri[64];
	int rc = 0;
	int count = 0;
	long long start = 0, elapsed = 0;
	long long cpu_start = 0, cpu_used = 0;
	int cpu_percent = 0;

	MyLog(LOGA_INFO, "Starting %s", testname);

	if (!start_full_listener(uri, sizeof(uri)))
		close_listener();
	if (listener == -1 && !check_blackhole(uri, sizeof(uri)))
	{
		MyLog(LOGA_INFO, "Could not get a pending TCP connect, skipping %s", testname);
		goto exit;
	}
	MyLog(LOGA_INFO, "Connecting to %s", uri);

	MQTTAsync_setTraceCallback(traceCallback);
	MQTTAsync_setTraceLevel(MQTTASYNC_TRACE_PROTOCOL);

	rc = MQTTAsync_create(&c, uri, "issue1700", MQTTCLIENT_PERSISTENCE_NONE, NULL);
	assert("good rc from create", rc == MQTTASYNC_SUCCESS, "rc was %d\n", rc);
	if (rc != MQTTASYNC_SUCCESS)
		goto exit;

	opts.keepAliveInterval = 20;
	opts.cleansession = 1;
	opts.connectTimeout = options.connect_timeout;
	opts.onSuccess = onConnect;
	opts.onFailure = onConnectFailure;

	start = now_ms();
	cpu_start = cpu_ms();
	rc = MQTTAsync_connect(c, &opts);
	assert("good rc from connect", rc == MQTTASYNC_SUCCESS, "rc was %d\n", rc);
	if (rc != MQTTASYNC_SUCCESS)
		goto destroy;

	while (!connected && !connect_failed && ++count < (options.connect_timeout + 10) * 100)
		MySleep(10);
	cpu_used = cpu_ms() - cpu_start;
	elapsed = now_ms() - start;
	MQTTAsync_setTraceCallback(NULL);

	assert("connect failed", connect_failed == 1, "connect_failed was %d, connected %d\n",
			connect_failed, connected);
	assert("connect stayed pending until the connect timeout",
			elapsed >= options.connect_timeout * 1000 - 500,
			"connect completed after %lld ms\n", elapsed);

	if (elapsed > 0)
		cpu_percent = (int)(cpu_used * 100 / elapsed);
	MyLog(LOGA_INFO, "Connect pending for %lld ms: %d \"TCP connect is pending\" trace entries, %lld ms CPU (%d%%)",
			elapsed, pending_logs, cpu_used, cpu_percent);

	assert("\"TCP connect is pending\" not logged excessively", pending_logs <= options.max_pending_logs,
			"%d entries in %lld ms, max allowed %d\n", pending_logs, elapsed, options.max_pending_logs);
	assert("CPU use not excessive while connect pending", cpu_percent <= options.max_cpu_percent,
			"%d%% CPU, max allowed %d%%\n", cpu_percent, options.max_cpu_percent);

destroy:
	MQTTAsync_destroy(&c);
exit:
	close_listener();
	MyLog(LOGA_INFO, "%s: test %s. %d tests run, %d failures.",
			(failures == 0) ? "passed" : "failed", testname, tests, failures);
#endif
	return failures;
}


int main(int argc, char** argv)
{
	int rc = 0;

	getopts(argc, argv);

	rc = test1(options);

	MyLog(LOGA_INFO, "Total tests run: %d", tests);
	if (rc == 0)
		MyLog(LOGA_INFO, "verdict pass");
	else
		MyLog(LOGA_INFO, "verdict fail");

	return rc;
}
