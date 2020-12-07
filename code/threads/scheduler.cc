// scheduler.cc 
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would 
//	end up calling FindNextToRun(), and that would put us in an 
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "scheduler.h"
#include "main.h"

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

//int SJF_cmp(Thread *a, Thread *b) {
//       if(a->getBurstTime() < b->getBurstTime()) return -1;
//        else return 1;
//}

int FCFS_cmp (Thread *a, Thread *b) {
	return 1;
}
int Priority_cmp(Thread *a, Thread *b) {
	if(a->getPriority() < b->getPriority()) return -1;
	else return 1;
}
int SleepTime_cmp(SleepingThread* x, SleepingThread* y) {
        if (x->getSleepTime() < y->getSleepTime()) return -1;
        else return 1;
}
//Modified
Scheduler::Scheduler(SchedulerType sType)
{
	sleepingList = new SortedList<SleepingThread *>(SleepTime_cmp);
	toBeDestroyed = NULL;
	AThreadWakeUp = false;

	cout << "====== Scheduler type is " << sType << " ======\n";
	schedulerType = sType;
    	switch(schedulerType) {
    	case RR:
        	readyList = new List<Thread *>;
        	break;
//    	case SJF:
//        	readyList = new SortedList<Thread *>(SJF_cmp);
//        break;
    	case FCFS:
        	readyList = new SortedList<Thread *>(FCFS_cmp);
        break;
    	case Priority:
        	readyList = new SortedList<Thread *>(Priority_cmp);
	break;
	}

} 
//End Modified




//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{ 
    delete readyList;
//Modified 
//    delete sleepingList; 
//End Modified
} 

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void
Scheduler::ReadyToRun (Thread *thread)
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());

    thread->setStatus(READY);
    readyList->Append(thread);
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun ()
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (readyList->IsEmpty()) {
	return NULL;
    } else {
    	return readyList->RemoveFront();
    }
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void
Scheduler::Run (Thread *nextThread, bool finishing)
{
    Thread *oldThread = kernel->currentThread;
 
    //cout << "Current Thread" << oldThread->getName() << "    Next Thread"<<nextThread->getName()<<endl;
   
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing) {	// mark that we need to delete current thread
         ASSERT(toBeDestroyed == NULL);
	 toBeDestroyed = oldThread;
    }
    
#ifdef USER_PROGRAM			// ignore until running user programs 
    if (oldThread->space != NULL) {	// if this thread is a user program,
        oldThread->SaveUserState(); 	// save the user's CPU registers
	oldThread->space->SaveState();
    }
#endif
    
    oldThread->CheckOverflow();		    // check if the old thread
					    // had an undetected stack overflow

    kernel->currentThread = nextThread;  // switch to the next thread
    nextThread->setStatus(RUNNING);      // nextThread is now running
    
    DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());
    
    // This is a machine-dependent assembly language routine defined 
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    SWITCH(oldThread, nextThread);

    // we're back, running oldThread
      
    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

    CheckToBeDestroyed();		// check if thread we were running
					// before this one has finished
					// and needs to be cleaned up
    
#ifdef USER_PROGRAM
    if (oldThread->space != NULL) {	    // if there is an address space
        oldThread->RestoreUserState();     // to restore, do it.
	oldThread->space->RestoreState();
    }
#endif
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void
Scheduler::CheckToBeDestroyed()
{
    if (toBeDestroyed != NULL) {
        delete toBeDestroyed;
	toBeDestroyed = NULL;
    }
}
 
//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void
Scheduler::Print()
{
    cout << "Ready list contents:\n";
    readyList->Apply(ThreadPrint);
}

//Modified
void
Scheduler::GoSleep(Thread* t, int sleepTime) 
{
    IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);
    SleepingThread* st = new SleepingThread(t, sleepTime);
    sleepingList->Insert(st);
//    cout << st->getThread()->getName() << " is going to sleep for " << sleepTime << endl;
//    cout << sleepingList->Front()->getThread()->getName() 
//         << " (the front thread in bed) still need to sleep for " 
//         << sleepingList->Front()->getSleepTime() << endl;

    t->Sleep(FALSE);
    kernel->interrupt->SetLevel(oldLevel);

}

void 
Scheduler::AlarmTicks()
{
	AThreadWakeUp = false;
	ListIterator<SleepingThread*> iter(sleepingList);
	for (; !iter.IsDone(); iter.Next()) {
	    iter.Item()->decreaseSleepTime();
	}	
	while(!sleepingList->IsEmpty()){
	    SleepingThread* st = sleepingList->Front();
	    if(st->getSleepTime() == 0){
	        ReadyToRun(st->getThread());
		AThreadWakeUp = true;
//		cout << "SleepintList: ";
//		ListIterator<SleepingThread*> iter(sleepingList);
//		for (; !iter.IsDone(); iter.Next()) {
//			iter.Item()->getThread()->Print();
//			cout << " --> ";
//		}
//		cout << "END\n";
	        sleepingList->RemoveFront();
	    }
	    else {
//		if (st->getSleepTime()%100 == 0) cout << st->getThread()->getName() << " still need " << st->getSleepTime() << " to sleep.\n"; 
		break; 
	    }
	}
}

//End Modified
