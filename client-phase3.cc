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
#include <time.h>
using namespace std;
// namespace fs = std::filesystem;

#define TRUE 1
#define FALSE 0
#define INF 1e9
#define RUNTIME 20
#define BUFF_SIZE 1024
#define MD5_DIGEST_LENGTH 16

string YES = "YES";
string NO = "NO";

struct NeighSocket
{   
    int neighsockets, ret, id, port;
    int private_ID = INF;
    struct sockaddr_in serverAddr;
    char buffer[1024];
    vector<tuple<string,bool> > requested; 
    map<string, string> fileAndMD5; 
    vector< string > hasFile; 
};

struct Master
{
    int sock;
    int CLIENT_ID, MY_PORT, PRIVATE_ID, numNeighs;
    struct sockaddr_in my_addr;
    vector< tuple<string, string> > owned;
    map<string, string> fileAndPath;
    vector<string> search;
    string PATH;
};

int sendFileFunc(int new_socket, string filePath);
int recvFileFunc(int client_socket, string filePath);

void errorHandler(int err, char *errMsg);
string md5_from_file( string path);

void server_thread_func(Master* master);
void client_thread_func(Master* master, NeighSocket *neigh);
void download_file_thread_func(Master* master, NeighSocket* neigh, string file);


int main(int argc, char *argv[])
{

    srand(time(NULL)); 

    int CLIENT_ID, MY_PORT, PRIVATE_ID, numNeighs;

    ifstream myFile(argv[1]);
    myFile >> CLIENT_ID >> MY_PORT >> PRIVATE_ID;
    struct Master master;
    master.PATH = string(argv[2]);
    string makeDirCommand = "mkdir "+ master.PATH + "Downloaded";
    system(makeDirCommand.c_str());

    myFile >> numNeighs;
    thread client_thread_array[numNeighs];

    int opt = TRUE;
    struct sockaddr_in address;

    /////////////////////////////////////////////////////////////////////////////////////

    NeighSocket neighstore[numNeighs];
    vector<string> FILES;

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

        // cout<<idNeigh<<" "<<portNeigh<<endl;
    }

    int numFiles;
    myFile >> numFiles;
    for (int i; i < numFiles; i++)
    {
        string file;
        myFile >> file;
        master.search.push_back(file);
        // cout<<file<<endl;
    }

    for (const auto &entry : std::filesystem::directory_iterator(argv[2]))
    {
        string file_name = std::filesystem::path(entry.path()).filename();
        string file_path = std::filesystem::path(entry.path());
        master.owned.push_back(make_tuple(file_name,file_path));
        master.fileAndPath.insert(pair<string, string>(file_name, file_path)); 
    }

    master.CLIENT_ID = CLIENT_ID;
    master.MY_PORT = MY_PORT;
    master.PRIVATE_ID = PRIVATE_ID;
    master.PATH = argv[2];
    master.numNeighs = numNeighs;

    int master_socket;

    if ((master_socket = socket(PF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    errorHandler(setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)), (char *)"setsockopt");

    memset(&address, '\0', sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(master.MY_PORT);
    address.sin_addr.s_addr = inet_addr("127.0.0.1");

    errorHandler(bind(master_socket, (struct sockaddr *)&address, sizeof(address)), (char *)"bind failed");
    errorHandler(listen(master_socket, 2 * master.numNeighs), (char *)"listen failed");

    master.sock = master_socket;


    // after this no problem
    thread server_thread = thread(server_thread_func, &master);

    for( int i= 0; i<numNeighs; i++){
        client_thread_array[i] = thread(client_thread_func, &master, &neighstore[i]);
    }

    for( int i = 0; i<numNeighs; i++){
        client_thread_array[i].join();
    }

    //cout<<"main thread of "<<master.CLIENT_ID<< " all queires done "<<endl;  
    for( auto filePtr = master.search.begin(); filePtr != master.search.end(); ++filePtr ){
        
        int onePosseing = INF;
        bool found = false;
        string MD5 = "to be calculated";
        NeighSocket neigh;

        //cout<<"################ checking for file: "<<*filePtr<<" in neighbStore\n";

        for (int i = 0; i < numNeighs; i++)
        {
            // auto itr = neighstore[i].fileAndMD5.find(string(*filePtr));
            if( find(neighstore[i].hasFile.begin(), neighstore[i].hasFile.end(), *filePtr) != neighstore[i].hasFile.end() ){
                if (neighstore[i].private_ID < onePosseing)
                {
                    onePosseing = neighstore[i].private_ID;
                    found = true;
                    neigh = neighstore[i];
                    //cout<<*filePtr<<"file found in main thread\n";
                }
            }
        }

        string output;
        if (found)
        {
            //cout<<"download file thread strating for file: "<< *filePtr<<endl;
            thread download_file = thread(download_file_thread_func, &master, &neigh, *filePtr);
            download_file.join();

            string filePath = master.PATH + "Downloaded/" + *filePtr;
            MD5 = md5_from_file(filePath);
            //cout<<"file path for MD5: "<<MD5<<endl;

            output = "Found " + *filePtr + " at " + to_string(onePosseing) + " with MD5 " + MD5 +" at depth 1";
        }
        else
        {
            output = "Found " + *filePtr + " at 0 with MD5 0 at depth 0";
        }
        cout << output << endl;

    }
    server_thread.join();
}

void server_thread_func(Master* master){

    int fdmax,newfd;
    int new_socket,sd;
    struct sockaddr_in remoteaddr;
    int addrlen = sizeof(struct sockaddr_in);
    int neighbour_socket[master->numNeighs];
    char buf[BUFF_SIZE];
    char buffer[BUFF_SIZE];

    for (int i = 0; i < master->numNeighs; i++)
    {
        neighbour_socket[i] = 0;
    }

    fd_set main;
    fd_set read_fds; 

    FD_ZERO(&main);
    // FD_ZERO(&read_fds);

    time_t endwait;
    int seconds = RUNTIME;
    endwait = time(NULL) + seconds;


    while (time(NULL) < endwait)
    {
        FD_ZERO(&read_fds);

        FD_SET(master->sock, &read_fds);
        fdmax = master->sock;

        for (int i = 0; i < master->numNeighs; i++)
        {
            sd = neighbour_socket[i];

            if (sd > 0)
                FD_SET(sd, &read_fds);
            if (sd > fdmax)
                fdmax = sd;
        }
        
        struct timeval timeout;
        timeout.tv_sec = RUNTIME/10;
        timeout.tv_usec = 0;

        int activity = select(fdmax + 1, &read_fds, NULL, NULL, &timeout);

        if ((activity < 0) && (errno != EINTR))
        {
            printf("select error");
        }
        if (FD_ISSET(master->sock, &read_fds))
        {
            if ((new_socket = accept(master->sock, (struct sockaddr *)&remoteaddr, (socklen_t *)&addrlen)) < 0)
            {
                string out = "accept " + to_string(master->sock) + " "+to_string(addrlen);
                perror(out.c_str());
                exit(EXIT_FAILURE);
            }

            //cout<<"w2\n";

            if (send(new_socket, &master->PRIVATE_ID, sizeof(master->PRIVATE_ID), 0) != sizeof(master->PRIVATE_ID))
            {
                perror("send");
            }

            for (int i = 0; i < master->numNeighs; i++)
            {
                if (neighbour_socket[i] == 0)
                {
                    neighbour_socket[i] = new_socket;
                    break;
                }
            }
        }

        for (int i = 0; i < master->numNeighs; i++)
        {
            sd = neighbour_socket[i];

            if (FD_ISSET(sd, &read_fds))
            {
                char BUF[BUFF_SIZE];
                int valread;
                if ((valread = read(sd, BUF, BUFF_SIZE)) == 0)
                {
                    getpeername(sd, (struct sockaddr *)&remoteaddr, (socklen_t *)&addrlen);
                    close(sd);
                    neighbour_socket[i] = 0;
                }
                else
                {
                    //cout<<BUF<<" printing buffer"<<endl;
                    if(string(BUF) == "QUERY"){

                        char buf[BUFF_SIZE];
                        int valread = recv(sd, buf, sizeof(buf), 0);
                        buf[valread] = '\0';

                        //cout<<buf<<" :query file\n";
                        if (master->fileAndPath.find(string(buf)) != master->fileAndPath.end())
                        {
                            //cout<<buf<<" found in server thread of "<<master->CLIENT_ID<<endl;
                            string temp_str = "YES";
                            char const *pchar = temp_str.c_str();
                            if (send(sd, pchar, strlen(pchar), 0) != strlen(pchar))
                            {
                                //perror("send in ack");
                            }
                        }
                        else
                        {
                           // cout<<"in no block\n";
                            string temp_str = "NO";
                            char const *pchar = temp_str.c_str();
                            if (send(sd, pchar, strlen(pchar), 0) != strlen(pchar))
                            {
                                //perror("send");
                            }
                        }

                    }else if (string(BUF) == "REQUEST") {
                        char buf[BUFF_SIZE];
                        int valread = recv(sd, buf, sizeof(buf), 0);
                        buf[valread] = '\0';

                        //cout<<"downlaod request for "<<buf<<endl;
                        
                        auto itr = master->fileAndPath.find(string(buf));
                        if( itr != master->fileAndPath.end() ){
                        
                           // cout<<"file "<<buf<<" found... beginning transfer\n";

                            char const *filepath = itr->second.c_str();
                            FILE *filePtr = fopen(filepath, "r"); 

                            fseek( filePtr , 0 , SEEK_END);
                            int file_size = ftell( filePtr );
                            rewind( filePtr );

                            char fileSendBuffer[BUFF_SIZE] ; 
                            int n;

                            //cout<<"sending file "<<buf<< " of size "<< file_size <<endl;

                            send ( sd , &file_size, sizeof(file_size), 0);

                            while ( ( n = fread( fileSendBuffer , sizeof(char) , BUFF_SIZE , filePtr ) ) > 0  && file_size > 0 )
                            {
                                    send (sd , fileSendBuffer, n, 0 );
                                    memset ( fileSendBuffer , '\0', BUFF_SIZE);
                                    file_size = file_size - n ;
                            }
                            //cout<<"sent\n";
                        }
                    }                     
                    
                }
            }
        }
    }
    close(master->sock);
}

void client_thread_func(Master* master, NeighSocket *neigh){
    int client_socket;
    if ((client_socket = socket(PF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int value;
    time_t endwait;
    int seconds = 20;

    endwait = time(NULL) + seconds;
    while ((value = connect(client_socket, (struct sockaddr *)&(neigh->serverAddr), sizeof(struct sockaddr))) == -1 && time(NULL) < endwait)
    {
        sleep(1);
    }
    //cout<<"conneted with "<<neigh->id<<endl;
    if (value != -1)
    {
        string ID = "ID";
        char const *pchar = ID.c_str();

        // if (send(client_socket, pchar, strlen(pchar), 0) != strlen(pchar))
        // {
        //     perror("send");
        // }

        //cout<<"request for id sent\n";

        int private_Int;
        recv(client_socket, &private_Int, sizeof(private_Int), 0);
        neigh->private_ID = private_Int;

        //cout<<"private id "<<neigh->private_ID<<endl;

        for(auto filePtr = master->search.begin(); filePtr != master->search.end(); ++filePtr ){
        
            string query = "QUERY";
            pchar = query.c_str(); 
            if (send(client_socket, pchar, strlen(pchar), 0) != strlen(pchar))
            {
                perror("send");
            }

            sleep(1);

            int valread;
            pchar = filePtr->c_str();
            //cout<<*filePtr<<endl;
            if (send(client_socket, pchar, strlen(pchar), 0) != strlen(pchar))
            {
                perror("send");
            }

            //cout<<"file search sent"<<endl;
            char buffer[BUFF_SIZE];
            if ((valread = read(client_socket, buffer, BUFF_SIZE)) == 0)
            {
                //perror("YES/NO error while recieve");
            }
            else
            {
                // buffer[valread] = '\0';
               // cout<<buffer<<endl;
                if (string(buffer) == "YES")
                {
                    // cout<<"ack by "<<neigh->id<<"for file "<< *filePtr<<endl;
                    sleep(1);

                    if (find(neigh->hasFile.begin(), neigh->hasFile.end(), string(*filePtr)) == neigh->hasFile.end()){
                        neigh->hasFile.push_back(string(*filePtr));
                        //cout<<neigh->id<< " has file "<<*filePtr<<endl;    
                    }
                }
            }
        }
    }

    close(client_socket);
    //cout<<"cloased socket for "<<master->CLIENT_ID<<" connecting with "<<neigh->id<<endl;
}

void download_file_thread_func(Master* master, NeighSocket* neigh, string file){

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

    int private_Int;
    recv(client_socket, &private_Int, sizeof(private_Int), 0);
    neigh->private_ID = private_Int;
   // cout<<neigh->private_ID<<endl;

    //cout<<"conneted with "<<neigh->id<<endl;
    if (value != -1)
    {    
        string query = "REQUEST";
        char const *pchar = query.c_str(); 
        if (send(client_socket, pchar, strlen(pchar), 0) != strlen(pchar))
        {
            perror("send");
        }

        //cout<<"REQUEST sent by "<<master->CLIENT_ID<<" to "<<neigh->id<<endl;

        sleep(1);

        int valread;
        pchar = file.c_str();
        //cout<<file<<endl;
        if (send(client_socket, pchar, strlen(pchar), 0) != strlen(pchar))
        {
            perror("send");
        }

        //cout<<file<< " name REQUEST sent by "<<master->CLIENT_ID<<" to "<<neigh->id<<endl;

        string filePath = master->PATH + "Downloaded/" + file;
        string command = "touch " + filePath;
        system(command.c_str());
        
        int file_size;
        recv(client_socket, &file_size, sizeof(file_size), 0);
        sleep(2);

        //cout<<"filesize recieved by "<<master->CLIENT_ID<<" for file "<<file<<" " << file_size<<endl;

        int n;
        char recvFileBuf[BUFF_SIZE];
        FILE *fp=fopen (filePath.c_str(), "w"); 

        int counter = 0;
        while ( file_size > 0 )
        { 
            if( (n = recv(client_socket, recvFileBuf , BUFF_SIZE, 0) ) <= 0) break;      
            fwrite(recvFileBuf , sizeof (char), n, fp);
            memset(recvFileBuf , '\0', BUFF_SIZE);
            file_size = file_size - n;
            counter++;
            if(counter/1000){
                //cout<<"packet "<<counter<<" recieved\n";
                counter = 0;
            }
        }
    }
    close(client_socket);
}


void errorHandler(int err, char *errMsg)
{
    if (err < 0)
    {
        perror(errMsg);
        exit(EXIT_FAILURE);
    }
}

string md5_from_file( string path){

    string command = "md5sum "+ path;
    char buffer[128];
    string result = "";

    // Open pipe to file
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return "popen failed!";
    }

    // read till end of process:
    while (!feof(pipe)) {

        // use buffer to read and add to result
        if (fgets(buffer, 128, pipe) != NULL)
            result += buffer;
    }

    pclose(pipe);
    
    vector <string> tokens;
     
    // stringstream class check1
    stringstream check1(result);
     
    string intermediate;
     
    // Tokenizing w.r.t. space ' '
    while(getline(check1, intermediate, ' '))
    {
        tokens.push_back(intermediate);
    }

    return tokens[0];
}

int sendFileFunc(int new_socket, string filePath){

    FILE *filePtr = fopen(filePath.c_str(), "rb"); 
    if(filePtr == nullptr) cout<<"not able to open file\n";

    fseek( filePtr , 0 , SEEK_END);
    int size = ftell( filePtr );
    rewind( filePtr );
    int file_size;

    char fileSendBuffer[BUFF_SIZE] ; 
    int n;

    send ( new_socket , &size, sizeof(file_size), 0);

    int counter;
    while ( ( n = fread( fileSendBuffer , sizeof(char) , BUFF_SIZE , filePtr ) ) > 0  && size > 0 )
    {
        // sleep(2+rand()%10);
        send (new_socket , fileSendBuffer, n, 0 );
        memset ( fileSendBuffer , '\0', BUFF_SIZE);
        size = size - n ;
        counter++;
        if(counter/1000){
            //cout<<"packet "<<counter<<" sent\n"<<"size of file remaining is"<<size<<endl;
            counter = 0;
        }
    }

    //cout<<filePath<<" sent\n";

    return 1;
}

int recvFileFunc(int client_socket, string filePath){
    
    string command = "touch " + filePath;
    system(command.c_str());
    
    int file_size;
    recv(client_socket, &file_size, sizeof(file_size), 0);
    sleep(2);

    int n;
    char recvFileBuf[BUFF_SIZE];
    FILE *fp=fopen (filePath.c_str(), "wb"); 

    int counter = 0;
    while ( file_size > 0 )
    { 
        if( (n = recv(client_socket, recvFileBuf , BUFF_SIZE, 0) ) <= 0) break;      
        fwrite(recvFileBuf , sizeof (char), n, fp);
        memset(recvFileBuf , '\0', BUFF_SIZE);
        file_size = file_size - n;
        counter++;
        if(counter/1000){
            //cout<<"packet "<<counter<<" recieved\n";
            counter = 0;
        }
    }

    //cout<<"file recieved\n";
    return 1;

}