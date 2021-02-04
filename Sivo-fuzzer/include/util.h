#include <stdio.h>		/* fprintf, stderr */
#include <stdlib.h>		/* exit 		   */

#ifndef _UTIL_H
#define _UTIL_H

#define RED 	"\033[31;1m"
#define GREEN	"\033[32;1m"
#define YELLOW 	"\033[33;1m"
#define BLUE	"\033[34;1m"
#define CLR		"\033[0m"

/* unconditional messages */
#define LOG_ERR(message)												\
	do {																\
		fprintf(stderr, RED "[ERR]" GREEN " %s:%d " CLR "| %s\n",		\
				__FILE__, __LINE__, message);							\
	} while (0)

#define LOG_WAR(message)												\
	do {																\
		fprintf(stderr, YELLOW "[WAR]" GREEN " %s:%d " CLR "| %s\n",	\
				__FILE__, __LINE__, message);							\
	} while (0)

#define LOG_INF(message)												\
	do {																\
		fprintf(stderr, BLUE "[INF]" GREEN " %s:%d " CLR "| %s\n",		\
				__FILE__, __LINE__, message);							\
	} while (0)

/* conditional messages (w/ potential side-effects) */
#define DIE(assertion, message)                 \
	do {								        \
		if (assertion) {					    \
			LOG_ERR(message);					\
			exit(-1);					        \
		}							            \
	} while (0)

#define WAR(assertion, message)					\
	do {										\
		if (assertion)							\
			LOG_WAR(message);					\
	} while (0)

#endif