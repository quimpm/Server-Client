#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "structs.c"
#include <sys/time.h>
#include <time.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <unistd.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>

struct config_client get_config(FILE *fitxer_config, char nom_fitxer[50]);
struct server_register registre(int sock, struct server_register register_req, struct sockaddr_in addr_server, char *argv[], struct config_client info_client, int fdr[2]);
struct server_register register_protocol(int sock, struct server_register register_req, struct sockaddr_in addr_server,char *argv[]);
void debuger(char debug_message [100]);
struct server_register alive_protocol (int sock, struct server_register register_response, struct sockaddr_in	addr_server, struct config_client info_client);
void alive (int sock, struct server_register register_response, struct sockaddr_in	addr_server, struct config_client info_client, int fdr[2],int sock_tcp,char buffer[20],int fd[2],
            struct server_register_tcp tcp_pdu, struct sockaddr_in addr_server_tcp, int size_buffer);
void quit(char buffer[20], int fd[2], struct timeval read_timeout, fd_set rfds,int buffer_size, int sock,int sock_tcp);
void console_listener(char buffer[20], int fd[2], int fdr[2], struct timeval read_timeout,int buffer_size, struct server_register_tcp tcp_pdu, int sock_tcp, 
                        struct server_register register_response, struct config_client info_client, struct sockaddr_in addr_server_tcp, int size_fitxer);
void get_parameters(int argc, char *argv[]);
void socket_bind(int sock, char *argv[],struct sockaddr_in addr_cli);
struct sockaddr_in info_server(struct config_client info_client,struct sockaddr_in	addr_server, int sock);
void send_conf(struct server_register_tcp tcp_pdu, int sock_tcp, struct server_register register_response, struct config_client info_client, 
                struct sockaddr_in addr_server_tcp, int size_fitxer);
void get_conf(struct server_register_tcp tcp_pdu, int sock_tcp, struct server_register register_response, struct config_client info_client, 
                struct sockaddr_in addr_server_tcp, int size_fitxer);

#define h_addr h_addr_list[0]
#define t	2
#define n	3
#define m	4
#define p	8
#define s	5
#define q	3
#define r   3
#define u   3
#define w   4

int num_reg = 0; /*Variable gloval per determinar els cops que s'ha iniciat una fase de registre*/
int consecutive_alive = 0; /*Variable gloval per determinar els cops que s'ha iniciat una fase d'enviament d'alives*/
char estat[20]; /*Variable gloval per a declarar l'estat en que es troba el client*/
int debug = 0;
char nom_fitxer_tcp [20];
char nom_fitxer[20];


int main(int argc, char *argv[]){
    
    char buffer[20];
    struct config_client info_client;
    FILE  *fitxer_config = NULL;
    struct server_register register_req, register_response;
    int sock, fd[2], fdr[2], sock_tcp;
    struct sockaddr_in	addr_server,addr_cli, addr_server_tcp;/*Estructura d'adreça del client i del servidor.*/
    struct timeval read_timeout;
    struct server_register_tcp tcp_pdu;
    fd_set rfds;
    debuger("Inici del client");
    strcpy(estat, "DISCONNECTED");
    /*Pipes d'anada i tornada fill-pare*/
    pipe(fdr);
    pipe(fd);

    /*Tractament dels paràmetres introduits*/
    get_parameters(argc, argv);
    /*Agafem dades del client*/
    info_client=get_config(fitxer_config,nom_fitxer);
    /*Creació socket UDP*/
    debuger("Creant socket client");
    sock=socket(AF_INET,SOCK_DGRAM,0);
    if(sock<0)
	{
		fprintf(stderr,"No puc obrir socket!!!\n");
		perror(argv[0]);
		exit(-1);
	}
    /*Binding*/
    socket_bind(sock,argv,addr_cli);
    /*Agafem informació del servidor*/
    addr_server=info_server(info_client,addr_server,sock);

    /*Creem procés fill per a llegir continuament de la consola*/
        
    sock_tcp=socket(AF_INET,SOCK_STREAM,0);
    if(sock_tcp<0)
    {
        fprintf(stderr,"No puc obrir socket!!!\n");
        perror(argv[0]);
        exit(-1);
    }

    while(1)
    {
            quit(buffer,fd,read_timeout,rfds,sizeof(buffer),sock,sock_tcp);
            if(strcmp(estat,"DISCONNECTED")==0)
            {
                register_response = registre(sock, register_req, addr_server, argv, info_client,fdr);
            }
            if(strcmp(estat,"REGISTERED")==0 || strcmp(estat,"ALIVE")==0)
            {
                alive(sock,register_response,addr_server,info_client,fdr,sock_tcp,buffer,fd,tcp_pdu,addr_server_tcp,sizeof(buffer)); 
            } 
    }
    return 0;
}

/*Tractament del fitxer de configuració del client*/
struct config_client get_config(FILE *fitxer_config, char nom_fitxer[50])
{

    char buffer[100], *token, dades_client[4][20];
    struct config_client info_client;
    int i=0;

    debuger("Agafant dades del fitxer de configuració");

    fitxer_config = fopen(nom_fitxer, "r"); 
    if(fitxer_config == NULL)
    {
        perror("No ha sigut possible obrir l'arxiu.\n");
        exit(-1);
    }

    while(fgets(buffer,100,fitxer_config)!=NULL)
    {
        token = strtok(buffer," ");
        token = strtok(NULL,"\n");
        strcpy(dades_client[i], token);
        i++; 
    }
    
    memset(&info_client,0,sizeof(info_client));
    strcpy(info_client.nom, dades_client[0]);
    strcpy(info_client.mac, dades_client[1]);
    strcpy(info_client.id_servidor, dades_client[2]);
    info_client.port_servidor = atoi(dades_client[3]);
    
    return info_client;
}

struct sockaddr_in info_server(struct config_client info_client,struct sockaddr_in	addr_server, int sock){
    struct hostent *ent = malloc(sizeof *ent);/*Informació del servidor*/
    ent=gethostbyname(info_client.id_servidor);

    /*Omplim l'estructura d'adreça del servidor amb les dades del servidor on enviarem la informació*/
    memset(&addr_server,0,sizeof (struct sockaddr_in));
	addr_server.sin_family=AF_INET;
	addr_server.sin_addr.s_addr=(((struct in_addr *)ent->h_addr)->s_addr);
	addr_server.sin_port=htons(info_client.port_servidor);

    return addr_server;
}

struct server_register registre(int sock, struct server_register register_req, struct sockaddr_in addr_server, char *argv[], struct config_client info_client, int fdr[2])
{
    
    struct server_register register_response;
    
    debuger("Iniciant un proces de registre");

    /*PDU que pasarem al servidor per registrarnos*/
    memset(&register_req,0,sizeof(register_req));
    register_req.tipus_paquet = 0x00;
    strcpy(register_req.nom_equip, info_client.nom);
    strcpy(register_req.mac_adress, info_client.mac);
    strcpy(register_req.num_aleatori, "000000");
    strcpy(register_req.dades, "");
    
    /*Tractament d'enviament de paquets*/
    num_reg=0;
    register_response = register_protocol(sock,register_req,addr_server,argv);
    

    /*Tractament en cas de rebre REGISTER NACK*/
    
    while(num_reg<q && register_response.tipus_paquet == 0x02)
    {
        register_response = register_protocol(sock,register_req,addr_server,argv);
    }
    
    /*Tractament encas de rebre REGISTER REJ*/
    if(register_response.tipus_paquet == 0x03)
    {
        write(fdr[1], "EXIT_REG", 20);
        fprintf(stderr,"La solicitació de registre ha estat rebutjada per el seguent motiu: %s\n",register_response.dades);
        close(sock);
        exit(-1);
    }
    /*Tractament en cas de rebre REGISTER ERROR*/
    if(register_response.tipus_paquet == 0x09){
        write(fdr[1], "EXIT_REG", 20);
        fprintf(stderr,"S'ha comés un error en el protocol\n");
        close(sock);
        exit(-1);
    }
    /*Tractament en cas de rebre REGISTER ACK*/
    if(register_response.tipus_paquet == 0x01)
    {
        strcpy(estat, "REGISTERED");
        write(fdr[1], estat, 20);
        debuger("Fi del procés de registre. Estat actual: REGISTERED");
    }
    /*Tractament en cas de passar nombre màxim de procesos de registre*/
    if(num_reg == q && (register_response.tipus_paquet == 0x02 || register_response.tipus_paquet == 0x00))
    {
        write(fdr[1], "EXIT_REG", 20);
        fprintf(stderr,"Error en el procés de registre, superat nombre maxim d'intents de registre.\n");
        close(sock);
        exit(-1);
    }
    return register_response;

}

/*Tractament d'enviament de paquets*/
struct server_register register_protocol(int sock, struct server_register register_req, struct sockaddr_in addr_server, char *argv[])
{
    int send, recv, x, j;
    struct timeval read_timeout;
    fd_set rfds;
    recv=-1;

        
    while (recv == -1 && num_reg<q)
    {
        x=2;
        for(j=1;j<=p && recv==-1;j++){
            /*Enviament de paquets en l'interval t*/
            if (j<n && recv==-1)
            {
                read_timeout.tv_sec = t;
                read_timeout.tv_usec = 0;
                FD_ZERO(&rfds);        
                FD_SET(sock, &rfds);
                send=sendto(sock,&register_req,sizeof(register_req)+1,0,(struct sockaddr*)&addr_server,sizeof(addr_server));
                if(send<0)
                {
                    fprintf(stderr,"Error al sendto\n");
                    exit(-1);
                }
                debuger("Petició de registre enviada. Estat actual: WAIT_REG");
                strcpy(estat,"WAIT_REG");
                select(sock+1, &rfds, NULL, NULL, &read_timeout);
                if(FD_ISSET(sock, &rfds))
                {
                    recv=recvfrom(sock,&register_req,sizeof(register_req),0,(struct sockaddr *)0,(socklen_t *) 0);
                }   
            }
            /*Enviament de paquets en l'interval xt*/
            if(x<m && j>=n && recv==-1)
            {
                read_timeout.tv_sec = x*t;
                read_timeout.tv_usec = 0;
                FD_ZERO(&rfds);        
                FD_SET(sock, &rfds);
                send=sendto(sock,&register_req,sizeof(register_req)+1,0,(struct sockaddr*)&addr_server,sizeof(addr_server));
                if(send<0)
                {
                    fprintf(stderr,"Error al sendto\n");
                    exit(-1);
                }
                x++;
                debuger("Petició de registre enviada. Estat actual: WAIT_REG");
                strcpy(estat,"WAIT_REG");
                select(sock+1, &rfds, NULL, NULL, &read_timeout);
                if(FD_ISSET(sock, &rfds))
                {
                    recv=recvfrom(sock,&register_req,sizeof(register_req),0,(struct sockaddr *)0,(socklen_t *) 0);
                } 
            }
            /*Enviament de paquets en l'interval mt*/

            if(j<p && j>=n+2 && recv==-1)
            {
                read_timeout.tv_sec = m*t;
                read_timeout.tv_usec = 0;
                FD_ZERO(&rfds);        
                FD_SET(sock, &rfds);
                send=sendto(sock,&register_req,sizeof(register_req)+1,0,(struct sockaddr*)&addr_server,sizeof(addr_server));
                if(send<0)
                {
                    fprintf(stderr,"Error al sendto\n");
                    exit(-1);
                }
                debuger("Petició de registre enviada. Estat actual: WAIT_REG");
                strcpy(estat,"WAIT_REG");
                select(sock+1, &rfds, NULL, NULL, &read_timeout);
                if(FD_ISSET(sock, &rfds))
                {
                    recv=recvfrom(sock,&register_req,sizeof(register_req),0,(struct sockaddr *)0,(socklen_t *) 0);
                }
            }

            if(j==p && recv==-1){
                read_timeout.tv_sec = 5;
                read_timeout.tv_usec = 0;
                FD_ZERO(&rfds);        
                FD_SET(sock, &rfds);
                send=sendto(sock,&register_req,sizeof(register_req)+1,0,(struct sockaddr*)&addr_server,sizeof(addr_server));
                if(send<0)
                {
                    fprintf(stderr,"Error al sendto\n");
                    exit(-1);
                }
                debuger("Petició de registre enviada. Estat actual: WAIT_REG");
                strcpy(estat,"WAIT_REG");
                select(sock+1, &rfds, NULL, NULL, &read_timeout);
                if(FD_ISSET(sock, &rfds))
                {
                    recv=recvfrom(sock,&register_req,sizeof(register_req),0,(struct sockaddr *)0,(socklen_t *) 0);
                }
            }
        }
        /*if(register_req.tipus_paquet != 0x09 && register_req.tipus_paquet != 0x03 && register_req.tipus_paquet != 0x01 && num_reg<q-1)
        {
            sleep(s);
        }*/
        num_reg++;
    }
    if(recv!=-1)
    {
        if(register_req.tipus_paquet==0x01){
            debuger("Rebut REGISTER_ACK");
        }
        if(register_req.tipus_paquet==0x02){
            debuger("Rebut REGISTER_NACK");
        }
        if(register_req.tipus_paquet==0x03){
            debuger("Rebut REGISTER_REJ");
        }
        if(register_req.tipus_paquet==0x09){
            debuger("Rebut ERROR");
        }
    }
    return register_req;
}

/*Tractament enviament alive*/
void alive (int sock, struct server_register register_response, struct sockaddr_in	addr_server, struct config_client info_client, int fdr[2],int sock_tcp,char buffer[20],int fd[2],
            struct server_register_tcp tcp_pdu, struct sockaddr_in addr_server_tcp, int size_buffer)
{       
    struct server_register alive_inf;
    struct timeval read_timeout;

    alive_inf=alive_protocol(sock,register_response,addr_server,info_client);

        if(strcmp(alive_inf.mac_adress,register_response.mac_adress)==0 && strcmp(alive_inf.nom_equip,register_response.nom_equip)==0 && alive_inf.tipus_paquet == 0x11)
        {
            if(strcmp(estat,"REGISTERED")==0){
                if (fork()==0)
                {
                    console_listener(buffer,fd,fdr,read_timeout,size_buffer,tcp_pdu,sock_tcp,register_response,info_client,addr_server_tcp, sizeof(nom_fitxer_tcp));   
                }
            }
            strcpy(estat,"ALIVE");
            debuger("S'ha rebut una confirmació: ALIVE_ACK. Estat actual: ALIVE");
            write(fdr[1], estat, 20);
            consecutive_alive=0;
        }
        if(alive_inf.tipus_paquet==0x12)
        {
            debuger("Rebut ALIVE_NACK");
            consecutive_alive++;
        }
        if(alive_inf.tipus_paquet==0x13)
        {
            debuger("El servidor ha rebutjat la conexió: ALIVE_REJ. Estat actual: DISCONNECTED");
            strcpy(estat,"DISCONNECTED");
            write(fdr[1], estat, 20);
            consecutive_alive=0;
        }
        if(consecutive_alive>=u)
        {
            debuger("No s'ha rebut resposta a 3 alives consecutius. Estat actual: DISCONNECTED");
            strcpy(estat,"DISCONNECTED");
            write(fdr[1], estat, 20);
            consecutive_alive=0;
        }
}

struct server_register alive_protocol (int sock, struct server_register register_response, struct sockaddr_in	addr_server, struct config_client info_client)
{
    struct server_register alive_inf;
    struct timeval read_timeout;
    fd_set rfds;
    int send,recv=-1;

    /*PDU que pasarem al servidor per enviar alives*/
    memset(&alive_inf,0,sizeof(alive_inf));
    alive_inf.tipus_paquet = 0x10;
    strcpy(alive_inf.nom_equip, info_client.nom);
    strcpy(alive_inf.mac_adress, info_client.mac);
    strcpy(alive_inf.num_aleatori, register_response.num_aleatori);
    strcpy(alive_inf.dades, "");
    
    read_timeout.tv_sec = 0;
    read_timeout.tv_usec = 0;
    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);
    send=sendto(sock,&alive_inf,sizeof(alive_inf)+1,0,(struct sockaddr*)&addr_server,sizeof(addr_server));
    if(send<0)
    {
        fprintf(stderr,"Error al sendto\n");
        exit(-1);
    }
    debuger("Alive enviat");
    sleep(r);
    select(sock+1, &rfds, NULL, NULL, &read_timeout);
    if(FD_ISSET(sock, &rfds))
    {
        recv=recvfrom(sock,&alive_inf,sizeof(alive_inf),0,(struct sockaddr *)0,(socklen_t *) 0);
    } 
    if(recv<0)
    {
        debuger("No s'ha rebut resposta del servidor");
        consecutive_alive++;
    }
    return alive_inf;
}
/*Funció debug*/
void debuger(char debug_message [100])
{
    time_t sec; 
    struct tm* current_time; 
    sec = time(NULL); 
    current_time = localtime(&sec); 
    if(debug==1)
    {
        printf("%02d:%02d:%02d ", 
           current_time->tm_hour, 
           current_time->tm_min, 
           current_time->tm_sec); 
        printf("DEBUG => %s\n", debug_message);
    }
}

/*Consola concurrent d'es d'on llegirem comandes*/
void console_listener(char buffer[20], int fd[2], int fdr[2], struct timeval read_timeout,int buffer_size, struct server_register_tcp tcp_pdu, int sock_tcp, 
                        struct server_register register_response, struct config_client info_client, struct sockaddr_in addr_server_tcp, int size_fitxer){
    fd_set rfds;
    close(fd[0]);
    
    while(1){
        memset(buffer,0,buffer_size);
        read_timeout.tv_sec = 0;
        read_timeout.tv_usec = 0;
        FD_ZERO(&rfds); 
        FD_SET(fdr[0], &rfds);
        if(select(fdr[0]+1, &rfds, NULL, NULL, &read_timeout)!=0)
        {
            read(fdr[0], estat, 20);
            if(strcmp(estat,"EXIT_REG")==0){
                close(sock_tcp);
                exit(0);
            }    
        }
        if(strcmp(estat,"DISCONNECTED")!=0){
            read(STDIN_FILENO, buffer, buffer_size);
            if (strcmp(buffer,"quit\n")==0)
            {
                write(fd[1], buffer, buffer_size);
                exit(0);
            }else if(strcmp(buffer,"send-conf\n")==0)
            {
                send_conf(tcp_pdu,sock_tcp,register_response,info_client,addr_server_tcp, sizeof(nom_fitxer_tcp));
            }else if(strcmp(buffer,"get-conf\n")==0)
            {
                get_conf(tcp_pdu,sock_tcp,register_response,info_client,addr_server_tcp, sizeof(nom_fitxer_tcp));
            }else{
                debuger("Comanda invàlida");
            }
        }
    }   
}
/*Tancar procés pare*/
void quit(char buffer[20], int fd[2], struct timeval read_timeout, fd_set rfds, int buffer_size, int sock,int sock_tcp){
    read_timeout.tv_sec = 0;
    read_timeout.tv_usec = 0;
    FD_ZERO(&rfds); 
    FD_SET(fd[0], &rfds);
    if(select(fd[0]+1, &rfds, NULL, NULL, &read_timeout)!=0)
    {
        read(fd[0], buffer, buffer_size);
        if(strcmp(buffer,"quit\n")==0)
        {
            close(sock);
            close(sock_tcp);
            wait(NULL);
            exit(0);
        }
                
    }
}
/*Agafar parametres d'entrada*/
void get_parameters(int argc, char *argv[]){
    int opt;
    strcpy(nom_fitxer,"client.cfg");
    strcpy(nom_fitxer_tcp,"boot.cfg");
     while((opt = getopt(argc, argv, "df:c:")) != -1)  
    {  
        switch(opt)  
        {  
            case 'd': 
                debug=1; 
                break;  
            case 'f':
                strcpy(nom_fitxer_tcp, optarg);
                break;  
            case 'c':  
                strcpy(nom_fitxer, optarg);  
                break;  
            case '?':  
                printf("Us:\n");
                printf("%s client.cfg\n",  argv[0]);
                printf("%s -c <nom-arxiu>\n",  argv[0]);
                exit(0);
                break;  
        }  
    }
}
/*Binding socket UDP*/
void socket_bind(int sock, char *argv[],struct sockaddr_in addr_cli){

    memset(&addr_cli,0,sizeof (struct sockaddr_in)); 
	addr_cli.sin_family=AF_INET;
	addr_cli.sin_addr.s_addr=htonl(INADDR_ANY);
	addr_cli.sin_port=htons(0);

    if(bind(sock,(struct sockaddr *)&addr_cli,sizeof(struct sockaddr_in))<0)
	{
		fprintf(stderr,"No puc fer el binding del socket!!!\n");
        close(sock);
        exit(-1);
	}
    
}
/*Tractament send-conf*/
void send_conf(struct server_register_tcp tcp_pdu, int sock_tcp, struct server_register register_response, struct config_client info_client, 
                struct sockaddr_in addr_server_tcp, int size_fitxer){
    char str [5], str2 [20];
    char buffer[150];
    struct timeval read_timeout;
    FILE  *fitxer_config_tcp = NULL;
    fd_set rfds;

    FD_ZERO(&rfds);
    FD_SET(sock_tcp, &rfds);
    read_timeout.tv_sec = w;
    read_timeout.tv_usec = 0;

    memset(&addr_server_tcp, '0', sizeof(addr_server_tcp));
    addr_server_tcp.sin_family = AF_INET; 
    addr_server_tcp.sin_port = htons(atoi(register_response.dades));
    if(inet_pton(AF_INET, "127.0.0.1", &addr_server_tcp.sin_addr)<=0)  
    { 
        printf("L'adreça no és vàlida \n");
        close(sock_tcp); 
        exit(-1); 
    } 
    if (connect(sock_tcp, (struct sockaddr *)&addr_server_tcp, sizeof(addr_server_tcp)) < 0) 
    { 
        printf("Conexió fallida \n"); 
        close(sock_tcp);
        exit(-1); 
    }
    tcp_pdu.tipus_paquet = 0x020;
    strcpy(tcp_pdu.nom_equip,info_client.nom);
    strcpy(tcp_pdu.mac_adress,info_client.mac);
    strcpy(tcp_pdu.num_aleatori,register_response.num_aleatori);
    sprintf(str, "%i", size_fitxer);
    strcpy(str2,nom_fitxer_tcp);
    strcpy(tcp_pdu.dades,strcat(strcat(str2,","),str));

    if(send(sock_tcp, &tcp_pdu, sizeof(tcp_pdu), 0)<0){
        fprintf(stderr,"Error en l'enviament per el canal TCP");
        close(sock_tcp);
        exit(-1);
    }
    debuger("Enviada pdu del tipus: SEND_FILE");
    select(sock_tcp+1, &rfds, NULL, NULL, &read_timeout);
    if(FD_ISSET(sock_tcp, &rfds))
    {
        read(sock_tcp, &tcp_pdu,sizeof(tcp_pdu));    
    }else{
        close(sock_tcp);
        debuger("No s'ha rebut resposta a SEND FILE");
        exit(0);
    }
    
    if(tcp_pdu.tipus_paquet==0x21 && strcmp(tcp_pdu.nom_equip,register_response.nom_equip)==0 && strcmp(tcp_pdu.mac_adress,register_response.mac_adress)==0){
        debuger("Rebut SEND ACK, inici d'enviament del fitxer de configuració");
        fitxer_config_tcp = fopen(nom_fitxer_tcp, "r"); 
        if(fitxer_config_tcp == NULL)
        {
            debuger("No ha sigut possible obrir l'arxiu.");
            close(sock_tcp);
            exit(-1);
        }
        tcp_pdu.tipus_paquet = 0x024;
        strcpy(tcp_pdu.nom_equip,info_client.nom);
        strcpy(tcp_pdu.mac_adress,info_client.mac);
        strcpy(tcp_pdu.num_aleatori,register_response.num_aleatori);
        while(fgets(buffer,150,fitxer_config_tcp)!=NULL)
        {
            strcpy(tcp_pdu.dades,buffer);
            if(send(sock_tcp, &tcp_pdu, sizeof(tcp_pdu), 0)<0){
                debuger("Error en l'enviament per el canal TCP");
                close(sock_tcp);
                exit(-1);
            }
        }

        fclose( fitxer_config_tcp );

        tcp_pdu.tipus_paquet = 0x025;
        strcpy(tcp_pdu.nom_equip,info_client.nom);
        strcpy(tcp_pdu.mac_adress,info_client.mac);
        strcpy(tcp_pdu.num_aleatori,register_response.num_aleatori);
        memset(&tcp_pdu.dades,0,sizeof(tcp_pdu.dades));
        strcpy(tcp_pdu.dades,buffer);
        if(send(sock_tcp, &tcp_pdu, sizeof(tcp_pdu), 0)<0){
            fprintf(stderr,"Error en l'enviament per el canal TCP");
            close(sock_tcp);
            exit(-1);
        }
        debuger("Finalitzat l'enviament del fitxer de configuració");
        close(sock_tcp);
    }else{
        debuger("No es pot enviar el fitxer de configuració");
        close(sock_tcp);
        exit(-1);
    }
}    
/*Tractament get-cong*/
void get_conf(struct server_register_tcp tcp_pdu, int sock_tcp, struct server_register register_response, struct config_client info_client, 
                struct sockaddr_in addr_server_tcp, int size_fitxer){

    char str [5], str2 [20];
    struct timeval read_timeout;
    FILE  *fitxer_config_tcp = NULL;
    fd_set rfds;

    FD_ZERO(&rfds);
    FD_SET(sock_tcp, &rfds);
    read_timeout.tv_sec = w;
    read_timeout.tv_usec = 0;

    memset(&addr_server_tcp, '0', sizeof(addr_server_tcp));
    addr_server_tcp.sin_family = AF_INET; 
    addr_server_tcp.sin_port = htons(atoi(register_response.dades));
    if(inet_pton(AF_INET, "127.0.0.1", &addr_server_tcp.sin_addr)<=0)  
    { 
        debuger("L'adreça no és vàlida"); 
        close(sock_tcp);
        exit(-1); 
    } 
    if (connect(sock_tcp, (struct sockaddr *)&addr_server_tcp, sizeof(addr_server_tcp)) < 0) 
    { 
        debuger("Conexió fallida"); 
        close(sock_tcp);
        exit(-1); 
    }

    tcp_pdu.tipus_paquet = 0x030;
    strcpy(tcp_pdu.nom_equip,info_client.nom);
    strcpy(tcp_pdu.mac_adress,info_client.mac);
    strcpy(tcp_pdu.num_aleatori,register_response.num_aleatori);
    sprintf(str, "%i", size_fitxer);
    strcpy(str2,nom_fitxer_tcp);
    strcpy(tcp_pdu.dades,strcat(strcat(str2,","),str));

    if(send(sock_tcp, &tcp_pdu, sizeof(tcp_pdu), 0)<0){
        fprintf(stderr,"Error en l'enviament per el canal TCP");
        close(sock_tcp);
        exit(-1);
    }
    debuger("Enviada pdu del tipus: GET_FILE");
    debuger("Inicia procés de rebuda i escriptura al fitxer de configuració de les dades");
    fitxer_config_tcp = fopen(nom_fitxer_tcp, "w"); 
    while(tcp_pdu.tipus_paquet!=0x35)
    {
        select(sock_tcp+1, &rfds, NULL, NULL, &read_timeout);
        if(FD_ISSET(sock_tcp, &rfds))
        {
            read(sock_tcp, &tcp_pdu,sizeof(tcp_pdu));    
        }else{
            close(sock_tcp);
            debuger("No s'ha rebut resposta a SEND FILE");
            exit(0);
        }
        if(tcp_pdu.tipus_paquet==0x031 || tcp_pdu.tipus_paquet==0x34 || tcp_pdu.tipus_paquet==0x35)
        {
            if(fitxer_config_tcp == NULL)
            {
                debuger("No ha sigut possible obrir l'arxiu.");
                fclose(fitxer_config_tcp);
                close(sock_tcp);
                exit(-1);
            }
            fprintf(fitxer_config_tcp,"%s",tcp_pdu.dades);
        }else{
            fprintf(stderr,"No s'ha rebut de forma correcta el fitxer de configuració\n");
            fclose(fitxer_config_tcp);
            close(sock_tcp);
            exit(-1);
        }
    }
    debuger("Fi de la rebuda i escriptura de dades");
    fclose(fitxer_config_tcp);
    close(sock_tcp);
    exit(1);
}