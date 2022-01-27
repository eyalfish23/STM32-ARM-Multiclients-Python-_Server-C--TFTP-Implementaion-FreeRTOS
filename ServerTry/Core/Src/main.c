/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "lwip.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "udp.h"
#include "ip_addr.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/dns.h"

#define PORT_NUM 69
#define BLOCK_SIZE 512
#define FILENAME_MAX_LEN 30
#define DATA_MAX_SIZE 1024 //2 BLOCKS MAX -1024 BYTES OF DATA
#define SERVER_MAX_FILES 10 //MAXIMUM 10 FILES CAN BE STORED
#define MODE_MAX_LEN 20
#define RECEIVED_PACKET_MAX_LEN 540 //512 Max block size + addition informations
#define BLOCK_7_FLASH_MEM_FIRST_ADDRESS 0x080C0000 //ATIM, Sector 7 (first address)
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

UART_HandleTypeDef huart3;

PCD_HandleTypeDef hpcd_USB_OTG_FS;

osThreadId defaultTaskHandle;
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USB_OTG_FS_PCD_Init(void);
void StartDefaultTask(void const * argument);

/* USER CODE BEGIN PFP */
void vSessionTask(void const * argument);
int write_data_to_flash_memory(uint8_t* filename,uint8_t* flash_mem_buffer);
int read_data_from_flash_memory(uint8_t *file_index);
void print_stored_files(uint8_t *file_index);
void get_all_files(char* buffer,int* buffer_size,uint8_t *file_index);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int __io_putchar(int ch)
{
	HAL_UART_Transmit(&huart3, (uint8_t *)&ch, 1, 0xFFFF);
	return ch;
}

struct A_FILE
{
	uint8_t filename[FILENAME_MAX_LEN];
	uint8_t data_content[DATA_MAX_SIZE];
};

struct Session
{
	uint16_t client_port;
	ip_addr_t host_addr;
	uint16_t session_opcode;
	uint8_t filename[FILENAME_MAX_LEN];
	uint8_t unused1;
	uint8_t unused2;
	uint8_t mode[MODE_MAX_LEN];
};

extern struct netif gnetif;
struct udp_pcb *conn;

uint32_t flash_addr=BLOCK_7_FLASH_MEM_FIRST_ADDRESS; //ATIM, Sector 7 (first address)
int i;
struct A_FILE files[SERVER_MAX_FILES];
const uint16_t write_opcode=2,read_opcode=1,data_opcode=3,ack_opcode=4,err_opcode=5,show_files=9;
const uint16_t file_not_found_error_code=1,file_already_exists_error_code=6,writing_reading_error_code=3;

uint8_t file_index=0;
uint8_t empty_message=0;

SemaphoreHandle_t flash_mem_semph=0;
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART3_UART_Init();
  MX_USB_OTG_FS_PCD_Init();
  /* USER CODE BEGIN 2 */
  //Read files from flash memory
  if(read_data_from_flash_memory(&file_index)<0)
  {
 	 Error_Handler();
  }

  //Print stored files (If any)
  print_stored_files(&file_index);
  printf("TFTP Server start working\r\n");
  /* USER CODE END 2 */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  vSemaphoreCreateBinary(flash_mem_semph);
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 256);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  //Dynamic task allocation

  /* USER CODE END RTOS_THREADS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 72;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 3;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief USB_OTG_FS Initialization Function
  * @param None
  * @retval None
  */
static void MX_USB_OTG_FS_PCD_Init(void)
{

  /* USER CODE BEGIN USB_OTG_FS_Init 0 */

  /* USER CODE END USB_OTG_FS_Init 0 */

  /* USER CODE BEGIN USB_OTG_FS_Init 1 */

  /* USER CODE END USB_OTG_FS_Init 1 */
  hpcd_USB_OTG_FS.Instance = USB_OTG_FS;
  hpcd_USB_OTG_FS.Init.dev_endpoints = 6;
  hpcd_USB_OTG_FS.Init.speed = PCD_SPEED_FULL;
  hpcd_USB_OTG_FS.Init.dma_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
  hpcd_USB_OTG_FS.Init.Sof_enable = ENABLE;
  hpcd_USB_OTG_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.lpm_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.vbus_sensing_enable = ENABLE;
  hpcd_USB_OTG_FS.Init.use_dedicated_ep1 = DISABLE;
  if (HAL_PCD_Init(&hpcd_USB_OTG_FS) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB_OTG_FS_Init 2 */

  /* USER CODE END USB_OTG_FS_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LD1_Pin|LD3_Pin|LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(USB_PowerSwitchOn_GPIO_Port, USB_PowerSwitchOn_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : USER_Btn_Pin */
  GPIO_InitStruct.Pin = USER_Btn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USER_Btn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD1_Pin LD3_Pin LD2_Pin */
  GPIO_InitStruct.Pin = LD1_Pin|LD3_Pin|LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : USB_PowerSwitchOn_Pin */
  GPIO_InitStruct.Pin = USB_PowerSwitchOn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(USB_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : USB_OverCurrent_Pin */
  GPIO_InitStruct.Pin = USB_OverCurrent_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USB_OverCurrent_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */
static void udp_server_recv(void *arg, struct udp_pcb *upcb, struct pbuf *p,const ip_addr_t *addr, u16_t port)
{
	//Waiting for clients
	struct Session *session=NULL;
	uint16_t num_bytes_filename,num_bytes_mode;

	session=(struct Session*)malloc(sizeof(struct Session));
	session->client_port=port;
	session->host_addr=*addr;
	memcpy(&session->session_opcode,p->payload,2);//get opcode (2B)
	num_bytes_filename=sprintf((char*)session->filename,"%s",(char*)p->payload+2); //n bytes filename
	memcpy(&session->unused1,p->payload+2+num_bytes_filename+1,1); //1 byte unused
	num_bytes_mode=sprintf((char*)session->mode,"%s",(char*)p->payload+2+num_bytes_filename+2); //n bytes mode
	memcpy(&session->unused2, p->payload+2+num_bytes_filename+2+num_bytes_mode+1,1); //1 byte unused

	pbuf_free(p);

	//Create Session for this client
	osThreadDef(defaultSessionTask, vSessionTask, osPriorityNormal, 0, 256);
	osThreadCreate(osThread(defaultSessionTask), session);

}

void vSessionTask(void const *argument)
{
	struct Session *session=NULL;
	uint8_t *send_packet=NULL;
	uint16_t client_port,opcode,blocknum,get_blocknum,received_opcode,num_bytes_data;
	uint8_t i,file_ref=-1,show_none_size=9;
	int sockfd,block_len,get_files_buffer_size=0;
	socklen_t len,res;
	struct sockaddr_in servaddr,client_addr;
	uint8_t buffer[RECEIVED_PACKET_MAX_LEN];
	uint8_t flash_mem_buffer[DATA_MAX_SIZE];
	ip_addr_t host_addr;
	char *get_files_buffer=NULL;


	session=(struct Session*)argument;

	client_port=session->client_port;
	host_addr=session->host_addr;
	opcode=session->session_opcode;


	memset(&client_addr,0,sizeof(client_addr));
	memset(&servaddr,0,sizeof(servaddr));

	bzero(&client_addr, sizeof(client_addr));
	client_addr.sin_family=AF_INET; // IPv4
	client_addr.sin_addr.s_addr=host_addr.addr;
	client_addr.sin_port = htons(client_port);
	len=sizeof(client_addr);

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0))<0)
	{
	        printf("Socket creation failed\r\n");
	        free(session);
	        session=NULL;
	        vTaskDelete(NULL);
	        return;
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family=AF_INET; // IPv4
	servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
	servaddr.sin_port=htons(client_port);

	//Bind client_port to socket
	if (bind(sockfd,(const struct sockaddr*)&servaddr,sizeof(servaddr))<0)
	{
		printf("Bind failed\r\n");
		free(session);
		session=NULL;
		vTaskDelete(NULL);
	    return;
	}

	printf("Client successfully created and binded to port: %d\r\n",client_port);
	len=sizeof(client_addr);


		if(opcode==show_files)
		{
			//Show files
			get_files_buffer=malloc(70*sizeof(*get_files_buffer));
			get_all_files(get_files_buffer,&get_files_buffer_size,&file_index);

			if(get_files_buffer_size==0)
			{
				//No files to show
				//Send back to client with special sign (=9)
				send_packet=(uint8_t*)malloc(1*sizeof(uint8_t));
				memcpy(send_packet,&show_none_size,1);
				//Send packet containing file's content back to client
				sendto(sockfd,send_packet,1,0,(struct sockaddr*)&client_addr, sizeof(client_addr));
			}
			else
			{
				//Send back to client
				send_packet=(uint8_t*)malloc(get_files_buffer_size*sizeof(uint8_t));
				memcpy(send_packet,get_files_buffer,get_files_buffer_size);
				//Send packet containing file's content back to client
				sendto(sockfd,send_packet,get_files_buffer_size,0,(struct sockaddr*)&client_addr, sizeof(client_addr));
			}

			free(send_packet);
			send_packet=NULL;
			free(get_files_buffer);
			get_files_buffer=NULL;
			//Close Session
			free(session);
			session=NULL;
			vTaskDelete(NULL);
			return;
		}else if(opcode==read_opcode)
		{
				//Read request
				//Search for file in the memory
				for(i=0;i<file_index;i++)
				{
					if(strcmp((char*)session->filename,(char*)files[i].filename)==0)
					{
						file_ref=i;
					}
				}

				//Send data back(opcode=1)
				//2B (opcode=3) | 2B (block number) | 0-512B (Block data)
				if(file_ref>=0) //file has been found
				{
					blocknum=1;
					block_len=strlen((char*)files[file_ref].data_content);
					if(block_len<BLOCK_SIZE)
					{
						//Send block len
						send_packet=(uint8_t*)malloc(4+block_len);
						memcpy(send_packet,&data_opcode,2);
						memcpy(send_packet+2,&blocknum,2);
						memcpy(send_packet+4,files[file_ref].data_content+(blocknum-1),block_len);
						//Send packet containing file's content back to client
						sendto(sockfd,send_packet,4+block_len,0,(struct sockaddr*)&client_addr, sizeof(client_addr));
					}
					else
					{
						//Send max block num - 512 Bytes
						send_packet=(uint8_t*)malloc(4+BLOCK_SIZE);
						memcpy(send_packet,&opcode,2);
						memcpy(send_packet+2,&blocknum,2);
						memcpy(send_packet+4,files[file_ref].data_content+(blocknum-1),BLOCK_SIZE);
						//Send packet containing file's content back to client
						sendto(sockfd,send_packet,4+BLOCK_SIZE,0,(struct sockaddr*)&client_addr, sizeof(client_addr));
					}

					free(send_packet);
					send_packet=NULL;
				}
				else
				{
					//No such file
					//Send error packet(opcode=5) NACK
					//2B (opcode=5) | 2B (error code)
					block_len=4;
					send_packet=(uint8_t*)malloc(block_len);
					memcpy(send_packet,&err_opcode,2); //error opcode (2bytes)
					memcpy(send_packet+2,&file_not_found_error_code,2); //err code (2bytes)
					sendto(sockfd,send_packet,block_len,0,(struct sockaddr*)&client_addr, sizeof(client_addr));

					free(send_packet);
					send_packet=NULL;

					//Close Session
					free(session);
					session=NULL;
					vTaskDelete(NULL);
					return;
				}


			}else if(opcode==write_opcode) //Write request from client
			{
				//Write request (OPCODE = 2)

				//Check if filename already exists
				for(i=0;i<file_index;i++)
				{
					if(strcmp((char*)session->filename,(char*)files[i].filename)==0)
					{
						//File already exists
						printf("File already exists\r\n");
						//Send error packet(opcode=5) NACK
						//2B (opcode=5) | 2B (error code)
						block_len=4;
						send_packet=(uint8_t*)malloc(block_len);
						memcpy(send_packet,&err_opcode,2); //error opcode (2bytes)
						memcpy(send_packet+2,&file_already_exists_error_code,2); //err code (2bytes)
						sendto(sockfd,send_packet,block_len,0,(struct sockaddr*)&client_addr, sizeof(client_addr));

						free(send_packet);
						send_packet=NULL;

						//Close Session
						free(session);
						session=NULL;
						vTaskDelete(NULL);
						return;
					}
				}
				//Send ACK back to client(OPCODE = 4)
				blocknum=0; //Starting from first block to send
				block_len=4;
				send_packet=(uint8_t*)malloc(block_len);
				memcpy(send_packet,&ack_opcode,2); //ack opcode (2bytes)
				memcpy(send_packet+2,&blocknum,2); //block number (2bytes)

				sendto(sockfd,send_packet,block_len,0,(struct sockaddr*)&client_addr, sizeof(client_addr));

				free(send_packet);
				send_packet=NULL;
			}else
			{
				//Invalid opcode
				printf("Invalid opcode\r\n");
				//Close Session
				free(session);
				session=NULL;
				vTaskDelete(NULL);
				return;
			}

	for(;;)
	{
		recvfrom(sockfd,buffer,sizeof(buffer),0,(struct sockaddr*)&client_addr,&len);
		//Extract packet's data
		memcpy(&received_opcode,buffer,2); //Get opcode

		if(received_opcode==data_opcode) //Data to write to memory
		{
			//Data to write (OPCODE = 3)
			memcpy(&get_blocknum,buffer+2,2); //Get blocknum
			num_bytes_data=sprintf((char*)flash_mem_buffer+(get_blocknum*BLOCK_SIZE),"%s",buffer+4); //n bytes data

			if(num_bytes_data<=BLOCK_SIZE) //Last block
			{
				//Copy to flash memory
				xSemaphoreTake(flash_mem_semph,osWaitForever);
				res=write_data_to_flash_memory(session->filename,flash_mem_buffer);
				osSemaphoreRelease(flash_mem_semph);
				if(res<0)
				{
					//fault
					printf("Error in writing to flash\r\n");
					//Send error packet(opcode=5) NACK
					//2B (opcode=5) | 2B (error code)
					block_len=4;
					send_packet=(uint8_t*)malloc(block_len);
					memcpy(send_packet,&err_opcode,2); //error opcode (2bytes)
					memcpy(send_packet+2,&writing_reading_error_code,2); //err code (2bytes)
					sendto(sockfd,send_packet,block_len,0,(struct sockaddr*)&client_addr, sizeof(client_addr));

					free(send_packet);
					send_packet=NULL;
					break;
				}
				//Recalc files from flash
				flash_addr=BLOCK_7_FLASH_MEM_FIRST_ADDRESS; //ATIM, Sector 7 (first address)
				file_index=0;
				if(read_data_from_flash_memory(&file_index)<0)
				{
					//fault
					printf("Error in reading from flash memory\r\n");
					free(send_packet);
					send_packet=NULL;
					break;
				}
				print_stored_files(&file_index);

				//Send ACK back to client(OPCODE = 4)
				get_blocknum++; //ACK this block
				//2B (opcode=4) | 2B (block number)
				block_len=4;
				send_packet=(uint8_t*)malloc(block_len);
				memcpy(send_packet,&ack_opcode,2); //error opcode (2bytes)
				memcpy(send_packet+2,&get_blocknum,2); //err code (2bytes)
				sendto(sockfd,send_packet,block_len,0,(struct sockaddr*)&client_addr, sizeof(client_addr));

				//Since its last block we can end this session
				free(send_packet);
				send_packet=NULL;
				break;

			}
			//Send ACK back to client(OPCODE = 4)
			get_blocknum++; //ACK this block
			//2B (opcode=4) | 2B (block number)
			block_len=4;
			send_packet=(uint8_t*)malloc(block_len);
			memcpy(send_packet,&ack_opcode,2); //error opcode (2bytes)
			memcpy(send_packet+2,&get_blocknum,2); //err code (2bytes)
			sendto(sockfd,send_packet,block_len,0,(struct sockaddr*)&client_addr, sizeof(client_addr));

			memset(buffer,0,sizeof(buffer));

			free(send_packet);
			send_packet=NULL;

		}
		else if(received_opcode==ack_opcode) //Received ACK from client (on read request)
		{
			//Data ACK (OPCODE = 4)
			memcpy(&get_blocknum, buffer+2,2); //Get blocknum

			get_blocknum++;//2

			block_len=strlen((char*)files[file_ref].data_content+((get_blocknum-1)*BLOCK_SIZE));

			if(block_len<BLOCK_SIZE)
			{
				//Send empty message indicating end of block
				send_packet=(uint8_t*)malloc(5);
				memcpy(send_packet,&data_opcode,2);
				memcpy(send_packet+2,&get_blocknum,2);
				memcpy(send_packet+4,&empty_message,1);
				//Send packet containing file's content back to client
				sendto(sockfd,send_packet,5,0,(struct sockaddr*)&client_addr, sizeof(client_addr));
				free(send_packet);
				send_packet=NULL;
				//Exit Session
				break;
			}
			else
			{
				//Send max block num - 512 Bytes
				send_packet=(uint8_t*)malloc(4+BLOCK_SIZE);
				memcpy(send_packet,&data_opcode,2);
				memcpy(send_packet+2,&get_blocknum,2);
				memcpy(send_packet+4,files[file_ref].data_content+(get_blocknum-1),BLOCK_SIZE);
				//Send packet containing file's content back to client
				sendto(sockfd,send_packet,4+BLOCK_SIZE,0,(struct sockaddr*)&client_addr, sizeof(client_addr));
				free(send_packet);
				send_packet=NULL;
			}


		}



	}//for loop

	printf("End client session, port: %d\r\n",client_port);
	//Release task's resources
	memset(buffer,0,sizeof(buffer));
	free(session);
	session=NULL;
	vTaskDelete(NULL);

}

int write_data_to_flash_memory(uint8_t* filename,uint8_t* flash_mem_buffer)
{

	uint32_t i;
	const char filename_seperator='^';
	const char data_seperator=';';

	if(HAL_FLASH_Unlock()!=HAL_OK)
	{
		printf("Error unlocking flash memory\r\n");
		return -1;
	}

	//Write filename
	for(i=0;i<strlen((char*)filename);i++)
	{
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, flash_addr, filename[i]);
		flash_addr++;
	}
	HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, flash_addr, filename_seperator);
	flash_addr++;
	//Write data
	for(i=0;i<strlen((char*)flash_mem_buffer);i++)
	{
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, flash_addr, flash_mem_buffer[i]);
		flash_addr++;
	}
	HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, flash_addr, data_seperator);
	flash_addr++;

	if(HAL_FLASH_Lock()!=HAL_OK)
	{
		printf("Error locking flash memory\r\n");
		return -1;
	}

	return 0;

}

int read_data_from_flash_memory(uint8_t *file_index)
{
	uint8_t j,k;
	uint8_t data_direction;
	const char filename_seperator='^';
	const char data_seperator=';';

	j=0;
	k=0;
	data_direction=0;

	if(HAL_FLASH_Unlock()!=HAL_OK)
	{
		printf("Error unlocking flash memory\r\n");
		return -1;
	}

	for(;;)
	{
		if((*(__IO uint8_t*)flash_addr)==255 || (*(__IO uint8_t*)flash_addr)==0xFF)
		{
			break;
		}

		if(!data_direction)//filename
		{
			if(*(__IO uint8_t*)flash_addr!=filename_seperator)
			{
				files[*file_index].filename[j++]=*(__IO uint8_t*)flash_addr;
				flash_addr++;
			}
			else
			{
				data_direction=1;
				flash_addr++;
			}
		}
		else
		{
			if(*(__IO uint8_t*)flash_addr!=data_seperator)
			{
				files[*file_index].data_content[k++]=*(__IO uint8_t*)flash_addr;
				flash_addr++;
			}
			else
			{
				j=0;
				k=0;
				data_direction=0;
				(*file_index)++;
				flash_addr++;
			}
		}

	}

	if(HAL_FLASH_Lock()!=HAL_OK)
	{
		printf("Error locking flash memory\r\n");
		return -1;
	}

	return 0;

}

void print_stored_files(uint8_t *file_index)
{
	uint8_t i;

	printf("Stored Files:\r\n");
	if(*file_index==0)
	{
		printf("There are currently no stored files\r\n");
	}
	else
	{
		for(i=0;i<*file_index;i++)
		{
		  	printf("%d) %s\r\n",(i+1),files[i].filename);
		}
	}

}

void get_all_files(char* buffer,int* buffer_size,uint8_t *file_index)
{
	uint8_t i;
	int bytes=0,total_bytes=0;

	if(*file_index==0)
	{
		//No files
		printf("no files\r\n");
		*buffer_size=0;
		return;
	}
	else
	{
		//Get files
		bytes=sprintf(buffer,"%s",files[0].filename);
		total_bytes=bytes;
		for(i=1;i<*file_index;i++)
		{
			bytes=sprintf(buffer+total_bytes,"%s",files[i].filename);
			total_bytes+=bytes;
		}
		*buffer_size=strlen(buffer);
	}


}

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* init code for LWIP */
  MX_LWIP_Init();
  /* USER CODE BEGIN 5 */
  conn=udp_new();
  udp_bind(conn, IP_ANY_TYPE, PORT_NUM);
  udp_recv(conn, udp_server_recv, NULL);


  /* Infinite loop */
  for(;;)
  {
    osDelay(100);
  }
  /* USER CODE END 5 */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

