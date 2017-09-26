#include <stdio.h>
#include <sys/types.h>
#include <sys/prex.h>
#include <sys/termios.h>

struct timerinfo freq;/*Holds clock frequency*/

uint32_t resetVal = 0;/*resetVal in ms*/
uint32_t pauseClock = 0;/*PauseClock variable,0 when clock not paused*/
uint8_t startStop = 0;
uint32_t timer_interrupt_interval = 10;/*Time interval between the interrupts in ms*/
uint32_t h_clock = 0;/*clock value in ms*/


enum CLOCK_STATE{
	CLOCK_MODE, STOPWATCH_MODE
} current_mode = STOPWATCH_MODE;

/*	keyboard inputs : This function takes care of keyboard press, and handles capital and non-capital   letters as inputs
	If no keyboard key pressed , return back
*/
void getKeyboardInput()
{
	char input = getchar();
	if(!input)
		return;

	switch(input)
	{		
		case 'Z':      
		case 'z':
			h_clock = h_clock - (h_clock % 60000);
		break;
		
		case 'T':
		case 't':
			if(current_mode == CLOCK_MODE){
			current_mode = STOPWATCH_MODE;
			}
			else{			
			current_mode=CLOCK_MODE;
			}
		break;
		case 'H':   
		case 'h':
			h_clock += 3600000;
		break;
		case 'M':       
		case 'm':
			h_clock += 60000;
		break;

		case 'R':        
		case 'r':
			resetVal = 0;
		break;
		case 'S':	  
		case 's':
			startStop = !startStop;
		break;
		case 'P':	
		case 'p':
			if(pauseClock==0){
			   pauseClock=resetVal;
			}
			else{
			   pauseClock=0;			
			}
			
		break;
		
	}
}

/* This void function Handles the interrupts (Void input) */

void setupInterrupt()
{
	u_long t1;
	static uint32_t last_interrupt = 0;
	static uint32_t remainder = 0;
	uint32_t keep_ticks; 	

	sys_time(&t1);
  	uint32_t temp=24*60*60*1000;	

	/* In each interrupt keep ticks lost and add as a remainder */
	
	uint32_t ticks = t1*1000 + remainder;
	uint32_t now =  ticks / freq.hz;
	remainder = ticks - now*freq.hz;
	
	/*Measure  current interrupt.If previous=0,get initial value (10 ms) */
	if(last_interrupt){
	  keep_ticks=now-last_interrupt; 
	}
	else{
	  keep_ticks=timer_interrupt_interval; 
	}	
	
	resetVal += startStop*keep_ticks;
	h_clock = (h_clock + keep_ticks);/* When time isafter 12 ,hours=00 (24 hours format) */
	if(h_clock>=temp) h_clock=h_clock%temp;

	if(current_mode == STOPWATCH_MODE){
		if(pauseClock)
			showTime(pauseClock,1);
		else
			showTime(resetVal, 1);		
	}	
	else{
		showTime(h_clock, 0);
	}
	last_interrupt = now;
	timer_alarm(timer_interrupt_interval, 0);
	exception_return();
}

/* flag shows which task is now happening ,CLOCK(flag=0) or TIMER(flag!=0)  */

void showTime(uint32_t ms, uint8_t flag)
{
	/*variables h,m,s hold hours,minutes and seconds .Passed in function by reference   */
	uint32_t h,m,s;
	
	/*	Separate ms in hours, minutes,and seconds in order to print 
	clock's time into string with HH:MM:SS format
	*/

	h = ms/(60*60*1000);
	ms -= h*(60*60*1000);

	m = ms/(60*1000);
	ms -= m*(60*1000);

	s = ms*0.001;
	ms -= s*1000;
	
	/* (\r) Helps overwritings previous printf variables . Screen reloads with  specific format HH:MM:SS*/

	if(flag!=0)
		printf("%02u:%02u:%02u.%02u\r", h, m, s, ms/10);
	else
		printf("   %02u:%02u:%02u\r", h, m, s);
	
}


int main(int argc, char *argv[])
{
	
	sys_info(INFO_TIMER, &freq);
	exception_setup(setupInterrupt);
	timer_alarm(timer_interrupt_interval, 0);
	startStop = 1;

	/* Infinite loop .Handling exceptions from keyboard*/
	while(1)  
	{
		getKeyboardInput();
		
		/*CPU rests for 290 ms */
		/*Sleep solves the problem with a CPU running full time ...*/
		/*Effectively decreases energy spent */

		timer_sleep(290, 0); 		
	}
}
