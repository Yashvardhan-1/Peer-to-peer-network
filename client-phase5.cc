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
#include <chrono>
using namespace std;
// using namespace std::chrono;
// namespace fs = std::filesystem;
#define cout std::cout

#define TRUE 1
#define FALSE 0
#define INF 1e9
#define RUNTIME 60
#define BUFF_SIZE 1024
#define MD5_DIGEST_LENGTH 16
#define MAX_NEIGH 10

string YES = "YES";
string NO = "NO";

mutex negih_haveFile_update;

struct NeighSocket
{   
    int neighsockets, ret, id, port;
    int private_ID = INF;
    struct sockaddr_in serverAddr;
    char buffer[1024];
    vector<tuple<string,bool> > requested; 
    map<string, string> fileAndMD5; 
    vector< string > hasFile; 
    vector<NeighSocket*> search2Neighs;
};

struct Master
{
    int sock;
    int CLIENT_ID, MY_PORT, PRIVATE_ID, numNeighs;
    struct sockaddr_in my_addr;
    vector< tuple<string, string> > owned;
    map<string, string> fileAndPath;
    vector<string> search1;
    vector<string> search2;
    string PATH;
};

int sendFileFunc(int new_socket, string filePath);
int recvFileFunc(int client_socket, string filePath);

void errorHandler(int err, char *errMsg);
string md5_from_file( string path);

void server_thread_func(Master* master, vector<NeighSocket> neighstore);
void client_thread_func(Master* master, NeighSocket *neigh, int depth);
void download_file_thread_func(Master* master, NeighSocket* neigh, string file);


int main(int argc, char *argv[])
{

    srand(time(NULL)); 

    int CLIENT_ID, MY_PORT, PRIVATE_ID, tempNeighs;

    ifstream myFile(argv[1]);
    myFile >> CLIENT_ID >> MY_PORT >> PRIVATE_ID;
    struct Master master;
    master.PATH = string(argv[2]);
    string makeDirCommand = "mkdir "+ master.PATH + "Downloaded";
    system(makeDirCommand.c_str());

    myFile >> tempNeighs;
    int const numNeighs = tempNeighs;
    thread client_thread_array[numNeighs];

    int opt = TRUE;
    struct sockaddr_in address;

    /////////////////////////////////////////////////////////////////////////////////////

    // NeighSocket neighstore[numNeighs];
    vector <NeighSocket> neighstore;
    vector<string> FILES;

    for (int i = 0; i < numNeighs; i++)
    {
        int idNeigh, portNeigh;
        myFile >> idNeigh >> portNeigh;

        NeighSocket neigh;

        neigh.neighsockets = socket(AF_INET, SOCK_STREAM, 0);
        memset(&neigh.serverAddr, '\0', sizeof(neighstore[i].serverAddr));
        neigh.serverAddr.sin_family = AF_INET;
        neigh.serverAddr.sin_port = htons(portNeigh);
        neigh.serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

        neigh.id = idNeigh;
        neigh.port = portNeigh;

        neighstore.push_back(neigh);

        // cout<<idNeigh<<" "<<portNeigh<<endl;
    }

    int numFiles;
    myFile >> numFiles;
    // cout<<"### num file: "<<numFiles<<endl;

    for (int i=0; i < numFiles; i++)
    {   
        string file;
        myFile >> file;
        master.search1.push_back(file);
        // cout<<"### file to be searched: "<<file<<endl;
    }

    sort(master.search1.begin(), master.search1.end());
    // cout<<"### num files in search: "<<master.search1.size()<<endl;

    for (const auto &entry : std::filesystem::directory_iterator(argv[2]))
    {
        string file_name = std::filesystem::path(entry.path()).filename();
        string file_path = std::filesystem::path(entry.path());
        master.owned.push_back(make_tuple(file_name,file_path));
        master.fileAndPath.insert(pair<string, string>(file_name, file_path)); 
    }

    sort(master.owned.begin(), master.owned.end());


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
    errorHandler(listen(master_socket, 20), (char *)"listen failed");

    master.sock = master_socket;

    // after this no problem
    thread server_thread = thread(server_thread_func, &master, neighstore);

    for( int i= 0; i<numNeighs; i++){
        client_thread_array[i] = thread(client_thread_func, &master, &neighstore[i], 2);
    }

    for( int i = 0; i<numNeighs; i++){
        client_thread_array[i].join();
    }

    //cout<<"### main thread of "<<master.CLIENT_ID<< " all queires done "<<endl;  
    for( auto filePtr = master.search1.begin(); filePtr != master.search1.end(); ++filePtr ){
        
        int onePosseing = INF;
        bool found1 = false;
        bool found2 = false;
        string MD5 = "to be calculated";
        NeighSocket neigh;

        // cout<<"### checking for file: "<<*filePtr<<" in neighbStore\n";

        for (int i = 0; i < numNeighs; i++)
        {
            // auto itr = neighstore[i].fileAndMD5.find(string(*filePtr));

            if( find(neighstore[i].hasFile.begin(), neighstore[i].hasFile.end(), *filePtr) != neighstore[i].hasFile.end() ){
                if (neighstore[i].private_ID < onePosseing)
                {
                    onePosseing = neighstore[i].private_ID;
                    found1 = true;
                    neigh = neighstore[i];
                    // cout<<"#"<<*filePtr<<"file found in main thread\n";
                }
            }else{
                // if(neighstore[i].search2Neighs.empty()) cout<< "|||||" << neighstore[i].id <<" has 0 search2 neigh"<<endl;
                // else cout<< "||||" << neighstore[i].id <<" has "<< neighstore[i].search2Neighs.size()<<" search2 neigh"<<endl;

                for(auto neighpp = neighstore[i].search2Neighs.begin(); neighpp != neighstore[i].search2Neighs.end(); ++neighpp){
                    // cout<<"## depth 2 neigh"<<endl;
                    if( find((*neighpp)->hasFile.begin(), (*neighpp)->hasFile.end(), *filePtr) != (*neighpp)->hasFile.end() ){
                        if((*neighpp)->private_ID < onePosseing){
                            found2 = true;
                            onePosseing = (*neighpp)->private_ID;
                            neigh = *(*neighpp);
                         //   cout<<"###### file:"<< *filePtr<< " found in depth 2"<<endl;
                        }
                    }
                }
            }
        }

        string output;
        if (found1)
        {
           // cout<<"# download file thread strating for file: "<< *filePtr<<endl;
            thread download_file = thread(download_file_thread_func, &master, &neigh, *filePtr);
            download_file.join();

            string filePath = master.PATH + "Downloaded/" + *filePtr;
            MD5 = md5_from_file(filePath);
            //cout<<"# file path for MD5: "<<MD5<<endl;

            output = "Found " + *filePtr + " at " + to_string(onePosseing) + " with MD5 "+ MD5 +" at depth 1";
            cout << output << endl;
        }else if(found2){
           // cout<<"# download file2 thread strating for file: "<< *filePtr<<endl;
            thread download_file = thread(download_file_thread_func, &master, &neigh, *filePtr);
            download_file.join();

            string filePath = master.PATH + "Downloaded/" + *filePtr;
            MD5 = md5_from_file(filePath);
           // cout<<"# file path2 for MD5: "<<MD5<<endl;

            output = "Found " + *filePtr + " at " + to_string(onePosseing) + " with MD5"+ MD5 +"at depth 2";
            cout << output << endl;
        }
        // else
        // {
        //     master.search2.push_back(*filePtr);
        //     cout<<"## file "<<*filePtr<<" for depth 2 search\n";
        //     output = "## NOT Found " + *filePtr + " at 0 with MD5 0 at depth 0";
        //     cout<<output;
        // }

    }

    //cout<<"##############################################################"<<endl;

    server_thread.join();
    //cout<<"&& end of main\n";
}

void server_thread_func(Master* master, vector<NeighSocket> neighstore){

    int fdmax,newfd;
    int new_socket,sd;
    struct sockaddr_in remoteaddr;
    int addrlen = sizeof(struct sockaddr_in);
    int neighbour_socket[MAX_NEIGH];
    char buf[BUFF_SIZE];
    char buffer[BUFF_SIZE];

    for (int i = 0; i < MAX_NEIGH; i++)
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

    // while (time(NULL) < endwait)
    // std::chrono::time_point<std::chrono::system_clock> start, end;
    // start = std::chrono::system_clock::now();
    while (true)
    {
        // end = std::chrono::system_clock::now();
        // std::chrono::duration<double> elapsed_seconds = end - start;
        // if(elapsed_seconds.count() > RUNTIME - 5) break;

        FD_ZERO(&read_fds);

        FD_SET(master->sock, &read_fds);
        fdmax = master->sock;

        for (int i = 0; i < MAX_NEIGH; i++)
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

            if (send(new_socket, &master->PRIVATE_ID, sizeof(master->PRIVATE_ID), 0) != sizeof(master->PRIVATE_ID))
            {
                perror("send");
            }

            for (int i = 0; i < MAX_NEIGH; i++)
            {
                if (neighbour_socket[i] == 0)
                {
                    neighbour_socket[i] = new_socket;
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_NEIGH; i++)
        {
            sd = neighbour_socket[i];

            if (FD_ISSET(sd, &read_fds))
            {
                char BUF[BUFF_SIZE];
                int valread;

                int client_id=555; 
                char id[BUFF_SIZE];

                if ((valread = read(sd,  &id, BUFF_SIZE)) == 0)
                {
                    getpeername(sd, (struct sockaddr *)&remoteaddr, (socklen_t *)&addrlen);
                    close(sd);
                    neighbour_socket[i] = 0;
                    // cout<<"!!! Connection closed, Client Id: "<<client_id<<endl;
                }
                else
                {

                    client_id = atoi(id);
                    //cout<<"! client_id: "<<id<<endl;
                    string random = "random_msg";
                    char const* send_random = random.c_str();
                    
                    errorHandler(send(sd, &send_random, strlen(send_random),0), (char *)"! send rand1 error");
                    
                    // cout<<"! sent rand1 from server to client "<<client_id<<endl;

                    if((valread = read(sd, BUF, BUFF_SIZE)) == 0){
                        perror("buf read error ");
                    }

                    BUF[valread] = '\0';

                    errorHandler(send(sd, &send_random, strlen(send_random),0), (char *)"! send rand2 error");
                    char* recv_random;

                    //cout<<"! "<<BUF<<" printing type in server of "<<master->CLIENT_ID<<endl;
                    if(string(BUF) == "SEARCH"){

                        int depth;
                        valread = recv(sd, &depth, sizeof(depth), 0);

                        errorHandler(send(sd, &send_random, strlen(send_random),0), (char *)"! send rand1 of search error");

                        char buf[BUFF_SIZE];
                        int valread = recv(sd, buf, sizeof(buf), 0);
                        buf[valread] = '\0';

                        //cout<<"! "<<buf<<" :SEARCH1 file\n";
                        if (master->fileAndPath.find(string(buf)) != master->fileAndPath.end() && depth >= 1)
                        {
                           // cout<<"! "<<buf<<" found in server thread of "<<master->CLIENT_ID<<endl;
                            string temp_str = "DEPTH1";
                            char const *pchar = temp_str.c_str();
                            if (send(sd, pchar, strlen(pchar), 0) != strlen(pchar))
                            {
                                perror("send in ack");
                            }
                        }
                        else
                        {
                            if(depth > 1){

                                //cout<<"!! "<< buf<<" :SEARCH2 file\n";

                                Master main = *master;
                                vector<string> search_server;
                                search_server.push_back(string(buf));
                                main.search1 = search_server; 
                                main.CLIENT_ID = 99;

                                //cout<<"!! Depth 2 search by server\n";

                                thread server_client_search_thread[master->numNeighs];
                                for( int i= 0; i<master->numNeighs; i++){
                                    if(client_id == neighstore[i].id) continue;
                                    server_client_search_thread[i] = thread(client_thread_func, &main, &neighstore[i], 1);
                                }

                                for( int i = 0; i<master->numNeighs; i++){
                                    if(client_id == neighstore[i].id) continue;
                                    server_client_search_thread[i].join();
                                }

                                //cout<<"!! Depth 2 search by server completed\n";

                                int onePosseing = INF;
                                bool found = false;
                                NeighSocket neigh;

                                for (int i = 0; i < master->numNeighs; i++)
                                {
                                    if(client_id == neighstore[i].id) continue;
                                    if( find(neighstore[i].hasFile.begin(), neighstore[i].hasFile.end(), string(buf)) != neighstore[i].hasFile.end() ){
                                        if (neighstore[i].private_ID < onePosseing)
                                        {
                                            onePosseing = neighstore[i].private_ID;
                                            found = true;
                                            neigh = neighstore[i];
                                            //cout<<"!! "<<buf<<"file found in server's neighbour\n";
                                        }
                                    }
                                }

                                if(found){
                                   // cout<<"!! "<<buf<<" found in server thread of "<<master->CLIENT_ID<<endl;
                                    string temp_str = "DEPTH2";
                                    
                                    char const *pchar = temp_str.c_str();
                                    if (send(sd, pchar, strlen(pchar), 0) != strlen(pchar))
                                    {
                                        perror("send in ack");
                                    }
                                    errorHandler(read(sd, &recv_random, BUFF_SIZE), (char *)"! recieve error rand1 of server");

                                    if (send(sd, &neigh.private_ID, sizeof(neigh.private_ID), 0) != sizeof(neigh.private_ID))
                                    {
                                        perror("send");
                                    }

                                    errorHandler(read(sd, &recv_random, BUFF_SIZE), (char *)"! recieve error rand2 of server");

                                    if (send(sd, &neigh.port, sizeof(neigh.port), 0) != sizeof(neigh.port))
                                    {
                                        perror("send");
                                    }
                                }else{
                                   // cout<<"!! in no block2\n";
                                    string temp_str = "NOT FOUND";
                                    char const *pchar = temp_str.c_str();
                                    if (send(sd, pchar, strlen(pchar), 0) != strlen(pchar))
                                    {
                                        perror("send");
                                    }
                                }    
                            }else
                            {
                                // cout<<"!! in no block2\n";
                                string temp_str = "DEPTH NOT 2";
                                char const *pchar = temp_str.c_str();
                                if (send(sd, pchar, strlen(pchar), 0) != strlen(pchar))
                                {
                                    perror("send");
                                }
                            }
                        }
                    }else if (string(BUF) == "REQUEST") {

                        //cout<<"!! Server recieve "<<BUF<<endl;
                        char buf[BUFF_SIZE];
                        int valread = recv(sd, buf, sizeof(buf), 0);
                        buf[valread] = '\0';

                        //cout<<"$$$ downlaod request for "<<buf<<endl;
                        
                        auto itr = master->fileAndPath.find(string(buf));
                        if( itr != master->fileAndPath.end() ){
                        
                           // cout<<"$$$ file "<<buf<<" found... beginning transfer\n";

                            char const *filepath = itr->second.c_str();
                            FILE *filePtr = fopen(filepath, "rb");


                            fseek( filePtr , 0 , SEEK_END);
                            int file_size = ftell( filePtr );
                            rewind( filePtr );

                            char fileSendBuffer[BUFF_SIZE] ; 
                            int n;

                           // cout<<"sending file "<<buf<< " of size "<< file_size <<endl;

                            send ( sd , &file_size, sizeof(file_size), 0);

                            while ( ( n = fread( fileSendBuffer , sizeof(char) , BUFF_SIZE , filePtr ) ) > 0  && file_size > 0 )
                            {
                                    send (sd , fileSendBuffer, n, 0 );
                                    memset ( fileSendBuffer , '\0', BUFF_SIZE);
                                    file_size = file_size - n ;
                            }
                            ////////////////////////////////////////////////////
                            //cout<<"file "<<buf<<" sent\n";
                        }
                    }else{
                        string OK = "NOPE";
                        char const pchr[BUFF_SIZE] = "NOPE";

                        if (send(sd, pchr, strlen(pchr), 0) != strlen(pchr))
                        {
                            perror("send in ack");
                        }
                    }                         
                }
            }
        }
    }
    close(master->sock);
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
    int seconds = 10;

    endwait = time(NULL) + seconds;
    while ((value = connect(client_socket, (struct sockaddr *)&(neigh->serverAddr), sizeof(struct sockaddr))) == -1 && time(NULL) < endwait)
    {
        sleep(1);
    }

    int private_Int;
    recv(client_socket, &private_Int, sizeof(private_Int), 0);
    negih_haveFile_update.lock();
    neigh->private_ID = private_Int;
    negih_haveFile_update.unlock();
    //cout<<"$ "<<neigh->private_ID<<endl;

    //cout<<"$ conneted with "<<neigh->private_ID<<endl;
    if (value != -1)
    {    

        int id = master->PRIVATE_ID;
        char const* id_client = to_string(id).c_str();
        //cout<<"$ sending own id for download: "<< id_client <<endl;

        if (send(client_socket, id_client, sizeof(id_client), 0) != sizeof(id_client))
        {
            perror("send");
        }
    
        //cout<<"$ downloading file: "<<file<<" at depth "<<endl;

        char* recv_random;
        errorHandler(recv(client_socket, &recv_random, BUFF_SIZE, 0), (char *)"@ recieve error rand1 of client");

        string query = "REQUEST\0";
        char const pchr[BUFF_SIZE] = "REQUEST\0"; 
        if (send(client_socket, pchr, strlen(pchr), 0) != strlen(pchr))
        {
            perror("send");
        }

       // cout<<"$ REQUEST sent by "<<master->CLIENT_ID<<" to "<<neigh->private_ID<<endl;

        errorHandler(recv(client_socket, &recv_random, BUFF_SIZE, 0), (char *)"@ recieve error rand1 of client");
        sleep(1);

        int valread;
        char const *pchar = file.c_str();
        //cout<<"$ "<<file<<endl;
        if (send(client_socket, pchar, strlen(pchar), 0) != strlen(pchar))
        {
            perror("send");
        }
        //cout<<"$ "<<file<< " name REQUEST sent by "<<master->CLIENT_ID<<" to "<<neigh->id<<endl;

        //
        string filePath = master->PATH + "Downloaded/" + file;
        string command = "touch " + filePath;
        system(command.c_str());

        int file_size;
        recv(client_socket, &file_size, sizeof(file_size), 0);
        sleep(2);

        //cout<<"$$ filesize recieved by "<<master->CLIENT_ID<<" for file "<<file<<" " << file_size<<endl;

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
                //cout<<"$ packet "<<counter<<" recieved\n";
                counter = 0;
            }
        }
        //cout<<"$$ download of file: "<<file<<" completed"<<endl;
        fclose(fp);
    }
    close(client_socket);
}

void client_thread_func(Master* master, NeighSocket *neigh, int depth){
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
    //cout<<"& conneted with "<<neigh->id<<" for depth: "<<depth << endl;
    if (value != -1)
    {
        string ID = "ID";
        char const *pchar = ID.c_str();

      //  cout<<"& request for id sent\n";

        int private_Int;
        if((value = recv(client_socket, &private_Int, sizeof(private_Int), 0)) == 0){
            perror("PID read error while recieve");
        }
        neigh->private_ID = private_Int;

        //cout<<"& private id "<<neigh->private_ID<<endl;
        //cout<<"& current depth: "<< depth <<endl;

        if(depth > 0){
            for(auto filePtr = master->search1.begin(); filePtr != master->search1.end(); ++filePtr ){

                int id = master->CLIENT_ID;
                char const* id_client = to_string(id).c_str();
                //cout<<"@ sending own id"<<endl;
                if (send(client_socket, id_client, sizeof(id_client), 0) != sizeof(id_client))
                {
                    perror("send");
                }
            
                //cout<<"@ searching for file: "<<*filePtr<<" at depth "<<depth<<endl;

                char* recv_random;
                errorHandler(recv(client_socket, &recv_random, BUFF_SIZE, 0), (char *)"@ recieve error rand1 of client");

                string query = "SEARCH\0";
                char const pchr[BUFF_SIZE] = "SEARCH\0"; 
                if (send(client_socket, pchr, strlen(pchr), 0) != strlen(pchr))
                {
                    perror("send");
                }

                //cout<<"@ SEARCH sent"<<endl;

                errorHandler(read(client_socket, &recv_random, BUFF_SIZE), (char *)"@ recieve error rand2 of client");

                sleep(1);

                if (send(client_socket, &depth, sizeof(depth), 0) != sizeof(depth))
                {
                    perror("send");
                }
               // cout<<"@ depth sent"<<endl;

                sleep(1);
                errorHandler(read(client_socket, &recv_random, BUFF_SIZE), (char *)"@ recieve error rand3 of client");

                int valread;
                pchar = filePtr->c_str();
                //cout<<"@ "<<*filePtr<<endl;
                if (send(client_socket, pchar, strlen(pchar), 0) != strlen(pchar))
                {
                    perror("send");
                }

                //cout<<"@ file search sent"<<endl;
                char buffer[BUFF_SIZE];

                if ((valread = read(client_socket, buffer, BUFF_SIZE)) == 0)
                {
                    perror("YES/NO error while recieve");
                }
                else
                {
                    buffer[valread] = '\0';
                   // cout<<"@@@@@ "<<buffer<<endl;
                    if (string(buffer) == "DEPTH1")
                    {
                        sleep(1);
                        if (find(neigh->hasFile.begin(), neigh->hasFile.end(), string(*filePtr)) == neigh->hasFile.end()){
                            
                            negih_haveFile_update.lock();
                            neigh->hasFile.push_back(string(*filePtr));  
                            negih_haveFile_update.unlock();
                        }
                    }else if(string(buffer) == "DEPTH2"){
                        int pid, port;
                        
                        string random = "random_msg";

                    //    cout<<"@@ file "<<*filePtr<<"found at depth"<<endl;

                        char const* send_random = random.c_str();
                        errorHandler(send(client_socket, &send_random, strlen(send_random),0), (char *)"@ send rand1 client error");

                        valread = read(client_socket, &pid, sizeof(pid));
                        
                        errorHandler(send(client_socket, &send_random, strlen(send_random),0), (char *)"@ send rand2 client error");

                        valread = read(client_socket, &port, sizeof(port));


                      //  cout<<"@@ pid and port depth 2 for file "<<*filePtr<<": "<<pid<<" and "<<port<<endl;    
                        NeighSocket* neigh2 = new NeighSocket;

                        neigh2->neighsockets = socket(AF_INET, SOCK_STREAM, 0);
                        memset(&neigh2->serverAddr, '\0', sizeof(neigh2->serverAddr));
                        neigh2->serverAddr.sin_family = AF_INET;
                        neigh2->serverAddr.sin_port = htons(port);
                        neigh2->serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

                        neigh2->private_ID = pid;
                        neigh2->port = port;

                        neigh2->hasFile.push_back(*filePtr);

                        negih_haveFile_update.lock();
                        neigh->search2Neighs.push_back(neigh2);
                        negih_haveFile_update.unlock();
                        
                    }
                }
            }
        }
    }

    close(client_socket);
    //cout<<"@ closed socket for "<<master->CLIENT_ID<<" connecting with "<<neigh->id<<endl;
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
