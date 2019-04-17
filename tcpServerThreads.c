#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <ctype.h>

#define BUFSIZE 800
#define MESSAGE_SIZE 1024
#define HEADER_SIZE 7
#define RESPONSE_SIZE 1024
#define MAX_CLIENTS 10
#define LOGIN_REQ 0
#define REG_MESSAGE 1
#define INFO 2

char RESPONSE_LOGIN_SUCCESSFUL[] = "2";	//cod login cu success
char RESPONSE_LOGIN_BADPASS[] = "3";	//cod parola incorecta	
char RESPONSE_LOGIN_BADUSR[] = "4";	//cod user incorect

char users[MAX_CLIENTS][15];
char passwords[MAX_CLIENTS][15];
int usersNr;

char connected_users[MAX_CLIENTS][15];
int connected_users_nr;  

int clients_fd[MAX_CLIENTS];
pthread_t clients_thread[MAX_CLIENTS];
int k;

void disconnect(int mySocket, char *user)	//cautam client corespunzator si il deconectam
{
    int i;
    for (i = 0; i < k; i++)
    {
        if (clients_fd[i] == mySocket)
            clients_fd[i] = 0;
    }
    for (i = 0; i < connected_users_nr; i++) 	// "stergem" numele userilor deconectati
    {
        if (strcmp(connected_users[i], user) == 0)
        {
            strcpy(connected_users[i], "OUT");
        }
    }
    close(mySocket);
}
void read_users()				//citim fisierul de useri si punem datele in user si parola
{
    FILE *f = fopen("users.txt", "r");
    char line[31];
    char *p;
    if (f == NULL)
    {
        printf("Error opening the users file\n");
        exit(2);
    }
    while ((fscanf(f, "%s", line)) != EOF)	//se parcurge fisier linie cu linie si se sparge in user si parola
    {

        p = strtok(line, ":");
        strcpy(users[usersNr], p);
        p = strtok(NULL, ":");
        strcpy(passwords[usersNr++], p);
    }
    fclose(f);
}
int login(int mySocket, char *user, char *password)	//daca si user-ul si parola corespund, se trimite ca raspuns codul corespunzator
{

    int i;
    for (i = 0; i < usersNr; i++)
    {
        if (strcmp(user, users[i]) == 0)
        {
            if (strcmp(password, passwords[i]) == 0)
            {
                send(mySocket, RESPONSE_LOGIN_SUCCESSFUL, 2, 0);	//cod corespunzator login cu success
                strcpy(connected_users[connected_users_nr++], user); 	//aici adaugam un user
                return 0;
            }
            else
            {
                send(mySocket, RESPONSE_LOGIN_BADPASS, 2, 0);		//cod corespunzator parola incorecta

                return -1;
            }
        }
    }
    send(mySocket, RESPONSE_LOGIN_BADUSR, 2, 0);			//cod corespunzator user incorect
    return -1;
}

void send_message(int clientSocket,int type,char * user,char *message){	//se transmite mesaj la client 
    char header[HEADER_SIZE];
    char messageBody[MESSAGE_SIZE];
    int messageSize;
    messageSize=(strlen(user)+strlen(message)+2)*sizeof(char);		// se calculeaza lungimea mesajului
    snprintf(messageBody, messageSize, "%s:%s", user, message);		//se creeaza corpul mesajului
    snprintf(header, HEADER_SIZE, "%d:%4d", type, messageSize);		//se creeaza header-ul mesajului
    int s1=send(clientSocket, header, HEADER_SIZE, 0);			//se trimite header
    if(s1<0){
        printf("Error in sending the message");
        exit(1);
    }
    int s2=send(clientSocket, messageBody, messageSize, 0);		//se trimite corpul mesajului
    if(s2<0){
        printf("Error in sending the message");
        exit(1);
    }

}

void send_message_to_all(int mySocket,int type, char *user, char *message)	//se trimite mesajul la toti inafara de cel care il compune 
{
    int i;
    printf("Client: %s Message: %s Socket: %d\n", user, message, mySocket);
    for (i = 0; i < k; i++)
    {
        if (clients_fd[i] != mySocket && clients_fd[i] != 0)			//se cauta un client existent si diferit de cel care compune mesajul
        {
            send_message(clients_fd[i], type,user, message);
        }
    }
}

void c_who_is_online(int mySocket, char *user)		//functie pentru a vedea cine e online 
{
    char tempUsers[MESSAGE_SIZE] = "-----------------------------------------:\n\tConnected users:\n";
    int i;
    for (i = 0; i < connected_users_nr; i++)
    { 							//compunem un string cu utilizatorii conectati
        if (strcmp(connected_users[i], "OUT") != 0)
        { 						//fara cei care s-au deconecatat intre timp
            strcat(tempUsers, connected_users[i]);
            strcat(tempUsers, "\n");
        }
    }
    strcat(tempUsers, ":-----------------------------------------:");
    send_message(mySocket, INFO, "", tempUsers);	//trimitre mesaj de tip INFO
}	

void c_help(int mySocket, char *user)		//functie pentru ajutor cu comenzile disponibile
{
    char tempString[MESSAGE_SIZE] = "-----------------------------------------:\n\tSupported comands:\n";
    strcat(tempString, "/online -> see who is currently online\n");
    strcat(tempString, "/help -> see current menu\n");
    strcat(tempString, "/exit -> exit the chat\n");
    strcat(tempString, ":-----------------------------------------:");
    send_message(mySocket, INFO, "", tempString);	//trimitere de mesaj tip INFO
}

void *thread_client(void *newSocket)		//functia corespunzatoare thread-ului
{
    int *mySocketTemp = (int *)newSocket;
    int mySocket = *mySocketTemp;

    char buffer[MESSAGE_SIZE];
    char header_buf[HEADER_SIZE];

    char *user, *message, *typeS, *lengthS;
    int type, length;

    char connectionMessage[MESSAGE_SIZE];
    char exitMessage[MESSAGE_SIZE];

    int login_response;

    while (1)
    {
        recv(mySocket, header_buf, HEADER_SIZE, 0);	// daca nu e nici un mesaj disponibil, se asteapta primirea unuia; se citeste header_buf de lungime HEADER_SIZE

        typeS = strtok(header_buf, ":");		//se va desparti in tipul mesajului si lungimea acestuia
        type = atoi(typeS);
        lengthS = strtok(NULL, ":");
        length = atoi(lengthS);

        recv(mySocket, buffer, length, 0);		//se citeste buffer de lungime length si se desparte in user si mesaj
        user = strtok(buffer, ":");
        message = strtok(NULL, ":");
        printf("Receptionare mesaj!\n");
        if (strstr(message, "/exit") != NULL)		//daca s-a citit "/exit" clientul se va deconecta
        {

            sprintf(exitMessage, " DISCONNECTED");
            send_message_to_all(mySocket,INFO, user, exitMessage);	//se vor anunta toti clienti in legatura cu deconectarea 
            //printf("Disconnected from %s:%d\n",inet_ntoa(newAddr.sin_addr),ntohs(newAddr.sin_port));
            break;
        }
        else if (strstr(message, "/online"))	
        {
            c_who_is_online(mySocket, user); 	//comanda /online
        } 
        else if(strstr(message, "/help")){	
            c_help(mySocket, user); 		//comanda /help
        }
        else
        {
            if (type == LOGIN_REQ)		//daca mesajul e de tipul login request se va atribui valoarea corespunzatoare raspunsului la login
            {
                login_response = login(mySocket, user, message);
                if (login_response == -1)
                    break;
                sprintf(connectionMessage, " IS ONLINE");
                send_message_to_all(mySocket,INFO, user, connectionMessage);
            }
            else if (type == REG_MESSAGE)	//daca mesajul e de tip text se va trimite catre client
            {
                send_message_to_all(mySocket,REG_MESSAGE, user, message);
            }

            bzero(buffer, MESSAGE_SIZE);	//se sterge cei MESSAGE_SIZE bytes de memorie incepand cu locatia pointata de buffer(se scriu '\0')
        }
    }
    disconnect(mySocket, user);			//se deconecteaza clientul
    return NULL;
}

int main(int argc, char *argv[])
{
    int sockfd, ret;
    int port;
    char ipServer[15];
    struct sockaddr_in serverAddr;
    int newSocket;
    struct sockaddr_in newAddr;
    socklen_t addr_size;

    if (argc != 3)		//ne trebuie argumentele necesare pt functionare
    {
        printf("Usage : %s port server-address", argv[0]);
        exit(1);
    }
    read_users();		//se citesc userii
    port = atoi(argv[1]);	//portul va fi reprezentat de primul argument
    strcpy(ipServer, argv[2]);	//ipServer va fi reprezenatt de al doilea argument
#pragma region socket_init
    sockfd = socket(AF_INET, SOCK_STREAM, 0);	//se creeaza un soclu cu familia de protocole IPv4 (AF_INET), orientat pe conexiune (SOCK_STREAM), folosind protocolul implicit pt acest tip de socket
    if (sockfd < 0)
    {
        printf("[-]Error in connection.\n");
        exit(1);
    }
    printf("[+]Server Socket is created.\n");

    memset(&serverAddr, '\0', sizeof(serverAddr));		//se umple zona de memorie pointata de serverAddr cu bytes de '\0'
    serverAddr.sin_family = AF_INET;				//se seteaza familia de adrese
    serverAddr.sin_port = htons(port);				//se converteste portul din format intern al masinii in format extern al retelei(host to network long)
    serverAddr.sin_addr.s_addr = inet_addr(ipServer);		// se converteste adresa IPv4 din format normal , in format binar in ordinea folosita de retea

    ret = bind(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr));		//asociem socului (sockfd) , care nu e inca legat , o adresa IP si un port( strctura serverAddr )
    if (ret < 0)
    {
        printf("[-]Error in binding.\n");
        exit(1);
    }
    
    printf("[+]Bind to port %d \n", port);

    if (listen(sockfd, 10) == 0)		//se doreste acceptarea de conexiuni; mesajele care stabilesc conexiuni catre soclul sockfd sunt puse intr-o coada de asteptare
    {
        printf("Listening....\n");
    }
    else
    {
        printf("[-]Error in binding.\n");
        exit(1);
    }

#pragma endregion socket_init

    while (1)
    {
        newSocket = accept(sockfd, (struct sockaddr *)&newAddr, &addr_size);	//se preia o cerere din coada de conexuni formata si se creeaza un nou socket conectat la clinetul care a fost preluat din coada
        clients_fd[k] = newSocket;	//se contorizeaza clientul
        if (newSocket < 0)
        {
            exit(1);
        }
        printf("Connection accepted from %s:%d\n", inet_ntoa(newAddr.sin_addr), ntohs(newAddr.sin_port));	//se convertesc inapoi adresele pentru a putea fi citite

        int pt = pthread_create(&clients_thread[k++], NULL, &thread_client, (void *)&newSocket);		//se creeaza cate un thread pentru fiecare client
        if (pt != 0)
        {
            printf("Error in creating thread.\n");
            exit(1);
        }
    }
    pthread_exit(NULL);
    close(newSocket);
    return 0;
}
