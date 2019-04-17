#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>

#define BUFSIZE 800
#define MESSAGE_SIZE 1023
#define HEADER_SIZE 7
#define RESPONSE_SIZE 1024

#define LOGIN_REQ 0
#define REG_MESSAGE 1
#define INFO 2

char RESPONSE_LOGIN_SUCCESSFUL[] = "2";	//cod pentru login cu succes
char RESPONSE_LOGIN_BADPASS[] = "3";	//cod pentru parola incorecta
char RESPONSE_LOGIN_BADUSR[] = "4";	//cod pentru user incorect

void *read_message(void *newSocket)	//functie corespunzatoare thread-ului
{
    int *mySocketTemp = (int *)newSocket;
    int mySocket = *mySocketTemp;
    char response[RESPONSE_SIZE];
    char header[HEADER_SIZE];
    char *user, *message, *typeS, *lengthS;
    int type, length;
    
    while (1)
    {
        recv(mySocket, header, HEADER_SIZE, 0);		// daca nu e nici un mesaj disponibil, se asteapta primirea unuia; se citeste header de lungime HEADER_SIZE
        typeS = strtok(header, ":");			//despartire in tip si lungime
        type = atoi(typeS);
        lengthS = strtok(NULL, ":");
        length = atoi(lengthS);
        //printf("length:%d\n",length);	
        recv(mySocket, response, length, 0);		//citire a raspunsului de lungime length
        printf("%s\n",response);

    }
    return NULL;
}

void send_message(int clientSocket,int type,char * user,char *message){
    char header[HEADER_SIZE];
    char messageBody[MESSAGE_SIZE];
    int messageSize;
    messageSize=(strlen(user)+strlen(message)+2)*sizeof(char);		//se calculeaza lungimea mesajului
    snprintf(messageBody, messageSize, "%s:%s", user, message);		//se creeaza corpul mesajului
    snprintf(header, HEADER_SIZE, "%d:%4d", type, messageSize);		//se creeaza header-ul
    int s1=send(clientSocket, header, HEADER_SIZE, 0);			//se trimite header
    if(s1<0){
        printf("Error in sending the message");
        exit(1);
    }
    int s2=send(clientSocket, messageBody, messageSize, 0);		//se trimite corp mesaj
    if(s2<0){
        printf("Error in sending the message");
        exit(1);
    }

}

int main(int argc, char *argv[])
{
    int clientSocket, ret;
    int port;
    char ipServer[15];
    char user[15];
    char password[10];
    char login_response[2];
    struct sockaddr_in serverAddres;
    pthread_t t;

    char buffer[BUFSIZE];


    if (argc != 3)	//ne trebuie argumentlee necesare
    {
        printf("Usage : %s port server-address", argv[0]);
        exit(1);
    }
    port = atoi(argv[1]);		//portul va fi reprezentat de primul argument
    strcpy(ipServer, argv[2]);		//ipServer va fi reprezentat de al doilea argument

    clientSocket = socket(AF_INET, SOCK_STREAM, 0);	//se creeaza un soclu cu familia de protocole IPv4 (AF_INET), orientat pe conexiune (SOCK_STREAM), folosind protocolul implicit pt acest tip de socket

    if (clientSocket < 0)
    {
        printf("[-]Error in connection\n");
        exit(1);
    }
    printf("[+]Client Socket is created\n");

    memset(&serverAddres, '\0', sizeof(serverAddres));		//se umple zona de memorie pointata de serverAddr cu bytes de '\0'
    serverAddres.sin_family = AF_INET;				//se seteaza familia de adrese
    serverAddres.sin_port = htons(port);			//se converteste portul din format intern al masinii in format extern al retelei(host to network long)
    serverAddres.sin_addr.s_addr = inet_addr(ipServer);		// se converteste adresa IPv4 din format normal , in format binar in ordinea folosita de retea

    ret = connect(clientSocket, (struct sockaddr *)&serverAddres, sizeof(serverAddres));	//se stabileste o conexiune intre procesul apelant si procesul aflat la distanta( serverAddress )
    if (ret < 0)
    {
        printf("[-]Error in connection\n");
        exit(1);
    }
    printf("[+]Connected to Server.\n");
    //login
    printf("Username:");	//se introduce user
    scanf("%s", user);
    printf("Password:");	//se introduce parola
    scanf("%s", password);

    send_message(clientSocket,LOGIN_REQ,user,password);		//se trimite mesajul
    
    recv(clientSocket, login_response, 2, 0);			//se citeste raspunsul venit de la login
    getchar();

    if ((strcmp(login_response, RESPONSE_LOGIN_SUCCESSFUL)) == 0)		//daca raspunsul a fost afirmativ se fac urmatoarele
    {
        pthread_create(&t, NULL, &read_message, (void *)&clientSocket);		//se creeza un thread care are rolul de a prelua mesaje

        while (1)
        {
            fgets(buffer, BUFSIZ, stdin);					//se citeste textul (buffer) de dimensiune BUFSIZ

            buffer[strlen(buffer) - 1] = '\0';

            if (strcmp(buffer, "\0") == 0 || strcmp(buffer, " ") == 0)		//se verifica textul sa nu fie gol, pentru a avea permisiune de a trimite mesaj
                printf("The message can not be empty! \n");
            else
            {
                send_message(clientSocket,REG_MESSAGE,user,buffer);		//se trimite mesajul
                
                if (strcmp(buffer, "/exit") == 0)				//daca mesajul e "/exit" clientul se va deconecta
                {
                    close(clientSocket);
                    printf("[-]Dissconected form server.\n");
                    exit(1);
                }
            }
        }
    }
    else if ((strcmp(login_response, RESPONSE_LOGIN_BADPASS)) == 0)		//daca parola e incorecta , mesaj de eroare
    {
        printf("Wrong password! \n");
    }
    else if ((strcmp(login_response, RESPONSE_LOGIN_BADUSR)) == 0)		//daca user e icorect ,mesaj de eroare
    {
        printf("No such user! \n");
    }
    return 0;
}
