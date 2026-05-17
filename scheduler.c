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
	int io_burst_times[MAX_IO]; // 각 I/O 요청에 필요한 시간
	int io_count; // I/O 요청의 수
	int io_remaining; // 현재 처리 중인 I/O 요청의 남은 시간
	int priority; // 프로세스의 우선순위 (높을수록 높은 우선순위)
	int cpu_used; // 프로세스가 CPU에서 사용한 시간
	int waiting_time; // 프로세스가 READY 상태에서 대기한 시간
	int completion_time; // 프로세스가 종료된 시간
	int current_io_idx; // 다음에 처리할 I/O 요청의 인덱스
	STATE state; // 프로세스의 현재 상태
} PROCESS;

int random_int(int min, int max) {
	return rand() % (max - min + 1) + min;
}

PROCESS create_process(int pid, int arrival_time) {
	PROCESS p;
	p.pid = pid; 
	p.arrival_time = random_int(0, 20); // 프로세스가 시스템에 들어오는 시간을 0에서 20 사이로 랜덤하게 설정
	p.burst_time = random_int(1, 20); // CPU burst time을 1에서 20 사이로 랜덤하게 설정
	p.io_count = random_int(0, MAX_IO); // I/O 요청 수를 1에서 MAX_IO 사이로 랜덤하게 설정
	for (int i = 0; i < p.io_count; i++) {
		p.io_request_times[i] = random_int(0, p.burst_time - 1); // I/O 요청이 발생하는 시간을 burst time 내에서 랜덤하게 설정
		p.io_burst_times[i] = random_int(1, 5); // 각 I/O 요청에 필요한 시간을 1에서 5 사이로 랜덤하게 설정
	}
	p.io_remaining = 0; // 초기에는 I/O 요청이 없음
	p.priority = random_int(0, 100); // 우선순위를 1에서 100 사이로 랜덤하게 설정
	p.cpu_used = 0; // 초기에는 CPU를 사용한 시간 0
	p.waiting_time = 0; // 초기 대기 시간 0
	p.completion_time = -1; // 초기에는 완료 시간이 설정되지 않음
	p.current_io_idx = 0; // 다음에 처리할 I/O 요청의 인덱스 초기화
	p.state = NEW; // 초기 상태는 NEW
	return p;
}

int main(void) {
	srand((unsigned int)time(NULL)); // 랜덤 시드 초기화

	return 0;
}