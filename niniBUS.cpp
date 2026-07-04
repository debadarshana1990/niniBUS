#include "niniBUS.h"

// Define static member
uint32_t niniBUS::dataStruct_idx = 0;

bool niniBUS::push_msg(msg message)
{
    //get the ID first
    uint32_t msgID = message.msgID;
    ///check if present in the map
    auto it = dataStruct_map.find(msgID);
    if(it == dataStruct_map.end())
    {
        //we are processing a new msg. this is costly
        //need to create a new object for this msgD
        niniMSG *newMSG = new niniMSG(msgID);
        dataStruct_map[msgID] = dataStruct_idx++;
        dataStruct.push_back(newMSG); //everything  is here
    }

        //we have already created the msgID object. just push the content
    uint32_t idx = dataStruct_map[msgID];
    dataStruct[idx]->content.push_back(message.content);

    return true;
}
/* subscribe*/
bool niniBUS::subscribe(uint32_t msgID)
{
    // Check if the message ID exists
    auto it = dataStruct_map.find(msgID);
    if (it == dataStruct_map.end())
    {
        cerr << "future subscribing at the moment no msg having mSG ID " << msgID <<endl;
        niniMSG *newMSG = new niniMSG(msgID);
        dataStruct_map[msgID] = dataStruct_idx++;
        dataStruct.push_back(newMSG); //everything  is here
        newMSG->num_receivers++;
    }

    return true;
}

/* pull message is interesting */
bool niniBUS::pull_msg(msg& message)
{
    //check if the msgID is present
    auto it = dataStruct_map.find(message.msgID);
    if (it == dataStruct_map.end())
    {
        cerr << "Message ID not found. Subscribing for future messages." << endl;
        subscribe(message.msgID);
        return false;
    }

    // Get the message object
    niniMSG* msgObj = dataStruct[it->second];
    if (msgObj->content.empty())
    {
        cerr << "No content available for message ID " << message.msgID << endl;
        return false;
    }

    // Pull the latest content
    message.content = msgObj->content.front();
    msgObj->content.pop_front();

    return true;
}