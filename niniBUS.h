#pragma once
#include <iostream>
#include <vector>
#include <queue>
#include <string>
#include <stdint.h>
#include <unordered_map>
#include <deque>

using namespace std;
typedef enum
{
    LOCAL_SOCKET = 0, //default 
    IPC,
    THREAD,
} interface;
typedef struct msg
{
    uint32_t msgID;
    string content;
}msg;
class niniMSG
{
public:
    // Message structure definition
    uint32_t msgID;
    deque<string> content;
    // Add more fields as needed
    uint32_t num_receivers; // number of receivers

    niniMSG(uint32_t id) : msgID(id), num_receivers(1)
    {
        cout<<"msgID: "<<msgID<<" created"<<endl;
        cout<<"num_receivers: "<<num_receivers<<endl;
    };
    niniMSG(const niniMSG& other) = default;
    niniMSG& operator=(const niniMSG& other) = delete; //no copy or move or assignment allowed
    ~niniMSG()
    {
        cout<<"msgID: "<<msgID<<" destroyed"<<endl;
    }
};
class niniBUS
{
private:
    vector<niniMSG*> dataStruct;
    static uint32_t dataStruct_idx; //hold the next new publisher idx 
    unordered_map<uint32_t, uint32_t > dataStruct_map; // map for msg iD, its idx 

public:
    niniBUS() = default;
    ~niniBUS()
    {
        cout<<"Are You Sure You Want To Destroy The Message Bus?"<<endl;
        cout<<"All messages will be lost."<<endl;
        cout<<"Be a good Human. World is enough for everyone."<<endl;
    }
    bool push_msg(msg message);
    bool pull_msg(msg& message);
    bool subscribe(uint32_t msgID);

};
