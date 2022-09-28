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
#include <filesystem>
#include <vector>
#include <thread>
using namespace std;
namespace fs = std::filesystem;

#define TRUE 1
#define FALSE 0
#define INF 1e9
#define RUNTIME 15

string YES = "YES";
string NO = "NO";

struct NeighSocket
{
    int neighsockets, ret, id, port;
    int private_ID = INF;
    struct sockaddr_in serverAddr;
    char buffer[1024];
    vector< tuple<string,bool> > requested;
    vector< string > hasFile; 
};

struct Master
{
    int master_socket;
    int CLIENT_ID, MY_PORT, PRIVATE_ID, numNeighs;
    struct sockaddr_in my_addr;
    vector<string> owned;
    vector<string> search;
};


////////////////////////////////////////////////////////////////
int RecvBuffer(int sock, char* buffer, int bufferSize, int chunkSize) { 
    int i = 0;
    while (i < bufferSize) {
        const int l = recv(sock, &buffer[i], min(chunkSize, bufferSize - i), 0);
        if (l < 0) { return l; } 
        i += l;
    }
    return i;
}

int SendBuffer(int sock, char* buffer, int bufferSize, int chunkSize) { 
    int i = 0;
    while (i < bufferSize) {
        const int l = send(sock, &buffer[i], min(chunkSize, bufferSize - i), 0);
        if (l < 0) { return l; } 
        i += l;
    }
    return i;
}



int RecvFile(int sock, char *filePath, int chunkSize, int fileSize) { 

    char *lastSlash = strrchr(filePath, '\\');
    char *filename = lastSlash ? lastSlash +1 : filePath;
    FILE *fp = fopen(filename, "wb");

    char buffer[chunkSize];
    int i = fileSize;

    while (i != 0) {
        const int r = RecvBuffer(sock, buffer, (int)min(i, (int)chunkSize), chunkSize);
        if ((r < 0) || !fwrite(buffer, sizeof(char), r, fp)) { break; }
        i -= r;
    }
    bzero(buffer, sizeof(buffer));

    fclose(fp);

    cout<<"download complete"<<endl;

    return -3;
}

int SendFile(int sock, char *fileName, int chunkSize, int fileSize) { 

    FILE *fp = fopen(fileName, "rb");

    char buffer[chunkSize];
    int i = fileSize;

    while (i != 0) {
        const int ssize = min(i, (int)chunkSize);
        if (!fread(buffer, sizeof(char), ssize, fp)) { break; }
        const int l = SendBuffer(sock, buffer, (int)ssize, (int)chunkSize);
        if (l < 0) { break; }
        i -= l;
    }
    bzero(buffer, sizeof(buffer));

    fclose(fp);

    cout<<"upload succesful"<<endl;

    return -3;
}
///////////////////////////////////



void errorHandler(int err, char *errMsg);

void client_thread_func(Master* master, NeighSocket *neigh)
{
    sleep(10);
    int client_socket;
    if ((client_socket = socket(PF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int value;
    time_t endwait;
    int seconds = RUNTIME - 3;

    endwait = time(NULL) + seconds;

    while ((value = connect(client_socket, (struct sockaddr *)&(neigh->serverAddr), sizeof(struct sockaddr))) == -1 && time(NULL) < endwait)
    {
        sleep(1);
    }
    //cout<<"conneted with "<<neigh->id<<endl;
    if (value != -1)
    {
        int valread;
        char private_id[4096]; // data buffer of 1K

        if ((valread = read(client_socket, private_id, 4096)) == 0) close(client_socket);
        else private_id[valread] = '\0';

        neigh->private_ID = stoi(private_id);

        for(auto filePtr = master->search.begin(); filePtr != master->search.end(); ++filePtr ){
            
            char const *pchar = filePtr->c_str();
            //cout<<*filePtr<<endl;
            if (send(client_socket, pchar, strlen(pchar), 0) != strlen(pchar))
            {
                perror("send");
            }

           // cout<<"file request sent"<<endl;
            char buffer[1024];
            if ((valread = read(client_socket, buffer, 1024)) == 0)
            {
                perror("YES/NO error while recieve");
            }
            else
            {
                // buffer[valread] = '\0';
                if (string(buffer) == "YES")
                {
                    if (find(neigh->hasFile.begin(), neigh->hasFile.end(), string(*filePtr)) == neigh->hasFile.end()){
                        neigh->hasFile.push_back(string(*filePtr));
                        //cout<<neigh->id<< " has file "<<*filePtr<<endl;    
                    }
                }
            }
        }
    }

    close(client_socket);
}

int main(int argc, char *argv[])
{

    int CLIENT_ID, MY_PORT, PRIVATE_ID;

    ifstream myFile(argv[1]); //what does this line do here ?
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

    char buffer[1024]; 
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

    errorHandler(setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)), (char *)"setsockopt");

    memset(&address, '\0', sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(MY_PORT);
    address.sin_addr.s_addr = inet_addr("127.0.0.1");

    errorHandler(bind(master_socket, (struct sockaddr *)&address, sizeof(address)), (char *)"bind failed");
    errorHandler(listen(master_socket, 2 * numNeighs), (char *)"listen failed");

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
    }

    myFile >> numFiles;//waht does this line means 
    for (int i; i < numFiles; i++)
    {
        string file;
        myFile >> file;
        master.search.push_back(file);
    }

    for (const auto &entry : fs::directory_iterator(argv[2]))
    {
        string file = fs::path(entry.path()).filename();
        master.owned.push_back(file);
        //cout << file << endl;
    }

    for( int i= 0; i<numNeighs; i++){
        client_thread_array[i] = thread(client_thread_func, &master, &neighstore[i]);
    }

    time_t endwait;
    int seconds = RUNTIME;

    endwait = time(NULL) + seconds;

    while (time(NULL) < endwait)
    {
        FD_ZERO(&readfds);

        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        for (i = 0; i < numNeighs; i++)
        {
            sd = neighbour_socket[i];

            if (sd > 0)
                FD_SET(sd, &readfds);
            if (sd > max_sd)
                max_sd = sd;
        }
        
        struct timeval timeout;
        timeout.tv_sec = RUNTIME/10;
        timeout.tv_usec = 0;

        activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);

        if ((activity < 0) && (errno != EINTR))
        {
            printf("select error");
        }
        if (FD_ISSET(master_socket, &readfds))
        {
            if ((new_socket = accept(master_socket,
                                     (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            stringstream ss;
            ss << PRIVATE_ID;
            string temp_str = ss.str();
            char const *pchar = temp_str.c_str();
            if (send(new_socket, pchar, strlen(pchar), 0) != strlen(pchar))
            {
                perror("send");
            }

            for (i = 0; i < numNeighs; i++)
            {
                if (neighbour_socket[i] == 0)
                {
                    neighbour_socket[i] = new_socket;
                    break;
                }
            }
        }

        for (i = 0; i < numNeighs; i++)
        {
            sd = neighbour_socket[i];

            if (FD_ISSET(sd, &readfds))
            {
                char BUF[1024];
                if ((valread = read(sd, BUF, 1024)) == 0)
                {
                    getpeername(sd, (struct sockaddr *)&address,
                                (socklen_t *)&addrlen);
                    close(sd);
                    neighbour_socket[i] = 0;
                }
                else
                {
                    BUF[valread] = '\0';
                    // cout<<"BUF\n";
                    if (find(master.owned.begin(), master.owned.end(), string(BUF)) != master.owned.end())
                    {
                        // cout<<buffer<<endl;
                        string temp_str = "YES";
                        char const *pchar = temp_str.c_str();
                        if (send(new_socket, pchar, strlen(pchar), 0) != strlen(pchar))
                        {
                            perror("send");
                        }
                    }
                    else
                    {
                        string temp_str = "NO";
                        char const *pchar = temp_str.c_str();
                        if (send(new_socket, pchar, strlen(pchar), 0) != strlen(pchar))
                        {
                            perror("send");
                        }
                    }
                }
            }
        }
    }

    for( int i = 0; i<numNeighs; i++){
        client_thread_array[i].join();
    }

    for( auto filePtr = master.search.begin(); filePtr != master.search.end(); ++filePtr ){
        
        int onePosseing = INF;
        bool found = false;
        
        for (int i = 0; i < numNeighs; i++)
        {
            vector<string> have = neighstore[i].hasFile;
            // for(auto itr = have.begin(); itr != have.end(); ++i ){
            //     cout<<*itr<<" wot\n";
            // }
            // cout<<endl;

            if (find(have.begin(), have.end(), *filePtr) != have.end())
            {
                if (neighstore[i].private_ID < onePosseing)
                {
                    onePosseing = neighstore[i].private_ID;
                    found = true;
                }
            }
        }
        string output;
        if (found)
        {    char bufferpath[PATH_MAX];
                if (getcwd(bufferpath, sizeof(bufferpath)) != NULL) {
                    //cout << bufferpath<<endl;
                } else {
                    perror("getcwd() error");
                    return 1;
                }
            //strcat(bufferpath,"/files/client");
            //cout<<buffer << endl;//just have to define the file path and use the above funcitons thats it 

            output = "Found " + *filePtr + " at " + to_string(onePosseing) + " with MD5"+ "0" +"at depth 1";//send file from here itself to
            //this code acts as a client and send it to the server and then in server implement the recieve thing
        }
        else
        {
            output = "Found " + *filePtr + " at 0 with MD5 0 at depth 0";
        }
        cout << output << endl;

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

