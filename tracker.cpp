#include <bits/stdc++.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <pthread.h>
#include "socket.cpp" 

using namespace std;

class trackerdata
{
    public:
        string shash,csocket,cfpath;
        trackerdata()
        {
            shash = csocket = cfpath = "";
        }
        trackerdata(string hash,string port,string fpath)
        {
            shash = hash;
            csocket = port;
            cfpath = fpath;
        }
};


char* seederfilep;
char* trackerlogpath;
map<string,vector<trackerdata> > trackertable;

void writelog(string str)
{
    ofstream myfile(trackerlogpath, std::ios_base::app | std::ios_base::out);
    myfile << str << endl;
    myfile.close();
}

//breaks down string into tokens based delimiter character
vector<string> strprocess(string command , char lim)
{
    vector<string> ans;
    string cur = "";
    for(char x : command)
    {
        if(x=='\\')
            continue;
        if(x==lim)
        {
            ans.push_back(cur);
            cur="";
        }
        else cur+=x;
    }
    ans.push_back(cur);
    return ans;
}


bool readseederlist(char* fpath)
{
    ifstream fp(fpath,ifstream::binary);
    if(!fp)
    {
        writelog("could not open seederlist file !\n");
        cout<<"could not open seederlistfile !"<<endl; 
        return false;  
    }
    string linecontent;
    writelog("Content of seederlist file : \n");
    while(getline(fp,linecontent))
    {
        string data = linecontent;
        vector<string> tokens = strprocess(data,' ');
        trackerdata t(tokens[0],tokens[1],tokens[2]);
        trackertable[tokens[0]].push_back(t);
    }
    return true;
}

void writeseeder(char* fpath,string data)
{
    ofstream myfile(fpath, std::ios_base::app | std::ios_base::out);
    myfile << data << endl;
    myfile.close();
}

void updateseeder(char *seederlistfp)
{
    ofstream filep(seederlistfp,std::ios_base::out);
    for(auto it : trackertable)
    {
        for(auto td : it.second)
        {
            string data = td.shash + " " + td.csocket + " " + td.cfpath;
            filep << data << endl;
        }
    }
    filep.close();
}

void printeverything()
{
    writelog("-----------seederlist data-------------\n");
    for(auto it : trackertable)
    {
        writelog("Data for (" + it.first + ") : ");
        for(auto td : it.second)
        {
            writelog(td.csocket + "---> " + td.cfpath);
        }
    }
    writelog("------------------------------------------\n");
}


string fget(vector<string> tokens)
{
    string ans = "" , hash = tokens[1];
    if(trackertable.find(hash)==trackertable.end())
        return "No client found !";
    int sz = trackertable[hash].size(),cnt=1;
    for(auto td : trackertable[hash])
    {
        ans+=td.csocket+"#"+td.cfpath;
        if(cnt!=sz)
            ans+="@";
        cnt++;
    }
    return ans;
}

string fremove(vector<string> tokens, string data, char *seederlistfp)
{
    string ans = "";
    int flag = 0;
    string shash = tokens[1],clsocket=tokens[2];
    if(trackertable.find(shash)!=trackertable.end())
    {
        int sz = trackertable[shash].size();
        for(auto it=trackertable[shash].begin(); it!=trackertable[shash].end(); it++)
        {
            if((*it).csocket == clsocket)
            {
                flag = 1;
                if(sz==1)
                    trackertable.erase(shash);
                else
                    trackertable[shash].erase(it);
                break;
            }
        }
    }
    if(flag)
    {
        updateseeder(seederlistfp);
        writelog("File successfully removed\n");
        return "pass";
    }
    else
    {
        writelog("File was not shared\n");
        return "fail";
    }
}


string fshare(vector<string> token, string data, char *seederlistfp)
{
    string sldata = token[1]+" "+token[2]+" "+token[3];
    trackerdata td(token[1],token[2],token[3]);
    if(trackertable.find(td.shash)!=trackertable.end())
    {
        bool found = false;
        for(auto it : trackertable[td.shash])
        {
            if(it.csocket==td.csocket)
            {
                found = true;
                break;
            }
        }
        if(found)
            return "PASS1";
        else
        {
            trackertable[td.shash].push_back(td);
            writeseeder(seederlistfp,sldata);
            return "PASS2";
        }
    }
    else
    {
        trackertable[td.shash].push_back(td);
        writeseeder(seederlistfp,sldata);
        return "PASS3";
    }
}

string fclose(vector<string> tokens, char* seederlistfp)
{
    string clientsocket = tokens[1];
    for(auto it1 : trackertable)
    {
        string hash = it1.first;

        for(auto it2 = it1.second.begin(); it2!=it1.second.end(); it2++)
        {
            if((*it2).csocket==clientsocket)
            {
                if(it1.second.size()==1)
                    trackertable.erase(it1.first);
                else
                    trackertable[it1.first].erase(it2);
            }
        }
    }
    updateseeder(seederlistfp);
    return "success";
}


void *serverservice(void *socket_desc)
{
    int new_socket = *(int *)socket_desc;
    while(true)
    {
        bool close1 = false;
        char buffer[1024] = {0};
        int rc = read(new_socket,buffer,1024);
        if(rc==0)
        {
            close(new_socket);
            return socket_desc;
        }
        writelog("Tracker get Data from client : " + string(buffer));
        string clientreply,data = string(buffer);
        vector<string> tokens = strprocess(data,'#');
        if(tokens[0]=="share")
        {
            writelog("tracker executing share command\n");
            clientreply = fshare(tokens,data,seederfilep);
        }
        else if(tokens[0]=="get")
        {
            writelog("tracker executing get command\n");
            clientreply = fget(tokens);
        }
        else if(tokens[0]=="remove")
        {
            writelog("tracker executing remove command\n");
            clientreply = fremove(tokens,data,seederfilep);
        }
        else if(tokens[0]=="close")
        {
            writelog("tracker executing close command\n");
            clientreply = fclose(tokens,seederfilep);
            close1 = true;
        }
        else
        {
            writelog("cannot identify client request\n");
        }
        printeverything();
        char *serverreply = new char[clientreply.length()+1];
        strcpy(serverreply,clientreply.c_str());
        send(new_socket,serverreply,strlen(serverreply),0);
        writelog("Reply message sent from tracker to client\n");
        if(close1)
        {
            close(new_socket);
            break;
        }
    }
    return socket_desc;
}



int main(int argc,char *argv[])
{
    socketclass trackersocket1,trackersocket2;
    pthread_t thread_id;
    if(argc!=5)
    {
        cout << "Invalid Argument !!!" << endl;
        writelog("invalid arguments");
    }
    else
    {
        cout<<"reader se pehle\n";
        readseederlist(argv[3]);
        cout<<"reader ke baad\n";
        writelog("-----initial data in seederfile-------");
        printeverything();
        trackersocket1.setdata(string(argv[1]));
        trackersocket2.setdata(string(argv[2]));
        seederfilep = argv[3];
        trackerlogpath = argv[4];
        ofstream myfile(trackerlogpath,std::ios_base::out);
        myfile.close();
        cout<<"arguments read\n";

        int server_fd, new_socket;
        struct sockaddr_in address;
        int opt = 1;
        int addrlen = sizeof(address);
        server_fd = socket(AF_INET,SOCK_STREAM,0);
        if(server_fd == 0)
        {
            cout<<"socket failed\n";
            writelog("socket failed");
            exit(EXIT_FAILURE);
        }

        if(setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR | SO_REUSEPORT,&opt,sizeof(opt)))
        {
            cout<<"setsockopt failed\n";
            writelog("setsockopt failed");
            exit(EXIT_FAILURE);
        }

        address.sin_family = AF_INET;
        address.sin_addr.s_addr = inet_addr(trackersocket1.ip);
        address.sin_port = htons(trackersocket1.port);

        int b = bind(server_fd,(struct sockaddr *)&address, sizeof(address));

        if(b<0)
        {
            cout<<"Bind failed\n";
            writelog("Bind Failed");
            exit(EXIT_FAILURE);
        }

        if(listen(server_fd,10)<0)
        {
            cout<<"listen failed\n";
            writelog("listening");
            exit(EXIT_FAILURE);
        }

        cout<<"tracker is active\n";
        while(1)
        {
            new_socket = accept(server_fd,(struct sockaddr *)&address,(socklen_t *)&addrlen);
            if(new_socket<0)
            {
                writelog("Failed to accept connection ");
                exit(EXIT_FAILURE);
            }

            writelog("Connection accepted");

            if(pthread_create(&thread_id,NULL,serverservice,(void *)&new_socket)<0)
            {
                writelog("failed to create thread\n");
            }
        }
        cout<<"the end\n";
    }
}







// int main()
// {
//     cout<<"success"<<endl;
//     trackerdata td1("hash1","port2","fpath3");
//     cout<<td1.shash<<" "<<td1.csocket<<" "<<td1.cfpath<<endl;
//     writelog("----sample log successful------\n");
//     string test = "abc:de\\fg:hi\\jkl:mnop\\";
//     vector<string> vs1 = stringProcessing(test,':');
//     vector<string> vs2 = strprocess(test,':');
//     for(auto x : vs1)
//         cout<<x<<" ";
//     cout<<endl;
//     for(auto x : vs2)
//         cout<<x<<" ";
//     cout<<endl;
// }
