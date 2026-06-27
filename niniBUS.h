#include <iostream>
#include <vector>
#include <queue>
#include <string>

using namespace std;
typedef enum
{
    LOCAL_SOCKET = 0, //default 
    IPC,
    THREAD,
} interface;    
class niniBUS
{
    private:
        vector<queue<string>> receivers; // Vector of queues for each receiver
        interface iftype;
        bool initNiniBUS()
        {
            iftype = LOCAL_SOCKET; //default
            // Initialization code here
            return true;
        }
        void setInterface(interface newInterface)
        {
            iftype = newInterface; // only if required after runtime
        }
    
    public:
        static const char* interface_to_string(interface ift)
        {
            switch (ift)
            {
                case LOCAL_SOCKET: return "LOCAL_SOCKET";
                case IPC: return "IPC";
                case THREAD: return "THREAD";
                default: return "UNKNOWN";
            }
        }

        niniBUS()
        {
            iftype = LOCAL_SOCKET; //default
            cout << "Welcome to niniBUS (interface: " << interface_to_string(iftype) << ")" << endl;
        }

        niniBUS(int interfaceType)
        {
            iftype = static_cast<interface>(interfaceType);
            cout << "Welcome to niniBUS (interface: " << interface_to_string(iftype) << ")" << endl;
        }

        ~niniBUS()
        {
            cout << "Exiting niniBUS" << endl;
        }

        int init_niniServer();
        int init_niniServerIPC();
        int init_niniServerThread();
};
