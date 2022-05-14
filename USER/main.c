#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "beep.h"
#include "rtc.h"
#include "tim3.h"
#include "rgb_led.h"
#include "esp8266.h"
#include "esp8266_mqtt.h"
#include "includes.h"

OS_TCB TASK_SYSTEM_INIT_TCB;
void task_system_init(void *parg);
CPU_STK task_system_init_stk[512];	


OS_TCB TASK_RGB_LED_TCB;
void task_rgb_led(void *parg);
CPU_STK task_rgb_led_stk[512];		

OS_TCB TASK_RTC_TCB;
void task_rtc(void *parg);
CPU_STK task_rtc_stk[512];	

OS_TCB TASK_MQTT_CONNECT_TCB;
void task_mqtt_connect(void *parg);
CPU_STK task_mqtt_connect_stk[512];			


OS_TCB TASK_RX_BUF_DEAL_TCB;
void task_rx_buf_deal(void *parg);
CPU_STK task_rx_buf_deal_stk[128];			



OS_MUTEX				g_mutex_printf;		
OS_FLAG_GRP   			g_flag_grp;			


#define DEBUG_PRINTF_EN	1
void dgb_printf_safe(const char *format, ...)
{
#if DEBUG_PRINTF_EN	
	OS_ERR err;
	
	va_list args;
	va_start(args, format);
	
	OSMutexPend(&g_mutex_printf,0,OS_OPT_PEND_BLOCKING,NULL,&err);	
	vprintf(format, args);
	OSMutexPost(&g_mutex_printf,OS_OPT_POST_NONE,&err);
	
	va_end(args);
#else
	(void)0;
#endif
}


int main(void)
{
	OS_ERR err;

	systick_init();  													
	
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);						
	
	usart_init(115200);  				 								

	OSInit(&err);

	OSTaskCreate(	(OS_TCB *)&TASK_SYSTEM_INIT_TCB,
					(CPU_CHAR *)"task_system_init",	
					(OS_TASK_PTR)task_system_init,	
					(void *)0,						
					(OS_PRIO)6,						
					(CPU_STK *)task_system_init_stk,
					(CPU_STK_SIZE)512/10,			
					(CPU_STK_SIZE)512,				
					(OS_MSG_QTY)0,					
					(OS_TICK)0,						
					(void  *)0,						
					(OS_OPT)OS_OPT_TASK_NONE,		
					&err												
				);
					
	if(err!=OS_ERR_NONE)
	{
		printf("task 1 create fail\r\n");
		
		while(1);
	
	}

					

	//启动系统调度
	OSStart(&err);
							
	printf(".......\r\n");
					
	while(1);
	
}


void task_system_init(void *parg)
{
	int32_t rt;
	OS_ERR err;
	printf("task_system_init is create ok\r\n");
	
	//8266引脚初始化
	esp8266_init();

	//连接路由器
	while(esp8266_connect_ap());
	
	//同步本地时间
	rt = sync_local_time();
	if(rt == -1)			
	{
		printf("sync time fail\r\n");
		while(1);
	}
	else if(rt == 1)		
	{
		if(esp8266_exit_transparent_transmission()==0)	
			printf("exit transmission success\r\n");
		
		while(esp8266_disconnect_server())				
			printf("disconnect time server fail\r\n");
		printf("disconnect time server success\r\n");
	}
	printf("sync time success\r\n");
	
	
	
	
	//LED_Init();
         											
	beep_init();
	rgb_led_init();
	
	tim3_init();
	
	//创建互斥锁
	OSMutexCreate(&g_mutex_printf,	"g_mutex_printf",&err);
	
	//创建事件标志组
	OSFlagCreate(&g_flag_grp,"g_flag_grp",0,&err);
	
	
	//创建任务
	OSTaskCreate(	(OS_TCB *)&TASK_RGB_LED_TCB,							
					(CPU_CHAR *)"task_rgb_led",								
					(OS_TASK_PTR)task_rgb_led,								
					(void *)0,												
					(OS_PRIO)7,									
					(CPU_STK *)task_rgb_led_stk,				
					(CPU_STK_SIZE)512/10,						
					(CPU_STK_SIZE)512,							
					(OS_MSG_QTY)0,								
					(OS_TICK)0,									
					(void  *)0,									
					(OS_OPT)OS_OPT_TASK_NONE,					
					&err										
				);
					
	OSTaskCreate(	(OS_TCB *)&TASK_RTC_TCB,					
					(CPU_CHAR *)"task_rtc",						
					(OS_TASK_PTR)task_rtc,						
					(void *)0,									
					(OS_PRIO)7,									
					(CPU_STK *)task_rtc_stk,					
					(CPU_STK_SIZE)512/10,						
					(CPU_STK_SIZE)512,										
					(OS_MSG_QTY)0,											
					(OS_TICK)0,												
					(void  *)0,												
					(OS_OPT)OS_OPT_TASK_NONE,								
					&err													
				);
					
	OSTaskCreate(	(OS_TCB *)&TASK_MQTT_CONNECT_TCB,						
					(CPU_CHAR *)"task_mqtt_connect",						
					(OS_TASK_PTR)task_mqtt_connect,							
					(void *)0,												
					(OS_PRIO)6,											 	
					(CPU_STK *)task_mqtt_connect_stk,						
					(CPU_STK_SIZE)512/10,									
					(CPU_STK_SIZE)512,										
					(OS_MSG_QTY)0,											
					(OS_TICK)0,												
					(void  *)0,												
					(OS_OPT)OS_OPT_TASK_NONE,								
					&err													
				);
					
	OSTaskCreate(	(OS_TCB *)&TASK_RX_BUF_DEAL_TCB,						
					(CPU_CHAR *)"task_rx_buf_deal",							
					(OS_TASK_PTR)task_rx_buf_deal,							
					(void *)0,												
					(OS_PRIO)6,											 	
					(CPU_STK *)task_rx_buf_deal,							
					(CPU_STK_SIZE)128/10,									
					(CPU_STK_SIZE)128,										
					(OS_MSG_QTY)0,											
					(OS_TICK)0,												
					(void  *)0,												
					(OS_OPT)OS_OPT_TASK_NONE,								
					&err													
				);
					
	//删除自身任务,进入挂起状态
	OSTaskDel(NULL,&err);
}



//mqtt 连接
void task_mqtt_connect(void *parg)
{
	OS_ERR 		err;
	
	printf("task_mqtt_connect is create ok\r\n");
	
	AliIoT_Parameter_Init();		

	while(1)
	{
		OSFlagPend(&g_flag_grp,FLAG_GRP_MQTT_CONNECT,0,\
										OS_OPT_PEND_FLAG_CLR_ALL+\
										OS_OPT_PEND_BLOCKING, NULL, &err);		
		mqtt_buffer_init();

		while(esp8266_mqtt_init());
	}
}


//mqtt接收消息处理
void task_rx_buf_deal(void *parg)
{
	OS_ERR 		err;
	
	printf("task_rx_buf_deal is create ok\r\n");
	

	while(1)
	{
		OSFlagPend(&g_flag_grp,FLAG_GRP_ESP8266_RX_END,0,\
										OS_OPT_PEND_FLAG_SET_ALL+\
										OS_OPT_PEND_BLOCKING, NULL, &err);		
		printf("\r\nrx end\r\n\r\n");
		
		OSFlagPost(&g_flag_grp,FLAG_GRP_ESP8266_RX_END,OS_OPT_POST_FLAG_CLR,&err);
	}
}


//led
void task_rgb_led(void *parg)
{
	int color=1;
	
	printf("task_rgb_led is create ok\r\n");

	while(1)
	{
		rgb_led_set_color(color++);
		
		delay_ms(1000);
		if(color >  7)
			color = 1;
	}
}

//rtc 
void task_rtc(void *parg)
{
	OS_ERR err;
	RTC_TimeTypeDef  RTC_TimeStructure;
	RTC_DateTypeDef  RTC_DateStructure;
	
	printf("task_rtc is create ok\r\n");
	

	while(1)
	{
		OSFlagPend(&g_flag_grp,FLAG_GRP_RTC_WAKEUP,0,OS_OPT_PEND_FLAG_SET_ALL +\
													OS_OPT_PEND_FLAG_CONSUME + \
													OS_OPT_PEND_BLOCKING, NULL, &err);
		RTC_GetDate(RTC_Format_BCD,&RTC_DateStructure);
		printf("20%02x/%02x/%02x Week:%x ",RTC_DateStructure.RTC_Year,RTC_DateStructure.RTC_Month,RTC_DateStructure.RTC_Date,RTC_DateStructure.RTC_WeekDay);			
				
		RTC_GetTime(RTC_Format_BCD,&RTC_TimeStructure);
		printf("%02x:%02x:%02x\r\n",RTC_TimeStructure.RTC_Hours,RTC_TimeStructure.RTC_Minutes,RTC_TimeStructure.RTC_Seconds);
			
	}
}








