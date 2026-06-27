#include <iostream>
#include "niniBUS.h"

using namespace std;

int main()
{
    cout << "Welcome to niniBUS" << endl;
    niniBUS bus(2);
    bus.init_niniServerThread();
    return 0;
}


