#include<bits/stdc++.h>
using namespace std;

class socketclass
{
    public:
    char *ip;
    int port;
    socketclass()
    {
        port = 0;
    }

    void setdata(string s)
    {
        vector<string> tokens;
        stringstream check1(s);
        string t;
        while(getline(check1,t,':'))
        {
            tokens.push_back(t);
        }
        string strip = tokens[0];
        ip = new char[strip.length()+1];
        strcpy(ip,strip.c_str());
        port = stoi(tokens[1]);
    }
};

// int main()
// {
//     socketclass sc;
//     sc.setdata("127.0.0.1:8080");
//     cout<<sc.ip<<" "<<sc.port<<endl;
// }