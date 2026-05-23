#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_PROCESSES 20
#define MAX_IO 5
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
	int priority; // 프로세스의 우선순위 (높을수록 높은 우선순위)
	int cpu_used; // 프로세스가 CPU에서 사용한 시간
	int waiting_time; // 프로세스가 READY 상태에서 대기한 시간
	int completion_time; // 프로세스가 종료된 시간
	int current_io_idx; // 다음에 처리할 I/O 요청의 인덱스
	int io_total; // 프로세스가 필요로 하는 총 I/O 시간
	STATE state; // 프로세스의 현재 상태
} PROCESS;

int random_int(int min, int max) {
	return rand() % (max - min + 1) + min;
}

PROCESS create_process(int pid) {
	PROCESS p;
	p.pid = pid; 
	p.arrival_time = random_int(0, 20); // 프로세스가 시스템에 들어오는 시간을 0에서 20 사이로 랜덤하게 설정
	p.burst_time = random_int(1, 20); // CPU burst time을 1에서 20 사이로 랜덤하게 설정
	p.io_count = random_int(0, MAX_IO); // I/O 요청 수를 1에서 MAX_IO 사이로 랜덤하게 설정
	p.io_total = 0; // 총 I/O 시간을 초기화
	for (int i = 0; i < p.io_count; i++) {
		p.io_request_times[i] = random_int(1, p.burst_time - 1); // I/O 요청이 발생하는 시간을 burst time 내에서 랜덤하게 설정
		p.io_burst_times[i] = random_int(1, 5); // 각 I/O 요청에 필요한 시간을 1에서 5 사이로 랜덤하게 설정
		p.io_total += p.io_burst_times[i]; // 총 I/O 시간을 누적
	}
	int i, j;
	// io_request_times, io_burst_times 같이 정렬 (버블 정렬)
	for (i = 0; i < p.io_count - 1; i++) {
		for (j = 0; j < p.io_count - 1 - i; j++) {
			if (p.io_request_times[j] > p.io_request_times[j + 1]) {
				int tmp = p.io_request_times[j];
				p.io_request_times[j] = p.io_request_times[j + 1];
				p.io_request_times[j + 1] = tmp;
				tmp = p.io_burst_times[j];
				p.io_burst_times[j] = p.io_burst_times[j + 1];
				p.io_burst_times[j + 1] = tmp;
			}
		}
	}
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
	int p_cnt = random_int(3, MAX_PROCESSES); // 생성할 프로세스 수를 3에서 MAX_PROCESSES 사이로 랜덤하게 설정
	int i, j;
	PROCESS original_processes[MAX_PROCESSES];
	PROCESS working_processes[MAX_PROCESSES];
	for (i = 1; i <= p_cnt; i++) {
		PROCESS p = create_process(i); // 프로세스 생성
		printf("Process %d: Arrival Time=%d, Burst Time=%d, IO Count=%d, Priority=%d\n", 
			p.pid, p.arrival_time, p.burst_time, p.io_count, p.priority);
		for (j = 0; j < p.io_count; j++) {
			printf("  IO Request %d: Request Time=%d, Burst Time=%d\n", 
				j + 1, p.io_request_times[j], p.io_burst_times[j]);
		}
		original_processes[i - 1] = p; // 원본 프로세스 배열에 저장
	}		
	memcpy(working_processes, original_processes, sizeof(PROCESS) * p_cnt); // 작업용 프로세스 배열에 원본 복사
	PROCESS* rqueue[MAX_PROCESSES+1]; // READY 상태의 프로세스들을 위한 큐
	PROCESS* wqueue[MAX_PROCESSES + 1]; // WAITING 상태의 프로세스들을 위한 큐
	int head = 0; // 큐에 새 프로세스를 추가할 위치
	int tail = 0; // 큐에서 프로세스를 꺼낼 위치
	int whead = 0; // WAITING 큐에 새 프로세스를 추가할 위치
	int now_cheking = 0; // 현재 실행 중인 프로세스의 인덱스
	int max_time = 1500; // 시뮬레이션 최대 시간
	int done = 0;
	PROCESS* current_process = NULL;
	for (i = 0; i < max_time; i++) {
		// 시뮬레이션 시간 단위마다 프로세스 상태 업데이트 및 스케줄링 로직 구현
		for (j = 0; j < p_cnt; j++) {
			if (working_processes[j].arrival_time == i && working_processes[j].state == NEW) {
				working_processes[j].state = READY;
				rqueue[head] = &working_processes[j];
				head = (head + 1) % (MAX_PROCESSES + 1);
			}
		}
		for (j = 0; j != whead; j++) {
			PROCESS* waiting_process = wqueue[j];
			waiting_process->io_burst_times[waiting_process->current_io_idx]--;
			if (waiting_process->io_burst_times[waiting_process->current_io_idx] == 0) {
				waiting_process->state = READY;
				rqueue[head] = waiting_process;
				waiting_process->current_io_idx++;
				head = (head + 1) % (MAX_PROCESSES + 1);
				wqueue[j] = wqueue[whead - 1]; // WAITING 큐에서 제거]
				whead--;
				j--; // 현재 인덱스에서 다음 프로세스를 확인하기 위해 인덱스 감소
			}
		}
		if (current_process == NULL && head != tail) {
			current_process = rqueue[tail];
			tail = (tail + 1) % (MAX_PROCESSES + 1);
			current_process->state = RUNNING;
		}
		// RUNNING 처리
		if (current_process != NULL) {
			// I/O 요청
			// I/O 요청 시점은 context switch 비용으로 CPU를 사용하지 않는다고 가정
			if (current_process->current_io_idx < current_process->io_count &&
				current_process->cpu_used >= current_process->io_request_times[current_process->current_io_idx]) {
				current_process->state = WAITING;
				wqueue[whead] = current_process;
				whead = (whead + 1) % (MAX_PROCESSES + 1);
				current_process = NULL;
			}
			// 종료
			else {
				current_process->cpu_used++;
				if (current_process->cpu_used == current_process->burst_time) {
					current_process->state = TERMINATED;
					current_process->completion_time = i;
					current_process = NULL;
					done++;
					if (done == p_cnt) {
						break; // 모든 프로세스가 종료되면 시뮬레이션 종료
					}
				}
			}
		}

		// 매 tick 상태 출력
		printf("t=%3d | ", i);
		if (current_process != NULL)
			printf("CPU: P%d (cpu_used=%d) | ", current_process->pid, current_process->cpu_used);
		else
			printf("CPU: IDLE            | ");

		printf("READY: ");
		int r = tail;
		while (r != head) {
			printf("P%d ", rqueue[r]->pid);
			r = (r + 1) % (MAX_PROCESSES + 1);
		}

		printf("| WAIT: ");
		for (j = 0; j < whead; j++)
			printf("P%d ", wqueue[j]->pid);

		printf("\n");
	}
	printf("\n=== FCFS result ===\n");
	int total_wait = 0, total_turnaround = 0;
	for (j = 0; j < p_cnt; j++) {
		int turnaround = working_processes[j].completion_time - working_processes[j].arrival_time;
		int waiting = turnaround - working_processes[j].burst_time - working_processes[j].io_total;
		total_wait += waiting;
		total_turnaround += turnaround;
		printf("P%d: completion=%d, turnaround=%d, waiting=%d\n",
			working_processes[j].pid, working_processes[j].completion_time, turnaround, waiting);
	}
	printf("Average Waiting Time: %.2f\n", (double)total_wait / p_cnt);
	printf("Average Turnaround Time: %.2f\n", (double)total_turnaround / p_cnt);
	return 0;
}