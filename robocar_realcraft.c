#include "..\inc\kernel.h"
#include "..\inc\hal_robo.h"

#define TASK_STK_SZ             128
#define TASK_START_PRIO         1
#define TASK_CHKCOLLIDE_PRIO    2
#define TASK_CTRLMOTOR_PRIO     3
#define TASK_NAVIGFOLLOW_PRIO   4 // Merged navigation and line following task

OS_STK TaskStartStk[TASK_STK_SZ];
OS_STK ChkCollideStk[TASK_STK_SZ];
OS_STK CtrlmotorStk[TASK_STK_SZ];
OS_STK NavigFollowStk[TASK_STK_SZ]; // Stack for the merged task

struct robostate {
    int rspeed;
    int lspeed;
    char obstacle;
    char l1_done;
    char l2_done;
} myrobot;

void CheckCollision(void *data) {
    for(;;) {
        myrobot.obstacle = (robo_proxSensor() == 1) ? 1 : 0;
        OSTimeDlyHMSM(0, 0, 0, 50);
    }
}

void CntrlMotors(void *data) {
    for(;;) {
        robo_motorSpeed(myrobot.lspeed, myrobot.rspeed);
        OSTimeDlyHMSM(0, 0, 0, 50);
    }
}

// This is the main logic task. It handles line following and special tasks L1 & L2.
// Merging these functions prevents conflicting motor commands.
void NavigateAndFollow(void *data) {
    for(;;) {
        // --- L1 TASK: Detect bright light ---
        // This must be checked continuously.
        if (robo_lightSensor() > 60 && myrobot.l1_done == 0) {
            myrobot.l1_done = 1; // Set flag to ensure it only runs once
            robo_Honk();
        }

        // --- L2 TASK: Handle obstacle ---
        // This check has priority over line following.
        if (myrobot.obstacle == 1 && myrobot.l2_done == 0) {
            myrobot.l2_done = 1; // Set flag to ensure it only runs once
            
            // Execute the L2 maneuver
            robo_Honk();
            
            // Reverse to clear the obstacle
            myrobot.rspeed = -55;
            myrobot.lspeed = -55;
            OSTimeDlyHMSM(0, 0, 1, 0); // Reverse for 1 second

            // Turn to re-acquire the line (e.g., turn right)
            myrobot.rspeed = 55;
            myrobot.lspeed = -55;
            OSTimeDlyHMSM(0, 0, 0, 500); // Turn for 0.5 seconds

            // After maneuver, continue the loop to start line-following again
            continue; 
        }

        // --- LINE FOLLOWING LOGIC ---
        // This runs if no special tasks are being executed.
        unsigned char ir = robo_lineSensor();
        
        switch(ir) {
            // 001
            case 1: 
                myrobot.lspeed = 40;
                myrobot.rspeed = 0;
                OSTimeDlyHMSM(0, 0, 0, 5);
                break;
            /// 010
            case 2: 
                myrobot.lspeed = 40;
                myrobot.rspeed = 40;
                OSTimeDlyHMSM(0, 0, 0, 50);
                break;
            // 011
            case 3: 
                myrobot.lspeed = 40;
                myrobot.rspeed = -40;
                OSTimeDlyHMSM(0, 0, 0, 50);
                break;
            // 100
            case 4:
                myrobot.lspeed = 0;
                myrobot.rspeed = 40;
                OSTimeDlyHMSM(0, 0, 0, 5);
                break;
            // 101
            case 5: 
            // after bridge
                myrobot.lspeed = 0;
                myrobot.rspeed = 40;
                OSTimeDlyHMSM(0, 0, 0, 5);
                break;
            // 110
            case 6: 
                myrobot.lspeed = -40;
                myrobot.rspeed = 40;
                OSTimeDlyHMSM(0, 0, 0, 50);
                break;
            // 111
            case 7: 
                myrobot.lspeed = 40;
                myrobot.rspeed = 40;
                OSTimeDlyHMSM(0, 0, 0, 50);
                break;
            // Default Case: Line is lost. Execute recovery maneuver.
            default:
                    myrobot.lspeed = -40;
                    myrobot.rspeed = -40;
                OSTimeDlyHMSM(0, 0, 0, 50);
                break;
        }
    }
}

void TaskStart(void *data) {
    OS_ticks_init();

    OSTaskCreate(CheckCollision, (void *)0,
                 (void *)&ChkCollideStk[TASK_STK_SZ - 1],
                 TASK_CHKCOLLIDE_PRIO);

    OSTaskCreate(CntrlMotors, (void *)0,
                 (void *)&CtrlmotorStk[TASK_STK_SZ - 1],
                 TASK_CTRLMOTOR_PRIO);

    OSTaskCreate(NavigateAndFollow, (void *)0,
                 (void *)&NavigFollowStk[TASK_STK_SZ - 1],
                 TASK_NAVIGFOLLOW_PRIO);

     // This loop handles the 50Hz LED blink for the L1 task.
    // It runs independently of the navigation logic.
    for(;;) {
        if(myrobot.l1_done){
            robo_LED_toggle(); // Toggle LED
        }
        // Delay of 10ms creates a 20ms toggle period (10ms on, 10ms off) = 50Hz.
        OSTimeDlyHMSM(0, 0, 0, 10); 
    }
}

int main(void) {
    robo_Setup();
    OSInit();

    myrobot.rspeed = STOP_SPEED;
    myrobot.lspeed = STOP_SPEED;
    myrobot.obsta
    cle = 0;
    myrobot.l1_done = 0;
    myrobot.l2_done = 0;

    robo_motorSpeed(0, 0); // Assuming STOP_SPEED is 0
    OSTaskCreate(TaskStart,
                 (void *)0,
                 (void *)&TaskStartStk[TASK_STK_SZ - 1],
                 TASK_START_PRIO);

    robo_Honk();
    robo_wait4goPress();  // Wait for start button
    OSStart();

    while(1); //�never�reached
}
