#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <netdb.h>
#include <fcntl.h>

#define BUFLEN 500
#define MAX_CLIENTS 10

/** structura care mapeaza numele fisierului comenzii asociate continutului **/
typedef struct
{	char name[50];
	char request[BUFLEN];	
}TCache;

int main(int argc, char *argv[]) {
	/** socketi **/
	int sockfd, newsockfd;
	/** port **/
	int portno;
	/** lungime structura client **/
	unsigned int clilen;
	/** buffer primire mesaj de la client **/
    char buffer[BUFLEN];
    struct sockaddr_in serv_addr, cli_addr;
    int i, n, N;

    /** multimea de citire folosita in select() **/
    fd_set read_fds;
    /** multime folosita temporar **/
    fd_set tmp_fds;	
    /** valoare maxima file descriptor din multimea read_fds **/
    int fdmax;		

    /** verificare numar parametrii **/
    if (argc < 2) {
        printf("Numar gresit de parametrii\n");
        exit(0);
    }

    /** golim multimea de descriptori de citire (read_fds) si multimea tmp_fds **/
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);
     
    /** creare socket **/
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
    	printf("Eroare deschidere socket\n");
    	exit(0);
    }
     
    portno = atoi(argv[1]);
    /** setare proprietati structura server **/
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    
    /** legare proprietati la socket **/
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0) {
    	printf("Eroare bined\n");
    	exit(0);
    }
        
    listen(sockfd, MAX_CLIENTS);

    /** adaugam noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds **/
    FD_SET(sockfd, &read_fds);
    fdmax = sockfd;

    /** vecotr de structuri pentru a retine numele tuturor fisierelor deschise si comanda asociata fiecaruia **/
    TCache *cached_commands = calloc(100, sizeof(TCache));
    N = 0;
    TCache request_cache;

	while (1) {
		tmp_fds = read_fds; 
		/** apel select **/
		if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1) {
			printf("Eroare select\n");
			exit(0);
		}
		/** parcurgem multimea descriptorilor **/
		for(i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == sockfd) {
					/** am primit un mesaj pe o noua conexiune, deci apelam accept() **/
					clilen = sizeof(cli_addr);
					if ((newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen)) == -1) {
						printf("Eroare accept\n");
						exit(0);
					} 
					else {
						/** adaugam noul socket intors de accept() la multimea descriptorilor de citire **/
						FD_SET(newsockfd, &read_fds);
						if (newsockfd > fdmax) { 
							fdmax = newsockfd;
						}
					}
					printf("Noua conexiune de la %s, port %d, socket_client %d\n ", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), newsockfd);
				}
					
				else {
					/** am primit date pe unul din socketii cu care vorbesc cu clientii **/
					memset(buffer, 0, BUFLEN);
					if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
						printf("Eroare recv\n");
						/** scoatem socketul din multimea socketilor ascultati **/
						close(i); 
						FD_CLR(i, &read_fds);
					} 
					
					else {
						/** raspunsul care va fi trimis clientului **/
						char*raspuns = calloc(BUFLEN, sizeof(char));
						/** copie de rezerva pentru comanda primita **/
						char*request = strdup(buffer);
						strcat(request, "\n\n");
    					printf("request: \n%s\n", request);
						printf ("Am primit de la clientul de pe socketul %d, mesajul: %s\n", i, buffer);
						/** extragem cmanda primita, care este primul "cuvant" din mesaj **/
						char*token = strtok(buffer, " ");
						char*comanda = strdup(token);
						/** verificam daca am primit o comanda de GET, POST sau HEAD. Daca nu, consider ca
							am primit o comanda invalida si trimit eroarea 400: Bad Request **/
						if(strcmp(comanda, "GET") != 0 && strcmp(comanda, "POST") != 0 && strcmp(comanda, "HEAD") != 0){
							strcpy(raspuns, "HTTP/1.0 400 Bad Request\n");
							send(i, raspuns, strlen(raspuns), 0);
							FD_CLR(i, &read_fds); // scoatem din multimea de citire socketul pe care
							close(i); 
							continue;
						}
						char* url;
						int found_url = 0;
						/** parsam url-ul pentru a extrage adresa serverului.
							verificam intai cazul in care comanda contine url-ul complet **/
						while(token != NULL){
							if(strncmp(token, "http://", 6) == 0){
								url = strdup(token);
								found_url = 1;
								break;
							}
							token = strtok(NULL, " ");
						}
						char*server;
						/** in cazul in care nu am primit o comanda cu url complet verificam cazul in care am pimit o
							cale relativa**/
						if(found_url == 1){
							printf("url: %s\n", url);
							char*aux = strtok(url, "/");
							aux = strtok(NULL, "/");
							server = strdup(aux);
						}else{
							char* host = calloc(BUFLEN, sizeof(char));
							host = strstr(request, "Host:");
							server = strdup(host + 6);
						}
						printf("server: %s\n", server);	
    					printf("comanda: %s\n", comanda);
    					int found = -1;

    					/** cautam sa vedem daca comanda primita de afla in vectorul de structuri, deci a mai fost
    						primita anterior, caz in care o vom lua direct din fisierul asociat **/
						for(int j = 0; j < N; j++){
							if(strcmp(request, cached_commands[j].request) == 0){
								found = j;
								break;
							}
						}
						/** cautam host-ul cu numele extras din comanda primita **/				
    					struct hostent *h = gethostbyname(server);
    					char*ip = calloc(30, sizeof(char));
    					/** daca comanda nu a fost gasita in vector si gethostbyname s-a apelat cu succes inseamna ca
    						trebuie sa trimitem cererea serverului si sa salvam intr-un nou fisier raspunsul primit **/
						if(h != NULL && found == -1){
							/** extragem ip-ul serverului din structura rezultata in urma apelului
								gethostbyname **/
							struct in_addr** a = (struct in_addr**)h->h_addr_list;
							for(int i = 0; a[i] != NULL; i++){
								strcpy(ip, inet_ntoa(*a[i]));
							}
							printf("ip: %s\n", ip);
							/** creem conexiunea cu serverul extras din comanda primita **/
							int sockserv = socket(AF_INET, SOCK_STREAM, 0);
							/** verificare apel socket **/
							if(sockserv < 0){
								strcpy(raspuns, "HTTP/1.0 500 Internal Server Error\n");
								send(i, raspuns, strlen(raspuns), 0);
								FD_CLR(i, &read_fds); // scoatem din multimea de citire socketul pe care
								close(i); 
								exit(0);
							}
							int port = 80;
							/** setare proprietati structura server **/
							struct sockaddr_in servaddr;
							memset( & servaddr, 0, sizeof(servaddr));
						    servaddr.sin_family = AF_INET;
						    servaddr.sin_port = htons(port);

						    /** eroare de la dns **/
						    if (inet_aton(ip, & servaddr.sin_addr) <= 0) {
						        printf("Adresa IP invalida.\n");
						        exit(0);
						    }

						    /*  conectare la server si verficare erori */
						    if (connect(sockserv, (struct sockaddr * ) & servaddr, sizeof(servaddr)) < 0) {
						        strcpy(raspuns, "HTTP/1.0 500 Internal Server Error\n");
								send(i, raspuns, strlen(raspuns), 0);
								FD_CLR(i, &read_fds);
								close(i); 
								exit(0);
						    }
						    /** trimitem comanda serverului **/
							send(sockserv, request, BUFLEN, 0);
							/** rezultat intors de recv **/
							int n;
							/** rezultat intors de send **/
							int send_rez;
							/** file descriptor **/
							int fd;
							/** creem noul fisier sub forma: file_nrcrt.txt **/
							char filename[50];
							strcpy(filename, "file_");
    						char aux[4];
    						sprintf(aux, "%d", N);
    						strcat(filename, aux);
    						strcat(filename, ".txt");
    						printf("fisier: %s\n", filename);
    						fd = open(filename, O_WRONLY | O_CREAT, 0777);
    						if(fd < 0){
    							strcpy(raspuns, "HTTP/1.0 500 Internal Server Error\n");
								send(i, raspuns, strlen(raspuns), 0);
								FD_CLR(i, &read_fds);
								close(i); 
								exit(0);
    						}
    						/** adaugam o noua intrare in vectorul de structuri pentru fisierul nou creat **/
    						strcpy(request_cache.name, filename);
							strcpy(request_cache.request, request);
							cached_commands[N] = request_cache;
							printf("%s %s\n", filename, request);
							N++;
							int c = 0;
							int first = 0;
							/** bucla primire mesaje **/
							while(1){
								/** primim mesaj de la server **/
								n = recv(sockserv, raspuns, BUFLEN, 0);
								/** verificam rezultatul intors de recv ca si conditie de iesire din while **/
								if(n <= 0){
									break;
								}
								/** trimitem clientului mesajul primit si verificam eventualele erori aparute **/
								send_rez = send(i, raspuns, n, 0);
								if (send_rez < 0){
									strcpy(raspuns, "HTTP/1.0 500 Internal Server Error\n");
									send(i, raspuns, strlen(raspuns), 0);
									FD_CLR(i, &read_fds);
									close(i); 
									exit(0);
								}
								/** verificam daca mesajul poate fi salvat si il scriem in fisier in caz afirmativ**/
								if (first == 0 && strstr(raspuns, "Cache-control: private") == NULL 
									&& strstr(raspuns, "Cache-control: no-cache") == NULL
									&& strstr(raspuns, "200 OK") != NULL){
									write(fd, raspuns, n);
								}else if(first == 0){
									c = 1;
								}else if(first != 0 && c == 0){
									write(fd, raspuns, n);
								}
								first = 1;
								memset(raspuns, 0, BUFLEN);
							}
							/** inchidem fisierul in care am scris **/
							if(c == 1){
								N--;
							}
							close(fd);
							/** inchidem conexiunea cu clientul curent **/
							FD_CLR(i, &read_fds);
							close(i); 
						}else if(found != -1){
							/** comanda a fost gasita i vectorul de structuri, deci ea a fost retinuta
								anterior si va fi trimisa direct din fisier **/
								char*raspuns = calloc(BUFLEN, sizeof(char));
								int n, send_rez, fd;
								/** deschidem fisierul in care se afla rezultatul comenzii **/
								fd = open(cached_commands[found].name, O_RDWR, 0777);
								while(1){
									/** citim cat timp recv nu intoarce ceva mai mic sau egal cu 0 **/
									n = read(fd, raspuns, BUFLEN);
									if(n <= 0){
										break;
									}
									/** trimitem clientului ce am citit si verificam rezultatul **/
									send_rez = send(i, raspuns, n, 0);
									if(send_rez < 0){
										strcpy(raspuns, "HTTP/1.0 500 Internal Server Error\n");
										send(i, raspuns, strlen(raspuns), 0);
										FD_CLR(i, &read_fds); // scoatem din multimea de citire socketul pe care
										close(i); 
										exit(0);
									}
									memset(raspuns, 0, BUFLEN);
								}
								/** inchidem fisierul **/
								close(fd);
								/** inchidem conexiunea **/
								FD_CLR(i, &read_fds);
								close(i); 
								break;		
						}else if(h == NULL){
							/** eroare dns la gethostbyname **/
							strcpy(raspuns, "HTTP/1.0 500 Internal Server Error\n");
							send(i, raspuns, strlen(raspuns), 0);
							FD_CLR(i, &read_fds);
							close(i); 
							exit(0);
						}
					}
				} 
			}
		}
     }
    close(sockfd);
	return 0;
}