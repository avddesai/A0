// File:	mypthread.c

// List all group members: Anirudh Srinivasa Raghavan (as4098), Ashwini Vijaysinh Desai (ad1727)
// iLab machine tested on:  working in local ubuntu machine, giving error on ilab machine(cp.cs.rutgers.edu)

#include "mypthread.h"
#define STACKSIZE 4096 //size of each context
#define Z 0 //zero flag
#define LEVELS 3 //priority levels of MLFQ
#define LIMIT -1 //limit for priority
#define QUANTUMTIME 5000 //size of quantum
#define TRESHOLD 10000 //threshold 
#define INIT_VALUE -1 //initil value of priority
#define FLAG 1 //one flag
static int id = Z; //thread id 
int init = INIT_VALUE; 
static sched* scheduler; 
struct itimerval it;
ucontext_t main_ctx;
mypthread_t Main;
mypthread_t threads[1024];

//initialize stack space for threads
void init_stackspace(mypthread_t* thread)
{
 		thread->context.uc_stack.ss_size = STACKSIZE;
        thread->context.uc_stack.ss_sp = malloc(STACKSIZE);
        thread->context.uc_link = &main_ctx; //so if this thread finishes, resume main
        thread->thread_id =thread->thread_id+ 1;
}

//creating thread space, stack pointers for thread, and adding to queue
/* create a new thread */
int mypthread_create(mypthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg)
{

   if (init == LIMIT) { 
                scheduler_initialization(); 
                init = Z;
                timer_init();
        }

    if(getcontext(&(thread->context)) == INIT_VALUE) { 
                printf("getcontext error\n");
                return -1;
        }
      init_stackspace(thread);
       
        makecontext(&(thread->context), (void *)run_thread, 3, thread, function, arg);
		
				setStatus(thread, READY);
    thread->priority =Z;
    insert_queue(&(scheduler->pq[Z]), thread);
        scheduler->thread_cur = NULL;
        schedule();
        return 0; 
};

//setting status to waiting or terminated
void setStatus(mypthread_t* thread, status flag)
{
	if(thread!=NULL)
	{
		thread-> thread_status=flag;
	}
}

void timer_init(){
	signal(SIGALRM, schedule);

	it.it_interval.tv_usec = QUANTUMTIME;

	it.it_value.tv_usec = QUANTUMTIME; 
	setitimer(ITIMER_REAL, &it, NULL); 
}


/* calls the scheduler program to run the next best thread */
int mypthread_yield()
{
	setStatus(scheduler->thread_cur, YIELD);
    schedule();
	return 0;
};

/* Wait for thread to terminate and picks the next best schedule*/
int mypthread_join(mypthread_t thread, void **value_ptr)
{
	while(thread.thread_status == TERMINATED){
		break;
	}
	mypthread_yield();
	thread.retval = value_ptr;

    return 0;
};


/* terminates a thread, frees stack space */
void mypthread_exit(void *value_ptr)
{
    scheduler->thread_cur->thread_status = TERMINATED;

    scheduler->thread_cur->retval = value_ptr;
    swapcontext(&scheduler->thread_cur->context, &Main.context);
};


/* initialize the mutex lock */
int mypthread_mutex_init(mypthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr)
{
	if (mutex==NULL)
	{
		return EINVAL;
	}
	mutex->flag=Z;
	return 0;
};

/* aquires mutex lock */
int mypthread_mutex_lock(mypthread_mutex_t *mutex)
{
	if (mutex==NULL)
	{
		mutex = malloc(sizeof(mypthread_mutex_t));
		mutex->waitq=malloc(sizeof(queue));
	}
	while(__sync_lock_test_and_set(&(mutex->flag), 1))
	mypthread_yield();
	return 0;
};

/* releases mutex lock */
int mypthread_mutex_unlock(mypthread_mutex_t *mutex)
{
	if (mutex==NULL)
	{
		return EINVAL;
	}
	__sync_synchronize();
	mutex->flag=Z;
	return 0;
};


/* destroys the mutex */
int mypthread_mutex_destroy(mypthread_mutex_t *mutex)
{
		if (mutex==NULL)
	{
		return EINVAL;
	}
	switch(mutex-> flag)
	{
		case 1:
		return EBUSY;
		break;
		case 0:
	
		mutex=NULL;
	}


};

/* schedule checks the macro definition and calls the appropriate algorithm */
static void schedule()
{

#ifdef DMLFQ 
	sched_MLFQ();
#else
	#ifdef PSJF
		sched_PSJF();
	#else
		sched_RR();
	#endif
#endif
}


// chooses next thread for round robin algorithm
mypthread_t * sched_chooseRR(){
	return remove_Queue(&(scheduler->pq[0]));
}

/* Round Robin scheduling algorithm */
static void sched_RR()
{

	sigset_t* mask= fire_alarm();
	
	mypthread_t * temp = scheduler->thread_cur;
	if(!isNull(temp)){

		switch(temp->thread_status)
		{
			case YIELD:
			temp->thread_status = READY;
         //   temp->priority = temp->priority;
            insert_queue(&(scheduler->pq[temp->priority]), temp);
	        break;
		}
	}
	scheduler->thread_cur = sched_chooseRR();
	
	if(isNull(scheduler->thread_cur)){
		run_thread(&Main, NULL, NULL);
	}

	setStatus(scheduler->thread_cur, RUNNING);
	//now swap contexts
	if(isNull(temp)){
		swapcontext(&main_ctx, &(scheduler->thread_cur->context));
	}
	else {
		
		swapcontext(&(temp->context), &(scheduler->thread_cur->context));
	}
	sigemptyset(&mask);
	

	return;
}

/* Preemptive PSJF (STCF) scheduling algorithm */
static void sched_PSJF()
{

}

/* Preemptive MLFQ scheduling algorithm */
/* Graduate Students Only */
static void sched_MLFQ() {
sigset_t* mask= fire_alarm();
	mypthread_t * temp = scheduler->thread_cur;
	if(!isNull(temp)){
		int np=0;
		switch(temp->thread_status)
		{
			case YIELD:
			 temp->thread_status = READY;
    temp->priority = temp->priority;
    insert_queue(&(scheduler->pq[temp->priority]), temp);
	break;

			case RUNNING:

			 np= (temp->priority)+FLAG > 2 ?2:  (temp->priority)+FLAG;
				
			    temp->thread_status = READY;
    temp->priority = np;
    insert_queue(&(scheduler->pq[np]), temp);
	break;

	case TERMINATED:
	case WAITING:
	break;
		}
	}

	scheduler->thread_cur = sched_choose();
	
	if(isNull(scheduler->thread_cur)){
		run_thread(&Main, NULL, NULL);
	}

	setStatus(scheduler->thread_cur, RUNNING);
	//now swap contexts
	if(isNull(temp)){
		swapcontext(&main_ctx, &(scheduler->thread_cur->context));
	}
	else {
		
		swapcontext(&(temp->context), &(scheduler->thread_cur->context));
	}
	sigemptyset(&mask);
	return;
}

// Feel free to add any other functions you need

// YOUR CODE HERE

//******
// HElper Functions 
//*****

// to insert queue
int Initialize_Queue(queue *Current_Queue)
{
    Current_Queue->tail = NULL;
    Current_Queue->size = Z;
    Current_Queue->head = NULL;
	return 1;
}
// to check if thread is null
int isNull(mypthread_t* thread)
{
	if(thread==NULL)
	{
		return 1;

	}
	else{
		return 0;
	}
}
// to peek queue
mypthread_t *peek_Queue(queue *Current_Queue)
{
    return Current_Queue->head;
}
// checker function to see if queue is empty
int isEmpty_Queue(queue *Current_Queue)
{
    return Current_Queue->size == Z;
}
// to insert queue
int insert_queue(queue *Current_Queue, mypthread_t *thr_node)
{
	if(Current_Queue==NULL)
	{
		return 0;
	}

	switch(Current_Queue-> size)
	{
		case 0:
		Current_Queue->head = thr_node;
        Current_Queue->tail = thr_node;
        Current_Queue->size++;
		break;

		default:
		Current_Queue->tail->next_thread = thr_node;
        Current_Queue->tail = thr_node;
        Current_Queue->size++;

	}

	return 1;
}
// to remove firsy element queue and return the value
mypthread_t *remove_Queue(queue *Current_Queue)
{
    if (Current_Queue->size == Z)
    {
        printf("Cant Remove");
        return NULL;
    }
    mypthread_t *tmp;
	switch(Current_Queue-> size)
	{
		case 1:
		tmp = Current_Queue->head;
        Current_Queue->head = NULL;
        Current_Queue->tail = NULL;
		break;

		default:
		tmp = Current_Queue->head;
        Current_Queue->head = Current_Queue->head->next_thread;

	}

    tmp->next_thread = NULL;
    Current_Queue->size--;
    return tmp;
}

// this function runs the thread
void run_thread(mypthread_t * thread_node, void *(*f)(void *), void * arg){

	setStatus(thread_node, RUNNING);
	scheduler->thread_cur = thread_node;
	thread_node = f(arg);
	setStatus(thread_node, TERMINATED);
	schedule();
}
// to queue initialize for mlfq
void initialize_queue( int num_levels)
{
	    scheduler->pq = (queue *)malloc(sizeof(queue)*num_levels); 
        int k;
        for(k = 0; k < LEVELS; k++){
          Initialize_Queue(&(scheduler->pq[k])); 
		}
}
//initialize scheduler structure and attributes, and invoke the main context
int scheduler_initialization(){
        if(init != LIMIT){
			return -1;
        }
        scheduler = malloc(sizeof(sched));
        getcontext(&main_ctx); 
        Main.context= main_ctx;
        Main.context.uc_link = NULL;
		setStatus(&Main, READY);
        scheduler->thread_main = &Main;
		setStatus(scheduler->thread_main, READY);
        scheduler->thread_main->thread_id = 0;
        scheduler->thread_cur = NULL;
		initialize_queue(LEVELS);

        }

//choose priority for a particular thread
mypthread_t * sched_choose(){
	int chosen_priority=-1;
	for(int i = 0; i < LEVELS; i++){//three levels
		if(scheduler->pq[i].head != NULL){
			chosen_priority=i;
		}
	}
	if(chosen_priority!=-1)
	{
		return remove_Queue(&(scheduler->pq[chosen_priority]));

	}
	else{
  		 return NULL;
	}
 
}

void signal_addset(sigset_t* mask)
{
	sigaddset (&mask, SIGALRM);
	sigaddset (&mask, SIGQUIT);
}

sigset_t* fire_alarm()
{
	struct sigaction act;
	sigset_t block_mask;
	sigemptyset (&block_mask);
	signal_addset(&block_mask);
	act.sa_handler = blankFunction;
	act.sa_mask = block_mask;
	act.sa_flags = Z;
	sigaction (SIGALRM, &act, NULL);
	return &block_mask; 
}

void blankFunction()
{
	return;
}


