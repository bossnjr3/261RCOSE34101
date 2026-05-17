#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_PROCESSES 20
#define MAX_IO 10
#define DEFAULT_QUANTUM 4

typedef enum {
    NEW,  // 들어온 상태
    READY, // 준비 상태
	RUNNING, // 현재 실행 중인 상태
	WAITING, // I/O 대기 상태
	TERMINATED // 종료 상태
} STATE;

typedef struct {
	int pid; // 프로세스 ID
	int arrival_time; // 프로세스가 시스템에 들어온 시간
	int burst_time; // 프로세스가 CPU에서 실행되는 데 필요한 총 시간 (IO 시간 제외)
	int io_request_times[MAX_IO]; // I/O 요청이 발생하는 시간
	int io_count; // I/O 요청의 수
	int io_remaining; // 현재 처리 중인 I/O 요청의 남은 시간
	int priority; // 프로세스의 우선순위 (높을수록 높은 우선순위)
	int cpu_used; // 프로세스가 CPU에서 사용한 시간
	int waiting_time; // 프로세스가 READY 상태에서 대기한 시간
	int completion_time; // 프로세스가 종료된 시간
	int current_io_idx; // 다음에 처리할 I/O 요청의 인덱스
	STATE state; // 프로세스의 현재 상태
} PROCESS;


int main(void) {
	return 0;
}