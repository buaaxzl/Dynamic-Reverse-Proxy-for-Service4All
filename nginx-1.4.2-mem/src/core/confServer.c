#include <signal.h> 
#include <sys/file.h>
#include <unistd.h>
#include <stdio.h>	
#include <string.h>
#include <stdlib.h> 	 
#include <sys/socket.h>  
#include <sys/types.h>		// for socket 
#include <netinet/in.h>		// for sockaddr_in
#include <string.h> 	
#include <pthread.h>
#include <sys/errno.h>		

#include <libxml/parser.h>	//for xml parsing
#include <libxml/tree.h>
#include <iconv.h>

#include <sys/sem.h>

#include <sys/ipc.h>		//for shared memory
#include <sys/shm.h>
#include "confServer.h"
#include <sys/msg.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>  

#define SERVER_PORT 4353
#define BACKLOG 20		// max length of listen queue
#define BUFFER_SIZE 2048

#define UPSTREAM 512
#define LOCATION 512

#define SHAREDMEM_SIZE 4096*90
#define SHAREDMEM_NAME 1236
#define BUF (1024*8-10)

FILE *fp = NULL;		// for nginx.conf
FILE *fpRead = NULL;		// for nginx.conf
FILE *fpsock0 = NULL;		// for closing serverPort(sockfd.ini), created in nginx.c
pthread_mutex_t mutex;		// for modifying nginx.conf
pthread_mutex_t mutex_1;

xmlDocPtr infoListDoc = NULL;
xmlNodePtr root = NULL; 
char* nginxPort = "5800";
char* nginxIp = "127.0.0.1";
static int recvCount = 0;
static int textCount = 0;

static char* upstream = NULL;
static char* location = NULL;
static char user_name[20] ={"nobody\0"};
static char user_group[20]={"\0"};
static char worker_processes[10]={"2\0"};
static char error_log[40]={"logs/error.log\0"};
static char pid[40]="logs/nginx.pid\0";
static char worker_rlimit_nofile[10]="51200\0";
static char use_what[10]="epoll\0";
static char worker_connections[10]="1024\0";
static char before_upstream[500]="include mime.types;\n default_type  application/octet-stream; \n sendfile on; \n keepalive_timeout  65;\n log_format my_log '$upstream_addr|$request_uri';\n  \0";
static char location_added[1000]="\0";//"location ~ .*\\.(gif|jpg|jpeg|png|bmp|swf)${ expires 7d; access_log off;} \n  location ~ .*\\.(js|css)?$ { expires  1d; access_log off; }\0";
static char head[3000];

static int beta = 1;
static int beta_count = 0;

struct message{          //消息结构体   
    long msg_type;  
    char msg_text[BUF];  
};  

union semun{
	int val;
	struct semid_ds* buf; unsigned short* array;
};

static char php_location[2000] ="location ~ \\.php$ { \nroot  html;\n fastcgi_pass  127.0.0.1:9800; \n fastcgi_index  index.php;\n fastcgi_param  SCRIPT_FILENAME /usr/local/nginx-1.4.2-mem/html$fastcgi_script_name;\n fastcgi_param  QUERY_STRING  $query_string;\n fastcgi_param  REQUEST_METHOD $request_method;\n fastcgi_param  CONTENT_TYPE  $content_type;\n fastcgi_param  CONTENT_LENGTH  $content_length;\n fastcgi_param  SCRIPT_NAME  $fastcgi_script_name;\n fastcgi_param  REQUEST_URI  $request_uri;\n fastcgi_param  DOCUMENT_URI  $document_uri;\n fastcgi_param  DOCUMENT_ROOT  $document_root;\n fastcgi_param  SERVER_PROTOCOL $server_protocol;\n fastcgi_param  HTTPS  $https if_not_empty;\n fastcgi_param  GATEWAY_INTERFACE  CGI/1.1;\n fastcgi_param  SERVER_SOFTWARE  nginx/$nginx_version;\n fastcgi_param  REMOTE_ADDR  $remote_addr;\n fastcgi_param  REMOTE_PORT  $remote_port;\n fastcgi_param  SERVER_ADDR  $server_addr;\n fastcgi_param  SERVER_PORT  $server_port;\n fastcgi_param  SERVER_NAME  $server_name;\n # PHP only, required if PHP was built with --enable-force-cgi-redirect\n fastcgi_param  REDIRECT_STATUS    200;}\0";

//generate the head of conf
void formHead()
{
	memset(head,0,sizeof(head));
	strcat(head,"user "); strcat(head,user_name); strcat(head," "); strcat(head,user_group); strcat(head,";\n");
	strcat(head,"worker_processes "); strcat(head,worker_processes); strcat(head,";\n");
	strcat(head,"error_log "); strcat(head,error_log); strcat(head,";\n");
	strcat(head,"pid "); strcat(head,pid); strcat(head,";\n");
	strcat(head,"worker_rlimit_nofile "); strcat(head,worker_rlimit_nofile); strcat(head,";\n");
	strcat(head,"events { use "); strcat(head,use_what); strcat(head,";\n");
	strcat(head,"worker_connections ");	strcat(head,worker_connections); strcat(head,";\n}\n");
	strcat(head,"http { "); strcat(head,before_upstream); 
}

//function to handle signals
void my_sig(int signo)
{

	/* this signal's operation is to write infoListDoc to applicationList.xml */
	if( signo == 10 )
	{
		int fd = open("/usr/local/nginx-1.4.2-mem/applicationList.xml",O_WRONLY);
		flock(fd,LOCK_EX);

		pthread_mutex_lock(&mutex);
		xmlSaveFile("/usr/local/nginx-1.4.2-mem/applicationList.xml",infoListDoc);
		pthread_mutex_unlock(&mutex);	

		flock(fd,LOCK_UN);		
		close(fd);
	}
	else
		/*  recieve new conf and make it take effect */
		if(signo == 12 )
		{
			pthread_mutex_lock(&mutex); 

			struct message msg;
			memset(msg.msg_text,0,sizeof(msg.msg_text));
			int msgid = msgget(456,IPC_CREAT|0666);			

			if( msgrcv(msgid,&msg,BUF,0,IPC_NOWAIT) == -1 )
				printf("rcv nodeNum failded\n");
			int nodeNum = atoi(msg.msg_text);

			memset(msg.msg_text,0,sizeof(msg.msg_text));
			if( msgrcv(msgid,&msg,BUF,0,IPC_NOWAIT) == -1 )
				printf("rcv user_name failded\n");
			strcpy(user_name,msg.msg_text);

			memset(msg.msg_text,0,sizeof(msg.msg_text));
			if( msgrcv(msgid,&msg,BUF,0,IPC_NOWAIT) == -1 )
				printf("rcv user_group failded\n");
			strcpy(user_group,msg.msg_text);

			memset(msg.msg_text,0,sizeof(msg.msg_text));
			if( msgrcv(msgid,&msg,BUF,0,IPC_NOWAIT) == -1 )
				printf("rcv worker_processes failded\n");
			strcpy(worker_processes,msg.msg_text);

			memset(msg.msg_text,0,sizeof(msg.msg_text));
			if( msgrcv(msgid,&msg,BUF,0,IPC_NOWAIT) == -1 )
				printf("rcv error_log failded\n");
			strcpy(error_log,msg.msg_text);			

			memset(msg.msg_text,0,sizeof(msg.msg_text));
			if( msgrcv(msgid,&msg,BUF,0,IPC_NOWAIT) == -1 )
				printf("rcv pid failded\n");
			strcpy(pid,msg.msg_text);

			memset(msg.msg_text,0,sizeof(msg.msg_text));
			if( msgrcv(msgid,&msg,BUF,0,IPC_NOWAIT) == -1 )
				printf("rcv worker_rlimit_nofile failded\n");
			strcpy(worker_rlimit_nofile,msg.msg_text);

			memset(msg.msg_text,0,sizeof(msg.msg_text));
			if( msgrcv(msgid,&msg,BUF,0,IPC_NOWAIT) == -1 )
				printf("rcv use_what failded\n");
			strcpy(use_what,msg.msg_text);

			memset(msg.msg_text,0,sizeof(msg.msg_text));
			if( msgrcv(msgid,&msg,BUF,0,IPC_NOWAIT) == -1 )
				printf("rcv worker_connections failded\n");
			strcpy(worker_connections,msg.msg_text);

			memset(msg.msg_text,0,sizeof(msg.msg_text));
			if( msgrcv(msgid,&msg,BUF,0,IPC_NOWAIT) == -1 )
				printf("rcv before_upstream failded\n");
			strcpy(before_upstream,msg.msg_text);

			xmlNodePtr temp_Node = NULL;
			temp_Node = root->xmlChildrenNode;

			while(temp_Node != NULL)
			{
				memset(msg.msg_text,0,sizeof(msg.msg_text));
				msgrcv(msgid,&msg,BUF,0,IPC_NOWAIT);

				xmlNodePtr temp_Node_Children = NULL;
				temp_Node_Children = temp_Node->xmlChildrenNode;
				while(temp_Node_Children!=NULL)
				{
					if(xmlStrcmp(temp_Node_Children->name,BAD_CAST"upstream")==0)
					{
						xmlNodeSetContent(temp_Node_Children,(xmlChar*)msg.msg_text);
					}
					temp_Node_Children = temp_Node_Children->next;			
				}
				temp_Node = temp_Node->next;
			}

			temp_Node = root->xmlChildrenNode;
			while(temp_Node != NULL)
			{
				memset(msg.msg_text,0,sizeof(msg.msg_text));
				msgrcv(msgid,&msg,BUF,0,IPC_NOWAIT);

				xmlNodePtr temp_Node_Children = NULL;
				temp_Node_Children = temp_Node->xmlChildrenNode;
				while(temp_Node_Children!=NULL)
				{
					if(xmlStrcmp(temp_Node_Children->name,BAD_CAST"location")==0)
					{
						xmlNodeSetContent(temp_Node_Children,(xmlChar*)msg.msg_text);
					}
					temp_Node_Children = temp_Node_Children->next;			
				}
				temp_Node = temp_Node->next;
			}

			memset(msg.msg_text,0,sizeof(msg.msg_text));
			if( msgrcv(msgid,&msg,BUF,0,IPC_NOWAIT) == -1 )
				printf("rcv php_location failded\n");
			strcpy(php_location,msg.msg_text);

			memset(msg.msg_text,0,sizeof(msg.msg_text));
			if( msgrcv(msgid,&msg,BUF,0,IPC_NOWAIT) == -1 )
				printf("rcv location_added failded\n");
			strcpy(location_added,msg.msg_text);

			printf("New configuration file has recieved.\n");

			
			bzero(upstream, (1024*UPSTREAM)*sizeof(char));
			bzero(location, (1024*LOCATION)*sizeof(char));
			formHead();
			strcpy(upstream, head);

			temp_Node = root->xmlChildrenNode;

			while(temp_Node != NULL) 
			{	
				if(!xmlStrcmp(temp_Node->name, BAD_CAST"application"))
				{	
					xmlNodePtr subNode = temp_Node->xmlChildrenNode;
					if(!xmlStrcmp(subNode->name, BAD_CAST"upstream"))
					{
						strcat(upstream, xmlNodeGetContent(subNode));
						strcat(upstream, "\n");
					}
					subNode = subNode->next;
					if(!xmlStrcmp(subNode->name, BAD_CAST"location"))
					{	
						strcat(location, xmlNodeGetContent(subNode));
						strcat(location, "\n");
					}
				}
				temp_Node = temp_Node->next;
			}
		
			strcat(location,php_location);
			strcat(location,location_added);			
			
			strcat(upstream, "\n server {\n");
			strcat(upstream, "listen  ");
			strcat(upstream, nginxPort);
			strcat(upstream, ";\n");

			strcat(upstream, "server_name  ");
			strcat(upstream, nginxIp);
			strcat(upstream, ";\n");
			strcat(upstream, "access_log logs/my_log.log my_log;\n");
			strcat(upstream, "access_log logs/access.log; \n");

			strcat(upstream, location);
			strcat(upstream, "  }\n }\n");

			printf("\n----New conf-------\n%s\n-----------\n",upstream);

			int shmId = shmget((key_t)SHAREDMEM_NAME,SHAREDMEM_SIZE,IPC_CREAT|0666);
			if(shmId == -1)
			{	
				printf("Shmget failed ! \n");
				exit(1);
			}
			char* shared_memory_1 = shmat(shmId,(void *)0, 0);
			if(shared_memory_1 == (void *)-1)
			{	
				printf("Shmat failed ! \n");
				exit(1);
			}
			strcpy(shared_memory_1, upstream);
			if(shmdt(shared_memory_1) == -1)
			{	
				printf("shmdt failed !\n");
				exit(1);
			}
			system("/usr/local/nginx-1.4.2-mem/sbin/nginx -t -c /usr/local/nginx-1.4.2-mem/conf/nginx.conf");
			system("/usr/local/nginx-1.4.2-mem/sbin/nginx -s reload");	
			pthread_mutex_unlock(&mutex);
			//msgctl(msgid,IPC_RMID,NULL);
		}
		else
			/* send conf to php */
			if(signo == 34 )
			{
				pthread_mutex_lock(&mutex_1);

				//semaphore

				int semID = semget(ftok("/usr/local/nginx-1.4.2-mem/applicationList.xml",'a'),1,IPC_CREAT|0666);
				if( semID == -1 ) printf("created semaphore unsuccessfully!\n");
				union semun arg;
				arg.val = 1;
				semctl(semID,0,SETVAL,arg);
				struct sembuf buf_1 ={ 0,-1,SEM_UNDO};
				semop(semID,&buf_1,1);

				int msgid = msgget(123,IPC_CREAT|0666);
				
				//send nodeNum
				int nodeNum = 0;
				xmlNodePtr temp = NULL;
				if(root != NULL)
				{					
					temp = root->xmlChildrenNode;
					while(temp!= NULL)
					{
						temp = temp->next;
						nodeNum++;
					}
				}
				struct message msg; memset(msg.msg_text,0,sizeof(msg.msg_text));
				msg.msg_type = 1;  sprintf(msg.msg_text,"%d",nodeNum); 
				msgsnd(msgid,(void*)&msg,strlen(msg.msg_text),IPC_NOWAIT);

				//send user_name
				memset(msg.msg_text,0,sizeof(msg.msg_text));
				msg.msg_type=1;  strcpy(msg.msg_text,user_name);
				msgsnd(msgid,(void*)&msg,strlen(msg.msg_text),IPC_NOWAIT);

				//send user_group
				memset(msg.msg_text,0,sizeof(msg.msg_text));
				msg.msg_type=1;  strcpy(msg.msg_text,user_group);
				msgsnd(msgid,(void*)&msg,strlen(msg.msg_text),IPC_NOWAIT);

				//send worker_processes
				memset(msg.msg_text,0,sizeof(msg.msg_text));
				msg.msg_type=1;  strcpy(msg.msg_text,worker_processes);
				msgsnd(msgid,(void*)&msg,strlen(msg.msg_text),IPC_NOWAIT);

				//send error_log
				memset(msg.msg_text,0,sizeof(msg.msg_text));
				msg.msg_type=1;  strcpy(msg.msg_text,error_log);
				msgsnd(msgid,(void*)&msg,strlen(msg.msg_text),IPC_NOWAIT);

				//send pid
				memset(msg.msg_text,0,sizeof(msg.msg_text));
				msg.msg_type=1;  strcpy(msg.msg_text,pid);
				msgsnd(msgid,(void*)&msg,strlen(msg.msg_text),IPC_NOWAIT);

				//send worker_rlimit_nofile
				memset(msg.msg_text,0,sizeof(msg.msg_text));
				msg.msg_type=1;  strcpy(msg.msg_text,worker_rlimit_nofile);
				msgsnd(msgid,(void*)&msg,strlen(msg.msg_text),IPC_NOWAIT);

				//send use_what
				memset(msg.msg_text,0,sizeof(msg.msg_text));
				msg.msg_type=1;  strcpy(msg.msg_text,use_what);
				msgsnd(msgid,(void*)&msg,strlen(msg.msg_text),IPC_NOWAIT);

				//send worker_connections
				memset(msg.msg_text,0,sizeof(msg.msg_text));
				msg.msg_type=1;  strcpy(msg.msg_text,worker_connections);
				msgsnd(msgid,(void*)&msg,strlen(msg.msg_text),IPC_NOWAIT);

				//send before_upstream
				memset(msg.msg_text,0,sizeof(msg.msg_text));
				msg.msg_type=1;  strcpy(msg.msg_text,before_upstream);
				msgsnd(msgid,(void*)&msg,strlen(msg.msg_text),IPC_NOWAIT);
				//printf("have sent before_upstream %s\n",msg_8.msg_text);

				//send upstreams
				if(root!=NULL)
				{
					temp = root->xmlChildrenNode;
					while( temp!=NULL )
					{
						xmlNodePtr temp_upstream = temp->xmlChildrenNode;
						struct message msg;
						msg.msg_type=1;

						while( temp_upstream != NULL )
						{
							if( !xmlStrcmp(temp_upstream->name,BAD_CAST"upstream") )
							{
								strcpy( msg.msg_text, (char*)xmlNodeGetContent(temp_upstream));
							}
							temp_upstream = temp_upstream->next;
						}

						msgsnd(msgid,(void*)&msg,strlen(msg.msg_text),IPC_NOWAIT);
						temp = temp->next;
					}
				}

				//send locations
				if(root!=NULL)
				{
					temp = root->xmlChildrenNode;
					while( temp!=NULL )
					{
						xmlNodePtr temp_location= temp->xmlChildrenNode;
						struct message msg;
						msg.msg_type=1;

						while( temp_location != NULL )
						{
							if( !xmlStrcmp(temp_location->name,BAD_CAST"location") )
							{
								strcpy( msg.msg_text, (char*)xmlNodeGetContent(temp_location));
							}
							temp_location = temp_location->next;
						}

						msgsnd(msgid,(void*)&msg,strlen(msg.msg_text),IPC_NOWAIT);
						temp = temp->next;
					}
				}

				//send php_location
				memset(msg.msg_text,0,sizeof(msg.msg_text));
				msg.msg_type=1;  
				strcpy(msg.msg_text,php_location);
				msgsnd(msgid,(void*)&msg,strlen(msg.msg_text),IPC_NOWAIT);				


				//send location_added
				memset(msg.msg_text,0,sizeof(msg.msg_text));
				msg.msg_type=1;  
				strcpy(msg.msg_text,location_added);
				msgsnd(msgid,(void*)&msg,strlen(msg.msg_text),IPC_NOWAIT);

				struct sembuf buf_2 ={ 0,1,SEM_UNDO};
				semop(semID,&buf_2,1);

				pthread_mutex_unlock(&mutex_1);
			}
}


int myServer()
{

	signal(SIGUSR1,my_sig);
	signal(SIGUSR2,my_sig);
	signal(SIGRTMIN,my_sig);

	xmlInitParser();
	xmlKeepBlanksDefault(0); 

	if(infoListDoc == NULL)
	{	
		infoListDoc = xmlParseFile("/usr/local/nginx-1.4.2-mem/applicationList.xml");
		root = xmlDocGetRootElement(infoListDoc);
	}

	int sockfd;
	struct sockaddr_in serverAddr; 	
	bzero(&serverAddr, sizeof (serverAddr));
	serverAddr.sin_family      = AF_INET;		// (TCP/IP)addr family
	serverAddr.sin_port        = htons(SERVER_PORT);
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);	// fill in with local IP addr

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{ 
		perror("Create socket failed !\n"); 
		exit(1); 
	}
		
	if (bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) 
	{ 
		printf("Bind port %d failed !\n", SERVER_PORT);
		exit(1); 
	}   
	if(listen(sockfd, BACKLOG) == -1)// server socket begin to listen
	{
		printf("Server listen failed !\n");
	  	exit(1);	
	}
	printf("readMem   listening...\n");
	
	pthread_mutex_init(&mutex_1,NULL);
	int lock_mutex = pthread_mutex_init(&mutex, NULL);
	if(lock_mutex != 0)
	{
		printf("Mutex init failed!\n");
	}
//------------------------Shared Memory    Create 
	int shmid_0 = shmget((key_t)SHAREDMEM_NAME,SHAREDMEM_SIZE,IPC_CREAT|0666);
	if(shmid_0 == -1)
	{	
		printf("Shmget failed ! \n");
		exit(1);
	}
//	printf("Create shared memory successfully ! shmid_0 = %d\n", shmid_0);	
//-----------------------
	upstream = (char*)malloc((1024*UPSTREAM)*sizeof(char));
	if (upstream == NULL) {
     		printf("**********  upstream malloc failed");
  	   	exit(1);
  	}
	char* upstream2 = upstream; 
	bzero(upstream, (1024*UPSTREAM)*sizeof(char));

	location = (char*)malloc((1024*LOCATION)*sizeof(char)); 
	if (location == NULL) {
     		printf("**********  location malloc failed");
  	   	exit(1);
  	}
	char* location2 = location; 
	bzero(location, (1024*LOCATION)*sizeof(char));
//---------------------
	pthread_t pidShut;	// for shutting down SERVERPORT
	int pthread_shut = pthread_create(&pidShut, NULL, (void*)closeServerPort, (void *)sockfd);
	if (pthread_shut != 0)
	{
		printf("Create closeServerPort thread failed !\n");
		exit(0);
	}
	
	while(1)
	{       
		struct sockaddr_in clientAddr; 
		socklen_t length = sizeof(clientAddr);
		int new_sockfd = accept(sockfd, (struct sockaddr*)&clientAddr, &length);
		if (new_sockfd == -1)
		{	//------------------------Shared Memory    Destroy 
			int shmid_3 = shmget((key_t)SHAREDMEM_NAME,SHAREDMEM_SIZE,IPC_CREAT|0666);
			if(shmid_3 == -1)
			{
				printf("Shmget failed ! \n");
				exit(1);
			}
		//	printf("Get shared memory successfully ! shmid_3 = %d\n", shmid_3);
			//------------ clear the content 
			char* shared_memory_3 = shmat(shmid_3,(void *)0, 0);
			if(shared_memory_3 == (void *)-1)
			{
				printf("Shmat failed ! \n");
				exit(1);
			}
			strcpy(shared_memory_3, "");
			if(shmdt(shared_memory_3) == -1)
			{	
				printf("shmdt failed !\n");
				exit(1);
			}
			//-------------
			if(shmctl(shmid_3, IPC_RMID, 0) == -1)
			{	
				printf("shmctl(IPC_RMID) failed !\n");
				exit(1);
			}
			printf("\n====== Shared Memory has been deleted !======\n");		
			//---------------------			
			printf("Server accept failed !\n");
			break;
		}
//		printf("Accept successfully !\n");

		// create new thread and communicate
		pthread_t pid;
		int pthread_err = pthread_create(&pid, NULL, (void*)recv_send_data, (void *)new_sockfd);
		
		if (pthread_err != 0)
		{
			printf("Create recv_send_data thread failed !\n");
			exit(0);
		}
		pthread_join(pid, NULL);
	}

//---------------------
	if(upstream2 != NULL)
	{
		free(upstream2);
		upstream2 = NULL;
	}
	if(location2 != NULL)
	{
		free(location2);
		location2 = NULL;
	}
	shutdown(sockfd,2);
	xmlCleanupParser();
	pthread_mutex_destroy(&mutex);
	pthread_mutex_destroy(&mutex_1);
	printf("myServer over\n");
	return 0;	
	
}

void closeServerPort(int sockfd)
{	
	int closeFlag = 0;	// 0:donnot close; 1:to close; 2:already closed
inquery:
	if((fpsock0 = fopen("/usr/local/nginx-1.4.2-mem/sockfd.ini","r")) == NULL)
	{
		printf("sockfd.ini not exists !\n");
		exit(0);
	}
	fscanf(fpsock0, "%d", &closeFlag);
//	printf("before shutdown ,closeFlag = %d\n", closeFlag);
        fclose(fpsock0);
	if(closeFlag == 1)
	{
		shutdown(sockfd,2);
		closeFlag = 2;
		exit(0);
	}
	else
	{
		usleep(100000);// 0.1s
		//closeServerPort(sockfd);
		goto inquery;
	}	
}

void recv_send_data(int new_sockfd)
{	
	char buffer[BUFFER_SIZE];
	bzero(buffer, BUFFER_SIZE);
	int fileLength = 0;
	if(recv(new_sockfd, buffer, BUFFER_SIZE, 0) > 0)
	{
		fileLength = atoi(buffer);
		bzero(buffer, BUFFER_SIZE);
	}
	else 
	{	
		printf("Error in Receiving!\n");
	}

	int sendNo = send(new_sockfd,"@@@@\n",5,0);
	if(sendNo == -1)
	{		
		printf("Error in sending @@@@ !\n");
	}

	char* text = (char*)malloc((fileLength+1)*sizeof(char)); 
	if (text == NULL) {
     	printf("**********  text malloc failed");
  	   	exit(1);
  	}
	char* text2 = text;
	bzero(text, (fileLength+1)*sizeof(char));

	int recvNo = 0;
	while ((recvNo = recv(new_sockfd, buffer, BUFFER_SIZE, 0)))
	{
		if (recvNo < 0)
		{
			printf("Server Recieve Data Failed! code %d\n", recvNo);
		}
		strcat(text, buffer);
		bzero(buffer, BUFFER_SIZE);	
	}
	text[fileLength]='\0';	
	pthread_mutex_lock(&mutex);

	printf("------1  text = %s\n", text);

//	由于当前配置内容保存在共享内存中，所以读取共享内存
//------------------------Shared Memory    Read old--------------------------------
	int shmid = shmget((key_t)SHAREDMEM_NAME,SHAREDMEM_SIZE,IPC_CREAT|0666);
	if(shmid == -1)
	{	
		printf("Shmget failed ! \n");
		exit(1);
	}

	char* shared_memory = shmat(shmid,(void *)0, 0);
	if(shared_memory == (void *)-1)
	{	
		printf("Shmat failed ! \n");
		exit(1);
	}
	char text_old[SHAREDMEM_SIZE];
	strcpy(text_old, shared_memory);

	if(shmdt(shared_memory) == -1)
	{	
		printf("shmdt failed !\n");
		exit(1);
	}
//-------------------------------------------------------------------------------	
	int parseNo = 0;
	parseNo = parseAndOperate(text, fileLength);// parse
	pthread_mutex_unlock(&mutex);

	if(parseNo == 2)
	{
		recvCount++;
		printf("recvCount = %d\n", recvCount);
	}  
	if(text2 != NULL)
	{
		free(text2);	
		text2 = NULL;
	}	
	shutdown(new_sockfd,2);
	pthread_exit(NULL);
		  	
}


int parseAndOperate(char* text, int infoLength)
{	
	xmlDocPtr doc = NULL;           // define parsing file pointer
	xmlNodePtr curNode = NULL;      // define Node pointer
	xmlNodePtr subroot = NULL; 
	xmlNodePtr subCurNode = NULL;
	xmlKeepBlanksDefault(0); 

	if(infoListDoc == NULL)
	{	
		infoListDoc = xmlParseFile("/usr/local/nginx-1.4.2-mem/applicationList.xml");
		root = xmlDocGetRootElement(infoListDoc);
	}

printf("-------------------------2\n");
	doc = xmlParseMemory(text,(infoLength+10)*sizeof(char));
	if (doc == NULL ) 
	{		
		printf("Document is null. \n");
		printf("Memory not parsed successfully. \n");
		return -1;
	}	
	subroot = xmlDocGetRootElement(doc); // get root "confInfoSegment"
	if (subroot == NULL)	
	{ 
		printf("Empty information!\n"); 
		if(doc != NULL)
		{
			xmlFreeDoc(doc);
			doc = NULL;
		} 
		return -1; 
	}	

	if (xmlStrcmp(subroot->name, (const xmlChar *) "confInfoSegment")) 
	{	
		printf("Text of the wrong type, subroot node != confInfoSegment");
		if(doc != NULL)
		{
			xmlFreeDoc(doc);
			doc = NULL;
		}
		return -1;
	}
	xmlChar * operationProp;
	operationProp = xmlGetProp(subroot, (xmlChar *)"operation");

printf("----------------------operationProp---%s\n", operationProp);

	if(strcmp((char *)operationProp, "de") == 0)
	{
		subCurNode = subroot->xmlChildrenNode;
		while(subCurNode != NULL)
		{
			if(!xmlStrcmp(subCurNode->name, BAD_CAST"application"))
			{
				xmlAddChild(root, subCurNode);
			}
			subCurNode = subCurNode->next;
		}
		curNode = root->xmlChildrenNode;
printf("-------------------------2_if\n");
	}
	else if(strcmp((char *)operationProp, "un") == 0)
	{
		xmlChar * idProp;
		xmlChar * appIdProp;
		xmlNodePtr listChildNode = NULL;

		subCurNode = subroot->xmlChildrenNode;
		while(subCurNode != NULL)
		{
			if(!xmlStrcmp(subCurNode->name, BAD_CAST"remove"))
			{				
				idProp =  xmlGetProp(subCurNode, (xmlChar *)"id");
				listChildNode = root->xmlChildrenNode;	
				while(listChildNode != NULL)
				{
					if(!xmlStrcmp(listChildNode->name, BAD_CAST"application"))
					{
						appIdProp = xmlGetProp(listChildNode, (xmlChar *)"id");
						if(strcmp((char *)idProp, (char *)appIdProp) == 0)
						{
							xmlNodePtr tempNode = NULL;
							tempNode = listChildNode->next;
							xmlUnlinkNode(listChildNode);
							xmlFreeNode(listChildNode);
							listChildNode = tempNode;
							break;
						}
					}
					listChildNode = listChildNode->next;
				}
			}
			subCurNode = subCurNode->next;
		}
		curNode = root->xmlChildrenNode;
		xmlFree(idProp);
		xmlFree(appIdProp);	
		printf("-------------------------2_if\n");
	}
	xmlFree(operationProp);	 
	
	
	beta_count++;	

	if((beta == 1)||(beta_count % beta == 1))	//create new conf
	{				
		bzero(upstream, (1024*UPSTREAM)*sizeof(char));
		bzero(location, (1024*LOCATION)*sizeof(char));
		formHead();
		strcpy(upstream, head);	
	}

	if(beta_count % beta == 0)//commit new conf
	{	
		while(curNode != NULL) 
		{	
			if(!xmlStrcmp(curNode->name, BAD_CAST"application"))
			{	
				xmlNodePtr subNode = curNode->xmlChildrenNode;
				if(!xmlStrcmp(subNode->name, BAD_CAST"upstream"))
				{
					strcat(upstream, xmlNodeGetContent(subNode));
					strcat(upstream, "\n");
				}
				printf("**-----0 upstream =  %s\n", upstream);
				subNode = subNode->next;
				if(!xmlStrcmp(subNode->name, BAD_CAST"location"))
				{	
					strcat(location, xmlNodeGetContent(subNode));
					strcat(location, "\n");
				}
				printf("**-----1 location =  %s\n", location);
			}
			curNode = curNode->next;
		}

		strcat(location,php_location);
		strcat(location,location_added);
		
		
		strcat(upstream, "\n server {\n");
		strcat(upstream, "listen  ");
		strcat(upstream, nginxPort);
		strcat(upstream, ";\n");
		strcat(upstream, "server_name  ");
		strcat(upstream, nginxIp);
		strcat(upstream, ";\n");
		strcat(upstream, "access_log logs/my_log.log my_log;\n");
		strcat(upstream, "access_log logs/access.log; \n");
	
		strcat(upstream, location);
		strcat(upstream, "  }\n }\n");

		printf("**-----2 upstream =  %s\n", upstream);

//------------------------Shared Memory    Modify ----------
		int shmid_1 = shmget((key_t)SHAREDMEM_NAME,SHAREDMEM_SIZE,IPC_CREAT|0666);
		if(shmid_1 == -1)
		{	
			printf("Shmget failed ! \n");
			exit(1);
		}

		char* shared_memory_1 = shmat(shmid_1,(void *)0, 0);
		if(shared_memory_1 == (void *)-1)
		{	
			printf("Shmat failed ! \n");
			exit(1);
		}
		strcpy(shared_memory_1, upstream);

		if(shmdt(shared_memory_1) == -1)
		{	
			printf("shmdt failed !\n");
			exit(1);
		}
//---------------------------------------------------------------

		system("/usr/local/nginx-1.4.2-mem/sbin/nginx -t -c /usr/local/nginx-1.4.2-mem/conf/nginx.conf");
		system("/usr/local/nginx-1.4.2-mem/sbin/nginx -s reload");
	}
	return 2;
}
 

