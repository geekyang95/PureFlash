#include <fcntl.h>
#include <sys/resource.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <arpa/inet.h>           // For inet_addr()
#include <sys/socket.h>
#include <netinet/in.h>
#include "/usr/include/signal.h" //avoid confuse with Ceph's signal.h
#include "s5_log.h"

#include "basetype.h"
#include "s5_utils.h"

#define MAXLINE 4096
void write_pid_file(const char* filename)
{
	FILE* f = fopen(filename, "w");
	if(!f)
	{
		S5LOG_WARN("Fail to create pid file:%s", filename);
		return ;
	}
	fprintf(f, "%d\n", getpid());
	fclose(f);
}

int exit_thread(pthread_t pid)
{
    int rc = 0;
    void* res;
    rc = pthread_cancel(pid);
    if(rc != 0)
        S5LOG_ERROR("Failed to do pthread_cancel,  pid(%llu): rc:%d.\n", (unsigned long long)pid, rc);

    rc = pthread_join(pid, &res);
    if (rc != 0)
        S5LOG_ERROR("Failed to do pthread_join pid(%llu) rc:%d.\n", (unsigned long long)pid, rc);
    if (res == PTHREAD_CANCELED)
        S5LOG_INFO("Thread(pid%llu) was canceled.\n", (unsigned long long)pid);
    else
        S5LOG_INFO("Thread(pid%llu) terminated normally\n", (unsigned long long)pid);
    return rc;
}

char* safe_strcpy(char* dest, const char* src, size_t dest_size)
{
	char* p = strncpy(dest, src, dest_size);
	if (dest_size > 0)
		dest[dest_size - 1]= '\0';
	return p;
}

static BOOL isDigit(char digit)
{
	BOOL flag = FALSE;
	if(digit >= '0' && digit <= '9')
	{
		flag = TRUE;
	}
	return flag;
}

static BOOL isFormatValid(const char* ip)
{
	int dotCnt = 0;
	BOOL flag = FALSE;
	while(*ip != '\0')
	{
		if(*ip == '.')
		{
			dotCnt++;
		}
		else if(!isDigit(*ip))
		{
			return FALSE;
		}
		flag = TRUE;
		ip++;
	}
	if(dotCnt == 3)
	{
		return flag;
	}
	else
	{
		return FALSE;
	}
}

static BOOL isValueValid(const char* ip)
{
	int integer = 0;
	while(*ip != '\0')
	{
		if(isDigit(*ip))
		{
			integer = integer*10 + *ip - '0';
		}
		else
		{
			if(integer > 255)
			{
				return FALSE;
			}
			integer = 0;
		}
		ip++;
	}
	return TRUE;
}

BOOL isIpValid(const char* ip)
{
	if(isFormatValid(ip) && isValueValid(ip))
	{
		return TRUE;
	}

	return FALSE;
}

uint64_t get_cbs_by_iops(uint64_t iops)
{
    return (iops * (uint64_t)2);
}

//get now time in nano seconds
uint64_t now_time_usec()
{
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC_RAW, &tp);
	return tp.tv_sec * 1000000LL + tp.tv_nsec/1000;
}




#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

static const char** log_level_str;
static const char* stderr_log[] = { KRED "CRIT" KNRM, KRED "FATA" KNRM, KRED "ERRO" KNRM, KYEL "WARN" KNRM, KBLU "INFO" KNRM, KGRN "DEBU" KNRM };
static const char* file_log[] = { "CRIT", "FATA", "ERRO", "WARN", "INFO", "DEBU" };

static void __attribute__((constructor)) initialize()
{
	if (isatty(stderr->_fileno))
	{
		log_level_str = stderr_log;
	}
	else
	{
		log_level_str = file_log;
	}
}

void s5log(int level, const char * format, ...)
{
	static __thread char buffer[2048];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	char time_buf[100];
	time_t now = time(0);
	strftime(time_buf, 100, "%Y-%m-%d %H:%M:%S", localtime(&now));
	fprintf(stderr, "[%s %s]%s\n", log_level_str[level], time_buf, buffer);
	if (level == S5LOG_LEVEL_FATAL)
		exit(-1);
}

const std::string format_string(const char * format, ...)
{
	static __thread char buffer[2048];
	va_list args;
	va_start(args, format);
	int len = vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	if (len < (int)sizeof buffer)
		// we fit in the buffer
		return{ buffer, len };
	else
		return{ buffer };
}

const std::string get_socket_addr(int sock_fd)
{
	struct sockaddr_in local_addr, remote_addr;
	socklen_t len = sizeof(local_addr);
	int rc = getsockname(sock_fd, (struct sockaddr *)&local_addr, &len);
	if(rc != 0)
	{
		S5LOG_ERROR("Failed get local addr, sock:%d, rc:%d", sock_fd, -errno);
		return "[Unknow socket addr]";
	}
	rc = getpeername(sock_fd, (struct sockaddr *)&remote_addr, &len);
	if (rc != 0)
	{
		S5LOG_ERROR("Failed get remote addr, sock:%d, rc:%d", sock_fd, -errno);
		return "[Unknow socket addr]";
	}
	return format_string("TCP://%s:%d<=%s:%d", inet_ntoa(local_addr.sin_addr), local_addr.sin_port,
		inet_ntoa(remote_addr.sin_addr), remote_addr.sin_port);
}
