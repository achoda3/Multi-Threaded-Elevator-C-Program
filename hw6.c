#include "hw6.h"
#include <stdio.h>
#include<pthread.h>
#include<assert.h>

pthread_mutex_t elevator_lock[ELEVATORS];
pthread_cond_t cond_var[PASSENGERS];
pthread_cond_t ret_var[ELEVATORS];

struct psg {
    int id;
    int from_floor;
    int to_floor;
    int elenumber;
    enum { WAITING = 1,
           RIDING = 2,
           DONE = 3 } state;
    struct psg* next;
} passengers[PASSENGERS];

struct ele {
    int current_floor;
    int direction;
    int occupancy;
    enum { ELEVATOR_ARRIVED = 1,
           ELEVATOR_OPEN = 2,
           ELEVATOR_CLOSED = 3 } state;
    struct psg* waiting;
    struct psg* riding;
} elevators[ELEVATORS];

// MUST hold lock and check for sufficient capacity before calling this,
// and MUST NOT be in the riding queue. adds to end of queue.
void add_to_list(struct psg** head, struct psg* passenger) {
    struct psg* p = *head;
    if (p == NULL)
        *head = passenger;
    else {
        while (p->next != NULL) {
            p = p->next;
        }
        p->next = passenger;
    }
}

// Must hold lock, does not update occupancy.
void remove_from_list(struct psg** head, struct psg* passenger) {
    struct psg* p = *head;
    struct psg* prev = NULL;
    while (p != passenger) {
        prev = p;
        p = (p)->next;
        // don't crash because you got asked to remove someone that isn't on this
        // list
        assert(p != NULL);
    }
    // removing the head - must fix head pointer too
    if (prev == NULL)
        *head = p->next;
    else
        prev->next = p->next;
    passenger->next = NULL;
}

void scheduler_init() {	
    for(int i=0; i<ELEVATORS; i++){
        elevators[i].current_floor=0;
        elevators[i].direction=-1;
        elevators[i].occupancy=0;
        elevators[i].state=ELEVATOR_CLOSED;
        elevators[i].riding=NULL;
        pthread_mutex_init(&elevator_lock[i],0);
        pthread_cond_init(&ret_var[i], NULL);
    }
    int j=0;
    for(int i=0; i<PASSENGERS; i++){
        passengers[i].state=WAITING;
        passengers[i].elenumber=j;
        add_to_list(&elevators[j].waiting, &passengers[i]);
        pthread_cond_init(&cond_var[i], NULL);
        j++;
        if(j==ELEVATORS){
            j=0;
        }
    }
    
}
// number of locks as elevators for continuos parallel

void passenger_request(int passenger, int from_floor, int to_floor, 
        void (*enter)(int, int), 
        void(*exit)(int, int))
{	
    passengers[passenger].from_floor=from_floor;
    passengers[passenger].to_floor=to_floor;
    passengers[passenger].id=passenger;
    int elevator = passengers[passenger].elenumber;
    // wait for the elevator to arrive at our origin floor, then get in
    int waiting = 1;
    while(waiting) {
        pthread_mutex_lock(&elevator_lock[elevator]);
        pthread_cond_wait(&cond_var[passenger], &elevator_lock[elevator]);
        if(elevators[elevator].current_floor == from_floor && elevators[elevator].state == ELEVATOR_OPEN && elevators[elevator].occupancy<MAX_CAPACITY) {
            enter(passenger, elevator);
            elevators[elevator].occupancy++;
            add_to_list(&elevators[elevator].riding, &passengers[passenger]);
            remove_from_list(&elevators[elevator].waiting, &passengers[passenger]);
            waiting=0;
            passengers[passenger].state=RIDING;
            pthread_cond_signal(&ret_var[elevator]);
        }
        pthread_mutex_unlock(&elevator_lock[elevator]);
    }

    // wait for the elevator at our destination floor, then get out
    int riding=1;
    while(riding) {
        pthread_cond_wait(&cond_var[passenger], &elevator_lock[elevator]);
        if(elevators[elevator].current_floor == to_floor && elevators[elevator].state == ELEVATOR_OPEN) {
            exit(passenger, elevator);
            elevators[elevator].occupancy--;
            remove_from_list(&elevators[elevator].riding, &passengers[passenger]);
            passengers[passenger].state=DONE;
            riding=0;
            pthread_cond_signal(&ret_var[elevator]);
        }

        pthread_mutex_unlock(&elevator_lock[elevator]);
    }
}

void elevator_ready(int elevator, int at_floor, 
        void(*move_direction)(int, int), 
        void(*door_open)(int), void(*door_close)(int)) {

    pthread_mutex_lock(&elevator_lock[elevator]);
    int direction = elevators[elevator].direction;
    int current_floor = elevators[elevator].current_floor;
    //printf("HI\n");
    if(elevators[elevator].state == ELEVATOR_ARRIVED) {
        //printf("awiodhwaidhwaipdhwapidh I REACHED AND DID A THING!!\n\n\n\n\n");
        door_open(elevator);
        elevators[elevator].state=ELEVATOR_OPEN;
        if(elevators[elevator].riding==NULL){
            pthread_cond_broadcast(&cond_var[elevators[elevator].waiting->id]);
        } else {
            pthread_cond_broadcast(&cond_var[elevators[elevator].riding->id]);
        }
        pthread_cond_wait(&ret_var[elevator], &elevator_lock[elevator]);
    }
    else if(elevators[elevator].state == ELEVATOR_OPEN) {
        door_close(elevator);
        elevators[elevator].state=ELEVATOR_CLOSED;
    }
    else {
        if(elevators[elevator].riding==NULL){
            if(elevators[elevator].waiting==NULL){
                pthread_mutex_destroy(&elevator_lock[elevator]);
                pthread_cond_destroy(&ret_var[elevator]);
                pthread_exit(NULL);
            }
            if(elevators[elevator].waiting->from_floor>at_floor){
                direction=1;
            } else if(elevators[elevator].waiting->from_floor<at_floor){
                direction=-1;
            }
            if(elevators[elevator].waiting->from_floor==at_floor){
                //printf("awiodhwaidhwaipdhwapidh I REACHED A THING!!\n\n\n\n\n");
                elevators[elevator].state=ELEVATOR_ARRIVED;
            } else {
                move_direction(elevator,direction);
                elevators[elevator].current_floor=at_floor+direction;
                if(elevators[elevator].waiting->from_floor==elevators[elevator].current_floor){
                    //printf("awiodhwaidhwaipdhwapidh I REACHED A THING!!\n\n\n\n\n");
                    elevators[elevator].state=ELEVATOR_ARRIVED;
                }
            }
            //printf("DIRECTION %d\n", direction);
            //printf("current floor, at floor, waiting floor %d %d %d\n", current_floor, at_floor, elevators[elevator].waiting->from_floor);
            
        } else {
            //printf("RIDING AINT NULL!\n");
            if(elevators[elevator].riding->to_floor>at_floor){
                direction=1;
            } else if(elevators[elevator].riding->to_floor<at_floor){
                direction=-1;
            } 
            if(elevators[elevator].riding->to_floor==at_floor){
                //printf("awiodhwaidhwaipdhwapidh I REACHED A THING!!\n\n\n\n\n");
                elevators[elevator].state=ELEVATOR_ARRIVED;
            } else {
                //printf("DIRECTION %d\n", direction);
                //printf("current floor, at floor, waiting floor %d %d %d\n", current_floor, at_floor, elevators[elevator].waiting->from_floor);
                move_direction(elevator,direction);
                elevators[elevator].current_floor=at_floor+direction;
                if(elevators[elevator].riding->to_floor==elevators[elevator].current_floor){
                    //printf("awiodhwaidhwaipdhwapidh I REACHED A THING!!\n\n\n\n\n");
                    elevators[elevator].state=ELEVATOR_ARRIVED;
                }
            }
            
        }
        
        
    }
    pthread_mutex_unlock(&elevator_lock[elevator]);
}
