#include<bits/stdc++.h>
#include<unistd.h>
#include<stdio.h>
#include<sys/socket.h>
#include<stdlib.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<fstream>
#include<sys/stat.h>
#include<sys/types.h>
#include<openssl/sha.h>
#include<pthread.h>
#include<dirent.h>
#include "socket.cpp"
#define CSIZE 1ll<<19

using namespace std;

const char* logpath;

void writelog(string str)
{
    ofstream myfile(logpath,std::ios_base::app | std::ios_base::out);
    myfile<<str<<endl;
    myfile.close();
}

//mtorrent generator's functions
string calHashofchunk(char *schunk,int length1,int shorthashflag)
{
    unsigned char hash[SHA_DIGEST_LENGTH];
    char buf[SHA_DIGEST_LENGTH*2];
    SHA1((unsigned char *)schunk,length1,hash);
    for(int i=0; i<SHA_DIGEST_LENGTH; i++)
        sprintf((char *)&(buf[i*2]),"%02x",hash[i]);
    string ans;
    int len = (shorthashflag==1) ? 20 : SHA_DIGEST_LENGTH*2;
    for(int i=0; i<len; i++)
        ans+=buf[i];
    return ans;
}

string getFileHash(char *fpath)
{
    string fileHash;
    ifstream file1(fpath, ifstream::binary);

    if(!file1)
    {
        cout<<"FILE DOES NOT EXIST : "<<string(fpath)<<endl;
        return "-1";
    }
    struct stat fstatus;
    stat(fpath,&fstatus);

    long long total_size = fstatus.st_size;
    long long chunk_size = CSIZE;

    int total_chunks = total_size/chunk_size;
    int last_chunk_size = total_size%chunk_size;

    if(last_chunk_size!=0)
        total_chunks++;
    else
        last_chunk_size = chunk_size;
    
    for(int chunk = 0; chunk<total_chunks; chunk++)
    {
        int cur_sz = 0;
        if(chunk == total_chunks-1)
            cur_sz = last_chunk_size;
        else
            cur_sz = chunk_size;

        char *chunk_data = new char[cur_sz];
        file1.read(chunk_data,cur_sz);
        string sh1out = calHashofchunk(chunk_data,cur_sz,1);
        fileHash = fileHash + sh1out; 
    }
    return fileHash;
}

string createTorrentFile(char *fpath,char *mtpath, string tcksocket1,string tcksocket2)
{
    writelog("Mtorrent creator called for filepath : " + string(fpath));
    struct stat ab;
    if(stat(fpath,&ab)==-1)
    {
        cout<<"FILE NOT FOUND (torrent creator) \n";
        return "-1";
    }
    ofstream myfile;
    myfile.open(string(mtpath));
    myfile<<tcksocket1<<endl;
    myfile<<tcksocket2<<endl;
    myfile<<string(fpath)<<endl;
    myfile<<ab.st_size<<endl;
    string filehash = getFileHash(fpath);
    myfile <<filehash << endl;
    myfile.close();
    writelog("Mtorrent created succesfully !");
    return filehash;
}



//Client Commands
string executeshareclient(vector<string> tokens,string clntsckstr,string trcksck1str,string trcksck2str)
{
    string cmd = tokens[0],fpath = tokens[1],mtpath = tokens[2];
    char *fp,*tp;
    fp = new char[fpath.length()+1];
    strcpy(fp,fpath.c_str());
    tp = new char[mtpath.length()+1];
    strcpy(tp,mtpath.c_str());
    string filehash = createTorrentFile(fp,tp,trcksck1str,trcksck2str);
    if(filehash == "-1")
    {
        writelog("\n Error Encounter for creating hash of file in sharing : " + fpath);
        return "-1";
    }
    writelog("share cmd gets Long Hash : "+ filehash);
    char *longhash = new char[filehash.length()+1];
    strcpy(longhash,filehash.c_str());
    string shorthash = calHashofchunk(longhash,filehash.length(),0);

    string ans = cmd + "#" + shorthash + "#" + clntsckstr + "#" + fpath;
    writelog("(SHARE cmd) Complex Data need to send to tracker : "+ ans);
    return ans;
}


string executegetclient(vector<string> tokens)
{
    cout<<"inside get execution\n";
    string cmd = tokens[0],mtpath = tokens[1];
    char *tp = new char[mtpath.length()+1];
    strcpy(tp,mtpath.c_str());
    ifstream fileptr(mtpath,ifstream::binary);
    if(!fileptr)
    {
        cout<<"Mtorrent does not exist : Get command failed : "<< string(mtpath) << endl;
        return "-1";
    }
    int count = 4;
    string linecontent;
    while((count>0) && getline(fileptr,linecontent))
        count--;
    getline(fileptr,linecontent);
    cout<<"outside while\n";
    string mtorrenthash = linecontent;
    fileptr.close();
    char *longhash = new char[mtorrenthash.length()+1];
    strcpy(longhash,mtorrenthash.c_str());
    cout<<"bfore call hash\n";
    string shorthash = calHashofchunk(longhash,mtorrenthash.length(),0);
    cout<<"after call hash\n";
    string ans = cmd + "#" + shorthash;
    writelog("GET command : Complex Data needed to send to tracker : "+ ans);
    cout<<"get success\n";
    return ans;
}

string executeremoveclient(vector<string> tokens, string clntsckstr)
{
    string cmd = tokens[0];
    string mtpath = tokens[1];
    char *tp = new char[mtpath.length()+1];
    strcpy(tp,mtpath.c_str());
    ifstream fileptr(mtpath,ifstream::binary);
    if(!fileptr)
    {
        cout<<"Mtorrent does not exist (REMOVE) : "<<string(mtpath)<<endl;
        return "-1";
    }
    int count = 4;
    string linecontent;
    while((count>0) && getline(fileptr,linecontent))
        count--;
    getline(fileptr,linecontent);
    string mtorrenthash = linecontent;
    fileptr.close();

    char *longhash = new char[mtorrenthash.length()+1];
    strcpy(longhash,mtorrenthash.c_str());
    string shorthash = calHashofchunk(longhash,mtorrenthash.length(),0);

    string ans = cmd + "#" + shorthash + "#" + clntsckstr;
    writelog("(REMOVE) Complex Data need to send to tracker : " + ans);
    return ans;
}


void* seederservice(void *socket_desc)
{
    int new_socket = *(int *)socket_desc;
    char buffer[1024] = {0};
    read(new_socket,buffer,1024);
    writelog("Seeder gets data from clients : " + string(buffer));
    string actualfilepath = string(buffer);

    char *fpath = new char[actualfilepath.length()+1];
    strcpy(fpath,actualfilepath.c_str());

    ifstream file1(fpath,ifstream::binary);

    if(!file1)
    {
        cout<<"Can't open file1 : "<<string(fpath)<<endl;
    }
    struct stat fstatus;
    stat(fpath,&fstatus);
    long long total_size = fstatus.st_size;
    long long chunk_size = CSIZE;
    long long total_chunks = total_size/chunk_size;
    long long last_chunk_size = total_size % chunk_size;

    if(last_chunk_size != 0)
        total_chunks++;
    else
        last_chunk_size = chunk_size;
    
    for(int chunk = 0; chunk<total_chunks; chunk++)
    {
        int cur_sz;
        if(chunk == total_chunks-1)
            cur_sz = last_chunk_size;
        else
            cur_sz = chunk_size;
        char *chunk_data = new char[cur_sz];
        file1.read(chunk_data,cur_sz);
        send(new_socket,chunk_data,cur_sz,0); 
    }
    close(new_socket);
    file1.close();
    return socket_desc;
}


void *seederserverservice(void *socket_desc)
{
    string csocket = *(string *)socket_desc;
    socketclass cserversocket;
    pthread_t thread_id;
    cserversocket.setdata(csocket);
    int server_fd,new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if((server_fd = socket(AF_INET,SOCK_STREAM,0))==0)
    {
        perror("socket creation failed in seeder");
        exit(EXIT_FAILURE);
    }
    if(setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR | SO_REUSEPORT,&opt,sizeof(opt)))
    {
        perror("setsocketopt in seeder");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(cserversocket.ip);
    address.sin_port = htons(cserversocket.port);

    if(bind(server_fd,(struct sockaddr *)&address,sizeof(address))<0)
    {
        perror("bind failed in seeder");
        exit(EXIT_FAILURE);
    }
    if(listen(server_fd,10)<0)
    {
        perror("listen error in seeder");
        exit(EXIT_FAILURE);
    }
    while(true)
    {
        if((new_socket = accept(server_fd,(struct sockaddr *)&address,(socklen_t *)&addrlen))<0)
        {
            perror("accept error in seeder");
            exit(EXIT_FAILURE);
        }
        writelog("******* COnnection accepted in seeder ******");

        if(pthread_create(&thread_id,NULL,seederservice,(void *)&new_socket)<0)
        {
            perror("could not create thread in seeder");
        }
        if(new_socket < 0)
        {
            perror("accept failed in seeder");
        }
    }
    return socket_desc;
}


struct complexData
{
    char *replydata1;
    char *destpath1;
    char *getmtorrentpath1;
    int sock1;
};

string clientsocketstr,trackersocket1str,trackersocket2str;
map<string,string> downloadstatus;
vector<pair<string,string> > clientfilepath;

int dofiletransfering(string replydata,string destpath)
{
    writelog("dofiletransfering (function waali) called : " + replydata);
    stringstream check1(replydata);
    string intermediate;
    while(getline(check1,intermediate,'@'))
    {
        stringstream check2(intermediate);
        vector<string> clientsocketvc;
        string subintermediate;
        while(getline(check2,subintermediate,'#'))
        {
            clientsocketvc.push_back(subintermediate);
        }
        clientfilepath.push_back({clientsocketvc[0],clientsocketvc[1]});
    }
    writelog("***** Available seeders for downloading file *******");
    for(auto x : clientfilepath)
    {
        writelog(x.first + "--->" + x.second);
    }
    writelog("******************************************************");
    socketclass csocket;
    csocket.setdata(clientfilepath[0].first);
    string filepath = clientfilepath[0].second;
    int sock = 0 ;
    struct sockaddr_in serv_addr;
    if((sock = socket(AF_INET,SOCK_STREAM,0))<0)
    {
        cout<<"\n Socket Creation Error \n";
        return -1;
    }
    memset(&serv_addr,'0',sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(csocket.port);

    if(inet_pton(AF_INET,csocket.ip,&serv_addr.sin_addr)<=0)
    {
        cout<<"\n Client File : Invalid address - Address Not Supported \n";
        return -1;
    }

    if(connect(sock,(struct sockaddr *)&serv_addr,sizeof(serv_addr))<0)
    {
        cout<<"\n Connection Failed in Client \n";
        return -1;
    }

    writelog("****** Connection established for file transfered ! \n");

    char *d_path = new char[destpath.length()+1];
    strcpy(d_path, destpath.c_str());

    ofstream myfile(d_path,ofstream::binary);

    char* clientreply = new char[filepath.length()+1];
    strcpy(clientreply,filepath.c_str());
    send(sock,clientreply,strlen(clientreply),0);
    int n;
    downloadstatus[destpath]="D";
    do
    {
        /* code */
        char *buffer = new char[CSIZE];
        n = read(sock,buffer,CSIZE);
        myfile.write(buffer,n);
    } while (n>0);

    myfile.close();
    return 1;
}


void *getcommandExecution(void *complexstruct)
{
    cout<<"inside get function\n";
    writelog("inside get command execution");
    struct complexData obj = *(struct complexData *)complexstruct;
    string replydata = string(obj.replydata1);
    string destpath = string(obj.destpath1);
    string getmtorrenpath = string(obj.getmtorrentpath1);
    int sock = obj.sock1;
    cout<<"before file transfering\n";
    int ans = dofiletransfering(replydata,destpath);
    cout<<"after file transfering\n";
    if(ans==1)
    {
        cout<<"File Successfully Downloaded\n";
        downloadstatus[destpath] = "S";
        vector<string> temptokens;
        temptokens.push_back("share");
        temptokens.push_back(destpath);
        temptokens.push_back(getmtorrenpath);
        string complexdata = executeshareclient(temptokens,clientsocketstr,trackersocket1str,trackersocket2str);
        if(complexdata != "-1")
        {
            char *clt = new char[complexdata.length()+1];
            strcpy(clt,complexdata.c_str());
            send(sock,clt,strlen(clt),0);
            char buff[1024] = {0};
            read(sock,buff,1024);
        }
    }
    else
    {
        cout<<"Error in Downloading The File\n";
    }

    return complexstruct;
}

void readallmtorrentfile(int sc)
{
    DIR *d;
    struct dirent *dir;
    d = opendir(".");
    if(d)
    {
        while((dir=readdir(d))!=NULL)
        {
            string mtfile = string(dir->d_name);
            string fileextention = ".mtorrent";
            if(mtfile.find(fileextention)!=string::npos)
            {
                ifstream fileptr(mtfile,ifstream::binary);
                int count = 4;
                string linecontent;
                string fpath;
                while((count>0) && getline(fileptr,linecontent))
                {
                    if(count==2)
                        fpath = linecontent;
                    count--;
                }
                getline(fileptr,linecontent);
                string mtorrenthash = linecontent;
                fileptr.close();

                char *longhash = new char[mtorrenthash.length()+1];
                strcpy(longhash,mtorrenthash.c_str());
                string shorthash = calHashofchunk(longhash,mtorrenthash.length(),0);
                string ans = "share#" + shorthash + "#" +clientsocketstr + "#" + fpath;
                char *clientrply = new char[ans.length()+1];
                strcpy(clientrply,ans.c_str());
                send(sc,clientrply,strlen(clientrply),0);
                char buffer[1024]={0};
                read(sc,buffer,1024);
                writelog("read mtorrentfile sent to server & got response : " + string(buffer));

            }
        }
        closedir(d);
    }
}


vector<string> stringProcessing(string command , char lim)
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


int main(int argc,char const *argv[])
{
    socketclass clientsocket,trackersocket1,trackersocket2;
    if(argc!=5)
    {
        cout<<"Invalid Argument in Client !!!\n";
        return 0;
    }
    clientsocketstr = string(argv[1]);
    trackersocket1str = string(argv[2]);
    trackersocket2str = string(argv[3]);
    logpath = argv[4];
    clientsocket.setdata(clientsocketstr);
    trackersocket1.setdata(trackersocket1str);
    trackersocket2.setdata(trackersocket2str);
    ofstream myfile(logpath, std::ios_base::out);
    myfile.close();
    writelog("******* new client started *********");
    writelog("CLient socket : " + clientsocketstr);
    writelog("Tracker 1 socket : " + trackersocket1str);
    writelog("Tracker 2 socket : " + trackersocket2str);
    pthread_t cserviceid;
    if(pthread_create(&cserviceid,NULL,seederserverservice,(void *)&clientsocketstr) < 0)
    {
        perror("\n could not create thread in client side \n");
    }
    int sock = 0;
    struct sockaddr_in serv_addr;
    if((sock = socket(AF_INET,SOCK_STREAM,0))<0)
    {
        cout<<"\n Socket creation errror in client side \n";
        return -1;
    }
    memset(&serv_addr,'0',sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(trackersocket1.port);

    if(inet_pton(AF_INET,trackersocket1.ip,&serv_addr.sin_addr)<=0)
    {
        printf("\n Client File : INvalid address/ Address not supported \n");
        return -1;
    }
    if(connect(sock,(struct sockaddr *)&serv_addr,sizeof(serv_addr))<0)
    {
        printf("\n Connection Failed in Client Side \n");
        return -1;
    }
    writelog("***********Connection Successfully Established with tracker !! *******");
    readallmtorrentfile(sock);

    while(true)
    {
        int getflag = 0,closeflag = 0;
        char *mtorrentfilepath;
        string strcmd,destpath,getcmdmtorrentpath;

        getline(cin>>ws,strcmd);
        writelog("Command from Client : " + strcmd);
        vector<string> tokens = stringProcessing(strcmd,' ');
        string complexdata;

        if(tokens[0] == "share")
        {
            if(tokens.size()!=3)
            {
                cout<<"INVALID ARGUMENTS --- SHARE COMMAND \n";
            }
            writelog("Share command execution in client side");
            complexdata = executeshareclient(tokens,clientsocketstr,trackersocket1str,trackersocket2str);
            if(complexdata=="-1")
                continue;
        }
        else if(tokens[0]=="get")
        {
            cout<<"inside get\n";
            if(tokens.size()!=3)
            {
                cout<<"INVALID ARGUMENTS --- GET COMMAND"<<endl;
                continue;
            }
            cout<<"get command in execution \n";
            writelog("GET command execution in client side");
            complexdata = executegetclient(tokens);
            cout<<"post execute get\n";
            destpath = tokens[2];
            getcmdmtorrentpath = tokens[1];
            if(complexdata == "-1")
                continue;
            else
                getflag = 1;
            cout<<"get khatam\n";
        }
        else if(tokens[0]=="remove")
        {
            if(tokens.size()!=2)
            {
                cout<<"INVALID ARGUMENTS ---- REMOVE COMMAND \n";
                continue;
            }
            writelog("REMOVE command exe in client side");
            complexdata = executeremoveclient(tokens,clientsocketstr);
            if(complexdata == "-1")
                continue;
        }
        else if(tokens[0]=="show_downloads")
        {
            writelog("SHOW_DOWNLOAD command in execution");
            if(downloadstatus.empty())
                cout<<"NO DOWNLOADS TILL NOW\n";
            else
            {
                cout<<"*********** DOWNLOADS ************\n";
                for(auto item : downloadstatus)
                    cout<<item.second<<" : "<<item.first<<endl;
            }
        }
        else if(tokens[0]=="close")
        {
            writelog("CLOSE command in execution in client side");
            complexdata = "close#" + clientsocketstr;
            closeflag = 1;
        }
        else
        {
            writelog("invalid command !!!!");
            cout<<"INVALID COMMAND !!!!\n";
            continue;
        }

        char *clientreply = new char[complexdata.length()+1];
        strcpy(clientreply,complexdata.c_str());

        send(sock,clientreply,strlen(clientreply),0);
        writelog("CLient message sent to tracker");

        char buffer[1024] = {0};
        read(sock,buffer,1024);
        writelog("Client("+clientsocketstr+")got reply from tracker ---> "+string(buffer));

        if(getflag != 1)
            cout<<string(buffer)<<endl;
        string response = string(buffer);

        if(getflag == 1)
        {
            struct complexData obj1;
            obj1.replydata1 = new char[response.length()+1];
            strcpy(obj1.replydata1,response.c_str());
            obj1.destpath1 = new char[destpath.length()+1];
            strcpy(obj1.destpath1,destpath.c_str());
            obj1.getmtorrentpath1 = new char[getcmdmtorrentpath.length()+1];
            strcpy(obj1.getmtorrentpath1,getcmdmtorrentpath.c_str());
            obj1.sock1 = sock;

            pthread_t getclientid;
            if(pthread_create(&getclientid,NULL,getcommandExecution,(void *)&obj1) < 0)
            {
                perror("\n could not create thread in client side \n");
            }
        }
        getflag = 0;

        if(response == "FILE SUCCESSFULLY REMOVED")
        {
            if(remove(mtorrentfilepath)!=0)
                perror("\n Error deleting mtorrent file \n");
        }

        if(closeflag == 1)
        {
            cout<<"thank you, bye !\n";
            close(sock);
            break;
        }
    }
    cout<<"success\n";
    return 0;
}
