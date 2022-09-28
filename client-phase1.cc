#include <bits/stdc++.h>
#include <stdlib.h>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <errno.h>
#include <unistd.h>    //close
#include <arpa/inet.h> //close
#include <netinet/in.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <netdb.h>
#include <netdb.h>

#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <filesystem>//.#include <experimental/filesystem>
#include <vector>
#include <thread>
using namespace std;
namespace fs = std::filesystem;

#define TRUE 1
#define FALSE 0

struct NeighSocket
{
    int neighsockets, ret, id, private_ID, port;
    struct sockaddr_in serverAddr;
    char buffer[1024];
};

struct Master
{
    int master_socket;
    int CLIENT_ID, MY_PORT, PRIVATE_ID, numNeighs;
    struct sockaddr_in my_addr;
    vector<string> owned;
    vector<string> search;
};

void errorHandler(int err, char *errMsg);

void client_thread_func(struct Master master, NeighSocket neighbour)
{
    sleep(10);
    int client_socket;
    if ((client_socket = socket(PF_INET, SOCK_STREAM, 0)) == 0)
    {
        //perror("socket failed");
        exit(EXIT_FAILURE);
    }
    while(connect(client_socket, (struct sockaddr *)&neighbour.serverAddr, sizeof(struct sockaddr))==-1){
        sleep(1);
    }
    // cout<<"connected"<<endl;
    // string ans = "Connected to "+ to_string(master.CLIENT_ID) +" with unique-ID "+ to_string(master.PRIVATE_ID)+ " on port " + to_string(master.MY_PORT);

    int valread;
    char buffer[4096];  //data buffer of 1K 
    if ((valread = read( client_socket , buffer, 4096)) == 0)  
    {   
        close( client_socket );    
    }   
    else 
    {  
        buffer[valread] = '\0';  
        // printf("%s\n", buffer);
        // cout<<buffer<<endl;
    }

    string ans = "Connected to "+ to_string(neighbour.id) +" with unique-ID "+ buffer + " on port " + to_string(neighbour.port);
    cout<<ans<<endl;
    close(client_socket);
}

int main(int argc, char *argv[])
{

    int CLIENT_ID, MY_PORT, PRIVATE_ID;

    ifstream myFile(argv[1]);
    myFile >> CLIENT_ID >> MY_PORT >> PRIVATE_ID;
    struct Master master;
    master.CLIENT_ID = CLIENT_ID;
    master.MY_PORT = MY_PORT;
    master.PRIVATE_ID = PRIVATE_ID;

    int numNeighs;
    myFile >> numNeighs;

    /////////////////////////////////////////////////////////////////////////////////////

    int sockfd;
    struct sockaddr_in my_addr;

    int opt = TRUE;
    int master_socket, addrlen, new_socket, neighbour_socket[numNeighs], activity, i, valread, sd;
    int max_sd;
    int numFiles;
    thread client_thread_array[numNeighs];
    struct sockaddr_in address;

    char buffer[4096];  //data buffer of 1K 
    fd_set readfds;

    NeighSocket neighstore[numNeighs];
    vector<string> FILES;

    for (i = 0; i < numNeighs; i++)
    {
        neighbour_socket[i] = 0;
    }


    if ((master_socket = socket(PF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // set master socket to allow multiple connections ,
    errorHandler(setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)), (char*)"setsockopt");

    memset(&address, '\0', sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(MY_PORT);
    address.sin_addr.s_addr = inet_addr("127.0.0.1");

    errorHandler(bind(master_socket, (struct sockaddr *)&address, sizeof(address)), (char*)"bind failed");
    errorHandler(listen(master_socket, 2 * numNeighs), (char*)"listen failed");

    for (int i = 0; i < numNeighs; i++)
    {
        int idNeigh, portNeigh;
        myFile >> idNeigh >> portNeigh;

        neighstore[i].neighsockets = socket(AF_INET, SOCK_STREAM, 0);
        memset(&neighstore[i].serverAddr, '\0', sizeof(neighstore[i].serverAddr));
        neighstore[i].serverAddr.sin_family = AF_INET;
        neighstore[i].serverAddr.sin_port = htons(portNeigh);
        neighstore[i].serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

        neighstore[i].id = idNeigh;
        neighstore[i].port = portNeigh;

        client_thread_array[i] = thread(client_thread_func, master, neighstore[i]);
    }

    myFile >> numFiles;
    for( int i; i< numFiles; i++){
        string file;
        myFile >> file;
        master.search.push_back(file);
        // cout<<file<<endl;
    }

    for (const auto &entry : fs::directory_iterator(argv[2]))
    {
        string file = (entry.path()).filename();
        master.owned.push_back(file);
        cout<<file<<endl;
    }

    while (TRUE)
    {
        FD_ZERO(&readfds);

        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        // add child sockets to set
        for (i = 0; i < numNeighs; i++)
        {
            // socket descriptor
            sd = neighbour_socket[i];

            // if valid socket descriptor then add to read list
            if (sd > 0)
                FD_SET(sd, &readfds);

            // highest file descriptor number, need it for the select function
            if (sd > max_sd)
                max_sd = sd;
        }

        // wait for an activity on one of the sockets , timeout is NULL ,
        // so wait indefinitely
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR))
        {
            printf("select error");
        }

        // If something happened on the master socket ,
        // then its an incoming connection
        if (FD_ISSET(master_socket, &readfds))
        {
            if ((new_socket = accept(master_socket, 
                    (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)  
            {  
                perror("accept");  
                exit(EXIT_FAILURE);  
            }  
             
            //inform user of socket number - used in send and receive commands 
            // printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , new_socket , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));  
           
            //send new connection greeting message 
            stringstream ss;
            ss << PRIVATE_ID;
            string temp_str = ss.str();
            char const* pchar = temp_str.c_str();
            if( send(new_socket, pchar, strlen(pchar), 0) != strlen(pchar) )  
            {  
                //perror("send");  
            }  
                 
            // puts("Welcome message sent successfully");  
                 
            //add new socket to array of sockets 
            for (i = 0; i < numNeighs; i++)  
            {  
                //if position is empty 
                if( neighbour_socket[i] == 0 )  
                {  
                    neighbour_socket[i] = new_socket;  
                    // printf("Adding to list of sockets as %d\n" , i);  
                         
                    break;  
                }  
            }  
        }

        for (i = 0; i < numNeighs; i++)  
        {  
            sd = neighbour_socket[i];  
                 
            if (FD_ISSET( sd , &readfds))  
            {  
                //Check if it was for closing , and also read the 
                //incoming message 
                void* buf;
                if ((valread = read( sd , buffer, 4096)) == 0)  
                {  
                    //Somebody disconnected , get his details and print 
                    getpeername(sd , (struct sockaddr*)&address , \
                        (socklen_t*)&addrlen);  
                    // printf("Host disconnected , ip %s , port %d \n", inet_ntoa(address.sin_addr) , ntohs(address.sin_port));  
                         
                    //Close the socket and mark as 0 in list for reuse 
                    close( sd );  
                    neighbour_socket[i] = 0;  
                }  
                     
                //Echo back the message that came in 
                else 
                {  
                    buffer[valread] = '\0';  
                    // printf("%s\n", buffer);
                    //cout<<buffer<<endl;
                }  
            } 
        }
    }

    for (int i = 0; i < numNeighs; i++)
    {
        client_thread_array[i].join();
    }

    close(master_socket);
}


void errorHandler(int err, char *errMsg)
{
    if (err < 0)
    {
        perror(errMsg);
        exit(EXIT_FAILURE);
    }
}

