/* GPIO Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "wm8978.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include <sys/socket.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "eth.h"
#include "event.h"
#include "wifi.h"
#include "hal_i2c.h"
#include "hal_i2s.h"
#include "wm8978.h"
#include "webserver.h"
#include "http.h"
#include "cJSON.h"
#include "mdns_task.h"
#include "audio.h"
#include <dirent.h>
#include "esp_heap_caps.h"
#include "euler.h"
#include "websocket.h"
#include "esp_heap_caps.h"
#include "aplay.h"
#include "ftpd.h"



#define TAG "main:"
// typedef int (*http_data_cb) (http_parser*, const char *at, size_t length);
// typedef int (*http_cb) (http_parser*);


//char* http_body;

#define GPIO_OUTPUT_IO_0    5
#define GPIO_OUTPUT_PIN_SEL  ((1<<GPIO_OUTPUT_IO_0))
#define SIZE_OF_20MS  320

	typedef enum _MESSAGE_TYPE{
		CONTROL,
		SPEACH	
	}MESSAGE_TYPE;

	typedef struct _MESSAGE_SPEACH{
		MESSAGE_TYPE message_type;
		char speach_data[SIZE_OF_20MS];
	}MESSAGE_SPEACH;

	typedef struct _MESSAGE_CONTROL{
		MESSAGE_TYPE message_type;
		int control_number;
	}MESSAGE_CONTROL;

	int MESSAGE_TYPE_SIZE = sizeof(MESSAGE_TYPE);
	int MESSAGE_SPEACH_SIZE = sizeof(MESSAGE_SPEACH);
 	int MESSAGE_CONTROL_SIZE = sizeof(MESSAGE_CONTROL);

	xQueueHandle record_data;
	xQueueHandle play_data;
	
int creat_server(in_port_t in_port, in_addr_t in_addr);
int connect_socket(char *addr, int port, int *sockfd);
void send_data(int sockfd, char *databuff, int data_len);
void recv_data(int sockfd, char *databuff, int data_len);
int get_socket_error_code(int socket);
int show_socket_error_reason(const char *str, int socket);

static void play_task( void *pvParameters )
{
//	MESSAGE_SPEACH message_speach;
	int sockfd;
	 vTaskDelay(50 / portTICK_PERIOD_MS);
	connect_socket("127.0.0.1", 888, &sockfd);
	portBASE_TYPE xStatus;
//	message_speach.message_type = SPEACH;
//	int *p_sockfd = (int*)pvParameters;
	char *databuff = (char *)malloc(320);
	for( ; ; )
	{
	//	xStatus = xQueueReceive(record_data, &message_speach, 0);
	//	if(message_speach.message_type == SPEACH){
			recv(sockfd, databuff, 320, 0);
	        hal_i2s_write(0,databuff,320,portMAX_DELAY);
			taskYIELD();
	//	}
	
	}
}

static void record_task( void *pvParameters )
{
   int client_fd;
	client_fd=creat_server( htons(888), htonl(INADDR_ANY));
//	xTaskCreate(play_task, "play_task", 4096, NULL, 3, NULL);
//	MESSAGE_SPEACH message_speach;
	portBASE_TYPE xStatus;
//	message_speach.message_type = SPEACH;
	char *databuff = (char *)malloc(320);
//	int *p_sockfd = (int*)pvParameters;
	for( ; ; )
	{
	    hal_i2s_read(0,databuff,320,portMAX_DELAY);
	//	xStatus = xQueueSendToBack(record_data,&message_speach, 0);
		write( client_fd, databuff,320);
		taskYIELD();
	
	}

}


void app_main()
{

    esp_err_t err;
    event_engine_init();
    nvs_flash_init();
    tcpip_adapter_init();
    wifi_init_sta("wt","123654789");
    //wifi_init_softap();
    /*init gpio*/
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    gpio_set_level(GPIO_OUTPUT_IO_0, 0);
    /*init codec */
    hal_i2c_init(0,19,18);
    hal_i2s_init(0,8000,16,1);
    WM8978_Init();
    WM8978_ADDA_Cfg(1,1); 
    WM8978_Input_Cfg(1,0,0);     
    WM8978_Output_Cfg(1,0); 
    WM8978_MIC_Gain(60);
    WM8978_AUX_Gain(0);
    WM8978_LINEIN_Gain(0);
    WM8978_SPKvol_Set(0);
    WM8978_HPvol_Set(15,15);
    WM8978_EQ_3D_Dir(0);
    WM8978_EQ1_Set(0,24);
    WM8978_EQ2_Set(0,24);
    WM8978_EQ3_Set(0,24);
    WM8978_EQ4_Set(0,24);
    WM8978_EQ5_Set(0,24);
    //creat queue
	record_data = xQueueCreate( 10, MESSAGE_SPEACH_SIZE);
	play_data = xQueueCreate( 10, MESSAGE_SPEACH_SIZE);
    /*init sd card*/
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 10
    };
    sdmmc_card_t* card;
    err = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    if (err != ESP_OK) {
        if (err == ESP_FAIL) {
            printf("Failed to mount filesystem. If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            printf("Failed to initialize the card (%d). Make sure SD card lines have pull-up resistors in place.", err);
        }
        return;
    }
    sdmmc_card_print_info(stdout, card);
    //wait got ip address
    xEventGroupWaitBits(station_event_group,STA_GOTIP_BIT,pdTRUE,pdTRUE,portMAX_DELAY);
    ESP_LOGI(TAG,"got ip address");
    //xEventGroupWaitBits(eth_event_group,ETH_GOTIP_BIT,pdTRUE,pdTRUE,portMAX_DELAY);
    //esp_err_t tcpip_adapter_get_ip_printf(tcpip_adapter_if_t tcpip_if, tcpip_adapter_ip_printf_t *ip_printf);
    gpio_set_level(GPIO_OUTPUT_IO_0, 1);
    tcpip_adapter_ip_info_t ip;
    memset(&ip, 0, sizeof(tcpip_adapter_ip_info_t));
    if (tcpip_adapter_get_ip_info(ESP_IF_WIFI_STA, &ip) == 0) {
        ESP_LOGI(TAG, "~~~~~~~~~~~");
        ESP_LOGI(TAG, "ETHIP:"IPSTR, IP2STR(&ip.ip));
        ESP_LOGI(TAG, "ETHPMASK:"IPSTR, IP2STR(&ip.netmask));
        ESP_LOGI(TAG, "ETHPGW:"IPSTR, IP2STR(&ip.gw));
        ESP_LOGI(TAG, "~~~~~~~~~~~");
    }
    /* task creat*/
    ftpd_start();
    // xTaskCreate(&ftpd_task, "ftpd_task",4096, NULL, 5, NULL);
    //xTaskCreate(&euler_task, "euler_task", 8196, NULL, 5, NULL);
    // xTaskCreate(webserver_task, "web_server_task", 4096, NULL, +6, NULL);

	/*print the last ram*/
    size_t free8start=heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t free32start=heap_caps_get_free_size(MALLOC_CAP_32BIT);
    ESP_LOGI(TAG,"free mem8bit: %d mem32bit: %d\n",free8start,free32start);

    gpio_set_level(GPIO_OUTPUT_IO_0, 1);

    uint8_t cnt=0;
	
//	int sockfd;
//	connect_socket("192.168.1.119", 8887, &sockfd);
//	int sockfd_server_test;
//	sockfd_server_test = creat_server(htons(888), htonl(INADDR_ANY));

//creat record xTask and play xTask
	xTaskCreate(record_task, "record_task", 9000, NULL, 4, NULL);
	xTaskCreate(play_task, "play_task", 4096, NULL, 3, NULL);

    while(1){
        gpio_set_level(GPIO_OUTPUT_IO_0, cnt%2);
        //memset(samples_data,0,1024);
        //vTaskDelay(1000 / portTICK_PERIOD_MS);
        //vTaskSuspend(NULL);
        //ESP_LOGI(TAG, "cnt:%d",cnt);
       // aplay_mp3("/sdcard/music.mp3");
       // aplay_wav("/sdcard/music.wav");
//        hal_i2s_read(0,samples_data,320,portMAX_DELAY);
//	send(test_client_sockfd, samples_data, 320*50, 0);
//        hal_i2s_write(0,samples_data,320,portMAX_DELAY);
      //  vTaskDelay(5000 / portTICK_PERIOD_MS);
        cnt++;
    }
}

int creat_server(in_port_t in_port, in_addr_t in_addr)
 {
   int server_fd, client_fd;
   struct sockaddr_in server, client;
   int socket_fd, on;
   socklen_t client_size=sizeof(client);
   //struct timeval timeout = {10,0};
 
   server.sin_family = AF_INET;
   server.sin_port = in_port;
   server.sin_addr.s_addr = in_addr;
 
  if((server_fd = socket(AF_INET, SOCK_STREAM, 0))<0) {
     perror("listen socket uninit\n");
      return -1;
    }
    on=1;
    //setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(t    imeout));
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int) );
    //CALIB_DEBUG("on %x\n", on);
    if((bind(server_fd, (struct sockaddr *)&server, sizeof(server)))<0) {
      perror("cannot bind srv socket\n");
      return -1;
    }
  
    if(listen(server_fd, 1)<0) {
      perror("cannot listen");
      close(server_fd);
      return -1;
    }
   client_fd = accept(server_fd, (struct sockaddr *)&client, &client_size);
    if (connect_socket < 0) {
        show_socket_error_reason("accept_server", client_fd);
        close(server_fd);
        return ESP_FAIL;
    }
    /*connection established，now can send/recv*/
    ESP_LOGI(TAG, "tcp connection established!");
   	return client_fd;
 }

int connect_socket(char *addr, int port, int *sockfd)
{
//creat socket  and conncet to server
	int test_client_sockfd;
	struct sockaddr_in test_client;
	memset(&test_client, 0, sizeof(test_client));
	test_client.sin_family = AF_INET;
	test_client.sin_addr.s_addr = inet_addr(addr);
	test_client.sin_port = htons(port);
	test_client_sockfd = socket(PF_INET, SOCK_STREAM, 0);
	connect(test_client_sockfd, (struct sockaddr *)&test_client, sizeof(struct sockaddr));
	*sockfd = test_client_sockfd;
	return 0;
}

void send_data( int sockfd, char *databuff, int data_len)
{
    int len = 0;
    memset(databuff, 0, data_len);
    ESP_LOGI(TAG, "start sending...");
    int to_write = data_len;

        //send function
        while (to_write > 0) {
            len = send(sockfd, databuff + (data_len - to_write), to_write, 0);
            if (len > 0) {
                to_write -= len;
            } else {
                int err = get_socket_error_code(sockfd);

                if (err != ENOMEM) {
                    show_socket_error_reason("send_data", sockfd);
                    break;
                }
            }
        }
//    free(databuff);
//    vTaskDelete(NULL);
}

//receive data
void recv_data( int sockfd, char *databuff, int data_len)
{
    int len = 0;  
    int to_recv = data_len;
        while (to_recv > 0) {
            len = recv(sockfd, databuff + (data_len - to_recv), to_recv, 0);
            if (len > 0) {
                to_recv -= len;
            } else {
                show_socket_error_reason("recv_data", sockfd);
                break;
            }
        }
//    free(databuff);
//    vTaskDelete(NULL);
}
int get_socket_error_code(int socket)
{
    int result;
    u32_t optlen = sizeof(int);
    int err = getsockopt(socket, SOL_SOCKET, SO_ERROR, &result, &optlen);
    if (err == -1) {
        ESP_LOGE(TAG, "getsockopt failed:%s", strerror(err));
        return -1;
    }
    return result;
}

int show_socket_error_reason(const char *str, int socket)
{
    int err = get_socket_error_code(socket);

    if (err != 0) {
        ESP_LOGW(TAG, "%s socket error %d %s", str, err, strerror(err));
    }

    return err;
}


