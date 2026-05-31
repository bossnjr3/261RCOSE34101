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

typedef struct {
	char name[32];
	double avg_waiting;
	double avg_turnaround;
	int gantt[1500];
	int gantt_len;
	int p_cnt;                  // 프로세스 수 (상세 출력용)
	// PROCESS procs[MAX_PROCESSES];  // 프로세스별 결과 (completion/turnaround/waiting 보려고)
	int ran;                    // 실행 여부 (0/1)
} RESULT;


int random_int(int min, int max) {
	return rand() % (max - min + 1) + min;
}

int sjf_compare(PROCESS* a, PROCESS* b) {
	int a_time = a->burst_time - a->cpu_used; // 남은 CPU burst time 계산
	int b_time = b->burst_time - b->cpu_used; // 남은 CPU burst time 계산
	if (a_time != b_time) {
		return a_time - b_time; // 남은 CPU burst time이 작은 순서대로 정렬
	}
	else {
		return a->pid - b->pid; // 남은 CPU burst time이 같으면 PID가 작은 순서대로 정렬
	}
}

int priority_compare(PROCESS* a, PROCESS* b) {
	if (a->priority != b->priority) {
		return b->priority - a->priority; // 우선순위가 높은 순서대로 정렬
	}
	else {
		return a->pid - b->pid; // 우선순위가 같으면 PID가 작은 순서대로 정렬
	}
}

PROCESS create_process(int pid) {
	PROCESS p;
	p.pid = pid; 
	p.arrival_time = random_int(0, 20); // 프로세스가 시스템에 들어오는 시간을 0에서 20 사이로 랜덤하게 설정
	p.burst_time = random_int(1, 20); // CPU burst time을 1에서 20 사이로 랜덤하게 설정
	p.io_count = random_int(0, MAX_IO); // I/O 요청 수를 1에서 MAX_IO 사이로 랜덤하게 설정
	p.io_total = 0; // 총 I/O 시간을 초기화
	// burst가 작으면 IO 넣을 자리가 없으니 io_count 제한
	int max_io_possible = p.burst_time - 1;   // 1~(burst-1) 범위에 들어갈 수 있는 최대 개수
	if (p.io_count > max_io_possible) {
		p.io_count = max_io_possible;
	}
	if (p.io_count < 0) p.io_count = 0;   // burst=1이면 max_io_possible=0
	// 1 ~ burst-1 후보를 셔플해서 중복 없이 io_count개 뽑기
	int candidates[MAX_PROCESSES];   // burst가 최대 20이라 충분
	int cnt = 0;
	int k, r, tmp;
	int i, j;
	for (k = 1; k < p.burst_time; k++) candidates[cnt++] = k;
	// Fisher-Yates 셔플
	for (k = cnt - 1; k > 0; k--) {
		r = rand() % (k + 1);
		tmp = candidates[k];
		candidates[k] = candidates[r];
		candidates[r] = tmp;
	}
	for (k = 0; k < p.io_count; k++) {
		p.io_request_times[k] = candidates[k];   // I/O 요청이 발생하는 시간을 중복 없이 burst time 내에서 랜덤하게 설정
		p.io_burst_times[k] = random_int(1, 5); // 각 I/O 요청에 필요한 시간을 1에서 5 사이로 랜덤하게 설정
		p.io_total += p.io_burst_times[k]; // 총 I/O 시간을 누적
	}
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
void heap_push(PROCESS* HEAP[], int size, PROCESS* p, int (*compare)(PROCESS*, PROCESS*));
PROCESS* heap_pop(PROCESS* HEAP[], int size, int (*compare)(PROCESS*, PROCESS*));
void doFCFS(PROCESS *original_processes, int p_cnt, RESULT *result);
void doSJF(PROCESS* original_processes, int p_cnt, RESULT *result);
void doSJF_preemptive(PROCESS* original_processes, int p_cnt, RESULT *result);
void doPriority(PROCESS* original_processes, int p_cnt, RESULT *result);
void doPriority_preemptive(PROCESS* original_processes, int p_cnt, RESULT *result);
void doRR(PROCESS* original_processes, int p_cnt, int time_quantum, RESULT *result);

void print_gantt(int gantt[], int total);
void print_tick(int t, PROCESS* current, PROCESS* rqueue[], int tail, int head, PROCESS* wqueue[], int whead);
void print_result(const char* name, PROCESS* working_processes, int p_cnt);


int main(void) {
	srand((unsigned int)time(NULL)); // 랜덤 시드 초기화
	int p_cnt = random_int(3, MAX_PROCESSES); // 생성할 프로세스 수를 3에서 MAX_PROCESSES 사이로 랜덤하게 설정
	int i, j;
	PROCESS original_processes[MAX_PROCESSES];
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
	RESULT results[10]; // 결과 저장용 배열 
	for (i = 0; i < 10; i++) {
		results[i].ran = 0;
	}

	int what = -1;
	while (what != 0) {
		printf("\nSelect scheduling algorithm to simulate:\n");
		printf("1. FCFS\n");
		printf("2. SJF\n");
		printf("3. SJF (Preemptive)\n");
		printf("4. Priority\n");
		printf("5. Priority (Preemptive)\n");
		printf("6. RR\n");
		printf("7. Run All\n");
		printf("0. Exit\n");
		printf("Enter your choice: ");
		scanf("%d", &what);
		switch (what) {
			case 1:
				doFCFS(original_processes, p_cnt, &results[0]);
				break;
			case 2:
				doSJF(original_processes, p_cnt, &results[1]);
				break;
			case 3:
				doSJF_preemptive(original_processes, p_cnt, &results[2]);
				break;
			case 4:
				doPriority(original_processes, p_cnt, &results[3]);
				break;
			case 5:
				doPriority_preemptive(original_processes, p_cnt, &results[4]);
				break;
			case 6:
				doRR(original_processes, p_cnt, 4, &results[5]);
				break;
			case 7:
				doFCFS(original_processes, p_cnt, &results[0]);
				doSJF(original_processes, p_cnt, &results[1]);
				doSJF_preemptive(original_processes, p_cnt, &results[2]);
				doPriority(original_processes, p_cnt, &results[3]);
				doPriority_preemptive(original_processes, p_cnt, &results[4]);
				doRR(original_processes, p_cnt, 4, &results[5]);
				break;
			case 0:
				printf("Exiting...\n");
				break;
			default:
				printf("Invalid choice. Please try again.\n");
		
		}

	}
	return 0;
}

void doFCFS(PROCESS* original_processes, int p_cnt, RESULT *result) {
	PROCESS working_processes[MAX_PROCESSES];
	int i, j;
	memcpy(working_processes, original_processes, sizeof(PROCESS) * p_cnt); // 작업용 프로세스 배열에 원본 복사
	PROCESS* rqueue[MAX_PROCESSES + 1]; // READY 상태의 프로세스들을 위한 큐
	PROCESS* wqueue[MAX_PROCESSES + 1]; // WAITING 상태의 프로세스들을 위한 큐
	int head = 0; // 큐에 새 프로세스를 추가할 위치
	int tail = 0; // READY 큐에서 프로세스를 제거할 위치
	int whead = 0; // WAITING 큐에 새 프로세스를 추가할 위치
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
		// WAITING 큐 처리
		for (j = 0; j != whead; j++) {
			PROCESS* waiting_process = wqueue[j];
			waiting_process->io_burst_times[waiting_process->current_io_idx]--;
			if (waiting_process->io_burst_times[waiting_process->current_io_idx] == 0) {
				waiting_process->state = READY;
				waiting_process->current_io_idx++;
			}
		}
		// READY 큐에서 프로세스 선택 (FCFS)
		if (current_process == NULL && head != tail) {
			current_process = rqueue[tail];
			tail = (tail + 1) % (MAX_PROCESSES + 1);
			current_process->state = RUNNING;
		}
		// RUNNING 처리
		if (current_process != NULL) {
			current_process->cpu_used++;
			// I/O 요청
			// I/O 요청 시점의 context switch 비용이 발생하지 않는다고 가정
			if (current_process->cpu_used == current_process->burst_time) {
				current_process->state = TERMINATED;
				current_process->completion_time = i + 1; // 프로세스가 종료된 시점은 현재 tick이 끝난 시점이므로 i+1로 설정
				done++;
			} else if (current_process->current_io_idx < current_process->io_count &&
				current_process->cpu_used >= current_process->io_request_times[current_process->current_io_idx]) {
				current_process->state = WAITING;
				wqueue[whead] = current_process;
				whead++;
			}
		}
		for (j = 0; j != whead; j++) {
			PROCESS* waiting_process = wqueue[j];
			if (waiting_process->state == READY) {
				rqueue[head] = waiting_process;
				head = (head + 1) % (MAX_PROCESSES + 1);
				wqueue[j] = wqueue[whead - 1]; // WAITING 큐에서 제거
				whead--;
				j--; // 현재 인덱스에서 다음 프로세스를 확인하기 위해 인덱스 감소
			}
		}
		if (current_process != NULL) {
			result->gantt[i] = current_process->pid; // 현재 tick에 실행된 프로세스의 PID 기록
		}
		else {
			result->gantt[i] = 0; // CPU가 IDLE 상태인 경우 0으로 기록
		}
		//print_tick(i, current_process, rqueue, tail, head, wqueue, whead); // 매 tick 상태 출력
		if (current_process != NULL && (current_process->state == TERMINATED || current_process->state == WAITING)) {
			current_process = NULL;
		}
		if (done == p_cnt) {
			break; // 모든 프로세스가 종료되면 시뮬레이션 종료
		}
	}
	// 평균 계산
	int total_wait = 0, total_turnaround = 0, turn, wait;
	for (j = 0; j < p_cnt; j++) {
		turn = working_processes[j].completion_time - working_processes[j].arrival_time;
		wait = turn - working_processes[j].burst_time - working_processes[j].io_total;
		total_wait += wait;
		total_turnaround += turn;
	}

	// result에 저장
	strcpy(result->name, "FCFS");
	result->avg_waiting = (double)total_wait / p_cnt;
	result->avg_turnaround = (double)total_turnaround / p_cnt;
	result->gantt_len = i + 1;
	result->ran = 1;
}

void doSJF(PROCESS* original_processes, int p_cnt, RESULT *result) {
	PROCESS working_processes[MAX_PROCESSES];
	int i, j;
	memcpy(working_processes, original_processes, sizeof(PROCESS) * p_cnt); // 작업용 프로세스 배열에 원본 복사
	PROCESS* rqueue[MAX_PROCESSES + 1]; // READY 상태의 프로세스들을 위한 큐
	PROCESS* wqueue[MAX_PROCESSES + 1]; // WAITING 상태의 프로세스들을 위한 큐
	int head = 0; // 큐에 새 프로세스를 추가할 위치
	int whead = 0; // WAITING 큐에 새 프로세스를 추가할 위치
	int max_time = 1500; // 시뮬레이션 최대 시간
	int done = 0;
	PROCESS* current_process = NULL;
	for (i = 0; i < max_time; i++) {
		// 시뮬레이션 시간 단위마다 프로세스 상태 업데이트 및 스케줄링 로직 구현
		for (j = 0; j < p_cnt; j++) {
			if (working_processes[j].arrival_time == i && working_processes[j].state == NEW) {
				working_processes[j].state = READY;
				heap_push(rqueue, head, &working_processes[j], sjf_compare);
				head++;
			}
		}
		// WAITING 큐 처리
		for (j = 0; j != whead; j++) {
			PROCESS* waiting_process = wqueue[j];
			waiting_process->io_burst_times[waiting_process->current_io_idx]--;
			if (waiting_process->io_burst_times[waiting_process->current_io_idx] == 0) {
				waiting_process->state = READY;
				waiting_process->current_io_idx++;
			}
		}
		// READY 큐에서 프로세스 선택 (SJF)
		if (current_process == NULL) {
			current_process = heap_pop(rqueue, head, sjf_compare);
			if (current_process != NULL) {
				head--;
				current_process->state = RUNNING;
			}
		}
		// RUNNING 처리
		if (current_process != NULL) {
			current_process->cpu_used++;
			// I/O 요청
			// I/O 요청 시점의 context switch 비용이 발생하지 않는다고 가정
			if (current_process->cpu_used == current_process->burst_time) {
				current_process->state = TERMINATED;
				current_process->completion_time = i + 1; // 프로세스가 종료된 시점은 현재 tick이 끝난 시점이므로 i+1로 설정
				done++;
			}
			else if (current_process->current_io_idx < current_process->io_count &&
				current_process->cpu_used >= current_process->io_request_times[current_process->current_io_idx]) {
				current_process->state = WAITING;
				wqueue[whead] = current_process;
				whead++;
			}
		}
		for (j = 0; j != whead; j++) {
			PROCESS* waiting_process = wqueue[j];
			if (waiting_process->state == READY) {
				heap_push(rqueue, head, waiting_process, sjf_compare);
				head++;
				wqueue[j] = wqueue[whead - 1]; // WAITING 큐에서 제거]
				whead--;
				j--; // 현재 인덱스에서 다음 프로세스를 확인하기 위해 인덱스 감소
			}
		}
		if (current_process != NULL) {
			result->gantt[i] = current_process->pid; // 현재 tick에 실행된 프로세스의 PID 기록
		}
		else {
			result->gantt[i] = 0; // CPU가 IDLE 상태인 경우 0으로 기록
		}
		//print_tick(i, current_process, rqueue, 0, head, wqueue, whead); // 매 tick 상태 출력
		if (current_process != NULL && (current_process->state == TERMINATED || current_process->state == WAITING)) {
			current_process = NULL;
		}
		if (done == p_cnt) {
			break; // 모든 프로세스가 종료되면 시뮬레이션 종료
		}
	}
	// 평균 계산
	int total_wait = 0, total_turnaround = 0, turn, wait;
	for (j = 0; j < p_cnt; j++) {
		turn = working_processes[j].completion_time - working_processes[j].arrival_time;
		wait = turn - working_processes[j].burst_time - working_processes[j].io_total;
		total_wait += wait;
		total_turnaround += turn;
	}

	// result에 저장
	strcpy(result->name, "SJF");
	result->avg_waiting = (double)total_wait / p_cnt;
	result->avg_turnaround = (double)total_turnaround / p_cnt;
	result->gantt_len = i + 1;
	result->ran = 1;
}


void doSJF_preemptive(PROCESS* original_processes, int p_cnt, RESULT *result) {
	PROCESS working_processes[MAX_PROCESSES];
	int i, j;
	memcpy(working_processes, original_processes, sizeof(PROCESS) * p_cnt); // 작업용 프로세스 배열에 원본 복사
	PROCESS* rqueue[MAX_PROCESSES + 1]; // READY 상태의 프로세스들을 위한 큐
	PROCESS* wqueue[MAX_PROCESSES + 1]; // WAITING 상태의 프로세스들을 위한 큐
	int head = 0; // 큐에 새 프로세스를 추가할 위치
	int whead = 0; // WAITING 큐에 새 프로세스를 추가할 위치
	int max_time = 1500; // 시뮬레이션 최대 시간
	int done = 0;
	PROCESS* current_process = NULL;
	PROCESS* compare_process = NULL;
	for (i = 0; i < max_time; i++) {
		// 시뮬레이션 시간 단위마다 프로세스 상태 업데이트 및 스케줄링 로직 구현
		for (j = 0; j < p_cnt; j++) {
			if (working_processes[j].arrival_time == i && working_processes[j].state == NEW) {
				working_processes[j].state = READY;
				heap_push(rqueue, head, &working_processes[j], sjf_compare);
				head++;
			}
		}
		// WAITING 큐 처리
		for (j = 0; j != whead; j++) {
			PROCESS* waiting_process = wqueue[j];
			waiting_process->io_burst_times[waiting_process->current_io_idx]--;
			if (waiting_process->io_burst_times[waiting_process->current_io_idx] == 0) {
				waiting_process->state = READY;
				waiting_process->current_io_idx++;
			}
		}
		// READY 큐에서 프로세스 선택 (SJF Preemptive)
		if (current_process == NULL) {
			current_process = heap_pop(rqueue, head, sjf_compare);
			if (current_process != NULL) {
				head--;
				current_process->state = RUNNING;
			}
		}
		// 현재 실행 중인 프로세스와 READY 큐의 가장 짧은 프로세스 비교
		else {
			if (head > 0 && sjf_compare(current_process, rqueue[0]) > 0) {
				compare_process = rqueue[0]; // READY 큐에서 가장 짧은 프로세스 확인
				compare_process = heap_pop(rqueue, head, sjf_compare); // READY 큐에서 가장 짧은 프로세스 꺼내기
				head--;
				current_process->state = READY;
				heap_push(rqueue, head, current_process, sjf_compare);
				head++;
				current_process = compare_process;
				current_process->state = RUNNING;
			}
		}
		// RUNNING 처리
		if (current_process != NULL) {
			current_process->cpu_used++;
			// I/O 요청
			// I/O 요청 시점의 context switch 비용이 발생하지 않는다고 가정
			if (current_process->cpu_used == current_process->burst_time) {
				current_process->state = TERMINATED;
				current_process->completion_time = i + 1; // 프로세스가 종료된 시점은 현재 tick이 끝난 시점이므로 i+1로 설정
				done++;
			}
			else if (current_process->current_io_idx < current_process->io_count &&
				current_process->cpu_used >= current_process->io_request_times[current_process->current_io_idx]) {
				current_process->state = WAITING;
				wqueue[whead] = current_process;
				whead++;
			}
		}
		for (j = 0; j != whead; j++) {
			PROCESS* waiting_process = wqueue[j];
			if (waiting_process->state == READY) {
				heap_push(rqueue, head, waiting_process, sjf_compare);
				head++;
				wqueue[j] = wqueue[whead - 1]; // WAITING 큐에서 제거]
				whead--;
				j--; // 현재 인덱스에서 다음 프로세스를 확인하기 위해 인덱스 감소
			}
		}
		if (current_process != NULL) {
			result->gantt[i] = current_process->pid; // 현재 tick에 실행된 프로세스의 PID 기록
		}
		else {
			result->gantt[i] = 0; // CPU가 IDLE 상태인 경우 0으로 기록
		}
		//print_tick(i, current_process, rqueue, 0, head, wqueue, whead); // 매 tick 상태 출력
		if (current_process != NULL && (current_process->state == TERMINATED || current_process->state == WAITING)) {
			current_process = NULL;
		}
		if (done == p_cnt) {
			break; // 모든 프로세스가 종료되면 시뮬레이션 종료
		}
	}
	// 평균 계산
	int total_wait = 0, total_turnaround = 0, turn, wait;
	for (j = 0; j < p_cnt; j++) {
		turn = working_processes[j].completion_time - working_processes[j].arrival_time;
		wait = turn - working_processes[j].burst_time - working_processes[j].io_total;
		total_wait += wait;
		total_turnaround += turn;
	}

	// result에 저장
	strcpy(result->name, "SJF Preemptive");
	result->avg_waiting = (double)total_wait / p_cnt;
	result->avg_turnaround = (double)total_turnaround / p_cnt;
	result->gantt_len = i + 1;
	result->ran = 1;
}

void doPriority(PROCESS* original_processes, int p_cnt, RESULT *result) {
	PROCESS working_processes[MAX_PROCESSES];
	int i, j;
	memcpy(working_processes, original_processes, sizeof(PROCESS) * p_cnt); // 작업용 프로세스 배열에 원본 복사
	PROCESS* rqueue[MAX_PROCESSES + 1]; // READY 상태의 프로세스들을 위한 큐
	PROCESS* wqueue[MAX_PROCESSES + 1]; // WAITING 상태의 프로세스들을 위한 큐
	int head = 0; // 큐에 새 프로세스를 추가할 위치
	int whead = 0; // WAITING 큐에 새 프로세스를 추가할 위치
	int max_time = 1500; // 시뮬레이션 최대 시간
	int done = 0;
	PROCESS* current_process = NULL;
	for (i = 0; i < max_time; i++) {
		// 시뮬레이션 시간 단위마다 프로세스 상태 업데이트 및 스케줄링 로직 구현
		for (j = 0; j < p_cnt; j++) {
			if (working_processes[j].arrival_time == i && working_processes[j].state == NEW) {
				working_processes[j].state = READY;
				heap_push(rqueue, head, &working_processes[j], priority_compare);
				head++;
			}
		}
		// WAITING 큐 처리
		for (j = 0; j != whead; j++) {
			PROCESS* waiting_process = wqueue[j];
			waiting_process->io_burst_times[waiting_process->current_io_idx]--;
			if (waiting_process->io_burst_times[waiting_process->current_io_idx] == 0) {
				waiting_process->state = READY;
				waiting_process->current_io_idx++;
			}
		}
		// READY 큐에서 프로세스 선택 (Priority)
		if (current_process == NULL) {
			current_process = heap_pop(rqueue, head, priority_compare);
			if (current_process != NULL) {
				head--;
				current_process->state = RUNNING;
			}
		}
		// RUNNING 처리
		if (current_process != NULL) {
			current_process->cpu_used++;
			// I/O 요청
			// I/O 요청 시점의 context switch 비용이 발생하지 않는다고 가정
			if (current_process->cpu_used == current_process->burst_time) {
				current_process->state = TERMINATED;
				current_process->completion_time = i + 1; // 프로세스가 종료된 시점은 현재 tick이 끝난 시점이므로 i+1로 설정
				done++;
			}
			else if (current_process->current_io_idx < current_process->io_count &&
				current_process->cpu_used >= current_process->io_request_times[current_process->current_io_idx]) {
				current_process->state = WAITING;
				wqueue[whead] = current_process;
				whead++;
			}
		}
		for (j = 0; j != whead; j++) {
			PROCESS* waiting_process = wqueue[j];
			if (waiting_process->state == READY) {
				heap_push(rqueue, head, waiting_process, priority_compare);
				head++;
				wqueue[j] = wqueue[whead - 1]; // WAITING 큐에서 제거]
				whead--;
				j--; // 현재 인덱스에서 다음 프로세스를 확인하기 위해 인덱스 감소
			}
		}
		if (current_process != NULL) {
			result->gantt[i] = current_process->pid; // 현재 tick에 실행된 프로세스의 PID 기록
		}
		else {
			result->gantt[i] = 0; // CPU가 IDLE 상태인 경우 0으로 기록
		}
		//print_tick(i, current_process, rqueue, 0, head, wqueue, whead); // 매 tick 상태 출력
		if (current_process != NULL && (current_process->state == TERMINATED || current_process->state == WAITING)) {
			current_process = NULL;
		}
		if (done == p_cnt) {
			break; // 모든 프로세스가 종료되면 시뮬레이션 종료
		}
	}
	// 평균 계산
	int total_wait = 0, total_turnaround = 0, turn, wait;
	for (j = 0; j < p_cnt; j++) {
		turn = working_processes[j].completion_time - working_processes[j].arrival_time;
		wait = turn - working_processes[j].burst_time - working_processes[j].io_total;
		total_wait += wait;
		total_turnaround += turn;
	}

	// result에 저장
	strcpy(result->name, "Priority");
	result->avg_waiting = (double)total_wait / p_cnt;
	result->avg_turnaround = (double)total_turnaround / p_cnt;
	result->gantt_len = i + 1;
	result->ran = 1;
}

void doPriority_preemptive(PROCESS* original_processes, int p_cnt, RESULT *result) {
	PROCESS working_processes[MAX_PROCESSES];
	int i, j;
	memcpy(working_processes, original_processes, sizeof(PROCESS) * p_cnt); // 작업용 프로세스 배열에 원본 복사
	PROCESS* rqueue[MAX_PROCESSES + 1]; // READY 상태의 프로세스들을 위한 큐
	PROCESS* wqueue[MAX_PROCESSES + 1]; // WAITING 상태의 프로세스들을 위한 큐
	int head = 0; // 큐에 새 프로세스를 추가할 위치
	int whead = 0; // WAITING 큐에 새 프로세스를 추가할 위치
	int max_time = 1500; // 시뮬레이션 최대 시간
	int done = 0;
	PROCESS* current_process = NULL;
	PROCESS* compare_process = NULL;
	for (i = 0; i < max_time; i++) {
		// 시뮬레이션 시간 단위마다 프로세스 상태 업데이트 및 스케줄링 로직 구현
		for (j = 0; j < p_cnt; j++) {
			if (working_processes[j].arrival_time == i && working_processes[j].state == NEW) {
				working_processes[j].state = READY;
				heap_push(rqueue, head, &working_processes[j], priority_compare);
				head++;
			}
		}
		// WAITING 큐 처리
		for (j = 0; j != whead; j++) {
			PROCESS* waiting_process = wqueue[j];
			waiting_process->io_burst_times[waiting_process->current_io_idx]--;
			if (waiting_process->io_burst_times[waiting_process->current_io_idx] == 0) {
				waiting_process->state = READY;
				waiting_process->current_io_idx++;
			}
		}
		// READY 큐에서 프로세스 선택 (Priority Preemptive)
		if (current_process == NULL) {
			current_process = heap_pop(rqueue, head, priority_compare);
			if (current_process != NULL) {
				head--;
				current_process->state = RUNNING;
			}
		}
		// 현재 실행 중인 프로세스와 READY 큐의 가장 높은 우선순위 프로세스 비교
		else {
			if (head > 0 && priority_compare(current_process, rqueue[0]) > 0) {
				compare_process = rqueue[0]; // READY 큐에서 가장 짧은 프로세스 확인
				compare_process = heap_pop(rqueue, head, priority_compare); // READY 큐에서 가장 짧은 프로세스 꺼내기
				head--;
				current_process->state = READY;
				heap_push(rqueue, head, current_process, priority_compare);
				head++;
				current_process = compare_process;
				current_process->state = RUNNING;
			}
		}
		// RUNNING 처리
		if (current_process != NULL) {
			current_process->cpu_used++;
			// I/O 요청
			// I/O 요청 시점의 context switch 비용이 발생하지 않는다고 가정
			if (current_process->cpu_used == current_process->burst_time) {
				current_process->state = TERMINATED;
				current_process->completion_time = i + 1; // 프로세스가 종료된 시점은 현재 tick이 끝난 시점이므로 i+1로 설정
				done++;
			}
			else if (current_process->current_io_idx < current_process->io_count &&
				current_process->cpu_used >= current_process->io_request_times[current_process->current_io_idx]) {
				current_process->state = WAITING;
				wqueue[whead] = current_process;
				whead++;
			}
		}
		for (j = 0; j != whead; j++) {
			PROCESS* waiting_process = wqueue[j];
			if (waiting_process->state == READY) {
				heap_push(rqueue, head, waiting_process, priority_compare);
				head++;
				wqueue[j] = wqueue[whead - 1]; // WAITING 큐에서 제거]
				whead--;
				j--; // 현재 인덱스에서 다음 프로세스를 확인하기 위해 인덱스 감소
			}
		}
		if (current_process != NULL) {
			result->gantt[i] = current_process->pid; // 현재 tick에 실행된 프로세스의 PID 기록
		}
		else {
			result->gantt[i] = 0; // CPU가 IDLE 상태인 경우 0으로 기록
		}
		//print_tick(i, current_process, rqueue, 0, head, wqueue, whead); // 매 tick 상태 출력
		if (current_process != NULL && (current_process->state == TERMINATED || current_process->state == WAITING)) {
			current_process = NULL;
		}
		if (done == p_cnt) {
			break; // 모든 프로세스가 종료되면 시뮬레이션 종료
		}
	}
	// 평균 계산
	int total_wait = 0, total_turnaround = 0, turn, wait;
	for (j = 0; j < p_cnt; j++) {
		turn = working_processes[j].completion_time - working_processes[j].arrival_time;
		wait = turn - working_processes[j].burst_time - working_processes[j].io_total;
		total_wait += wait;
		total_turnaround += turn;
	}

	// result에 저장
	strcpy(result->name, "Priority Preemptive");
	result->avg_waiting = (double)total_wait / p_cnt;
	result->avg_turnaround = (double)total_turnaround / p_cnt;
	result->gantt_len = i + 1;
	result->ran = 1;
}


void doRR(PROCESS* original_processes, int p_cnt, int time_quantum, RESULT *result) {
	PROCESS working_processes[MAX_PROCESSES];
	int i, j;
	memcpy(working_processes, original_processes, sizeof(PROCESS) * p_cnt); // 작업용 프로세스 배열에 원본 복사
	PROCESS* rqueue[MAX_PROCESSES + 1]; // READY 상태의 프로세스들을 위한 큐
	PROCESS* wqueue[MAX_PROCESSES + 1]; // WAITING 상태의 프로세스들을 위한 큐
	int head = 0; // 큐에 새 프로세스를 추가할 위치
	int tail = 0; // READY 큐에서 프로세스를 제거할 위치
	int whead = 0; // WAITING 큐에 새 프로세스를 추가할 위치
	int max_time = 1500; // 시뮬레이션 최대 시간
	int now_time = 0; // 현재 사용한 시간 퀀텀
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
		// WAITING 큐 처리
		for (j = 0; j != whead; j++) {
			PROCESS* waiting_process = wqueue[j];
			waiting_process->io_burst_times[waiting_process->current_io_idx]--;
			if (waiting_process->io_burst_times[waiting_process->current_io_idx] == 0) {
				waiting_process->state = READY;
				waiting_process->current_io_idx++;
			}
		}
		// READY 큐에서 프로세스 선택 (FCFS)
		if (current_process == NULL && head != tail) {
			current_process = rqueue[tail];
			tail = (tail + 1) % (MAX_PROCESSES + 1);
			current_process->state = RUNNING;
			now_time = 0; // 새로운 프로세스가 실행되면 시간 퀀텀 초기화
		}
		// RUNNING 처리
		if (current_process != NULL) {
			current_process->cpu_used++;
			now_time++;
			// I/O 요청
			// I/O 요청 시점의 context switch 비용이 발생하지 않는다고 가정
			if (current_process->cpu_used == current_process->burst_time) {
				current_process->state = TERMINATED;
				current_process->completion_time = i + 1; // 프로세스가 종료된 시점은 현재 tick이 끝난 시점이므로 i+1로 설정
				done++;
			}
			else if (current_process->current_io_idx < current_process->io_count &&
				current_process->cpu_used >= current_process->io_request_times[current_process->current_io_idx]) {
				current_process->state = WAITING;
				wqueue[whead] = current_process;
				whead++;
			} else if (now_time == time_quantum) { // 시간 퀀텀 도달 시
				current_process->state = READY;
				rqueue[head] = current_process;
				head = (head + 1) % (MAX_PROCESSES + 1);
			}
		}
		for (j = 0; j != whead; j++) {
			PROCESS* waiting_process = wqueue[j];
			if (waiting_process->state == READY) {
				rqueue[head] = waiting_process;
				head = (head + 1) % (MAX_PROCESSES + 1);
				wqueue[j] = wqueue[whead - 1]; // WAITING 큐에서 제거
				whead--;
				j--; // 현재 인덱스에서 다음 프로세스를 확인하기 위해 인덱스 감소
			}
		}
		if (current_process != NULL) {
			result->gantt[i] = current_process->pid; // 현재 tick에 실행된 프로세스의 PID 기록
		}
		else {
			result->gantt[i] = 0; // CPU가 IDLE 상태인 경우 0으로 기록
		}
		//print_tick(i, current_process, rqueue, tail, head, wqueue, whead); // 매 tick 상태 출력
		if (current_process != NULL && (current_process->state == TERMINATED || current_process->state == WAITING || current_process->state == READY)) {
			current_process = NULL;
		}
		if (done == p_cnt) {
			break; // 모든 프로세스가 종료되면 시뮬레이션 종료
		}
	}
	// 평균 계산
	int total_wait = 0, total_turnaround = 0, turn, wait;
	for (j = 0; j < p_cnt; j++) {
		turn = working_processes[j].completion_time - working_processes[j].arrival_time;
		wait = turn - working_processes[j].burst_time - working_processes[j].io_total;
		total_wait += wait;
		total_turnaround += turn;
	}

	// result에 저장
	strcpy(result->name, "RR");
	result->avg_waiting = (double)total_wait / p_cnt;
	result->avg_turnaround = (double)total_turnaround / p_cnt;
	result->gantt_len = i + 1;
	result->ran = 1;
}




void heap_push(PROCESS* HEAP[], int size, PROCESS* p, int (*compare)(PROCESS*, PROCESS*)) {
	HEAP[size] = p;
	int i = size;
	int parent;
	while (i > 0) {
		parent = (i - 1) / 2;
		if (compare(HEAP[i], HEAP[parent]) < 0) {
			PROCESS* temp = HEAP[i];
			HEAP[i] = HEAP[parent];
			HEAP[parent] = temp;
			i = parent;
		}
		else {
			break;
		}
	}
}

PROCESS* heap_pop(PROCESS* HEAP[], int size, int (*compare)(PROCESS*, PROCESS*)) {
	if (size == 0) return NULL;
	PROCESS* top = HEAP[0];
	HEAP[0] = HEAP[size - 1];
	if(size != 1) HEAP[size - 1] = NULL;
	int i = 0;
	int left, right, smallest;
	size--;
	while (1) {
		left = 2 * i + 1;
		right = 2 * i + 2;
		smallest = i;
		if (left < size && compare(HEAP[left], HEAP[smallest]) < 0) {
			smallest = left;
		}
		if (right < size && compare(HEAP[right], HEAP[smallest]) < 0) {
			smallest = right;
		}
		if (smallest != i) {
			PROCESS* temp = HEAP[i];
			HEAP[i] = HEAP[smallest];
			HEAP[smallest] = temp;
			i = smallest;
		}
		else {
			break;
		}
	}
	return top;
}

void print_gantt(int gantt[], int total) {
	printf("=== Gantt Chart ===\n");
	int t = 0;
	int pid, start;
	while (t < total) {
		pid = gantt[t];
		start = t;
		while (t < total && gantt[t] == pid) t++;
		if (pid == 0)
			printf("[IDLE %d~%d] ", start, t);
		else
			printf("[P%d %d~%d] ", pid, start, t);
	}
	printf("\n");
}

void print_tick(int t, PROCESS* current, PROCESS* rqueue[], int tail, int head, PROCESS* wqueue[], int whead) {
	printf("t=%3d | ", t);
	if (current != NULL)
		printf("CPU: P%d (cpu_used=%d) | ", current->pid, current->cpu_used);
	else
		printf("CPU: IDLE            | ");

	printf("READY: ");
	while (tail != head) {
		printf("P%d ", rqueue[tail]->pid);
		tail = (tail + 1) % (MAX_PROCESSES + 1);
	}
	int j;
	printf("| WAIT: ");
	for (j = 0; j < whead; j++)
		printf("P%d ", wqueue[j]->pid);

	printf("\n");
}

void print_result(const char* name, PROCESS* working_processes, int p_cnt) {
	printf("\n=== %s result ===\n", name);
	int total_wait = 0, total_turnaround = 0;
	int j, turnaround, waiting;
	for (j = 0; j < p_cnt; j++) {
		turnaround = working_processes[j].completion_time - working_processes[j].arrival_time;
		waiting = turnaround - working_processes[j].burst_time - working_processes[j].io_total;
		total_wait += waiting;
		total_turnaround += turnaround;
		printf("P%d: completion=%d, turnaround=%d, waiting=%d\n",
			working_processes[j].pid, working_processes[j].completion_time, turnaround, waiting);
	}
	printf("Average Waiting Time: %.2f\n", (double)total_wait / p_cnt);
	printf("Average Turnaround Time: %.2f\n", (double)total_turnaround / p_cnt);
}